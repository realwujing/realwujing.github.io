# 第三篇：日志 — JBD2 句柄、事务提交与快速提交

> 源码：`fs/ext4/ext4_jbd2.c`, `fs/ext4/fast_commit.c` | 头文件：`include/linux/jbd2.h`

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. 为什么需要日志

ext4 是日志型文件系统。这意味着**元数据的修改首先写入日志区域，然后再写入最终位置**。这保证了即使系统在写入过程中崩溃，重放日志也能将文件系统恢复到一致状态。

```
没有日志的崩溃场景:
  分配 inode → 更新 inode 位图 → 更新 GDT → 写入目录项
  ─────────────── 系统崩溃 ──────────────
  结果: inode 位图标记已用但实际未链接到任何目录 → 空间泄漏

有日志的崩溃场景:
  事务开始 → journal: [inode位图] [GDT] [目录项] → 事务提交
  ─────────────── 系统崩溃 ──────────────
  重放日志 → 三种操作全部重新应用 → 一致状态
```

ext4 的日志子系统基于 **JBD2 (Journaling Block Device 2)**，位于 `fs/jbd2/`。

---

## 2. 三种日志模式 (`ext4_jbd2.c:10`)

```c
// fs/ext4/ext4_jbd2.c:10
int ext4_inode_journal_mode(struct inode *inode)
```

| 挂载选项 | 模式 | 元数据 | 数据 | 一致性保证 |
|----------|------|--------|------|-----------|
| `data=writeback` | WRITEBACK | 日志 | 直接写 | 最弱：元数据可能指向错误数据 |
| `data=ordered` (默认) | ORDERED | 日志 | 先于元数据写 | 数据先落盘，元数据再日志，不会看到旧数据 |
| `data=journal` | JOURNAL_DATA | 日志 | 也日志 | 最强：所有数据也写日志（性能最差） |

模式判定逻辑 (`ext4_jbd2.c:10`)：

```c
int ext4_inode_journal_mode(struct inode *inode)
{
    if (EXT4_JOURNAL(inode) == NULL)
        return EXT4_INODE_WRITEBACK_DATA_MODE;    // 无日志 → 写回

    if (!S_ISREG(inode->i_mode) ||
        ext4_test_inode_flag(inode, EXT4_INODE_EA_INODE) ||
        test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_JOURNAL_DATA ||
        (ext4_test_inode_flag(inode, EXT4_INODE_JOURNAL_DATA) &&
         !test_opt(inode->i_sb, DELALLOC))) {
        // 加密文件不使用数据日志
        if (S_ISREG(inode->i_mode) && IS_ENCRYPTED(inode))
            return EXT4_INODE_ORDERED_DATA_MODE;   // 加密 → ordered
        return EXT4_INODE_JOURNAL_DATA_MODE;       // 数据日志
    }
    if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_ORDERED_DATA)
        return EXT4_INODE_ORDERED_DATA_MODE;       // ordered
    if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_WRITEBACK_DATA)
        return EXT4_INODE_WRITEBACK_DATA_MODE;     // writeback
    BUG();
}
```

**特殊情况**：加密文件 (`IS_ENCRYPTED`) 即使设置了 `JOURNAL_DATA` 也被强制降级为 ORDERED，因为日志区域不在加密保护范围内。

---

## 3. 句柄生命周期 (`ext4_jbd2.c:92-118`)

JBD2 使用 **句柄 (handle_t)** 抽象一个事务操作。句柄的生命周期严格在 begin/stop 之间：

### 3.1 开始句柄 (`ext4_jbd2.c:92`)

```c
// fs/ext4/ext4_jbd2.c:92
handle_t *__ext4_journal_start_sb(struct inode *inode,
                                  struct super_block *sb, unsigned int line,
                                  int type, int blocks, int rsv_blocks,
                                  int revoke_creds)
{
    journal_t *journal;
    int err;

    // 1. trace 记录（用于 perf/Kdebug）
    trace_ext4_journal_start_inode(inode, blocks, rsv_blocks, ...);

    // 2. 前置检查
    err = ext4_journal_check_start(sb);
    if (err < 0)
        return ERR_PTR(err);

    journal = EXT4_SB(sb)->s_journal;

    // 3. 无日志模式或 fast commit 重放 → 返回伪句柄
    if (!journal || (EXT4_SB(sb)->s_mount_state & EXT4_FC_REPLAY))
        return ext4_get_nojournal();

    // 4. 向 jbd2 申请事务空间
    return jbd2__journal_start(journal, blocks, rsv_blocks,
                               revoke_creds, GFP_NOFS, type, line);
}
```

