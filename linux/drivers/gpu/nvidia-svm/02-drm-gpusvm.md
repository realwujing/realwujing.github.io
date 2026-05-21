# 第2篇：GPU 共享虚拟内存抽象层 — DRM GPUSVM

> 源码：`drivers/gpu/drm/drm_gpusvm.c` | 头文件：`include/drm/drm_gpusvm.h`  
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. GPUSVM 是什么

在上一篇中，我们深入了 HMM（`mm/hmm.c`），它提供了 `hmm_range_fault()` 来镜像 CPU 页表。但 HMM 只是一个底层工具——它不知道"GPU"的概念，不知道 GPU 故障、DMA 映射、设备内存迁移。

**DRM GPUSVM** 是 DRM 框架在 HMM 之上构建的 GPU SVM 抽象层。它由 Intel 的 Matthew Brost 于 2024 年贡献（`drivers/gpu/drm/drm_gpusvm.c:1-7`），为所有 DRM GPU 驱动提供统一的 SVM 框架，负责：

- **Notifier 管理**：将 `mmu_interval_notifier` 包装为 GPU SVM Notifier，按地址区间跟踪 CPU 页表变更
- **Range 管理**：将 GPU 故障地址映射为"Range"（GPU 视角的页表区域），支持动态创建/销毁
- **DMA 映射**：将 CPU 页的 PFN 数组转换为 GPU 可以理解的 DMA 地址
- **设备内存迁移**：提供与 `dev_pagemap` / `drm_pagemap` 协作的设备内存迁移框架

NVIDIA 内核驱动（`nouveau_svm.c`）就是 GPUSVM 的一个驱动侧调用者。

```
┌─────────────────────────────────────────────────────────┐
│                   用户空间 (CUDA)                         │
│            cudaMallocManaged() / cudaMemPrefetchAsync()  │
├─────────────────────────────────────────────────────────┤
│                DRM 驱动层 (nouveau_svm.c)                 │
│    GPU 页故障处理 ← drm_gpusvm_range_find_or_insert()    │
│    GPU 页表编程   ← drm_gpusvm_range_get_pages()         │
├─────────────────────────────────────────────────────────┤
│              DRM GPUSVM (drm_gpusvm.c)    ← 本篇         │
│    Notifier → Range → Pages (DMA) → Evict/Migrate        │
├─────────────────────────────────────────────────────────┤
│                     HMM (mm/hmm.c)                       │
│               hmm_range_fault()  ← 第1篇                  │
├─────────────────────────────────────────────────────────┤
│                 内存管理子系统 (MM)                        │
│        struct page, PTEs, migrate, ZONE_DEVICE           │
└─────────────────────────────────────────────────────────┘
```

## 2. 核心数据结构

### 2.1 `struct drm_gpusvm` — 整个 SVM 实例

```c
// include/drm/drm_gpusvm.h:202-222
struct drm_gpusvm {
    const char *name;
    struct drm_device *drm;
    struct mm_struct *mm;          // 对应的 CPU 地址空间
    unsigned long mm_start;        // SVM 起始地址
    unsigned long mm_range;        // SVM 范围大小
    unsigned long notifier_size;   // 每个 notifier 的大小 (建议 >= 512M)
    const struct drm_gpusvm_ops *ops;  // 驱动实现的 vtable
    const unsigned long *chunk_sizes;  // Range chunk 大小数组 (降序 2的幂)
    int num_chunks;
    struct rw_semaphore notifier_lock;  // 全局 notifier 读写锁
    struct rb_root_cached root;         // notifier 红黑树根
    struct list_head notifier_list;     // notifier 链表 (快速遍历)
};
```

核心设计：一个 `drm_gpusvm` 对应一个 GPU 进程（一个 `mm_struct`）。notifier_size 控制一个 notifier 覆盖的地址范围（建议 ≥ 512MB），chunk_sizes 控制 GPU 故障处理的粒度。

### 2.2 `struct drm_gpusvm_ops` — 驱动 vtable

```c
// include/drm/drm_gpusvm.h:29-77
struct drm_gpusvm_ops {
    // 可选：自定义 notifier 分配
    struct drm_gpusvm_notifier *(*notifier_alloc)(void);
    void (*notifier_free)(struct drm_gpusvm_notifier *notifier);

    // 可选：自定义 range 分配
    struct drm_gpusvm_range *(*range_alloc)(struct drm_gpusvm *gpusvm);
    void (*range_free)(struct drm_gpusvm_range *range);

    // 必须：驱动侧的 GPU 页表失效回调
    void (*invalidate)(struct drm_gpusvm *gpusvm,
                       struct drm_gpusvm_notifier *notifier,
                       const struct mmu_notifier_range *mmu_range);
};
```

