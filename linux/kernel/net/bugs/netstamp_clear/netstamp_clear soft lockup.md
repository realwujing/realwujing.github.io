# netstamp_clear soft lockup

## 问题描述

1. 故障时间：2025-06-28-20:15:54
2. 故障节点：内蒙08多AZ测试 dpu host主机 10.8.94.98
3. 故障现象：watchdog: BUG: soft lockup - CPU#4 stuck for 22s! [kworker/4:1:3759981]
4. 操作系统：5.10.0-136.12.0.90.ctl3.x86_64
5. cpu: Hygon C86-4G (OPN:7493)

## 问题分析

16张网卡反复打iperf tcp和udp的过程中，内核出现watchdog: BUG: soft lockup - CPU#114 stuck for 23s! [kworker/114:1:4093782]的报错，有call trace。

这次是打的iperf tcp和udp，打起来1min，kill 掉，在重新打，反复执行。

```bash
# vim /var/log/messages-20250629 +44817

44817 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.013351] watchdog: BUG: soft lockup - CPU#4 stuck for 22s! [kworker/4:1:3759981]
44818 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.024045] Modules linked in: vfio_pci vfio_virqfd vfio_iommu_type1 vfio 8021q garp mrp stp llc bonding rfkill sunrpc vfat fat ext4 mbcache jbd2 amd64_edac_mod edac_mce_amd ast      drm_vram_helper drm_ttm_helper i2c_algo_bit kvm_amd ttm ccp drm_kms_helper kvm syscopyarea sysfillrect irqbypass ipmi_ssif sysimgblt sg pcspkr joydev rapl fb_sys_fops ipmi_devintf ngbe cec k10temp i2c_piix4 ipmi_msghandler      i2c_designware_platform i2c_designware_core acpi_cpufreq fuse drm xfs libcrc32c dm_multipath sd_mod t10_pi crct10dif_pclmul crc32_pclmul crc32c_intel ahci ghash_clmulni_intel libahci libata virtio_blk virtio_net net_failove      r failover dm_mirror dm_region_hash dm_log dm_mod
44819 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.097328] CPU: 4 PID: 3759981 Comm: kworker/4:1 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
44820 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.113132] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.18 06/02/2025
44821 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.123287] Workqueue: events netstamp_clear
44822 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.130137] RIP: 0010:text_poke_bp_batch+0xed/0x290
44823 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.137652] Code: 01 00 00 85 c9 74 14 0f b6 86 00 00 00 a1 41 88 04 24 f6 c1 02 0f 85 8d 01 00 00 49 8d 75 01 4c 89 c7 83 c5 01 e8 b3 fa ff ff <49> 63 7d f6 41 0f b6 d6 4c 89 e9       48 8d 74 24 1b 49 89 d0 49 83 c5
44824 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.161840] RSP: 0018:ffffab073cd0fe10 EFLAGS: 00000286
44825 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.169729] RAX: ffffffffa18c388e RBX: ffffffffa34ec7fa RCX: 0000000000000000
44826 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.179777] RDX: 0000000000000000 RSI: 0000000000000004 RDI: ffffd54642400ae8
44827 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.189799] RBP: 0000000000000001 R08: ffffffffa34ec7ab R09: 00000000000000e0
44828 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.199824] R10: 0000000000000003 R11: 0000000000000003 R12: ffffab073cd0fe2c
44829 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.209761] R13: ffffffffa34ec7aa R14: 0000000000000005 R15: ffffffffa34ec7aa
44830 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.219679] FS:  0000000000000000(0000) GS:ffff905a86000000(0000) knlGS:0000000000000000
44831 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.230724] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
44832 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.239098] CR2: 00007f9f7b74c000 CR3: 000000cc7200a000 CR4: 0000000000350ee0
44833 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.249067] Call Trace:
44834 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.253883]  text_poke_finish+0x1b/0x30
44835 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.260084]  arch_jump_label_transform_apply+0x16/0x30
44836 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.267771]  static_key_enable_cpuslocked+0x5f/0x90
44837 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.275163]  static_key_enable+0x16/0x20
44838 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.281498]  process_one_work+0x1b0/0x350
44839 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.287965]  worker_thread+0x49/0x310
44840 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.293972]  ? rescuer_thread+0x370/0x370
44841 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.300380]  kthread+0xfe/0x140
44842 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.305718]  ? kthread_park+0x90/0x90
44843 Jun 28 20:15:54 gaoji-10-8-94-99 kernel: [176044.311741]  ret_from_fork+0x22/0x30
```

内核通过 events 工作队列调度执行了 netstamp_clear 任务，但该任务在执行过程中卡住（如等待锁或 CPU 响应），导致看门狗触发 soft lockup。

netstamp_clear 负责清理网络时间戳（netstamp）相关的状态（如禁用静态分支、重置计数器等）。

### 代码分析

#### RIP: 0010:text_poke_bp_batch+0xed/0x290

```bash
crash> dis -sx text_poke_bp_batch+0xed
FILE: arch/x86/kernel/alternative.c
LINE: 1544

  1539                   *use the timestamp as the point at which to modify the
  1540* executable code.
  1541                   *The old instruction is recorded so that the event can be
  1542* processed forwards or backwards.
  1543                   */

* 1544                  perf_event_text_poke(text_poke_addr(&tp[i]), old, len,
  1545                                       tp[i].text, len);
  1546          }
  1547
  1548          if (do_sync) {
  1549                  /*
  1550* According to Intel, this core syncing is very likely
  1551                   *not necessary and we'd be safe even without it. But
  1552* better safe than sorry (plus there's not only Intel).
  1553                   */
  1554                  text_poke_sync();
  1555          }
  1556
  1557          /*
  1558           *Third step: replace the first byte (int3) by the first byte of
  1559* replacing opcode.
  1560           */
```

