# 第一篇：DMAR 发现 — ACPI 表的解析与 IOMMU 注册

> 源码：`drivers/iommu/intel/dmar.c` `drivers/iommu/intel/iommu.c` | 头文件：`include/linux/dmar.h` | 系列目录：[Intel IOMMU 内核源码深度解析](./README.md)

---

## 1. 什么是 DMAR 表

DMAR（DMA Remapping Reporting）是 ACPI 中的一个系统描述表，由 BIOS/UEFI 在启动时构建，告知操作系统平台上有哪些 IOMMU 硬件单元及其负责的设备范围。内核通过解析 DMAR 表来发现所有 VT-d 硬件。

DMAR 表包含多种子结构，每种用 `type` 字段区分：

```c
// drivers/iommu/intel/dmar.c:634-650 — parse_dmar_table() 中的回调表
static int __init parse_dmar_table(void)
{
    struct dmar_res_callback cb = {
        .print_entry = true,
        .ignore_unhandled = true,
        .arg[ACPI_DMAR_TYPE_HARDWARE_UNIT] = &drhd_count,
        .cb[ACPI_DMAR_TYPE_HARDWARE_UNIT]   = &dmar_parse_one_drhd,    // 硬件单元
        .cb[ACPI_DMAR_TYPE_RESERVED_MEMORY] = &dmar_parse_one_rmrr,    // 保留内存
        .cb[ACPI_DMAR_TYPE_ROOT_ATS]        = &dmar_parse_one_atsr,    // ATS 根端口
        .cb[ACPI_DMAR_TYPE_HARDWARE_AFFINITY]= &dmar_parse_one_rhsa,   // NUMA 亲和性
        .cb[ACPI_DMAR_TYPE_NAMESPACE]       = &dmar_parse_one_andd,    // ACPI 命名设备
        .cb[ACPI_DMAR_TYPE_SATC]            = &dmar_parse_one_satc,    // SoC ATC
    };
    ...
}
```

| 结构类型 | 枚举值 | 含义 |
|----------|--------|------|
| **DRHD** — DMA Remapping Hardware Unit | `ACPI_DMAR_TYPE_HARDWARE_UNIT` | IOMMU 硬件单元，包含寄存器基地址、PCI segment、设备范围 |
| **RMRR** — Reserved Memory Region Reporting | `ACPI_DMAR_TYPE_RESERVED_MEMORY` | 保留内存区域，DMA 不经过重映射（BIOS/CSM/Legacy 使用） |
| **ATSR** — Root Port ATS Capability | `ACPI_DMAR_TYPE_ROOT_ATS` | 支持 ATS（Address Translation Service）的根端口 |
| **RHSA** — Remapping Hardware Status Affinity | `ACPI_DMAR_TYPE_HARDWARE_AFFINITY` | IOMMU 对应的 NUMA 节点 |
| **ANDD** — ACPI Namespace Device Declaration | `ACPI_DMAR_TYPE_NAMESPACE` | 非 PCI 独立设备（如 HPET）的 IOMMU 关联 |
| **SATC** — SoC Integrated Address Translation Cache | `ACPI_DMAR_TYPE_SATC` | SoC 集成设备的 ATC 信息 |

---

## 2. 关键数据结构

### 2.1 `struct dmar_drhd_unit` — IOMMU 单元描述符

```c
// include/linux/dmar.h:38-50
struct dmar_drhd_unit {
    struct list_head list;          /* 全局链表节点              */
    struct acpi_dmar_header *hdr;   /* ACPI 头指针               */
    u64 reg_base_addr;              /* MMIO 寄存器物理基地址     */
    unsigned long reg_size;         /* 寄存器页面大小            */
    struct dmar_dev_scope *devices; /* 设备 scope 数组           */
    int devices_cnt;                /* scope 设备数量            */
    u16 segment;                    /* PCI domain 号             */
    u8  ignored:1;                  /* 是否忽略此 drhd           */
    u8  include_all:1;              /* 是否覆盖所有本 segment 设备 */
    u8  gfx_dedicated:1;            /* 是否专用于集成显卡        */
    struct intel_iommu *iommu;      /* 指向具体 IOMMU 实例       */
};
```

