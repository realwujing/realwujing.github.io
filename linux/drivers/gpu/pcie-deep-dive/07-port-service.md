# 第七篇：PCIe Port 服务模型

> 源码：`drivers/pci/pcie/portdrv.c` (849 lines) | `drivers/pci/pcie/portdrv.h` (138 lines) | `drivers/pci/pcie/aer.c` (1740 lines) | `drivers/pci/pcie/dpc.c` (536 lines) | `drivers/pci/pcie/pme.c` (473 lines) | `drivers/pci/hotplug/pciehp_core.c` (384 lines)

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. 问题：一个 Root Port 中断如何服务多种能力？

PCIe Root Port（或 Downstream Port）同时具备多种能力：

```
┌──────────────────────────────────────────────────┐
│                  Root Port                        │
│                                                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐          │
│  │  AER     │ │ Hotplug  │ │  PME     │          │
│  │ 错误报告  │ │ 热插拔    │ │ 唤醒事件  │          │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘          │
│       │             │             │                │
│  ┌────┴─────┐ ┌────┴─────┐      │                │
│  │  DPC     │ │ BWCTRL   │      │                │
│  │ 端口隔离  │ │ 带宽控制  │      │                │
│  └────┬─────┘ └────┬─────┘      │                │
│       │             │            │                │
│       └──────┬──────┴────────────┘                │
│              │ 共享一根物理中断线                    │
│              ▼                                    │
│         INTx / MSI / MSI-X                        │
└──────────────────────────────────────────────────┘
```

如果简单地把所有处理逻辑塞进一个中断处理函数，会导致：
- **强耦合**：修改热插拔代码可能破坏 AER 处理
- **不可独立加载**：某些场景不需要热插拔但也必须链接
- **单一故障域**：一个服务崩溃会影响所有其他服务
- **中断不可细分**：无法利用 MSI-X 的能力为不同服务分配独立向量

PCIe Port Bus Driver (简称 portdrv) 就是为解决这个问题而设计的：它将 Root/Upstream/Downstream Port 的多个能力拆解为独立的**服务 (service)**，每个服务拥有自己的 `struct pcie_device`、独立的 probe/remove 生命周期，以及（在 MSI-X 模式下的）独立中断向量。

---

## 2. 服务位掩码定义 (`portdrv.h:15-25`)

```c
// drivers/pci/pcie/portdrv.h:15-25
/* Service Type */
#define PCIE_PORT_SERVICE_PME_SHIFT    0    /* Power Management Event */
#define PCIE_PORT_SERVICE_PME          (1 << PCIE_PORT_SERVICE_PME_SHIFT)
#define PCIE_PORT_SERVICE_AER_SHIFT    1    /* Advanced Error Reporting */
#define PCIE_PORT_SERVICE_AER          (1 << PCIE_PORT_SERVICE_AER_SHIFT)
#define PCIE_PORT_SERVICE_HP_SHIFT     2    /* Native Hotplug */
#define PCIE_PORT_SERVICE_HP           (1 << PCIE_PORT_SERVICE_HP_SHIFT)
#define PCIE_PORT_SERVICE_DPC_SHIFT    3    /* Downstream Port Containment */
#define PCIE_PORT_SERVICE_DPC          (1 << PCIE_PORT_SERVICE_DPC_SHIFT)
#define PCIE_PORT_SERVICE_BWCTRL_SHIFT 4    /* Bandwidth Controller */
#define PCIE_PORT_SERVICE_BWCTRL       (1 << PCIE_PORT_SERVICE_BWCTRL_SHIFT)

#define PCIE_PORT_DEVICE_MAXSERVICES   5
```

5 个服务，用 5-bit bitmask 表示。`PCIE_PORT_DEVICE_MAXSERVICES` 定义了最大服务数。

---

## 3. 核心数据结构

### 3.1 `struct pcie_device` — 服务设备 (`portdrv.h:59-66`)

```c
// drivers/pci/pcie/portdrv.h:59-66
struct pcie_device {
    int         irq;        /* Service IRQ/MSI/MSI-X Vector */
    struct pci_dev *port;   /* Root/Upstream/Downstream Port */
    u32         service;    /* Port service this device represents */
    void        *priv_data; /* Service Private Data */
    struct device   device; /* Generic Device Interface */
};
#define to_pcie_device(d) container_of(d, struct pcie_device, device)

// drivers/pci/pcie/portdrv.h:68-76
static inline void set_service_data(struct pcie_device *dev, void *data)
{
    dev->priv_data = data;
}

static inline void *get_service_data(struct pcie_device *dev)
{
    return dev->priv_data;
}
```

