# 第四篇：中断重映射与 PASID — DMA 之外的能力

> 源码：`drivers/iommu/intel/irq_remapping.c` `drivers/iommu/intel/pasid.c` `drivers/iommu/intel/pasid.h` | 系列目录：[Intel IOMMU 内核源码深度解析](./README.md)

---

## 第一部分：中断重映射

## 1. 为什么需要中断重映射

在没有 IOMMU 中断重映射的传统 x86 系统中，设备 MSI/MSI-X 中断直接写入 CPU 的 Local APIC（或 x2APIC），Destination ID 和 Vector 由 PCI 配置空间写入。这带来两个问题：

1. **虚拟化隔离**：恶意或故障设备可以伪造中断消息，触发任意 CPU 中断向量
2. **地址空间限制**：xAPIC 的 8-bit Destination ID 限制了两 socket 以上的扩展

VT-d 中断重映射通过在 IOMMU 中引入 **IRTE (Interrupt Remapping Table Entry)** 来解决这些问题。

### 中断重映射流程

```
PCIe MSI/MSI-X TLP (设备 → IOMMU → 系统总线)
│
│  传统路径 (无 IR):
│  ┌──────────────────────────────────────────┐
│  │ MSI Addr = 0xFEEXXXXX                    │
│  │ MSI Data = Vector + Delivery Mode etc.   │
│  │ 直接写入 LAPIC                            │
│  └──────────────────────────────────────────┘
│
│  IOMMU 重映射路径 (有 IR):
│  ┌──────────────────────────────────────────┐
│  │ MSI Addr = 0xFEEXXXXX + IR Index         │
│  │ MSI Data = 写入 IRTE 指定的格式           │
│  │              ↓                            │
│  │ IOMMU 截获 → IRTE 查找                    │
│  │              ↓                            │
│  │ 转换：destination ID + vector + delivery  │
│  │              ↓                            │
│  │ 经过重映射的中断消息 → LAPIC/x2APIC        │
│  └──────────────────────────────────────────┘
```

---

## 2. IRTE 表结构

### 2.1 `struct ir_table` — 中断重映射表

```c
// drivers/iommu/intel/iommu.h:501-504
#define INTR_REMAP_TABLE_ENTRIES  65536

struct ir_table {
    struct irte *base;          /* 指向 IRTE 表 (物理连续)  */
    unsigned long *bitmap;      /* IRTE 分配位图            */
};
```

IRTE 表固定为 65536 个条目，每条目 16 字节（总计 1MB），由 `bitmap` 管理分配。

### 2.2 `struct irq_2_iommu` — IRQ → IOMMU 映射

```c
// drivers/iommu/intel/irq_remapping.c:43-50
struct irq_2_iommu {
    struct intel_iommu *iommu;  /* 所属 IOMMU               */
    u16 irte_index;             /* IRTE 表中的索引          */
    u16 sub_handle;             /* 子句柄 (用于多条目分配)  */
    u8  irte_mask;              /* 掩码 (0 表示单条目)      */
    bool posted_msi;            /* 是否使用 Posted MSI      */
    bool posted_vcpu;           /* 是否投递到虚拟 CPU       */
};
```

### 2.3 `struct intel_ir_data` — 每个 IRQ 的私有数据

```c
// drivers/iommu/intel/irq_remapping.c:52-58
struct intel_ir_data {
    struct irq_2_iommu  irq_2_iommu;
    struct irte         irte_entry;     /* IRTE 格式值       */
    union {
        struct msi_msg  msi_entry;      /* MSI 消息          */
    };
};
```

---

## 3. IRTE 格式

标准的 IRTE 128-bit 格式（VT-d spec 3.0, section 9.10）：

```
Bit   名称            说明
─────────────────────────────────────────────────────────
127   IRTA_E          扩展中断使能 (判断 EIM 模式)
82    Posted Mode     1=Posted Interrupt, 0=Remapped
79:72 Vector          中断向量 (0-255)
63:56 DST (xAPIC)     Destination ID, 低 8 位
48:40 DST (x2APIC)    扩展 Destination ID 高 8 位
31:16 DST (x2APIC)    扩展 Destination ID 低 16 位 → 总计 32 位
18    IM              IRTE 索引模式
17:16 AVL             Available / 软件可用
15    TM              Trigger Mode (0=edge, 1=level)
12    RH              重定向提示 (Redirect Hint)
11    DM              Delivery Mode (0=Fixed, 1=LowPri, ...)
8     FPD             故障处理禁用
4:0   DST (xAPIC)     高 5 位 (xAPIC 模式组合后总计 8 位)
```