关键字段：

- **`reg_base_addr`**：IOMMU 寄存器 MMIO 区域的物理地址，从 DRHD ACPI entry 的 `address` 字段获取
- **`reg_size`**：寄存器区域大小 = `2^N × 4KB`，N 从 DRHD header 的 `size` 字段获取
- **`include_all`**：BIT 0 of DRHD flags。`include_all=1` 表示此 IOMMU 负责该 PCI segment 下的所有设备（除非有更精确的 scope 匹配）
- **`segment`**：PCI domain 号，多 socket 系统中每个 socket 可能使用不同的 segment
- **`devices[]`**：`dmar_dev_scope` 数组，精确描述此 IOMMU 负责的总线/设备/功能号列表

### 2.2 `struct dmar_dev_scope` — 设备范围

```c
// include/linux/dmar.h:30-34
struct dmar_dev_scope {
    struct device __rcu *dev;   /* 指向内核 device 对象（后期填充） */
    u8 bus;                     /* PCI 总线号                       */
    u8 devfn;                   /* PCI 设备+功能号                  */
};
```

设备范围枚举类型（`entry_type`）：
- **`ACPI_DMAR_SCOPE_TYPE_ENDPOINT`**：端点设备
- **`ACPI_DMAR_SCOPE_TYPE_BRIDGE`**：PCI 桥
- **`ACPI_DMAR_SCOPE_TYPE_IOAPIC`**：I/O APIC
- **`ACPI_DMAR_SCOPE_TYPE_HPET`**：高精度定时器
- **`ACPI_DMAR_SCOPE_TYPE_NAMESPACE`**：ACPI 命名空间设备

```c
// drivers/iommu/intel/dmar.c:82-103 — dmar_alloc_dev_scope()
void *dmar_alloc_dev_scope(void *start, void *end, int *cnt)
{
    while (start < end) {
        scope = start;
        if (scope->entry_type == ACPI_DMAR_SCOPE_TYPE_NAMESPACE ||
            scope->entry_type == ACPI_DMAR_SCOPE_TYPE_ENDPOINT ||
            scope->entry_type == ACPI_DMAR_SCOPE_TYPE_BRIDGE)
            (*cnt)++;
        else if (scope->entry_type != ACPI_DMAR_SCOPE_TYPE_IOAPIC &&
                 scope->entry_type != ACPI_DMAR_SCOPE_TYPE_HPET)
            pr_warn("Unsupported device scope\n");
        start += scope->length;
    }
    return kzalloc_objs(struct dmar_dev_scope, *cnt);
}
```

---

## 3. 全局链表的组织

### 3.1 `dmar_drhd_units` — 全局 DRHD 链表

所有解析出的 DRHD 单元都注册到一个全局链表中：

```c
// drivers/iommu/intel/dmar.c:60-61
DECLARE_RWSEM(dmar_global_lock);
LIST_HEAD(dmar_drhd_units);
```

- **`dmar_global_lock`**：全局读写信号量，保护 DRHD 链表及 IOMMU 配置
- **`dmar_drhd_units`**：所有 DRHD 单元的头节点

### 3.2 遍历宏

```c
// include/linux/dmar.h:70-87 — 四种遍历宏
#define for_each_drhd_unit(drhd)
    list_for_each_entry_rcu(drhd, &dmar_drhd_units, list, dmar_rcu_check())

#define for_each_active_drhd_unit(drhd)
    list_for_each_entry_rcu(drhd, &dmar_drhd_units, list, dmar_rcu_check())
        if (drhd->ignored) {} else

#define for_each_active_iommu(i, drhd)
    list_for_each_entry_rcu(drhd, &dmar_drhd_units, list, dmar_rcu_check())
        if (i=drhd->iommu, drhd->ignored) {} else

#define for_each_iommu(i, drhd)
    list_for_each_entry_rcu(drhd, &dmar_drhd_units, list, dmar_rcu_check())
        if (i=drhd->iommu, 0) {} else
```

