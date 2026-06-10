# PCIe 内核源码深度解析 — 系列导读

> AI 服务器的物理互联基础：从设备枚举到错误恢复

## 阅读指南

| # | 文章 | 核心源码 | 学完能理解 |
|---|------|---------|-----------|
| 1 | [枚举：内核怎么发现 PCIe 设备](01-enumeration.md) | `probe.c` `ecam.c` | 加电后 GPU/NIC/NVMe 怎么被发现 |
| 2 | [BAR/MMIO：设备的寄存器窗口](02-bar-mmio.md) | `setup-res.c` `setup-bus.c` | GPU VRAM BAR 怎么分配，64-bit 和 prefetchable |
| 3 | [驱动注册与匹配](03-driver-probe.md) | `pci-driver.c` `search.c` | `struct pci_dev` 和 `struct pci_driver` 怎么对上 |
| 4 | [MSI/MSI-X 中断机制](04-msi-msix.md) | `msi/msi.c` `msi/api.c` | 为什么 GPU/RDMA 网卡都用 MSI-X |
| 5 | [AER 高级错误报告](05-aer.md) | `pcie/aer.c` `pcie/err.c` | GPU PCIe 错误怎么检测和恢复 |
| 6 | [热插拔与电源管理](06-hotplug-pm.md) | `hotplug/pciehp_*.c` `pcie/aspm.c` | GPU 热插拔怎么走，ASPM 省电机制 |
| 7 | [PCIe Port 服务模型](07-port-service.md) | `pcie/portdrv.c` `pcie/dpc.c` | AER/HP/PME/DPC 服务怎么分发 |

## 阅读顺序

```
01 枚举 ──→ 02 BAR/MMIO ──→ 03 驱动probe
                                    │
                         ┌──────────┼──────────┐
                         ▼          ▼          ▼
                   04 MSI-X     05 AER     06 热插拔/PM
                         │          │          │
                         └──────────┼──────────┘
                                    ▼
                             07 Port服务模型
```

前 3 篇必须顺序读，4/5/6 可独立阅读，7 是后三篇的基石。

## 与已有专栏的关系

| 本系列 | 已有系列 |
|--------|---------|
| BAR/MMIO (02) | `nvidia-svm/09` GPUDirect RDMA 依赖 BAR 映射 |
| MSI-X (04) | RDMA 网卡 GPUDirect 依赖 MSI-X 中断 |
| AER (05) | GPU 出错时 AER 触发 `error_detected` → `reset_subordinates` |
| ATS/PRI | `soc-unified-memory/04` 讲 ATS/PRI 协议，本系列讲 ATS capability 从哪来 (enumerate) |

## 核心源码索引

| 文件 | 核心内容 |
|------|---------|
| `drivers/pci/probe.c` | `pci_scan_device`(L2602) `pci_init_capabilities`(L2655) `pci_scan_child_bus_extend`(L3087) |
| `drivers/pci/pci-driver.c` | `pci_match_device`(L136) `pci_device_probe`(L473) `pci_bus_type`(L1732) |
| `drivers/pci/msi/api.c` | `pci_alloc_irq_vectors`(L205) `pci_irq_vector`(L300) |
| `drivers/pci/pcie/aer.c` | `aer_err_source`(L49) `aer_irq`(L1470) `aer_isr`(L1449) |
| `drivers/pci/pcie/err.c` | `report_error_detected`(L49) `pcie_do_recovery`(L210) |
| `drivers/pci/pcie/aspm.c` | `pcie_link_state`(L228) `pci_configure_aspm_l1ss`(L70) |
| `drivers/pci/pcie/portdrv.c` | `pcie_port_service_register`(L586) |
| `include/linux/pci.h` | `pci_dev`(L351) `pci_driver`(L1021) `pci_ops`(L872) |