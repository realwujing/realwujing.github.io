# 第三篇：驱动注册与匹配

> 源码：`drivers/pci/pci-driver.c` | 头文件：`include/linux/pci.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. 概述

枚举阶段内核建立了 `struct pci_dev` 对象树，BAR 也分配好了。现在的问题是：怎么让正确的驱动（如 `nvidia.ko`、`mlx5_core.ko`）接管正确的设备？

答案依赖三个核心机制：
1. **`struct pci_driver`** — 驱动向 PCI 子系统自我介绍
2. **`pci_bus_type`** — Linux 驱动模型中的 bus_type，负责 match/probe/remove
3. **`pci_device_id` 匹配表** — Vendor/Device/Class 的组合匹配

---

## 2. struct pci_driver

`include/linux/pci.h:1021-1038`：

```c
// pci.h:1021-1038
struct pci_driver {
    const char      *name;
    const struct pci_device_id *id_table;   /* Must be non-NULL for probe to be called */
    int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove)(struct pci_dev *dev);
    int  (*suspend)(struct pci_dev *dev, pm_message_t state);
    int  (*resume)(struct pci_dev *dev);
    void (*shutdown)(struct pci_dev *dev);
    int  (*sriov_configure)(struct pci_dev *dev, int num_vfs);  /* On PF */
    int  (*sriov_set_msix_vec_count)(struct pci_dev *vf, int msix_vec_count);
    u32  (*sriov_get_vf_total_msix)(struct pci_dev *pf);
    const struct pci_error_handlers *err_handler;
    const struct attribute_group **groups;
    const struct attribute_group **dev_groups;
    struct device_driver    driver;
    struct pci_dynids       dynids;
    bool driver_managed_dma;
};
```

关键字段解释：

| 字段 | 含义 |
|------|------|
| `id_table` | 匹配表，NULL 则不调用 probe |
| `probe` | 匹配成功后调用，驱动初始化入口 |
| `remove` | 设备拔出或驱动卸载时调用 |
| `suspend/resume` | 电源管理回调 |
| `sriov_configure` | SR-IOV 配置回调 |
| `err_handler` | PCIe AER 错误处理回调（第 5 篇） |
| `dynids` | 动态 ID，通过 sysfs new_id 添加 |
| `driver_managed_dma` | 驱动自行管理 DMA 映射 |

---

## 3. pci_bus_type — 连接 pci_dev 和 pci_driver

`drivers/pci/pci-driver.c:1732-1749`：

```c
// pci-driver.c:1732-1749
const struct bus_type pci_bus_type = {
    .name       = "pci",
    .driver_override = true,
    .match      = pci_bus_match,
    .uevent     = pci_uevent,
    .probe      = pci_device_probe,
    .remove     = pci_device_remove,
    .shutdown   = pci_device_shutdown,
    .irq_get_affinity = pci_device_irq_get_affinity,
    .dev_groups = pci_dev_groups,
    .bus_groups = pci_bus_groups,
    .drv_groups = pci_drv_groups,
    .pm         = PCI_PM_OPS_PTR,
    .num_vf     = pci_bus_num_vf,
    .dma_configure = pci_dma_configure,
    .dma_cleanup = pci_dma_cleanup,
};
EXPORT_SYMBOL(pci_bus_type);
```

这是 Linux 驱动模型的标准 `struct bus_type`。所有 PCI 设备都挂在 `pci_bus_type` 上，所有 PCI 驱动也注册到这个 bus_type。

核心工作流：

```
注册驱动:  driver_register()  →  bus_for_each_dev()
              ↓
匹配设备:  bus->match()       →  pci_bus_match()  →  pci_match_device()
              ↓
调用 probe: bus->probe()      →  pci_device_probe()
              ↓
            drv->probe()      →  驱动入口
```

`driver_override = true` 意味着用户可以在 sysfs 中强制指定驱动：
```bash
echo nvidia > /sys/bus/pci/devices/0000:03:00.0/driver_override
```

---

## 4. pci_match_device — 匹配引擎

`drivers/pci/pci-driver.c:136-180`：

```c
// pci-driver.c:136-180
static const struct pci_device_id *pci_match_device(struct pci_driver *drv,
                            struct pci_dev *dev)
{
    struct pci_dynid *dynid;
    const struct pci_device_id *found_id = NULL, *ids;
    int ret;

