# dpu iperf soft lockup

- [linux stop_machine 停机机制应用及一次触发 soft lockup 分析](https://blog.csdn.net/maybeYoc/article/details/135350981)
- [【OLK-5.10/openEuler-1.0-LTS】softlockup in rcu_momentary_dyntick_idle](https://gitee.com/openeuler/kernel/issues/I82QPR)
  - [!2282 sdei_watchdog: Avoid exception during sdei handler](https://gitee.com/openeuler/kernel/pulls/2282)

## crash

```bash
crash> sys
      KERNEL: /usr/lib/debug/lib/modules/5.10.0-136.12.0.88.4.ctl3.aarch64/vmlinux  [TAINTED]
    DUMPFILE: vmcore  [PARTIAL DUMP]
        CPUS: 256 [OFFLINE: 1]
        DATE: Thu Feb 27 15:52:42 CST 2025
      UPTIME: 1 days, 22:04:57
LOAD AVERAGE: 144.05, 56.30, 37.88
       TASKS: 3958
    NODENAME: gaoji-10-8-94-87
     RELEASE: 5.10.0-136.12.0.88.4.ctl3.aarch64
     VERSION: #1 SMP Tue Sep 10 12:39:42 UTC 2024
     MACHINE: aarch64  (unknown Mhz)
      MEMORY: 1023.7 GB
       PANIC: "Kernel panic - not syncing: softlockup: hung tasks"
```

```bash
crash> bt
PID: 463      TASK: ffff5dd79b964d00  CPU: 90   COMMAND: "migration/90"
 #0 [ffff8000102d3b70] __crash_kexec at ffffdbd07119d250
 #1 [ffff8000102d3d00] panic at ffffdbd071ccc0fc
 #2 [ffff8000102d3de0] watchdog_timer_fn at ffffdbd0711e485c
 #3 [ffff8000102d3e30] __run_hrtimer at ffffdbd07116f5c4
 #4 [ffff8000102d3e80] __hrtimer_run_queues at ffffdbd07116f87c
 #5 [ffff8000102d3ee0] hrtimer_interrupt at ffffdbd07116fec8
 #6 [ffff8000102d3f50] arch_timer_handler_phys at ffffdbd071a52c98
 #7 [ffff8000102d3f60] handle_percpu_devid_irq at ffffdbd0711423cc
 #8 [ffff8000102d3f90] __handle_domain_irq at ffffdbd071139260
 #9 [ffff8000102d3fd0] gic_handle_irq at ffffdbd071000124
--- <IRQ stack> ---
#10 [ffff80001401bd40] el1_irq at ffffdbd071002374
#11 [ffff80001401bd60] rcu_momentary_dyntick_idle at ffffdbd0711505fc
#12 [ffff80001401bdb0] cpu_stopper_thread at ffffdbd0711c178c
#13 [ffff80001401be10] smpboot_thread_fn at ffffdbd0710d39a8
#14 [ffff80001401be50] kthread at ffffdbd0710ca188
```

```bash
crash> bt -l
PID: 463      TASK: ffff5dd79b964d00  CPU: 90   COMMAND: "migration/90"
 #0 [ffff8000102d3b70] __crash_kexec at ffffdbd07119d250
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/./arch/arm64/include/asm/kexec.h: 52
 #1 [ffff8000102d3d00] panic at ffffdbd071ccc0fc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/panic.c: 251
 #2 [ffff8000102d3de0] watchdog_timer_fn at ffffdbd0711e485c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/watchdog.c: 448
 #3 [ffff8000102d3e30] __run_hrtimer at ffffdbd07116f5c4
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/time/hrtimer.c: 1583
 #4 [ffff8000102d3e80] __hrtimer_run_queues at ffffdbd07116f87c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/time/hrtimer.c: 1647
 #5 [ffff8000102d3ee0] hrtimer_interrupt at ffffdbd07116fec8
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/time/hrtimer.c: 1709
 #6 [ffff8000102d3f50] arch_timer_handler_phys at ffffdbd071a52c98
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/drivers/clocksource/arm_arch_timer.c: 647
 #7 [ffff8000102d3f60] handle_percpu_devid_irq at ffffdbd0711423cc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/irq/chip.c: 933
 #8 [ffff8000102d3f90] __handle_domain_irq at ffffdbd071139260
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/./include/linux/irqdesc.h: 153
 #9 [ffff8000102d3fd0] gic_handle_irq at ffffdbd071000124
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/./include/linux/irqdesc.h: 171
--- <IRQ stack> ---
#10 [ffff80001401bd40] el1_irq at ffffdbd071002374
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/arch/arm64/kernel/entry.S: 672
#11 [ffff80001401bd60] rcu_momentary_dyntick_idle at ffffdbd0711505fc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/rcu/tree.c: 421
#12 [ffff80001401bdb0] cpu_stopper_thread at ffffdbd0711c178c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/stop_machine.c: 504
#13 [ffff80001401be10] smpboot_thread_fn at ffffdbd0710d39a8
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/smpboot.c: 164
#14 [ffff80001401be50] kthread at ffffdbd0710ca188
    /usr/src/debug/kernel-5.10.0-136.12.0.88.4.ctl3.aarch64/linux-5.10.0-136.12.0.88.4.ctl3.aarch64/kernel/kthread.c: 313
```

```bash
crash> ps | grep migration | grep IN | wc -l
254
crash> ps | grep migration | grep RU | wc -l
2
```

```bash
crash> ps | grep migration | grep RU
>     463       2  90  ffff5dd79b964d00  RU   0.0        0        0  [migration/90]
      974       2 192  ffff7dd7111fcd00  RU   0.0        0        0  [migration/192]
```

```bash
crash> ps | grep migration
       14       2   0  ffff5dd79a79b9c0  IN   0.0        0        0  [migration/0]
       17       2   1  ffff5dd79aa58000  IN   0.0        0        0  [migration/1]
       22       2   2  ffff5dd79aa94d00  IN   0.0        0        0  [migration/2]
       27       2   3  ffff5dd79aada680  IN   0.0        0        0  [migration/3]
       32       2   4  ffff5dd79ab239c0  IN   0.0        0        0  [migration/4]
       38       2   5  ffff5dd79ab38000  IN   0.0        0        0  [migration/5]
```

```bash
crash> bt -p 974
PID: 974      TASK: ffff7dd7111fcd00  CPU: 192  COMMAND: "migration/192"
 #0 [ffff80001673bd50] __switch_to at ffffdbd0710098ec
 #1 [ffff80001673bd70] __schedule at ffffdbd071ce8124
 #2 [ffff80001673bdf0] schedule at ffffdbd071ce852c
 #3 [ffff80001673be10] smpboot_thread_fn at ffffdbd0710d39e4
 #4 [ffff80001673be50] kthread at ffffdbd0710ca188
```

## 初步结论

这看起来像是 Linux 内核崩溃（kernel panic）的日志，具体是由“softlockup: hung tasks”（软锁死：任务挂起）触发的。以下是对日志的分析和可能的修复建议：

### 1. **日志分析**
- **时间和系统信息**：
  - 崩溃发生在 2025 年 2 月 27 日 15:52:42 CST。
  - 系统运行了 1 天 22 小时 94 分钟 57 秒。
  - 内核版本为 `5.10.0-136.12.0.88.4.ctl3.aarch64`，标记为 `[TAINTED]`，表示内核可能存在第三方模块或不推荐的配置。
  - 机器是基于 `aarch64` 架构（ARM 64 位），内存为 1023.7 GB，CPU 核心数为 256（其中 11 个离线）。
  - 崩溃的文件为 `vmcore`（内核转储文件），表示系统在崩溃时生成了内存转储。

- **崩溃原因**：
  - 主要问题是一个软锁死（softlockup），意味着某个或某些任务在 CPU 上运行时间过长（通常超过 20 秒），导致系统无法响应。
  - 涉及的任务（PID 463，命令为 `migration/90`）可能与 CPU 迁移线程（migration thread）有关，这通常用于管理进程在多核 CPU 之间的调度。

- **调用栈（Backtrace）**：
  - 日志显示了崩溃时的调用栈，涉及多个内核函数，如 `crash_kexec`、`panic`、`watchdog_timer_fn` 等。
  - 其中 `watchdog_timer_fn` 通常与内核的看门狗（watchdog）机制有关，用于检测系统是否响应。如果看门狗检测到任务挂起，它会触发 panic。
  - 其他函数（如 `hrtimer_interrupt`、`handle_percpu_devirq`）表明问题可能与定时器、中断或多核调度相关。

- **TAINTED 标记**：
  - `[TAINTED]` 提示内核可能被第三方模块或不受支持的硬件/驱动污染。这可能是问题的根源之一。

### 2. **可能的原因**
根据日志，以下是可能导致软锁死的原因：
- **硬件问题**：如 CPU 过热、内存错误或硬件不兼容。
- **内核问题**：内核版本可能存在 bug，或者配置不正确（例如启用了一些实验性功能）。
- **驱动问题**：第三方驱动（如 GPU、存储或网络驱动）可能不稳定，导致内核崩溃。
- **负载过高**：系统负载较高（日志显示负载平均值为 56.30、37.88），可能导致任务挂起。
- **配置问题**：内核参数或调度器配置可能不适合当前工作负载。
