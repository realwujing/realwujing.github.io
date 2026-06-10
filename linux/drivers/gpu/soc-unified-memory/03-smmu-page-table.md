# 第3篇：ARM SMMU 页表镜像 — IOMMU 如何把 CPU 页表给 GPU 用

> 源码：`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c` `drivers/iommu/io-pgtable-arm.c` | 对应头文件：`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h`

系列目录：[SoC 统一内存架构深度解析](./README.md)

---

## 问题背景

CPU 有自己的一套页表，根指针在 `TTBR0_EL1`（用户态）和 `TTBR1_EL1`（内核态），MMU 用这套页表将虚拟地址翻译为物理地址。但 GPU 是一个独立的 bus master，它通过 IOMMU（SMMU）发起内存访问——GPU 看到的 IOVA（I/O Virtual Address）需要通过 SMMU 翻译为物理地址。

**核心挑战**：CPU 页表和 SMMU 页表是两套独立的页表结构，但它们必须翻译出**相同的物理地址**，才能实现 CPU 和 GPU 之间的统一内存共享。

```
┌─────────────────────────────────────┐
│          物理内存 (DDR)              │
│  ┌──────┐ ┌──────┐ ┌──────┐        │
│  │ page │ │ page │ │ page │ ...    │
│  └──┬───┘ └──┬───┘ └──────┘        │
└─────┼────────┼─────────────────────┘
      │        │
   ┌──┘        └──┐
   │              │
┌──▼──────┐  ┌────▼──────┐
│CPU MMU  │  │GPU SMMU   │
│TTBR0_EL1│  │STE→CD→S1  │
│  ↓      │  │  PgTable  │
│PGD→PUD  │  │  ↓        │
│→PMD→PTE │  │PGD→PUD    │
│         │  │→PMD→PTE   │
│CPU reads│  │GPU reads  │
│  VA→PA  │  │ IOVA→PA   │
└─────────┘  └───────────┘
      ↑              ↑
  0x7f...      0x10000...
  (VA)         (IOVA)
```

---

## ARM SMMUv3 架构概述

SMMUv3 是 ARM 体系下的 IOMMU 实现，支持两阶段地址翻译：

### 关键数据结构关系

```
StreamID (来自 PCIe Requester ID)
    │
    ▼
Stream Table (STRTAB) → STE (Stream Table Entry)
    │                       │
    │              ┌────────┼────────┐
    │              ▼                 ▼
    │    Stage-1 Context Desc      Stage-2 (可选)
    │    (CD: 指向 S1 页表)       (直接嵌入 STE)
    │              │
    │    ┌─────────▼──────────┐
    │    │  Stage-1 页表       │ ← 与 CPU 页表格式兼容!
    │    │  (PGD→PUD→PMD→PTE) │
    │    └────────────────────┘
    │
    ▼
  IOVA → [Stage-1: VA→IPA] → [Stage-2: IPA→PA] → 物理地址
```

- **Stream Table Entry (STE)**：根据 StreamID 索引，配置该设备的 Stage-1/Stage-2 翻译参数
- **Context Descriptor (CD)**：指向 Stage-1 页表基地址（等同于 CPU 的 TTBR0_EL1），存储在 CD 表中
- **Stage-1 页表**：使用与 ARM64 CPU 页表相同的 LPAE（Large Physical Address Extension）格式
- **Stage-2 页表**：可选的 hypervisor 级别翻译，用于虚拟化场景

### arm_smmu_device — 硬件设备实例

`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h:824`：

