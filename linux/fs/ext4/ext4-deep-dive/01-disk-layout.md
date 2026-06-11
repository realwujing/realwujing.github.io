# 第一篇：磁盘布局 — 超级块、块组描述符与索引节点

> 源码：`fs/ext4/super.c`, `fs/ext4/ext4.h` | 头文件：`include/linux/ext4.h`

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. ext4 整体磁盘布局

ext4 将整个文件系统划分为固定大小的**块组 (block group)**，每个块组的布局如下：

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Block Group N                                │
│                                                                      │
│ ┌────────┐ ┌────────────┐ ┌──────┐ ┌──────┐ ┌────────┐ ┌──────────┐│
│ │Super   │ │ Group      │ │Block │ │Inode │ │ Inode  │ │ Data     ││
│ │Block   │ │ Descriptor │ │Bitmap│ │Bitmap│ │ Table  │ │ Blocks   ││
│ │(1 blk) │ │ Table (N)  │ │(1+N) │ │(1)   │ │ (M)    │ │ (rest)   ││
│ └────────┘ └────────────┘ └──────┘ └──────┘ └────────┘ └──────────┘│
│                                                                      │
│  仅在 BG 0 和其他 sparse 备份 BG 中存在超级块                          │
└─────────────────────────────────────────────────────────────────────┘
```

- **超级块 (superblock)**: 内核启动时必须读到的元数据，偏移 1024 字节 (1KB)，长度 1024 字节
- **GDT**: 所有块组的描述符，内核在内存中缓存，块组 0 保存完整备份，其他组可选保存（`sparse_super` 策略）
- **块位图 (block bitmap)**: 每个 bit 代表一个块的使用情况，1=已用，0=空闲
- **inode 位图 (inode bitmap)**: 同上，管理 inode 的使用
- **inode 表 (inode table)**: 128/256/512 字节一个 inode 的数组
- **数据块**: 文件数据 + 目录数据 + 符号链接目标 + 扩展属性

### 弹性块组 (flex_bg / flex group)

多个连续的块组可以聚合成一个弹性块组：

```
┌──────────────────────────────────────────────────────────────────┐
│            Flex Group (s_log_groups_per_flex=16)                  │
│                                                                    │
│  BG 0:  [SB+GDT] [block bitmap x16] [inode bitmap x16]            │
│         [inode table x16] [data]                                   │
│  BG 1:  [data]                                                     │
│  ...    [data]                                                     │
│ BG 15:  [SB backup] [GDT backup] [data]                            │
└──────────────────────────────────────────────────────────────────┘
```

`flex_bg` 把多个 BG 的位图和 inode 表集中到第一个 BG，后面的 BG 全部是纯数据区，这对大型元数据操作更友好、减少寻道。

---

## 2. `struct ext4_super_block` — 超级块 (`ext4.h:1329`)

```c
// fs/ext4/ext4.h:1329
struct ext4_super_block {
/*00*/  __le32  s_inodes_count;            // 总 inode 数
        __le32  s_blocks_count_lo;         // 低 32 位总块数
        __le32  s_r_blocks_count_lo;       // 保留块数（仅 root 可分配）
        __le32  s_free_blocks_count_lo;    // 空闲块数
/*10*/  __le32  s_free_inodes_count;       // 空闲 inode 数
        __le32  s_first_data_block;        // 第一个数据块编号（通常 0 或 1）
        __le32  s_log_block_size;          // 块大小 = 2^(10+s_log_block_size)
        __le32  s_log_cluster_size;        // 簇大小（用于 bigalloc）
/*20*/  __le32  s_blocks_per_group;        // 每块组块数（通常 32768）
        __le32  s_clusters_per_group;
        __le32  s_inodes_per_group;        // 每块组 inode 数
        __le32  s_mtime;                   // 最后挂载时间
/*30*/  __le32  s_wtime;                   // 最后写入时间
        __le16  s_mnt_count;               // 挂载次数
        __le16  s_max_mnt_count;           // 强制 fsck 前最大挂载次数
        __le16  s_magic;                   // 0xEF53，ext 家族魔数
        __le16  s_state;                   // 干净/脏状态
        __le16  s_errors;                  // 错误处理策略
        __le16  s_minor_rev_level;
/*40*/  __le32  s_lastcheck;               // 最后一次 fsck 时间
        __le32  s_checkinterval;           // 最大 fsck 间隔
        __le32  s_creator_os;              // 创建该 FS 的 OS
        __le32  s_rev_level;               // 修订级别（good old vs dynamic）
/*50*/  __le16  s_def_resuid;              // 保留块默认 UID
        __le16  s_def_resgid;              // 保留块默认 GID
        __le32  s_first_ino;               // 第一个非保留 inode（通常 11）
        __le16  s_inode_size;              // inode 结构体大小
        __le16  s_block_group_nr;          // 该超级块所在的块组编号
        __le32  s_feature_compat;          // 兼容特性位图
/*60*/  __le32  s_feature_incompat;        // 不兼容特性位图
        __le32  s_feature_ro_compat;       // 只读兼容特性位图
/*68*/  __u8    s_uuid[16];                // 128 位 UUID
/*78*/  char    s_volume_name[16];         // 卷标
/*88*/  char    s_last_mounted[64];        // 最后挂载目录
/*C8*/  __le32  s_algorithm_usage_bitmap;
        __u8    s_prealloc_blocks;         // 预分配块数
        __u8    s_prealloc_dir_blocks;     // 目录预分配块数
        __le16  s_reserved_gdt_blocks;     // 在线扩容预留 GDT 块
/*D0*/  __u8    s_journal_uuid[16];        // 日志 UUID
/*E0*/  __le32  s_journal_inum;            // 日志 inode 编号（内部日志）
        __le32  s_journal_dev;             // 日志设备号（外部日志）
        __le32  s_last_orphan;             // 孤儿 inode 链表头
        __le32  s_hash_seed[4];            // HTree 哈希种子
        __u8    s_def_hash_version;        // 默认哈希版本
        __u8    s_jnl_backup_type;
        __le16  s_desc_size;               // 块组描述符大小（通常 64）
/*100*/ __le32  s_default_mount_opts;
        __le32  s_first_meta_bg;           // 第一个元数据块组
        __le32  s_mkfs_time;               // 创建时间
        __le32  s_jnl_blocks[17];          // 日志 inode 备份块列表
        // ... 64bit support, s_*_hi fields ...
        // ... MMP, RAID, encryption, checksum ...
        // ... s_error_count, s_first_error_*, s_last_error_* ...
};
```

### 魔数与块大小

`s_magic = 0xEF53` 是 ext2/3/4 的标识符，位于超级块的偏移 `0x38` 位置。用 `dumpe2fs` 或 `hexdump -C -s 1024 -n 64` 可以观察到。

块大小由 `s_log_block_size` 计算：

```
block_size = 1024 << s_log_block_size
```

例如 `s_log_block_size = 0` → 1024 字节，`= 2` → 4096 字节。绝大多数现代 ext4 文件系统使用 4096 字节块。

---

## 3. 特性标志位

特性标志位分三类，位于超级块偏移 `0x5C` 起：

### s_feature_compat（兼容特性，`ext4.h:1371`）

| 标志位 | 宏定义 | 含义 |
|--------|--------|------|
| 0x0001 | `EXT4_FEATURE_COMPAT_DIR_PREALLOC` | 目录预分配 |
| 0x0002 | `EXT4_FEATURE_COMPAT_IMAGIC_INODES` | imagic inode |
| 0x0004 | `EXT4_FEATURE_COMPAT_HAS_JOURNAL` | 有日志 |
| 0x0008 | `EXT4_FEATURE_COMPAT_EXT_ATTR` | 扩展属性 |
| 0x0010 | `EXT4_FEATURE_COMPAT_RESIZE_INODE` | 保留 GDT 块 |
| 0x0020 | `EXT4_FEATURE_COMPAT_DIR_INDEX` | 目录索引 (HTree) |
| 0x0040 | `EXT4_FEATURE_COMPAT_LAZY_BG` | 惰性块组 |
| 0x0080 | `EXT4_FEATURE_COMPAT_EXCLUDE_INODE` | 快照排除 inode |
| 0x0100 | `EXT4_FEATURE_COMPAT_EXCLUDE_BITMAP` | 快照排除位图 |
| 0x0200 | `EXT4_FEATURE_COMPAT_SPARSE_SUPER2` | sparse super v2 |

**兼容特性**即使内核不认识，也应该安全挂载（只是可能不能使用该特性）。

### s_feature_incompat（不兼容特性，`ext4.h:1372`）

| 标志位 | 宏定义 | 含义 |
|--------|--------|------|
| 0x0002 | `EXT4_FEATURE_INCOMPAT_FILETYPE` | 目录项记录文件类型 |
| 0x0004 | `EXT4_FEATURE_INCOMPAT_RECOVER` | 需要恢复 |
| 0x0008 | `EXT4_FEATURE_INCOMPAT_JOURNAL_DEV` | 日志设备 |
| 0x0010 | `EXT4_FEATURE_INCOMPAT_META_BG` | 元数据块组 |
| 0x0040 | `EXT4_FEATURE_INCOMPAT_EXTENTS` | extent 树 |
| 0x0080 | `EXT4_FEATURE_INCOMPAT_64BIT` | 64 位块号 |
| 0x0100 | `EXT4_FEATURE_INCOMPAT_MMP` | 多挂载保护 |
| 0x0200 | `EXT4_FEATURE_INCOMPAT_FLEX_BG` | 弹性块组 |
| 0x0400 | `EXT4_FEATURE_INCOMPAT_EA_INODE` | 大扩展属性存为 inode |
| 0x1000 | `EXT4_FEATURE_INCOMPAT_DIRDATA` | 目录数据校验 |
| 0x2000 | `EXT4_FEATURE_INCOMPAT_CSUM_SEED` | CSUM_SEED |
| 0x4000 | `EXT4_FEATURE_INCOMPAT_LARGEDIR` | 大目录 (>2GB) |
| 0x8000 | `EXT4_FEATURE_INCOMPAT_INLINE_DATA` | 内联数据 |
| 0x10000 | `EXT4_FEATURE_INCOMPAT_ENCRYPT` | 文件级加密 (fscrypt) |

**不兼容特性**：如果内核不认识某个位，必须拒绝挂载。

### s_feature_ro_compat（只读兼容特性，`ext4.h:1373`）

| 标志位 | 宏定义 | 含义 |
|--------|--------|------|
| 0x0001 | `EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER` | 稀疏超级块 |
| 0x0002 | `EXT4_FEATURE_RO_COMPAT_LARGE_FILE` | >2GB 文件 |
| 0x0008 | `EXT4_FEATURE_RO_COMPAT_BTREE_DIR` | HV1 格式目录 |
| 0x0010 | `EXT4_FEATURE_RO_COMPAT_HUGE_FILE` | huge file |
| 0x0020 | `EXT4_FEATURE_RO_COMPAT_GDT_CSUM` | GDT 校验和 |
| 0x0040 | `EXT4_FEATURE_RO_COMPAT_DIR_NLINK` | 子目录数 > 65000 |
| 0x0080 | `EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE` | 大 inode |
| 0x0100 | `EXT4_FEATURE_RO_COMPAT_QUOTA` | quota |
| 0x0200 | `EXT4_FEATURE_RO_COMPAT_BIGALLOC` | bigalloc 簇 |
| 0x0400 | `EXT4_FEATURE_RO_COMPAT_METADATA_CSUM` | 元数据校验 |
| 0x0800 | `EXT4_FEATURE_RO_COMPAT_READONLY` | 只读 |
| 0x1000 | `EXT4_FEATURE_RO_COMPAT_PROJECT` | 项目 ID |
| 0x2000 | `EXT4_FEATURE_RO_COMPAT_VERITY` | fs-verity |
| 0x4000 | `EXT4_FEATURE_RO_COMPAT_ORPHAN_FILE` | 孤儿文件 |

**只读兼容特性**：如果不认识，内核应该以只读方式挂载。

---

## 4. `struct ext4_group_desc` — 块组描述符 (`ext4.h:402`)

```c
// fs/ext4/ext4.h:402
struct ext4_group_desc {
    __le32  bg_block_bitmap_lo;         // 块位图块号 低32位
    __le32  bg_inode_bitmap_lo;         // inode 位图块号 低32位
    __le32  bg_inode_table_lo;          // inode 表起始块号 低32位
    __le16  bg_free_blocks_count_lo;    // 空闲块计数 低16位
    __le16  bg_free_inodes_count_lo;    // 空闲 inode 计数 低16位
    __le16  bg_used_dirs_count_lo;      // 已用目录数 低16位
    __le16  bg_flags;                   // 标志位
    __le32  bg_exclude_bitmap_lo;       // 快照排除位图
    __le16  bg_block_bitmap_csum_lo;    // 块位图校验和 低16位
    __le16  bg_inode_bitmap_csum_lo;    // inode 位图校验和 低16位
    __le16  bg_itable_unused_lo;        // inode 表中未使用的 inode 数
    __le16  bg_checksum;                // 描述符校验和
    // --- 64bit 支持字段 ---
    __le32  bg_block_bitmap_hi;         // 块位图块号 高32位
    __le32  bg_inode_bitmap_hi;         // inode 位图块号 高32位
    __le32  bg_inode_table_hi;          // inode 表起始块号 高32位
    __le16  bg_free_blocks_count_hi;    // 空闲块计数 高16位
    __le16  bg_free_inodes_count_hi;    // 空闲 inode 计数 高16位
    __le16  bg_used_dirs_count_hi;      // 已用目录数 高16位
    __le16  bg_itable_unused_hi;        // 未使用 inode 数 高16位
    __le32  bg_exclude_bitmap_hi;
    __le16  bg_block_bitmap_csum_hi;
    __le16  bg_inode_bitmap_csum_hi;
    __u32   bg_reserved;
};
```

每个描述符通常为 64 字节（由 `s_desc_size` 决定，`ext4.h:1395`）。

### bg_flags 重要标志位

```c
// fs/ext4/ext4.h:446
#define EXT4_BG_INODE_UNINIT   0x0001  // inode 表/位图未初始化
#define EXT4_BG_BLOCK_UNINIT   0x0002  // 块位图未初始化
// EXT4_BG_ZEROED is referenced in bg_flags context; often used with
// lazy_bg to skip zeroing
```

- **INODE_UNINIT**: 此块组的 inode 表和位图尚未初始化。这使得 `mkfs` 可以快速创建大文件系统，位图在实际使用时才分配。
- **BLOCK_UNINIT**: 块位图未初始化，原理同上。

### 64 位扩展

在启用 `EXT4_FEATURE_INCOMPAT_64BIT` 时，块号和计数器的高位部分存储在 `_hi` 字段中。例如完整的块位图块号为：

```c
ext4_fsblk_t bitmap_blk = bg_block_bitmap_lo | ((ext4_fsblk_t)bg_block_bitmap_hi << 32);
```

---

## 5. `struct ext4_inode` — 磁盘索引节点 (`ext4.h:794`)

```c
// fs/ext4/ext4.h:794
struct ext4_inode {
    __le16  i_mode;                     // 文件模式（权限+类型）
    __le16  i_uid;                      // 所有者 UID 低 16 位
    __le32  i_size_lo;                  // 文件大小低 32 位
    __le32  i_atime;                    // 最后访问时间
    __le32  i_ctime;                    // 最后 inode 变更时间
    __le32  i_mtime;                    // 最后修改时间
    __le32  i_dtime;                    // 删除时间
    __le16  i_gid;                      // 组 GID 低 16 位
    __le16  i_links_count;              // 硬链接计数
    __le32  i_blocks_lo;               // 占用块数（512 字节扇区计）
    __le32  i_flags;                    // ext4 特有标志位
    union {
        struct { __le32 l_i_version; } linux1;
        struct { __u32  h_i_translator; } hurd1;
        struct { __u32  m_i_reserved1; } masix1;
    } osd1;
    __le32  i_block[EXT4_N_BLOCKS];    // 60 字节：extent 或间接块指针
    __le32  i_generation;               // 文件版本号（NFS 用）
    __le32  i_file_acl_lo;              // 扩展属性块 低 32 位
    __le32  i_size_high;                // 文件大小高 32 位
    __le32  i_obso_faddr;               // 已废弃碎片地址
    union {
        struct {
            __le16  l_i_blocks_high;
            __le16  l_i_file_acl_high;
            __le16  l_i_uid_high;
            __le16  l_i_gid_high;
            __le16  l_i_checksum_lo;
            __le16  l_i_reserved;
        } linux2;
        // ...
    } osd2;
    __le16  i_extra_isize;              // 额外 inode 字段大小
    __le16  i_checksum_hi;
    __le32  i_ctime_extra;              // ctime ns(低30位) + epoch(高2位)
    __le32  i_mtime_extra;
    __le32  i_atime_extra;
    __le32  i_crtime;                   // 创建时间
    __le32  i_crtime_extra;             // 创建时间 ns + epoch
    __le32  i_version_hi;              // 64 位版本号高位
    __le32  i_projid;                   // 项目 ID
};
```

### i_block — 数据块映射的核心

`i_block[EXT4_N_BLOCKS]` 即 `i_block[15]` 共 60 字节，是 ext4 最精妙的设计：

```
当 EXT4_EXTENTS_FL 设置时（extent 树）:
┌──────────────────────────────────────────────────────────┐
│  i_block[0..2]   ext4_extent_header (eh_magic=0xf30a)   │
│  i_block[3..5]   第 1 个 ext4_extent (ee_block, ee_len, │
│                    ee_start_lo/hi)                       │
│  i_block[6..8]   第 2 个 ext4_extent                     │
│  i_block[9..11]  第 3 个 ext4_extent                     │
│  i_block[12..14] 第 4 个 ext4_extent                     │
└──────────────────────────────────────────────────────────┘
共 1 header(12B) + 4 extents(12B×4=48B) = 60 字节恰好填满

