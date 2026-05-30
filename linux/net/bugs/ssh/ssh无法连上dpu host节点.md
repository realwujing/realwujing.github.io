# ssh无法连上dpu host节点

## 问题信息

1. 故障时间：2025-05-06 10:00
2. 故障节点：内蒙08多AZ测试 94.106 94.236
3. 故障现象：ssh无法连上物理节点，物理节点每次soft lockup栈都不一样，vmcore中RIP寄存器每次地址都不一样
4. 操作系统：ctyunos3 kernel-5.10 90
5. cpu: Hygon C86-4G (OPN:7493)

## 问题分析

### /var/lib/systemd/coredump

94.106出现coredump之后计算结点登录不上，重启了计算结点 可以正常登录。

![17464949551974](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17464949551974.png)

通过日志分析，systemd-hostname、systemd-journal、systemd-logind、systemd-machine、systemd-udevd产生core的原因均是因为看门狗超时，接收SIGABRT信号强制退出的。

![17465005414991](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17465005414991.png)

94.106 /var/log/messages日志中可以看到，物理机多次报过soft lockup。

![17465026293468](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17465026293468.png)

麻烦开启软死锁检测后复现，复现后会有内核coredump生成：

```bash
sysctl -w kernel.softlockup_panic=1
```

### crash报错分析

从vmcore-dmesg.txt中可以看到，内核panic的原因是softlockup: hung tasks。每次soft lockup的栈信息都不一样，vmcore中RIP寄存器每次地址都不一样。

共同点是栈都跟软中断上下文相关。

