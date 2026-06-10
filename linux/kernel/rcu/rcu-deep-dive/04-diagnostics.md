# 第四篇：RCU 诊断 — 拖延检测、加速 GP 与回调卸载

> 源码：kernel/rcu/tree_stall.h (1185行) | tree_exp.h (1118行) | tree_nocb.h (1702行) | tree_plugin.h (1369行)

系列目录：[RCU 内核源码深度解析](./README.md)

---

## 1. RCU CPU 拖延检测 —— Stall Detection

### 1.1 什么是 RCU Stall？

当 CPU 在 RCU 读端临界区内停留过久，或 GP kthread 长时间没有进展时，RCU 子系统发出 "RCU CPU stall warning"。这通常意味着内核存在性能问题或死锁。

```
正常情况：                             Stall 情况：
CPU: [rcu_read_lock() ─ 工作 ─ rcu_read_unlock()]
      ←────  几百微秒到几毫秒  ────→

                                    CPU: [rcu_read_lock() ── 死循环/spinlock ── ...]
                                          ←────  数十秒  ────→
                                                                ↑ stall detected!
```

### 1.2 拖延阈值常量

来自 `tree_stall.h:74-80`：

```c
// tree_stall.h:74-80
#ifdef CONFIG_PROVE_RCU
#define RCU_STALL_DELAY_DELTA       (5 * HZ)    // PROVE_RCU 下增加 5 秒容限
#else
#define RCU_STALL_DELAY_DELTA       0
#endif
#define RCU_STALL_MIGHT_DIV         8
#define RCU_STALL_MIGHT_MIN         (2 * HZ)    // 最小 2 秒
```

最终的超时计算在 `rcu_jiffies_till_stall_check()`（`tree_stall.h:111-128`）：

```c
// tree_stall.h:111-128
int rcu_jiffies_till_stall_check(void)
{
    int till_stall_check = READ_ONCE(rcu_cpu_stall_timeout);

    // 限制在 3~300 秒范围内
    if (till_stall_check < 3) {
        WRITE_ONCE(rcu_cpu_stall_timeout, 3);
        till_stall_check = 3;
    } else if (till_stall_check > 300) {
        WRITE_ONCE(rcu_cpu_stall_timeout, 300);
        till_stall_check = 300;
    }
    return till_stall_check * HZ + RCU_STALL_DELAY_DELTA;
}
```

默认超时 = `CONFIG_RCU_CPU_STALL_TIMEOUT` × HZ，可通过 `/sys/module/rcupdate/parameters/rcu_cpu_stall_timeout` 调整。

### 1.3 sysctl 与 sysfs 接口

**sysctl 接口**（`tree_stall.h:20-50`）：

```c
// tree_stall.h:20-50
static int sysctl_panic_on_rcu_stall __read_mostly;       // 检测到 stall 即内核 panic
static int sysctl_max_rcu_stall_to_panic __read_mostly;  // 最多容忍多少次 stall 后 panic

static int __init init_rcu_stall_sysctl(void)
{
    register_sysctl_init("kernel", rcu_stall_sysctl_table);
    return 0;
}
subsys_initcall(init_rcu_stall_sysctl);  // 内建于 RCU 初始化中
```

可通过 `/proc/sys/kernel/panic_on_rcu_stall` 和 `/proc/sys/kernel/max_rcu_stall_to_panic` 控制。

**sysfs 接口**（`tree_stall.h:52-70`）：

```c
// tree_stall.h:52-70
static ssize_t rcu_stall_count_show(struct kobject *kobj, ...)
{
    return sysfs_emit(page, "%u\n", rcu_stall_count);  // 累加计数器
}
late_initcall(kernel_rcu_stall_sysfs_init);  // 较晚注册 → /sys/kernel/rcu_stall_count
```

### 1.4 Stall 抑制机制

内核 panic 时抑制 stall 警告（`tree_stall.h:144-152`）：

```c
// tree_stall.h:144-152
static int rcu_panic(struct notifier_block *this, unsigned long ev, void *ptr)
{
    rcu_cpu_stall_suppress = 1;  // 抑制所有 stall 输出
    return NOTIFY_DONE;
}
early_initcall(check_cpu_stall_init);
```

警告输出期间使用 `nbcon_cpu_emergency_enter/exit()` 确保可抢占控制台。

### 1.5 Stall 检测流程