当 EXT4_EXTENTS_FL 未设置时（间接块映射，兼容 ext2/3）:
  i_block[0..11]  直接指针（指向数据块的物理块号）
  i_block[12]     间接指针（指向一个存满块号的块）
  i_block[13]     双重间接指针
  i_block[14]     三重间接指针
```

extent 模式下，不足 5 个 extents 直接存储在 inode 中；超过 4 个 extent 时，header 的 `eh_depth > 0` 指向外部索引块，形成树结构。详见第二篇。

### i_flags 重要标志位

```c
// fs/ext4/ext4.h:503
#define EXT4_EXTENTS_FL         0x00080000  // extent 树模式
// fs/ext4/ext4.h:510
#define EXT4_INLINE_DATA_FL     0x10000000  // 数据存储在 inode 内部
```

其他常见标志：
- `EXT4_SECRM_FL` (0x00000001): 安全删除
- `EXT4_SYNC_FL` (0x00000008): 同步写入
- `EXT4_IMMUTABLE_FL` (0x00000010): 不可变
- `EXT4_APPEND_FL` (0x00000020): 仅追加
- `EXT4_NODUMP_FL` (0x00000040): 不 dump
- `EXT4_NOATIME_FL` (0x00000080): 不更新 atime
- `EXT4_JOURNAL_DATA_FL` (0x00004000): 数据日志模式
- `EXT4_ENCRYPT_FL` (0x00000800): 加密

### 纳秒时间戳

ext4 通过 `*_extra` 字段将时间精度从秒扩展到纳秒：

```c
// 时间戳 = seconds + (extra >> 2) 纳秒
// epoch = extra & 0x3 （每 2 位加 140 年）
//
// fs/ext4/ext4.h:855
#define EXT4_EPOCH_BITS  2
#define EXT4_EPOCH_MASK  ((1 << EXT4_EPOCH_BITS) - 1)
#define EXT4_NSEC_MASK   (~0UL << EXT4_EPOCH_BITS)
```

`i_ctime_extra` 的低 2 位是 epoch，高 30 位是纳秒。这个设计使得 34 位秒值可以覆盖远超 2038 的时间范围。

### i_extra_isize

`i_extra_isize` (`ext4.h:844`) 指定 inode 中超出标准 ext4_inode (128 字节) 部分的额外字段大小。这使得 ext4 能在不破坏兼容性的情况下扩展 inode 结构。典型的 inode 大小为 256 字节。

---

## 6. `ext4_fill_super` — 挂载流程 (`super.c:5804`)

```c
// fs/ext4/super.c:5804
static int ext4_fill_super(struct super_block *sb, struct fs_context *fc)
{
    struct ext4_fs_context *ctx = fc->fs_private;
    struct ext4_sb_info *sbi;
    int ret;

    sbi = ext4_alloc_sbi(sb);          // 分配 ext4_sb_info
    if (!sbi)
        return -ENOMEM;

    fc->s_fs_info = sbi;

    strreplace(sb->s_id, '/', '!');     // 清理块设备名中的 '/'

    sbi->s_sb_block = 1;                // 默认超级块位置（块号 1）
    if (ctx->spec & EXT4_SPEC_s_sb_block)
        sbi->s_sb_block = ctx->s_sb_block;

    ret = __ext4_fill_super(fc, sb);    // 实际挂载逻辑
    if (ret < 0)
        goto free_sbi;

    // 打印挂载信息：device UUID, rw/ro, journal mode, quota mode
    // ...
    return 0;

free_sbi:
    ext4_free_sbi(sbi);
    fc->s_fs_info = NULL;
    return ret;
}
```

挂载由 `get_tree_bdev(fc, ext4_fill_super)` (`super.c:5856`) 触发，实际工作在 `__ext4_fill_super` (`super.c:5334`) 中完成。

### `__ext4_fill_super` 核心步骤

```
ext4_fill_super()
  │
  ├─ ext4_alloc_sbi(sb)                   — 分配 ext4_sb_info 私有结构
  ├─ __ext4_fill_super(fc, sb)            — 核心逻辑
  │    │
  │    ├─ 读取/验证超级块                  — sb_bread(sb, sbi->s_sb_block)
  │    │    ├─ 检查魔数 0xEF53
  │    │    ├─ 检查 s_log_block_size 合法
  │    │    ├─ 检查 s_desc_size >= 偏移
  │    │    └─ 检查 feature incompat 位
  │    │
  │    ├─ 计算块组数量和相关偏移
  │    │    ├─ blocks_count = s_blocks_count_lo + (hi << 32)
  │    │    ├─ 检验 blocks_count 对齐
  │    │    └─ num_bg = blocks_count / blocks_per_group
  │    │
  │    ├─ 读取 GDT — ext4_check_descriptors()
  │    │    ├─ 验证每个块组的位图/表块号在范围内
  │    │    └─ 对 flex_bg 做边界检查
  │    │
  │    ├─ 解析挂载选项 — parse_options(fc, sb)
  │    │    覆盖: data=ordered/writeback/journal, errors=,
  │    │    commit=, bsddf/minixdf, stripe=, etc.
  │    │
  │    ├─ 加载日志 — ext4_load_journal(sb, es, devnum)
  │    │    └─ 详见第三篇
  │    │
  │    ├─ 初始化 percpu 计数器 — percpu_counter_init()
  │    ├─ 初始化 mballoc — ext4_mb_init(sb)
  │    ├─ 初始化 inode 表 — ext4_init_inode_table()
  │    ├─ 注册 sysfs — ext4_register_sysfs(sb)
  │    │
  │    └─ sb->s_op = &ext4_sops (super.c:5462)
  │
  ├─ 打印挂载成功信息（设备 UUID、挂载模式、journal 模式）
  └─ ext4_update_overhead(sb, false)       — 更新元数据开销统计
