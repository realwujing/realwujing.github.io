# 第一篇：Buddy 页分配器 — 伙伴系统、水位线与迁移类型

> 源码：mm/page_alloc.c, mm/internal.h | 头文件：include/linux/mmzone.h, include/linux/gfp.h

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. 概述

Buddy 页分配器是 Linux 内核物理页管理的基石。它以 `MAX_PAGE_ORDER` 阶（order）组织空闲页、按迁移类型隔离页面、通过水位线控制内存回收节奏，最终为 SLUB/slab 分配器、缺页异常处理、文件缓存等上层子系统提供物理页。

**核心文件**：
- `mm/page_alloc.c` — 分配器主体逻辑（~7800 行）
- `include/linux/mmzone.h` — zone、free_area、migratetype、watermark 数据结构
- `mm/internal.h` — ALLOC_WMARK_* 等内部分配标志
- `include/linux/gfp.h` — 用户可见的 GFP 标志与 alloc_pages 接口

**架构关系**：
```
用户调用             内核路径
───────────────────────────────────────────────
kmalloc()      ──►  SLUB/slab allocator
                       │ 调用 alloc_pages()
                       ▼
alloc_pages()  ──►  伙伴系统 (Buddy Allocator)
                       │ 从 free_area[order] 取页
                       │ 不足时分裂高阶块
                       ▼
物理页          ──►  struct page / struct folio
```

---

## 2. 核心数据结构

### 2.1 MAX_PAGE_ORDER — 最大分配阶

`include/linux/mmzone.h:31`
```c
#ifndef CONFIG_ARCH_FORCE_MAX_ORDER
#define MAX_PAGE_ORDER 10
#else
#define MAX_PAGE_ORDER CONFIG_ARCH_FORCE_MAX_ORDER
#endif
#define MAX_ORDER_NR_PAGES (1 << MAX_PAGE_ORDER)  // 1024 pages = 4MB (4K base)
#define NR_PAGE_ORDERS (MAX_PAGE_ORDER + 1)       // order 0..10, 共11个阶
```

x86_64 默认配置下：
- `MAX_PAGE_ORDER = 10`，单次最大分配 **1024 个连续物理页 = 4MB**（4K 页）
- 若启用大页（HugeTLB），base page 为 2MB 时最大分配可达 2GB
- `order=0` 分配 1 页，`order=3` 分配 8 页，依此类推

### 2.2 struct free_area — 空闲区域

`include/linux/mmzone.h:192-195`
```c
struct free_area {
    struct list_head    free_list[MIGRATE_TYPES];
    unsigned long       nr_free;
};
```

每个 zone 包含 `NR_PAGE_ORDERS`（11）个 `free_area`，每个 `free_area`：
- `free_list[MIGRATE_TYPES]`：按迁移类型分组的空闲页链表数组
- `nr_free`：该阶所有迁移类型的空闲页总数

**zone → free_area 层次关系**：
```
pglist_data (NUMA node)
├── node_zones[]
│   ├── ZONE_DMA32
│   │   └── free_area[MAX_PAGE_ORDER + 1]  (order 0..10)
│   │       └── free_area[order]
│   │           ├── free_list[MIGRATE_UNMOVABLE]
│   │           ├── free_list[MIGRATE_MOVABLE]
│   │           ├── free_list[MIGRATE_RECLAIMABLE]
│   │           ├── free_list[MIGRATE_CMA]
│   │           └── nr_free
│   ├── ZONE_NORMAL
│   │   └── free_area[...]
│   └── ZONE_MOVABLE
│       └── free_area[...]
└── ...
```

### 2.3 migratetype — 迁移类型（反碎片）

`include/linux/mmzone.h:118-144`
```c
enum migratetype {
    MIGRATE_UNMOVABLE,      // 不可移动页：内核数据结构
    MIGRATE_MOVABLE,        // 可移动页：用户进程页
    MIGRATE_RECLAIMABLE,    // 可回收页：文件缓存
    MIGRATE_PCPTYPES,       // PCP list 类型边界
    MIGRATE_HIGHATOMIC = MIGRATE_PCPTYPES,  // 高优先级原子分配
#ifdef CONFIG_CMA
    MIGRATE_CMA,            // CMA 连续内存区
#endif
#ifdef CONFIG_MEMORY_ISOLATION
    MIGRATE_ISOLATE,        // 内存隔离（热插拔/离线）
#endif
    MIGRATE_TYPES
};
```

