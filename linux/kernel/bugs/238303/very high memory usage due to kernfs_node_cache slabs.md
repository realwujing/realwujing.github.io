# very high memory usage due to kernfs_node_cache slabs

<https://pms.uniontech.com/bug-view-238303.html>

## bug环境

```bash
uname -a
Linux 0000000g-A8cUta6pu5 4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819 SMP Fri Jan 5 12:27:47 CST 2024 aarch64 GNU/Linux
```

```bash
apt policy systemd
systemd:
  已安装：241.61-deepin1
  候选： 241.61-deepin1
  版本列表：
 *** 241.61-deepin1 500
        500 http://pools.uniontech.com/ppa/dde-eagle eagle/1070/main arm64 Packages
        100 /var/lib/dpkg/status
```

## 初步分析

### Slab

```bash
cat /proc/meminfo | grep Slab: -A2
Slab:             913952 kB
SReclaimable:     175568 kB
SUnreclaim:       738384 kB
```

#### slabtop

```bash
slabtop -s c -o | head -n20
 Active / Total Objects (% used)    : 924058 / 1039323 (88.9%)
 Active / Total Slabs (% used)      : 43496 / 43496 (100.0%)
 Active / Total Caches (% used)     : 121 / 187 (64.7%)
 Active / Total Size (% used)       : 815879.95K / 908598.60K (89.8%)
 Minimum / Average / Maximum Object : 0.36K / 0.87K / 16.81K

  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME                   
 55251  52846  95%    4.44K   7893        7    252576K names_cache            
229119 223856  97%    0.48K   6943       33    111088K kernfs_node_cache      
141850 137390  96%    0.62K   5674       25     90784K kmalloc-128            
 13776  12659  91%    4.50K   1968        7     62976K kmalloc-4096           
104670  87963  84%    0.53K   3489       30     55824K dentry                 
 63020  52097  82%    0.69K   2740       23     43840K filp                   
 72090  59483  82%    0.53K   2403       30     38448K vm_area_struct         
 21142  19921  94%    1.41K    961       22     30752K ext4_inode_cache       
 58555  55044  94%    0.45K   1673       35     26768K buffer_head            
 38775  34343  88%    0.62K   1551       25     24816K skbuff_head_cache      
 23256  21205  91%    0.94K    684       34     21888K inode_cache            
 21248  17845  83%    1.00K    664       32     21248K kmalloc-512            
 47502  40866  86%    0.41K   1218       39     19488K anon_vma_chain       
```

#### slub_debug

```bash
echo 1 > /sys/kernel/slab/kernfs_node_cache/trace  && sleep 60 && echo 0 > /sys/kernel/slab/kernfs_node_cache/trace
```

kernfs_node_cache alloc:

```bash
1673 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.452633] TRACE kernfs_node_cache alloc 0x00000000a05f4917 inuse=35 fp=0x          (null)
1674 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.454989] CPU: 7 PID: 1 Comm: systemd Tainted: G           O      4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819
1675 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.457735] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
1676 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.459811] Call trace:
1677 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.460466]  dump_backtrace+0x0/0x190
1678 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.461368]  show_stack+0x14/0x20
1679 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.462291]  dump_stack+0xa8/0xcc
1680 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.463168]  alloc_debug_processing+0x58/0x188
1681 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.464272]  ___slab_alloc.constprop.34+0x31c/0x388
1682 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.465491]  kmem_cache_alloc+0x210/0x278
1683 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.466538]  __kernfs_new_node+0x60/0x1f8
1684 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.467553]  kernfs_new_node+0x24/0x48
1685 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.468508]  kernfs_create_dir_ns+0x30/0x88
1686 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.469584]  cgroup_mkdir+0x2f0/0x4e8
1687 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.470516]  kernfs_iop_mkdir+0x58/0x88
1688 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.471497]  vfs_mkdir+0xfc/0x1c0
1689 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.472328]  do_mkdirat+0xec/0x100
1690 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.473172]  __arm64_sys_mkdirat+0x1c/0x28
1691 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.474210]  el0_svc_common+0x90/0x178
1692 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.475125]  el0_svc_handler+0x9c/0xa8
1693 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.476049]  el0_svc+0x8/0xc
```

```bash
grep "kernfs_node_cache alloc" kern.log | wc -l
7239
```

```bash
./scripts/faddr2line vmlinux vfs_mkdir+0xfc/0x1c0
vfs_mkdir+0xfc/0x1c0:
vfs_mkdir 于 fs/namei.c:3820
```

