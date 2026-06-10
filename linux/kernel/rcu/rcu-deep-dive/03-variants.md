# 第三篇：RCU 变体 — SRCU、Tasks RCU 与 Tiny RCU

> 源码：kernel/rcu/srcutree.c (2203行) | kernel/rcu/tasks.h (1608行) | kernel/rcu/tiny.c (252行)
> 头文件：include/linux/srcutree.h (377行)

系列目录：[RCU 内核源码深度解析](./README.md)

---

## 1. 为什么需要 RCU 变体？

经典 RCU（Tree RCU）有两个硬性约束，无法满足所有内核场景：

| 约束 | 经典 RCU | 问题场景 |
|------|---------|---------|
| 读端临界区**不可睡眠** | `rcu_read_lock()` 内部禁止抢占 | `srcu_read_lock()` 需要持有期间可睡眠（如设备驱动、DM 映射器） |
| 无法等待**主动上下文切换** | 基于 `preempt_disable`/HZ tick 检测静默态 | 需要"所有任务至少主动上下文切换过一次"的场景（如回收 trampoline） |

这催生了三个 RCU 变体：

```
┌────────────────────────────────────────────────────┐
│                  Linux RCU 家族                      │
├──────────┬────────────────┬───────────┬─────────────┤
│ Tree RCU │     SRCU       │ Tasks RCU │  Tiny RCU   │
│ (经典)    │  (可睡眠读端)    │ (自愿ctx切换)│ (小型/UP)    │
│ ~5000行  │   ~2200行       │ ~1600行    │  ~252行     │
│ tree.c   │  srcutree.c     │ tasks.h   │  tiny.c     │
└──────────┴────────────────┴───────────┴─────────────┘
```

---

## 2. SRCU — Sleepable RCU

### 2.1 核心区别：读端可睡眠

经典 RCU 的 `rcu_read_lock()` 仅禁止抢占（`preempt_disable()`），但**不能睡眠**。SRCU 的读端使用每个 SRCU 域独立的索引计数器，**不依赖 preempt_disable**：

```
经典 RCU 读端:                          SRCU 读端:
rcu_read_lock()                         idx = srcu_read_lock(&ssp);
  → __acquire(RCU)                       → 原子递增 ssp->srcu_ctrp[idx] 的计数
  → preempt_disable()                    → 允许睡眠!
      // 不可睡眠!                        // 可调度、可阻塞
      do_something();                        do_something_blocking();
  → preempt_enable()                     srcu_read_unlock(&ssp, idx);
rcu_read_unlock()                         → 原子递增 ssp->srcu_ctrp[idx] 的解锁计数
```

### 2.2 数据结构层次结构

SRCU 使用**两级计数器**替代经典 RCU 的抢占计数：

```
struct srcu_struct {                      // include/linux/srcutree.h:105
    struct srcu_ctr __percpu *srcu_ctrp;  // 每个 SRCU 域有 [2] 个索引计数器
    struct srcu_data __percpu *sda;       // 每 CPU 数据
    struct srcu_usage *srcu_sup;          // 更新端数据
};

struct srcu_ctr {                         // include/linux/srcutree.h:21
    atomic_long_t srcu_locks;             // 读锁定次数（每 CPU, 每索引）
    atomic_long_t srcu_unlocks;           // 读解锁次数（每 CPU, 每索引）
};

struct srcu_data {                        // include/linux/srcutree.h:30
    struct srcu_ctr srcu_ctrs[2];         // 两个计数器槽位（双缓冲）
    struct rcu_segcblist srcu_cblist;     // 分段回调链表
    struct srcu_node *mynode;             // 所属叶子 srcu_node
    unsigned long grpmask;                // 在 srcu_node 中的 CPU 掩码
};

struct srcu_node {                        // include/linux/srcutree.h:57
    raw_spinlock_t __private lock;
    unsigned long srcu_have_cbs[4];       // 各子节点有回调节点的 GP 序号
    unsigned long srcu_data_have_cbs[4];  // 有回调的 srcu_data 位掩码
    struct srcu_node *srcu_parent;        // 父节点
    int grplo, grphi;                     // CPU 范围
};

struct srcu_usage {                       // include/linux/srcutree.h:71
    struct srcu_node *node;               // 组合树根节点
    struct srcu_node *level[];            // 每层首节点
    unsigned long srcu_gp_seq;            // 全局 GP 序号
    unsigned long srcu_gp_seq_needed;     // 最新被请求的 GP 序号
    struct mutex srcu_gp_mutex;           // 串行化 GP 工作
    ...
};
```

