# xfs_inodegc_flush 导致 statfs 阻塞引发大面积 hung_task

## 环境信息

- 磁盘故障处理，华东1 资源池
- 主机ID：bef9d276-d2af-7729-90e9-939b
- 内核版本：`5.10.0-136.12.0.92.ctl3.x86_64`
- Hypervisor：KVM（`GoStack Foundation OpenStack Nova`）
- 故障磁盘：`vdb`，virtio-blk，6291456000 个 512 字节逻辑块（3.22 TB / 2.93 TiB），XFS V5 文件系统
- 系统盘 `vda1` 同为 XFS，未受影响

## 问题现象

`2026-07-21 15:11:00` 与 `15:13:03`（hung_task 检测器默认 120 秒巡检一次，两次告警是同一次卡死在相邻巡检周期分别被记录，阻塞时长从 122 秒累加到 245 秒），大量进程进入 D 状态：

```
INFO: task 19100_node_expo:2394 blocked for more than 122 seconds.
INFO: task BgSchPool:3519866 blocked for more than 122 seconds.
INFO: task AsyncMetrics:3520121 blocked for more than 122 seconds.
INFO: task kworker/u34:3:3702130 blocked for more than 122 seconds.
INFO: task kworker/2:0:3702423 blocked for more than 122 seconds.
INFO: task telegraf:2335 blocked for more than 122 seconds.
INFO: task 19100_node_expo:2394 blocked for more than 245 seconds.
INFO: task titanagent:2970602 blocked for more than 122 seconds.
INFO: task BgSchPool:3519866 blocked for more than 245 seconds.
INFO: task AsyncMetrics:3520121 blocked for more than 245 seconds.
```

被卡住的进程分两类：

1. **一堆用户态监控/agent 进程**（`19100_node_expo`、`BgSchPool`、`AsyncMetrics`、`telegraf`、`titanagent`），都卡在同一条调用链上；
2. **两个 XFS 内核工作线程**（`kworker/u34:3` 对应 `writeback`，`kworker/2:0` 对应 `xfs-inodegc/vdb`），卡在 buffer 锁上。

日志里反复出现的 `"echo 0 > /proc/sys/kernel/hung_task_timeout_secs" disables this message.` 只是 hung_task 检测器每条告警都会打印的固定提示语，并不代表发生了多次独立故障；整个 dmesg 里更早的 Dec 25 cgroup OOM、Jul 16/17 的 `SLUB: Unable to allocate memory`（skbuff_head_cache 分配失败）是另外的、时间上不相关的历史事件。

## 日志分析

### 用户态进程：卡在 statfs -> xfs_inodegc_flush

```
task:19100_node_expo state:D stack:    0 pid: 2394 ppid:     1 flags:0x00000080
Call Trace:
 __schedule+0x2ea/0x640
 schedule+0x46/0xb0
 schedule_timeout+0x240/0x2b0
 ? ttwu_queue+0x41/0xc0
 ? __legitimize_path+0x27/0x60
 wait_for_common+0x97/0x140
 __flush_work.isra.0+0x60/0x80
 ? flush_workqueue_prep_pwqs+0x140/0x140
 ? is_vmalloc_or_module_addr+0x24/0x30
 ? __check_object_size.part.0+0x135/0x1c0
 xfs_inodegc_flush.part.0+0x3e/0x90 [xfs]
 xfs_fs_statfs+0x2d/0x1a0 [xfs]
 statfs_by_dentry+0x64/0x90
 user_statfs+0x57/0xc0
 __se_sys_statfs+0x25/0x60
 do_syscall_64+0x3d/0x80
 entry_SYSCALL_64_after_hwframe+0x61/0xc6
```

`BgSchPool`、`AsyncMetrics`、`telegraf`、`titanagent` 的调用栈与此完全一致，都是调用 `statfs(2)`/`statvfs(3)` 查询磁盘使用率（常见于监控采集、容量上报等定时任务），在内核里落到：

```
sys_statfs -> user_statfs -> statfs_by_dentry -> xfs_fs_statfs -> xfs_inodegc_flush -> __flush_work
```

`xfs_fs_statfs()` 为了拿到准确的空闲 inode/block 计数，会先调用 `xfs_inodegc_flush()` 把当前 CPU 上挂起的 inode 延迟回收（inodegc）工作刷完，也就是 `flush_work()` 等待 `xfs-inodegc` workqueue 上的 worker 跑完。如果这个 worker 本身卡住，所有调用 `statfs` 的进程都会被一起拖住——这正是本次告警里进程数量远多于"真正故障点"的原因。

### xfs-inodegc worker：卡在 AGF buffer 锁上

