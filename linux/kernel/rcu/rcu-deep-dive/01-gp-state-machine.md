# 第一篇：RCU 核心 — Grace Period 状态机与合并树

> 源码：`kernel/rcu/tree.c` (4931 lines) | 头文件：`kernel/rcu/tree.h` (547 lines)
> 简化实现参考：`kernel/rcu/tiny.c` (252 lines)

系列目录：[RCU 内核源码深度解析](./README.md)

## 1. 什么是 RCU

RCU 是一种读侧零开销的同步机制。核心约定：

- **读者（Reader）**：不阻塞，`rcu_read_lock()` 和 `rcu_read_unlock()` 之间是读侧临界区。
- **写者（Writer）**：调用 `synchronize_rcu()` 等待所有**已存在的**读临界区结束，然后安全回收旧数据。
- **回调（Callback）**：`call_rcu()` 注册一个函数，在宽限期结束后被调用（用于延迟释放）。

关键洞察：读者从不阻塞写者，写者等待的是读者**离开**临界区，而非进入。

```
   reader:        ───[read_lock]────[临界区]────[read_unlock]───
   writer:   ──────────────────────[synchronize_rcu() 阻塞]─────────[free old data]───
                                      ↑ 等待这个读临界区结束
```

在 UP 系统上 `synchronize_rcu()` 几乎什么都不做——因为读者不抢占，`synchronize_rcu()` 本身就是一个静默状态（见 `tiny.c:141-150`）。而 SMP 系统则需要完整的宽限期（Grace Period, GP）机制。

## 2. 核心数据结构纵览

```
   rcu_state (全局唯一)
   ├── gp_seq         ── 宽限期序列号
   ├── gp_flags       ── GP kthread 命令标志
   ├── gp_state       ── 当前状态机状态
   ├── node[]         ── rcu_node 树形数组
   └── level[]        ── 指向每层起始节点的指针
        │
        ▼
   rcu_node[0] (Root, level=0)
   ├── qsmask         ── 哪些子节点还未报告 QS
   ├── gp_tasks       ── 阻塞当前 GP 的任务链表
   ├── blkd_tasks     ── 所有阻塞任务
   └── parent = NULL
        │
    ┌───┴───────────┐
    ▼               ▼
   rcu_node[1]    rcu_node[2]  (level=1)
   │               │
   ├── qsmask      ├── qsmask
   └── children ──→ rcu_data per-CPU (leaf)
        │               │
        ▼               ▼
    rcu_data[0..3]  rcu_data[4..7]  (per-CPU)
    ├── gp_seq      ├── cpu_no_qs
    ├── cblist      ├── core_needs_qs
    └── mynode      └── grpmask
```

### 2.1 struct rcu_state — 全局 RCU 状态

**定义位置**：`tree.h:351-438`

```c
// tree.h:351
struct rcu_state {
    struct rcu_node node[NUM_RCU_NODES];   // 树形数组, "堆"形式密集存储
    struct rcu_node *level[RCU_NUM_LVLS + 1]; // 每层起始节点指针

    unsigned long gp_seq;                  // 宽限期序列号 (tree.h:361)
    struct task_struct *gp_kthread;        // GP 内核线程 (tree.h:365)
    short gp_flags;                        // GP kthread 命令标志 (tree.h:367)
    short gp_state;                        // GP 状态机状态 (tree.h:368)

    unsigned long jiffies_force_qs;        // 强制 QS 的时间 (tree.h:395)
    unsigned long jiffies_stall;           // CPU stall 检测时间 (tree.h:409)

    struct mutex barrier_mutex;            // rcu_barrier() 互斥锁 (tree.h:377)
    atomic_t barrier_cpu_count;            // barrier 等待 CPU 计数
    struct completion barrier_completion;  // barrier 完成信号

    struct mutex exp_mutex;                // 加速 GP 互斥锁 (tree.h:386)
    atomic_t expedited_need_qs;            // 加速 GP 剩余 CPU 计数 (tree.h:389)

    // synchronize_rcu() 部分
    struct llist_head srs_next;            // 请求 GP 的用户链表 (tree.h:427)
    struct work_struct srs_cleanup_work;   // 清理工作 (tree.h:431)
};
```

全局实例在 `tree.c:92`：
```c
// tree.c:92-100
static struct rcu_state rcu_state = {
    .level = { &rcu_state.node[0] },
    .gp_state = RCU_GP_IDLE,
    .gp_seq = (0UL - 300UL) << RCU_SEQ_CTR_SHIFT,
    .barrier_mutex = __MUTEX_INITIALIZER(rcu_state.barrier_mutex),
    .name = RCU_NAME,
    .abbr = RCU_ABBR,
};
```

`gp_seq` 初始为 `(0UL - 300UL) << 2`，即从一个很大的负偏置开始，防止早期启动阶段的计数器环绕问题。

### 2.2 gp_flags — GP kthread 命令

**定义位置**：`tree.h:441-443`

```c
#define RCU_GP_FLAG_INIT 0x1   // 需要初始化新宽限期
#define RCU_GP_FLAG_FQS  0x2   // 需要强制静默状态
#define RCU_GP_FLAG_OVLD 0x4   // 回调过载
```

kthread 在 `gp_wq` 上睡眠，当这些标志位被设置时被唤醒。例如 `call_rcu()` 路径会设置 `RCU_GP_FLAG_INIT`，然后通过 `rcu_gp_kthread_wake()` 唤醒 GP kthread。

### 2.3 gp_state — 9 状态状态机

**定义位置**：`tree.h:446-454`

```c
#define RCU_GP_IDLE      0  // 初始状态，无 GP 进行中
#define RCU_GP_WAIT_GPS  1  // 等待宽限期启动
#define RCU_GP_DONE_GPS  2  // 宽限期启动完成
#define RCU_GP_ONOFF     3  // 热插拔初始化（处理 CPU 上线/离线）
#define RCU_GP_INIT       4  // 宽限期初始化中
#define RCU_GP_WAIT_FQS  5  // 等待强制静默状态时间
#define RCU_GP_DOING_FQS 6  // 执行强制静默状态
#define RCU_GP_CLEANUP   7  // 宽限期清理中
#define RCU_GP_CLEANED   8  // 宽限期清理完成
```

状态流转图：

```
                        ┌──────────────────────────────────────────┐
                        │                                          │
                        ▼                                          │
  IDLE(0) ──► WAIT_GPS(1) ──► DONE_GPS(2) ──► ONOFF(3) ──► INIT(4)
    ▲                                                             │
    │                                                             ▼
    │                              ┌── WAIT_FQS(5) ◄──► DOING_FQS(6)
    │                              │        │
    │                              │   (所有 CPU QS 完成)
    │                              │        │
    │                              │        ▼
    │                              └── CLEANUP(7) ──► CLEANED(8)
    │
    └──────────────────────────────────────────────────┘
```

## 3. struct rcu_node — 合并树节点

**定义位置**：`tree.h:41-139`