```bash
[root@nm08-az2-compute-hcm3ne-10e8e94e236 crash]# find /var/crash -type f -name "vmcore-dmesg.txt" -exec sh -c 'printf "\n\n\nProcessing file: %s\n" "$1"; awk "/Kernel panic/{p=1} p {print \"$1:\" \$0}" "$1"' _ {} \;



Processing file: /var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.514630] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.521917] CPU: 123 PID: 637 Comm: ksoftirqd/123 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.535305] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.544135] Call Trace:
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.547641]  <IRQ>
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.550656]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.555136]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.559227]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.564776]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.569643]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.575088]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.580433]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.586755]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.592297]  </IRQ>
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.595417]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.601544]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.608059] RIP: 0010:packet_rcv+0x35e/0x3d0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.613599] Code: 00 0f 84 01 fd ff ff 8b 85 f4 00 00 00 83 f8 01 0f 84 f2 fc ff ff 8b 04 24 4c 89 b5 e8 00 00 00 89 45 70 e9 e0 fc ff ff 3c 04 <0f> 85 4b fd ff ff 0f b7 b5 b4 00 00 00 48 03 b5 e0 00 00 00 48 89
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.635335] RSP: 0018:ffffc3e1daa27c30 EFLAGS: 00000293
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.641944] RAX: 0000000000000001 RBX: ffffa10c55cf2000 RCX: ffffffffc09125c0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.650677] RDX: ffffa0ec593d2e00 RSI: 000000000000001c RDI: ffffa04ceaf37f00
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.659417] RBP: ffffa04ceaf37f00 R08: ffffa04ceaf37f00 R09: ffffc3e1daa27ca0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.668159] R10: 00000000e0ccdeeb R11: 0000000000000001 R12: ffffa0ec593d2800
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.676901] R13: ffffa0ec53d90000 R14: ffffa04cdc202652 R15: 000000000000001c
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.685647]  __netif_receive_skb_list_core+0x27b/0x2e0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.692160]  __netif_receive_skb_list+0xfd/0x190
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.698081]  netif_receive_skb_list_internal+0xfc/0x1e0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.704681]  napi_complete_done+0x6f/0x190
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.710032]  virtnet_poll+0x14e/0x210 [virtio_net]
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.716158]  napi_poll+0x95/0x1c0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.720633]  net_rx_action+0xaa/0x1b0
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.725491]  __do_softirq+0xc4/0x280
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.730260]  run_ksoftirqd+0x1e/0x30
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.735026]  smpboot_thread_fn+0xc5/0x160
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.740277]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.744940]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.749220]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.754083]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-08-13:13:40/vmcore-dmesg.txt:[ 9487.761662] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.954987] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.962276] CPU: 5 PID: 40 Comm: ksoftirqd/5 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.975187] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.984024] Call Trace:
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.987532]  <IRQ>
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.990554]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.995034]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1967.999127]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.004674]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.009539]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.014985]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.020336]  ? handle_irq_event+0x76/0xc0
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.025595]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.031917]  sysvec_apic_timer_interrupt+0x31/0x80
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.038040]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.044544] RIP: 0010:_raw_spin_trylock+0x14/0x30
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.050571] Code: 84 00 00 00 00 00 0f 1f 44 00 00 c6 07 00 56 9d e9 01 67 32 00 90 0f 1f 44 00 00 8b 07 85 c0 75 15 ba 01 00 00 00 f0 0f b1 17 <75> 0a b8 01 00 00 00 e9 e0 66 32 00 31 c0 e9 d9 66 32 00 66 0f 1f
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.072296] RSP: 0018:ffffbcb00057cf30 EFLAGS: 00000246
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.078906] RAX: 0000000000000000 RBX: 000000010018d79c RCX: 00000000000006ff
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.087648] RDX: 0000000000000001 RSI: 0000000000000001 RDI: ffffffff9995ac54
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.096381] RBP: ffffa02a860b5540 R08: 0000000000000000 R09: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.105115] R10: 00000000ffffffff R11: 0000000000000000 R12: 000000000006a5ab
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.113854] R13: 0000000000000001 R14: 0000000000000001 R15: ffffa00b9980b400
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.122601]  rebalance_domains+0x2a6/0x3b0
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.127952]  __do_softirq+0xc4/0x280
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.132720]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.138261]  </IRQ>
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.141382]  do_softirq_own_stack+0x37/0x50
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.146831]  irq_exit_rcu+0xbe/0x100
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.151599]  common_interrupt+0x74/0x130
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.156758]  asm_common_interrupt+0x1e/0x40
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.162205] RIP: 0010:smpboot_thread_fn+0x8b/0x160
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.168329] Code: 75 1e 8b 7d 00 65 8b 15 83 38 b0 68 39 d7 0f 85 e4 00 00 00 e8 b6 c2 ce 00 c7 45 04 02 00 00 00 e8 4a 76 ff ff eb 95 8b 7d 00 <65> 8b 05 5e 38 b0 68 39 c7 75 6e 8b 45 04 85 c0 74 2d 83 f8 02 74
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.190065] RSP: 0018:ffffbcb0195b3ef0 EFLAGS: 00000246
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.196674] RAX: 0000000000000000 RBX: ffffa00b98f4c000 RCX: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.205416] RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000005
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.214158] RBP: ffffa00b98d2d9e0 R08: ffffa00bdabb80e8 R09: ffffa00bdabb80e8
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.222899] R10: 0000000000000308 R11: 00000000e0ccdeeb R12: ffffffff98c4c300
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.231641] R13: ffffbcb000147cb0 R14: ffffa00b98d2d9e0 R15: ffffa00b98f4c000
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.240381]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.245052]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.249336]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.254200]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-08-16:07:16/vmcore-dmesg.txt:[ 1968.261766] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.047867] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.055158] CPU: 119 PID: 617 Comm: ksoftirqd/119 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.068554] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.077383] Call Trace:
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.080892]  <IRQ>
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.083921]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.088401]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.092495]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.098042]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.102907]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.108354]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.113710]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.120035]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.125582]  </IRQ>
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.128701]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.134826]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.141342] RIP: 0010:ipt_do_table+0x23c/0x4a0 [ip_tables]
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.148243] Code: 24 28 4c 89 fd 49 89 df 4c 89 f3 49 89 d6 49 8b 46 08 49 8d 56 20 48 89 de 48 89 ef 48 89 54 24 50 48 89 44 24 48 48 8b 40 30 <e8> df 9e 02 d8 84 c0 0f 84 22 01 00 00 41 0f b7 06 49 01 c6 41 0f
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.169977] RSP: 0018:ffff9c06da9777f8 EFLAGS: 00000283
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.176587] RAX: ffffffffc0801000 RBX: ffff9c06da977840 RCX: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.185329] RDX: ffff8de3eab904e8 RSI: ffff9c06da977840 RDI: ffff8de3df166000
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.194063] RBP: ffff8de3df166000 R08: ffff8ec2cfbccc80 R09: 0000000000000001
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.202804] R10: 0000000000000000 R11: 0000000000000000 R12: ffffffffc07dc3d8
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.211547] R13: ffff8ea35ccb1000 R14: ffff8de3eab904c8 R15: ffff8de3eab90458
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.220298]  ? 0xffffffffc0801000
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.224777]  ? ipt_do_table+0x326/0x4a0 [ip_tables]
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.231007]  nf_hook_slow+0x3f/0xb0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.235679]  ip_output+0xdf/0x120
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.240157]  ? __ip_finish_output+0x160/0x160
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.245797]  __ip_queue_xmit+0x196/0x420
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.250956]  __tcp_transmit_skb+0x8d0/0x990
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.256402]  tcp_v4_do_rcv+0x140/0x1f0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.261364]  tcp_v4_rcv+0xdf7/0x10d0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.266131]  ip_protocol_deliver_rcu+0xb2/0x190
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.271963]  ip_local_deliver_finish+0x44/0x60
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.277700]  ip_sublist_rcv_finish+0x57/0x70
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.283242]  ip_list_rcv_finish.constprop.0+0x190/0x1c0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.289853]  ip_list_rcv+0x135/0x160
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.294627]  __netif_receive_skb_list_core+0x2b0/0x2e0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.301146]  __netif_receive_skb_list+0xfd/0x190
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.307078]  netif_receive_skb_list_internal+0xfc/0x1e0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.313688]  gro_normal_one+0x77/0xa0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.318550]  napi_gro_receive+0x152/0x190
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.323808]  virtnet_receive+0x85/0x1f0 [virtio_net]
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.330129]  virtnet_poll+0x54/0x210 [virtio_net]
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.336158]  napi_poll+0x95/0x1c0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.340636]  net_rx_action+0xaa/0x1b0
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.345505]  __do_softirq+0xc4/0x280
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.350278]  run_ksoftirqd+0x1e/0x30
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.355048]  smpboot_thread_fn+0xc5/0x160
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.360304]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.364975]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.369258]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.374125]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-08-13:34:06/vmcore-dmesg.txt:[  967.381718] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.876250] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.883545] CPU: 6 PID: 45 Comm: ksoftirqd/6 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.896463] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.905310] Call Trace:
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.908820]  <IRQ>
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.911852]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.916332]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.920427]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.925979]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.930847]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.936298]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.941656]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.947983]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.953527]  </IRQ>
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.956652]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.962781]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.969301] RIP: 0010:expire_timers+0xa3/0x110
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.975040] Code: 74 04 48 89 50 08 48 c7 45 08 00 00 00 00 48 8b 75 18 4c 89 e7 4c 89 75 00 f6 45 22 20 75 9b c6 07 00 0f 1f 40 00 fb 4c 89 ea <48> 89 ef e8 55 fe ff ff 4c 89 e7 e8 2d 2e 95 00 49 c7 44 24 08 00
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30618.996778] RSP: 0018:ffffab60595dbe00 EFLAGS: 00000246
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.003390] RAX: 0000000000000000 RBX: ffffab60595dbe30 RCX: ffff9bf206124980
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.012134] RDX: 0000000101ce0168 RSI: ffffffffb93947d0 RDI: ffff9bf206124980
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.020876] RBP: ffff9c52a3d4e670 R08: 0000000000000000 R09: 0000000000000002
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.029619] R10: 0000000000000202 R11: ffffab60595dbe38 R12: ffff9bf206124980
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.038361] R13: 0000000101ce0168 R14: dead000000000122 R15: 0000000000000202
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.047111]  ? tcp_delack_timer_handler+0x170/0x170
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.053334]  run_timer_softirq+0x165/0x1d0
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.058691]  ? asm_common_interrupt+0x1e/0x40
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.064329]  ? sched_clock+0x5/0x10
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.069003]  ? sched_clock_cpu+0xc/0xb0
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.074066]  __do_softirq+0xc4/0x280
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.078841]  run_ksoftirqd+0x1e/0x30
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.083613]  smpboot_thread_fn+0xc5/0x160
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.088867]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.093540]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.097825]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.102694]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-07-23:32:03/vmcore-dmesg.txt:[30619.110293] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316139] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316139] CPU: 98 PID: 247602 Comm: ps Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316139] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316139] Call Trace:
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316139]  <IRQ>
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316205]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  </IRQ>
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RIP: 0010:smp_call_function_single+0x9b/0x120
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] Code: be e7 7d a9 00 01 ff 00 0f 85 8e 00 00 00 85 c9 75 4c 48 c7 c6 80 67 03 00 65 48 03 35 e6 5f e7 7d 8b 46 08 a8 01 74 09 f3 90 <8b> 46 08 a8 01 75 f7 83 4e 08 01 4c 89 46 10 48 89 56 18 e8 8d fe
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RSP: 0018:ffffbe79f6cdbc60 EFLAGS: 00000202
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RAX: 0000000000000001 RBX: 000023b608e9f502 RCX: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RDX: 0000000000000000 RSI: ffffa0ad4d436780 RDI: 000000000000007d
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RBP: ffffbe79f6cdbca8 R08: ffffffff8203e8a0 R09: 000000000000007d
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] R10: 0000000000010000 R11: 0000000000000007 R12: 0000000000000001
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] R13: 000000000001f8a8 R14: 0000000000000001 R15: 000000000eaa1b4d
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? cpu_show_retbleed+0x50/0x50
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? ktime_get+0x38/0xa0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  arch_freq_prepare_all+0x8b/0xe0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? proc_reg_release+0x90/0x90
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  cpuinfo_open+0xe/0x20
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  do_dentry_open+0x14b/0x370
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  do_open+0x1dc/0x320
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  path_openat+0x10b/0x1d0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  do_filp_open+0x90/0x140
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? rcu_nocb_try_bypass+0x1f3/0x370
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? page_counter_try_charge+0x2f/0xc0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  ? files_cgroup_alloc_fd+0x5c/0x70
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  do_sys_openat2+0x207/0x2e0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  __x64_sys_openat+0x54/0xa0
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  do_syscall_64+0x40/0x80
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222]  entry_SYSCALL_64_after_hwframe+0x61/0xc6
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RIP: 0033:0x7f0af54fc8eb
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] Code: 25 00 00 41 00 3d 00 00 41 00 74 4b 64 8b 04 25 18 00 00 00 85 c0 75 67 44 89 e2 48 89 ee bf 9c ff ff ff b8 01 01 00 00 0f 05 <48> 3d 00 f0 ff ff 0f 87 91 00 00 00 48 8b 4c 24 28 64 48 2b 0c 25
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RSP: 002b:00007ffd915fe0d0 EFLAGS: 00000246 ORIG_RAX: 0000000000000101
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RAX: ffffffffffffffda RBX: 000056199e62e2d0 RCX: 00007f0af54fc8eb
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RDX: 0000000000000000 RSI: 00007f0af56eb7f0 RDI: 00000000ffffff9c
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] RBP: 00007f0af56eb7f0 R08: 0000000000000008 R09: 0000000000000001
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] R10: 0000000000000000 R11: 0000000000000246 R12: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316222] R13: 000056199e62e2d0 R14: 0000000000000001 R15: 000056199e650e30
/var/crash/127.0.0.1-2025-05-08-10:31:14/vmcore-dmesg.txt:[39291.316383] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.525521] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.532809] CPU: 10 PID: 65 Comm: ksoftirqd/10 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.545913] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.554750] Call Trace:
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.558256]  <IRQ>
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.561271]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.565750]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.569840]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.575386]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.580251]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.585695]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.591051]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.597372]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.602915]  </IRQ>
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.606035]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.612158]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.618662] RIP: 0010:update_blocked_averages+0xd5/0x120
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.625358] Code: 1e 83 bd b8 09 00 00 01 49 8b 5d 00 76 3b 48 8b b5 c0 09 00 00 31 d2 4c 89 ef e8 c6 84 cd 00 48 89 ef e8 8e 2c ff ff 41 54 9d <48> 8b 44 24 08 65 48 2b 04 25 28 00 00 00 75 2f 48 83 c4 10 5b 5d
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.647092] RSP: 0018:ffffa2a799683e48 EFLAGS: 00000246
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.653701] RAX: ffff91b346320510 RBX: 0000000000000001 RCX: 0000000000000050
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.662442] RDX: 000000000000000a RSI: ffff91b3463355c0 RDI: ffff91b346335540
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.671183] RBP: ffff91b346335540 R08: 000008f4f22c8a69 R09: 000008f4f22c8a69
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.679922] R10: 0000000000000322 R11: 00000000d744fcc9 R12: 0000000000000246
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.688662] R13: 0000000000000000 R14: ffffffff936060f8 R15: 00000000000000a0
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.697404]  run_rebalance_domains+0x3e/0x60
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.702935]  __do_softirq+0xc4/0x280
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.707706]  run_ksoftirqd+0x1e/0x30
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.712471]  smpboot_thread_fn+0xc5/0x160
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.717720]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.722389]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.726669]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.731533]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-07-14:57:26/vmcore-dmesg.txt:[12355.739114] kexec: Bye!



Processing file: /var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.222135] Kernel panic - not syncing: softlockup: hung tasks
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.229422] CPU: 13 PID: 79 Comm: migration/13 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.x86_64 #1
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.242519] Hardware name: Suma MH221/62DC24-C, BIOS 08.03.04.71.14 03/14/2025
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.251348] Call Trace:
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.254854]  <IRQ>
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.257871]  dump_stack+0x57/0x6e
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.262350]  panic+0xfb/0x2dc
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.266442]  watchdog_timer_fn.cold+0xc/0x16
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.271991]  __run_hrtimer+0x5e/0x190
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.276856]  __hrtimer_run_queues+0x81/0xf0
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.282302]  hrtimer_interrupt+0x110/0x2c0
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.287645]  __sysvec_apic_timer_interrupt+0x5f/0xe0
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.293964]  asm_call_irq_on_stack+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.299506]  </IRQ>
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.302627]  sysvec_apic_timer_interrupt+0x72/0x80
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.308755]  asm_sysvec_apic_timer_interrupt+0x12/0x20
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.315270] RIP: 0010:finish_task_switch+0x86/0x290
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.321491] Code: 00 0f 1f 44 00 00 0f 1f 44 00 00 41 c7 45 3c 00 00 00 00 0f 1f 44 00 00 4c 89 e7 e8 c4 fe ff ff fb 65 48 8b 04 25 80 f8 01 00 <e9> 45 00 00 00 4d 85 f6 74 21 65 48 8b 04 25 80 f8 01 00 4c 3b b0
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.343214] RSP: 0018:ffffb296d96ffe48 EFLAGS: 00000246
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.349824] RAX: ffff8b7ad90f8000 RBX: ffff8b7ad90f8000 RCX: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.358565] RDX: 0000000000000000 RSI: 0000000000000000 RDI: ffff8b99c64b5540
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.367307] RBP: ffffb296d96ffe70 R08: 0000000000000047 R09: 0000000000000047
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.376046] R10: ffff8b7ad91040c0 R11: 0000000000000000 R12: ffff8b99c64b5540
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.384785] R13: ffff8b7ad9104000 R14: 0000000000000000 R15: 0000000000000000
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.393529]  ? finish_task_switch+0x7c/0x290
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.399072]  __schedule+0x2f2/0x640
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.403734]  schedule+0x46/0xb0
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.408021]  smpboot_thread_fn+0x10b/0x160
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.413360]  ? sort_range+0x20/0x20
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.418021]  kthread+0xfe/0x140
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.422303]  ? kthread_park+0x90/0x90
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.427167]  ret_from_fork+0x22/0x30
/var/crash/127.0.0.1-2025-05-08-15:30:09/vmcore-dmesg.txt:[ 6703.434780] kexec: Bye!
```