    // ═══ Step 1: driver_override 检查 ═══
    ret = device_match_driver_override(&dev->dev, &drv->driver);
    if (ret == 0)
        return NULL;               // override 指向其他驱动 → 不匹配
```

**Line 143-146**: 如果设备的 `driver_override` 已设置且指向的不是当前驱动，直接返回 NULL。这是 sysfs 强制绑定的机制。

```c
    // ═══ Step 2: 动态 ID 表优先匹配 ═══
    spin_lock(&drv->dynids.lock);
    list_for_each_entry(dynid, &drv->dynids.list, node) {
        if (pci_match_one_device(&dynid->id, dev)) {
            found_id = &dynid->id;
            break;
        }
    }
    spin_unlock(&drv->dynids.lock);

    if (found_id)
        return found_id;
```

**Lines 148-159**: 遍历 `dynids` 链表（通过 sysfs `new_id` 添加的 ID）。动态 ID 的优先级**高于**静态 ID 表。这意味着用户可以在不修改驱动代码的情况下支持新设备：
```bash
echo "10de 2684" > /sys/bus/pci/drivers/nvidia/new_id
```

```c
    // ═══ Step 3: 静态 id_table 匹配 ═══
    for (ids = drv->id_table; (found_id = pci_match_id(ids, dev));
         ids = found_id + 1) {
        /*
         * The match table is split based on driver_override.
         * In case override_only was set, enforce driver_override
         * matching.
         */
        if (found_id->override_only) {
            if (ret > 0)
                return found_id;
        } else {
            return found_id;
        }
    }
```

**Lines 161-174**: 遍历驱动的 `id_table`，调用 `pci_match_id()`。每找到一个匹配项，检查 `override_only` 标志：
- `override_only=0`：正常匹配，直接返回
- `override_only=1`：仅在 `driver_override` 匹配时才返回（即强制绑定场景）

```c
    // ═══ Step 4: driver_override fallback ═══
    /* driver_override will always match, send a dummy id */
    if (ret > 0)
        return &pci_device_id_any;
    return NULL;
}
```

**Lines 176-178**: 如果 `driver_override` 指向当前驱动，但静态表中没有匹配条目，返回通配符 `pci_device_id_any`（匹配所有设备）。

### 4.1 匹配决策树

```
                pci_match_device(drv, dev)
                         │
                ┌────────┴────────┐
                │ driver_override │
                │ 是否已设置？     │
                └────────┬────────┘
                    是  │  否
                ┌───────┴───────┐
                │ 指向本驱动？   │
                └───────┬───────┘
                  yes   │   no → return NULL
                        │
            ┌───────────┼───────────┐
            ▼           ▼           ▼
      dynids 匹配?   id_table?   driver_override
         │              │            │
    yes  │         yes  │       yes  │
         ▼              ▼            ▼
    return dynid   return entry   return pci_device_id_any
    (最高优先级)   (正常匹配)     (强制绑定)
```

---

## 5. pci_device_id 匹配字段

`pci_match_one_device()` 检查以下字段的匹配：

```c
struct pci_device_id {
    __u32 vendor, device;          // Vendor ID, Device ID
    __u32 subvendor, subdevice;    // Subsystem Vendor/Device ID
    __u32 class, class_mask;       // Class Code + Mask
    kernel_ulong_t driver_data;    // 驱动私有数据
    __u32 override_only;           // 仅 driver_override 匹配
};
```

匹配规则：

| 字段 | 匹配规则 |
|------|---------|
| `vendor` | `PCI_ANY_ID`（0xFFFF）→ 通配；精确匹配 |
| `device` | `PCI_ANY_ID` → 通配；精确匹配 |
| `subvendor` | `PCI_ANY_ID` → 通配；精确匹配 |
| `subdevice` | `PCI_ANY_ID` → 通配；精确匹配 |
| `class` | `(dev->class & class_mask) == class` |

### 5.1 常用匹配宏

```c
// vendor + device 精确匹配
#define PCI_DEVICE(vend, dev) \
    .vendor = (vend), .device = (dev), \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

