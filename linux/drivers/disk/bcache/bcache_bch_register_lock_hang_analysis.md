# bcache 全局锁 bch_register_lock 长时间持有导致 sysfs 管理平面瘫痪的分析与修复

## 背景：bcache 在存储栈中的位置

bcache 是 Linux 内核的**块设备级缓存层**，位于文件系统和物理磁盘之间：

```
ext4 / xfs / LVM 等
    ↓
/dev/bcacheN          ← 对上层表现为普通块设备，ext4 直接 mount 在这个上
    ↓
bcache 内核模块       ← 缓存逻辑：热数据命中 SSD，冷数据落到 HDD
   ↙       ↘
/dev/nvme0n1p1      /dev/sdb
(cache 设备，SSD)    (backing 设备，HDD)
```

IO 路径是 **ext4 先写 `bcacheN`，bcache 再决定写到 SSD 缓存还是 HDD 后端**，不是 "先写 ext4 再写 bcache"，而是 **ext4 始终在 bcache 之上**。bcache 用快速的 SSD 当慢速 HDD 的缓存，加速随机读写。

本补丁涉及的 `bch_register_lock` 全局锁问题，就发生在 bcache 模块注册和管理这些 cache/backing 设备时。

---

## 1. 核心现象：大面积进程进入 State D (Uninterruptible Sleep)

日志文件 `messages.txt` 中记录了大量 `INFO: task ... blocked for more than 120 seconds` 警告。

### 证据 A：sysfs 配置进程阻塞（`bcache_cache_set_conf.sh`）

12 个 NVMe 分区的 `bcache_cache_set_conf.sh` 脚本全部阻塞在 `mutex_lock(&bch_register_lock)` 上，等待同一个全局锁。以下以 PID 3492（nvme0n1p11）的完整 Call Trace 为例，其余 11 个进程的 trace 完全一致：

```
Mar 12 11:04:17 kernel: [  242.656072] INFO: task bcache_cache_se:3492 blocked for more than 120 seconds.
Mar 12 11:04:17 kernel: [  242.664618]       Tainted: G           OE     4.19.326-0092.ctl3.aarch64 #1
Mar 12 11:04:17 kernel: [  242.672880] "echo 0 > /proc/sys/kernel/hung_task_timeout_secs" disables this message.
Mar 12 11:04:17 kernel: [  242.682037] task:bcache_cache_se state:D stack:    0 pid: 3492 ppid:     1 flags:0x00000801
Mar 12 11:04:17 kernel: [  242.691720] Call trace:
Mar 12 11:04:17 kernel: [  242.695174]  __switch_to+0x7c/0xbc
Mar 12 11:04:17 kernel: [  242.699571]  __schedule+0x338/0x6f0
Mar 12 11:04:17 kernel: [  242.704046]  schedule+0x50/0xe0
Mar 12 11:04:17 kernel: [  242.708164]  schedule_preempt_disabled+0x18/0x24
Mar 12 11:04:17 kernel: [  242.713756]  __mutex_lock.constprop.0+0x1d4/0x5ec
Mar 12 11:04:17 kernel: [  242.719429]  __mutex_lock_slowpath+0x1c/0x30
Mar 12 11:04:17 kernel: [  242.724657]  mutex_lock+0x50/0x60
Mar 12 11:04:17 kernel: [  242.728967]  bch_cache_set_store+0x40/0x80 [bcache]
Mar 12 11:04:17 kernel: [  242.734805]  sysfs_kf_write+0x4c/0x5c
Mar 12 11:04:17 kernel: [  242.739419]  kernfs_fop_write_iter+0x130/0x1c0
Mar 12 11:04:17 kernel: [  242.744809]  new_sync_write+0xec/0x18c
Mar 12 11:04:17 kernel: [  242.749497]  vfs_write+0x214/0x2ac
Mar 12 11:04:17 kernel: [  242.753837]  ksys_write+0x70/0xfc
Mar 12 11:04:17 kernel: [  242.758083]  __arm64_sys_write+0x24/0x30
Mar 12 11:04:17 kernel: [  242.762937]  invoke_syscall+0x50/0x11c
Mar 12 11:04:17 kernel: [  242.767605]  el0_svc_common.constprop.0+0x158/0x164
Mar 12 11:04:17 kernel: [  242.773407]  do_el0_svc+0x2c/0x9c
Mar 12 11:04:17 kernel: [  242.777644]  el0_svc+0x20/0x30
Mar 12 11:04:17 kernel: [  242.781606]  el0_sync_handler+0xb0/0xb4
Mar 12 11:04:17 kernel: [  242.786350]  el0_sync+0x160/0x180
```

全部 12 个阻塞进程（PID 3492/3493/3494/3496/3518/3519/3520/3521/3522/3523/3524/3526）的 Call Trace 完全一致，全部卡在 `mutex_lock+0x50/0x60` -> `bch_cache_set_store+0x40/0x80 [bcache]`。

### 证据 B：注册进程阻塞（`register_cache`）

另一个无关的 cache 设备注册也被同一个锁阻塞，PID 8104 阻塞在 `register_cache()`：

