# 第三篇：页缓存与缓冲区 I/O — 从 folio 到 bio

> 源码：`mm/filemap.c` (4752行), `fs/buffer.c` (3078行) | 头文件：`include/linux/fs.h` (3660行), `include/linux/buffer_head.h` (538行)

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 1. 双重缓存架构

Linux 内核在文件 I/O 层面存在两种缓存抽象：

```
 用户态              内核态                       存储设备
┌────────┐    ┌─────────────────────────────┐
│ read   │    │                             │
│ write  │───►│  Page Cache (folio-based)   │  ← 现代主缓存
│ mmap   │    │    mm/filemap.c             │
└────────┘    │    xarray (i_pages)         │
              │     ┌──────────────────┐    │
              │     │  address_space   │    │
              │     │   (fs.h:473)     │    │
              │     └────────┬─────────┘    │
              │              │              │
              │     ┌────────▼─────────┐    │
              │     │  Buffer Cache    │    │  ← 传统块级缓存
              │     │   fs/buffer.c    │    │
              │     │   buffer_head ───┼────┼──► bio → block layer → disk
              │     └──────────────────┘    │
              └─────────────────────────────┘
```

**Page Cache**（`mm/filemap.c`）：以 folio 为单位的页面缓存，使用 xarray 基树管理，是主要缓存层。**Buffer Cache**（`fs/buffer.c`）：以 buffer_head 为单位的块级缓存，用于文件系统元数据（superblock、inode table、位图）和传统文件系统（ext*、FAT）。两者共享同一个 folio：buffer_head 通过 `b_folio` 指向 folio 中的页面，确保一致性。现代文件系统（XFS、btrfs）正迁移到 iomap 替代 buffer_head。

---

## 2. struct address_space (fs.h:473-491)

```c
struct address_space {
    struct inode        *host;             // 474: 所属 inode 或块设备
    struct xarray       i_pages;           // 475: ★ folio 的主存储! xarray 基树
    struct rw_semaphore invalidate_lock;   // 476: 失效/截断的排他锁
    gfp_t               gfp_mask;          // 477: 页面分配的 GFP 标志
    atomic_t            i_mmap_writable;   // 478: VM_SHARED 写映射计数
#ifdef CONFIG_READ_ONLY_THP_FOR_FS
    atomic_t            nr_thps;           // 481: THP 大页计数
#endif
    struct rb_root_cached i_mmap;          // 483: VMA 映射的红黑树(回写用)
    unsigned long       nrpages;           // 484: 页面总数 (xa_lock 保护)
    pgoff_t             writeback_index;   // 485: 回写起始偏移(页号)
    const struct address_space_operations *a_ops; // 486: 操作方法表
    unsigned long       flags;             // 487: AS_* 标志 (错误等)
    errseq_t            wb_err;            // 488: 写回错误序号跟踪
    spinlock_t          i_private_lock;    // 489: 拥有者私有锁
    struct rw_semaphore i_mmap_rwsem;      // 490: 保护 i_mmap + writable
} __attribute__((aligned(sizeof(long)))) __randomize_layout;
```

### 核心字段

**host (474)**：指向拥有此 address_space 的 inode。对块设备，指向 `bdev->bd_inode`。

**i_pages (475)**：xarray——高效的基树（radix tree 的现代替代）。每个条目是一个 folio 指针。支持：
- O(log n) 查找、插入、删除
- **位标记**（mark）进行脏页/回写跟踪：

```c
// include/linux/fs.h:498-501
#define PAGECACHE_TAG_DIRTY      XA_MARK_0  // folio 脏
#define PAGECACHE_TAG_WRITEBACK  XA_MARK_1  // folio 正在回写
#define PAGECACHE_TAG_TOWRITE    XA_MARK_2  // 待回写
```

**i_mmap (483)**：红黑树（`rb_root_cached`），存储所有映射到该文件的 `vm_area_struct`。回写脏页时，需要遍历 i_mmap 找到所有映射此 folio 的进程，刷新 TLB 并使缓存行失效。

**nrpages (484)**：快速空缓存判断——`mapping->nrpages == 0` 表示无缓存页。

