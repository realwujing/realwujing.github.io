---
title: 'rcu hard lockup'
date: '2025/06/01 17:06:52'
updated: '2025/06/17 19:26:08'
---

# rcu hard lockup

## 问题信息

1. 故障时间：2025-05-11-23:11:38
2. 故障节点：内蒙古呼和浩特IT上云1 10.141.181.26
3. 故障现象：PANIC: "Kernel panic - not syncing: Hard LOCKUP"
4. 操作系统：4.19.90-2102.2.0.0062.ctl2.aarch64
5. cpu: Kunpeng-920

## 问题分析

### vmcore-dmesg.txt

#### Kernel panic - not syncing: Hard LOCKUP

```bash
1762 [  929.239675] NMI watchdog: Watchdog detected hard LOCKUP on cpu 11
1763 [  929.239675] Modules linked in: tcp_diag inet_diag binfmt_misc bonding ipt_REJECT nf_reject_ipv4 iptable_filter rfkill ip6t_REJECT nf_reject_ipv6 xt_state xt_conntrack nf_conntrack nf_defrag_ipv6 nf_defrag_ipv4 ip6table_fi     lter ip6_tables hns_roce_hw_v2 hns_roce ib_core sunrpc vfat fat ext4 mbcache jbd2 hclge aes_ce_blk crypto_simd cryptd ses aes_ce_cipher enclosure ghash_ce sha2_ce sha256_arm64 sha1_ce ofpart hisi_sas_v3_hw sbsa_gwdt hns3 cmd     linepart hisi_sas_main ipmi_ssif sg libsas hi_sfc hnae3 ipmi_devintf scsi_transport_sas nfit i2c_designware_platform mtd ipmi_msghandler i2c_designware_core libnvdimm spi_dw_mmio sch_fq_codel ip_tables xfs libcrc32c dm_multi     path sd_mod mlx5_core(OE) mlxfw(OE) tls strparser psample mlxdevm(OE) auxiliary(OE) ahci libahci libata megaraid_sas(OE) mlx_compat(OE) devlink
1764 [  929.239691]  dm_mirror dm_region_hash dm_log dm_mod
1765 [  929.239693] CPU: 11 PID: 0 Comm: swapper/11 Kdump: loaded Tainted: G           OE     4.19.90-2102.2.0.0062.ctl2.aarch64 #1
1766 [  929.239693] Hardware name: RCSIT TG225 B1/BC82AMDQ, BIOS 5.25 09/14/2022
1767 [  929.239693] pstate: 80400089 (Nzcv daIf +PAN -UAO)
1768 [  929.239694] pc : queued_spin_lock_slowpath+0x1d8/0x320
1769 [  929.239694] lr : rcu_process_callbacks+0x544/0x5c0
1770 [  929.239694] sp : ffff00000816fe00
1771 [  929.239695] x29: ffff00000816fe00 x28: ffff3eb87f209280
1772 [  929.239695] x27: 000080ad00d40000 x26: ffff3eb87f1d5000
1773 [  929.239696] x25: ffffbf657fbfe280 x24: ffff3eb87f1d3000
1774 [  929.239697] x23: ffff3eb87eea0000 x22: ffff3eb87f1fe000
1775 [  929.239698] x21: ffff3eb87eebe280 x20: 0000000000380101
1776 [  929.239698] x19: ffff3eb87f209280 x18: ffff3eb87f1d1000
1777 [  929.239699] x17: 0000000000000000 x16: 0000000000000000
1778 [  929.239700] x15: 0000000000000001 x14: 0000000000000000
1779 [  929.239701] x13: 0000000000000004 x12: 0000000000000000
1780 [  929.239701] x11: ffff3eb87e9ee348 x10: 0000000000000000
1781 [  929.239702] x9 : 0000000000000000 x8 : 0000000000000000
1782 [  929.239703] x7 : ffff3eb87f1d37c8 x6 : ffffbf657fbfe240
1783 [  929.239704] x5 : 0000000000000000 x4 : ffffbf657fbfe240
1784 [  929.239704] x3 : ffff3eb87eebe000 x2 : 0000000000300000
1785 [  929.239705] x1 : 0000000000000000 x0 : ffffbf657fbfe248
1786 [  929.239706] Call trace:
1787 [  929.239706]  queued_spin_lock_slowpath+0x1d8/0x320
1788 [  929.239706]  rcu_process_callbacks+0x544/0x5c0
1789 [  929.239707]  __do_softirq+0x11c/0x33c
1790 [  929.239707]  irq_exit+0x11c/0x128
1791 [  929.239708]  __handle_domain_irq+0x6c/0xc0
1792 [  929.239708]  gic_handle_irq+0x6c/0x170
1793 [  929.239708]  el1_irq+0xb8/0x140
1794 [  929.239709]  arch_cpu_idle+0x38/0x1d0
1795 [  929.239709]  default_idle_call+0x24/0x58
1796 [  929.239709]  do_idle+0x1d4/0x2b0
1797 [  929.239710]  cpu_startup_entry+0x28/0x78
1798 [  929.239710]  secondary_start_kernel+0x17c/0x1c8
1799 [  929.239711] Kernel panic - not syncing: Hard LOCKUP
1800 [  929.239711] CPU: 11 PID: 0 Comm: swapper/11 Kdump: loaded Tainted: G           OE     4.19.90-2102.2.0.0062.ctl2.aarch64 #1
1801 [  929.239712] Hardware name: RCSIT TG225 B1/BC82AMDQ, BIOS 5.25 09/14/2022
1802 [  929.239712] Call trace:
1803 [  929.239712]  dump_backtrace+0x0/0x198
1804 [  929.239713]  show_stack+0x24/0x30
1805 [  929.239713]  dump_stack+0xa4/0xe8
1806 [  929.239713]  panic+0x130/0x318
1807 [  929.239713]  __stack_chk_fail+0x0/0x28
1808 [  929.239714]  watchdog_hardlockup_check+0x138/0x158
1809 [  929.239714]  sdei_watchdog_callback+0x64/0x98
1810 [  929.239715]  sdei_event_handler+0x50/0xf0
1811 [  929.239715]  __sdei_handler+0xe0/0x240
1812 [  929.239715]  __sdei_asm_handler+0xbc/0x154
1813 [  929.239716]  queued_spin_lock_slowpath+0x1d8/0x320
1814 [  929.239716]  rcu_process_callbacks+0x544/0x5c0
1815 [  929.239716]  __do_softirq+0x11c/0x33c
1816 [  929.239717]  irq_exit+0x11c/0x128
1817 [  929.239717]  __handle_domain_irq+0x6c/0xc0
1818 [  929.239717]  gic_handle_irq+0x6c/0x170
1819 [  929.239718]  el1_irq+0xb8/0x140
1820 [  929.239718]  arch_cpu_idle+0x38/0x1d0
1821 [  929.239718]  default_idle_call+0x24/0x58
1822 [  929.239719]  do_idle+0x1d4/0x2b0
1823 [  929.239719]  cpu_startup_entry+0x28/0x78
1824 [  929.239719]  secondary_start_kernel+0x17c/0x1c8
```

#### NMI watchdog

```bash
[root@obs-arm-worker-01 127.0.0.1-2025-05-11-23:11:38]# grep 'NMI watchdog:' -inr vmcore-dmesg.txt --color
1208:[    9.230149] SDEI NMI watchdog: SDEI Watchdog registered successfully
1688:[  929.239598] NMI watchdog: Watchdog detected hard LOCKUP on cpu 5
1725:[  929.239638] NMI watchdog: Watchdog detected hard LOCKUP on cpu 9
1762:[  929.239675] NMI watchdog: Watchdog detected hard LOCKUP on cpu 11
1825:[  929.239720] NMI watchdog: Watchdog detected hard LOCKUP on cpu 13
1915:[  936.855640] NMI watchdog: Watchdog detected hard LOCKUP on cpu 60
1946:[  949.303293] NMI watchdog: Watchdog detected hard LOCKUP on cpu 2
1983:[  949.303334] NMI watchdog: Watchdog detected hard LOCKUP on cpu 6
2020:[  949.303374] NMI watchdog: Watchdog detected hard LOCKUP on cpu 7
2057:[  949.303413] NMI watchdog: Watchdog detected hard LOCKUP on cpu 12
2094:[  949.303456] NMI watchdog: Watchdog detected hard LOCKUP on cpu 23
```

首先发生在5号CPU的NMI watchdog，5号cpu的x19: ffff3eb87f209280，和11号CPU的x19是一样的。

#### RCU Stall 日志中 `idle=XXX/0/0x1` 标志位解析

```bash
grep 'idle=' . -inr --include="vmcore-dmesg.txt"

./127.0.0.1-2025-05-11-23:11:38/vmcore-dmesg.txt:1585:[  916.158850] rcu:       44-...!: (1 ticks this GP) idle=4ca/0/0x1 softirq=1046/1046 fqs=0 
./127.0.0.1-2025-05-11-18:26:08/vmcore-dmesg.txt:1584:[887136.218705] rcu:      5-...!: (1 GPs behind) idle=e32/0/0x1 softirq=2547154/2547154 fqs=1 
./127.0.0.1-2025-05-11-18:26:08/vmcore-dmesg.txt:1645:[1622263.640745] rcu:     47-...!: (1 ticks this GP) idle=a36/0/0x1 softirq=2425136/2425136 fqs=0 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:1585:[ 3885.234398] rcu:       29-...!: (6 GPs behind) idle=a6a/0/0x1 softirq=11528/11528 fqs=1 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:1617:[15488.588605] rcu:       58-...!: (3 GPs behind) idle=e52/0/0x1 softirq=72598/72598 fqs=1 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:1736:[49506.456605] rcu:       53-...!: (1 ticks this GP) idle=70e/0/0x1 softirq=164776/164776 fqs=0 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:2475:[190935.928694] rcu:      35-...!: (1 ticks this GP) idle=576/0/0x1 softirq=680003/680003 fqs=0 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:3132:[223980.504804] rcu:      47-...!: (2 ticks this GP) idle=d5e/0/0x1 softirq=893065/893066 fqs=1 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:3164:[230566.588728] rcu:      18-...!: (1 GPs behind) idle=9e6/1/0x4000000000000002 softirq=730680/730682 fqs=1 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:3308:[320459.544436] rcu:      55-...!: (1 ticks this GP) idle=39e/0/0x1 softirq=1181112/1181112 fqs=1 
./127.0.0.1-2025-05-26-02:29:07/vmcore-dmesg.txt:3340:[601083.460709] rcu:      59-...!: (1 ticks this GP) idle=436/0/0x1 softirq=5886999/5886999 fqs=0
```

