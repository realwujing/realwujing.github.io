# 第5篇：SoC 统一内存完整时序 — 从 GPU 缺页到页表填充

> 源码：`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c`, `drivers/iommu/io-pgfault.c`, `drivers/iommu/io-pgtable-arm.c`, `mm/hmm.c`, `drivers/gpu/drm/nouveau/nouveau_svm.c`, `drivers/gpu/drm/nouveau/nouveau_dmem.c`

系列目录：[SoC 统一内存架构深度解析](./README.md)

---

## 1. 目标

本文追踪从 GPU 虚拟地址访问到 GPU TLB 被填充的**完整序列**，对比 **Discrete GPU**（H100）和 **SoC 统一内存**（NVIDIA+MTK）两条路径，指出每一步的差异。

核心问题：GPU 访问一个尚未映射的虚拟地址时，到底发生了什么？

---

## 2. Part A：Discrete GPU（H100）序列

### 2.1 整体流程

```
GPU 访问 VA → GPU MMU 遍历 GPU 内部页表 → miss
  → GPU 硬件写 fault buffer
  → nouveau 驱动中断 → work handler (nouveau_svm_fault)
  → hmm_range_fault → 读 CPU 页表 → 获取 PFN[]
  → [若页在 VRAM] DEVICE_PRIVATE PFN → nouveau_dmem 解析
  → [若页在系统内存] 普通 PFN
  → nvif_vmm_pfnmap → 写 GPU 内部页表
  → [可选] migrate_vma → 迁移页面到 VRAM
  → GPU 重试 → 命中
```

### 2.2 Fault Buffer 结构

GPU 硬件将缺页信息记录在 fault buffer 中：

`nouveau_svm.c:46-70`：
```c
struct nouveau_svm_fault_buffer {
    int id;
    struct nvif_object object;
    u32 entries;
    u32 getaddr, putaddr;
    u32 get, put;
    struct nvif_event notify;
    struct work_struct work;

    struct nouveau_svm_fault {          // line 57
        u64 inst;                       // 实例指针
        u64 addr;                       // 缺页虚拟地址
        u64 time;                       // 时间戳
        u32 engine;                     // 引发故障的引擎
        u8  gpc, hub, access, client, fault;
        struct nouveau_svmm *svmm;      // SVM manager
    } **fault;
    int fault_nr;
} buffer[];
```

GPU 维护一个环形 buffer（`get`/`put` 指针），硬件写 `put`，驱动读 `get`。

### 2.3 work handler 核心逻辑

`nouveau_svm.c:717-755`：

```c
static void nouveau_svm_fault(struct work_struct *work)
{
    struct nouveau_svm_fault_buffer *buffer =
        container_of(work, typeof(*buffer), work);     // line 719
    struct nouveau_svm *svm =
        container_of(buffer, typeof(*svm), buffer[buffer->id]); // line 720

    // Step 1: 解析 fault buffer，读 GET/PUT 指针
    if (buffer->get == buffer->put) {                  // line 733
        buffer->put = nvif_rd32(device, buffer->putaddr);
        buffer->get = nvif_rd32(device, buffer->getaddr);
        if (buffer->get == buffer->put)
            return;  // 没有待处理的故障
    }

    // Step 2: 将硬件记录解析到 nouveau_svm_fault 结构体
    while (buffer->get != buffer->put) {
        nouveau_svm_fault_cache(svm, buffer, buffer->get * 0x20); // line 743
        if (++buffer->get == buffer->entries)
            buffer->get = 0;
    }
    nvif_wr32(device, buffer->getaddr, buffer->get);  // line 747

    // Step 3: 排序故障，按实例→地址→访问类型
    sort(buffer->fault, buffer->fault_nr, sizeof(*buffer->fault),
         nouveau_svm_fault_cmp, NULL);                 // line 754

    // ... 之后遍历 fault_nr 个故障，对每个调用 hmm_range_fault
}
```

### 2.4 HMM 获取 PFN：`hmm_range_fault`