```
时钟中断 → rcu_flavor_sched_clock_irq()    // tree.c
             ↓
          check_cpu_stall(rdp)              // tree_stall.h:772
             ↓
          ┌ gp_seq 未变化? ─→ return        // GP 已完成
          │  ↓ changed
          │ jiffies > jiffies_stall? ─→ return // 未到超时
          │  ↓ yes
          │ 自己是拖延者(self_detected)?
          │ ├─ yes → print_cpu_stall()      // tree_stall.h:709
          │ └─ no  → print_other_cpu_stall() // tree_stall.h:631
          ↓
        (可选) panic_on_rcu_stall()          // tree_stall.h:162
```

### 1.6 `check_cpu_stall()` 核心逻辑（`tree_stall.h:772-867`）

```c
// tree_stall.h:772-867 (核心逻辑简化)
static void check_cpu_stall(struct rcu_data *rdp)
{
    // 检查是否应该抑制 stall 输出
    if ((rcu_stall_is_suppressed() && !rcu_kick_kthreads) || !rcu_gp_in_progress())
        return;

    // 通过 gp_seq 二次读取来消除误报（false positive）
    gs1 = READ_ONCE(rcu_state.gp_seq);
    smp_rmb();
    js = READ_ONCE(rcu_state.jiffies_stall);
    smp_rmb();
    gps = READ_ONCE(rcu_state.gp_start);
    smp_rmb();
    gs2 = READ_ONCE(rcu_state.gp_seq);

    // 如果两次 gp_seq 不同则 GP 已完成，退出
    if (gs1 != gs2 || ULONG_CMP_LT(j, js) || ULONG_CMP_GE(gps, js))
        return;

    self_detected = READ_ONCE(rnp->qsmask) & rdp->grpmask;
    if (self_detected) {
        print_cpu_stall(gs2, gps);       // 自己拖延
    } else {
        print_other_cpu_stall(gs2, gps); // 其他 CPU 拖延
    }
}
```

**误报预防**：通过两次读取 `gp_seq`、中间插入内存屏障，保证在一个完整 GP 的上下文中判断，避免跨 GP 的时序问题。

### 1.7 `print_cpu_stall()` — 自我拖延报告（`tree_stall.h:709-767`）

```c
// tree_stall.h:709-767
static void print_cpu_stall(unsigned long gp_seq, unsigned long gps)
{
    pr_err("INFO: %s self-detected stall on CPU\n", rcu_state.name);
    print_cpu_stall_info(smp_processor_id());  // 输出当前 CPU 状态
    // 输出：t=... jiffies g=... q=... ncpus=...
    rcu_check_gp_kthread_expired_fqs_timer();  // 检查 FQS 定时器是否过期
    rcu_check_gp_kthread_starvation();         // 检查 GP kthread 是否饥饿
    rcu_dump_cpu_stacks(gp_seq);               // dump 所有 CPU 栈
    set_need_resched_current();                // 强制重新调度
}
```

### 1.8 `print_other_cpu_stall()` — 标记他人拖延（`tree_stall.h:631-707`）

```c
// tree_stall.h:631-707
static void print_other_cpu_stall(unsigned long gp_seq, unsigned long gps)
{
    pr_err("INFO: %s detected stalls on CPUs/tasks:\n", rcu_state.name);
    rcu_for_each_leaf_node(rnp) {
        if (rnp->qsmask != 0) {
            for_each_leaf_node_possible_cpu(rnp, cpu)
                if (rnp->qsmask & leaf_node_cpu_bit(rnp, cpu)) {
                    print_cpu_stall_info(cpu);  // 逐 CPU 输出诊断信息
                    ndetected++;
                }
        }
        ndetected += rcu_print_task_stall(rnp, flags);  // 阻塞任务
    }
    pr_err("\t(detected by %d, t=%ld jiffies, g=%ld, q=%lu)\n", ...);
}
```

### 1.9 `print_cpu_stall_info()` — 单 CPU 诊断信息（`tree_stall.h:518-566`）

输出格式：

```
  CPU#-OoN.: (2000 ticks this GP) idle=... softirq=... fqs=...
```

格式解析：
- `O` → CPU 在线，`o` → qsmaskinit 中，`N` → qsmaskinitnext 中
- 第四个字符 → irq_work 状态（数字=待处理的 GP 数量，`!`=无待处理，`?`=未配置）