idle陷入深度睡眠后未能被唤醒，无法响应时钟中断，进而引发 RCU Stall、调度停滞等问题？

##### 定时器停止的底层原理

###### 1. 硬件行为
- **时钟源停摆**：
  - 在深度 idle 状态（如 ARM 的 WFI 或 x86 的 C3）下，为节省功耗，CPU 可能关闭本地时钟（如 `arch_timer` 或 TSC）。
  - 本地定时器中断（如 `tick_sched_timer`）不再触发。
  - `jiffies` 停止更新（依赖时钟中断递增）。

- **唤醒依赖**：
  - 需依赖外部中断唤醒 CPU，包括：
    - 处理器间中断（IPI）：其他 CPU 通过 `smp_call_function()` 发送。
    - 设备中断：如网卡、磁盘等外设触发的中断。
    - 全局时钟中断：部分架构的全局定时器（如 ARM sp804）可能仍运行。

###### 2. 软件配置
- **内核控制**：
  - 通过 `cpuidle` 驱动决定是否允许定时器停止：
    ```c
    static struct cpuidle_state states[] = {
        {
            .name = "C2",
            .flags = CPUIDLE_FLAG_TIMER_STOP, // 关键标志位
            .enter = arm_cpuidle_enter,
        },
    };
    ```

- **唤醒路径**：
  - 若定时器停止，内核需确保至少一个唤醒源有效：
    ```
    外部中断 → GIC/APIC → CPU 退出 idle → 恢复定时器 → 处理中断
    ```

##### 唤醒失败的常见原因

###### 1. 中断控制器故障
| 问题                     | 检测方法                                              | 解决方案                             |
|--------------------------|-----------------------------------------------------|-------------------------------------|
| GIC 未路由中断           | `cat /proc/interrupts` 查看目标 CPU 的中断计数是否增长 | 调整 `smp_affinity` 绑定中断         |
| APIC 配置错误            | `dmesg | grep -i "apic\ irq"` 检查日志错误            | 更新 BIOS 或内核 APIC 驱动          |
| 中断优先级过低           | 检查 GIC 的 `GICD_IPRIORITYRn` 寄存器（需 JTAG）     | 提高中断优先级                      |

###### 2. 电源管理缺陷
| 问题                     | 检测方法                                              | 解决方案                             |
|--------------------------|-----------------------------------------------------|-------------------------------------|
| PSCI 调用失败            | `dmesg | grep -i "psci"` 查看 CPU_ON/CPU_OFF 错误    | 更新 ATF（ARM Trusted Firmware）     |
| SoC 时钟未恢复           | 读取 SoC 寄存器（如 `CNTCTLBase.CNTCR`）确认时钟使能 | 联系厂商提供固件补丁                 |
| 电压域未退出             | 检查 PMIC 寄存器（需厂商工具）                        | 调整电源管理配置                     |

###### 3. 内核配置问题
| 问题                     | 检测方法                                              | 解决方案                             |
|--------------------------|-----------------------------------------------------|-------------------------------------|
| NO_HZ 配置冲突           | `cat /sys/kernel/debug/tracing/events/timer/timer_stop/enable` 追踪停钟事件 | 禁用 `CONFIG_NO_HZ_FULL`            |
| cpuidle 驱动缺陷         | `perf probe -L arm_cpuidle_enter` 检查驱动逻辑        | 升级或回退内核版本                   |
| RCU 回调阻塞             | `ftrace` 追踪 `rcu_core` 和 `irq_enter` 的延迟        | 修复长时间关中断的代码               |

##### 诊断工具与方法

###### 1. 硬件层诊断
- **检查 ARM 定时器状态（需 root）**：
  ```bash
  devmem2 0x<CNTCTLBase.CNTCR>  # 读时钟控制寄存器
  devmem2 0x<GICD_ISENABLER>    # 读中断使能状态
  ```

- **使用 JTAG 调试器验证 CPU 唤醒信号**：

  ```bash
  openocd -f <soc_config.cfg> -c "halt" -c "reg cpsr"
  ```

###### 2. 内核层诊断

- **监控 idle 状态切换**：

  ```bash
  echo 1 > /sys/kernel/debug/tracing/events/power/cpu_idle/enable
  cat /sys/kernel/debug/tracing/trace_pipe
  ```

- **追踪中断唤醒路径**：

  ```bash
  perf probe -a 'gic_handle_irq' -a 'arch_cpu_idle_enter'
  ```

###### 3. 日志分析重点

- **关键错误**：

  ```
  arch_timer: IRQ 30 missed      # 时钟中断丢失
  PSCI: CPU29 on failed (-5)     # 电源管理调用失败
  ```

- **唤醒成功日志**：

  ```
  cpu_idle: state=2 -> state=0    # 退出深度 idle
  irq_enter: irq=30              # 中断处理开始
  ```

##### 解决方案总结

###### 临时措施

- **强制禁用深度 idle（所有 CPU）**：

  ```bash
  for cpu in /sys/devices/system/cpu/cpu*/cpuidle/state[2-9]; do
      echo 1 > $cpu/disable
  done
  ```

- **关闭动态 tick**：

  ```bash
  echo 0 > /sys/kernel/debug/tracing/events/timer/timer_stop/enable
  ```

###### 长期修复

- **硬件**：更新微码/BIOS，修复时钟和中断控制器缺陷。
- **内核**：升级到修复版本（如 Linux 5.10+ 的 ARM idle 驱动补丁）。
- **配置**：调整内核启动参数：

  ```bash
  idle=poll                   # 完全禁用 idle
  clocksource=arch_sys_counter # 强制使用可靠时钟源
  ```

###### 厂商协作

- **华为"..鹏**：提交 `hisi_cpufreq` 驱动日志给厂商。
- **Intel**：收集 `pmc_core` 和 `intel_idle` 模块日志。

##### 深度 idle 唤醒流程图解

```plaintext
+-------------------+     +-------------------+     +-------------------+
| 进入深度 idle     |     | 定时器停止         |     | 等待外部中断       |
| (WFI/C3)         | --> | (CPUIDLE_FLAG_     | --> | (IPI/设备中断)     |
|                  |     | TIMER_STOP)        |     |                   |
+-------------------+     +-------------------+     +-------------------+
                                                    |
+-------------------+     +-------------------+     +-------------------+
| 中断控制器接收     | --> | CPU 退出 idle     | --> | 恢复定时器并       |
| (GIC/APIC)        |     | (PSCI_CPU_ON)      |     | 处理中断           |
+-------------------+     +-------------------+     +-------------------+
```

### crash

#### queued_spin_lock_slowpath+0x1d8

`pc : queued_spin_lock_slowpath+0x1d8/0x320`

```bash
crash> dis -sx queued_spin_lock_slowpath+0x1d8
FILE: kernel/locking/qspinlock.c
LINE: 507

dis: queued_spin_lock_slowpath+0x1d8: source code is not available
```

```bash
crash> dis -flx rcu_process_callbacks | grep -i 0x544 -C10
0xffff3eb87e0a8d4c <rcu_process_callbacks+0x524>:       nop
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree_plugin.h: 1381
0xffff3eb87e0a8d50 <rcu_process_callbacks+0x528>:       brk     #0x800
0xffff3eb87e0a8d54 <rcu_process_callbacks+0x52c>:       b       0xffff3eb87e0a88e8
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/./include/asm-generic/qspinlock.h: 88
0xffff3eb87e0a8d58 <rcu_process_callbacks+0x530>:       mov     w1, w0
0xffff3eb87e0a8d5c <rcu_process_callbacks+0x534>:       str     x4, [x29,#112]
0xffff3eb87e0a8d60 <rcu_process_callbacks+0x538>:       mov     x0, x6
0xffff3eb87e0a8d64 <rcu_process_callbacks+0x53c>:       str     x6, [x29,#128]
0xffff3eb87e0a8d68 <rcu_process_callbacks+0x540>:       bl      0xffff3eb87e07e558
0xffff3eb87e0a8d6c <rcu_process_callbacks+0x544>:       ldr     x4, [x29,#112]
0xffff3eb87e0a8d70 <rcu_process_callbacks+0x548>:       ldr     x6, [x29,#128]
0xffff3eb87e0a8d74 <rcu_process_callbacks+0x54c>:       b       0xffff3eb87e0a8c64
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree.c: 2621
0xffff3eb87e0a8d78 <rcu_process_callbacks+0x550>:       brk     #0x800
0xffff3eb87e0a8d7c <rcu_process_callbacks+0x554>:       b       0xffff3eb87e0a8b0c
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree.c: 2572
0xffff3eb87e0a8d80 <rcu_process_callbacks+0x558>:       brk     #0x800
0xffff3eb87e0a8d84 <rcu_process_callbacks+0x55c>:       b       0xffff3eb87e0a8a2c
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree.c: 2415
0xffff3eb87e0a8d88 <rcu_process_callbacks+0x560>:       strb    wzr, [x25,#26]
```

```bash
crash> sym 0xffff3eb87e0a8c64
ffff3eb87e0a8c64 (t) rcu_process_callbacks+1084 /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree.c: 2397
```

```c
// vim kernel/rcu/tree.c +2397

2387 static void
2388 rcu_report_qs_rdp(int cpu, struct rcu_state *rsp, struct rcu_data *rdp)
2389 {
2390     unsigned long flags;
2391     unsigned long mask;
2392     bool needwake;
2393     struct rcu_node *rnp;
2394
2395     rnp = rdp->mynode;
2396     raw_spin_lock_irqsave_rcu_node(rnp, flags);    // queued_spin_lock_slowpath+0x1d8入口在这
2397     if (rdp->cpu_no_qs.b.norm || rdp->gp_seq != rnp->gp_seq ||
2398     ¦   rdp->gpwrap) {
```

