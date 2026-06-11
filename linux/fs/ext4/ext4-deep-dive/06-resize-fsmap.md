# 第六篇：在线扩容与 FSMap — 不停机扩展与块所有者查询

> 源码：fs/ext4/resize.c (2192 lines) · fs/ext4/fsmap.c (792 lines)
> 头文件：fs/ext4/ext4.h · fs/ext4/fsmap.h

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. 概述

ext4 是少数支持**在线扩容**的文件系统之一——无需卸载、无需停机，就能扩展文件系统容量。这项能力的背后是精巧的元数据处理：新增块组描述符、更新超级块、调整伙伴位图，一切都在 journal 保护下完成。

与扩容相辅相成的是 **FSMap**（FS_IOC_GETFSMAP）ioctl —— 一种块级取证工具，能报告每个物理块的所有者（超级块、GDT、inode table、日志、数据块或空闲块）。它建立在 mballoc 的查询基础设施之上，为磁盘取证、碎片分析和管理工具提供基础数据。

本篇将逐层剖析这两个子系统的完整实现：

| 函数 | 文件:行号 | 功能 |
|------|-----------|------|
| `ext4_resize_begin` | resize.c:46 | 扩容前检查和加锁 |
| `ext4_flex_group_add` | resize.c:1535 | 添加新 flex group |
| `ext4_group_extend` | resize.c:1834 | 扩展最后一个块组 |
| `ext4_setup_new_descs` | resize.c:1338 | 初始化新组描述符 |
| `ext4_update_super` | resize.c:1418 | 更新超级块统计 |
| `ext4_getfsmap` | fsmap.c:703 | FSMap 主入口 |
| `ext4_getfsmap_helper` | fsmap.c:85 | 格式化输出记录 |
| `ext4_getfsmap_datadev` | fsmap.c:528 | 数据设备块遍历 |

---

## 2. 扩容前检查：ext4_resize_begin

`ext4_resize_begin`（resize.c:46）是所有扩容操作的入口栅检查，它在真正开始操作之前验证所有前置条件：

```c
// resize.c:46
int ext4_resize_begin(struct super_block *sb)
```

**检查清单**：

```
ext4_resize_begin(sb)
  │
  ├─[1] CAP_SYS_RESOURCE 权限检查                           (line 51)
  │     无权限 → -EPERM
  │
  ├─[2] resize_inode 一致性检查                             (line 58-62)
  │     s_reserved_gdt_blocks>0 但 !FEATURE_RESIZE_INODE
  │     → -EFSCORRUPTED
  │
  ├─[3] 必须使用主超级块 (非备份)                             (line 69-74)
  │     s_sbh->b_blocknr != s_first_data_block
  │     → -EPERM
  │
  ├─[4] 文件系统无错误                                       (line 80-84)
  │     s_mount_state & EXT4_ERROR_FS
  │     → -EPERM (错误状态下扩容风险太高)
  │
  ├─[5] sparse_super2 不支持在线扩容                          (line 86-89)
  │     → -EOPNOTSUPP
  │
  └─[6] test_and_set_bit_lock(EXT4_FLAGS_RESIZING)          (line 91-93)
        防止并发扩容 → -EBUSY
```

**EXT4_FLAGS_RESIZING** 锁是原子操作的位锁，确保一次只有一个进程执行扩容。`ext4_resize_end`（resize.c:98）在操作完成后清除此标志。

```c
// resize.c:98-105
int ext4_resize_end(struct super_block *sb, bool update_backups)
{
    clear_bit_unlock(EXT4_FLAGS_RESIZING, &sbi->s_ext4_flags);
    smp_mb__after_atomic();  // 内存屏障
    if (update_backups)
        return ext4_update_overhead(sb, true);
    return 0;
}
```

---

## 3. 两种扩容模式

ext4 支持两种在线扩容模式：

