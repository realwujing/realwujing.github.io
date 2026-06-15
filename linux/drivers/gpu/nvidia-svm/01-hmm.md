# 第 1 篇：CPU↔GPU 内存镜像基础 — HMM 深度解析

> 源码：`mm/hmm.c` | 头文件：`include/linux/hmm.h`  
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. HMM 是什么

HMM（Heterogeneous Memory Management，异构内存管理）是 Linux 内核中让 **CPU 和 GPU/加速器共享同一份页表视图** 的基础设施。

不用 HMM 时：CPU 和 GPU 各有一套独立页表，GPU 显存的数据要拷贝到系统内存才能被 CPU 访问，反之亦然。用 HMM 后：内核在 CPU 缺页时自动把 GPU 显存的数据迁移到系统内存，或在 GPU 缺页时把系统内存的数据镜像过来。用户看到的是一块"统一内存"。

通俗类比：HMM 就是 CPU 和 GPU 之间的"翻译官"——CPU 说"我要访问这块虚拟地址"，HMM 查一下，发现这页实际在 GPU 显存里，就把它迁到系统内存，然后告诉 CPU "现在你可以读了"。

## 2. 核心 API：`hmm_range_fault()`

HMM 对 GPU 驱动暴露的核心函数只有一个：

```c
// mm/hmm.c:659-685
int hmm_range_fault(struct hmm_range *range)
{
    struct hmm_vma_walk hmm_vma_walk = {
        .range = range,
        .last = range->start,
    };

    mmap_assert_locked(mm);  // 必须持 mmap 锁

    do {
        if (mmu_interval_check_retry(range->notifier, range->notifier_seq))
            return -EBUSY;  // 页表变了，调用者需要重试

        ret = walk_page_range(mm, hmm_vma_walk.last, range->end,
                              &hmm_walk_ops, &hmm_vma_walk);
        // -EBUSY: 有部分页需要 fault，继续走
        // 0: 全部完成
        // -EFAULT/-EPERM/etc: 错误
    } while (ret == -EBUSY);

    return ret;
}
```

调用者（如 `nouveau_svm.c`）传入一个 `hmm_range`，HMM 走一遍 CPU 页表（通过 `walk_page_range`），把结果填到 `range->hmm_pfns[]` 数组里。每个数组元素是一个 PFN + 标志位，GPU 驱动据此建立自己的 GPU 页表。

## 3. `hmm_range` 数据结构

```c
// include/linux/hmm.h
struct hmm_range {
    struct mmu_interval_notifier *notifier;  // MMU interval notifier
    unsigned long notifier_seq;              // mmu_interval_read_begin() 的返回值
    unsigned long start;                     // 虚拟地址起始（含）
    unsigned long end;                       // 虚拟地址结束（不含）
    unsigned long *hmm_pfns;                 // 输出：PFN 数组
    unsigned long default_flags;             // 范围默认标志
    unsigned long pfn_flags_mask;            // 按页应用的掩码
    void *dev_private_owner;                 // device private page 的 owner
};
```

每个 `hmm_pfns[i]` 的低位存物理页帧号（PFN），高位存标志位：

```c
// include/linux/hmm.h:38-58
enum hmm_pfn_flags {
    HMM_PFN_VALID = 1UL << (BITS_PER_LONG - 1),    // PFN 有效
    HMM_PFN_WRITE = 1UL << (BITS_PER_LONG - 2),    // 页可写
    HMM_PFN_ERROR = 1UL << (BITS_PER_LONG - 3),    // 访问不可能
    HMM_PFN_DMA_MAPPED = 1UL << (BITS_PER_LONG - 4), // 已 DMA 映射
    HMM_PFN_P2PDMA     = 1UL << (BITS_PER_LONG - 5), // P2P 页
    HMM_PFN_P2PDMA_BUS = 1UL << (BITS_PER_LONG - 6), // 总线地址 P2P
    // 输入标志（用于请求 fault）：
    HMM_PFN_REQ_FAULT = HMM_PFN_VALID,              // 请求缺页
    HMM_PFN_REQ_WRITE = HMM_PFN_WRITE,              // 请求可写
};
```

输入和输出共用一个 PFN 数组。输入标志告诉 HMM "我想要什么"，输出标志告诉调用者"实际拿到了什么"。

## 4. walk_page_range — 遍历 CPU 页表

HMM 的核心实现是一个自定义的 `mm_walk_ops`：

