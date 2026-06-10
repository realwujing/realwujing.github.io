# SoC 统一内存架构深度解析 — 系列导读

> 源码：`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c`, `drivers/pci/ats.c`, `drivers/iommu/io-pgfault.c`, `mm/hmm.c`, `drivers/gpu/drm/nouveau/nouveau_svm.c`

本系列是 [nvidia-svm 深度解析](../nvidia-svm/README.md) 的姊妹篇。两者的核心区别在于：

- **Discrete GPU（nvidia-svm 系列）**：GPU 拥有独立的 VRAM，HMM 负责迁移页面、`DEVICE_PRIVATE` 内存、通过 `migrate_vma` 搬运数据。
- **SoC 统一内存（本系列）**：CPU 和 GPU 共享**同一物理内存**，没有独立 VRAM。GPU 通过 SMMU 访问物理内存，**CPU 页表和 SMMU 页表是同一套物理页表**。不需要迁移，不需要 DMA 拷贝，翻译即完成。

虽然 `hmm_range_fault`、`drm_gpusvm`、`nouveau_svm` 等核心代码对两种架构是**同一份内核代码**，本系列将镜头聚焦在 SoC 特有的层次：SMMU 页表、ATS/PRI 协议、IOPF 框架以及完整时序。

---

## 目录

| 篇目 | 标题 | 核心内容 |
|------|------|----------|
| 01 | [SMMU 与 io-pgtable — 为什么 CPU 和 GPU 共享页表？](./01-smmu-io-pgtable.md) | ARM SMMUv3 架构、io-pgtable 格式与 CPU MMU 页表的一致性 |
| 02 | [HMM — CPU 页表的镜像读取](./02-hmm.md) | `hmm_range_fault` 如何读取 CPU MMU 页表、返回 PFN 数组 |
| 03 | [IOPF — 缺页中断的硬件到软件路径](./03-iopf.md) | iommu_report_device_fault、IOPF 队列、故障分组与响应 |
| 04 | [ATS/PRI — 页表共享的最后一块拼图](./04-ats-pri.md) | ATS 缓存、PRI 缺页请求、PASID 多进程支持 |
| 05 | [SoC 统一内存完整时序 — 从 GPU 缺页到页表填充](./05-soc-timing.md) | 端到端时序、Discrete vs SoC 对比、代码调用链 |

---

## 前置知识

- 理解 CPU MMU 页表的基本概念（PGD/PUD/PMD/PTE 四级页表）
- 了解 Linux IOMMU 子系统的基本角色（`iommu_ops`、domain、group）
- 阅读过 [nvidia-svm 系列](../nvidia-svm/README.md) 的前几篇（尤其是 HMM 部分）会有帮助，但不是必须
- 具备 PCIe 协议的基本概念（ATS/PRI 是 PCIe 扩展能力）

---

## 架构总览

```
┌───────────────────────────────────────────────┐
│        SoC Unified Memory Stack               │
│                                               │
│  GPU 缺页 ──→ SMMU stall ──→ IOPF ──→ handler │
│       ↑                           │           │
│       │    ATS (TLB cache)        │           │
│       │    PRI (page request)     │           │
│       └───────────────────────────┘           │
│                                               │
│  HMM: hmm_range_fault → 读 CPU 页表 → PFN[]  │
│  SMMU: io-pgtable → Stage-1 翻译 → IOVA→PA   │
│                                               │
│  CPU 页表 ←── 同一格式 ──→ SMMU 页表          │
└───────────────────────────────────────────────┘
```

整个 SoC 统一内存栈可以看作三层：

1. **协议层**：ATS 提供 GPU 本地 TLB 缓存（ATC），PRI 允许 GPU 在 TLB miss 时主动请求页表填充。PASID 让多个 GPU 上下文（进程）共享同一个 SMMU。
2. **中断传递层**：SMMU 事件队列捕获翻译故障 → `iommu_report_device_fault` → IOPF 框架分组故障 → 调用 driver 的 `iopf_handler`。
3. **缺页处理层**：`iopf_handler` 内部调用 `hmm_range_fault` 扫描 CPU 页表 → 获取 PFN → 写入 SMMU 页表 → 通过 `arm_smmu_page_response` 通知 GPU 重试。

---

## 与 nvidia-svm 系列的关系

| 维度 | Discrete GPU (H100) | SoC 统一内存 (NVIDIA+MTK) |
|------|---------------------|---------------------------|
| 物理内存 | GPU 独立 VRAM + 系统 DDR | 统一物理内存（无 VRAM） |
| 设备页表 | GPU 内部 MMU 页表，由驱动管理 | SMMU 页表 = CPU 页表（同格式） |
| 缺页处理 | GPU 内部 fault buffer → work handler → `hmm_range_fault` | PRI → SMMU event queue → IOPF → `hmm_range_fault` |
| 页面迁移 | `DEVICE_PRIVATE` + `migrate_vma` → 搬运到 VRAM | 无需迁移，翻译即完成 |
| 页表写入 | `nvif_vmm_pfnmap` 写 GPU 内部页表 | `arm_lpae_map_pages` 写 SMMU 页表（即 CPU 页表） |
| DMA 拷贝 | 可能发生 | 不发生 |

相同的内核代码（`mm/hmm.c`、`drivers/gpu/drm/drm_gpusvm.c`、`drivers/gpu/drm/nouveau/nouveau_svm.c`）在两种架构上都能工作 — 差异被封装在 IOMMU 层（SMMU 驱动）和 GPU 驱动（nouveau）中。

---

## 开始阅读

建议按目录顺序阅读。如果你已经熟悉 SMMU 和 HMM 基础，可以直接跳到 [第四篇：ATS/PRI](./04-ats-pri.md) 了解协议层，或 [第五篇：完整时序](./05-soc-timing.md) 看端到端全貌。
