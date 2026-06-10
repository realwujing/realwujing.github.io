# 第一篇：枚举 — 内核怎么发现 PCIe 设备

> 源码：`drivers/pci/probe.c` `drivers/pci/ecam.c` | 头文件：`include/linux/pci.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. 概述

服务器加电后，固件（BIOS/UEFI）或内核需要遍历整个 PCIe 拓扑树，发现每一条总线上插着的每一个设备。这个过程叫做**枚举 (enumeration)**。对于 AI 服务器来说，这一步决定了内核是否能"看见"那张 80GB 的 GPU、200Gbps 的 RDMA 网卡，以及 NVMe 盘。

内核枚举分两个阶段：

1. **配置空间访问方法建立** — x86 Port I/O 还是 ECAM MMIO
2. **递归扫描** — 从 Root Complex 出发，沿着桥向下，发现设备、分配 bus number

---

## 2. 两种配置空间访问方法

### 2.1 传统 x86 Port I/O：0xCF8 / 0xCFC

这是最古老的 PCI 配置空间访问方式，x86 上至今仍然支持。

```
写入 0xCF8 (CONFIG_ADDRESS):      读取 0xCFC (CONFIG_DATA):

 31  24|23    16|15    11|10     8|7      2|1 0
+------+-------+--------+--------+--------+----+
| 使能 | 总线号 | 设备号  | 功能号  | 寄存器  | 00 |
+------+-------+--------+--------+--------+----+

使能位 bit[31]=1 表示这是一次有效的配置请求。
一次完整的访问 = outl(addr, 0xCF8) + inl(0xCFC) / outl(data, 0xCFC)
```

缺点很明显：设备号只有 5 位（32 个设备），功能号 3 位（8 个功能），寄存器偏移 8 位（256 字节）。PCIe 扩展配置空间（4KB）无法用这种方式访问。

### 2.2 ECAM：Enhanced Configuration Access Mechanism

PCIe 规范引入了 ECAM，将每个 bus/device/function 的 4KB 配置空间直接映射到一段连续的物理内存地址：

```
ECAM 地址 = ECAM 基址 + (bus × 1MiB) + (device × 32KiB) + (function × 4KiB) + offset

