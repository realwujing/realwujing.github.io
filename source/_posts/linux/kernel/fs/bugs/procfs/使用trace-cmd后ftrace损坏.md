---
title: '使用trace-cmd后ftrace损坏'
date: '2025/04/24 16:47:37'
updated: '2025/04/24 16:47:37'
---

# 使用trace-cmd后ftrace损坏

## 问题信息

使用trace-cmd后ftrace损坏，ftrace再也用不了，读取`/proc/sys/kernel/ftrace_enabled`报错设备不存在。

```bash
[root@kwe10-az2-compute-s3-55e119e16e21 wujing]# cat /proc/sys/kernel/ftrace_enabled
cat: /proc/sys/kernel/ftrace_enabled: No such device
```

## 问题分析

### strace 追踪

```bash
strace -f -o cat_ftrace_enabled.log cat /proc/sys/kernel/ftrace_enabled

 1 426068 execve("/usr/bin/cat", ["cat", "/proc/sys/kernel/ftrace_enabled"], 0x7ffce33a7570 /* 33 vars */) = 0
 2 426068 brk(NULL)                        = 0x55fcae89d000
 3 426068 access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)
 4 426068 openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
 5 426068 fstat(3, {st_mode=S_IFREG|0644, st_size=79487, ...}) = 0
 6 426068 mmap(NULL, 79487, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fea8856e000
 7 426068 close(3)                         = 0
 8 426068 openat(AT_FDCWD, "/usr/lib64/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
 9 426068 read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\\\2\0\0\0\0\0"..., 832) = 832
10 426068 fstat(3, {st_mode=S_IFREG|0755, st_size=1791192, ...}) = 0
11 426068 mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fea8856c000
12 426068 mmap(NULL, 1799384, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7fea883b4000
13 426068 mprotect(0x7fea883d9000, 1609728, PROT_NONE) = 0
14 426068 mmap(0x7fea883d9000, 1327104, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x25000) = 0x7fea883d9000
15 426068 mmap(0x7fea8851d000, 278528, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x169000) = 0x7fea8851d000
16 426068 mmap(0x7fea88562000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1ad000) = 0x7fea88562000
17 426068 mmap(0x7fea88568000, 13528, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7fea88568000
18 426068 close(3)                         = 0
19 426068 arch_prctl(ARCH_SET_FS, 0x7fea8856d540) = 0
20 426068 mprotect(0x7fea88562000, 12288, PROT_READ) = 0
21 426068 mprotect(0x55fcae839000, 4096, PROT_READ) = 0
22 426068 mprotect(0x7fea885a8000, 4096, PROT_READ) = 0
23 426068 munmap(0x7fea8856e000, 79487)    = 0
24 426068 brk(NULL)                        = 0x55fcae89d000
25 426068 brk(0x55fcae8be000)              = 0x55fcae8be000
26 426068 brk(NULL)                        = 0x55fcae8be000
27 426068 openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY|O_CLOEXEC) = 3
28 426068 fstat(3, {st_mode=S_IFREG|0644, st_size=215157200, ...}) = 0
29 426068 mmap(NULL, 215157200, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fea7b683000
30 426068 close(3)                         = 0
31 426068 fstat(1, {st_mode=S_IFCHR|0620, st_rdev=makedev(0x88, 0x4), ...}) = 0
32 426068 openat(AT_FDCWD, "/proc/sys/kernel/ftrace_enabled", O_RDONLY) = 3
33 426068 fstat(3, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
34 426068 fadvise64(3, 0, 0, POSIX_FADV_SEQUENTIAL) = 0
35 426068 mmap(NULL, 139264, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fea7b661000
36 426068 read(3, 0x7fea7b662000, 131072)  = -1 ENODEV (No such device)
37 426068 write(2, "cat: ", 5)             = 5
38 426068 write(2, "/proc/sys/kernel/ftrace_enabled", 31) = 31
39 426068 openat(AT_FDCWD, "/usr/share/locale/locale.alias", O_RDONLY|O_CLOEXEC) = 4
40 426068 fstat(4, {st_mode=S_IFREG|0644, st_size=2997, ...}) = 0
41 426068 read(4, "# Locale name alias data base.\n#"..., 4096) = 2997
42 426068 read(4, "", 4096)                = 0
43 426068 close(4)                         = 0
44 426068 openat(AT_FDCWD, "/usr/share/locale/en_US.UTF-8/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
45 426068 openat(AT_FDCWD, "/usr/share/locale/en_US.utf8/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
46 426068 openat(AT_FDCWD, "/usr/share/locale/en_US/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
47 426068 openat(AT_FDCWD, "/usr/share/locale/en.UTF-8/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
48 426068 openat(AT_FDCWD, "/usr/share/locale/en.utf8/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
49 426068 openat(AT_FDCWD, "/usr/share/locale/en/LC_MESSAGES/libc.mo", O_RDONLY) = -1 ENOENT (No such file or directory)
50 426068 write(2, ": No such device", 16) = 16
51 426068 write(2, "\n", 1)                = 1
52 426068 munmap(0x7fea7b661000, 139264)   = 0
53 426068 close(3)                         = 0
54 426068 close(1)                         = 0
55 426068 close(2)                         = 0
56 426068 exit_group(1)                    = ?
57 426068 +++ exited with 1 +++
```