```c
// tree.h:41-139
struct rcu_node {
    raw_spinlock_t __private lock;     // 节点自旋锁
    unsigned long gp_seq;              // 本节点感知的 GP 序列号 (tree.h:45)
    unsigned long completedqs;         // 本节点所有 QS 完成 (tree.h:47)
    unsigned long qsmask;              // 需要切换的 CPU/子组掩码 (tree.h:48)
    unsigned long qsmaskinit;          // 每个 GP 的 qsmask 初始值 (tree.h:55)
    unsigned long qsmaskinitnext;      // 在线 CPU，供下一个 GP 使用 (tree.h:59)

    unsigned long expmask;             // 加速 GP 等待的 CPU 掩码 (tree.h:60)

    unsigned long grpmask;             // 在父节点 qsmask 中对应的位 (tree.h:77)
    int grplo;                         // 本节点最低 CPU 编号 (tree.h:79)
    int grphi;                         // 本节点最高 CPU 编号 (tree.h:80)
    u8 grpnum;                         // 在上一层的组号 (tree.h:81)
    u8 level;                          // 层级，根节点 level=0 (tree.h:82)

    struct rcu_node *parent;           // 父节点 (tree.h:87)

    struct list_head blkd_tasks;       // 在 RCU 读临界区阻塞的任务链表 (tree.h:88)
    struct list_head *gp_tasks;        // 阻塞当前 GP 的第一个任务 (tree.h:92)
    struct list_head *exp_tasks;       // 阻塞当前加速 GP 的第一个任务 (tree.h:96)
    struct list_head *boost_tasks;     // 需要优先级提升的任务 (tree.h:102)
    struct rt_mutex boost_mtx;         // 优先级提升互斥锁 (tree.h:110)

    unsigned long boost_time;          // 何时开始优先级提升 (tree.h:113)
};
```

### 3.1 合并树的工作原理

每个 leaf `rcu_node` 覆盖 `[grplo, grphi]` 范围的 CPU。当 leaf 节点上所有 CPU 都报告了静默状态（`qsmask` 清零），leaf 节点清除父节点的对应 `qsmask` 位。这层层向上传播，直到 root 节点的 `qsmask` 清零，表明整个系统的 GP 完成。

```
   8 CPUs, fanout=4, RCU_FANOUT_LEAF=2 的合并树示例:

   Level 0 (root):        rcu_node[0]
                         qsmask: 0b11
                        /           \
   Level 1 (internal):  rcu_node[1]   rcu_node[2]
                       qsmask: 0b11   qsmask: 0b11
                      /      \        /      \
   Level 2 (leaf): rcu_node[3] [4]  [5]     [6]
                   grplo=0,1   2,3   4,5    6,7
                   qsmask: bitmap of child CPUs not yet QS

   rcu_node 数组布局 (node[]):
   [0:root] [1:internal] [2:internal] [3:leaf_0..1] [4:leaf_2..3] [5:leaf_4..5] [6:leaf_6..7]
```

`qsmask` 在 leaf 节点中每位对应一个 CPU，在内部节点中每位对应一个子 `rcu_node`。

### 3.2 leaf_node_cpu_bit 宏

```c
// tree.h:146
#define leaf_node_cpu_bit(rnp, cpu) (BIT((cpu) - (rnp)->grplo))
```

例如 CPU 5 在 leaf 节点 `grplo=4` 上，其位为 `BIT(5-4) = BIT(1) = 0x2`。

## 4. struct rcu_data — Per-CPU 数据

**定义位置**：`tree.h:189-297`

```c
// tree.h:189
struct rcu_data {
    /* 1) 静默状态和宽限期处理 */
    unsigned long gp_seq;              // 感知的 GP 序列号 (tree.h:191)
    unsigned long gp_seq_needed;       // 需要的下一个 GP (tree.h:192)
    union rcu_noqs cpu_no_qs;          // 尚未 QS (tree.h:193)
    bool core_needs_qs;                // 核心等待静默状态 (tree.h:194)
    struct rcu_node *mynode;           // 本 CPU 的 leaf rcu_node (tree.h:199)
    unsigned long grpmask;             // 在 leaf qsmask 中的位 (tree.h:200)
    unsigned long ticks_this_gp;       // 本 GP 中的时钟滴答数 (tree.h:201)

    /* 2) 回调批处理 */
    struct rcu_segcblist cblist;       // 分段回调链表 (tree.h:210)
    long qlen_last_fqs_check;          // 上次 FQS 检查的回调数 (tree.h:213)
    unsigned long n_cbs_invoked;       // 启动以来调用的回调数 (tree.h:215)

    /* 3) dynticks 接口 */
    int watching_snap;                 // 每个 GP 的 dynticks 跟踪 (tree.h:221)
    bool rcu_need_heavy_qs;            // GP 老化，需要重型 QS (tree.h:222)
    bool rcu_urgent_qs;                // GP 老化，需要轻型 QS (tree.h:223)

    /* 5) 回调卸载 (NOCB) */
#ifdef CONFIG_RCU_NOCB_CPU
    struct swait_queue_head nocb_cb_wq; // NOCB kthread 睡眠 (tree.h:234)
    struct rcu_cblist nocb_bypass;      // 锁竞争绕行列表 (tree.h:246)
    // ... 更多 NOCB 字段
#endif

    /* 6) RCU 优先级提升 */
    struct task_struct *rcu_cpu_kthread_task; // rcuc 每 CPU 线程 (tree.h:274)

    /* 7) 诊断数据 */
    unsigned int softirq_snap;         // softirq 活动快照 (tree.h:281)
    unsigned long last_sched_clock;    // 上次调度时钟 (tree.h:291)
    struct rcu_snap_record snap_record; // CPU stall 诊断快照 (tree.h:292)

    int cpu;                           // CPU 编号 (tree.h:296)
};
```

全系统 `rcu_data` 定义在 `tree.c:80`：
```c
// tree.c:80-82
static DEFINE_PER_CPU_SHARED_ALIGNED(struct rcu_data, rcu_data) = {
    .gpwrap = true,
};
```

## 5. rcu_gp_kthread — GP 状态机的主循环

**定义位置**：`tree.c:2271`

这是 RCU 的心脏——一个永不返回的内核线程，循环驱动整个宽限期状态机。

```c
// tree.c:2271-2304
static int __noreturn rcu_gp_kthread(void *unused)
{
    rcu_bind_gp_kthread();
    for (;;) {

        /* 处理宽限期启动 */
        for (;;) {
            // ===== 状态: IDLE → WAIT_GPS =====
            WRITE_ONCE(rcu_state.gp_state, RCU_GP_WAIT_GPS);  // line 2280
            swait_event_idle_exclusive(rcu_state.gp_wq,
                         READ_ONCE(rcu_state.gp_flags) &
                         RCU_GP_FLAG_INIT);                    // line 2281-2283
            // 被唤醒：有人请求了新的 GP

            // ===== 状态: WAIT_GPS → DONE_GPS =====
            WRITE_ONCE(rcu_state.gp_state, RCU_GP_DONE_GPS);  // line 2285

            // 尝试初始化 GP，如果成功则跳出内层循环
            if (rcu_gp_init())                                 // line 2287
                break;
            // 初始化失败（虚假唤醒或 GP 已在进展中），继续等待
            cond_resched_tasks_rcu_qs();
            WRITE_ONCE(rcu_state.gp_activity, jiffies);
        }

        /* 处理强制静默状态 */
        rcu_gp_fqs_loop();                                     // line 2297

        /* 处理宽限期结束 */
        // ===== 状态: WAIT_FQS → CLEANUP =====
        WRITE_ONCE(rcu_state.gp_state, RCU_GP_CLEANUP);        // line 2300
        rcu_gp_cleanup();                                      // line 2301

        // ===== 状态: CLEANUP → CLEANED → 循环回 WAIT_GPS =====
        WRITE_ONCE(rcu_state.gp_state, RCU_GP_CLEANED);        // line 2302
    }
}
```