```
Mar 12 11:04:18 kernel: [  244.092476] INFO: task bcache:8104 blocked for more than 122 seconds.
Mar 12 11:04:18 kernel: [  244.099820]       Tainted: G           OE     4.19.326-0092.ctl3.aarch64 #1
Mar 12 11:04:18 kernel: [  244.107860] "echo 0 > /proc/sys/kernel/hung_task_timeout_secs" disables this message.
Mar 12 11:04:18 kernel: [  244.116777] task:bcache          state:D stack:    0 pid: 8104 ppid:     1 flags:0x00000a00
Mar 12 11:04:18 kernel: [  244.126240] Call trace:
Mar 12 11:04:18 kernel: [  244.129590]  __switch_to+0x7c/0xbc
Mar 12 11:04:18 kernel: [  244.133893]  __schedule+0x338/0x6f0
Mar 12 11:04:18 kernel: [  244.138279]  schedule+0x50/0xe0
Mar 12 11:04:18 kernel: [  244.142315]  schedule_preempt_disabled+0x18/0x24
Mar 12 11:04:18 kernel: [  244.147828]  __mutex_lock.constprop.0+0x1d4/0x5ec
Mar 12 11:04:18 kernel: [  244.153436]  __mutex_lock_slowpath+0x1c/0x30
Mar 12 11:04:18 kernel: [  244.158598]  mutex_lock+0x50/0x60
Mar 12 11:04:18 kernel: [  244.162833]  register_cache+0x11c/0x1a0 [bcache]
Mar 12 11:04:18 kernel: [  244.168361]  register_bcache+0x1d4/0x3cc [bcache]
Mar 12 11:04:18 kernel: [  244.173962]  kobj_attr_store+0x18/0x30
Mar 12 11:04:18 kernel: [  244.178608]  sysfs_kf_write+0x4c/0x5c
Mar 12 11:04:18 kernel: [  244.183161]  kernfs_fop_write_iter+0x130/0x1c0
Mar 12 11:04:18 kernel: [  244.188498]  new_sync_write+0xec/0x18c
Mar 12 11:04:18 kernel: [  244.193140]  vfs_write+0x214/0x2ac
Mar 12 11:04:18 kernel: [  244.197434]  ksys_write+0x70/0xfc
Mar 12 11:04:18 kernel: [  244.201607]  __arm64_sys_write+0x24/0x30
Mar 12 11:04:18 kernel: [  244.206391]  invoke_syscall+0x50/0x11c
Mar 12 11:04:18 kernel: [  244.210998]  el0_svc_common.constprop.0+0x158/0x164
Mar 12 11:04:18 kernel: [  244.216736]  do_el0_svc+0x2c/0x9c
Mar 12 11:04:18 kernel: [  244.220913]  el0_svc+0x20/0x30
Mar 12 11:04:18 kernel: [  244.224826]  el0_sync_handler+0xb0/0xb4
Mar 12 11:04:18 kernel: [  244.229522]  el0_sync+0x160/0x180
```

### 证据 C：锁持有者——正在做 `run_cache_set()` 的注册进程

PID 8103 是正在持锁的进程，它在 `run_cache_set()` -> `bch_btree_map_keys()` 中做 btree 遍历，同时持有着 `bch_register_lock`：

