---
title: 'GPU Buddy 与内核 Buddy / SLUB 深度对比分析'
date: '2026/05/20 19:42:17'
updated: '2026/05/20 19:42:17'
---

# GPU Buddy 与内核 Buddy / SLUB 深度对比分析

## 目录
1. [概览](#概览)
2. [内核 Buddy (page_alloc) 深度解析](#内核-buddy-page_alloc-深度解析)
3. [SLUB 深度解析](#slub-深度解析)
4. [GPU Buddy 深度解析](#gpu-buddy-深度解析)
5. [三者的层次关系](#三者的层次关系)
6. [关键差异对比表](#关键差异对比表)

---

## 概览

Linux 内核中有三个不同的"buddy"或分配器概念，它们管理不同层面的内存：

| 分配器 | 管理对象 | 层次 | 源码位置 |
|--------|---------|------|---------|
| **内核 Buddy** | 物理 RAM 页框 | 最底层，管理物理内存 | `mm/page_alloc.c` |
| **SLUB** | 内核堆对象 (`kmalloc`/`kmem_cache_alloc`) | 中间层，基于内核 Buddy | `mm/slub.c` |
| **GPU Buddy** | GPU 显存地址空间 | 独立层，只借用 SLUB 存元数据 | `drivers/gpu/buddy.c` |

---

## 内核 Buddy (page_alloc) 深度解析

### 管理对象

管理**系统物理 RAM 页框**（`struct page`），每个页面典型大小为 4KB。这是 Linux 内核最底层的物理内存分配器。

### 数据结构

每个内存 **zone** 维护一个 `free_area` 数组，按 order 和迁移类型组织空闲页：

```c
// include/linux/mmzone.h:192-195
struct free_area {
    struct list_head free_list[MIGRATE_TYPES];  // 每个迁移类型一条链表
    unsigned long     nr_free;                  // 该 order 的空闲页数
};

// include/linux/mmzone.h:1087
struct free_area free_area[NR_PAGE_ORDERS];     // order 0..MAX_PAGE_ORDER
```

**关键设计：链表而非红黑树。** 每个 `free_list[migratetype]` 是一条双向链表，空闲页通过 `page->buddy_list` 挂入链表。伙伴信息直接编码在 `struct page` 中：

```c
// mm/page_alloc.c:708-711
static inline void set_buddy_order(struct page *page, unsigned int order)
{
    set_page_private(page, order);  // order 写入 page->private
    __SetPageBuddy(page);           // 设置 PageBuddy 标志
}
```

### 分配路径

```
alloc_pages(gfp, order)
  → __alloc_pages()
    → get_page_from_freelist()
      → rmqueue()
        → __rmqueue_smallest()   // 从请求的 order 开始向上扫描 free_list
```

分配策略是从请求 order 的 `free_list` 中取一个空闲页，如果该 order 为空则向更大 order 搜索，找到后分裂大块。**始终使用 preferred migratetype 的链表，优先从链表头部取**（FIFO）。

### 释放与合并

```c
// mm/page_alloc.c:934-956
static inline void __free_one_page(struct page *page,
        unsigned long pfn, struct zone *zone, unsigned int order,
        int migratetype, fpi_t fpi_flags)
{
    while (order < MAX_PAGE_ORDER) {
        buddy = find_buddy_page_pfn(page, pfn, order, &buddy_pfn);
        if (!buddy)
            goto done_merging;

        // 迁移类型不兼容则停止合并
        if (migratetype != buddy_mt &&
            (!migratetype_is_mergeable(migratetype) ||
             !migratetype_is_mergeable(buddy_mt)))
            goto done_merging;

        // 从 free_list 摘下 buddy，继续向上一级合并
        __del_page_from_free_list(buddy, zone, order, buddy_mt);
    }
done_merging:
    __add_to_free_list(page, zone, order, migratetype, to_tail);
}
```

合并逻辑：**循环向上**，通过 `find_buddy_page_pfn()` 根据 pfn 和 order 位运算找到伙伴页，检查是否为 free 且迁移类型兼容，满足则合并并继续向上一级。

### 碎片化控制

内核 Buddy 有最完善的碎片化控制：

- **多迁移类型**：UNMOVABLE、MOVABLE、RECLAIMABLE、CMA、HIGHATOMIC 等，每种类型独立链表
- **pageblock 粒度**：以 `pageblock_order` 为单位分组页面，不同迁移类型按 pageblock 隔离，防止不可移动页面永久碎片化可移动区域
- **compaction (内存压缩)**：`mm/compaction.c` 通过 `fragmentation_index()` 评估碎片程度，触发 `kcompactd` 迁移页面生成大块连续内存
- **暴露接口**：`/proc/buddyinfo` 显示每个 zone 每个 order 的空闲块数，`debugfs/extfrag/` 输出碎片化指标

### 最大 Order

```c
#ifndef CONFIG_FORCE_MAX_ZONEORDER
#define MAX_PAGE_ORDER 10           // 1024页 = 4MB (典型值，可配置)
#endif
```

这意味着内核 Buddy 能分配的最大连续物理内存为 `PAGE_SIZE << 10`（典型 4MB）。

---

## SLUB 深度解析

### 管理对象

管理**内核堆对象** — `kmalloc()` 和 `kmem_cache_alloc()` 的后端。SLUB 是对象分配器，不是块分配器。

### 数据结构

```c
// mm/slub.c:404-424
struct slab_sheaf {
    union {
        struct rcu_head rcu_head;
        struct list_head barn_list;
        struct {
            unsigned int capacity;
            bool pfmemalloc;
        };
    };
    struct kmem_cache *cache;
    unsigned int size;
    int node;
    void *objects[];         // 预分配的对象指针数组
};

struct slub_percpu_sheaves {
    local_trylock_t lock;
    struct slab_sheaf *main;     // 主缓存，永不为 NULL
    struct slab_sheaf *spare;    // 备用缓存，可为空
    struct slab_sheaf *rcu_free; // kfree_rcu 批量释放
};
```

每个 `kmem_cache` 有：
- **Per-CPU sheaf 缓存**：`main`/`spare`/`rcu_free` 三块，实现**几乎无锁**的快速路径
- **Per-node 链表**：partial（部分空闲 slab）、full（全满 slab）

### 分配路径

```
kmalloc(size, GFP_KERNEL)
  → slab_alloc()
    → slab_alloc_node()
      → 尝试从 per-CPU main sheaf 取对象        (无锁快速路径)
      → 若 main 为空，从 spare 交换过来         (local_trylock)
      → 若 spare 也为空，从 node partial list 补充 (spinlock)
      → 若 partial list 为空，调用 new_slab() 从内核 Buddy 申请新 slab
```

### slab 来源

```c
// mm/slub.c:581-600
static inline struct kmem_cache_order_objects oo_make(unsigned int order,
        unsigned int size)
{
    struct kmem_cache_order_objects x = {
        (order << OO_SHIFT) + order_objects(order, size)
    };
    return x;
}

static inline unsigned int oo_order(struct kmem_cache_order_objects x)
{
    return x.x >> OO_SHIFT;  // slab 的 order (从页分配器申请的页数)
}

static inline unsigned int oo_objects(struct kmem_cache_order_objects x)
{
    return x.x & ((1 << OO_SHIFT) - 1);  // 该 slab 可容纳的对象数
}
```

SLUB 从内核 Buddy 申请 page 作为 slab，再用 `oo_objects()` 将 slab 切成等大的对象槽位。**SLUB 自己不实现 buddy 逻辑**，它借用内核 Buddy 作为底层页来源。

### 释放路径

```
kfree(ptr)
  → slab_free()
    → 尝试加入 per-CPU main sheaf                  (无锁快速路径)
    → 若 main 已满，将其变为 spare，新 sheaf 作为 main
    → 若 slab 变得全空，释放 slab 归还内核 Buddy
```

**SLUB 不合并对象** — 释放只将对象指针还给同 slab 的 freelist，不会将相邻对象合并成大对象。

---

## GPU Buddy 深度解析

### 管理对象

管理 **GPU 显存地址空间** — 抽象的字节地址范围，不是系统物理内存。分配器以 `u64` 字节偏移量寻址。

### 核心数据结构

```c
// include/linux/gpu_buddy.h:91-133
struct gpu_buddy_block {
    u64 header;                              // 位域编码：offset + order + state + clear
    struct gpu_buddy_block *left;            // 左子节点
    struct gpu_buddy_block *right;           // 右子节点
    struct gpu_buddy_block *parent;          // 父节点
    void *private;                           // 用户私有数据
    union {
        struct rb_node rb;                   // 红黑树节点（空闲时）
        struct list_head link;               // 链表节点（已分配时）
    };
    struct list_head tmp_link;               // DFS 遍历临时链表
    unsigned int subtree_max_alignment;      // 子树最大对齐值（增广 RB 树）
};

// include/linux/gpu_buddy.h:158-182
struct gpu_buddy {
    struct rb_root **free_trees;             // 二维数组：free_trees[clear/dirty][order]
    struct gpu_buddy_block **roots;          // 根块数组（size 非 2 的幂时有多个根）
    unsigned int n_roots;
    unsigned int max_order;
    u64 chunk_size;                          // 最小分配粒度（>= 4KB，必须 2 的幂）
    u64 size;                                // 管理的总地址空间
    u64 avail;                               // 当前可用字节数
    u64 clear_avail;                         // 已清零的可用字节数
};
```

**关键设计：增广红黑树而非链表。** 每个 order 有 `clear` 和 `dirty` 两棵红黑树，按 block offset 排序，并通过 `subtree_max_alignment` 增广以支持高效对齐查找。

### Header 位域编码

```c
// include/linux/gpu_buddy.h:101-109
#define GPU_BUDDY_HEADER_OFFSET GENMASK_ULL(63, 12)   // offset: 52 bits
#define GPU_BUDDY_HEADER_STATE  GENMASK_ULL(11, 10)   // state: 2 bits
#define   GPU_BUDDY_ALLOCATED    (1 << 10)             // 00/01: allocated
#define   GPU_BUDDY_FREE         (2 << 10)             // 10: free
#define   GPU_BUDDY_SPLIT        (3 << 10)             // 11: split
#define GPU_BUDDY_HEADER_CLEAR  GENMASK_ULL(9, 9)     // clear: 1 bit
#define GPU_BUDDY_HEADER_ORDER  GENMASK_ULL(5, 0)     // order: 6 bits
```

一个 64-bit `header` 字段编码了 block 的完整状态信息，无需像内核 Buddy 依赖 `struct page` 的标志位。

### 元数据分配：借用 SLUB

```c
// drivers/gpu/buddy.c:36
static struct kmem_cache *slab_blocks;

// drivers/gpu/buddy.c:1514
slab_blocks = KMEM_CACHE(gpu_buddy_block, 0);

// drivers/gpu/buddy.c:84 -- 申请 block 节点
block = kmem_cache_zalloc(slab_blocks, GFP_KERNEL);

// drivers/gpu/buddy.c:101 -- 释放 block 节点
kmem_cache_free(slab_blocks, block);

// drivers/gpu/buddy.c:387-396 -- 分配 RB 树根数组
mm->free_trees = kmalloc_array(GPU_BUDDY_MAX_FREE_TREES,
                               sizeof(*mm->free_trees), GFP_KERNEL);
mm->free_trees[i] = kmalloc_array(mm->max_order + 1,
                                  sizeof(struct rb_root), GFP_KERNEL);

// drivers/gpu/buddy.c:406-408 -- 分配 roots 数组
mm->roots = kmalloc_array(mm->n_roots,
                          sizeof(struct gpu_buddy_block *), GFP_KERNEL);
```

GPU Buddy 从内核借用了三种内存：
1. **`kmem_cache`**: 每个 `gpu_buddy_block` 节点（约 72 字节）
2. **`kmalloc_array`**: 红黑树根指针数组（`struct rb_root *[]`）
3. **`kmalloc_array`**: 根块指针数组（`struct gpu_buddy_block *[]`）

**它管理的 GPU 显存本身不经过内核页分配器**，只是用内核内存来"记账"。

### 初始化

```c
// drivers/gpu/buddy.c:363-451
int gpu_buddy_init(struct gpu_buddy *mm, u64 size, u64 chunk_size)
{
    size = round_down(size, chunk_size);
    mm->size = size;
    mm->avail = size;
    mm->clear_avail = 0;
    mm->chunk_size = chunk_size;
    mm->max_order = ilog2(size) - ilog2(chunk_size);

    // 分配红黑树数组
    mm->free_trees = kmalloc_array(GPU_BUDDY_MAX_FREE_TREES, ...);
    for_each_free_tree(i) {
        mm->free_trees[i] = kmalloc_array(mm->max_order + 1, ...);
        for (j = 0; j <= mm->max_order; ++j)
            mm->free_trees[i][j] = RB_ROOT;   // 初始化为空树
    }

    // 分配根块数组
    mm->n_roots = hweight64(size);            // size 的二进制中 1 的个数
    mm->roots = kmalloc_array(mm->n_roots, ...);

    // 将 size 拆分为 power-of-two 块，每个作为一棵子树的 root
    do {
        order = ilog2(size) - ilog2(chunk_size);
        root_size = chunk_size << order;
        root = gpu_block_alloc(mm, NULL, order, offset);  // 从 kmem_cache 申请
        mark_free(mm, root);                               // 插入红黑树
        mm->roots[root_count] = root;
        offset += root_size;
        size -= root_size;
    } while (size);
}
```

**非 2 的幂大小的处理**：当 `size` 不是 2 的幂时（例如 6GB = 4GB + 2GB），`hweight64(size)` 返回二进制中 1 的个数（6GB = 0x180000000，1 个 1），`n_roots` 就是子树根的数量。每个 root 是最大的 2 的幂块，恰好填满剩余空间。

> 例如 size=0x180000000 (6GB):
> - 第一个 root: `1 << floor(log2(6GB))` = 4GB, offset=0
> - 第二个 root: `1 << floor(log2(2GB))` = 2GB, offset=4GB

### 分配路径

```c
// drivers/gpu/buddy.c:1277-1449
int gpu_buddy_alloc_blocks(struct gpu_buddy *mm,
                           u64 start, u64 end, u64 size,
                           u64 min_block_size,
                           struct list_head *blocks,
                           unsigned long flags)
```

三种分配策略，由 flags 控制：

1. **`GPU_BUDDY_RANGE_ALLOCATION`**: 范围受限分配 (`__gpu_buddy_alloc_range_bias`)
   - 从根块开始 DFS 遍历，找到包含在 `[start, end)` 范围内的空闲块
   - 沿途分裂过大的块
   
2. **偏移对齐分配** (`gpu_buddy_offset_aligned_allocation`): 当 `size < min_block_size` 且 size 为 2 的幂时
   - 利用增广红黑树的 `subtree_max_alignment` 字段高效搜索对齐块
   - 优先右子树（高地址），找到满足对齐约束的块

3. **自由分配** (`alloc_from_freetree`): 无范围限制
   - 从优先红黑树（clear 或 dirty）中搜索
   - 支持 `GPU_BUDDY_TOPDOWN_ALLOCATION`（从高地址分配）
   - 优先树耗尽后 fallback 到另一棵树
   - 找到的块若阶数大于请求阶数，循环分裂

分配完成后支持 `GPU_BUDDY_TRIM_DISABLE` 控制的自动 Trim：将未使用的多余空间归还。

### 分裂逻辑

```c
// drivers/gpu/buddy.c:491-523
static int split_block(struct gpu_buddy *mm,
                       struct gpu_buddy_block *block)
{
    unsigned int block_order = gpu_buddy_block_order(block) - 1;
    u64 offset = gpu_buddy_block_offset(block);

    block->left  = gpu_block_alloc(mm, block, block_order, offset);
    block->right = gpu_block_alloc(mm, block, block_order,
                                   offset + (mm->chunk_size << block_order));
    mark_split(mm, block);       // 从红黑树移除，标记为 SPLIT

    if (gpu_buddy_block_is_clear(block)) {
        mark_cleared(block->left);
        mark_cleared(block->right);
        clear_reset(block);      // 清除父块的 clear 标志
    }

    mark_free(mm, block->left);  // 左右子节点插入红黑树
    mark_free(mm, block->right);
}
```

分裂一个 order=n 的空闲块时：
1. 从 `kmem_cache` 分配两个新 `gpu_buddy_block` 作为 left/right
2. 父块标记为 SPLIT，从空闲红黑树移除
3. 左右子块标记为 FREE，插入对应红黑树
4. 若父块原本是 clear，子块继承 clear，父块清除 clear 标志

### 释放与合并

```c
// drivers/gpu/buddy.c:245-287
static unsigned int __gpu_buddy_free(struct gpu_buddy *mm,
                                     struct gpu_buddy_block *block,
                                     bool force_merge)
{
    struct gpu_buddy_block *parent;

    while ((parent = block->parent)) {
        buddy = __get_buddy(block);          // 获取兄弟节点

        if (!gpu_buddy_block_is_free(buddy))
            break;                           // 兄弟不空闲，停止合并

        if (!force_merge) {
            // clear/dirty 不匹配时无法合并
            if (gpu_buddy_block_is_clear(block) !=
                gpu_buddy_block_is_clear(buddy))
                break;

            if (gpu_buddy_block_is_clear(block))
                mark_cleared(parent);        // 合并后父块继承 clear
        }

        rbtree_remove(mm, buddy);            // 从红黑树移除兄弟
        gpu_block_free(mm, block);           // 归还 block 到 kmem_cache
        gpu_block_free(mm, buddy);           // 归还 buddy 到 kmem_cache
        block = parent;                      // 继续向上一级合并
    }

    mark_free(mm, block);                    // 标记为 FREE，插入红黑树
}
```

合并逻辑与内核 Buddy 类似但有关键差异：

1. **clear/dirty 屏障**：两个伙伴 clear 状态不同时阻止合并（`force_merge=false`）。这是 GPU 特有的优化 — 清零和未清零内存不能混并。
2. **归还到 kmem_cache**：合并后子节点 block 对象归还到 SLUB，而非释放"物理页面"。

### 强制合并 (`__force_merge`)

```c
// drivers/gpu/buddy.c:289-349
static int __force_merge(struct gpu_buddy *mm,
                         u64 start, u64 end, unsigned int min_order)
{
    for_each_free_tree(tree) {
        for (i = min_order - 1; i >= 0; i--) {
            // 从低 order 红黑树逆序遍历
            iter = rb_last(&mm->free_trees[tree][i]);
            while (iter) {
                block = rbtree_get_free_block(iter);
                buddy = __get_buddy(block);
                if (gpu_buddy_block_is_free(buddy)) {
                    // clear/dirty 不匹配也能强制合并！
                    gpu_buddy_assert(gpu_buddy_block_is_clear(block) !=
                                     gpu_buddy_block_is_clear(buddy));
                    __gpu_buddy_free(mm, block, true);  // force_merge=true
                    if (order >= min_order) return 0;
                }
            }
        }
    }
}
```

当正常分配路径失败时，`__force_merge` 被调用以**强制合并 clear 和 dirty 伙伴**，打破 clear/dirty 碎片化，生成更大的块。

### 增广红黑树与对齐分配

```c
// drivers/gpu/buddy.c:70-73
RB_DECLARE_CALLBACKS_MAX(static, gpu_buddy_augment_cb,
                         struct gpu_buddy_block, rb,
                         unsigned int, subtree_max_alignment,
                         gpu_buddy_block_offset_alignment);
```

红黑树按 offset 排序，增广属性 `subtree_max_alignment` 记录了子树中块的最大对齐值（`__ffs64(offset)`）。这使得可以 O(log n) 搜索满足对齐约束的空闲块：

```c
// drivers/gpu/buddy.c:864-899
static struct gpu_buddy_block *
gpu_buddy_find_block_aligned(struct gpu_buddy *mm, ...)
{
    while (rb) {
        block = rbtree_get_free_block(rb);
        // 先看右子树能否满足对齐要求
        if (right_node && gpu_buddy_subtree_can_satisfy(right_node, alignment))
            { rb = right_node; continue; }
        // 再看当前节点
        if (gpu_buddy_block_offset_alignment(block) >= alignment)
            return block;
        // 最后看左子树
        if (left_node && gpu_buddy_subtree_can_satisfy(left_node, alignment))
            { rb = left_node; continue; }
        break;
    }
}
```

这是 GPU Buddy 独有的特性 — 内核 Buddy 只支持按页边界对齐，不支持任意 2 的幂对齐。

---

## 三者的层次关系

```
                              ┌─────────────────────────────────┐
                              │         用户程序                 │
                              └───────────────┬─────────────────┘
                                              │
        ┌─────────────────────────────────────┼──────────────────────────┐
        │                                     │                          │
        ▼                                     ▼                          ▼
┌───────────────┐                   ┌─────────────────┐      ┌──────────────────┐
│   kmalloc()   │                   │  GPU 驱动        │      │  kmem_cache_alloc │
│  (通用堆分配)  │                   │  (i915/amdgpu等) │      │  (专用对象缓存)    │
└───────┬───────┘                   └────────┬────────┘      └────────┬─────────┘
        │                                    │                        │
        ▼                                    ▼                        ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                              SLUB (mm/slub.c)                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                    │
│  │main sheaf    │  │spare sheaf   │  │rcu_free      │  per-CPU 无锁缓存   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                    │
│         │                 │                 │                             │
│         └─────────────────┼─────────────────┘                             │
│                           ▼                                               │
│                ┌─────────────────────┐                                    │
│                │ kmem_cache_node    │  per-node 链表                     │
│                │ (partial/full slab)│                                    │
│                └─────────┬───────────┘                                    │
└──────────────────────────┼────────────────────────────────────────────────┘
                           │ new_slab() / free_slab()
                           ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                      内核 Buddy (mm/page_alloc.c)                          │
│  ┌──────────────────────────────────────────────────────────┐            │
│  │ zone->free_area[order].free_list[MIGRATE_TYPES]         │            │
│  └──────────────────────────────────────────────────────────┘            │
│  管理：物理 RAM 页框 (struct page)                                        │
└───────────────────────────────────────────────────────────────────────────┘

                     ╔═══════════════════════════════════╗
                     ║  GPU Buddy (drivers/gpu/buddy.c) ║
                     ║                                   ║
                     ║  ┌─────────────────────────────┐  ║
                     ║  │ free_trees[clear/dirty]     │  ║
                     ║  │   [order].rb_root (RB树)    │  ║
                     ║  └─────────────────────────────┘  ║
                     ║  管理：GPU 显存地址空间 (u64)        ║
                     ║                                   ║
                     ║  元数据来源：                       ║
                     ║   gpu_buddy_block ──► kmem_cache  ║
                     ║   RB 树根数组     ──► kmalloc     ║
                     ╚═══════════════════════════════════╝
```

**关键路径**：

1. **用户申请内核内存** → `kmalloc()` → SLUB per-CPU sheaf → (if miss) → 内核 Buddy 申请 slab page → 切成对象返回
2. **GPU 驱动申请显存** → `gpu_buddy_alloc_blocks()` → 红黑树查找 → 分裂 → 返回 block → 驱动据此操作 GPU 显存
3. **GPU Buddy 申请元数据** → `kmem_cache_zalloc(slab_blocks)` → SLUB → `gpu_buddy_block` 节点

---

## 关键差异对比表

| 维度 | 内核 Buddy (page_alloc) | SLUB (slab allocator) | GPU Buddy |
|------|------------------------|----------------------|-----------|
| **源码位置** | `mm/page_alloc.c` | `mm/slub.c` | `drivers/gpu/buddy.c` |
| **管理对象** | 物理 RAM 页框 | 内核堆对象 | GPU 显存地址空间 |
| **分配器类型** | 块/页分配器 (binary buddy) | 对象/slab 分配器 | 块/页分配器 (binary buddy) |
| **分配粒度** | 1 页 (4KB)，order 0..MAX_ORDER | 8B ~ 8KB+，由 cache object_size 决定 | chunk_size 字节 (>=4KB)，order 0..n |
| **空闲块结构** | 双向链表 `free_list[MIGRATE_TYPES]` | per-CPU sheaf + per-node partial 链表 | **增广红黑树** `free_trees[2][order]` |
| **伙伴合并** | `__free_one_page()` 循环向上，检查迁移类型 | **不合并** — 对象回到 slab freelist | `__gpu_buddy_free()` 循环向上，检查 clear/dirty |
| **合并屏障** | 迁移类型不兼容 | N/A | clear/dirty 不匹配（可 force_merge 打破） |
| **分裂** | `__rmqueue_smallest()` 向上搜索后向下分裂 | N/A | `split_block()` 从 kmem_cache 分配左右子节点 |
| **元数据位置** | `struct page` 内嵌在 mem_map 中 | `kmem_cache` + `struct slab`（在 page 元数据区） | **独立 kmem_cache** 分配 `gpu_buddy_block` |
| **对齐支持** | 仅页对齐 (4KB) | 可配置 per-cache `align` | **增广 RB 树支持任意 2 的幂对齐** |
| **分配策略** | preferred migratetype 链表头部取 | per-CPU 无锁 sheaf | clear 优先 + top-down + 范围约束 |
| **碎片化控制** | 多迁移类型 + pageblock 隔离 + compaction | 无（依赖底层 Buddy） | clear/dirty 双树 + force_merge + top-down |
| **锁粒度** | zone spinlock (IRQ-safe) | local_trylock (per-CPU) + node spinlock | **外部** — 调用者提供 mutex |
| **NUMA 感知** | 有（per-zone, per-node） | 有（per-node partial, per-CPU） | **无** |
| **存储依赖** | 管理的页本身就是物理内存 | 从内核 Buddy 申请 slab page | 只从内核 SLUB 申请元数据，显存独立管理 |
| **最大分配** | `MAX_PAGE_ORDER` (10, ~4MB 典型) | `KMALLOC_MAX_SIZE` (通常 8KB, 大分配走页分配器) | `GPU_BUDDY_MAX_ORDER=51` 即 `chunk_size << 51` |

---

## GPU Buddy 为什么用红黑树而不是链表？

内核 Buddy 的 `free_list` 链表方案在以下条件成立时运作良好：
- 伙伴通过 `pfn ^ (1 << order)` 位运算直接算出
- 物理页是连续的线性地址空间
- 不需要按地址范围搜索（分配时不指定地址）

GPU Buddy 需要的能力链表无法满足：
- **范围约束分配**（`GPU_BUDDY_RANGE_ALLOCATION`）：需要在 `[start, end)` 范围内找到空闲块
- **对齐分配**（`gpu_buddy_offset_aligned_allocation`）：需要搜索满足 `__ffs64(offset) >= alignment` 的块
- **Top-down 分配**：需要从最高地址开始分配

红黑树按 offset 排序 + `subtree_max_alignment` 增广，使得上述操作都是 O(log n)。链表只能做 O(n) 线性扫描。

---

## GPU Buddy 与内核 Buddy 的关键行为差异

### 1. 元数据与数据分离

内核 Buddy 中 `struct page` 嵌入在 `mem_map[]` 数组中，与管理的物理内存共存。每个 struct page 约 64 字节，直接对应一个物理页。

GPU Buddy 的 `gpu_buddy_block` 从 `kmem_cache` 独立分配，与管理的显存完全分离。显存大小与 block 节点数量无直接耦合。

### 2. 状态机

```
内核 Buddy 状态机（简化）:
  PageBuddy flag + page->private(order) → free
  PageBuddy unset                         → allocated

GPU Buddy 状态机:
  FREE           → 在红黑树中，可分配
  ALLOCATED      → 已分配，从红黑树移除
  SPLIT          → 已分裂为左右子节点，从红黑树移除
  FREE+clear     → 在 clear 树中
  FREE+!clear    → 在 dirty 树中
```

GPU Buddy 多了一个 SPLIT 状态和 clear/dirty 属性。

### 3. Clear/Dirty 双树

这是 GPU 特有的优化。GPU 显存清零（memset to 0）是昂贵的操作。通过维护两棵红黑树：
- `CLEAR_TREE`：内容已清零的空闲块
- `DIRTY_TREE`：内容不确定的空闲块

分配时优先使用 CLEAR_TREE 可避免 GPU 清零开销。释放时通过 `GPU_BUDDY_CLEARED` 标志告知块已清零。

---

## 总结

| | 内核 Buddy | SLUB | GPU Buddy |
|---|---|---|---|
| **核心** | 管理物理页，链表+迁移类型 | 管理堆对象，per-CPU 无锁缓存 | 管理显存地址，增广红黑树 |
| **借用内核** | —（最底层） | 从内核 Buddy 拿 slab page | 从 SLUB 拿 block 节点 |
| **产出** | 连续物理页框 | 固定大小堆对象 | GPU 显存地址范围 |
| **独有特性** | 迁移类型隔离、compaction | per-CPU sheaf、kfree_rcu 批量 | clear/dirty 双树、对齐分配、范围约束、force_merge |