```c
// arm-smmu-v3.h:824-893
struct arm_smmu_device {
    struct device       *dev;
    void __iomem        *base;
    void __iomem        *page1;

#define ARM_SMMU_FEAT_PRI       (1 << 4)    // line 836 — Page Request Interface
#define ARM_SMMU_FEAT_ATS       (1 << 5)    // line 837 — Address Translation Svc
#define ARM_SMMU_FEAT_STALLS    (1 << 11)   // line 843 — 支持 Stall 模式
#define ARM_SMMU_FEAT_VAX       (1 << 14)   // line 846 — 52-bit VA
#define ARM_SMMU_FEAT_SVA       (1 << 17)   // line 849 — Shared Virtual Addressing
#define ARM_SMMU_FEAT_HA        (1 << 21)   // line 853 — Hardware Access Flag
#define ARM_SMMU_FEAT_HD        (1 << 22)   // line 854 — Hardware Dirty
    u32                 features;

    struct arm_smmu_cmdq    cmdq;        // line 866 — Command Queue
    struct arm_smmu_evtq    evtq;        // line 867 — Event Queue (fault events)
    struct arm_smmu_priq    priq;        // line 868 — PRI Queue

    unsigned long       oas;             // line 873 — 输出地址宽度 (PA bits)
    unsigned long       pgsize_bitmap;   // line 874 — 支持的页大小位图
    unsigned int        asid_bits;       // line 877 — ASID 位数
    unsigned int        ssid_bits;       // line 883 — SSID 位数
    unsigned int        sid_bits;        // line 884 — Stream ID 位数
};
```

### arm_smmu_master — 设备侧表示

`arm-smmu-v3.h:927-948`：

```c
// arm-smmu-v3.h:927-948
struct arm_smmu_master {
    struct arm_smmu_device      *smmu;         // line 928
    struct device               *dev;          // line 929
    struct arm_smmu_stream      *streams;      // line 930
    unsigned int                num_streams;   // line 942
    bool                        ats_enabled:1; // line 943 — ATS 使能
    bool                        ste_ats_enabled:1; // line 944
    bool                        stall_enabled; // line 945 — Stall 使能
    unsigned int                ssid_bits;     // line 946 — SSID 位数
    unsigned int                iopf_refcount; // line 947 — IOPF 引用计数
};
```

每个 PCIe 设备（GPU）对应一个 `arm_smmu_master`。`ats_enabled` 标志表示该设备支持 Address Translation Service（见第4篇），`stall_enabled` 表示可以 stall 设备在缺页时等待 SW 处理。

### arm_smmu_domain — 地址空间

`arm-smmu-v3.h:957-980`：

```c
// arm-smmu-v3.h:957-980
struct arm_smmu_domain {
    struct arm_smmu_device      *smmu;         // line 958
    struct io_pgtable_ops       *pgtbl_ops;    // line 960 — 页表操作接口
    atomic_t                    nr_ats_masters; // line 961 — ATS master 计数

    enum arm_smmu_domain_stage  stage;         // line 963
    // ARM_SMMU_DOMAIN_S1 = 0,                 // line 952 — Stage-1
    // ARM_SMMU_DOMAIN_S2,                     // line 953 — Stage-2
    // ARM_SMMU_DOMAIN_SVA,                    // line 954 — SVA

    union {
        struct arm_smmu_ctx_desc cd;           // Stage-1 Context Descriptor
        struct arm_smmu_s2_cfg  s2_cfg;        // Stage-2 配置
    };

    struct iommu_domain         domain;        // line 969 — 通用 IOMMU domain

    struct mmu_notifier         mmu_notifier;  // line 979 — CPU 页表变更通知
};
```

`pgtbl_ops` 是页表操作的抽象接口，通过 `alloc_io_pgtable_ops()` 初始化为 LPAE 实现。

---

## IOMMU Operations 注册

`arm-smmu-v3.c:4372-4402` 定义了完整的 `iommu_ops` 结构体：

```c
// arm-smmu-v3.c:4372-4402
static const struct iommu_ops arm_smmu_ops = {
    .identity_domain = &arm_smmu_identity_domain,
    .blocked_domain   = &arm_smmu_blocked_domain,
    .capable          = arm_smmu_capable,
    .hw_info          = arm_smmu_hw_info,
    .domain_alloc_sva = arm_smmu_sva_domain_alloc,
    .probe_device     = arm_smmu_probe_device,
    .release_device   = arm_smmu_release_device,
    .page_response    = arm_smmu_page_response,   // PRI 响应回调

    .default_domain_ops = &(const struct iommu_domain_ops) {
        .attach_dev        = arm_smmu_attach_dev,
        .map_pages         = arm_smmu_map_pages,      // → ops->map_pages
        .unmap_pages       = arm_smmu_unmap_pages,    // → ops->unmap_pages
        .iova_to_phys      = arm_smmu_iova_to_phys,   // → ops->iova_to_phys
        .flush_iotlb_all   = arm_smmu_flush_iotlb_all,
        .iotlb_sync        = arm_smmu_iotlb_sync,
        .free              = arm_smmu_domain_free_paging,
    }
};
```