### 2.3 双计数器索引机制：为何用两个槽位？

SRCU 的 `srcu_ctrs[2]` 是核心设计。GP 开始时使用一个索引（如 idx=0），所有新进入读端的选择另一个索引（idx=1）。这样 GP 只需等待旧索引的读端退出，新读端不受影响：

```
时间轴 ──────────────────────────────────────────────►
         GP 开始              GP 结束
           ↓                     ↓
idx=0:  [读端A────────]  [读端B──]   ← GP 关注的索引
idx=1:          [读端C────────────────────────] ← 新读端不受影响
```

SRCU 使用 GP 序号（`srcu_gp_seq`）的低位来选择当前活跃槽位：

```c
// srcutree.c 中的 GP 状态机常量 (srcutree.h:137-140)
#define SRCU_STATE_IDLE   0   // 空闲，无 GP 在进行
#define SRCU_STATE_SCAN1  1   // 扫描索引 0 的读端
#define SRCU_STATE_SCAN2  2   // 扫描索引 1 的读端
```

### 2.4 自动加速与大小模式

`srcutree.c:34-37` 定义了自动加速机制：

```c
// srcutree.c:34-37
/* Holdoff in nanoseconds for auto-expediting. */
#define DEFAULT_SRCU_EXP_HOLDOFF (25 * 1000)  // 25 微秒
static ulong exp_holdoff = DEFAULT_SRCU_EXP_HOLDOFF;
```

当两个 GP 间隔小于 25us 时，SRCU 自动转为加速模式，更快推进 GP。

**SRCU 大小模式**（`srcutree.c:51-62`）：

```c
// srcutree.c:51-62
#define SRCU_SIZING_NONE     0   // 永远不转大模式
#define SRCU_SIZING_INIT     1   // init_srcu_struct 时转
#define SRCU_SIZING_TORTURE  2   // rcutorture 请求时转
#define SRCU_SIZING_AUTO     3   // 启动时根据系统形状决定（默认）
#define SRCU_SIZING_CONTEND  0x10  // 遇到过多争用时转
```

**小模式 → 大模式转换**发生在 `big_cpu_lim = 128` 时（`srcutree.c:65`）：

```c
// srcutree.c:65-67
/* Number of CPUs to trigger init_srcu_struct()-time transition to big. */
static int big_cpu_lim __read_mostly = 128;
static int small_contention_lim __read_mostly = 100;  // 每 jiffy 100 次争用即转
```

转换状态机（`srcutree.h:118-135`）：

```
SRCU_SIZE_SMALL (0)
    ↓ __srcu_transition_to_big()  // srcutree.c:351-355
SRCU_SIZE_ALLOC (1)          分配 srcu_node 组合树
    ↓ smp_store_release()
SRCU_SIZE_WAIT_BARRIER (2)   srcu_barrier() 开始感知新树
    ↓
SRCU_SIZE_WAIT_CALL (3)      call_srcu() 开始使用新树
    ↓
SRCU_SIZE_WAIT_CBS1~CBS4(4~7)  初始化 ->srcu_have_cbs[] 和 ->srcu_data_have_cbs[]
    ↓
SRCU_SIZE_BIG (8)            完全使用组合树
```

**争用触发转换**（`srcutree.c:380-394`）：

```c
// srcutree.c:380-394
static void raw_spin_lock_irqsave_check_contention(struct srcu_struct *ssp)
{
    if (!SRCU_SIZING_IS_CONTEND() || ssp->srcu_sup->srcu_size_state)
        return;
    j = jiffies;
    if (ssp->srcu_sup->srcu_size_jiffies != j) {
        ssp->srcu_sup->srcu_size_jiffies = j;
        ssp->srcu_sup->srcu_n_lock_retries = 0;  // 每 jiffy 重置统计
    }
    if (++ssp->srcu_sup->srcu_n_lock_retries <= small_contention_lim)
        return;  // 未达阈值，继续小模式
    __srcu_transition_to_big(ssp);  // 触发转换！
}
```

### 2.5 启动早期回调链表

在 RCU 子系统完全初始化前，`call_srcu()` 注册的回调暂存到全局链表 `srcu_boot_list`（`srcutree.c:73`）：

```c
// srcutree.c:72-74
/* Early-boot callback-management, so early that no lock is required! */
static LIST_HEAD(srcu_boot_list);
static bool __read_mostly srcu_init_done;
```