### 5.1 状态转换详解

| 行号 | 转换 | 触发条件 | 含义 |
|------|------|----------|------|
| 2280 | IDLE→WAIT_GPS | `RCU_GP_FLAG_INIT` 被设置 | kthread 等待新 GP 请求 |
| 2285 | WAIT_GPS→DONE_GPS | swait 被唤醒 | 准备初始化 |
| 1885 | DONE_GPS→ONOFF | rcu_gp_init() 内部 | 处理 CPU 热插拔变化 |
| 1953 | ONOFF→INIT | 热插拔处理完毕 | 初始化所有 rcu_node |
| 2094 | INIT→WAIT_FQS | 进入 rcu_gp_fqs_loop() | 等待/执行强制 QS |
| 2098 | WAIT_FQS→DOING_FQS | 超时或强制触发 | 执行强制 QS 检查 |
| 2300 | →CLEANUP | 所有 CPU 报告 QS | 清理 GP，推进 gp_seq |
| 2302 | CLEANUP→CLEANED | rcu_gp_cleanup() 完成 | 标记 GP 结束 |
| 2280 | CLEANED→WAIT_GPS | for(;;) 循环 | 等待下一个 GP |

## 6. rcu_gp_init() — GP 初始化

**定义位置**：`tree.c:1804`

```c
// tree.c:1804-1998
static noinline_for_stack bool rcu_gp_init(void)
{
    unsigned long flags;
    struct rcu_node *rnp = rcu_get_root();

    raw_spin_lock_irq_rcu_node(rnp);
    if (!rcu_state.gp_flags) {
        // 虚假唤醒，返回 false 让 kthread 继续等待
        raw_spin_unlock_irq_rcu_node(rnp);
        return false;                                        // line 1819
    }
    WRITE_ONCE(rcu_state.gp_flags, 0); // 清除所有标志，开始新 GP  // line 1821

    if (WARN_ON_ONCE(rcu_gp_in_progress())) {
        // GP 已在进展中（不应发生）
        raw_spin_unlock_irq_rcu_node(rnp);
        return false;                                        // line 1829
    }

    // 推进 gp_seq，记录新 GP 的开始
    rcu_seq_start(&rcu_state.gp_seq);                        // line 1853
    trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq, TPS("start"));

    // ===== ONOFF 阶段: 处理 CPU 热插拔 =====
    WRITE_ONCE(rcu_state.gp_state, RCU_GP_ONOFF);            // line 1885
    rcu_for_each_leaf_node(rnp) {
        arch_spin_lock(&rcu_state.ofl_lock);                 // 与 CPU 离线串行化
        raw_spin_lock_rcu_node(rnp);
        if (rnp->qsmaskinit == rnp->qsmaskinitnext &&
            !rnp->wait_blkd_tasks) {
            // 此 leaf 节点无变化
            raw_spin_unlock_rcu_node(rnp);
            arch_spin_unlock(&rcu_state.ofl_lock);
            continue;
        }
        // 将 qsmaskinitnext (新的在线 CPU 集合) 应用到 qsmaskinit
        oldmask = rnp->qsmaskinit;
        rnp->qsmaskinit = rnp->qsmaskinitnext;               // line 1906
        // ... 处理 CPU 首次上线/最后离线/阻塞任务传播
        raw_spin_unlock_rcu_node(rnp);
        arch_spin_unlock(&rcu_state.ofl_lock);
    }

    // ===== INIT 阶段: 初始化所有 rcu_node =====
    WRITE_ONCE(rcu_state.gp_state, RCU_GP_INIT);             // line 1953
    rcu_for_each_node_breadth_first(rnp) {
        raw_spin_lock_irqsave_rcu_node(rnp, flags);
        rnp->qsmask = rnp->qsmaskinit;                       // 设置需要 QS 的 CPU 集
        WRITE_ONCE(rnp->gp_seq, rcu_state.gp_seq);           // 同步 gp_seq
        // ... 处理离线 CPU 的 QS 报告
        mask = rnp->qsmask & ~rnp->qsmaskinitnext;
        rnp->rcu_gp_init_mask = mask;
        if ((mask || rnp->wait_blkd_tasks) && rcu_is_leaf_node(rnp))
            rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags); // line 1976
        else
            raw_spin_unlock_irq_rcu_node(rnp);
    }

    // GP kthread 自身的 CPU 立即报告 QS（它不可能在读临界区）
    preempt_disable();
    rcu_qs();
    rcu_report_qs_rdp(this_cpu_ptr(&rcu_data));              // line 1995
    preempt_enable();

    return true;                                             // line 1998
}
```

关键点：
- `gp_seq` 使用 `rcu_seq_start()` 推进，这是一个 2 位低位的序列值操作
- `rcu_seq_ctr_shift` 为 2，所以 gp_seq 的低 2 位编码状态，高位编码计数器
- 离线 CPU 在 GP 启动时立即被标记为 QS 完成（它们不可能在读临界区）
- GP kthread 本身的 CPU 总是第一个 QS

## 7. rcu_gp_fqs_loop() — 强制静默状态循环

**定义位置**：`tree.c:2064`

```c
// tree.c:2064-2145
static noinline_for_stack void rcu_gp_fqs_loop(void)
{
    bool first_gp_fqs = true;
    int gf = 0;
    unsigned long j;
    struct rcu_node *rnp = rcu_get_root();

    j = READ_ONCE(jiffies_till_first_fqs);
    for (;;) {
        // 设置下次强制 QS 的时间
        WRITE_ONCE(rcu_state.jiffies_force_qs, jiffies + j); // line 2083

        // ===== WAIT_FQS: 等待超时或被唤醒 =====
        WRITE_ONCE(rcu_state.gp_state, RCU_GP_WAIT_FQS);     // line 2094
        (void)swait_event_idle_timeout_exclusive(rcu_state.gp_wq,
                     rcu_gp_fqs_check_wake(&gf), j);         // line 2095-2096

        // ===== DOING_FQS: 检查 GP 是否已完成 =====
        WRITE_ONCE(rcu_state.gp_state, RCU_GP_DOING_FQS);    // line 2098

        // 如果 root rcu_node 的 qsmask 清零且没有阻塞的读者，GP 完成
        if (!READ_ONCE(rnp->qsmask) &&
            !rcu_preempt_blocked_readers_cgp(rnp))
            break;                                           // line 2109-2111

        // 是时候强制 QS 了
        if (!time_after(rcu_state.jiffies_force_qs, jiffies) ||
            (gf & (RCU_GP_FLAG_FQS | RCU_GP_FLAG_OVLD))) {
            rcu_gp_fqs(first_gp_fqs);                        // line 2117
            gf = 0;
            if (first_gp_fqs) {
                first_gp_fqs = false;
                gf = rcu_state.cbovld ? RCU_GP_FLAG_OVLD : 0;
            }
            j = READ_ONCE(jiffies_till_next_fqs);            // line 2128
        } else {
            // 虚假信号，保持旧的 FQS 计时
            ret = 1;
            j = jiffies;
            // ...
        }
    }
}
```