1. **`find /var/crash -type f -name "vmcore-dmesg.txt"`**：查找所有 `vmcore-dmesg.txt` 文件。
2. **`-exec sh -c '...'`**：为每个文件运行 shell 脚本。
3. **`printf "\n\n\nProcessing file: %s\n" "$1"`**：
   - `printf "\n\n\n"`：输出三行空行（每个 `\n` 是一行）。
   - `Processing file: %s\n`：打印文件名（`$1` 是文件路径）。
4. **`awk "/Kernel panic/{p=1} p {print \"$1:\" \$0}" "$1"`**：处理文件内容，打印包含 "Kernel panic" 的行及其后续行，每行前加上文件名。
5. **`_ {}`**：`_` 是占位符，`{}` 是文件路径。

### 测试场景分析

当前vm中的iperf流量不会走到物理机上，直接通过net-vf转走了，vm中的fio流量会通过qemu走到物理机的pf。

| 流量来源        | 经 QEMU？ | 走 VF？ | 走 PF？ | 说明                             |
| ----------- | ------- | ----- | ----- | ------------------------------ |
| `iperf`     | 否       | 是     | 否     | net-vf 直通，不经 QEMU              |
| `fio`（Ceph） | 是       | 否     | 是     | QEMU 发起 RADOS over TCP，走 PF 网卡 |

