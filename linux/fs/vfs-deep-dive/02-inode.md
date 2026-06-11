# 第二篇：Inode 生命周期 — 创建、查找、引用计数与回收

> 源码：`fs/inode.c` (3061行) | 头文件：`include/linux/fs.h` (3660行)

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 1. 什么是 inode

inode（index node）是 VFS 中代表**文件系统对象**的核心结构体。它持有文件的元数据（权限、大小、时间戳）、页缓存（通过 `i_mapping`）和锁。与 dentry 不同的是：
- inode 在文件打开/关闭之间持续存在（只要还在缓存中）
- 多个 dentry 可通过硬链接指向同一个 inode
- inode 是页缓存的入口（`i_mapping` → address_space）

当 `open("/home/user/file", O_RDWR)` 时：
1. 路径查找逐组件解析，每个组件创建或查找 dentry
2. 最终 dentry 的 `d_inode` 指向文件的 inode
3. 内核通过 inode 的 `i_mapping` 找到页缓存，执行 read/write

```
             ┌──────────────┐
  dentry1 ───┤              │
  dentry2 ───┤    inode     ├─── i_mapping (address_space)
  dentry3 ───┤  (硬链接)     │        │
             └──────────────┘        ▼
                                  page cache
                                  (文件内容, xarray)
```

---

## 2. struct inode 字段解析 (fs.h:767-856)

```c
struct inode {
    umode_t          i_mode;        // 768: 文件类型+权限位
    unsigned short   i_opflags;     // 769: inode_operations 标志缓存
    unsigned int     i_flags;       // 770: S_IMMUTABLE, S_APPEND 等
#ifdef CONFIG_FS_POSIX_ACL
    struct posix_acl *i_acl;        // 772: 访问控制列表
    struct posix_acl *i_default_acl;// 773: 默认 ACL
#endif
    kuid_t           i_uid;         // 775: 所有者 UID
    kgid_t           i_gid;         // 776: 所属组 GID

    const struct inode_operations *i_op; // 778: inode 操作表(mkdir,link,symlink…)
    struct super_block *i_sb;       // 779: 所属超级块
    struct address_space *i_mapping; // 780: 页缓存! 核心桥梁字段

#ifdef CONFIG_SECURITY
    void             *i_security;   // 783: LSM 安全上下文
#endif

    /* Stat data, not accessed from path walking */
    u64              i_ino;         // 787: inode 号(文件系统内唯一)
    union {
        const unsigned int i_nlink; // 796: 硬链接数(只读)
        unsigned int __i_nlink;     // 797: 可写(需 set_nlink 等函数操作)
    };
    dev_t            i_rdev;        // 799: 块/字符设备号
    loff_t           i_size;        // 800: 文件大小(字节)
    time64_t         i_atime_sec;   // 801: access time (秒)
    time64_t         i_mtime_sec;   // 802: modify time (秒)
    time64_t         i_ctime_sec;   // 803: change time (秒)
    u32              i_atime_nsec;  // 804: access time (纳秒)
    u32              i_mtime_nsec;  // 805
    u32              i_ctime_nsec;  // 806
    u32              i_generation;  // 807: NFS 文件句柄版本号

    spinlock_t       i_lock;        // 808: 保护 i_state, i_hash, i_io_list
    unsigned short   i_bytes;       // 809: 最后一个块的字节数
    u8               i_blkbits;     // 810: 块大小的位数(如 12 = 4096)
    blkcnt_t         i_blocks;      // 812: 已分配的块数

#ifdef __NEED_I_SIZE_ORDERED
    seqcount_t       i_size_seqcount; // 815: 32位平台保证 i_size 读取原子性
#endif

    struct inode_state_flags i_state; // 819: 状态位 (I_NEW, I_FREEING 等)
    struct rw_semaphore i_rwsem;     // 821: 读写信号量(inode级锁!)
                                     //     目录: 保护子项增删
                                     //     文件: 保护 truncate/大小变更

    unsigned long    dirtied_when;   // 823: 首次变脏的 jiffies
    unsigned long    dirtied_time_when; // 824: 时间戳变脏的 jiffies

    struct hlist_node i_hash;        // 826: 哈希表节点
    struct list_head i_io_list;      // 827: 脏/回写链表(bdi->wb.b_{dirty,io,…})
#ifdef CONFIG_CGROUP_WRITEBACK
    struct bdi_writeback *i_wb;      // 829: cgroup writeback
#endif
    struct list_head i_lru;          // 836: inode LRU 回收链表
    struct list_head i_sb_list;      // 837: 挂入 sb->s_inodes
    struct list_head i_wb_list;      // 838: writeback 工作链表
    union {
        struct hlist_head i_dentry;  // 840: dentry 别名链表头!
        struct rcu_head   i_rcu;     // 841: RCU 延迟释放
    };
    atomic64_t       i_version;      // 843: 版本号 (I/O 顺序保证)
    atomic64_t       i_sequence;     // 844: futex 序列号
    atomic_t         i_count;        // 845: ★ 引用计数! 核心
    atomic_t         i_dio_count;    // 846: 进行中 DIO 计数
    atomic_t         i_writecount;   // 847: 写者计数

    union {
        const struct file_operations *i_fop; // 852: 文件操作表
        void (*free_inode)(struct inode *);  // 853: RCU 释放回调
    };
    struct file_lock_context *i_flctx; // 855: 文件锁上下文
    struct address_space i_data;       // 856: 内嵌的 address_space
};
```

