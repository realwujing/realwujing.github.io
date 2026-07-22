# 清 log 挂死与 UUID 冲突导致系统盘损坏

## 环境信息

- 复现主机：`testzc0721`（同一台机器上先后跑了两个内核版本做对比）
- 内核版本 A：`3.10.0-957.el7.x86_64`（CentOS/RHEL 7.6）——对应 `centos_dmesg.txt`
- 内核版本 B：`4.19.90-2102.2.0.0062.ctl2.x86_64`（CTyunOS）——对应 `ctyunos_dmesg.txt`
- 涉及磁盘：`/dev/vdb1` / `/dev/vdc1`，均为 virtio-blk 云盘，XFS V5 文件系统
- 触发操作：A 是 `mount -o nouuid /dev/vdc1 /datatest/`；B 是 `xfs_repair -L /dev/vdb1`

## 问题现象

### 现象一：mount / xfs_repair -L 在清 log 阶段挂死

**A（3.10，`centos_dmesg.txt`）**：`mount -o nouuid /dev/vdc1 /datatest/` 之后，`mount` 进程 D 状态挂死，hung_task 每 120 秒告警一次，反复多次（>6 分钟未恢复），调用栈始终不变：

```
xfs_buf_submit_wait -> wait_for_completion -> schedule_timeout
  xfs_bwrite
  xlog_bwrite
  xlog_write_log_records
  xlog_clear_stale_blocks
  xlog_find_tail
  xlog_recover
  xfs_log_mount
  xfs_mountfs
  xfs_fs_fill_super -> mount_bdev -> ... -> SyS_mount
```

**B（4.19，`ctyunos_dmesg.txt`）**：`xfs_repair -L /dev/vdb1` 卡在 "Phase 2 - zero log" 阶段，同样反复 hung_task 告警，调用栈：

```
submit_bio_wait -> wait_for_completion_io
  blkdev_issue_zeroout
  blkdev_fallocate (FALLOC_FL_ZERO_RANGE)
  vfs_fallocate -> ksys_fallocate -> __x64_sys_fallocate
```

两次卡住的操作本质相同：**对 XFS log 区域做清零写**，只是内核版本不同，清零走的内核代码路径不同（A 走 xfs 自身同步 buffer 写；B 的 `blkdev_issue_zeroout` 会优先尝试 `WRITE_ZEROES`/discard 类硬件卸载命令）。两份 dmesg 里都没有任何 XFS 损坏检测信息（无 `Internal error`、无 corruption 报错），说明不是文件系统数据本身损坏导致的报错退出，而是**提交的 I/O 请求提交后一直没有 completion 返回**。

行为差异：3.10 环境下 `xfs_repair -L` **有概率**修复成功、进系统；4.19 环境下 `xfs_repair -L` **必现**卡死，从未修复成功过。

### 现象二：xfs_repair -L 处理过的盘接到正常 VM 后，系统盘随之损坏

`xfs_repair -L` 处理过的盘，添加到一台正常的 VM 后无法 mount；强制重启该 VM 后，**该 VM 原本的系统盘也损坏、进不去了**。

## 根因分析

### 挂死是否是内核代码 bug

对当前内核源码树（本仓库 `linux`，7.2-rc1 合并窗口附近）做了直接核实：

1. `fs/xfs/xfs_buf.c:1298-1306` 的 `xfs_buf_iowait()` 用的是无超时版本的 `wait_for_completion(&bp->b_iowait)`，没有任何超时保护。
2. `drivers/block/virtio_blk.c:1234-1240` 的 `virtio_mq_ops` **没有实现 `.timeout` 回调**。
3. `block/blk-mq.c:1626-1639` 的 `blk_mq_rq_timed_out()`：驱动没实现 `.timeout` 时，块层通用超时机制只会不断重新挂表（`blk_add_timer`），**永远不会把请求判失败**。
4. `block/blk-lib.c` 里 `blkdev_issue_zeroout()` 的 `WRITE_ZEROES` → 普通写 fallback，只在设备**明确报错**时触发（`ret && !bdev_write_zeroes_sectors(bdev)` 才返回 `-EOPNOTSUPP`），如果请求既不成功也不报错（无响应），fallback 逻辑走不到。

