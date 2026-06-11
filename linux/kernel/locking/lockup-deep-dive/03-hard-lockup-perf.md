# 第三篇：Hard Lockup (Perf) — NMI 事件与溢出回调

> 源码：`kernel/watchdog_perf.c` `kernel/watchdog.c` | 头文件：`include/linux/nmi.h`

系列目录：[Hard/Soft Lockup 内核源码深度解析](./README.md)

---

## 1. Hard Lockup 概述

**Hard Lockup** 指 CPU 完全停止响应——连中断都无法处理。与 soft lockup 不同，hard lockup 意味着：
- CPU 关闭了中断（cli/关中断状态下死循环）
- CPU 陷入了某种硬件级别的 hang（如 MMIO 读卡死）
- NMI (Non-Maskable Interrupt) 也无法送达

软锁的检测依赖 hrtimer（软中断上下文），硬锁的检测必须依赖**比普通中断更高优先级**的机制——perf NMI 事件。

### 两种 hardlockup 检测器

| 检测器 | 文件 | 原理 | miss_thresh | 依赖 |
|--------|------|------|-------------|------|
| **Perf** | `watchdog_perf.c` | PMU 溢出 NMI 中断 | 1 | 硬件 PMU (PERF_COUNT_HW_CPU_CYCLES) |
| **Buddy** | `watchdog_buddy.c` | 相邻 CPU 互检 hrtimer 心跳 | 3 | 仅需 hrtimer，无硬件依赖 |

本文聚焦 Perf 检测器。

---

## 2. 架构全景

```
 ┌───────────┐          ┌───────────┐
 │  hrtimer  │ 每 4s    │ Perf PMU  │ 每个 sample_period
 │  (每CPU)  │ 触发     │ (每CPU)    │ 触发一次 NMI
 └─────┬─────┘          └─────┬─────┘
       │                      │
       ▼                      ▼
 watchdog_timer_fn()    watchdog_overflow_callback()
 [watchdog.c:795]       [watchdog_perf.c:105]
       │                      │
       ├─ kick_hardlockup()   ├─ watchdog_check_timestamp()
       │   └─ atomic_inc      │   └─ kmime校验(防虚机误报)
       │     (hrtimer_ints)   │
       │                      ├─ watchdog_hardlockup_check()
       └─ is_softlockup()     │   [watchdog.c:207]
                              │      │
                              │      ├─ is_hardlockup()
                              │      │   └─ hrtimer 计数停增？
                              │      │       → hardlockup!
                              │      │
                              │      └─ 报告 → panic?
                              │
 (两者共享 hrtimer_interrupts 计数器)
```

**关键设计**：hrtimer 和 perf NMI 共享 `hrtimer_interrupts` 原子计数器。
- hrtimer tick → `atomic_inc_return(hrtimer_interrupts)`（每 4s 增 1）
- perf NMI 溢出 → `watchdog_overflow_callback` → `is_hardlockup()` → 检查计数器是否在增长
- 计数器停止增长 → CPU 连 hrtimer 中断都处理不了 → HARD LOCKUP

---

## 3. perf event 创建与配置

### 3.1 标准属性 (watchdog_perf.c:88)

```c
// [watchdog_perf.c:88-94]
static struct perf_event_attr wd_hw_attr = {
    .type       = PERF_TYPE_HARDWARE,
    .config     = PERF_COUNT_HW_CPU_CYCLES,
    .size       = sizeof(struct perf_event_attr),
    .pinned     = 1,
    .disabled   = 1,
};
```

- `PERF_TYPE_HARDWARE` — 硬件事件类型（使用 PMC/PMU 寄存器）
- `PERF_COUNT_HW_CPU_CYCLES` — 计数 CPU 非停机周期（unhalted cycles）
- `.pinned = 1` — 优先级固定，即使 CPU 只有一个 PMU 计数器也优先分配
- `.disabled = 1` — 创建后初始为禁用状态，启动时再 enable

### 3.2 fallback 属性 (watchdog_perf.c:96)

```c
// [watchdog_perf.c:96-102]
static struct perf_event_attr fallback_wd_hw_attr = {
    .type       = PERF_TYPE_HARDWARE,
    .config     = PERF_COUNT_HW_CPU_CYCLES,
    .size       = sizeof(struct perf_event_attr),
    .pinned     = 1,
    .disabled   = 1,
};
```