**核心字段深入**：

- **i_mapping (line 780)**：指向 address_space。对普通文件是 `&inode->i_data`，对块设备可能指向 `bdev->bd_mapping`。这是 inode 与页缓存之间的纽带——所有 read/write/mmap 都通过 i_mapping 访问数据。

- **i_rwsem (line 821)**：读写信号量。目录：保护目录内容的修改（新增/删除文件）；普通文件：保护文件大小变更（truncate）。原名为 `i_mutex`，后改为 `i_rwsem`。

- **i_count (line 845)**：原子引用计数。每次 `iget`/`ihold` 递增，每次 `iput` 递减。归零时触发驱逐（evict）。`i_dio_count` 和 `i_writecount` 是辅助计数器。

- **i_state (line 819)**：记录 inode 当前生命周期状态。受 `i_lock` 保护。主要标志见第 8 节。

- **i_nlink (line 796)**：硬链接数。为 0 时文件被标记为已删除，但如果仍有进程打开，inode 不会被释放直到最后的 `iput`。

- **i_dentry (line 839-842)**：hlist_head，链接着所有指向此 inode 的 dentry（别名链表）。遍历此链表可找到文件的所有名字。

- **i_lock (line 808)**：保护 i_state、i_hash、i_io_list 和 `__iget()`。不保护 i_size、i_mode 等（这些由 i_rwsem 保护）。

---

## 3. 锁定规则 (inode.c:32-61)

```
使用规则:
  inode->i_lock 保护:
    inode->i_state, inode->i_hash, __iget(), inode->i_io_list
  Inode LRU list locks 保护:
    inode->i_sb->s_inode_lru, inode->i_lru
  inode->i_sb->s_inode_list_lock 保护:
    inode->i_sb->s_inodes, inode->i_sb_list
  bdi->wb.list_lock 保护:
    bdi->wb.b_{dirty,io,more_io,dirty_time}, inode->i_io_list
  inode_hash_lock 保护:
    inode_hashtable, inode->i_hash

锁的顺序(层次图):
  iunique_lock
    └─► inode_hash_lock
          ├─► inode->i_sb->s_inode_list_lock
          │     └─► inode->i_lock
          │           └─► Inode LRU list locks
          └─► bdi->wb.list_lock
                └─► inode->i_lock
```

与 dentry 锁层次不同，inode 的锁层次是：
- **hash_lock 在最外层**：获取哈希桶锁先于 i_lock
- **i_lock 在中间层**：受 s_inode_list_lock 和 bdi->wb.list_lock 保护
- **LRU 锁在最内层**：最后获取，防止与 i_lock 的逆序死锁
- **注意 i_lock 不保护 i_size**：i_size 通常由 i_rwsem 保护（除 32 位平台的 i_size_seqcount 外）

---

## 4. Inode 哈希表 (inode.c:63-66)

```c
static unsigned int i_hash_mask __ro_after_init;
static unsigned int i_hash_shift __ro_after_init;
static struct hlist_head *inode_hashtable __ro_after_init;
static __cacheline_aligned_in_smp DEFINE_SPINLOCK(inode_hash_lock);
```

按 `(super_block 指针, inode 号)` 索引：

```c
static inline unsigned long hash(struct super_block *sb, unsigned long hashval)
{
    unsigned long tmp;
    tmp = (hashval * (unsigned long)sb) ^ (GOLDEN_RATIO_PRIME + hashval) /
            L1_CACHE_BYTES;
    tmp = tmp + ((tmp >> (unsigned long)i_hash_shift) & i_hash_mask);
    return tmp & i_hash_shift;
}
```