**设计目标**：同类页面聚集在同一 pageblock（通常 2^(MAX_PAGE_ORDER-1) 页），便于连续内存分配和外碎片预防：
- **UNMOVABLE**：内核代码、页表、slab 对象等——不可迁移
- **MOVABLE**：匿名页、用户态 malloc——可迁移，通过 `move_pages()` 或 compaction
- **RECLAIMABLE**：page cache、tmpfs——可回收（直接丢弃或写回）
- **CMA**：为 DMA 预留的连续内存区，只用 MOVABLE 页面填充，需要时可迁移
- **ISOLATE**：标记为离线的 pageblock，禁止分配

---

## 3. 水位线 (Watermarks)

### 3.1 水位线枚举

`include/linux/mmzone.h:796-802`
```c
enum zone_watermarks {
    WMARK_MIN,      // 最低警戒：仅允许紧急分配（PF_MEMALLOC、OOM）
    WMARK_LOW,      // 低水位：唤醒 kswapd 后台回收
    WMARK_HIGH,     // 高水位：kswapd 可休眠
    WMARK_PROMO,    // 晋升水位（内存分层/PROMOTION）
    NR_WMARK
};
```

每个 zone 在初始化时计算三条水位线：
```
WMARK_MIN  = 根据 zone 大小按比例计算的最小保留页
WMARK_LOW  = WMARK_MIN + (WMARK_MIN / 4)
WMARK_HIGH = WMARK_MIN + (WMARK_MIN / 2)

┌───────────────────────────────────┐
│          free pages               │
│                                   │  above HIGH  → kswapd 休眠
│  ─ ─ HIGH watermark ─ ─ ─ ─ ─ ─ │
│                                   │  HIGH ~ LOW  → 正常分配，kswapd 不启动
│  ─ ─ LOW  watermark ─ ─ ─ ─ ─ ─ │
│                                   │  LOW ~ MIN   → 申请分配 → wake kswapd
│  ─ ─ MIN  watermark ─ ─ ─ ─ ─ ─ │
│  xxxxx 保留给紧急分配 xxxxxx      │  below MIN   → 直接回收 / OOM
└───────────────────────────────────┘
```

### 3.2 分配标志 — ALLOC_WMARK_*

`mm/internal.h:1445-1480`
```c
#define ALLOC_WMARK_MIN      WMARK_MIN       // 使用 MIN 水位做检查
#define ALLOC_WMARK_LOW      WMARK_LOW       // 使用 LOW 水位做检查
#define ALLOC_WMARK_HIGH     WMARK_HIGH      // 使用 HIGH 水位做检查
#define ALLOC_NO_WATERMARKS  0x04            // 不检查水位（紧急分配）
#define ALLOC_OOM            0x08            // OOM 场景，允许突破 MIN
#define ALLOC_NOFRAGMENT     0x100           // 禁止混合 pageblock 类型
#define ALLOC_KSWAPD         0x800           // 允许唤醒 kswapd
```

内核分配路径中的水位线升级策略：
```
初始尝试    ALLOC_WMARK_LOW  —— 正常分配
  ↓ 失败
慢速路径    ALLOC_WMARK_MIN  —— 降低水位要求
  ↓ 仍失败
            ALLOC_OOM        —— OOM 受害者可以突破 MIN
  ↓ 所有都失败
OOM killer  ALLOC_NO_WATERMARKS —— 最后手段
```

---

## 4. fallback 数组 — 迁移类型降级