或者写成：
ECAM 地址 = ECAM 基址 + (bus << 20) + (devfn << 12) + offset
```

| 参数 | 偏移粒度 | 说明 |
|------|---------|------|
| bus | 1 MiB | 256 个 bus |
| devfn | 4 KiB | 32 devices × 8 functions = 256 个 devfn |
| offset | 1 byte | 4KB 配置空间 |

内核通过 `pci_ecam_create()` 来建立这个映射窗口，源码在 `drivers/pci/ecam.c:27-60`：

```c
// ecam.c:27-60 — 核心逻辑
struct pci_config_window *pci_ecam_create(struct device *dev,
        struct resource *cfgres, struct resource *busr,
        const struct pci_ecam_ops *ops)
{
    unsigned int bus_shift = ops->bus_shift;
    struct pci_config_window *cfg;
    unsigned int bus_range, bus_range_max, bsz;
    struct resource *conflict;
    int err;

    if (busr->start > busr->end)
        return ERR_PTR(-EINVAL);                     // 入参校验

    cfg = kzalloc_obj(*cfg);
    if (!cfg)
        return ERR_PTR(-ENOMEM);

    // ECAM 兼容平台不需要提供 ops->bus_shift
    if (!bus_shift)
        bus_shift = PCIE_ECAM_BUS_SHIFT;              // 默认 20 位（1 MiB）

    cfg->parent = dev;
    cfg->ops = ops;
    cfg->busr.start = busr->start;
    cfg->busr.end = busr->end;
    cfg->busr.flags = IORESOURCE_BUS;
    cfg->bus_shift = bus_shift;

    // 检查 ECAM 内存区域是否能容纳请求的 bus 范围
    bus_range = resource_size(&cfg->busr);
    bus_range_max = resource_size(cfgres) >> bus_shift;
    if (bus_range > bus_range_max) {
        bus_range = bus_range_max;
        resource_set_size(&cfg->busr, bus_range);
        dev_warn(dev, "ECAM area %pR can only accommodate %pR "
                      "(reduced from %pR desired)\n",
                 cfgres, &cfg->busr, busr);
    }

    cfg->res.start = cfgres->start;
    cfg->res.end = cfgres->end;
    cfg->res.flags = IORESOURCE_MEM | IORESOURCE_BUSY;
    cfg->res.name = "PCI ECAM";

    // 在 iomem_resource 树中注册 ECAM 区域，避免冲突
    conflict = request_resource_conflict(&iomem_resource, &cfg->res);
    if (conflict) {
        err = -EBUSY;
        dev_err(dev, "can't claim ECAM area %pR: "
                "address conflict with %s %pR\n",
                &cfg->res, conflict->name, conflict);
        goto err_exit;
    }
    // ...
}
```

关键点：
- `bus_shift` 决定了每个 bus 占用的地址空间大小，标准 ECAM 是 20 位 = 1 MiB
- `request_resource_conflict(&iomem_resource, ...)` 在全局 IO 内存资源树中锁定这段地址，防止其他驱动冲突
- 如果请求的 bus 范围超出 ECAM 能容纳的范围，会静默裁剪并发出警告

---

## 3. 配置空间访问抽象：struct pci_ops

`include/linux/pci.h:872-878`：

```c
struct pci_ops {
    int (*add_bus)(struct pci_bus *bus);
    void (*remove_bus)(struct pci_bus *bus);
    void __iomem *(*map_bus)(struct pci_bus *bus, unsigned int devfn, int where);
    int (*read)(struct pci_bus *bus, unsigned int devfn,
                int where, int size, u32 *val);
    int (*write)(struct pci_bus *bus, unsigned int devfn,
                 int where, int size, u32 val);
};
```

`map_bus` 返回 ECAM 基址 + 偏移后的虚拟地址，`read`/`write` 直接做 MMIO 读写。上层代码完全不需要关心底层是 Port I/O 还是 ECAM。

---

## 4. 扫描流程全景 ASCII

```
                    HOST BRIDGE (Root Complex)
                           |
                  pci_scan_child_bus()  ←─ 从 bus 0 开始
                           |
                 pci_scan_child_bus_extend(available_buses=0)
                           |
             ┌─────────────┼─────────────┐
             │  Step 1: 扫描 32 个 devfn │
             │  pci_scan_slot(bus, devfn)│
             └─────────────┼─────────────┘
                           |
                    对每个 devfn:
                  pci_scan_device(bus, devfn)         ← probe.c:2602
                           |
              ┌── pci_bus_read_dev_vendor_id() ── 读 vendor=0xFFFF? → 空槽
              │
              ├── pci_alloc_dev(bus)               ← 分配 struct pci_dev
              │
              ├── pci_setup_device(dev)             ← probe.c:2021
              │       ├── 读 hdr_type               ← Type 0 (EP) / Type 1 (Bridge)
              │       ├── 读 class code/revision
              │       ├── 设置 dma_mask = 0xFFFFFFFF
              │       ├── 设置 msi_addr_mask = 64-bit
              │       └── dev_set_name("0000:02:00.0")
              │
              ├── pci_init_capabilities(dev)         ← probe.c:2655
              │       ├── pci_ea_init()              Enhanced Allocation
              │       ├── pci_msi_init() / pci_msix_init()
              │       ├── pci_pm_init()              Power Management
              │       ├── pci_vpd_init()             Vital Product Data
              │       ├── pci_iov_init()             SR-IOV
              │       ├── pci_ats_init()             ATS
              │       ├── pci_pri_init()             PRI
              │       ├── pci_pasid_init()           PASID
              │       ├── pci_aer_init()             AER ✦
              │       ├── pci_dpc_init()             DPC
              │       ├── pci_ptm_init()             PTM
              │       ├── pci_rebar_init()           Resizable BAR
              │       ├── pci_doe_init()             DOE
              │       └── pci_ide_init()             IDE
              │
              └── 加入 bus->devices 链表
                           |
             ┌─────────────┼─────────────┐
             │ Step 2: pcibios_fixup_bus │
             └─────────────┼─────────────┘
                           |
             ┌─────────────┼─────────────┐
             │ Step 3: 遍历桥并递归扫描   │
             │          (两遍)            │
             └─────────────┼─────────────┘
                           |
              第一遍：已配置的桥 (reassign=0)
              ┌──────────────────────────┐
              │ pci_scan_bridge_extend() │ → 递归 pci_scan_child_bus_extend()
              └──────────────────────────┘
                           |
              第二遍：需要重新分配 bus number 的桥
              ┌──────────────────────────┐
              │ 热插拔桥分配 extra buses │ → available_buses / hotplug_bridges
              │ 单桥获得全部 available   │
              └──────────────────────────┘
                           |
                   返回 max bus number