```
┌──────────────────────────────────────────────────────────┐
│                    在线扩容模式                            │
├──────────────────┬───────────────────────────────────────┤
│ flex_group_add   │  添加完整的新 flex group(s)            │
│ (resize.c:1535)  │  增加组数，增加块数，增加 inode 数      │
│                  │  mkfs 后从未被填充的连续空间             │
├──────────────────┼───────────────────────────────────────┤
│ group_extend     │  扩展最后一个块组的大小                  │
│ (resize.c:1834)  │  不增加组数，只增加最后一个组的块数      │
│                  │  用于设备扩容后末尾残留空间               │
└──────────────────┴───────────────────────────────────────┘
```

### 3.1 ext4_flex_group_add — 添加新 Flex Group

`ext4_flex_group_add`（resize.c:1535）是最常用的扩容方式，每次添加一个完整的 flex group（多个 block group 的捆绑）。

```c
// resize.c:1535
static int ext4_flex_group_add(struct super_block *sb,
    struct inode *resize_inode,
    struct ext4_new_flex_group_data *flex_gd)
```

**输入结构**：`ext4_new_flex_group_data` 包含：
- `groups[]` — 每个新块组的 `ext4_new_group_data`
- `count` — 新组的数量
- `bg_flags[]` — 每个组的标志（如 `EXT4_BG_INODE_ZEROED` 等）

**完整流程**：

```
ext4_flex_group_add(sb, resize_inode, flex_gd)
  │
  ├─[1] setup_new_flex_group_blocks(sb, flex_gd)              (line 1555)
  │     确保新块组的块在设备上存在且可写入
  │     也涉及块位图的初始化
  │
  ├─[2] 计算 journal credit:                                  (line 1565)
  │     credit = 3                    // sb, resize inode, dindirect
  │            + 1 + DIV_ROUND_UP(count, DESC_PER_BLOCK)  // GDT
  │            + reserved_gdb          // Reserved GDT blocks
  │
  ├─[3] handle = ext4_journal_start_sb(sb, HT_RESIZE, credit) (line 1569)
  │
  ├─[4] ext4_journal_get_write_access(sb, s_sbh)              (line 1576)
  │     获取超级块的写访问
  │
  ├─[5] ext4_add_new_descs(handle, sb, group,                 (line 1583)
  │         resize_inode, count)
  │     在 GDT 中为新组添加描述符条目
  │     处理 resize_inode 备份（非 meta_bg 模式）
  │
  ├─[6] ext4_setup_new_descs(handle, sb, flex_gd)             (line 1588)
  │     初始化新组描述符的详细内容
  │
  ├─[7] ext4_update_super(sb, flex_gd)                        (line 1592)
  │     更新超级块的块计数和 inode 计数
  │
  ├─[8] ext4_handle_dirty_metadata(s_sbh)                     (line 1594)
  │
  ├─[9] ext4_journal_stop(handle)                             (line 1597)
  │
  └─[10] update_backups()                                     (line 1610)
         将超级块和 GDT 更新传播到所有备份位置
```

### 3.2 ext4_group_extend — 扩展最后一个块组

`ext4_group_extend`（resize.c:1834）处理设备扩容后最后一个块组的膨胀：

```c
// resize.c:1834
int ext4_group_extend(struct super_block *sb,
    struct ext4_super_block *es,
    ext4_fsblk_t n_blocks_count)
```

**为什么需要这个函数？**
当底层块设备（如 LVM LV 或虚拟磁盘）增大时，最后一个块组往往不满。`ext4_group_extend` 直接将空闲块追加到最后一个块组，无需创建新的 block group 描述符。

**流程**：

```
ext4_group_extend(sb, es, n_blocks_count)
  │
  ├─[1] 验证:
  │     n_blocks_count > o_blocks_count (不可缩小)              (line 1860-1863)
  │     n_blocks_count 不溢出设备地址空间                       (line 1853-1857)
  │
  ├─[2] 计算最后一个块组的剩余空间:
  │     ext4_get_group_no_and_offset(o_blocks_count, &group, &last) (line 1866)
  │     last == 0? → 组刚好满，改用 flex_group_add              (line 1868)
  │     add = BLOCKS_PER_GROUP - last                          (line 1873)
  │     add = min(add, n_blocks_count - o_blocks_count)        (line 1880)
  │
  ├─[3] 检查设备容量:
  │     ext4_sb_bread(o_blocks_count + add - 1)                (line 1888)
  │     读取最后一个块验证设备容量 → IS_ERR → -ENOSPC
  │
  └─[4] ext4_group_extend_no_check(sb, o_blocks_count, add)    (line 1895)
        实际的块扩展操作:
        ├─更新 block bitmap (新块标记为空闲)
        ├─更新 buddy cache
        ├─更新组描述符中的 free_blocks_count
        └─更新超级块
```