**wb_err (488)**：`errseq_t` 类型。每个文件的 `file->f_wb_err` 记录最近见过的错误序号。通过 `filemap_sample_wb_err` / `filemap_check_wb_err` 检测自上次检查以来是否有新的写回错误。

**invalidate_lock (476)**：在 `invalidate_inode_pages2_range` 等截断操作期间阻止新页插入。页缓存一致性关键——防止添加和截断之间的竞态。

---

## 3. struct address_space_operations (fs.h:401-442)

```c
struct address_space_operations {
    int (*read_folio)(struct file *, struct folio *);              // 402
    int (*writepages)(struct address_space *, struct writeback_control *); // 405
    bool (*dirty_folio)(struct address_space *, struct folio *);   // 408
    void (*readahead)(struct readahead_control *);                 // 410
    int (*write_begin)(const struct kiocb *, struct address_space *mapping,
                loff_t pos, unsigned len,
                struct folio **foliop, void **fsdata);             // 412
    int (*write_end)(const struct kiocb *, struct address_space *mapping,
                loff_t pos, unsigned len, unsigned copied,
                struct folio *folio, void *fsdata);                // 415
    ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter);   // 424
    int (*migrate_folio)(struct address_space *, struct folio *dst,
                struct folio *src, enum migrate_mode);              // 429
    bool (*release_folio)(struct folio *, gfp_t);                  // 422
    bool (*is_partially_uptodate)(struct folio *, size_t, size_t); // 432
    void (*is_dirty_writeback)(struct folio *, bool *, bool *);    // 434
    int (*error_remove_folio)(struct address_space *, struct folio *); // 435
};
```

**核心操作对**：
- `read_folio`：从存储设备读取一个 folio 的内容到页缓存
- `write_begin` + `write_end`：两阶段写——`begin` 获取并锁定 folio，`end` 标记脏
- `writepages`：writeback 线程调用，批量将脏 folio 写入存储
- `direct_IO`：绕过页缓存的直接 I/O
- `migrate_folio`：内存迁移（NUMA、compaction）

---

## 4. 读路径：从系统调用到 folio

```
用户 read(fd, buf, 4096)
        │
        ▼ (SYSCALL_DEFINE3(read, ...))
    ksys_read(fd, buf, count)
        │
        ▼ (VFS layer)
    vfs_read(file, buf, count, &pos)        [fs/read_write.c]
        │
        ▼ (file_operations→read_iter)
    generic_file_read_iter(iocb, iter)      [mm/filemap.c:2957]
        │
        ├── IOCB_DIRECT? → direct_IO → 块层 → 磁盘
        │   └── 短读(Btrfs压缩…)? → 回退到 filemap_read
        │
        └── filemap_read(iocb, iter)         [mm/filemap.c:2769]
              │
              ├── filemap_get_pages()        [内部函数]
              │     ├── xarray 查找 folio (O(log n))
              │     ├── 未命中/非uptodate → readahead + read_folio
              │     │     └── a_ops→read_folio → buffer_head → bio → 磁盘
              │     └── 收集到 folio_batch (PAGEVEC_SIZE=15)
              │
              ├── flush_dcache_folio (架构缓存一致性)
              ├── copy_folio_to_iter() → 复制到用户空间
              └── file_accessed() → 更新 atime
```

### generic_file_read_iter (filemap.c:2957-2999)

```c
generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
    size_t count = iov_iter_count(iter);
    ssize_t retval = 0;
    if (!count) return 0;

    if (iocb->ki_flags & IOCB_DIRECT) {
        struct inode *inode = mapping->host;
        retval = kiocb_write_and_wait(iocb, count); // 等回写完成
        file_accessed(file);
        retval = mapping->a_ops->direct_IO(iocb, iter); // 直接读盘
        // ... Btrfs 压缩短读回退逻辑 ...
    }
    return filemap_read(iocb, iter, retval); // ← 通用页缓存读
}
```

DIO 路径的特殊处理：`kiocb_write_and_wait` 确保所有脏数据先写入磁盘，避免 DIO 读到旧数据。短读场景（Btrfs 压缩、EOF）回退到 `filemap_read` 走页缓存。

### filemap_read — 核心批量读 (filemap.c:2769-2881)