```
task:kworker/2:0     state:D stack:    0 pid:3702423 ppid:     2 flags:0x00004080
Workqueue: xfs-inodegc/vdb xfs_inodegc_worker [xfs]
Call Trace:
 __schedule+0x2ea/0x640
 schedule+0x46/0xb0
 schedule_timeout+0x240/0x2b0
 __down+0x83/0xe0
 down+0x3b/0x50
 xfs_buf_lock+0x2c/0xb0 [xfs]
 xfs_buf_find+0x345/0x640 [xfs]
 xfs_buf_get_map+0x44/0x230 [xfs]
 xfs_buf_read_map+0x54/0x280 [xfs]
 xfs_trans_read_buf_map+0x13c/0x2a0 [xfs]
 xfs_read_agf+0x87/0x110 [xfs]
 xfs_alloc_read_agf+0x33/0x190 [xfs]
 xfs_alloc_fix_freelist+0x326/0x390 [xfs]
 xfs_alloc_vextent+0x23b/0x460 [xfs]
 __xfs_inobt_alloc_block.isra.0+0xbf/0x1a0 [xfs]
 __xfs_btree_split+0xec/0x660 [xfs]
 xfs_btree_split+0x4b/0x100 [xfs]
 xfs_btree_make_block_unfull+0x197/0x1d0 [xfs]
 xfs_btree_insrec+0x4ad/0x590 [xfs]
 xfs_btree_insert+0xa8/0x1f0 [xfs]
 xfs_difree_finobt+0xae/0x250 [xfs]
 xfs_difree+0x125/0x1a0 [xfs]
 xfs_ifree+0x88/0x150 [xfs]
 xfs_inactive_ifree.isra.0+0xa2/0x1b0 [xfs]
 xfs_inactive+0xf5/0x170 [xfs]
 xfs_inodegc_inactivate+0x16/0x50 [xfs]
 xfs_inodegc_worker+0x76/0xc0 [xfs]
 process_one_work+0x1ad/0x350
 worker_thread+0x49/0x310
 kthread+0xfb/0x140
 ret_from_fork+0x1f/0x30
```

释放一个 inode（`xfs_inactive_ifree` -> `xfs_difree`）需要更新 inobt/finobt，触发了一次 btree 分裂（`__xfs_btree_split`），分裂又需要从 AG 空闲块里分配新块（`xfs_alloc_vextent` -> `xfs_alloc_fix_freelist` -> `xfs_alloc_read_agf`），于是要去读/锁 AGF（Allocation Group Free space）buffer。栈顶停在 `xfs_buf_lock -> down()`，即在等待这块 buffer 的信号量——说明该 buffer 当前被别的上下文持有且迟迟没有释放。

### writeback worker：卡在 inode cluster buffer 锁上

```
task:kworker/u34:3   state:D stack:    0 pid:3702130 ppid:     2 flags:0x00004080
Workqueue: writeback wb_workfn (flush-253:16)
Call Trace:
 __schedule+0x2ea/0x640
 schedule+0x46/0xb0
 schedule_timeout+0x240/0x2b0
 ? xfs_buf_read_map+0x54/0x280 [xfs]
 ? xfs_btree_read_buf_block.constprop.0+0x9d/0xe0 [xfs]
 ? xfs_trans_add_item+0x33/0xa0 [xfs]
 __down+0x83/0xe0
 down+0x3b/0x50
 xfs_buf_lock+0x2c/0xb0 [xfs]
 xfs_buf_find+0x345/0x640 [xfs]
 xfs_buf_get_map+0x44/0x230 [xfs]
 xfs_buf_read_map+0x54/0x280 [xfs]
 xfs_trans_read_buf_map+0x13c/0x2a0 [xfs]
 xfs_imap_to_bp+0x61/0xb0 [xfs]
 xfs_trans_log_inode+0x1b7/0x270 [xfs]
 xfs_bmap_btalloc+0x773/0x840 [xfs]
 xfs_bmapi_allocate+0xd0/0x2e0 [xfs]
 xfs_bmapi_convert_delalloc+0x293/0x480 [xfs]
 xfs_map_blocks+0x1b0/0x400 [xfs]
 iomap_writepage_map+0x15c/0x570
 write_cache_pages+0x169/0x4f0
 iomap_writepages+0x1c/0x40
 xfs_vm_writepages+0x64/0x90 [xfs]
 do_writepages+0x31/0xc0
 __writeback_single_inode+0x39/0x200
 writeback_sb_inodes+0x203/0x540
 __writeback_inodes_wb+0x4c/0xd0
 wb_writeback+0x1d8/0x2a0
 wb_check_old_data_flush+0xb6/0xc0
 wb_do_writeback+0xc1/0x180
 wb_workfn+0x5a/0x180
 process_one_work+0x1ad/0x350
 worker_thread+0x49/0x310
 kthread+0xfb/0x140
 ret_from_fork+0x1f/0x30
```