当 `wd_hw_attr` 通过 `hardlockup_config_perf_event()` 被改写为 `PERF_TYPE_RAW` 后，fallback 属性保留原始的 `PERF_TYPE_HARDWARE` 配置用于创建失败时的二次尝试。

### 3.3 event 创建流程

```c
// [watchdog_perf.c:121-140]
static struct perf_event *hardlockup_detector_event_create(unsigned int cpu)
{
    struct perf_event_attr *wd_attr;
    struct perf_event *evt;

    wd_attr = &wd_hw_attr;
    wd_attr->sample_period = hw_nmi_get_sample_period(watchdog_thresh);

    /* 尝试使用硬件 perf 事件注册 */
    evt = perf_event_create_kernel_counter(wd_attr, cpu, NULL,
                                           watchdog_overflow_callback, NULL);
    if (IS_ERR(evt)) {
        /* 回退：使用 fallback 属性重试 */
        wd_attr = &fallback_wd_hw_attr;
        wd_attr->sample_period = hw_nmi_get_sample_period(watchdog_thresh);
        evt = perf_event_create_kernel_counter(wd_attr, cpu, NULL,
                                               watchdog_overflow_callback, NULL);
    }

    return evt;
}
```

架构相关函数：

```
hw_nmi_get_sample_period(watchdog_thresh)
  ├─ x86: 基于 CPU 频率计算周期值（约 watchdog_thresh * 频率 的 cycle 数）
  └─ 返回值 0 → 表示无法计算，probe 失败
```

---

## 4. watchdog_overflow_callback — NMI 回调

```c
// [watchdog_perf.c:105-119]
static void watchdog_overflow_callback(struct perf_event *event,
                                       struct perf_sample_data *data,
                                       struct pt_regs *regs)
{
    /* 确保 watchdog 事件永远不会被 throttled */
    event->hw.interrupts = 0;

    if (panic_in_progress())
        return;

    if (!watchdog_check_timestamp())
        return;

    watchdog_hardlockup_check(smp_processor_id(), regs);
}
```

### 4.1 防止 throttle (line 110)

```c
event->hw.interrupts = 0;
```

perf 子系统会限制高频率 PMU 中断：当中断频率过高时自动降低采样率。**但 watchdog 正是要检测"CPU 卡住"的反常场景**——我们不能被 throttle 掉。每次溢出回调都将计数器清零，确保 watchdog 事件**永不被限流**。

### 4.2 时间戳校验 (line 115)

```c
if (!watchdog_check_timestamp())
    return;
```

详见第 5 节。

### 4.3 委托到通用判定函数 (line 118)

```c
watchdog_hardlockup_check(smp_processor_id(), regs);
```

将 `regs` 传入——如果是**当前 CPU** 发生 hard lockup，可以直接 `show_regs(regs)` 打印寄存器，提供更详细的诊断信息。

---

## 5. 时间戳校验 — 防虚机误报

```c
// [watchdog_perf.c:59-76]
// CONFIG_HARDLOCKUP_CHECK_TIMESTAMP 启用时

static bool watchdog_check_timestamp(void)
{
    ktime_t delta, now = ktime_get_mono_fast_ns();

    delta = now - __this_cpu_read(last_timestamp);
    if (delta < watchdog_hrtimer_sample_threshold) {
        /*
         * 如果 ktime 基于 jiffies，卡住的 timer 会阻止
         * jiffies 增长——导致一直看到 stale timestamp 而永不触发。
         * 用 nmi_rearmed 计数器做 10 次重试来检测这种情形。
         */
        if (__this_cpu_inc_return(nmi_rearmed) < 10)
            return false;
    }
    __this_cpu_write(nmi_rearmed, 0);
    __this_cpu_write(last_timestamp, now);
    return true;
}
```

### 问题背景

虚拟机被 hypervisor 暂停时：
- Guest 的所有 vCPU 都被冻结
- PMU 计数器停止递增
- Guest 恢复后，perf 事件周期可能与实际 wall-clock 时间对不上
- 容易出现误报：hrtimer 已触发 N 次（虚拟时钟仍在走），但 PMU 周期还没到

### 解决方案

```c
// [watchdog_perf.c:56]
watchdog_hrtimer_sample_threshold = period * 2;
```

其中 `period = sample_period = 4s`（在 `set_sample_period()` 中调用 `watchdog_update_hrtimer_threshold(sample_period)` 设置）。

**工作机制**：