```c
ssize_t filemap_read(struct kiocb *iocb, struct iov_iter *iter,
        ssize_t already_read)
{
    struct address_space *mapping = filp->f_mapping;
    struct folio_batch fbatch;
    // 边界检查...

    folio_batch_init(&fbatch);
    do {
        cond_resched();

        // ★ 批量获取 folio (预读+锁+等待uptodate)
        error = filemap_get_pages(iocb, iter->count, &fbatch, false);

        isize = i_size_read(inode);
        end_offset = min_t(loff_t, isize, iocb->ki_pos + iter->count);
        writably_mapped = mapping_writably_mapped(mapping);

        for (i = 0; i < folio_batch_count(&fbatch); i++) {
            struct folio *folio = fbatch.folios[i];
            offset = iocb->ki_pos & (folio_size(folio) - 1);
            bytes = min_t(loff_t, end_offset - iocb->ki_pos,
                          folio_size(folio) - offset);

            if (writably_mapped)
                flush_dcache_folio(folio); // 架构级缓存一致性

            // ★ 复制到用户空间
            copied = copy_folio_to_iter(folio, offset, bytes, iter);
            already_read += copied;
            iocb->ki_pos += copied;
        }
        // 释放所有 folio 引用
        for (i = 0; i < folio_batch_count(&fbatch); i++)
            folio_put(fbatch.folios[i]);
    } while (iov_iter_count(iter) && iocb->ki_pos < isize && !error);

    file_accessed(filp);
    return already_read ? already_read : error;
}
```

### filemap_get_pages 内部逻辑

1. 在 xarray 中查找 `iocb->ki_pos` 对应的 folio
2. 不存在或非 uptodate → 触发预读 (`filemap_readahead`) → 等待 folio 锁定 → 等待 uptodate
3. 批量收集连续的 folio → `folio_batch`（一次最多 `PAGEVEC_SIZE=15`）
4. 返回后 `filemap_read` 遍历复制

folio_batch 批量收集减少 xarray 锁的获取次数和 `copy_to_user` 的调用开销。

---

## 5. filemap_fault — mmap 缺页中断 (filemap.c:3513)

`filemap_fault` 是 VMA 的 `.fault` 回调，处理 mmap 映射文件的缺页：

```c
vm_fault_t filemap_fault(struct vm_fault *vmf)
{
    struct address_space *mapping = file->f_mapping;
    pgoff_t index = vmf->pgoff;
    struct folio *folio;

    // 1. 在 xarray 中查找
    folio = filemap_get_folio(mapping, index);
    if (likely(!IS_ERR(folio))) {
        // 找到了! 触发异步预读
        if (!(vmf->flags & FAULT_FLAG_TRIED))
            fpin = do_async_mmap_readahead(vmf, folio);
    } else {
        // 主缺页 (MAJOR FAULT) — 需要读磁盘
        count_vm_event(PGMAJFAULT);
        fpin = do_sync_mmap_readahead(vmf);
        // 分配并加入 folio:
        folio = __filemap_get_folio(mapping, index,
                          FGP_CREAT|FGP_FOR_MMAP, vmf->gfp_mask);
    }

    // 2. 锁 folio (可能需要释放 mmap_lock 避免死锁)
    lock_folio_maybe_drop_mmap(vmf, folio, &fpin);

    // 3. 截断检查
    if (unlikely(folio->mapping != mapping)) { retry; }

    // 4. uptodate 检查 —— 不是最新则 read_folio
    if (unlikely(!folio_test_uptodate(folio)))
        goto page_not_uptodate;  // → 可能需要 read_folio

    // 5. 插入进程页表
    // ... vmf_insert_* → set_pte ...
    return VM_FAULT_LOCKED;
}
```

流程：
1. xarray 查找 → 命中触发异步预读 → 未命中触发同步预读 + 分配 folio
2. `lock_folio_maybe_drop_mmap`：锁 folio，如果锁竞争可能释放 mmap_lock
3. 验证 `folio->mapping == mapping`（防止在锁竞争期间被截断）
4. 非 uptodate → `read_folio` → 磁盘 I/O → 等待 → 插入页表
5. 返回 VM_FAULT_LOCKED / VM_FAULT_MAJOR / VM_FAULT_OOM