- 输入混合 sb 指针确保跨文件系统隔离
- 保护：`inode_hash_lock` 全局自旋锁（非 dentry 那样的 bit-lock）
- 使用 `hlist_head`（非 bl）——普通自旋锁而非位锁
- 操作：`__insert_inode_hash`(插入)、`__remove_inode_hash`(移除)、`find_inode`/`find_inode_fast`(查找)

---

## 5. Inode 分配

### alloc_inode (内部函数, inode.c)

```c
// fs/inode.c:79
static struct kmem_cache *inode_cachep __ro_after_init;

// 分配:
inode = alloc_inode_sb(sb, inode_cachep, GFP_KERNEL);  // 还可能通过 sb->s_op->alloc_inode
// 释放:
kmem_cache_free(inode_cachep, inode);                   // 或 sb->s_op->destroy_inode
```

`inode_cachep` 在 `inode_init()` → `kmem_cache_create("inode_cache", sizeof(struct inode), ...)` (inode.c:2601) 中创建。

### new_inode (inode.c:1175-1183)

```c
struct inode *new_inode(struct super_block *sb)
{
    struct inode *inode;
    inode = alloc_inode(sb);            // slab 分配, I_NEW 状态
    if (inode)
        inode_sb_list_add(inode);       // 加入 sb->s_inodes 链表
    return inode;
}
EXPORT_SYMBOL(new_inode);
```

调用者（文件系统实现）的后续步骤：
1. 初始化 i_ino, i_mode, i_size, i_uid/gid 等字段
2. 设置 i_op, i_fop, a_ops 操作表
3. `insert_inode_hash(inode)` → 加入哈希表，可被查找
4. `unlock_new_inode(inode)` → 清除 I_NEW + I_CREATING 标志，唤醒等待者 (inode.c:1213-1221)

### unlock_new_inode (inode.c:1213)

```c
void unlock_new_inode(struct inode *inode)
{
    lockdep_annotate_inode_mutex_key(inode);
    spin_lock(&inode->i_lock);
    WARN_ON(!(inode_state_read(inode) & I_NEW));
    inode_state_clear(inode, I_NEW | I_CREATING);
    inode_wake_up_bit(inode, __I_NEW);  // 唤醒所有 wait_on_inode 的线程
    spin_unlock(&inode->i_lock);
}
```

### discard_new_inode (inode.c:1224)

初始化失败时的清理路径：清除 I_NEW → 唤醒等待者 → `iput(inode)` 触发驱逐。

---

## 6. Inode 查找

### iget5_locked (inode.c:1375-1391)

最通用的查找+创建函数：

```c
struct inode *iget5_locked(struct super_block *sb, u64 hashval,
        int (*test)(struct inode *, void *),     // 匹配判断
        int (*set)(struct inode *, void *), void *data)  // 初始化新 inode
{
    // 先查找已有
    struct inode *inode = ilookup5(sb, hashval, test, data);

    if (!inode) {
        // 未找到 → 分配新 inode
        struct inode *new = alloc_inode(sb);
        if (new) {
            // inode_insert5 会再次查找哈希表(防竞态)然后插入
            inode = inode_insert5(new, hashval, test, set, data);
            if (unlikely(inode != new))         // 竞态:有人先插入了!
                destroy_inode(new);             // 释放多余的
        }
    }
    return inode;
}
```

内部流程：
```
ilookup5(sb, hashval, test, data)
  └── find_inode → 哈希查找
        ├── 找到且 test 返回 true → 返回 inode (i_count++)
        └── 未找到 → 返回 NULL

[如果未找到]:
alloc_inode(sb) → 新 inode (I_NEW 状态)
inode_insert5(new, hashval, test, set, data)
  └── 再次哈希查找
        ├── 找到 → 返回已有 inode, new 被销毁
        └── 未找到 → set(new, data) + 插入哈希表 + sb->s_inodes
```

test/set 回调给文件系统用于判断匹配（检查 generation、树形定位等）。

### iget_locked (inode.c:1452)

便捷版，仅需 ino 作为 key：

```c
struct inode *iget_locked(struct super_block *sb, u64 ino)
{
    struct hlist_head *head = inode_hashtable + hash(sb, ino);
    // ... 使用 find_inode_fast (仅按 ino 匹配) ...
}
```

假设文件系统 inode 号全局唯一。

### ilookup5 / ilookup (inode.c:1644/1674)