`arm_smmu_map_pages`（`arm-smmu-v3.c:4025`）和 `arm_smmu_unmap_pages`（`arm-smmu-v3.c:4037`）只是薄封装，直接转发到 `smmu_domain->pgtbl_ops`：

```c
// arm-smmu-v3.c:4025-4034
static int arm_smmu_map_pages(struct iommu_domain *domain, unsigned long iova,
                              phys_addr_t paddr, size_t pgsize, size_t pgcount,
                              int prot, gfp_t gfp, size_t *mapped)
{
    struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;
    if (!ops) return -ENODEV;
    return ops->map_pages(ops, iova, paddr, pgsize, pgcount, prot, gfp, mapped);
}

// arm-smmu-v3.c:4072-4080
static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain, dma_addr_t iova)
{
    struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;
    if (!ops) return 0;
    return ops->iova_to_phys(ops, iova);
}
```

---

## 页表分配 — arm_smmu_domain_finalise

`arm-smmu-v3.c:2928-2993` 根据 domain 类型分配页表：

```c
// arm-smmu-v3.c:2928-2993
static int arm_smmu_domain_finalise(struct arm_smmu_domain *smmu_domain,
                                    struct arm_smmu_device *smmu, u32 flags)
{
    struct io_pgtable_cfg pgtbl_cfg = {
        .pgsize_bitmap   = smmu->pgsize_bitmap,
        .coherent_walk   = smmu->features & ARM_SMMU_FEAT_COHERENCY,
        .tlb             = &arm_smmu_flush_ops,
        .iommu_dev       = smmu->dev,
    };

    switch (smmu_domain->stage) {
    case ARM_SMMU_DOMAIN_S1: {
        unsigned long ias = (smmu->features &
                             ARM_SMMU_FEAT_VAX) ? 52 : 48;  // IAS: 48 or 52

        pgtbl_cfg.ias = min_t(unsigned long, ias, VA_BITS);
        pgtbl_cfg.oas = smmu->oas;
        fmt = ARM_64_LPAE_S1;                     // line 2955: S1 format
        finalise_stage_fn = arm_smmu_domain_finalise_s1;
        break;
    }
    case ARM_SMMU_DOMAIN_S2:
        pgtbl_cfg.ias = smmu->oas;
        pgtbl_cfg.oas = smmu->oas;
        fmt = ARM_64_LPAE_S2;                     // line 2964: S2 format
        finalise_stage_fn = arm_smmu_domain_finalise_s2;
        break;
    default:
        return -EINVAL;
    }

    // line 2974: 分配 io-pgtable 操作接口
    pgtbl_ops = alloc_io_pgtable_ops(fmt, &pgtbl_cfg, smmu_domain);
    if (!pgtbl_ops)
        return -ENOMEM;

    // line 2990: 赋值给 domain
    smmu_domain->pgtbl_ops = pgtbl_ops;
    smmu_domain->smmu = smmu;
    return 0;
}
```

Stage-1 使用 `ARM_64_LPAE_S1` 格式，这是与 ARM64 CPU 页表**完全相同**的 LPAE 格式。这意味着 SMMU 的 Stage-1 页表可以直接 mirror CPU 页表的结构。

---

## io-pgtable-arm — LPAE 页表实现

`drivers/iommu/io-pgtable-arm.c` 是 ARM LPAE 页表的通用实现，支持三种格式：`ARM_32_LPAE_S1/S2`、`ARM_64_LPAE_S1/S2`、`ARM_MALI_LPAE`。

