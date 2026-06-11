# 第一篇：初始化 — lockup_detector_init 全链路

> 源码：`kernel/watchdog.c` | 头文件：`include/linux/nmi.h`

系列目录：[Hard/Soft Lockup 内核源码深度解析](./README.md)

---

## 1. 概述

lockup 检测器的初始化从 `main.c:1693` 的 `lockup_detector_init()` 调用开始，经过硬件探测、全局变量初始化、sysctl 注册，最终通过 `late_initcall_sync` 完成 late 阶段的收尾校验。整个初始化链路分三个阶段：**early boot**（硬件探测 + 基础设施）、**sysctl 初始化**（/proc 接口注册）、**late check**（delayed init 收敛）。

```
计时线:

start_kernel()            [init/main.c]
  ...
  lockup_detector_init()  [init/main.c:1693] ─── early boot 阶段
    ├─ cpumask_copy()
    ├─ watchdog_hardlockup_probe()              硬件探测
    └─ lockup_detector_setup()
         └─ __lockup_detector_reconfigure()
              ├─ stop → set_sample_period → start  起搏启动

  ...arch init...          (driver probe, 等)

  lockup_detector_check() [late_initcall_sync] ─── late check 阶段
    ├─ allow_lockup_detector_init_retry = false
    ├─ flush_work()                              等待 delayed init
    └─ watchdog_sysctl_init()                    注册 /proc/sys/kernel/*

  ...free_initmem()        (释放 init 段内存)
```

---

## 2. 全局变量一览

初始化时的核心全局变量，均定义在 `kernel/watchdog.c` 头部附近：

```c
// [watchdog.c:46]
unsigned long __read_mostly watchdog_enabled;
//   bitmask: WATCHDOG_HARDLOCKUP_ENABLED (bit 0) | WATCHDOG_SOFTOCKUP_ENABLED (bit 1)
//   启动后由 lockup_detector_update_enable() 填充

// [watchdog.c:47]
int __read_mostly watchdog_user_enabled = 1;
//   用户态总开关(/proc/sys/kernel/watchdog)，默认启用

// [watchdog.c:48]
static int __read_mostly watchdog_hardlockup_user_enabled = WATCHDOG_HARDLOCKUP_DEFAULT;
//   硬锁检测用户开关(/proc/sys/kernel/nmi_watchdog)
//   WATCHDOG_HARDLOCKUP_DEFAULT: 在非s390平台默认为1

// [watchdog.c:49]
static int __read_mostly watchdog_softlockup_user_enabled = 1;
//   软锁检测用户开关(/proc/sys/kernel/soft_watchdog)，默认启用

// [watchdog.c:50]
int __read_mostly watchdog_thresh = 10;
//   阈值(秒)，默认10s。软锁检测阈值 = watchdog_thresh * 2 = 20s

// [watchdog.c:51]
static int __read_mostly watchdog_thresh_next;
//   sysctl 写入时先更新此变量，reconfigure 时再同步到 watchdog_thresh

// [watchdog.c:52]
static int __read_mostly watchdog_hardlockup_available;
//   硬锁探测器是否可用（probe 成功置 1）

// [watchdog.c:54]
struct cpumask watchdog_cpumask __read_mostly;
//   运行 watchdog 的 CPU 集合（初始化时从 housekeeping 复制）

// [watchdog.c:67]
int __read_mostly watchdog_hardlockup_miss_thresh = 1;
//   连续 missed 阈值：perf=1, buddy=3
```

---

## 3. watchdog_enabled 位掩码

定义在 `include/linux/nmi.h:79-82`：

```c
// [nmi.h:67-82]
/*
 * The run state of the lockup detectors is controlled by the content of the
 * 'watchdog_enabled' variable. Each lockup detector has its dedicated bit -
 * bit 0 for the hard lockup detector and bit 1 for the soft lockup detector.
 *
 * 'watchdog_user_enabled', 'watchdog_hardlockup_user_enabled' and
 * 'watchdog_softlockup_user_enabled' are variables that are only used as an
 * 'interface' between the parameters in /proc/sys/kernel and the internal
 * state bits in 'watchdog_enabled'. The 'watchdog_thresh' variable is
 * handled differently because its value is not boolean, and the lockup
 * detectors are 'suspended' while 'watchdog_thresh' is equal zero.
 */
#define WATCHDOG_HARDLOCKUP_ENABLED_BIT  0
#define WATCHDOG_SOFTOCKUP_ENABLED_BIT   1
#define WATCHDOG_HARDLOCKUP_ENABLED     (1 << WATCHDOG_HARDLOCKUP_ENABLED_BIT)
#define WATCHDOG_SOFTOCKUP_ENABLED      (1 << WATCHDOG_SOFTOCKUP_ENABLED_BIT)
```