### 2.6 核心 API

`init_srcu_struct`（`srcutree.c:304-309`）初始化 SRCU 域，调用 `init_srcu_struct_fields`（`srcutree.c:204`）完成内存分配和初始状态设置。

`call_srcu`（`srcutree.c:1467`）注册回调，`synchronize_srcu`（`srcutree.c:1574`）同步等待 GP 完成。

`cleanup_srcu_struct`（`srcutree.c:709`）释放 SRCU 域资源。

---

## 3. Tasks RCU — 基于自愿上下文切换的 RCU

### 3.1 设计动机

某些内核特性需要保证"所有任务都至少经历过一次自愿上下文切换"后才能释放资源。典型场景：

- **trampoline 回收**：BPF 程序 trampoline 可能在任意任务的内核栈上被执行，回收前必须确保所有任务都已离开 trampoline 代码路径。
- **追踪 / kprobes**：需要在追踪点安全移除后确保不再有任务正在使用追踪设施。

### 3.2 静默态定义

Tasks RCU 的静默态**完全不同于**经典 RCU：

```
经典 RCU:  preempt_disable() 区间之外 → 静默态
Tasks RCU: 自愿上下文切换后       → 静默态
           (t->on_rq == 0, 即任务不在 CPU 运行队列上)
```

没有 `rcu_read_lock_tasks()` / `rcu_read_unlock_tasks()` 这样的读端原语——这意味着 Tasks RCU **没有显式读端临界区**，也不需要。

### 3.3 核心数据结构

**回调函数类型**（`tasks.h:16-21`）：

```c
// tasks.h:16-21
typedef void (*rcu_tasks_gp_func_t)(struct rcu_tasks *rtp);
typedef void (*pregp_func_t)(struct list_head *hop);       // GP 前准备
typedef void (*pertask_func_t)(struct task_struct *t, struct list_head *hop);  // 逐任务扫描
typedef void (*postscan_func_t)(struct list_head *hop);    // 扫描后处理
typedef void (*holdouts_func_t)(struct list_head *hop, bool ndrpt, bool *frptp); // 持留任务检查
typedef void (*postgp_func_t)(struct rcu_tasks *rtp);      // GP 后处理
```

**percpu 数据**（`tasks.h:40-55`）：

```c
// tasks.h:40-55
struct rcu_tasks_percpu {
    struct rcu_segcblist cblist;             // 分段回调链表
    raw_spinlock_t __private lock;           // 保护回调链表
    unsigned long rtp_jiffies;               // 统计用 jiffies
    struct timer_list lazy_timer;            // 懒惰回调定时器
    unsigned int urgent_gp;                  // 非懒惰 GP 计数
    struct work_struct rtp_work;             // 回调调用工作项
    struct irq_work rtp_irq_work;            // IRQ 工作项（用于延迟唤醒）
    struct rcu_head barrier_q_head;          // 屏障操作的回调节点
    struct list_head rtp_blkd_tasks;         // 被阻塞的读端任务列表
    struct list_head rtp_exit_list;          // 正在 do_exit() 的任务列表
    int cpu;
    int index;
    struct rcu_tasks *rtpp;
};
```

**全局 Tasks RCU 结构**（`tasks.h:94-129`）：

```c
// tasks.h:94-129
struct rcu_tasks {
    struct rcuwait cbs_wait;                 // 等待回调
    raw_spinlock_t cbs_gbl_lock;             // 全局回调锁
    struct mutex tasks_gp_mutex;             // 保护 GP（启动期间需要）
    int gp_state;                            // GP 状态（调试用）
    int gp_sleep;                            // 每 GP 睡眠时间（防 CPU 死循环）
    int init_fract;                          // 初始退避间隔
    unsigned long gp_start;                  // GP 开始 jiffies
    unsigned long tasks_gp_seq;              // 完成的 GP 数量
    unsigned long n_ipis;                    // 发送的 IPI 数量
    unsigned long n_ipis_fails;              // IPI 发送失败次数
    struct task_struct *kthread_ptr;         // GP/回调调用 kthread
    unsigned long lazy_jiffies;              // 懒惰回调延迟 jiffies
    pregp_func_t pregp_func;                 // 各阶段函数指针
    pertask_func_t pertask_func;
    postscan_func_t postscan_func;
    holdouts_func_t holdouts_func;
    postgp_func_t postgp_func;
    call_rcu_func_t call_func;               // call_rcu 等价函数
    struct rcu_tasks_percpu __percpu *rtpcpu; // Per-CPU 数据
    char *name;                              // 变体名称
    char *kname;                             // kthread 名称
};
```