纯查找，不创建，不在哈希表中 → 直接返回 NULL。`ilookup5_nowait` (inode.c:1613) 是非阻塞版。

### iget5_locked_rcu (inode.c:1405-1437)

RCU 安全版：test 回调必须**容忍不稳定的 inode**（可能正在销毁中）。适用于已持有 RCU 读锁的场景。

### 文件系统典型调用

```c
// ext4, xfs 等文件系统的 iget 实现:
struct inode *myfs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode = iget_locked(sb, ino);
    if (!inode)   return ERR_PTR(-ENOMEM);
    if (!(inode->i_state & I_NEW))   return inode;  // 已在缓存

    // I_NEW set → 从磁盘读取
    // ... 读磁盘 inode, 填充 i_mode/i_size/i_blocks/文件系统私有字段 ...
    unlock_new_inode(inode);
    return inode;
}
```

---

## 7. Superblock inode 链表

每个超级块维护其所有存活 inode 的链表：

```
super_block
  │
  └── s_inodes (list_head)
        │   (受 s_inode_list_lock 保护)
        ├──► inode (ino=1, root dir)
        │     i_sb_list → next
        ├──► inode (ino=42, /etc/passwd)
        │     i_sb_list
        ├──► inode (ino=1003, /home/user/file)
        │     i_sb_list
        └──► ...
```

新 inode 创建时通过 `inode_sb_list_add(inode)` (inode.c 内部) 加入此链表。`evict_inodes` 遍历此链表批量驱逐。

---

## 8. Inode 状态标志

受 `i_lock` 保护，通过 `inode_state_read/set/clear` 系列函数操作：

| 标志 | 值 | 含义 |
|------|-----|------|
| `I_NEW` | 0x01 | 初始化中（alloc → unlock_new_inode 清除） |
| `I_FREEING` | 0x02 | 正在释放 |
| `I_WILL_FREE` | 0x04 | 即将释放 |
| `I_CLEAR` | 0x10 | i_state 被清零（RCU 释放阶段） |
| `I_DIRTY_SYNC` | 0x20 | inode 元数据脏（需 sync） |
| `I_DIRTY_DATASYNC` | 0x40 | 仅数据需 fdatasync |
| `I_DIRTY_PAGES` | 0x80 | 关联的页缓存页脏 |
| `I_REFERENCED`| 0x100 | LRU 引用标志（再给一次机会） |
| `I_CREATING` | 0x200 | 初始化阶段 |
| `I_SYNC` | 0x400 | inode 正在被回写 |
| `I_DIRTY_TIME` | 0x800 | 仅时间戳脏（lazytime） |

状态转换示例：
- I_NEW: alloc_inode → unlock_new_inode 清除 → inode 可用
- I_DIRTY_SYNC: mark_inode_dirty → write_inode 回写完成后清除
- I_FREEING: iput_final 设 → evict() → destroy_inode → kmem_cache_free

---

## 9. 脏 inode 跟踪与回写

### 脏 inode 链表

脏 inode 通过 backing device 的 writeback 机制管理：

```
i_io_list (fs.h:827) → 将 inode 链入:
  bdi->wb.b_dirty       — 脏 inode，等待回写
  bdi->wb.b_io          — 正在回写中的 inode
  bdi->wb.b_more_io     — 还有更多要回写的 inode
  bdi->wb.b_dirty_time  — 仅时间戳脏的 inode (lazytime)
```

- `dirtied_when` (fs.h:823)：记录首次变脏的 jiffies，writeback 据此决定回写顺序（越老越优先）
- `mark_inode_dirty(inode)` (`fs/fs-writeback.c`)：设置 I_DIRTY_SYNC，将 inode 移入 b_dirty 链表

### 回写执行流

```
触发: 定期(wb_workfn) / 内存压力(shrinker) / sync/fsync / dirty_ratio 超限

wb_workfn → wb_writeback → writeback_sb_inodes
  → __writeback_single_inode(inode, wbc)
      ├── do_writepages(mapping, wbc)     → 回写脏页(页缓存)
      │     └── mapping->a_ops->writepages → ext4_writepages 等
      │           → 收集脏 folio → bio_alloc → submit_bio
      └── write_inode(inode, wbc)         → 回写 inode 元数据
            └── sb->s_op->write_inode(inode, wbc)
```

---

## 10. iput — 引用计数释放 (inode.c:1972-2010)