- **`for_each_drhd_unit`**：遍历所有 DRHD（包括 ignored）
- **`for_each_active_drhd_unit`**：跳过 `ignored=1` 的 DRHD
- **`for_each_active_iommu`**：直接遍历活动的 `struct intel_iommu *` 指针
- **`for_each_iommu`**：遍历所有 `struct intel_iommu *`

---

## 4. DMAR 表解析流程

### 4.1 `dmar_table_detect` — 检测 DMAR 表是否存在

```c
// drivers/iommu/intel/dmar.c:568-581
static int __init dmar_table_detect(void)
{
    acpi_status status = AE_OK;

    /* if we could find DMAR table, then there are DMAR devices */
    status = acpi_get_table(ACPI_SIG_DMAR, 0, &dmar_tbl);

    if (ACPI_SUCCESS(status) && !dmar_tbl) {
        pr_warn("Unable to map DMAR\n");
        status = AE_NOT_FOUND;
    }

    return ACPI_SUCCESS(status) ? 0 : -ENOENT;
}
```

通过 `acpi_get_table(ACPI_SIG_DMAR, ...)` 请求 ACPI 子系统查找签名为 "DMAR" 的表。

### 4.2 `parse_dmar_table` — 解析所有子结构

```c
// drivers/iommu/intel/dmar.c:634-679
static int __init parse_dmar_table(void)
{
    struct acpi_table_dmar *dmar;
    int drhd_count = 0;
    int ret;
    struct dmar_res_callback cb = { ... };

    dmar_table_detect();              // 1. 获取 DMAR 表
    dmar_tbl = tboot_get_dmar_table(dmar_tbl);  // 2. TXT 安全启动校验

    dmar = (struct acpi_table_dmar *)dmar_tbl;
    if (!dmar) return -ENODEV;

    // 3. 校验 HAW (Host Address Width)
    if (dmar->width < PAGE_SHIFT - 1) {
        pr_warn("Invalid DMAR haw\n");
        return -EINVAL;
    }

    pr_info("Host address width %d\n", dmar->width + 1);
    ret = dmar_walk_dmar_table(dmar, &cb);
    ...
}
```

**`dmar_walk_dmar_table`** 遍历 DMAR 表中每个子结构，分发到对应的回调：

```c
// drivers/iommu/intel/dmar.c:583-617 — dmar_walk_remapping_entries()
for (iter = start; iter < end; iter = next) {
    next = (void *)iter + iter->length;
    if (iter->type >= ACPI_DMAR_TYPE_RESERVED)
        continue; // 前向兼容未知类型
    else if (cb->cb[iter->type])
        cb->cb[iter->type](iter, cb->arg[iter->type]);
}
```

### 4.3 `dmar_parse_one_drhd` — 解析一个 DRHD

```c
// drivers/iommu/intel/dmar.c:408-456
static int dmar_parse_one_drhd(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_hardware_unit *drhd;
    struct dmar_drhd_unit *dmaru;
    int ret;

    drhd = (struct acpi_dmar_hardware_unit *)header;
    dmaru = dmar_find_dmaru(drhd);
    if (dmaru)
        goto out;             // 已注册则跳过

    dmaru = kzalloc(sizeof(*dmaru) + header->length, GFP_KERNEL);
    // ...

    dmaru->hdr = (void *)(dmaru + 1);
    memcpy(dmaru->hdr, header, header->length);
    dmaru->reg_base_addr = drhd->address;
    dmaru->segment = drhd->segment;
    dmaru->reg_size = 1UL << (drhd->size + 12); // 2^N × 4KB
    dmaru->include_all = drhd->flags & 0x1;
    dmaru->devices = dmar_alloc_dev_scope(...); // 分配设备 scope 数组

    ret = alloc_iommu(dmaru);       // 分配 struct intel_iommu
    dmar_register_drhd_unit(dmaru); // 注册到全局链表
    ...
}
```