设计要点：
- `watchdog_user_enabled` / `watchdog_hardlockup_user_enabled` / `watchdog_softlockup_user_enabled` 是**用户接口层**的变量，对应 `/proc/sys/kernel/` 下的各个文件
- `watchdog_enabled` 是**内部运行时**的位掩码，`lockup_detector_update_enable()` 负责将用户接口层同步到运行时位掩码
- 这种两层设计让 sysctl 的读操作总是返回用户期望的值，而写操作需要经过 `__lockup_detector_reconfigure()` 才能生效

---

## 4. lockup_detector_init() — 第一阶段

**调用点**：`init/main.c:1693`，在 `start_kernel()` 的 rest_init 前调用。

```c
// [watchdog.c:1386-1400]
void __init lockup_detector_init(void)
{
    if (tick_nohz_full_enabled())
        pr_info("Disabling watchdog on nohz_full cores by default\n");

    cpumask_copy(&watchdog_cpumask,
                 housekeeping_cpumask(HK_TYPE_TIMER));

    if (!watchdog_hardlockup_probe())
        watchdog_hardlockup_available = true;
    else
        allow_lockup_detector_init_retry = true;

    lockup_detector_setup();
}
```

### 4.1 cpumask 设置 (line 1391)

```c
cpumask_copy(&watchdog_cpumask, housekeeping_cpumask(HK_TYPE_TIMER));
```

- `housekeeping_cpumask(HK_TYPE_TIMER)` 返回允许运行 timer 的 CPU 集合
- 在 `nohz_full` 配置下，nohz_full CPU 被排除在 housekeeping 之外
- `watchdog_cpumask` 定义了 watchdog 可以运行在哪些 CPU 上

### 4.2 硬锁检测器探测 (line 1394)

```c
if (!watchdog_hardlockup_probe())
    watchdog_hardlockup_available = true;
else
    allow_lockup_detector_init_retry = true;
```

`watchdog_hardlockup_probe()` 是 weak 函数，有两种实现：

**Perf 版本** (`kernel/watchdog_perf.c:259-286`)：

```c
// [watchdog_perf.c:259]
int __init watchdog_hardlockup_probe(void)
{
    struct perf_event *evt;
    unsigned int cpu;
    int ret;

    if (!arch_perf_nmi_is_available())
        return -ENODEV;

    if (!hw_nmi_get_sample_period(watchdog_thresh))
        return -EINVAL;

    // 创建临时 perf event 验证 PMU 可用
    cpu = raw_smp_processor_id();
    evt = hardlockup_detector_event_create(cpu);
    if (IS_ERR(evt)) {
        pr_info("Perf NMI watchdog permanently disabled\n");
        ret = PTR_ERR(evt);
    } else {
        perf_event_release_kernel(evt);  // 立即释放
        ret = 0;
    }
    return ret;
}
```

**Buddy 版本** (`kernel/watchdog_buddy.c:22-26`)：

```c
// [watchdog_buddy.c:22]
int __init watchdog_hardlockup_probe(void)
{
    watchdog_hardlockup_miss_thresh = 3;  // buddy 需要 3 次 missed 才判定
    return 0;  // 总是成功
}
```

如果 probe 返回非 0（perf PMU 不可用），设置 `allow_lockup_detector_init_retry = true`，允许后续通过 `lockup_detector_retry_init()` 重新尝试。

### 4.3 延迟重试机制