```
正常情况:
  hrtimer 每 4s 发一次，NMI 每 ≈10s~采样周期 发一次

  时间线:  |---4s---|---4s---|
  hrtimer:  ^       ^        ^
  NMI:               ^
  delta > 4s*2 = 8s → 校验通过 → 执行 watchdog_hardlockup_check

虚机暂停恢复:
  Guest 暂停 30s，恢复后
  时间线:  [========暂停30s========][恢复]
  PMU:     停在某一 cycle 值        继续
  NMI:     可能在恢复后短时间内再次触发
  delta ≈ 0 (因为 last_timestamp 是暂停前的值)
  delta < 8s → nmi_rearmed++, 跳过本次检查
  最多跳过 10 次 → 如果 10 次 NMI 后仍 delta < 8s
      → 认为 ktime 基于 jiffies (也停了)
      → 强制通过校验，检查 hardlockup
```

per-CPU 相关变量（`watchdog_perf.c:29-31`）：

```c
static DEFINE_PER_CPU(ktime_t, last_timestamp);        // 上次 NMI 的 ktime
static DEFINE_PER_CPU(unsigned int, nmi_rearmed);      // 重试计数 (max 10)
static ktime_t watchdog_hrtimer_sample_threshold __read_mostly; // = sample_period * 2
```

---

## 6. 计数器初始化与 probe

### 6.1 watchdog_hardlockup_probe() — Perf 版本

```c
// [watchdog_perf.c:259-286]
int __init watchdog_hardlockup_probe(void)
{
    struct perf_event *evt;
    unsigned int cpu;
    int ret;

    if (!arch_perf_nmi_is_available())
        return -ENODEV;

    if (!hw_nmi_get_sample_period(watchdog_thresh))
        return -EINVAL;

    /*
     * 通过创建临时 perf event 测试硬件 PMU 可用性。
     * 事件创建后立即释放。
     */
    cpu = raw_smp_processor_id();
    evt = hardlockup_detector_event_create(cpu);
    if (IS_ERR(evt)) {
        pr_info("Perf NMI watchdog permanently disabled\n");
        ret = PTR_ERR(evt);
    } else {
        perf_event_release_kernel(evt);
        ret = 0;
    }

    return ret;
}
```

创建临时事件 → 验证 PMU 可用 → 立即释放。这是对 `lockup_detector_init()` 阶段 `watchdog_hardlockup_probe()` 调用的实现。

### 6.2 arch_perf_nmi_is_available()

```c
// [watchdog_perf.c:251-254]
bool __weak __init arch_perf_nmi_is_available(void)
{
    return true;
}
```

weak 函数，特定架构可以重写。例如某些 ARM 平台不支持 watchdog perf NMI。

---

## 7. 启动与停止

### 7.1 watchdog_hardlockup_enable()

```c
// [watchdog_perf.c:146-168]
void watchdog_hardlockup_enable(unsigned int cpu)
{
    struct perf_event *evt;

    WARN_ON_ONCE(cpu != smp_processor_id());

    evt = hardlockup_detector_event_create(cpu);
    if (IS_ERR(evt)) {
        pr_debug("Perf event create on CPU %d failed with %ld\n", cpu,
                 PTR_ERR(evt));
        return;
    }

    /* 只在第一个 CPU 创建成功时打印消息 */
    if (!atomic_fetch_inc(&watchdog_cpus))
        pr_info("Enabled. Permanently consumes one hw-PMU counter.\n");

    WARN_ONCE(this_cpu_read(watchdog_ev), "unexpected watchdog_ev leak");
    this_cpu_write(watchdog_ev, evt);

    watchdog_init_timestamp();
    perf_event_enable(evt);
}
```

关键点：
- `atomic_fetch_inc(&watchdog_cpus)` — 只在计数器从 0→1 时打印消息
- `Permanently consumes one hw-PMU counter` 说明每个监控 CPU 消耗一个硬件 PMU 计数器
- `watchdog_init_timestamp()` — 初始化时间戳校验的 `last_timestamp`

### 7.2 watchdog_hardlockup_disable()

```c
// [watchdog_perf.c:174-186]
void watchdog_hardlockup_disable(unsigned int cpu)
{
    struct perf_event *event = this_cpu_read(watchdog_ev);

    WARN_ON_ONCE(cpu != smp_processor_id());

    if (event) {
        perf_event_disable(event);
        perf_event_release_kernel(event);
        this_cpu_write(watchdog_ev, NULL);
        atomic_dec(&watchdog_cpus);
    }
}
```