### 3.4 GP 状态机

Tasks RCU 的 GP 在 `rcu_tasks_wait_gp()`（`tasks.h:806`）中实现，是一个六阶段流水线：

```
         pregp_func          pertask_func        postscan_func
 ┌──────────▼────────┐  ┌────────▼───────┐  ┌───────▼────────┐
 │ RTGS_PRE_WAIT_GP  │→│RTGS_SCAN_TASKLIST│→│RTGS_POST_SCAN   │
 │ synchronize_rcu() │  │ 遍历所有任务     │  │ 处理退出任务     │
 │ 确保 nvcsw 转换   │  │ 标记 holdouts   │  │ 等待 do_exit    │
 └───────────────────┘  └────────────────┘  └────────────────┘
                                                      ↓
         postgp_func          holdouts_func     ┌───────────────┐
 ┌──────────▼────────┐  ┌────────▼───────┐     │RTGS_WAIT_SCAN │
 │ RTGS_POST_GP      │←│RTGS_SCAN_HOLDOUTS│←───│  _HOLDOUTS    │
 │ synchronize_rcu() │  │ 循环检查持留任务 │     │ 退避睡眠      │
 │ 最终内存屏障       │  │ 移除已完成的任务 │     │ 每次扫描      │
 └───────────────────┘  └────────────────┘     └───────────────┘
```

### 3.5 逐阶段详解

#### Phase 1: pregp — `rcu_tasks_pregp_step()`（`tasks.h:955`）

```c
// tasks.h:955-971
static void rcu_tasks_pregp_step(struct list_head *hop)
{
    // 等待所有在途的 t->on_rq 和 t->nvcsw 转换完成
    // synchronize_rcu() 已足够，因为所有这些转换都在关中断下进行
    synchronize_rcu();
}
```

这一步保证了 GP 开始前的所有状态转换已完成。

#### Phase 2: pertask — 逐任务扫描（`tasks.h:828-833`）

```c
// tasks.h:828-833 (in rcu_tasks_wait_gp)
if (rtp->pertask_func) {
    rcu_read_lock();
    for_each_process_thread(g, t)
        rtp->pertask_func(t, &holdouts);  // 检查每个任务
    rcu_read_unlock();
}
```

`rcu_tasks_pertask()`（`tasks.h:1009`）对每个任务检查 `t->on_rq` 状态，通过 `get_task_struct()` 钉住任务，并记录其自愿上下文切换次数。尚未自愿切换的任务加入 `holdouts` 链表。

#### Phase 3: postscan — 处理退出中的任务（`tasks.h:1023`）

`rcu_tasks_postscan()` 处理 `rtp_exit_list` 中的任务——这些任务正在 `do_exit()` 流程中，可能已经从全局任务列表移除，需要额外跟踪。

#### Phase 4: holdouts — 循环等待（`tasks.h:850-888`）

```c
// tasks.h:850-888 (simplified)
while (!list_empty(&holdouts)) {
    // 退避睡眠，间隔从 init_fract 逐渐增加到 HZ
    schedule_timeout_idle(fract);
    if (fract < HZ)
        fract++;

    // 调用 holdouts_func 检查并移除已完成的任务
    rtp->holdouts_func(&holdouts, needreport, &firstreport);
}
```

`check_all_holdout_tasks()` 遍历持留链表，检查每个任务是否：
- `t->on_rq == 0`（不在运行队列→已自愿睡眠）
- `is_idle_task(t)`（空闲任务→静默态）
- nvcsw 计数器已变化（发生过自愿上下文切换）

#### Phase 5: postgp — `rcu_tasks_postgp()`（`tasks.h:1131`）

```c
// tasks.h:1131-1138 (approx)
static void rcu_tasks_postgp(struct rcu_tasks *rtp)
{
    // 最后一次 synchronize_rcu() 确保所有转换可见
    synchronize_rcu();
}
```

#### Phase 6: 回调注册函数指针绑定（`tasks.h:1239-1243`）

```c
// tasks.h:1239-1243
rcu_tasks.pregp_func = rcu_tasks_pregp_step;
rcu_tasks.pertask_func = rcu_tasks_pertask;
rcu_tasks.postscan_func = rcu_tasks_postscan;
rcu_tasks.holdouts_func = check_all_holdout_tasks;
rcu_tasks.postgp_func = rcu_tasks_postgp;
```