```c
// [watchdog.c:1325-1349]
static void __init lockup_detector_delay_init(struct work_struct *work)
{
    int ret;

    ret = watchdog_hardlockup_probe();
    if (ret) {
        // probe 仍然失败——永久禁用
        ...
        pr_info("Hard watchdog permanently disabled\n");
        return;
    }

    allow_lockup_detector_init_retry = false;
    watchdog_hardlockup_available = true;
    lockup_detector_setup();
}

// [watchdog.c:1358-1365]
void __init lockup_detector_retry_init(void)
{
    if (!allow_lockup_detector_init_retry)
        return;

    schedule_work(&detector_work);  // 调度 delayed work
}
```

`lockup_detector_retry_init()` 被 arch 代码在特定初始化点调用（如 `x86_late_time_init`），适用于 perf PMU 未被早期 probe 检测到但后续初始化的场景。

---

## 5. lockup_detector_setup() — 基础设施搭建

```c
// [watchdog.c:1035-1051]
static __init void lockup_detector_setup(void)
{
    // 步骤1: 同步用户接口 → watchdog_enabled 位掩码
    lockup_detector_update_enable();

    // 步骤2: 如果 watchdog 被完全禁用(通过cmdline或Kconfig)，直接返回
    if (!IS_ENABLED(CONFIG_SYSCTL) &&
        !(watchdog_enabled && watchdog_thresh))
        return;

    // 步骤3: 停止旧配置 → 重采样 → 启动
    mutex_lock(&watchdog_mutex);
    __lockup_detector_reconfigure(false);
    softlockup_initialized = true;  // 后续 sysctl 写入时生效
    mutex_unlock(&watchdog_mutex);
}
```

### 5.1 lockup_detector_update_enable() — 同步位掩码

```c
// [watchdog.c:354-363]
static void lockup_detector_update_enable(void)
{
    watchdog_enabled = 0;
    if (!watchdog_user_enabled)
        return;
    if (watchdog_hardlockup_available && watchdog_hardlockup_user_enabled)
        watchdog_enabled |= WATCHDOG_HARDLOCKUP_ENABLED;
    if (watchdog_softlockup_user_enabled)
        watchdog_enabled |= WATCHDOG_SOFTOCKUP_ENABLED;
}
```

转换表：

| watchdog_user_enabled | hardlockup_available_and_enabled | softlockup_enabled | watchdog_enabled |
|-----------------------|----------------------------------|-------------------|------------------|
| 0                     | —                                | —                 | 0                |
| 1                     | 0, 0                             | 0                 | 0                |
| 1                     | 0, 0                             | 1                 | 0x2              |
| 1                     | 1, 1                             | 0                 | 0x1              |
| 1                     | 1, 1                             | 1                 | 0x3              |

---

## 6. __lockup_detector_reconfigure() — 核心配置引擎

```c
// [watchdog.c:1002-1023]
static void __lockup_detector_reconfigure(bool thresh_changed)
{
    cpus_read_lock();
    watchdog_hardlockup_stop();               // 停止硬锁检测

    softlockup_stop_all();                    // 停止所有CPU的软锁检测
    if (thresh_changed)
        watchdog_thresh = READ_ONCE(watchdog_thresh_next);
    set_sample_period();                      // 重新计算采样周期
    lockup_detector_update_enable();          // 更新位掩码
    if (watchdog_enabled && watchdog_thresh)
        softlockup_start_all();               // 启动所有CPU的软锁检测

    watchdog_hardlockup_start();              // 启动硬锁检测
    cpus_read_unlock();
}
```

**注意 `thresh_changed` 的时序保护**：
- 如果 `thresh_changed=true`，`watchdog_thresh` 在 `softlockup_stop_all()` 之后、`set_sample_period()` 之前更新
- 这保证了 `watchdog_timer_fn` 不会同时看到旧的 `sample_period` 和新的 `watchdog_thresh`，避免误报

### 6.1 set_sample_period() — 采样周期计算

```c
// [watchdog.c:667-678]
static void set_sample_period(void)
{
    /*
     * convert watchdog_thresh from seconds to ns
     * the divide by 5 is to give hrtimer several chances (two
     * or three with the current relation between the soft
     * and hard thresholds) to increment before the
     * hardlockup detector generates a warning
     */
    sample_period = get_softlockup_thresh() * ((u64)NSEC_PER_SEC / NUM_SAMPLE_PERIODS);
    watchdog_update_hrtimer_threshold(sample_period);
}
```