### x2APIC vs xAPIC 模式

```c
// drivers/iommu/intel/irq_remapping.c:60-61
#define IR_X2APIC_MODE(mode)  (mode ? (1 << 11) : 0)
#define IRTE_DEST(dest)       ((eim_mode) ? dest : dest << 8)
```

**xAPIC 模式** (eim_mode=0)：
- Destination ID 8-bit → 最多 256 CPUs
- DST 字段分散在两个位置组合

**x2APIC 模式** (eim_mode=1)：
- Destination ID 32-bit → 支持大规模多处理器
- 需要 `ecap_eim_support` (iommu.h:214) 为 1

---

## 4. IRTE 分配与管理

### 4.1 分配 IRTE

```c
// drivers/iommu/intel/irq_remapping.c:104-145 — alloc_irte()
static int alloc_irte(struct intel_iommu *iommu,
                      struct irq_2_iommu *irq_iommu, u16 count)
{
    struct ir_table *table = iommu->ir_table;
    unsigned int mask = 0;
    unsigned long flags;
    int index;

    if (count > 1) {
        count = __roundup_pow_of_two(count);
        mask = ilog2(count);         // 多条目: 2^mask 对齐
    }

    if (mask > ecap_max_handle_mask(iommu->ecap))
        return -1;

    raw_spin_lock_irqsave(&irq_2_ir_lock, flags);
    index = bitmap_find_free_region(table->bitmap,
                                    INTR_REMAP_TABLE_ENTRIES, mask);
    raw_spin_unlock_irqrestore(&irq_2_ir_lock, flags);

    irq_iommu->iommu = iommu;
    irq_iommu->irte_index = index;
    irq_iommu->sub_handle = 0;
    irq_iommu->irte_mask = mask;
    ...
    return index;
}
```

### 4.2 锁序

```c
// drivers/iommu/intel/irq_remapping.c:67-78
/*
 * Lock ordering:
 * ->dmar_global_lock
 *      ->irq_2_ir_lock
 *              ->qi->q_lock
 *      ->iommu->register_lock
 * Note:
 * intel_irq_remap_ops.{supported,prepare,enable,disable,reenable} are called
 * in single-threaded environment with interrupt disabled, so no need to take
 * the dmar_global_lock.
 */
DEFINE_RAW_SPINLOCK(irq_2_ir_lock);
```

```
dmar_global_lock (rwsem)
│
├── irq_2_ir_lock (raw_spinlock)  ← 保护 IRTE 位图分配
│   └── qi->q_lock (raw_spinlock) ← 保护 QI 提交
│
└── iommu->register_lock (raw_spinlock) ← 保护寄存器操作
```

---

## 5. 中断重映射的启停

### 5.1 准备阶段

```c
// drivers/iommu/intel/irq_remapping.c:700-766 — intel_prepare_irq_remapping()
static int __init intel_prepare_irq_remapping(void)
{
    // 1. 检查硬件 erratum
    if (irq_remap_broken) { ... }

    // 2. 确保 DMAR 表存在
    if (dmar_table_init() < 0) return -ENODEV;

    // 3. 检查 DMAR 中断重映射支持
    if (!dmar_ir_support()) return -ENODEV;

    // 4. 解析 IOAPIC scope
    if (parse_ioapics_under_ir()) { ... }

    // 5. 验证所有 IOMMU 都支持 IR
    for_each_iommu(iommu, drhd)
        if (!ecap_ir_support(iommu->ecap)) goto error;

    // 6. 检测 x2APIC 模式
    if (x2apic_supported()) {
        eim = !dmar_x2apic_optout();
        if (!eim) pr_info("x2apic disabled by BIOS\n");
    }

    for_each_iommu(iommu, drhd) {
        if (eim && !ecap_eim_support(iommu->ecap)) {
            eim = 0;  // 降级到 xAPIC
        }
    }

    eim_mode = eim;

    // 7. 为每个 IOMMU 设置中断重映射
    for_each_iommu(iommu, drhd) {
        if (intel_setup_irq_remapping(iommu))
            goto error;
    }

    return 0;
}
```

### 5.2 启用阶段