循环逻辑：
1. 设置 `jiffies_force_qs` 为未来的时间点
2. 在 `gp_wq` 上等待超时（`jiffies_till_first_fqs` 大约是 1~3 个 jiffy）
3. 超时后检查：root `qsmask` 是否清零？
   - 是且无阻塞任务 → GP 完成，退出循环
   - 否 → 调用 `rcu_gp_fqs()` 强制 QS（发送 IPI 或设置 `rcu_urgent_qs`）
4. 下一个超时用 `jiffies_till_next_fqs`（可能更长，以避免 IPI 风暴）
5. 回调过载模式下（`RCU_GP_FLAG_OVLD`），间隔缩短为 `(j+2)/3`

## 8. 静默状态检测机制

### 8.1 Tiny RCU 中的 QS 检测

**定义位置**：`tiny.c:52-77`

```c
// tiny.c:52-63
void rcu_qs(void)
{
    unsigned long flags;

    local_irq_save(flags);
    if (rcu_ctrlblk.donetail != rcu_ctrlblk.curtail) {
        // 有新的回调：将 curtail "提交" 到 donetail
        rcu_ctrlblk.donetail = rcu_ctrlblk.curtail;
        raise_softirq_irqoff(RCU_SOFTIRQ);                   // 触发回调处理
    }
    WRITE_ONCE(rcu_ctrlblk.gp_seq, rcu_ctrlblk.gp_seq + 2);  // 推进 gp_seq
    local_irq_restore(flags);
}

// tiny.c:71-77
void rcu_sched_clock_irq(int user)
{
    if (user)
        rcu_qs();          // 用户模式 = 静默状态
    else if (rcu_ctrlblk.donetail != rcu_ctrlblk.curtail)
        set_need_resched_current();  // 内核模式但有回调，设置需要调度
}
```

Tiny RCU 的 QS 检测极其简单：每次调度时钟中断时，如果 CPU 在用户模式（`user=1`），直接调用 `rcu_qs()`。`rcu_qs()` 推进 `gp_seq`（每次 +2，跳过低位状态编码），并将待处理回调区段 "提交"。

### 8.2 Tree RCU 中的 QS 检测

Tree RCU 不像 Tiny RCU 那样在 tick 中"立即"推进 GP。它是一个**异步三级检测机制**：tick 做轻量检测 → 上下文切换做标记 → GP kthread 汇总推进。

#### 8.2.1 怎么才算"度过静默期"——三条路径

一个 CPU "度过静默期"意味着它**已经不在 RCU 读临界区中**。Tree RCU 检测这一点有三个入口：

```
① 调度时钟中断 (tick):
   rcu_sched_clock_irq(user)              ← tree.c:2696
   如果 user=1（用户态）或 idle → 这个 tick 期间 CPU 一定不在读临界区
     → rcu_note_voluntary_context_switch(current)  ← 记录"度过了 QS"

② 上下文切换:
   rcu_note_context_switch()              ← kernel/sched/core.c
   任何 CPU 发生 context switch → 之前的进程如果持了 rcu_read_lock
   也会随 switch 被打断 → CPU 离开读临界区
     → rcu_qs()                            ← tree.c:1994 (softirq中)
     → rcu_report_qs_rdp(this_cpu_ptr)     ← tree.c:1995 上报

③ GP kthread 主动逼迫 (force QS):
   如果某个 CPU 长时间不报告 QS（既不 tick 用户态，也不切换上下文）
   可能是内核态死循环或 dyntick idle CPU
     → force_qs_rnp(rcu_watching_snap_save)  ← tree.c:2732
     → 向目标 CPU 发送 resched IPI
     → 目标 CPU 收到后必须调度 → 走路径②
```

#### 8.2.2 详细：rcu_sched_clock_irq — tick 中的 QS 检测

```c
// tree.c:2696-2728 — 调度时钟中断的 RCU 入口
void rcu_sched_clock_irq(int user)
{
    // 计数器：这次 GP 的第几个 tick
    raw_cpu_inc(rcu_data.ticks_this_gp);                         // line 2708

    // ★ 关键：urgent_qs 标志处理
    if (smp_load_acquire(this_cpu_ptr(&rcu_data.rcu_urgent_qs))) {
        // GP kthread 设置了 urgent QS 需求 → 加速退出读临界区
        if (!rcu_is_cpu_rrupt_from_idle() && !user) {
            // 不在 idle、不在用户态 → 可能在内核态读临界区
            // 设置 need_resched 标志 → 下一次 preempt_enable/中断返回时触发调度
            set_need_resched_current();                           // line 2713
        }
        __this_cpu_write(rcu_data.rcu_urgent_qs, false);         // line 2714
    }

    // 调用 flavors（RCU/RUDP/TREE）
    rcu_flavor_sched_clock_irq(user);                            // line 2717

    // 检查是否有 pending 的 RCU 工作需要做
    if (rcu_pending(user))                                       // line 2719
        invoke_rcu_core();                                       // line 2720 → 触发 RCU_SOFTIRQ

    // ★ 如果 CPU 在用户态或 idle 中断中 → 立即标记为 QS
    if (user || rcu_is_cpu_rrupt_from_idle())                    // line 2721
        rcu_note_voluntary_context_switch(current);              // line 2722
}
```

#### 8.2.3 详细：rcu_report_qs_rdp — 将 QS 上报到合并树

当 CPU 知道自己度过了静默期（通过上述三条路径之一），它调用 `rcu_report_qs_rdp` 将这一事实传播到合并树：

```c
// tree.c:2443-2486 — per-CPU QS 上报
void rcu_report_qs_rdp(struct rcu_data *rdp)
{
    rnp = rdp->mynode;                          // 获取本 CPU 所属的叶 rcu_node

    // 检查 gp_seq 是否匹配——如果 CPU 的 gp_seq 已经落后 rnp->gp_seq，
    // 说明本 GP 已经过去了，这个 QS 属于"旧"GP，不能上报
    if (rdp->cpu_no_qs.b.norm || rdp->gp_seq != rnp->gp_seq || rdp->gpwrap) {
        rdp->cpu_no_qs.b.norm = true;          // 标记需要为"新"GP 重新检测 QS
        return;                                  // 不上报
    }

    mask = rdp->grpmask;                        // 此 CPU 在叶节点的位掩码
    rdp->core_needs_qs = false;                 // 清除"需要 QS" 标志

    // ★ 检查叶 rcu_node 的 qsmask 中此 CPU 位是否还在
    if ((rnp->qsmask & mask) == 0) {
        // 已经被清过了（其它路径已经报告了这个 CPU 的 QS）
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
    } else {
        // ★ 调用 rcu_report_qs_rnp 向上游传播 QS 清除
        rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);  // line 2486
    }
}
```