---

## 6. 写路径

```
用户 write(fd, buf, count)
        │
        ▼
    ksys_write → vfs_write
        │
        ▼
    generic_file_write_iter(iocb, from)   [filemap.c:4459]
        │
        ├── inode_lock(inode) — 获取 i_rwsem 写锁
        ├── generic_write_checks — 边界/权限/文件大小扩展
        │
        └── __generic_file_write_iter     [内部循环]
              │
              └── 循环每段数据:
                    ├── write_begin(pos, len)
                    │     ├── xarray 查找/创建 folio
                    │     ├── 确保 folio uptodate (触发 read_folio)
                    │     └── 锁定 folio
                    │
                    ├── copy_from_user → 数据复制到 folio
                    │
                    └── write_end(pos, len, copied, folio)
                          ├── folio_mark_dirty
                          ├── 更新 i_size (如果扩展了文件)
                          └── folio_unlock
```

### generic_file_write_iter (filemap.c:4459-4474)

```c
ssize_t generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
    struct inode *inode = file->f_mapping->host;
    ssize_t ret;

    inode_lock(inode);                       // i_rwsem 写锁
    ret = generic_write_checks(iocb, from);  // 杀 SUID、文件大小检查
    if (ret > 0)
        ret = __generic_file_write_iter(iocb, from);
    inode_unlock(inode);

    if (ret > 0)
        ret = generic_write_sync(iocb, ret); // O_SYNC/O_DSYNC 强制回写
    return ret;
}
```

### write_begin / write_end 两阶段设计

分离允许 VFS 在两个调用之间做 `copy_from_user`：持锁时间最小化，文件系统在 `write_end` 中进行事务提交（如 ext4 journal）。

---

## 7. struct buffer_head (buffer_head.h:60-82)

```c
struct buffer_head {
    unsigned long b_state;            // 61: 状态位 (BH_Uptodate, BH_Dirty…)
    struct buffer_head *b_this_page;  // 62: 页面内循环链表
    union {
        struct page *b_page;          // 64: 关联页面(legacy)
        struct folio *b_folio;        // 65: 关联 folio (modern)
    };

    sector_t b_blocknr;               // 68: 磁盘逻辑块号
    size_t b_size;                    // 69: buffer 大小(通常=blksize)
    char *b_data;                     // 70: 指向页内数据的指针

    struct block_device *b_bdev;      // 72: 块设备
    bh_end_io_t *b_end_io;            // 73: I/O 完成回调
    void *b_private;                  // 74: 回调私有数据
    struct list_head b_assoc_buffers; // 75: 关联的映射链表
    struct mapping_metadata_bhs *b_mmb; // 76: 元数据 bh 链表头
    atomic_t b_count;                 // 78: 引用计数
    spinlock_t b_uptodate_lock;       // 79: uptodate 序列化 (第一个bh)
};
```

**buffer_head 与 folio 的关联**：

```
folio
┌──────────────────────────────────────┐
│        page (PAGE_SIZE 4096)         │
│ ┌────────┬────────┬────────┬──────┐ │
│ │ bh[0]  │ bh[1]  │ bh[2]  │bh[3] │ │  (如果 blksize=1024)
│ │ b_data │ b_data │ b_data │b_data│ │
│ │ Uptod? │ Dirty! │ Dirty! │Clean │ │
│ └───┬────┴────┬───┴────┬───┴──┬───┘ │
│     │ p_next→ │ p_next→│p_nxt→│ ↺   │  b_this_page 循环链表
└─────┼─────────┼────────┼──────┼──────┘
      │         │        │      │
      ▼         ▼        ▼      ▼
  block N   block N+1 block N+2 block N+3  (磁盘)
```

- `folio_buffers(folio)` → 获取该 folio 的第一个 buffer_head
- `b_this_page` → 页面内循环链表，遍历所有 buffer_head
- `b_data` → 指向页内偏移的指针
- `b_bdev` + `b_blocknr` → 唯一标识磁盘位置

---

## 8. buffer_head 状态标志和 API