```c
// mm/hmm.c:631-638
static const struct mm_walk_ops hmm_walk_ops = {
    .pud_entry      = hmm_vma_walk_pud,      // PUD 级大页
    .pmd_entry      = hmm_vma_walk_pmd,      // PMD 级（主要路径）
    .pte_hole       = hmm_vma_walk_hole,     // 空洞：缺页或无 VMA
    .hugetlb_entry  = hmm_vma_walk_hugetlb_entry, // 大页
    .test_walk      = hmm_vma_walk_test,     // VMA 权限检查
    .walk_lock      = PGWALK_RDLOCK,
};
```

内核的 `walk_page_range` 框架按页表层级回调这些函数。

### 4.1 PMD 级遍历 — 主路径

```c
// mm/hmm.c:396-471
static int hmm_vma_walk_pmd(pmd_t *pmdp, unsigned long start,
                            unsigned long end, struct mm_walk *walk)
{
    pmd = pmdp_get_lockless(pmdp);

    if (pmd_none(pmd))
        return hmm_vma_walk_hole(start, end, -1, walk);  // 空洞

    if (!pmd_present(pmd))
        return hmm_vma_handle_absent_pmd(walk, ...);      // 交换/迁移/设备私有页

    if (pmd_trans_huge(pmd))
        return hmm_vma_handle_pmd(walk, addr, end, hmm_pfns, pmd); // 透明大页

    // 普通页表：逐 PTE 处理
    ptep = pte_offset_map(pmdp, addr);
    for (; addr < end; addr += PAGE_SIZE, ptep++, hmm_pfns++) {
        r = hmm_vma_handle_pte(walk, addr, end, pmdp, ptep, hmm_pfns);
    }
}
```

### 4.2 PTE 级处理 — DEVICE_PRIVATE 页的特殊路径

```c
// mm/hmm.c:235-332
static int hmm_vma_handle_pte(struct mm_walk *walk, ...)
{
    pte = ptep_get(ptep);

    if (pte_none(pte)) {
        required_fault = hmm_pte_need_fault(hmm_vma_walk, pfn_req_flags, 0);
        goto fault;  // 页不存在 → 触发缺页
    }

    if (!pte_present(pte)) {
        // 关键：DEVICE_PRIVATE 页！
        if (softleaf_is_device_private(entry) &&
            page_pgmap(...)->owner == range->dev_private_owner) {
            cpu_flags = HMM_PFN_VALID;
            if (softleaf_is_device_private_write(entry))
                cpu_flags |= HMM_PFN_WRITE;
            new_pfn_flags = softleaf_to_pfn(entry) | cpu_flags;
            goto out;   // 直接返回设备 PFN，不迁移
        }
        // swap / migration 等其他情况 → 等待或报错
    }

    // 正常的 presente 页 → 提取 pfn 和 write 权限
    new_pfn_flags = pte_pfn(pte) | pte_to_hmm_pfn_flags(range, pte);
}
```

**这是 HMM 最核心的逻辑**：遇到 `DEVICE_PRIVATE` 页时，如果 owner 匹配（比如 nouveau 在查自己的显存页），就直接返回设备 PFN，**不触发迁移**。这是为了 GPU 页面迁移时，`migrate_vma` 框架能正确识别"这页在 GPU 上，归属当前驱动"。

## 5. 缺页处理

```c
// mm/hmm.c:73-94
static int hmm_vma_fault(unsigned long addr, unsigned long end,
                         unsigned int required_fault, struct mm_walk *walk)
{
    for (; addr < end; addr += PAGE_SIZE)
        if (handle_mm_fault(vma, addr, fault_flags, NULL) &
            VM_FAULT_ERROR)
            return -EFAULT;
    return -EBUSY;
}
```

逐页调用 `handle_mm_fault()`，让内核的缺页处理机制完成"把 DEVICE_PRIVATE 页迁到 RAM"或"把匿名页填充进来"等操作，然后返回 `-EBUSY` 让外层 `walk_page_range` 重新走一遍（因为页表可能在缺页处理中变了）。

## 6. HMM + DMA 映射

除了基本 PFN 填充，HMM 还提供 DMA 映射支持：

```c
// mm/hmm.c:698-736
int hmm_dma_map_alloc(struct device *dev, struct hmm_dma_map *map, ...);

// mm/hmm.c:771-857
dma_addr_t hmm_dma_map_pfn(struct device *dev, struct hmm_dma_map *map,
                           size_t idx, struct pci_p2pdma_map_state *p2pdma_state);
```