---

## 4. 组描述符初始化：ext4_setup_new_descs

`ext4_setup_new_descs`（resize.c:1338）负责初始化新增块组的组描述符（group descriptor）：

```c
// resize.c:1338
static int ext4_setup_new_descs(handle_t *handle,
    struct super_block *sb,
    struct ext4_new_flex_group_data *flex_gd)
```

**流程**：

```
ext4_setup_new_descs(handle, sb, flex_gd)
  │
  for (i = 0; i < flex_gd->count; i++):
  │
  ├─ 定位 GDT 块:                                           (line 1353-1361)
  │   gdb_off = group % DESC_PER_BLOCK
  │   gdb_num = group / DESC_PER_BLOCK
  │   gdb_bh = sbi->s_group_desc[gdb_num]
  │
  ├─ 定位组描述符:                                           (line 1361-1362)
  │   gdp = gdb_bh->b_data + gdb_off * DESC_SIZE(sb)
  │
  ├─ memset(gdp, 0, DESC_SIZE)                              (line 1364)
  │
  ├─ 设置关键字段:
  │   ext4_block_bitmap_set(sb, gdp, block_bitmap)          (line 1365)
  │   ext4_inode_bitmap_set(sb, gdp, inode_bitmap)          (line 1366)
  │   ext4_inode_table_set(sb, gdp, inode_table)            (line 1373)
  │   ext4_free_group_clusters_set(sb, gdp, free_clusters)  (line 1374)
  │   ext4_free_inodes_set(sb, gdp, INODES_PER_GROUP)       (line 1376)
  │   ext4_itable_unused_set(sb, gdp, INODES_PER_GROUP)    (line 1378)
  │   gdp->bg_flags = *bg_flags                             (line 1380)
  │
  ├─ ext4_group_desc_csum_set(sb, group, gdp)               (line 1381)
  │   设置组描述符校验和（metadata checksum）
  │
  ├─ ext4_handle_dirty_metadata(gdb_bh)                     (line 1383)
  │
  └─ ext4_mb_add_groupinfo(sb, group, gdp)                  (line 1393)
      将新组注册到 mballoc 的 per-group 信息数组中
```

**关键点**：`ext4_mb_add_groupinfo`（第1393行）在这里被调用，将新块组添加到多块分配器的数据结构中。这确保新块组立即可用于块分配。

---

## 5. 超级块更新：ext4_update_super

`ext4_update_super`（resize.c:1418）在添加新块组后更新超级块的全局计数：

```c
// resize.c:1418
static void ext4_update_super(struct super_block *sb,
    struct ext4_new_flex_group_data *flex_gd)
```

**内存屏障协议**：

```c
// resize.c:1477
smp_wmb();
```

这是一个**写内存屏障**，确保在更新 `s_groups_count` 之前，所有从属数据（`s_blocks_count`, `s_inodes_count` 等）已经对所有 CPU 可见。

协议的另半部分在读者端：
```c
// 读者端示例
count = sbi->s_groups_count;
smp_rmb();  // 读内存屏障
// 现在可以安全读取 s_blocks_count 等
```

**更新内容**：

