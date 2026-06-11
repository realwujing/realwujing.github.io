# 第四篇：块与索引节点分配 — 多块分配器与 Orlov 算法

> 源码：fs/ext4/mballoc.c (7283 lines) · fs/ext4/ialloc.c (1626 lines) · fs/ext4/balloc.c (1003 lines)
> 头文件：fs/ext4/ext4.h

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. 概述

ext4 的块分配是文件系统性能的核心。与 ext2/ext3 每次只分配单个块的简单策略不同，ext4 引入了 **多块分配器（multiblock allocator, mballoc）**，一次调用尽可能分配连续的多块，大幅减少碎片和元数据开销。

索引节点（inode）的分配则使用 **Orlov 算法**，这是一种专为类 Unix 文件系统设计的目录分散算法，能将顶层目录均匀散布到各个块组，避免局部过热。

本篇将深入分析以下核心函数的实现：

| 函数 | 文件:行号 | 功能 |
|------|-----------|------|
| `ext4_mb_new_blocks` | mballoc.c:6229 | 多块分配主入口 |
| `ext4_mb_mark_bb` | mballoc.c:4259 | 标记块位图 |
| `ext4_get_group_number` | balloc.c:36 | 物理块→块组号 |
| `ext4_get_group_no_and_offset` | balloc.c:54 | 物理块→(组号, 组内偏移) |
| `__ext4_new_inode` | ialloc.c:931 | inode 分配主入口 |
| `find_group_orlov` | ialloc.c:422 | Orlov 目录分配算法 |
| `find_group_other` | ialloc.c:570 | 普通文件组选择 |
| `ext4_free_inode` | ialloc.c:235 | inode 释放 |

---

## 2. 分配请求结构体：ext4_allocation_request

整个块分配流程的输入由 `struct ext4_allocation_request`（ext4.h:210）描述：

```c
// ext4.h:210
struct ext4_allocation_request {
    struct inode *inode;    // 目标 inode
    unsigned int len;       // 需要分配的块数
    ext4_lblk_t logical;    // 目标逻辑块号
    ext4_lblk_t lleft;      // 左边最近已分配的逻辑块
    ext4_lblk_t lright;     // 右边最近已分配的逻辑块
    ext4_fsblk_t goal;      // 物理块目标（提示）
    ext4_fsblk_t pleft;     // 左边最近已分配对应的物理块
    ext4_fsblk_t pright;    // 右边最近已分配对应的物理块
    unsigned int flag;      // 标志位：EXT4_MB_HINT_*
};
```

**关键字段解析：**

| 字段 | 作用 |
|------|------|
| `inode` | 指明是哪个文件的分配请求 |
| `len` | 希望分配的块数，分配器会先"规范化"（放大） |
| `logical` | 文件内的逻辑偏移，用于维持逻辑→物理的顺序 |
| `goal` | 物理块提示——分配器优先在此附近搜索 |
| `lleft` / `lright` | 左右邻居逻辑块号，用于选择连续的物理位置 |
| `pleft` / `pright` | 左右邻居对应的物理块，辅助就近分配 |
| `flag` | 控制预分配/保留等行为 |

提示信息构成了分配的"软约束"：分配器会优先满足这些约束，但不强制。当无法满足时会回退到更宽松的策略。

---

## 3. 多块分配器三阶段策略

mballoc 的设计文档位于 mballoc.c 的注释区（第43–120行），是了解整个分配流程的最佳入口。核心思想是 **三级回退策略**：

```
分配请求
  │
  ├─[阶段1] 检查 inode 预分配空间 ──▶ 命中 ──▶ 直接使用
  │
  ├─[阶段2] 检查 per-CPU locality group 预分配 ──▶ 命中 ──▶ 直接使用
  │
  └─[阶段3] buddy cache 搜索 ──▶ 找到连续块 ──▶ 更新位图并返回
```

### 3.1 Inode 预分配空间