驱动实现 `.invalidate` 回调，当 CPU 页表变化时，GPUSVM 框架调用此函数通知驱动更新 GPU 页表。

### 2.3 `struct drm_gpusvm_notifier` — 页表变化监听

```c
// include/drm/drm_gpusvm.h:96-106
struct drm_gpusvm_notifier {
    struct drm_gpusvm *gpusvm;
    struct mmu_interval_notifier notifier;  // HMM 的 interval notifier
    struct interval_tree_node itree;        // 插入 gpusvm->root 的节点
    struct list_head entry;                 // 链表节点
    struct rb_root_cached root;             // 该 notifier 下的 range RB树
    struct list_head range_list;            // range 链表
    struct { u32 removed : 1; } flags;
};
```

Notifier 是 GPUSVM 的核心监听单元。每个 notifier 覆盖一个地址区间（由 `notifier_size` 决定），内部维护一棵 Range RB 树。当 CPU 页表在该区间内变化时，`mmu_interval_notifier` 回调触发驱动 invalidate。

```
gpusvm->root (RB树)
    │
    ├── notifier [0, 512M]        ← notifier_size = 512M
    │       │
    │       ├── range [0, 2M]      ← chunk_size = 2M
    │       ├── range [2M, 6M]
    │       └── range [8M, 10M]
    │
    └── notifier [512M, 1G]
            │
            └── range [512M, 514M]
```

### 2.4 `struct drm_gpusvm_range` — GPU 页表区域

```c
// include/drm/drm_gpusvm.h:167-174
struct drm_gpusvm_range {
    struct drm_gpusvm *gpusvm;
    struct drm_gpusvm_notifier *notifier;
    struct kref refcount;              // 引用计数
    struct interval_tree_node itree;   // 插入 notifier->root 的节点
    struct list_head entry;
    struct drm_gpusvm_pages pages;     // 该 range 的页映射信息
};
```

Range 是 GPUSVM 的最小管理单元，代表一段已被 GPU 映射的 CPU 虚拟地址区间。每个 GPU 故障对应一个 Range（按 chunk_size 对齐）。

### 2.5 `struct drm_gpusvm_pages` — 页映射

```c
// include/drm/drm_gpusvm.h:147-152
struct drm_gpusvm_pages {
    struct drm_pagemap_addr *dma_addr;  // DMA 地址数组
    struct drm_pagemap *dpagemap;       // 设备页的 drm_pagemap
    unsigned long notifier_seq;         // notifier 序列号
    struct drm_gpusvm_pages_flags flags;
};
```

`dma_addr[]` 是 GPUSVM 的关键创新：它将 HMM 返回的 PFN 数组转换为 GPU 可用的 DMA 地址数组。每个 `dma_addr[i]` 编码了 DMA 地址、互连类型（system/devmem）、order 和方向。

### 2.6 `struct drm_gpusvm_ctx` — 控制上下文

```c
// include/drm/drm_gpusvm.h:244-253
struct drm_gpusvm_ctx {
    void *device_private_page_owner;
    unsigned long check_pages_threshold;
    unsigned long timeslice_ms;
    unsigned int in_notifier :1;
    unsigned int read_only :1;
    unsigned int devmem_possible :1;   // 是否允许迁移到设备内存
    unsigned int devmem_only :1;       // 是否仅使用设备内存
    unsigned int allow_mixed :1;       // 是否允许混合 system/devmem 映射
};
```

驱动通过 `ctx` 控制 GPUSVM 的行为。例如：
- `devmem_possible=1, devmem_only=0`：优先设备内存，但可回退到系统内存
- `devmem_only=1`：必须有设备内存，否则失败
- `read_only=1`：只读映射（DMA_TO_DEVICE）
- `allow_mixed=1`：允许一个 range 内同时有 system 和 device 页

## 3. 核心流程

### 3.1 初始化

```c
// drm_gpusvm.c:383-426
int drm_gpusvm_init(struct drm_gpusvm *gpusvm,
    const char *name, struct drm_device *drm,
    struct mm_struct *mm,
    unsigned long mm_start, unsigned long mm_range,
    unsigned long notifier_size,
    const struct drm_gpusvm_ops *ops,
    const unsigned long *chunk_sizes, int num_chunks)
```

