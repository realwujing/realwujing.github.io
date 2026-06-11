# 第二篇：范围树 — 二叉搜索、插入分裂与合并

> 源码：`fs/ext4/ext4_extents.h`, `fs/ext4/extents.c` | 头文件：`fs/ext4/ext4_extents.h`

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. 为什么需要 extent 树

传统的 Unix 文件系统使用**间接块 (indirect blocks)** 来映射文件逻辑块到物理块。在 ext2/ext3 中，`i_block[15]` 的前 12 个是直接指针，后 3 个是间接/双重间接/三重间接块。这种方案的问题是：

1. **大文件的元数据开销大**：一个 1 GB 文件需要 ~256 个间接块
2. **碎片化严重**：每个块是独立映射的，无法表达连续区域
3. **查找效率低**：访问文件尾部需要遍历多层间接块

**extent** 将连续物理块表示为一个范围 `(logical_block, length, physical_block)`：

```
传统间接块映射:            extent 映射:
                           ┌──────────────────────────────┐
block map:                 │ extent: [0 → 128, pblk=1000] │
  0 → pblk=1000            │ extent: [128 → 64, pblk=1200]│
  1 → pblk=1001            │ extent: [192 → 256, pblk=800]│
  2 → pblk=1002            └──────────────────────────────┘
  ...                      3 条记录 vs 448 条记录
```

ext4 的 extent 编码非常紧凑——每条 extent 只有 12 字节。下一个问题是：当文件有很多 extent 时怎么办？答案就是**extent 树**。

---

## 2. 从 `i_block[15]` 说起：树根就在 inode 里

```c
// fs/ext4/ext4.h:817
__le32  i_block[EXT4_N_BLOCKS];  // EXT4_N_BLOCKS = 15, 共 60 字节
```

当 `EXT4_EXTENTS_FL` 设置时，`i_block[15]` 的内容完全变了：

```
i_block[15] = 60 字节的 extent 树根节点：
┌───────────────────────────────────────────────────┐
│  offset 0: ext4_extent_header (12 字节)           │
│    eh_magic      (2B) = 0xf30a                    │
│    eh_entries    (2B) = 当前条目数                 │
│    eh_max        (2B) = 最大条目数 (行内通常为4)     │
│    eh_depth      (2B) = 0=叶子, >0=内部节点        │
│    eh_generation (4B) = 树代数                      │
│  offset 12: 条目区 (48 字节 = 4 条 × 12 字节)      │
│    当 eh_depth=0: ext4_extent × 4                 │
│    当 eh_depth>0: ext4_extent_idx × 4              │
└───────────────────────────────────────────────────┘
```

**关键设计**：eh_depth=0 且 eh_entries≤4 → 所有 extent 都在 inode 内，不需要任何额外磁盘块。这覆盖了绝大多数小文件 (<16 KB with 4KB blocks) 的场景。

---

## 3. 核心数据结构 (`ext4_extents.h`)

### 3.1 `struct ext4_extent_header` — 节点头 (`ext4_extents.h:78`)

```c
// fs/ext4/ext4_extents.h:78
struct ext4_extent_header {
    __le16  eh_magic;         // 魔数 0xf30a
    __le16  eh_entries;       // 当前有效条目数
    __le16  eh_max;           // 最大条目数（由块大小决定）
    __le16  eh_depth;         // 0=叶子节点, >0=内部索引节点
    __le32  eh_generation;    // 树代数，匹配 inode 的 i_generation
};
```

魔数常量：`#define EXT4_EXT_MAGIC cpu_to_le16(0xf30a)` (`ext4_extents.h:86`)

最大深度：`#define EXT4_MAX_EXTENT_DEPTH 5` (`ext4_extents.h:87`)

`eh_max` 的计算：
```
// 对于外部块 (block_size 字节):
eh_max = (block_size - header_size - tail_size) / entry_size
       = (block_size - 12 - 4) / 12
       = (4096 - 16) / 12 = 340 条（4KB 块）

// 对于内联的 inode 根:
eh_max = 4  （硬编码为 ext4_extent_header 后的前 4 个 extent）
```

