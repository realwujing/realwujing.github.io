# 第二篇：Soft Lockup — hrtimer 驱动与 is_softlockup 判定

> 源码：`kernel/watchdog.c` | 头文件：`include/linux/nmi.h`

系列目录：[Hard/Soft Lockup 内核源码深度解析](./README.md)

---

## 1. 软锁检测原理

软锁（Soft Lockup）指 CPU 长时间无法调度——内核无法切出当前上下文。检测机制不是看"CPU 在干什么"，而是看"CPU 有没有调度过"：

```
检测通路:
  hrtimer 定时触发 ──→ watchdog_timer_fn()
                          ├─ 启动 stop_one_cpu_nowait(softlockup_fn)
                          │   └─ 成功后 → update_touch_ts()（更新"我还活着"时间戳）
                          └─ 数秒后检查 ──→ is_softlockup()
                              └─ touch_ts 过旧？→ BUG
```

核心数据结构（per-CPU，均在 `watchdog.c`）：

```c
// [watchdog.c:416]  最后一次成功调度的时刻
static DEFINE_PER_CPU(unsigned long, watchdog_touch_ts);

// [watchdog.c:418]  最后一次打印 softlockup 报告的时刻
static DEFINE_PER_CPU(unsigned long, watchdog_report_ts);

// [watchdog.c:419]  每个 CPU 的 hrtimer
static DEFINE_PER_CPU(struct hrtimer, watchdog_hrtimer);

// [watchdog.c:420]  同步 touch 标志（被 touch_softlockup_watchdog_sync 设置）
static DEFINE_PER_CPU(bool, softlockup_touch_sync);

// [watchdog.c:774]  softlockup_fn 完成的同步点
static DEFINE_PER_CPU(struct completion, softlockup_completion);

// [watchdog.c:775]  CPU stop 工作结构体
static DEFINE_PER_CPU(struct cpu_stop_work, softlockup_stop_work);
```

---

## 2. 采样周期的作用

回顾第一篇中的 `set_sample_period()`（`watchdog.c:667`）：

```
sample_period = (watchdog_thresh * 2) * (NSEC_PER_SEC / 5)
              = 4 秒  (默认 watchdog_thresh=10)
```

关键阈值关系：

| 参数 | 默认值 | 含义 |
|------|--------|------|
| `watchdog_thresh` | 10s | 用户可配阈值 |
| `get_softlockup_thresh()` | 20s | 软锁报告阈值 = `watchdog_thresh * 2` |
| `sample_period` | 4s | hrtimer 触发间隔 |
| 软锁判定时间 | 20s | `now - touch_ts > get_softlockup_thresh()` |

一个完整检测周期包含 5 个 sample_period（20s ÷ 4s = 5 个 tick）。每个 tick 都执行 `watchdog_timer_fn()`。前 4 个 tick 如果 `softlockup_fn` 能跑完，`touch_ts` 被刷新，`is_softlockup()` 返回 0。第 5 个 tick 如果 CPU 仍被卡住（`touch_ts` 未更新），判定为软锁。

---

## 3. get_timestamp() — 时间精度

```c
// [watchdog.c:662-665]
/*
 * Returns seconds, approximately.  We don't need nanosecond
 * resolution, and we don't need to waste time with a big divide when
 * 2^30ns == 1.074s.
 */
static unsigned long get_timestamp(void)
{
    return running_clock() >> 30LL;  /* 2^30 ~= 10^9 */
}
```

- `running_clock()` 返回纳秒值（基于 CLOCK_MONOTONIC）
- 右移 30 位 = 除以 2^30 ≈ 1.074s，**近似秒但不需要昂贵的除法**
- 精度对 lockup 检测足够——锁定秒级而非纳秒级

---

## 4. Touch 函数 — 心跳刷新

### 4.1 update_touch_ts()

```c
// [watchdog.c:686-690]
static void update_touch_ts(void)
{
    __this_cpu_write(watchdog_touch_ts, get_timestamp());
    update_report_ts();          // 同时刷新报告时间戳
}
```