// class code 匹配
#define PCI_DEVICE_CLASS(dev_class, dev_class_mask) \
    .class = (dev_class), .class_mask = (dev_class_mask), \
    .vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

// vendor + device + subvendor + subdevice 全匹配
#define PCI_VDEVICE(vend, dev) \
    .vendor = PCI_VENDOR_ID_##vend, .device = (dev), \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
```

### 5.2 典型 id_table 示例

```c
static const struct pci_device_id nvkm_device_table[] = {
    { PCI_DEVICE(0x10DE, 0x2684), .driver_data = 0x170 },  // A100
    { PCI_DEVICE(0x10DE, 0x2330), .driver_data = 0x190 },  // H100
    { PCI_DEVICE(0x10DE, 0x2342), .driver_data = 0x199 },  // H200
    { }
};
MODULE_DEVICE_TABLE(pci, nvkm_device_table);
```

`driver_data` 是一个 `kernel_ulong_t`，驱动可以用它传递任意数据（如芯片代际、特性位掩码）。

---

## 6. pci_device_probe — 从匹配到 probe

`drivers/pci/pci-driver.c:473-496`：

```c
// pci-driver.c:473-496
static int pci_device_probe(struct device *dev)
{
    int error;
    struct pci_dev *pci_dev = to_pci_dev(dev);
    struct pci_driver *drv = to_pci_driver(dev->driver);

    if (!pci_device_can_probe(pci_dev))           // 设备不可用？→ -ENODEV
        return -ENODEV;

    pci_assign_irq(pci_dev);                      // ★ 分配 IRQ

    error = pcibios_alloc_irq(pci_dev);           // arch 级 IRQ 分配
    if (error < 0)
        return error;

    pci_dev_get(pci_dev);                         // 增加引用计数
    error = __pci_device_probe(drv, pci_dev);     // 调用驱动的 probe()
    if (error) {
        pcibios_free_irq(pci_dev);
        pci_dev_put(pci_dev);                     // 减少引用计数
    }

    return error;
}
```

关键步骤：
1. `pci_device_can_probe()` — 检查设备状态（是否被其他驱动占用、是否在复位中等）
2. `pci_assign_irq()` — **在 probe 之前分配 IRQ**。这意味着驱动的 `probe()` 被调用时，`pci_dev->irq` 已经是有效的
3. `__pci_device_probe()` — 最终调用 `drv->probe(pci_dev, id)`

### 6.1 __pci_device_probe 内部

```c
static int __pci_device_probe(struct pci_driver *drv, struct pci_dev *pci_dev)
{
    const struct pci_device_id *id;
    int error = 0;

    if (drv->probe) {
        error = -ENODEV;
        id = pci_match_device(drv, pci_dev);     // 再次匹配获取 driver_data
        if (id)
            error = drv->probe(pci_dev, id);     // 调用驱动入口
    }
    return error;
}
```

这里有一个**第二次匹配**：虽然 `pci_bus_type.match()` 已经匹配过了，但 `__pci_device_probe` 再匹配一次是为了拿到 `pci_device_id` 指针，以便将 `driver_data` 传递给驱动的 `probe()`。

---

## 7. __pci_register_driver — 驱动注册入口

`drivers/pci/pci-driver.c:1471-1488`：

```c
// pci-driver.c:1471-1488
int __pci_register_driver(struct pci_driver *drv, struct module *owner,
              const char *mod_name)
{
    /* initialize common driver fields */
    drv->driver.name = drv->name;
    drv->driver.bus = &pci_bus_type;          // ★ 绑定到 pci_bus_type
    drv->driver.owner = owner;
    drv->driver.mod_name = mod_name;
    drv->driver.groups = drv->groups;
    drv->driver.dev_groups = drv->dev_groups;

    spin_lock_init(&drv->dynids.lock);
    INIT_LIST_HEAD(&drv->dynids.list);

    /* register with core */
    return driver_register(&drv->driver);      // 标准驱动模型注册
}
EXPORT_SYMBOL(__pci_register_driver);
```

驱动通常使用宏封装：
```c
#define pci_register_driver(drv) \
    __pci_register_driver(drv, THIS_MODULE, KBUILD_MODNAME)