### 3.2 `struct ext4_extent` — 叶子 extent (`ext4_extents.h:56`)

```c
// fs/ext4/ext4_extents.h:56
struct ext4_extent {
    __le32  ee_block;       // 第一个逻辑块号
    __le16  ee_len;         // 长度（块数），MSB=1 表示未初始化的 extent
    __le16  ee_start_hi;    // 物理起始块号 高 16 位
    __le32  ee_start_lo;    // 物理起始块号 低 32 位
};
```

总共 12 字节，表示一个连续的物理区域。物理块号 = `(ee_start_hi << 32) | ee_start_lo`，共 48 位。

`ee_len` 的编码：

```
ee_len 的 bit[15] = 0: 正常 extent，值为实际块数 (1~32767)
ee_len 的 bit[15] = 1: 未初始化 extent (unwritten)
    ee_len & 0x7FFF = 块数 (1~32767)
```

**未初始化 extent** 表示"磁盘上已分配空间，但尚未写入数据"。用于 `fallocate()` 预分配，只在首次写入时才转换。好处：
- 避免预分配导致的零填充 I/O
- 支持 thin provisioning 语义
- 与 `punch hole` / `collapse range` 等操作协同

### 3.3 `struct ext4_extent_idx` — 内部索引 (`ext4_extents.h:67`)

```c
// fs/ext4/ext4_extents.h:67
struct ext4_extent_idx {
    __le32  ei_block;       // 该索引覆盖的逻辑块范围下限
    __le16  ei_leaf_hi;     // 下一层块的物理地址 高 16 位
    __u16   ei_unused;      // 未使用
    __le32  ei_leaf_lo;     // 下一层块的物理地址 低 32 位
};
```

内部节点存的是 **索引条目**，不是 extent。`ei_block` 表示该子树覆盖的最小逻辑块号，`ei_leaf` 指向下一层节点所在的物理块。

查找时采用二分搜索：
```
对于要查找的逻辑块 L:
  在 [ei_block[0], ei_block[1], ..., ei_block[n-1]] 中二分
  找到满足 ei_block[i] ≤ L < ei_block[i+1] 的条目 i
  向下进入 ei_leaf[i] 指向的子节点
```

### 3.4 `struct ext4_extent_tail` — 块尾校验和 (`ext4_extents.h:48`)

```c
// fs/ext4/ext4_extents.h:48
struct ext4_extent_tail {
    __le32  et_checksum;    // crc32c(uuid + inum + extent_block)
};
```

在 `metadata_csum` 特性启用时，每个 extent 块的最后一个 4 字节是校验和。计算方式：

```c
// fs/ext4/ext4_extents.h:89
#define EXT4_EXTENT_TAIL_OFFSET(hdr) \
    (sizeof(struct ext4_extent_header) + \
     (sizeof(struct ext4_extent) * le16_to_cpu((hdr)->eh_max)))
```

这个偏移恰好落在 `block_size` 的末尾，因为 `block_size % 12 ≥ 4` 对所有合法的 ext4 块大小成立。

### 3.5 `struct ext4_ext_path` — 查找路径 (`ext4_extents.h:105`)

```c
// fs/ext4/ext4_extents.h:105
struct ext4_ext_path {
    ext4_fsblk_t                p_block;   // 该层节点的物理块号
    __u16                       p_depth;   // 该层的深度（从根开始计）
    __u16                       p_maxdepth;// 路径数组的总深度
    struct ext4_extent         *p_ext;     // 指向找到的 extent
    struct ext4_extent_idx     *p_idx;     // 指向找到的索引条目
    struct ext4_extent_header  *p_hdr;     // 本层节点的 header
    struct buffer_head         *p_bh;      // buffer_head（用于释放）
};
```