这是**内部刷新函数**，被 `softlockup_fn` 调用。当 CPU 能成功调度并执行函数时，表示并未锁死。

### 4.2 update_report_ts()

```c
// [watchdog.c:680-683]
static void update_report_ts(void)
{
    __this_cpu_write(watchdog_report_ts, get_timestamp());
}
```

刷新 `watchdog_report_ts`——这个时间戳有两重作用：
1. 作为 `is_softlockup()` 中 `period_ts` 的来源——标记本次检查周期的起点
2. 作为 `watchdog_timer_fn()` 的 DELAY_REPORT 检查——见 6.1 节

### 4.3 touch_softlockup_watchdog_sched()

```c
// [watchdog.c:700-707]
notrace void touch_softlockup_watchdog_sched(void)
{
    /*
     * Preemption can be enabled.  It doesn't matter which CPU's watchdog
     * report period gets restarted here, so use the raw_ operation.
     */
    raw_cpu_write(watchdog_report_ts, SOFTLOCKUP_DELAY_REPORT);
}
```

被调度器在**合法停滞**时调用（如进入 idle）。设置 `SOFTLOCKUP_DELAY_REPORT` 特殊值告诉 `watchdog_timer_fn`："跳过本次检查，等我恢复"。

### 4.4 touch_softlockup_watchdog()

```c
// [watchdog.c:709-714]
notrace void touch_softlockup_watchdog(void)
{
    touch_softlockup_watchdog_sched();
    wq_watchdog_touch(raw_smp_processor_id());
}
EXPORT_SYMBOL(touch_softlockup_watchdog);
```

通用外部 API，分两步：
1. `touch_softlockup_watchdog_sched()` — 刷新 `report_ts` 为 DELAY_REPORT
2. `wq_watchdog_touch()` — 通知 workqueue watchdog（独立子系统）

任何长时间运行的内核代码**必须**周期性地调用此函数来刷新心跳。典型的调用点：
- `rcu_sched_clock_irq()` — RCU 长时间操作
- `debug_objects` — 大批量调试对象操作
- `spin_dump()` — 自旋锁调试 dump

### 4.5 touch_all_softlockup_watchdogs()

```c
// [watchdog.c:716-733]
void touch_all_softlockup_watchdogs(void)
{
    int cpu;

    for_each_cpu(cpu, &watchdog_allowed_mask) {
        per_cpu(watchdog_report_ts, cpu) = SOFTLOCKUP_DELAY_REPORT;
        wq_watchdog_touch(cpu);
    }
}
```

遍历所有受监控 CPU，全部设置 DELAY_REPORT。用于全局操作如 `sysrq` 和 `suspend/resume` 路径。

### 4.6 touch_softlockup_watchdog_sync()

```c
// [watchdog.c:735-739]
void touch_softlockup_watchdog_sync(void)
{
    __this_cpu_write(softlockup_touch_sync, true);
    __this_cpu_write(watchdog_report_ts, SOFTLOCKUP_DELAY_REPORT);
}
```

与 `touch_softlockup_watchdog_sched()` 的区别：额外设置 `softlockup_touch_sync = true`，告诉 `watchdog_timer_fn` 在恢复时需要调用 `sched_clock_tick()` 同步调度器时钟。

**使用场景对比：**

| 函数 | report_ts | softlockup_touch_sync | 调度器时钟同步 | 应用场景 |
|------|-----------|----------------------|----------------|----------|
| touch_softlockup_watchdog | DELAY_REPORT | — | 否 | 通用长时间操作 |
| touch_softlockup_watchdog_sched | DELAY_REPORT | — | 否 | 调度器 idle 进入 |
| touch_softlockup_watchdog_sync | DELAY_REPORT | true | 是 | CPU 长时间停止（如虚拟机暂停） |
| update_touch_ts | get_timestamp() | — | 否 | softlockup_fn 内部刷新 |