##### 注释

```c
1520         /*
1521         ¦* Emit a perf event to record the text poke, primarily to
1522         ¦* support Intel PT decoding which must walk the executable code
1523         ¦* to reconstruct the trace. The flow up to here is:
1524         ¦*   - write INT3 byte
1525         ¦*   - IPI-SYNC
1526         ¦*   - write instruction tail
1527         ¦* At this point the actual control flow will be through the
1528         ¦* INT3 and handler and not hit the old or new instruction.
1529         ¦* Intel PT outputs FUP/TIP packets for the INT3, so the flow
1530         ¦* can still be decoded. Subsequently:
1531         ¦*   - emit RECORD_TEXT_POKE with the new instruction
1532         ¦*   - IPI-SYNC
1533         ¦*   - write first byte
1534         ¦*   - IPI-SYNC
1535         ¦* So before the text poke event timestamp, the decoder will see
1536         ¦* either the old instruction flow or FUP/TIP of INT3. After the
1537         ¦* text poke event timestamp, the decoder will see either the
1538         ¦* new instruction flow or FUP/TIP of INT3. Thus decoders can
1539         ¦* use the timestamp as the point at which to modify the
1540         ¦* executable code.
1541         ¦* The old instruction is recorded so that the event can be
1542         ¦* processed forwards or backwards.
1543         ¦*/
1544         perf_event_text_poke(text_poke_addr(&tp[i]), old, len,
```

```c
/*
 * 发射（emit）一个性能事件（perf event）以记录代码修改（text poke），主要用于支持 Intel PT（Processor Trace）的解码。
 * Intel PT 必须遍历可执行代码来重建执行流，其流程如下：
 * 
 * 1. 写入 INT3 断点指令（单字节 0xCC）
 * 2. 发送处理器间中断（IPI-SYNC）同步所有 CPU
 * 3. 写入新指令的剩余部分（除首字节外的部分）
 * 
 * 此时，实际控制流会通过 INT3 断点及其处理程序执行，而不会执行旧指令或新指令。
 * Intel PT 会为 INT3 断点生成 FUP（Flow Update Packet）/TIP（Target IP Packet）数据包，因此执行流仍可被解码。
 * 
 * 后续步骤：
 * 1. 发射 RECORD_TEXT_POKE 性能事件（包含新指令内容）
 * 2. 再次发送 IPI-SYNC 同步
 * 3. 写入新指令的首字节
 * 4. 最后一次 IPI-SYNC 同步
 * 
 * 在代码修改事件（text poke）的时间戳之前，解码器会看到：
 *   - 旧指令的执行流，或
 *   - INT3 断点的 FUP/TIP 数据包。
 * 
 * 在时间戳之后，解码器会看到：
 *   - 新指令的执行流，或
 *   - INT3 断点的 FUP/TIP 数据包。
 * 
 * 因此，解码器可以利用该时间戳作为修改可执行代码的临界点。
 * 记录旧指令是为了支持事件的正向或反向解析（如时间回溯调试）。
 */
```

---

###### **关键点解析**

1. **Intel PT 支持**  
   - **目的**：确保动态代码修改（如 `static_key`、`ftrace`）能被 Intel Processor Trace 正确解码。
   - **挑战**：直接修改运行时代码会导致 PT 解码流不一致，需通过 `INT3` 断点和事件标记同步。

2. **安全修改代码的流程**  
   - **INT3 断点**：先写入断点指令，强制控制流转入处理程序，避免执行不完整的指令。
   - **IPI-SYNC**：通过处理器间中断同步所有 CPU，确保内存可见性和指令一致性。
   - **分阶段写入**：先改尾部字节，再通过事件记录时间戳，最后改首字节。

3. **时间戳的作用**  
   - 为解码器提供明确的代码切换边界，使其能正确处理修改前后的执行流。

4. **旧指令记录的意义**  
   - 支持双向分析（如 `perf report --time` 中的时间回溯），避免解码歧义。

---

###### **关联上下文**

在 `soft lockup` 日志中，`text_poke_bp_batch` 正是实现这一流程的函数。若某 CPU 未及时响应 `IPI-SYNC`（如因中断禁用或高负载），可能导致同步超时，触发看门狗告警。

当前dpu host主机的iperf流量最终会走到 dpu pf 节点上去，dpu 后端fpga中断处理逻辑可能会导致中断响应延迟，从而引发 `soft lockup`。

在 `text_poke` 场景中：

1. **IPI-SYNC 的不可打断性**：  
   - `text_poke` 发送的 `IPI-SYNC` 要求目标 CPU 立即暂停并响应，**通常不可被中断**。
   - 若目标 CPU 因死锁、长时间中断禁用或硬件故障未响应，会触发 `soft lockup`。

2. **潜在问题**：  
   - **硬件延迟**：CPU 内存总线或缓存同步耗时过长。
   - **中断禁用**：目标 CPU 执行了 `local_irq_disable()` 且未及时恢复。
   - **优先级反转**：低优先级线程持有锁，阻塞了 IPI 处理。

## 解决方案