每个服务实例都是嵌入 `struct device` 的设备模型对象：
- `irq`：此服务使用的 IRQ/MSI-X 向量
- `port`：指向所属的 PCIe Port（Root/Upstream/Downstream）
- `service`：服务类型 bitmask（`PCIE_PORT_SERVICE_AER` / `_HP` / `_PME` / `_DPC` / `_BWCTRL`）
- `priv_data`：服务私有数据（AER services 用它指向 `aer_rpc`，hotplug 指向 `controller`）

`set_service_data` / `get_service_data` 是简单的 accessor，避免直接访问 `priv_data`。

### 3.2 `struct pcie_port_service_driver` — 服务驱动 (`portdrv.h:78-97`)

```c
// drivers/pci/pcie/portdrv.h:78-97
struct pcie_port_service_driver {
    const char *name;
    int (*probe)(struct pcie_device *dev);
    void (*remove)(struct pcie_device *dev);
    int (*suspend)(struct pcie_device *dev);
    int (*resume_noirq)(struct pcie_device *dev);
    int (*resume)(struct pcie_device *dev);
    int (*runtime_suspend)(struct pcie_device *dev);
    int (*runtime_resume)(struct pcie_device *dev);

    int (*slot_reset)(struct pcie_device *dev);

    int port_type;  /* Type of the port this driver can handle */
    u32 service;    /* Port service this device represents */

    struct device_driver driver;
};
#define to_service_driver(d) \
    container_of(d, struct pcie_port_service_driver, driver)
```

类比 `struct pci_driver`，但接口更精简：
- `port_type`：限制服务适用的 Port 类型（Root/Upstream/Downstream/ANY）
- `service`：注册时声明的服务类型，用于匹配 (match)
- `slot_reset`：AER/DPC 错误恢复时调用（第五篇的 slot_reset 阶段）
- 标准的 PM callbacks：suspend/resume/runtime_suspend/runtime_resume

与 `struct pci_driver` 的关键区别：`pcie_port_service_driver` 的 `probe` 接收的是 `struct pcie_device *` 而不是 `struct pci_dev *`。

---

## 4. portdrv 注册为 PCI 驱动 (`portdrv.c`)

portdrv 本身以一个普通的 `struct pci_driver` 注册在 PCI 总线上，只匹配 PCIe Port 类型的设备：

```c
// drivers/pci/pcie/portdrv.c (实际注册在文件后半部分)
// portdrv 通过 pci_register_driver() 注册，匹配 class 为 PCI_CLASS_BRIDGE_PCI
// 且 PCIe type 为 Root/Upstream/Downstream Port 的设备
```

portdrv 的 probe 流程 (`pcie_port_device_register`, `portdrv.c:331-380`)：

```c
// drivers/pci/pcie/portdrv.c:331-380
static int pcie_port_device_register(struct pci_dev *dev)
{
    int status, capabilities, i, nr_service;
    int irqs[PCIE_PORT_DEVICE_MAXSERVICES];

    status = pci_enable_device(dev);
    if (status)
        return status;

    /* 读取 port 支持哪些服务 */
    capabilities = get_port_device_capability(dev);
    if (!capabilities)
        return 0;

    pci_set_master(dev);

    /* 为各服务分配 IRQ 向量 */
    status = pcie_init_service_irqs(dev, irqs, capabilities);
    if (status) {
        capabilities &= PCIE_PORT_SERVICE_HP;
        if (!capabilities)
            goto error_disable;
    }

    /* 为每个服务创建 pcie_device */
    status = -ENODEV;
    nr_service = 0;
    for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++) {
        int service = 1 << i;
        if (!(capabilities & service))
            continue;
        if (!pcie_device_init(dev, service, irqs[i]))
            nr_service++;
    }
    if (!nr_service)
        goto error_cleanup_irqs;

    return 0;

error_cleanup_irqs:
    pci_free_irq_vectors(dev);
error_disable:
    pci_disable_device(dev);
    return status;
}
```

probe 的三个阶段：
1. **发现能力** — `get_port_device_capability` 读取配置空间，确定 port 支持哪些服务
2. **分配中断** — `pcie_init_service_irqs` 分配 MSI-X/MSI 向量
3. **创建设备** — `pcie_device_init` 为每个启用的服务创建 `struct pcie_device`