---

## 5. softlockup_fn() — 调度证明

```c
// [watchdog.c:785-793]
/*
 * The watchdog feed function - touches the timestamp.
 *
 * It only runs if the local CPU can schedule — forced by stop_one_cpu_nowait().
 */
static int softlockup_fn(void *data)
{
    update_touch_ts();                            // line 789
    complete(this_cpu_ptr(&softlockup_completion)); // line 790
    return 0;                                     // line 791
}
```

这是一个**极小化**函数：只做两件事——更新时间戳和 complete 完成量。它的价值在于**能否被调度运行**：
- 能运行 → CPU 未锁死 → `touch_ts` 被刷新
- 不能运行 → CPU 繁忙/死锁 → `touch_ts` 不变 → 检测触发

---

## 6. watchdog_timer_fn() — 主循环

```c
// [watchdog.c:795-911]
static enum hrtimer_restart watchdog_timer_fn(struct hrtimer *hrtimer)
```

这是整个 softlockup 检测的核心入口。以下按代码执行流分段剖析。

### 6.1 早期退出路径

```c
// [watchdog.c:803-810]
if (!watchdog_enabled)
    return HRTIMER_NORESTART;

if (panic_in_progress())
    return HRTIMER_NORESTART;
```

两个立即退出条件：
- `watchdog_enabled == 0`：watchdog 已关闭（`lockup_detector_soft_poweroff` 或 sysctl）
- `panic_in_progress()`：panic 过程中不继续监测，避免嵌套 panic

### 6.2 硬锁联动 (line 815)

```c
watchdog_hardlockup_kick();
```

在 `CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER` 下：

```c
// [watchdog.c:199-205]
static void watchdog_hardlockup_kick(void)
{
    int new_interrupts;

    new_interrupts = atomic_inc_return(this_cpu_ptr(&hrtimer_interrupts));
    watchdog_buddy_check_hardlockup(new_interrupts);
}
```

- `atomic_inc_return(hrtimer_interrupts)` — hrtimer 心跳+1，perf NMI 路径用它判断硬锁
- `watchdog_buddy_check_hardlockup()` — buddy 模式下检查下一个 CPU（详见第四篇）

### 6.3 启动 softlockup_fn (line 818)

```c
// [watchdog.c:818-823]
if (completion_done(this_cpu_ptr(&softlockup_completion))) {
    reinit_completion(this_cpu_ptr(&softlockup_completion));
    stop_one_cpu_nowait(smp_processor_id(),
                        softlockup_fn, NULL,
                        this_cpu_ptr(&softlockup_stop_work));
}
```

`stop_one_cpu_nowait` 的工作方式：

```
stop_one_cpu_nowait(cpu, fn)
  ├─ 在目标 CPU 上排队 cpu_stop 工作
  ├─ 发送 IPI (Inter-Processor Interrupt) 或设置标志
  └─ 目标 CPU 在处理 IPI / 调度点时执行 fn
      └─ fn = softlockup_fn → update_touch_ts()
                              → complete(completion)
```

**为什么用 `completion_done()` 检查？**
- 如果上一次的 `softlockup_fn` 还没完成（completion 未 done），说明 CPU 已被卡住
- 此时不再启动新的 `stop_one_cpu_nowait`，避免积压
- 下一次 hrtimer tick 会再次检查——如果 completion 一直未 done → 软锁

### 6.4 重新启动 hrtimer (line 826)

```c
hrtimer_forward_now(hrtimer, ns_to_ktime(sample_period));
```

将 hrtimer 的过期时间从**此刻**前移一个 `sample_period`（4s）。使用 `_forward_now` 而不是 `_start` 避免累积漂移。

### 6.5 时间戳读取 (line 833-844)

```c
now = get_timestamp();                          // [line 833]

kvm_check_and_clear_guest_paused();             // [line 838]
// 若虚拟机被 host 暂停，此函数刷新时间戳避免误报

period_ts = READ_ONCE(*this_cpu_ptr(&watchdog_report_ts)); // [line 844]
```