```bash
./scripts/faddr2line vmlinux kernfs_iop_mkdir+0x58/0x88
kernfs_iop_mkdir+0x58/0x88:
kernfs_iop_mkdir 于 fs/kernfs/dir.c:1120
```

```bash
./scripts/faddr2line vmlinux cgroup_mkdir+0x2f0/0x4e8
cgroup_mkdir+0x2f0/0x4e8:
kernfs_create_dir 于 include/linux/kernfs.h:507
(已内连入)cgroup_mkdir 于 kernel/cgroup/cgroup.c:5032
```

kernfs_node_cache free:

```bash
3106 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.212900] TRACE kernfs_node_cache free 0x00000000e2ea365c inuse=34 fp=0x00000000a8805aea
3107 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.215538] Object 00000000e2ea365c: 00 00 00 00 01 00 00 80 78 a2 37 a6 03 80 ff ff  ........x.7.....
3108 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.218656] Object 000000003a5b5659: 00 52 45 de 03 80 ff ff 40 92 37 a6 03 80 ff ff  .RE.....@.7.....
3109 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.221710] Object 000000007229340d: 80 b6 37 a6 03 80 ff ff 00 00 00 00 00 00 00 00  ..7.............
3110 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.224824] Object 00000000cc138d0e: 00 00 00 00 00 00 00 00 7e 16 88 71 00 00 00 00  ........~..q....
3111 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.227947] Object 000000001cecdc04: b0 e4 a2 09 00 00 ff ff 00 00 00 00 00 00 00 00  ................
3112 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.231088] Object 00000000c3ac2227: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
3113 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.234279] Object 00000000737356c8: 98 dc a2 09 00 00 ff ff 14 09 00 00 01 00 00 00  ................
3114 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.237384] Object 00000000ba78c59c: 52 20 a4 81 00 00 00 00 00 00 00 00 00 00 00 00  R ..............
3115 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.240508] CPU: 3 PID: 1 Comm: systemd Tainted: G           O      4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819
3116 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.244280] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
3117 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.246454] Call trace:
3118 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.247264]  dump_backtrace+0x0/0x190
3119 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.248512]  show_stack+0x14/0x20
3120 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.249584]  dump_stack+0xa8/0xcc
3121 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.250672]  free_debug_processing+0x19c/0x3a0
3122 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.252143]  __slab_free+0x230/0x3f8
3123 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.253321]  kmem_cache_free+0x200/0x220
3124 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.254657]  kernfs_put+0x100/0x238
3125 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.255806]  kernfs_evict_inode+0x2c/0x38
3126 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.257119]  evict+0xc0/0x1c0
3127 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.258067]  iput+0x1c8/0x288
3128 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.259371]  dentry_unlink_inode+0xb0/0xe8
3129 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.260600]  __dentry_kill+0xc4/0x1d0
3130 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.261434]  shrink_dentry_list+0x1ac/0x2c0
3131 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.262413]  shrink_dcache_parent+0x78/0x80
3132 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.263336]  vfs_rmdir+0xf0/0x190
3133 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.264066]  do_rmdir+0x1c0/0x1f8
3134 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.264791]  __arm64_sys_unlinkat+0x4c/0x60
3135 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.265710]  el0_svc_common+0x90/0x178
3136 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.266968]  el0_svc_handler+0x9c/0xa8
3137 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.267828]  el0_svc+0x8/0xc
```

```bash
grep "kernfs_node_cache free" kern.log | wc -l
5034
```

### trace-bpfcc

```bash
sudo apt install systemd-dbgsym
apt source systemd=241.61-deepin1
```

```bash
bpftrace -l | grep cgroup_mkdir
tracepoint:cgroup:cgroup_mkdir
```

```bash
trace-bpfcc -tKU cgroup_mkdir | tee cgroup_mkdir.log
```

vim cgroup_mkdir.log:

```bash
TIME     PID     TID     COMM            FUNC             
11.08041 1       1       systemd         cgroup_mkdir     
        cgroup_mkdir+0x0 [kernel]
        vfs_mkdir+0xfc [kernel]
        do_mkdirat+0xec [kernel]
        __arm64_sys_mkdirat+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        [unknown] [libc-2.28.so (deleted)]
        cg_create.localalias.13+0x64 [libsystemd-shared-241.so]
        cg_create_everywhere+0x30 [libsystemd-shared-241.so]
        unit_create_cgroup+0xb4 [systemd]
        unit_realize_cgroup_now.lto_priv.665+0xa8 [systemd]
        unit_realize_cgroup+0x16c [systemd]
        unit_prepare_exec+0x18 [systemd]
        service_spawn.lto_priv.382+0x80 [systemd]
        service_start.lto_priv.56+0x144 [systemd]
        job_perform_on_unit.lto_priv.422+0x6b4 [systemd]
        manager_dispatch_run_queue.lto_priv.589+0x358 [systemd]
        source_dispatch+0x118 [libsystemd-shared-241.so]
        sd_event_dispatch+0x150 [libsystemd-shared-241.so]
        sd_event_run+0x90 [libsystemd-shared-241.so]
        invoke_main_loop+0xff4 [systemd]
        main+0x1660 [systemd]
        [unknown] [libc-2.28.so (deleted)]
        [unknown] [systemd]

11.08056 1       1       systemd         cgroup_mkdir     
        cgroup_mkdir+0x0 [kernel]
        vfs_mkdir+0xfc [kernel]
        do_mkdirat+0xec [kernel]
        __arm64_sys_mkdirat+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        [unknown] [libc-2.28.so (deleted)]
        cg_create.localalias.13+0x64 [libsystemd-shared-241.so]
        cg_create.localalias.13+0xe8 [libsystemd-shared-241.so]
        cg_create_everywhere+0x30 [libsystemd-shared-241.so]
        unit_create_cgroup+0xb4 [systemd]
        unit_realize_cgroup_now.lto_priv.665+0xa8 [systemd]
        unit_realize_cgroup+0x16c [systemd]
        unit_prepare_exec+0x18 [systemd]
        service_spawn.lto_priv.382+0x80 [systemd]
        service_start.lto_priv.56+0x144 [systemd]
        job_perform_on_unit.lto_priv.422+0x6b4 [systemd]
        manager_dispatch_run_queue.lto_priv.589+0x358 [systemd]
        source_dispatch+0x118 [libsystemd-shared-241.so]
        sd_event_dispatch+0x150 [libsystemd-shared-241.so]
        sd_event_run+0x90 [libsystemd-shared-241.so]
        invoke_main_loop+0xff4 [systemd]
        main+0x1660 [systemd]
        [unknown] [libc-2.28.so (deleted)]
        [unknown] [systemd]
```

```c
1626 static int unit_create_cgroup( // vim src/core/cgroup.c +1626
1627                 Unit *u,
1628                 CGroupMask target_mask,
1629                 CGroupMask enable_mask,
1630                 ManagerState state) {
1631 
1632         bool created;
1633         int r;
1634 
1635         assert(u);
1636 
1637         if (!UNIT_HAS_CGROUP_CONTEXT(u))
1638                 return 0;
1639 
1640         /* Figure out our cgroup path */
1641         r = unit_pick_cgroup_path(u);
1642         if (r < 0)
1643                 return r;
1644 
1645         /* First, create our own group */
1646         r = cg_create_everywhere(u->manager->cgroup_supported, target_mask, u->cgroup_path);
1647         if (r < 0)
1648                 return log_unit_error_errno(u, r, "Failed to create cgroup %s: %m", u->cgroup_path);
1649         created = r;
```

### systemd log

```bash
grep -i failed syslog | grep systemd | wc -l
146225
```

```bash
grep -i failed syslog | grep systemd | grep .service | grep -v "Failed to start" > systemd.log
```

#### deepin-anything-monitor.service

```bash
grep deepin-anything-monitor.service systemd.log | wc -l
835
```

```bash
systemctl status deepin-anything-monitor.service
● deepin-anything-monitor.service - Deepin anything service
   Loaded: loaded (/lib/systemd/system/deepin-anything-monitor.service; enabled; vendor preset: enabled)
   Active: activating (auto-restart) (Result: exit-code) since Thu 2024-01-25 10:24:27 CST; 2s ago
  Process: 19672 ExecStartPre=/usr/sbin/modprobe vfs_monitor (code=exited, status=1/FAILURE)
  Process: 19674 ExecStopPost=/usr/sbin/rmmod vfs_monitor (code=exited, status=1/FAILURE)
```

#### logrotate.service

```bash
grep logrotate.service systemd.log | wc -l
568
```

```bash
2024-01-01 00:00:12 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 01:00:01 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 02:00:01 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 03:00:21 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 04:00:31 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
```