### 4.1 get_port_device_capability — 能力发现 (`portdrv.c:218-282`)

```c
// drivers/pci/pcie/portdrv.c:218-282
static int get_port_device_capability(struct pci_dev *dev)
{
    struct pci_host_bridge *host = pci_find_host_bridge(dev->bus);
    int services = 0;

    /* Hotplug */
    if (dev->is_pciehp &&
        (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT ||
         pci_pcie_type(dev) == PCI_EXP_TYPE_DOWNSTREAM) &&
        (pcie_ports_native || host->native_pcie_hotplug)) {
        services |= PCIE_PORT_SERVICE_HP;

        if (!IS_ENABLED(CONFIG_HOTPLUG_PCI_PCIE))
            pcie_capability_clear_word(dev, PCI_EXP_SLTCTL,
                PCI_EXP_SLTCTL_CCIE | PCI_EXP_SLTCTL_HPIE);
    }

#ifdef CONFIG_PCIEAER
    /* AER */
    if ((pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT ||
         pci_pcie_type(dev) == PCI_EXP_TYPE_RC_EC) &&
        dev->aer_cap && pci_aer_available() &&
        (pcie_ports_native || host->native_aer))
        services |= PCIE_PORT_SERVICE_AER;
#endif

    /* PME */
    if ((pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT ||
         pci_pcie_type(dev) == PCI_EXP_TYPE_RC_EC) &&
        (pcie_ports_native || host->native_pme)) {
        services |= PCIE_PORT_SERVICE_PME;
        pcie_pme_interrupt_enable(dev, false);
    }

    /* DPC */
    if (pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DPC) &&
        pci_aer_available() &&
        (pcie_ports_dpc_native || (services & PCIE_PORT_SERVICE_AER)))
        services |= PCIE_PORT_SERVICE_DPC;

    /* BWCTRL */
    if (pci_pcie_type(dev) == PCI_EXP_TYPE_DOWNSTREAM ||
        pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT) {
        u32 linkcap;
        pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &linkcap);
        if (linkcap & PCI_EXP_LNKCAP_LBNC &&
            hweight8(dev->supported_speeds) > 1)
            services |= PCIE_PORT_SERVICE_BWCTRL;
    }

    return services;
}
```

各服务的启用条件概要：

| 服务 | Port 类型要求 | 额外条件 |
|------|--------------|---------|
| **HP** | Root/Downstream | `is_pciehp` + native hotplug 控制权 |
| **AER** | Root/RCEC | `aer_cap` 存在 + `pci_aer_available()` |
| **PME** | Root/RCEC | native PME 控制权 |
| **DPC** | Any (有 DPC cap) | `pci_aer_available()` + native 或 AER 已启 |
| **BWCTRL** | Root/Downstream | `supported_speeds > 1` |

### 4.2 pcie_init_service_irqs — IRQ 分配 (`portdrv.c:177-206`)

```c
// drivers/pci/pcie/portdrv.c:177-206
static int pcie_init_service_irqs(struct pci_dev *dev, int *irqs, int mask)
{
    int ret, i;

    for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++)
        irqs[i] = -1;

    /* PME 不能用 MSI → 直接回退到 INTx */
    if ((mask & PCIE_PORT_SERVICE_PME) && pcie_pme_no_msi())
        goto intx_irq;

    if (pcie_port_enable_irq_vec(dev, irqs, mask) == 0)
        return 0;

intx_irq:
    ret = pci_alloc_irq_vectors(dev, 1, 1, PCI_IRQ_INTX);
    if (ret < 0)
        return -ENODEV;

    for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++)
        irqs[i] = pci_irq_vector(dev, 0);

    return 0;
}
```

优先尝试 MSI-X/MSI，失败则回退到 INTx。INTx 模式下所有服务共享同一个 IRQ 向量。

### 4.3 pcie_port_enable_irq_vec — MSI 向量分配 (`portdrv.c:113-167`)

