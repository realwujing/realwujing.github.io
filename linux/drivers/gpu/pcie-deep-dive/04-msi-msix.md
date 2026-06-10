# 第四篇：MSI/MSI-X 中断机制

> 源码：`drivers/pci/msi/msi.c` `drivers/pci/msi/api.c` | 头文件：`include/linux/pci.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. 概述

中断是设备通知 CPU 的核心机制。PCIe 支持三种中断类型：

| 类型 | 机制 | 向量数 | 可否共享 | 向量独立 mask |
|------|------|--------|---------|--------------|
| INTx (Legacy) | 物理线 (wire-OR) INTA#–INTD# | 4 线 | 是 | 否 |
| MSI | 带内 Memory Write TLP | 最多 32 | 否 | 否 |
| MSI-X | 带内 Memory Write TLP + 向量表 | 最多 2048 | 否 | 是 ✅ |

对于 AI 服务器上的 GPU 和 RDMA 网卡来说，MSI-X 是**唯一合理的选择**，因为：

1. **向量数**：80GB GPU 可能需要每个 SM 一个中断门铃 + 多个队列中断 → 需要几十到几百个向量
2. **Per-vector 独立 mask**：MSI-X 每个向量有自己的 mask 位，驱动可以精确控制哪个向量接收中断
3. **Per-vector 亲和性**：每个向量可以绑定到不同的 CPU 核心，实现 NUMA-aware 中断路由
4. **独立地址/数据**：每个 MSI-X 向量有自己的 Message Address 和 Message Data，不同的向量可以指向不同的 APIC/LAPIC

---

## 2. INTx：传统中断（已基本废弃）

```
                INTA#  INTB#  INTC#  INTD#
                   │      │      │      │
        ┌──────────┼──────┼──────┼──────┼──────┐
        │     ┌────┴──────┴──────┴──────┴────┐ │
        │     │  中断控制器 (I/O APIC)        │ │
        │     └─────────────┬────────────────┘ │
        │                   │  向量             │
        │     ┌─────────────┴────────────────┐ │
        │     │        LAPIC (CPU Core)      │ │
        │     └──────────────────────────────┘ │
        └──────────────────────────────────────┘
```

- 4 根物理线（INTA#–INTD#），4 个单功能设备各用一根；多功能设备按 function 依次轮转
- 电平触发，需要软件显式清除中断源
- **wire-OR 共享**：多个设备共用同一根线，ISR 必须遍历所有共享该线的设备来找到中断源（O(N) 开销）
- 设备没有 MSI/MSI-X 能力时才回退到 INTx

---

## 3. MSI：带内 Memory Write

MSI 不是传统的物理电平信号，而是一种特殊的 **PCIe Memory Write TLP**：

```
┌─────────────────────────────────────────────────────────┐
│   PCIe MSI Memory Write TLP                             │
│                                                         │
│   Header:                                               │
│     Fmt=3DW, Type=MemWr, TC=0, TD=0, EP=0, Attr=0      │
│     Length=1 DW (4 bytes)                               │
│     Requester ID = Bus:Dev.Func                         │
│     Address    = Message Address (来自 MSI Capability)   │
│                                                         │
│   Data Payload (1 DW):                                  │
│     Message Data = Base + vector_number                 │
└─────────────────────────────────────────────────────────┘
```

设备通过向特定地址写入特定数据来触发中断：
- **Message Address**：指向中断控制器（APIC/x2APIC）的内存映射寄存器
- **Message Data**：标识中断向量编号
- Root Complex 收到这个 TLP 后，将其翻译为系统中断，路由到目标 CPU

### 3.1 MSI Capability 结构

```
MSI Capability (PCIe 配置空间):
  Offset  Cap ID   Next Ptr   MSI Control
   0x??   0x05     ->next     [Enable|MultiMsgEn|MultiMsgCap]

  Message Address (32-bit 或 64-bit)
  Message Upper Address (仅 64-bit)
  Message Data
  Mask Bits (可选，Per-Vector Masking)
  Pending Bits (可选)
