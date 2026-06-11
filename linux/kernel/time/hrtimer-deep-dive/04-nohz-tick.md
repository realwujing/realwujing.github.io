# 第4篇：NO_HZ 与动态时钟 (tick-sched)

> 源码：kernel/time/tick-sched.c (1716行) | 头文件：kernel/time/tick-sched.h (124行) | 相关：kernel/time/clocksource.c (1600行)

系列目录：[hrtimer 内核源码深度解析](./README.md)

---

## 1. 引言

前几篇我们分析了 hrtimer 和 timer wheel 两种定时器引擎。但在现代内核中，一个关键问题浮出水面：**当 CPU 空闲时，是否还有必要每毫秒触发一次时钟中断？**

答案是否定的。NO_HZ（动态时钟，dyntick）机制使得空闲 CPU 可以完全关闭周期性 tick，直到有实际事件需要处理。本篇深入 tick-sched 子系统——包含了 tick 模拟、idle 时停止 tick、sched_timer 的 hrtimer 实现，以及 clocksource 的评选和监控。

---

## 2. tick-sched 架构概览

### 2.1 问题：为什么需要动态时钟？

```
传统周期性 tick (HZ=1000):
   CPU0: [tick][tick][tick][tick][tick][tick]...  ← 每秒 1000 次中断
   CPU1: [tick][tick][tick][tick][tick][tick]...  ← 即使 idle 也触发
   CPU2: [tick][tick][tick][tick][tick][tick]...
   CPU3: [tick][tick][tick][tick][tick][tick]...

   浪费：空闲 CPU 被频繁唤醒，消耗电能
   尤其在虚拟化和移动设备上问题严重

动态时钟 (dyntick/nohz):
   CPU0: [tick]..................[tick]............  ← 只在必要时触发
   CPU1: ........[tick]...........................
   CPU2: [tick][tick][tick].......................  ← 忙 CPU 保持 tick
   CPU3: ........................................[tick]
```

### 2.2 核心组件

```
                        tick-sched 子系统
┌───────────────────────────────────────────────────────────────┐
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │              struct tick_sched (per-CPU)                 │ │
│  │  ┌─────────────────────────────────────────────────┐    │ │
│  │  │  sched_timer (struct hrtimer)                   │    │ │
│  │  │  flags (STOPPED/INIDLE/HIGHRES/NOHZ/...)       │    │ │
│  │  │  last_tick, next_tick, idle_* stats             │    │ │
│  │  │  tick_dep_mask                                  │    │ │
│  │  └─────────────────────────────────────────────────┘    │ │
│  │                                                         │ │
│  │  关键函数:                                              │ │
│  │  tick_nohz_handler()    ← hrtimer 回调（tick 模拟）    │ │
│  │  tick_nohz_idle_enter() ← 进入 idle                    │ │
│  │  tick_nohz_idle_exit()  ← 退出 idle                    │ │
│  │  tick_nohz_stop_tick()  ← 停止 tick                    │ │
│  │  tick_nohz_restart_sched_tick() ← 恢复 tick            │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  依赖:                                                        │
│  ┌──────────────────────┐  ┌──────────────────────────────┐  │
│  │ hrtimer 子系统        │  │ clock_event_device           │  │
│  │ (sched_timer 是高精度) │  │ (one-shot 编程 next_event)   │  │
│  └──────────────────────┘  └──────────────────────────────┘  │
│                                                               │
│  依赖:                                                        │
│  ┌──────────────────────────────────────────────────────┐    │
│  │ clocksource 子系统 (clocksource.c)                    │    │
│  │ - 评选最佳时钟源                                      │    │
│  │ - watchdog: 检测时钟源漂移和不稳定性                  │    │
│  │ - suspend/resume: 休眠时切换/恢复时钟源               │    │
│  └──────────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────────┘
```

---

## 3. struct tick_sched

源码位置：`kernel/time/tick-sched.h:64-103`