```
Mar 12 11:04:18 kernel: [  243.814774] INFO: task bcache:8103 blocked for more than 121 seconds.
Mar 12 11:04:18 kernel: [  243.822161]       Tainted: G           OE     4.19.326-0092.ctl3.aarch64 #1
Mar 12 11:04:18 kernel: [  243.830259] "echo 0 > /proc/sys/kernel/hung_task_timeout_secs" disables this message.
Mar 12 11:04:18 kernel: [  243.839220] task:bcache          state:D stack:    0 pid: 8103 ppid:     1 flags:0x00000a08
Mar 12 11:04:18 kernel: [  243.848695] Call trace:
Mar 12 11:04:18 kernel: [  243.852040]  __switch_to+0x7c/0xbc
Mar 12 11:04:18 kernel: [  243.856339]  __schedule+0x338/0x6f0
Mar 12 11:04:18 kernel: [  243.860722]  schedule+0x50/0xe0
Mar 12 11:04:18 kernel: [  243.864758]  rwsem_down_read_slowpath+0x1a0/0x660
Mar 12 11:04:18 kernel: [  243.870362]  down_read+0xa4/0xc0
Mar 12 11:04:18 kernel: [  243.874516]  bch_btree_map_keys+0xe8/0x180 [bcache]
Mar 12 11:04:18 kernel: [  243.880308]  cache_lookup+0x94/0x170 [bcache]
Mar 12 11:04:18 kernel: [  243.885578]  cached_dev_read.constprop.0+0xe8/0xf0 [bcache]
Mar 12 11:04:18 kernel: [  243.892065]  cached_dev_submit_bio+0x444/0x460 [bcache]
Mar 12 11:04:18 kernel: [  243.898199]  __submit_bio_noacct+0x98/0x27c
Mar 12 11:04:18 kernel: [  243.903285]  submit_bio_noacct+0x4c/0xb0
Mar 12 11:04:18 kernel: [  243.908112]  submit_bio+0x50/0x150
Mar 12 11:04:18 kernel: [  243.912416]  submit_bh_wbc+0x180/0x1e0
Mar 12 11:04:18 kernel: [  243.917065]  block_read_full_page+0x4b8/0x580
Mar 12 11:04:18 kernel: [  243.922322]  blkdev_readpage+0x24/0x30
Mar 12 11:04:18 kernel: [  243.926977]  do_read_cache_page+0x36c/0x530
Mar 12 11:04:18 kernel: [  243.932064]  read_cache_page+0x1c/0x30
Mar 12 11:04:18 kernel: [  243.936716]  read_part_sector+0x50/0x1a0
Mar 12 11:04:18 kernel: [  243.941535]  read_lba+0xcc/0x1b0
Mar 12 11:04:18 kernel: [  243.945659]  find_valid_gpt.constprop.0+0xc8/0x360
Mar 12 11:04:18 kernel: [  243.951350]  efi_partition+0x68/0x37c
Mar 12 11:04:18 kernel: [  243.955913]  check_partition+0x114/0x220
Mar 12 11:04:18 kernel: [  243.960733]  blk_add_partitions+0x44/0x1c0
Mar 12 11:04:18 kernel: [  243.965721]  bdev_disk_changed+0xb8/0x144
Mar 12 11:04:18 kernel: [  243.970617]  __blkdev_get+0x434/0x73c
Mar 12 11:04:18 kernel: [  243.975158]  blkdev_get+0x58/0xe4
Mar 12 11:04:18 kernel: [  243.979343]  blkdev_get_by_dev+0x40/0x6c
Mar 12 11:04:18 kernel: [  243.984130]  disk_init_partition+0x78/0x120
Mar 12 11:04:18 kernel: [  243.989173]  __device_add_disk+0x1b4/0x320
Mar 12 11:04:18 kernel: [  243.994132]  device_add_disk+0x1c/0x2c
Mar 12 11:04:18 kernel: [  243.998757]  bch_cached_dev_run+0xe0/0x2b0 [bcache]
Mar 12 11:04:18 kernel: [  244.004507]  bch_cached_dev_attach+0x31c/0x5f0 [bcache]
Mar 12 11:04:18 kernel: [  244.010605]  run_cache_set+0x24c/0x630 [bcache]
Mar 12 11:04:18 kernel: [  244.016008]  register_cache_set+0x20c/0x234 [bcache]
Mar 12 11:04:18 kernel: [  244.021846]  register_cache+0x124/0x1a0 [bcache]
Mar 12 11:04:18 kernel: [  244.027334]  register_bcache+0x1d4/0x3cc [bcache]
Mar 12 11:04:18 kernel: [  244.032906]  kobj_attr_store+0x18/0x30
Mar 12 11:04:18 kernel: [  244.037517]  sysfs_kf_write+0x4c/0x5c
Mar 12 11:04:18 kernel: [  244.042038]  kernfs_fop_write_iter+0x130/0x1c0
Mar 12 11:04:18 kernel: [  244.047340]  new_sync_write+0xec/0x18c
Mar 12 11:04:18 kernel: [  244.051948]  vfs_write+0x214/0x2ac
Mar 12 11:04:18 kernel: [  244.056206]  ksys_write+0x70/0xfc
Mar 12 11:04:18 kernel: [  244.060379]  __arm64_sys_write+0x24/0x30
Mar 12 11:04:18 kernel: [  244.065163]  invoke_syscall+0x50/0x11c
Mar 12 11:04:18 kernel: [  244.069770]  el0_svc_common.constprop.0+0x158/0x164
Mar 12 11:04:18 kernel: [  244.075508]  do_el0_svc+0x2c/0x9c
Mar 12 11:04:18 kernel: [  244.079685]  el0_svc+0x20/0x30
Mar 12 11:04:18 kernel: [  244.083598]  el0_sync_handler+0xb0/0xb4
Mar 12 11:04:18 kernel: [  244.088294]  el0_sync+0x160/0x180
```

**关键分析**：PID 8103 持有 `bch_register_lock` 走过的完整调用链，按层次拆解：

整个调用链从 `register_cache_set()` 获取 `bch_register_lock` 开始，穿越了 sysfs 层 → bcache 注册层 → 块设备层 → 分区表读取 → 磁盘 IO → 又回到 bcache 的 btree 查找，锁持有时间完全取决于磁盘 IO 和 btree 遍历速度，对 HDD 可能长达数分钟。

**注意：这个问题发生在 bcache 设备注册阶段，远在创建 ext4 文件系统之前。** 从 Call Trace 看，锁持有者在做的是 bcache 模块初始化 cache_set —— 把缓存设备（NVMe 分区）和后备设备（HDD）关联起来，让 `/dev/bcacheN` 出现。此时所有其他 bcache 设备的 sysfs 操作全部阻塞。ext4 是之后的事情：`/dev/bcacheN` 出现后，LVM 扫描到 PV，激活 VG/LV，然后 Ceph OSD 才会 mount 上面的 ext4。所以这个 hang 发生在整个存储栈的最底层 —— bcache 设备还没完全就绪，上游的 LVM 和文件系统都还没开始。

### 证据 D：udev 超时 kill/retry cascade

全部 12 个 NVMe 分区的 `bcache_cache_set_conf.sh` 脚本在 11:02:03 触发 59s 警告，在 11:04:03 全部超时被 kill，随后重试，在 11:07:04 和 11:10:05 再次全部超时 kill，形成稳定的 3 分钟周期：