### 1.10 GP kthread 饥饿检测（`tree_stall.h:569-599`）

```c
// tree_stall.h:569-599
static void rcu_check_gp_kthread_starvation(void)
{
    if (rcu_is_gp_kthread_starving(&j)) {
        pr_err("%s kthread starved for %ld jiffies!...\n", ...);
        pr_err("\tUnless %s kthread gets sufficient CPU time, OOM is expected.\n", ...);
        sched_show_task(gpk);   // dump GP kthread 栈
        wake_up_process(gpk);   // 尝试唤醒
    }
}
```

### 1.11 典型 Stall 消息解读示例

```
INFO: rcu_preempt self-detected stall on CPU
    1-...: (21002 ticks this GP) idle=e3a/1/0 softirq=52304/52304 fqs=3431
    (t=21002 jiffies g=5897 q=129 ncpus=8)
```

含义：
- CPU 1 自我检测到拖延，当前 GP 中已过 21002 个 tick
- CPU 状态为在线、在 qsmaskinit 中、在 qsmaskinitnext 中、无待处理 IRQ work
- softirq 处理计数 52304/52304（快照/当前）→ 表明 softirq handler 正常运行
- 当前 GP 过 21002 jiffies，总回调数 129

---

## 2. 加速 GP —— Expedited Grace Period

### 2.1 为什么需要加速 GP？

正常 RCU GP 的延迟在 10~100ms 级别。某些场景（如内存热插拔、设备移除）需要**尽可能快地**完成 GP。`synchronize_rcu_expedited()` 通过 **IPI 轰炸**所有 CPU 来强制获取静默态。

```
正常 GP 流程：                    加速 GP 流程：

synchronize_rcu()                 synchronize_rcu_expedited()
  ↓ 等待自然 GP                     ↓ IPI 所有 CPU
  CPU0: ...tick...qs...              CPU0: rcu_exp_handler → 立即返回 qs
  CPU1: ...tick...qs...              CPU1: rcu_exp_handler → 立即返回 qs
  CPU2: ...tick...qs...              CPU2: rcu_exp_handler → 立即返回 qs
  ↓ ~10ms 后                       ↓ ~几十微秒后
  GP 完成 ✓                         GP 完成 ✓
```

### 2.2 Expedited GP 序列号管理（`tree_exp.h:20-68`）

```c
// tree_exp.h:20-24,38-43,50-58,65-68
static void rcu_exp_gp_seq_start(void)  { rcu_seq_start(&rcu_state.expedited_sequence); }
static void rcu_exp_gp_seq_end(void)    { rcu_seq_end(&rcu_state.expedited_sequence); smp_mb(); }
static unsigned long rcu_exp_gp_seq_snap(void) { smp_mb(); return rcu_seq_snap(...); }
static bool rcu_exp_gp_seq_done(unsigned long s) { return rcu_seq_done(...); }
```

### 2.3 Funnel Lock — 多调用者串行化（`tree_exp.h:301-352`）

多个线程同时调用 `synchronize_rcu_expedited()` 时，通过 **funnel lock** 收集起来，只让一个线程执行实际的 GP 工作：

```c
// tree_exp.h:301-352
static bool exp_funnel_lock(unsigned long s)
{
    // 快速路径：低争用尝试
    if (ULONG_CMP_LT(rnp->exp_seq_rq, s) && (...)
        && mutex_trylock(&rcu_state.exp_mutex))
        goto fastpath;

    // 慢速路径：沿 rcu_node 树向上推进
    for (; rnp != NULL; rnp = rnp->parent) {
        if (sync_exp_work_done(s)) return true;  // piggyback：复用他人 GP

        spin_lock(&rnp->exp_lock);
        if (ULONG_CMP_GE(rnp->exp_seq_rq, s)) {
            // 已有其他线程在做，等待即可
            spin_unlock(&rnp->exp_lock);
            wait_event(rnp->exp_wq[...], sync_exp_work_done(s));
            return true;  // 复用他人 GP 完成
        }
        WRITE_ONCE(rnp->exp_seq_rq, s);  // 标记我们的请求
        spin_unlock(&rnp->exp_lock);
    }
    mutex_lock(&rcu_state.exp_mutex);  // 到达树根，由我们执行
fastpath:
    rcu_exp_gp_seq_start();
    return false;
}
```

Funnel lock 示意图：