`writeback` 内核线程在做延迟分配落盘（`xfs_bmap_btalloc` 里已经完成一次 extent 分配），随后要把新分配结果写回 inode（`xfs_trans_log_inode` -> `xfs_imap_to_bp`），需要读取该 inode 所在的 inode cluster buffer，同样卡在 `xfs_buf_lock -> down()`。

## 根因分析

两个内核工作线程都不是"死锁"在 CPU 上打转，而是在等 **buffer 信号量**（`xfs_buf_lock`），本质上是在等一次 buffer I/O（读盘）完成或等持锁者释放锁：

```
用户态 statfs 一堆进程
        │  等待
        ▼
xfs-inodegc/vdb worker（卡在 AGF buffer 锁 / down()）
                                  │
                                  ▼（同一块 vdb 上的另一 buffer）
writeback (flush-253:16) worker（卡在 inode cluster buffer 锁 / down()）
                                  │
                                  ▼
                          vdb 磁盘 I/O 长时间未完成
```

两个 worker 各自等待的是**不同的 buffer**（一个是 AGF，一个是 inode cluster），日志里没有证据显示是 XFS 内部的 ABBA 死锁；更符合"底层 `vdb`（virtio-blk 云盘）在这几分钟内 I/O 迟迟不返回"这一现象——buffer 锁在发起 I/O 的线程一直持有到 I/O completion callback 里才释放，如果磁盘慢/丢中断/后端抖动，锁自然长时间不释放。

真正让故障"扩散"的是 XFS 的一个设计点：`xfs_fs_statfs()` 在返回 df/statfs 结果前会调用 `xfs_inodegc_flush()` 等待 inodegc 全部跑完，以保证空闲块/inode 计数准确。这本是为了拿到精确结果，但代价是：**只要 inodegc worker 因为任何原因（包括单纯的磁盘慢）被卡住，所有调用 statfs/statvfs 的进程都会被一起挂起**，包括各类监控 agent（`node_exporter`、`telegraf`、`titanagent` 等定时采集磁盘使用率的组件），造成"一次局部 I/O 卡顿"被放大成"大面积业务进程 D 状态"的假象。

## 结论

1. 触发点大概率是 `vdb`（华东1 资源池、主机 `bef9d276-d2af-7729-90e9-939b`）后端存储/虚拟化层出现短暂 I/O 停顿或抖动，属于磁盘故障范畴，而非 XFS 自身死锁。
2. `xfs-inodegc` 与 `writeback` 两个 worker 卡在各自的 `xfs_buf_lock` 上，是在等对应 buffer 的 I/O 完成，未见 ABBA 循环等待。
3. 监控/agent 进程大量 D 状态是 `xfs_fs_statfs -> xfs_inodegc_flush -> flush_work` 这条路径的放大效应，只要 inodegc worker 恢复，statfs 调用方会一起解除阻塞，不需要单独处理。

## inodegc 特性背景：它是干什么的

`inodegc`（inode garbage collection）是 XFS 在 **v5.15-rc1** 引入的优化（`ab23a7768739`，"xfs: per-cpu deferred inode inactivation queues"），本次故障里 `xfs-inodegc/vdb` worker 就是这个特性的产物。

### 要解决的问题

一个文件被 `unlink`、引用计数归零后，内核会走 VFS 的 `->evict_inode` -> XFS 的 `xfs_inactive()` 去做"清理"：

- 如果文件有很多 extent（比如被打散得很碎的大文件），要把这些 extent 全部释放回 AG 的空闲块，可能触发多轮事务
- 释放 inode 本身（`xfs_difree`），可能触发 inobt/finobt 的 btree 分裂或合并（就是本次日志里 `kworker/2:0` 卡住的那条路径 `xfs_inactive_ifree -> xfs_difree -> __xfs_btree_split`）

在没有 inodegc 之前，这些工作是**同步**跑在触发 `unlink()`、或者最后一次 `iput()` 释放引用的那个进程/CPU 上下文里的。文件 extent 数特别多、或者盘比较慢时，一次 `rm 大文件` 可能会在原地卡住很久。

### inodegc 的做法

commit 原文：

> Move inode inactivation to background work contexts so that it no longer runs in the context that releases the final reference to an inode.

具体实现：