当前在物理机上跑很多vm，vm间只打iperf流不崩溃，只打fio流也不崩溃，vm间iperf流+fio流混合打，物理机就会崩溃。

#### 【核心猜测】混合负载引发**资源竞争或软中断风暴**导致崩溃（例如 soft lockup）

---

##### 一、可能原因分析

###### 1. **内核软中断饱和（ksoftirqd 占满 CPU）**

- iperf 是大量 UDP/TCP 流量直通 VF → 宿主机 PF。
- fio 是 QEMU 用户态 I/O → librbd → 宿主机内核 TCP stack → PF。
- 两者都压 PF 网卡、走 TCP 栈和 softirq。
- **大量混合流量可能触发 ksoftirqd 长时间运行，触发 soft lockup，最终宕机**。

###### 2. **宿主机 PF 网卡或驱动（如 mlx5_core）处理能力不足**

- net-vf 和 QEMU + RBD 流量同时堆积在 PF 上。
- 如果驱动中断处理不及时，可能导致 NAPI 驱动收包 backlog 爆炸。
- 一旦 backlog 占满 softnet 结构，会导致丢包、不可中断阻塞、CPU hang。

###### 3. **Ceph librbd 与网络栈/软中断路径耦合太紧**

- librbd 默认是同步调用 RADOS（同步 socket send/recv）；
- 如果同时大量收发包 → socket 被阻塞 → QEMU 卡死 → VM 卡住；
- 宿主机可能因进程堆积 + 内核软中断延迟 → 出现 soft lockup。