```c
void iput(struct inode *inode)
{
    if (unlikely(!inode)) return;

retry:
    // 快速路径: count > 1, 仅递减
    if (atomic_add_unless(&inode->i_count, -1, 1))
        return;

    // count 可能 == 1, 需要处理 lazytime
    if (inode->i_nlink && sync_lazytime(inode))
        goto retry;

    spin_lock(&inode->i_lock);
    if (unlikely((inode_state_read(inode) & I_DIRTY_TIME) &&
                  inode->i_nlink)) {
        spin_unlock(&inode->i_lock);
        goto retry;                     // 时间戳脏, 回写后重试
    }

    if (!atomic_dec_and_test(&inode->i_count)) {
        spin_unlock(&inode->i_lock);
        return;                         // 又有人引用了!
    }

    iput_final(inode);                  // 真正的最后一个引用 → 驱逐
}
EXPORT_SYMBOL(iput);
```

iput_final 内部流程：
```
iput_final(inode)
  ├── drop_inode(inode) → 询问文件系统
  │     ├── false → 保留在缓存 (重新加入 LRU, 标记 I_REFERENCED)
  │     └── true  → generic_delete_inode(inode)
  │           └── evict(inode)
  │               ├── truncate_inode_pages_final(&inode->i_data)
  │               │     └── 清空页缓存(脏页丢弃！文件已删除或卸载)
  │               ├── sb->s_op->evict_inode(inode)     [super_types.h:90]
  │               │     └── 文件系统清理: 释放磁盘块和资源
  │               ├── remove_inode_hash(inode) → 移出哈希表
  │               ├── inode_sb_list_del(inode)  → 移出 sb->s_inodes
  │               ├── wake_up_bit(I_WILL_FREE)
  │               └── destroy_inode(inode) → kmem_cache_free
  │
  └── inode LRU 入队/出队管理
```

`iput_not_last` (inode.c:2017) 是优化：调用者确信不是最后引用，避免 cmpxchg 开销。

---

## 11. evict_inodes — 批量驱逐 (inode.c:897-938)

```c
void evict_inodes(struct super_block *sb)
{
    struct inode *inode;
    LIST_HEAD(dispose);

again:
    spin_lock(&sb->s_inode_list_lock);
    list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
        if (icount_read(inode))               // 仍在使用? 跳过
            continue;

        spin_lock(&inode->i_lock);
        if (icount_read(inode)) {             // 重检查(锁后)
            spin_unlock(&inode->i_lock);
            continue;
        }
        if (inode_state_read(inode) &
            (I_NEW | I_FREEING | I_WILL_FREE)) {
            spin_unlock(&inode->i_lock);
            continue;
        }

        inode_state_set(inode, I_FREEING);     // 标记为释放
        inode_lru_list_del(inode);             // 移出 LRU
        spin_unlock(&inode->i_lock);
        list_add(&inode->i_lru, &dispose);     // 收集到待释放链

        if (need_resched()) {                  // 太多 inode? 让 CPU
            spin_unlock(&sb->s_inode_list_lock);
            cond_resched();
            dispose_list(&dispose);            // 批量驱逐
            goto again;
        }
    }
    spin_unlock(&sb->s_inode_list_lock);
    dispose_list(&dispose);                    // 最终批量驱逐
}
EXPORT_SYMBOL_GPL(evict_inodes);
```

调用时机：文件系统卸载（umount）时。遍历 sb->s_inodes 中所有 i_count==0 的 inode，收集到 dispose 链表，然后 `dispose_list()` 对每个 inode 调用 `evict()` 清理。

---

## 12. drop_inode 回调

当 i_count 归零时，VFS 调用 `super_operations->drop_inode` 询问：

```c
// 默认策略 (fs/inode.c internal):
int generic_drop_inode(struct inode *inode)
{
    return !inode->i_nlink || inode_unhashed(inode);
    //      硬链接数为0     || 已从哈希表移除
}
```

- **false** → inode 保留在哈希表和 LRU 中。下次查找直接命中，无需磁盘读
- **true** → inode 被驱逐。`evict(inode)` 调用 `sb->s_op->evict_inode` (super_types.h:90)，文件系统释放磁盘块、清理资源

特殊案例：`i_nlink == 0` 但仍有进程打开 (i_count > 0) → inode 不会被驱逐，文件内容在页缓存中持续可用，直到最后一个 close → iput → 驱逐。

---

## 13. Inode LRU 回收

`inode_lru_isolate` (inode.c:952-976) 是注册到 slab shrinker 的回调：