```cpp
struct tick_sched {
    /* Common flags */
    unsigned long           flags;

    /* Tick handling: jiffies stall check */
    unsigned int            stalled_jiffies;
    unsigned long           last_tick_jiffies;

    /* Tick handling */
    struct hrtimer          sched_timer;        // hrtimer 实现 tick 模拟
    ktime_t                 last_tick;          // 上次 tick 的过期时间
    ktime_t                 next_tick;          // 下一个 tick
    unsigned long           idle_jiffies;
    ktime_t                 idle_waketime;
    unsigned int            got_idle_tick;

    /* Idle entry */
    seqcount_t              idle_sleeptime_seq;
    ktime_t                 idle_entrytime;

    /* Tick stop */
    unsigned long           last_jiffies;
    u64                     timer_expires_base;
    u64                     timer_expires;
    u64                     next_timer;
    ktime_t                 idle_expires;
    unsigned long           idle_calls;
    unsigned long           idle_sleeps;

    /* Idle exit */
    ktime_t                 idle_exittime;
    ktime_t                 idle_sleeptime;
    ktime_t                 iowait_sleeptime;

    /* Full dynticks handling */
    atomic_t                tick_dep_mask;

    /* Clocksource changes */
    unsigned long           check_clocks;
};
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| flags | unsigned long | TS_FLAG_* 位掩码（见下文） |
| sched_timer | struct hrtimer | 实现周期性 tick 模拟的高精度定时器 |
| last_tick | ktime_t | 上次 tick 的过期时间，用于恢复 tick 时计算偏移 |
| next_tick | ktime_t | dyntick 模式下的下一次 tick 时间 |
| last_jiffies | unsigned long | 上次计算 next_event 时的 jiffies 快照 |
| timer_expires | u64 | 预期的定时器过期时间（tick 停止时） |
| idle_calls / idle_sleeps | unsigned long | 统计：idle 调用次数 / tick 被停止的次数 |
| idle_sleeptime | ktime_t | idle 中 tick 停止时的累计睡眠时间 |
| iowait_sleeptime | ktime_t | idle 中有未完成 I/O 的睡眠时间（用于 iowait 统计）|
| tick_dep_mask | atomic_t | 依赖掩码（有人需要 tick 时设位） |
| check_clocks | unsigned long | clocksource 变化通知 |

**TS_FLAG_* 标志**（tick-sched.h:17-31）：

```cpp
#define TS_FLAG_INIDLE          BIT(0)   // CPU 在 idle 模式
#define TS_FLAG_STOPPED         BIT(1)   // idle tick 已停止
#define TS_FLAG_IDLE_ACTIVE     BIT(2)   // CPU 在 idle 活跃状态 (IRQ 时清除)
#define TS_FLAG_DO_TIMER_LAST   BIT(3)   // 此 CPU 是最后一个更新 jiffies 的
#define TS_FLAG_NOHZ            BIT(4)   // NO_HZ 已启用
#define TS_FLAG_HIGHRES         BIT(5)   // 高精度 tick 模式
```

---

## 4. sched_timer：用 hrtimer 模拟周期性 tick

### 4.1 初始化

源码位置：`kernel/time/tick-sched.c:1615-1642`

```cpp
void tick_setup_sched_timer(bool hrtimer)
{
    struct tick_sched *ts = this_cpu_ptr(&tick_cpu_sched);

    /* Emulate tick processing via per-CPU hrtimers: */
    hrtimer_setup(&ts->sched_timer, tick_nohz_handler,
                  CLOCK_MONOTONIC, HRTIMER_MODE_ABS_HARD);

    if (IS_ENABLED(CONFIG_HIGH_RES_TIMERS) && hrtimer)
        tick_sched_flag_set(ts, TS_FLAG_HIGHRES);

    /* Get the next period (per-CPU) */
    hrtimer_set_expires(&ts->sched_timer, tick_init_jiffy_update());

    /* Offset the tick to avert 'jiffies_lock' contention. */
    if (sched_skew_tick) {
        u64 offset = TICK_NSEC >> 1;
        do_div(offset, num_possible_cpus());
        offset *= smp_processor_id();
        hrtimer_add_expires_ns(&ts->sched_timer, offset);
    }

    hrtimer_forward_now(&ts->sched_timer, TICK_NSEC);
    if (IS_ENABLED(CONFIG_HIGH_RES_TIMERS) && hrtimer)
        hrtimer_start_expires(&ts->sched_timer, HRTIMER_MODE_ABS_PINNED_HARD);
    else
        tick_program_event(hrtimer_get_expires(&ts->sched_timer), 1);
    tick_nohz_activate(ts);
}
```

关键设计点：

1. **使用 CLOCK_MONOTONIC + HRTIMER_MODE_ABS_HARD**：tick 模拟必须是 hard timer（在硬中断中执行），保证确定性。
2. **tick skew**：为了避免多 CPU 同时触发 `jiffies_lock` 竞争，每个 CPU 的初始 tick 被微小偏移。
3. **PINNED**：tick timer 固定在当前 CPU，不允许迁移。

### 4.2 tick_nohz_handler — tick 回调

源码位置：`kernel/time/tick-sched.c:306-334`

```cpp
static enum hrtimer_restart tick_nohz_handler(struct hrtimer *timer)
{
    struct tick_sched *ts = container_of(timer, struct tick_sched, sched_timer);
    struct pt_regs *regs = get_irq_regs();
    ktime_t now = ktime_get();