回溯一下调用链路：

```c
// vim kernel/rcu/tree.c +2427

2436 static void
2437 rcu_check_quiescent_state(struct rcu_state *rsp, struct rcu_data *rdp)
2438 {
2439     /* Check for grace-period ends and beginnings. */
2440     note_gp_changes(rsp, rdp);
2441
2442     /*
2443     ¦* Does this CPU still need to do its part for current grace period?
2444     ¦* If no, return and let the other CPUs do their part as well.
2445     ¦*/
2446     if (!rdp->core_needs_qs)
2447         return;
2448
2449     /*
2450     ¦* Was there a quiescent state since the beginning of the grace
2451     ¦* period? If no, then exit and wait for the next call.
2452     ¦*/
2453     if (rdp->cpu_no_qs.b.norm)
2454         return;
2455
2456     /*
2457     ¦* Tell RCU we are done (but rcu_report_qs_rdp() will be the
2458     ¦* judge of that).
2459     ¦*/
2460     rcu_report_qs_rdp(rdp->cpu, rsp, rdp);
2461 }
```

```c
// vim kernel/rcu/tree.c +2839

2838 static void
2839 __rcu_process_callbacks(struct rcu_state *rsp)
2840 {
2841     unsigned long flags;
2842     struct rcu_data *rdp = raw_cpu_ptr(rsp->rda);
2843     struct rcu_node *rnp = rdp->mynode;
2844
2845     WARN_ON_ONCE(!rdp->beenonline);
2846
2847     /* Update RCU state based on any recent quiescent states. */
2848     rcu_check_quiescent_state(rsp, rdp);
```

```c
// vim kernel/rcu/tree.c +2872

2872 static __latent_entropy void rcu_process_callbacks(struct softirq_action *unused)
2873 {
2874     struct rcu_state *rsp;
2875
2876     if (cpu_is_offline(smp_processor_id()))
2877         return;
2878     trace_rcu_utilization(TPS("Start RCU core"));
2879     for_each_rcu_flavor(rsp)
2880         __rcu_process_callbacks(rsp);
2881     trace_rcu_utilization(TPS("End RCU core"));
2882 }
```

```c
rcu_process_callbacks
    __rcu_process_callbacks
        rcu_check_quiescent_state
            rcu_report_qs_rdp
                raw_spin_lock_irqsave_rcu_node
                    queued_spin_lock_slowpath
```

```c
// vim -t raw_spin_lock_irqsave_rcu_node
// vim kernel/rcu/rcu.h +415

415 #define raw_spin_lock_irqsave_rcu_node(p, flags)            \
416 do {                                    \
417     raw_spin_lock_irqsave(&ACCESS_PRIVATE(p, lock), flags); \     // 访问p->lock
418     smp_mb__after_unlock_lock();                    \
419 } while (0)
```

raw_spin_lock_irqsave()、queued_spin_lock_slowpath()调用关系图：

```c
raw_spin_lock_irqsave()
 └── __raw_spin_lock_irqsave()
      └── __raw_spin_lock()
           └── arch_spin_lock()
                └── queued_spin_lock()       // fastpath
                     └── ... fallback ...
                          └── queued_spin_lock_slowpath() // 如果 fastpath 失败

```

```c

// vim kernel/locking/qspinlock.c +507
354 void queued_spin_lock_slowpath(struct qspinlock *lock, u32 val)
355 {
356     struct mcs_spinlock *prev, *next, *node;
357     u32 old, tail;
358     int idx;
...
432 pv_queue:
433     node = this_cpu_ptr(&qnodes[0].mcs);
...
506         pv_wait_node(node, prev);
507         arch_mcs_spin_lock_contended(&node->locked);
```

```bash
crash> dis -lx queued_spin_lock_slowpath | head -n8
/usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/locking/qspinlock.c: 355
0xffff3eb87e07e558 <queued_spin_lock_slowpath>: stp     x29, x30, [sp,#-32]!
0xffff3eb87e07e55c <queued_spin_lock_slowpath+0x4>:     mov     x29, sp
0xffff3eb87e07e560 <queued_spin_lock_slowpath+0x8>:     stp     x19, x20, [sp,#16]
0xffff3eb87e07e564 <queued_spin_lock_slowpath+0xc>:     mov     w20, w1
0xffff3eb87e07e568 <queued_spin_lock_slowpath+0x10>:    mov     x19, x0     // x0是qspinlock地址，原始地址保存在x19
0xffff3eb87e07e56c <queued_spin_lock_slowpath+0x14>:    mov     x0, x30
0xffff3eb87e07e570 <queued_spin_lock_slowpath+0x18>:    nop
```

从vmcore-dmesg.txt中可以看到发生panic的11号cpu，11号cpu的x19: ffff3eb87f209280:

```bash
crash> qspinlock ffff3eb87f209280
struct qspinlock {
  {
    val = {
      counter = 6291713
    },
    {
      locked = 1 '\001',    // 锁被持有
      pending = 1 '\001'    // 有1位等待者
    },
    {
      locked_pending = 257,
      tail = 96
    }
  }
}
```

```bash
crash> struct -o qspinlock
struct qspinlock {
      union {
  [0]     atomic_t val;
          struct {
  [0]         u8 locked;
  [1]         u8 pending;
          };
          struct {
  [0]         u16 locked_pending;
  [2]         u16 tail;
          };
      };
}
SIZE: 4
```

```c
// vim include/asm-generic/qspinlock_types.h +76

 68 /*
 69  * Bitfields in the atomic value:
 70  *
 71  * When NR_CPUS < 16K
 72  *  0- 7: locked byte
 73  *     8: pending
 74  *  9-15: not used
 75  * 16-17: tail index
 76  * 18-31: tail cpu (+1)
 77  *
 78  * When NR_CPUS >= 16K
 79  *  0- 7: locked byte
 80  *     8: pending
 81  *  9-10: tail index
 82  * 11-31: tail cpu (+1)
 83  */
```

根据提供的结构体定义和注释，`tail` 字段的编码方式如下（假设 `NR_CPUS < 16K`，即常见场景）：

- `tail`（16位）由高位的 CPU 编号和低位的队列索引组成：
  - **高位**：`tail cpu (+1)`，位于第 18-31 位（16-17 位是 tail index，18-31 位是 tail cpu+1）
  - **低 2 位**：`tail index`，即每CPU队列节点索引
  - **tail = (cpu+1) << 2 | idx`**

这里 `tail = 96`，即二进制 `0b00000000_01100000`。

---

##### 计算过程

1. **tail = 96 = 0x60 = 0b0110_0000**
2. **低 2 位**（`idx`）：`tail & 0x3`
   `96 & 0x3 = 0`
   所以 `idx = 0`

3. **高位**（`cpu+1`）：`tail >> 2`
   `96 >> 2 = 24`
   所以 `cpu + 1 = 24`，即 `cpu = 23`

---

##### 结论

**等待者的 CPU 编号是 23。**

**只能确定23号CPU正在等待这把锁，不能确定它就是锁的持有者。**

- qspinlock 的 tail 字段（这里为 96，解码为 cpu 23）**只表示当前等待队列的队尾是谁**，即最后一个排队等锁的 CPU 是 23。
- `locked = 1` 说明锁被某个 CPU 持有，但 qspinlock 结构本身**并不记录锁主是谁**。
   11 号 CPU 卡在 `queued_spin_lock_slowpath`，它可能是正在尝试获取锁的等待者之一，也可能是因为死锁等原因卡住，但不能据此断定 11 号 CPU 就是锁主。
- 23 号 CPU 作为队尾，**一定还没有拿到锁，只是在排队**。

---

#### cpu 23在干啥？

```bash
crash> bt -c 23
PID: 6214   TASK: ffffbf655d94d900  CPU: 23  COMMAND: "kworker/23:0"
bt: WARNING: cannot determine starting stack frame for task ffffbf655d94d900
```

```c
// vmcore-dmesg.txt