- 每个 CPU 一个无锁链表（`llist`）队列 + 一个 workqueue work item：inode 变成 inactive 后不再原地处理，而是丢进当前 CPU 的队列
- 典型场景：`unlink` 一个目录下一堆大文件时，进程可以立刻处理下一个 inode，不用等上一个 inode 的 extent 全部释放完；真正的释放工作交给后台的 `xfs-inodegc/<dev>` 内核线程异步做
- 队列深度 32（对应一个 inode cluster buffer 里的 inode 数），攒够一批或超过阈值（256 个）就触发 worker 处理，以保持 CPU cache 局部性

一句话：**inodegc 把"删文件时清理 inode 元数据"从"谁删的谁等着"改成了"扔给后台线程异步做"，本意是避免大量 unlink/reflink 场景下用户态被阻塞。**

### 和本次故障的关系

正因为 inactivation 被挪到了后台的 `xfs-inodegc/vdb` worker 上，这个 worker 才会单独出现在 hung_task 列表里——它卡在 `xfs_buf_lock` 上本身是 inodegc 正常工作流程的一部分，不是 bug。

但特性上线后暴露出一个副作用：既然 inactivation 变成异步了，`xfs_fs_statfs()` 想拿到"准确"的空闲块/inode 数，就得先等这个后台队列跑完（`xfs_inodegc_flush`）——这一等，如果后台 worker 因为慢盘卡住，所有调用 `statfs` 的进程就全被拖下水了。这正是本次故障放大成一大片 D 状态进程的根本机制，也是下面 `xfs_inodegc_push()` 补丁要修的问题。

## 内核层面的修复：xfs_inodegc_push()

`statfs -> xfs_inodegc_flush -> flush_work` 这条阻塞路径本身，上游已经在 **v5.19-rc5** 修掉了：

```
commit 5e672cd69f0a534a445df4372141fd0d1d00901d
Author: Dave Chinner <dchinner@redhat.com>
Date:   Thu Jun 16 07:44:32 2022 -0700

    xfs: introduce xfs_inodegc_push()

    The current blocking mechanism for pushing the inodegc queue out to
    disk can result in systems becoming unusable when there is a long
    running inodegc operation. This is because the statfs()
    implementation currently issues a blocking flush of the inodegc
    queue and a significant number of common system utilities will call
    statfs() to discover something about the underlying filesystem.

    This can result in userspace operations getting stuck on inodegc
    progress, and when trying to remove a heavily reflinked file on slow
    storage with a full journal, this can result in delays measuring in
    hours.

    Avoid this problem by adding "push" function that expedites the
    flushing of the inodegc queue, but doesn't wait for it to complete.

    Convert xfs_fs_statfs() and xfs_qm_scall_getquota() to use this
    mechanism so they don't block but still ensure that queued
    operations are expedited.

    Fixes: ab23a7768739 ("xfs: per-cpu deferred inode inactivation queues")
    Reported-by: Chris Dunlop <chris@onthe.net.au>
    Signed-off-by: Dave Chinner <dchinner@redhat.com>
    [djwong: fix _getquota_next to use _inodegc_push too]
    Reviewed-by: Darrick J. Wong <djwong@kernel.org>
    Signed-off-by: Darrick J. Wong <djwong@kernel.org>
```

Dave Chinner 描述的场景与本次故障几乎一致：大量调用 `statfs()` 的用户态工具因为 `xfs_fs_statfs` 阻塞式等待 inodegc 而被一起拖死。补丁把 `xfs_fs_statfs()`（以及 `xfs_qm_scall_getquota()`）改成调用不阻塞的 `xfs_inodegc_push(mp)`——只是把 inodegc 队列"催一下"尽快跑，但 `statfs` 本身不再 `flush_work` 等它跑完：

```c
// fs/xfs/xfs_super.c（当前 upstream 主线）
STATIC int
xfs_fs_statfs(
	struct dentry		*dentry,
	struct kstatfs		*st)
{
	struct xfs_mount	*mp = XFS_M(dentry->d_sb);
	struct xfs_inode	*ip = XFS_I(d_inode(dentry));

	/*
	 * Expedite background inodegc but don't wait. We do not want to block
	 * here waiting hours for a billion extent file to be truncated.
	 */
	xfs_inodegc_push(mp);
	...
```

代价是返回的空闲块/inode 计数可能有极短暂的不精确（因为不再等最新一批 inactivation 落盘），换来的是不会被无关的慢 I/O 拖死一大片进程。

