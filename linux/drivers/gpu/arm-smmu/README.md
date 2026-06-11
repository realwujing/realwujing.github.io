# ARM SMMU 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
>
> 系列文章基于 Linux 内核源码，逐行解析 ARM System Memory Management Unit (SMMU) 驱动实现。

## 系列目录

| 篇序 | 标题 | 涉及文件 | 简述 |
|------|------|----------|------|
| 第一篇 | [架构全景：SMMUv3 的硬件模型与初始化](./01-architecture.md) | `arm-smmu-v3.c` (5625 行), `arm-smmu-v3.h` (1229 行) | SMMU 世代演进、核心 struct 全景、probe 流程与硬件特性检测 |
| 第二篇 | [流表、命令队列与事件队列](./02-stream-table.md) | `arm-smmu-v3.h` (STE/CD/Queue structs), `arm-smmu-v3.c` (queue init, event handling) | STE/CD 硬件格式、CMDQ/EVTQ/PRIQ 三队列、事件处理与错误报告 |
| 第三篇 | [页表引擎：LPAE 格式与 Stage 1/2](./03-page-table.md) | `io-pgtable-arm.c` (1267 行), `arm-smmu-v3.c` (`domain_finalise`) | ARM LPAE 页表格式、4 级页表遍历、Stage-1/Stage-2 配置与 quirk |
| 第四篇 | [SVA 与 IOMMUFD：共享虚拟地址与嵌套翻译](./04-sva-iommufd.md) | `arm-smmu-v3-sva.c` (348 行), `arm-smmu-v3-iommufd.c` (487 行), `tegra241-cmdqv.c` (1293 行) | SVA 实现、IOMMUFD 嵌套域、Tegra241 虚拟命令队列 |

## 前置知识

建议在阅读本系列前，先了解以下概念：

1. **SMMU 基础概念**
   - SMMU 即 System MMU，是 ARM 体系下的 IOMMU，负责将外设 DMA 地址（IOVA）翻译为物理地址（PA）
   - 核心翻译场景：StreamID → STE（Stream Table Entry）→ Context Descriptor → 页表
   - 与 Intel VT-d 的对应关系：StreamID ↔ RequesterID, STE ↔ Context Entry, CD ↔ PASID Entry

2. **推荐前置系列**
   - [SoC 统一内存架构系列](https://realwujing.github.io/linux/drivers/soc/soc-unified-memory/) — DMA 与 cache coherence 基础
   - [PCIe 深度解析系列](https://realwujing.github.io/linux/drivers/pci/pcie-deep-dive/) — ATS/PRI/PASID 的 PCIe 层实现

3. **ARM 架构手册**
   - *ARM IHI 0070*: SMMUv3 Architecture Specification
   - *ARM DDI 0487*: ARM Architecture Reference Manual (VMSA 章节)

## SMMUv3 架构总览

```
                          ┌─────────────────────────────────────────┐
                          │            ARM SMMUv3 硬件               │
                          │                                         │
   PCIe Device            │  Stream Table     Context Descriptors   │
   ┌──────────┐           │  ┌──────────┐     ┌──────────┐         │
   │StreamID=5│──────────▶│  │ STE[5]   │────▶│  CD[0]   │         │
   │SSID=0    │           │  │ CFG=S1   │     │ TTB0 ────┼────┐    │
   │DMA Addr  │           │  │ S1CTXPTR │     │ ASID=3   │    │    │
   └──────────┘           │  └──────────┘     └──────────┘    │    │
                          │                                    │    │
                          │  ┌──────────┐                      │    │
   PCIe Device            │  │ STE[9]   │     ┌──────────┐    │    │
   ┌──────────┐           │  │ CFG=S2   │────▶│  S2 页表  │    │    │
   │StreamID=9│──────────▶│  │ S2TTB    │     │(IPA→PA)  │    │    │
   │DMA Addr  │           │  │ VMID=5   │     └──────────┘    │    │
   └──────────┘           │  └──────────┘                      │    │
                          │                                    │    │
                          │  ┌──────────────────────────────────┘    │
                          │  │  Stage-1 页表 (VA→IPA)                │
                          │  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐        │
                          │  └─▶│PGD │→│PUD │→│PMD │→│PTE │→PA     │
                          │     └────┘ └────┘ └────┘ └────┘        │
                          │                                         │
                          │  命令队列(CMDQ)  事件队列(EVTQ)          │
                          │  ┌────────────┐  ┌────────────┐        │
                          │  │TLBI NH_ASID│  │F_TRANSLATION│        │
                          │  │ATC_INV     │  │F_PERMISSION │        │
                          │  │CMD_SYNC    │  │C_BAD_STE    │        │
                          │  └────────────┘  └────────────┘        │
                          └─────────────────────────────────────────┘
```

**关键数据流**：

1. **设备发起 DMA** → StreamID 索引 Stream Table → 找到 STE
2. **STE 判断翻译模式**：
   - `CFG_S1_TRANS`: STE → S1ContextPtr → CD → TTB0 → S1 页表 → VA→IPA
   - `CFG_S2_TRANS`: STE → S2TTB → S2 页表 → IPA→PA
   - `CFG_NESTED`: 先 S1 翻译再 S2 翻译（嵌套）
   - `CFG_BYPASS`: 直通，不翻译
   - `CFG_ABORT`: 中止所有 transaction
3. **TLB 未命中** → SMMU 硬件自动页表遍历（PTW）
4. **页表修改** → 软件通过 CMDQ 发送 TLB/ATC 无效化命令

## 代码导航

| 文件 | 路径 | 职责 |
|------|------|------|
| `arm-smmu-v3.c` | `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c` | SMMUv3 驱动主体 (5625 行) |
| `arm-smmu-v3.h` | `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h` | 数据结构定义与寄存器宏 (1229 行) |
| `arm-smmu-v3-sva.c` | `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-sva.c` | SVA (Shared Virtual Addressing) 支持 (348 行) |
| `arm-smmu-v3-iommufd.c` | `drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-iommufd.c` | iommufd 嵌套翻译支持 (487 行) |
| `tegra241-cmdqv.c` | `drivers/iommu/arm/arm-smmu-v3/tegra241-cmdqv.c` | NVIDIA Tegra241 虚拟命令队列扩展 (1293 行) |
| `io-pgtable-arm.c` | `drivers/iommu/io-pgtable-arm.c` | 通用 ARM LPAE 页表分配器 (1267 行) |
| `arm-smmu-qcom.c` | `drivers/iommu/arm/arm-smmu/arm-smmu-qcom.c` | Qualcomm SMMU 实现扩展 (838 行) |

## ARM SMMU 三代演进

| 特性 | SMMUv1 | SMMUv2 | SMMUv3 |
|------|--------|--------|--------|
| 流匹配 | StreamID 匹配 | StreamID 匹配 | Stream Table (优化) |
| Stage-2 翻译 | 无 | 支持 | 支持 |
| ATS | 无 | 支持 | 支持 (增强) |
| PRI | 无 | 支持 | 支持 (增强) |
| MSI 中断 | 无 | 无 | 支持 |
| 命令队列 | 无 | 无 | CMDQ (环形缓冲) |
| SVA | 无 | 无 | 支持 (PASID) |
| 嵌套翻译 | 无 | 无 | 支持 (iommufd) |

## 下一篇文章

[第一篇：架构全景：SMMUv3 的硬件模型与初始化](./01-architecture.md)