`hmm_dma_map_pfn()` 将 `hmm_range_fault()` 输出的 PFN 映射为 DMA 总线地址。支持三种模式：
- 普通 DMA 映射：`dma_map_phys(dev, paddr, ...)`
- IOVA 映射：`dma_iova_link(dev, state, ...)`
- P2PDMA：`pci_p2pdma_bus_addr_map()`

RDMA 的 `umem_odp.c` 就用这套 API 做按需 DMA 映射（第 8 篇会详讲）。

## 7. 完整调用流程

```
GPU 驱动（nouveau_svm.c）
  │
  ├── mmu_interval_notifier_insert()   ← 注册区间通知器
  │     （CPU 页表变化时收到回调）
  │
  ├── mmu_interval_read_begin()        ← 获取当前页表版本号
  ├── hmm_range_fault(&range)         ← 走 CPU 页表，填 PFN 数组
  │     │
  │     ├── walk_page_range()
  │     │     ├── hmm_vma_walk_pmd()
  │     │     │     ├── hmm_vma_handle_pte()    ← 提取 PFN + 写权限
  │     │     │     ├── hmm_vma_handle_pmd()    ← 透明大页处理
  │     │     │     └── hmm_vma_walk_hole()     ← 缺页
  │     │     │           └── hmm_vma_fault()
  │     │     │                 └── handle_mm_fault() ← 触发内核缺页
  │     │     └── hmm_vma_walk_pud()            ← 1GB 大页
  │     └── -EBUSY → 重走
  │
  ├── 遍历 hmm_pfns[]:
  │     提取 PFN → 写入 GPU 页表（通过 NVIF 命令）
  │     或直接返回 PFN 给用户态
  │
  └── hmm_dma_map_pfn()               ← （可选）建立 DMA 映射
```

## 8. 关键要点

1. **HMM 不管理 GPU 页表** — 它只负责读取 CPU 页表内容，GPU 驱动自己建立 GPU 侧的页表
2. **DEVICE_PRIVATE 是桥梁** — 当数据在 GPU 显存时，CPU 页表里存的是 DEVICE_PRIVATE 条目（不是普通 PTE），HMM 能识别它并直接返回 PFN 给驱动
3. **迁移由 migrate_vma 框架完成** — HMM 只负责"读"，实际的页面迁移（CPU→GPU 或 GPU→CPU）由 `migrate_vma_setup()` + `migrate_vma_pages()` + `migrate_vma_finalize()` 完成（第 4 篇会讲）
4. **`mmu_interval_notifier` 是必须品** — 没有它，HMM 不知道 CPU 侧页表何时被改写了
5. **HMM 是 read-only 的操作** — 它不修改 CPU 页表（缺页除外），只读不变

## 8. 深入：HMM 读取了 CPU 页表然后干什么？

`hmm_range_fault` 本身**只是一个"读"操作**——它遍历 CPU 页表，把物理页帧号（PFN）填回 array。真正让数据流动的，是调用者接下来做的事。

### 8.1 nouveau 的两条路径

GPU 缺页后，`nouveau_svm` 的 work handler 调用 `hmm_range_fault()` 拿到 PFN 数组，然后分两条路走：

**路径 A：数据在系统 DRAM → 直接写 GPU 页表**

```c
// nouveau_svm.c:923-937 — PFN 直接映射到 GPU 页表
void nouveau_pfns_map(struct nouveau_svmm *svmm, struct mm_struct *mm,
                      unsigned long addr, u64 *pfns, unsigned long npages,
                      unsigned int page_shift)
{
    args->p.addr = addr;
    args->p.size = npages << page_shift;
    args->p.page = page_shift;

    // ★ 通过 NVIF 接口写 GPU 内部 MMU 页表
    // GPU 此后可以通过自己的 MMU 直接访问系统 DRAM
    nvif_object_ioctl(&svmm->vmm->vmm.object, args, ...);
}
```

这是最快路径：HMM 拿到的 PFN 就是系统 DRAM 的物理地址，GPU 侧页表直接映射过去。GPU 访问这块内存时走 PCIe 读系统 DRAM。

**路径 B：数据在 VRAM → 先迁移到系统 DRAM**