    tick_sched_do_timer(ts, now);      // 更新 jiffies

    if (regs)
        tick_sched_handle(ts, regs);   // 处理进程统计、调度器更新
    else
        ts->next_tick = 0;

    // 如果 tick 已停止 (idle 模式)，不再重新入队
    if (unlikely(tick_sched_flag_test(ts, TS_FLAG_STOPPED)))
        return HRTIMER_NORESTART;

    // 前进到下一个 tick 周期
    hrtimer_forward(timer, now, TICK_NSEC);

    return HRTIMER_RESTART;  // 自动重新入队，形成周期性 tick
}
```

**周期性 tick 的流程：**

```
tick_setup_sched_timer (tick-sched.c:1615)
    │
    ├─ hrtimer_setup(&sched_timer, tick_nohz_handler, ...)
    ├─ hrtimer_start_expires → 第一次到期
    │
    ▼
┌── tick_nohz_handler (tick-sched.c:306)
│   │
│   ├─ tick_sched_do_timer → 更新 jiffies
│   ├─ tick_sched_handle → 更新进程统计/负载均衡触发
│   │
│   ├─ TS_FLAG_STOPPED?
│   │   YES → return HRTIMER_NORESTART  ← 退出周期性 tick
│   │   NO  →
│   │       ├─ hrtimer_forward(timer, now, TICK_NSEC)  ← 前进一个周期
│   │       └─ return HRTIMER_RESTART
│   │
│   └──→ 下一周期自动触发 ──┘
```

---

## 5. Idle 时停止 Tick

### 5.1 tick_nohz_idle_enter — 进入 idle

源码位置：`kernel/time/tick-sched.c:1294-1310`

```cpp
void tick_nohz_idle_enter(void)
{
    struct tick_sched *ts;

    lockdep_assert_irqs_enabled();

    local_irq_disable();

    ts = this_cpu_ptr(&tick_cpu_sched);

    WARN_ON_ONCE(ts->timer_expires_base);

    tick_sched_flag_set(ts, TS_FLAG_INIDLE);   // 标记"进入 idle"
    tick_nohz_start_idle(ts);                   // 记录 idle 进入时间

    local_irq_enable();
}
```

`tick_nohz_idle_enter` 是轻量级的"声明"——标记 `TS_FLAG_INIDLE` 和记录进入时间。此时 tick 尚未真正停止。停止发生于 idle 循环的后续步骤。

### 5.2 tick_nohz_stop_tick — 真正停止 tick

源码位置：`kernel/time/tick-sched.c:1013-1114`

```cpp
static void tick_nohz_stop_tick(struct tick_sched *ts, int cpu)
{
    struct clock_event_device *dev = __this_cpu_read(tick_cpu_device.evtdev);
    unsigned long basejiff = ts->last_jiffies;
    u64 basemono = ts->timer_expires_base;
    bool timer_idle = tick_sched_flag_test(ts, TS_FLAG_STOPPED);
    int tick_cpu;
    u64 expires;

    /* Make sure we won't be trying to stop it twice in a row. */
    ts->timer_expires_base = 0;

    // 标记 timer base 为 idle，获取下一个到期时间
    expires = timer_base_try_to_set_idle(basejiff, basemono, &timer_idle);
    if (expires > ts->timer_expires) {
        expires = ts->timer_expires;
    }

    if (!timer_idle)
        return;  // 不能进入 idle (有正在执行的定时器)

    // 放弃 jiffies 更新职责
    tick_cpu = READ_ONCE(tick_do_timer_cpu);
    if (tick_cpu == cpu) {
        WRITE_ONCE(tick_do_timer_cpu, TICK_DO_TIMER_NONE);
        tick_sched_flag_set(ts, TS_FLAG_DO_TIMER_LAST);
    }

    // 保存 tick 停止时的状态
    if (!tick_sched_flag_test(ts, TS_FLAG_STOPPED)) {
        calc_load_nohz_start();
        quiet_vmstat();
        ts->last_tick = hrtimer_get_expires(&ts->sched_timer);
        tick_sched_flag_set(ts, TS_FLAG_STOPPED);  // ← 关键！
        trace_tick_stop(1, TICK_DEP_MASK_NONE);
    }

    ts->next_tick = expires;

    // 如果在高精度模式下，取消 sched_timer
    if (tick_sched_flag_test(ts, TS_FLAG_HIGHRES))
        hrtimer_cancel(&ts->sched_timer);
    else
        tick_program_event(KTIME_MAX, 1);
}
```

**停止 tick 的关键步骤：**

```
tick_nohz_stop_tick (tick-sched.c:1013)
    │
    ├─ timer_base_try_to_set_idle:
    │   └─ 检查是否有正在执行中的定时器
    │      → 如果有，不能进入 idle
    │
    ├─ 放弃 jiffies 更新职责 (tick_do_timer_cpu)
    │   └─ 让下一个醒来的 CPU 接管 do_timer()
    │
    ├─ 如果 TS_FLAG_STOPPED 尚未设置:
    │   ├─ ts->last_tick = hrtimer_get_expires(&sched_timer)
    │   ├─ tick_sched_flag_set(STOPPED)
    │   └─ trace_tick_stop
    │
    ├─ 如果高精度模式:
    │   └─ hrtimer_cancel(&ts->sched_timer)  ← 取消周期性 tick!
    │
    └─ ts->next_tick = expires  (下次需要醒来的时间)