```c
// drivers/iommu/intel/irq_remapping.c:797-826 — intel_enable_irq_remapping()
static int __init intel_enable_irq_remapping(void)
{
    for_each_iommu(iommu, drhd) {
        if (!ir_pre_enabled(iommu))
            iommu_enable_irq_remapping(iommu);  // 写 GCMD 寄存器
        setup = true;
    }

    irq_remapping_enabled = 1;
    set_irq_posting_cap();

    pr_info("Enabled IRQ remapping in %s mode\n",
            eim_mode ? "x2apic" : "xapic");

    return eim_mode ? IRQ_REMAP_X2APIC_MODE : IRQ_REMAP_XAPIC_MODE;
}
```

### 5.3 Posted Interrupt

Posted Interrupt 是 x2APIC 模式下的高级特性，用于虚拟化场景：

```c
// drivers/iommu/intel/irq_remapping.c:771-795 — set_irq_posting_cap()
static inline void set_irq_posting_cap(void)
{
    if (!disable_irq_post) {
        // PI 需要 CPU cmpxchg16b 支持 (跨 128-bit 边界的原子更新)
        if (boot_cpu_has(X86_FEATURE_CX16))
            intel_irq_remap_ops.capability |= 1 << IRQ_POSTING_CAP;

        // 所有 IOMMU 都必须支持 cap_pi_support
        for_each_iommu(iommu, drhd)
            if (!cap_pi_support(iommu->cap)) {
                intel_irq_remap_ops.capability &=
                        ~(1 << IRQ_POSTING_CAP);
                break;
            }
    }
}
```

Posted Interrupt 允许 IOMMU 直接将中断投递到虚拟 CPU 的 vAPIC，绕过 VMM，显著降低虚拟化中断延迟。

---

## 6. 中断重映射硬件交互图

```
设备 MSI/MSI-X 配置
│
│  pci_alloc_irq_vectors() / request_irq()
│
├── 软件设置:
│   ├── alloc_irte(iommu, irq_iommu, count)  ← 分配 IRTE
│   ├── 填充 IRTE 字段 (DST, Vector, DM, TM)
│   ├── 计算 MSI Address = 0xFEEXXXXX | IRTE_INDEX
│   ├── 计算 MSI Data
│   └── pci_write_config_dword(dev, MSI_ADDR, addr)
│       pci_write_config_dword(dev, MSI_DATA, data)
│
├── 硬件路径 (设备发起 MSI):
│   │
│   │  PCIe TLP: MWr { Addr=0xFEE00000 | IRTE_INDEX, Data }
│   │          │
│   │          ▼
│   │  IOMMU 拦截:
│   │  ├── 检查 Addr[19:5] 是否在中断地址范围 (0xFEEX_XXXX)
│   │  ├── 读取 IRTE[index] → DST, Vector, DM, TM, Posted
│   │  ├── 权限检查: req_id 匹配
│   │  ├── Posted Mode?
│   │  │   ├── 是 → 写入 vAPIC page (虚拟化直通)
│   │  │   └── 否 → 构造 xAPIC/x2APIC 消息 → 系统总线 → LAPIC
│   │  └── IQA invalidation (如果在 IRTE 修改后)
│   │
│   └── CPU LAPIC → IRR → ISR → EOI
│
└── 软件释放:
    ├── free_irq()
    └── bitmap_release_region(ir_table->bitmap, irte_index, mask)
```

---

## 第二部分：PASID

## 7. PASID 概念

PASID（Process Address Space ID）是一个 **20-bit** 的标识符，允许单个 PCIe 设备同时使用多个 I/O 页表。这样，不同进程可以拥有独立的 IOVA 空间。

```c
// drivers/iommu/intel/pasid.h:13-19
#define PASID_MAX           0x100000    // 1,048,576 (2^20)
#define MAX_NR_PASID_BITS   20
#define PASID_PTE_MASK      0x3F
#define PASID_PTE_PRESENT   1
#define PASID_PTE_FPD       2           // Fault Processing Disable
#define PDE_PFN_MASK        PAGE_MASK
#define PASID_PDE_SHIFT     6           // 每个目录项指向 64 条 PASID
#define PASID_TBL_ENTRIES   BIT(PASID_PDE_SHIFT)
```

### 全局 PASID 限制