### 基本参数

```c
// io-pgtable-arm.c:25-27
#define ARM_LPAE_MAX_ADDR_BITS   52    // 最大地址宽度 52 bits
#define ARM_LPAE_MAX_LEVELS      4     // 最大 4 级页表
```

级别选择逻辑（`io-pgtable-arm.c:940-942`）：

```c
va_bits = cfg->ias - pg_shift;
levels = DIV_ROUND_UP(va_bits, data->bits_per_level);
data->start_level = ARM_LPAE_MAX_LEVELS - levels;
```

### 映射操作 — __arm_lpae_map

`io-pgtable-arm.c:422-477` — 递归页表映射：

```c
// io-pgtable-arm.c:422-477
static int __arm_lpae_map(struct arm_lpae_io_pgtable *data, unsigned long iova,
                          phys_addr_t paddr, size_t size, size_t pgcount,
                          arm_lpae_iopte prot, int lvl, arm_lpae_iopte *ptep,
                          gfp_t gfp, size_t *mapped)
{
    size_t block_size = ARM_LPAE_BLOCK_SIZE(lvl, data);
    int map_idx_start = ARM_LPAE_LVL_IDX(iova, lvl, data);
    ptep += map_idx_start;

    // 如果请求的大小等于该级别 block size → 安装 leaf entry
    if (size == block_size) {
        max_entries = arm_lpae_max_entries(map_idx_start, data);
        num_entries = min_t(int, pgcount, max_entries);
        return arm_lpae_init_pte(data, iova, paddr, prot, lvl,
                                 num_entries, ptep);
    }

    // 不能超过最大级别 (line 449)
    if (WARN_ON(lvl >= ARM_LPAE_MAX_LEVELS - 1))
        return -EINVAL;

    // 下一级页表不存在→分配 (line 454)
    if (!pte) {
        cptep = __arm_lpae_alloc_pages(tblsz, gfp, cfg, data->iop.cookie);
        pte = arm_lpae_install_table(cptep, ptep, 0, data);
    }

    // 递归进入下一级 (line 475)
    return __arm_lpae_map(data, iova, paddr, size, pgcount, prot, lvl + 1,
                          cptep, gfp, mapped);
}
```

### 遍历操作 — arm_lpae_iova_to_phys

`io-pgtable-arm.c:734-753` — 逆向查找 IOVA→PA：

```c
// io-pgtable-arm.c:734-753
static phys_addr_t arm_lpae_iova_to_phys(struct io_pgtable_ops *ops,
                                         unsigned long iova)
{
    struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
    struct iova_to_phys_data d;
    struct io_pgtable_walk_data walk_data = {
        .data = &d,
        .visit = visit_iova_to_phys,
        .addr = iova,
        .end = iova + 1,
    };

    ret = __arm_lpae_iopte_walk(data, &walk_data,
                                data->pgd, data->start_level);
    if (ret)
        return 0;

    iova &= (ARM_LPAE_BLOCK_SIZE(d.lvl, data) - 1);
    return iopte_to_paddr(d.pte, data) | iova;  // 页内偏移
}
```

### ops 注册

`io-pgtable-arm.c:947-953`：

```c
// io-pgtable-arm.c:947-953
data->iop.ops = (struct io_pgtable_ops) {
    .map_pages           = arm_lpae_map_pages,        // line 948
    .unmap_pages         = arm_lpae_unmap_pages,      // line 949
    .iova_to_phys        = arm_lpae_iova_to_phys,     // line 950
    .read_and_clear_dirty= arm_lpae_read_and_clear_dirty, // line 951
    .pgtable_walk        = arm_lpae_pgtable_walk,     // line 952
};
```

---

## SMMU 缺页处理

### 事件队列线程

`arm_smmu_evtq_thread`（`arm-smmu-v3.c:2262-2293`）是 SMMU 事件队列的 IRQ 线程：