这是一个深度为 `p_maxdepth` 的数组：`path[0]` 是根节点，`path[depth]` 是找到 extent 的叶子节点。

图解：
```
path[depth=0] ──► 根节点 (i_block 或外部块)
path[depth=1] ──► 内部索引节点 (如果树深 > 1)
...
path[depth=N] ──► 叶子节点，p_ext 指向目标 extent
```

---

## 4. `ext4_find_extent` — 二分查找 (`extents.c:886`)

```c
// fs/ext4/extents.c:886
struct ext4_ext_path *
ext4_find_extent(struct inode *inode, ext4_lblk_t block,
                 struct ext4_ext_path *path, int flags)
```

### 算法

```
INPUT:  inode, 逻辑块号 block, 可选的已有 path
OUTPUT: ext4_ext_path 数组，path[depth].p_ext 指向覆盖 block 的 extent

1. 从 inode 读取 eh (ext4_extent_header)
2. depth = eh->eh_depth
3. 校验 depth 在 [0, EXT4_MAX_EXTENT_DEPTH]
4. 分配/复用 path 数组 (depth+1 项)
5. 从根节点开始遍历:
   for i = 0; i <= depth; i++:
     a. 读当前节点 (path[i].p_hdr = eh)
     b. 如果 i == depth (叶子节点):
          二分搜索 ex → 返回满足 ee_block ≤ block < ee_block+ee_len 的 extent
     c. 否则 (内部节点):
          二分搜索 ix → 返回满足 ei_block ≤ block 的最右索引
          block_no = ix->ei_leaf  (下一层物理块)
          bh = sb_bread(block_no)   (读取下一层)
          eh = bh 的 header
```

核心的二分搜索（内部节点）：

```
// 在 n 个索引条目中二分查找
k = 0, upper = n-1
while k < upper:
    mid = (k + upper + 1) / 2
    if ei_block[mid] <= block:
        k = mid
    else:
        upper = mid - 1
// k 是满足 ei_block[k] ≤ block 的最大索引
```

这个"偏右二分搜索"保证返回的索引是正确的：搜索键 `block` 在 `ei_block[k]` 和 `ei_block[k+1]` 之间（或 k 是最后一个条目）。

叶子节点的二分搜索类似：
```
// 找到覆盖 block 的那个 extent
k = 0, upper = n-1
while k < upper:
    mid = (k + upper + 1) / 2
    if ee_block[mid] <= block:
        k = mid
    else:
        upper = mid - 1
// 返回 ex[k]，如果 block < ee_block[k] + ee_len[k] 则命中
```

### 路径重用

如果调用者传入已有 `path`，`ext4_find_extent` 会释放旧 buffer_head 并尝试重用 path 数组（可能扩展）。

```
ext4_ext_drop_refs(path);        // 释放旧的 bh 引用
if (depth > path[0].p_maxdepth)  // 树变深了
    kfree(path);                  // 需要重新分配
    path = kcalloc(depth+1, ...);
```

---

## 5. `ext4_ext_split` — 节点分裂 (`extents.c:1053`)

```c
// fs/ext4/extents.c:1053
static int ext4_ext_split(handle_t *handle, struct inode *inode,
                          unsigned int flags,
                          struct ext4_ext_path *path,
                          struct ext4_extent *newext, int at)
```

当一个节点已满（eh_entries == eh_max），需要插入新条目时，调用 `ext4_ext_split`。

### 分裂算法