---

## 5. DRHD 注册策略

```c
// drivers/iommu/intel/dmar.c:70-80
static void dmar_register_drhd_unit(struct dmar_drhd_unit *drhd)
{
    /*
     * add INCLUDE_ALL at the tail, so scan the list will find it at
     * the very end.
     */
    if (drhd->include_all)
        list_add_tail_rcu(&drhd->list, &dmar_drhd_units);
    else
        list_add_rcu(&drhd->list, &dmar_drhd_units);
}
```

**设计思想**：
- **非 `include_all`** 的 DRHD 加入链表头部（`list_add_rcu`），匹配时先被扫描
- **`include_all`** 的 DRHD 加入链表尾部（`list_add_tail_rcu`），作为"兜底"选项
- 设备查找 IOMMU 时，先精确匹配 scope 列表，找不到再用 `include_all`

```
dmar_drhd_units 全局链表:
HEAD → [DRHD-A: include_all=0, scope: 0:1.0, 0:2.0]
     → [DRHD-B: include_all=0, scope: 0:3.0]
     → [DRHD-C: include_all=1, seg=0]  ← 兜底，最后匹配
     → [DRHD-D: include_all=0, scope: 1:0.0]
     → [DRHD-E: include_all=1, seg=1]  ← 兜底
```

---

## 6. RMRR — 保留内存区域

RMRR 描述 BIOS/固件使用的、不能被 DMA 重映射的内存区域。典型用途：集成显卡的 stolen memory、CSM 兼容内存。

```c
// drivers/iommu/intel/dmar.c:308 — dmar_parse_one_rmrr()
static int dmar_parse_one_rmrr(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_reserved_memory *rmrr;
    struct dmar_rmrr_unit *rmrru;

    rmrr = (struct acpi_dmar_reserved_memory *)header;
    rmrru = kzalloc(sizeof(*rmrru), GFP_KERNEL);
    rmrru->hdr = rmrr;
    rmrru->base_address = rmrr->base_address;
    rmrru->end_address = rmrr->end_address;
    rmrru->devices = dmar_alloc_dev_scope(...); // 哪些设备受此 RMRR 约束
    list_add(&rmrru->list, &dmar_rmrr_units);
    ...
}
```

RMRR 中的设备不能对其 reserved range 的 DMA 请求进行重映射，内核必须设置 1:1 映射（identity mapping）。

### 典型 RMRR 示例

```
RMRR base: 0xBD000000 ~ 0xBFFFFFFF (48MB)
Devices: 00:02.0 (集成显卡)
→ 集成显卡对 0xBD0_0000 ~ 0xBFF_FFFF 的 DMA 不经过 IOMMU 转译
```

---

## 7. ATSR — Root Port 的 ATS 能力

ATSR（ATS Required）标记哪些根端口下挂的设备支持 PCIe ATS（Address Translation Services）。ATS 允许设备缓存 IOVA→HPA 的转换结果，减少 IOMMU 查询延迟。

```c
// drivers/iommu/intel/dmar.c:347 — dmar_parse_one_atsr()
static int dmar_parse_one_atsr(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_atsr *atsr;
    struct dmar_atsr_unit *atsru;

    atsr = (struct acpi_dmar_atsr *)header;
    atsru = kzalloc(sizeof(*atsru), GFP_KERNEL);
    atsru->hdr = atsr;
    // ALL_PORTS flag: 此端口下的所有设备都支持 ATS
    atsru->include_all = atsr->flags & 0x1;
    atsru->devices = dmar_alloc_dev_scope(...);
    list_add(&atsru->list, &dmar_atsr_units);
    ...
}
```

ATSR 与后续章节中的 **设备 TLB (devTLB) 刷新** 直接相关 — 有 ATS 能力的设备必须在 IOTLB Invalidation 时同步刷新 devTLB。

---

## 8. RHSA — NUMA 亲和性

