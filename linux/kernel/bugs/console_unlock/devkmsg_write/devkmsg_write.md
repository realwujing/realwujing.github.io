# devkmsg write

![console_unlock](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/console_unlock.png)

```c
[25956.114454] rcu: INFO: rcu_sched self-detected stall on CPU
[25956.114459] rcu: 	196-....: (10810 ticks this GP) idle=82a/1/0x4000000000000002 softirq=640301/640310 fqs=3557 
[25956.114480] 	(t=15001 jiffies g=806129 q=338468)
[25956.114489] NMI backtrace for cpu 196
[25956.114493] CPU: 196 PID: 1776231 Comm: systemd-coredum Kdump: loaded Not tainted 5.10.0-136.12.0.90.ctl3.aarch64 #1
[25956.114494] Hardware name: Huawei To be filled by O.E.M./BC83AMDAE, BIOS 09.04.02.01.11 12/21/2024
[25956.114495] Call trace:
[25956.114503]  dump_backtrace+0x0/0x1e4
[25956.114505]  show_stack+0x20/0x2c
[25956.114516]  dump_stack+0xd8/0x140
[25956.114521]  nmi_cpu_backtrace+0xbc/0x124
[25956.114523]  nmi_trigger_cpumask_backtrace+0x198/0x22c
[25956.114527]  arch_trigger_cpumask_backtrace+0x4c/0x60
[25956.114529]  rcu_dump_cpu_stacks+0x130/0x1ac
[25956.114536]  print_cpu_stall+0x1dc/0x304
[25956.114537]  check_cpu_stall+0x148/0x2b0
[25956.114538]  rcu_pending+0x40/0x160
[25956.114541]  rcu_sched_clock_irq+0x70/0x180
[25956.114544]  update_process_times+0x68/0xb0
[25956.114554]  tick_sched_handle+0x38/0x74
[25956.114556]  tick_sched_timer+0x54/0xb0
[25956.114558]  __run_hrtimer+0x98/0x2a0
[25956.114559]  __hrtimer_run_queues+0xb0/0x134
[25956.114560]  hrtimer_interrupt+0x13c/0x3c0
[25956.114565]  arch_timer_handler_phys+0x3c/0x50
[25956.114570]  handle_percpu_devid_irq+0x90/0x1f4
[25956.114576]  __handle_domain_irq+0x84/0xf0
[25956.114578]  gic_handle_irq+0x78/0x2c0
[25956.114580]  el1_irq+0xb8/0x140
[25956.114581]  console_unlock+0x1b0/0x3e0
[25956.114583]  vprintk_emit+0x19c/0x1f4
[25956.114584]  devkmsg_emit.constprop.0+0x6c/0x94
[25956.114585]  devkmsg_write+0x16c/0x190
[25956.114589]  do_iter_readv_writev+0x100/0x1a4
[25956.114591]  do_iter_write+0x98/0x17c
[25956.114592]  vfs_writev+0xb4/0x180
[25956.114594]  do_writev+0x7c/0x140
[25956.114595]  __arm64_sys_writev+0x28/0x34
[25956.114605]  invoke_syscall+0x50/0x11c
[25956.114606]  el0_svc_common.constprop.0+0x158/0x164
[25956.114608]  do_el0_svc+0x2c/0x9c
[25956.114617]  el0_svc+0x20/0x30
[25956.114619]  el0_sync_handler+0xb0/0xb4
[25956.114620]  el0_sync+0x160/0x180
[26112.439500] watchdog: BUG: soft lockup - CPU#196 stuck for 134s! [systemd-coredum:1776231]
[26112.439502] Modules linked in: xts salsa20_generic chacha_generic chacha_neon libchacha xxhash_generic wp512 tgr192 rmd320 rmd256 rmd160 rmd128 poly1305_generic poly1305_neon nhpoly1305_neon nhpoly1305 libpoly1305 michael_mic md4 crc32_generic blake2b_generic twofish_generic twofish_common tea sm4_generic sm4_neon serpent_generic seed khazad fcrypt des_generic libdes cast6_generic cast5_generic cast_common camellia_generic blowfish_generic blowfish_common anubis binfmt_misc nft_fib_inet nft_fib_ipv4 nft_fib_ipv6 nft_fib nft_reject_inet nf_reject_ipv4 nf_reject_ipv6 nft_reject nft_ct nft_chain_nat nf_tables ebtable_nat ebtable_broute ip6table_nat ip6table_mangle ip6table_raw ip6table_security iptable_nat nf_nat nf_conntrack nf_defrag_ipv6 nf_defrag_ipv4 libcrc32c iptable_mangle iptable_raw rfkill iptable_security ip_set nfnetlink ebtable_filter ebtables ip6table_filter ip6_tables iptable_filter ip_tables rpcrdma sunrpc rdma_ucm ib_srpt ib_isert iscsi_target_mod
[26112.439566]  target_core_mod ib_iser rdma_cm iw_cm libiscsi ib_umad ib_ipoib ib_cm scsi_transport_iscsi ipmi_ssif ses enclosure uio_pdrv_genirq mlx5_ib hns_roce_hw_v2 uio hibmc_drm acpi_ipmi ib_uverbs ofpart sg drm_vram_helper ipmi_si cmdlinepart ib_core drm_ttm_helper hns3_pmu spi_nor ipmi_devintf ttm hisi_ptt hisi_pcie_pmu mtd ipmi_msghandler ptp_hisi spi_hisi_sfc_v3xx hisi_uncore_cpa_pmu hisi_uncore_pa_pmu hisi_uncore_sllc_pmu hisi_uncore_hha_pmu hisi_uncore_l3c_pmu hisi_uncore_ddrc_pmu hisi_uncore_pmu vfat fat sch_fq_codel fuse ext4 mbcache jbd2 ghash_ce sm4_ce sm4_ce_cipher sm4 sd_mod sm3_ce sha3_ce t10_pi hclge hisi_sas_v3_hw sha3_generic sha512_ce sha512_arm64 sha2_ce sha256_arm64 hisi_sas_main sha1_ce mlx5_core sbsa_gwdt ahci libsas hns3 libahci megaraid_sas scsi_transport_sas libata hnae3 host_edma_drv mlxfw hisi_trng_v2 hns_mdio dm_mirror dm_region_hash dm_log dm_mod aes_neon_bs aes_neon_blk aes_ce_blk crypto_simd cryptd aes_ce_cipher
[26112.439645] CPU: 196 PID: 1776231 Comm: systemd-coredum Kdump: loaded Not tainted 5.10.0-136.12.0.90.ctl3.aarch64 #1
[26112.439646] Hardware name: Huawei To be filled by O.E.M./BC83AMDAE, BIOS 09.04.02.01.11 12/21/2024
[26112.439649] pstate: 40400009 (nZcv daif +PAN -UAO -TCO BTYPE=--)
[26112.439654] pc : console_unlock+0x1b0/0x3e0
[26112.439655] lr : console_unlock+0x1ac/0x3e0
[26112.439656] sp : ffff80007f8cb9f0
[26112.439657] x29: ffff80007f8cb9f0 x28: 000000000000004b 
[26112.439658] x27: ffffd8280876a860 x26: 0000000000000000 
[26112.439660] x25: 0000000000000000 x24: ffffd828083df008 
[26112.439661] x23: ffffd82808731e48 x22: 0000000000000000 
[26112.439663] x21: ffffd8280874a1f0 x20: ffffd828087326f0 
[26112.439664] x19: ffffd82808d151c8 x18: 0000000000000020 
[26112.439666] x17: 0000000000000000 x16: 0000000000000000 
[26112.439668] x15: ffffffffffffffff x14: 0000000000000001 
[26112.439669] x13: 0000000000000020 x12: ffff1f1ddc942f60 
[26112.439671] x11: 0000000000000008 x10: ffffd82807b15710 
[26112.439672] x9 : ffffd82806e83280 x8 : 0000000000000000 
[26112.439674] x7 : ffff1f1de7fa5800 x6 : 0000000000000000 
[26112.439675] x5 : 0000000000000000 x4 : 0000000000000000 
[26112.439677] x3 : 0000000000000000 x2 : 00000000ffffffff 
[26112.439678] x1 : ffff6772b63ed000 x0 : ffff3f9abe7cf3a8 
[26112.439680] Call trace:
[26112.439681]  console_unlock+0x1b0/0x3e0
[26112.439683]  vprintk_emit+0x19c/0x1f4
[26112.439686]  devkmsg_emit.constprop.0+0x6c/0x94
[26112.439687]  devkmsg_write+0x16c/0x190
[26112.439690]  do_iter_readv_writev+0x100/0x1a4
[26112.439691]  do_iter_write+0x98/0x17c
[26112.439692]  vfs_writev+0xb4/0x180
[26112.439694]  do_writev+0x7c/0x140
[26112.439695]  __arm64_sys_writev+0x28/0x34
[26112.439701]  invoke_syscall+0x50/0x11c
[26112.439703]  el0_svc_common.constprop.0+0x158/0x164
[26112.439704]  do_el0_svc+0x2c/0x9c
[26112.439709]  el0_svc+0x20/0x30
[26112.439710]  el0_sync_handler+0xb0/0xb4
[26112.439712]  el0_sync+0x160/0x180
[26112.439714] Kernel panic - not syncing: softlockup: hung tasks
[26112.439716] CPU: 196 PID: 1776231 Comm: systemd-coredum Kdump: loaded Tainted: G             L    5.10.0-136.12.0.90.ctl3.aarch64 #1
[26112.439717] Hardware name: Huawei To be filled by O.E.M./BC83AMDAE, BIOS 09.04.02.01.11 12/21/2024
[26112.439718] Call trace:
[26112.439719]  dump_backtrace+0x0/0x1e4
[26112.439720]  show_stack+0x20/0x2c
[26112.439722]  dump_stack+0xd8/0x140
[26112.439724]  panic+0x168/0x390
[26112.439727]  watchdog_timer_fn+0x230/0x290
[26112.439730]  __run_hrtimer+0x98/0x2a0
[26112.439732]  __hrtimer_run_queues+0xb0/0x134
[26112.439733]  hrtimer_interrupt+0x13c/0x3c0
[26112.439736]  arch_timer_handler_phys+0x3c/0x50
[26112.439738]  handle_percpu_devid_irq+0x90/0x1f4
[26112.439741]  __handle_domain_irq+0x84/0xf0
[26112.439742]  gic_handle_irq+0x78/0x2c0
[26112.439743]  el1_irq+0xb8/0x140
[26112.439744]  console_unlock+0x1b0/0x3e0
[26112.439745]  vprintk_emit+0x19c/0x1f4
[26112.439746]  devkmsg_emit.constprop.0+0x6c/0x94
[26112.439747]  devkmsg_write+0x16c/0x190
[26112.439748]  do_iter_readv_writev+0x100/0x1a4
[26112.439749]  do_iter_write+0x98/0x17c
[26112.439750]  vfs_writev+0xb4/0x180
[26112.439751]  do_writev+0x7c/0x140
[26112.439752]  __arm64_sys_writev+0x28/0x34
[26112.439754]  invoke_syscall+0x50/0x11c
[26112.439756]  el0_svc_common.constprop.0+0x158/0x164
[26112.439757]  do_el0_svc+0x2c/0x9c
[26112.439758]  el0_svc+0x20/0x30
[26112.439760]  el0_sync_handler+0xb0/0xb4
[26112.439761]  el0_sync+0x160/0x180
[26112.439877] SMP: stopping secondary CPUs
[26112.445052] Starting crashdump kernel...
[26112.445055] Bye!
```