```
CPU0            CPU1            CPU2            CPU3
  ↓               ↓               ↓               ↓
  ├─ leaf rnp    ├─ leaf rnp    ├─ leaf rnp    ├─ leaf rnp
  │  设置seq_rq   │  发现已有      │  发现已有      │  发现已有
  │      ↓        │  等待        │  等待        │  等待
  ├─ mid  rnp     └──────────────┴──────────────┘
  │  设置seq_rq
  │      ↓
  └─ root rnp
     获取exp_mutex ← 唯一执行者!
     执行GP ←─── GP 完成后唤醒所有等待者
```

### 2.4 CPU 选择与 IPI 发送（`tree_exp.h:358-446`）

`__sync_rcu_exp_select_node_cpus()` 对 `expmask` 中的每个 CPU：跳过自身和离线 CPU、检查 extended quiescent state（idle），对剩余 CPU 通过 `smp_call_function_single()` 发送 IPI。失败时重试或标记 CPU 离线。

### 2.5 `rcu_exp_handler()` — IPI 处理函数

**PREEMPT_RCU 版本**（`tree_exp.h:749-804`）：

```c
// tree_exp.h:749-804
static void rcu_exp_handler(void *unused)
{
    int depth = rcu_preempt_depth();
    // 不在读端临界区内 → 立即报告静默态
    if (!depth) {
        if (!(preempt_count() & (PREEMPT_MASK | SOFTIRQ_MASK)) || idle)
            rcu_report_exp_rdp(rdp);
        else
            rcu_exp_need_qs();  // 设置标志，下次调度时报告
        return;
    }

    // 在读端临界区内 → 设置 deferred QS 标志
    if (depth > 0) {
        raw_spin_lock_irqsave_rcu_node(rnp, flags);
        if (rnp->expmask & rdp->grpmask) {
            WRITE_ONCE(rdp->cpu_no_qs.b.exp, true);    // 标记需要加速 QS
            t->rcu_read_unlock_special.b.exp_hint = true; // 提示 rcu_read_unlock
        }
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
    }
}
```

**非 PREEMPT_RCU 版本**（`tree_exp.h:865-881`）：

```c
// tree_exp.h:865-881
static void rcu_exp_handler(void *unused)
{
    if (rcu_is_cpu_rrupt_from_idle() || preempt_bh_enabled)
        rcu_report_exp_rdp(rdp);   // 立即报告
    else
        rcu_exp_need_qs();         // 推迟报告
}
```

### 2.6 等待 GP 完成（`tree_exp.h:624-680`）

```c
// tree_exp.h:624-680
static void synchronize_rcu_expedited_wait(void)
{
    jiffies_start = jiffies;

    // NO_HZ_FULL 优化：设置 tick 依赖，确保目标 CPU 收到 tick
    if (tick_nohz_full_enabled()) {
        rcu_for_each_leaf_node(rnp)
            for_each_leaf_node_cpu_mask(rnp, cpu, mask)
                tick_dep_set_cpu(cpu, TICK_DEP_BIT_RCU_EXP);
    }

    for (;;) {
        if (synchronize_rcu_expedited_wait_once(jiffies_stall))
            return;  // GP 完成!
        // 超时 → 打印 expedited stall 警告
        synchronize_rcu_expedited_stall(jiffies_start, j);
        panic_on_rcu_stall();
    }
}
```

### 2.7 热插拔处理（`tree_exp.h:77-128`）

```c
// tree_exp.h:77-128
static void sync_exp_reset_tree_hotplug(void)
{
    if (likely(ncpus == rcu_state.ncpus_snap))
        return;  // 无新 CPU 上线，快速返回

    rcu_for_each_leaf_node(rnp) {
        if (rnp->expmaskinit == rnp->expmaskinitnext)
            continue;  // 此节点无新 CPU
        oldmask = rnp->expmaskinit;
        rnp->expmaskinit = rnp->expmaskinitnext;

        // 向上传播新 CPU 的掩码
        if (!oldmask) {
            rnp_up = rnp->parent;
            while (rnp_up) {
                rnp_up->expmaskinit |= mask;
                rnp_up = rnp_up->parent;
            }
        }
    }
}
```

### 2.8 加速 GP 完整流程

