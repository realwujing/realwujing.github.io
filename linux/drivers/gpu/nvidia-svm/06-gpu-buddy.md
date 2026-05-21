# 第6篇：GPU 显存 Buddy 分配器

> 源码：`drivers/gpu/buddy.c` | 头文件：`include/linux/gpu_buddy.h`
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. 为什么 GPU 需要自己的 Buddy 分配器

内核已经有了经典的 `mm/page_alloc.c`  buddy 分配器，为什么还要在 `drivers/gpu/` 下再写一个？因为 GPU 和 CPU 对内存分配器的需求有本质区别：

| 特性 | 内核 buddy (mm/page_alloc.c) | GPU buddy (drivers/gpu/buddy.c) |
|------|------------------------------|--------------------------------|
| 管理对象 | `struct page` (物理页) | `gpu_buddy_block` (显存块) |
| 最小粒度 | PAGE_SIZE (4K) | chunk_size (可配置，最少 4K) |
| 地址空间 | 物理地址 | GPU 虚拟地址空间 |
| 对齐要求 | 普通 | 严格（alignment-sensitive） |
| 清零追踪 | 无 | 两棵树分开管理（clear/dirty） |
| 范围分配 | 不支持 | 支持 range allocation |
| 顺序 | 自底向上 | top-down 可选 |
| 数据结构 | 链表 free_area | 增广红黑树（子树最大对齐）+ 链表 |

GPU 显存不仅有普通的分配需求，还需要满足 GPU 硬件对**地址对齐**的严格要求。例如，某些纹理缓冲区需要 64KB 对齐，而帧缓冲区可能需要 256 字节对齐即可。

## 2. 核心数据结构

### 2.1 gpu_buddy_block — 一个 64 位的"瑞士军刀"

`include/linux/gpu_buddy.h:91-133`

```c
struct gpu_buddy_block {
    /*
     * Header bit layout:
     * - Bits 63:12: block offset within the address space
     * - Bits 11:10: state (ALLOCATED, FREE, or SPLIT)
     * - Bit 9: clear bit (1 if memory is zeroed)
     * - Bits 8:6: reserved
     * - Bits 5:0: order (log2 of size relative to chunk_size)
     */
#define GPU_BUDDY_HEADER_OFFSET GENMASK_ULL(63, 12)
#define GPU_BUDDY_HEADER_STATE  GENMASK_ULL(11, 10)
#define   GPU_BUDDY_ALLOCATED   (1 << 10)
#define   GPU_BUDDY_FREE        (2 << 10)
#define   GPU_BUDDY_SPLIT       (3 << 10)
#define GPU_BUDDY_HEADER_CLEAR  GENMASK_ULL(9, 9)
#define GPU_BUDDY_HEADER_ORDER  GENMASK_ULL(5, 0)
    u64 header;

    struct gpu_buddy_block *left;
    struct gpu_buddy_block *right;
    struct gpu_buddy_block *parent;

    void *private; /* owned by creator */

    union {
        struct rb_node rb;       /* 在 RB 树中时使用 */
        struct list_head link;   /* 用户分配后使用 */
    };
    struct list_head tmp_link;   /* DFS 临时链表 */
    unsigned int subtree_max_alignment; /* 子树中最大对齐值 */
};
```

**64-bit header 的位布局图解**：

```
 63                                12 11  10   9   8  6  5    0
┌──────────────────────────────────┬─┬─┬─┬───┬───────┬───────┐
│       offset (52 bits)           │S│S│C│ R │U N U S│ORDER  │
│                                  │1│0│ │ E │  E  D │ (6bit)│
└──────────────────────────────────┴─┴─┴─┴───┴───────┴───────┘
                                     │  │  │
                                     │  │  └─ CLEAR: 内存是否已清零
                                     │  └──── STATE: 00=ALLOCATED, 01=FREE, 11=SPLIT
                                     └─────── STATE 高位
```

**三个状态位**（`include/linux/gpu_buddy.h:103-105`）：