每个 inode 维护一个预分配空间列表 `i_prealloc_list`，存储在 `ext4_inode_info` 中的红黑树上。预分配空间的属性（mballoc.c:65-68）：

```
pa_lstart → 预分配的逻辑起始块
pa_pstart → 预分配的物理起始块
pa_len    → 预分配长度（以簇为单位）
pa_free   → 预分配空间中的空闲簇数
```

**关键机制**：inode 预分配基于**逻辑块号**匹配。如果请求的逻辑块号落在预分配范围内，就直接消费该预分配空间。这保证了同一文件的物理块在磁盘上连续——这是保持顺序读性能的关键。

预分配空间的分配在 `ext4_mb_normalize_request` 中完成，它会根据文件大小启发式地决定预分配大小：
- 小文件 → 较少的预分配
- 大文件 → 较大的预分配（最多 `s_mb_large_req`，默认 256MB）

inode 预分配的红黑树遍历在 `ext4_mb_pa_rb_next_iter`（mballoc.c:4332）中实现，按照逻辑块号顺序进行二分查找。

### 3.2 Per-CPU Locality Group 预分配

当 inode 预分配未命中时，分配器检查 per-CPU 的 **locality group** 预分配：

```c
ext4_sb_info.s_locality_groups[smp_processor_id()]
```

每个 CPU 拥有独立的预分配列表。**目的**：
- 减少 CPU 之间的锁竞争
- 将同一 CPU 上的分配请求聚合到相同区域

locality group 预分配基于**空闲空间**匹配（而非逻辑块号），只要有足够的空闲簇即可使用。

### 3.3 Buddy Cache —— 位图伙伴系统

这是 mballoc 的核心数据结构，也是最后一道防线。如果两级预分配都未命中，分配器就进入 buddy cache 进行实际搜索。

#### Buddy Cache 布局

Buddy cache 是一个**纯内存的虚拟 inode** `s_buddy_cache`，它永远不写入磁盘，卸载时丢弃（mballoc.c:110-111）。它在 mballoc.c:3518 创建：

```c
sbi->s_buddy_cache = new_inode(sb);
sbi->s_buddy_cache->i_ino = EXT4_BAD_INO;
EXT4_I(sbi->s_buddy_cache)->i_disksize = 0;
```

每个块组占用 **2 个块**：一个块存储位图（bitmap），一个块存储 buddy 信息。这些块通过 buddy cache inode 的页缓存访问。

```
┌──────────────────────────────────────────────────────┐
│                   单个 folio 布局                      │
│                                                      │
│  ┌──────────────┬──────────────┬──────────────┬────  │
│  │  group 0     │  group 0     │  group 1     │      │
│  │  bitmap      │  buddy       │  bitmap      │ ...  │
│  │  (1 block)   │  (1 block)   │  (1 block)   │      │
│  └──────────────┴──────────────┴──────────────┴────  │
│                                                      │
│  groups_per_folio = folio_size / blocksize / 2       │
└──────────────────────────────────────────────────────┘
```

**Buddy 的工作原理**：

每个块组的 buddy 页维护一个 **2阶幂的位图**，类似伙伴分配器（buddy allocator）。它将组内空闲块按照 2^n 大小组织：
- 第 0 阶：单个块
- 第 1 阶：连续 2 个块
- 第 2 阶：连续 4 个块
- ...直到 2^order 级别

这允许 O(log n) 时间找到指定大小的连续空闲块。

加载 buddy 信息通过 `ext4_mb_load_buddy` 完成，它会从磁盘读取块位图并在内存中构建 buddy 结构。

### 3.4 小文件的 Group Preallocation

默认行为（mballoc.c:53）：当文件大小 < `s_mb_stream_request`（默认 16 块，即 64KB with 4KB blocks）时，使用 **group preallocation** 而非 inode preallocation。

**动机**：将小文件聚合在磁盘的同一区域，减少寻道。这个阀值可通过 sysfs 调整：
```
echo 32 > /sys/fs/ext4/<dev>/mb_stream_req
```

---