### 7.3 Per-CPU 存储

```c
// [watchdog_perf.c:24-26]
static DEFINE_PER_CPU(struct perf_event *, watchdog_ev);
static atomic_t watchdog_cpus = ATOMIC_INIT(0);
```

`watchdog_ev` 存储每个 CPU 的 perf event 指针，`watchdog_cpus` 跟踪已启用 CPU 的数量。

---

## 8. 周期调整

```c
// [watchdog_perf.c:193-208]
void hardlockup_detector_perf_adjust_period(u64 period)
{
    struct perf_event *event = this_cpu_read(watchdog_ev);

    if (!(watchdog_enabled & WATCHDOG_HARDLOCKUP_ENABLED))
        return;

    if (!event)
        return;

    if (event->attr.sample_period == period)
        return;

    if (perf_event_period(event, period))
        pr_err("failed to change period to %llu\n", period);
}
```

当 CPU 调频时调用（如 Cpufreq 驱动），调整 PMU 采样周期以匹配新的频率。被架构代码调用。

---

## 9. 硬锁判定核心 — is_hardlockup() 与 watchdog_hardlockup_check()

### 9.1 hrtimer 中断计数器

```c
// [watchdog.c:145-148]
// 仅在 CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER 启用
static DEFINE_PER_CPU(atomic_t, hrtimer_interrupts);      // 原子递增
static DEFINE_PER_CPU(int, hrtimer_interrupts_saved);     // 上次检查的快照
static DEFINE_PER_CPU(int, hrtimer_interrupts_missed);    // 连续 missed 计数
static DEFINE_PER_CPU(bool, watchdog_hardlockup_warned);  // 是否已打印过警告
```

### 9.2 watchdog_hardlockup_kick() — 心跳递增

```c
// [watchdog.c:199-205]
static void watchdog_hardlockup_kick(void)
{
    int new_interrupts;

    new_interrupts = atomic_inc_return(this_cpu_ptr(&hrtimer_interrupts));
    watchdog_buddy_check_hardlockup(new_interrupts);
}
```

每 4s 由 `watchdog_timer_fn()` 调用一次→`hrtimer_interrupts` 递增。
- Perf 模式下：`watchdog_buddy_check_hardlockup()` 可能是空函数（不在 buddy 模式中）
- Buddy 模式下：用于检查相邻 CPU → 下一篇会详细讲

### 9.3 is_hardlockup()

```c
// [watchdog.c:183-197]
static bool is_hardlockup(unsigned int cpu)
{
    int hrint = atomic_read(&per_cpu(hrtimer_interrupts, cpu));

    // 步骤1: 计数器在增长 → 正常
    if (per_cpu(hrtimer_interrupts_saved, cpu) != hrint) {
        watchdog_hardlockup_update_reset(cpu);
        return false;
    }

    // 步骤2: 计数器未增长 → 累计 missed
    per_cpu(hrtimer_interrupts_missed, cpu)++;
    if (per_cpu(hrtimer_interrupts_missed, cpu) % watchdog_hardlockup_miss_thresh)
        return false;

    // 步骤3: missed 次数达到 miss_thresh → HARD LOCKUP!
    return true;
}
```

`watchdog_hardlockup_miss_thresh` 的含义：
- Perf 模式：`= 1`（一次 missed 即判定）—— NMI 周期 ≈ 10s，足够长
- Buddy 模式：`= 3`（需要 3 次）—— buddy 检查频率与 hrtimer 同步（4s），需要更多采样

### 9.4 watchdog_hardlockup_update_reset()

```c
// [watchdog.c:170-181]
static void watchdog_hardlockup_update_reset(unsigned int cpu)
{
    int hrint = atomic_read(&per_cpu(hrtimer_interrupts, cpu));

    per_cpu(hrtimer_interrupts_saved, cpu) = hrint;
    per_cpu(hrtimer_interrupts_missed, cpu) = 0;
}
```

将当前计数值保存为"快照"，清空 missed 计数。每当 `hrtimer_interrupts` 增长就调用，相当于重置检测周期。

### 9.5 watchdog_hardlockup_check() — 完整判定