```
synchronize_rcu_expedited()
    ↓
exp_funnel_lock(s)              ← 与其他调用者串行化 / piggyback
    ↓
sync_rcu_exp_select_cpus()      ← 为每个 leaf rcu_node 发起工作
    ├─ sync_exp_reset_tree()    ← 重置 expmask
    └─ 多 leaf rcu_node 并行:
        __sync_rcu_exp_select_node_cpus()
        ├─ 检查 EQS/离线 → 立即标记 QS
        └─ 对剩余 CPU 发送 IPI → rcu_exp_handler()
            ├─ 不在临界区 → rcu_report_exp_rdp()
            └─ 在临界区内 → 设置 exp/deferred_qs 标志
    ↓
synchronize_rcu_expedited_wait()
    ├─ swait_event_timeout(expedited_wq, root done)
    │   → 等待所有 CPU 报告 QS
    └─ 超时 → synchronize_rcu_expedited_stall() 打印警告
    ↓
rcu_exp_wait_wake(s)            ← 设置 exp_seq_rq、唤醒等待者
```

---

## 3. NO_CB CPUs —— 回调卸载

### 3.1 设计动机

正常 RCU 下每个 CPU 自行调用回调，这在 `nohz_full`/RT 场景下有问题（打断深度睡眠、回调延迟不可预测）。NO_CB 将回调处理**卸载**到专门的 kthread。

### 3.2 架构设计

NO_CB CPU 使用**两级 kthread 架构**：

```
   ┌──────────────────────┐      ┌──────────────────────┐
   │ GP kthread (rcuog/N) │ ──→  │ CB kthread (rcuoc/N) │
   │ 管理回调、等待 GP      │      │ 仅调用回调函数         │
   └──────────────────────┘      └──────────────────────┘
```

`tree_nocb.h:71`: 启动参数 `rcu_nocbs=0-3,7` 指定卸载 CPU，`rcu_nocb_poll` 开启轮询模式。

### 3.3 Bypass 链表 —— 回调风暴时的优雅降级

当 `call_rcu()` 速率过高时，回调暂存到 `->nocb_bypass` 链表（`tree_nocb.h:85` 阈值 `16*1000/HZ` per jiffy）：

```c
// tree_nocb.h:431-553 (核心逻辑)
static bool rcu_nocb_try_bypass(struct rcu_data *rdp, struct rcu_head *rhp, ...)
{
    // 低频率 → 直接入 cblist，同时 flush bypass
    if (rdp->nocb_nobypass_count < nocb_nobypass_lim_per_jiffy && !lazy) {
        rcu_nocb_flush_bypass(rdp, NULL, j, false);
        return false;  // 调用者直接入 cblist
    }
    // 高频率 → 回调进入 bypass
    rcu_cblist_enqueue(&rdp->nocb_bypass, rhp);
    return true;
}
```

Bypass 冲刷条件（`tree_nocb.h:386-411`）：超过 `qhimark` 高水位，或超时（懒惰回调 10*HZ、普通 0-1 jiffy）时冲刷。

### 3.4 GP kthread 主循环（`tree_nocb.h:658-846`）

遍历组内所有 NO_CB CPU，冲刷 bypass、推进回调、确定最小待等待 GP，然后 `swait_event` 等待该 GP 完成，最后唤醒对应的 CB kthread。

### 3.5 懒惰回调 —— LAZY Callbacks

```c
// tree_nocb.h:250-252
#define LAZY_FLUSH_JIFFIES (10 * HZ)  // 懒惰回调最多延迟 10 秒冲刷
static unsigned long jiffies_lazy_flush = LAZY_FLUSH_JIFFIES;
```

懒惰回调是一种**能效优化**：将回调暂存在 bypass 链表中，最长延迟 10 秒批处理，减少 kthread 唤醒次数。

---

## 4. Preemptible RCU 优先级提升 —— Priority Boosting

当 `CONFIG_PREEMPT_RCU=y` 时，任务可能在 `rcu_read_lock()` 内被抢占。如果这个任务阻塞了 GP 进展，而调度器不给它 CPU 时间，就会导致 GP 无限延迟。

### 4.1 阻塞任务跟踪（`tree_plugin.h:128-132`）

```c
// tree_plugin.h:128-132
#define RCU_GP_TASKS   0x8   // 正常 GP 有等待的任务
#define RCU_EXP_TASKS  0x4   // 加速 GP 有等待的任务
#define RCU_GP_BLKD    0x2   // 正常 GP 被此 CPU 阻塞
#define RCU_EXP_BLKD   0x1   // 加速 GP 被此 CPU 阻塞
```