###### 4. **NUMA、IRQ 绑定不合理导致瓶颈 CPU 被拖死**

- 若 PF 中断和 QEMU/librbd 都调度在同一 CPU（如 node0）：

  - iperf、fio、RADOS 三重流量一起挤爆 CPU → soft lockup。

### 关中断时常统计

#### /proc/softirqs

从下图可以看到软中断在部分cpu上是很集中的。

![17467712465771](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17467712465771.png)

#### ksoftirqd 线程

使用trace-irqoff观察到ksoftirqd线程存在长时间关闭软中断情况。

![17467660274205](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17467660274205.png)

![1746766102899](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/1746766102899.png)

![17467664157822](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17467664157822.png)

#### mpstat

使用mpstat观察到硬中断耗时比软中断耗时高。这里不太正常，毕竟硬中断在顶半部中处理，是非常快的。

![17467813142241](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17467813142241.png)

#### vmstat

### vifo中断注册

![17467813142242](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17467813142242.png)

当前intel好像实现了postinterupt，貌似可以将vf中断直通到vm中，海光4号好像不行。

这涉及到 SR-IOV VF 中断的一种高级优化机制 —— **Post-interrupt / Posted Interrupts（PI）**，也叫 **Interrupt Remapping with Posted Interrupts**。

---