```

---

## 7. `ext4_put_super` — 卸载流程 (`super.c:1282`)

```c
// fs/ext4/super.c:1282
static void ext4_put_super(struct super_block *sb)
{
    struct ext4_sb_info *sbi = EXT4_SB(sb);
    int aborted = 0;

    // 1. 先注销 sysfs，防止日志销毁后访问 j_task
    ext4_unregister_sysfs(sb);

    // 2. 打印卸载信息
    ext4_msg(sb, KERN_INFO, "unmounting filesystem %pU.", &sb->s_uuid);

    // 3. 取消惰性 inode 表初始化请求
    ext4_unregister_li_request(sb);

    // 4. 关闭配额
    ext4_quotas_off(sb, EXT4_MAXQUOTAS);

    // 5. 销毁转换工作队列
    destroy_workqueue(sbi->rsv_conversion_wq);

    // 6. 清空孤儿 inode 链表
    ext4_release_orphan_info(sb);

    // 7. 销毁日志 (ext4_journal_destroy -> jbd2_journal_destroy)
    if (sbi->s_journal) {
        aborted = is_journal_aborted(sbi->s_journal);
        err = ext4_journal_destroy(sbi, sbi->s_journal);
        if ((err < 0) && !aborted)
            ext4_abort(sb, -err, "Couldn't clean up the journal");
    }

    // 8. 释放各种内存结构 ...
    ext4_xattr_destroy_cache(sbi->s_ea_block_cache);
    ext4_mb_release(sb);
    ext4_flex_groups_free(sbi);
    ext4_es_unregister_shrinker(sbi);
    percpu_counter_destroy_many(sbi->s_counters);
    ext4_free_sbi(sbi);
}
```

顺序很重要：**先销毁 sysfs**（防止外部访问正在释放的结构），再销毁日志（因为日志可能在 sysfs 中被引用），最后释放内存。

---

## 8. `ext4_sops` — 超级块操作表 (`super.c:1659`)

```c
// fs/ext4/super.c:1659
static const struct super_operations ext4_sops = {
    .alloc_inode    = ext4_alloc_inode,      // 分配 ext4_inode_info
    .free_inode     = ext4_free_in_core_inode, // 释放 inode 缓存
    .destroy_inode  = ext4_destroy_inode,    // 销毁 inode
    .write_inode    = ext4_write_inode,      // 写回 inode
    .dirty_inode    = ext4_dirty_inode,      // 标记 inode 脏
    .drop_inode     = ext4_drop_inode,       // 判断是否可删除 inode
    .evict_inode    = ext4_evict_inode,      // 删除/截断 inode
    .put_super      = ext4_put_super,        // 卸载 ↑
    .sync_fs        = ext4_sync_fs,          // sync 文件系统
    .freeze_fs      = ext4_freeze,           // 冻结
    .unfreeze_fs    = ext4_unfreeze,         // 解冻
    .statfs         = ext4_statfs,           // df 信息
    .show_options   = ext4_show_options,     // /proc/mounts 选项
    .shutdown       = ext4_shutdown,         // 紧急关闭
    // CONFIG_QUOTA:
    .quota_read     = ext4_quota_read,
    .quota_write    = ext4_quota_write,
    .get_dquots     = ext4_get_dquots,
};
```

VFS 通过 `sb->s_op` 指向这个表，所有上层操作（如 `sync()`、`statfs()`、`umount`）都通过该表多态地调用 ext4 的实现。

---

## 9. 校验和机制

现代 ext4（`metadata_csum` 特性）对几乎所有元数据结构加入了 CRC32c 校验：

| 结构 | 校验和字段 | 计算方式 |
|------|-----------|----------|
| 超级块 | `s_checksum` | crc32c(UUID + superblock_tail) |
| 块组描述符 | `bg_checksum` | crc16(sb_uuid + group + desc) |
| inode | `l_i_checksum_lo` + `i_checksum_hi` | crc32c(uuid + inum + inode) |
| extent 节点 | `et_checksum` | crc32c(uuid + inum + extent_block) |
| 位图 | `bg_*_bitmap_csum_lo/hi` | crc32c(uuid + grp_num + bitmap) |
| 目录块 | 内嵌尾校验 | crc32c(...) |

这意味着每个元数据 I/O 都可以在读取时验证完整性。

---

## 10. 实践：用 dumpe2fs 查看磁盘布局

```bash
# 查看超级块信息
sudo dumpe2fs -h /dev/sda1

# 查看特定块组
sudo dumpe2fs /dev/sda1 | grep -A15 "Group 0:"

# 用 debugfs 查看 inode
sudo debugfs -R "stat <8>" /dev/sda1
```

`dumpe2fs` 输出的关键字段：

```
Filesystem features:      has_journal ext_attr dir_index filetype
                          needs_recovery extent 64bit flex_bg
                          sparse_super large_file huge_file dir_nlink
                          extra_isize metadata_csum
Block size:               4096
Inode size:               256
Blocks per group:         32768
Inodes per group:         8192
```

---

## 下一篇文章

[第二篇：范围树 — 二叉搜索、插入分裂与合并](./02-extent-tree.md)