结论：**只要宿主机/传输层把一个 virtio-blk 请求"吞掉"不回应，guest 内核在任何版本（包括当前最新主线）下都会永久挂死，没有任何自愈机制。** 这是 virtio_blk 一直以来的设计取向（信任传输层一定会完成请求），当前主线仍未变化，属于确认存在的防护缺口，但不是某次改坏了又没人管的 regression。

### 挂死是否是磁盘数据损坏

两份 dmesg 里均无 XFS 损坏检测信息；且需要清 log/清 stale blocks 本身是"非正常关机后进入 log recovery"的正常场景，不等于数据损坏。更可能的触发条件是**宿主机存储/虚拟化后端对某类清零写命令（`WRITE_ZEROES`/discard）处理异常**，导致请求无响应，叠加上面内核侧毫无超时保护，共同造成永久挂死。

### 复现实验记录（`blkdiscard -z`，绕开 XFS/xfs_repair 直接测块层）

在 `testzc0721` 上用 `blkdiscard -v -z -o <offset> -l <len> /dev/vdb1` 直接测试 `BLKZEROOUT`（与 `blkdev_issue_zeroout` 同一内核路径）：

- `-o 0 -l 4M`、`-o 0 -l 8M`、`-o 0 -l 16M` 均**秒成功**，未复现挂死。
- 副作用：`-o 0 -l 16M` 把 `/dev/vdb1` 开头 16MB（含主超级块）清零，导致后续 `xfs_db -c "sb 0" -c print` 报 `unexpected SB magic number 0x00000000`，无法再从主超级块查 log 位置。
- 待办：改用备份超级块（`xfs_db -c "sb 1"`）或新建同规格 scratch 盘获取 `logstart`/`logblocks`，精确定位到 log 实际所在 offset 后再测，目前**尚未在真实 log 区域复现挂死**，"宿主机后端清零命令处理异常"这一具体触发条件仍是假设，未实锤。

### 衍生现象：系统盘损坏，高度怀疑 UUID 冲突

最早的 mount 命令是 `mount -o nouuid /dev/vdc1 /datatest/`——特意加 `-o nouuid` 说明这块盘的 XFS UUID 与当时系统里另一块已挂载文件系统的 UUID 冲突（否则内核会直接拒绝挂载）。

结合"新盘接到正常 VM 后无法 mount，强制重启后系统盘也损坏"的现象，高度怀疑：`xfs_repair -L` 处理过的盘与该正常 VM 的系统盘**共享同一个 XFS UUID**（同镜像/模板克隆产生），导致 `blkid`/udev 缓存或 `UUID=` 解析在两块盘之间发生混淆，使本该针对新盘的操作（或 boot 时按 UUID 找 root 的逻辑）作用到了错误的设备上。另一种可能是 `/dev/vdX` 瞬态设备名在新盘接入后发生漂移，导致引用设备名的配置（fstab/grub.cfg）指向错误的盘。这两点尚未做隔离环境的对照实验验证，只是基于现象的推断。

## 修复步骤（已验证：CentOS、CTyunOS 下 mount 均恢复正常）

改 UUID 解决了"接到正常 VM 后无法 mount"的问题：

### 1. 先确认确实有冲突

```bash
blkid /dev/vdb1          # 这块出问题的新盘
blkid /dev/vda1          # 系统盘（设备名以实际为准）
```

对比两边的 `UUID=` 是不是真的一样。

### 2. 改 UUID 前必须先卸载

```bash
umount /dev/vdb1   # 确认这块盘没被挂载,而且没有进程在用(lsof/fuser 检查一下)
```

`xfs_admin` 改 UUID 要求文件系统处于**卸载状态**,挂着改是不允许的(会拒绝执行,不会让你在挂载状态下搞出更糟的情况)。

### 3. 生成一个新的随机 UUID

```bash
xfs_admin -U generate /dev/vdb1
```

- `generate` 让它随机生成一个全新 UUID,和系统盘/其他盘都不会撞
- 也可以手动指定:`xfs_admin -U <你指定的uuid> /dev/vdb1`
- 确认改完:`blkid /dev/vdb1` 再看一遍,确认 UUID 已经变了

### 4. 刷新系统的 blkid 缓存

改完 UUID 后,系统里可能还缓存着旧的 UUID→设备 映射,尤其是 udev:

```bash
udevadm trigger --subsystem-match=block
udevadm settle
blkid -g          # 清一下 blkid 自己的缓存文件 /run/blkid/blkid.tab
```

稳妥起见,改完之后**重启一次**确认干净,别留旧缓存的隐患。

### 重要提醒

1. **只改这块新盘,别动系统盘的 UUID。** 系统盘如果有 `/etc/fstab`、grub.cfg、initramfs 里通过 `UUID=` 引用自己,改了会直接把系统盘也搞得起不来。
2. **改 UUID 只能防止"以后"再发生同类混淆,不能撤销已经发生的损坏。** 如果之前那次系统盘已经真被误操作坏了,这个操作救不回来,该盘该修还是得单独修。
3. 如果这块新盘将来还要被"正常 VM"引用挂载(比如写进它自己的 fstab),记得对应更新成新 UUID,否则新盘自己都挂不上。
4. 如果之后还想复现/验证 UUID 冲突到底是不是根因,可以先在一个**隔离的测试环境**里故意用 `dd`/`xfs_admin -U <same-uuid>` 制造出两块 UUID 相同的盘,接到同一台测试 VM 上,重启看看是否真的会复现"系统盘被误操作"这个现象——这样能确认根因,而不是停留在假设上。

## 生产环境完整处置流程（OpenStack/Nova 实操记录）

以下是在真实 OpenStack/Nova 环境下，针对"系统盘/数据盘因本文档描述的挂死问题损坏、无法直接在故障主机上修复"的场景，借助一台测试云主机挂载磁盘离线修复、再挂回故障主机的完整操作记录。磁盘 ID、云主机 ID 均为示例，实际操作以当次故障对应的真实 ID 为准。

**⚠️ 开始前必须对客户主机磁盘做好备份。**

示例 ID（仅用于说明命令格式，不代表真实故障）：

- 创建的测试云主机：`1dfff170-60b0-4c9a-a018-fa39a0d5e4ca`
- 创建的临时数据盘：`06f0fd78-586e-4c23-a41f-a897bd12ce50`
- 故障主机 ID：`28d6c84a-2613-4eb1-801f-dc153e2d8096`
- 系统盘 ID：`eb9eba3c-04c7-4740-a10a-71e1d3dd14a5`
- 数据盘 ID：`da38baf2-ef92-4d9c-a113-70ecf5f42f19`

### 一、修复系统盘（借助测试云主机离线处理）

> 本步骤磁盘 ID 以实际故障当次提供的为准（下面均为示例）。

**1. control 节点：关闭要维修的云主机**

```bash
# 操作前需先确认好可挂载修复的磁盘 ID
```

**2. 将（系统盘对应的）备份磁盘挂载至测试云主机**

```bash
nova volume-attach 1dfff170-60b0-4c9a-a018-fa39a0d5e4ca 06f0fd78-586e-4c23-a41f-a897bd12ce50
```

**3. 测试云主机内执行**

```bash
# 查看修复磁盘是否存在
lsblk

# 执行修复
xfs_repair -L /dev/vdb1

# 修改 UUID
xfs_admin -U generate /dev/vdb1

# 查看 UUID 是否修改成功
blkid

# 修改 fstab（按需，若 fstab 里引用了旧 UUID）
vi /mnt/etc/fstab

umount /mnt
```

**4. 控制节点：卸盘、后台改名、启动故障主机、救援模式重建引导**

```bash
nova volume-detach 1dfff170-60b0-4c9a-a018-fa39a0d5e4ca 06f0fd78-586e-4c23-a41f-a897bd12ce50
```

后台 rename rbd 卷（存储侧操作，把修复好的卷改回故障主机原来引用的卷名/路径）。

```bash
nova start 故障主机ID
```

救援模式依次在计算节点执行：

```bash
mkdir /mnt
mount /dev/vda1 /mnt
mount -o bind /dev /mnt/dev
mount -o bind /dev/pts /mnt/dev/pts
mount -o bind /proc /mnt/proc
mount -o bind /run /mnt/run
mount -o bind /sys /mnt/sys
chroot /mnt

dracut -f --regenerate-all   # CTyunOS 略有差异，具体操作前咨询基础架构侧确认

grub2-mkconfig -o /boot/grub2/grub.cfg
grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg

cat /boot/grub2/grub.cfg | grep 修改后的UUID   # 确认新 UUID 已写入引导配置

umount -R /mnt/
exit
reboot
```