```c
// [watchdog.c:207-294]
void watchdog_hardlockup_check(unsigned int cpu, struct pt_regs *regs)
{
    int hardlockup_all_cpu_backtrace;
    unsigned int this_cpu;
    unsigned long flags;

    // ========== 步骤1: touched 处理 ==========
    if (per_cpu(watchdog_hardlockup_touched, cpu)) {
        watchdog_hardlockup_update_reset(cpu);
        per_cpu(watchdog_hardlockup_touched, cpu) = false;
        return;
    }

    hardlockup_all_cpu_backtrace =
        (hardlockup_si_mask & SYS_INFO_ALL_BT) ?
            1 : sysctl_hardlockup_all_cpu_backtrace;

    // ========== 步骤2: is_hardlockup 判定 ==========
    if (!is_hardlockup(cpu)) {
        per_cpu(watchdog_hardlockup_warned, cpu) = false;
        return;
    }

#ifdef CONFIG_SYSFS
    ++hardlockup_count;
#endif

    // ========== 步骤3: sched_ext BPF 弹射 ==========
    if (scx_hardlockup(cpu))
        return;

    // ========== 步骤4: 去重 — 每个 CPU 只报告一次 ==========
    if (per_cpu(watchdog_hardlockup_warned, cpu))
        return;

    // ========== 步骤5: 全CPU backtrace 互斥 ==========
    if (hardlockup_all_cpu_backtrace) {
        if (test_and_set_bit_lock(0, &hard_lockup_nmi_warn))
            return;
    }

    // ========== 步骤6: 报告 ==========
    this_cpu = smp_processor_id();
    pr_emerg("CPU%u: Watchdog detected hard LOCKUP on cpu %u\n",
             this_cpu, cpu);
    printk_cpu_sync_get_irqsave(flags);

    print_modules();
    print_irqtrace_events(current);
    if (cpu == this_cpu) {
        // 本 CPU 自己锁死 → show_regs（有 regs 指针）
        if (regs)
            show_regs(regs);
        else
            dump_stack();
        printk_cpu_sync_put_irqrestore(flags);
    } else {
        // 其他 CPU 锁死 → trigger_single_cpu_backtrace
        printk_cpu_sync_put_irqrestore(flags);
        trigger_single_cpu_backtrace(cpu);
    }

    // ========== 步骤7: 全 CPU backtrace ==========
    if (hardlockup_all_cpu_backtrace) {
        trigger_allbutcpu_cpu_backtrace(cpu);
        if (!hardlockup_panic)
            clear_bit_unlock(0, &hard_lockup_nmi_warn);
    }

    // ========== 步骤8: 系统信息 + panic? ==========
    sys_info(hardlockup_si_mask & ~SYS_INFO_ALL_BT);
    if (hardlockup_panic)
        nmi_panic(regs, "Hard LOCKUP");

    per_cpu(watchdog_hardlockup_warned, cpu) = true;
}
```

### 流程图解

```
watchdog_hardlockup_check(cpu, regs)
        │
        ▼
┌──────────────┐
│ touched?     │──是──→ reset counter → return
└──────┬───────┘
       │ 否
       ▼
┌──────────────┐
│ is_hardlockup │──否──→ 清除 warned 标志 → return
│ (cpu)?       │        (计数器在增长)
└──────┬───────┘
       │ 是
       ▼
┌──────────────┐
│ scx_hardlockup│──是──→ return  (BPF调度器自救)
│ (cpu)?       │
└──────┬───────┘
       │ 否
       ▼
┌──────────────┐
│ 已 warned?   │──是──→ return  (去重)
└──────┬───────┘
       │ 否
       ▼
┌──────────────┐
│ 全CPU BT?    │──是──→ test_and_set 互斥锁
│              │       │ 未获取 → return
│              │       │ 获取→继续
└──────┬───────┘
       │
       ▼
  pr_emerg → print_modules → print_irqtrace_events
  → show_regs/dump_stack/trigger_single_cpu_backtrace
  → trigger_allbutcpu_cpu_backtrace
  → sys_info
  → hardlockup_panic? → nmi_panic
  → watchdog_hardlockup_warned = true
```

---

## 10. 全局 stop/restart (x86 HT bug)

```c
// [watchdog_perf.c:215-249]
void __init hardlockup_detector_perf_stop(void)
{
    int cpu;
    lockdep_assert_cpus_held();
    for_each_online_cpu(cpu) {
        struct perf_event *event = per_cpu(watchdog_ev, cpu);
        if (event)
            perf_event_disable(event);
    }
}

void __init hardlockup_detector_perf_restart(void)
{
    int cpu;
    lockdep_assert_cpus_held();
    if (!(watchdog_enabled & WATCHDOG_HARDLOCKUP_ENABLED))
        return;
    for_each_online_cpu(cpu) {
        struct perf_event *event = per_cpu(watchdog_ev, cpu);
        if (event)
            perf_event_enable(event);
    }
}
```