```c
static enum lru_status inode_lru_isolate(struct list_head *item,
        struct list_lru_one *lru, void *arg)
{
    struct inode *inode = container_of(item, struct inode, i_lru);

    if (!spin_trylock(&inode->i_lock))   // trylock避免死锁
        return LRU_SKIP;

    if (icount_read(inode) ||            // 还在使用?
        (inode_state_read(inode) & ~I_REFERENCED) || // 有非引用标志
        !mapping_shrinkable(&inode->i_data)) {       // 页缓存不可收缩?
        list_lru_isolate(lru, &inode->i_lru);        // 移出LRU
        spin_unlock(&inode->i_lock);
        this_cpu_dec(nr_unused);
        return LRU_REMOVED;
    }

    // ... 可回收:
    // I_REFERENCED → 清除标志, 移到 LRU 尾部(再给一次机会)
    // 否则 → 驱逐
}
```

- `list_lru` 实现 per-superblock 的 LRU
- `spin_trylock` 的 i_lock 避免 LRU 锁 → i_lock 的逆序死锁
- `I_REFERENCED` 的 inode 获得第二次机会（移到 LRU 末尾）

---

## 14. Inode 完整生命周期状态机

```
                        不存在
                          │ new_inode() / alloc_inode()
                          ▼
                   ┌──────────────┐
                   │  ALLOCATED   │  I_NEW, count=1, 不在哈希表
                   │  (初始化中)   │
                   └──────┬───────┘
                          │ insert_inode_hash()
                          │ 初始化 i_ino/i_mode/i_size...
                          │ unlock_new_inode() 清 I_NEW
                          ▼
                   ┌──────────────────┐
                   │   ACTIVE (hashed) │  可被查找
         ┌─────────│ 在 inode_hashtable│
         │         │ 在 sb->s_inodes   │
         │         └────────┬─────────┘
         │                  │
         │     ┌────────────┼────────────┐
         │     │ 文件操作     │ mark_dirty │
         │     ▼            ▼             │
         │ ┌───────┐  ┌───────────┐      │
         │ │ CLEAN │  │   DIRTY   │      │
         │ │       │◄─┤ I_DIRTY_* │      │
         │ └───┬───┘  └─────┬─────┘      │
         │     │            │写回完成 │
         │     │    ┌───────▼──────┐      │
         │     │    │  WRITEBACK   │──────┤
         │     │    │  (I_SYNC)    │      │
         │     │    └──────────────┘      │
         │     │                          │
         │     │ 最后 iput() → drop_inode │
         │     ├──────────────┬───────────┘
         │     ▼              ▼
         │ ┌────────┐  ┌──────────────┐
         │ │ 保留在  │  │   EVICTING   │  I_FREEING
         │ │ 缓存中  │  │  (驱逐中)     │
         │ └────────┘  └──────┬───────┘
         │                    │ truncate_inode_pages_final
         │                    │ evict_inode() 回调
         │                    │ remove_inode_hash()
         │                    ▼
         │            ┌──────────────┐
         │            │    FREED     │  RCU 延迟
         │            │  (已释放)     │  kmem_cache_free
         │            └──────────────┘
         │
         └──► (重新被 iget 命中 → 回到 ACTIVE)
```

---

## 15. 总结

| 概念 | 要点 |
|------|------|
| struct inode | 文件系统对象：元数据 + 页缓存 + 锁 |
| i_mapping (780) | address_space，inode 与页缓存的桥梁 |
| i_count (845) | 引用计数，归零时触发驱逐 |
| inode_hashtable | 按 (super_block, ino) 哈希，受 inode_hash_lock 保护 |
| iget5_locked | 查找 + 创建，文件系统提供 test/set 回调 |
| ilookup | 纯查找，不创建 |
| I_NEW | inode 正在初始化，等待者阻塞在 wait_on_inode |
| I_DIRTY_* | 脏标志系列，驱动 writeback 线程回写 |
| 锁层次 | hash_lock → s_inode_list_lock → i_lock → LRU |
| evict_inodes | umount 时批量驱逐所有 inode |
| drop_inode | 文件系统的钩子，决定 inode 是否驱逐 |
| i_nlink==0 | 硬链接删完，但进程仍可持有打开的文件 |
| inode LRU | list_lru per-sb + slab shrinker 回收 |

inode 是 VFS 的"心脏"——连接文件名空间 (dentry)、数据 (page cache) 和存储 (bio)。理解 inode 的生命周期是理解整个 VFS 层的基础。

---

## 下一篇文章

➡️ [第三篇：页缓存与缓冲区 I/O — 从 folio 到 bio](./03-page-cache.md)