```
ext4_update_super(sb, flex_gd)
  │
  ├─ 遍历 flex_gd->groups:                                       (line 1440)
  │   blocks_count += group_data[i].blocks_count
  │   free_blocks  += group_data[i].free_clusters_count
  │
  ├─ 更新超级块 disk 字段:                                        (line 1450)
  │   ext4_blocks_count_set(es, es->s_blocks_count + blocks_count)
  │   ext4_free_blocks_count_set(es, es->s_free_blocks_count + free_blocks)
  │   le32_add_cpu(&es->s_inodes_count, INODES_PER_GROUP * count)
  │   le32_add_cpu(&es->s_free_inodes_count, INODES_PER_GROUP * count)
  │
  ├─ smp_wmb()                                                    (line 1477)
  │   内存屏障：所有数据写入完成后才更新组数
  │
  ├─ sbi->s_groups_count += flex_gd->count                        (line 1480)
  │   更新 s_blockfile_groups（块文件最大组数）                     (line 1481)
  │
  ├─ ext4_r_blocks_count_set — 预留块按比例增加                    (line 1486)
  │
  └─ percpu_counter_add — 更新 per-CPU 空闲计数器                  (line 1490-1493)
     s_freeclusters_counter, s_freeinodes_counter
```

---

## 6. Meta_bg 格式与 resize_inode

### 6.1 Meta Block Groups (meta_bg)

传统 ext2/ext3 文件系统中，所有块组描述符（GDT）都在第一个块组中。对于超大文件系统（>2^32 blocks），GDT 本身可能超过一个块组。

**meta_bg** 解决了这个问题：块组描述符不再局限于第一组，而是散布在各个块组中，每个"meta block group"（通常是 64 个组）有自己的 GDT 块。

```
传统布局 (non-meta_bg):
  Group 0: [superblock, GDT(for ALL groups), reserved GDT, block bitmap, ...]
  Group 1: [data...]
  Group 2: [data...]

meta_bg 布局 (groups_per_meta=64):
  Group 0:  [superblock, GDT(for groups 0-63), ...]
  Group 1:  [GDT(for groups 1-63 的备份), data...]
  Group 64: [GDT(for groups 64-127), ...]
  Group 65: [GDT(for groups 65-127 的备份), data...]
```

meta_bg 让扩容可以轻松添加 GDT 块，不再受限于第一组的空间限制。

### 6.2 resize_inode

对于**非 meta_bg** 的文件系统，ext4 使用**resize_inode** 来管理 GDT 的扩展。这是一个特殊的 inode，存储着指向 GDT 备份块的间接块指针。

当添加新组时，`ext4_add_new_descs`（resize.c:约1000行）会：
1. 在 GDT 中添加新组描述符
2. 更新 resize_inode 的间接块指针（如果需要新的 GDT 备份块）
3. 处理 reserved GDT block 的分配

随着文件系统增长到 meta_bg 阈值（通常是 128 个组），可以调用 `ext4_convert_meta_bg`（resize.c:1909）将文件系统转换为 meta_bg 格式，之后就不再需要 resize_inode 了。

---

## 7. FSMap — FS_IOC_GETFSMAP

FS_IOC_GETFSMAP 是一个 ioctl（`FS_IOC_GETFSMAP`），提供底层的"哪个块属于谁"的查询能力。它最初在 XFS 中实现，ext4 在 Linux 4.12 中跟进。

### 7.1 查询状态结构

```c
// fsmap.c:42-54
struct ext4_getfsmap_info {
    struct ext4_fsmap_head  *gfi_head;         // 用户提供的查询头
    ext4_fsmap_format_t     gfi_formatter;     // 格式化回调
    void                    *gfi_format_arg;   // 格式化参数
    ext4_fsblk_t            gfi_next_fsblk;    // 上次返回的最后一个块+1
    u32                     gfi_dev;           // 设备 ID
    ext4_group_t            gfi_agno;          // 当前组号
    struct ext4_fsmap       gfi_low;           // 下限查询键
    struct ext4_fsmap       gfi_high;          // 上限查询键
    struct ext4_fsmap       gfi_lastfree;      // 上一组尾部的空闲区域
    struct list_head        gfi_meta_list;     // 固定元数据链表
    bool                    gfi_last;          // 最后一个 extent?
};
```

**关键字段说明**：