计算展开：
```
sample_period = (watchdog_thresh * 2) * (NSEC_PER_SEC / 5)
              = (10 * 2) * (1_000_000_000 / 5)
              = 20 * 200_000_000
              = 4_000_000_000 ns = 4 seconds
```

**为什么除以 5？** 硬锁检测依赖 hrtimer 心跳递增 `hrtimer_interrupts` 计数器，而 NMI perf event 的周期约为 `watchdog_thresh` 秒（10s）。除以 5 让 hrtimer 在每个 NMI 周期内触发 2-3 次，确保 `is_hardlockup()` 有足够样本做判断。

### 6.2 softlockup_stop_all() / softlockup_start_all()

```c
// [watchdog.c:960-971]
static void softlockup_stop_all(void)
{
    int cpu;
    if (!softlockup_initialized)
        return;

    for_each_cpu(cpu, &watchdog_allowed_mask)
        smp_call_on_cpu(cpu, softlockup_stop_fn, NULL, false);

    cpumask_clear(&watchdog_allowed_mask);
}

// [watchdog.c:954-958]
static int softlockup_stop_fn(void *data)
{
    watchdog_disable(smp_processor_id());
    return 0;
}
```

```c
// [watchdog.c:979-986]
static void softlockup_start_all(void)
{
    int cpu;

    cpumask_copy(&watchdog_allowed_mask, &watchdog_cpumask);
    for_each_cpu(cpu, &watchdog_allowed_mask)
        smp_call_on_cpu(cpu, softlockup_start_fn, NULL, false);
}

// [watchdog.c:973-977]
static int softlockup_start_fn(void *data)
{
    watchdog_enable(smp_processor_id());
    return 0;
}
```

### 6.3 watchdog_enable() — 核心启动

```c
// [watchdog.c:913-936]
static void watchdog_enable(unsigned int cpu)
{
    struct hrtimer *hrtimer = this_cpu_ptr(&watchdog_hrtimer);
    struct completion *done = this_cpu_ptr(&softlockup_completion);

    WARN_ON_ONCE(cpu != smp_processor_id());

    init_completion(done);
    complete(done);

    // hrtimer 设置：CLOCK_MONOTONIC, REL_HARD/HARD_PINNED 模式
    hrtimer_setup(hrtimer, watchdog_timer_fn, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
    hrtimer_start(hrtimer, ns_to_ktime(sample_period),
                  HRTIMER_MODE_REL_PINNED_HARD);

    // 初始化时间戳
    update_touch_ts();
    // 联动启动硬锁检测器
    if (watchdog_enabled & WATCHDOG_HARDLOCKUP_ENABLED)
        watchdog_hardlockup_enable(cpu);
}
```

关键点：
- `HRTIMER_MODE_REL_PINNED_HARD`：hrtimer 在硬中断上下文运行且**绑定到当前 CPU**
- `init_completion(done); complete(done);`：让首次 `watchdog_timer_fn` 的 `stop_one_cpu_nowait` 检查能通过（completion 已经 done）
- `update_touch_ts()`：初始化 `watchdog_touch_ts`，防止立即误报软锁

### 6.4 watchdog_disable()

```c
// [watchdog.c:938-952]
static void watchdog_disable(unsigned int cpu)
{
    struct hrtimer *hrtimer = this_cpu_ptr(&watchdog_hrtimer);

    WARN_ON_ONCE(cpu != smp_processor_id());

    // 先停硬锁检测——防止 timer 被取消后 NMI 仍触发误报
    watchdog_hardlockup_disable(cpu);
    hrtimer_cancel(hrtimer);
    wait_for_completion(this_cpu_ptr(&softlockup_completion));
}
```

停止顺序很重要：先 disable hardlockup、再 cancel hrtimer、最后等待 softlockup completion——确保不会留下任何 dangling 的检测路径。

---

## 7. CPU Hotplug

```c
// [watchdog.c:988-1000]
int lockup_detector_online_cpu(unsigned int cpu)
{
    if (cpumask_test_cpu(cpu, &watchdog_allowed_mask))
        watchdog_enable(cpu);
    return 0;
}

int lockup_detector_offline_cpu(unsigned int cpu)
{
    if (cpumask_test_cpu(cpu, &watchdog_allowed_mask))
        watchdog_disable(cpu);
    return 0;
}
```