## 4. 主分配函数：ext4_mb_new_blocks

`ext4_mb_new_blocks`（mballoc.c:6229）是整个块分配流程的入口，所有文件数据写入最终都汇聚到这里。

```c
// mballoc.c:6229
ext4_fsblk_t ext4_mb_new_blocks(handle_t *handle,
                struct ext4_allocation_request *ar, int *errp)
```

**完整流程**：

```
ext4_mb_new_blocks(ar)
  │
  ├─[fast commit replay?] → ext4_mb_new_blocks_simple()   (line 6247)
  ├─[quota file?] → ar->flags |= EXT4_MB_USE_ROOT_BLOCKS  (line 6250)
  │
  ├─[delalloc reserved?]
  │   NO → claim free clusters + quota                    (line 6253-6288)
  │        如果不够，ar->len 减半重试
  │        如果 quota 不够，逐块递减 ar->len
  │
  ├─alloc ac = kmem_cache_zalloc(ext4_ac_cachep)          (line 6290)
  ├─ext4_mb_initialize_context(ac, ar)                    (line 6297)
  │
  ├─ext4_mb_use_preallocated(ac)                          (line 6301)
  │   YES → 直接使用预分配空间
  │   NO  → ext4_mb_normalize_request(ac)                 (line 6303)
  │          ext4_mb_pa_alloc(ac)                         (line 6305)
  │          ext4_mb_regular_allocator(ac) ←── buddy 搜索  (line 6310)
  │          [失败→释放 PA, 释放已分配块, goto errout]    (line 6318-6321)
  │
  └─ac_status == AC_STATUS_FOUND?
      YES → ext4_mb_mark_diskspace_used(ac, handle)      (line 6328)
            实际更新磁盘块位图
```

**关键细节**：

1. **规范化请求**（line 6303）：`ext4_mb_normalize_request` 将用户请求的块数放大，以便将多余的块加入预分配列表。例如用户请求 4 块，规范化后可能请求 64 块，分配成功后 60 块留给后续使用。

2. **重试机制**：当 free clusters 不足时，分配器将 `ar->len` 减半重试（line 6263），这是一种优雅的降级策略——宁可分配少一些，也不返回 ENOSPC。

3. **预分配优先**（line 6301）：如果 `ext4_mb_use_preallocated` 成功，整个 buddy cache 搜索路径完全跳过，这是热路径上的关键优化。

---

## 5. 块标记函数：ext4_mb_mark_bb

`ext4_mb_mark_bb`（mballoc.c:4259）负责标记块位图中的块为"已用"或"空闲"：

```c
// mballoc.c:4259
void ext4_mb_mark_bb(struct super_block *sb, ext4_fsblk_t block,
                     int len, bool state)
```

**幂等性设计**：这个函数在 fast commit 重放期间可以被安全地重复调用——即使同一块被标记多次也不会出错。关键实现：

```c
// mballoc.c:4268-4279
while (len > 0) {
    ext4_get_group_no_and_offset(sb, block, &group, &blkoff);

    // 处理跨块组边界的情况
    thisgrp_len = min(len,
        EXT4_BLOCKS_PER_GROUP(sb) - EXT4_C2B(sbi, blkoff));
    clen = EXT4_NUM_B2C(sbi, thisgrp_len);

    // 验证块不在 system zone 中
    if (!ext4_sb_block_valid(sb, NULL, block, thisgrp_len)) {
        ext4_error(...);
        break;
    }

    err = ext4_mb_mark_context(NULL, sb, state, group, blkoff, clen, ...);
    block += thisgrp_len;
    len -= thisgrp_len;
}
```

**跨组处理**：使用 while 循环逐组处理，每次只标记当前组内的块，然后推进偏移。

**System zone 检查**（line 4282）：超级块、GDT、位图等系统元数据块不可标记，`ext4_sb_block_valid` 通过红黑树 `ext4_system_blocks` 进行 O(log n) 的合法性检查。