```c
// drivers/iommu/intel/dmar.c:496 — dmar_parse_one_rhsa()
static int dmar_parse_one_rhsa(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_rhsa *rhsa;
    struct dmar_drhd_unit *drhd;

    rhsa = (struct acpi_dmar_rhsa *)header;
    for_each_drhd_unit(drhd) {
        if (drhd->reg_base_addr == rhsa->base_address) {
            int node = acpi_map_pxm_to_online_node(rhsa->proximity_domain);
            drhd->iommu->node = node;
            break;
        }
    }
    return 0;
}
```

RHSA 通过 `proximity_domain` 关联到 ACPI SRAT 表中的 NUMA 节点，确保 IOMMU 的 DMA 缓冲区分配从本地 NUMA 节点取内存。

---

## 9. ANDD — ACPI 命名空间设备

```c
// drivers/iommu/intel/dmar.c:467 — dmar_parse_one_andd()
static int dmar_parse_one_andd(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_andd *andd = (void *)header;

    // 检查 device_name 是否为 NUL-terminated
    if (strnlen(andd->device_name, header->length - 8) == header->length - 8) {
        pr_warn(FW_BUG "ANDD object name is not NUL-terminated\n");
    }
    ...
}
```

ANDD 将非 PCI 设备（如平台设备）关联到 IOMMU，这在 SoC 集成场景中常见。

---

## 10. SATC — SoC ATC

```c
// drivers/iommu/intel/dmar.c:547 — dmar_parse_one_satc()
static int dmar_parse_one_satc(struct acpi_dmar_header *header, void *arg)
{
    struct acpi_dmar_satc *satc = (struct acpi_dmar_satc *)header;
    pr_info("SATC flags: 0x%x\n", satc->flags);
    ...
}
```

SoC ATC 描述集成在 SoC 中的设备端地址转换缓存。

---

## 11. 设备 → IOMMU 的查找

当内核需要为一个 PCI 设备找到对应的 IOMMU 时：

```c
// drivers/iommu/intel/iommu.c:457-516 — device_lookup_iommu()
static struct intel_iommu *device_lookup_iommu(struct device *dev, u8 *bus, u8 *devfn)
{
    struct dmar_drhd_unit *drhd = NULL;
    struct pci_dev *pdev = NULL;
    struct intel_iommu *iommu;
    struct device *tmp;
    u16 segment = 0;
    int i;

    if (dev_is_pci(dev)) {
        struct pci_dev *pf_pdev;
        pdev = pci_real_dma_dev(to_pci_dev(dev));

        /* VFs aren't listed in scope tables; we need to look up
         * the PF instead to find the IOMMU. */
        pf_pdev = pci_physfn(pdev);
        dev = &pf_pdev->dev;            // SR-IOV VF → 使用 PF 查找
        segment = pci_domain_nr(pdev->bus);
    } else if (has_acpi_companion(dev))
        dev = &ACPI_COMPANION(dev)->dev;

    rcu_read_lock();
    for_each_iommu(iommu, drhd) {
        if (pdev && segment != drhd->segment)
            continue;

        // 1. 精确匹配 scope 列表
        for_each_active_dev_scope(drhd->devices, drhd->devices_cnt, i, tmp) {
            if (tmp == dev) {
                if (pdev && pdev->is_virtfn)
                    goto got_pdev;
                if (bus && devfn) {
                    *bus = drhd->devices[i].bus;
                    *devfn = drhd->devices[i].devfn;
                }
                goto out;
            }
            // 2. 下游桥设备
            if (is_downstream_to_pci_bridge(dev, tmp))
                goto got_pdev;
        }

        // 3. include_all 兜底
        if (pdev && drhd->include_all) {
got_pdev:
            if (bus && devfn) {
                *bus = pdev->bus->number;
                *devfn = pdev->devfn;
            }
            goto out;
        }
    }
    ...
}
```

查找策略：
1. **精确匹配**：遍历每个 DRHD 的 `devices[]` scope，匹配 `BDF` 或 `ACPI companion`
2. **下游桥匹配**：设备在 scope 中的 bridge 下游，则也归此 IOMMU
3. **`include_all` 兜底**：段内找不到精确匹配时用 `include_all` 的 IOMMU
4. **SR-IOV 特殊处理**：VF 不在 scope 表中，通过 PF（`pci_physfn`）查找