| 字段 | 作用 |
|------|------|
| `gfi_next_fsblk` | 跟踪连续扫描进度，用于检测块之间的"间隙" |
| `gfi_low` / `gfi_high` | 用户指定的扫描范围，{physical, owner} 二维键 |
| `gfi_lastfree` | 跨块组边界的空闲区域跟踪 |
| `gfi_meta_list` | 本组内的固定元数据（有序链表），用于后续与空闲块的交叉输出 |

### 7.2 块所有者常量

```c
// fsmap.h:46-54
#define EXT4_FMR_OWN_FREE       FMR_OWN_FREE        // 0: 空闲空间
#define EXT4_FMR_OWN_UNKNOWN    FMR_OWN_UNKNOWN     // 0: 未知所有者
#define EXT4_FMR_OWN_FS         FMR_OWNER('X', 1)   // 静态 fs 元数据 (超级块)
#define EXT4_FMR_OWN_LOG        FMR_OWNER('X', 2)   // 日志
#define EXT4_FMR_OWN_INODES     FMR_OWNER('X', 5)   // inode 表
#define EXT4_FMR_OWN_GDT        FMR_OWNER('f', 1)   // 组描述符表
#define EXT4_FMR_OWN_RESV_GDT   FMR_OWNER('f', 2)   // 保留 GDT 块
#define EXT4_FMR_OWN_BLKBM      FMR_OWNER('f', 3)   // 块位图
#define EXT4_FMR_OWN_INOBM      FMR_OWNER('f', 4)   // inode 位图
```

`FMR_OWNER` 宏将两个字符编码为 64 位所有者 ID：
```
FMR_OWNER('f', 1) → 0x00 0x31 0x00 0x00 0x00 0x00 0x00 0x66
                      [--- 大端序填充 ---]  [1]  [--- 'f' ---]
```

这允许工具通过可读字符区分不同类型。

### 7.3 ext4_getfsmap — 主入口

```c
// fsmap.c:703
int ext4_getfsmap(struct super_block *sb,
    struct ext4_fsmap_head *head,
    ext4_fsmap_format_t formatter, void *arg)
```

**流程**：

```
ext4_getfsmap(sb, head, formatter, arg)
  │
  ├─[1] 验证输入:                                      (line 712-716)
  │     fmh_iflags 只允许 FMH_IF_VALID
  │     fmh_keys[0] 和 fmh_keys[1] 必须是有效设备
  │
  ├─[2] 设置设备处理器:                                 (line 720-731)
  │     handlers[0] = {data_dev, ext4_getfsmap_datadev}
  │     if (journal device): handlers[1] = {journal_dev, ext4_getfsmap_logdev}
  │     sort(handlers, ...) 按设备 ID 排序
  │
  ├─[3] 处理 continuation key:                         (line 744-747)
  │     dkeys[0] = head->fmh_keys[0]
  │     dkeys[0].fmr_physical += dkeys[0].fmr_length   // 从上次结束处继续
  │     dkeys[0].fmr_owner = 0
  │     dkeys[0].fmr_length = 0
  │     dkeys[1] = {0xFF, 0xFF, ..., 0xFF}
  │
  ├─[4] 验证 bounds:                                   (line 750)
  │     ext4_getfsmap_check_keys(dkeys, &head->fmh_keys[1])
  │
  ├─[5] 初始化 info 结构:                              (line 753-757)
  │     info.gfi_next_fsblk = keys[0].fmr_physical + keys[0].fmr_length
  │     info.gfi_formatter = formatter
  │     info.gfi_head = head
  │
  └─[6] 遍历每个设备处理器:                             (line 760+)
        for (i = 0; i < EXT4_GETFSMAP_DEVS; i++)
            handlers[i].gfd_fn(sb, dkeys, &info)
            └── ext4_getfsmap_datadev(sb, dkeys, &info)
                └── 迭代块组 + 固定元数据 + mballoc 查询
```

### 7.4 ext4_getfsmap_datadev — 数据设备遍历

```c
// fsmap.c:528
static int ext4_getfsmap_datadev(struct super_block *sb,
    struct ext4_fsmap *keys,
    struct ext4_getfsmap_info *info)
```