`period_ts` 是"本次检测周期的起点"——它来自 `watchdog_report_ts`，在以下时机被更新：
- `update_report_ts()` — softlockup_fn 成功后的正常更新
- `touch_softlockup_watchdog_sched()` — 设置为 `SOFTLOCKUP_DELAY_REPORT`
- 检测到软锁后 — `watchdog_timer_fn` 自身在 line 881 更新

### 6.6 DELAY_REPORT 处理 (line 849)

```c
// [watchdog.c:849-861]
if (period_ts == SOFTLOCKUP_DELAY_REPORT) {
    if (unlikely(__this_cpu_read(softlockup_touch_sync))) {
        __this_cpu_write(softlockup_touch_sync, false);
        sched_clock_tick();                  // 同步调度器时钟
    }

    update_report_ts();                      // 重置 period_ts
    return HRTIMER_RESTART;
}
```

当 `report_ts` 被某个 `touch_*` 函数设置为 `SOFTLOCKUP_DELAY_REPORT` 时，跳过本轮检查：
- 刷新 `report_ts` 为当前时间（开启新的检测周期）
- 如果 `softlockup_touch_sync` 被设置，先同步调度器时钟再清除标志
- 直接返回 `HRTIMER_RESTART`

### 6.7 中断风暴统计 (line 846)

```c
update_cpustat();                              // [watchdog.c:486]
```

在 `CONFIG_SOFTLOCKUP_DETECTOR_INTR_STORM` 配置下启用，详见第 8 节。

### 6.8 核心判定 (line 864-908)

```c
touch_ts = __this_cpu_read(watchdog_touch_ts);
duration = is_softlockup(touch_ts, period_ts, now);
if (unlikely(duration)) {
    // ========== 软锁检测到 ==========
#ifdef CONFIG_SYSFS
    ++softlockup_count;
#endif

    // 全 CPU backtrace 互斥
    if (softlockup_all_cpu_backtrace) {
        if (test_and_set_bit_lock(0, &soft_lockup_nmi_warn))
            return HRTIMER_RESTART;  // 另一个 CPU 正在 dump
    }

    update_report_ts();  // 启动下一个检测周期

    // 打印报告
    printk_cpu_sync_get_irqsave(flags);
    pr_emerg("BUG: soft lockup - CPU#%d stuck for %us! [%s:%d]\n",
             smp_processor_id(), duration,
             current->comm, task_pid_nr(current));
    report_cpu_status();           // 打印 CPU 利用率/中断统计
    print_modules();               // 打印已加载模块
    print_irqtrace_events(current); // 打印中断/抢占追踪
    if (regs)
        show_regs(regs);
    else
        dump_stack();
    printk_cpu_sync_put_irqrestore(flags);

    // 全 CPU backtrace（可选）
    if (softlockup_all_cpu_backtrace) {
        trigger_allbutcpu_cpu_backtrace(smp_processor_id());
        if (!softlockup_panic)
            clear_bit_unlock(0, &soft_lockup_nmi_warn);
    }

    add_taint(TAINT_SOFTLOCKUP, LOCKDEP_STILL_OK);  // 污染标志
    sys_info(softlockup_si_mask & ~SYS_INFO_ALL_BT); // 打印系统信息

    thresh_count = duration / get_softlockup_thresh(); // 持续了多少个 thresh 周期
    if (softlockup_panic && thresh_count >= softlockup_panic)
        panic("softlockup: hung tasks");
}

return HRTIMER_RESTART;
```

**`softlockup_panic` 逻辑**：不是检测到就 panic，而是持续了 `softlockup_panic` 个阈值周期才 panic（默认 softlockup_panic=0 不 panic）。

**TAINT_SOFTLOCKUP**：污染内核，在产品环境中标记内核曾发生过软锁。

---

## 7. is_softlockup() — 判定核心