```

### 5.3 tick_nohz_idle_exit — 退出 idle

源码位置：`kernel/time/tick-sched.c:1491-1510`（大致范围）

```cpp
void tick_nohz_idle_exit(void)
{
    struct tick_sched *ts = this_cpu_ptr(&tick_cpu_sched);
    ktime_t now;

    local_irq_disable();

    tick_sched_flag_clear(ts, TS_FLAG_INIDLE);
    ts->inidle = 0;
    now = ktime_get();

    if (tick_sched_flag_test(ts, TS_FLAG_STOPPED)) {
        tick_nohz_restart_sched_tick(ts, now);
        tick_nohz_account_idle_time(ts, now);
    }

    local_irq_enable();
}
```

退出 idle 时：
1. 清除 `TS_FLAG_INIDLE` 标记。
2. 如果 tick 处于停止状态，调用 `tick_nohz_restart_sched_tick` 恢复。
3. 核算 idle 时间（`idle_sleeptime`、`iowait_sleeptime`）。

### 5.4 tick_nohz_restart_sched_tick — 恢复 tick

源码位置：`kernel/time/tick-sched.c:1131-1148`

```cpp
static void tick_nohz_restart_sched_tick(struct tick_sched *ts, ktime_t now)
{
    /* Update jiffies first */
    tick_do_update_jiffies64(now);

    // 清除 timer idle 标记，避免远程入队时的 IPI
    // 以及入队路径中的时钟前移检查
    timer_clear_idle();

    calc_load_nohz_stop();
    touch_softlockup_watchdog_sched();

    /* Cancel the scheduled timer and restore the tick: */
    tick_sched_flag_clear(ts, TS_FLAG_STOPPED);
    tick_nohz_restart(ts, now);
}
```

`tick_nohz_restart` 内部重新启动 `sched_timer`：

```cpp
// 伪代码：tick_nohz_restart 大致逻辑
hrtimer_set_expires(&ts->sched_timer, ts->last_tick);
hrtimer_forward(&ts->sched_timer, now, TICK_NSEC);  // 赶上时间的进度
// 可能会补偿因 idle 错过的 tick
while (hrtimer_get_expires(&ts->sched_timer) < now) {
    // 如果 idle 时间很长，可能需要更新 jiffies
    hrtimer_add_expires_ns(&ts->sched_timer, TICK_NSEC);
}
hrtimer_start_expires(&ts->sched_timer, HRTIMER_MODE_ABS_PINNED_HARD);
```

---

## 6. 完整 idle 生命周期

```
User space: while(1) { syscall(...); }  → CPU 忙碌
    │
    │  没有可运行的任务
    ▼