`mm/page_alloc.c:1915-1919`
```c
static int fallbacks[MIGRATE_PCPTYPES][MIGRATE_PCPTYPES - 1] = {
    [MIGRATE_UNMOVABLE]   = { MIGRATE_RECLAIMABLE, MIGRATE_MOVABLE   },
    [MIGRATE_MOVABLE]     = { MIGRATE_RECLAIMABLE, MIGRATE_UNMOVABLE },
    [MIGRATE_RECLAIMABLE] = { MIGRATE_UNMOVABLE,   MIGRATE_MOVABLE   },
};
```

当首选迁移类型的空闲链表为空时，按 fallback 顺序从其他类型"借用"页：
```
UNMOVABLE 请求:
  ① MIGRATE_UNMOVABLE 链表
  ② MIGRATE_RECLAIMABLE 链表 (fallback)
  ③ MIGRATE_MOVABLE 链表 (fallback)

MOVABLE 请求:
  ① MIGRATE_MOVABLE 链表
  ② MIGRATE_RECLAIMABLE 链表 (fallback)
  ③ MIGRATE_UNMOVABLE 链表 (fallback)
```

注意 `MIGRATE_MOVABLE` 的 fallback 不会首先借用 UNMOVABLE——保持 MOVABLE 区域尽量纯净，有利于后续 compaction。

---

## 5. 快速路径：单页/低阶分配

### 5.1 alloc_pages 入口

`include/linux/gfp.h:322`
```c
struct page *alloc_pages_noprof(gfp_t gfp, unsigned int order);
```

用户态最常用的接口（GFP_KERNEL、GFP_ATOMIC 等）：

```c
// 分配 order-0 页
struct page *page = alloc_page(GFP_KERNEL);

// 分配 order-3 页（8 pages）
struct page *pages = alloc_pages(GFP_KERNEL, 3);
```

### 5.2 __alloc_frozen_pages_noprof — 分配器心脏

`mm/page_alloc.c:5185-5247`

这是整个 buddy 分配器的核心函数：

```c
struct page *__alloc_frozen_pages_noprof(gfp_t gfp, unsigned int order,
        int preferred_nid, nodemask_t *nodemask)
{
    struct page *page;
    unsigned int alloc_flags = ALLOC_WMARK_LOW;    // 初始使用 LOW 水位
    struct alloc_context ac = { };

    // 1. 参数校验：order 必须 <= MAX_PAGE_ORDER
    if (WARN_ON_ONCE_GFP(order > MAX_PAGE_ORDER, gfp))
        ...

    // 2. GFP 上下文修正（NOFS/NOIO 继承等）
    gfp = current_gfp_context(gfp);

    // 3. 准备分配上下文（zonelist、migratetype 确定）
    prepare_alloc_pages(gfp, order, preferred_nid, nodemask, &ac, ...);

    // 4. 禁止碎片化降级标志
    alloc_flags |= alloc_flags_nofragment(...);

    // 5. 第一次尝试：快速路径
    page = get_page_from_freelist(alloc_gfp, order, alloc_flags, &ac);
    if (likely(page))
        goto out;

    // 6. 快速路径失败 → 进入慢速路径
    page = __alloc_pages_slowpath(alloc_gfp, order, &ac);

out:
    // 7. memcg kmem 记账
    if (memcg_kmem_online() && (gfp & __GFP_ACCOUNT) && page)
        __memcg_kmem_charge_page(page, gfp, order);

    return page;
}
```

### 5.3 get_page_from_freelist — 遍历 zonelist

`mm/page_alloc.c:3787-3820`
```c
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
                       const struct alloc_context *ac)
{
    struct zoneref *z;
    struct zone *zone;

retry:
    // 按 zonelist 顺序遍历所有候选 zone
    z = ac->preferred_zoneref;
    for_next_zone_zonelist_nodemask(zone, z, ...) {
        // 水位检查：free pages 是否足够
        if (!zone_watermark_fast(zone, order,
                watermark (from alloc_flags), ...))
            continue;  // 水位不足，跳过该 zone

        // 尝试从该 zone 分配
        page = rmqueue(ac->preferred_zoneref->zone, zone, order,
                       gfp_mask, alloc_flags, ac->migratetype);
        if (page) {
            prep_new_page(page, order, gfp_mask, alloc_flags);
            return page;
        }
    }
    return NULL;
}
```