在 `ext4_mb_new_blocks` 成功后也会调用 `ext4_mb_mark_bb`（mballoc.c:6217）进行分配提交：
```c
ext4_mb_mark_bb(sb, block, 1, true);  // state=true 表示标记为已用
```

---

## 6. 块组定位函数

### 6.1 ext4_get_group_number (balloc.c:36)

```c
// balloc.c:36
ext4_group_t ext4_get_group_number(struct super_block *sb,
                                   ext4_fsblk_t block)
{
    if (test_opt2(sb, STD_GROUP_SIZE))
        group = (block - le32_to_cpu(s_first_data_block)) >>
                (EXT4_BLOCK_SIZE_BITS(sb) + EXT4_CLUSTER_BITS(sb) + 3);
    else
        ext4_get_group_no_and_offset(sb, block, &group, NULL);
    return group;
}
```

`STD_GROUP_SIZE` 是固定 32768 块的组大小（128MB with 4KB blocks），此时可以用移位快速计算。否则调用通用版本。

### 6.2 ext4_get_group_no_and_offset (balloc.c:54)

```c
// balloc.c:54
void ext4_get_group_no_and_offset(struct super_block *sb,
        ext4_fsblk_t blocknr,
        ext4_group_t *blockgrpp, ext4_grpblk_t *offsetp)
{
    blocknr = blocknr - le32_to_cpu(es->s_first_data_block);
    offset = do_div(blocknr, EXT4_BLOCKS_PER_GROUP(sb)) >>
             EXT4_SB(sb)->s_cluster_bits;
    if (offsetp) *offsetp = offset;
    if (blockgrpp) *blockgrpp = blocknr;
}
```

`do_div` 是一个宏，对 64 位数做除法，同时返回余数作为组内偏移。`blocknr` 既是输入（被除数）也是输出（商=组号）。偏移再除以 `s_cluster_bits` 得到簇偏移。

---

## 7. 索引节点分配：__ext4_new_inode

`__ext4_new_inode`（ialloc.c:931）是 inode 分配的唯一入口，被 `ext4_new_inode_start_handle` 等上层包装函数调用。

```c
// ialloc.c:931
struct inode *__ext4_new_inode(struct mnt_idmap *idmap,
    handle_t *handle, struct inode *dir,
    umode_t mode, const struct qstr *qstr,
    __u32 goal, uid_t *owner, __u32 i_flags,
    int handle_type, unsigned int line_no, int nblocks)
```

**完整流程**（370+ 行，从第931行到约1300行）：

```
__ext4_new_inode()
  │
  ├─[1] 验证目录有效性、紧急状态检查                     (line 955-963)
  ├─[2] 调用 new_inode(sb) 从 slab 分配 inode           (line 967)
  ├─[3] 设置 owner/uid/gid/projid                       (line 977-992)
  ├─[4] fscrypt_prepare_new_inode 准备加密上下文          (line 995)
  ├─[5] 选择目标块组：
  │     S_ISDIR(mode) → find_group_orlov()               (line 1024)
  │     否则          → find_group_other()               (line 1026)
  │
  ├─[6] 遍历块组搜索空闲 inode 槽位:                      (line 1039)
  │     ├─读取 inode 位图
  │     ├─find_inode_bit() 在位图中找空闲 bit            (line 1073)
  │     ├─ext4_test_and_set_bit() 原子设置 bit           (line 1104)
  │     └─被抢占？持有锁重试 find_inode_bit              (line 1109)
  │
  ├─[7] 标记 inode 位图脏 + journal 日志                  (line 1131)
  ├─[8] 初始化块位图（如果 BG_BLOCK_UNINIT）              (line 1146-1184)
  ├─[9] 更新组描述符中的统计信息                          (line 1187+)
  ├─[10] 初始化 ext2_inode 磁盘结构                       (line ~1240)
  ├─[11] 设置 extent 树/xattr/加密/inline_data            (line ~1260+)
  └─[12] 返回初始化好的 inode
```

**竞争处理**（line 1103-1117）：