#### 8.2.4 详细：rcu_report_qs_rnp — 沿合并树上溯

```c
// tree.c:2339-2393 — 沿合并树向上传播 QS
static void rcu_report_qs_rnp(unsigned long mask, struct rcu_node *rnp,
                              unsigned long gps, unsigned long flags)
{
    for (;;) {
        // ① 清掉当前节点中对应子节点/CPU 的 qsmask 位
        WRITE_ONCE(rnp->qsmask, rnp->qsmask & ~mask);          // line 2362

        // ② 检查：当前节点还有其它子节点未报告 QS 吗？
        if (rnp->qsmask != 0 || rcu_preempt_blocked_readers_cgp(rnp)) {
            // 有！停止上溯，释放锁
            raw_spin_unlock_irqrestore_rcu_node(rnp, flags);   // line 2370
            return;
        }

        // ③ 所有子节点都报告了 QS → 准备向父节点报告
        rnp->completedqs = rnp->gp_seq;
        mask = rnp->grpmask;  // 当前节点在父节点中的位

        if (rnp->parent == NULL) {
            // ★ 到达根节点！所有 CPU/子组完成 QS → GP 可以结束
            break;
        }

        // ④ 获取父节点锁，继续上溯
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
        rnp = rnp->parent;
        raw_spin_lock_irqsave_rcu_node(rnp, flags);
    }

    // ★ 到达根节点且所有子节点完成 → 唤醒 GP kthread
    rcu_report_qs_rsp(flags);                                   // line 2393
}
```

#### 8.2.5 合并树 QS 传播示例

```
8 CPUs, 3 层合并树:
                           rcu_node[1] (Root)
                          qsmask = 0b011 ← bit0=CPU0组已QS? bit1=CPU1组已QS?
                         ┌────────┴────────┐
                  rcu_node[2]          rcu_node[3] (叶,各管4 CPU)
              qsmask = 0b1111       qsmask = 0b1100
               ↑ CPU0-3 未QS          ↑ CPU6-7 未QS (CPU4-5 已QS)

CPU0 报告 QS:
  → rcu_report_qs_rdp(CPU0)
    → rcu_report_qs_rnp(mask=0b0001, rcu_node[2])
      → rcu_node[2].qsmask: 0b1111 → 0b1110 (CPU0 bit cleared)
      → rcu_node[2].qsmask != 0 (还有 CPU1-3 未 QS) → 停止

CPU1-3 逐个报告 QS:
  → rcu_node[2].qsmask 逐步变为 0b0000
  → 最后一个 CPU 报告后: rcu_node[2].qsmask == 0 → 上溯到 Root!
    → rcu_report_qs_rnp(mask=rcu_node[2].grpmask, rcu_node[1])
      → rcu_node[1].qsmask: 0b011 → 0b001 (CPU0组 bit cleared)

CPU6-7 报告 QS:
  → rcu_node[3].qsmask: 0b1100 → 0b0000
  → 上溯到 Root: rcu_node[1].qsmask: 0b001 → 0b000
  → 到达根节点 + qsmask==0 + 无阻塞读者 → rcu_report_qs_rsp → GP 完成!
```

#### 8.2.6 dyntick idle CPU 的特殊处理

当 CPU 进入 idle（`CT_RCU_WATCHING` 位被清除），它处于"扩展静默状态"（Extended QS）。此时没有 tick 中断来报告 QS，GP kthread 通过 `rcu_watching_snap_stopped_since`（tree.c:312）检测：

```c
// 原理：保存 CPU 的 watching_snap，如果 CPU 进入 idle (watching 位翻转)
// re-check 时发现值变了 → 说明经过了一次静默状态
static bool rcu_watching_snap_stopped_since(struct rcu_data *rdp, int snap)
{
    return snap != ct_rcu_watching_cpu_acquire(rdp->cpu);
}
```

这个函数和 `force_qs_rnp`（tree.c:2732）配合使用——`force_qs_rnp` 向未报告的 CPU 发送 resched IPI，逼迫它们从 idle 或内核循环中出来并报告 QS。

#### 8.2.7 实战：`rcu_nocbs=3 nohz_full=3 isolcpus=3` 时 CPU 3 如何度过 QS

当内核启动参数设置为 `rcu_nocbs=3 nohz_full=3 isolcpus=3` 时，CPU 3 同时承担三种角色：**NO_HZ_FULL 自适应滴答关闭** + **RCU 回调卸载到其他 CPU** + **调度隔离**。此时 CPU 3 可能很长时间没有 tick 中断，RCU 不能依赖 tick 来检测 QS。

**核心问题**：tick 被关了，CPU 3 跑一个用户态实时进程长时不切，RCU 怎么知道它度过了静默期？

**答案**：NO_HZ_FULL CPU 不再依赖 tick 逐 tick 检测 QS，而是通过**边界事件检测**——用户的进出、内核的进出、中断的进出、以及 GP kthread 的"逼迫"。

##### NO_HZ_FULL CPU 的 QS 检测状态机

```
用户态进程                         内核态（中断/trap）
────────────────────────          ───────────────────────
用户态执行 = 天然静默态             内核态执行 = 可能持有 rcu_read_lock
  │                                  │
  │ rcu_user_enter()                 │ __rcu_irq_enter_check_tick (tree.c:662)
  │   → context tracking 记录        │   → 如果 RCU 需要 QS，重新开启 tick
  │   → RCU 直接认为 CPU 在 QS       │   → 有 tick 走 tick 路径
  │                                  │   → 还是有 resched IPI
  │                                  │
  │ 进程切换到内核态:                  │ rcu_user_exit()
  │   rcu_user_exit()                │   → 进入内核，RCU 需要"监视"
  │   走下文各路径                     │
  └──────────────────────────────────┘
```

##### 三条 NO_HZ_FULL 独有的 QS 路径

**① 用户态 → 内核态切换 (最主要的 QS 来源)**

```c
// CPU 3 从用户态进内核（系统调用/中断/trap）
→ rcu_user_exit() → 内核态
// 用户态本身就是 QS → 直接标记
```

用户态在 context tracking 中被标记为 `CT_RCU_WATCHING` 位被清除。从用户态进入内核时，context tracking 看到这个切换，等价于一次 QS。

**② 内核态下短暂恢复 tick (`__rcu_irq_enter_check_tick`)**

当 CPU 3 因为中断/异常从用户态进入内核后，RCU 需要知道 CPU 3 在内核态有没有持有 `rcu_read_lock`。但 tick 已经被关了，RCU 拿不到 tick。这时 `__rcu_irq_enter_check_tick`（tree.c:662）介入：

```c
// tree.c:662-696 — NO_HZ_FULL CPU 的内核入口检查
void __rcu_irq_enter_check_tick(void)
{
    struct rcu_data *rdp = this_cpu_ptr(&rcu_data);

    // 如果不是 nohz_full CPU，或者 RCU 不需要 CPU 帮助 → 返回
    if (!tick_nohz_full_cpu(rdp->cpu) ||
        !READ_ONCE(rdp->rcu_urgent_qs) || ...)
        return;                                          // line 673-677

    // ★ NO_HZ_FULL CPU 进入内核 + RCU 需要它报告 QS
    // → 临时恢复 tick 以便 RCU 能走正常 tick 路径检测 QS
    tick_nohz_dep_set_cpu(rdp->cpu, TICK_DEP_MASK_RCU);  // line 689
}
```