```c
// drivers/pci/pcie/portdrv.c:113-167
static int pcie_port_enable_irq_vec(struct pci_dev *dev, int *irqs, int mask)
{
    int nr_entries, nvec, pcie_irq;
    u32 pme = 0, aer = 0, dpc = 0;

    /* 分配最大可能的 MSI-MSIX 向量数 (最多 32) */
    nr_entries = pci_alloc_irq_vectors(dev, 1, PCIE_PORT_MAX_MSI_ENTRIES,
            PCI_IRQ_MSIX | PCI_IRQ_MSI);
    if (nr_entries < 0)
        return nr_entries;

    /* 读取各服务的 Interrupt Message Number */
    nvec = pcie_message_numbers(dev, mask, &pme, &aer, &dpc);
    if (nvec > nr_entries) {
        pci_free_irq_vectors(dev);
        return -EIO;
    }

    /* 如有过多向量，缩减到实际需要数 */
    if (nvec != nr_entries) {
        pci_free_irq_vectors(dev);
        nr_entries = pci_alloc_irq_vectors(dev, nvec, nvec,
                PCI_IRQ_MSIX | PCI_IRQ_MSI);
        if (nr_entries < 0)
            return nr_entries;
    }

    /* 分配向量：PME/HP/BWCTRL 共享第一个向量 */
    if (mask & (PCIE_PORT_SERVICE_PME | PCIE_PORT_SERVICE_HP |
            PCIE_PORT_SERVICE_BWCTRL)) {
        pcie_irq = pci_irq_vector(dev, pme);
        irqs[PCIE_PORT_SERVICE_PME_SHIFT]    = pcie_irq;
        irqs[PCIE_PORT_SERVICE_HP_SHIFT]     = pcie_irq;
        irqs[PCIE_PORT_SERVICE_BWCTRL_SHIFT] = pcie_irq;
    }

    if (mask & PCIE_PORT_SERVICE_AER)
        irqs[PCIE_PORT_SERVICE_AER_SHIFT] = pci_irq_vector(dev, aer);

    if (mask & PCIE_PORT_SERVICE_DPC)
        irqs[PCIE_PORT_SERVICE_DPC_SHIFT] = pci_irq_vector(dev, dpc);

    return 0;
}
```

**关键设计**：PME、Hotplug、BWCTRL 共享同一个 MSI-X 向量 (Interrupt Message Number 0)；AER 和 DPC 各自有独立的向量。这样设计是因为：
- PME/HP/BWCTRL 在 PCIe Capability 中共享相同的 Interrupt Message Number
- AER 和 DPC 各自有独立的 Interrupt Message Number（分别在 AER Capability 和 DPC Capability 中定义）

### 4.4 pcie_message_numbers — 读取 Interrupt Message Number (`portdrv.c:57-101`)

```c
// drivers/pci/pcie/portdrv.c:57-101
static int pcie_message_numbers(struct pci_dev *dev, int mask,
                u32 *pme, u32 *aer, u32 *dpc)
{
    u32 nvec = 0, pos;
    u16 reg16;

    /* PME/HP/BWCTRL: 从 PCIe Capability 读取 */
    if (mask & (PCIE_PORT_SERVICE_PME | PCIE_PORT_SERVICE_HP |
            PCIE_PORT_SERVICE_BWCTRL)) {
        pcie_capability_read_word(dev, PCI_EXP_FLAGS, &reg16);
        *pme = FIELD_GET(PCI_EXP_FLAGS_IRQ, reg16);
        nvec = *pme + 1;
    }

#ifdef CONFIG_PCIEAER
    /* AER: 从 AER Capability (PCI_ERR_ROOT_STATUS) 读取 */
    if (mask & PCIE_PORT_SERVICE_AER) {
        u32 reg32;
        pos = dev->aer_cap;
        if (pos) {
            pci_read_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, &reg32);
            *aer = FIELD_GET(PCI_ERR_ROOT_AER_IRQ, reg32);
            nvec = max(nvec, *aer + 1);
        }
    }
#endif

    /* DPC: 从 DPC Capability (PCI_EXP_DPC_CAP) 读取 */
    if (mask & PCIE_PORT_SERVICE_DPC) {
        pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DPC);
        if (pos) {
            pci_read_config_word(dev, pos + PCI_EXP_DPC_CAP, &reg16);
            *dpc = FIELD_GET(PCI_EXP_DPC_IRQ, reg16);
            nvec = max(nvec, *dpc + 1);
        }
    }

    return nvec;
}
```

每个 Capability 结构中都有一个 Interrupt Message Number 字段，告诉软件该服务期望使用 MSI/MSI-X 表中的哪个向量。portdrv 读取这些值，计算所需的总向量数 (`nvec`)，优先尝试 MSI-X（最多 32 个向量）。

