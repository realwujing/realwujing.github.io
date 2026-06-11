# Intel IOMMU (VT-d) 内核源码深度解析 — 系列导读

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
>
> 从 ACPI DMAR 表到嵌套翻译：Intel VT-d 的完整软件实现

## 阅读指南

| # | 文章 | 核心源码 | 学完能理解 |
|---|------|---------|-----------|
| 1 | [DMAR 发现 — ACPI表的解析与IOMMU注册](01-dmar.md) | `dmar.c` `iommu.c` (init) | IOMMU 硬件怎么被内核发现和注册 |
| 2 | [IOMMU 引擎 — 硬件寄存器、关键数据结构与初始化](02-engine.md) | `iommu.h` `iommu.c` (probe) | CAP/ECAP 寄存器、Root/Context table、设备 probe 全流程 |
| 3 | [DMA 重映射 — 页表遍历、TLB 刷新与缓存标签](03-dma-remap.md) | `iommu.c` `cache.c` | DMA 页表如何走，QI 队列怎么做 IOTLB invalidation |
| 4 | [中断重映射与 PASID — DMA 之外的能力](04-irq-pasid.md) | `irq_remapping.c` `pasid.c` | MSI/MSI-X 怎么重映射，PASID 怎么给进程分配地址空间 |
| 5 | [SVA、嵌套翻译与 IOMMUFD — VT-d 的未来](05-sva-nested.md) | `svm.c` `nested.c` `prq.c` `perfmon.c` | 共享虚拟地址 SVA，用户态 IOMMU 控制面 |

## 阅读顺序

```
01 DMAR 发现 ──→ 02 引擎与数据结构 ──→ 03 DMA 重映射
                                          │
                               ┌──────────┼──────────┐
                               ▼          ▼          ▼
                         04 中断重映射  04 PASID   05 SVA/嵌套/IOMMUFD
```

前 3 篇必须顺序读，第 4 篇两节可独立阅读，第 5 篇需要前 4 篇基础。

## 前置知识

- **VT-d 概念**：Intel VT-d 通过 IOMMU 对 DMA/中断进行地址转换和访问控制，将设备 IOVA（I/O Virtual Address）映射到 HPA（Host Physical Address）
- **[PCIe 内核源码深度解析](../pcie-deep-dive/README.md)**：理解 PCIe 设备枚举、BDF 标识、ATS/PRI 能力
- **[ARM SMMU 内核源码深度解析](../arm-smmu/README.md)**：对标 ARM 侧实现，对比理解 x86/ARM 差异

## 核心架构图

```
ACPI DMAR Table (BIOS 提供)
│
├── DRHD (DMA Remapping Hardware Definition) ── 每个描述一个 IOMMU 硬件单元
│   │
│   ├─→ struct dmar_drhd_unit (dmar.h:38) ──→ struct intel_iommu (iommu.h:682)
│   │                                          │
│   │   reg_base_addr ───────────────────────── reg_phys → MMIO 寄存器
│   │   devices[] ────────────────────────────  scope 设备列表
│   │   include_all ──────────────────────────  全局覆盖标志
│   │
│   └─→ iommu->root_entry ──→ Root Table (256 entries per bus)
│           │                       │
│           │                       ├── root_entry.lctp → Context Table (lower)
│           │                       └── root_entry.uctp → Context Table (upper)
│           │                               │
│           │                               ├── context_entry: 3-level DMA PT
│           │                               ├── context_entry: 5-level DMA PT (LA57)
│           │                               └── context_entry: PASID → PASID Table
│           │                                       │
│           │                                       ├── pasid_entry[FL_ONLY]  → 1st-level PT (SVA)
│           │                                       ├── pasid_entry[SL_ONLY]  → 2nd-level PT (IOVA)
│           │                                       ├── pasid_entry[NESTED]   → 1st-level on 2nd-level (VM)
│           │                                       └── pasid_entry[PT]       → pass-through
│           │
│           ├── qi (Queued Invalidation) ──→ IOTLB / devTLB / context cache flush
│           ├── ir_table ──→ Interrupt Remapping Table (IRTE)
│           ├── prq ──→ Page Request Queue (PRI)
│           └── pmu ──→ Performance Monitoring Unit
│
├── RMRR (Reserved Memory Region Reporting)
│   └── dmar_rmrr_units ── 保留内存区域（BIOS/内核不可重映射范围）
│
├── ATSR (ATS Root Port)
│   └── dmar_atsr_units ── 支持 ATS 的根端口
│
├── RHSA (NUMA Node Affinity)
│   └── 每个 IOMMU 所属 NUMA 节点
│
├── ANDD (ACPI Namespace Device)
│   └── 非 PCI 设备的 IOMMU 关联
│
└── SATC (SoC ATC)
    └── SoC 集成设备的 ATC 信息
```

## Intel VT-d vs ARM SMMUv3 速览

| 维度 | Intel VT-d | ARM SMMUv3 |
|------|-----------|-----------|
| 发现方式 | ACPI DMAR 表 | ACPI IORT 表 / Device Tree |
| 页表格式 | x86 单页表 / 嵌套 | ARM 标准页表 (stage-1+stage-2) |
| 中断重映射 | IRTE 表 (x2APIC) | 无独立中断重映射（使用 ITS/GICv4） |
| PASID 支持 | 20-bit PASID, VT-d 3.0+ | 20-bit SubStreamID |
| PRI (Page Request) | PRQ + Page Request Descriptor | PRI 队列 (SMMUv3.2+) |
| 配置空间 | 通过 PCIe BDF 绑定 | 通过 StreamID 绑定 (SID) |
| 软件接口 | `intel_iommu_ops` (iommu.c:3913) | `arm_smmu_ops` |

## 核心源码索引

| 文件 | 行数 | 核心内容 |
|------|------|---------|
| `drivers/iommu/intel/dmar.c` | 2391 | DMAR 表解析、DRHD/RMRR/ATSR 构建、QI 提交 |
| `drivers/iommu/intel/iommu.c` | 4227 | IOMMU 主驱动：init/probe/DMA map/DMA unmap/TLB flush |
| `drivers/iommu/intel/iommu.h` | 1388 | 核心数据结构：intel_iommu, dmar_domain, root/context entry |
| `drivers/iommu/intel/cache.c` | 534 | cache_tag 批量 invalidation 策略 |
| `drivers/iommu/intel/irq_remapping.c` | 1605 | 中断重映射：IRTE 分配/配置/x2APIC 支持 |
| `drivers/iommu/intel/pasid.c` | 984 | PASID 表管理：分配/回收/first-level/second-level/nested 配置 |
| `drivers/iommu/intel/pasid.h` | 326 | PASID entry 位操作宏 |
| `drivers/iommu/intel/svm.c` | 235 | Shared Virtual Memory：MMU notifier/SVA 绑定 |
| `drivers/iommu/intel/nested.c` | 241 | 嵌套翻译：stage-1 on stage-2 |
| `drivers/iommu/intel/prq.c` | 396 | Page Request Queue：PRI 页面故障处理 |
| `drivers/iommu/intel/perfmon.c` | 790 | VT-d 4.0+ 性能计数器 |
| `include/linux/dmar.h` | 301 | DMAR 全局接口 |
