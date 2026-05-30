# dpu iperf soft lockup

- [linux stop_machine 停机机制应用及一次触发 soft lockup 分析](https://blog.csdn.net/maybeYoc/article/details/135350981)
- [【OLK-5.10/openEuler-1.0-LTS】softlockup in rcu_momentary_dyntick_idle](https://gitee.com/openeuler/kernel/issues/I82QPR)
  - [!2282 sdei_watchdog: Avoid exception during sdei handler](https://gitee.com/openeuler/kernel/pulls/2282)

## dmesg

- https://gitee.com/openeuler/community-issue/issues/IA5ACV?skip_mobile=true

```c
40825 [329724.749710] watchdog: BUG: soft lockup - CPU#8 stuck for 23s! [migration/8:53]
40826 [329724.758280] Modules linked in: btrfs blake2b_generic xor xor_neon raid6_pq ntfs msdos 8021q garp stp mrp llc bonding rfkill rpcrdma sunrpc rdma_ucm ib_srpt ib_isert iscsi_target_mod vfat target_core_mod fat ib_iser rdma_cm iw_cm ib_cm ext4 libiscsi scsi_transport_iscsi m      bcache jbd2 sg hns_roce_hw_v2 ib_uverbs ib_core hns3_pmu uio_pdrv_genirq uio hibmc_drm drm_vram_helper ofpart drm_ttm_helper ipmi_ssif cmdlinepart ttm ipmi_devintf spi_nor mtd ipmi_msghandler spi_hisi_sfc_v3xx hisi_uncore_cpa_pmu hisi_uncore_pa_pmu hisi_uncore_l3c_pmu hisi_u      ncore_ddrc_pmu hisi_uncore_hha_pmu hisi_uncore_sllc_pmu hisi_ptt hisi_pcie_pmu hisi_uncore_pmu sch_fq_codel fuse xfs libcrc32c sd_mod t10_pi virtio_net net_failover virtio_blk failover ghash_ce sm4_ce sm4_ce_cipher sm4 sm3_ce sha3_ce sha3_generic hisi_sas_v3_hw sha512_ce ahc      i hisi_sas_main sha512_arm64 virtio_pci sha2_ce hclge libsas libahci virtio sha256_arm64 sha1_ce sbsa_gwdt virtio_pci_modern_dev libata hns3 virtio_ring hnae3
40827 [329724.758363]  scsi_transport_sas host_edma_drv hisi_trng_v2 hns_mdio dm_mirror dm_region_hash dm_log dm_mod aes_ce_blk crypto_simd cryptd aes_ce_cipher
40828 [329724.863203] CPU: 8 PID: 53 Comm: migration/8 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.88.4.yql.ctl3.aarch64 #1
40829 [329724.876429] Hardware name: Huawei S920X20/BC83AMDA, BIOS 09.04.02.01.13 01/11/2025
40830 [329724.885280] pstate: 60400009 (nZCv daif +PAN -UAO -TCO BTYPE=--)
40831 [329724.892563] pc : rcu_momentary_dyntick_idle+0x40/0x60
40832 [329724.898931] lr : multi_cpu_stop+0x100/0x1a0
40833 [329724.904391] sp : ffff800012093d60
40834 [329724.908982] x29: ffff800012093d60 x28: ffff80006ac139e0
40835 [329724.915591] x27: ffffb88970482dd0 x26: 00000000000000e0
40836 [329724.922207] x25: 0000000000000000 x24: 0000000000000000
40837 [329724.928793] x23: 0000000000000000 x22: ffffb88970ff0418
40838 [329724.935385] x21: 0000000000000001 x20: ffff80006ac139e0
40839 [329724.941995] x19: 0000000000000001 x18: 0000000000000000
40840 [329724.948587] x17: 0000000000000000 x16: 0000000000000000
40841 [329724.955177] x15: 0000000000000000 x14: 0000000000000000
40842 [329724.961763] x13: 0000000000000000 x12: 0000000000000000
40843 [329724.968346] x11: 00000a74804c995c x10: 19d0e478f99fa735
40844 [329724.974917] x9 : ffffb88970482e70 x8 : ffff12795ab84c88
40845 [329724.981495] x7 : 00000000000001e7 x6 : 0000000000000008
40846 [329724.988080] x5 : 00000000480fd020 x4 : 0000000000000000
40847 [329724.994708] x3 : 00000000f465d74a x2 : ffff12f6bdfb0920
40848 [329725.001286] x1 : 00000000f465d74e x0 : ffff12f6bdfb0800
40849 [329725.007858] Call trace:
40850 [329725.011564]  rcu_momentary_dyntick_idle+0x40/0x60
40851 [329725.017565]  cpu_stopper_thread+0x100/0x1b0
40852 [329725.023016]  smpboot_thread_fn+0x15c/0x1a0
40853 [329725.028373]  kthread+0x108/0x13c
40854 [329725.032893]  ret_from_fork+0x10/0x18
40855 [329725.037722] Kernel panic - not syncing: softlockup: hung tasks
40856 [329725.044834] CPU: 8 PID: 53 Comm: migration/8 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.88.4.yql.ctl3.aarch64 #1
40857 [329725.058096] Hardware name: Huawei S920X20/BC83AMDA, BIOS 09.04.02.01.13 01/11/2025
40858 [329725.066964] Call trace:
40859 [329725.070717]  dump_backtrace+0x0/0x1e4
40860 [329725.075690]  show_stack+0x20/0x2c
40861 [329725.080307]  dump_stack+0xd8/0x140
40862 [329725.085000]  panic+0x168/0x390
40863 [329725.089349]  watchdog_timer_fn+0x230/0x290
40864 [329725.094725]  __run_hrtimer+0x98/0x2a0
40865 [329725.099642]  __hrtimer_run_queues+0xb0/0x134
40866 [329725.105202]  hrtimer_interrupt+0x13c/0x3c0
40867 [329725.110535]  arch_timer_handler_phys+0x3c/0x50
40868 [329725.116203]  handle_percpu_devid_irq+0x90/0x1f4
40869 [329725.121937]  __handle_domain_irq+0x84/0xf0
40870 [329725.127220]  gic_handle_irq+0x78/0x2c0
40871 [329725.132142]  el1_irq+0xb8/0x140
40872 [329725.136442]  rcu_momentary_dyntick_idle+0x40/0x60
40873 [329725.142325]  cpu_stopper_thread+0x100/0x1b0
40874 [329725.147641]  smpboot_thread_fn+0x15c/0x1a0
40875 [329725.152880]  kthread+0x108/0x13c
40876 [329725.157211]  ret_from_fork+0x10/0x18
40877 [329725.162035] SMP: stopping secondary CPUs
40878 [329725.177300] Starting crashdump kernel...
40879 [329725.188123] Bye!
```

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