初始化时：
1. 调用 `mmgrab(mm)` 持有 mm_struct 引用（line 394）
2. 初始化 RB 树根 `gpusvm->root = RB_ROOT_CACHED`（line 411）
3. 初始化 notifier 链表（line 412）
4. 初始化 `notifier_lock` 读写信号量（line 414）
5. 注册 lockdep 注释（line 420-422）

支持两种模式：
- **完整 SVM 模式**：提供 mm + ops + chunk_sizes 等全部参数
- **简单 pages API 模式**：只提供 name + drm，仅使用 get/unmap/free 页面操作

### 3.2 GPU 故障处理 — `drm_gpusvm_range_find_or_insert`

当 GPU 访问一个不在 TLB 中的虚拟地址时，驱动调用这个函数来查找或创建对应的 Range：

```c
// drm_gpusvm.c:1013-1117
struct drm_gpusvm_range *
drm_gpusvm_range_find_or_insert(struct drm_gpusvm *gpusvm,
    unsigned long fault_addr,
    unsigned long gpuva_start, unsigned long gpuva_end,
    const struct drm_gpusvm_ctx *ctx)
```

核心流程：

```
1. fault_addr 必须落在 [mm_start, mm_start+mm_range] 内
   ↓ (line 1031-1033)
2. mmget_not_zero(mm) — 持有 mm 引用
   ↓ (line 1035)
3. drm_gpusvm_notifier_find() — 查找 fault_addr 所在的 notifier
   ├─ 找到 → line 1068: 直接查找已有 range
   └─ 未找到 → 分配新 notifier, mmu_interval_notifier_insert()
   ↓ (line 1038-1053)
4. vma_lookup(mm, fault_addr) — 查找 VMA
   ├─ 未找到 → -ENOENT
   └─ 检查 VM_WRITE 标志
   ↓ (line 1057-1066)
5. drm_gpusvm_range_find(notifier, fault_addr, fault_addr+1)
   ├─ 找到 → 直接返回已有 range
   └─ 未找到 → 创建新 range
   ↓ (line 1068-1070)
6. 判断是否可迁移到设备内存：
   migrate_devmem = ctx->devmem_possible && vma_is_anonymous(vas)
                    && !is_vm_hugetlb_page(vas)
   ↓ (line 1076-1077)
7. drm_gpusvm_range_chunk_size() — 选择 range 大小
   ├─ 遍历 chunk_sizes[] 从大到小
   ├─ 找到第一个落在 VMA/notifier/gpuva 边界内的对齐区间
   ├─ 如果是非 4K 页，检查是否与已有 range 重叠
   └─ 可选：drm_gpusvm_check_pages() 检查 CPU 是否已 fault 这些页
   ↓ (line 1079-1087)
8. drm_gpusvm_range_alloc() — 分配 range 结构，设置 itree.start/end
   ↓ (line 1089-1094)
9. drm_gpusvm_range_insert() — 插入 notifier->root RB 树
   ↓ (line 1096)
10. drm_gpusvm_notifier_insert() — 如果是新 notifier，插入 gpusvm->root
```

**Range chunk_size 选择的关键**（`drm_gpusvm_range_chunk_size`，line 884-946）：

```
chunk_sizes = [2M, 64K, 4K]

fault_addr = 0x1005000 (约 16.3M)

尝试 2M:  start = ALIGN_DOWN(0x1005000, 2M) = 0x1000000
         end   = ALIGN(0x1005000+1, 2M)     = 0x1200000
         → 在 VMA/notifier/gpuva 边界内吗？
         → 不与已有 range 重叠吗？
  ✓ → 返回 2M

否则尝试 64K:
         start = 0x1000000, end = 0x1010000
  ✓ → 返回 64K

否则尝试 4K:
         start = 0x1005000, end = 0x1006000
  ✓ → 返回 4K

都不行 → 返回 LONG_MAX（错误）
```

### 3.3 获取页映射 — `drm_gpusvm_get_pages`

一旦有了 Range，驱动调用此函数将 CPU 页转换为 GPU 可用的 DMA 地址：

```c
// drm_gpusvm.c:1382-1587
int drm_gpusvm_get_pages(struct drm_gpusvm *gpusvm,
    struct drm_gpusvm_pages *svm_pages,
    struct mm_struct *mm,
    struct mmu_interval_notifier *notifier,
    unsigned long pages_start, unsigned long pages_end,
    const struct drm_gpusvm_ctx *ctx)
```

核心流程：