### 4.5 pcie_device_init — 创建服务设备 (`portdrv.c:290-322`)

```c
// drivers/pci/pcie/portdrv.c:290-322
static int pcie_device_init(struct pci_dev *pdev, int service, int irq)
{
    int retval;
    struct pcie_device *pcie;
    struct device *device;

    pcie = kzalloc_obj(*pcie);
    if (!pcie)
        return -ENOMEM;
    pcie->port = pdev;
    pcie->irq = irq;
    pcie->service = service;

    /* Initialize generic device interface */
    device = &pcie->device;
    device->bus = &pcie_port_bus_type;
    device->release = release_pcie_device;
    dev_set_name(device, "%s:pcie%03x",
             pci_name(pdev),
             get_descriptor_id(pci_pcie_type(pdev), service));
    device->parent = &pdev->dev;
    device_enable_async_suspend(device);

    retval = device_register(device);
    if (retval) {
        put_device(device);
        return retval;
    }

    pm_runtime_no_callbacks(device);

    return 0;
}
```

每个服务设备被创建为一个 `struct device`，挂在 `pcie_port_bus_type` 虚拟总线下。设备名格式为：`<BDF>:pcie<descriptor_id>`。

`device_register` 会触发 driver core 的 match/probe 流程，寻找已注册的服务驱动。

---

## 5. 服务驱动注册与匹配

### 5.1 pcie_port_bus_type (`portdrv.c:575-580`)

```c
// drivers/pci/pcie/portdrv.c:575-580
const struct bus_type pcie_port_bus_type = {
    .name   = "pci_express",
    .match  = pcie_port_bus_match,
    .probe  = pcie_port_bus_probe,
    .remove = pcie_port_bus_remove,
};
```

虚拟总线 `pci_express` 是 portdrv 框架的核心。所有服务设备都注册在这条总线上，所有服务驱动也注册在这条总线上。match/probe/remove 均由 portdrv 实现。

### 5.2 pcie_port_bus_match (`portdrv.c:511-524`)

```c
// drivers/pci/pcie/portdrv.c:511-524
static int pcie_port_bus_match(struct device *dev,
                   const struct device_driver *drv)
{
    struct pcie_device *pciedev = to_pcie_device(dev);
    const struct pcie_port_service_driver *driver = to_service_driver(drv);

    if (driver->service != pciedev->service)
        return 0;

    if (driver->port_type != PCIE_ANY_PORT &&
        driver->port_type != pci_pcie_type(pciedev->port))
        return 0;

    return 1;
}
```

匹配规则：
1. **service 必须一致**：AER 驱动只匹配 `PCIE_PORT_SERVICE_AER` 设备
2. **port_type 兼容**：大多数驱动使用 `PCIE_ANY_PORT`，不限制端口类型

### 5.3 pcie_port_bus_probe (`portdrv.c:534-551`)

```c
// drivers/pci/pcie/portdrv.c:534-551
static int pcie_port_bus_probe(struct device *dev)
{
    struct pcie_device *pciedev;
    struct pcie_port_service_driver *driver;
    int status;

    driver = to_service_driver(dev->driver);
    if (!driver || !driver->probe)
        return -ENODEV;

    pciedev = to_pcie_device(dev);
    status = driver->probe(pciedev);
    if (status)
        return status;

    get_device(dev);
    return 0;
}
```

match 成功后，调用驱动注册的 `probe` 函数。注意 `get_device(dev)` 增加引用计数，因为 `device_register` 在成功后会放掉初始引用。

### 5.4 pcie_port_service_register (`portdrv.c:586-595`)

```c
// drivers/pci/pcie/portdrv.c:586-595
int pcie_port_service_register(struct pcie_port_service_driver *new)
{
    if (pcie_ports_disabled)
        return -ENODEV;

    new->driver.name = new->name;
    new->driver.bus = &pcie_port_bus_type;

    return driver_register(&new->driver);
}
```

服务驱动通过 `pcie_port_service_register` 注册到 `pcie_port_bus_type` 总线。如果 `pcie_ports_disabled` 为真（通过 `pcie_ports=compat` 内核参数设置），则拒绝注册。

---

## 6. 五个服务驱动的注册入口

每个服务驱动都通过 `pcie_port_service_register` 注册：

### 6.1 AER 服务 (`aer.c:1719-1740`)