```c
// [watchdog.c:741-771]
static int is_softlockup(unsigned long touch_ts,
                         unsigned long period_ts,
                         unsigned long now)
{
    if ((watchdog_enabled & WATCHDOG_SOFTOCKUP_ENABLED) && watchdog_thresh) {

        // 步骤1: 中间检查点 — 可能中断风暴
        if (time_after_eq(now, period_ts + get_softlockup_thresh() / NUM_SAMPLE_PERIODS) &&
            need_counting_irqs())
            start_counting_irqs();

        // 步骤2: 3/4 阈值 — sched_ext BPF 调度器弹射
        if (time_after_eq(now, period_ts + get_softlockup_thresh() * 3 / 4))
            scx_softlockup(now - touch_ts);

        // 步骤3: 最终判定 — 超过阈值
        if (time_after(now, period_ts + get_softlockup_thresh()))
            return now - touch_ts;   // 返回持续时长（给 panic 判断用）
    }
    return 0;  // 无锁
}
```

### 逐步判定分解：

```
时间轴 (默认 thresh=10):

period_ts (检测周期起点)
  │
  │─ 1/5 thresh (4s) ─────── 开始计数中断 [步骤1]
  │   只有 need_counting_irqs() 时才会启动
  │
  │─ 3/4 thresh (15s) ───── scx_softlockup [步骤2]
  │   给 sched_ext BPF 调度器一次自救机会
  │
  │─ full thresh (20s) ───── BUG 判定 [步骤3]
  │   time_after(now, period_ts + get_softlockup_thresh())
  │   → return now - touch_ts (> 0)
  ▼
```

关键设计细节：

1. **步骤1 (1/5 thresh = 4s)**：如果在第一个 sample_period 结束后 `period_ts` 没有被更新（意味着 `softlockup_fn` 这轮没跑完），且系统配置了中断风暴检测，就开始采样 IRQ 统计。

2. **步骤2 (3/4 thresh = 15s)**：调用 `scx_softlockup()`，给 sched_ext BPF 调度器一次"弹射"机会。如果软锁是由 BPF 调度器引起的，这个钩子可以主动中止 BPF 调度器。

3. **步骤3 (full thresh = 20s)**：最终判定。`time_after(now, period_ts + get_softlockup_thresh())` 检查从检测周期开始到现在是否超过了软锁阈值。返回值 `now - touch_ts` 是 CPU 实际被卡住的时长（约等于 `now - period_ts`，因为 `touch_ts` 在 `period_ts` 之前或同时被更新）。

### 为什么 `touch_ts` 和 `period_ts` 两个时间戳？

```
touch_ts:    最后一次 softlockup_fn 成功运行的时刻（即 CPU 最后一次证明自己能调度）
period_ts:   当前检测周期的起点（= 上一次更新 report_ts 的时刻）
now:         当前时刻

正常情况下: touch_ts ≈ period_ts（都在同一次 tick 内被更新）
锁死情况下: touch_ts << period_ts（CPU 在周期开始前就卡住了，softlockup_fn 从未运行）
```

---

## 8. 检测时间线实例

### 8.1 正常运行

```
T=0s   hrtimer fires
       ├─ watchdog_hardlockup_kick()
       ├─ stop_one_cpu_nowait(softlockup_fn)
       │   └─ CPU 调度到 → update_touch_ts(touch_ts=T0)
       │                   update_report_ts(report_ts=T0)
       │                   complete(completion)
       ├─ period_ts = report_ts = T0
       └─ is_softlockup(touch_ts=T0, period_ts=T0, now=T0)
           → time_after(T0, T0+20s) = false → return 0

T=4s   hrtimer fires
       ├─ completion_done() → true
       │   └─ stop_one_cpu_nowait(softlockup_fn) → touch_ts=T4
       │                   update_report_ts → report_ts=T4
       ├─ period_ts = report_ts = T4
       └─ is_softlockup(touch_ts=T4, period_ts=T4, now=T4)
           → return 0

T=8s   同上 → return 0
T=12s  同上 → return 0
T=16s  同上 → return 0
T=20s  同上 → return 0
...
```

