# Hard Lockup 与 Soft Lockup 的区别

> 📌 **详细专题**: [Hard/Soft Lockup 内核源码深度解析](./lockup-deep-dive/README.md) (4 篇系列)
> 源码: git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)

## 核心区别

| | Hard Lockup | Soft Lockup |
|---|---|---|
| **根因** | 中断被关闭 + CPU 进入死循环，NMI 也受阻 | `preempt_disable()` 段太久未释放，调度器饿死 |
| **检测机制** | Perf NMI (PMU 溢出) / Buddy CPU 互检 | hrtimer + `softlockup_fn` (`stop_one_cpu_nowait`) |
| **检测时间** | 6-20s (Perf) / 8-12s (Buddy) | 20s (2×`watchdog_thresh`) |
| **中断状态** | 硬中断全关 | 硬中断能来，hrtimer 正常 |
| **能否拿栈** | 能 (NMI 上下文) | 能 (进程上下文) |
| **sysfs 计数器** | `hardlockup_count` (`watchdog.c:233`) | `softlockup_count` (`watchdog.c:868`) |
| **典型日志** | `NMI watchdog: Watchdog detected hard LOCKUP on cpu 0` | `watchdog: BUG: soft lockup - CPU#0 stuck for 22s!` |

## 检测原理一句话

- **Soft lockup**: hrtimer 每 4s 通过 `stop_one_cpu_nowait` 把 `softlockup_fn` 调度到目标 CPU。如果目标 CPU 能执行这个函数（`update_touch_ts`），说明调度器没死。超过 20s 没更新 `touch_ts` → soft lockup。
- **Hard lockup (Perf)**: PMU 计数器溢出产生 NMI → `watchdog_overflow_callback` 检查 hrtimer 心跳计数器。计数器不变 → CPU 硬锁死。
- **Hard lockup (Buddy)**: 每个 CPU 的 hrtimer tick 检查"下一个 buddy CPU"的心跳计数器。没变 + 连续 miss 3 次 → 判定 buddy 硬锁死。

## 关键源码

| 组件 | 文件 | 行号 |
|------|------|------|
| 初始化全链路 | `kernel/watchdog.c` | `lockup_detector_init:1386`, `lockup_detector_setup:1035` |
| Soft lockup 判定 | `kernel/watchdog.c` | `watchdog_timer_fn:795`, `is_softlockup:741`, `softlockup_fn:785` |
| Hard lockup via Perf | `kernel/watchdog_perf.c` | `watchdog_overflow_callback:105` |
| Hard lockup 判定 | `kernel/watchdog.c` | `watchdog_hardlockup_check:207`, `is_hardlockup:183` |
| Buddy 互检 | `kernel/watchdog_buddy.c` | `watchdog_buddy_check_hardlockup:86` |
| x86 NMI period | `arch/x86/kernel/apic/hw_nmi.c` | `hw_nmi_get_sample_period:27` |
| sysctl 参数表 | `kernel/watchdog.c` | `watchdog_sysctls[]:1212-1314` |

## 专题系列

1. [初始化全链路 — lockup_detector_init](lockup-deep-dive/01-init.md)
2. [Soft Lockup — hrtimer 驱动与 is_softlockup 判定](lockup-deep-dive/02-soft-lockup.md)
3. [Hard Lockup Perf NMI — watchdog_overflow_callback](lockup-deep-dive/03-hard-lockup-perf.md)
4. [Hard Lockup Buddy + sysctl 用户接口](lockup-deep-dive/04-hard-lockup-buddy.md)