```c
// drivers/pci/pcie/aer.c:1719-1740
static struct pcie_port_service_driver aerdriver = {
    .name       = "aer",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_AER,

    .probe      = aer_probe,
    .suspend    = aer_suspend,
    .resume     = aer_resume,
    .remove     = aer_remove,
};

int __init pcie_aer_init(void)
{
    if (!pci_aer_available())
        return -ENXIO;
    return pcie_port_service_register(&aerdriver);
}
```

### 6.2 Hotplug 服务 (`pciehp_core.c:353-384`)

```c
// drivers/pci/hotplug/pciehp_core.c:353-384
static struct pcie_port_service_driver hpdriver_portdrv = {
    .name       = "pciehp",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_HP,

    .probe      = pciehp_probe,
    .remove     = pciehp_remove,
    .slot_reset = pciehp_slot_reset,
    // ... PM callbacks ...
};

int __init pcie_hp_init(void)
{
    return pcie_port_service_register(&hpdriver_portdrv);
}
```

### 6.3 PME 服务 (`pme.c:456-473`)

```c
// drivers/pci/pcie/pme.c:456-473
static struct pcie_port_service_driver pcie_pme_driver = {
    .name       = "pcie_pme",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_PME,

    .probe      = pcie_pme_probe,
    .suspend    = pcie_pme_suspend,
    .resume     = pcie_pme_resume,
    .remove     = pcie_pme_remove,
};

int __init pcie_pme_init(void)
{
    return pcie_port_service_register(&pcie_pme_driver);
}
```

### 6.4 DPC 服务 (`dpc.c:523-536`)

```c
// drivers/pci/pcie/dpc.c:523-536
static struct pcie_port_service_driver dpcdriver = {
    .name       = "dpc",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_DPC,
    .probe      = dpc_probe,
    .suspend    = dpc_suspend,
    .resume     = dpc_resume,
    .remove     = dpc_remove,
};

int __init pcie_dpc_init(void)
{
    return pcie_port_service_register(&dpcdriver);
}
```

### 6.5 BWCTRL 服务 (`bwctrl.c:319`)

```c
// drivers/pci/pcie/bwctrl.c:319
static struct pcie_port_service_driver pcie_bwctrl_driver = {
    .name       = "pcie_bwctrl",
    .port_type  = ...,
    .service    = PCIE_PORT_SERVICE_BWCTRL,
    .probe      = pcie_bwctrl_probe,
    // ...
};
```

---

## 7. portdrv 的 PM 分发

portdrv 在系统 suspend/resume 时遍历所有已注册的服务设备，依次调用各服务驱动的 PM 回调：

```c
// drivers/pci/pcie/portdrv.c:385-398
typedef int (*pcie_callback_t)(struct pcie_device *);

static int pcie_port_device_iter(struct device *dev, void *data)
{
    struct pcie_port_service_driver *service_driver;
    size_t offset = *(size_t *)data;
    pcie_callback_t cb;

    if ((dev->bus == &pcie_port_bus_type) && dev->driver) {
        service_driver = to_service_driver(dev->driver);
        cb = *(pcie_callback_t *)((void *)service_driver + offset);
        if (cb)
            return cb(to_pcie_device(dev));
    }
    return 0;
}

static int pcie_port_device_suspend(struct device *dev)
{
    size_t off = offsetof(struct pcie_port_service_driver, suspend);
    return device_for_each_child(dev, &off, pcie_port_device_iter);
}
```

这里用了一个巧妙的技巧：通过 `offsetof` 计算回调指针在 `struct pcie_port_service_driver` 中的偏移量，然后通过 `device_for_each_child` 遍历所有子设备，对每个子设备取其对应的回调并调用。这样 `suspend`、`resume`、`resume_noirq`、`runtime_suspend`、`runtime_resume` 都复用同一套迭代逻辑。

---

## 8. 架构全景图