```bash
systemctl status logrotate.service
● logrotate.service - Rotate log files
   Loaded: loaded (/lib/systemd/system/logrotate.service; static; vendor preset: enabled)
   Active: failed (Result: exit-code) since Thu 2024-01-25 09:00:05 CST; 53min ago
     Docs: man:logrotate(8)
           man:logrotate.conf(5)
  Process: 7067 ExecStart=/usr/sbin/logrotate /etc/logrotate.conf (code=exited, status=1/FAILURE)
 Main PID: 7067 (code=exited, status=1/FAILURE)

1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: clink-agent:8, unexpected text after }
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/apt/term.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set "su
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/apt/history.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set 
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/mirror/printer/printer.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/cups/access_log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set 
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/cups/error_log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set "
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/mirror/ctyunInstall/ctyunInstall.log" because parent directory has insecure permissions (It's world writable or writable by group whic
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: logrotate.service: Main process exited, code=exited, status=1/FAILURE
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: logrotate.service: Failed with result 'exit-code'.
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: Failed to start Rotate log files.
```

#### avahi-daemon.service

```bash
uptime 
 10:36:02 up 18:29,  1 user,  load average: 0.18, 0.27, 0.25
```

```bash
grep dbus-org.freedesktop.Avahi.service systemd.log | wc -l
143146
```

```bash
2024-01-16 05:45:03 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:08 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:13 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:18 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
```

5S重启一次，本次系统启动也就18个半钟头，跑了143146次。

```bash
systemctl cat avahi-daemon.service
# /lib/systemd/system/avahi-daemon.service
# This file is part of avahi.
#
# avahi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# avahi is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with avahi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

[Unit]
Description=Avahi mDNS/DNS-SD Stack
Requires=avahi-daemon.socket

[Service]
Type=dbus
BusName=org.freedesktop.Avahi
ExecStart=/usr/sbin/avahi-daemon -s
ExecReload=/usr/sbin/avahi-daemon -r
NotifyAccess=main

[Install]
WantedBy=multi-user.target
Also=avahi-daemon.socket
Alias=dbus-org.freedesktop.Avahi.service
```

```bash
apt show avahi-daemon
Package: avahi-daemon
Version: 0.7.6-1+dde
Priority: optional
Section: net
Source: avahi
Maintainer: Utopia Maintenance Team <pkg-utopia-maintainers@lists.alioth.debian.org>
Installed-Size: 263 kB
Depends: libavahi-common3 (>= 0.6.16), libavahi-core7 (>= 0.6.24), libc6 (>= 2.27), libcap2 (>= 1:2.10), libdaemon0 (>= 0.14), libdbus-1-3 (>= 1.9.14), libexpat1 (>= 2.0.1), adduser, dbus (>= 0.60), lsb-base (>= 3.0-6), bind9-host | host
Recommends: libnss-mdns (>= 0.11)
Suggests: avahi-autoipd
Homepage: http://avahi.org/
Download-Size: 89.4 kB
APT-Manual-Installed: no
APT-Sources: https://professional-packages.chinauos.com/desktop-professional eagle/main arm64 Packages
Description: Avahi mDNS/DNS-SD daemon
 Avahi is a fully LGPL framework for Multicast DNS Service Discovery.
 It allows programs to publish and discover services and hosts
 running on a local network with no specific configuration. For
 example you can plug into a network and instantly find printers to
 print to, files to look at and people to talk to.
 .
 This package contains the Avahi Daemon which represents your machine
 on the network and allows other applications to publish and resolve
 mDNS/DNS-SD records.
```

### 解决方案

```bash
sudo apt reinstall avahi-daemon
sudo systemctl daemon-reload
sudo systemctl restart avahi-daemon
```

## More

- [very high memory usage due to kernfs_node_cache slabs #1927](https://github.com/coreos/bugs/issues/1927)
  - [socket activation: dentry slab cache keeps increasing #6567](https://github.com/systemd/systemd/issues/6567)
- [cgroup 内存泄露问题排查记录](https://www.jianshu.com/p/cae1525ffe5a)
  - [Bug 1507149 - [LLNL 7.5 Bug] slab leak causing a crash when using kmem control group](https://bugzilla.redhat.com/show_bug.cgi?id=1507149)
- <https://lore.kernel.org/linux-mm/CA+CK2bCQcnTpzq2wGFa3D50PtKwBoWbDBm56S9y8c+j+pD+KSw@mail.gmail.com/t/>
- [ [Solved] dbus-org.freedesktop.Avahi.service missing from distribution?](https://bbs.archlinux.org/viewtopic.php?id=261924)