```c
// nouveau_dmem.c:822 — 把系统页迁到 VRAM
// 或反向: nouveau_dmem_migrate_to_ram 把 VRAM 迁回系统 RAM
int nouveau_dmem_migrate_vma(struct nouveau_drm *drm,
                             struct nouveau_svmm *svmm,
                             struct vm_area_struct *vma,
                             unsigned long start, unsigned long end)
{
    args.pgmap_owner = drm->dev;
    args.flags = MIGRATE_VMA_SELECT_SYSTEM;  // ★ 选择系统页

    migrate_vma_setup(&args);
    // 为每个页面: 锁定 → 在 VRAM 中分配新页 → DMA copy → 更新 PTE 为 DEVICE_PRIVATE
    // 最终 CPU PTE 被替换为 DEVICE_PRIVATE 条目（CPU 无法直接访问）
}
```

### 8.2 HMM 怎么比 CPU MMIO 更高效？

CPU 可以直接通过 PCIe BAR 以 MMIO 方式读写 GPU VRAM。但 HMM 比这个快得多，**不是因为它加速了 MMIO，而是因为它根本不用 MMIO**：

```
MMIO 路径:
  CPU 读 VRAM → PCIe MMIO 读（单次 ~500ns-1µs）→ 每次访问都走 PCIe 往返
  CPU 写 VRAM → PCIe MMIO 写（单次 ~500ns-1µs）→ 同样每次都走
  带宽: ~1-2 GB/s（受限于 CPU 单次 MMIO 事务开销）

HMM + DMA 迁移路径:
  ① GPU DMA 引擎把 VRAM 批量搬到系统 DRAM（DMA burst, 全 PCIe 带宽 32GB/s）
  ② 更新 CPU PTE 指向新的系统 DRAM 地址（不再是 DEVICE_PRIVATE）
  ③ CPU 此后直接访问本地 DDR（~100ns, 不走 PCIe, 可命 CPU cache）
  ④ 所有后续访问全是本地内存速度
```

HMM 的效率来自两点：

1. **DMA 带宽 >> CPU MMIO**：GPU DMA 引擎可以一次搬 2MB THP，饱和 PCIe 带宽。CPU MMIO 每个读都是一次 PCIe 事务往返，开销巨大
2. **一次搬，永久用**：数据搬到系统 DRAM 后，CPU 页表指向的就是本地 DDR 地址了。后续访问零 PCIe、零迁移，全速命中的 CPU cache。本质上"HMM 把一次性的 DMA 迁移延迟≈远程内存访问变成了无穷次本地访问≈零延迟"

### 8.3 迁移期间怎么保证数据一致性？

迁移**不是异步的**，而是**同步阻塞**的——CPU 访问线程被卡在 page fault 中直到 DMA 完成：

```
CPU 访问 VRAM 中的页
  → MMU 查 PTE: DEVICE_PRIVATE（不能被 CPU 直接读）
  → 触发 CPU page fault
  → handle_mm_fault → do_swap_page → migrate_vma

  migrate_vma 流程 (阻塞等待 DMA 完成):
    ① 分配系统 DRAM 页
    ② GPU DMA 引擎: VRAM → DRAM（全 PCIe 带宽搬运, 2MB THP 一次搬）
    ③ CPU 等待 DMA 完成（阻塞!）
    ④ 更新 PTE: 指向新的 DRAM 页
    ⑤ 返回用户态 → CPU 重新执行访问指令 → 命中本地 DDR
```

**没有"实时性"问题**，因为这个操作就是同步的——DMA 完成之前 CPU 不会继续往下走。机制上完全类比 `read()` 等磁盘 IO——block 直到数据到达。

**GPU 端的一致性**由 `mmu_notifier` 保证。GPU 修改了自己的页表后，通过 MMU notifier 通知 CPU 侧"这个页我改过了"。CPU 再访问时 DEVICE_PRIVATE PTE 已经失效，触发新的迁移。

### 8.4 VRAM 是什么

VRAM = Video RAM（显存），GPU 的专用内存，物理上与系统 DRAM 分离：

- 物理上：GPU 板载的 GDDR/HBM 内存颗粒，通过 GPU 内部内存控制器访问
- 内核中：VRAM 通过 TTM（`drivers/gpu/drm/ttm/`）管理，nouveau 等驱动用 `gpu_buddy_alloc_blocks()` 分配块
- CPU 只能通过 PCIe BAR 以 MMIO 方式间接访问——这正是 HMM/DMA 要绕过的瓶颈

### 8.5 统一内存到底实现了没有

**HMM 实现了统一内存的基础设施，但不等于"HMM 就是统一内存"。** HMM 是内核提供的一套 API（`hmm_range_fault` + `migrate_vma`），nouveau 驱动**用它实现了 NVIDIA 的统一内存**：