```

`MultiMsgCap` 表示设备支持的最大向量数（2^n，n=0..5，最多 32）。

`MultiMsgEn` 是内核实际请求的向量数。

### 3.2 MSI 限制

- 最多 32 个向量（规范限制）
- Mask/Pending 位可选，大多数设备不实现 → 没有 fine-grained 控制
- 所有向量共享同一个 Message Address → 不能路由到不同的 CPU 核

---

## 4. MSI-X：下一代中断

MSI-X 解决了 MSI 的所有限制：

| 特性 | MSI | MSI-X |
|------|-----|-------|
| 最大向量数 | 32 | **2048** |
| 向量表位置 | Capability 结构内 | **指向内存中的 BAR** |
| Message Address | 全部向量共享 | **每个向量独立** |
| 每向量 mask | 可选 | **强制** |
| Per-vector 亲和性 | 不支持 | **支持** |

### 4.1 MSI-X Capability 结构

```
MSI-X Capability:
  Offset  Cap ID   Next Ptr   MSI-X Control
   0x??   0x11     ->next     [Enable|Table Size|Func Mask]

  Table Offset  │  BIR  │  Table Offset
    +---+---+---+---+---+---+---+---+
    | BIR |    Table Offset [31:3]  |
    +---+---+---+---+---+---+---+---+
  BIR = BAR Indicator Register (0-5 表示 BAR0-BAR5)

  PBA Offset  │  BIR  │  PBA Offset
    +---+---+---+---+---+---+---+---+
    | BIR |    PBA Offset [31:3]    |
    +---+---+---+---+---+---+---+---+
```

- **Table BIR**：向量表在哪个 BAR 中（通常是 BAR0 或 BAR4）
- **Table Offset**：向量表在 BAR 中的偏移

### 4.2 MSI-X 向量表条目

每个条目 16 字节，位于设备 BAR 映射的内存中：

```
MSI-X Table Entry (per vector):
  Offset   Content
  ------   -----------------------------------------
  0x00     Message Address (lower 32 bits)
  0x04     Message Address (upper 32 bits)
  0x08     Message Data
  0x0C     Vector Control [0:Mask 1:Unmask]
```

这意味着**每个向量可以有不同的 Message Address**！不同的向量可以指向不同 CPU 的 LAPIC，从而实现完美的 IRQ 亲和性。

### 4.3 MSI-X 物理流程

```
GPU (NVIDIA A100) — 128 个 MSI-X 向量
│
│  Vector 0  → Memory Write → APIC ID 0 (CPU 0, NUMA node 0)
│  Vector 1  → Memory Write → APIC ID 1 (CPU 1, NUMA node 0)
│  ...
│  Vector 63 → Memory Write → APIC ID 63 (CPU 63, NUMA node 0)
│  Vector 64 → Memory Write → APIC ID 64 (CPU 64, NUMA node 1)  ← GPU 近端
│  ...
│  Vector 127 → Memory Write → APIC ID 127 (CPU 127, NUMA node 1)

Memory Write TLP:
  ┌──────────────────────────────────────────────┐
  │  Addr: 0xFEE00000 + (APIC_ID << 12)          │
  │  Data: vector + base (e.g. 0x40)             │
  │  Requester: GPU BDF (03:00.0)                │
  └──────────────────────────────────────────────┘
          │
          ▼
    ┌──────────┐     ┌──────────┐
    │Root Cmplx│────▶│ I/O APIC │────▶ CPU Core
    └──────────┘     │  (x2APIC)│
                     └──────────┘
```

---

## 5. struct pci_dev 中的 MSI 字段

`include/linux/pci.h:376-380`：

```c
// pci.h:376-380
    u8      pcie_cap;       /* PCIe capability offset */
    u8      msi_cap;        /* MSI capability offset */
    u8      msix_cap;       /* MSI-X capability offset */