### 3.6 与经典 RCU 的关键区别

```
经典 RCU:                    Tasks RCU:
- detect preempt_disable()   - detect voluntary ctx switch
- ~ms 级 GP 延迟              - 可能 ~秒级 GP 延迟
- 有显式 read-side CS        - 无显式 read-side CS
- 支持高频率 call_rcu        - 不支持高频率 call_rcu_tasks
- 基于 rcu_node 组合树        - 基于全局任务链表遍历
```

---

## 4. Tiny RCU — Bloatwatch 版

Tiny RCU 专为**单处理器 (UP)** 和小型嵌入式系统设计，仅 **252 行代码**——对比 Tree RCU 的 ~5000 行。

### 4.1 核心数据结构

```c
// tiny.c:31-36
struct rcu_ctrlblk {
    struct rcu_head *rcucblist;   // 待处理回调链表（单链表）
    struct rcu_head **donetail;   // "已处理"回调节点的 ->next 指针
    struct rcu_head **curtail;    // 最后一个回调节点的 ->next 指针
    unsigned long gp_seq;         // GP 计数（每次 +2）
};

// tiny.c:39-43 静态初始化
static struct rcu_ctrlblk rcu_ctrlblk = {
    .donetail = &rcu_ctrlblk.rcucblist,
    .curtail  = &rcu_ctrlblk.rcucblist,
    .gp_seq   = 0 - 300UL,  // 初始值为 -300，避免早期竞争
};
```

### 4.2 单链表回调管理

Tiny RCU 没有 `rcu_data`，没有 `rcu_node` 树，只有**一个全局回调链表**，用 `donetail` 和 `curtail` 两个尾指针分割：

```
rcucblist ───► CB₁ ───► CB₂ ───► NULL
                ↑               ↑
             donetail        curtail
             
[已完成的回调]  [正在等待下一次 GP 的回调]
```

### 4.3 静默态检测：`rcu_qs()`（`tiny.c:52-63`）

```c
// tiny.c:52-63
void rcu_qs(void)
{
    unsigned long flags;
    local_irq_save(flags);
    if (rcu_ctrlblk.donetail != rcu_ctrlblk.curtail) {
        // 有新回调，推进 donetail，触发 softirq 处理
        rcu_ctrlblk.donetail = rcu_ctrlblk.curtail;
        raise_softirq_irqoff(RCU_SOFTIRQ);
    }
    WRITE_ONCE(rcu_ctrlblk.gp_seq, rcu_ctrlblk.gp_seq + 2);  // GP 序号 +2
    local_irq_restore(flags);
}
```

**每次 GP 序号 +2**：低一位用于标识 GP 状态（奇偶），高 31 位才是真正的 GP 序号。

### 4.4 时钟中断入口：`rcu_sched_clock_irq()`（`tiny.c:71-77`）

```c
// tiny.c:71-77
void rcu_sched_clock_irq(int user)
{
    if (user)
        rcu_qs();      // 用户态直接判定为静默态
    else if (rcu_ctrlblk.donetail != rcu_ctrlblk.curtail)
        set_need_resched_current();  // 内核态但有待处理回调，强制调度
}
```

在 UP 系统中逻辑极简：
- **用户态** → 必定不是临界区 → 直接推进 GP
- **内核态有回调** → 设置 `TIF_NEED_RESCHED` 触发调度，调度本身就是静默态

### 4.5 `synchronize_rcu()` — 几乎什么都不用做（`tiny.c:141-151`）

```c
// tiny.c:141-151
void synchronize_rcu(void)
{
    // 检查是否在非法上下文中调用
    RCU_LOCKDEP_WARN(...);
    preempt_disable();
    WRITE_ONCE(rcu_ctrlblk.gp_seq, rcu_ctrlblk.gp_seq + 2);
    preempt_enable();
}
```

**UP 系统上 `synchronize_rcu()` 几乎为空操作**：在单处理器上，任何调用 `synchronize_rcu()` 的代码路径本身必然不在 RCU 读端临界区内（否则违反 RCU 协议），因此调用点自身就是静默态。只需推进 `gp_seq` 让 poll API 感知到 GP 已完成。

### 4.6 `call_rcu()`（`tiny.c:158-184`）