### 4.2 上下文切换中的阻塞队列（`tree_plugin.h:324-374`）

```c
// tree_plugin.h:324-374
void rcu_note_context_switch(bool preempt)
{
    if (rcu_preempt_depth() > 0 && !t->rcu_read_unlock_special.b.blocked) {
        // 在 RCU 读端临界区内被换出
        rnp = rdp->mynode;
        raw_spin_lock_rcu_node(rnp);
        t->rcu_read_unlock_special.b.blocked = true;
        t->rcu_blocked_node = rnp;

        // 决策：将任务入队到何处（链表头还是尾）
        rcu_preempt_ctxt_queue(rnp, rdp);  // 基于 GP/EXP 状态决策

        // 通知 RCU 此 CPU 的静默态（尽管任务被阻塞）
        rcu_qs();
    }
}
```

任务入队决策由 `rcu_preempt_ctxt_queue()`（`tree_plugin.h:162-279`）根据当前的 GP 状态、EXP 状态、CPU 阻塞状态进行 10 种情况的分支判断。

### 4.3 优先级提升实现（`tree_plugin.h:1154-1212`）

```c
// tree_plugin.h:1154-1212
static int rcu_boost(struct rcu_node *rnp)
{
    // 优先提升阻塞加速 GP 的任务
    if (rnp->exp_tasks != NULL)
        tb = rnp->exp_tasks;
    else
        tb = rnp->boost_tasks;

    // 通过制造 rt_mutex 来提升目标任务的优先级
    // 任务 t 在外层 rcu_read_unlock 时会释放这个 rt_mutex
    t = container_of(tb, struct task_struct, rcu_node_entry);
    rt_mutex_init_proxy_locked(&rnp->boost_mtx.rtmutex, t);
    raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
    rt_mutex_lock(&rnp->boost_mtx);    // 这会提升 t 的优先级!
    rt_mutex_unlock(&rnp->boost_mtx);
    rnp->n_boosts++;
}
```

### 4.4 Boost kthread（`tree_plugin.h:1217-1247`）

每个 leaf `rcu_node` 有一个 boost kthread（`rcub/N`）：

```c
// tree_plugin.h:1217-1247
static int rcu_boost_kthread(void *arg)
{
    struct rcu_node *rnp = (struct rcu_node *)arg;
    for (;;) {
        WRITE_ONCE(rnp->boost_kthread_status, RCU_KTHREAD_WAITING);
        rcu_wait(READ_ONCE(rnp->boost_tasks) || READ_ONCE(rnp->exp_tasks));
        WRITE_ONCE(rnp->boost_kthread_status, RCU_KTHREAD_RUNNING);
        more2boost = rcu_boost(rnp);  // 提升一个任务
        if (spincnt > 10) {
            // 连续提升 10 次后 yield
            schedule_timeout_idle(2);
            spincnt = 0;
        }
    }
}
```

### 4.5 启动提升（`tree_plugin.h:1259-1282`）

```c
// tree_plugin.h:1259-1282
static void rcu_initiate_boost(struct rcu_node *rnp, unsigned long flags)
{
    if (!rnp->boost_kthread_task ||
        (!rcu_preempt_blocked_readers_cgp(rnp) && !rnp->exp_tasks)) {
        raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
        return;
    }
    // 加速 GP 总是立刻提升
    // 正常 GP 需要等待 RCU_BOOST_DELAY (默认 500ms)
    if (rnp->exp_tasks != NULL ||
        (rnp->gp_tasks != NULL && rnp->boost_tasks == NULL &&
         rnp->qsmask == 0 && time_after(jiffies, rnp->boost_time))) {
        if (rnp->exp_tasks == NULL)
            rnp->boost_tasks = rnp->gp_tasks;
        rcu_wake_cond(rnp->boost_kthread_task, ...);
    }
}
```

### 4.6 `rcu_preempt_deferred_qs_irqrestore()` — 解锁特殊处理（`tree_plugin.h:477-589`）

当一个被阻塞的任务最终执行 `rcu_read_unlock()` 时：