```

---

## 5. pci_scan_child_bus_extend 逐行解析

`drivers/pci/probe.c:3087-3214`：

```c
// probe.c:3087-3089
static unsigned int pci_scan_child_bus_extend(struct pci_bus *bus,
                          unsigned int available_buses)
{
    unsigned int used_buses, normal_bridges = 0, hotplug_bridges = 0;
    unsigned int start = bus->busn_res.start;
    unsigned int devnr, cmax, max = start;
    struct pci_dev *dev;

    dev_dbg(&bus->dev, "scanning bus\n");
```

**Line 3095**: 打印 "scanning bus" — 每个 bus 被扫描时打印一条 debug 信息，打开 `pci=earlydump` 可以看到完整扫描过程。

### 5.1 Step 1：逐个 slot 扫描 (L3098-3099)

```c
    // probe.c:3098-3099
    /* Go find them, Rover! */         // ← 经典注释
    for (devnr = 0; devnr < PCI_MAX_NR_DEVS; devnr++)
        pci_scan_slot(bus, PCI_DEVFN(devnr, 0));
```

`PCI_MAX_NR_DEVS` 通常是 32。对每个 slot（device number 0-31），先读 function 0 的 vendor ID。如果是 0xFFFF，说明这个 slot 是空的；如果不是 0xFFFF，再检查 header type 的 bit[7]（multifunction 位），如果是 multi-function 设备，继续扫描 function 1-7。

`pci_scan_slot` 内部调用 `pci_scan_device`，我们稍后详细展开。

### 5.2 Step 2：SR-IOV bus 预留 (L3101-3103)

```c
    // probe.c:3101-3103
    /* Reserve buses for SR-IOV capability */
    used_buses = pci_iov_bus_range(bus);
    max += used_buses;
```

如果该 bus 上有支持 SR-IOV 的设备，VF 需要额外的 bus number。内核提前预留。

### 5.3 Step 3：arch fixup (L3109-3113)

```c
    // probe.c:3109-3113
    if (!bus->is_added) {
        dev_dbg(&bus->dev, "fixups for bus\n");
        pcibios_fixup_bus(bus);
        bus->is_added = 1;
    }
```

平台相关（pcibios）fixup，只执行一次。例如 x86 上做一些 bus number 调整。

### 5.4 Step 4：统计 hotplug 与 normal 桥 (L3120-3125)

```c
    // probe.c:3120-3125
    for_each_pci_bridge(dev, bus) {
        if (dev->is_hotplug_bridge)
            hotplug_bridges++;
        else
            normal_bridges++;
    }
```

遍历当前 bus 上的所有 PCI bridge，统计哪些支持热插拔。这个数据在后续分配 bus number 时至关重要——热插拔桥需要额外的 bus 空间，因为将来用户可能插入带有下游总线的设备。

### 5.5 Step 5：第一遍 — 已配置的桥 (L3132-3143)

```c
    // probe.c:3132-3143
    for_each_pci_bridge(dev, bus) {
        cmax = max;
        max = pci_scan_bridge_extend(bus, dev, max, 0, 0);
        //              bus, bridge_dev, current_max, extra_buses, reassign

        /*
         * Reserve one bus for each bridge now to avoid extending
         * hotplug bridges too much during the second scan below.
         */
        used_buses++;
        if (max - cmax > 1)
            used_buses += max - cmax - 1;
    }
```

`reassign=0` 表示"先不动已有配置的桥"。内核检查桥的 primary/secondary/subordinate bus number 寄存器，如果配置合理就直接递归扫描下游。`used_buses` 累计已消耗的 bus number。

### 5.6 Step 6：第二遍 — 需要 reconfig 的桥 (L3145-3171)

```c
    // probe.c:3145-3171
    /* Scan bridges that need to be reconfigured */
    for_each_pci_bridge(dev, bus) {
        unsigned int buses = 0;

        if (!hotplug_bridges && normal_bridges == 1) {
            /*
             * There is only one bridge on the bus (upstream
             * port) so it gets all available buses which it
             * can then distribute to the possible hotplug
             * bridges below.
             */
            buses = available_buses;
        } else if (dev->is_hotplug_bridge) {
            /*
             * Distribute the extra buses between hotplug
             * bridges if any.
             */
            buses = available_buses / hotplug_bridges;
            buses = min(buses, available_buses - used_buses + 1);
        }

        cmax = max;
        max = pci_scan_bridge_extend(bus, dev, cmax, buses, 1);
        /* One bus is already accounted so don't add it again */
        if (max - cmax > 1)
            used_buses += max - cmax - 1;
    }
```

关键逻辑：
- **单桥（上游端口）**：获得所有 `available_buses`，然后自己再分给下游的热插拔桥
- **多桥 + 热插拔**：将额外 bus 均分给热插拔桥：`available_buses / hotplug_bridges`
- 用 `min()` 限制不超过剩余可用量

`reassign=1` 参数告诉 `pci_scan_bridge_extend` 可以修改桥的 bus number 寄存器。

### 5.7 Step 7：热插拔桥最低保证 (L3178-3189)

```c
    // probe.c:3178-3189
    if (bus->self && bus->self->is_hotplug_bridge) {
        used_buses = max(available_buses, pci_hotplug_bus_size - 1);
        if (max - start < used_buses) {
            max = start + used_buses;

            /* Do not allocate more buses than we have room left */
            if (max > bus->busn_res.end)
                max = bus->busn_res.end;

            dev_dbg(&bus->dev, "%pR extended by %#02x\n",
                &bus->busn_res, max - start);
        }
    }
```

即使没有足够的 `available_buses`，热插拔桥也要保证至少 `pci_hotplug_bus_size` 个子总线（通常为 8）。但不能超出 `busn_res.end` 的上界，否则会和兄弟总线冲突。

### 5.8 包装函数 (L3210-3214)

```c
// probe.c:3210-3214
unsigned int pci_scan_child_bus(struct pci_bus *bus)
{
    return pci_scan_child_bus_extend(bus, 0);
}
EXPORT_SYMBOL_GPL(pci_scan_child_bus);
```

外部调用者用 `pci_scan_child_bus()`，它只是把 `available_buses` 设为 0，表示"没有额外 bus 需要分配"。

---

## 6. pci_scan_device — 发现单个设备

`drivers/pci/probe.c:2602-2625`：

```c
// probe.c:2602-2625
static struct pci_dev *pci_scan_device(struct pci_bus *bus, int devfn)
{
    struct pci_dev *dev;
    u32 l;

    // 1. 读 Vendor ID + Device ID，超时 60s
    if (!pci_bus_read_dev_vendor_id(bus, devfn, &l, 60*1000))
        return NULL;                    // 0xFFFF → 空槽，退出

    // 2. 分配 struct pci_dev
    dev = pci_alloc_dev(bus);
    if (!dev)
        return NULL;

    dev->devfn = devfn;
    dev->vendor = l & 0xffff;          // 低 16 位 = vendor ID
    dev->device = (l >> 16) & 0xffff;  // 高 16 位 = device ID

    // 3. 进一步初始化设备
    if (pci_setup_device(dev)) {
        pci_bus_put(dev->bus);
        kfree(dev);
        return NULL;                    // 初始化失败 → 释放并返回 NULL
    }

    return dev;
}
```

`pci_bus_read_dev_vendor_id()` 的超时参数是 60000 ms（60 秒）。这看起来很夸张，但历史上某些设备在复位后需要很长时间才能响应配置读取。CRS（Configuration Request Retry Status）机制允许设备返回 0x0001 表示"还没准备好"，PCIe 规范规定 Root Complex 应该透明重试。

---

## 7. pci_setup_device — 设备结构体初始化

`drivers/pci/probe.c:2021-2067`：

```c
// probe.c:2021-2067
int pci_setup_device(struct pci_dev *dev)
{
    u32 class;
    u16 cmd;
    u8 hdr_type;
    int err, pos = 0;
    struct pci_bus_region region;
    struct resource *res;

    hdr_type = pci_hdr_type(dev);                        // 读 header type 寄存器

    dev->sysdata = dev->bus->sysdata;                    // 平台私有数据
    dev->dev.parent = dev->bus->bridge;
    dev->dev.bus = &pci_bus_type;                        // 挂在 pci_bus_type 总线上
    dev->hdr_type = FIELD_GET(PCI_HEADER_TYPE_MASK, hdr_type);
    dev->multifunction = FIELD_GET(PCI_HEADER_TYPE_MFD, hdr_type);
    dev->error_state = pci_channel_io_normal;
    set_pcie_port_type(dev);                             // Root Port / SW USP / SW DSP / EP

    err = pci_set_of_node(dev);                          // Device Tree 关联
    if (err)
        return err;
    pci_set_acpi_fwnode(dev);                            // ACPI 固件节点关联

    pci_dev_assign_slot(dev);                            // 分配物理 slot

    /*
     * Assume 32-bit PCI; let 64-bit PCI cards (which are far rarer)
     * set this higher, assuming the system even supports it.
     */
    dev->dma_mask = 0xffffffff;                          // 默认 32-bit DMA

    /*
     * Assume 64-bit addresses for MSI initially. Will be changed to 32-bit
     * if MSI (rather than MSI-X) capability does not have
     * PCI_MSI_FLAGS_64BIT. Can also be overridden by driver.
     */
    dev->msi_addr_mask = DMA_BIT_MASK(64);               // 默认 64-bit MSI 地址

    dev_set_name(&dev->dev, "%04x:%02x:%02x.%d",
                 pci_domain_nr(dev->bus),
                 dev->bus->number, PCI_SLOT(dev->devfn),
                 PCI_FUNC(dev->devfn));                  // 名字："0000:03:00.0"

    class = pci_class(dev);                              // 读 class code

    dev->revision = class & 0xff;                        // 低 8 位 = revision ID
    dev->class = class >> 8;                             // 高 24 位 = class code
    // ...
```

注意 `dev_set_name` 生成的设备名形如 `0000:03:00.0`：
- `0000` = PCI domain (segment)
- `03` = bus number
- `00` = device number (slot)
- `0` = function number

这个名称会出现在 `/sys/bus/pci/devices/` 下，也是 `lspci` 显示的 BDF 格式。

---

## 8. pci_init_capabilities — 24 种 Capability 逐一初始化

`drivers/pci/probe.c:2655-2685`：

```c
// probe.c:2655-2685
static void pci_init_capabilities(struct pci_dev *dev)
{
    pci_ea_init(dev);        /* Enhanced Allocation */
    pci_msi_init(dev);       /* Disable MSI */
    pci_msix_init(dev);      /* Disable MSI-X */

    /* Buffers for saving PCIe and PCI-X capabilities */
    pci_allocate_cap_save_buffers(dev);

    pci_imm_ready_init(dev); /* Immediate Readiness */
    pci_pm_init(dev);        /* Power Management */
    pci_vpd_init(dev);       /* Vital Product Data */
    pci_configure_ari(dev);  /* Alternative Routing-ID Forwarding */
    pci_iov_init(dev);       /* Single Root I/O Virtualization */
    pci_ats_init(dev);       /* Address Translation Services */
    pci_pri_init(dev);       /* Page Request Interface */
    pci_pasid_init(dev);     /* Process Address Space ID */
    pci_acs_init(dev);       /* Access Control Services */
    pci_ptm_init(dev);       /* Precision Time Measurement */
    pci_aer_init(dev);       /* Advanced Error Reporting */
    pci_dpc_init(dev);       /* Downstream Port Containment */
    pci_rcec_init(dev);      /* Root Complex Event Collector */
    pci_doe_init(dev);       /* Data Object Exchange */
    pci_tph_init(dev);       /* TLP Processing Hints */
    pci_rebar_init(dev);     /* Resizable BAR */
    pci_dev3_init(dev);      /* Device 3 capabilities */
    pci_ide_init(dev);       /* Link Integrity and Data Encryption */

    pcie_report_downtraining(dev);
    pci_init_reset_methods(dev);
}
```

整整 24 个 capability 的初始化。每个 `_init` 函数首先调用 `pci_find_capability()` 或 `pci_find_ext_capability()` 在配置空间中查找对应的 capability ID，如果找到就解析并保存到 `struct pci_dev` 的对应字段中。

注意：
- **MSI/MSI-X 先 disable**（L2658-2659），后续 driver probe 时再按需 enable
- **ATS/PRI/PASID** 是 GPU/加速器做 Unified Memory 的基础，后续系列会专门讲
- **AER/DPC** 是错误处理的核心，第 5 篇会深度展开
- **Resizable BAR**（rebar_init）允许驱动在运行时调整 BAR 大小，GPU 经常用到

---

## 9. struct pci_dev 核心字段

`include/linux/pci.h:351` 开始：

```c
// pci.h:351-365
struct pci_dev {
    struct list_head bus_list;          // 挂在 bus->devices 链表上
    struct pci_bus  *bus;               // 所属总线
    struct pci_bus  *subordinate;       // 如果是桥，指向下游总线

    void        *sysdata;
    struct proc_dir_entry *procent;
    struct pci_slot  *slot;

    unsigned int    devfn;              // 设备号 + 功能号
    unsigned short  vendor;             // Vendor ID (e.g. 0x10DE = NVIDIA)
    unsigned short  device;             // Device ID
    unsigned short  subsystem_vendor;
    unsigned short  subsystem_device;
    unsigned int    class;              // 3 bytes: (base, sub, prog-if)
```

```c
// pci.h:376-380 — MSI 和 PCIe 相关
    u32     devcap;                     // PCIe Device Capabilities
    u16     rebar_cap;                  // Resizable BAR capability offset
    u8      pcie_cap;                   // PCIe capability offset
    u8      msi_cap;                    // MSI capability offset
    u8      msix_cap;                   // MSI-X capability offset
```

```c
// pci.h:447-454 — 资源相关
    int     cfg_size;                   // 配置空间大小
    unsigned int    irq;                // 当前中断号
    struct resource resource[DEVICE_COUNT_RESOURCE]; // I/O 和内存资源
    struct resource driver_exclusive_resource;       // 驱动独占资源
```

`DEVICE_COUNT_RESOURCE` 通常是 17（6 个 BAR + ROM + 可能的 bridge 窗口 + SR-IOV BAR），第 2 篇详细展开。

---

## 10. 扫描结果：sysfs 中的体现

枚举完成后，每个 `struct pci_dev` 都在 sysfs 中有对应目录：

```
/sys/bus/pci/devices/0000:00:00.0/
├── vendor           → 0x8086 (Intel)
├── device           → 0x1234
├── class            → 0x060000 (Host bridge)
├── config           → 原始配置空间 (binary blob)
├── resource         → BAR 信息
├── msi_bus          → MSI routing
└── ...
```

总线拓扑可以通过 `lspci -t` 查看：

```
-[0000:00]-+-00.0  Intel Host Bridge
           +-01.0-[01]--+-00.0  NVIDIA GPU
           |            \-00.1  NVIDIA Audio
           +-02.0-[02]----00.0  Mellanox RDMA NIC
           \-03.0-[03]----00.0  Samsung NVMe
```

---

## 11. 总结：枚举的三个关键问题

| 问题 | 答案 |
|------|------|
| 怎么访问配置空间？ | x86 用 `0xCF8/0xCFC` 或 ECAM MMIO，经 `pci_ops` 抽象 |
| 怎么发现设备？ | 扫 32 个 slot，读 vendor ID，0xFFFF=空，非空则 `pci_alloc_dev` |
| 怎么递归？ | `pci_scan_child_bus_extend` 两遍扫描桥，第一遍不动已配置桥，第二遍分配 bus number 给需要 reconfig 的桥 |

枚举是整个 PCIe 子系统的**入口**。没有这一步，后续的 BAR 分配、驱动匹配、MSI-X 配置都无从谈起。

---

## 下一篇文章

[第二篇：BAR/MMIO — 设备的寄存器窗口](./02-bar-mmio.md)