```
# 第一轮 — 59s 警告 (11:02:03)
Mar 12 11:02:03 systemd-udevd[1618]: nvme1n1p11: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p11' [3526] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[2054]: nvme0n1p10: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p10' [3522] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1922]: nvme1n1p10: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p10' [3520] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1771]: nvme0n1p8: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p8' [3496] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[2033]: nvme1n1p12: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p12' [3521] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1594]: nvme1n1p8: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p8' [3524] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[2101]: nvme1n1p7: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p7' [3523] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[2014]: nvme0n1p9: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p9' [3494] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1729]: nvme0n1p11: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p11' [3492] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1648]: nvme0n1p7: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p7' [3493] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1646]: nvme1n1p9: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p9' [3519] is taking longer than 59s to complete
Mar 12 11:02:03 systemd-udevd[1713]: nvme0n1p12: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p12' [3518] is taking longer than 59s to complete

# 第一轮 — 超时 kill (11:04:03)
Mar 12 11:04:03 systemd-udevd[1771]: nvme0n1p8: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p8' [3496] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[2014]: nvme0n1p9: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p9' [3494] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1618]: nvme1n1p11: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p11' [3526] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[2033]: nvme1n1p12: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p12' [3521] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1594]: nvme1n1p8: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p8' [3524] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[2101]: nvme1n1p7: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p7' [3523] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[2054]: nvme0n1p10: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p10' [3522] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1922]: nvme1n1p10: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p10' [3520] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1648]: nvme0n1p7: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p7' [3493] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1646]: nvme1n1p9: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme1n1p9' [3519] timed out after 2min 59s, killing
Mar 12 11:04:03 systemd-udevd[1729]: nvme0n1p11: Spawned process '/usr/sbin/bcache_cache_set_conf.sh nvme0n1p11' [3492] timed out after 2min 59s, killing

# Worker 失败后重试
Mar 12 11:04:16 systemd-udevd[1587]: bcache0: Worker [1926] failed
Mar 12 11:04:16 systemd-udevd[1587]: bcache0: Retry 1 times.

# 同时 bcache cached_dev 的 udev 脚本也超时
Mar 12 11:04:17 systemd-udevd[1879]: bcache7: Spawned process '/usr/sbin/bcache_cached_dev_conf.sh /sys/devices/virtual/block/bcache7/bcache' [12892] timed out after 2min 59s, killing
Mar 12 11:04:17 systemd-udevd[1587]: bcache7: Worker [1879] failed
Mar 12 11:04:17 systemd-udevd[1587]: bcache7: Retry 1 times.

# 第二轮 — 11:05:04 再次全部 59s 警告，11:07:04 再次全部超时 kill
# 第三轮 — 11:08:05 再次全部 59s 警告，11:10:05 再次全部超时 kill
```

实际生产环境：Ceph OSD 节点，2 块 NVMe 各分 6 个分区，共 12 个 cache_set 缓存 12 块 HDD 后备设备。启动时每个 bcacheN 设备出现后 udev 触发调优脚本写入 sysfs，因为 `bch_register_lock` 被 PID 8103 在 `run_cache_set()` 中做 journal replay + btree check + cached_dev attach（其中包含 `device_add_disk` -> `check_partition` -> `efi_partition` -> `read_lba` 的完整磁盘 IO 路径）长期持有，所有设备的 sysfs 写操作全部阻塞。udev worker 超时后被 systemd-udevd kill，设备 add 事件失败并重试，kill/retry 循环每约 3 分钟重复一次，波及所有 NVMe 分区，导致整个 bcache 管理平面启动失败，进而延迟 LVM PV 激活和 Ceph OSD 启动。

---

## 2. 根源定位：`bch_register_lock` 的长时间持有

### 2.1 为什么会导致进程进入 D 状态？

根据 Call Trace 和源码逻辑，进程进入 D 状态（不可中断睡眠）的完整路径如下：

1.  **全局互斥锁抢占**：bcache 使用一个全局互斥锁 `bch_register_lock` 来保护几乎所有的 sysfs 操作和设备注册逻辑。
2.  **长时间持锁任务**：当系统注册新设备或启动时，`register_cache()` 在 `mutex_lock(&bch_register_lock)` 保护下调用 `register_cache_set()` -> `run_cache_set()` -> `bch_btree_check()`。
    -   `bch_btree_check` 需要遍历磁盘上的 B+ 树索引以检测一致性，对于大容量磁盘，这可能耗时**数分钟**。
    -   在此期间，`register_cache()` 一直持有全局 Mutex，直到 `register_cache_set()` 返回后才 `mutex_unlock()`。
3.  **并发请求阻塞**：此时，监控脚本（如 `cat dirty_data`）或 `udev` 脚本尝试操作 sysfs。这些操作在内核中也会调用 `mutex_lock(&bch_register_lock)`。
4.  **进入不可中断睡眠**：由于锁被占用，新进程无法获取锁，被内核放入等待队列，并标记为 `TASK_UNINTERRUPTIBLE`（即 D 状态）。
    -   由于是在内核态争抢这种关键资源，进程不响应任何信号（包括 `kill -9`），直到锁被释放。

### 2.2 具体被阻塞的接口

来自生产环境 call trace，三类接口被阻塞，全部在 `mutex_lock(&bch_register_lock)` 上：

| 接口 | 调用栈关键帧 | 场景 |
|------|-------------|------|
| `bch_cache_set_store()` | `mutex_lock` -> `bch_cache_set_store+0x40/0x80 [bcache]` | `bcache_cache_set_conf.sh` 脚本写 `congested_read_threshold_us` / `congested_write_threshold_us` 等调优参数到 cache_set 的 sysfs 节点 |
| `bch_cached_dev_store()` | `mutex_lock` -> `sysfs_kf_write` -> `kernfs_fop_write_iter` | `bcache_cached_dev_conf.sh` 脚本写 `sequential_cutoff` 到已 attach 的后备设备 bcacheN 的 sysfs 节点 |
| `register_cache()` | `mutex_lock` -> `register_cache+0x11c/0x1a0 [bcache]` | 注册另一个不相关的 cache 设备，也通过 `register_bcache` -> `kobj_attr_store` -> `sysfs_kf_write` 路径 |