### 5.4 rmqueue — 单 zone 分配入口

`mm/page_alloc.c:3389-3410`
```c
static inline
struct page *rmqueue(struct zone *preferred_zone,
                     struct zone *zone, unsigned int order,
                     gfp_t gfp_flags, unsigned int alloc_flags,
                     int migratetype)
{
    struct page *page;

    // order ≤ pcp 允许的最大阶数 → 从 per-CPU 页缓存分配（快速）
    if (likely(pcp_allowed_order(order))) {
        page = rmqueue_pcplist(preferred_zone, zone, order,
                               migratetype, alloc_flags);
        if (likely(page))
            goto out;
    }

    // PCP 缓存没有 → 从 buddy free_area 直接分配
    page = rmqueue_buddy(zone, order, migratetype, alloc_flags);
    ...
}
```

### 5.5 Per-CPU Pages (PCP) — order-0 的极致快速路径

每个 CPU 维护一个 `per_cpu_pages` 结构，缓存当前 CPU 最近的 order-0 页：

```
CPU0  per_cpu_pages            CPU1  per_cpu_pages
┌──────────────────┐          ┌──────────────────┐
│ pcp->lists[]     │          │ pcp->lists[]     │
│  [MIGRATE_       │          │  [MIGRATE_       │
│   UNMOVABLE]     │          │   UNMOVABLE]     │
│  [MIGRATE_       │          │  [MIGRATE_       │
│   MOVABLE]       │          │   MOVABLE]       │
│  [MIGRATE_       │          │  [MIGRATE_       │
│   RECLAIMABLE]   │          │   RECLAIMABLE]   │
│                  │          │                  │
│ count: 剩余页数   │          │ count: 剩余页数   │
│ high:  缓存上限   │          │ high:  缓存上限   │
│ batch: 批量数量   │          │ batch: 批量数量   │
└──────────────────┘          └──────────────────┘
         │                            │
         │ 不足时批量补充              │
         ▼                            ▼
┌──────────────────────────────────────────┐
│         zone->free_area[]                │
│         (buddy allocator)                │
└──────────────────────────────────────────┘
```

`mm/page_alloc.c:3302-3343` — `__rmqueue_pcplist`：
```c
static inline
struct page *__rmqueue_pcplist(struct zone *zone, unsigned int order,
        int migratetype, unsigned int alloc_flags,
        struct per_cpu_pages *pcp, struct list_head *list)
{
    struct page *page;

    do {
        // PCP 链表为空 → 从 buddy bulk 补充
        if (list_empty(list)) {
            int batch = nr_pcp_alloc(pcp, zone, order);
            int alloced;

            alloced = rmqueue_bulk(zone, order, batch, list,
                                   migratetype, alloc_flags);
            pcp->count += alloced << order;
            if (unlikely(list_empty(list)))
                return NULL;
        }

        // 从 PCP 链表取第一页
        page = list_first_entry(list, struct page, pcp_list);
        list_del(&page->pcp_list);
        pcp->count -= 1 << order;  // order-0 减1，order-N 减 2^N
    } while (check_new_pages(page, order));

    return page;
}
```

**PCP 缓存热/冷**：PCP 页被释放时置于链表头部（热端），下次分配直接取头部——L1/L2 cache 仍然命中。

### 5.6 __rmqueue_smallest — 伙伴分裂

`mm/page_alloc.c:1883-1906`

当 PCP 缓存为空，或者请求 order > 0 时，直接从 buddy free_area 分配：

```c
static __always_inline
struct page *__rmqueue_smallest(struct zone *zone, unsigned int order,
                                int migratetype)
{
    unsigned int current_order;
    struct free_area *area;
    struct page *page;

    // 从请求的 order 开始向上搜索更大的空闲块
    for (current_order = order; current_order < NR_PAGE_ORDERS; ++current_order) {
        area = &(zone->free_area[current_order]);
        page = get_page_from_free_area(area, migratetype);
        if (!page)
            continue;  // 当前阶没空闲页，尝试更大的阶

        // 找到后执行 expand：将高阶块一半一半地分裂
        page_del_and_expand(zone, page, order, current_order, migratetype);
        return page;
    }

    return NULL;  // 所有阶都为空
}
```