这两个回调在 CPU 热插拔链中注册，对允许范围内的 CPU 自动启用/禁用 watchdog。

---

## 8. lockup_detector_check() — Late Check

```c
// [watchdog.c:1371-1384]
static int __init lockup_detector_check(void)
{
    // 阻止任何后续重试
    allow_lockup_detector_init_retry = false;

    // 确保没有 pending 的 delayed init work
    flush_work(&detector_work);

    watchdog_sysctl_init();

    return 0;
}
late_initcall_sync(lockup_detector_check);
```

`late_initcall_sync` 保证这个函数在 `free_initmem()` 之前执行，确保所有 delayed init work 都已完成并有结果。

### 8.1 watchdog_sysctl_init() — /proc 接口

```c
// [watchdog.c:1316-1319]
static void __init watchdog_sysctl_init(void)
{
    register_sysctl_init("kernel", watchdog_sysctls);
}
```

注册到 `/proc/sys/kernel/` 下的 sysctl 项（`watchdog.c:1212-1314`）：

| 路径 | 变量 | 处理器 |
|------|------|--------|
| `kernel.watchdog` | `watchdog_user_enabled` | `proc_watchdog` |
| `kernel.nmi_watchdog` | `watchdog_hardlockup_user_enabled` | `proc_nmi_watchdog` |
| `kernel.soft_watchdog` | `watchdog_softlockup_user_enabled` | `proc_soft_watchdog` |
| `kernel.watchdog_thresh` | `watchdog_thresh` | `proc_watchdog_thresh` |
| `kernel.watchdog_cpumask` | `watchdog_cpumask` | `proc_watchdog_cpumask` |
| `kernel.softlockup_panic` | `softlockup_panic` | `proc_dointvec_minmax` |
| `kernel.softlockup_all_cpu_backtrace` | `sysctl_softlockup_all_cpu_backtrace` | `proc_dointvec_minmax` |
| `kernel.hardlockup_all_cpu_backtrace` | `sysctl_hardlockup_all_cpu_backtrace` | `proc_dointvec_minmax` |

#### sysctl 写操作流程

以写入 `watchdog_thresh` 为例（`watchdog.c:1170-1187`）：

```c
static int proc_watchdog_thresh(const struct ctl_table *table, int write,
                                void *buffer, size_t *lenp, loff_t *ppos)
{
    int err, old;
    mutex_lock(&watchdog_mutex);

    watchdog_thresh_next = READ_ONCE(watchdog_thresh);  // 从当前值初始化
    old = watchdog_thresh_next;
    err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

    if (!err && write && old != READ_ONCE(watchdog_thresh_next))
        proc_watchdog_update(true);  // thresh_changed=true 触发 full reconfigure

    mutex_unlock(&watchdog_mutex);
    return err;
}
```

`proc_watchdog_update()` 定义在 `watchdog.c:1088-1093`：

```c
static void proc_watchdog_update(bool thresh_changed)
{
    cpumask_and(&watchdog_cpumask, &watchdog_cpumask, cpu_possible_mask);
    __lockup_detector_reconfigure(thresh_changed);
}
```

---

## 9. 内核命令行参数

### nmi_watchdog= (watchdog.c:139)

```c
// [watchdog.c:118-139]
static int __init hardlockup_panic_setup(char *str)
{
next:
    if (!strncmp(str, "panic", 5))
        hardlockup_panic = 1;
    else if (!strncmp(str, "nopanic", 7))
        hardlockup_panic = 0;
    else if (!strncmp(str, "0", 1))
        watchdog_hardlockup_user_enabled = 0;
    else if (!strncmp(str, "1", 1))
        watchdog_hardlockup_user_enabled = 1;
    else if (!strncmp(str, "r", 1))
        hardlockup_config_perf_event(str + 1);
    // 向下遍历逗号分隔的选项...
}
__setup("nmi_watchdog=", hardlockup_panic_setup);
```

支持组合参数，如 `nmi_watchdog=1,panic` 即启用并设置 panic on hardlockup。

### softlockup_panic= (watchdog.c:423)