### 8.2 检测到软锁

```
T=0s   hrtimer fires
       ├─ stop_one_cpu_nowait(softlockup_fn) → touch_ts=T0
       └─ is_softlockup → return 0

T=4s   hrtimer fires
       ├─ completion_done() → true (上一次的已完成)
       │   └─ stop_one_cpu_nowait(softlockup_fn)  ← CPU 能调度
       │       → touch_ts=T4
       └─ is_softlockup → return 0

────────────────  CPU 开始繁忙  ────────────────

T=8s   hrtimer fires
       ├─ completion_done() → true (T4s的已完成)
       │   └─ stop_one_cpu_nowait(softlockup_fn)
       │       → 但是 CPU 卡在 spinlock 里，无法调度到 stop work
       │       → completion 不会 done
       ├─ period_ts = watchdog_report_ts = T4s
       │   (注意：report_ts 仍是 T4s 的值，
       │    因为本次 stop_one_cpu_nowait 还没完成 update_report_ts)
       └─ is_softlockup(touch_ts=T4, period_ts=T4, now=T8)
           → time_after(T8, T4+20s) = false
           → 但步骤1: T8 >= T4+4s → 是 → start_counting_irqs()
           → return 0 (尚未超 threshold)

T=12s  hrtimer fires
       ├─ completion_done() → false (T8s启动的还没完成)
       │   └─ 不再启动新的 stop_one_cpu_nowait
       ├─ period_ts = T4s (仍未更新)
       └─ is_softlockup(touch_ts=T4, period_ts=T4, now=T12)
           → time_after(T12, T4+20s) = false → return 0
           (已触发步骤1中断计数)

T=16s  hrtimer fires
       ├─ completion_done() → false
       ├─ period_ts = T4s
       └─ is_softlockup(touch_ts=T4, period_ts=T4, now=T16)
           → 步骤2: T16 >= T4+15s → scx_softlockup()
           → time_after(T16, T4+20s) = false → return 0

T=20s  hrtimer fires
       ├─ completion_done() → false
       ├─ period_ts = T4s
       └─ is_softlockup(touch_ts=T4, period_ts=T4, now=T20)
           → time_after(T20, T4+20s) = true
           → return T20 - T4 = 16s
           → 触发 BUG 报告!
```

### 8.3 软锁状态机

```
                    ┌──────────┐
                    │ NORMAL   │  touch_ts 不断更新
                    │  (初始)  │
                    └────┬─────┘
                         │ softlockup_fn 无法调度 (completion 未 done)
                         ▼
                    ┌──────────┐
                    │ WATCHING │  period_ts 跟踪起点，累积时长
                    │ (4s-16s) │
                    └────┬─────┘
           ┌─────────────┼─────────────┐
           │ 4s后         │ 15s后        │ 20s后
           ▼              ▼              ▼
    ┌──────────────┐ ┌──────────┐ ┌──────────────┐
    │ 启动IRQ计数   │ │ scx自救  │ │ BUG 报告     │
    │ start_count  │ │ 弹射尝试  │ │ pr_emerg()   │
    │ _irqs()      │ │          │ │ panic?       │
    └──────────────┘ └──────────┘ └──────────────┘
```

---

## 9. 中断风暴检测

在 `CONFIG_SOFTLOCKUP_DETECTOR_INTR_STORM` 启用的内核中，软锁检测联动中断风暴分析。

### 9.1 CPU 利用率快照