**伙伴分裂示意图**（请求 order=1，当前 order=3 有块）：
```
order=3: ┌──────────────────────────────────────────┐  (8 pages)
         │  page0  page1  page2  page3  page4...    │
         └────────┬─────────────────────────────────┘
                  │ expand (分裂)
                  ▼
order=2: ┌──────────────────┐ ┌──────────────────┐
         │   新伙伴A (4页)    │ │   新伙伴B (4页)   │  ← B 归还 free_area[2]
         └────────┬─────────┘ └──────────────────┘
                  │ expand (继续分裂)
                  ▼
order=1: ┌───────────┐ ┌───────────┐
         │  返回调用者  │ │ 归还 free_area[1] │
         └───────────┘ └───────────┘
```

**合并场景**（释放 order=1 页）：
```
释放前:
order=1: [页A] (空闲)  +  [页B] (已分配)

释放 order=1 页B:
order=1: [页A] [页B]  → 二者的 PFN 是伙伴关系 →  合并
order=2: [页A+B]                                  → 继续检查上级伙伴
order=3: [页A+B+伙伴]                             → 最终合并到最高可能阶
```

伙伴地址计算公式：
```c
// 给定 page 的 PFN，找到其伙伴的 PFN
buddy_pfn = page_pfn ^ (1 << order);
// 例如：order=2，PFN=8 → buddy_pfn = 8 ^ 4 = 12
```

---

## 6. 慢速路径：__alloc_pages_slowpath

`mm/page_alloc.c:4682-4820`

```
__alloc_pages_slowpath
│
├─ 1. 唤醒 kswapd（后台回收线程）
│      wake_all_kswapds(order, gfp_mask, ac)
│
├─ 2. 降低水位再次尝试
│      alloc_flags = gfp_to_alloc_flags() → ALLOC_WMARK_MIN
│      page = get_page_from_freelist()
│
├─ 3. 尝试直接 compaction
│      (costly order 或 non-MOVABLE 高阶分配)
│      page = __alloc_pages_direct_compact()
│
├─ 4. 尝试直接回收 (direct reclaim)
│      page = __alloc_pages_direct_reclaim()
│      → 回收干净页 / 写回脏页 / swap out 匿名页
│
├─ 5. 再次尝试分配
│      page = get_page_from_freelist()
│      → 成功则返回
│
├─ 6. should_compact_retry / should_reclaim_retry
│      → 决定是否再次尝试 compaction/reclaim
│      → 最多循环 16 次 (MAX_RECLAIM_RETRIES)
│
└─ 7. 所有手段耗尽
       → __GFP_NOFAIL ? goto retry : goto nopage
```

### 6.1 __alloc_pages_direct_reclaim

`mm/page_alloc.c:4409-4430`

直接回收：当 kswapd 无法及时补充空闲页时，分配者自己进行回收：

```c
static inline struct page *
__alloc_pages_direct_reclaim(gfp_t gfp_mask, unsigned int order,
        unsigned int alloc_flags, const struct alloc_context *ac,
        unsigned long *did_some_progress)
{
    struct page *page = NULL;

    // 进入 direct reclaim
    // → __perform_reclaim() → try_to_free_pages() → shrink_node()
    *did_some_progress = __perform_reclaim(gfp_mask, order, ac);

    if (unlikely(!(*did_some_progress)))
        return NULL;

    // 回收了一些页 → 重试分配
retry:
    page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

    if (!page && !(gfp_mask & __GFP_NORETRY))
        goto retry;  // 允许重试，继续循环

    return page;
}
```

### 6.2 循环重试策略

```
循环计数器: no_progress_loops
compaction_retries: 最多 COMPACT_MAX_DEFERRED (6次)
MAX_RECLAIM_RETRIES: 16

每次循环:
  ① 尝试分配                      ── 成功 → 返回
  ② 尝试 compaction               ── 成功 → 返回
  ③ 尝试 direct reclaim           ── 未进展 → no_progress_loops++
  ④ should_compact_retry()         ── 是 → 继续
  ⑤ should_reclaim_retry()         ── 是 → 继续
  ⑥ 否 → goto nopage (返回 NULL)
```