```

这三个字段在 `pci_init_capabilities()` 阶段被设置（`probe.c` 中 `pci_msi_init` / `pci_msix_init` 会先查找 capability 位置）。

---

## 6. pci_msi_supported — 设备能开 MSI 吗？

`drivers/pci/msi/msi.c:29-67`：

```c
// msi/msi.c:29-67
static int pci_msi_supported(struct pci_dev *dev, int nvec)
{
    struct pci_bus *bus;

    /* MSI must be globally enabled and supported by the device */
    if (!pci_msi_enable)
        return 0;                                 // 内核参数 pci=nomsi 禁用了 MSI

    if (!dev || dev->no_msi)
        return 0;                                 // 设备标记为不支持 MSI

    /*
     * You can't ask to have 0 or less MSIs configured.
     *  a) it's stupid ..
     *  b) the list manipulation code assumes nvec >= 1.
     */
    if (nvec < 1)
        return 0;

    /*
     * Any bridge which does NOT route MSI transactions from its
     * secondary bus to its primary bus must set NO_MSI flag on
     * the secondary pci_bus.
     *
     * The NO_MSI flag can either be set directly by:
     * - arch-specific PCI host bus controller drivers (deprecated)
     * - quirks for specific PCI bridges
     *
     * or indirectly by platform-specific PCI host bridge drivers by
     * advertising the 'msi_domain' property, which results in
     * the NO_MSI flag when no MSI domain is found for this bridge
     * at probe time.
     */
    for (bus = dev->bus; bus; bus = bus->parent)
        if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
            return 0;                             // 上游桥不支持 MSI 转发

    return 1;
}
```

**向上遍历 bus tree**：从设备所在 bus 开始，一直走到 root bus，检查路径上每一段 bus 是否设置了 `PCI_BUS_FLAGS_NO_MSI`。由于 MSI 是 Memory Write TLP，如果某个桥不能正确转发，整个子树都不能用 MSI。

---

## 7. API 总览：驱动怎么获得中断

驱动的通用中断申请流程：

```c
// Step 1: 申请中断向量
int nvecs = pci_alloc_irq_vectors(dev, min_vecs, max_vecs, flags);

// Step 2: 获取 Linux IRQ 编号
int irq = pci_irq_vector(dev, vector_idx);

// Step 3: 注册中断处理函数
int ret = request_irq(irq, handler, 0, "my_driver", dev);

// Step 4: 释放
pci_free_irq_vectors(dev);
```

---

## 8. pci_alloc_irq_vectors

`drivers/pci/msi/api.c:205-237`：

```c
// msi/api.c:232-237
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
              unsigned int max_vecs, unsigned int flags)
{
    return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs,
                          flags, NULL);
}
EXPORT_SYMBOL(pci_alloc_irq_vectors);
```

这是对 `pci_alloc_irq_vectors_affinity` 的简单包装，`affd=NULL` 表示不需要特殊亲和性。

---

## 9. pci_alloc_irq_vectors_affinity — 核心分配逻辑

`drivers/pci/msi/api.c:241-296`：

```c
// msi/api.c:252-296
int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
                   unsigned int max_vecs, unsigned int flags,
                   struct irq_affinity *affd)
{
    struct irq_affinity msi_default_affd = {0};
    int nvecs = -ENOSPC;

    if (flags & PCI_IRQ_AFFINITY) {
        if (!affd)
            affd = &msi_default_affd;
    } else {
        if (WARN_ON(affd))
            affd = NULL;
    }

    // ═══ Priority 1: MSI-X ═══
    if (flags & PCI_IRQ_MSIX) {
        nvecs = __pci_enable_msix_range(dev, NULL, min_vecs, max_vecs,
                        affd, flags);
        if (nvecs > 0)
            return nvecs;              // MSI-X 成功 → 直接返回
    }

    // ═══ Priority 2: MSI ═══
    if (flags & PCI_IRQ_MSI) {
        nvecs = __pci_enable_msi_range(dev, min_vecs, max_vecs, affd);
        if (nvecs > 0)
            return nvecs;              // MSI 成功 → 返回
    }