这是数据设备上最复杂的逻辑，它需要交错输出固定元数据和空闲/已用区域：

**交叉输出算法**：

```
ext4_getfsmap_datadev()
  │
  ├─[1] 计算组范围:                                    (line 544-559)
  │     start_ag = 起始组号, end_ag = 结束组号
  │
  ├─[2] 搜集固定元数据:                                 (line 573)
  │     ext4_getfsmap_find_fixed_metadata(sb, &meta_list)
  │     对每个组生成: 超级块, GDT, 保留 GDT, 块位图, inode 位图, inode table
  │     存储在有序链表 meta_list 中
  │
  ├─[3] 遍历每个组 (start_ag → end_ag):                (line 578)
  │     │
  │     ├─ ext4_mb_query_range(sb,
  │     │     ext4_getfsmap_meta_helper,   ← 处理固定元数据
  │     │     ext4_getfsmap_datadev_helper, ← 处理空闲/已用区域
  │     │     info)
  │     │
  │     │  mballoc 内部: 位图/buddy 遍历
  │     │  → 对每个 extent 调用 datadev_helper
  │     │
  │     └─ ext4_getfsmap_helper(sb, info, &info->gfi_lastfree)
  │        输出组尾部的空闲区域
  │
  ├─[4] 输出最后的空闲区域:                             (line 620)
  │
  └─[5] 发送终止记录 (dummy EXT4_FMR_OWN_UNKNOWN):     (line 636)
         ext4_getfsmap_helper(sb, info, &termination_rec)
```

**`ext4_mb_query_range`** 是 mballoc 提供的块位图查询接口，它通过 buddy cache 高效遍历每个组的所有 extent（空闲区域和已分配区域），并调用回调函数。

### 7.5 ext4_getfsmap_helper — 格式化输出

```c
// fsmap.c:85
static int ext4_getfsmap_helper(struct super_block *sb,
    struct ext4_getfsmap_info *info,
    struct ext4_fsmap *rec)
```

这是 FSMap 的核心输出函数，负责：

1. **过滤**：丢弃在查询下限之前的记录（line 103）

2. **计数模式**：当 `fmh_count == 0` 时仅计数而不返回实际数据
   ```c
   if (info->gfi_head->fmh_count == 0) {
       if (info->gfi_head->fmh_entries == UINT_MAX)
           return EXT4_QUERY_RANGE_ABORT;
       info->gfi_head->fmh_entries++;
       // ...
   }
   ```

3. **间隙检测**：发现物理块序列中的空洞
   ```c
   // fsmap.c:134
   if (rec_fsblk > info->gfi_next_fsblk) {
       // 发现间隙！输出为 EXT4_FMR_OWN_UNKNOWN
       fmr.fmr_physical = info->gfi_next_fsblk;
       fmr.fmr_owner = EXT4_FMR_OWN_UNKNOWN;
       fmr.fmr_length = rec_fsblk - info->gfi_next_fsblk;
       fmr.fmr_flags = FMR_OF_SPECIAL_OWNER;
       error = info->gfi_formatter(&fmr, info->gfi_format_arg);
   }
   ```

4. **格式化输出**：调用用户提供的 `formatter` 回调将记录复制到用户空间缓冲区

5. **边界检查**：当 `fmh_entries >= fmh_count` 时返回 `EXT4_QUERY_RANGE_ABORT`，避免缓冲区溢出

### 7.6 固定元数据的有序合并

`ext4_getfsmap_find_fixed_metadata`（fsmap.c:470）为每个块组生成一系列固定元数据记录：

```
每个块组的固定元数据 (按物理位置顺序):
  ┌──────────────────┐
  │ SUPERBLOCK        │  ← EXT4_FMR_OWN_FS (仅 group 0 或 sparse_super 组)
  │ GDT               │  ← EXT4_FMR_OWN_GDT
  │ RESV_GDT          │  ← EXT4_FMR_OWN_RESV_GDT
  │ BLOCK BITMAP      │  ← EXT4_FMR_OWN_BLKBM
  │ INODE BITMAP      │  ← EXT4_FMR_OWN_INOBM
  │ INODE TABLE       │  ← EXT4_FMR_OWN_INODES
  └──────────────────┘
```