```c
// arm-smmu-v3.c:2262-2293
static irqreturn_t arm_smmu_evtq_thread(int irq, void *dev)
{
    struct arm_smmu_device *smmu = dev;
    struct arm_smmu_queue *q = &smmu->evtq.q;
    struct arm_smmu_ll_queue *llq = &q->llq;

    do {
        while (!queue_remove_raw(q, evt)) {       // 从 EVTQ 出队
            arm_smmu_decode_event(smmu, evt, &event);
            if (arm_smmu_handle_event(smmu, evt, &event))  // 处理
                arm_smmu_dump_event(smmu, evt, &event, &rs); // 无法处理的 dump
            put_device(event.dev);
        }

        if (queue_sync_prod_in(q) == -EOVERFLOW)
            dev_err(smmu->dev, "EVTQ overflow -- events lost\n");
    } while (!queue_empty(llq));

    return IRQ_HANDLED;
}
```

### 事件解析与分发

`arm_smmu_handle_event`（`arm-smmu-v3.c:2136-2201`）解码 SMMU 报告的错误事件：

```c
// arm-smmu-v3.c:2136-2201
static int arm_smmu_handle_event(struct arm_smmu_device *smmu, u64 *evt,
                                 struct arm_smmu_event *event)
{
    switch (event->id) {
    case EVT_ID_BAD_STE_CONFIG:          // STE 配置错误
    case EVT_ID_STREAM_DISABLED_FAULT:   // Stream 被禁用
    case EVT_ID_BAD_SUBSTREAMID_CONFIG:  // SSID 配置错误
    case EVT_ID_BAD_CD_CONFIG:           // CD 配置错误
    case EVT_ID_TRANSLATION_FAULT:       // ★ 翻译 fault
    case EVT_ID_ADDR_SIZE_FAULT:         // 地址范围 fault
    case EVT_ID_ACCESS_FAULT:            // ★ 访问 fault (AF 未置)
    case EVT_ID_PERMISSION_FAULT:        // ★ 权限 fault
        break;
    default:
        return -EOPNOTSUPP;
    }

    // Stall 模式: 构造 iommu_fault 结构体 (line 2159-2182)
    if (event->stall) {
        flt->type = IOMMU_FAULT_PAGE_REQ;
        flt->prm = (struct iommu_fault_page_request){
            .flags = IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE,
            .grpid = event->stag,
            .perm  = perm,
            .addr  = event->iova,
        };
        if (event->ssv) {
            flt->prm.flags |= IOMMU_FAULT_PAGE_REQUEST_PASID_VALID;
            flt->prm.pasid = event->ssid;
        }
    }

    // 上报给 IOMMU 核心 (line 2193)
    if (event->stall)
        ret = iommu_report_device_fault(master->dev, &fault_evt);
    ...
}
```

SMMU 的事件类型与 CPU 的 `fault_info[]` 表（`fault.c:908-972`）对应：
- `EVT_ID_TRANSLATION_FAULT` ↔ CPU translation fault（`fault.c:913-916`）
- `EVT_ID_ACCESS_FAULT` ↔ CPU access flag fault（`fault.c:917-920`）
- `EVT_ID_PERMISSION_FAULT` ↔ CPU permission fault（`fault.c:921-924`）

### Event ID 详解

SMMUv3 定义了完整的事件类型码（`arm-smmu-v3.h` 中定义，`arm-smmu-v3.c:2136-2201` 中处理）：

| Event ID | 宏 | CPU 等价 | 含义 |
|----------|-----|---------|------|
| 0x10 | `EVT_ID_TRANSLATION_FAULT` | FSC 0x04-0x07 | IOVA 对应页表项不存在（从未映射或已 unmap） |
| 0x11 | `EVT_ID_ADDR_SIZE_FAULT` | FSC 0x00-0x03 | IOVA 超出 IAS（Input Address Size）范围 |
| 0x12 | `EVT_ID_ACCESS_FAULT` | FSC 0x08-0x0B | PTE.AF=0，Access Flag 未置位 |
| 0x13 | `EVT_ID_PERMISSION_FAULT` | FSC 0x0C-0x0F | PTE 权限位不满足请求（如写只读页） |