| 宏 | 值 | 含义 |
|----|-----|------|
| `GPU_BUDDY_ALLOCATED` | `1 << 10` | 已分配（不属于自由树） |
| `GPU_BUDDY_FREE` | `2 << 10` | 空闲（在 RB 树中等待分配） |
| `GPU_BUDDY_SPLIT` | `3 << 10` | 已分裂（有左右子块） |

**关键设计**：state 占 2 位（bits 11:10），三个值足够了。`MAX_ORDER` 定义为 `63 - 12 = 51`（`include/linux/gpu_buddy.h:136`），因为 63 位可用，其中 12 位被 header 保留，offset 占 52 位（bits 63:12），最大寻址 2^52 × 4K = 16PB。

**树结构**：每个 block 包含 `parent`、`left`、`right` 指针，形成一棵**二叉树**。`rb` 和 `link` 用 union 共用内存——block 在自由树中时用 `rb`，被用户分配出去后用 `link` 挂到用户链表。

### 2.2 gpu_buddy — 全局管理器

`include/linux/gpu_buddy.h:158-182`

```c
struct gpu_buddy {
    struct rb_root **free_trees;   /* [CLEAR/DIRTY][order] = RB root */
    struct gpu_buddy_block **roots; /* 顶层块数组（size 非 2 的幂时多个根） */
    unsigned int n_roots;
    unsigned int max_order;
    u64 chunk_size;                  /* 最小分配粒度 */
    u64 size;                        /* 管理的总地址空间 */
    u64 avail;                       /* 可用字节数 */
    u64 clear_avail;                 /* 清零块可用字节数（avail 子集） */
};
```

### 2.3 两棵树：CLEAR vs DIRTY

这是 GPU buddy 相比内核 buddy 的**创新之一**。内核 buddy 只管"空闲/已分配"二分，GPU buddy 对空闲块还区分是否需要清零：

```
free_trees[GPU_BUDDY_CLEAR_TREE][order]  ← 已清零的空闲块 (order 层级)
free_trees[GPU_BUDDY_DIRTY_TREE][order]  ← 未清零的空闲块 (order 层级)
```

`include/linux/gpu_buddy.h:70-77`：

```c
enum gpu_buddy_free_tree {
    GPU_BUDDY_CLEAR_TREE = 0,
    GPU_BUDDY_DIRTY_TREE,
    GPU_BUDDY_MAX_FREE_TREES,
};

#define for_each_free_tree(tree) \
    for ((tree) = 0; (tree) < GPU_BUDDY_MAX_FREE_TREES; (tree)++)
```

这样 GPU 驱动可以用 `GPU_BUDDY_CLEAR_ALLOCATION` 标志（行 50）优先从 CLEAR 树分配，避免显式做 GPU memset 清零操作。

## 3. 初始化和销毁

### 3.1 gpu_buddy_init

`drivers/gpu/buddy.c:363-424`

```c
int gpu_buddy_init(struct gpu_buddy *mm, u64 size, u64 chunk_size)
{
    unsigned int i, j, root_count = 0;
    u64 offset = 0;

    if (size < chunk_size)
        return -EINVAL;
    if (chunk_size < SZ_4K)
        return -EINVAL;
    if (!is_power_of_2(chunk_size))
        return -EINVAL;

    size = round_down(size, chunk_size);
    mm->size = size;
    mm->avail = size;
    mm->clear_avail = 0;
    mm->chunk_size = chunk_size;
    mm->max_order = ilog2(size) - ilog2(chunk_size);
```

初始化流程（行 377-430）：

```
1. 参数校验: chunk_size >= 4K, 2 的幂, size >= chunk_size
2. size 向下对齐到 chunk_size
3. max_order = ilog2(size) - ilog2(chunk_size)
4. 分配 free_trees[2][max_order+1]：kmalloc_array 二维索引
5. 初始化每个 RB_ROOT
6. n_roots = hweight64(size)（size 的二进制 1 的个数）
7. kmalloc_array 分配 roots[]
8. 将 size 按 2 的幂拆成多个根块
```

**为什么需要多个根**？假设 size = 6GB（不是 2 的幂），分配器会将其拆为：