2094 [  949.303456] NMI watchdog: Watchdog detected hard LOCKUP on cpu 23
2095 [  949.303457] Modules linked in: tcp_diag inet_diag binfmt_misc bonding ipt_REJECT nf_reject_ipv4 iptable_filter rfkill ip6t_REJECT nf_reject_ipv6 xt_state xt_conntrack nf_conntrack nf_defrag_ipv6 nf_defrag_ipv4 i     p6table_filter ip6_tables hns_roce_hw_v2 hns_roce ib_core sunrpc vfat fat ext4 mbcache jbd2 hclge aes_ce_blk crypto_simd cryptd ses aes_ce_cipher enclosure ghash_ce sha2_ce sha256_arm64 sha1_ce ofpart hisi_sas_v3_h     w sbsa_gwdt hns3 cmdlinepart hisi_sas_main ipmi_ssif sg libsas hi_sfc hnae3 ipmi_devintf scsi_transport_sas nfit i2c_designware_platform mtd ipmi_msghandler i2c_designware_core libnvdimm spi_dw_mmio sch_fq_codel ip     _tables xfs libcrc32c dm_multipath sd_mod mlx5_core(OE) mlxfw(OE) tls strparser psample mlxdevm(OE) auxiliary(OE) ahci libahci libata megaraid_sas(OE) mlx_compat(OE) devlink
2096 [  949.303474]  dm_mirror dm_region_hash dm_log dm_mod
2097 [  949.303476] CPU: 23 PID: 6214 Comm: kworker/23:0 Kdump: loaded Tainted: G        W  OE     4.19.90-2102.2.0.0062.ctl2.aarch64 #1
2098 [  949.303477] Hardware name: RCSIT TG225 B1/BC82AMDQ, BIOS 5.25 09/14/2022
2099 [  949.303477] Workqueue: rcu_gp wait_rcu_exp_gp
2100 [  949.303478] pstate: 80c00089 (Nzcv daIf +PAN +UAO)
2101 [  949.303478] pc : queued_spin_lock_slowpath+0x1d8/0x320
2102 [  949.303479] lr : sync_rcu_exp_select_cpus+0x25c/0x3a8
2103 [  949.303479] sp : ffff000064bcfd10
2104 [  949.303479] x29: ffff000064bcfd10 x28: 0000000000000000
2105 [  949.303480] x27: 0000000000000060 x26: ffff3eb87f1d5adc
2106 [  949.303481] x25: 0000000000000000 x24: 0000000000000280
2107 [  949.303482] x23: ffff3eb87f209000 x22: ffff3eb87e0a61c8
2108 [  949.303483] x21: ffff3eb87f1d5000 x20: 0000000000340101
2109 [  949.303484] x19: ffff3eb87f209280 x18: ffff3eb87ec30518
2110 [  949.303485] x17: 0000000000000000 x16: 0000000000000000
2111 [  949.303485] x15: 0000000000000001 x14: ffffbf657fe22408
2112 [  949.303486] x13: 0000000000000004 x12: 0000000000000228
2113 [  949.303487] x11: 0000000000000020 x10: 0000000000000b90
2114 [  949.303488] x9 : ffff000064bcfd40 x8 : 0000000000000000
2115 [  949.303489] x7 : ffff3eb87f1d37c8 x6 : ffffbf657fe3e240
2116 [  949.303490] x5 : 0000000000000000 x4 : ffffbf657fe3e240
2117 [  949.303491] x3 : ffff3eb87eebe000 x2 : 0000000000600000
2118 [  949.303491] x1 : 0000000000000000 x0 : ffffbf657fe3e248
2119 [  949.303492] Call trace:
2120 [  949.303493]  queued_spin_lock_slowpath+0x1d8/0x320
2121 [  949.303493]  sync_rcu_exp_select_cpus+0x25c/0x3a8
2122 [  949.303494]  wait_rcu_exp_gp+0x28/0x40
2123 [  949.303494]  process_one_work+0x1b0/0x450
2124 [  949.303494]  worker_thread+0x54/0x468
2125 [  949.303495]  kthread+0x134/0x138
2126 [  949.303495]  ret_from_fork+0x10/0x18
```

```bash
crash> foreach bt > foreach_bt.log

12152 PID: 8564   TASK: ffffbf63ca351700  CPU: 23  COMMAND: "systemd-hostnam"
12153  #0 [ffff00006188fad0] __switch_to at ffff3eb87dfa8e0c
12154  #1 [ffff00006188faf0] __schedule at ffff3eb87e971438
12155  #2 [ffff00006188fb80] schedule at ffff3eb87e971b28
12156  #3 [ffff00006188fb90] _synchronize_rcu_expedited.constprop.49 at ffff3eb87e0aab98
12157  #4 [ffff00006188fca0] synchronize_sched_expedited at ffff3eb87e0aacac
12158  #5 [ffff00006188fcb0] synchronize_rcu_expedited at ffff3eb87e0aae88
12159  #6 [ffff00006188fcc0] namespace_unlock at ffff3eb87e2a4f78
12160  #7 [ffff00006188fcf0] drop_collected_mounts at ffff3eb87e2a7ee0
12161  #8 [ffff00006188fd20] put_mnt_ns at ffff3eb87e2aa1b0
12162  #9 [ffff00006188fd40] free_nsproxy at ffff3eb87e03dd30
12163 #10 [ffff00006188fd60] switch_task_namespaces at ffff3eb87e03df04
12164 #11 [ffff00006188fd80] exit_task_namespaces at ffff3eb87e03df48
12165 #12 [ffff00006188fda0] do_exit at ffff3eb87e017614
12166 #13 [ffff00006188fe10] do_group_exit at ffff3eb87e017968
12167 #14 [ffff00006188fe40] __arm64_sys_exit_group at ffff3eb87e017a28
12168 #15 [ffff00006188fe60] el0_svc_common at ffff3eb87dfb91ec
12169 #16 [ffff00006188fea0] el0_svc_handler at ffff3eb87dfb92e4
12170 #17 [ffff00006188fff0] el0_svc at ffff3eb87dfa4184
12171      PC: 0000fffd16566b4c   LR: 0000fffd164f9060   SP: 0000ffffd7e6d440
12172     X29: 0000ffffd7e6d440  X28: 0000fffd16635000  X27: 0000000000000000
12173     X26: 0000000000000002  X25: 0000fffd1669f708  X24: 0000fffd16630588
12174     X23: 0000fffd1662f000  X22: 0000000000000000  X21: 0000fffd16632308
12175     X20: 0000000000000008  X19: 0000000000000008  X18: 0000000000000001
12176     X17: 0000fffd164f93c8  X16: 0000fffd161afea0  X15: 0000000000000000
12177     X14: 0000fffd164c7208  X13: 0000fffd164d4960  X12: 0000fffd166a82b0
12178     X11: 0000000000000008  X10: 0000000000000000   X9: 0000fffd166a7150
12179      X8: 000000000000005e   X7: 0000000000000001   X6: 0000000000000000
12180      X5: 0000000000000000   X4: 0000000000000020   X3: 0000fffd151d0710
12181      X2: 0000000000000000   X1: 0000000000000000   X0: 0000000000000000
12182     ORIG_X0: 0000000000000000  SYSCALLNO: 5e  PSTATE: 60001000
```

```bash
crash> sym ffff3eb87e0aab98
ffff3eb87e0aab98 (t) _synchronize_rcu_expedited.constprop.49+728 /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/rcu/tree_exp.h: 686
```

```c
// vim kernel/rcu/tree_exp.h +686

674     } else {
675         /* Marshall arguments & schedule the expedited grace period. */
676         rew.rew_func = func;
677         rew.rew_rsp = rsp;
678         rew.rew_s = s;
679         INIT_WORK_ONSTACK(&rew.rew_work, wait_rcu_exp_gp);
680         queue_work(rcu_gp_wq, &rew.rew_work);
681     }
682
683     /* Wait for expedited grace period to complete. */
684     rdp = per_cpu_ptr(rsp->rda, raw_smp_processor_id());
685     rnp = rcu_get_root(rsp);
686     wait_event(rnp->exp_wq[rcu_seq_ctr(s) & 0x3],
687         ¦  sync_exp_work_done(rsp, s));
688     smp_mb(); /* Workqueue actions happen before return. */
689
690     /* Let the next expedited grace period start. */
691     mutex_unlock(&rsp->exp_mutex);
692 }
```

从`foreach_bt.log`中可以看到之前在cpu 23上的进程`systemd-hostnam`触发了`_synchronize_rcu_expedited.constprop.49`，它在等待一个加速的RCU回调完成。`systemd-hostnam`在等待rcu的加速回调完成时主动schedule了一个工作队列`rcu_gp_wq`，这个工作队列的回调函数是`wait_rcu_exp_gp`。从`vmcore-dmesg.txt`中可以看到`kworker/23:0`线程在执行`wait_rcu_exp_gp`时卡在了`queued_spin_lock_slowpath+0x1d8`，即在等待一个qspinlock。

##### wait_rcu_exp_gp

```c
// vim kernel/rcu/tree_exp.h +636

636 /*
637  * Work-queue handler to drive an expedited grace period forward.
638  */
639 static void wait_rcu_exp_gp(struct work_struct *wp)
640 {
641     struct rcu_exp_work *rewp;
642
643     rewp = container_of(wp, struct rcu_exp_work, rew_work);
644     rcu_exp_sel_wait_wake(rewp->rew_rsp, rewp->rew_func, rewp->rew_s);
645 }
```

```c
// vim kernel/rcu/tree_exp.h +626

626 static void rcu_exp_sel_wait_wake(struct rcu_state *rsp,
627                 ¦ smp_call_func_t func, unsigned long s)
628 {
629     /* Initialize the rcu_node tree in preparation for the wait. */
630     sync_rcu_exp_select_cpus(rsp, func);
631
632     /* Wait and clean up, including waking everyone. */
633     rcu_exp_wait_wake(rsp, s);
634 }
```

可以看到`wait_rcu_exp_gp`函数会调用`rcu_exp_sel_wait_wake`，而`rcu_exp_sel_wait_wake`又会通过ipi中断调用`sync_rcu_exp_select_cpus`让所有cpu主动检查静默期状态。

#### rcu_node与qspinlock关系

结合前文

`2396     raw_spin_lock_irqsave_rcu_node(rnp, flags);    // queued_spin_lock_slowpath+0x1d8入口在这`

`417     raw_spin_lock_irqsave(&ACCESS_PRIVATE(p, lock), flags); \     // 访问p->lock`

来看一下`rcu_node`与`qspinlock`的关系：

```bash
crash> struct -o rcu_node
struct rcu_node {
    [0] raw_spinlock_t lock;
    [8] unsigned long gp_seq;
   [16] unsigned long gp_seq_needed;
   [24] unsigned long completedqs;
   [32] unsigned long qsmask;
   [40] unsigned long rcu_gp_init_mask;
   [48] unsigned long qsmaskinit;
   [56] unsigned long qsmaskinitnext;
   [64] unsigned long expmask;
   [72] unsigned long expmaskinit;
   [80] unsigned long expmaskinitnext;
   [88] unsigned long ffmask;
   [96] unsigned long grpmask;
  [104] int grplo;
  [108] int grphi;
  [112] u8 grpnum;
  [113] u8 level;
  [114] bool wait_blkd_tasks;
  [120] struct rcu_node *parent;
  [128] struct list_head blkd_tasks;
  [144] struct list_head *gp_tasks;
  [152] struct list_head *exp_tasks;
  [160] struct list_head *boost_tasks;
  [168] struct rt_mutex boost_mtx;
  [200] unsigned long boost_time;
  [208] struct task_struct *boost_kthread_task;
  [216] unsigned int boost_kthread_status;
  [224] struct swait_queue_head nocb_gp_wq[2];
  [320] raw_spinlock_t fqslock;
  [384] spinlock_t exp_lock;
  [392] unsigned long exp_seq_rq;
  [400] wait_queue_head_t exp_wq[4];
  [496] struct rcu_exp_work rew;
  [584] bool exp_need_flush;
}
SIZE: 640
```