Idle 进程调度
    │
    ├─ tick_nohz_idle_enter (tick-sched.c:1294)
    │   └─ TS_FLAG_INIDLE = 1
    │
    ├─ [idle loop]
    │   │
    │   ├─ tick_nohz_next_event (tick-sched.c:988)
    │   │   └─ 计算: 下一个定时器何时到期？
    │   │      (合并 hrtimer + timer wheel)
    │   │
    │   ├─ tick_nohz_stop_tick (tick-sched.c:1013)
    │   │   └─ TS_FLAG_STOPPED = 1, cancel sched_timer
    │   │
    │   ├─ 进入 C-state (ACPI 深度睡眠)
    │   │   └─ CPU 暂停, 等待中断唤醒
    │   │
    │   ... (睡眠中, 可能几微秒到几小时) ...
    │   │
    │   ├─ 中断到达 (I/O 完成, 网络包, IPI, ...)
    │   │
    │   └─ 退出 C-state
    │
    ├─ tick_nohz_irq_exit (tick-sched.c:1331)
    │   └─ 如 INIDLE: tick_nohz_start_idle
    │      (IRQ 处理后重新启动 idle 计时)
    │
    │   如果有可运行任务:
    │
    ├─ tick_nohz_idle_exit (tick-sched.c:1491)
    │   ├─ TS_FLAG_INIDLE = 0
    │   ├─ tick_nohz_restart_sched_tick (恢复 tick)
    │   │   ├─ tick_do_update_jiffies64  ← 补偿错过的 jiffies
    │   │   ├─ timer_clear_idle
    │   │   ├─ TS_FLAG_STOPPED = 0
    │   │   └─ hrtimer_start_expires(&sched_timer)  ← 重新启动
    │   └─ tick_nohz_account_idle_time  ← 记录 idle 时长
    │
    └─ 返回用户态, CPU 恢复忙碌
```

---

## 7. Clocksource 子系统

### 7.1 概述

clocksource 是 hrtimer 的硬件基础——没有高精度时钟源，就没有纳秒级定时器。`kernel/time/clocksource.c` (1600行) 管理所有注册的时钟源设备。

**关键概念：**

```
clocksource (时钟源)
    │
    ├─ 是单调递增的硬件计数器 (TSC, HPET, ACPI PM Timer, ...)
    ├─ 读取 counter → 通过 mult/shift → 转换为纳秒
    └─ 由 rating 字段评选最佳时钟源

clock_event_device (时钟事件设备)
    │
    ├─ 可编程的一次性 (one-shot) 或周期性 (periodic) 中断源
    ├─ hrtimer 通过它设置 "在时刻 X 触发中断"
    └─ 可以是 Local APIC Timer, HPET, ARM Arch Timer...

关系:
    clocksource → 提供 "现在几点?"
    clock_event_device → 提供 "在时刻 X 叫我"
    hrtimer → 将两者结合，实现高精度定时
```

### 7.2 评选、注册与监控

源码位置：`kernel/time/clocksource.c:104-109` (全局变量), `clocksource.c:1110-1173` (select), `clocksource.c:1303-1338` (register), `clocksource.c:643-691` (watchdog)

**评选标准**（`clocksource_find_best`）：按 rating 降序排列，one-shot 模式下要求 `CLOCK_SOURCE_VALID_FOR_HRES`，用户可通过 `clocksource=xxx` 覆盖。典型 rating：TSC/ARM arch_timer=300–400，HPET=250，ACPI PM=200，jiffies=1。

**注册流程**（`__clocksource_register_scale`）：初始化 mult/shift → 加入 `clocksource_list`（按 rating 排序）→ 加入 `watchdog_list` → `clocksource_select()` 触发重新评选，如果新源 rating 更高则自动切换。

**Watchdog 监控**：使用 per-CPU timer (`timer_list`)，每 0.5 秒遍历 `watchdog_list`，对每个时钟源比较与参考源（watchdog）的频率偏差（阈值 50μs），同时检测 CPU 间 clock skew。标记 `CLOCK_SOURCE_UNSTABLE` 后启动 kthread 执行 `clocksource_select()` 切换到备选源。

**Suspend/Resume**：`clocksource_suspend()` 遍历调用每个源的 suspend 回调（TSC 会重置）；`clocksource_resume()` 恢复并触发 watchdog 重新校准（`clocksource_resume_watchdog` → reset_pending）。

---

## 8. NO_HZ_FULL

### 8.1 NO_HZ_IDLE vs NO_HZ_FULL

| 模式 | 触发条件 | 行为 |
|------|----------|------|
| NO_HZ_IDLE | CPU 进入 idle | 停止 tick，CPU 休眠 |
| NO_HZ_FULL | CPU 只有一个可运行任务 | 停止 tick，但 CPU 仍活跃 |

`NO_HZ_FULL` (full dynticks) 解决的是高性能/低延迟场景——当 CPU 上只有一个任务运行时，消除周期 tick 以减少上下文切换和缓存抖动。

**依赖检查（tick_dep_mask）：**

```cpp
// 某些子系统要求 tick 持续运行，例如：
#define TICK_DEP_MASK_POSIX_TIMER   BIT(0)  // POSIX 定时器
#define TICK_DEP_MASK_PERF_EVENTS   BIT(1)  // perf 事件
#define TICK_DEP_MASK_SCHED         BIT(2)  // 调度器
#define TICK_DEP_MASK_CLOCK_UNSTABLE BIT(3) // 时钟源不稳定
// ... 更多
```

如果 `tick_dep_mask` 的任何位被设置，`tick_nohz_full_stop_tick` 将保留 tick 而不停止。

### 8.2 can_stop_full_tick

```cpp
// tick-sched.c 内部
static bool can_stop_full_tick(int cpu, struct tick_sched *ts)
{
    // 1. 检查是否有多个可运行任务
    if (nr_iowait_cpu(cpu) > 0 || rcu_pending(cpu))
        return false;

    // 2. 检查 tick 依赖掩码
    if (atomic_read(&ts->tick_dep_mask))
        return false;

    // 3. 检查时钟源是否稳定
    // ... 更多条件

    return true;
}
```

---

## 9. 性能与节能分析

### 9.1 Tick 停止的节能量

```
HZ=1000 on idle CPU (无 NO_HZ):
    每秒 1000 次 tick 中断
    每次 ~2-5 μs (中断处理 + 调度器检查)
    年累计: 1000 × 2μs × 86400 × 365 ≈ 630 秒/年的 CPU 时间
    更重要的是: 中断阻止了深度 C-state (C6/C7) 的进入