实际日志中具体的 sysfs 写入操作证据：

```
Mar 12 11:01:14 kernel: bcache: bch_cached_dev_store() write file /sys/devices/.../block/sdb/bcache/sequential_cutoff: 1048576
Mar 12 11:01:15 kernel: bcache: bch_cached_dev_store() write file /sys/devices/.../block/sdc/bcache/sequential_cutoff: 1048576
...
Mar 12 11:04:17 kernel: [  242.656072] INFO: task bcache_cache_se:3492 blocked for more than 120 seconds.
```

注意：不仅是 "show" vs "store" 的问题，也不仅限于当前正在初始化的设备——**另一个无关设备自己的注册也被阻塞了**。PID 8103 作为锁持有者，在 `run_cache_set()` -> `bch_cached_dev_attach()` -> `bch_cached_dev_run()` -> `device_add_disk()` -> `check_partition()` -> `read_lba()` 的完整调用链中持续持有锁，这是磁盘 IO 密集路径，持续时间取决于磁盘性能。

---

## 3. 源码审计与日志 call trace 对比

1.  **持有者行为**：`register_cache()` 获取锁后调用 `register_cache_set()` -> `run_cache_set()`。
2.  **重度操作**：在 `run_cache_set()` 中，内核执行了 `bch_btree_check(c)`。
    -   这是一个耗时操作，会遍历整个 bset 树确认一致性。
    -   对于大容量或 IO 繁忙的磁盘，该过程可能持续数十秒甚至更久。
3.  **锁的颗粒度**：整个 `register_cache_set()` 过程一直持有 `bch_register_lock` (全局互斥锁)。
4.  **连带效应**：由于 `sysfs.c` 所有 show/store 都需要 `mutex_lock(&bch_register_lock)`，**任何** 对 **任何** bcache 设备的操作（无论是否属于正在初始化的那个 set）都必须等待这个锁，导致整个系统的 bcache 管理面瞬间瘫痪，产生大量 State D 进程。

---

## 4. 日志洪流 (Log Spamming)

-   **统计**：`bch_writeback_thread() dirty_data = 0` 打印了约 **8834 次**。
-   **影响**：这种高频 `pr_info` 在 IO 压力大或系统启动阶段会进一步拖累内核日志子系统，甚至可能导致 `dmesg` 缓冲区溢出，掩盖真正的错误。

---

## 5. 修复方案演进

### 5.1 v1/v2：Mutex -> rw_semaphore 方案（已废弃）

v1/v2 尝试将 `bch_register_lock` 从 `struct mutex` 转为 `struct rw_semaphore`，让 sysfs SHOW 路径用 `down_read()` 并发执行，并在 `run_cache_set()` 中通过 `downgrade_write()` 在 btree check 期间降级为读锁。

**废弃原因**（Coly Li 指出）：
1. **未解决根本问题**：`register_cache()` 仍需持有写锁贯穿整个 `register_cache_set()`，sysfs STORE 路径仍需独占写锁，所以与报告 hang 无关的设备的 store 操作仍然阻塞同样长的时间。
2. **KABI 顾虑**：改变全局变量类型（即使 bcache 内部未 EXPORT_SYMBOL）。
3. **真实回归**：v1 的 `downgrade_write()`/`up_read()`/`down_write()` 舞蹈在 `bch_btree_check()` 周围存在 race window——在 `up_read()` 和 `down_write()` 之间如果触发 unregister，可能导致 use-after-free。

### 5.2 v3：分阶段注册方案（最终方案，已修复）

v3 放弃了锁类型变更，改用**根因修复**：不改变 `bch_register_lock` 的类型（仍是 mutex），而是改变它被持有的时间段。

**核心思路**：把注册一个全新 cache_set 拆成两个阶段——

```
Phase 1（无锁恢复）:  journal replay + btree check + allocator start
Phase 2（加锁发布）:  kobject_add + sysfs 链接 + attach pending 设备
```

Phase 1 是耗时大户（数秒到数分钟），v3 让它完全不持有 `bch_register_lock`。Phase 2 是轻量操作（毫秒级），在锁保护下完成，与 `register_bdev_worker()` 现有的持锁成本一致。

---

## 6. v3 补丁逐文件代码分析

### 6.1 `bcache.h`：新增 `CACHE_SET_REGISTERING` 标志位

```diff
 #define CACHE_SET_UNREGISTERING       0
 #define CACHE_SET_STOPPING            1
 #define CACHE_SET_RUNNING             2
 #define CACHE_SET_IO_DISABLE          3
+#define CACHE_SET_REGISTERING         4
```

新增标志位 `CACHE_SET_REGISTERING`（bit 4），附有详细注释说明其语义：

> CACHE_SET_REGISTERING is set while a freshly allocated cache set is running its recovery path (journal replay, btree check, allocator start) without holding bch_register_lock. The cache set is already linked into bch_cache_sets at this point (so duplicate-uuid detection and reboot bookkeeping can see it), but its kobjects are not added to sysfs yet, so it is not reachable from userspace or from cache-set lookups that attach backing devices. Cleared once the cache set is fully published.