```c
ext4_lock_group(sb, group);
ret2 = ext4_test_and_set_bit(ino, inode_bitmap_bh->b_data);
if (ret2) {
    // inode位已被抢占，持有锁重新搜索
    ret2 = find_inode_bit(sb, group, inode_bitmap_bh, &ino);
    if (ret2) {
        ext4_set_bit(ino, inode_bitmap_bh->b_data);
        ret2 = 0;
    }
}
ext4_unlock_group(sb, group);
```

这是经典的乐观-悲观并发控制：先快速查找空闲位，查到后加锁原子测试并设置。如果被抢占，在锁保护下重新搜索。

---

## 8. Orlov 算法：find_group_orlov

`find_group_orlov`（ialloc.c:422）实现了著名的 **Orlov 目录分配算法**，用于为目录选择最佳的块组。

**算法目标**：将同一层的顶层目录均匀散布到不同块组，避免所有目录集中在少数块组中。

```c
// ialloc.c:422
static int find_group_orlov(struct super_block *sb, struct inode *parent,
                            ext4_group_t *group, umode_t mode,
                            const struct qstr *qstr)
```

**算法分两个模式**：

### 模式A：顶层目录分配（line 455-508）

当父目录是**根目录**或带有 `EXT4_INODE_TOPDIR` 标志时触发：

```
顶层目录分配步骤：
  1. 如果是目录 && 父是根目录或 TOPDIR:
  2. 使用 half-MD4 哈希计算起始 flex group           (line 462-465)
     hinfo.seed = s_hash_seed;
     ext4fs_dirhash(parent, qstr->name, qstr->len, &hinfo);
     parent_group = hinfo.hash % ngroups;
  3. 遍历所有 flex group，选择最优:
     - free_inodes > 0
     - used_dirs 最少（best_ndir）
     - free_inodes >= avefreei
     - free_clusters >= avefreec
  4. 在选中的 flex group 内找有空闲 inode 的子组    (line 498-507)
```

目录哈希散列（ext4fs_dirhash）将目录名映射到数值，确保同名的目录在不同文件系统中分配到相似的组。这让 `rsync` 等工具的行为更具可预测性。

如果 `qstr` 为空（非创建路径），使用 `get_random_u32_below(ngroups)` 随机化起始组（line 467）。

### 模式B：一般目录分配（line 511-538）

定义一个滚动窗口，阈值：
- `max_dirs = ndirs/ngroups + inodes_per_group*flex_size/16` — 最大目录数
- `min_inodes = avefreei - inodes_per_group*flex_size/4` — 最小 free inode 数
- `min_clusters = avefreec - EXT4_CLUSTERS_PER_GROUP*flex_size/4` — 最小 free cluster 数

从 `i_last_alloc_group`（上次分配的组）开始线性扫描，找到第一个同时满足三个约束的 flex group。

### Fallback 路径（line 541-567）

如果上述两种模式都没找到合适的组，执行**递减阈值重试**：
1. 将阈值放宽到：只要 `free_inodes >= 平均空闲 inode 数` 即可
2. 再失败则将 `avefreei` 设为 0，不要求任何最低空闲量
3. 最终返回 -1，调用者会 try next group 或返回 ENOSPC

---

## 9. 普通文件组选择：find_group_other

`find_group_other`（ialloc.c:570）为**非目录文件**（普通文件、符号链接等）选择块组：

```c
// ialloc.c:570
static int find_group_other(struct super_block *sb, struct inode *parent,
                            ext4_group_t *group, umode_t mode)
```

**策略优先级**：

```
┌──────────────────────────────────────────────────┐
│ find_group_other 分配策略                          │
├────────────┬─────────────────────────────────────┤
│ 优先级      │ 策略                                 │
├────────────┼─────────────────────────────────────┤
│ 1 (最高)    │ 与父目录相同 flex group                │
│ 2          │ i_last_alloc_group 所在 flex group    │
│ 3          │ find_group_orlov() 回退到 Orlov 算法  │
│ 4 (无flex)  │ 与父目录相同 block group              │
│ 5          │ 二次哈希搜索                           │
│ 6 (最低)    │ 线性搜索（仅需 1 个 free inode）       │
└────────────┴─────────────────────────────────────┘
```

