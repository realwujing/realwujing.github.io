# Hard/Soft Lockup 内核源码深度解析

> 📌 源码：`git@github.com:torvalds/linux.git`, `torvalds/master`, `eb3f4b7426cf` (v7.1-rc5-26)

## 系列文章

本系列深入解析 Linux 内核 hard/soft lockup 检测机制的完整源码实现，从初始化到逐行剖析检测逻辑，覆盖 perf NMI 和 buddy 两种 hardlockup 检测器。

| 篇目 | 标题 | 内容 |
|------|------|------|
| 第一篇 | [初始化 — lockup_detector_init 全链路](./01-init.md) | 启动流程、内核参数解析、sysctl 接口、CPU hotplug |
| 第二篇 | [Soft Lockup — hrtimer 驱动与 is_softlockup 判定](./02-soft-lockup.md) | hrtimer 心跳、touch 机制、softlockup_fn 调度、中断风暴检测 |
| 第三篇 | [Hard Lockup (Perf) — NMI 事件与溢出回调](./03-hard-lockup-perf.md) | perf event 创建、NMI 溢出回调、is_hardlockup 判定、时间戳校验 |
| 第四篇 | [Hard Lockup (Buddy) — CPU 互检与 miss 阈值](./04-hard-lockup-buddy.md) | buddy 环形检测链、hrtimer_interrupts 计数、CPU hotplug 屏障 |

## 总体架构

```
lockup_detector_init()                              [watchdog.c:1386]
  ├─ cpumask_copy(watchdog_cpumask, housekeeping)   [watchdog.c:1391]
  ├─ watchdog_hardlockup_probe()                    [watchdog.c:1394]
  │   ├─ perf: 创建临时 event 验证 PMU               [watchdog_perf.c:259]
  │   └─ buddy: 设置 miss_thresh=3                  [watchdog_buddy.c:22]
  └─ lockup_detector_setup()                        [watchdog.c:1399]
       └─ __lockup_detector_reconfigure()            [watchdog.c:1002]
            ├─ watchdog_hardlockup_stop()            [weak]
            ├─ softlockup_stop_all()                 [watchdog.c:960]
            ├─ set_sample_period()                   [watchdog.c:667]
            ├─ lockup_detector_update_enable()        [watchdog.c:354]
            ├─ softlockup_start_all()                [watchdog.c:979]
            └─ watchdog_hardlockup_start()           [weak]

┌─────────────────────────────────────────────────────┐
│ hrtimer (sample_period = 4s)                         │
│   │                                                   │
│   ▼                                                   │
│ watchdog_timer_fn()               [watchdog.c:795]   │
│   ├─ watchdog_hardlockup_kick()   [watchdog.c:199]   │
│   │   ├─ atomic_inc(hrtimer_interrupts) // 心跳+1   │
│   │   └─ watchdog_buddy_check_hardlockup()           │
│   │       └─ watchdog_hardlockup_check(next_cpu)     │
│   │           └─ is_hardlockup() 判定                │
│   ├─ stop_one_cpu_nowait(softlockup_fn)              │
│   │   └─ update_touch_ts() — "我还活着"              │
│   └─ is_softlockup(touch_ts, period_ts, now)          │
│       └─ now - touch_ts > 2*thresh → BUG             │
│                                                       │
│ perf NMI (独立触发)                                    │
│   └─ watchdog_overflow_callback()                    │
│       [watchdog_perf.c:105]                           │
│       └─ watchdog_hardlockup_check(cpu, regs)        │
│           └─ is_hardlockup() (hrtimer计数不增)       │
└─────────────────────────────────────────────────────┘
```

## 前置知识

阅读本系列前，建议了解以下概念：

- **hrtimer**：高精度定时器子系统，每个 CPU 注册一个周期性 hrtimer
- **perf_event**：硬件性能计数器，NMI 硬锁检测依赖 PMU 溢出中断
- **stop_one_cpu_nowait**：CPU stop 机制，强制目标 CPU 执行指定函数
- **/proc/sys/kernel 接口**：
  - `watchdog` — 总开关 (0/1)
  - `nmi_watchdog` — hardlockup 开关
  - `soft_watchdog` — softlockup 开关
  - `watchdog_thresh` — 阈值秒数 (默认 10)
  - `watchdog_cpumask` — 监控 CPU 掩码
  - `softlockup_panic` — softlockup 是否 panic
  - `hardlockup_panic` — hardlockup 是否 panic

## 关键源文件

| 文件 | 描述 |
|------|------|
| `kernel/watchdog.c` | Lockup 检测核心：初始化、softlockup、hardlockup 判定、sysctl |
| `kernel/watchdog_perf.c` | 基于 perf 的 hardlockup 检测器实现 |
| `kernel/watchdog_buddy.c` | 基于 buddy 的 hardlockup 检测器实现 |
| `include/linux/nmi.h` | 公共 API 声明、`watchdog_enabled` 位掩码常量 |