```
1. hmm_range_fault() — 调用 HMM 获取 PFN 数组
   ↓ (line 1430-1444)
2. 持有 notifier_lock，检查 seqno 是否重试
   ↓ (line 1455-1468)
3. 遍历 hmm_pfns[]，逐个建立 DMA 映射：
   ├─ 设备私有/一致页 → dpagemap->ops->device_map()
   │   检查 allow_mixed, devmem_only 标志
   ├─ 普通页 → dma_map_page() (DMA_BIDIRECTIONAL 或 DMA_TO_DEVICE)
   │   编码为 drm_pagemap_addr_encode(addr, DRM_INTERCONNECT_SYSTEM, ...)
   ↓ (line 1485-1558)
4. 设置 pages.flags (has_dma_mapping, has_devmem_pages)
   ↓ (line 1560-1568)
5. 记录 notifier_seq
   ↓ (line 1573)
```

关键设计：
- DMA 映射在 `notifier_lock` 保护下进行，notifier 回调可以安全地取消映射
- `allow_mixed=0` 时，不允许单个 range 中混合多个 `dpagemap`（line 1494-1498）
- `devmem_only=1` 时，遇到普通页直接返回 `-EFAULT`（line 1537-1540）
- 使用 `hmm_pfn_to_map_order()` 支持大页映射，一次 DMA 映射覆盖多个页（line 1488, 1542-1549）

### 3.4 notifier 回调 — 页表失效

当 CPU 页表变化时（例如 `munmap`、`mprotect` 等），`mmu_interval_notifier` 触发 GPUSVM 的回调：

```c
// drm_gpusvm.c:331-349
static bool
drm_gpusvm_notifier_invalidate(struct mmu_interval_notifier *mni,
    const struct mmu_notifier_range *mmu_range,
    unsigned long cur_seq)
{
    struct drm_gpusvm_notifier *notifier =
        container_of(mni, typeof(*notifier), notifier);
    struct drm_gpusvm *gpusvm = notifier->gpusvm;

    if (!mmu_notifier_range_blockable(mmu_range))
        return false;

    down_write(&gpusvm->notifier_lock);      // 写锁
    mmu_interval_set_seq(mni, cur_seq);       // 设置 seqno
    gpusvm->ops->invalidate(gpusvm, notifier, mmu_range);  // 驱动回调
    up_write(&gpusvm->notifier_lock);

    return true;
}
```

这对应 HMM 文档中的 `driver->update` 锁（`drm_gpusvm.c:96-99`）。notifier 回调持有**写锁**，与 get_pages 的**读锁**互斥——确保 DMA 映射期间页不会被释放。

### 3.5 Page 有效性检查 — `drm_gpusvm_range_pages_valid`

在驱动最终提交 GPU 绑定时，需要做最后一次检查：

```c
// drm_gpusvm.c:1333-1338
bool drm_gpusvm_range_pages_valid(struct drm_gpusvm *gpusvm,
                                  struct drm_gpusvm_range *range)
{
    return drm_gpusvm_pages_valid(gpusvm, &range->pages);
}

// drm_gpusvm.c:1311-1317
static bool drm_gpusvm_pages_valid(struct drm_gpusvm *gpusvm,
                                   struct drm_gpusvm_pages *svm_pages)
{
    lockdep_assert_held(&gpusvm->notifier_lock);
    return svm_pages->flags.has_devmem_pages || svm_pages->flags.has_dma_mapping;
}
```

这是 GPUSVM 的"最后一公里"保护——对每个 Range 做细粒度检查（而非整个 notifier 的 seqno 检查），因为 notifier 覆盖多个 range，只靠 notifier seqno 不够精确。

### 3.6 扫描内存状态 — `drm_gpusvm_scan_mm`

GPUSVM 提供了一个"咨询性"扫描函数，用于判断 Range 对应的物理页当前处于什么迁移状态：

```c
// drm_gpusvm.c:762-865
enum drm_gpusvm_scan_result drm_gpusvm_scan_mm(struct drm_gpusvm_range *range,
    void *dev_private_owner, const struct dev_pagemap *pagemap)
```

扫描结果枚举（`include/drm/drm_gpusvm.h:347-353`）：

```
DRM_GPUSVM_SCAN_UNPOPULATED  — 至少有一个页未 present
DRM_GPUSVM_SCAN_EQUAL        — 所有页都属于 @pagemap
DRM_GPUSVM_SCAN_OTHER        — 所有页都属于另一个 pagemap
DRM_GPUSVM_SCAN_SYSTEM       — 所有页都是系统内存
DRM_GPUSVM_SCAN_MIXED_DEVICE — 混合了多个 dev_pagemap
DRM_GPUSVM_SCAN_MIXED        — 混合了系统页和设备页
```

状态转换逻辑（line 816-857）：