### dmesg 查看

dmesg中查看到2025年4月12日15:53:40左右有ftrace相关报错：
```bash
dmesg -T | grep - i ftrace

19304 [Sat Apr 12 15:53:40 2025] ftrace: Failed on adding breakpoints (8237):
19305 [Sat Apr 12 15:53:40 2025] WARNING: CPU: 32 PID: 2646730 at kernel/trace/ftrace.c:2034 ftrace_bug+0x93/0x2b0
19306 [Sat Apr 12 15:53:40 2025] Modules linked in: kvm_intel kvm netlink_diag udp_diag vhost_net vhost tap binfmt_misc tcp_diag inet_diag xt_CT tun xt_multiport ipt_rpfilter iptable_raw ip_set_hash_ip ip_set_hash_net sch_ingress       ipip tunnel4 ip_tunnel nf_tables veth nf_conntrack_netlink xt_addrtype xt_set ip_set_bitmap_port ip_set_hash_ipportnet ip_set_hash_ipportip ip_set_hash_ipport ip_set nfnetlink dummy openvswitch nf_conncount xt_CHECKSUM ipt      _MASQUERADE ipt_REJECT nf_reject_ipv4 ip6table_mangle ip6table_nat nf_nat_ipv6 iptable_mangle ebtable_filter ebtables ip6table_filter ip6_tables xt_conntrack xt_comment iptable_filter xt_mark iptable_nat nf_nat_ipv4 nf_nat      8021q garp mrp bonding rfkill scsi_transport_iscsi overlay livepatch_memory(OEK) intel_rapl_msr iTCO_wdt iTCO_vendor_support intel_rapl_common sunrpc isst_if_common
19307 [Sat Apr 12 15:53:40 2025]  vfat fat skx_edac nfit libnvdimm x86_pkg_temp_thermal intel_powerclamp coretemp ast ext4 i2c_algo_bit ttm ipmi_ssif mbcache jbd2 crct10dif_pclmul drm_kms_helper crc32_pclmul syscopyarea sysfillre      ct ghash_clmulni_intel ses sysimgblt fb_sys_fops enclosure intel_cstate mei_me scsi_transport_sas intel_uncore ipmi_si ixgbe pcspkr mdio intel_rapl_perf ioatdma sg drm i40e ipmi_devintf i2c_i801 lpc_ich mei pcc_cpufreq wmi      dca joydev ipmi_msghandler acpi_power_meter acpi_pad vfio_pci vfio_virqfd vfio_iommu_type1 vfio irqbypass br_netfilter bridge stp llc ip_vs_sh ip_vs_wrr ip_vs_rr ip_vs nf_conntrack nf_defrag_ipv6 nf_defrag_ipv4 ip_tables xf      s libcrc32c dm_multipath sd_mod megaraid_sas(OE) crc32c_intel ahci libahci libata dm_mirror dm_region_hash dm_log dm_mod [last unloaded: kvm]
19308 [Sat Apr 12 15:53:40 2025] CPU: 32 PID: 2646730 Comm: trace-cmd Kdump: loaded Tainted: G        W  OE K   4.19.90-2102.2.0.0062.ctl2.x86_64 #1
19309 [Sat Apr 12 15:53:40 2025] Hardware name: FiberHome R2200 V5/Xeon Boards, BIOS 3.1a 02/24/2020
19310 [Sat Apr 12 15:53:40 2025] RIP: 0010:ftrace_bug+0x93/0x2b0
19311 [Sat Apr 12 15:53:40 2025] Code: 13 68 aa 01 83 f8 02 0f 84 21 01 00 00 0f 87 9e 00 00 00 83 f8 01 0f 84 34 01 00 00 48 85 ed 0f 85 3c 01 00 00 5b 5d 41 5c c3 <0f> 0b 48 c7 c7 c0 a0 09 93 c7 05 82 bb 2c 01 01 00 00 00 c7 05       88
19312 [Sat Apr 12 15:53:40 2025] RSP: 0018:ffff9849e0f7fca0 EFLAGS: 00010246
19313 [Sat Apr 12 15:53:40 2025] RAX: 000000000000002c RBX: ffffffff92258020 RCX: 0000000000000000
19314 [Sat Apr 12 15:53:40 2025] RDX: 0000000000000000 RSI: ffff8b2907ca02d0 RDI: 00000000ffffffea
19315 [Sat Apr 12 15:53:40 2025] RBP: ffff8b2907ca02d0 R08: 000000000003c757 R09: 0000000000aaaaaa
19316 [Sat Apr 12 15:53:40 2025] R10: 0000000000000000 R11: ffff8b2f73decb70 R12: ffffffff93c42cf0
19317 [Sat Apr 12 15:53:40 2025] R13: 0000000000000001 R14: 00000000ffffffea R15: 00000000ffffffe8
19318 [Sat Apr 12 15:53:40 2025] FS:  00007f551f3e4740(0000) GS:ffff8b6740c00000(0000) knlGS:0000000000000000
19319 [Sat Apr 12 15:53:40 2025] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
19320 [Sat Apr 12 15:53:40 2025] CR2: 0000000000000000 CR3: 00000001ac228004 CR4: 00000000007626e0
19321 [Sat Apr 12 15:53:40 2025] DR0: 0000000000000000 DR1: 0000000000000000 DR2: 0000000000000000
19322 [Sat Apr 12 15:53:40 2025] DR3: 0000000000000000 DR6: 00000000fffe0ff0 DR7: 0000000000000400
19323 [Sat Apr 12 15:53:40 2025] PKRU: 55555554
19324 [Sat Apr 12 15:53:40 2025] Call Trace:
19325 [Sat Apr 12 15:53:40 2025]  ftrace_replace_code+0x287/0x370
19326 [Sat Apr 12 15:53:40 2025]  ? ftrace_regs_caller_op_ptr+0x6b/0x6b
19327 [Sat Apr 12 15:53:40 2025]  ftrace_modify_all_code+0xba/0x170
19328 [Sat Apr 12 15:53:40 2025]  arch_ftrace_update_code+0xc/0x20
19329 [Sat Apr 12 15:53:40 2025]  ftrace_run_update_code+0x13/0x80
19330 [Sat Apr 12 15:53:40 2025]  ftrace_startup+0xc1/0x170
19331 [Sat Apr 12 15:53:40 2025]  register_ftrace_function+0x44/0x60
19332 [Sat Apr 12 15:53:40 2025]  function_trace_init+0x5a/0x70
19333 [Sat Apr 12 15:53:40 2025]  tracing_set_tracer+0xef/0x1b0
19334 [Sat Apr 12 15:53:40 2025]  tracing_set_trace_write+0xc8/0x110
19335 [Sat Apr 12 15:53:40 2025]  ? _copy_to_user+0x22/0x30
19336 [Sat Apr 12 15:53:40 2025]  ? cp_new_stat+0x150/0x180
19337 [Sat Apr 12 15:53:40 2025]  __vfs_write+0x36/0x1a0
19338 [Sat Apr 12 15:53:40 2025]  ? __do_sys_newfstat+0x3c/0x60
19339 [Sat Apr 12 15:53:40 2025]  vfs_write+0xad/0x1a0
19340 [Sat Apr 12 15:53:40 2025]  ksys_write+0x5a/0xd0
19341 [Sat Apr 12 15:53:40 2025]  do_syscall_64+0x5b/0x1d0
19342 [Sat Apr 12 15:53:40 2025]  entry_SYSCALL_64_after_hwframe+0x44/0xa9
19343 [Sat Apr 12 15:53:40 2025] RIP: 0033:0x7f551f4d0504
19344 [Sat Apr 12 15:53:40 2025] Code: 00 f7 d8 64 89 02 48 c7 c0 ff ff ff ff eb b3 0f 1f 80 00 00 00 00 48 8d 05 39 dd 0c 00 8b 00 85 c0 75 13 b8 01 00 00 00 0f 05 <48> 3d 00 f0 ff ff 77 54 f3 c3 66 90 41 54 55 49 89 d4 53 48 89       f5
19345 [Sat Apr 12 15:53:40 2025] RSP: 002b:00007ffd13fc9608 EFLAGS: 00000246 ORIG_RAX: 0000000000000001
19346 [Sat Apr 12 15:53:40 2025] RAX: ffffffffffffffda RBX: 0000000000000008 RCX: 00007f551f4d0504
19347 [Sat Apr 12 15:53:40 2025] RDX: 0000000000000008 RSI: 00005596d49c0120 RDI: 0000000000000006
19348 [Sat Apr 12 15:53:40 2025] RBP: 00005596d49c0120 R08: 00007f551f3e4740 R09: 00005596d49b8010
19349 [Sat Apr 12 15:53:40 2025] R10: 0000000000000000 R11: 0000000000000246 R12: 00005596d49be0e0
19350 [Sat Apr 12 15:53:40 2025] R13: 0000000000000008 R14: 00007f551f5997c0 R15: 0000000000000008
19351 [Sat Apr 12 15:53:40 2025] ---[ end trace 18d1051f4b9494da ]---
19352 [Sat Apr 12 15:53:40 2025] ftrace failed to modify
19353 [Sat Apr 12 15:53:40 2025] [<ffffffff92258020>] clear_gigantic_page_chunk+0x0/0xb0
19354 [Sat Apr 12 15:53:40 2025]  actual:   e9:8b:22:33:2e
19355 [Sat Apr 12 15:53:40 2025]  expected: 0f:1f:44:00:00
19356 [Sat Apr 12 15:53:40 2025] Setting ftrace call site to call ftrace function
19357 [Sat Apr 12 15:53:40 2025] ftrace record flags: 10000001
19358 [Sat Apr 12 15:53:40 2025]  (1)
19359                             expected tramp: ffffffff92a01840
19360 [Mon Apr 14 11:05:09 2025] WARNING: stack recursion on stack type 4
19361 [Mon Apr 14 11:05:09 2025] WARNING: can't dereference registers at 000000005a8d6c69 for ip swapgs_restore_regs_and_return_to_usermode+0x25/0x87
```