`mm/hmm.c:659-686`：

```c
int hmm_range_fault(struct hmm_range *range)
{
    struct hmm_vma_walk hmm_vma_walk = {
        .range = range,
        .last = range->start,
    };
    struct mm_struct *mm = range->notifier->mm;

    mmap_assert_locked(mm);

    do {
        if (mmu_interval_check_retry(range->notifier,
                                     range->notifier_seq))   // line 672
            return -EBUSY;

        ret = walk_page_range(mm, hmm_vma_walk.last, range->end,
                              &hmm_walk_ops, &hmm_vma_walk); // line 675
    } while (ret == -EBUSY);
    return ret;
}
```

核心机制：
- `walk_page_range`（line 675）遍历 CPU 的 **mm_struct 页表**（PGD→PUD→PMD→PTE），使用 `hmm_walk_ops` 回调（`mm/hmm.c:635`）
- **只读遍历**，不触发缺页（注释 line 656-657）
- 遇到 `DEVICE_PRIVATE` 页时，PFN 不是物理地址，需要 `nouveau_dmem` 层解析

### 2.5 GPU 页表写入：`nvif_vmm_pfnmap`

Discrete GPU 有自己的页表硬件，通过 `NVIF` 接口写入 GPU 内部 MMU 页表。这不是 Linux 内核的 `mm_struct` 页表，也不是 SMMU 的 `io-pgtable`。

### 2.6 页面迁移：`nouveau_dmem_migrate_vma`

`nouveau_dmem.c:822-879`：

```c
int nouveau_dmem_migrate_vma(struct nouveau_drm *drm,
                             struct nouveau_svmm *svmm,
                             struct vm_area_struct *vma,
                             unsigned long start, unsigned long end)
{
    struct migrate_vma args = {
        .vma            = vma,
        .start          = start,
        .pgmap_owner    = drm->dev,                           // line 833
        .flags          = MIGRATE_VMA_SELECT_SYSTEM
                        | MIGRATE_VMA_SELECT_COMPOUND,        // line 834
    };

    // ...
    for (i = 0; i < npages; i += max) {
        ret = migrate_vma_setup(&args);                       // line 872
        if (args.cpages)
            nouveau_dmem_migrate_chunk(drm, svmm, &args,
                                       dma_info, pfns);
        args.start = args.end;
    }
}
```

将系统内存页面迁移到 GPU 的 DEVICE_PRIVATE 内存（VRAM）。这是 Discrete GPU 的额外开销——SoC 完全不需要。

---

## 3. Part B：SoC 统一内存序列

### 3.1 整体流程

```
GPU 访问 VA → GPU ATC 查询 (ATS) → miss
  → GPU PRI Page Request → SMMU
  → SMMU 事件队列 (EVT_ID_TRANSLATION_FAULT, 0x10)
  → arm_smmu_handle_event (arm-smmu-v3.c:2136)
  → iommu_report_device_fault (io-pgfault.c:214)
  → IOPF 框架 → iopf_handler (domain 注册的回调)
  → [handler 内部] hmm_range_fault → 读 CPU MMU 页表 → PFN[]
  → arm_smmu_map_pages → arm_lpae_map_pages → 写 SMMU io-pgtable
  → iopf_group_response(SUCCESS) → arm_smmu_page_response(RETRY)
  → CMDQ_RESUME → GPU 重试 → ATC 命中 → 成功
```

**关键区别**：没有设备内存，没有 `migrate_vma`，没有 DMA 拷贝。IOVA→PA 的翻译本身就是答案。

### 3.2 SMMU 事件接收：`arm_smmu_handle_event`

`arm-smmu-v3.c:2136-2201`：