```c
// [watchdog.c:486-515]
static void update_cpustat(void)
{
    int i;
    u8 util;
    u16 old_stat, new_stat;
    struct kernel_cpustat kcpustat;
    u64 *cpustat = kcpustat.cpustat;
    u8 tail = __this_cpu_read(cpustat_tail);
    u16 sample_period_16 = get_16bit_precision(sample_period);

    kcpustat_cpu_fetch(&kcpustat, smp_processor_id());

    for (i = 0; i < NUM_STATS_PER_GROUP; i++) {
        old_stat = __this_cpu_read(cpustat_old[i]);
        new_stat = get_16bit_precision(cpustat[tracked_stats[i]]);
        util = DIV_ROUND_UP(100 * (new_stat - old_stat), sample_period_16);
        if (util > 100)
            util = 100;   // 防止舍入导致超过100%
        __this_cpu_write(cpustat_util[tail][i], util);
        __this_cpu_write(cpustat_old[i], new_stat);
    }

    __this_cpu_write(cpustat_tail, (tail + 1) % NUM_SAMPLE_PERIODS);
}
```

每个 hrtimer tick 采集一次 snapshot：
- `kcpustat_cpu_fetch()` — 获取当前 CPU 的 kernel cpustat（user/sys/irq/softirq/steal 等的 tick 计数）
- `get_16bit_precision()` — 将纳秒转为 16ms 精度（2^24ns ≈ 16.8ms），用一个 u16 存储，约每 1000 秒回绕
- 计算每个统计项的利用率 = `100 * (new - old) / sample_period`
- 存储到 `cpustat_util[tail][i]` — 循环缓冲区，保留 NUM_SAMPLE_PERIODS(5) 个快照

### 9.2 中断统计打印

当 `is_softlockup()` 的步骤1触发 `start_counting_irqs()`，后续的 hrtimer tick 会开始统计每种 IRQ 的中断次数。检测到软锁时：

```c
// [watchdog.c:632-636]
static void report_cpu_status(void)
{
    print_cpustat();     // 打印 CPU 利用率快照历史
    print_irq_counts();  // 打印中断频率排序
}
```

`print_irq_counts()`（`watchdog.c:593`）打印最近 5 个采样周期内每种 IRQ 的中断次数，帮助诊断**中断风暴**导致的软锁：
- 中断频率过高 → CPU 无法执行用户态/内核态正常代码 → 表现为软锁
- 典型症状：某个 IRQ 中断次数占 90%+ CPU 时间

---

## 10. 关键常量和阈值速查

| 常量 | 定义 | 默认值 | 说明 |
|------|------|--------|------|
| `NUM_SAMPLE_PERIODS` | `watchdog.c:44` | 5 | sample_period 分割数 |
| `watchdog_thresh` | `watchdog.c:50` | 10 s | 硬锁阈值 |
| `get_softlockup_thresh()` | `watchdog.c:652` | 20 s | 软锁阈值 = thresh × 2 |
| `sample_period` | `watchdog.c:667` | 4 s | hrtimer 间隔 |
| `SOFTLOCKUP_DELAY_REPORT` | 特殊值 | — | 延迟报告标记 |
| `NUM_STATS_PER_GROUP` | cpustat 宏 | 5 | 统计项数 |
| `HARDIRQ_PERCENT_THRESH` | irq 计数 | 50% | 硬中断占用阈值 |

---

## 11. 诊断信息解读

一条完整的软锁报告：

```
BUG: soft lockup - CPU#3 stuck for 23s! [kswapd0:115]
```

- `CPU#3` — 发生软锁的 CPU 编号
- `stuck for 23s` — `duration = now - touch_ts`，实际锁死时长
- `[kswapd0:115]` — 当前在该 CPU 上运行的进程名和 PID
- 23s > 20s (get_softlockup_thresh) → 触发报告

随后：
- `report_cpu_status()` → CPU 利用率历史 + IRQ 风暴分析
- `print_modules()` → 排查是否模块引起
- `show_regs()` / `dump_stack()` → 锁死点的内核栈
- `trigger_allbutcpu_cpu_backtrace()` → 所有其他 CPU 的 backtrace（排查锁争用）

---

## 下一篇文章

[第三篇：Hard Lockup (Perf) — NMI 事件与溢出回调](./03-hard-lockup-perf.md)