```

调用 `driver_register()` 会触发驱动模型的自动匹配流程：
1. 将驱动加入 `pci_bus_type` 的驱动列表
2. 遍历该 bus_type 下所有已注册的设备
3. 对每个设备调用 `pci_bus_type.match()`
4. 匹配成功则调用 `pci_bus_type.probe()` → `drv->probe()`

这意味着驱动注册时，**已存在的设备会被自动匹配并 probe**。

---

## 8. Quirks — 驱动匹配前的修正

`drivers/pci/quirks.c` 是一个巨大的修正表，在 PCI 扫描和驱动 probe 之间运行。它包含数千个对特定设备的 workaround（勘误修正）。

```c
// quirks.c 中的一个典型条目
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
            quirk_nvidia_hda);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID,
            quirk_nvidia_hda);
```

quirks 有多个运行阶段：

| 阶段 | 时机 | 用途 |
|------|------|------|
| `FIXUP_EARLY` | 设备刚分配、capability 初始化之前 | 修正 class code、header type |
| `FIXUP_HEADER` | 在 `pci_setup_device` 之后 | 修正 header 配置 |
| `FIXUP_FINAL` | 在 `pci_init_capabilities` 之后、驱动 probe 之前 | 修正 BAR、中断配置 |
| `FIXUP_ENABLE` | 在 `pci_enable_device` 时 | 设备使能时应用 |
| `FIXUP_SUSPEND` / `FIXUP_RESUME` | 挂起/恢复阶段 | 电源管理修正 |

quirks 的优先级高于驱动：在驱动看到设备之前，quirks 已经修正了设备的行为。

---

## 9. 完整 probe 时序

```
pci_scan_device()
    │
    ├── pci_setup_device()         → dev->vendor, dev->device, dev->class
    │
    ├── pci_init_capabilities()    → AER, MSI, ATS, PRI, SR-IOV...
    │
    ├── __pci_read_base()          → BAR 大小探测、resource[n] 初始化
    │
    └── (回到 pci_scan_child_bus_extend，完成枚举)

    ... 资源分配阶段 ...
    pci_assign_unassigned_resources()
    ├── pci_bus_assign_resources()  → 给各 BAR 分配地址
    └── pci_std_update_resource()   → 写入 BAR 寄存器

    ... 驱动注册 ...
    __pci_register_driver()
    └── driver_register()
        └── bus_for_each_dev()
            └── pci_bus_type.match()  → pci_match_device()
                └── pci_bus_type.probe() → pci_device_probe()
                    ├── pci_assign_irq()           ★ IRQ 就位
                    ├── pci_enable_device()        ★ 使能设备 (quirks FINAL)
                    ├── pcibios_alloc_irq()
                    └── drv->probe(dev, id)        ★ 驱动入口
                        ├── pci_request_regions()  → 驱动声称 BAR
                        ├── pci_alloc_irq_vectors()→ MSI-X 分配 (第4篇)
                        ├── ioremap()              → 映射 BAR 到内核虚拟地址
                        └── request_irq()          → 注册中断处理函数
```

---

## 10. 总结

| 问题 | 答案 |
|------|------|
| 驱动怎么注册？ | `__pci_register_driver()` 设置 `driver.bus = &pci_bus_type`，然后 `driver_register()` |
| 怎么匹配？ | `pci_match_device()`：driver_override → dynids → id_table → fallback |
| IRQ 什么时候分配？ | `pci_device_probe()` 在调用驱动的 probe 之前调用 `pci_assign_irq()` |
| 动态 ID 怎么加？ | `echo "vendor device" > /sys/bus/pci/drivers/xxx/new_id` |
| quirks 跑在什么时候？ | 扫描之后、驱动 probe 之前，多个阶段依次执行 |

匹配完成只是开始。接下来驱动需要建立中断通道，这就是下一篇文章要讲的 MSI/MSI-X。

---

## 下一篇文章

[第四篇：MSI/MSI-X 中断机制](./04-msi-msix.md)