`tick_nohz_dep_set_cpu(TICK_DEP_MASK_RCU)` 告诉 NO_HZ 子系统"RCU 需要这个 CPU 的 tick"，tick 被暂时恢复。恢复后 `rcu_sched_clock_irq` 就能正常跑了，走 8.2.2 的标准 tick 路径。

**③ GP kthread 的 resched IPI (强制逼迫)**

如果 CPU 3 在内核态死循环且不触发 tick 恢复（极端场景），GP kthread 的 `force_qs_rnp`（tree.c:2048/2051）会向它发送 resched IPI：

```
rcu_gp_fqs_loop:
  → force_qs_rnp(rcu_watching_snap_save)          // 先快照 watching 状态
  → force_qs_rnp(rcu_watching_snap_recheck)       // 再检查是否有变化
    → 向未报告 QS 的 CPU 发 resched IPI
      → CPU 3 收到 IPI → 必须调度 → rcu_note_context_switch
        → rcu_qs() → rcu_report_qs_rdp → rcu_report_qs_rnp → 上报
```

##### NO_HZ_FULL + NO_CB 的叠加效应

`rcu_nocbs=3` 把 CPU 3 的 RCU callback 处理卸载到其他 CPU 的 `rcuog` kthread 上。但这**不影响 QS 检测**——QS 检测和 callback 处理是两条路径：

```
QS 检测 (GP 推进)                        Callback 处理 (释放内存)
─────────────────                       ─────────────────────
仍在 CPU 3 上运行                       卸载到 CPU N 的 kthread
走用户态切换/内核入口/GP IPI 三条路径    CPU 3 不碰 callback 软中断
RCU_SOFTIRQ 仍在 CPU 3 上被调度        CPU N 的 rcuog kthread 负责
  (invoke_rcu_core → rcu_core)          rcu_do_batch
```

`rcu_needs_cpu`（tree.c:710）是两者之间的桥梁——它告诉 tick 子系统"即使 tick 被关了，RCU 也可能需要 tick 来处理 callback 或推进 GP"：

```c
// tree.c:710-730
int rcu_needs_cpu(void)
{
    struct rcu_data *rdp = this_cpu_ptr(&rcu_data);
    
    // NO_HZ_FULL CPU 被"强制保留 tick" → 肯定需要
    if (tick_nohz_full_cpu(rdp->cpu) && rdp->rcu_forced_tick)
        return 1;                                        // line 726
    // ... 其他检查
}
```

##### 总结：NO_HZ_FULL + NO_CB CPU 的 QS 保障机制

```
CPU 3 状态               QS 如何被检测                代码入口
─────────────────────   ─────────────────────────     ─────────────────
用户态运行 (tick OFF)   天然静默，无需检测             rcu_user_enter/exit
用户→内核切换           切换本身就是 QS               context tracking boundary
内核态 (tick OFF)       __rcu_irq_enter_check_tick    tree.c:662
                        临时恢复 tick → 走 tick 路径
内核态 (死循环)          GP kthread force_qs_rnp       tree.c:2048
                        发 resched IPI → 逼切
中断/异常触发 callback   RCU_SOFTIRQ 正常触发          invoke_rcu_core
NO_CB callback          卸载到 rcuog kthread           tree_nocb.h
```

**核心思想**：NO_HZ_FULL 下的 RCU QS 不再依赖"每个 tick 都检查一次"，而是依赖**状态切换的边界**。只要 CPU 3 从用户态进过内核、或从内核退出到用户态、或收到过中断、或被子 GP kthread 的 IPI 打扰，RCU 就能检测到它度过了静默期。

##### 8.2.7.1 补：如果进程一直在用户态运行？

用户态本身就是静默态——`rcu_read_lock()` 是内核态机制（`preempt_disable()`），用户态进程不可能持有它。进程切回用户态时，`preempt_enable()` 已经释放，context tracking 通过 `rcu_user_enter()` 把 `CT_RCU_WATCHING` 位清零。

GP kthread 在 FQS 阶段**直接利用这个位**，一次快照裁决：

```c
// tree.c:819-833 — GP kthread 快照每个 CPU 的 watching 状态
static int rcu_watching_snap_save(struct rcu_data *rdp)
{
    rdp->watching_snap = ct_rcu_watching_cpu_acquire(rdp->cpu);   // line 832
    if (rcu_watching_snap_in_eqs(rdp->watching_snap)) {            // line 833
        // ★ CT_RCU_WATCHING 位未置 → CPU 在扩展静默态
        // → 直接标记 QS 完成，不需要等、不需要 IPI、不需要恢复 tick
    }
}

// tree.c:294 — 判断是否在静默态
static bool rcu_watching_snap_in_eqs(int snap)
{
    return !(snap & CT_RCU_WATCHING);  // bit 未置 = 在静默态
}
```

**完整时序**：

```
CPU 3 用户态进程运行中 (tick OFF, NO_HZ_FULL)
  │
  │ GP kthread: rcu_gp_init() 启动新 GP
  │   → rnp->qsmask = rnp->qsmaskinit    // CPU 3 在"需要等待"集合中
  │
  │ GP kthread: rcu_gp_fqs_loop() 进入 FQS 循环
  │   → force_qs_rnp(rcu_watching_snap_save)    // tree.c:2048
  │     → rcu_watching_snap_save(CPU3)
  │       → snap = ct_rcu_watching_cpu_acquire(cpu)
  │       → CT_RCU_WATCHING = 0  (因为 CPU 3 在用户态!)
  │       → rcu_watching_snap_in_eqs → true
  │         → ★★ CPU 3 直接标记为 QS 已完成 ★★
  │         → rcu_node[leaf].qsmask &= ~CPU3_mask
  │         → 沿树上溯，清除 mask...
  │
  │ GP 等其余 CPU 也都完成 → root qsmask==0 → GP 结束
  ▼
CPU 3 全程在用户态跑，完全不知道上面的 GP 存在
```

这是 RCU **最快**通过的路径：一次快照读取就判了，零等待、零 IPI、零 tick 恢复。和内核态死循环形成鲜明对比——后者需要 `nohz_full_patience_delay`（默认 5 个 jiffy）后 GP kthread 才会发 resched IPI 逼迫。

##### "用户态不可能持有 rcu_read_lock" 的源码级证明

上面说了"用户态天然静默"，但为什么用户态进程不可能持有 `rcu_read_lock`？根因在于 `rcu_read_lock()` 的底层实现不是一个"标记"，而是一个**抢占禁止原语**：

```c
// include/linux/rcupdate.h:101
static inline void __rcu_read_lock(void) { preempt_disable(); }
```

`preempt_disable()` 操作的是 per-CPU 的 `__preempt_count` 变量——这是内核态的调度器数据结构。用户态没有这个变量，调度器的概念在用户态也不存在（进程是直接被踢出 CPU 的）。