这些记录被插入到一个链表中，然后排序和合并。merge 阶段（`ext4_getfsmap_merge_fixed_metadata`，fsmap.c:435）将相邻的同类元数据记录合并为更大的 extent。

---

## 8. FSMap 完整查询流程

```
用户空间: ioctl(fd, FS_IOC_GETFSMAP, &head)
              │
              ▼
ext4_ioc_getfsmap()
              │
              ▼
ext4_getfsmap(sb, head, formatter, arg)                      // fsmap.c:703
              │
              ├─ 设备 1: ext4_getfsmap_datadev(sb, keys, info) // fsmap.c:528
              │     │
              │     ├─ ext4_getfsmap_find_fixed_metadata()
              │     │   ┌────────────────────────────────────┐
              │     │   │ meta_list (已排序):                 │
              │     │   │ block 0-0:     SUPERBLOCK (OWN_FS)  │
              │     │   │ block 1-5:     GDT (OWN_GDT)        │
              │     │   │ block 6-10:    RESV_GDT (OWN_RESV)  │
              │     │   │ block 11-11:   BLOCK BITMAP (BLKBM) │
              │     │   │ block 12-12:   INODE BITMAP (INOBM) │
              │     │   │ block 13-100:  INODE TABLE (INODES) │
              │     │   └────────────────────────────────────┘
              │     │
              │     └─ for each group:
              │           ext4_mb_query_range()
              │           ├─ meta_helper: 逐条输出 meta_list 项
              │           └─ datadev_helper:
              │               每次 mballoc 报告一个空闲 extent:
              │               ext4_getfsmap_helper(info, rec)
              │               {
              │                 检测间隙 → OWN_UNKNOWN
              │                 映射 owner → OWN_FREE
              │                 formatter(&fmr, arg)
              │               }
              │
              └─ 设备 2 (可选): ext4_getfsmap_logdev(sb, keys, info)
                    └── 日志设备上的 OWN_LOG 记录
```

**输出示例**（概念性）:

```
PHYSICAL OFFSET    LENGTH  OWNER
0                  1       EXT4_FMR_OWN_FS       (超级块)
1                  5       EXT4_FMR_OWN_GDT      (组描述符)
6                  5       EXT4_FMR_OWN_RESV_GDT (保留 GDT)
11                 1       EXT4_FMR_OWN_BLKBM    (块位图)
12                 1       EXT4_FMR_OWN_INOBM    (inode 位图)
13                 88      EXT4_FMR_OWN_INODES   (inode 表)
101                500     EXT4_FMR_OWN_FREE     (空闲数据块)
601                200     EXT4_FMR_OWN_UNKNOWN  (已分配数据块)
...
```

---

## 9. 扩容安全性设计

### 9.1 Journal 保护

所有扩容操作都在 journal 事务保护下完成。如果扩容过程中系统崩溃：
1. Journal replay 会回滚或完成未完成的事务
2. 超级块的更新是原子性的（通过 JBD2 revoke 机制）
3. 不会出现"部分扩容"导致的文件系统损坏

### 9.2 内存屏障协议

```
写入者(扩容):                       读者(分配器):
  s_blocks_count = new_value           s_groups_count = READ_ONCE(...)
  s_inodes_count = new_value           smp_rmb()
  smp_wmb()                            blocks_count = s_blocks_count  ← 安全
  smp_wmb()                            ...
  s_groups_count = new_value
```

这确保了分配器只有在看到新的 `s_groups_count` 后才会读取新组的元数据——此时元数据已经初始化完成。

### 9.3 Per-CPU 计数器

扩容通过 `percpu_counter_add` 更新空闲块计数器（resize.c:1490-1493），这是无锁的、per-CPU 的原子累加操作。相比直接修改全局计数器，这避免了扩容时的全局锁竞争。

---

## 10. fscrypt 集成总览