```
┌──────────────────┬──────────┐
│   root[0]: 4GB   │ root[1]: │
│   (order=20)     │  2GB     │
│                  │ (order=19)│
└──────────────────┴──────────┘
      6GB 总显存 = 4GB + 2GB (两个 2 的幂)
```

**元数据分配**（行 387-408）：

```c
mm->free_trees = kmalloc_array(GPU_BUDDY_MAX_FREE_TREES,
                               sizeof(*mm->free_trees), GFP_KERNEL);

for_each_free_tree(i) {
    mm->free_trees[i] = kmalloc_array(mm->max_order + 1,
                                      sizeof(struct rb_root), GFP_KERNEL);
    for (j = 0; j <= mm->max_order; ++j)
        mm->free_trees[i][j] = RB_ROOT;
}

mm->n_roots = hweight64(size);
mm->roots = kmalloc_array(mm->n_roots,
                          sizeof(struct gpu_buddy_block *), GFP_KERNEL);
```

**Slab 缓存**（行 36, 1514）：

```c
static struct kmem_cache *slab_blocks;

static int __init gpu_buddy_module_init(void)
{
    slab_blocks = KMEM_CACHE(gpu_buddy_block, 0);
    // ...
}
```

所有 `gpu_buddy_block` 对象从同一个 `kmem_cache` 分配，避免通用 `kmalloc` 的碎片和额外开销。

### 3.2 gpu_buddy_fini

`drivers/gpu/buddy.c:465-488` — 释放所有树和根数组，确认 `mm->avail == mm->size`（所有内存已归还）。

## 4. 核心操作

### 4.1 split_block — 分裂一个块

`drivers/gpu/buddy.c:491-519`

```c
static int split_block(struct gpu_buddy *mm,
                       struct gpu_buddy_block *block)
{
    unsigned int block_order = gpu_buddy_block_order(block) - 1;
    u64 offset = gpu_buddy_block_offset(block);

    BUG_ON(!gpu_buddy_block_is_free(block));
    BUG_ON(!gpu_buddy_block_order(block));

    block->left = gpu_block_alloc(mm, block, block_order, offset);
    if (!block->left)
        return -ENOMEM;

    block->right = gpu_block_alloc(mm, block, block_order,
                                   offset + (mm->chunk_size << block_order));
    if (!block->right) {
        gpu_block_free(mm, block->left);
        return -ENOMEM;
    }

    mark_split(mm, block);      // 父块状态 → SPLIT
```

**图解**：一个 order=N 的块分裂成两个 order=N-1 的 buddy 块：

```
        ┌─────────────┐
        │  parent     │  order=N
        │  SPLIT      │
        └──┬───────┬──┘
           │       │
    ┌──────┴┐  ┌───┴──────┐
    │ left  │  │  right   │  order=N-1
    │ FREE  │  │  FREE    │  offset + chunk_size * 2^(N-1)
    └───────┘  └──────────┘
     offset      offset + chunk_size * 2^(N-1)
```

**Clear 属性继承**：如果父块是 cleared，子块也标记为 cleared（行 513-517）——清零过的块分裂后，子块内容自然也是零。

### 4.2 合并（buddy coalescing）

#### free_block 内部合并

`drivers/gpu/buddy.c:580-589`

```c
void gpu_buddy_free_block(struct gpu_buddy *mm,
                          struct gpu_buddy_block *block)
{
    BUG_ON(!gpu_buddy_block_is_allocated(block));
    mm->avail += gpu_buddy_block_size(mm, block);
    if (gpu_buddy_block_is_clear(block))
        mm->clear_avail += gpu_buddy_block_size(mm, block);

    __gpu_buddy_free(mm, block, false);
}
```

内部的 `__gpu_buddy_free` 会反复尝试向上合并：检查当前块的 buddy 是否也是 FREE，如果是则从 RB 树摘除，合并到父块，直到不能继续。

#### free_list 批量释放

`drivers/gpu/buddy.c:630-637`