```
遍历所有页:
  if 页 == pagemap:           new_state = EQUAL
  else if 页 == other || !other: new_state = OTHER
  else if 页是 device:         new_state = MIXED_DEVICE
  else (系统页):               new_state = SYSTEM

  合并到 state:
    UNPOPULATED + ANY = ANY
    EQUAL + SYSTEM => MIXED
    EQUAL + other device => MIXED_DEVICE
    SYSTEM + device => MIXED
```

注意：结果可能随时失效——这只是一个**建议性的快照**。

## 4. 驱动使用模式

GPUSVM 文档（`drm_gpusvm.c:128-253`）给出了三个驱动组件示例：

### 4.1 GPU 页故障处理流程

```c
// 伪代码，体现核心循环
int driver_gpu_fault(struct drm_gpusvm *gpusvm, unsigned long fault_addr,
                     unsigned long gpuva_start, unsigned long gpuva_end)
{
    struct drm_gpusvm_ctx ctx = {};
    // ...
retry:
    driver_garbage_collector(gpusvm);  // 先处理 pending UNMAP

    range = drm_gpusvm_range_find_or_insert(gpusvm, fault_addr,
        gpuva_start, gpuva_end, &ctx);

    err = drm_gpusvm_range_get_pages(gpusvm, range, &ctx);
    // -EOPNOTSUPP → evict + retry
    // -EFAULT     → retry
    // -EAGAIN     → driver_bind_range retry

    err = driver_bind_range(gpusvm, range);
    // ...
}
```

核心思想：**循环重试直到页表稳定**。

### 4.2 垃圾回收器

```c
void __driver_garbage_collector(struct drm_gpusvm *gpusvm,
                                struct drm_gpusvm_range *range)
{
    if (range->flags.partial_unmap)  // 部分 unmap
        drm_gpusvm_range_evict(gpusvm, range);  // 迁回 RAM

    driver_unbind_range(range);
    drm_gpusvm_range_remove(gpusvm, range);
}
```

### 4.3 Notifier 回调

```c
void driver_invalidation(struct drm_gpusvm *gpusvm,
                         struct drm_gpusvm_notifier *notifier,
                         const struct mmu_notifier_range *mmu_range)
{
    struct drm_gpusvm_ctx ctx = { .in_notifier = true, };

    drm_gpusvm_for_each_range(range, notifier, mmu_range->start, mmu_range->end) {
        drm_gpusvm_range_unmap_pages(gpusvm, range, &ctx);  // 取消 DMA 映射

        if (mmu_range->event != MMU_NOTIFY_UNMAP)
            continue;

        drm_gpusvm_range_set_unmapped(range, mmu_range);
        driver_garbage_collector_add(gpusvm, range);
    }
}
```

## 5. Locking 架构

```
┌──────────────────────────────────────────────┐
│              driver_svm_lock                   │
│  保护: range_find_or_insert, range_remove,    │
│        garbage_collector                      │
│  类型: 驱动自定义锁 (或 lockdep 注解)          │
├──────────────────────────────────────────────┤
│              gpusvm->notifier_lock             │
│  保护: notifier->root (RB树), range->pages    │
│        (DMA 映射, seqno)                      │
│  get_pages           → down_read              │
│  notifier_invalidate → down_write             │
│  pages_valid/unmap   → 持锁检查                │
├──────────────────────────────────────────────┤
│              mm->mmap_lock                    │
│  hmm_range_fault() → mmap_read_lock(mm)       │
└──────────────────────────────────────────────┘
```

`drm_gpusvm.c:88-108` 文档详细描述了锁定层次。最关键的是 `notifier_lock` 的读写锁设计——DMA 映射期间持 **读锁**，notifier 回调持 **写锁**，确保驱动永远不会在已释放的页上做 DMA 操作。

## 6. 总结

DRM GPUSVM 为 GPU 驱动提供了完整的 SVM 基础设施。它：
- 在 notifier 层面：用 RB 树 + Interval Tree 管理 `mmu_interval_notifier`
- 在 range 层面：按 chunk_size 将故障地址映射为 GPU 页表区域
- 在 pages 层面：将 HMM PFN 转换为 GPU DMA 地址（支持大页/设备页）
- 在迁移层面：通过 `drm_gpusvm_ctx` 控制设备内存迁移策略

下一篇文章将看 NVIDIA 驱动（`nouveau_svm.c`）如何使用这个框架处理真实的 GPU 页故障。

## 下一篇文章

[第3篇：NVIDIA HMM 调用者：nouveau_svm.c](03-nouveau-svm.md)