这个标志位的设计意图非常明确：**cache_set 在 `bch_cache_sets` 全局链表中可见（供 duplicate-uuid 检测和 `bcache_reboot()` 使用），但对 sysfs 和 `bch_cached_dev_attach()` 不可达**。这是一种"半可见"状态——链表可见、kobject 不可见。

### 6.2 `super.c`：核心重构

#### 6.2.1 `__uuid_write()` 的 lockdep 断言放宽

```diff
-	lockdep_assert_held(&bch_register_lock);
+	/*
+	 * Called either with bch_register_lock held, or from
+	 * bch_cache_set_recover() on a cache_set that is still
+	 * CACHE_SET_REGISTERING and therefore not yet reachable by any
+	 * other thread.
+	 */
+	if (!test_bit(CACHE_SET_REGISTERING, &c->flags))
+		lockdep_assert_held(&bch_register_lock);
```

**分析**：`__uuid_write()` 在无锁恢复路径的 fresh-cache 分支中会被调用。原来的无条件 `lockdep_assert_held()` 会在无锁路径上触发 lockdep 告警。修改后，如果是 `CACHE_SET_REGISTERING` 状态（即 cache_set 尚未对任何其他线程可见），则跳过断言——因为此时没有并发访问风险。

#### 6.2.2 `bch_cached_dev_attach()` 门控

```diff
+	if (test_bit(CACHE_SET_REGISTERING, &c->flags)) {
+		pr_err("Can't attach %pg: cache set is still registering\n",
+		       dc->bdev);
+		return -EINVAL;
+	}
```

**分析**：在 attach 入口处增加门控。如果 cache_set 还在 `CACHE_SET_REGISTERING` 状态（即恢复尚未完成），拒绝 backing device 的 attach 请求。这防止了在 cache_set 完全初始化之前就 attach backing device 的风险。

#### 6.2.3 函数拆分：`run_cache_set()` -> `bch_cache_set_recover()` + `bch_cache_set_attach_devices()`

原来的 `run_cache_set()` 做了两件事：恢复 + 发布。v3 将其拆分为两个独立函数：

**`bch_cache_set_recover()`**（无锁恢复阶段）：

```diff
-static int run_cache_set(struct cache_set *c)
+/*
+ * Recover a cache_set from disk (journal replay, btree check, allocator
+ * start) or initialize a brand new one. This is the expensive, I/O-bound
+ * part of bringing a cache_set up.
+ *
+ * For a freshly allocated cache_set, this is called without
+ * bch_register_lock held: the cache_set is marked CACHE_SET_REGISTERING
+ * and is not yet reachable via c->kobj/sysfs or via any attach lookup
+ * ...
+ */
+static int bch_cache_set_recover(struct cache_set *c)
 {
 	const char *err = "cannot allocate memory";
-	struct cached_dev *dc, *t;
 	struct cache *ca = c->cache;
 	struct closure cl;
 	LIST_HEAD(journal);
```

关键变化：函数签名中去掉了 `struct cached_dev *dc, *t` 局部变量声明——因为 attach 逻辑被移到了 `bch_cache_set_attach_devices()` 中。这个函数**只做纯粹的磁盘恢复**（journal replay, btree check, allocator start），不再涉及设备 attach。

结尾处也移除了原来在 `run_cache_set()` 末尾的 attach + RUNNING 标志位设置：

```diff
-	list_for_each_entry_safe(dc, t, &uncached_devices, list)
-		bch_cached_dev_attach(dc, c, NULL);
-
-	flash_devs_run(c);
-
-	bch_journal_space_reserve(&c->journal);
-	set_bit(CACHE_SET_RUNNING, &c->flags);
	return 0;
```

**`bch_cache_set_attach_devices()`**（加锁发布阶段）：

```c
+/*
+ * Publish a recovered cache_set: attach pending backing/flash devices and
+ * mark it running. Must be called with bch_register_lock held. Unlike
+ * bch_cache_set_recover(), nothing here can fail.
+ */
+static void bch_cache_set_attach_devices(struct cache_set *c)
+{
+	struct cached_dev *dc, *t;
+
+	lockdep_assert_held(&bch_register_lock);
+
+	list_for_each_entry_safe(dc, t, &uncached_devices, list)
+		bch_cached_dev_attach(dc, c, NULL);
+
+	flash_devs_run(c);
+
+	bch_journal_space_reserve(&c->journal);
+	set_bit(CACHE_SET_RUNNING, &c->flags);
+}
```

**分析**：这个函数是原来 `run_cache_set()` 末尾的 attach 逻辑的独立封装。加了 `lockdep_assert_held(&bch_register_lock)` 强制调用者必须持有锁。返回值是 `void`（不会失败），因为恢复已经完成，attach 只是将已就绪的设备挂上去。

**新的 `run_cache_set()`**（仅用于 "found" 路径）：

```c
+/*
+ * Recover and publish a cache_set as a single locked unit. Used only for
+ * attaching a cache device to an already-published cache_set (the "found:"
+ * case in register_cache_set()) — that cache_set is already reachable via
+ * bch_cache_sets/sysfs, so it must not go through the unlocked recovery
+ * path used for a brand new cache_set.
+ */
+static int run_cache_set(struct cache_set *c)
+{
+	int ret;
+
+	lockdep_assert_held(&bch_register_lock);
+
+	ret = bch_cache_set_recover(c);
+	if (ret)
+		return ret;
+
+	bch_cache_set_attach_devices(c);
+	return 0;
+}
```

