---
title: 'do_coredump'
date: '2025/11/21 18:06:27'
updated: '2025/11/21 18:06:27'
---

# do_coredump

![console_unlock1](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/console_unlock1.png)

```c
[299885.010654] x13: 0000000000000020 x12: ffff206a17b32cb0
[299885.010656] x11: 0000000000000008 x10: ffffc2be01c45710
[299885.010657] x9 : ffffc2be00fb3280 x8 : 0000000000000000
[299885.010684] x7 : ffff406953362800 x6 : 0000000000000000
[299885.010686] x5 : 0000000000000000 x4 : 0000000000000000
[299885.010687] x3 : 0000000000000000 x2 : 00000000ffffffff
[299885.010688] x1 : fffff767fc740000 x0 : ffff4125fec523a8
[299885.010691] Call trace:
[299885.010712] console_unlock+0x1b0/0x3e0
[299885.010714] vprintk_emit+0x19c/0x1f4
[299885.010715] vprintk_default+0x40/0x4c
[299885.010717] vprintk_func+0x10c/0x2d0
[299885.010742] printk+0x64/0x8c
[299885.010745] do_coredump+0x7a0/0x7f0
[299885.010774] get_signal+0x1d0/0x71c
[299885.010776] do_signal+0x13c/0x1f0
[299885.010777] do_notify_resume+0x158/0x24c
[299885.010779] work_pending+0xc/0xa4
[299885.010803] Kernel panic - not syncing: softlockup: hung tasks
[299885.010805] CPU: 229 PID: 1221286 Comm: stress-ng-munmap Kdump: loaded Tainted: G    W    L
    5.10.0-136.12.0.90.ctl3.aarch64 #1
[299885.010806] Hardware name: Huawei To be filled by O.E.M./BC83AMDAE, BIOS 09.04.02.01.13 01/11/
2025
[299885.010807] Call trace:
[299885.010808] dump_backtrace+0x0/0x1e4
[299885.010809] show_stack+0x20/0x2c
[299885.010832] dump_stack+0xd8/0x140
[299885.010833] panic+0x168/0x390
[299885.010838] watchdog_timer_fn+0x230/0x290
[299885.010864] __run_hrtimer+0x98/0x2a0
[299885.010866] __hrtimer_run_queues+0xb0/0x134
[299885.010867] hrtimer_interrupt+0x13c/0x3c0
[299885.010870] arch_timer_handler_phys+0x3c/0x50
[299885.010893] handle_percpu_devid_irq+0x90/0x1f4
[299885.010895] __handle_domain_irq+0x84/0xf0
[299885.010896] gic_handle_irq+0x78/0x2c0
[299885.010898] el1_irq+0xb8/0x140
[299885.010899] console_unlock+0x1b0/0x3e0
[299885.010924] vprintk_emit+0x19c/0x1f4
[299885.010926] vprintk_default+0x40/0x4c
[299885.010927] vprintk_func+0x10c/0x2d0
[299885.010928] printk+0x64/0x8c
[299885.010929] do_coredump+0x7a0/0x7f0
[299885.010931] get_signal+0x1d0/0x71c
[299885.010959] do_signal+0x13c/0x1f0
[299885.010960] do_notify_resume+0x158/0x24c
[299885.010961] work_pending+0xc/0xa4
[299885.011465] SMP: stopping secondary CPUs
[299885.015604] Starting crashdump kernel...
[299885.015607] Bye!
```