看一条完整的生命周期：

```
用户态进程 → 进入内核 (系统调用/中断/trap)
  → rcu_read_lock()    ← preempt_disable(), __preempt_count + 1
  → ... RCU 读临界区 ...   ← 此期间内核抢占被禁止
  → rcu_read_unlock()  ← preempt_enable(), __preempt_count - 1
  → 返回用户态
```

**关键点**：如果进程在 RCU 读临界区内被调用 `schedule()` 或被 scheduler tick 选中切走，这个切换**只能在 `preempt_enable()` 之后发生**。因为 `preempt_disable()` 期间调度器看到 `__preempt_count > 0`，会跳过这个 CPU 不切换。

而 `preempt_enable()` 本身就是 `rcu_read_unlock()`。所以：

> **没有任何 RCU 读锁可以"跨"到用户态存活。进程只要回到用户态，它之前持有的所有 `rcu_read_lock` 都一定已经随 `preempt_enable()` 释放了。**

这就是"CT_RCU_WATCHING 位 → 静默态"可以成立的逻辑链——context tracking 的 `CT_RCU_WATCHING` 位只是个**表象**，真正的根因在 `preempt_disable()` 的语义本身就保证了 rcu_read_lock 的生存周期不可能超出内核态边界。

反之，在 PREEMPT_RCU 内核中（`CONFIG_PREEMPT_RCU=y`），`rcu_read_lock()` 的实现不同——它允许抢占，因此 RCU 必须额外维护一个 per-task 的 `rcu_read_lock_nesting` 计数器。这种情况下通过 `rcu_blocked_task`（被抢占时 rcu_read_lock 还持着）来追踪，GP kthread 必须等这个 task 重新被调度运行并走到 `rcu_read_unlock()` 才认为 QS 完成。这就是为什么**低延迟实时环境中 PREEMPT_RCU 的 GP 延迟显著高于 standard RCU**。

## 9. QS 上报 — rcu_report_qs_rnp()

**定义位置**：`tree.c:2339`

这是将静默状态沿合并树向上传播的核心函数：

```c
// tree.c:2339-2394
static void rcu_report_qs_rnp(unsigned long mask, struct rcu_node *rnp,
                              unsigned long gps, unsigned long flags)
    __releases(rnp->lock)
{
    unsigned long oldmask = 0;
    struct rcu_node *rnp_c;

    for (;;) {
        // 如果该位已经清除或 GP 已结束，直接返回
        if ((!(rnp->qsmask & mask) && mask) || rnp->gp_seq != gps) {
            raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
            return;
        }

        // 清除本节点的对应 qsmask 位
        WRITE_ONCE(rnp->qsmask, rnp->qsmask & ~mask);        // line 2362

        // 本节点还有其它位未清零或有阻塞任务 → 停止上溯
        if (rnp->qsmask != 0 || rcu_preempt_blocked_readers_cgp(rnp)) {
            raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
            return;                                          // line 2370-2371
        }

        rnp->completedqs = rnp->gp_seq;                      // 记录完成
        mask = rnp->grpmask;                                 // 准备上溯掩码

        if (rnp->parent == NULL) {
            // 到达根节点！这是最后一个 CPU/子组报告 QS
            break;                                           // line 2379
        }

        // 释放当前节点锁，获取父节点锁
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
        rnp_c = rnp;
        rnp = rnp->parent;
        raw_spin_lock_irqsave_rcu_node(rnp, flags);
        oldmask = READ_ONCE(rnp_c->qsmask);
    }

    // 所有 CPU/子组完成 → 唤醒 GP kthread 执行清理
    rcu_report_qs_rsp(flags);                                // line 2393
}
```

`rcu_report_qs_rsp()` (line 2315) 设置 `RCU_GP_FLAG_FQS` 并唤醒 GP kthread，后者发现所有 QS 完成，跳出 `rcu_gp_fqs_loop()` 进入 `rcu_gp_cleanup()`。

### 9.1 QS 传播示例

```
8 CPUs 的 QS 上报过程（CPU 3 最后报告）:

Step 1: CPU 3 报告 QS 给其 leaf rcu_node[4] (覆盖 CPU 2,3)
        qsmask: 0b11 → 清除 bit1 → 0b10 (只有 CPU 2 还未报告)
        → CPU 2 还在读临界区，停止上溯

Step 2: CPU 2 调用 rcu_read_unlock() → 触发 QS
        qsmask: 0b10 → 清除 bit0 → 0b00
        → 本节点完成，上溯到父节点 rcu_node[2]

Step 3: rcu_node[2] (覆盖 CPU 0..3)
        qsmask: 0b11 → 清除 rcu_node[4] 对应的 bit → 0b01
        → rcu_node[3] 还未完成，停止上溯

Step 4: rcu_node[3] 的所有 CPU 也完成
        qsmask: 0b01 → 清除 → 0b00
        → 上溯到 root rcu_node[0]

Step 5: root qsmask 清零
        → 本 GP 完成，唤醒 GP kthread → rcu_gp_cleanup()
```

## 10. rcu_report_qs_rdp() — Per-CPU QS 上报入口

**定义位置**：`tree.c:2442`

```c
// tree.c:2442-2489
static void rcu_report_qs_rdp(struct rcu_data *rdp)
{
    unsigned long flags;
    unsigned long mask;
    struct rcu_node *rnp;

    WARN_ON_ONCE(rdp->cpu != smp_processor_id());
    rnp = rdp->mynode;
    raw_spin_lock_irqsave_rcu_node(rnp, flags);

    // 如果 GP 已经结束或本 CPU 还需要新 QS
    if (rdp->cpu_no_qs.b.norm || rdp->gp_seq != rnp->gp_seq ||
        rdp->gpwrap) {
        rdp->cpu_no_qs.b.norm = true;  // 需要为新的 GP 报告 QS
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
        return;
    }

    mask = rdp->grpmask;
    rdp->core_needs_qs = false;

    if ((rnp->qsmask & mask) == 0) {
        // 该位已经清除（重复报告），直接返回
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
    } else {
        // 加速回调处理（当前 GP 还未完成，所以下一个 GP 才能处理回调）
        if (!rcu_rdp_is_offloaded(rdp)) {
            WARN_ON_ONCE(rcu_accelerate_cbs(rnp, rdp));
        }
        rcu_disable_urgency_upon_qs(rdp);
        // 向上传播 QS
        rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
    }
}
```

## 11. rcu_gp_cleanup() — GP 清理

**定义位置**：`tree.c:2150`