与 CPU 缺页的对应关系：
- `EVT_ID_TRANSLATION_FAULT` (0x10) ≈ CPU `pte_none`，`fault_info` 派发给 `do_translation_fault`，最后走到 `do_page_fault` → `handle_mm_fault` → `__handle_mm_fault` → `handle_pte_fault` → `do_pte_missing`
- `EVT_ID_ACCESS_FAULT` (0x12) ≈ CPU `PTE_AF=0`，`fault_info` 派发给 `do_page_fault`，`handle_pte_fault` 走 `ptep_set_access_flags` 路径
- `EVT_ID_PERMISSION_FAULT` (0x13) ≈ CPU `FAULT_FLAG_WRITE && !pte_write`，`handle_pte_fault` 走 `do_wp_page`（写时复制）

关键差异：CPU 缺页路径直接由 `handle_mm_fault` 修页表后 retry 指令；SMMU 路径需要通过 IOPF 框架异步发送 Page Response，SMMU 发 CMDQ_RESUME 才能让 GPU 重试。

### SMMU stall 模式的两种子模式

`arm_smmu_handle_event`（`arm-smmu-v3.c:2136`）在 `event->stall` 为真时才构造 `iommu_fault_page_request`（line 2159）：

- **Stall 模式**：SMMU 暂停出错的设备事务，等待 OS 响应 RESUME 命令。IOPF 框架依赖此模式
- **Non-stall（Abort）模式**：SMMU 直接终止事务，返回 abort 给 GPU。无需 IOPF

Stall 模式是 `CONFIG_ARM_SMMU_V3_SVA` 支持的前提（`arm-smmu-v3.c:3174`）。`arm_smmu_master.stall_enabled`（`arm-smmu-v3.h:945`）标志设备是否配置为 stall 模式。

---

## CPU 页表 vs SMMU 页表对比

```
╔═══════════════════════════════════════════════════════════════╗
║                      CPU 页表                                ║
╠═══════════════════════════════════════════════════════════════╣
║ 输入地址:  VA (Virtual Address)                              ║
║ 根指针:    TTBR0_EL1 / TTBR1_EL1                            ║
║ 页表格式:  ARM64 LPAE (4级)                                  ║
║ 遍历入口:  do_mem_abort → do_page_fault → handle_mm_fault   ║
║           → __handle_mm_fault → handle_pte_fault             ║
║ 错误处理:  fault_info[] 表分派                               ║
║           (fault.c:908)                                      ║
║ 页表管理:  内核 mm 子系统 (mmap, brk, page fault)            ║
║ TLB:       CPU TLB (ITLB + DTLB)                             ║
║ TLB 刷新:  flush_tlb_* / tlbi 指令                           ║
╚═══════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════╗
║                      SMMU 页表                                ║
╠═══════════════════════════════════════════════════════════════╣
║ 输入地址:  IOVA (I/O Virtual Address)                        ║
║ 根指针:    STE → CD (Context Descriptor)                     ║
║ 页表格式:  ARM64 LPAE S1/S2 (与 CPU 格式相同!)               ║
║ 遍历入口:  硬件自动 (IOMMU 按需查找)                          ║
║ 错误处理:  arm_smmu_handle_event → iommu_report_device_fault ║
║           (arm-smmu-v3.c:2136)                                ║
║ 页表管理:  alloc_io_pgtable_ops → LPAE 实现                   ║
║           (io-pgtable-arm.c)                                  ║
║ TLB:       SMMU TLB (IOTLB)                                   ║
║ TLB 刷新:  arm_smmu_cmdq_issue_cmd(TLBI_NH_ASID)             ║
╚═══════════════════════════════════════════════════════════════╝
```

**核心结论**：SMMU Stage-1 页表使用与 CPU 页表**完全相同的 LPAE 格式**。这意味着：
1. 同一套 PGD→PUD→PMD→PTE 结构可以同时理解
2. HMM 遍历 CPU 页表得到的 PFN 可以直接用于填充 SMMU 页表
3. `mmu_notifier`（`arm_smmu_domain.mmu_notifier`，`arm-smmu-v3.h:979`）让 SMMU 感知 CPU 页表变更