```
┌─────────────────────────────────────────────────────────────────┐
│                    PCIe Root Port / Downstream Port               │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  PCI Bus Driver (portdrv.c)                                │   │
│  │                                                             │   │
│  │  pcie_port_device_register() ───────┐                      │   │
│  │    ├── get_port_device_capability() │  (detect)            │   │
│  │    ├── pcie_init_service_irqs()    │  (MSI-X allocation)  │   │
│  │    └── pcie_device_init() × N      │  (create devices)    │   │
│  │                                     │                      │   │
│  └─────────────────────────────────────┼──────────────────────┘   │
│                                        │                          │
│         pci_express bus (pcie_port_bus_type)                       │
│                                        │                          │
│     ┌──────────┬──────────┬───────────┼────────┬──────────┐     │
│     │          │          │           │        │          │     │
│     ▼          ▼          ▼           ▼        ▼          ▼     │
│  ┌───────┐ ┌───────┐ ┌──────┐  ┌────────┐ ┌────────┐ ┌───────┐│
│  │pciehp │ │  aer  │ │ pme  │  │  dpc   │ │bwctrl  │ │ ...   ││
│  │ (HP)  │ │ (AER) │ │(PME) │  │ (DPC)  │ │(BWCTRL)│ │       ││
│  ├───────┤ ├───────┤ ├──────┤  ├────────┤ ├────────┤ ├───────┤│
│  │irq=X  │ │irq=Y  │ │irq=X │  │irq=Z   │ │irq=X   │ │       ││
│  │probe  │ │probe  │ │probe │  │probe   │ │probe   │ │       ││
│  │remove │ │remove │ │remove│  │remove  │ │remove  │ │       ││
│  │suspend│ │suspend│ │suspend│ │suspend │ │suspend │ │       ││
│  │resume │ │resume │ │resume│  │resume  │ │resume  │ │       ││
│  └───────┘ └───────┘ └──────┘  └────────┘ └────────┘ └───────┘│
│                                                                  │
│  MSI-X 向量分配:                                                  │
│    Vector 0: PME + Hotplug + BWCTRL (共享, INT#A)                │
│    Vector 1: AER (独立, INT#B)                                    │
│    Vector 2: DPC (独立, INT#C)                                    │
└─────────────────────────────────────────────────────────────────┘
```

INTx 回退模式下，所有服务共享同一根中断线。但即使在共享中断线上，各服务的 probe/remove 仍然是独立的——一个服务的 bug 不会让另一个服务无法加载（虽然中断风暴可能互相影响）。

---

## 9. 设计收益

1. **故障隔离**：hoptplug 故障（如轮询线程死锁）不影响 AER 错误报告继续工作；AER 错误恢复过程中的重置失败不影响 PME 唤醒能力
2. **独立加载**：通过 Kconfig 可以选择性编译各服务（`CONFIG_HOTPLUG_PCI_PCIE`、`CONFIG_PCIEAER`、`CONFIG_PCIE_PME`、`CONFIG_PCIE_DPC`、`CONFIG_PCIE_BWCTRL`）
3. **代码解耦**：每个服务驱动只需实现 `struct pcie_port_service_driver` 接口，不必关心其他服务的存在
4. **PM 自动化**：portdrv 统一管理所有服务的 suspend/resume 顺序，各驱动无需关心调用时机
5. **中断向量优化**：MSI-X 模式下 AER 和 DPC 有独立向量，降低中断延迟，避免 PME 处理阻塞错误恢复

---

## 系列结语

本系列从 PCIe 枚举开始，逐步深入到地址空间管理、设备驱动绑定、MSI-X 中断分配、AER 错误报告与恢复链、热插拔与 ASPM 电源管理，最后到 Port 服务模型的架构设计。

回顾整套流程：

```
GPU 物理插入
   │
   ▼
PCI Bus 扫描 (枚举) ─── 发现设备, 读取 Vendor/Device ID
   │
   ▼
BAR 分配 ─── 为 VRAM 窗口分配 MMIO 地址空间
   │
   ▼
驱动匹配与 Probe ─── drm/nouveau/amdgpu/i915 驱动绑定
   │
   ▼
MSI-X 向量分配 ─── 独立中断通道，降低延迟
   │
   ▼
AER 能力初始化 ─── 错误计数器 + 报告使能
   │
   ▼
(运行时) ─── 一旦发生 PCIe Error:
   │          aer_irq → aer_isr → pcie_do_recovery
   │          → error_detected → reset → slot_reset → resume
   │          驱动 err_handler 参与恢复全过程
   │
   ▼
(热插拔) ─── 用户拔出 GPU:
              pciehp_isr → pciehp_ist → 驱动 remove → 断电
```

所有这一切都运行在 PCIe Port 服务模型的框架之上：AER、Hotplug、PME、DPC 五个服务各自独立又协同工作，构成了一套完整的 PCIe 运行时基础设施。

理解这套基础设施，对于调试 GPU 驱动中的 PCIe 错误、热插拔问题、电源管理异常、中断分配错误等场景至关重要。