```c
void gpu_buddy_free_list(struct gpu_buddy *mm,
                         struct list_head *objects,
                         unsigned int flags)
{
    bool mark_clear = flags & GPU_BUDDY_CLEARED;
    __gpu_buddy_free_list(mm, objects, mark_clear, !mark_clear);
}
```

批量释放时通过 `tmp_link` 做 DFS 后序遍历，确保子块先于父块释放。

### 4.3 gpu_buddy_alloc_blocks — 带有对齐感知的分配

`drivers/gpu/buddy.c:1277-1299`

```c
int gpu_buddy_alloc_blocks(struct gpu_buddy *mm,
                           u64 start, u64 end, u64 size,
                           u64 min_block_size,
                           struct list_head *blocks,
                           unsigned long flags)
{
    struct gpu_buddy_block *block = NULL;
    u64 original_size, original_min_size;
    unsigned int min_order, order;
    LIST_HEAD(allocated);
    unsigned long pages;
    int err;

    if (size < mm->chunk_size)
        return -EINVAL;
    if (min_block_size < mm->chunk_size)
        return -EINVAL;
    if (!is_power_of_2(min_block_size))
        return -EINVAL;
    if (!IS_ALIGNED(start | end | size, mm->chunk_size))
        // ...
```

分配 flag（`include/linux/gpu_buddy.h:23-68`）：

| Flag | 位 | 含义 |
|------|-----|------|
| `GPU_BUDDY_RANGE_ALLOCATION` | BIT(0) | 在 `[start, end)` 范围内分配 |
| `GPU_BUDDY_TOPDOWN_ALLOCATION` | BIT(1) | 从高地址往低地址分配 |
| `GPU_BUDDY_CONTIGUOUS_ALLOCATION` | BIT(2) | 必须返回一个连续块 |
| `GPU_BUDDY_CLEAR_ALLOCATION` | BIT(3) | 优先从 CLEAR 树分配 |
| `GPU_BUDDY_TRIM_DISABLE` | BIT(5) | 不裁剪多余空间 |

**分配流程简化**：

```
1. 参数校验 (size >= chunk_size, 对齐检查)
2. 如果有 RANGE_ALLOCATION:
   → 遍历所有根块，找到与 [start,end) 有交集的根
   → 在增广红黑树中搜索满足对齐+大小+范围的块
3. 如果无范围限制:
   → 直接从 free_trees 中取最小 fit 的块
4. 如果找到的块太大，用 split_block 不断分裂
5. 如果需要 CLEAR 但块是 DIRTY，尝试从 CLEAR 树分配
6. GPU_BUDDY_TRIM_DISABLE 未设置时，裁剪多余尾部
```

### 4.4 gpu_buddy_block_trim — 裁剪溢出空间

`drivers/gpu/buddy.c:1163-1184`

```c
int gpu_buddy_block_trim(struct gpu_buddy *mm,
                         u64 *start,
                         u64 new_size,
                         struct list_head *blocks)
{
    struct gpu_buddy_block *parent;
    struct gpu_buddy_block *block;
    u64 block_start, block_end;
    LIST_HEAD(dfs);
    u64 new_start;
    int err;

    if (!list_is_singular(blocks))
        return -EINVAL;
    block = list_first_entry(blocks, struct gpu_buddy_block, link);
    block_start = gpu_buddy_block_offset(block);
    block_end = block_start + gpu_buddy_block_size(mm, block);
```

例如：分配 3MB 但实际只需要 2MB。alloc 返回了一个 4MB 的块（2 的幂）。trim 函数将多余的 2MB 释放回 allocator。

## 5. 增广红黑树：O(log n) 对齐搜索

这是 GPU buddy 最精妙的设计。普通的 buddy 分配器在限制地址范围时，需要遍历整棵树，复杂度 O(n)。GPU buddy 使用**增广红黑树**（augmented RB tree），在每个节点存储 `subtree_max_alignment`，实现 O(log n) 的 aligned range 搜索。

### 增广节点定义

`include/linux/gpu_buddy.h:132`

```c
unsigned int subtree_max_alignment;  /* 子树中最大对齐值 */
```