参数说明：
- `blocks`: 期望消耗的最大日志块数（用于空间预留）
- `rsv_blocks`: 预留块数（可多次重用的预留空间）
- `revoke_creds`: revoke 操作占用的 credit

前置检查 (`ext4_journal_check_start`, `ext4_jbd2.c:64`)：
```c
static int ext4_journal_check_start(struct super_block *sb)
{
    // 检查紧急只读状态
    ret = ext4_emergency_state(sb);
    if (unlikely(ret))
        return ret;

    // 检查是否只读挂载
    if (WARN_ON_ONCE(sb_rdonly(sb)))
        return -EROFS;

    // 检查文件系统冻结
    WARN_ON(sb->s_writers.frozen == SB_FREEZE_COMPLETE);

    // 检查日志是否已中止 (eg. EIO)
    journal = EXT4_SB(sb)->s_journal;
    if (journal && is_journal_aborted(journal)) {
        ext4_abort(sb, -journal->j_errno, "Detected aborted journal");
        return -EROFS;
    }
    return 0;
}
```

### 3.2 停止句柄 (`ext4_jbd2.c:118`)

```c
// fs/ext4/ext4_jbd2.c:118
int __ext4_journal_stop(const char *where, unsigned int line,
                        handle_t *handle)
```

停止句柄时，JBD2 将句柄关联的所有修改提交到当前事务。当事务积累足够多或超时后，后台线程提交事务到磁盘。

---

## 4. 元数据缓冲区的日志协议

ext4 对元数据缓冲区的修改遵循严格的协议：

### 4.1 获取写权限 (`ext4_jbd2.c:231`)

```c
// fs/ext4/ext4_jbd2.c:231
int __ext4_journal_get_write_access(const char *where, unsigned int line,
                                    handle_t *handle, struct super_block *sb,
                                    struct buffer_head *bh,
                                    enum ext4_journal_trigger_type trigger_type)
{
    int err;
    might_sleep();

    if (ext4_handle_valid(handle)) {
        // 1. jbd2 拷贝当前缓冲区内容到日志
        //    (log the "before image" for undo)
        err = jbd2_journal_get_write_access(handle, bh);
        if (err) {
            ext4_journal_abort_handle(where, line, __func__, bh, handle, err);
            return err;
        }
    } else
        ext4_check_bdev_write_error(sb);

    // 2. 如果启用 metadata_csum，设置 CRC 触发器
    //    写入后 jbd2 会自动重新计算 CRC
    if (trigger_type == EXT4_JTR_NONE ||
        !ext4_has_feature_metadata_csum(sb))
        return 0;
    jbd2_journal_set_triggers(bh,
        &EXT4_SB(sb)->s_journal_triggers[trigger_type].tr_triggers);
    return 0;
}
```

### 4.2 标记脏元数据 (`ext4_jbd2.c:353`)

```c
// fs/ext4/ext4_jbd2.c:353
int __ext4_handle_dirty_metadata(const char *where, unsigned int line,
                                 handle_t *handle, struct inode *inode,
                                 struct buffer_head *bh)
{
    int err = 0;
    might_sleep();

    set_buffer_meta(bh);          // 标记为元数据缓冲区
    set_buffer_prio(bh);          // 高优先级写入
    set_buffer_uptodate(bh);      // 标记有效

    if (ext4_handle_valid(handle)) {
        // 提交到当前 JBD2 事务
        err = jbd2_journal_dirty_metadata(handle, bh);
        if (!is_handle_aborted(handle) && WARN_ON_ONCE(err)) {
            ext4_journal_abort_handle(where, line, __func__, bh, handle, err);
        }
    } else {
        // 无日志模式：直接标记脏
        if (inode)
            mark_buffer_dirty_inode(bh, inode);
        else
            mark_buffer_dirty(bh);
    }
    return err;
}
```