```c
static int arm_smmu_handle_event(struct arm_smmu_device *smmu, u64 *evt,
                                 struct arm_smmu_event *event)
{
    struct iopf_fault fault_evt = { };
    struct iommu_fault *flt = &fault_evt.fault;

    switch (event->id) {
    case EVT_ID_TRANSLATION_FAULT:           // line 2150: key event ID = 0x10
    case EVT_ID_ADDR_SIZE_FAULT:
    case EVT_ID_ACCESS_FAULT:
    case EVT_ID_PERMISSION_FAULT:
        break;                               // 这些是我们要处理的
    default:
        return -EOPNOTSUPP;
    }

    if (event->stall) {                      // line 2159: stall 模式
        // 构造 iommu_fault_page_request
        flt->type = IOMMU_FAULT_PAGE_REQ;    // line 2171
        flt->prm = (struct iommu_fault_page_request){
            .flags = IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE,  // line 2173
            .grpid = event->stag,                          // line 2174
            .perm  = perm,                                 // line 2175
            .addr  = event->iova,                          // line 2176
        };
        if (event->ssv) {                    // line 2179: 有 PASID
            flt->prm.flags |= IOMMU_FAULT_PAGE_REQUEST_PASID_VALID;
            flt->prm.pasid = event->ssid;    // line 2181
        }
    }

    // ...
    if (event->stall)
        ret = iommu_report_device_fault(master->dev, &fault_evt); // line 2193
}
```

`EVT_ID_TRANSLATION_FAULT` (值 `0x10`，`arm-smmu-v3.h:464`) 是 SMMU 表示"页表里没有这个 IOVA 的映射"的事件 ID。

- **line 2159**：只处理 `stall` 模式的事件——事务被暂停，等待 OS 响应
- **line 2171-2177**：将 SMMU 事件翻译为 `iommu_fault_page_request` 结构体
- **line 2193**：调用 `iommu_report_device_fault` 进入 IOPF 框架

### 3.3 IOPF 框架分发

`io-pgfault.c:214-262`（详见第 4 篇），最终调用 `domain->iopf_handler(group)`（line 262）。这个 handler 就是 GPU SVM 驱动注册的回调。

### 3.4 HMM 读取 CPU 页表

IOPF handler 内部调用 `hmm_range_fault(range)`（`mm/hmm.c:659`），**读取的是 CPU 的 `mm_struct` 页表，不是 SMMU 的 `io-pgtable`**。

这是架构的关键点：
- CPU MMU 遍历：`mm->pgd → pud → pmd → pte → pfn`
- SMMU 遍历：`smmu_domain->pgtbl_ops → io-pgtable → pte → paddr`

在 SoC 统一内存中，**这两个遍历最终得到的是同一个物理页**。`hmm_range_fault` 只需要返回 PFN，不需要关心是从哪个 MMU 查到的。

### 3.5 写入 SMMU 页表：`arm_smmu_map_pages` → `arm_lpae_map_pages`

handler 获得 PFN 数组后，需要调用 IOMMU 的 `map_pages` 操作将映射写入 SMMU 页表。

`arm-smmu-v3.c:4025-4034`：

```c
static int arm_smmu_map_pages(struct iommu_domain *domain, unsigned long iova,
                              phys_addr_t paddr, size_t pgsize, size_t pgcount,
                              int prot, gfp_t gfp, size_t *mapped)
{
    struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;  // line 4029

    if (!ops)
        return -ENODEV;

    return ops->map_pages(ops, iova, paddr, pgsize, pgcount, prot,
                          gfp, mapped);                              // line 4034
}
```

`arm_smmu_map_pages` 直接委托给 `io-pgtable` 的 `map_pages`。对于 ARM LPAE 格式：

`io-pgtable-arm.c:549-581`：

```c
static int arm_lpae_map_pages(struct io_pgtable_ops *ops, unsigned long iova,
                              phys_addr_t paddr, size_t pgsize, size_t pgcount,
                              int iommu_prot, gfp_t gfp, size_t *mapped)
{
    struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
    arm_lpae_iopte *ptep = data->pgd;
    int ret, lvl = data->start_level;
    arm_lpae_iopte prot;

    // 验证 pgsize、IOVA 范围
    if (WARN_ON(!pgsize || (pgsize & cfg->pgsize_bitmap) != pgsize))
        return -EINVAL;                                            // line 560

    prot = arm_lpae_prot_to_pte(data, iommu_prot);                 // line 571
    ret = __arm_lpae_map(data, iova, paddr, pgsize, pgcount, prot,
                         lvl, ptep, gfp, mapped);                  // line 572

    wmb();  // 确保 PTE 更新对其他观察者可见                      // line 578
    return ret;
}
```