**二次哈希搜索**（line 640-648）：
```c
*group = (*group + (unsigned int)parent->i_ino) % ngroups;
for (i = 1; i < ngroups; i <<= 1) {
    *group += i;
    if (*group >= ngroups)
        *group -= ngroups;
    // 同时检查 free inodes 和 free clusters
    if (desc && ext4_free_inodes_count(sb, desc) &&
        ext4_free_group_clusters(sb, desc))
        return 0;
}
```

步长按 2 的幂次增加（1, 2, 4, 8, ...），这是一种分散策略——在块组空间中以越来越大的间隔探测，确保最终能找到可用组而不会全部挤在相邻组。

**线性扫描降级**（line 654-661）：如果二次哈希也失败，退化为简单的线性扫描，只要求至少 1 个空闲 inode。

---

## 10. 无日志模式的 recently_deleted 保护

`recently_deleted`（ialloc.c:675）是无日志模式的特有保护机制：

```c
#define RECENTCY_MIN   60   // 60 秒
#define RECENTCY_DIRTY 300  // 300 秒

static int recently_deleted(struct super_block *sb,
                            ext4_group_t group, int ino)
```

**工作原理**：
- 读取 inode table 中的 `dtime`（删除时间）
- 如果 `dtime + RECENTCY_MIN` > 当前时间，拒绝复用该 inode
- 如果 inode table 块脏但未回写，将保护窗口扩大到 `RECENTCY_DIRTY`

**原因**：无日志模式下没有 journal replay 保证，如果文件系统崩溃，刚删除的 inode 数据可能还没来得及回写到磁盘。稍等片刻再复用这些 inode 给了回写一个完成的窗口期。

---

## 11. Inode 释放：ext4_free_inode

`ext4_free_inode`（ialloc.c:235）是 inode 分配的反向操作：

```c
// ialloc.c:235
void ext4_free_inode(handle_t *handle, struct inode *inode)
```

**流程**：

```
ext4_free_inode(inode)
  │
  ├─验证 inode: i_count<=1, i_nlink==0, ino 范围有效     (line 255-284)
  ├─ext4_clear_inode(inode) — 释放所有块、清除 xattr      (line 278)
  ├─计算 group = (ino-1) / EXT4_INODES_PER_GROUP        (line 285)
  ├─读取 inode 位图 + 获取写访问                          (line 287-304)
  ├─获取组描述符写访问                                     (line 309-313)
  ├─ext4_clear_bit(ino) — 清除位图中的位                  (line ~325)
  ├─ext4_handle_dirty_metadata — 标记位图脏              (line ~340)
  ├─更新组描述符:
  │  gdp->free_inodes_count++                            (line ~350)
  │  if (is_directory) gdp->used_dirs_count--            (line ~355)
  ├─标记组描述符脏                                        (line ~370)
  └─更新 percpu 计数器:
     percpu_counter_inc(s_freeinodes_counter)            (line ~380)
     if (is_directory) percpu_counter_dec(s_dirs_counter)
```

---

## 12. Flex Block Groups

Flex block group 是 ext4 的一项重要优化，将多个连续的块组捆绑在一起：

```
柔性块组（flex_bg, s_log_groups_per_flex=2 → 4 groups per flex）:

┌─────────────────────────────────────────────────────────────────┐
│  Group 0          │ Group 1  │ Group 2  │ Group 3               │
│  [bitmaps]        │ [data]   │ [data]   │ [data]                │
│  [inode tables]   │          │          │                       │
│  [data...]        │          │          │                       │
└─────────────────────────────────────────────────────────────────┘
```

**关键属性**：

- 所有 bitmap 和 inode table 打包在 flex group 的第一个块组中
- 其余块组全部留给数据块
- 这样可以减少寻道：元数据查找集中在连续区域，数据则分布更广