### 4.3 完整的修改协议

```
任何修改元数据缓冲区的操作：
┌───────────────────────────────────────────────────────────┐
│                                                           │
│  handle = ext4_journal_start(inode, EXT4_HT_*, nblocks)   │
│  │                                                        │
│  ├─ lock_buffer(bh)                                       │
│  ├─ ext4_journal_get_write_access(handle, sb, bh, ...)    │
│  │    └─ jbd2 拷贝前映像到日志 (undo record)                │
│  │                                                        │
│  ├─ 修改缓冲区内容                                          │
│  │    bh->b_data[offset] = new_value;                     │
│  │                                                        │
│  ├─ ext4_handle_dirty_metadata(handle, inode, bh)         │
│  │    └─ 标记缓冲区脏，加入当前事务的元数据列表              │
│  │                                                        │
│  ├─ unlock_buffer(bh)                                     │
│  │                                                        │
│  └─ ext4_journal_stop(handle)                             │
│       └─ 句柄结束，当条件满足时触发事务提交                  │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

---

## 5. `__ext4_forget` — 日志撤销 (`ext4_jbd2.c:267`)

```c
// fs/ext4/ext4_jbd2.c:267
int __ext4_forget(const char *where, unsigned int line, handle_t *handle,
                  int is_metadata, struct inode *inode,
                  struct buffer_head *bh, ext4_fsblk_t blocknr)
```

当释放数据块时，必须"告诉日志忘记它们"：

```
无日志模式:
  if (bh) clear_buffer_dirty(bh);
  wait_on_buffer(bh);
  __bforget(bh);            // 直接丢弃

数据日志模式 或 非日志数据块:
  jbd2_journal_forget(handle, bh)
  // 只是标记：日志回复时不恢复这个块

元数据块 (非数据日志模式):
  jbd2_journal_revoke(handle, blocknr, bh)
  // 撤销：从日志中移除该块的记录，防止重放
```

**journal_forget vs journal_revoke 的区别**：
- `forget`: 如果该块的数据尚未写入磁盘，先写入，然后确保重放时不覆盖
- `revoke`: 在日志中记录"不恢复该块"的信息，重放时跳过

---

## 6. 日志提交流程 — 完整提交

```
                         ┌──────────────────────────────────────┐
                         │         应用线程 1 / 2 / ...          │
                         │                                      │
                         │  ext4_journal_start()                │
                         │  get_write_access(bh1)               │
                         │  modify bh1                          │
                         │  dirty_metadata(bh1)                 │
                         │  ext4_journal_stop()                 │
                         └──────────────┬───────────────────────┘
                                        │ 将句柄加入事务
                                        ▼
┌──────────────────────────────────────────────────────────────────┐
│                        当前运行事务 (running transaction)          │
│                                                                   │
│  t_outstanding_credits: 累积的 credit                             │
│  t_buffers: 被修改的元数据缓冲区链                                 │
│  t_reserved_list: 预留的缓冲区链                                   │
│  t_updates: 正在运行的句柄计数                                     │
└──────────────────────────────┬───────────────────────────────────┘
                               │ 触发条件: 时间到 / credit 用完
                               │ / 用户 sync
                               ▼
┌──────────────────────────────────────────────────────────────────┐
│                       提交事务 (committing transaction)            │
│                                                                   │
│  Phase 1: 等待数据落盘 (ORDERED 模式)                              │
│  Phase 2: 将元数据缓冲区的拷贝写入日志区域                           │
│  Phase 3: 写入日志描述符块，标记事务完成                             │
│  Phase 4: 等待元数据写入最终位置 (checkpoint)                       │
│                                                                   │
│  输出: 日志区域的连续记录:                                          │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌─────────────────────────┐ │
│  │Desc.Blk│  │Data Blk│  │Data Blk│  │Commit Block (checksum) │ │
│  │(元数据)│  │(bh1)   │  │(bh2)   │  │(标记事务原子性结束)      │ │
│  └────────┘  └────────┘  └────────┘  └─────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### 日志是环形的