```
INPUT:  path (从根到叶子的完整路径), at (在哪个深度分裂)
OUTPUT: 树被重新平衡，path 更新为新的查找结果

1. 确定需要分配的新块:
   depth = ext_depth(inode)
   需要 depth 个新块（每个深度一个）

2. 分配新块:
   for i = at; i <= depth; i++:
       分配一个新块 newblock

3. 从叶子节点开始，自下而上分裂:
   for i = at; i <= depth; i++:
     a. 当前节点 = path[i].p_hdr
     b. 新节点 = 分配的新块 newblock[i]
     c. 确定分裂点:
        border = newext->ee_block  (边界逻辑块号)
        a = 当前节点中，满足 ei_block[在 border 序列右边] 的数量
        m = eh_entries - a  (余下的条目)
     d. 将右边 a 条条目移到新节点
     e. 初始化新节点的 header:
        neh->eh_entries = a
        neh->eh_max 不变
        neh->eh_depth = 当前深度 == 叶子 ? 0 : 对应子树深度
     f. 在父节点插入指向新节点的索引:
        fidx = &path[i-1].p_idx[插入位置]
        fidx->ei_block = 新节点的最小逻辑块
        fidx->ei_leaf = newblock[i]
        path[i-1].p_hdr->eh_entries++
```

分裂示意图：

```
分裂前 (节点满，需要插入 new):       分裂后:
┌─────────────────────┐             ┌──────────┐   ┌──────────┐
│ [A] [B] [C]      [D]│  eh_max=4   │ [A] [B]  │   │ [C] [D]  │ new
│ 条目已满，不能插入   │             │ 3 条左移  │   │ 2 条右移  │
└─────────────────────┘             └──────────┘   └──────────┘
                                         ▲
                                   父节点新增索引指向右子
```

### 分配失败处理

如果分裂过程中磁盘空间不足，`ext4_ext_split` 返回 -ENOSPC，上层代码可以回退或重试。

---

## 6. `ext4_ext_insert_extent` — 插入入口 (`extents.c:1992`)

```c
// fs/ext4/extents.c:1992
struct ext4_ext_path *
ext4_ext_insert_extent(handle_t *handle, struct inode *inode,
                       struct ext4_ext_path *path,
                       struct ext4_extent *newext, int gb_flags)
```

这是向 extent 树插入一条新 extent 的核心函数。

### 插入流程

```
ext4_ext_insert_extent(handle, inode, path, newext, gb_flags)
│
├─ 1. depth = ext_depth(inode), eh = path[depth].p_hdr
│
├─ 2. 对待插入位置附近的 extent 尝试合并:
│       ext4_ext_try_to_merge(handle, inode, path, nearex)
│       （见下一节）
│       ├─ 如果合并成功 (newext 被融入已有 extent)
│       │    return updated path
│       └─ 如果无法合并，继续
│
├─ 3. 如果当前叶子节点未满 (eh_entries < eh_max):
│       └─ 移动后续条目腾出空间，插入 newext
│       └─ eh_entries++
│
├─ 4. 如果叶子节点已满:
│       ├─ ext4_ext_split(handle, inode, flags, path, newext, depth)
│       │   — 分裂叶子节点（可能递归到父节点）
│       ├─ 分裂完成后，newext 被插入到左半或右半
│       │
│       └─ 根节点也可能溢出:
│           if ext_depth(inode) 变深了:
│               ext4_ext_grow_indepth(handle, inode, flags, newext)
│               — 树的高度 +1
│
└─ 5. 更新 inode (mark dirty)
```

`nearex` 是通过 `ext4_find_extent` 找到的最接近待插入逻辑块号的 extent：

```
insert position: 要在 ee_block=N 插入新 extent
   [extent A] [extent B]    [extent C]  [extent D]
   ee_block=0  ee_block=100           ee_block=500

情况1: N=50  → nearex 可能是 B 或 插入间隙
情况2: N=300 → nearex 是 C（二分搜索找到的后续 extent）
```

### 树高度增长 (`ext4_ext_grow_indepth`)

当根节点本身也分裂导致溢出时，需要增加树的高度：

```
分裂前 (depth=1):                   分裂后 (depth=2):
 inode                                inode
 [header eh_depth=1]                  [header eh_depth=2]
 [idx→leafA] [idx→leafB]              [idx→new_root]
    ▲          ▲                       │
    │          │                ┌──────┴──────┐
 leafA      leafB            leafA     [空槽] leafB
                             (可插入)           (可插入)
```