`__arm_lpae_map` 完成实际的页表遍历和 PTE 填充。内部逻辑：
- 从 `data->pgd` 出发，逐级遍历（line 553, 572）
- 缺失的中间页表动态分配（`gfp` 参数）
- 最终将 `paddr | prot` 写入 leaf PTE
- `wmb()` 保证写顺序（line 578）

### 3.6 通知 GPU 重试

handler 填充完 SMMU 页表后，调用：
1. `iopf_group_response(group, IOMMU_PAGE_RESP_SUCCESS)`（`io-pgfault.c:322`）
2. 内部调用 `ops->page_response` → `arm_smmu_page_response`（`arm-smmu-v3.c:997`）
3. `arm_smmu_page_response` 发送 `CMDQ_OP_RESUME` + `CMDQ_RESUME_0_RESP_RETRY`（line 1016）
4. GPU 收到 RESUME 命令，重试被暂停的内存访问
5. 这次 SMMU 页表命中 → 翻译成功 → GPU ATC 缓存结果 → 后续访问全部命中

---

## 4. 完整时序图

```
Time →
GPU              SMMU                IOPF                 OS Handler          CPU MMU
 │                │                    │                    │                    │
 │ DMA Read(IOVA) │                    │                    │                    │
 │───────────────►│                    │                    │                    │
 │                │                    │                    │                    │
 │ [ATC miss]     │                    │                    │                    │
 │ PRI Page Req───►                    │                    │                    │
 │                │                    │                    │                    │
 │                │ EVTQ 入队          │                    │                    │
 │                │ EVT_ID_TRANSLATION_FAULT (0x10)         │                    │
 │                │ arm_smmu_handle_event (line 2136)      │                    │
 │                │                    │                    │                    │
 │                │ iommu_report_device_fault (line 2193)  │                    │
 │                │───────────────────►│                    │                    │
 │                │                    │                    │                    │
 │                │                    │ find_fault_handler │                    │
 │                │                    │ (line 222)         │                    │
 │                │                    │                    │                    │
 │                │                    │ PAGE_REQUEST_LAST_PAGE (line 234)     │
 │                │                    │ iopf_group_alloc   │                    │
 │                │                    │ (line 252)         │                    │
 │                │                    │                    │                    │
 │                │                    │ domain->iopf_handler(group)           │
 │                │                    │ (line 262)         │                    │
 │                │                    │───────────────────►│                    │
 │                │                    │                    │                    │
 │                │                    │                    │ hmm_range_fault()  │
 │                │                    │                    │ (mm/hmm.c:659)     │
 │                │                    │                    │ walk_page_range()  │
 │                │                    │                    │ (line 675)         │
 │                │                    │                    │───────────────────►│
 │                │                    │                    │                    │
 │                │                    │                    │      ← PFN[] ──────│
 │                │                    │                    │                    │
 │                │                    │                    │ arm_smmu_map_pages │
 │                │                    │                    │ (arm-smmu-v3:4025) │
 │                │                    │                    │      →             │
 │                │                    │                    │ arm_lpae_map_pages │
 │                │                    │                    │ (io-pgtable-arm:549)│
 │                │                    │                    │ __arm_lpae_map     │
 │                │                    │                    │ (line 572)         │
 │                │                    │                    │ wmb()              │
 │                │                    │                    │ (line 578)         │
 │                │                    │                    │                    │
 │                │                    │                    │ iopf_group_response│
 │                │                    │                    │ (SUCCESS)          │
 │                │                    │◄───────────────────│                    │
 │                │                    │                    │                    │
 │                │ arm_smmu_page_response(RETRY)           │                    │
 │                │ (line 1016)        │                    │                    │
 │                │◄───────────────────│                    │                    │
 │                │                    │                    │                    │
 │ CMDQ_RESUME    │                    │                    │                    │
 │ RESP_RETRY     │                    │                    │                    │
 │◄───────────────│                    │                    │                    │
 │                │                    │                    │                    │
 │ GPU 重试事务    │                    │                    │                    │
 │ 访问同一 IOVA   │                    │                    │                    │
 │───────────────►│                    │                    │                    │
 │                │                    │                    │                    │
 │                │ SMMU 页表遍历 HIT   │                    │                    │
 │                │ IOVA → PA 翻译成功  │                    │                    │
 │                │                    │                    │                    │
 │ ◄── PA ────────│                    │                    │                    │
 │                │                    │                    │                    │
 │ [ATC 缓存]     │                    │                    │                    │
 │ DMA 成功完成    │                    │                    │                    │
 │                │                    │                    │                    │
 │ 后续同 IOVA 访问│                    │                    │                    │
 │ 直接 ATC HIT   │                    │                    │                    │
 │ [不再经过 SMMU] │                    │                    │                    │
```