从`struct rcu_node`中可以看到`lock`成员是一个`raw_spinlock_t`类型的锁。且是第一个成员，意味着qspinlock地址即rcu_node地址。

调用栈回溯：

```c
// vim 127.0.0.1-2025-05-11-18:26:08/vmcore-dmesg.txt +1580

1583 [887136.212952] rcu: INFO: rcu_sched self-detected stall on CPU
1584 [887136.218705] rcu:    5-...!: (1 GPs behind) idle=e32/0/0x1 softirq=2547154/2547154 fqs=1
1585 [887136.226836] rcu:     (t=69795 jiffies g=60317529 q=32)
1586 [887136.231966] NMI backtrace for cpu 5
1587 [887136.235591] CPU: 5 PID: 0 Comm: swapper/5 Kdump: loaded Tainted: G           OE     4.19.90-2102.2.0.0062.ctl2.aarch64 #1
1588 [887136.246811] Hardware name: RCSIT TG225 B1/BC82AMDQ, BIOS 5.25 09/14/2022
1589 [887136.253702] Call trace:  # 调用栈跟踪
1590 [887136.256267]  dump_backtrace+0x0/0x198  # 打印当前CPU的回溯信息
1591 [887136.260066]  show_stack+0x24/0x30  # 显示内核堆栈
1592 [887136.263516]  dump_stack+0xa4/0xe8  # 打印完整堆栈信息
1593 [887136.266963]  nmi_cpu_backtrace+0xf4/0xf8  # NMI下打印CPU回溯
1594 [887136.271026]  nmi_trigger_cpumask_backtrace+0x170/0x1c0  # 触发指定CPU集合的NMI回溯
1595 [887136.276328]  arch_trigger_cpumask_backtrace+0x30/0xd8  # 架构相关的NMI回溯触发
1596 [887136.281543]  rcu_dump_cpu_stacks+0xf4/0x134  # RCU打印所有stall CPU的堆栈
1597 [887136.285872]  print_cpu_stall+0x16c/0x228  # 打印CPU卡死（stall）信息
1598 [887136.289937]  rcu_check_callbacks+0x590/0x7a8  # 检查RCU回调，检测stall
1599 [887136.297318]  update_process_times+0x34/0x60  # 更新进程时间（时钟中断处理）
1600 [887136.304678]  tick_sched_handle.isra.4+0x34/0x70  # 定时器调度处理
1601 [887136.312346]  tick_sched_timer+0x50/0xa0  # 定时器中断处理主函数
1602 [887136.319222]  __hrtimer_run_queues+0x114/0x368  # 高精度定时器队列处理
1603 [887136.326551]  hrtimer_interrupt+0xf8/0x2d0  # 高精度定时器中断入口
1604 [887136.333458]  arch_timer_handler_phys+0x38/0x58  # 架构相关物理定时器中断处理
1605 [887136.340774]  handle_percpu_devid_irq+0x90/0x248  # 处理per-cpu设备中断
1606 [887136.348161]  generic_handle_irq+0x3c/0x58  # 通用中断处理
1607 [887136.354969]  __handle_domain_irq+0x68/0xc0  # 处理中断域
1608 [887136.361812]  gic_handle_irq+0x6c/0x170  # ARM GIC中断控制器处理
1609 [887136.368233]  el1_irq+0xb8/0x140  # EL1异常级别的IRQ入口
1610 [887136.373961]  arch_cpu_idle+0x38/0x1d0  # 架构相关CPU空闲处理
1611 [887136.380163]  default_idle_call+0x24/0x58  # 默认CPU空闲调用
1612 [887136.386580]  do_idle+0x1d4/0x2b0  # CPU空闲主循环
1613 [887136.392193]  cpu_startup_entry+0x28/0x78  # CPU启动入口
1614 [887136.398475]  secondary_start_kernel+0x17c/0x1c8  # 次级CPU启动内核
```

```c
// vim kernel/rcu/tree.c +1336

1330 static void rcu_dump_cpu_stacks(struct rcu_state *rsp)
1331 {
1332     int cpu;
1333     unsigned long flags;
1334     struct rcu_node *rnp;
1335
1336     rcu_for_each_leaf_node(rsp, rnp) {
1337         raw_spin_lock_irqsave_rcu_node(rnp, flags);
1338         for_each_leaf_node_possible_cpu(rnp, cpu)
1339             if (rnp->qsmask & leaf_node_cpu_bit(rnp, cpu))
1340 +-----  2 lines: if (!trigger_single_cpu_backtrace(cpu))-----------------------------------------------------------------------------------
1342         raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
1343     }
1344 }
```

```c
// vim kernel/rcu/rcu.h +362

362 #define rcu_for_each_leaf_node(rsp, rnp) \
363     for ((rnp) = rcu_first_leaf_node(rsp); \
364     ¦   ¦(rnp) < &(rsp)->node[rcu_num_nodes]; (rnp)++)
```

```bash
// vim kernel/rcu/rcu.h +332

331 /* Returns first leaf rcu_node of the specified RCU flavor. */
332 #define rcu_first_leaf_node(rsp) ((rsp)->level[rcu_num_lvls - 1])  
```

```bash
crash> rcu_num_lvls
rcu_num_lvls = $83 = 2
```

从`#define rcu_first_leaf_node(rsp) ((rsp)->level[rcu_num_lvls - 1])`可以得到`rsp->level[1]`。

```bash
crash> struct rcu_state.level
struct rcu_state {
  [41600] struct rcu_node *level[3];
}
```

```bash
crash> p rcu_sched_state.level
$87 = {0xffff3eb87f209000, 0xffff3eb87f209280, 0x0}
```

```bash
crash> p rcu_sched_state.level[1]
$88 = (struct rcu_node *) 0xffff3eb87f209280
```

到这里44号cpu通过rcu_dump_cpu_stacks中的raw_spin_lock_irqsave_rcu_node函数持有的rcu_node->lock就很清晰了。

5号cpu 等待的就是44号cpu的锁。

```bash
crash> rcu_node ffff3eb87f209280
struct rcu_node {
  lock = {
    raw_lock = {
      {
        val = {
          counter = 6291713
        },
        {
          locked = 1 '\001',
          pending = 1 '\001'
        },
        {
          locked_pending = 257,
          tail = 96
        }
      }
    }
  },
  gp_seq = 65421,
  gp_seq_needed = 65428,
  completedqs = 65417,
  qsmask = 49141,   // qsmask 是 rcu_node 结构体中的一个掩码字段，表示本节点下哪些 CPU 还没有报告“静止状态（quiescent state）”。
  rcu_gp_init_mask = 0,
  qsmaskinit = 65535,
  qsmaskinitnext = 65535,
  expmask = 0,
  expmaskinit = 65535,
  expmaskinitnext = 65535,
  ffmask = 65535,
  grpmask = 1,
  grplo = 0,
  grphi = 15,
  grpnum = 0 '\000',
  level = 1 '\001',
  wait_blkd_tasks = false,
  parent = 0xffff3eb87f209000,
  blkd_tasks = {
    next = 0xffff3eb87f209300,
    prev = 0xffff3eb87f209300
  },
  gp_tasks = 0x0,
  exp_tasks = 0x0,
  boost_tasks = 0x0,
  boost_mtx = {
    wait_lock = {
      raw_lock = {
        {
          val = {
            counter = 0
          },
          {
            locked = 0 '\000',
            pending = 0 '\000'
          },
          {
            locked_pending = 0,
            tail = 0
          }
        }
      }
    },
    waiters = {
      rb_root = {
        rb_node = 0x0
      },
      rb_leftmost = 0x0
    },
    owner = 0x0
  },
  boost_time = 0,
  boost_kthread_task = 0x0,
  boost_kthread_status = 0,
  nocb_gp_wq = {{
      lock = {
        raw_lock = {
          {
            val = {
              counter = 0
            },
            {
              locked = 0 '\000',
              pending = 0 '\000'
            },
            {
              locked_pending = 0,
              tail = 0
            }
          }
        }
      },
      task_list = {
        next = 0xffff3eb87f209368,
        prev = 0xffff3eb87f209368
      }
    }, {
      lock = {
        raw_lock = {
          {
            val = {
              counter = 0
            },
            {
              locked = 0 '\000',
              pending = 0 '\000'
            },
            {
              locked_pending = 0,
              tail = 0
            }
          }
        }
      },
      task_list = {
        next = 0xffff3eb87f209380,
        prev = 0xffff3eb87f209380
      }
    }},
  fqslock = {
    raw_lock = {
      {
        val = {
          counter = 0
        },
        {
          locked = 0 '\000',
          pending = 0 '\000'
        },
        {
          locked_pending = 0,
          tail = 0
        }
      }
    }
  },
  exp_lock = {
    {
      rlock = {
        raw_lock = {
          {
            val = {
              counter = 0
            },
            {
              locked = 0 '\000',
              pending = 0 '\000'
            },
            {
              locked_pending = 0,
              tail = 0
            }
          }
        }
      }
    }
  },
  exp_seq_rq = 764,
  exp_wq = {{
      lock = {
        {
          rlock = {
            raw_lock = {
              {
                val = {
                  counter = 0
                },
                {
                  locked = 0 '\000',
                  pending = 0 '\000'
                },
                {
                  locked_pending = 0,
                  tail = 0
                }
              }
            }
          }
        }
      },
      head = {
        next = 0xffff3eb87f209418,
        prev = 0xffff3eb87f209418
      }
    }, {
      lock = {
        {
          rlock = {
            raw_lock = {
              {
                val = {
                  counter = 0
                },
                {
                  locked = 0 '\000',
                  pending = 0 '\000'
                },
                {
                  locked_pending = 0,
                  tail = 0
                }
              }
            }
          }
        }
      },
      head = {
        next = 0xffff3eb87f209430,
        prev = 0xffff3eb87f209430
      }
    }, {
      lock = {
        {
          rlock = {
            raw_lock = {
              {
                val = {
                  counter = 0
                },
                {
                  locked = 0 '\000',
                  pending = 0 '\000'
                },
                {
                  locked_pending = 0,
                  tail = 0
                }
              }
            }
          }
        }
      },
      head = {
        next = 0xffff3eb87f209448,
        prev = 0xffff3eb87f209448
      }
    }, {
      lock = {
        {
          rlock = {
            raw_lock = {
              {
                val = {
                  counter = 0
                },
                {
                  locked = 0 '\000',
                  pending = 0 '\000'
                },
                {
                  locked_pending = 0,
                  tail = 0
                }
              }
            }
          }
        }
      },
      head = {
        next = 0xffff3eb87f209460,
        prev = 0xffff3eb87f209460
      }
    }},
  rew = {
    rew_func = 0xffff3eb87e0a61c8,
    rew_rsp = 0xffff3eb87f209000,
    rew_s = 0,
    rew_work = {
      data = {
        counter = 0
      },
      entry = {
        next = 0xffff3eb87f209490,
        prev = 0xffff3eb87f209490
      },
      func = 0xffff3eb87e0aa180,
      kabi_reserved1 = 0,
      kabi_reserved2 = 0,
      kabi_reserved3 = 0,
      kabi_reserved4 = 0
    }
  },
  exp_need_flush = true
}
```