根据demsg中的日志`WARNING: CPU: 32 PID: 2646730 at kernel/trace/ftrace.c:2034 ftrace_bug+0x93/0x2b0`查看对应源码：

```c
// vim kernel/trace/ftrace.c +2034

2023 void ftrace_bug(int failed, struct dyn_ftrace *rec)
2024 {
2025     unsigned long ip = rec ? rec->ip : 0;
2026
2027     switch (failed) {
2028     case -EFAULT:
2029         FTRACE_WARN_ON_ONCE(1);
2030         pr_info("ftrace faulted on modifying ");
2031         print_ip_sym(ip);
2032         break;
2033     case -EINVAL:
2034         FTRACE_WARN_ON_ONCE(1);
2035         pr_info("ftrace failed to modify ");
2036         print_ip_sym(ip);
2037         print_ip_ins(" actual:   ", (unsigned char *)ip);
2038         pr_cont("\n");
2039         if (ftrace_expected) {
2040             print_ip_ins(" expected: ", ftrace_expected);
2041             pr_cont("\n");
2042         }
2043         break;
```

安装对应内核调试包：
```bash
yum install crash kernel-debuginfo-`uname -r` -y
```

crash中查看`ftrace_replace_code+0x287`函数位置：
```bash
crash> dis -sx ftrace_replace_code+0x287
FILE: arch/x86/kernel/ftrace.c
LINE: 630

  625           return;
  626
  627    remove_breakpoints:
  628           pr_warn("Failed on %s (%d):\n", report, count);
  629           ftrace_bug(ret, rec);
* 630           for_ftrace_rec_iter(iter) {
  631                   rec = ftrace_rec_iter_record(iter);
  632                   /*
  633                    * Breakpoints are handled only when this function is in
  634                    * progress. The system could not work with them.
  635                    */
  636                   if (remove_breakpoint(rec))
  637                           BUG();
  638           }
  639           run_sync();
  640   }
```

## 问题结果

无法复现。