HZ=1000 on idle CPU (NO_HZ):
    找到 next_event: 0 次或极少数次 tick 中断
    可进入 C6/C7 深度睡眠
    典型节能: 数瓦 (移动设备) 到数十瓦 (数据中心)
```

### 9.2 Tick 恢复的延迟

```
从 idle 恢复到重新开始 tick:
    tick_nohz_idle_exit (tick-sched.c:1491)
        ├─ tick_do_update_jiffies64  ← 补偿 jiffies
        ├─ 如果 miss_count 小 (< 10): O(1)
        ├─ 如果 miss_count 大: O(miss_count) (while 循环)
        └─ hrtimer_start_expires

    典型延迟: ~1-5 μs (取决于 missed ticks 数量)
```

### 9.3 Clocksource 切换的代价

```
clocksource 切换:
    ├─ timekeeping_notify → 通知 timekeeping 更新基准
    ├─ 遍历所有 CPU 的 tick_sched
    ├─ 更新 hrtimer 基准（各时钟域的 offset 需重新计算）
    └─ 典型时间: ~50-200 μs

clocksource watchdog:
    每 0.5 秒检查一次
    读取时钟源值 + 比较
    如果标记为 unstable:
      启动 kthread → 获取全局锁 → clocksource_select()
      这可能阻塞 timekeeping 更新
```

---

## 10. 总结

NO_HZ 是 Linux 电源管理和性能优化的关键组件：

| 概念 | 核心要点 |
|------|----------|
| struct tick_sched | per-CPU 结构，用 hrtimer (sched_timer) 模拟周期性 tick |
| tick_nohz_handler | hrtimer 回调，hrtimer_forward + HRTIMER_RESTART 维持周期 |
| TS_FLAG_STOPPED | CPU idle 时停止 sched_timer，传递 do_timer 职责 |
| tick_nohz_restart_sched_tick | 恢复 tick，补偿错过的 jiffies |
| clocksource_select | 按 rating 评选最佳时钟源，支持用户覆盖 |
| clocksource watchdog | 每 0.5 秒检测漂移和 CPU 间偏差 |
| NO_HZ_FULL | 单任务 CPU 也可停止 tick |

调试接口：`/proc/timer_list` 查看 tick_sched 状态和 hrtimer 信息，`/sys/devices/system/clocksource/` 管理时钟源。

---

## 下一篇文章

本系列已完结。建议回顾 [系列目录](./README.md) 查阅所有文章和关键源文件索引。

进一步阅读：`kernel/time/timekeeping.c`（时间保持核心）、`kernel/time/clockevents.c`（时钟事件设备）、`kernel/events/core.c`（perf 中 hrtimer 使用实例）。