##### rcu_node.qsmask ffff3eb87f209280

```bash
crash> rcu_node.qsmask ffff3eb87f209280
  qsmask = 49141
crash> eval 49141
hexadecimal: bff5
    decimal: 49141
      octal: 137765
     binary: 0000000000000000000000000000000000000000000000001011111111110101
```

#### rcu_state

```bash
crash> rcu_node.parent ffff3eb87f209280
  parent = 0xffff3eb87f209000
```

```bash
crash> struct -o rcu_state
struct rcu_state {
      [0] struct rcu_node node[65];
  [41600] struct rcu_node *level[3];
  [41624] struct rcu_data *rda;
  [41632] call_rcu_func_t call;
  [41640] int ncpus;
  [41664] u8 boost;
  [41672] unsigned long gp_seq;
  [41680] struct task_struct *gp_kthread;
  [41688] struct swait_queue_head gp_wq;
  [41712] short gp_flags;
  [41714] short gp_state;
  [41720] struct mutex barrier_mutex;
  [41752] atomic_t barrier_cpu_count;
  [41760] struct completion barrier_completion;
  [41792] unsigned long barrier_sequence;
  [41800] struct mutex exp_mutex;
  [41832] struct mutex exp_wake_mutex;
  [41864] unsigned long expedited_sequence;
  [41872] atomic_t expedited_need_qs;
  [41880] struct swait_queue_head expedited_wq;
  [41904] int ncpus_snap;
  [41912] unsigned long jiffies_force_qs;
  [41920] unsigned long jiffies_kick_kthreads;
  [41928] unsigned long n_force_qs;
  [41936] unsigned long gp_start;
  [41944] unsigned long gp_activity;
  [41952] unsigned long gp_req_activity;
  [41960] unsigned long jiffies_stall;
  [41968] unsigned long jiffies_resched;
  [41976] unsigned long n_force_qs_gpstart;
  [41984] unsigned long gp_max;
  [41992] const char *name;
  [42000] char abbr;
  [42008] struct list_head flavors;
  [42048] spinlock_t ofl_lock;
}
SIZE: 42112
```

```bash
rcu_state 0xffff3eb87f209000 > rcu_state_0xffff3eb87f209000.log
```

```bash
# include/linux/swait.h
# 知道swait_queue_head地址，crash中怎么用list命令得到第62行的task地址

crash> p &(rcu_sched_state.gp_wq)
$96 = (struct swait_queue_head *) 0xffff3eb87f2132d8

crash> list -o swait_queue.task_list -s task_struct.pid,comm -O swait_queue_head.task_list -h 0xffff3eb87f2132d8
list: invalid option -- 'O'
Usage:
  list [[-o] offset][-e end][-[s|S] struct[.member[,member] [-l offset]] -[x|d]]
       [-r|-B] [-h|-H] start
Enter "help list" for details.
```

```bash
[root@obs-arm-worker-01 127.0.0.1-2025-05-11-23:11:38]# crash --version

crash 7.2.8-5.ctl2
```

crash版本太低，改用`-o`选项：
```bash
crash> p &(rcu_sched_state.gp_wq.task_list)
$98 = (struct list_head *) 0xffff3eb87f2132e0

crash> list -o swait_queue.task -s task_struct.pid,comm -H 0xffff3eb87f2132e0
(empty)
```

可以看到rcu_sched_state.gp_wq.task_list是空的，说明rcu_sched_state.gp_wq中没有等待的任务。

```bash
crash> task_struct.on_cpu,cpu,recent_used_cpu,wake_cpu ffffbf6546fae880
  on_cpu = 1
  cpu = 60
  recent_used_cpu = 44
  wake_cpu = 60
crash> bt -c 60
PID: 11     TASK: ffffbf6546fae880  CPU: 60  COMMAND: "rcu_sched"
bt: WARNING: cannot determine starting stack frame for task ffffbf6546fae880
crash>
crash> task_struct.on_cpu,cpu,recent_used_cpu,wake_cpu ffffbf6546fae880
  on_cpu = 1
  cpu = 60
  recent_used_cpu = 44
  wake_cpu = 60
```

rcu_sched最近一次使用44号cpu，当前运行在60号cpu上。

#### rcu_node

从rcu_state中可以得到所有的rcu_node信息：
```bash
rcu_node[0]：grplo=0, grphi=15, qsmask=49141
rcu_node[1]：grplo=16, grphi=31, qsmask=32384
rcu_node[2]：grplo=32, grphi=47, qsmask=64435
rcu_node[3]：grplo=48, grphi=63, qsmask=32636
```

```bash
crash> eval 49141
hexadecimal: bff5
    decimal: 49141
      octal: 137765
     binary: 0000000000000000000000000000000000000000000000001011111111110101
crash>
crash> eval 32384
hexadecimal: 7e80
    decimal: 32384
      octal: 77200
     binary: 0000000000000000000000000000000000000000000000000111111010000000
crash>
crash> eval 64435
hexadecimal: fbb3
    decimal: 64435
      octal: 175663
     binary: 0000000000000000000000000000000000000000000000001111101110110011
crash>
crash> eval 32636
hexadecimal: 7f7c
    decimal: 32636
      octal: 77574
     binary: 0000000000000000000000000000000000000000000000000111111101111100
crash>
```

根据crash输出的二进制结果**重新给出准确的四张表**：

---

##### rcu_node[0]：grplo=0, grphi=15, qsmask=49141

49141 = 0b1011111111110101

| bit编号 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---------|----|----|----|----|----|----|---|---|---|---|---|---|---|---|---|---|
| bit值   | 1  | 0  | 1  | 1  | 1  | 1  | 1 | 1 | 1 | 1 | 1 | 1 | 0 | 1 | 0 | 1 |
| CPU号   |15  |14  |13  |12  |11  |10  |9  |8  |7  |6  |5  |4  |3  |2  |1  |0  |

**未QS的CPU号：0,2,4,5,6,7,8,9,10,11,12,13,15**

---

##### rcu_node[1]：grplo=16, grphi=31, qsmask=32384

32384 = 0b0111111010000000

| bit编号 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---------|----|----|----|----|----|----|---|---|---|---|---|---|---|---|---|---|
| bit值   | 0  | 1  | 1  | 1  | 1  | 1  | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| CPU号   |31  |30  |29  |28  |27  |26  |25 |24 |23 |22 |21 |20 |19 |18 |17 |16 |

**未QS的CPU号：23,25,26,27,28,29,30**

---

##### rcu_node[2]：grplo=32, grphi=47, qsmask=64435

64435 = 0b1111101110110011

| bit编号 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---------|----|----|----|----|----|----|---|---|---|---|---|---|---|---|---|---|
| bit值   | 1  | 1  | 1  | 1  | 1  | 0  | 1 | 1 | 1 | 0 | 1 | 1 | 0 | 0 | 1 | 1 |
| CPU号   |47  |46  |45  |44  |43  |42  |41 |40 |39 |38 |37 |36 |35 |34 |33 |32 |

**未QS的CPU号：32,33,35,36,38,39,40,41,43,44,45,46,47**

---

##### rcu_node[3]：grplo=48, grphi=63, qsmask=32636

32636 = 0b0111111101111100

| bit编号 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---------|----|----|----|----|----|----|---|---|---|---|---|---|---|---|---|---|
| bit值   | 0  | 1  | 1  | 1  | 1  | 1  | 1 | 1 | 1 | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| CPU号   |63  |62  |61  |60  |59  |58  |57 |56 |55 |54 |53 |52 |51 |50 |49 |48 |

**未QS的CPU号：50,51,53,54,55,56,57,58,59,60,61,62**

---

#### rcu_data

##### rcu_state.rda

```c
// vim kernel/locking/rcu.h +317

312 struct rcu_state {
313     struct rcu_node node[NUM_RCU_NODES];    /* Hierarchy. */
314     struct rcu_node *level[RCU_NUM_LVLS + 1];
315 +-----  2 lines: Hierarchy levels (+1 to -------------------------------------------------------------------
317     struct rcu_data __percpu *rda;      /* pointer of percu rcu_data. */
```

```bash
crash> rcu_state.rda ffff3eb87f209000
  rda = 0xffff3eb87eebe280
```

```bash
crash> sym 0xffff3eb87eebe280
ffff3eb87eebe280 (d) rcu_sched_data
```

d 表示 rcu_sched_data 是一个全局变量（数据段符号），不是函数。

rcu_sched_data 是一个 per-CPU 变量:

```bash
crash> rcu_sched_data > rcu_sched_data.log

PER-CPU DATA TYPE:
  struct rcu_data rcu_sched_data;
PER-CPU ADDRESSES:
  [0]: ffffbf657f9ee280
  [1]: ffffbf657fa1e280
  [2]: ffffbf657fa4e280
  [3]: ffffbf657fa7e280
  [4]: ffffbf657faae280
  [5]: ffffbf657fade280
  [6]: ffffbf657fb0e280
  [7]: ffffbf657fb3e280
  [8]: ffffbf657fb6e280
  [9]: ffffbf657fb9e280
  [10]: ffffbf657fbce280
  [11]: ffffbf657fbfe280
  [12]: ffffbf657fc2e280
  [13]: ffffbf657fc5e280
  [14]: ffffbf657fc8e280
  [15]: ffffbf657fcbe280
  [16]: ffffbf657fcee280
  [17]: ffffbf657fd1e280
  [18]: ffffbf657fd4e280
  [19]: ffffbf657fd7e280
  [20]: ffffbf657fdae280
  [21]: ffffbf657fdde280
  [22]: ffffbf657fe0e280
  [23]: ffffbf657fe3e280
  [24]: ffffbf657fe6e280
  [25]: ffffbf657fe9e280
  [26]: ffffbf657fece280
  [27]: ffffbf657fefe280
  [28]: ffffbf657ff2e280
  [29]: ffffbf657ff5e280
  [30]: ffffbf657ff8e280
  [31]: ffffbf657ffbe280
  [32]: ffffdf653f92e280
  [33]: ffffdf653f95e280
  [34]: ffffdf653f98e280
  [35]: ffffdf653f9be280
  [36]: ffffdf653f9ee280
  [37]: ffffdf653fa1e280
  [38]: ffffdf653fa4e280
  [39]: ffffdf653fa7e280
  [40]: ffffdf653faae280
  [41]: ffffdf653fade280
  [42]: ffffdf653fb0e280
  [43]: ffffdf653fb3e280
  [44]: ffffdf653fb6e280
  [45]: ffffdf653fb9e280
  [46]: ffffdf653fbce280
  [47]: ffffdf653fbfe280
  [48]: ffffdf653fc2e280
  [49]: ffffdf653fc5e280
  [50]: ffffdf653fc8e280
  [51]: ffffdf653fcbe280
  [52]: ffffdf653fcee280
  [53]: ffffdf653fd1e280
  [54]: ffffdf653fd4e280
  [55]: ffffdf653fd7e280
  [56]: ffffdf653fdae280
  [57]: ffffdf653fdde280
  [58]: ffffdf653fe0e280
  [59]: ffffdf653fe3e280
  [60]: ffffdf653fe6e280
  [61]: ffffdf653fe9e280
  [62]: ffffdf653fece280
  [63]: ffffdf653fefe280
```

##### cpu 44 rcu_data

44号cpu 没有msc 锁的父节点，msc.locked = 1, 表明44号cpu是持有rcu_node锁的。

查看per_cpu变量有两种方法。

###### 方法一

```bash
crash> kmem -o > kmem_o.log
```

```bash
crash> kmem -o | grep 'CPU 44'
 CPU 44: a0acc0cb0000
```

```bash
crash> eval 0xa0acc0cb0000 + 0xffff3eb87eebe280
hexadecimal: ffffdf653fb6e280
    decimal: 18446708224686482048  (-35849023069568)
      octal: 1777776766247755561200
     binary: 1111111111111111110111110110010100111111101101101110001010000000
```

```bash
crash> rcu_data ffffdf653fb6e280 > rcu_data_ffffdf653fb6e280_cpu_44.log

struct rcu_data {
  gp_seq = 65421,
  gp_seq_needed = 65420,
  rcu_qs_ctr_snap = 0,
  cpu_no_qs = {
    b = {
      norm = 0 '\000',
      exp = 0 '\000'
    },
    s = 0
  },
  core_needs_qs = true,
  beenonline = true,
  gpwrap = false,
  mynode = 0xffff3eb87f209780,
  grpmask = 4096,
  ticks_this_gp = 1,
  cblist = {
    head = 0x0,
    tails = {0xffffdf653fb6e2b8, 0xffffdf653fb6e2b8, 0xffffdf653fb6e2b8, 0xffffdf653fb6e2b8},
...
```

###### 方法二

```bash
crash> p rcu_sched_data:44 > p_rcu_sched_data_44.log
```

两种方法都可以查看到44号CPU的`rcu_data`结构体数据，内容是一样的。

```bash
crash> p rcu_data:44 > p_rcu_data_44.log
```

core_needs_qs = true ← 需要静默点（QS），说明本CPU还没上报QS。

gp_seq 表示当前全局RCU宽限期（Grace Period）的序列号，即系统推进到的最新RCU同步序号。

gp_seq_needed 表示本CPU上次需要参与的RCU宽限期序列号。

当 gp_seq > gp_seq_needed 时，说明全局RCU宽限期已经推进到新的阶段，而本CPU上次需要参与的宽限期已经被推进过去。

也就是说，本CPU已经不再需要为旧的gp_seq_needed参与QS（静默点）上报，但如果本CPU还未上报QS，可能会影响新的宽限期推进。

本CPU（如44号）落后于全局RCU进度，还没有完成上一次宽限期的静默点上报，导致RCU同步推进受阻。
这也是RCU卡住、死锁的常见根因之一。

##### cpu 5 rcu_data

###### 方法一

```bash
crash> kmem -o | grep -w 'CPU 5'
  CPU 5: 80ad00c20000
```

```bash
crash> eval 0x80ad00c20000 + 0xffff3eb87eebe280
hexadecimal: ffffbf657fade280
    decimal: 18446673041387545216  (-71032322006400)
      octal: 1777775766257753361200
     binary: 1111111111111111101111110110010101111111101011011110001010000000
```

```bash
crash> rcu_data ffffbf657fade280 > rcu_data_fffbf657fade280_cpu_5.log
```

###### 方法二

```bash
crash> p rcu_sched_data:5 > p_rcu_sched_data_5.log
```

前文查看了cpu 44和cpu 5的`rcu_data`，太慢了，将所有cpu上的rcu_data导出来看看。

```bash
crash> p rcu_sched_data:0-63 > p_rcu_sched_data_0-63.log
```

##### 查找core_needs_qs = true

```bash
#!/bin/bash
# filepath: find_core_needs_qs_true.sh

awk '
BEGIN { RS="per_cpu\\(rcu_sched_data,"; ORS=""; }
/core_needs_qs = true/ && NR>1 {
    print "per_cpu(rcu_sched_data," $0 "\n----------------------------------------\n"
}
' p_rcu_sched_data_0-63.log | tee core_needs_qs_true.log
```

```bash
grep 'per_cpu(rcu_sched_data' core_needs_qs_true.log | awk -F '[, )]+' '{print $2}' | paste -sd,

0,2,5,6,7,9,11,12,13,15,23,25,26,28,30,32,33,36,37,40,41,44,45,46,47,50,51,53,54,56,58,60,61
```

##### 查看core_needs_qs = true的CPU栈

```bash
crash> bt -c 0,2,5,6,7,9,11,12,13,15,23,25,26,28,30,32,33,36,37,40,41,44,45,46,47,50,51,53,54,56,58,60,61 > bt_c_rcu_sched_data_core_needs_qs_true.log
```

##### 23号cpu是否关中断？
```bash
crash> task_struct.thread_info ffffbf655d94d900
  thread_info = {
    flags = 552,
    addr_limit = 281474976710655,
    preempt_count = 1114112
  }
crash> eval 552
hexadecimal: 228
    decimal: 552
      octal: 1050
     binary: 0000000000000000000000000000000000000000000000000000001000101000
crash> eval 1114112
hexadecimal: 110000  (1088KB)
    decimal: 1114112
      octal: 4200000
     binary: 0000000000000000000000000000000000000000000100010000000000000000
crash> bt -c 23
PID: 6214   TASK: ffffbf655d94d900  CPU: 23  COMMAND: "kworker/23:0"
bt: WARNING: cannot determine starting stack frame for task ffffbf655d94d900
```

```bash
list callback_head.next -s callback_head  0xffffbf6521b07e68 > cblist_head_cpu_5.log
```

##### rcu_callback_head.next不为0的情况

```bash
#!/bin/bash

# find_rcu_sched_data_len_not_zero.sh

awk '
BEGIN { RS="per_cpu\\(rcu_sched_data,"; ORS=""; }
/len *= *[1-9][0-9]*/ && NR>1 {
    print "per_cpu(rcu_sched_data," $0 "\n----------------------------------------\n"
}
' p_rcu_sched_data_0-63.log > rcu_sched_data_len_not_zero.log
```

```bash
crash> list callback_head.next -s callback_head.func 0xffffbf6556214c00 | grep func | awk '{print $3}' | sort | uniq
0x28
0xffff3eb87e1dd6a0
0xffff3eb87e27c128
0xffff3eb87e2986d8
crash> sym 0xffff3eb87e1dd6a0
ffff3eb87e1dd6a0 (t) shmem_destroy_callback /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/mm/shmem.c: 3635
crash> sym 0xffff3eb87e27c128
ffff3eb87e27c128 (t) file_free_rcu /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/file_table.c: 45
crash> sym 0xffff3eb87e2986d8
ffff3eb87e2986d8 (t) __d_free /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/dcache.c: 254
```

```bash
crash> list callback_head.next -s callback_head.func 0xffffbf657fa4e2b8 | grep func | awk '{print $3}' | sort | uniq

0xffff3eb87e2986d8
0xffffbf657fa4e2b8
crash>
crash> sym 0xffff3eb87e2986d8
ffff3eb87e2986d8 (t) __d_free /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/dcache.c: 254
crash> sym 0xffffbf657fa4e2b8
sym: invalid address: 0xffffbf657fa4e2b8
```

```bash
crash> list callback_head.next -s callback_head.func 0xffffbf657faae2b8 | grep func | awk '{print $3}' | sort | uniq
0xffff3eb87e014d18
0xffff3eb87e038970
0xffff3eb87e2986d8
0xffff3eb87e30fdc0
0xffffbf657faae2b8
crash> sym 0xffff3eb87e014d18
ffff3eb87e014d18 (t) delayed_put_task_struct /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/exit.c: 176
crash> sym 0xffff3eb87e038970
ffff3eb87e038970 (t) delayed_put_pid /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/kernel/pid.c: 118
crash> sym 0xffff3eb87e2986d8
ffff3eb87e2986d8 (t) __d_free /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/dcache.c: 254
crash> sym 0xffffbf657faae2b8
sym: invalid address: 0xffffbf657faae2b8
```