#### **1. 什么是 Posted Interrupt（PI）？**

Posted Interrupt 是 **Intel VT-d 支持的一种优化机制**，用于将 **设备中断直接投递给 VM，而不经过宿主机的中断处理流程**，也就是说：

- 传统中断路径：**VF → Host CPU → KVM/vfio → 注入 VM**
- Posted Interrupt 路径：**VF → 直接送到 VM vCPU → Guest 内核响应中断**

这样可以大大减少物理机 CPU 的开销，尤其是大规模网卡/高频中断时。

---

#### **2. 实现 Posted Interrupt 的条件**

要实现 PI，需要满足：

| 条件                   | 描述                                             |
| -------------------- | ---------------------------------------------- |
| **Intel VT-d 支持 PI** | 硬件 IOMMU 必须支持 Posted Interrupt                 |
| **CPU 支持 APICv**     | 支持 APIC virtualization                         |
| **KVM + vfio-pci**   | KVM 启用 interrupt remapping 且 vfio 使用 MSI/MSI-X |
| **使用 Intel 架构**      | 当前只在 Intel 平台上支持（AMD、海光、飞腾目前均不支持）              |

---

#### **3. 海光 4 号为何不支持？**

海光 4（x86 架构，Hygon Dhyana）虽然基于 AMD Zen 系列微架构，但目前**并未开放对 Intel 风格 VT-d 的 Posted Interrupt 机制的支持**：

- 海光支持 SR-IOV 和 IOMMU（通过 AMD-Vi）；
- 但 **没有实现 Posted Interrupt（也可能内核没有适配支持）**；
- VF 的中断依然走传统路径 → 仍然吃宿主机 CPU。