**分析**：保留了 `run_cache_set()` 函数名，但语义完全变了——它现在是**带锁的恢复+发布合一**，仅用于 "found" 路径（即 cache device 重新 attach 到一个已经发布过的 cache_set）。因为那个 cache_set 已经通过 sysfs 可达，必须全程持锁。`lockdep_assert_held()` 保证调用者不会错误地在无锁场景下调用它。

#### 6.2.4 `register_cache_set()` 重构——核心注册流程

这是整个补丁最关键的变更，原始函数被完全重写以支持分阶段注册：

```c
 static const char *register_cache_set(struct cache *ca)
 {
 	char buf[12];
 	const char *err = "cannot allocate memory";
 	struct cache_set *c;
+	bool fresh = false;
+
+	mutex_lock(&bch_register_lock);
```

**分析**：锁的获取从 `register_cache()` 移到了 `register_cache_set()` 内部。这样 `register_cache_set()` 可以在不同阶段自主决定持锁/释放锁，而 `register_cache()` 的调用者不再需要关心锁。

**Duplicate-UUID 检测（锁内）**：

```c
 	list_for_each_entry(c, &bch_cache_sets, list)
 		if (!memcmp(c->set_uuid, ca->sb.set_uuid, 16)) {
-			if (c->cache)
+			if (c->cache) {
+				mutex_unlock(&bch_register_lock);
 				return "duplicate cache set member";
+			}

 			goto found;
 		}
```

**分析**：遍历全局 `bch_cache_sets` 链表查找同 UUID 的 cache_set。如果找到且 `c->cache` 已设置（说明该 cache_set 已有活跃的 cache 设备），返回 "duplicate cache set member"。注意这里在返回前需要 `mutex_unlock()`——因为锁现在在函数内部获取。如果 `c->cache == NULL`（即 cache_set 存在但 cache 设备丢失了，需要重新 attach），走 `goto found` 路径。

**新 cache_set 分配与标记（锁内）**：

```c
 	c = bch_cache_set_alloc(&ca->sb);
-	if (!c)
+	if (!c) {
+		mutex_unlock(&bch_register_lock);
 		return err;
+	}
+
+	/*
+	 * Link the new cache_set into bch_cache_sets right away, marked
+	 * CACHE_SET_REGISTERING, before dropping the lock for recovery.
+	 * bch_cache_set_alloc() already set c->cache = ca above, so the
+	 * duplicate-uuid check above will correctly reject a second
+	 * concurrent registration for the same uuid while this one is
+	 * mid-recovery.
+	 */
+	set_bit(CACHE_SET_REGISTERING, &c->flags);
+	list_add(&c->list, &bch_cache_sets);
+	fresh = true;
+
+	mutex_unlock(&bch_register_lock);
```

**分析**：这是分阶段注册的核心。在同一个锁临界区内完成三件事：
1. `bch_cache_set_alloc()`（已设置 `c->cache = ca`）
2. `set_bit(CACHE_SET_REGISTERING, &c->flags)`（标记为注册中）
3. `list_add(&c->list, &bch_cache_sets)`（加入全局链表）

然后**立即释放锁**。此时 cache_set 对全局链表遍历可见（duplicate-uuid 检测可用），但对 sysfs 和 attach 不可达。`fresh = true` 标记这是新分配的 cache_set，后续用于区分走哪条路径。

**无锁恢复阶段**：

```c
+	err = "failed to recover cache set";
+	if (bch_cache_set_recover(c) < 0) {
+		mutex_lock(&bch_register_lock);
+		goto err;
+	}
+
+	mutex_lock(&bch_register_lock);
```

**分析**：在无锁状态下调用 `bch_cache_set_recover()` 进行 journal replay + btree check + allocator start。这是最耗时的阶段，现在完全不阻塞其他设备的 sysfs 操作。恢复失败时重新获取锁，跳转到 `err` 标签执行清理。恢复成功时重新获取锁，进入发布阶段。

**发布阶段（锁内）**：

```c
 	err = "error creating kobject";
 	if (kobject_add(&c->kobj, bcache_kobj, "%pU", c->set_uuid) ||
 	    kobject_add(&c->debug, &c->kobj, "debug") ||
 	    ...)
 		goto err;

 	bch_debug_init_cache_set(c);
-
-	list_add(&c->list, &bch_cache_sets);
+	clear_bit(CACHE_SET_REGISTERING, &c->flags);
```

**分析**：`kobject_add()` 创建 sysfs 入口，这是 cache_set 首次对用户空间可见。成功后清除 `CACHE_SET_REGISTERING` 标志位——此时 cache_set 完全就绪。注意原来的 `list_add()` 被移到了无锁恢复之前（见上文），这里不再是首次加入链表。

**发布完成后的路径选择**：

```c
 	err = "failed to run cache set";
-	if (run_cache_set(c) < 0)
+	if (fresh)
+		bch_cache_set_attach_devices(c);
+	else if (run_cache_set(c) < 0)
 		goto err;

+	mutex_unlock(&bch_register_lock);
 	return NULL;
```

**分析**：这是路径选择的关键：
- **`fresh` 路径**：新分配的 cache_set，恢复已经在无锁阶段完成，这里只需调用 `bch_cache_set_attach_devices()`（纯加锁发布，不会失败）。
- **`found` 路径**：cache_set 已存在但 cache 设备丢失，需要重新恢复。调用 `run_cache_set()`（带锁的恢复+发布合一），因为该 cache_set 已经通过 sysfs 可达，必须全程持锁。