cpu 5:
```bash
crash> list callback_head.next -s callback_head.func 0xffffbf657fade2b8 | grep func | awk '{print $3}' | sort | uniq
0xffff3eb87e2986d8
0xffff3eb87e2db538
0xffffbf657fade2b8
crash> sym 0xffff3eb87e2986d8
ffff3eb87e2986d8 (t) __d_free /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/dcache.c: 254
crash> sym 0xffff3eb87e2db538
ffff3eb87e2db538 (t) epi_rcu_free /usr/src/debug/kernel-4.19.90-2102.2.0.0062.ctl2.aarch64/linux-4.19.90-2102.2.0.0062.ctl2.aarch64/fs/eventpoll.c: 763
crash> sym 0xffffbf657fade2b8
sym: invalid address: 0xffffbf657fade2b8
```

#### per_cpu(qnodes)

qspinlock的qnodes是一个per-CPU变量。

```c
// vim kernel/locking/qspinlock.c +119
119 static DEFINE_PER_CPU_ALIGNED(struct qnode, qnodes[MAX_NODES]);
```

```bash
crash> qnodes > qnodes.log
```

```bash
crash> p qnodes:0-63 > p_qnodes.log
```

##### find_unused_qnodes.sh

```bash
#!/bin/bash
# 文件名：find_unused_qnodes.sh

# 先清理旧结果
rm -f used_addr.txt unused_qnodes.txt

# 1. 提取 p_qndoes.log 中所有出现过的地址
grep -oE 'ffff[0-9a-f]{12}' p_qndoes.log > used_addr.txt

# 2. 遍历 qnodes.log 里的地址和CPU编号，输出未被指向的
grep -oP '\[\d+\]: ffff[0-9a-f]+' qnodes.log | while read line; do
    cpu=$(echo "$line" | grep -oP '\[\d+\]' | tr -d '[]')
    addr=$(echo "$line" | grep -oP 'ffff[0-9a-f]+')
    if ! grep -q "$addr" used_addr.txt; then
        echo "CPU $cpu: $addr" >> unused_qnodes.txt
    fi
done

# 3. 打印结果
cat unused_qnodes.txt
```

没有父节点的qnode cpu编号:
```bash
# cat unused_qnodes.txt

CPU 3: ffffbf657fa7e240
CPU 4: ffffbf657faae240
CPU 5: ffffbf657fade240
CPU 22: ffffbf657fe0e240
CPU 27: ffffbf657fefe240
CPU 31: ffffbf657ffbe240
CPU 37: ffffdf653fa1e240
CPU 44: ffffdf653fb6e240
CPU 48: ffffdf653fc2e240
CPU 60: ffffdf653fe6e240
```

根据cpu msc锁看到没有父节点的qnode cpu编号大致分类两类，bt -c <cpuid> 栈正常的如下，正常的栈内容基本一样：
- `bt -c 3`
- `bt -c 4`
- `bt -c 22`
- `bt -c 27`
- `bt -c 31`
- `bt -c 37`
- `bt -c 48`

异常的栈如下：

- `bt -c 5`
- `bt -c 44`
- `bt -c 60`

```bash
crash> bt -c 5
PID: 0      TASK: ffffbf6546fa2e80  CPU: 5   COMMAND: "swapper/5"
bt: WARNING: cannot determine starting stack frame for task ffffbf6546fa2e80
crash>
crash> bt -c 44
PID: 0      TASK: ffffdf64e643f000  CPU: 44  COMMAND: "swapper/44"
bt: WARNING: cannot determine starting stack frame for task ffffdf64e643f000
crash>
crash> bt -c 60
PID: 11     TASK: ffffbf6546fae880  CPU: 60  COMMAND: "rcu_sched"
bt: WARNING: cannot determine starting stack frame for task ffffbf6546fae880
```

##### find_duplicate_next_cpu_fixed.sh

```bash
#!/bin/bash
# 文件名：find_duplicate_next_cpu_fixed.sh

# 1. 找出被多个节点指向的 next 地址（去掉0x前缀）
grep -oP 'next = 0x[0-9a-f]+' p_qndoes.log | grep -v 'next = 0x0' | awk '{print $3}' | sed 's/^0x//' | sort | uniq -c | awk '$1 > 1 {print $2}' > dup_next.txt

# 2. 建立地址到CPU编号的映射表
awk '/\[[0-9]+\]: ffff/ {
    match($0, /\[([0-9]+)\]: (ffff[0-9a-f]+)/, arr);
    if (arr[1] && arr[2]) print arr[2] " " arr[1];
}' qnodes.log > addr2cpu.txt

# 3. 查找每个重复next对应的CPU编号
while read addr; do
    cpu=$(grep -i "^$addr " addr2cpu.txt | awk '{print $2}')
    if [ -n "$cpu" ]; then
        echo "$addr 对应的CPU编号: $cpu" >> dup_next_cpu.txt
    else
        echo "$addr 未找到对应CPU"
    fi
done < dup_next.txt
```

多个qnode指向同一个next的cpu：
```bash
# cat dup_next_cpu.txt

ffffbf657fe3e240 对应的CPU编号: 23
ffffbf657ff5e240 对应的CPU编号: 29
ffffdf653fa7e240 对应的CPU编号: 39
ffffdf653fb3e240 对应的CPU编号: 43
ffffdf653fece240 对应的CPU编号: 62
```

观察一下cpu mcs:

cpu mcs：5 2 9 13 11 6 7 12 23 null

cpu mcs：48 21 23 null

cpu mcs: 44 62

cpu mcs: 60 39 32 40 51 33 36 41 24 52 0 53 54 55 42 59 56 57 62 47 26 46 45 null

cpu mcs: 20 35 30 10 50 61 39

```bash
crash> irq -s -c 44 | awk '$2 != 0'
          CPU44
  4:       2183    GICv3 arch_timer
147:         81  ITS-MSI megasas0-msix12
164:         18  ITS-MSI megasas0-msix29
166:          3  ITS-MSI megasas0-msix31
325:          3  ITS-MSI ens1-43
390:          1  ITS-MSI eth1-43
412:        448  ITS-MSI ens0-1
413:        585  ITS-MSI ens0-2
422:          8  ITS-MSI ens0-11
486:          1  ITS-MSI eth3-11
```

```bash
crash> irq -s -c 5 | awk '$2 != 0'
           CPU5
  4:       8155    GICv3 arch_timer
148:        250  ITS-MSI megasas0-msix13
286:          3  ITS-MSI ens1-4
351:          1  ITS-MSI eth1-4
447:          3  ITS-MSI ens0-36
511:          1  ITS-MSI eth3-36
```

```bash
crash> irq -s -c 9 | awk '$2 != 0'
           CPU9
  4:       7024    GICv3 arch_timer
152:        141  ITS-MSI megasas0-msix17
290:          3  ITS-MSI ens1-8
355:          1  ITS-MSI eth1-8
451:         10  ITS-MSI ens0-40
515:          1  ITS-MSI eth3-40
```

```bash
crash> irq -s -c 13 | awk '$2 != 0'
          CPU13
  4:      12107    GICv3 arch_timer
156:         24  ITS-MSI megasas0-msix21
230:         58  ITS-MSI megasas5-msix21
294:          8  ITS-MSI ens1-12
359:          1  ITS-MSI eth1-12
455:          5  ITS-MSI ens0-44
519:          1  ITS-MSI eth3-44
```

```bash
crash> irq -s -c 11 | awk '$2 != 0'
          CPU11
  4:       8495    GICv3 arch_timer
154:         71  ITS-MSI megasas0-msix19
228:          2  ITS-MSI megasas5-msix19
292:          8  ITS-MSI ens1-10
357:          1  ITS-MSI eth1-10
409:         20  ITS-MSI mlx5_async63@pci:0000:01:00.1
453:         18  ITS-MSI ens0-42
517:          1  ITS-MSI eth3-42
```

```bash
crash> irq -s -c 2 | awk '$2 != 0'
           CPU2
  4:       6420    GICv3 arch_timer
145:         75  ITS-MSI megasas0-msix10
219:        102  ITS-MSI megasas5-msix10
283:          3  ITS-MSI ens1-1
348:          1  ITS-MSI eth1-1
444:          3  ITS-MSI ens0-33
508:          1  ITS-MSI eth3-33
733:          1  ITS-MSI hclge-misc-0000:7d:00.2
```

```bash
crash> irq -s -c 0 | awk '$2 != 0'
           CPU0
  4:      13484    GICv3 arch_timer
 15:          7    GICv3 ttyS0
 75:         33    GICv3 ehci_hcd:usb1,ehci_hcd:usb2,ohci_hcd:usb3,ohci_hcd:usb4
 76:        921  ITS-MSI hibmc
 77:         76  ITS-MSI xhci_hcd
143:         41  ITS-MSI megasas0-msix8
281:          4  ITS-MSI ens1--1
344:       5084  ITS-MSI mlx5_async63@pci:0000:01:00.0
346:          1  ITS-MSI eth1--1
409:       2908  ITS-MSI mlx5_async63@pci:0000:01:00.1
442:          3  ITS-MSI ens0-31
473:       5100  ITS-MSI mlx5_async63@pci:0000:82:00.0
506:          1  ITS-MSI eth3-31
537:       2908  ITS-MSI mlx5_async63@pci:0000:82:00.1
538:          1  ITS-MSI hclge-misc-0000:7d:00.0
```

## 问题结果

疑点：时钟中断上报有问题或某段代码中长时间关中断，导致rcu_sched无法被调度，rcu回调函数无法被执行。

同一批鲲鹏920机器，都是装的4.19 62内核，只有一台机器开机后放着不动出现了这个问题。

硬件侧排查时钟中断未上报问题。

内核测观察是否有长时间关中断的代码，当前并没有观察到有长时间关中断行为。

idle状态下cpu陷入深度睡眠，htimer无法上报中断？

更换主板后，问题不再修复。