计算节点硬重启故障主机：

```bash
virsh destroy 28d6c84a-2613-4eb1-801f-dc153e2d8096 && virsh start 28d6c84a-2613-4eb1-801f-dc153e2d8096
```

### 二、修复数据盘（借助测试云主机离线处理）

> 本步骤磁盘 ID 以实际故障当次提供的为准（下面均为示例），操作前需先确认好可挂载修复的磁盘 ID。

**1. 控制节点执行**

```bash
nova volume-attach 1dfff170-60b0-4c9a-a018-fa39a0d5e4ca da38baf2-ef92-4d9c-a113-70ecf5f42f19
```

**2. 测试云主机内执行**

```bash
lsblk   # 查看数据盘符

xfs_repair -L /dev/vdb

# 修改 UUID
xfs_admin -U generate /dev/vdb

mount /dev/vdb /mnt
ll /mnt   # 查看是否可看到数据
umount /mnt
```

**3. 后台 rename 磁盘信息**（存储侧操作）

**4. 计算节点执行**

```bash
nova volume-detach 1dfff170-60b0-4c9a-a018-fa39a0d5e4ca da38baf2-ef92-4d9c-a113-70ecf5f42f19
```

**5. 挂载修复好的磁盘至报障主机**

```bash
nova volume-attach 28d6c84a-2613-4eb1-801f-dc153e2d8096 da38baf2-ef92-4d9c-a113-70ecf5f42f19
```

### 说明

- 系统盘的修复比数据盘多一步"救援模式重建 initramfs/grub 配置"，原因是系统盘的 UUID 被 grub.cfg / initramfs 引用来定位 root 分区，改了 UUID 之后如果不同步 `dracut -f --regenerate-all` + `grub2-mkconfig` 重新生成，故障主机会在启动阶段就因为找不到 root 而起不来——这也和本文档前面分析的"UUID 冲突导致系统盘损坏"高度相关：系统盘一旦 UUID 被旧的引导配置引用错，同样会表现为"进不去系统"。
- 数据盘没有引导依赖，改完 UUID 挂载验证数据可见即可，流程更简单。
- 全程通过一台专用的测试云主机离线操作，不在故障主机或其他生产云主机上直接执行 `xfs_repair -L`/`xfs_admin`，避免了本文档前面提到的"新盘接到正常 VM 后连带损坏系统盘"的风险再次发生。

## 结论

1. **清 log 阶段挂死**：内核代码层面确认存在防护缺口（virtio_blk 无超时机制，当前主线仍未修），但这只是"没有兜底"，不是"制造故障"；真正的触发条件（宿主机/存储后端对清零命令处理异常）目前只有间接证据（两个不同内核版本、不同代码路径均卡在清零写上），尚未在真实 log offset 上直接复现坐实，也不是磁盘数据损坏导致。
2. **系统盘损坏**：高度怀疑与 XFS UUID 冲突有关（最初 mount 需要 `nouuid` 已经暗示了这一点），通过 `xfs_admin -U generate` 给新盘换一个不冲突的 UUID 后，在 CentOS、CTyunOS 两种环境下问题均消失、mount 恢复正常，两个不同发行版上都验证有效，进一步支持"UUID 冲突"是根因而非某个发行版特有的偶然现象；但尚未通过隔离环境的对照实验完全排除设备名漂移等其他可能性。

## 后续待办

1. 精确定位 log 在盘上的 byte offset（用备份超级块 `xfs_db -c "sb 1"` 或新建同规格 scratch 盘），针对真实 log 区域重跑 `blkdiscard -z` 复现实验，坐实"宿主机清零命令处理异常"这一根因。
2. 在隔离测试环境中人为构造两块 UUID 相同的盘接入同一 VM，验证是否必现"系统盘被误操作/损坏"，从而把 UUID 冲突从"高度怀疑"升级为"确认根因"。
3. 如果 1、2 两点都能复现，建议同时推动：宿主机/存储侧排查 `WRITE_ZEROES`/discard 命令处理逻辑；以及在涉及磁盘克隆/挂载的运维流程里强制要求先检查并规避 UUID 冲突（例如统一在克隆后自动跑一次 `xfs_admin -U generate`）。