当向 RB 树插入一个块时，更新整条路径上所有祖先的 `subtree_max_alignment`。搜索时：

```
┌─────────────────────────────────┐
│ 在节点 N 的子树中搜索            │
│ 条件: 合适的 offset且对齐≥请求  │
│ 剪枝: N.subtree_max_alignment <  │
│       请求对齐 → 跳过整棵子树    │
└─────────────────────────────────┘
```

### __force_merge — 紧急合并

`drivers/gpu/buddy.c:289-297`

```c
static int __force_merge(struct gpu_buddy *mm,
                         u64 start,
                         u64 end,
                         unsigned int min_order)
{
    unsigned int tree, order;
    int i;

    if (!min_order)
        return -ENOMEM;
```

当常规分配失败时，`__force_merge` 打破 **clear/dirty 隔离**，将 CLEAR 树和 DIRTY 树中的小块强制合并成大块。这是 "最后的手段"——内存碎片已经严重到无法满足请求时启用。

## 6. 完整分配流程示例

假设 GPU 有 16GB 显存，chunk_size = 4KB（order 0），请求：
- size = 64MB
- flags = `GPU_BUDDY_CONTIGUOUS_ALLOCATION | GPU_BUDDY_CLEAR_ALLOCATION`

```
初始状态:
  root[0]: order=22 (16GB), FREE, DIRTY

步骤 1: 在 CLEAR_TREE 中搜索
  → CLEAR 树为空，进入 DIRTY 树

步骤 2: 在 DIRTY_TREE[22] 中找到 root[0]
  → order 22 (16GB) 太大，需要分裂

步骤 3: 连续分裂 4 次 (22→21→20→19→18)
   order=22  ─split→  order=21 + order=21
   order=21  ─split→  order=20 + order=20
   ...
   order=18  ─split→  order=17 + order=17
   order=17 → 128MB, 仍太大
   order=17  ─split→  order=16 + order=16
   order=16 → 64MB = 刚好! 

步骤 4: mark_allocated(order=16 块)
   → mm->avail -= 64MB
   → 用户拿到 64MB 连续显存
   → 兄弟 order=16 块留在 DIRTY_TREE[16]
```

```
分裂后的树结构:
               root (order=22)                    ← 顶层
              /              \
        order=21           order=21
       /        \          /       \
   order=20    order=20  ...      ...
   /       \
order=19  order=19
   |
order=18  ← 所有中间节点标记为 SPLIT
   |
┌───────────────────────────┐
│ order=16 (ALLOCATED)     │  ← 返回给用户
│ size=64MB                │
└───────────────────────────┘
```

## 7. 与内核 buddy 对比

内核 `mm/page_alloc.c` buddy 是**最先出现**的 buddy 实现，但它为管理物理页而设计：

1. **free_area 是链表数组**，GPU buddy 用**红黑树+链表**实现对齐搜索
2. 内核 buddy 的 free_block 内嵌在 `struct zone` 中，GPU buddy 独立抽象
3. 内核 buddy 支持 watermark 和 GFP 标志（`GFP_ATOMIC`、`GFP_KERNEL`等），GPU buddy 只做块管理
4. GPU buddy 的 CLEAR/DIRTY 双树是独有特性——GPU 显存清零是昂贵的硬件操作

## 8. 在内核中的使用者

GPU buddy allocator 被多个 GPU 驱动使用，最常见的是通过 DRM buddy allocator 包装层调用。Intel i915、AMD amdgpu、NVIDIA nouveau 的显存管理器都依赖它。

NVIDIA AI Infra 场景中，当 GPU 驱动需要管理大块连续显存（如 CUDA 的 `cudaMalloc` 的大块分配、RDMA 超大 MR 的 GPU buffer），底层就是 buddy allocator 在切分和回收显存。

---

## 下一篇文章

[第7篇：GPU 命令提交调度：DRM Scheduler](07-gpu-scheduler.md)

简介：`drivers/gpu/drm/scheduler/` 中基于信用的优先级调度系统，控制 GPU 命令队列的流控、超时检测和并发提交。