---

## 5. Discrete GPU vs SoC 对比表

| 步骤 | Discrete GPU (H100) | SoC 统一内存 (NVIDIA+MTK) |
|------|---------------------|---------------------------|
| **缺页检测** | GPU 内部 MMU 遍历 GPU 私有的页表 → miss | GPU ATC miss → PRI Page Request → SMMU |
| **故障记录** | GPU 硬件写 `nouveau_svm_fault_buffer`（环形 buffer，`nouveau_svm.c:46`） | SMMU 硬件写 EVTQ（事件队列） |
| **中断/通知** | nvif 事件通知 → work handler `nouveau_svm_fault`（`nouveau_svm.c:717`） | SMMU 中断 → `arm_smmu_handle_event`（`arm-smmu-v3.c:2136`） |
| **故障传递** | work handler 直接处理 | `iommu_report_device_fault` → IOPF 框架（`io-pgfault.c:214`） |
| **页表读取** | `hmm_range_fault` → walk CPU `mm_struct` 页表（`mm/hmm.c:659`） | 同上，完全一致 |
| **PFN 类型** | 可能是 `DEVICE_PRIVATE`（VRAM 页），需要 `nouveau_dmem` 解析 | 始终是普通系统内存 PFN |
| **页表写入** | `nvif_vmm_pfnmap` → 写 GPU 内部 MMU 页表 | `arm_smmu_map_pages`（`arm-smmu-v3.c:4025`）→ `arm_lpae_map_pages`（`io-pgtable-arm.c:549`）→ 写 SMMU `io-pgtable` |
| **页面迁移** | `nouveau_dmem_migrate_vma`（`nouveau_dmem.c:822`）→ `migrate_vma_setup`（line 872） | **无**（IOVA→PA 翻译即答案） |
| **DMA 拷贝** | 可能发生（page migration 时） | **不发生** |
| **GPU 重试** | 驱动写 replay 寄存器 → `nouveau_svm_fault_replay`（`nouveau_svm.c:381`） | `arm_smmu_page_response`（`arm-smmu-v3.c:997`）→ `CMDQ_RESUME_RESP_RETRY`（line 1016） |
| **TLB 缓存** | GPU 内部 TLB | GPU ATC（通过 ATS 协议管理） |

---

## 6. 代码调用链汇总