---

## 7. `ext4_ext_try_to_merge` — 合并 (`extents.c:1912`)

```c
// fs/ext4/extents.c:1912
static void ext4_ext_try_to_merge(handle_t *handle,
                                  struct inode *inode,
                                  struct ext4_ext_path *path,
                                  struct ext4_extent *ex)
```

合并是保持 extent 树紧凑的关键机制。

### 三种合并

#### (a) 向右合并 (`ext4_ext_try_to_merge_right`, `extents.c:1825`)

```
合并前:                 合并后:
 [extent A] [extent B]   [extent A']
 ee_block=0, len=10      ee_block=0, len=20
 ee_block=10, len=10
 物理连续: A.start+10 == B.start  ✓
```

检查条件：
1. 逻辑连续性：`A.ee_block + A.ee_len == B.ee_block`
2. 物理连续性：`A.start + A.ee_len == B.start`
3. 未初始化状态一致：A 和 B 的 unwritten 位相同

合并后删除 B。

#### (b) 向上合并 (`ext4_ext_try_to_merge_up`, `extents.c:1866`)

```
合并前 (depth=1):                 合并后 (depth=0):
 inode                              inode
 [header eh_depth=1]                [header eh_depth=0]
 [idx→leaf]                         [extent A] [extent B] [extent C]
   │                                所有 extent 上移到 inode
 leaf: [extent A] [extent B] [extent C]
```

如果叶子节点的所有 extent 能装进父节点（inode 根），就把子树扁平化。条件是 `leaf->eh_entries ≤ inode's eh_max (4)`。

#### (c) 合并流程

```c
// fs/ext4/extents.c:1912
static void ext4_ext_try_to_merge(handle_t *handle,
                                  struct inode *inode,
                                  struct ext4_ext_path *path,
                                  struct ext4_extent *ex) {
    int merge_done = 0;

    // 1. 先尝试跟左边邻居合并 (ex-1 与 ex 向右合并)
    if (ex > EXT_FIRST_EXTENT(eh))
        merge_done = ext4_ext_try_to_merge_right(inode, path, ex - 1);

    // 2. 再尝试跟右边邻居合并 (ex 与 ex+1 向右合并)
    if (!merge_done)
        ext4_ext_try_to_merge_right(inode, path, ex);

    // 3. 如果合并后叶子为空，尝试向上收缩树
    ext4_ext_try_to_merge_up(handle, inode, path);
}
```

`ext4_ext_try_to_merge_up` 还会触发连锁反应：
```
叶子空了 → 删除叶子块 → 父节点的指针减少 → 
父节点也可能空了 → 继续向上 → 直到根节点
```

---

## 8. 完整的 extent 操作生命周期

### 分配新块并插入 extent

```
ext4_ext_map_blocks(handle, inode, map, flags)
│
├─ 获取 handle (journal 事务)
│
├─ ext4_find_extent(inode, map->m_lblk, NULL, flags)
│    └─ 找到覆盖目标逻辑块的 extent（或 nearest）
│
├─ 判断情况:
│   ├─ 目标区域已经在某个 extent 内 → 直接返回物理块
│   ├─ 部分覆盖已有的 extent → 需要分裂零碎的 extent
│   └─ 全新的范围 → 继续分配
│
├─ ext4_mb_new_blocks(handle, inode, goal, ...)
│    └─ 通过 mballoc 分配物理块
│
├─ ext4_ext_insert_extent(handle, inode, path, &newext, flags)
│    ├─ ext4_ext_try_to_merge → 尝试合并相邻 extent
│    ├─ 叶子满 → ext4_ext_split → 分裂
│    └─ mark inode dirty
│
└─ 返回映射结果 (map->m_pblk)
```

### 截断 / 删除块的 extent 删除