```
BH_Uptodate   — buffer 数据最新
BH_Dirty      — buffer 数据脏 (需回写)
BH_Lock       — I/O 进行中 (lock_buffer 阻塞到 unlock)
BH_Req        — 已提交 I/O 请求
BH_Mapped     — 有磁盘映射 (b_blocknr 有效)
BH_New        — 新分配的 buffer
BH_Delay      — 延迟分配 (未分配磁盘块)
BH_Unwritten  — 分配但未写入
BH_Meta       — 元数据 buffer (调度器可优先)
BH_Prio       — 高优先级 I/O
```

宏展开（buffer_head.h:90-103）生成：
```c
buffer_uptodate(bh), set_buffer_dirty(bh), buffer_dirty(bh)
clear_buffer_dirty(bh), lock_buffer(bh), unlock_buffer(bh)
```

`lock_buffer` (buffer.c:69-72) 使用 `wait_on_bit_lock_io(&bh->b_state, BH_Lock, TASK_UNINTERRUPTIBLE)` 阻塞等待。

---

## 9. __bread_gfp — 同步读块 (buffer.c:1399-1418)

```c
struct buffer_head *__bread_gfp(struct block_device *bdev, sector_t block,
        unsigned size, gfp_t gfp)
{
    struct buffer_head *bh;
    gfp |= mapping_gfp_constraint(bdev->bd_mapping, ~__GFP_FS);
    gfp |= __GFP_NOFAIL;                     // 不失败原则

    bh = bdev_getblk(bdev, block, size, gfp); // 获取或分配 bh

    if (likely(bh) && !buffer_uptodate(bh))
        bh = __bread_slow(bh);               // 非最新 → 等待 I/O
    return bh;
}
```

同步读流程：
```
__bread_gfp(bdev, block, 1024, GFP_NOFS)
  │
  ├── bdev_getblk(bdev, block, size, gfp)
  │     ├── __find_get_block → bh LRU 查找
  │     │     └── 命中 → b_count++, 返回
  │     └── __getblk_slow → 分配 bh + 关联 folio
  │           └── 找不到 folio → 分配新 folio + attach bh
  │
  ├── buffer_uptodate(bh)?
  │     ├── YES → 直接返回
  │     └── NO  → __bread_slow(bh)
  │           ├── lock_buffer(bh) → 等之前 I/O 完成
  │           ├── 再查 uptodate? → unlock+return
  │           └── bh->b_end_io = end_buffer_read_sync
  │               submit_bh(REQ_OP_READ, bh)
  │               wait_on_bit_io(BH_Lock) ← 阻塞等 I/O 完成
  │               buffer_uptodate? → 返回
```

---

## 10. submit_bh_wbc — I/O 提交 (buffer.c:2692-2738)

```c
static void submit_bh_wbc(blk_opf_t opf, struct buffer_head *bh,
                          enum rw_hint write_hint,
                          struct writeback_control *wbc)
{
    struct bio *bio;

    // 保证: 已锁, 已映射, 有 end_io
    BUG_ON(!buffer_locked(bh) || !buffer_mapped(bh) || !bh->b_end_io);

    // 重写时清除旧错误
    if (test_set_buffer_req(bh) && (op == REQ_OP_WRITE))
        clear_buffer_write_io_error(bh);
    if (buffer_meta(bh)) opf |= REQ_META;    // 元数据 → 调度器优待
    if (buffer_prio(bh)) opf |= REQ_PRIO;

    bio = bio_alloc(bh->b_bdev, 1, opf, GFP_NOIO);

    // ★ bh → bio 映射:
    bio->bi_iter.bi_sector = bh->b_blocknr * (bh->b_size >> 9);
    bio->bi_write_hint = write_hint;
    bio_add_folio_nofail(bio, bh->b_folio, bh->b_size, bh_offset(bh));
    bio->bi_end_io = end_bio_bh_io_sync;
    bio->bi_private = bh;

    guard_bio_eod(bio);                   // 截断到设备末尾
    if (wbc) {
        wbc_init_bio(wbc, bio);           // writeback 统计
        wbc_account_cgroup_owner(wbc, bh->b_folio, bh->b_size);
    }
    blk_crypto_submit_bio(bio);           // 提交到块层!
}

// 便捷接口 (buffer.c:2740-2744)
void submit_bh(blk_opf_t opf, struct buffer_head *bh)
{
    submit_bh_wbc(opf, bh, WRITE_LIFE_NOT_SET, NULL);
}
```