```c
// drivers/iommu/intel/pasid.c:28
u32 intel_pasid_max_id = PASID_MAX;  // = 0x100000

// 每个 IOMMU 初始化时可以限缩:
// iommu.c:1631-1632
intel_pasid_max_id = min_t(u32, temp, intel_pasid_max_id);
```

---

## 8. PASID 译类型

```c
// drivers/iommu/intel/pasid.h:43-46
#define PASID_ENTRY_PGTT_FL_ONLY   (1)  /* First-Level only — SVA          */
#define PASID_ENTRY_PGTT_SL_ONLY   (2)  /* Second-Level only — IOVA        */
#define PASID_ENTRY_PGTT_NESTED    (3)  /* Nested — FL on SL (VM)          */
#define PASID_ENTRY_PGTT_PT        (4)  /* Pass-Through — identity mapping */
```

| 类型 | 用途 | 页表格式 |
|------|------|---------|
| **FL_ONLY** | Shared Virtual Addressing (SVA) | First-level x86-64 page table |
| **SL_ONLY** | Per-PASID IOVA mapping | Second-level VT-d page table |
| **NESTED** | VM: guest VA → guest PA → host PA | First-level nested on second-level |
| **PT** | Pass-through (identity) | 无转换 |

---

## 9. PASID 表结构

### 9.1 表层次结构

```
PASID Table Hierarchy (3-level)
│
│  设备 → Context Entry (scalable mode)
│      │
│      └── PASID Directory Table (PDE)
│          │  pasid_dir_entry[512] (9-bit dir index)
│          │
│          └── PASID Table Entry Group (每 512 条为一个 table)
│                pasid_entry[PASID % 512]
│                  └── pasid_entry.val[8] ← 64 字节 per entry
│
└── PASID 地址:
    | Dir Index (9 bits) | Table Index (6 bits) | Reserved |
    | PASID[19:11]       | PASID[10:6]          | PASID[5:0]|
```

### 9.2 关键数据结构

```c
// drivers/iommu/intel/pasid.h:35-52
struct pasid_dir_entry {
    u64 val;                // 指向一个 PASID Table Entry Group
};

struct pasid_entry {
    u64 val[8];             // 64 字节 PASID entry (8 × u64)
};

struct pasid_table {
    void    *table;         // PASID directory table 虚拟地址
    u32     max_pasid;      // 最大 PASID 值
};
```

### 9.3 访问宏

```c
// pasid.h:55-58 — 检查 Directory Entry Present
static inline bool pasid_pde_is_present(struct pasid_dir_entry *pde)
{
    return READ_ONCE(pde->val) & PASID_PTE_PRESENT;
}

// pasid.h:61-68 — 从 PDE 获取 PASID Table
static inline struct pasid_entry *
get_pasid_table_from_pde(struct pasid_dir_entry *pde)
{
    if (!pasid_pde_is_present(pde))
        return NULL;
    return phys_to_virt(READ_ONCE(pde->val) & PDE_PFN_MASK);
}

// pasid.h:71-74 — 检查 PASID Entry Present
static inline bool pasid_pte_is_present(struct pasid_entry *pte)
{
    return READ_ONCE(pte->val[0]) & PASID_PTE_PRESENT;
}
```

---

## 10. PASID 表分配

```c
// drivers/iommu/intel/pasid.c:38-78 — intel_pasid_alloc_table()
int intel_pasid_alloc_table(struct device *dev)
{
    struct device_domain_info *info;
    struct pasid_table *pasid_table;
    struct pasid_dir_entry *dir;
    u32 max_pasid = 0;
    int order, size;

    info = dev_iommu_priv_get(dev);
    pasid_table = kzalloc_obj(*pasid_table);

    // 确定 PASID 表大小
    if (info->pasid_supported)
        max_pasid = min_t(u32, pci_max_pasids(to_pci_dev(dev)),
                          intel_pasid_max_id);

    // 每个目录项覆盖 64 个 PASID (PASID_PDE_SHIFT=6)
    // 每个目录项 8 字节 → size = (max_pasid >> 6) × 8
    size = max_pasid >> (PASID_PDE_SHIFT - 3);
    order = size ? get_order(size) : 0;

    // 从 IOMMU 所在 NUMA 节点分配
    dir = iommu_alloc_pages_node_sz(info->iommu->node, GFP_KERNEL,
                                    1 << (order + PAGE_SHIFT));

    pasid_table->table = dir;
    pasid_table->max_pasid = 1 << (order + PAGE_SHIFT + 3);
    info->pasid_table = pasid_table;

    // 非 coherent 架构需要 cache flush
    if (!ecap_coherent(info->iommu->ecap))
        clflush_cache_range(pasid_table->table, (1 << order) * PAGE_SIZE);

    return 0;
}
```