**错误路径**：

```c
 err:
+	mutex_unlock(&bch_register_lock);
 	bch_cache_set_unregister(c);
 	return err;
```

**分析**：错误路径也需要 `mutex_unlock()`，因为函数现在自己管理锁。`bch_cache_set_unregister()` 在锁释放后调用，这与原来的行为一致。

#### 6.2.5 `register_cache()` 简化

```diff
-	mutex_lock(&bch_register_lock);
 	err = register_cache_set(ca);
-	mutex_unlock(&bch_register_lock);
```

**分析**：`register_cache()` 不再负责获取/释放锁，因为锁管理已经完全移入 `register_cache_set()` 内部。这简化了调用者，也让锁的持有/释放更加精细——`register_cache_set()` 可以在无锁恢复期间释放锁，而不需要 `register_cache()` 知道这些细节。

### 6.3 `sysfs.c`：`__bch_cache` 的 SHOW/STORE 门控

```diff
 SHOW(__bch_cache)
 {
 	struct cache *ca = container_of(kobj, struct cache, kobj);

+	/*
+	 * ca->set is still being recovered without bch_register_lock held
+	 * (see bch_cache_set_recover()); its bucket_lock and on-disk
+	 * superblock are not safe to touch yet.
+	 */
+	if (ca->set && test_bit(CACHE_SET_REGISTERING, &ca->set->flags))
+		return -EAGAIN;
+
 	sysfs_hprint(bucket_size,	bucket_bytes(ca));
```

```diff
 STORE(__bch_cache)
 {
 	...
 	if (bcache_is_reboot)
 		return -EBUSY;

+	/*
+	 * ca->set is still being recovered without bch_register_lock held
+	 * (see bch_cache_set_recover()); its bucket_lock and on-disk
+	 * superblock are not safe to touch yet.
+	 */
+	if (ca->set && test_bit(CACHE_SET_REGISTERING, &ca->set->flags))
+		return -EAGAIN;
+
```

**分析**：在 `__bch_cache` 的 SHOW 和 STORE 入口增加 `CACHE_SET_REGISTERING` 检查。如果 cache_set 仍在恢复中（标志位置位），返回 `-EAGAIN` 让用户空间稍后重试。这是因为此时 `ca->set` 的 `bucket_lock` 和 on-disk superblock 正在被恢复过程写入，读取不安全。

注意：这里返回 `-EAGAIN` 而不是阻塞等待，是比原来的行为更优的选择——原来调用者会进入 D 状态阻塞数分钟，现在立即返回错误让用户空间决定是否重试。对于 `bcache_cached_dev_conf.sh` 这类 udev 脚本，快速失败并重试比长时间阻塞更合理。

---

## 7. 修复前后对比

| | 修复前 | 修复后 (v3) |
|---|---|---|
| `bch_register_lock` 类型 | `struct mutex` | `struct mutex`（不变） |
| 持锁范围 | `register_cache_set()` 整个过程 | 仅 allocation + list_add + kobject_add + attach |
| 恢复期间持锁 | 是（数分钟） | 否（完全不持锁） |
| 无关设备的 sysfs 访问 | 阻塞 | 不受影响 |
| 无关设备的注册 | 阻塞 | 不受影响 |
| sysfs 访问正在初始化的 cache_set | 进入 D 状态阻塞 | 立即返回 `-EAGAIN` |
| KABI | — | 无影响（锁类型不变） |

---

## 8. 补丁链接

- v3: https://lore.kernel.org/r/20260706-feat-bcache-v3-1-8824683656ca@gmail.com
- v2: https://lore.kernel.org/r/20260706-feat-bcache-v2-1-70a4b6e246c0@gmail.com
- v1: https://lore.kernel.org/r/20260318-wujing-bcache-v1-1-f0b9aaf3f81d@gmail.com

---

## 9. 验证结果

v3 补丁已在云基础设施的生产环境中部署测试，验证结果如下：

- **udev 超时重试消除**：之前启动时每个 bcache 设备的 udev worker 都会因 sysfs 写操作阻塞超时，被 systemd-udevd kill 后重试，循环重复。v3 后不再复现。
- **管理平面恢复**：12 个 cache_set 并行启动时，已运行的 cache_set 的 sysfs 调优操作不再被其他 cache_set 的恢复过程阻塞，LVM PV 激活和 Ceph OSD 启动正常进行。
- **Duplicate-UUID 检测正常**：并发注册同一 UUID 的 cache device 仍被正确拒绝。
- **`bcache_reboot()` 路径分析**：`bch_cache_set_recover()` 只在刚分配的 cache_set 上无锁运行，`CACHE_SET_REGISTERING` 置位和 `list_add()` 在同一个锁临界区内完成，所以 `bcache_reboot()` 的 `list_empty(&bch_cache_sets)` 检查和 `mutex_lock()` 保护的等待循环能在恢复期间看到该 cache_set，不存在窗口期。

---

> [!TIP]
> **结论**：v3 方案从根因出发，不改变锁类型，而是精细控制锁的持有时间窗口——将耗时的恢复操作（`bch_cache_set_recover()`）移出锁外，轻量的发布操作（`bch_cache_set_attach_devices()`）留在锁内。通过 `CACHE_SET_REGISTERING` 标志位在全局链表可见性和 sysfs 可达性之间建立了"半可见"状态，确保并发安全。修复后，bcache 管理平面不再因单个 cache_set 的恢复而全局瘫痪，生产环境验证通过。