bh → bio 转换：块号×块大小→扇区号 → `bio_add_folio_nofail` 添加 folio 片段 → `blk_crypto_submit_bio`。

---

## 11. I/O 完成回调

```c
// buffer.c:158-163
void end_buffer_read_sync(struct buffer_head *bh, int uptodate)
{
    put_bh(bh);                              // 释放 __bread_gfp 的额外引用
    __end_buffer_read_notouch(bh, uptodate); // uptodate→set_uptodate, unlock
}

// buffer.c:165-177
void end_buffer_write_sync(struct buffer_head *bh, int uptodate)
{
    if (uptodate) set_buffer_uptodate(bh);
    else { buffer_io_error(bh, ", lost sync page write"); clear… }
    unlock_buffer(bh);       // 唤醒 wait_on_buffer 的等待者
    put_bh(bh);
}
```

```
完成流:
block layer → bio->bi_end_io = end_bio_bh_io_sync(bio)
  → bh = bio->bi_private
  → bh->b_end_io(bh, !bio->bi_status)
      ├── end_buffer_read_sync → __end_buffer_read_notouch
      │     → set_buffer_uptodate + clear BH_Lock(unlock)
      └── end_buffer_write_sync → set/clear_uptodate + unlock
```

---

## 12. Writeback 回写路径

回写由内核 writeback 线程统一调度，而非 write 系统调用直接触发：

```
脏页产生:
  write → write_end → folio_mark_dirty
  mmap → PTE dirty 位 → __set_page_dirty (通过页表扫描)

回写触发:
  ├── 定期: wb_workfn (bdi_writeback 工作队列)
  ├── 内存压力: shrinker → shrink_page_list → writepages
  ├── 显式: sync() / fsync() / fdatasync()
  └── 脏页比例: /proc/sys/vm/dirty_ratio 超限

回写执行:
  wb_workfn → wb_writeback → writeback_sb_inodes
    → __writeback_single_inode(inode, wbc)
        ├── do_writepages(mapping, wbc)
        │     → mapping->a_ops->writepages(mapping, wbc)  // ext4_writepages
        │         → 收集脏 folio (通过 PAGECACHE_TAG_DIRTY)
        │         → bio_alloc + bio_add_folio(多页合并)
        │         → submit_bio → block layer → disk
        └── write_inode(inode, wbc)
              → sb->s_op->write_inode(inode, wbc)  // 回写元数据

evict 时脏页丢弃:
  evict(inode) → truncate_inode_pages_final(&inode->i_data)
    → 遍历 xarray 所有 folio
        → 脏 → cancel_dirty_page() 丢弃(文件已删)
        → 从 xarray 删除 → put_folio
```

---

## 13. Folio 时代的变更

| 旧术语 | 新术语 | 说明 |
|--------|--------|------|
| struct page | struct folio | 可包含多个连续物理页 |
| readpage → read_folio | a_ops 读操作 | |
| writepage → writepages | 批量回写 | 单页回写已废弃 |
| lock_page → folio_lock | 锁 folio | |
| set_page_dirty → folio_mark_dirty | 标记脏 | |
| page_mapping → folio_mapping | 获取 mapping | |
| PageUptodate → folio_test_uptodate | uptodate 检查 | |

迁移理由：
1. **THP 大页**：folio 可以是 2MB（512 基页），一次操作代替 512 次
2. **类型安全**：编译器可区分 folio* 和 page*
3. **减少 cache 污染**：folio 的 head struct 放最前面，tail pages 不含元数据

### iomap — 现代替代

传统路径：write_begin/write_end + 每个块的 buffer_head 分配 → 锁开销大
现代路径：`iomap` 一次映射整个 I/O 范围：

```c
// iomap 接口 (fs/iomap/):
iomap_file_buffered_write()    // 代替 write_begin/write_end
iomap_file_unshare()
iomap_zero_range()
iomap_truncate_page()
```

XFS 和 btrfs 已主要使用 iomap，减少了 buffer_head 的创建和锁开销。

---