---

## 11. PASID Entry 配置

### 11.1 First-Level Setup

```c
// drivers/iommu/intel/pasid.c:381-418 — intel_pasid_setup_first_level()
int intel_pasid_setup_first_level(struct intel_iommu *iommu, struct device *dev,
                                  phys_addr_t fsptptr, u32 pasid, u16 did,
                                  int flags)
{
    struct pasid_entry *pte;

    // 检查硬件能力
    if (!ecap_flts(iommu->ecap))
        return -EINVAL;

    if ((flags & PASID_FLAG_FL5LP) && !cap_fl5lp_support(iommu->cap))
        return -EINVAL;

    spin_lock(&iommu->lock);
    pte = intel_pasid_get_entry(dev, pasid);  // 获取或分配 PASID entry
    if (pasid_pte_is_present(pte)) {
        spin_unlock(&iommu->lock);
        return -EBUSY;
    }

    pasid_pte_config_first_level(iommu, pte, fsptptr, did, flags);
    spin_unlock(&iommu->lock);

    pasid_flush_caches(iommu, pte, pasid, did);  // IOTLB cache flush
    return 0;
}
```

### 11.2 Second-Level Setup

```c
// drivers/iommu/intel/pasid.c:423-444 — pasid_pte_config_second_level()
static void pasid_pte_config_second_level(struct intel_iommu *iommu,
                                          struct pasid_entry *pte,
                                          struct dmar_domain *domain, u16 did)
{
    pasid_clear_entry(pte);
    pasid_set_domain_id(pte, did);
    pasid_set_slptr(pte, pt_info.ssptptr);         // 第二级页表指针
    pasid_set_address_width(pte, pt_info.aw);
    pasid_set_translation_type(pte, PASID_ENTRY_PGTT_SL_ONLY);
    pasid_set_fault_enable(pte);
    pasid_set_page_snoop(pte, ...);                // Snoop 控制
    if (domain->dirty_tracking)
        pasid_set_ssade(pte);                      // Dirty tracking
    pasid_set_present(pte);
}
```

### 11.3 Pass-Through Setup

```c
// drivers/iommu/intel/pasid.c:572-596 — intel_pasid_setup_pass_through()
int intel_pasid_setup_pass_through(struct intel_iommu *iommu,
                                   struct device *dev, u32 pasid)
{
    u16 did = FLPT_DEFAULT_DID;  // Domain ID = 1

    spin_lock(&iommu->lock);
    pte = intel_pasid_get_entry(dev, pasid);
    if (pasid_pte_is_present(pte))
        return -EBUSY;

    pasid_pte_config_pass_through(iommu, pte, did);
    spin_unlock(&iommu->lock);
    pasid_flush_caches(iommu, pte, pasid, did);
    return 0;
}
```

### 11.4 Nested Setup

```c
// drivers/iommu/intel/pasid.c:621-654 — pasid_pte_config_nestd()
static void pasid_pte_config_nestd(struct intel_iommu *iommu,
                                   struct pasid_entry *pte,
                                   struct iommu_hwpt_vtd_s1 *s1_cfg,
                                   struct dmar_domain *s2_domain,
                                   u16 did)
{
    pasid_clear_entry(pte);

    if (s1_cfg->addr_width == ADDR_WIDTH_5LEVEL)
        pasid_set_flpm(pte, 1);     // 5-level first-level

    pasid_set_flptr(pte, s1_cfg->pgtbl_addr);  // S1 页表地址

    if (s1_cfg->flags & IOMMU_VTD_S1_SRE) {
        pasid_set_sre(pte);          // Supervisor Request Enable
        if (s1_cfg->flags & IOMMU_VTD_S1_WPE)
            pasid_set_wpe(pte);      // Write Protect Enable
    }

    if (s1_cfg->flags & IOMMU_VTD_S1_EAFE)
        pasid_set_eafe(pte);         // Extended Access Flag Enable

    ...
}
```

---

## 12. 综合架构图