```
日志区域 (循环缓冲区):
┌──────────────────────────────────────────────────────────────┐
│ [tail] ← free space → [head]                                 │
│                                                               │
│  tail: 最老的有效事务起始位置                                   │
│  head: 下一个写入位置                                          │
│                                                               │
│  tail 推进: checkpoint 完成后释放旧事务                         │
│  head 推进: 新事务提交时写入                                    │
└──────────────────────────────────────────────────────────────┘
```

当 head 追上 tail 时，空间不足，新的提交必须等待 tail 推进（checkpoint）。

---

## 7. Fast Commit — 快速提交 (`fast_commit.c:1-110`)

### 7.1 设计动机

普通 JBD2 提交是 `O(所有日志记录的块数)`，每次提交都要写出整个事务的所有元数据。对于大量小文件操作（创建、unlink、rename），完整提交的开销很大。

Fast Commit 的思想：**只记录变化的逻辑操作，而非整个块的拷贝**。

```
完整提交:                  Fast Commit:
  写入 10 个目录项的块        记录 [LINK A] [LINK B] [UNLINK C] ...
  写入 5 个 inode 块         记录 [INODE X] [INODE Y] ...
  写提交块                   写 TAIL (CRC + TID)
  ──────────                  ──────────
  复杂度 O(总块数)             复杂度 O(变化的 inode 数)
```

### 7.2 TLV 日志格式

Fast Commit 日志是 TLV (Tag-Length-Value) 格式的线性日志：

```
struct ext4_fc_tl {
    __le16  fc_tag;      // TAG 类型
    __le16  fc_len;      // 值长度
    __u8    fc_value[];  // 可变长度值
};
```

支持的 TLV 标签 (`fast_commit.c:31-44`)：

| 标签 | 含义 | 值格式 |
|------|------|--------|
| `EXT4_FC_TAG_LINK` | 目录项链接 | (parent_ino, name, target_ino) |
| `EXT4_FC_TAG_UNLINK` | 目录项取消链接 | (parent_ino, name) |
| `EXT4_FC_TAG_CREAT` | 创建 (inode + 目录项) | (parent_ino, name, inode_data) |
| `EXT4_FC_TAG_ADD_RANGE` | 数据块范围增加 | (lblk, pblk, len) |
| `EXT4_FC_TAG_DEL_RANGE` | 数据块范围删除 | (lblk, len) |
| `EXT4_FC_TAG_INODE` | inode 元数据变更 | 完整的 inode 内容 |
| `EXT4_FC_TAG_TAIL` | 原子性标记 | CRC32 + TID |

### 7.3 提交协议 (`fast_commit.c:48-71`)

```
Fast Commit 提交流程:

[1] 设置 EXT4_STATE_FC_FLUSHING_DATA
    │   防止相关 inode 在提交过程中被删除
    │
[2] 将数据缓冲区刷盘，清除 FLUSHING_DATA 状态
    │   ORDERED 语义：数据先于元数据
    │
[3] jbd2_journal_lock_updates(sbi->s_journal)
    │   锁住日志：等待现有句柄完成，阻止新句柄开始
    │
[4] 设置所有 FC 合规 inode 为 EXT4_STATE_FC_COMMITTING
    │   标记这些 inode 正在进行 fast commit
    │
[5] jbd2_journal_unlock_updates(sbi->s_journal)
    │   解锁：允许新句柄开始
    │   如果有新句柄尝试修改正在提交的 inode，会被阻塞
    │
[6] 将目录项更新写出到 fast commit 区域
    │   按操作顺序写出 TLV
    │
[7] 将 inode 变更写出到 fast commit 区域
    │   清除 COMMITTING 状态
    │
[8] 写 TAIL 标签（包含 CRC 和 TID）
    │   保证原子性
```

### 7.4 原子性保证 (`fast_commit.c:85-105`)

TAIL 标签是原子性的关键：

```
TAIL 包含:
  - fc_tid: 该 fast commit 对应的 JBD2 事务 ID
  - CRC:    fast commit 所有内容的 CRC

重放时:
  - 检查 TAIL 的 CRC 匹配 → 有效提交
  - CRC 不匹配 → 丢弃该 fast commit
  - 应用有效提交中的 TLV 条目

Fast Commit 区域可能有多个 TAIL:
  [CREAT A] [UNLINK B] [TAIL#1] [ADD_RANGE C] [DEL_RANGE A] [TAIL#2]
  |<────  Fast Commit 1 ────>|<──────  Fast Commit 2 ───────>|
```