```c
// tiny.c:158-184
void call_rcu(struct rcu_head *head, rcu_callback_t func)
{
    // Double-free 检测
    head->func = func;
    head->next = NULL;

    local_irq_save(flags);
    *rcu_ctrlblk.curtail = head;         // 追加到 curtail
    rcu_ctrlblk.curtail = &head->next;   // 推进 curtail
    local_irq_restore(flags);

    if (unlikely(is_idle_task(current))) {
        resched_cpu(0);  // 空闲任务：强制调度以触发 rcu_qs()
    }
}
```

### 4.7 回调处理：`rcu_process_callbacks()`（`tiny.c:99-127`）

```c
// tiny.c:99-127
static __latent_entropy void rcu_process_callbacks(void)
{
    // donetail 之前的回调全部已完成 GP，移动到本地链表
    list = rcu_ctrlblk.rcucblist;
    rcu_ctrlblk.rcucblist = *rcu_ctrlblk.donetail;
    *rcu_ctrlblk.donetail = NULL;
    if (rcu_ctrlblk.curtail == rcu_ctrlblk.donetail)
        rcu_ctrlblk.curtail = &rcu_ctrlblk.rcucblist;
    rcu_ctrlblk.donetail = &rcu_ctrlblk.rcucblist;

    // 逐一调用已完成回调
    while (list) {
        next = list->next;
        prefetch(next);
        rcu_reclaim_tiny(list);
        list = next;
    }
}
```

### 4.8 `rcu_barrier()`（`tiny.c:45-48`）

```c
// tiny.c:45-49
void rcu_barrier(void)
{
    wait_rcu_gp(call_rcu_hurry);  // 注册回调并等待其完成
}
```

### 4.9 `rcu_init()`（`tiny.c:247-252`）

```c
// tiny.c:247-252
void __init rcu_init(void)
{
    open_softirq(RCU_SOFTIRQ, rcu_process_callbacks);  // 注册 softirq 处理函数
    rcu_early_boot_tests();
    tasks_cblist_init_generic();  // 初始化 Tasks RCU（即使 Tiny 也包含）
}
```

---

## 5. 四大 RCU 变体对比

| 特性 | Tree RCU | SRCU | Tasks RCU | Tiny RCU |
|------|----------|------|-----------|----------|
| 读端可睡眠 | ❌ | ✅ | N/A（无读端 CS） | ❌ |
| 等待自愿 ctx 切换 | ❌ | ❌ | ✅ | ❌ |
| 支持 SMP | ✅ | ✅ | ✅ | ❌（UP only） |
| 组合树结构 | rcu_node 树 | srcu_node 树 | 无（任务链表遍历） | 无（单链表） |
| GP 检测机制 | preempt_disable + HZ tick | 双计数器 idx 切换 | 自愿上下文切换 | 用户态/调度 |
| 典型 GP 延迟 | ~10-100ms | ~10-100ms | ~100ms-数秒 | ~1 tick |
| 显式读端原语 | rcu_read_lock/unlock | srcu_read_lock/unlock | 无 | rcu_read_lock/unlock |
| 代码行数 | ~5000 | ~2200 | ~1608 | ~252 |
| 多实例支持 | 全局单例 | ✅ 多个 srcu_struct | 全局单例 | 全局单例 |
| 适用场景 | 通用 | 设备驱动、DM、FS | BPF trampoline、追踪 | UP 嵌入式 |
| 首发版本 | 2.6 | 2.6 | 4.9 | 2.6.28 |

### 5.1 对比细节

**SRCU 多实例化**：
```
用户可定义多个 srcu_struct，每个独立管理自己的 GP。
例如文件系统 A 和 B 各自维护一个 SRCU 域，互不干扰。
DEFINE_SRCU(fs_a_srcu);
DEFINE_SRCU(fs_b_srcu);
```

**Tasks RCU 没有读端**：
```
这是 Tasks RCU 最反直觉的特性。因为 tasks RCU 的静默态
是"自愿上下文切换"，所以不需要 rcu_read_lock_tasks()。
这意味着任何代码路径都可能被 Tasks RCU 要求"等一下"。
```

**Tiny RCU 的 `gp_seq += 2` 设计**：
```
gp_seq 的最低位用作 GP 初态/终态标识：
  位 0 = 0 → GP 空闲/已结束
  位 0 = 1 → GP 进行中
因此每次推进 GP 序列化两个状态变化：开始(+1) + 结束(+1) = +2
```

---

## 下一篇文章

[第四篇：RCU 诊断 — 拖延检测、加速 GP 与回调卸载](./04-diagnostics.md)