---

## 7. 触发 OOM 的条件

当慢速路径所有尝试失败后：
```c
nopage:
    // __alloc_pages_may_oom()
    //   → out_of_memory() → oom_kill_process()
```

触发 OOM 的前置条件（在慢速路径中逐步升级）：
```
ALLOC_WMARK_LOW  失败
  → ALLOC_WMARK_MIN  失败
  → direct_compact   失败
  → direct_reclaim   失败 (no progress)
  → should_compact_retry() = false
  → should_reclaim_retry() = false
  → __alloc_pages_may_oom()  → 如果没有 OOM 受害者正在退出
  → out_of_memory() → 选择并杀死进程
```

---

## 8. 完整分配路径总结

```
alloc_pages(gfp, order)
  │
  ▼
__alloc_frozen_pages_noprof()
  │
  ├─[Fast Path]─────────────────────────────────────────
  │  alloc_flags = ALLOC_WMARK_LOW
  │  │
  │  ▼
  │  get_page_from_freelist()
  │    │ 遍历 zonelist 中的每个 zone
  │    │ 检查 zone_watermark_fast()
  │    │
  │    ▼
  │    rmqueue()
  │      │  order 小且 pcp_allowed → rmqueue_pcplist()
  │      │    └─ __rmqueue_pcplist() 从 CPU 本地缓存取
  │      │
  │      └─ order 大或 PCP 空 → rmqueue_buddy()
  │           └─ __rmqueue_smallest()
  │                从 free_area 找合适阶，expand 分裂
  │
  └─[Slow Path]─────────────────────────────────────────
     __alloc_pages_slowpath()
       │
       ├─ wake kswapd
       ├─ 降低水位 ALLOC_WMARK_MIN → 再试分配
       ├─ compaction
       ├─ direct reclaim
       ├─ 循环重试 (最多16次)
       └─ OOM killer
```

---

## 9. 伙伴分配器中的伙伴关系验证

内核在释放页时会验证 PFN 伙伴关系，核心逻辑：

```c
// 检查两个 page 是否为伙伴关系
static inline bool page_is_buddy(struct page *page, struct page *buddy,
                                  unsigned int order)
{
    // 1. buddy 必须是空闲页 (PageBuddy)
    // 2. buddy 必须属于同一 order
    // 3. buddy 必须在同一 zone
    // 4. buddy 的 PFN = page_pfn ^ (1 << order)
}
```

伙伴分裂与合并保证了分配后的内部碎片最小化，同时释放时尽可能将小块合并为大块。

---

## 10. 关键配置与调试

### /proc 接口

```bash
# 查看伙伴分配器状态
cat /proc/buddyinfo
Node 0, zone   Normal   1203   876   432   201    98    42    18     5    1

# 解读：每列是一个 order 的空闲块数量
# order 0: 1203 个单页
# order 1: 876  个连续2页块
# order 2: 432  个连续4页块
# ...
# order 9: 1    个连续512页块（2MB）
```

```bash
# 查看水位线
cat /proc/zoneinfo | grep -A5 "pages free"
```

### 常见 GFP 标志组合

| GFP 标志 | 含义 | 能否睡眠 |
|----------|------|---------|
| GFP_KERNEL | 内核常规分配 | 可以 |
| GFP_ATOMIC | 中断/原子上下文 | 不可以 |
| GFP_NOWAIT | 不等待 | 不可以 |
| GFP_NOFS | 禁止文件系统递归 | 可以 |
| GFP_NOIO | 禁止 I/O 递归 | 可以 |
| __GFP_DIRECT_RECLAIM | 允许直接回收 | 可以 |
| __GFP_NOFAIL | 不允许失败（危险） | 可以（无限重试） |

---

## 下一篇文章

[第二篇：SLUB 分配器 — sheaf 缓存、slab freelist 与 kmalloc](./02-slub.md)