```c
// tree.c:2150-2266
static noinline void rcu_gp_cleanup(void)
{
    struct rcu_node *rnp = rcu_get_root();

    raw_spin_lock_irq_rcu_node(rnp);
    rcu_state.gp_end = jiffies;
    gp_duration = rcu_state.gp_end - rcu_state.gp_start;
    if (gp_duration > rcu_state.gp_max)
        rcu_state.gp_max = gp_duration;                      // 记录最大 GP 时长
    raw_spin_unlock_irq_rcu_node(rnp);

    // 传播新的 gp_seq 到所有 rcu_node
    new_gp_seq = rcu_state.gp_seq;
    rcu_seq_end(&new_gp_seq);                                // line 2189
    rcu_for_each_node_breadth_first(rnp) {
        raw_spin_lock_irq_rcu_node(rnp);
        WARN_ON_ONCE(rnp->qsmask);                           // 应该已经清零
        WRITE_ONCE(rnp->gp_seq, new_gp_seq);                 // line 2195
        // 检查每个 CPU 是否需要新 GP
        rdp = this_cpu_ptr(&rcu_data);
        if (rnp == rdp->mynode)
            needgp = __note_gp_changes(rnp, rdp) || needgp;
        needgp = rcu_future_gp_cleanup(rnp) || needgp;
        // ...
        raw_spin_unlock_irq_rcu_node(rnp);
    }

    // 正式结束 GP
    rcu_seq_end(&rcu_state.gp_seq);                          // line 2221
    WRITE_ONCE(rcu_state.gp_state, RCU_GP_IDLE);            // line 2223

    // 检查是否已经有新的 GP 请求（通过 gp_seq_needed）
    if (!needgp && ULONG_CMP_LT(rnp->gp_seq, rnp->gp_seq_needed)) {
        needgp = true;                                       // line 2229
    }

    if ((offloaded || !rcu_accelerate_cbs(rnp, rdp)) && needgp) {
        // 需要新 GP，设置 RCU_GP_FLAG_INIT
        WRITE_ONCE(rcu_state.gp_flags, RCU_GP_FLAG_INIT);    // line 2245
    } else {
        // 不需要新 GP，仅保留可能已有 INIT 标志
        WRITE_ONCE(rcu_state.gp_flags,
                   rcu_state.gp_flags & RCU_GP_FLAG_INIT);    // line 2256
    }
    raw_spin_unlock_irq_rcu_node(rnp);

    // 通知 synchronize_rcu() 用户 GP 结束
    rcu_sr_normal_gp_cleanup();                              // line 2261
}
```

清理阶段的关键操作：
1. 更新所有 rcu_node 的 `gp_seq` 为新的已完成的序列号
2. 将 `gp_state` 设回 `RCU_GP_IDLE`
3. 检查是否需要立即启动下一个 GP（`gp_seq_needed > gp_seq`）
4. 如果需要，设置 `RCU_GP_FLAG_INIT`，GP kthread 的下一次循环会检测到

## 12. rcu_init() — 启动序列

**定义位置**：`tree.c:4874`

```c
// tree.c:4874-4926
void __init rcu_init(void)
{
    int cpu = smp_processor_id();

    rcu_early_boot_tests();
    rcu_bootup_announce();
    sanitize_kthread_prio();
    rcu_init_geometry();           // 计算 rcu_node 树结构
    rcu_init_one();                // 初始化 rcu_state 和第一个 rcu_node

    if (dump_tree)
        rcu_dump_rcu_node_tree();  // 打印树结构供调试

    if (use_softirq)
        open_softirq(RCU_SOFTIRQ, rcu_core_si);  // 注册 RCU_SOFTIRQ 处理函数

    pm_notifier(rcu_pm_notify, 0); // 注册电源管理通知
    rcutree_prepare_cpu(cpu);      // 准备 boot CPU
    rcutree_report_cpu_starting(cpu);
    rcutree_online_cpu(cpu);       // 将 boot CPU 上线

    rcu_gp_wq = alloc_workqueue("rcu_gp", WQ_MEM_RECLAIM | WQ_PERCPU, 0);

    if (rcu_normal_wake_from_gp < 0) {
        if (num_possible_cpus() <= WAKE_FROM_GP_CPU_THRESHOLD)
            rcu_normal_wake_from_gp = 1;
    }

    // 填充默认的 qovld (回调过载阈值) 参数
    if (qovld < 0)
        qovld_calc = DEFAULT_RCU_QOVLD_MULT * qhimark;
    else
        qovld_calc = qovld;

    tasks_cblist_init_generic();   // 初始化 Tasks RCU
}
```

### 12.1 rcu_init_geometry() — 合并树几何计算

**定义位置**：`tree.c:4768`

```c
// tree.c:4819-4847 关键计算
rcu_capacity[0] = rcu_fanout_leaf;                           // line 4819
for (i = 1; i < RCU_NUM_LVLS; i++)
    rcu_capacity[i] = rcu_capacity[i - 1] * RCU_FANOUT;      // line 4821

// 计算需要的层数
for (i = 0; nr_cpu_ids > rcu_capacity[i]; i++) { }           // line 4834
rcu_num_lvls = i + 1;                                        // line 4836

// 计算每层的节点数
for (i = 0; i < rcu_num_lvls; i++) {
    int cap = rcu_capacity[(rcu_num_lvls - 1) - i];
    num_rcu_lvl[i] = DIV_ROUND_UP(nr_cpu_ids, cap);
}

// 计算总节点数
rcu_num_nodes = 0;
for (i = 0; i < rcu_num_lvls; i++)
    rcu_num_nodes += num_rcu_lvl[i];
```

例如 64 个 CPU，`RCU_FANOUT_LEAF=16`, `RCU_FANOUT=8`：
- capacity: `[16, 128, 1024]`
- 64 个 CPU：`rcu_num_lvls = 2`（`64 > 16` 但 `64 <= 128`）
- level 0: `DIV_ROUND_UP(64, 128) = 1` 个 root
- level 1: `DIV_ROUND_UP(64, 16) = 4` 个 leaf
- 总节点 = 5

## 13. GP 序列号编码

`gp_seq` 的低 `RCU_SEQ_CTR_SHIFT` 位（2 位）编码 GP 阶段：

```
gp_seq 格式 (64-bit):
  ┌────────────────────────────────────────────┬───┬───┐
  │              计数器 (62 bits)               │ S │ S │
  └────────────────────────────────────────────┴───┴───┘
                                                  [1] [0]

位[1:0]:
  00 = GP idle (无活跃 GP)
  01 = GP 进行中
  10 = GP 进行中
  11 = GP 进行中
```

相关宏（`rcupdate.h:47-48`）：
```c
#define RCU_SEQ_CTR_SHIFT    2
#define RCU_SEQ_STATE_MASK   ((1 << RCU_SEQ_CTR_SHIFT) - 1)  // 0x3
```

`rcu_seq_start()` 递增低 2 位，`rcu_seq_end()` 清零低 2 位并递增计数器。这允许 poll 函数只需要检查 `gp_seq` 的低 2 位状态和计数器值。

## 14. 总结

RCU 的 GP 状态机是内核中最精妙的并发控制设计之一：

1. **解耦读写作**：写者从不阻塞读者，读者永远不阻塞
2. **层次化 QS 收集**：`rcu_node` 树将 O(N) 的全系统扫描降为 O(log N)
3. **懒惰检测**：不主动打断读者，等待自然调度点或使用超时强制
4. **SMP 扩展性**：每 CPU 的 `rcu_data` 最小化缓存行争用
5. **Kthread 驱动**：`rcu_gp_kthread()` 串行化所有 GP 操作，避免锁竞争

---

## 下一篇文章

[第二篇：RCU API — rcu_read_lock、synchronize_rcu、call_rcu 与指针宏](./02-api.md)