在代码中，`ext4_flex_bg_size(sbi)` 返回 `2^s_log_groups_per_flex`，当它 > 1 时，许多算法以 flex group 为单位而不是单个 block group 为单位工作。

---

## 13. 分配完整流程总结

```
应用层: write(fd, buf, 4KB)
  │
  ▼
VFS: generic_file_write_iter
  │
  ▼
ext4: ext4_file_write_iter
  │
  ▼
ext4_da_write_begin 或 ext4_write_begin
  │
  ▼
ext4_map_blocks(map)
  │
  ▼
ext4_ext_map_blocks(handle, inode, map)
  │ m_lblk=100, m_len=1
  ▼
ext4_mb_new_blocks(handle, &ar, &err)
  │ ar.inode=..., ar.len=1, ar.logical=100, ar.goal=...
  ▼
  ├── ext4_mb_use_preallocated(ac)    【阶段1: inode PA】
  │   └── 命中? → 直接返回物理块号
  │
  ├── ext4_mb_normalize_request(ac)   【规范化: 1块→64块】
  ├── ext4_mb_pa_alloc(ac)            【预留 PA 槽位】
  ├── ext4_mb_regular_allocator(ac)   【阶段3: buddy cache】
  │   ├── ext4_mb_find_by_goal()      → 在目标块附近搜索
  │   ├── ext4_mb_find_best_pa_group()→ 从 locality group 找最佳组
  │   └── ext4_mb_complex_scan_group()→ buddy 位图扫描
  │
  └── ext4_mb_mark_diskspace_used(ac) 【写入磁盘位图 + buddy更新】
      ├── ext4_mb_mark_bb(sb, block, len, true)
      ├── mb_set_bits(bh->b_data, bit, len)
      └── ext4_handle_dirty_metadata(bitmap_bh)
```

对于 inode 分配：
```
mkdir("mydir")
  ▼
ext4_mkdir(namei.c:2995)
  │
  ▼
ext4_new_inode_start_handle(idmap, dir, S_IFDIR, ...)
  │
  ▼
__ext4_new_inode(idmap, handle, dir, S_IFDIR, ...)  (ialloc.c:931)
  │
  ├── find_group_orlov(sb, dir, &group, S_IFDIR, qstr)
  │   └── Orlov 算法选择最佳块组
  │
  ├── find_inode_bit() → ext4_test_and_set_bit()
  │   └── 在位图中抢占 inode 槽位
  │
  └── 填充 ext4_inode_info 结构:
      ├── ext4_init_extent_tree → extent 树
      ├── ext4_init_security  → xattr 安全属性
      └── fscrypt 上下文设置
```

---

## 14. Sysfs 调优接口

| 参数 | 路径 | 默认值 | 说明 |
|------|------|--------|------|
| mb_stream_request | /sys/fs/ext4/\<dev\>/mb_stream_req | 16 | 小文件 group prealloc 阈值 |
| mb_group_prealloc | /sys/fs/ext4/\<dev\>/mb_group_prealloc | 512 | group prealloc 标准化大小 |
| mb_max_to_scan | /sys/fs/ext4/\<dev\>/mb_max_to_scan | 200 | buddy 扫描的最大组数 |
| mb_min_to_scan | /sys/fs/ext4/\<dev\>/mb_min_to_scan | 16 | buddy 扫描的最小组数 |
| mb_order2_req | /sys/fs/ext4/\<dev\>/mb_order2_req | 2 | order-2 请求的最少块数 |
| inode_readahead_blks | /sys/fs/ext4/\<dev\>/inode_readahead_blks | 32 | inode table 预读块数 |

---

## 下一篇文章

下一篇 [第五篇：目录操作与 HTree 索引](./05-dir-htree.md) 将深入分析 ext4 的目录结构——从简单的线性目录项到复杂的 HTree B 树索引，以及 fscrypt 加密目录查找的完整实现。
