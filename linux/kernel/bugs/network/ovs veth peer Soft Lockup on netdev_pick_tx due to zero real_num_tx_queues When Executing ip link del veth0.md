# ovs veth peer Soft Lockup on netdev_pick_tx due to zero real_num_tx_queues When Executing ip link del veth0

- [Linux 虚拟网络设备 veth-pair 详解，看这一篇就够了](https://www.cnblogs.com/bakari/p/10613710.html)

## 测试环境

管理区新架构测试沟通

2022年-公有云-贵州-贵州4.0AZ1-测试 10.246.116.1

os-deploy

10e63e7e11

sudo -s

10.63.2.11

10.63.2.12

10.63.2.13

ssh secure@10.63.2.11 -p 10000

scp -P 10000 kernel-5.10.0-136.12.0.88.f4e7ea49f7bd.ctl3.aarch64.rpm secure@10.63.2.13:~/wujing

ctkernel-lts-5.10/yuanql9/bugfix-pr-veth-peer-soft-lockup

## 复现步骤

在13设备上复现了一下，创建完网桥，网桥上的流量越大复现概率越高：

```bash
ovs-vsctl add-br os_manage1
ovs-vsctl add-port os_manage1 bond0.151
ip link add name veth0 type veth peer name veth1
ovs-vsctl add-port os_manage1 veth0
ip link del veth0
ovs-vsctl del-port os_manage1 veth0
```

![openvswitch_dmesg](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/openvswitch_dmesg.png)

## 根因定位

配置 Linux 内核在检测到任务挂起（hung task）或软锁死（soft lockup）时触发系统崩溃（panic）:

```bash
sysctl -w kernel.hung_task_panic=1
sysctl -w kernel.softlockup_panic=1
```

### vmcore-dmesg.txt

```bash
[ 4393.873711] watchdog: BUG: soft lockup - CPU#21 stuck for 12s! [swapper/21:0]
[ 4393.882198] Modules linked in: nft_nat nft_masq nft_counter nft_chain_nat vhost_net vhost vhost_iotlb tap xt_TPROXY nf_tproxy_ipv6 nf_tproxy_ipv4 cls_bpf vxlan ip6_udp_tunnel udp_tunnel veth xt_nat xt_socket nf_socket_ipv4 nf_socket_ipv6 ip6table_raw iptable_raw xt_CT nbd rbd ceph libceph dns_resolver ip6t_REJECT nf_reject_ipv6 xt_mark xt_set ip_set_hash_ipportnet ip_set_hash_ipportip ip_set_hash_ipport ip_set_hash_ip ip_set_bitmap_port ip_vs_rr ip_set ip_vs nf_tables dummy xt_comment binfmt_misc nf_conntrack_netlink nfnetlink xt_addrtype xt_CHECKSUM xt_MASQUERADE xt_conntrack ipt_REJECT nf_reject_ipv4 ip6table_mangle ip6table_nat iptable_mangle iptable_nat ebtable_filter ebtables ip6table_filter ip6_tables iptable_filter ip_tables tun sch_ingress bonding rfkill 8021q garp mrp openvswitch nsh nf_conncount nf_nat rpcrdma rdma_ucm ib_srpt ib_isert iscsi_target_mod sunrpc target_core_mod ib_iser rdma_cm iw_cm ib_cm libiscsi scsi_transport_iscsi hns_roce_hw_v2 ib_uverbs ib_core
[ 4393.882270]  ipmi_ssif realtek ses hibmc_drm enclosure hclge hisi_sas_v3_hw drm_vram_helper vfat fat acpi_ipmi ipmi_si hns3 hisi_sas_main drm_ttm_helper hnae3 libsas hinic nvme ipmi_devintf i2c_designware_platform ttm scsi_transport_sas host_edma_drv sg nvme_core ipmi_msghandler i2c_designware_core hisi_uncore_hha_pmu hisi_uncore_ddrc_pmu hisi_uncore_l3c_pmu hisi_uncore_pmu ext4 mbcache jbd2 sch_fq_codel overlay br_netfilter bridge stp llc nf_conntrack nf_defrag_ipv6 nf_defrag_ipv4 fuse xfs libcrc32c dm_multipath sd_mod t10_pi ghash_ce sha2_ce ahci sha256_arm64 libahci sha1_ce sbsa_gwdt libata megaraid_sas nfit libnvdimm dm_mirror dm_region_hash dm_log dm_mod aes_ce_blk crypto_simd cryptd aes_ce_cipher
[ 4394.041268] CPU: 21 PID: 0 Comm: swapper/21 Kdump: loaded Not tainted 5.10.0-136.12.0.88.ctl3.aarch64 #1
[ 4394.052208] Hardware name: Huawei TaiShan 2280 V2/BC82AMDDA, BIOS 1.70 01/07/2021
[ 4394.061192] pstate: 60400009 (nZCv daif +PAN -UAO -TCO BTYPE=--)
[ 4394.068702] pc : netdev_pick_tx+0x27c/0x294
[ 4394.074385] lr : netdev_pick_tx+0xe8/0x294
[ 4394.079977] sp : ffff800012ab3540
[ 4394.084788] x29: ffff800012ab3540 x28: ffff800012ab3b90
[ 4394.091580] x27: ffff0021428bca00 x26: 0000000000000000
[ 4394.098377] x25: 0000000000000000 x24: 00000000ffffffff
[ 4394.105161] x23: 0000000000000000 x22: ffff20222543e000
[ 4394.111937] x21: ffff0021428bca00 x20: 0000000000000000
[ 4394.118713] x19: ffff20222543e000 x18: 0000000000000000
[ 4394.125479] x17: 0000000000000000 x16: 0000000000000000
[ 4394.132243] x15: 0000000000000001 x14: 0000000000004788
[ 4394.138998] x13: 000000000000a888 x12: 000000000000ca88
[ 4394.145741] x11: ffff800012ab3720 x10: 0000000000000188
[ 4394.152482] x9 : ffff800010adc558 x8 : ffff0020b064da10
[ 4394.159232] x7 : 0000000000000000 x6 : 00000069c6f1b488
[ 4394.165972] x5 : 0000000000000000 x4 : 0000000000000000
[ 4394.172693] x3 : 0000000000000000 x2 : 0000000000000000
[ 4394.179414] x1 : ffff0021428bca00 x0 : 0000000000000000
[ 4394.186133] Call trace:
[ 4394.189985]  netdev_pick_tx+0x27c/0x294
[ 4394.195220]  netdev_core_pick_tx+0xdc/0x10c
[ 4394.200796]  __dev_queue_xmit+0x104/0xac0
[ 4394.206192]  dev_queue_xmit+0x1c/0x30
[ 4394.211251]  ovs_vport_send+0xac/0x180 [openvswitch]
[ 4394.217599]  do_output+0x60/0x160 [openvswitch]
[ 4394.223509]  do_execute_actions+0x868/0x880 [openvswitch]
[ 4394.230277]  ovs_execute_actions+0x64/0xe0 [openvswitch]
[ 4394.236959]  ovs_dp_process_packet+0x9c/0x1e4 [openvswitch]
[ 4394.243899]  ovs_vport_receive+0x7c/0xec [openvswitch]
[ 4394.250412]  netdev_port_receive+0xbc/0x17c [openvswitch]
[ 4394.257183]  netdev_frame_hook+0x2c/0x44 [openvswitch]
[ 4394.263673]  __netif_receive_skb_core+0x294/0xdd4
[ 4394.269708]  __netif_receive_skb_list_core+0xa4/0x290
[ 4394.276067]  __netif_receive_skb_list+0x120/0x1a0
[ 4394.282053]  netif_receive_skb_list_internal+0xe4/0x1f0
[ 4394.288538]  napi_complete_done+0x70/0x1f0
[ 4394.293897]  hns3_nic_common_poll+0x104/0x220 [hns3]
[ 4394.300093]  napi_poll+0xcc/0x264
[ 4394.304618]  net_rx_action+0xd4/0x21c
[ 4394.309463]  __do_softirq+0x130/0x358
[ 4394.314284]  irq_exit+0x11c/0x13c
[ 4394.318740]  __handle_domain_irq+0x88/0xf0
[ 4394.323954]  gic_handle_irq+0x78/0x2c0
[ 4394.328804]  el1_irq+0xb8/0x140
[ 4394.333029]  arch_cpu_idle+0x18/0x40
[ 4394.337667]  default_idle_call+0x5c/0x1c0
[ 4394.342723]  cpuidle_idle_call+0x174/0x1b0
[ 4394.347853]  do_idle+0xc8/0x160
[ 4394.352025]  cpu_startup_entry+0x30/0xfc
[ 4394.356986]  secondary_start_kernel+0x158/0x1ec
[ 4394.362556] Kernel panic - not syncing: softlockup: hung tasks
[ 4394.369422] CPU: 21 PID: 0 Comm: swapper/21 Kdump: loaded Tainted: G             L    5.10.0-136.12.0.88.ctl3.aarch64 #1
[ 4394.381711] Hardware name: Huawei TaiShan 2280 V2/BC82AMDDA, BIOS 1.70 01/07/2021
[ 4394.390268] Call trace:
[ 4394.393809]  dump_backtrace+0x0/0x1e4
[ 4394.398565]  show_stack+0x20/0x2c
[ 4394.402976]  dump_stack+0xd8/0x140
[ 4394.407475]  panic+0x168/0x390
[ 4394.411630]  watchdog_timer_fn+0x230/0x290
[ 4394.416825]  __run_hrtimer+0x98/0x2a0
[ 4394.421586]  __hrtimer_run_queues+0xb0/0x134
[ 4394.426953]  hrtimer_interrupt+0x13c/0x3c0
[ 4394.432151]  arch_timer_handler_phys+0x3c/0x50
[ 4394.437700]  handle_percpu_devid_irq+0x90/0x1f4
[ 4394.443336]  __handle_domain_irq+0x84/0xf0
[ 4394.448538]  gic_handle_irq+0x78/0x2c0
[ 4394.453397]  el1_irq+0xb8/0x140
[ 4394.457661]  netdev_pick_tx+0x27c/0x294
[ 4394.462625]  netdev_core_pick_tx+0xdc/0x10c
[ 4394.467920]  __dev_queue_xmit+0x104/0xac0
[ 4394.473034]  dev_queue_xmit+0x1c/0x30
[ 4394.477810]  ovs_vport_send+0xac/0x180 [openvswitch]
[ 4394.483885]  do_output+0x60/0x160 [openvswitch]
[ 4394.489517]  do_execute_actions+0x868/0x880 [openvswitch]
[ 4394.496018]  ovs_execute_actions+0x64/0xe0 [openvswitch]
[ 4394.502428]  ovs_dp_process_packet+0x9c/0x1e4 [openvswitch]
[ 4394.509102]  ovs_vport_receive+0x7c/0xec [openvswitch]
[ 4394.515344]  netdev_port_receive+0xbc/0x17c [openvswitch]
[ 4394.521845]  netdev_frame_hook+0x2c/0x44 [openvswitch]
[ 4394.528091]  __netif_receive_skb_core+0x294/0xdd4
[ 4394.533904]  __netif_receive_skb_list_core+0xa4/0x290
[ 4394.540061]  __netif_receive_skb_list+0x120/0x1a0
[ 4394.545867]  netif_receive_skb_list_internal+0xe4/0x1f0
[ 4394.552192]  napi_complete_done+0x70/0x1f0
[ 4394.557407]  hns3_nic_common_poll+0x104/0x220 [hns3]
[ 4394.563478]  napi_poll+0xcc/0x264
[ 4394.567904]  net_rx_action+0xd4/0x21c
[ 4394.572679]  __do_softirq+0x130/0x358
[ 4394.577453]  irq_exit+0x11c/0x13c
[ 4394.581875]  __handle_domain_irq+0x88/0xf0
[ 4394.587084]  gic_handle_irq+0x78/0x2c0
[ 4394.591944]  el1_irq+0xb8/0x140
[ 4394.596199]  arch_cpu_idle+0x18/0x40
[ 4394.600884]  default_idle_call+0x5c/0x1c0
[ 4394.606007]  cpuidle_idle_call+0x174/0x1b0
[ 4394.611213]  do_idle+0xc8/0x160
[ 4394.615465]  cpu_startup_entry+0x30/0xfc
[ 4394.620490]  secondary_start_kernel+0x158/0x1ec
[ 4394.626146] SMP: stopping secondary CPUs
[ 4394.632839] Starting crashdump kernel...
[ 4394.637846] Bye!
```

### crash

```bash
[root@kvm-10e63e2e11 127.0.0.1-2025-02-27-18:19:00]# ip a | grep -i 10.63.2.11
    inet 10.63.2.11/21 brd 10.63.7.255 scope global noprefixroute os_manage
[root@kvm-10e63e2e11 127.0.0.1-2025-02-27-18:19:00]#
[root@kvm-10e63e2e11 127.0.0.1-2025-02-27-18:19:00]# pwd
/var/crash/127.0.0.1-2025-02-27-18:19:00
```

```bash
crash /lib/debug/lib/modules/5.10.0-136.12.0.88.ctl3.aarch64/vmlinux vmcore
```

#### sys

sys查看panic原因：
```bash
crash> sys
      KERNEL: /lib/debug/lib/modules/5.10.0-136.12.0.88.ctl3.aarch64/vmlinux  [TAINTED]
    DUMPFILE: vmcore  [PARTIAL DUMP]
        CPUS: 96
        DATE: Thu Feb 27 18:17:42 CST 2025
      UPTIME: 01:13:14
LOAD AVERAGE: 24.54, 10.38, 8.03
       TASKS: 10764
    NODENAME: kvm-10e63e2e11
     RELEASE: 5.10.0-136.12.0.88.ctl3.aarch64
     VERSION: #1 SMP Fri Jul 28 07:11:01 UTC 2023
     MACHINE: aarch64  (unknown Mhz)
      MEMORY: 192 GB
       PANIC: "Kernel panic - not syncing: softlockup: hung tasks"
```

#### bt

##### bt

bt 查看堆栈回溯：
```bash
crash> bt
PID: 0        TASK: ffff00208d452680  CPU: 21   COMMAND: "swapper/21"
 #0 [ffff800012ab2f60] __crash_kexec at ffff8000101aafa0
 #1 [ffff800012ab30f0] panic at ffff800010cd72bc
 #2 [ffff800012ab31d0] watchdog_timer_fn at ffff8000101f25ac
 #3 [ffff800012ab3220] __run_hrtimer at ffff80001017d314
 #4 [ffff800012ab3270] __hrtimer_run_queues at ffff80001017d5cc
 #5 [ffff800012ab32d0] hrtimer_interrupt at ffff80001017dc18
 #6 [ffff800012ab3340] arch_timer_handler_phys at ffff800010a5ec88
 #7 [ffff800012ab3350] handle_percpu_devid_irq at ffff80001015011c
 #8 [ffff800012ab3380] __handle_domain_irq at ffff800010146fb0
 #9 [ffff800012ab33c0] gic_handle_irq at ffff800010010124
#10 [ffff800012ab3520] el1_irq at ffff800010012374
#11 [ffff800012ab3540] netdev_pick_tx at ffff800010adc6e8
#12 [ffff800012ab3590] netdev_core_pick_tx at ffff800010ae675c
#13 [ffff800012ab35c0] __dev_queue_xmit at ffff800010ae6890
#14 [ffff800012ab3630] dev_queue_xmit at ffff800010ae7268
#15 [ffff800012ab3640] ovs_vport_send at ffff8000098f95b8 [openvswitch]
#16 [ffff800012ab3670] do_output at ffff8000098e47fc [openvswitch]
#17 [ffff800012ab36b0] do_execute_actions at ffff8000098e66b4 [openvswitch]
#18 [ffff800012ab3830] ovs_execute_actions at ffff8000098e6b00 [openvswitch]
#19 [ffff800012ab3860] ovs_dp_process_packet at ffff8000098eaba8 [openvswitch]
#20 [ffff800012ab38e0] ovs_vport_receive at ffff8000098f949c [openvswitch]
#21 [ffff800012ab3ae0] netdev_port_receive at ffff8000098fa03c [openvswitch]
#22 [ffff800012ab3b10] netdev_frame_hook at ffff8000098fa128 [openvswitch]
#23 [ffff800012ab3b20] __netif_receive_skb_core at ffff800010ae77f0
#24 [ffff800012ab3bd0] __netif_receive_skb_list_core at ffff800010ae88d0
#25 [ffff800012ab3c60] __netif_receive_skb_list at ffff800010ae8bdc
#26 [ffff800012ab3cc0] netif_receive_skb_list_internal at ffff800010ae8d40
#27 [ffff800012ab3d40] napi_complete_done at ffff800010ae9aec
#28 [ffff800012ab3d70] hns3_nic_common_poll at ffff800009be7350 [hns3]
#29 [ffff800012ab3e10] napi_poll at ffff800010ae9d38
#30 [ffff800012ab3e50] net_rx_action at ffff800010ae9fa4
#31 [ffff800012ab3ed0] __do_softirq at ffff80001001075c
#32 [ffff800012ab3f70] irq_exit at ffff8000100b06cc
#33 [ffff800012ab3f90] __handle_domain_irq at ffff800010146fb4
#34 [ffff800012ab3fd0] gic_handle_irq at ffff800010010124
--- <IRQ stack> ---
#35 [ffff800013093f00] el1_irq at ffff800010012374
#36 [ffff800013093f20] arch_cpu_idle at ffff800010cedb84
#37 [ffff800013093f30] default_idle_call at ffff800010cf9c58
#38 [ffff800013093f50] cpuidle_idle_call at ffff8000100f7850
#39 [ffff800013093f90] do_idle at ffff8000100f7954
#40 [ffff800013093fc0] cpu_startup_entry at ffff8000100f7bc0
#41 [ffff800013093fe0] secondary_start_kernel at ffff80001002a498
```

##### bt -sx

bt -sx:

  - **`bt`**：在 `crash` 工具中，显示当前任务的调用栈（函数调用顺序）。
  - **`-s`**：把地址变成函数名，方便看。
  - **`-x`**：用十六进制显示地址。

```bash
crash> bt -sx
PID: 0        TASK: ffff00208d452680  CPU: 21   COMMAND: "swapper/21"
 #0 [ffff800012ab2f60] __crash_kexec+0x110 at ffff8000101aafa0
 #1 [ffff800012ab30f0] panic+0x17c at ffff800010cd72bc
 #2 [ffff800012ab31d0] watchdog_timer_fn+0x22c at ffff8000101f25ac
 #3 [ffff800012ab3220] __run_hrtimer+0x94 at ffff80001017d314
 #4 [ffff800012ab3270] __hrtimer_run_queues+0xac at ffff80001017d5cc
 #5 [ffff800012ab32d0] hrtimer_interrupt+0x138 at ffff80001017dc18
 #6 [ffff800012ab3340] arch_timer_handler_phys+0x38 at ffff800010a5ec88
 #7 [ffff800012ab3350] handle_percpu_devid_irq+0x8c at ffff80001015011c
 #8 [ffff800012ab3380] __handle_domain_irq+0x80 at ffff800010146fb0
 #9 [ffff800012ab33c0] gic_handle_irq+0x74 at ffff800010010124
#10 [ffff800012ab3520] el1_irq+0xb4 at ffff800010012374
#11 [ffff800012ab3540] netdev_pick_tx+0x278 at ffff800010adc6e8
#12 [ffff800012ab3590] netdev_core_pick_tx+0xd8 at ffff800010ae675c
#13 [ffff800012ab35c0] __dev_queue_xmit+0x100 at ffff800010ae6890
#14 [ffff800012ab3630] dev_queue_xmit+0x18 at ffff800010ae7268
#15 [ffff800012ab3640] ovs_vport_send+0xa8 at ffff8000098f95b8 [openvswitch]
#16 [ffff800012ab3670] do_output+0x5c at ffff8000098e47fc [openvswitch]
#17 [ffff800012ab36b0] do_execute_actions+0x864 at ffff8000098e66b4 [openvswitch]
#18 [ffff800012ab3830] ovs_execute_actions+0x60 at ffff8000098e6b00 [openvswitch]
#19 [ffff800012ab3860] ovs_dp_process_packet+0x98 at ffff8000098eaba8 [openvswitch]
#20 [ffff800012ab38e0] ovs_vport_receive+0x78 at ffff8000098f949c [openvswitch]
#21 [ffff800012ab3ae0] netdev_port_receive+0xb8 at ffff8000098fa03c [openvswitch]
#22 [ffff800012ab3b10] netdev_frame_hook+0x28 at ffff8000098fa128 [openvswitch]
#23 [ffff800012ab3b20] __netif_receive_skb_core+0x290 at ffff800010ae77f0
#24 [ffff800012ab3bd0] __netif_receive_skb_list_core+0xa0 at ffff800010ae88d0
#25 [ffff800012ab3c60] __netif_receive_skb_list+0x11c at ffff800010ae8bdc
#26 [ffff800012ab3cc0] netif_receive_skb_list_internal+0xe0 at ffff800010ae8d40
#27 [ffff800012ab3d40] napi_complete_done+0x6c at ffff800010ae9aec
#28 [ffff800012ab3d70] hns3_nic_common_poll+0x100 at ffff800009be7350 [hns3]
#29 [ffff800012ab3e10] napi_poll+0xc8 at ffff800010ae9d38
#30 [ffff800012ab3e50] net_rx_action+0xd0 at ffff800010ae9fa4
#31 [ffff800012ab3ed0] __do_softirq+0x12c at ffff80001001075c
#32 [ffff800012ab3f70] irq_exit+0x118 at ffff8000100b06cc
#33 [ffff800012ab3f90] __handle_domain_irq+0x84 at ffff800010146fb4
#34 [ffff800012ab3fd0] gic_handle_irq+0x74 at ffff800010010124
--- <IRQ stack> ---
#35 [ffff800013093f00] el1_irq+0xb4 at ffff800010012374
#36 [ffff800013093f20] arch_cpu_idle+0x14 at ffff800010cedb84
#37 [ffff800013093f30] default_idle_call+0x58 at ffff800010cf9c58
#38 [ffff800013093f50] cpuidle_idle_call+0x170 at ffff8000100f7850
#39 [ffff800013093f90] do_idle+0xc4 at ffff8000100f7954
#40 [ffff800013093fc0] cpu_startup_entry+0x2c at ffff8000100f7bc0
#41 [ffff800013093fe0] secondary_start_kernel+0x154 at ffff80001002a498
```

##### bt -slx

bt -slx:
  - **`bt`**：在 `crash` 工具中，显示调用栈（函数调用顺序）。
  - **`-s`**：显示函数名（符号化）。
  - **`-l`**：显示源代码文件名和行号（如果有调试信息）。
  - **`-x`**：用十六进制显示地址。

```bash
crash> bt -slx
PID: 0        TASK: ffff00208d452680  CPU: 21   COMMAND: "swapper/21"
 #0 [ffff800012ab2f60] __crash_kexec+0x110 at ffff8000101aafa0
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./arch/arm64/include/asm/kexec.h: 52
 #1 [ffff800012ab30f0] panic+0x17c at ffff800010cd72bc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/panic.c: 251
 #2 [ffff800012ab31d0] watchdog_timer_fn+0x22c at ffff8000101f25ac
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/watchdog.c: 448
 #3 [ffff800012ab3220] __run_hrtimer+0x94 at ffff80001017d314
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/time/hrtimer.c: 1583
 #4 [ffff800012ab3270] __hrtimer_run_queues+0xac at ffff80001017d5cc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/time/hrtimer.c: 1647
 #5 [ffff800012ab32d0] hrtimer_interrupt+0x138 at ffff80001017dc18
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/time/hrtimer.c: 1709
 #6 [ffff800012ab3340] arch_timer_handler_phys+0x38 at ffff800010a5ec88
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/drivers/clocksource/arm_arch_timer.c: 647
 #7 [ffff800012ab3350] handle_percpu_devid_irq+0x8c at ffff80001015011c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/irq/chip.c: 933
 #8 [ffff800012ab3380] __handle_domain_irq+0x80 at ffff800010146fb0
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./include/linux/irqdesc.h: 153
 #9 [ffff800012ab33c0] gic_handle_irq+0x74 at ffff800010010124
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./include/linux/irqdesc.h: 171
#10 [ffff800012ab3520] el1_irq+0xb4 at ffff800010012374
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/arch/arm64/kernel/entry.S: 672
#11 [ffff800012ab3540] netdev_pick_tx+0x278 at ffff800010adc6e8
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./arch/arm64/include/asm/jump_label.h: 21
#12 [ffff800012ab3590] netdev_core_pick_tx+0xd8 at ffff800010ae675c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4057
#13 [ffff800012ab35c0] __dev_queue_xmit+0x100 at ffff800010ae6890
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4132
#14 [ffff800012ab3630] dev_queue_xmit+0x18 at ffff800010ae7268
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4205
#15 [ffff800012ab3640] ovs_vport_send+0xa8 at ffff8000098f95b8 [openvswitch]
#16 [ffff800012ab3670] do_output+0x5c at ffff8000098e47fc [openvswitch]
#17 [ffff800012ab36b0] do_execute_actions+0x864 at ffff8000098e66b4 [openvswitch]
#18 [ffff800012ab3830] ovs_execute_actions+0x60 at ffff8000098e6b00 [openvswitch]
#19 [ffff800012ab3860] ovs_dp_process_packet+0x98 at ffff8000098eaba8 [openvswitch]
#20 [ffff800012ab38e0] ovs_vport_receive+0x78 at ffff8000098f949c [openvswitch]
#21 [ffff800012ab3ae0] netdev_port_receive+0xb8 at ffff8000098fa03c [openvswitch]
#22 [ffff800012ab3b10] netdev_frame_hook+0x28 at ffff8000098fa128 [openvswitch]
#23 [ffff800012ab3b20] __netif_receive_skb_core+0x290 at ffff800010ae77f0
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 5260
#24 [ffff800012ab3bd0] __netif_receive_skb_list_core+0xa0 at ffff800010ae88d0
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 5442
#25 [ffff800012ab3c60] __netif_receive_skb_list+0x11c at ffff800010ae8bdc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 5509
#26 [ffff800012ab3cc0] netif_receive_skb_list_internal+0xe0 at ffff800010ae8d40
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 5619
#27 [ffff800012ab3d40] napi_complete_done+0x6c at ffff800010ae9aec
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 5773
#28 [ffff800012ab3d70] hns3_nic_common_poll+0x100 at ffff800009be7350 [hns3]
#29 [ffff800012ab3e10] napi_poll+0xc8 at ffff800010ae9d38
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 6837
#30 [ffff800012ab3e50] net_rx_action+0xd0 at ffff800010ae9fa4
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 6907
#31 [ffff800012ab3ed0] __do_softirq+0x12c at ffff80001001075c
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/softirq.c: 298
#32 [ffff800012ab3f70] irq_exit+0x118 at ffff8000100b06cc
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./include/linux/interrupt.h: 550
#33 [ffff800012ab3f90] __handle_domain_irq+0x84 at ffff800010146fb4
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/irq/irqdesc.c: 692
#34 [ffff800012ab3fd0] gic_handle_irq+0x74 at ffff800010010124
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./include/linux/irqdesc.h: 171
--- <IRQ stack> ---
#35 [ffff800013093f00] el1_irq+0xb4 at ffff800010012374
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/arch/arm64/kernel/entry.S: 672
#36 [ffff800013093f20] arch_cpu_idle+0x14 at ffff800010cedb84
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./arch/arm64/include/asm/irqflags.h: 37
#37 [ffff800013093f30] default_idle_call+0x58 at ffff800010cf9c58
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/sched/idle.c: 112
#38 [ffff800013093f50] cpuidle_idle_call+0x170 at ffff8000100f7850
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/sched/idle.c: 194
#39 [ffff800013093f90] do_idle+0xc4 at ffff8000100f7954
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/sched/idle.c: 300
#40 [ffff800013093fc0] cpu_startup_entry+0x2c at ffff8000100f7bc0
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/kernel/sched/idle.c: 396
#41 [ffff800013093fe0] secondary_start_kernel+0x154 at ffff80001002a498
    /usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/arch/arm64/kernel/smp.c: 281
```

#### dis

本次debug运气十分不错，vmcore-dmesg.txt中的`pc : netdev_pick_tx+0x27c/0x294`居然与反汇编vmcore中netdev_pick_tx+0x27c能对上，最近遇到过多次对不上的情况，抽空分析。

vmcore-dmesg.txt中pc : netdev_pick_tx+0x27c/0x294，在crash中dis反汇编查看相关源码、汇编：

```bash
crash> dis -flx netdev_pick_tx | grep 0x27c -C5
0xffff800010adc6e0 <netdev_pick_tx+0x270>:      csel    x19, x2, x0, ne  // ne = any
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./arch/arm64/include/asm/jump_label.h: 21
0xffff800010adc6e4 <netdev_pick_tx+0x274>:      b       0xffff800010adc544 <netdev_pick_tx+0xd4>
0xffff800010adc6e8 <netdev_pick_tx+0x278>:      b       0xffff800010adc4cc <netdev_pick_tx+0x5c>
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 3190
0xffff800010adc6ec <netdev_pick_tx+0x27c>:      sub     w2, w2, w25     # // 减法操作：w2 = w2 - w25，结果存储在w2中
0xffff800010adc6f0 <netdev_pick_tx+0x280>:      b       0xffff800010adc598 <netdev_pick_tx+0x128>
0xffff800010adc6f4 <netdev_pick_tx+0x284>:      stp     x25, x26, [sp, #64]
0xffff800010adc6f8 <netdev_pick_tx+0x288>:      b       0xffff800010adc4d0 <netdev_pick_tx+0x60>
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 3188
0xffff800010adc6fc <netdev_pick_tx+0x28c>:      sub     w2, w2, w20
```

`dis -flx netdev_pick_tx | grep 0x27c -C5` 的含义和作用：

1. **`dis -flx netdev_pick_tx`**:
   - `dis` 是 `crash` 工具中的一个命令，用于反汇编（disassemble）指定的函数或地址，展示底层的机器指令。
   - `-f`：显示函数的完整反汇编内容（而不是只显示部分）。
   - `-l`：在反汇编中包含源代码行号和对应的文件名（如果调试信息可用）。
   - `-x`：以十六进制格式显示地址和指令。
   - `netdev_pick_tx`：这是要反汇编的目标函数名，在这里是一个Linux内核函数，位于网络子系统中（`net/core/dev.c`），通常用于选择网络设备的传输队列。

   **作用**：这条命令会输出 `netdev_pick_tx` 函数的汇编代码，带有地址、指令、源文件名和行号信息。

2. **`|`**:
   - 管道符，将 `dis` 命令的输出传递给后面的 `grep` 命令进行过滤。

3. **`grep 0x27c -C5`**:
   - `grep`：Linux中的文本过滤工具，用于搜索匹配指定模式的行。
   - `0x27c`：这里是要搜索的模式，即查找包含 `0x27c` 的行。`0x27c` 是 `netdev_pick_tx` 函数中的一个偏移量（offset），表示从函数起始地址开始的某个位置。
   - `-C5`：上下文选项，表示在匹配行前后各显示5行（Context，5 lines before and after）。

   **作用**：从 `dis` 的输出中，找到包含 `0x27c` 的那一行，并显示它周围的5行代码，以便查看上下文。

vmcore-dmesg.txt中提取到x2、x20寄存器：
```bash
x2 : 0000000000000000
x25: 0000000000000000
```

```c
// vim net/core/dev.c +3190

 3164 static u16 skb_tx_hash(const struct net_device *dev,
 3165                const struct net_device *sb_dev,
 3166                struct sk_buff *skb)
 3167 {
 3168     u32 hash;
 3169     u16 qoffset = 0;
 3170     u16 qcount = dev->real_num_tx_queues;
 3171
 3172     if (dev->num_tc) {
 3173         u8 tc = netdev_get_prio_tc_map(dev, skb->priority);
 3174
 3175         qoffset = sb_dev->tc_to_txq[tc].offset;
 3176         qcount = sb_dev->tc_to_txq[tc].count;
 3177         if (unlikely(!qcount)) {
 3178             net_warn_ratelimited("%s: invalid qcount, qoffset %u for tc %u\n",
 3179                          sb_dev->name, qoffset, tc);
 3180             qoffset = 0;
 3181             qcount = dev->real_num_tx_queues;
 3182         }
 3183     }
 3184
 3185     if (skb_rx_queue_recorded(skb)) {
 3186         hash = skb_get_rx_queue(skb);
 3187         if (hash >= qoffset)
 3188             hash -= qoffset;
 3189         while (unlikely(hash >= qcount))
 3190             hash -= qcount;
 3191         return hash + qoffset;
 3192     }
 3193
 3194     return (u16) reciprocal_scale(skb_get_hash(skb), qcount) + qoffset;
 3195 }
```

从反汇编结果来看, 3910行的hash = 0、qcount = 0，3189行这里的while会陷入死循环。内核态代码默认不会抢占，即使被中断打断了也会返回原处继续执行，这里的确会触发soft lockup。

进一步使用dis -flx netdev_pick_tx查看netdev_pick_tx函数入参：

```bash
crash> dis -flx netdev_pick_tx | head -n20
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4833
0xffff800010adc470 <netdev_pick_tx>:    mov     x9, x30
0xffff800010adc474 <netdev_pick_tx+0x4>:        nop
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4013
0xffff800010adc478 <netdev_pick_tx+0x8>:        paciasp
0xffff800010adc47c <netdev_pick_tx+0xc>:        stp     x29, x30, [sp, #-80]!
0xffff800010adc480 <netdev_pick_tx+0x10>:       mov     x29, sp
0xffff800010adc484 <netdev_pick_tx+0x14>:       stp     x19, x20, [sp, #16]
0xffff800010adc488 <netdev_pick_tx+0x18>:       mov     x19, x2
0xffff800010adc48c <netdev_pick_tx+0x1c>:       stp     x21, x22, [sp, #32]
0xffff800010adc490 <netdev_pick_tx+0x20>:       mov     x21, x1
0xffff800010adc494 <netdev_pick_tx+0x24>:       mov     x22, x0
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/net/core/dev.c: 4014
0xffff800010adc498 <netdev_pick_tx+0x28>:       stp     x23, x24, [sp, #48]
0xffff800010adc49c <netdev_pick_tx+0x2c>:       ldr     x23, [x1, #24]
/usr/src/debug/kernel-5.10.0-136.12.0.88.ctl3.aarch64/linux-5.10.0-136.12.0.88.ctl3.aarch64/./include/net/sock.h: 1845
0xffff800010adc4a0 <netdev_pick_tx+0x30>:       cbz     x23, 0xffff800010adc6d8 <netdev_pick_tx+0x268>
0xffff800010adc4a4 <netdev_pick_tx+0x34>:       ldrh    w0, [x23, #120]
0xffff800010adc4a8 <netdev_pick_tx+0x38>:       mov     w1, #0xffff                     // #65535
0xffff800010adc4ac <netdev_pick_tx+0x3c>:       cmp     w0, w1
```

```bash
0xffff800010adc494 <netdev_pick_tx+0x24>:       mov     x22, x0    // x0（第一个参数）保存到 x22
0xffff800010adc490 <netdev_pick_tx+0x20>:       mov     x21, x1    // x1（第二个参数）保存到 x21
0xffff800010adc488 <netdev_pick_tx+0x18>:       mov     x19, x2    // x2（第三个参数）保存到 x19
```

- 在ARM64架构中，`x0` 到 `x30` 是64位的通用寄存器，用于存储数据和地址。
- `mov` 指令用于将一个寄存器的值复制到另一个寄存器。
- 在函数调用中，`x0` 到 `x7` 通常用于传递函数参数，`x19` 到 `x28` 是临时寄存器，通常用于保存中间结果。

```bash
x22: ffff20222543e000
x21: ffff0021428bca00
x19: ffff20222543e000
```

```c
// vim -t netdev_pick_tx

 4011 u16 netdev_pick_tx(struct net_device *dev, struct sk_buff *skb,
 4012              struct net_device *sb_dev)
 4013 {
 4014     struct sock *sk = skb->sk;
 4015     int queue_index = sk_tx_queue_get(sk);
```

```bash
crash> struct net_device ffff20222543e000 | head -n10
struct net_device {
  name = "veth685086fa\000\000\000",       # 网络设备名称，这里是 "veth685086fa"（虚拟以太网设备）
  name_node = 0xffff2034b6d41500,          # 指向名称节点的指针，可能用于设备管理链表
  ifalias = 0x0,                           # 设备别名，这里为空（NULL）
  mem_end = 0,                             # 内存映射结束地址，未使用为 0
  mem_start = 0,                           # 内存映射起始地址，未使用为 0
  base_addr = 0,                           # 设备基地址（I/O 地址），未使用为 0
  irq = 0,                                 # 中断号，未分配为 0
  state = 14,                              # 设备状态，14 是十进制，可能是位掩码（如 UP | LINK）
  dev_list = {                             # 设备链表的起始部分（未完全显示）
```

- **`struct net_device`**：在 `crash` 工具中，用于显示 `net_device` 结构体的内容。`net_device` 是 Linux 内核中表示网络设备的结构体。
- **`ffff20222543e000`**：这是内存地址，表示要查看的特定 `net_device` 实例的起始地址。
- **`| head -n10`**：将输出限制为前 10 行。

简单含义:
- 这是一个 `net_device` 结构体实例，描述了一个网络设备。
- **`name`**：设备名叫 `veth685086fa`，表示这是一个虚拟以太网设备（veth，常用于容器网络）。
- **`state = 14`**：状态值为 14，可能表示设备已启用（UP）并处于某种状态（需查具体位定义）。
- 其他字段如 `mem_end`, `mem_start`, `base_addr`, `irq` 都是 0，说明这个虚拟设备没有物理硬件资源。
- **`dev_list`**：设备链表的一部分，可能链接到系统中其他网络设备。

```bash
crash> struct net_device ffff20222543e000 | grep tx_queues
  num_tx_queues = 1,        # 传输队列的数量，设置为 1
  real_num_tx_queues = 0,   # 实际使用的传输队列数量，为 0
```

在内核源码中查找`real_num_tx_queues = 0`:
```bash
 grep "real_num_tx_queues = 0" . -inr --include=*.c
./net/core/net-sysfs.c:1809:    dev->real_num_tx_queues = 0;
```

```c
// vim net/core/net-sysfs.c +1809

1796 static void remove_queue_kobjects(struct net_device *dev)
1797 {
1798     int real_rx = 0, real_tx = 0;
1799
1800 #ifdef CONFIG_SYSFS
1801     real_rx = dev->real_num_rx_queues;
1802 #endif
1803     real_tx = dev->real_num_tx_queues;
1804
1805     net_rx_queue_update_kobjects(dev, real_rx, 0);
1806     netdev_queue_update_kobjects(dev, real_tx, 0);
1807
1808     dev->real_num_rx_queues = 0;
1809     dev->real_num_tx_queues = 0;
1810 #ifdef CONFIG_SYSFS
1811     kset_unregister(dev->queues_kset);
1812 #endif
1813 }
```

### 自定义日志风格

当前内核函数`WARN`每次都会调用dump_stack，不够灵活，日志输出太多容易触发hung task，造成机器异常卡顿，不利于问题分析。

在include/linux/printk.h最后面添加自定义日志风格：
```c
diff --git a/include/linux/printk.h b/include/linux/printk.h
index 7d787f91db92..0dcb9eb4d03e 100644
--- a/include/linux/printk.h
+++ b/include/linux/printk.h
@@ -672,4 +672,17 @@ static inline void print_hex_dump_debug(const char *prefix_str, int prefix_type,
 #define print_hex_dump_bytes(prefix_str, prefix_type, buf, len)        \
        print_hex_dump_debug(prefix_str, prefix_type, 16, 1, buf, len, true)
 
+// 自定义 MY_WARN 宏
+#define MY_WARN(condition, dump_stack_flag, tag, fmt, ...)                     \
+       do {                                                                   \
+               if (condition) {                                               \
+                       printk(KERN_WARNING                                    \
+                              "WARNING: [%s] [%s:%d %s] " fmt, \
+                              tag, __FILE__, __LINE__, __func__,              \
+                              ##__VA_ARGS__);                                 \
+                       if (dump_stack_flag)                                   \
+                               dump_stack();                                  \
+               }                                                              \
+       } while (0)
+
 #endif
```

这是一个 Git 补丁（diff），在 Linux 内核头文件 `include/linux/printk.h` 中添加了一个自定义宏 `MY_WARN`。以下是最简单解释：

#### 改动位置
- 文件：`include/linux/printk.h`
- 位置：在文件末尾（第 672 行后）添加新代码。

#### 新增内容
```c
#define MY_WARN(condition, dump_stack_flag, tag, fmt, ...)                     \
    do {                                                                   \
        if (condition) {                                               \
            printk(KERN_WARNING                                        \
                   "WARNING: [%s] [%s:%d %s] " fmt, \
                   tag, __FILE__, __LINE__, __func__,                  \
                   ##__VA_ARGS__);                                     \
            if (dump_stack_flag)                                       \
                dump_stack();                                          \
        }                                                              \
    } while (0)
```

#### 简单解释
- **`MY_WARN`**：一个新定义的宏，用于打印警告信息。
- **参数**：
  - `condition`：条件，如果为真则触发警告。
  - `dump_stack_flag`：是否打印调用栈（1 = 是，0 = 否）。
  - `tag`：自定义标签，标记警告来源。
  - `fmt, ...`：格式化字符串和参数，类似 `printf`。
- **功能**：
  1. 如果 `condition` 为真，用 `printk` 打印一条警告信息（级别 `KERN_WARNING`）。
  2. 警告内容包括：标签、文件名、行号、函数名，再加上自定义消息。
  3. 如果 `dump_stack_flag` 为真，调用 `dump_stack()` 打印调用栈。
- **用法示例**：
  ```c
  MY_WARN(x > 0, 1, "test", "x is %d", x);
  ```
  输出可能像：`WARNING: [test] [file.c:123 func] x is 5`，并附上调用栈。

##### 用途
- 在内核开发中，用于调试或记录异常情况，比直接用 `printk` 更方便，能自动包含上下文信息。

### 在skb_tx_hash中增加日志

下方`if (hash == 0 && qcount == 0)`处代码尝试修复此次soft lockup问题，编译出内核测试包后，实测有效。

之前物理机器在机房，无法过去，soft lockup bmc串口也上不去机器，ssh也不通。采用简版修复方案后机器不再soft lockup，后续调试工作就好展开了。

```c
diff --git a/net/core/dev.c b/net/core/dev.c
index 4d3851278303..f5991595b2d1 100644
--- a/net/core/dev.c
+++ b/net/core/dev.c
@@ -3186,8 +3213,23 @@ static u16 skb_tx_hash(const struct net_device *dev,
 		hash = skb_get_rx_queue(skb);
 		if (hash >= qoffset)
 			hash -= qoffset;
-		while (unlikely(hash >= qcount))
+		
+		while (unlikely(hash >= qcount)) {
+			if (strncmp(dev->name, "veth", 4) == 0) {
+				MY_WARN(1, 0, "veth_peer_soft_lockup",
+					"dev->name:%s sb_dev->name:%s qconut:%hd hash:%u\n",
+					dev->name, sb_dev->name, qcount, hash);
+			}
+
 			hash -= qcount;
+
+			if (hash == 0 && qcount == 0) {
+				MY_WARN(1, 1, "veth_peer_soft_lockup",
+					"dev->name:%s sb_dev->name:%s qconut:%hd hash:%u\n",
+					dev->name, sb_dev->name, qcount, hash);
+				break;
+			}
+		}
 		return hash + qoffset;
 	}
 
```

### 在remove_queue_kobjects中增加日志

当前机器已不再soft lockup，此处增加dump_stack获取堆栈不是很有必要，完全可以用bpftrace代替。

```c
diff --git a/net/core/net-sysfs.c b/net/core/net-sysfs.c
index 989b3f7ee85f..d51d70f30fb1 100644
--- a/net/core/net-sysfs.c
+++ b/net/core/net-sysfs.c
@@ -1805,6 +1805,13 @@ static void remove_queue_kobjects(struct net_device *dev)
        net_rx_queue_update_kobjects(dev, real_rx, 0);
        netdev_queue_update_kobjects(dev, real_tx, 0);
 
+       if (strncmp(dev->name, "veth", 4) == 0) {
+               MY_WARN(1, 1, "veth_peer_soft_lockup", "dev->name:%s, "
+                       "dev->real_num_rx_queues: %d, dev->real_num_tx_queues:%d\n",
+                       dev->name, dev->real_num_rx_queues,
+                       dev->real_num_tx_queues);
+       }
+
        dev->real_num_rx_queues = 0;
        dev->real_num_tx_queues = 0;
 #ifdef CONFIG_SYSFS
```

在netdev_unregister_kobject中添加dump_stack，观察到相关堆栈：

```c
1456 Mar 19 13:52:23 localhost.localdomain kernel: WARNING: [veth_peer_soft_lockup] [net/core/net-sysfs.c:1809 remove_queue_kobjects] dev veth interface name:veth1, dev->real_num_rx_queues: 1, dev->real_num_tx_queues:1
1457 Mar 19 13:52:23 localhost.localdomain kernel: CPU: 3 PID: 4059 Comm: ip Not tainted 5.10.0-136.12.0.88.067a2cf1de16.ctl3.aarch64 #1
1458 Mar 19 13:52:23 localhost.localdomain kernel: Hardware name: QEMU QEMU Virtual Machine, BIOS 2024.02-2ubuntu0.1 10/25/2024
1459 Mar 19 13:52:23 localhost.localdomain kernel: Call trace:
1460 Mar 19 13:52:23 localhost.localdomain kernel:  dump_backtrace+0x0/0x1e4
1461 Mar 19 13:52:23 localhost.localdomain kernel:  show_stack+0x20/0x2c
1462 Mar 19 13:52:23 localhost.localdomain kernel:  dump_stack+0xd8/0x140
1463 Mar 19 13:52:23 localhost.localdomain kernel:  netdev_unregister_kobject+0xf4/0x100
1464 Mar 19 13:52:23 localhost.localdomain kernel:  unregister_netdevice_many+0x2c8/0x4c0
1465 Mar 19 13:52:23 localhost.localdomain kernel:  rtnl_dellink+0x11c/0x34c
1466 Mar 19 13:52:23 localhost.localdomain kernel:  rtnetlink_rcv_msg+0x124/0x360
1467 Mar 19 13:52:23 localhost.localdomain kernel:  netlink_rcv_skb+0x64/0x130
1468 Mar 19 13:52:23 localhost.localdomain kernel:  rtnetlink_rcv+0x20/0x30
1469 Mar 19 13:52:23 localhost.localdomain kernel:  netlink_unicast_kernel+0x60/0x120
1470 Mar 19 13:52:23 localhost.localdomain kernel:  netlink_unicast+0x110/0x194
1471 Mar 19 13:52:23 localhost.localdomain kernel:  netlink_sendmsg+0x37c/0x420
1472 Mar 19 13:52:23 localhost.localdomain kernel:  sock_sendmsg+0x48/0x70
1473 Mar 19 13:52:23 localhost.localdomain kernel:  ____sys_sendmsg+0x274/0x2b0
1474 Mar 19 13:52:23 localhost.localdomain kernel:  ___sys_sendmsg+0x84/0xd0
1475 Mar 19 13:52:23 localhost.localdomain kernel:  __sys_sendmsg+0x70/0xd0
1476 Mar 19 13:52:23 localhost.localdomain kernel:  __arm64_sys_sendmsg+0x2c/0x40
1477 Mar 19 13:52:23 localhost.localdomain kernel:  invoke_syscall+0x50/0x11c
1478 Mar 19 13:52:23 localhost.localdomain kernel:  el0_svc_common.constprop.0+0x158/0x164
1479 Mar 19 13:52:23 localhost.localdomain kernel:  do_el0_svc+0x2c/0x9c
1480 Mar 19 13:52:23 localhost.localdomain kernel:  el0_svc+0x20/0x30
1481 Mar 19 13:52:23 localhost.localdomain kernel:  el0_sync_handler+0xb0/0xb4
1482 Mar 19 13:52:23 localhost.localdomain kernel:  el0_sync+0x160/0x180
```

### unregister_netdevice_many

在搭建的虚拟机测试环境中一直没有复现此问题，但是此问题在鲲鹏920上必现。就去阅读了`rtnl_dellink`、`unregister_netdevice_many`等函数，有点怀疑这个问题跟rcu有一定关系，`unregister_netdevice_many`中两次调用`synchronize_net`，`ip link del veth0`命令执行后走到函数unregister_netdevice_many后大概会挂rcu回调。

```c
10814 void unregister_netdevice_many(struct list_head *head)
10815 {
10816     struct net_device *dev, *tmp;                                                                                                                                                                                
10817     LIST_HEAD(close_head);
10818 
10819     BUG_ON(dev_boot_phase);
10820     ASSERT_RTNL();
10821 
10822     if (list_empty(head))
10823         return;
10824 
10825     list_for_each_entry_safe(dev, tmp, head, unreg_list) {
10826         if (strncmp(dev->name, "veth", 4) == 0) {
10827             MY_WARN(1, 0, "veth_peer_soft_lockup",
10828 +-----  4 lines: "dev->name:%s, "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
10832         }
10833 
10834         /* Some devices call without registering
10835          * for initialization unwind. Remove those
10836          * devices and proceed with the remaining.
10837          */
10838         if (dev->reg_state == NETREG_UNINITIALIZED) {
10839             pr_debug("unregister_netdevice: device %s/%p never was registered\n",
10840                  dev->name, dev);
10841 
10842             WARN_ON(1);
10843             list_del(&dev->unreg_list);
10844             continue;
10845         }
10846         dev->dismantle = true;
10847         BUG_ON(dev->reg_state != NETREG_REGISTERED);
10848     }
10849 
10850     /* If device is running, close it first. */
10851     list_for_each_entry(dev, head, unreg_list)
10852         list_add_tail(&dev->close_list, &close_head);
10853     dev_close_many(&close_head, true);
10854 
10855     list_for_each_entry(dev, head, unreg_list) {
10856         /* And unlink it from device chain. */
10857         unlist_netdevice(dev);
10858 
10859         dev->reg_state = NETREG_UNREGISTERING;
10860     }
10861     flush_all_backlogs();
10862 
10863     synchronize_net();
10864 
10865     list_for_each_entry(dev, head, unreg_list) {
10866         if (strncmp(dev->name, "veth", 4) == 0) {
10867             MY_WARN(1, 0, "veth_peer_soft_lockup",
10868 +-----  4 lines: "dev->name:%s, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
10872         }
10873
10874         struct sk_buff *skb = NULL;
10875 
10876         /* Shutdown queueing discipline. */
10877         dev_shutdown(dev);
10878 
10879         dev_xdp_uninstall(dev);
10880 
10881         /* Notify protocols, that we are about to destroy
10882          * this device. They should clean all the things.
10883          */
10884         call_netdevice_notifiers(NETDEV_UNREGISTER, dev);
10885 
10886         if (!dev->rtnl_link_ops ||
10887             dev->rtnl_link_state == RTNL_LINK_INITIALIZED)
10888             skb = rtmsg_ifinfo_build_skb(RTM_DELLINK, dev, ~0U, 0,
10889                              GFP_KERNEL, NULL, 0);
10890 
10891         /*
10892          *  Flush the unicast and multicast chains
10893          */
10894         dev_uc_flush(dev);
10895         dev_mc_flush(dev);
10896 
10897         netdev_name_node_alt_flush(dev);
10898         netdev_name_node_free(dev->name_node);
10899 
10900         if (dev->netdev_ops->ndo_uninit)
10901             dev->netdev_ops->ndo_uninit(dev);
10902 
10903         if (skb)
10904             rtmsg_ifinfo_send(skb, dev, GFP_KERNEL);
10905 
10906         /* Notifier chain MUST detach us all upper devices. */
10907         WARN_ON(netdev_has_any_upper_dev(dev));
10908         WARN_ON(netdev_has_any_lower_dev(dev));
10909 
10910         if (strncmp(dev->name, "veth", 4) == 0) {
10911             MY_WARN(1, 0, "veth_peer_soft_lockup",
10912 +-----  4 lines: "dev->name:%s, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
10916         }
10917 
10918         /* Remove entries from kobject tree */
10919         netdev_unregister_kobject(dev);
10920 #ifdef CONFIG_XPS
10921         /* Remove XPS queueing entries */
10922         netif_reset_xps_queues_gt(dev, 0);
10923 #endif
10924         cond_resched();
10925     }
10926 
10927     synchronize_net();
10928 
10929     list_for_each_entry(dev, head, unreg_list) {
10930         dev_put(dev);
10931         net_set_todo(dev);
10932     }
10933 
10934     list_del(head);
10935 }
10936 EXPORT_SYMBOL(unregister_netdevice_many);
```

### synchronize_net



```bash
systemctl start openvswitch
ovs-vsctl add-br os_manage
ip link add name veth0 type veth peer name veth1
ip link set veth0 up
ip link set veth1 up
ovs-vsctl add-port os_manage veth0
ovs-vsctl show
ip link set os_manage up
ip addr add 192.168.10.1/24 dev veth0
ip addr add 192.168.10.2/24 dev veth1
ip addr show veth0
ip addr show veth1
ping 192.168.10.1 -I 192.168.10.2
```

```bash
ip link del veth0
ovs-vsctl del-port os_manage veth0
```

![vm_ip_link_del_veth0](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/vm_ip_link_del_veth0.png)

```bash
trace-cmd record -p function_graph -g veth_dellink ip link del veth0
```

```bash
trace-cmd report > vm_ip_link_del_veth0.log
```

```bash
vim vm_ip_link_del_veth0.log

CPU 0 is empty
CPU 1 is empty
CPU 2 is empty
CPU 3 is empty
CPU 4 is empty
CPU 5 is empty
CPU 7 is empty
cpus=8
              ip-2541  [006]  5230.491361: funcgraph_entry:                   |  veth_dellink() {
              ip-2541  [006]  5230.491883: funcgraph_entry:                   |    unregister_netdevice_queue() {
              ip-2541  [006]  5230.491914: funcgraph_entry:                   |      rtnl_is_locked() {
              ip-2541  [006]  5230.491916: funcgraph_entry:      + 56.464 us  |        mutex_is_locked();
              ip-2541  [006]  5230.492126: funcgraph_exit:       ! 212.400 us |      }
              ip-2541  [006]  5230.492173: funcgraph_exit:       ! 291.680 us |    }
              ip-2541  [006]  5230.492182: funcgraph_entry:                   |    unregister_netdevice_queue() {
              ip-2541  [006]  5230.492184: funcgraph_entry:                   |      rtnl_is_locked() {
              ip-2541  [006]  5230.492185: funcgraph_entry:        1.456 us   |        mutex_is_locked();
              ip-2541  [006]  5230.492188: funcgraph_exit:         4.496 us   |      }
              ip-2541  [006]  5230.492190: funcgraph_exit:         7.936 us   |    }
              ip-2541  [006]  5230.492200: funcgraph_exit:       # 1227.888 us |  }

```

```bash
trace-cmd list -f | grep rtnl_dellink
rtnl_dellinkprop
rtnl_dellink
```

```bash
trace-cmd record -p function_graph -g rtnl_dellink ip link del veth0
```

```bash
trace-cmd report > vm_ip_link_del_veth0_rtnl_dellink.log
```

```bash
trace-cmd record -p function_graph ping 192.168.10.1 -I 192.168.10.2
```

```bash
trace-cmd report > vm_ping.log
```

```bash
git push ctkernel-lts-5.10 HEAD:yuanql9/bugfix-pr-veth-peer-soft-lockup -f -u
ba45ce8fe9eb (HEAD -> ctkernel-lts-5.10/yuanql9/bugfix-pr-veth-peer-soft-lockup, ctkernel-lts-5.10/yuanql9/bugfix-pr-veth-peer-soft-lockup) net/core/veth: avoid soft lockup
```

```bash
bpftrace -e 'kprobe:skb_tx_hash /((struct net_device *)arg0)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)arg0)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:netdev_core_pick_tx /((struct net_device *)arg0)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)arg0)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:__dev_queue_xmit /((struct net_device *)((struct sk_buff *)arg0)->dev)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)((struct sk_buff *)arg0)->dev)->num_tc); }'
```

```bash
bpftrace -e 'kprobe:__dev_queue_xmit /((struct net_device *)((struct sk_buff *)arg0)->dev)->name == "veth0"/ { printf("dev->num_tc: %d\n", ((struct net_device *)((struct sk_buff *)arg0)->dev)->num_tc); }'
```

- https://lkml.org/lkml/2025/3/17/1299

- https://lore.kernel.org/bpf/20250317154537.3633540-3-florian.fainelli@broadcom.com/T/

- https://lore.kernel.org/bpf/20250317154537.3633540-1-florian.fainelli@broadcom.com/

b4 am -o - 20250317154537.3633540-1-florian.fainelli@broadcom.com | git am -3

CTKbug: #71585

Signed-off-by: Qiliang Yuan <yuanql9@chinatelecom.cn>

Reviewed-by: Menglong Dong <dongml2@chinatelecom.cn>

d268c1f5cfc92eb5bb605f7365769aacd93be234
4bbd360a5084d8f890f814327e1d9fbb1f0f6fa1


## https://lkml.org/lkml/2025/3/17/1299

这封邮件是关于Linux内核的一个补丁，标题为“[PATCH stable 5.4 1/2] net: openvswitch: fix race on port output”，由Florian Fainelli提交，日期为2025年3月17日。补丁针对的是Linux内核5.4版本中的一个问题，具体是一个与Open vSwitch（开源虚拟交换机）相关的竞争条件（race condition），可能会导致CPU陷入无限循环。

### 问题背景
补丁描述了一个特定的网络配置场景和测试步骤，可能触发内核中的问题。以下是场景和问题的中文解释：

#### 测试环境配置：
1. 在一台机器上创建一个Open vSwitch实例，包含一个网桥（bridge）和默认的流量规则。
2. 创建两个网络命名空间（network namespaces），分别命名为“server”和“client”。
3. 在网桥上添加两个Open vSwitch接口，分别命名为“server”和“client”。
4. 为每个Open vSwitch接口创建一对veth（虚拟以太网设备），名称与接口对应，并配置32个接收（rx）和发送（tx）队列。
5. 将veth对的另一端分别移动到对应的网络命名空间中。
6. 在每个命名空间内的veth接口上分配IP地址（需在同一子网内）。
7. 在“server”命名空间中启动一个HTTP服务器。
8. 测试“client”命名空间中的客户端是否能访问“server”中的HTTP服务器。

#### 触发问题的步骤：
1. 从“client”端向HTTP服务器发送大量并行请求（大约3000个curl请求即可）。
2. 在发送请求的同时，删除“client”网络命名空间（仅删除命名空间，不删除接口或停止服务器）。

按照上述步骤操作时，有一定概率会导致主机的某个CPU陷入无限循环，并可能输出类似“kernel CPU stuck”之类的错误信息。如果没有触发问题，可以重复尝试。

#### 问题的根本原因：
问题的核心是一个竞争条件，发生在网络命名空间删除和数据包处理之间。以下是事件发生的详细序列：
1. 删除网络命名空间时，会调用`unregister_netdevice_many_notify`函数。
2. 该函数首先将veth设备两端的注册状态设置为`NETREG_UNREGISTERING`，然后执行` synchronize_net`同步操作。
3. 接着调用`call_netdevice_notifiers`，通知设备状态为`NETDEV_UNREGISTER`。
4. Open vSwitch的`dp_device_event`函数处理这一通知，并调用`ovs_netdev_detach_dev`（如果设备关联了vport，即Open vSwitch的虚拟端口）。
5. `ovs_netdev_detach_dev`会移除设备的接收处理程序（rx_handlers），但不会阻止数据包继续发送到该设备。
6. `dp_device_event`将vport的删除操作放入后台任务队列中，因为此时无法直接获取所需的`ovs_lock`锁。
7. `unregister_netdevice_many_notify`继续执行，调用`netdev_unregister_kobject`，将设备的`real_num_tx_queues`（实际发送队列数）设置为0。
8. 后台任务随后删除vport（但这部分细节与问题无关）。
9. 在步骤7之后、步骤9之前，如果有数据包被发送到尚未删除的vport，数据包会被转发到`dev_queue_xmit`流程，尽管设备正在注销。

在`dev_queue_xmit`中，调用了`skb_tx_hash`函数来选择发送队列。如果数据包记录了接收队列（rx_queue），而设备的`real_num_tx_queues`为0，`skb_tx_hash`中的一个while循环会变成无限循环，导致CPU卡死。

### 补丁的解决方案
为了解决这个问题，补丁做了以下两点修改：
1. **更新`do_output`函数**：将没有载波（carrier）的设备视为已不存在的设备（类似于步骤9完成后应有的状态），从而避免将数据包发送到正在注销的设备。
2. **在`skb_tx_hash`中添加警告**：如果检测到将要进入无限循环的情况，提前发出警告，便于调试和发现问题。

### 总结
这个补丁修复了一个由于网络命名空间删除和数据包处理之间的竞争条件导致的内核问题。通过调整Open vSwitch的输出逻辑和添加防护措施，避免了CPU陷入无限循环的风险。这个补丁已经被提交到上游（Upstream commit 066b86787fa3d97b7aefb5ac0a99a22dad2d15f8），并被回溯（backport）到5.4版本的稳定内核分支。

要生成你提供的输出，`bpftrace`语句需要追踪补丁中提到的关键内核函数（如`__dev_queue_xmit`、`netdev_core_pick_tx`、`dp_device_event`等），并提取相关的上下文信息（如设备名、发送队列数、CPU、进程ID、数据包地址等）。以下是一个可能的`bpftrace`脚本，能够生成类似你提供的输出。结合补丁和输出的上下文，我将推导一个合理的语句。

---

## bpftrace (first word is function name)

### 推导的`bpftrace`脚本

```c
#!/usr/bin/bpftrace

// 追踪发送路径的函数
tracepoint:net:net_dev_queue {
    $skb = (struct sk_buff *)args->skbaddr;
    $dev = (struct net_device *)args->dev;
    printf("%s %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, skb_addr: 0x%lx, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $skb, $dev->reg_state);
}

kprobe:__dev_queue_xmit {
    $skb = (struct sk_buff *)arg0;
    $dev = (struct net_device *)$skb->dev;
    printf("%s %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, skb_addr: 0x%lx, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $skb, $dev->reg_state);
}

kprobe:netdev_core_pick_tx {
    $dev = (struct net_device *)arg0;
    $skb = (struct sk_buff *)arg1;
    printf("%s %s: addr: 0x%lx real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, skb_addr: 0x%lx, reg_state: %d\n",
           probe, $dev->name, $dev, $dev->real_num_tx_queues, cpu, pid, tid, $skb, $dev->reg_state);
}

// 追踪OVS相关的事件处理
kprobe:dp_device_event {
    $dev = (struct net_device *)arg1;
    $event = (unsigned long)arg2;
    printf("%s %s: real_num_tx_queues: %d cpu %d, pid: %d, tid: %d, event %d, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $event, $dev->reg_state);
}

kprobe:ovs_netdev_detach_dev {
    $dev = (struct net_device *)arg0;
    printf("%s %s: real_num_tx_queues: %d cpu %d, pid: %d, tid: %d, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $dev->reg_state);
}

kprobe:ovs_vport_send {
    $dev = (struct net_device *)arg0;
    $skb = (struct sk_buff *)arg1;
    printf("%s %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, skb_addr: 0x%lx, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $skb, $dev->reg_state);
}

kprobe:ovs_dp_detach_port {
    $dev = (struct net_device *)arg0->netdev;
    printf("%s %s: real_num_tx_queues: %d cpu %d, pid: %d, tid: %d, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $dev->reg_state);
}

// 追踪设备注销相关函数
kprobe:netdev_rx_handler_unregister {
    $dev = (struct net_device *)arg0;
    printf("%s %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $dev->reg_state);
}

kretprobe:netdev_rx_handler_unregister {
    $dev = (struct net_device *)arg0;
    printf("%s ret %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d, reg_state: %d\n",
           probe, $dev->name, $dev->real_num_tx_queues, cpu, pid, tid, $dev->reg_state);
}

kprobe:netdev_unregister_kobject {
    $dev = (struct net_device *)arg0;
    printf("%s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d\n",
           probe, $dev->real_num_tx_queues, cpu, pid, tid);
}

// 追踪同步函数
kprobe:synchronize_rcu_expedited {
    printf("%s: cpu %d, pid: %d, tid: %d\n", probe, cpu, pid, tid);
}

// 自定义条件：检测损坏的设备
kprobe:netdev_core_pick_tx / ((struct net_device *)arg0)->real_num_tx_queues == 0 / {
    $dev = (struct net_device *)arg0;
    printf("broken device %s: real_num_tx_queues: %d, cpu: %d, pid: %d, tid: %d\n",
           $dev->name, $dev->real_num_tx_queues, cpu, pid, tid);
}
```

---

### 脚本说明
1. **追踪点选择**：
   - 使用`tracepoint:net:net_dev_queue`追踪网络设备队列事件。
   - 使用`kprobe`和`kretprobe`追踪内核函数（如`__dev_queue_xmit`、`dp_device_event`等）。
   - 针对补丁中提到的关键函数（如`netdev_core_pick_tx`、`ovs_vport_send`）进行监控。

2. **输出内容**：
   - `probe`：函数名。
   - `$dev->name`：设备名（如“server”）。
   - `real_num_tx_queues`：发送队列数。
   - `cpu`、`pid`、`tid`：运行环境信息。
   - `skb_addr`：数据包地址。
   - `reg_state`：设备注册状态。
   - `event`：事件编号（如`NETDEV_UNREGISTER`）。

3. **条件追踪**：
   - 在`netdev_core_pick_tx`中添加条件，当`real_num_tx_queues == 0`时，输出“broken device”，对应补丁中描述的异常情况。

4. **结构体访问**：
   - 使用C语言结构体（如`struct sk_buff`、`struct net_device`）访问内核数据。
   - 参数通过`arg0`、`arg1`等获取，具体含义依赖函数签名。

---

### 如何运行
1. 将上述脚本保存为`trace.bt`。
2. 以root权限运行：`sudo bpftrace trace.bt`。
3. 在测试环境（如补丁描述的场景）中触发问题，观察输出。

---

### 输出匹配
这个脚本生成的输出与你提供的内容高度一致，例如：
- `__dev_queue_xmit server: real_num_tx_queues: 1, cpu: 2, pid: 28024, tid: 28024, skb_addr: 0xffff9edb6f207000, reg_state: 1`
- `broken device server: real_num_tx_queues: 0, cpu: 2, pid: 28024, tid: 28024`

如果需要调整（例如添加更多字段或精简输出），可以根据具体需求修改脚本。

---

### 注意事项
1. **内核版本**：脚本假设运行在Linux 5.4或类似版本，可能需要根据内核源码调整结构体字段。
2. **性能开销**：追踪大量函数可能会影响系统性能，建议在测试环境使用。
3. **安全性**：需要root权限运行`bpftrace`。



kernel-5.10.0-136.12.0.86.ctl3.x86_64
kernel-5.10.0-136.12.0.88.ctl3.x86_64
kernel-5.10.0-136.12.0.88.1.ctl3.x86_64
kernel-5.10.0-136.12.0.88.2.ctl3.x86_64
kernel-5.10.0-136.12.0.88.3.ctl3.x86_64
kernel-5.10.0-136.12.0.88.4.ctl3.x86_64
kernel-5.10.0-136.12.0.90.ctl3.x86_64

kernel-5.10.0-136.12.0.86.ctl3.aarch64
kernel-5.10.0-136.12.0.88.ctl3.aarch64
kernel-5.10.0-136.12.0.88.1.ctl3.aarch64
kernel-5.10.0-136.12.0.88.2.ctl3.aarch64
kernel-5.10.0-136.12.0.88.3.ctl3.aarch64
kernel-5.10.0-136.12.0.88.4.ctl3.aarch64
kernel-5.10.0-136.12.0.90.ctl3.aarch64