```c
// [watchdog.c:423-428]
static int __init softlockup_panic_setup(char *str)
{
    softlockup_panic = simple_strtoul(str, NULL, 0);
    return 1;
}
__setup("softlockup_panic=", softlockup_panic_setup);
```

---

## 10. lockup_detector_soft_poweroff()

```c
// [watchdog.c:1080-1083]
void lockup_detector_soft_poweroff(void)
{
    watchdog_enabled = 0;
}
```

供 parisc 架构使用的特殊接口。`pm_poweroff()` 默认实现是一个无限忙等待循环，会触发 lockup 告警。将此接口清零 `watchdog_enabled` 后，`watchdog_timer_fn` 在 line 803 检查到 `!watchdog_enabled` 后立即返回 `HRTIMER_NORESTART`。

---

## 11. 完整初始化时序图

```
                   boot CPU                          NMI/其他 CPU
                   =======                          ============
 start_kernel()
   ...
   lockup_detector_init()           [watchdog.c:1386]
     │
     ├─ cpumask_copy(wdog, hk)      [watchdog.c:1391]
     │
     ├─ watchdog_hardlockup_probe() [watchdog.c:1394]
     │   ├─ perf: create temp evt   [watchdog_perf.c:259]
     │   │   ├─ OK → hwlp_avail=1
     │   │   └─ fail → retry=true
     │   └─ buddy: miss_thresh=3    [watchdog_buddy.c:22]
     │       └─ return 0 → hwlp_avail=1
     │
     └─ lockup_detector_setup()     [watchdog.c:1399]
          └─ update_enable()        [watchdog.c:354]
          └─ __reconfigure()        [watchdog.c:1002]
               ├─ hwlp_stop()
               ├─ softlockup_stop_all()
               │   └─ for each CPU: smp_call_on_cpu ────→ watchdog_disable()
               │                                            ├─ hwlp_disable()
               │                                            ├─ hrtimer_cancel()
               │                                            └─ wait_for_completion()
               ├─ set_sample_period() [watchdog.c:667]
               │   sample = 4s (default)
               ├─ update_enable()
               ├─ softlockup_start_all()
               │   └─ for each CPU: smp_call_on_cpu ────→ watchdog_enable()
               │                                            ├─ hrtimer_setup + start
               │                                            ├─ update_touch_ts()
               │                                            └─ hwlp_enable()
               │                                                ├─ perf: create evt
               │                                                └─ buddy: touch+cpumask_set
               └─ hwlp_start()
               softlockup_initialized = true

 ─── userland init, module loading, etc ───

 lockup_detector_check()          [watchdog.c:1371, late_initcall_sync]
   ├─ allow_retry = false          [watchdog.c:1374]
   ├─ flush_work(detector_work)    [watchdog.c:1377]
   │   (等待任何 delayed init completion)
   └─ watchdog_sysctl_init()       [watchdog.c:1379]
        └─ /proc/sys/kernel/{watchdog,nmi_watchdog,soft_watchdog,watchdog_thresh,...}

 ─── free_initmem() ───

 hrtimer fires every 4s ────────────→ watchdog_timer_fn()  [watchdog.c:795]
                                       │ 正式运行
                                       ├─ watchdog_hardlockup_kick()
                                       ├─ softlockup_fn (stop_one_cpu_nowait)
                                       └─ is_softlockup()
```

---

## 12. 关键门控逻辑总结

| 条件 | 结果 |
|------|------|
| `tick_nohz_full_enabled()` | nohz_full CPU 排除在 watchdog_cpumask 外 |
| `watchdog_hardlockup_probe()` 返回非 0 | `allow_lockup_detector_init_retry = true`（boss CPU 延迟重试） |
| `!watchdog_user_enabled` | 所有检测器禁用 |
| `!watchdog_hardlockup_available` | 硬锁检测 bit 不置位 |
| `watchdog_thresh == 0` | hrtimer 不启动（softlockup_start_all 跳过） |
| `softlockup_initialized == false` | sysctl 写入静默忽略、stop_all 跳过 |

---

## 下一篇文章

[第二篇：Soft Lockup — hrtimer 驱动与 is_softlockup 判定](./02-soft-lockup.md)