`ab23a7768739`（per-cpu deferred inode inactivation queues，即 inodegc 特性本身）是 **v5.15-rc1** 合入主线的特性，`5e672cd69f0a534a445df4372141fd0d1d00901d` 这个修复则是 **v5.19-rc5** 才合入。故障主机跑的是 `5.10.0-136.12.0.92.ctl3` —— 说明厂商把 inodegc 特性从 5.15 背移植（backport）到了 5.10，但没有一并带上晚于 inodegc 四个大版本才合入的这个修复，属于"只背新特性、漏背配套修复"的典型问题。

在 `ctkernel-lts-5.10-develop` 仓库里核实了这条 inodegc 特性引入与修复的现状：

- inodegc 特性由 **`705fccfbcf0b8` "xfs: per-cpu deferred inode inactivation queues"**（2022-01-07，走的是 openEuler 的 `mainline-inclusion from mainline-v5.14-rc4` 流程，Signed-off-by 到 Huawei 的 Lihong Kou/Zhang Yi/Zheng Zengkai）引入，是 `heads/ctkernel-lts-5.10/develop` 分支的祖先提交，确认已经在线上。
- 对应的修复提交在**当前 develop 分支上还没有合入**：`git merge-base --is-ancestor` 对 upstream 原始 hash `5e672cd69f0a534a` 以及各个厂商 cherry-pick 变体（`99e65f075e6cf`/`cc10c7d993f28`/`0c38d6918523a`/`bef4b481178cb`/`a435f02700ae6`）逐一核对，全部返回 `NOT ancestor`，说明这台故障主机（乃至当前 develop 分支上所有环境）目前都还暴露在这个问题下。
- 好消息是**修复已经有人准备好了**：仓库里已经存在一个未合并的远程分支 `remotes/ctkernel-lts-5.10/lchen/xfs-statfs`，比对 `heads/ctkernel-lts-5.10/develop` 领先两个提交：

  ```
  26b5a22d7a83f xfs: flush inodegc workqueue tasks before cancel
  a435f02700ae6 xfs: introduce xfs_inodegc_push()
  ```

  其中 `a435f02700ae6` 就是本节要的那个修复（openEuler/Huawei 背移植版本，`mainline inclusion from mainline-v5.19-rc2`，Signed-off-by Guo Xuenan/Yang Erkun/Jialin Zhang），提交时间 2025-08-27，比对的 merge-base（`53b37ccd0bd7`）比当前 develop 的 HEAD（`a8d377cf6862`，2025-11-20 "release: develop 0096"）要早，也就是这个分支落后了 develop 一段时间、尚未 rebase/合并。

**注意**：`a8d377cf6862`（"release: develop 0096"）是 `heads/ctkernel-lts-5.10/develop` 当前已经发布的最新版本，用 `git merge-base --is-ancestor a435f02700ae6 heads/ctkernel-lts-5.10/develop` 核实过，结果是 **0096 里也没有这个修复**——不是"要等到 0096 才修"，而是**0096 目前也是缺这个修复的**。截至目前，任何一个已发布的 develop 版本都不包含它，修复只静静躺在落后主线的 `lchen/xfs-statfs` 分支上。要让它生效，需要把这个分支 rebase 到最新 develop 后合入，落到**下一个**版本（0097 或之后的某个版本），而不是已发布的 0096。

## 后续排查建议

1. 对照该主机同一时间窗口（15:09–15:13）的存储后端/hypervisor 侧监控与日志，确认 `vdb` 是否存在慢 I/O、超时重传或宿主机侧异常。
2. 检查 `/proc/diskstats` 或历史 `iostat -x` 采样数据，确认 `vdb` 在故障窗口内的 `await`/`svctm` 是否有异常尖峰。
3. 确认故障是否随宿主机 I/O 恢复而自愈（后续 dmesg 里若无 XFS shutdown / `Corruption of in-memory data` 等信息，说明只是慢，未导致文件系统被强制只读）。
4. **不需要重新 cherry-pick，直接跟 `remotes/ctkernel-lts-5.10/lchen/xfs-statfs` 这个已有分支的作者对齐、把它 rebase 到最新 develop 后合入即可**：分支上的两个提交 `26b5a22d7a83f`（flush inodegc workqueue tasks before cancel）+ `a435f02700ae6`（introduce xfs_inodegc_push）已经是背移植好的版本，只是落后 develop 一段时间没跟上；同时建议核对该厂商内核在背 inodegc 特性（`705fccfbcf0b8` / 对应 upstream `ab23a7768739`）时，到这个修复提交之间是否还漏了其他相关 fix。
5. 这个内核补丁解决的是"慢盘不该拖死无关进程"这个放大效应，并不能让 `vdb` 本身变快——存储侧的慢 I/O 根因仍需按第 1、2 条单独排查。