这是为处理 x86 Hyper-Threading bug 而设的特殊接口。在 HT 兄弟线程交替上下电时，perf NMI 看门狗需要全局关闭/重启以避免伪故障。

---

## 11. hardlockup 检测时间线对比 (Perf vs Buddy)

```
Perf 模式 (miss_thresh=1):

hrtimer: |--4s--|--4s--|--4s--|--4s--|--4s--|
int_cnt:  1      2      3      4      5
NMI:      ======NMI=====NMI=====NMI=====  (每 ~10s)
check:           ✓      ✓      ✓
                        如果 int_cnt 停在 2 → miss+1 → miss_thresh(1) → HARD LOCKUP

Buddy 模式 (miss_thresh=3): (下一篇文章详述)

hrtimer: |--4s--|--4s--|--4s--|--4s--|--4s--|
check:   cpu0→cpu1  cpu1→cpu2  cpu2→cpu0  ...
            如果 cpu1 int_cnt 停在 3:
            miss+1, miss%3=1 → 不报
            miss+2, miss%3=2 → 不报
            miss+3, miss%3=0 → HARD LOCKUP!
            (需要 3*4s=12s 才报告)
```

---

## 12. 关键差异：Perf vs Buddy 架构级别对比

```
┌─────────────────────┬──────────────────────────────────────────┐
│                     │    Perf (watchdog_perf.c)                │
├─────────────────────┼──────────────────────────────────────────┤
│ 触发源              │  PMU 溢出 → NMI                           │
│ check 调用点        │  watchdog_overflow_callback()             │
│                     │  [watchdog_perf.c:105] ← NMI 上下文       │
│ regs 来源           │  perf 子系统传入 (硬件寄存器上下文)       │
│ miss_thresh         │  1 (立即)                                │
│ 硬件依赖            │  必须: PMU + NMI                          │
│ CPU 关系            │  每 CPU 独立                              │
│ 额外校验            │  watchdog_check_timestamp() (防虚机)      │
│ 防 throttle         │  event->hw.interrupts = 0                │
│ 频率感知            │  hardlockup_detector_perf_adjust_period() │
│ 全局 stop/restart   │  HT bug 专用接口                          │
│ hardlockup_touch    │  arch_touch_nmi_watchdog()                │
├─────────────────────┼──────────────────────────────────────────┤
│                     │    Buddy (watchdog_buddy.c)               │
├─────────────────────┼──────────────────────────────────────────┤
│ 触发源              │  hrtimer → 软中断上下文                    │
│ check 调用点        │  watchdog_hardlockup_kick()               │
│                     │  [watchdog.c:199] ← hrtimer 回调中        │
│ regs 来源           │  NULL (软中断上下文)                      │
│ miss_thresh         │  3 (需要 12s)                            │
│ 硬件依赖            │  无 (纯软件)                             │
│ CPU 关系            │  环形互检链                               │
│ 额外校验            │  无                                       │
│ 防 throttle         │  不需要 (hrtimer 不会被 throttle)        │
│ 频率感知            │  不需要 (基于时间不减 cycles)             │
│ 全局 stop/restart   │  无                                       │
│ hardlockup_touch    │  watchdog_hardlockup_touch_cpu()          │
└─────────────────────┴──────────────────────────────────────────┘
```

---

## 13. /proc/sys/kernel 相关接口速查

| 路径 | 处理器 | 效果 |
|------|--------|------|
| `kernel.nmi_watchdog` | `proc_nmi_watchdog` | 设置 `watchdog_hardlockup_user_enabled` |
| `kernel.hardlockup_panic` | `proc_dointvec` | 控制是否 panic |
| `kernel.hardlockup_all_cpu_backtrace` | `proc_dointvec_minmax` | 是否 dump 所有 CPU backtrace |
| `kernel.watchdog_thresh` | `proc_watchdog_thresh` | 同时影响 hard/soft 阈值 |
| `kernel.watchdog_cpumask` | `proc_watchdog_cpumask` | 限制 watchdog 运行的 CPU |

---

## 下一篇文章

[第四篇：Hard Lockup (Buddy) — CPU 互检与 miss 阈值](./04-hard-lockup-buddy.md)