```
__rcu_read_unlock()
  → rcu_read_unlock_special(t)
    → rcu_preempt_deferred_qs_irqrestore(t, flags)
        ├─ 处理 need_qs 标志 → 报告正常 QS
        ├─ 处理 cpu_no_qs.b.exp → 报告加速 QS
        └─ 处理 blocked 标志
            ├─ 从 rcu_node->blkd_tasks 链表移除
            ├─ 更新 gp_tasks / exp_tasks / boost_tasks 指针
            ├─ 如果最后一任务 → 报告 QS 到上级
            └─ 如果持有 boost_mtx → 释放
```

### 4.7 启动时配置公告（`tree_plugin.h:45-112`）

`rcu_bootup_announce_oddness()` 在启动时打印 fanout、boost、回调水线、softirq/kthread 模式等非默认配置，帮助确认当前 RCU 子系统的行为参数。

---

## 5. 诊断工具速查

| 工具/接口 | 来源 | 作用 |
|-----------|------|------|
| `rcu_cpu_stall_timeout` | `/sys/module/rcupdate/parameters/` | 调整 stall 检测超时 (3-300s) |
| `panic_on_rcu_stall` | `/proc/sys/kernel/panic_on_rcu_stall` | stall 时触发 panic |
| `max_rcu_stall_to_panic` | `/proc/sys/kernel/max_rcu_stall_to_panic` | 容忍 N 次 stall 后 panic |
| `rcu_stall_count` | `/sys/kernel/rcu_stall_count` | stall 事件累计计数 |
| `rcu_nocbs=` | 启动参数 | 指定 NO_CB CPU 列表 |
| `rcu_nocb_poll` | 启动参数 | NO_CB kthread 轮询模式 |
| `show_rcu_gp_kthreads()` | sysrq-x / debugfs | 显示 GP kthread 状态 |
| `rcu_expedited` | `/sys/kernel/rcu_expedited` | 强制使用加速 GP |
| `exp_holdoff` | `/sys/module/srcutree/parameters/` | SRCU 自动加速阈值 (ns) |

### 5.1 关键 Kconfig 选项

```
CONFIG_RCU_STALL_COMMON          -- stall 警告基础支持
CONFIG_RCU_CPU_STALL_TIMEOUT    -- stall 超时秒数 (默认 21)
CONFIG_PROVE_RCU                -- 额外 5*HZ 容限 + lockdep 检查
CONFIG_RCU_BOOST                -- PREEMPT_RCU 优先级提升
CONFIG_RCU_BOOST_DELAY          -- 提升延迟 (默认 500ms)
CONFIG_RCU_NOCB_CPU             -- NO_CB 回调卸载支持
CONFIG_RCU_EXPERT               -- 显示专家级 sysctl 参数
CONFIG_RCU_LAZY                 -- 懒惰回调批处理
CONFIG_RCU_STRICT_GRACE_PERIOD  -- 严格 GP: 所有 GP 都当作加速 GP
```

---

## 系列结语

本系列四篇文章深度解析了 Linux 内核 RCU 子系统的四个维度：

| 篇目 | 内容 | 核心文件 |
|------|------|---------|
| 第一篇 | 核心 API 与 Tree RCU 架构 | `rcupdate.h`, `tree.c`, `tree.h` |
| 第二篇 | 回调卸载、分段链表与 GP 状态机 | `rcu_segcblist.h`, `tree.c` |
| 第三篇 | SRCU、Tasks RCU 与 Tiny RCU 变体 | `srcutree.c`, `tasks.h`, `tiny.c` |
| 第四篇 | 拖延检测、加速 GP 与回调卸载 | `tree_stall.h`, `tree_exp.h`, `tree_nocb.h`, `tree_plugin.h` |

RCU 是 Linux 内核中最精妙的同步机制之一。理解其内部实现不仅有助于编写正确的内核代码，更能帮助开发者利用各种诊断接口定位生产环境中的 RCU 相关问题。

涉及的核心源码文件总计（去重后 9 个文件，~9437 行）：

```
kernel/rcu/srcutree.c  (2203 lines)  -- SRCU 树形实现
kernel/rcu/tasks.h     (1608 lines)  -- Tasks RCU 实现
kernel/rcu/tiny.c       (252 lines)  -- Tiny RCU for UP
kernel/rcu/tree_stall.h (1185 lines) -- Stall 检测与报告
kernel/rcu/tree_exp.h   (1118 lines) -- 加速 GP
kernel/rcu/tree_nocb.h  (1702 lines) -- NO_CB 回调卸载
kernel/rcu/tree_plugin.h(1369 lines) -- 抢占 RCU + 优先级提升
```