```
=== Discrete GPU (H100) ===
GPU hardware fault
  → nouveau_svm_fault_buffer (nouveau_svm.c:46-70)
    → nouveau_svm_fault (nouveau_svm.c:717) [work handler]
      → hmm_range_fault (mm/hmm.c:659)
        → walk_page_range (mm/hmm.c:675)
      → [if DEVICE_PRIVATE] nouveau_dmem resolve
      → nvif_vmm_pfnmap → GPU page table write
      → [optional] nouveau_dmem_migrate_vma (nouveau_dmem.c:822)
        → migrate_vma_setup (nouveau_dmem.c:872)
      → nouveau_svm_fault_replay (nouveau_svm.c:381)

=== SoC Unified Memory ===
GPU ATC miss → PRI Page Request → SMMU
  → arm_smmu_handle_event (arm-smmu-v3.c:2136)
    EVT_ID_TRANSLATION_FAULT (arm-smmu-v3.h:464)
    → construct iommu_fault_page_request (arm-smmu-v3.c:2171-2177)
    → iommu_report_device_fault (arm-smmu-v3.c:2193; io-pgfault.c:214)
      → find_fault_handler (io-pgfault.c:118)
      → report_partial_fault OR iopf_group_alloc (io-pgfault.c:237/252)
      → domain->iopf_handler(group) (io-pgfault.c:262)
        → hmm_range_fault (mm/hmm.c:659) [READS CPU MMU PAGE TABLES]
          → walk_page_range (mm/hmm.c:675)
        → arm_smmu_map_pages (arm-smmu-v3.c:4025)
          → arm_lpae_map_pages (io-pgtable-arm.c:549)
            → __arm_lpae_map (io-pgtable-arm.c:572)
            → wmb() (io-pgtable-arm.c:578)
        → iopf_group_response(SUCCESS) (io-pgfault.c:322)
          → arm_smmu_page_response (arm-smmu-v3.c:997)
            → CMDQ_RESUME_0_RESP_RETRY (arm-smmu-v3.c:1016)
```

---

## 7. 为什么这对 NVIDIA+MTK+MS 笔记本很重要

在 NVIDIA 与 MediaTek 合作的 ARM SoC 平台上（以及 Microsoft 的 ARM 笔记本）：

1. **没有独立 VRAM**：GPU 和 CPU 共享同一块 LPDDR 内存，物理上就是同一片芯片
2. **不需要迁移**：不存在"page 在 VRAM 还是系统内存"的问题，所有 page 都在同一物理地址空间
3. **页表统一**：SMMU 的 Stage-1 页表格式与 CPU MMU 页表兼容（ARM LPAE 格式），不需要"写一份 GPU 专用页表"
4. **Linux 内核已经提供了完整栈**：HMM（读取 CPU 页表）+ SMMU（IOMMU 翻译）+ ATS/PRI（硬件缓存与缺页请求）+ IOPF（软件缺页框架），这四个组件串起来就是完整的 SoC 统一内存
5. **性能优势**：少了一层页表管理，少了一条内存迁移路径，延迟更低

---

## 8. 系列结语

通过这五篇文章，我们走完了 SoC 统一内存架构的完整纵深：

- **第 1 篇**建立了 SMMU 和 io-pgtable 的基础——为什么 CPU 和 GPU 能共享页表
- **第 2 篇**解剖了 HMM——内核如何**只读遍历**CPU 页表获取 PFN
- **第 3 篇**讲述了 IOPF——缺页中断如何从硬件传递到 OS handler
- **第 4 篇**说明了 ATS/PRI——GPU 如何缓存翻译、如何请求缺失的页表映射
- **第 5 篇**（本篇）将以上所有模块串联成完整的端到端时序

读者现在应该理解：**SoC 统一内存不是"把 VRAM 搬到系统内存"，而是根本取消了"设备内存"这个概念**。IOVA→PA 的翻译本身就是对 GPU 内存访问的完整响应。Linux 内核已有的 HMM + SMMU + IOPF + ATS/PRI 基础设施恰好为这一架构提供了开箱即用的支持。

对本系列的姊妹篇 [nvidia-svm 深度解析](../nvidia-svm/README.md) 感兴趣的读者，可以对比两种架构在 `hmm_range_fault` 之后的处理路径——一个是写 GPU 内部页表、可能迁移内存；一个是写 SMMU 页表、直接返回。理解了这两种路径，就真正理解了 Linux 内核统一内存架构的全貌。