---

## 12. 完整初始化序列

```c
// drivers/iommu/intel/iommu.c:2552-2631 — intel_iommu_init()
int __init intel_iommu_init(void)
{
    int ret = -ENODEV;
    struct dmar_drhd_unit *drhd;
    struct intel_iommu *iommu;

    // 检查强制开启（TXT/tboot 安全启动）
    force_on = (!intel_iommu_tboot_noforce && tboot_force_iommu()) ||
                platform_optin_force_iommu();

    down_write(&dmar_global_lock);
    if (dmar_table_init()) {        // ① 解析 DMAR 表
        if (force_on) panic(...);
        goto out_free_dmar;
    }

    if (dmar_dev_scope_init() < 0) { // ② 初始化设备 scope
        if (force_on) panic(...);
        goto out_free_dmar;
    }
    up_write(&dmar_global_lock);

    dmar_register_bus_notifier();   // ③ 注册 PCI 总线通知器

    down_write(&dmar_global_lock);

    if (!no_iommu)
        intel_iommu_debugfs_init(); // ④ debugfs 初始化

    if (no_iommu || dmar_disabled) {
        intel_disable_iommus();     // ⑤ 关闭所有 IOMMU（kexec 场景）
        goto out_free_dmar;
    }

    // ⑥ 检查可选项
    if (list_empty(&dmar_rmrr_units))
        pr_info("No RMRR found\n");
    if (list_empty(&dmar_atsr_units))
        pr_info("No ATSR found\n");

    init_no_remapping_devices();

    ret = init_dmars();             // ⑦ 初始化所有 IOMMU 硬件
    if (ret) { ... }

    up_write(&dmar_global_lock);
    ...
}
```

完整流程图：

```
intel_iommu_init()
│
├── dmar_table_init()
│   └── parse_dmar_table()
│       ├── dmar_table_detect()           → acpi_get_table("DMAR")
│       ├── dmar_walk_dmar_table()
│       │   ├── dmar_parse_one_drhd()     → 注册 DRHD → dmar_drhd_units 链表
│       │   ├── dmar_parse_one_rmrr()     → 注册 RMRR → dmar_rmrr_units 链表
│       │   ├── dmar_parse_one_atsr()     → 注册 ATSR → dmar_atsr_units 链表
│       │   ├── dmar_parse_one_rhsa()     → NUMA 节点绑定
│       │   ├── dmar_parse_one_andd()     → ACPI 设备绑定
│       │   └── dmar_parse_one_satc()     → SoC ATC 绑定
│       └── dmar_validate_drhd()          → 验证寄存器可访问
│
├── dmar_dev_scope_init()                 → 初始化设备 scope（ACPI 设备查找）
├── dmar_register_bus_notifier()          → 监听 PCI 设备热插拔
├── init_dmars()                          → 初始化每个 IOMMU 硬件
│   ├── intel_iommu_setup_hardware()      → 配置寄存器（RTADDR/CCMD/GCMD）
│   ├── intel_iommu_init_qi()             → 初始化 Queued Invalidation
│   ├── intel_svm_check()                 → SVA 兼容性检查
│   └── iommu_device_register()           → 注册到 IOMMU 核心层
│
└── up_write(&dmar_global_lock)
```

---

## 13. DMAR 表到 IOMMU 对象的关系