ext4 的 fscrypt 是 per-inode 的加密框架，密钥通过 `fscrypt_prepare_new_inode`（ialloc.c:995）从父目录继承。`EXT4_INODE_FSCRYPT_MASK` 标记加密 inode。文件名以密文存储于磁盘，`ext4_fname_prepare`/`ext4_fname_diskcmp` 处理明文↔密文转换。块数据加密发生在 bio 层而非 ext4 内部。关键集成点：inode 创建（ialloc.c:995）、目录加密上下文验证（namei.c:1792）、加密目录项查找（namei.c`ext4_fname_prepare`）。

---

## 11. 扩容完整流程

```
resize2fs ioctl → ext4_resize_fs(sb, n_blocks_count)
  │
  ├─ ext4_resize_begin(sb)                                // resize.c:46
  │   └── CAP_SYS_RESOURCE, 无错误, test_and_set_bit(RESIZING)
  ├─ 计算 add = DIV_ROUND_UP(n_blocks, BLOCKS_PER_GROUP) - s_groups_count
  ├─ for each new flex group:
  │   ├─ ext4_setup_new_descs(handle, sb, flex_gd)        // resize.c:1338
  │   ├─ ext4_update_super(sb, flex_gd)                   // resize.c:1418
  │   │   └── s_blocks_count+=..., smp_wmb(), s_groups_count+=flex_size
  │   └─ ext4_handle_dirty_metadata(s_sbh)
  └─ ext4_resize_end(sb)                                  // resize.c:98
      └── clear_bit(RESIZING), update_backups
```

---

## 12. 总结与性能考量

**扩容性能**：使用 `flex_group_add` 模式，瓶颈在块位图初始化。`EXT4_FLAGS_RESIZING` 位锁保证串行化，正常 I/O 在扩容过程中不受影响。

**FSMap 应用**：磁盘取证（块→文件映射）、碎片分析（空闲 extent 分布）、空间审计（保留 GDT/日志等隐藏空间）、调试（验证分配器输出）。

**限制**：`ext4_group_extend` 只能扩展最后一组，sparse_super2 不支持在线扩容，64-bit 支持最大 ~1 EiB，FSMap 需要 `CAP_SYS_ADMIN`。

---

## 系列结语

六篇文章纵览了 ext4 内核源码的核心子系统：

```
第一篇：超级块与块组 — 磁盘布局:      从宏观层面理解 ext4 的"建筑蓝图"
第二篇：Extent 树 — 逻辑到物理映射:    文件数据块的精确寻址
第三篇：JBD2 日志 — 崩溃一致性:       断电不丢数据的底层保障
第四篇：块与 inode 分配:               多块分配器与 Orlov 算法的空间管理
第五篇：目录与 HTree 索引:             从 O(n) 到 O(log n) 的查找优化
第六篇：在线扩容与 FSMap:              运行时弹性扩展与块取证
```

从超级块的结构体字段到 mballoc 的 buddy cache，从 ext4_lookup 的 HTree 二分查找到 ext4_resize_begin 的内存屏障协议——这些代码共同构成了 Linux 最广泛使用的文件系统的完整图景。

ext4 的设计哲学是**渐进式演化**：在 ext2/ext3 的骨架上，通过特性标志（features bitmap）逐步加入 extent、flex_bg、meta_bg、HTree、journal checksum、metadata_csum 等能力，保持向后兼容的同时拥抱现代存储需求。这 6 篇文章试图揭示的正是这种演化背后的代码实现。

内核源码探索无止境，每个函数调用背后都可能藏着设计者深思熟虑的权衡。希望这系列文章能成为你深入 ext4 源码的导航图。

---

### 延伸阅读

- [Linux Kernel Documentation: filesystems/ext4/](https://www.kernel.org/doc/html/latest/filesystems/ext4/)
- Ted Ts'o, "The ext4 file system" (USENIX ;login:, 2009)
- Mingming Cao et al., "Ext4 block and inode allocator improvements" (OLS 2008)
- `Documentation/filesystems/ext4/*.rst` in the kernel tree
- ext4 邮件列表: linux-ext4@vger.kernel.org