---

#### **4. 如何确认当前是否启用了 Posted Interrupt？**

在 **Intel 平台** 上可以通过：

```bash
dmesg | grep -i 'posted interrupt'
```

或者查看：

```bash
cat /sys/module/kvm/parameters/enable_apicv
```

但在海光平台上基本不会看到这些项，表示不支持。

---

#### **5. 影响总结**

| 平台           | VF 中断路径            | CPU 中断开销 | 支持 PI |
| ------------ | ------------------ | -------- | ----- |
| Intel（支持 PI） | VF → VM vCPU       | 低        | 是     |
| AMD / 海光     | VF → Host CPU → VM | 高        | 否     |

---

所以在海光 4 上观察到的现象（VF 中断还是会拖累宿主机 CPU，甚至引发 soft lockup）是**预期行为**，也是因为**没有 Posted Interrupt 支持**所致。

### vm vf中断打断ksoftirqd?

---

#### **核心问题：VF 中断和 fio 流量为什么有关联？**

首先明确两条通路：

##### ① **VM 内 `iperf`（VF 网络）流量路径**

- VF（`net-vf`）SR-IOV 设备直通给 VM；
- VF 中断 **直接打到物理机 CPU**，由 host IRQ handler 处理；
- **不会经过 QEMU，不走 host 内核协议栈（net stack）**；
- 不会产生 host 的 ksoftirqd（NET_RX）负载；
- 除非 VF 中断也配置为 MSI-X 且频繁打断 host 的 ksoftirq。

##### ② **VM 内 `fio`（virtio-blk）流量路径**

- QEMU 通过 virtio-blk 模拟磁盘 → host 上的 Ceph/rbd/PF 实设备；
- 该 I/O 请求最终会在 host 上走块层 → 网络层 → PF 网卡；
- 中间需要 host 的 softirq（如 `BLOCK`, `NET_TX`, `NET_RX`）支持；
- 所以 **fio 触发的是 host 的真实软中断和网络中断压力**。

---

#### **关键在于“打断”行为：VF 中断可能打断 ksoftirqd 正在执行的 softirq 处理路径**

虽然 VF 流量**并不会**走 fio 流量路径，但：

##### VF 中断到来时，可能发生

1. VF 中断触发，打在 host 上某个 CPU 上（中断 affinity 绑定核心）；
2. 正好这个核正在执行 `ksoftirqd/n`，处理 PF 的软中断（如 Ceph 发包）；
3. 被 VF 中断抢占，软中断堆积，`ksoftirqd/n` 执行被延迟；
4. 导致软中断 backlog 堆积，进而触发 `soft lockup` 报警。

---

#### **前面描述的现象说明：**

- **VF 中断压在物理机某核上频繁打断 `ksoftirqd`**；
- 同时，fio 走的 PF 需要的 softirq（net_tx/net_rx）无法及时处理；
- 结果：fio 变慢 + ksoftirqd 超时 + soft lockup。

---

#### **总结：**

| 类型         | 是否打断 ksoftirqd | 是否走 host 协议栈 | 是否走 host 网络（PF） |
| ---------- | -------------- | ------------ | --------------- |
| VF 中断      | 是，频繁中断         | 否            | 否               |
| fio blk 流量 | 否，但依赖 softirq  | 是            | 是（如 Ceph → PF）  |

**所以问题核心不是“VF 中断通知 fio”，而是：VF 中断干扰了 fio 所依赖的 host softirq 执行。**

### 解决方案

经测试，有如下两种解决方案：

1. FPGA中断聚合0x4770设置0x8f、0x87、0x85这几个都测试了下都正常，0x8f跑了20个小时、0x87跑了一个多小时、0x85跑了11个小时，都正常。（0x20a80寄存器保持的默认0）。

2. 手动设置了vfio中断的cpu亲和性，使中断均衡后。测试脚本跑了一晚上，目前原问题没有出现。观察到node0上的cpu的irq负载也降了下去。

![17470987709495](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17470987709495.png)

## 问题结果

vfio注册中断的流程中会设置中断的smp_affinity为vf设备所在node节点(node0)的所有cpu。irqbalance会修改这个smp_affinity，但是观察到也只是修改为node0上的某个cpu。手动设置smp_affinity需要关闭irqbalance。