    // ═══ Priority 3: INTx ═══
    if (flags & PCI_IRQ_INTX) {
        if (min_vecs == 1 && dev->irq) {
            if (affd)
                irq_create_affinity_masks(1, affd);
            pci_intx(dev, 1);          // 使能 INTx
            return 1;                  // 返回 1 个向量
        }
    }

    return nvecs;                        // 全部失败 → -ENOSPC
}
EXPORT_SYMBOL(pci_alloc_irq_vectors_affinity);
```

优先级硬编码在代码中：**MSI-X > MSI > INTx**。

- **Line 267 (MSI-X)**：如果 flags 包含 `PCI_IRQ_MSIX`，首先尝试 MSI-X
- **Line 274 (MSI)**：MSI-X 失败再试 MSI
- **Line 281 (INTx)**：MSI 失败再回退到 INTx，但只有 `min_vecs==1` 时才允许（INTx 只能提供 1 个中断线）

### 9.1 flags 组合示例

```c
// GPU 驱动：要 MSI-X，MSI 作为 fallback
nvecs = pci_alloc_irq_vectors(dev, 1, 128,
    PCI_IRQ_MSIX | PCI_IRQ_MSI | PCI_IRQ_AFFINITY);

// RDMA 网卡：只要 MSI-X，不接受 fallback
nvecs = pci_alloc_irq_vectors(dev, 64, 64,
    PCI_IRQ_MSIX | PCI_IRQ_AFFINITY);

// 老设备：MSI 也行
nvecs = pci_alloc_irq_vectors(dev, 1, 32, PCI_IRQ_MSI);
```

---

## 10. pci_irq_vector — 获取 Linux IRQ 编号

`drivers/pci/msi/api.c:300-319`：

```c
// msi/api.c:311-319
int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
    unsigned int irq;

    if (!dev->msi_enabled && !dev->msix_enabled)
        return !nr ? dev->irq : -EINVAL;          // INTx 模式：只有 nr=0 有效

    irq = msi_get_virq(&dev->dev, nr);
    return irq ? irq : -EINVAL;
}
EXPORT_SYMBOL(pci_irq_vector);
```

`nr` 的含义取决于当前中断模式：

| 模式 | nr 含义 |
|------|---------|
| MSI-X | MSI-X 向量表中的索引（0-based） |
| MSI | 已启用的 MSI 向量索引（0-based） |
| INTx | 必须为 0 |

`msi_get_virq()` 从 MSI 描述符中获取对应的 Linux IRQ 编号（virq）。

---

## 11. pci_free_irq_vectors

`drivers/pci/msi/api.c:366-383`：

```c
// msi/api.c:379-383
void pci_free_irq_vectors(struct pci_dev *dev)
{
    pci_disable_msix(dev);
    pci_disable_msi(dev);
}
EXPORT_SYMBOL(pci_free_irq_vectors);
```

非常简单：依次 disable MSI-X 和 MSI。注意如果 MSI-X 启用，disable MSI 是 no-op；如果两者都未启用（INTx 模式），两个 disable 都是 no-op。

---

## 12. GPU/RDMA 为什么必须用 MSI-X

### 12.1 Per-vector 独立亲和性

GPU 通过 NVLink 或 PCIe switch 连接到特定的 NUMA 节点。MSI-X 允许把不同队列的中断路由到不同 NUMA 节点的 CPU：

```
GPU 在 NUMA node 1:
  MSI-X vector 0-31  → CPU 0-31  (NUMA node 0, 远端)
  MSI-X vector 32-63 → CPU 32-63 (NUMA node 1, 近端)  ★ 低延迟

RDMA 网卡在 NUMA node 0:
  MSI-X vector 0-63  → CPU 0-63  (NUMA node 0, 近端)  ★ 数据就在本地