---

## SMMU 翻译全流程

```
GPU 发起内存访问 (IOVA)
    │
    ├── SMMU 接收请求，提取 StreamID
    │
    ▼
Stream Table Lookup
    │
    ├── STR TAB Base (SMMU_STRTAB_BASE)
    │   └── 通过 StreamID 索引找到 STE
    │
    ▼
STE (Stream Table Entry)
    │
    ├── STE.S1ContextPtr → 指向 CD 表
    │
    ▼
CD Table
    │
    ├── SSID 索引 (或默认 0) → 找到 Context Descriptor
    │
    ▼
Context Descriptor (CD)
    │
    ├── CD.TTB0 → Stage-1 页表 PGD 基地址
    ├── CD.TCR  → 翻译控制 (与 CPU TTBCR 格式相同!)
    ├── CD.ASID → 地址空间 ID
    │
    ▼
Stage-1 Page Table Walk (硬件自动)
    │
    ├── IOVA[47:39] → PGD[L0] → PUD 物理地址
    ├── IOVA[38:30] → PUD[L1] → PMD 物理地址
    ├── IOVA[29:21] → PMD[L2] → PTE 物理地址 (或 Block 映射)
    ├── IOVA[20:12] → PTE[L3] → 物理页帧号
    │
    ▼
    [可选] Stage-2 Translation
    │
    ├── IPA → Physical Address (由 hypervisor 管理)
    │
    ▼
最终物理地址 + 权限检查
    │
    ├── 翻译成功 → 访问物理内存
    │
    └── 翻译失败 →
            ├── Stall 模式: 构造 iommu_fault → iommu_report_device_fault
            │   └── arm_smmu_handle_event (arm-smmu-v3.c:2136)
            │
            └── Non-Stall 模式:
                ├── PRI (ATS): 设备发起 Page Request
                │   └── arm_smmu_handle_ppr (arm-smmu-v3.c:2295)
                │
                └── 无 ATS: 非致命 abort 返回给设备
```

---

## 统一内存视角：HMM + SMMU 的协同

```
┌───────────────────────────────────────────────────────────────┐
│ CPU 侧                                                         │
│                                                                 │
│  HMM: hmm_range_fault(range)     // 遍历 CPU 页表               │
│    └── walk_page_range(mm, ..., &hmm_walk_ops)                  │
│        └── hmm_vma_walk_pmd → hmm_vma_handle_pte                │
│            └── range->hmm_pfns[i] = pfn | flags                 │
│                                                                 │
│  输出: PFN 数组 (物理页帧号列表)                                │
└──────────────────────────┬────────────────────────────────────┘
                           │
                           │ 将 PFN 填入 SMMU 页表
                           ▼
┌───────────────────────────────────────────────────────────────┐
│ SMMU 侧                                                        │
│                                                                 │
│  arm_smmu_map_pages(domain, iova, paddr, ...)                  │
│    └── ops->map_pages → arm_lpae_map_pages                      │
│        └── __arm_lpae_map(data, iova, paddr, ...)              │
│            └── arm_lpae_init_pte(pte, paddr, prot)             │
│                                                                 │
│  arm_smmu_iova_to_phys(domain, iova)                            │
│    └── ops->iova_to_phys → arm_lpae_iova_to_phys               │
│        └── __arm_lpae_iopte_walk → 返回 PA                     │
│                                                                 │
│  arm_smmu_handle_event(evt)         // 处理 SMMU fault         │
│    └── iommu_report_device_fault()   // → PRI handler           │
└───────────────────────────────────────────────────────────────┘
```

HMM 提供 "读 CPU 页表" 的能力，`alloc_io_pgtable_ops` + `arm_lpae_map_pages` 提供 "写 SMMU 页表" 的能力。两者的桥梁是 HMM 输出的 PFN 数组。

---

## 下一篇文章

→ [第4篇：ATS 与 PRI — 设备侧页表请求与 Page Fault 处理](./04-ats-pri.md)