## 14. DAX — 绕过页缓存

```
传统:            DAX (Direct Access):
                 
用户 buffer       进程 mmap 地址
  copy_from_user    │ CPU load/store (直接!)
  ▼                 ▼
page cache        持久内存 (PMEM/NVDIMM)
  buffer_head       │ 内存总线
  ▼                 ▼
block layer       持久内存 DIMM
  ▼
disk
```

`fs/dax.c` (~61KB) 实现 DAX 模式：
- 绕过页缓存、buffer_head、bio、block layer
- 持久内存的物理地址直接映射到进程页表
- `dax_iomap_fault` 通过 `pfn_t` 插入 PMEM 页到页表
- `IS_DAX(inode)` 判断是否在 DAX 模式

适用：数据库、虚拟机镜像等延迟敏感、自管理缓存的应用。

---

## 15. 读路径完整 ASCII 流程

```
read(fd, buf, 4096)
  │
  ▼
vfs_read()
  │
  ▼
generic_file_read_iter(iocb, iter)              [filemap.c:2957]
  │
  ├── IOCB_DIRECT?
  │     └── YES → kiocb_write_and_wait() — 排空脏数据
  │              → direct_IO() — 直接读盘(PBLOCK)
  │              → 短读? → 回退到缓冲 I/O
  │
  └── filemap_read(iocb, iter, retval)           [filemap.c:2769]
        │
        ├── filemap_get_pages(&fbatch)            [内部函数]
        │     │
        │     ├── xa_load() — xarray 查找 folio
        │     ├── 命中 → folio_get, folio_mark_accessed
        │     └── 未命中 → filemap_readahead()
        │           └── a_ops→readahead()
        │               └── read_folio()            [a_ops:402]
        │                   ├── buffer_head: bdev_getblk→__bread_slow
        │                   │   → submit_bh(READ) → block layer   [buffer.c:2740]
        │                   ├── iomap: iomap_read_folio
        │                   │   → submit_bio → block layer
        │                   └── block layer → SCSI/NVMe → 磁盘
        │                       end_io → set_uptodate → unlock folio
        │
        ├── for each folio in fbatch:
        │     ├── flush_dcache_folio() — 架构缓存一致性
        │     ├── copy_folio_to_iter() — copy_to_user
        │     └── folio_put() — 释放引用
        │
        └── file_accessed() — 更新 atime (如果 atime 更新策略)
```

---

## 16. 总结

| 组件 | 位置 | 职责 |
|------|------|------|
| address_space | fs.h:473 | inode 的页缓存抽象，连接 inode 和 folio |
| i_pages (xarray) | fs.h:475 | 高效基树，存储 folio，支持标记 |
| read_folio | a_ops:402 | 从磁盘读取一个 folio 到缓存 |
| write_begin/end | a_ops:412/415 | 两阶段写：准备 folio + 标记脏 |
| writepages | a_ops:405 | 批量回写脏 folio |
| direct_IO | a_ops:424 | 绕过页缓存的直接 I/O |
| buffer_head | buffer_head.h:60 | 传统块级缓存，连接 folio 和块设备 |
| __bread_gfp | buffer.c:1399 | 同步块读取（元数据常用） |
| submit_bh | buffer.c:2740 | 提交 bh 到 bio/块层 |
| end_buffer_read_sync | buffer.c:158 | I/O 完成回调，unlock+set_uptodate |
| filemap_read | filemap.c:2769 | 核心批量读（folio_batch 收集） |
| generic_file_read_iter | filemap.c:2957 | 通用读入口（DIO + buffered） |
| filemap_fault | filemap.c:3513 | mmap 缺页处理 |
| generic_file_write_iter | filemap.c:4459 | 通用写入口 |
| writeback (flusher) | fs/fs-writeback.c | 定期回写脏页线程 |
| folio | — | 现代页面单元，可多个物理页 |
| iomap | fs/iomap/ | buffer_head 的现代替代 |

理解 `address_space → xarray → folio → buffer_head → bio → block layer` 这条数据通路是深入 VFS 与存储层交互的关键。

---

## 下一篇文章

➡️ [第四篇：路径名查找 — 从 open() 到 inode](./04-path-lookup.md)