```

MSI 不支持这种分级，所有向量共享同一个 Message Address，只能路由到同一个 APIC。

### 12.2 Per-vector 独立 Mask

MSI-X 向量表条目中的 Vector Control 字段允许**每个向量独立 mask/unmask**。这对中断处理性能很重要：

```c
// MSI-X: 可以 mask 单个繁忙的向量，其他向量继续接收中断
// MSI:  全局 mask，一旦 mask 所有向量都不响应
```

### 12.3 大量向量支持

GPU 的典型需求：
- 每个 CE（Copy Engine）：1–2 个向量
- 每个 GR（Graphics/Compute Engine）：1–2 个向量
- 每个 NVENC/NVDEC：1–2 个向量
- 最多可达到 128+ 个向量

MSI 上限 32 不够用，只有 MSI-X 的 2048 能满足。

---

## 13. 驱动典型用法

```c
static int my_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int i, nvecs, irq;

    // 申请 MSI-X 中断
    nvecs = pci_alloc_irq_vectors(pdev, MIN_VECS, MAX_VECS,
                   PCI_IRQ_MSIX | PCI_IRQ_AFFINITY);
    if (nvecs < 0) {
        // Fallback: 试试 MSI
        nvecs = pci_alloc_irq_vectors(pdev, 1, 32, PCI_IRQ_MSI);
        if (nvecs < 0) {
            // 最后尝试 INTx
            dev_err(&pdev->dev, "No interrupt available\n");
            return nvecs;
        }
    }

    // 检查 MSI-X 是否实际启用
    if (pdev->msix_enabled)
        dev_info(&pdev->dev, "MSI-X enabled: %d vectors\n", nvecs);
    else if (pdev->msi_enabled)
        dev_info(&pdev->dev, "MSI enabled: %d vectors\n", nvecs);

    // 为每个向量注册中断处理函数
    for (i = 0; i < nvecs; i++) {
        irq = pci_irq_vector(pdev, i);
        ret = request_irq(irq, my_handler, 0, "mydrv-vec", &my_data[i]);
        if (ret)
            goto free_vectors;
    }

    return 0;

free_vectors:
    pci_free_irq_vectors(pdev);
    return ret;
}
```

---

## 14. MSI Domain 与 IRQ 分配层级

```
pci_alloc_irq_vectors()
    │
    ├── PCI_IRQ_MSIX → __pci_enable_msix_range()
    │       ├── pci_msix_vec_count()          → 读 MSI-X Table Size
    │       ├── msi_domain_alloc_irqs()       → IRQ domain 层分配
    │       │       └── 分配 Linux IRQ + MSI 描述符
    │       ├── msix_capability_init()        → 写向量表 (Message Addr/Data)
    │       └── pci_msix_set_ctrl(ENABLE)     → 置位 MSI-X Enable
    │
    ├── PCI_IRQ_MSI  → __pci_enable_msi_range()
    │       ├── msi_capability_init()
    │       │       └── 写 MSI Capability 中的 Message Addr/Data
    │       └── pci_msi_set_enable(ENABLE)
    │
    └── PCI_IRQ_INTX → pci_intx(dev, 1)
            └── 写 Command Register bit[10] (Interrupt Disable = 0)
```

IRQ domain 层负责将虚拟 IRQ 映射到硬件向量号。在 x86 上，最终通过 APIC/x2APIC 完成消息路由。

---

## 15. 总结

| 问题 | 答案 |
|------|------|
| 三种中断优先级？ | MSI-X > MSI > INTx（硬编码在 `pci_alloc_irq_vectors_affinity` 中） |
| GPU 为什么用 MSI-X？ | 独立亲和性、Per-vector mask、>32 向量需求 |
| 每向量独立地址怎么实现？ | MSI-X 向量表每个条目有独立的 Message Address/Data |
| nvecs 上限？ | MSI 32，MSI-X 2048 |
| `pci_irq_vector(dev, nr)` 怎么映射？ | MSI-X: 表索引 → msi_get_virq；INTx: 只能 nr=0 |

有了 BAR（可以读写寄存器）和 MSI-X（可以接收中断），驱动终于可以和设备对话了。下一篇文章将讨论 PCIe 错误处理 — 当链路出错时，内核如何检测和恢复。

---

## 下一篇文章

[第五篇：AER 高级错误报告](./05-aer.md)