```
IOMMU Hardware
│
├── GCMD_REG (0x18)
│   ├── TE (bit 31)  — DMA Translation Enable
│   ├── IRE (bit 25) — Interrupt Remapping Enable
│   └── QIE (bit 26) — Queued Invalidation Enable
│
├── RTADDR_REG (0x20) → Root Table → Context Table → DMA PT
│
├── IRTA_REG (0xB8) → IRTE Table (65536 entries)
│   ├── IRTE[0]: {DST, Vector, Delivery Mode, Posted?}
│   ├── IRTE[1]: ...
│   └── IRTE[N]: ...
│
├── QI (Queued Invalidation)
│   ├── IQH_REG (0x80) — Head pointer
│   ├── IQT_REG (0x88) — Tail pointer
│   └── IQA_REG (0x90) — Queue address
│
├── PRQ (Page Request Queue, PRI)
│   ├── PQH_REG (0xC0) — Head
│   ├── PQT_REG (0xC8) — Tail
│   └── PQA_REG (0xD0) — Address
│
├── Fault Registers
│   ├── FSTS_REG (0x34) — Fault Status
│   └── FRCD (0x40+) — Fault Recording Registers
│
└── PerfMon (VT-d 4.0+)
    └── PERFCAP_REG (0x300) → Counters
```

---

## 13. PASID 与中断重映射的交互

在虚拟化场景中，PASID 和中断重映射协同工作：

```
VM Guest
│
├── vCPU (PASID-aware)
│   │
│   ├── GPU 计算 (PASID=101): 第一级页表 (guest VA → guest PA)
│   │   └── 嵌套在 S2 (guest PA → host PA)
│   │
│   └── NVMe 中断 (IRTE[x]):
│       ├── Posted Interrupt → 直接投递到 vCPU's vAPIC page
│       └── 无需 VM-Exit
│
└── IOMMUFD (用户态控制)
    ├── IOAS → IOVA 空间
    ├── HWPT_PAGING / HWPT_NESTED → 硬件页表
    └── VIOMMU → 安全设备分配
```

---

## 14. 关键行号速查

| 符号 | 文件 | 行号 | 说明 |
|------|------|------|------|
| `struct irq_2_iommu` | irq_remapping.c | 43 | IRQ→IOMMU 映射 |
| `struct intel_ir_data` | irq_remapping.c | 52 | per-IRQ 数据 |
| `DEFINE_RAW_SPINLOCK(irq_2_ir_lock)` | irq_remapping.c | 78 | IRTE 分配锁 |
| `alloc_irte` | irq_remapping.c | 104 | IRTE 分配 |
| `intel_prepare_irq_remapping` | irq_remapping.c | 700 | IR 准备 |
| `intel_enable_irq_remapping` | irq_remapping.c | 797 | IR 启用 |
| `set_irq_posting_cap` | irq_remapping.c | 771 | Posted Interrupt |
| `intel_pasid_max_id` | pasid.c | 28 | 全局 PASID 限制 |
| `intel_pasid_alloc_table` | pasid.c | 38 | PASID 表分配 |
| `intel_pasid_free_table` | pasid.c | 80 | PASID 表释放 |
| `intel_pasid_setup_first_level` | pasid.c | 381 | FL 配置 |
| `intel_pasid_setup_second_level` | pasid.c | 446 | SL 配置 |
| `intel_pasid_setup_pass_through` | pasid.c | 572 | PT 配置 |
| `intel_pasid_setup_nested` | pasid.c | 675 | Nested 配置 |
| `pasid_pte_config_nestd` | pasid.c | 621 | Nested PTE 填充 |
| `pasid_pte_config_second_level` | pasid.c | 423 | SL PTE 填充 |
| `struct pasid_entry` | pasid.h | 39 | PASID entry (val[8]) |
| `struct pasid_table` | pasid.h | 49 | PASID 表 |
| `PASID_MAX` | pasid.h | 13 | 最大 PASID (0x100000) |
| `PASID_ENTRY_PGTT_FL_ONLY` | pasid.h | 43 | FL 类型常量 |
| `PASID_ENTRY_PGTT_SL_ONLY` | pasid.h | 44 | SL 类型常量 |
| `PASID_ENTRY_PGTT_NESTED` | pasid.h | 45 | Nested 类型常量 |
| `PASID_ENTRY_PGTT_PT` | pasid.h | 46 | PT 类型常量 |

---

## 下一篇文章

[第五篇：SVA、嵌套翻译与 IOMMUFD — VT-d 的未来](05-sva-nested.md)