```
ext4_ext_remove_space(inode, start, end)
│
├─ 找到 start 和 end 的 extent
│
├─ 对于完整覆盖的 extent → 删除整个 extent
├─ 对于部分覆盖的 extent → 截短或分裂（例如 [0,100] 删除 [20,80]
│   变成 [0,20) 和 [80,100) 两条 extent）
│
├─ ext4_ext_rm_leaf → 从叶子删除
├─ ext4_ext_rm_idx → 如果叶子空了，删除索引条目
│
└─ ext4_ext_try_to_merge_up → 收缩树
```

### extent 分裂示例 (split unwritten extent on first write)

```
写入前:
 [unwritten extent: ee_block=100, len=128, start=5000]
 (磁盘上 128 块已分配，但数据未初始化)

第一次写入 100~103:
 需要将 unwritten 转为 written:

写入后:
 [written extent: ee_block=100, len=4, start=5000]
 [unwritten extent: ee_block=104, len=124, start=5004]
 (可能需要分裂成 3 段: [前unwritten] [written] [后unwritten])
```

---

## 9. 未初始化 extent 与延迟分配

ext4 支持两种高级空间管理：

### 9.1 未初始化 extent (unwritten)

```c
#define EXT4_EXT_MARK_UNWRITTEN (1 << 15)
#define EXT4_EXT_MAY_UNWRITTEN  (1 << 15)  // 检查 ee_len 的 MSB
```

操作对应：
- `fallocate(FALLOC_FL_KEEP_SIZE)` → 创建 unwritten extent
- 首次写入 → `ext4_convert_unwritten_extents()` 将其转为 written
- `fallocate(FALLOC_FL_PUNCH_HOLE)` → 将 written extent 转为 hole（或删除 extent）

### 9.2 延迟分配 (delayed allocation)

ext4 默认使用 `nodelalloc` 挂载选项时会实时分配，但默认行为是延迟分配：

```
write() → 数据存入 page cache（只标记脏页）
↓
writeback 线程 → writepages() → ext4_writepages()
↓
mpage_map_and_submit_extent():
  └─ 为连续的脏页范围分配 extent:
     ext4_map_blocks(DELALLOC) → ext4_es_lookup_extent() 找已有的延迟 extent
     → 如果找不到 → ext4_mb_new_blocks() 分配物理块
     → ext4_ext_insert_extent() 插入 extent
     → submit_bio() 写入磁盘
```

延迟分配 + unwritten extent 的组合实现了"分配写在物理块的时刻才分配实际物理块"，并且使用 unwritten extent 避免了"先零填充再写入"的浪费。

---

## 10. 校验和与 extent 树完整性

```c
// fs/ext4/ext4_extents.h:89
#define EXT4_EXTENT_TAIL_OFFSET(hdr) \
    (sizeof(struct ext4_extent_header) + \
     (sizeof(struct ext4_extent) * le16_to_cpu((hdr)->eh_max)))

static inline struct ext4_extent_tail *
find_ext4_extent_tail(struct ext4_extent_header *eh)
{
    return (struct ext4_extent_tail *)(((void *)eh) +
                                       EXT4_EXTENT_TAIL_OFFSET(eh));
}
```

读取 extent 块时，`ext4_extent_block_csum_verify()` 会计算 CRC32c 并与 `et_checksum` 比对。如果校验和不匹配，ext4 会报错并将文件系统标记为需要 fsck。

---

## 11. 实践：查看 extent 树

```bash
# debugfs 查看文件 extent 树
sudo debugfs -R "extents /path/to/file" /dev/sda1

# 示例输出:
# Level Entries       Logical      Physical Length Flags
#  0/ 0   1/  1    0 -    99  102400            100
#  0/ 0   2/  2  100 -   199  103000            100 Uninit
#  1/ 1   1/  2    0 -    99  102400            100
#  1/ 1   2/  2  100 -   199  103000            100 Uninit
```

也可以通过 `filefrag`：
```bash
filefrag -v /path/to/file
```

---

## 下一篇文章

[第三篇：日志 — JBD2 句柄、事务提交与快速提交](./03-journal.md)