### 7.5 不支持的操作 — 回退到完整提交

某些操作不被 fast commit 支持，此时调用 `ext4_fc_mark_ineligible()` 标记：

```
不支持的操作包括:
  - 扩展属性 (xattr) 操作
  - inode 块号的变更（由 replay 从 extent 推导）
  - 某些复杂的目录操作
  - 涉及多 inode 同步的复杂场景

当标记为 ineligible 后，下一次提交自动退化为完整 JBD2 提交。
```

---

## 8. 日志超级块与加载 (`super.c:68/6080`)

```c
// fs/ext4/super.c:68 (声明)
static int ext4_load_journal(struct super_block *, struct ext4_super_block *,
                              unsigned long journal_devnum);

// fs/ext4/super.c:6080 (实现)
static int ext4_load_journal(struct super_block *sb,
                              struct ext4_super_block *es,
                              unsigned long journal_devnum)
```

加载日志有两种方式：

1. **内部日志**：日志存储在文件系统内部的一个常规文件
   - `s_journal_inum` (`ext4.h:1389`): 日志文件 inode 编号
   - `s_jnl_blocks[17]` (`ext4.h:1399`): 日志 inode 的前 17 个块的备份
   - 原因：如果日志 inode 自身也需要经过日志才能读取，就变成鸡生蛋问题
   - 解决：超级块中直接备份前几块，启动时可以直接读取日志超级块

2. **外部日志**：日志存储在独立的块设备上
   - `s_journal_dev` (`ext4.h:1390`): 外部设备号
   - `s_journal_uuid[16]` (`ext4.h:1388`): 外部日志 UUID

加载流程：
```
ext4_load_journal(sb, es, devnum)
│
├─ 如果是外部设备:
│    └─ bdev_open_by_dev() → 直接打开日志设备
│
├─ 如果是内部日志:
│    ├─ ext4_get_journal_inode() → 通过 s_journal_inum 读取 inode
│    └─ jbd2_journal_init_inode(inode) → 初始化日志
│
└─ jbd2_journal_load(journal) → 检查并重放日志
```

---

## 9. Fast Commit 重放 (`fast_commit.c:200+`)

重放是 TLV 解码和应用的过程：

```
ext4_fc_replay(sb, tid)
│
├─ 扫描 Fast Commit 区域，查找所有有效 TAIL
│    ├─ 验证 TAIL CRC
│    └─ 收集 tid > replay_tid 的有效 TAIL
│
├─ 对每个有效 Tail 之前的 TLV 进行重放:
│    ├─ LINK: 重建目录项链接
│    ├─ UNLINK: 删除目录项
│    ├─ CREATE: 创建 inode + 目录项
│    ├─ ADD_RANGE: 重建 extent（通过 ext4_ext_insert_extent）
│    ├─ DEL_RANGE: 删除 extent
│    └─ INODE: 更新 inode 元数据
│
└─ 如果遇到不支持的 TLV 或出错:
    └─ 回退到完整 JBD2 重放 (ext4_fc_replay_abort)
```

重放的 idempotent 性：Fast commit 的每个 TLV 重放是幂等的，因为重放代码会检查操作是否已经被应用（例如目录项是否已经存在）。

---

## 10. 实践：查看日志信息

```bash
# 查看日志超级块
sudo dumpe2fs -h /dev/sda1 | grep -i journal

# 输出示例:
# Journal inode:            8
# Journal backup:           inode blocks
# Journal features:         journal_incompat_revoke
#                           journal_64bit
#                           journal_async_commit
# Journal size:             128M

# 查看 fast commit 支持
sudo dumpe2fs -h /dev/sda1 | grep -i fast_commit
# Fast commit length:       256 blocks (if supported)
```

启用 fast commit（需要 e2fsprogs ≥ 1.46）：
```bash
sudo tune2fs -O fast_commit /dev/sda1
```

---

## 下一篇文章

[第四篇：块与索引节点分配 — 多块分配器与 inode 分配策略](./04-alloc.md)