```
ACPI DMAR Table (内存中)
│
├── DRHD Entry #0 (ACPI_DMAR_HARDWARE_UNIT)
│   ├── address  = 0xFED90000           → iommu->reg_phys
│   ├── segment  = 0                    → iommu->segment
│   ├── size     = 1                    → reg_size = 2^1 × 4KB = 8KB
│   ├── flags/include_all = 0
│   └── Device Scope:
│       ├── Endpoint: bus=0, dev=2, func=0  (显卡)
│       └── Endpoint: bus=0, dev=3, func=0  (网卡)
│
├── DRHD Entry #1
│   ├── address  = 0xFED91000
│   ├── segment  = 1
│   └── flags/include_all = 1
│
├── RMRR Entry: base=0xBD000000, end=0xBFFFFFFF, target: 0:2.0
├── ATSR Entry: root port 0:1.0
└── RHSA Entry: DRHD=0xFED90000, NUMA node=0
        │
        ▼
struct dmar_drhd_unit (dmar.h:38)        struct intel_iommu (iommu.h:682)
┌───────────────────────────┐           ┌──────────────────────────────┐
│ list (→全局链表)          │           │ reg → MMIO 虚拟地址           │
│ hdr → ACPI 结构           │           │ reg_phys = 0xFED90000         │
│ reg_base_addr ← 硬件地址  │  ──→     │ cap/ecap/vccap              │
│ reg_size ← 硬件计算       │           │ gcmd = TE|EAFL|SRTP|SFL|WBF │
│ devices[] ← scope 设备    │           │ root_entry → Root Table      │
│ devices_cnt               │           │ qi → Queued Invalidation     │
│ segment = 0               │           │ ir_table → IRTE Table        │
│ include_all = 0/1         │           │ device_rbtree ← 跟踪所有设备 │
│ gfx_dedicated             │           │ segment = 0                  │
│ iommu ────────────────────┘           │ drhd ──────────→ 回指        │
└───────────────────────────┘           │ node ← NUMA 节点             │
                                        └──────────────────────────────┘
```

---

## 14. 锁序

DMAR 子系统使用 `dmar_global_lock`（`rwsem`）作为顶层保护：

```c
// drivers/iommu/intel/dmar.c:60
DECLARE_RWSEM(dmar_global_lock);
```

```
dmar_global_lock (rwsem)
│
├── 保护 dmar_drhd_units 链表
├── 保护 dmar_rmrr_units 链表
├── 保护 dmar_atsr_units 链表
├── 保护 IOMMU register 操作
│
└── RCU 读端：for_each_drhd_unit/for_each_iommu
    不持锁时读需要 rcu_read_lock()
```

---

## 15. 关键行号速查

| 符号 | 文件 | 行号 | 说明 |
|------|------|------|------|
| `LIST_HEAD(dmar_drhd_units)` | dmar.c | 61 | 全局 DRHD 链表 |
| `DECLARE_RWSEM(dmar_global_lock)` | dmar.c | 60 | 全局锁 |
| `dmar_register_drhd_unit` | dmar.c | 70 | DRHD 注册函数 |
| `dmar_alloc_dev_scope` | dmar.c | 82 | 设备 scope 分配 |
| `dmar_parse_one_drhd` | dmar.c | 408 | 单个 DRHD 解析 |
| `dmar_parse_one_rmrr` | dmar.c | 308 | RMRR 解析 |
| `dmar_parse_one_atsr` | dmar.c | 347 | ATSR 解析 |
| `dmar_parse_one_rhsa` | dmar.c | 496 | RHSA 解析 |
| `dmar_parse_one_andd` | dmar.c | 467 | ANDD 解析 |
| `dmar_parse_one_satc` | dmar.c | 547 | SATC 解析 |
| `dmar_table_detect` | dmar.c | 568 | DMAR 表检测 |
| `parse_dmar_table` | dmar.c | 634 | 表解析 |
| `dmar_table_init` | dmar.c | 844 | 表初始化入口 |
| `for_each_drhd_unit` | dmar.h | 70 | 遍历宏 |
| `for_each_active_iommu` | dmar.h | 79 | 活跃 IOMMU 遍历宏 |
| `device_lookup_iommu` | iommu.c | 457 | 设备→IOMMU 查找 |
| `intel_iommu_init` | iommu.c | 2552 | 主初始化入口 |

---

## 下一篇文章

[第二篇：IOMMU 引擎 — 硬件寄存器、关键数据结构与初始化](02-engine.md)
