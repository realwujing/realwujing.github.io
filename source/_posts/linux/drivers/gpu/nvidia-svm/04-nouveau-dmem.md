---
title: '第4篇：NVIDIA 设备显存管理 — nouveau_dmem.c'
date: '2026/05/21 12:01:44'
updated: '2026/05/21 12:01:44'
---

# 第4篇：NVIDIA 设备显存管理 — nouveau_dmem.c

> 源码：`drivers/gpu/drm/nouveau/nouveau_dmem.c` | 头文件：`drivers/gpu/drm/nouveau/nouveau_dmem.h`  
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](https://github.com/realwujing/realwujing.github.io/blob/main/linux/drivers/gpu/nvidia-svm/README.md)

## 1. 本篇位置

上一篇 `nouveau_svm.c` 通过 `hmm_range_fault()` 镜像了 CPU 页表——但 GPU 故障时，如果对应物理页在系统内存，直接映射即可。问题是：**GPU 想把自己的显存 (VRAM) 也纳入统一内存系统**。

这就是 `nouveau_dmem.c` 的职责：将 GPU VRAM 暴露为 Linux 内核的 `DEVICE_PRIVATE` 内存，让内核像管理普通内存一样管理显存——缺页时自动迁移、空闲时回收。

```
应用: cudaMallocManaged(p, size) → GPU 访问 p[0]
    ↓
nouveau_svm.c: hmm_range_fault()
    ├─ 页在系统内存 → 映射系统物理地址
    └─ 页在 VRAM?    → 查 ZONE_DEVICE page → nouveau_dmem_page_addr()
    ↓
CPU 访问 p[0] (页在 VRAM)
    ↓
CPU 缺页 → ZONE_DEVICE → .migrate_to_ram = nouveau_dmem_migrate_to_ram()
    ↓                                                 ← 本篇核心!
VRAM → DMA copy → RAM → 重新映射 PTE
```

## 2. 核心数据结构

### 2.1 `struct nouveau_dmem` — 设备内存管理器

```c
// nouveau_dmem.c:81-89
struct nouveau_dmem {
    struct nouveau_drm *drm;
    struct nouveau_dmem_migrate migrate;   // DMA 迁移引擎
    struct list_head chunks;               // VRAM chunk 链表
    struct mutex mutex;                    // chunk 列表锁
    struct page *free_pages;               // 空闲单页链表头
    struct folio *free_folios;             // 空闲大页链表头
    spinlock_t lock;                       // 空闲列表锁
};
```

每个 GPU 设备有且仅有一个 `nouveau_dmem` 实例，管理该设备所有用于 SVM 的 VRAM 块。

### 2.2 `struct nouveau_dmem_chunk` — VRAM 块

```c
// nouveau_dmem.c:67-73
struct nouveau_dmem_chunk {
    struct list_head list;                 // 链接到 dmem->chunks
    struct nouveau_bo *bo;                 // TTM BO（固定的 VRAM 分配）
    struct nouveau_drm *drm;
    unsigned long callocated;              // 已分配的页数
    struct dev_pagemap pagemap;            // ZONE_DEVICE 的 pagemap
};
```

每个 chunk 代表一块连续 VRAM。关键设计：
- `bo` 是一个 **pinned** TTM Buffer Object，保证 VRAM 不会被驱逐
- `pagemap` 将这块 VRAM 注册为 ZONE_DEVICE 内存
- chunk 大小 = `DMEM_CHUNK_SIZE * NR_CHUNKS` = **`2MB × 128 = 256MB`**

```c
// nouveau_dmem.c:51-53
#define DMEM_CHUNK_SIZE    (2UL << 20)    // 2MB
#define DMEM_CHUNK_NPAGES  (DMEM_CHUNK_SIZE >> PAGE_SHIFT)  // 512 pages
#define NR_CHUNKS          (128)          // 128 个子块
```

### 2.3 `struct nouveau_dmem_migrate` — 迁移引擎

```c
// nouveau_dmem.c:75-79
struct nouveau_dmem_migrate {
    nouveau_migrate_copy_t copy_func;     // DMA 拷贝函数
    nouveau_clear_page_t clear_func;      // DMA 清零函数
    struct nouveau_channel *chan;         // GPU 命令通道
};
```

拷贝/清零函数指针（line 61-65）：

```c
typedef int (*nouveau_migrate_copy_t)(struct nouveau_drm *drm, u64 npages,
    enum nouveau_aper dst_aper, u64 dst_addr,
    enum nouveau_aper src_aper, u64 src_addr);
typedef int (*nouveau_clear_page_t)(struct nouveau_drm *drm, u32 length,
    enum nouveau_aper dst_aper, u64 dst_addr);
```

地址空间枚举（line 55-59）：

```c
enum nouveau_aper {
    NOUVEAU_APER_VIRT,   // 虚拟地址
    NOUVEAU_APER_VRAM,   // 显存
    NOUVEAU_APER_HOST,   // 系统内存
};
```

## 3. 初始化 — `nouveau_dmem_init`

```c
// nouveau_dmem.c:702-725
void nouveau_dmem_init(struct nouveau_drm *drm)
{
    // 仅 Pascal 及以上支持
    if (drm->client.device.info.family < NV_DEVICE_INFO_V0_PASCAL)
        return;

    drm->dmem = kzalloc_obj(*drm->dmem);    // 分配 dmem 结构
    drm->dmem->drm = drm;
    mutex_init(&drm->dmem->mutex);
    INIT_LIST_HEAD(&drm->dmem->chunks);
    spin_lock_init(&drm->dmem->lock);

    // 初始化迁移引擎
    ret = nouveau_dmem_migrate_init(drm);
}
```

`nouveau_dmem_migrate_init`（line 684-699）根据 GPU 代选择 DMA 拷贝引擎：

```c
// nouveau_dmem.c:684-699
static int nouveau_dmem_migrate_init(struct nouveau_drm *drm)
{
    switch (drm->ttm.copy.oclass) {
    case PASCAL_DMA_COPY_A:
    case PASCAL_DMA_COPY_B:
    case  VOLTA_DMA_COPY_A:
    case TURING_DMA_COPY_A:
        drm->dmem->migrate.copy_func = nvc0b5_migrate_copy;
        drm->dmem->migrate.clear_func = nvc0b5_migrate_clear;
        drm->dmem->migrate.chan = drm->ttm.chan;
        return 0;
    }
    return -ENODEV;
}
```

注意：初始化时只设置了函数指针，**没有预先分配 VRAM**。VRAM 按需分配（lazy allocation），在第一次迁移请求时才调用 `nouveau_dmem_chunk_alloc`。

## 4. VRAM 分配 — `nouveau_dmem_chunk_alloc`

```c
// nouveau_dmem.c:296-391
static int
nouveau_dmem_chunk_alloc(struct nouveau_drm *drm, struct page **ppage,
                         bool is_large)
```

这个函数是 ZONE_DEVICE 化的核心，流程如下：

### 4.1 分配物理地址空间

```c
// line 313-318
res = request_free_mem_region(&iomem_resource, DMEM_CHUNK_SIZE * NR_CHUNKS,
                              "nouveau_dmem");
```

关键！VRAM 的物理地址不是"真实的系统物理地址"，而是在 `iomem_resource` 中分配的**虚拟物理地址区间**。这是 `DEVICE_PRIVATE` 的设计——这些地址永远不会有真实的系统内存映射，它们只是一个"占位符"，用于创建 `struct page` 结构体。

### 4.2 设置 pagemap

```c
// line 320-326
chunk->drm = drm;
chunk->pagemap.type = MEMORY_DEVICE_PRIVATE;  // ← 标记为 DEVICE_PRIVATE
chunk->pagemap.range.start = res->start;
chunk->pagemap.range.end = res->end;
chunk->pagemap.nr_range = 1;
chunk->pagemap.ops = &nouveau_dmem_pagemap_ops; // ← 回调函数集
chunk->pagemap.owner = drm->dev;
```

`MEMORY_DEVICE_PRIVATE` 是关键类型。内核的 MM 子系统会特殊对待这种内存——CPU 无法直接访问，缺页时调用 `.migrate_to_ram` 回调。

### 4.3 分配真实 VRAM

```c
// line 328-331
ret = nouveau_bo_new_pin(&drm->client, NOUVEAU_GEM_DOMAIN_VRAM,
                         DMEM_CHUNK_SIZE, &chunk->bo);
```

通过 TTM 在 VRAM 中分配一个 **pinned** BO（2MB）。pinned 意味着这个 BO 永远不会被 TTM 驱逐。

### 4.4 创建 struct pages

```c
// line 333
ptr = memremap_pages(&chunk->pagemap, numa_node_id());
```

`memremap_pages()` 是为物理地址区间批量创建 `struct page` 的神奇函数。它使用 `pagemap` 中的 ops 回调集合来管理这些页的生命周期。

### 4.5 初始化空闲列表

```c
// line 343-375
pfn_first = chunk->pagemap.range.start >> PAGE_SHIFT;
page = pfn_to_page(pfn_first);

for (i = 0; i < NR_CHUNKS; i++) {
    if (!IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE) || !is_large) {
        // 512 个 4K 单页
        for (j = 0; j < DMEM_CHUNK_NPAGES - 1; j++, pfn++) {
            page = pfn_to_page(pfn);
            page->zone_device_data = drm->dmem->free_pages;  // 链到空闲列表头
            drm->dmem->free_pages = page;
        }
    } else {
        // 1 个 2MB 大页 (folio)
        page = pfn_to_page(pfn);
        page->zone_device_data = drm->dmem->free_folios;
        drm->dmem->free_folios = page_folio(page);
        pfn += DMEM_CHUNK_NPAGES;
    }
}
```

空闲页通过 `zone_device_data` 字段形成单向链表——这是一个精巧的复用设计，无需额外分配链表结构。支持两种分配粒度：
- **单页模式**：512 个独立的 4K 页
- **大页模式**：1 个 2MB folio（当 `CONFIG_TRANSPARENT_HUGEPAGE` 启用时）

### 4.6 Chunk 设计的物理意义

```
┌──────────────────────────────────────────────────────┐
│                  nouveau_dmem                         │
│                                                       │
│  chunk[0]: 256MB VRAM 块                              │
│  ┌─────────────────────────────────────────────────┐ │
│  │ iomem_resource [0x???, 0x???+256MB)             │ │
│  │           ↕ (虚拟物理地址 ↔ VRAM 偏移)           │ │
│  │ nouveau_bo (2MB pinned VRAM)                     │ │
│  │           → 128 个子块，每块 2MB                 │ │
│  │  ┌────┐ ┌────┐ ┌────┐     ┌────┐               │ │
│  │  │2MB │ │2MB │ │2MB │ ... │2MB │               │ │
│  │  │folio│ │512 │ │512 │     │512 │               │ │
│  │  │    │ │4K页│ │4K页│     │4K页│               │ │
│  │  └────┘ └────┘ └────┘     └────┘               │ │
│  └─────────────────────────────────────────────────┘ │
│                                                       │
│  chunk[1]: 256MB VRAM 块 (按需分配)                   │
│  ...                                                  │
└──────────────────────────────────────────────────────┘
```

## 5. VRAM→RAM 迁移 — `nouveau_dmem_migrate_to_ram`

```c
// nouveau_dmem.c:183-278
static vm_fault_t nouveau_dmem_migrate_to_ram(struct vm_fault *vmf)
```

这是 CPU 缺页处理的核心——当 CPU 访问一个在 VRAM 中的页时：

```
CPU 访问地址 A
    ↓
缺页处理程序查找页表 → PTE 指向 ZONE_DEVICE 页
    ↓
内核调用 dev_pagemap_ops->migrate_to_ram
    = nouveau_dmem_migrate_to_ram(vmf)
    ↓
```

内部流程：

```c
// 第1步: 准备 migrate_vma
struct migrate_vma args = {
    .vma          = vmf->vma,
    .pgmap_owner  = drm->dev,
    .fault_page   = vmf->page,
    .flags        = MIGRATE_VMA_SELECT_DEVICE_PRIVATE |
                    MIGRATE_VMA_SELECT_COMPOUND,
};

// 第2步: 确定页大小 (支持大页)
sfolio = page_folio(vmf->page);
order = folio_order(sfolio);
nr = 1 << order;

// 如果 pte 级别不是大页，降级为单页
if (vmf->pte) {
    order = 0;
    nr = 1;
}

// 第3步: migrate_vma_setup() — 准备迁移
args.start = ALIGN_DOWN(vmf->address, (PAGE_SIZE << order));
args.end = args.start + (PAGE_SIZE << order);
args.src = kcalloc(nr, sizeof(*args.src), GFP_KERNEL);
args.dst = kcalloc(nr, sizeof(*args.dst), GFP_KERNEL);
migrate_vma_setup(&args);
// → args.cpages > 0 表示有页需要迁移

// 第4步: 在系统内存分配目标页
if (order)
    dpage = folio_page(vma_alloc_folio(GFP_HIGHUSER | __GFP_ZERO,
                order, vmf->vma, vmf->address), 0);
else
    dpage = alloc_page_vma(GFP_HIGHUSER | __GFP_ZERO, vmf->vma, vmf->address);

args.dst[0] = migrate_pfn(page_to_pfn(dpage));
if (order)
    args.dst[0] |= MIGRATE_PFN_COMPOUND;

// 第5步: DMA 拷贝 — VRAM → RAM
svmm = folio_zone_device_data(sfolio);
mutex_lock(&svmm->mutex);
nouveau_svmm_invalidate(svmm, args.start, args.end);     // 失效 GPU 页表
nouveau_dmem_copy_folio(drm, sfolio, dfolio, &dma_info); // ← DMA COPY!
mutex_unlock(&svmm->mutex);

// 第6步: 提交迁移
nouveau_fence_new(&fence, dmem->migrate.chan);
migrate_vma_pages(&args);      // 更新 PTE，让 CPU 指向新 RAM 页
nouveau_dmem_fence_done(&fence); // 等待 GPU DMA 完成
dma_unmap_page(drm->dev->dev, dma_info.dma_addr, PAGE_SIZE, DMA_BIDIRECTIONAL);

// 第7步: 最终化
migrate_vma_finalize(&args);   // 释放源页（放回 VRAM 空闲列表）
```

### 5.1 DMA 拷贝细节 — `nouveau_dmem_copy_folio`

```c
// nouveau_dmem.c:155-181
static int nouveau_dmem_copy_folio(struct nouveau_drm *drm,
    struct folio *sfolio, struct folio *dfolio,
    struct nouveau_dmem_dma_info *dma_info)
{
    struct device *dev = drm->dev->dev;
    struct page *dpage = folio_page(dfolio, 0);
    struct page *spage = folio_page(sfolio, 0);

    folio_lock(dfolio);

    // 对目标页做 DMA 映射（目标 = HOST 内存）
    dma_info->dma_addr = dma_map_page(dev, dpage, 0, page_size(dpage),
                                      DMA_BIDIRECTIONAL);

    // 调用 GPU DMA 拷贝引擎：VRAM → HOST
    drm->dmem->migrate.copy_func(drm, folio_nr_pages(sfolio),
        NOUVEAU_APER_HOST, dma_info->dma_addr,   // 目标 = 系统内存
        NOUVEAU_APER_VRAM,                        // 源 = VRAM
        nouveau_dmem_page_addr(spage));           // VRAM 物理地址

    return 0;
}
```

注意拷贝方向：**源是 VRAM，目标是系统内存**。`copy_func` 是 GPU 的 DMA 拷贝引擎（`nvc0b5_migrate_copy`），方向编码为 `NOUVEAU_APER_VRAM` → `NOUVEAU_APER_HOST`。

### 5.2 `nouveau_dmem_page_addr` — 从 page 获取 VRAM 地址

```c
// nouveau_dmem.c:109-116
unsigned long nouveau_dmem_page_addr(struct page *page)
{
    struct nouveau_dmem_chunk *chunk = nouveau_page_to_chunk(page);
    unsigned long off = (page_to_pfn(page) << PAGE_SHIFT) -
                        chunk->pagemap.range.start;
    return chunk->bo->offset + off;
}
```

```
page → chunk (via page_pgmap → container_of → chunk)
off  = page 的 iomem PFN - chunk 的 iomem 起始
VRAM 地址 = BO 的 VRAM 偏移 + off
```

## 6. RAM→VRAM 迁移 — `nouveau_dmem_migrate_vma`

```c
// nouveau_dmem.c:821-893
int
nouveau_dmem_migrate_vma(struct nouveau_drm *drm,
                         struct nouveau_svmm *svmm,
                         struct vm_area_struct *vma,
                         unsigned long start,
                         unsigned long end)
```

这是反向迁移——将系统内存迁移到 VRAM。由用户空间 ioctl 触发（`nouveau_svmm_bind`）或内核迁移策略触发。

```
// MIGRATE_VMA_SELECT_SYSTEM → 选择系统内存页迁移
struct migrate_vma args = {
    .vma          = vma,
    .start        = start,
    .pgmap_owner  = drm->dev,
    .flags        = MIGRATE_VMA_SELECT_SYSTEM |
                    MIGRATE_VMA_SELECT_COMPOUND,
};

// 批量迁移，每次最多 HPAGE_PMD_NR（512）页
if (IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE))
    if (max > (unsigned long)HPAGE_PMD_NR)
        max = (unsigned long)HPAGE_PMD_NR;

for (i = 0; i < npages; i += max) {
    migrate_vma_setup(&args);
    if (args.cpages)
        nouveau_dmem_migrate_chunk(drm, svmm, &args, dma_info, pfns);
    args.start = args.end;
}
```

### 6.1 逐页拷贝 — `nouveau_dmem_migrate_copy_one`

```c
// nouveau_dmem.c:727-781
static unsigned long nouveau_dmem_migrate_copy_one(struct nouveau_drm *drm,
    struct nouveau_svmm *svmm, unsigned long src,
    struct nouveau_dmem_dma_info *dma_info, u64 *pfn)
{
    spage = migrate_pfn_to_page(src);     // 源：系统内存页

    // 在 VRAM 中分配目标页
    dpage = nouveau_dmem_page_alloc_locked(drm, is_large);
    paddr = nouveau_dmem_page_addr(dpage);

    if (spage) {
        // 有数据：DMA 拷贝 HOST → VRAM
        dma_map_page(dev, spage, 0, page_size(spage), DMA_BIDIRECTIONAL);
        drm->dmem->migrate.copy_func(drm, ...,
            NOUVEAU_APER_VRAM, paddr,          // 目标 = VRAM
            NOUVEAU_APER_HOST, dma_info->dma_addr); // 源 = 系统内存
    } else {
        // 无数据（零页）：DMA 清零 VRAM
        drm->dmem->migrate.clear_func(drm, page_size(dpage),
            NOUVEAU_APER_VRAM, paddr);
    }

    // 设置 zone_device_data 指向 svmm，用于后续迁移回 RAM 时查找
    dpage->zone_device_data = svmm;

    // 构造 GPU PFN（包含 VRAM 地址）
    *pfn = NVIF_VMM_PFNMAP_V0_V | NVIF_VMM_PFNMAP_V0_VRAM |
           ((paddr >> PAGE_SHIFT) << NVIF_VMM_PFNMAP_V0_ADDR_SHIFT);
    if (src & MIGRATE_PFN_WRITE)
        *pfn |= NVIF_VMM_PFNMAP_V0_W;

    return migrate_pfn(page_to_pfn(dpage));    // 返回 migrate PFN
}
```

注意 `dpage->zone_device_data = svmm`——这个字段在迁移回 RAM 时被 `nouveau_dmem_migrate_to_ram` 用来查找 `svmm` 并失效 GPU 页表。

### 6.2 Chunk 级迁移提交 — `nouveau_dmem_migrate_chunk`

```c
// nouveau_dmem.c:783-818
static void nouveau_dmem_migrate_chunk(struct nouveau_drm *drm,
    struct nouveau_svmm *svmm, struct migrate_vma *args,
    struct nouveau_dmem_dma_info *dma_info, u64 *pfns)
{
    // 逐个拷贝页
    for (i = 0; addr < args->end; ) {
        args->dst[i] = nouveau_dmem_migrate_copy_one(drm, svmm,
            args->src[i], dma_info + nr_dma, pfns + i);
        // 跳过失败和零页
        if (!args->dst[i]) { i++; addr += PAGE_SIZE; continue; }
        if (!dma_mapping_error(..., dma_info[nr_dma].dma_addr))
            nr_dma++;
        // 大页跳过多个 PFN
        folio = page_folio(migrate_pfn_to_page(args->dst[i]));
        order = folio_order(folio);
        i += 1 << order;
        addr += (1 << order) * PAGE_SIZE;
    }

    // 提交迁移
    nouveau_fence_new(&fence, drm->dmem->migrate.chan);
    migrate_vma_pages(args);          // 更新 PTE，让源页指向 VRAM
    nouveau_dmem_fence_done(&fence);  // 等待 GPU DMA

    // 映射 GPU 页表 (关键!)
    nouveau_pfns_map(svmm, args->vma->vm_mm, args->start, pfns, i, order);

    // 清理 DMA 映射
    while (nr_dma--)
        dma_unmap_page(...);

    migrate_vma_finalize(args);
}
```

注意 `nouveau_pfns_map` 调用——迁移完成后，需要立即更新 GPU 页表，让 GPU 知道这些页现在在 VRAM 中。

## 7. DMA 拷贝引擎 — `nvc0b5_migrate_copy`

```c
// nouveau_dmem.c:553-625
static int
nvc0b5_migrate_copy(struct nouveau_drm *drm, u64 npages,
    enum nouveau_aper dst_aper, u64 dst_addr,
    enum nouveau_aper src_aper, u64 src_addr)
```

这是 GPU DMA 拷贝引擎的命令构造——通过 `nvif_push` 写入 GPU 命令流：

```
1. SET_SRC_PHYS_MODE: LOCAL_FB (VRAM) 或 COHERENT_SYSMEM (系统内存)
2. SET_DST_PHYS_MODE: LOCAL_FB 或 COHERENT_SYSMEM
3. OFFSET_IN:  源地址 (64位)
4. OFFSET_OUT: 目标地址 (64位)
5. PITCH_IN/PITCH_OUT: PAGE_SIZE
6. LINE_LENGTH_IN: PAGE_SIZE
7. LINE_COUNT: npages (行数)
8. LAUNCH_DMA: 
   - DATA_TRANSFER_TYPE: NON_PIPELINED
   - SRC_TYPE/DST_TYPE: PHYSICAL
   - FLUSH_ENABLE: TRUE
   - MULTI_LINE_ENABLE: TRUE
   - SRC/DST_MEMORY_LAYOUT: PITCH
```

GPU 收到 `LAUNCH_DMA` 命令后，硬件 DMA 引擎执行 VRAM←→系统内存的实际数据搬移。

`nvc0b5_migrate_clear`（line 627-681）用于清零 VRAM（使用 REMAP 功能，将目标映射为常量 0）。

## 8. Chunk 驱逐 — `nouveau_dmem_evict_chunk`

```c
// nouveau_dmem.c:474-527
static void
nouveau_dmem_evict_chunk(struct nouveau_dmem_chunk *chunk)
{
    // 在设备关闭时，将所有仍映射的 VRAM 页迁回系统内存
    migrate_device_range(src_pfns, chunk->pagemap.range.start >> PAGE_SHIFT,
                         npages);

    for (i = 0; i < npages; i++) {
        if (src_pfns[i] & MIGRATE_PFN_MIGRATE) {
            // 分配系统内存目标页
            dpage = alloc_page(GFP_HIGHUSER | __GFP_NOFAIL);
            dst_pfns[i] = migrate_pfn(page_to_pfn(dpage));

            // DMA 拷贝 VRAM → RAM
            nouveau_dmem_copy_folio(chunk->drm,
                page_folio(migrate_pfn_to_page(src_pfns[i])),
                page_folio(dpage), &dma_info[i]);
        }
    }

    migrate_device_pages(src_pfns, dst_pfns, npages);
    migrate_device_finalize(src_pfns, dst_pfns, npages);
}
```

与常规的 `migrate_vma_*` 不同，驱逐使用 `migrate_device_*` API——因为此时没有进程上下文（设备正在关闭）。

## 9. pagemap ops — 生命周期回调

```c
// nouveau_dmem.c:289-293
static const struct dev_pagemap_ops nouveau_dmem_pagemap_ops = {
    .folio_free      = nouveau_dmem_folio_free,      // 页释放
    .migrate_to_ram  = nouveau_dmem_migrate_to_ram,  // 迁移回 RAM
    .folio_split     = nouveau_dmem_folio_split,      // 大页分裂
};
```

- `folio_free`（line 118-140）：页被释放时，放回 chunk 的空闲链表。支持大页和单页两种路径
- `migrate_to_ram`（line 183-278）：CPU 缺页时自动调用，完成 VRAM→RAM 迁移
- `folio_split`（line 280-287）：大页分裂时，子页继承 `pgmap` 和 `zone_device_data`

## 10. 完整迁移状态机

```
                CPU 访问                      GPU 访问
                   │                             │
           ┌───────▼────────┐          ┌────────▼────────┐
           │ 页在 VRAM 吗?   │          │ 页在 VRAM 吗?    │
           └───────┬────────┘          └────────┬────────┘
                   │ YES                         │ YES
                   ▼                             ▼
     ┌─────────────────────────┐    ┌──────────────────────────┐
     │ nouveau_dmem_           │    │ 直接访问 VRAM             │
     │ migrate_to_ram()        │    │ (通过 GPU 页表映射)       │
     │ ├─ DMA copy VRAM→RAM    │    └──────────────────────────┘
     │ ├─ migrate_vma_pages()  │
     │ └─ PTE → RAM            │
     └───────────┬─────────────┘
                 │                             NO
                 ▼                             ▼
        页现在在 RAM 中                 ┌──────────────────────────┐
                                       │ nouveau_svm.c:           │
                                       │ hmm_range_fault()        │
                                       │ ├─ 页在 RAM → 映射        │
                                       │ └─ 页在 VRAM →           │
                                       │    nouveau_dmem_page_addr│
                                       └──────────────────────────┘

                 用户: cudaMemPrefetchAsync(p, size, GPU)
                         ↓
            ┌────────────────────────────┐
            │ nouveau_dmem_migrate_vma() │
            │ ├─ migrate_vma_setup()      │
            │ ├─ DMA copy RAM→VRAM        │
            │ ├─ migrate_vma_pages()      │
            │ ├─ nouveau_pfns_map()       │ ← 更新 GPU 页表
            │ └─ 页现在在 VRAM 中         │
            └────────────────────────────┘
```

## 11. 总结

`nouveau_dmem.c` 实现了 GPU 显存的 ZONE_DEVICE 化，核心要点：

1. **DEVICE_PRIVATE 内存**：VRAM 通过 `memremap_pages(MEMORY_DEVICE_PRIVATE)` 获得 `struct page`，但 CPU 不可直接访问
2. **缺页自动迁移**：CPU 缺页 → `nouveau_dmem_migrate_to_ram` → DMA 拷贝 → 重新映射
3. **主动迁移**：用户/驱动调用 `nouveau_dmem_migrate_vma` → DMA 拷贝 → 更新 GPU 页表
4. **DMA 引擎**：`nvc0b5_migrate_copy/clear` 通过 GPU push buffer 命令 DMA 引擎搬数据
5. **Chunk 管理**：256MB 的 chunk，内部 128 个 2MB 子块，按需分配，空闲链表回收

下一篇文章将深入 TTM 框架，看 GPU 如何在 VRAM、GTT、SYSTEM 三种内存类型之间管理 BO placement。

## 下一篇文章

[第5篇：GPU 多内存类型管理：TTM 框架](05-ttm.md)