```
用户看到的效果（CUDA Unified Memory / cudaMallocManaged）:
  GPU 和 CPU 可以访问同一块虚拟地址，数据自动在两边迁移

实现链路:
  GPU 缺页 → nouveau_svm 故障处理 → hmm_range_fault(读 CPU 页表)
    → 如果页在 DRAM → nouveau_pfns_map 写 GPU 页表 (路径 A)
    → 如果页在 VRAM → nouveau_dmem_migrate_to_ram DMA 迁到 DRAM (路径 B)
  CPU 缺页 (DEVICE_PRIVATE) → host page fault → migrate_vma → DMA 搬到 DRAM
```

**所以 HMM 本身只管"读 CPU 页表 + 搬运数据"。它不负责 GPU 页表管理（那是驱动做的），不负责分配 VRAM（那是 TTM/GPU Buddy 做的），不负责 DMA 引擎调度（那是 nouveau_dmem 做的）。统一内存在用户看来是一体化的 `cudaMallocManaged`，但在内核中是由 HMM + DRM GPUSVM + nouveau_svm + nouveau_dmem + TTM + GPU Buddy 六个子系统协作完成的。

### 8.6 离散 GPU 的"软件改造统一内存" — 内核已做到

能，而且**已经做到了**——我们 nvidia-svm 系列分析的内核代码就是这套方案。

就是 **HMM + `migrate_vma` + CUDA Unified Memory**：

```
CUDA 程序员写: cudaMallocManaged(ptr, size)
  ptr 在 GPU 和 CPU 上都能直接读写 ← 看起来像统一内存

内核做的事（本系列覆盖的全部内容）:
  GPU 访问 ptr → GPU MMU miss → nouveau_svm fault handler
    → hmm_range_fault 读 CPU 页表

    情况 A: 数据在系统 DRAM
      → nouveau_pfns_map 写 GPU 内部页表 → GPU 通过 PCIe 读 DRAM（慢但能读）

    情况 B: 数据在 VRAM
      → nouveau_dmem_page_addr 返回 DEVICE_PRIVATE PFN
      → CPU 再访问 → DEVICE_PRIVATE PTE → 触发 CPU page fault
        → nouveau_dmem_migrate_to_ram → DMA 从 VRAM 搬运到 DRAM → 更新 CPU PTE
```

所以**能力上**离散 GPU 已经能做统一内存——CUDA 从 2013 年 (Kepler) 就支持 `cudaMallocManaged`。内核的 `mm/hmm.c` 就是为这个场景写的。

**性能上**做不到和 SoC 统一内存一样，原因是物理路径不同：

| | SoC UMA（NVIDIA+MTK） | 离散 GPU + HMM 模拟 |
|---|---|---|
| GPU 访问"CPU 的页" | 走 SMMU → DDR（本地速度） | 走 PCIe → DRAM（~32GB/s，本地 1/3） |
| CPU 访问"GPU 的页" | 不存在"GPU 专有"页 | DMA 搬运到 DRAM → 等搬运 → 访问 |
| 页迁移 | 无 | `migrate_vma` DMA 搬运, 每次微秒级延迟 |
| 编程模型 | `cudaMallocManaged` | `cudaMallocManaged` ← **完全一样** |

**软件改造的本质就是把"独立显存"包装成"看起来统一"，代价是迁移延迟和 PCIe 带宽。但用户代码不需要改一行。** 我们两个系列的区别正是在这里——nvidia-svm 系列讲的就是这套"包装"是怎么实现的，soc-unified-memory 系列讲的是"物理上就是统一的"不需要包装。

**版本支持**：HMM 在 Linux 4.14（2017 年）合入主线，`nouveau_svm.c` 和 `nouveau_dmem.c` 在 4.16 合入。6.6 内核（2023 年 10 月）完全包含这里分析的全部代码——只要 `CONFIG_DRM_NOUVEAU_SVM=y`，CUDA Unified Memory 在 NVIDIA 开源驱动下的内核侧路径和我们分析的完全一致。**

## 9. 下一篇文章

[第 2 篇：GPU 共享虚拟内存抽象层 — DRM GPUSVM](02-drm-gpusvm.md)
— HMM 只负责读 CPU 页表，DRM GPUSVM 在 HMM 之上构建了完整的 GPU SVM 框架：notifier 管理、range 分配、设备内存迁移、DMA 映射协调。