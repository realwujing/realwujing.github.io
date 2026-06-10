# 第二篇：IOMMU 引擎 — 硬件寄存器、关键数据结构与初始化

> 源码：`drivers/iommu/intel/iommu.h` `drivers/iommu/intel/iommu.c` | 系列目录：[Intel IOMMU 内核源码深度解析](./README.md)

---

## 1. 核心数据结构全景

VT-d 的代码围绕几个中心数据结构展开，理解它们的层次关系是掌握全部代码的前提。

### 1.1 `struct intel_iommu` — IOMMU 实例

```c
// drivers/iommu/intel/iommu.h:682-737
struct intel_iommu {
    void __iomem    *reg;           /* MMIO 寄存器虚拟地址               */
    u64             reg_phys;       /* 寄存器组物理地址                  */
    u64             reg_size;       /* 寄存器组大小                      */
    u64             cap;            /* 能力寄存器值                       */
    u64             ecap;           /* 扩展能力寄存器值                   */
    u64             vccap;          /* 虚拟命令能力                       */
    u64             ecmdcap[DMA_MAX_NUM_ECMDCAP]; /* 扩展命令能力        */
    u32             gcmd;           /* GCMD 缓存值 (TE, EAFL, SRTP...)  */
    raw_spinlock_t  register_lock;  /* 寄存器访问保护                    */
    int             seq_id;         /* IOMMU 序列号                      */
    int             agaw;           /* 实际 AGAW                         */
    int             msagaw;         /* 最大支持的 AGAW                    */
    unsigned int    irq, pr_irq, perf_irq;  /* 三类中断线                */
    u16             segment;        /* PCI segment                       */
    unsigned char   name[16];       /* 设备名称                          */

    /* 域管理 */
    struct mutex    did_lock;
    struct ida      domain_ida;      /* 域 ID 分配器                    */
    unsigned long   *copied_tables;  /* 已拷贝 context 表位图            */
    spinlock_t      lock;            /* context/domain id 保护           */
    struct root_entry *root_entry;   /* Root Table 虚拟地址              */

    struct iommu_flush flush;

    /* 页面请求 */
    struct page_req_dsc *prq;        /* Page Request Queue               */
    unsigned char   prq_name[16];
    unsigned long   prq_seq_number;
    struct completion prq_complete;
    struct iopf_queue *iopf_queue;
    unsigned char   iopfq_name[16];
    struct mutex    iopf_lock;

    /* Queued Invalidation */
    struct q_inval  *qi;            /* QI 队列                           */

    u32 iommu_state[MAX_SR_DMAR_REGS];  /* 休眠/恢复状态                */

    /* 设备跟踪 */
    struct rb_root  device_rbtree;   /* 所有 probed 设备的红黑树         */
    spinlock_t      device_rbtree_lock;

    /* 中断重映射 */
    struct ir_table *ir_table;       /* IRTE 表                          */
    struct irq_domain *ir_domain;

    struct iommu_device iommu;       /* IOMMU 核心层句柄                  */
    int             node;            /* NUMA 节点                         */
    u32             flags;           /* 软件标志                          */
    struct dmar_drhd_unit *drhd;     /* 回指 DRHD                        */
    void            *perf_statistic;
    struct iommu_pmu *pmu;           /* 性能计数器                        */
};
```

每个 `intel_iommu` 对应一个物理 VT-d 硬件单元，通过 `drhd` 指针回指到 DMAR 解析出的 `dmar_drhd_unit`。

### 1.2 `struct dmar_domain` — DMA 地址域

```c
// drivers/iommu/intel/iommu.h:592-639
struct dmar_domain {
    union {
        struct iommu_domain domain;     /* IOMMU 核心层 domain */
        struct pt_iommu iommu;
        struct pt_iommu_x86_64 fspt;    /* 第一阶段（first-stage）页表 */
        struct pt_iommu_vtdss sspt;     /* 第二阶段（second-stage）页表 */
    };

    struct xarray iommu_array;          /* 已绑定的 IOMMU 数组 */

    u8 force_snooping:1;
    u8 dirty_tracking:1;               /* 脏页面跟踪 */
    u8 nested_parent:1;                 /* 是否有嵌套域以其为 S2 */
    u8 iotlb_sync_map:1;               /* 建立映射时是否需要 flush IOTLB */

    spinlock_t lock;                    /* 设备列表保护 */
    struct list_head devices;          /* 设备链表 */
    struct list_head dev_pasids;       /* 附加的 PASID 链表 */

    spinlock_t cache_lock;             /* cache_tag 列表保护 */
    struct list_head cache_tags;       /* 缓存标签链表 */
    struct qi_batch *qi_batch;         /* 批量 QI 描述符 */

    union {
        /* DMA 重映射域 */
        struct {
            spinlock_t s1_lock;
            struct list_head s1_domains;
        };
        /* 嵌套用户域 */
        struct {
            struct dmar_domain *s2_domain;    /* S2 父域 */
            struct iommu_hwpt_vtd_s1 s1_cfg;
            struct list_head s2_link;
        };
        /* SVA 域 */
        struct {
            struct mm_struct *mm;
        };
    };
};
```

关键转换宏：

```c
// iommu.h:784-786
static inline struct dmar_domain *to_dmar_domain(struct iommu_domain *dom)
{
    return container_of(dom, struct dmar_domain, domain);
}
```

Domain ID 分配说明（iommu.h:789-804）：
- **Domain ID 0**：保留给 caching mode 的 invalid translation，非分配标记
- **Domain ID 1**：保留给 first-level/pass-through/nested 模式（`FLPT_DEFAULT_DID`）
- **可用域 ID**：从 2 开始 — 每个 IOMMU 独立维护 `domain_ida`

### 1.3 `struct device_domain_info` — 设备-域绑定

```c
// drivers/iommu/intel/iommu.h:740-765
struct device_domain_info {
    struct list_head link;       /* 域内兄弟链表                    */
    u32 segment;                 /* PCI segment                     */
    u8 bus;                      /* PCI 总线号                      */
    u8 devfn;                    /* PCI devfn                       */
    u16 pfsid;                   /* SRIOV PF source ID (DIT 模式)  */
    u8 pasid_supported:3;
    u8 pasid_enabled:1;
    u8 pri_supported:1;
    u8 pri_enabled:1;
    u8 ats_supported:1;
    u8 ats_enabled:1;
    u8 dtlb_extra_inval:1;       /* quirk: 需要额外 devTLB 刷新    */
    u8 domain_attached:1;
    u8 ats_qdep;                 /* ATS 队列深度                    */
    unsigned int iopf_refcount;
    struct device *dev;          /* 指向设备 (PCI bridge 为 NULL)   */
    struct intel_iommu *iommu;  /* 所属 IOMMU                      */
    struct dmar_domain *domain;  /* 所属域                          */
    struct pasid_table *pasid_table; /* PASID 表                    */
    struct rb_node node;         /* IOMMU device_rbtree 节点        */
#ifdef CONFIG_INTEL_IOMMU_DEBUGFS
    struct dentry *debugfs_dentry;
#endif
};
```

每个设备通过 `device_domain_info` 绑定到特定 IOMMU 和域，同时携带 ATS/PRI/PASID 能力标志。

---

## 2. 页表层级数据结构

### 2.1 `struct root_entry` — 根表项

```c
// drivers/iommu/intel/iommu.h:550-553
/*
 * 0: Present
 * 1-11: Reserved
 * 12-63: Context Ptr (12 - (haw-1))
 * 64-127: Reserved
 */
struct root_entry {
    u64     lo;
    u64     hi;
};
```

每个 root_entry 对应一个 **PCI bus number**（256 个 root_entry 覆盖 0-255 bus）。`root_entry.lo[12:63]` 指向该 bus 的 context table 物理地址。

Scalable mode 下 root_entry 用法不同 — 同时包含 lower 和 upper context table 指针。

### 2.2 `struct context_entry` — 上下文表项

```c
// drivers/iommu/intel/iommu.h:566-569
/*
 * low 64 bits:
 * 0: present
 * 1: fault processing disable
 * 2-3: translation type
 * 12-63: address space root
 * high 64 bits:
 * 0-2: address width
 * 3-6: aval
 * 8-23: domain id
 */
struct context_entry {
    u64 lo;
    u64 hi;
};
```

Context entry 的 translation type（iommu.h:60-63）：
- **`CONTEXT_TT_MULTI_LEVEL (0)`**：标准多级 DMA 页表
- **`CONTEXT_TT_DEV_IOTLB (1)`**：使用设备 IOTLB 的旁路模式
- **`CONTEXT_TT_PASS_THROUGH (2)`**：直通模式（不转换）
- **`CONTEXT_PASIDE (bit 3)`**：PASID 模式（允许 per-PASID 页表）

Context entry 操作宏：

```c
// iommu.h:865-867 — 检查 present
static inline bool context_present(struct context_entry *context)
{
    return (context->lo & 1);
}

// iommu.h:925-930 — 设置 translation type
static inline void context_set_translation_type(struct context_entry *context,
                                                unsigned long value)
{
    context->lo &= ~((u64)0x3 << 2);
    context->lo |= (value & 0x3) << 2;
}

// iommu.h:945-949 — 设置 domain id
static inline void context_set_domain_id(struct context_entry *context,
                                         unsigned long value)
{
    context->hi |= (value & GENMASK_ULL(15, 0)) << 8;
}

// iommu.h:951-954 — 设置 PASID enable
static inline void context_set_pasid(struct context_entry *context)
{
    context->lo |= CONTEXT_PASIDE;
}
```

### 2.3 `struct dma_pte` — DMA 页表项

```c
// drivers/iommu/intel/iommu.h:841-843
/*
 * 0: readable
 * 1: writable
 * 2-6: reserved
 * 7: super page  (bit 7 标记大页)
 * 8-10: available
 * 11: snoop behavior
 * 12-63: Host physical address
 */
struct dma_pte {
    u64 val;
};

// iommu.h:845-853 — 读取物理地址
static inline u64 dma_pte_addr(struct dma_pte *pte)
{
    return pte->val & VTD_PAGE_MASK;
}

// iommu.h:855-858 — 检查 present
static inline bool dma_pte_present(struct dma_pte *pte)
{
    return (pte->val & 3) != 0;    // bit 0-1 不全为 0 即 present
}

// iommu.h:860-863 — 检查超级页
static inline bool dma_pte_superpage(struct dma_pte *pte)
{
    return (pte->val & DMA_PTE_LARGE_PAGE);
}
```

---

## 3. 页表地址转换层级

VT-d 页表走查从 root_entry 开始，经过 context_entry 后进入多级 DMA 页表：

```
Device → DMA Request (IOVA)
│
├── PCI Bus# ──→ Root Table (256 entries, 4KB aligned)
│   │               [bus 0] root_entry.lo → context table
│   │               [bus 1] root_entry
│   │               ...
│   │
│   └── Context Table (256 entries per bus = 14-bit devfn)
│       │               [devfn=N] context_entry
│       │                   ├── translation_type = CONTEXT_TT_MULTI_LEVEL
│       │                   ├── domain_id
│       │                   ├── address_width
│       │                   └── address_space_root → DMA Page Table
│       │
│       └── 3-Level DMA Page Table (standard, AGAW ≤ 48-bit)
│           │
│           │  dma_pte[level=2] → 512 entries (9 bits ea.)
│           │       │   addr[47:39] index into L2
│           │       │
│           │       └── dma_pte[level=1] → 512 entries
│           │               │   addr[38:30] index into L1
│           │               │
│           │               └── dma_pte[level=0] → 512 entries
│           │                       │   addr[29:21] index into L0
│           │                       │   HPA = pte[12:63] | addr[20:0]
│           │
│           └── 超级页 shortcut:
│               L2 superpage: 1GB (bit 7 set in L2 pte)
│               L1 superpage: 2MB (bit 7 set in L1 pte)
│
└── 5-Level DMA Page Table (LA57, AGAW = 57-bit)
        dma_pte[level=4] → L4 (9 bits, addr[56:48])
            └── dma_pte[level=3] → ... same as 3-level from here
```

AGAW → Level 转换 (iommu.h:875-878)：

```c
static inline int agaw_to_level(int agaw)
{
    return agaw + 2;  // AGAW 30 → 4 levels (32-bit), AGAW 39 → 5 levels (48-bit)
}
```

`LEVEL_STRIDE = 9` (iommu.h:870) 意味着每层覆盖 9 位地址 (512 entries × 4KB = 2MB per page table)。

---

## 4. MMIO 寄存器映射

VT-d 硬件寄存器通过 MMIO 访问，基础定义在 `iommu.h:65-143`：

```
Offset  Register               说明
─────────────────────────────────────────────────────────────
0x00    DMAR_VER_REG          版本号 (MAJ.MIN)
0x08    DMAR_CAP_REG          能力寄存器 (64-bit)
0x10    DMAR_ECAP_REG         扩展能力寄存器 (64-bit)
0x18    DMAR_GCMD_REG         全局命令寄存器
0x1C    DMAR_GSTS_REG         全局状态寄存器
0x20    DMAR_RTADDR_REG       根表地址 (Root Table Address)
0x28    DMAR_CCMD_REG         上下文命令寄存器
0x34    DMAR_FSTS_REG         故障状态寄存器
0x38    DMAR_FECTL_REG        故障控制寄存器
0x3C    DMAR_FEDATA_REG       故障事件数据
0x40    DMAR_FEADDR_REG       故障事件地址 (基址)
0x44    DMAR_FEUADDR_REG      故障事件上地址
0x64    DMAR_PMEN_REG         PMR 使能
0x68-0x78 DMAR_PLMBASE/PLMLIMIT/PHMBASE/PHMLIMIT  PMR 范围
0x80    DMAR_IQH_REG          QI 队列头
0x88    DMAR_IQT_REG          QI 队列尾
0x90    DMAR_IQA_REG          QI 队列地址
0x9C    DMAR_ICS_REG          QI 完成状态
0xB0    DMAR_IQER_REG         QI 错误记录
0xB8    DMAR_IRTA_REG         中断重映射表地址
0xC0    DMAR_PQH_REG          页面请求队列头
0xC8    DMAR_PQT_REG          页面请求队列尾
0xD0    DMAR_PQA_REG          页面请求队列地址
0xDC    DMAR_PRS_REG          页面请求状态
0xE0    DMAR_PECTL_REG        页面请求事件控制
0x100+  DMAR_MTRRCAP_REG      MTRR 能力
0x300   DMAR_PERFCAP_REG      性能计数器能力 (VT-d 4.0+)
0x380   DMAR_PERFEVNTCAP_REG  性能事件能力
0x400   DMAR_ECMD_REG         扩展命令寄存器
```

### 4.1 GCMD (Global Command) 位定义

```c
// iommu.h:288-305
#define DMA_GCMD_TE     BIT(31)   // Translation Enable
#define DMA_GCMD_SRTP   BIT(30)   // Set Root Table Pointer
#define DMA_GCMD_SFL    BIT(29)   // Set Fault Log
#define DMA_GCMD_EAFL   BIT(28)   // Enable Advanced Fault Logging
#define DMA_GCMD_WBF    BIT(27)   // Write Buffer Flush
#define DMA_GCMD_QIE    BIT(26)   // Queued Invalidation Enable
#define DMA_GCMD_SIRTP  BIT(24)   // Set Interrupt Remap Table Pointer
#define DMA_GCMD_IRE    BIT(25)   // Interrupt Remapping Enable
#define DMA_GCMD_CFI    BIT(23)   // Compatibility Format Interrupt
```

### 4.2 GSTS (Global Status) 位定义

```c
// iommu.h:307-325
#define DMA_GSTS_TES    BIT(31)   // Translation Enable Status
#define DMA_GSTS_RTPS   BIT(30)   // Root Table Pointer Status
#define DMA_GSTS_FLS    BIT(29)   // Fault Log Status
#define DMA_GSTS_AFLS   BIT(28)   // Advanced Fault Logging Status
#define DMA_GSTS_WBFS   BIT(27)   // Write Buffer Flush Status
#define DMA_GSTS_QIES   BIT(26)   // Queued Invalidation Enable Status
#define DMA_GSTS_IRTPS  BIT(24)   // Interrupt Remap Table Pointer Status
#define DMA_GSTS_IRES   BIT(25)   // Interrupt Remap Enable Status
#define DMA_GSTS_CFIS   BIT(23)   // Compatibility Format Interrupt Status
```

---

## 5. CAP / ECAP 寄存器解码

CAP 寄存器（iommu.h:157-184）编码 IOMMU 的能力：

| 宏 | CAP 位 | 含义 |
|----|--------|------|
| `cap_esrtps(c)` | 63 | Enhanced Set Root Table Pointer Support |
| `cap_fl5lp_support(c)` | 60 | 5-level paging (LA57) 支持 |
| `cap_pi_support(c)` | 59 | Posted Interrupt 支持 |
| `cap_fl1gp_support(c)` | 56 | 1GB 第一级大页支持 |
| `cap_read_drain(c)` | 55 | Read drain 支持 |
| `cap_write_drain(c)` | 54 | Write drain 支持 |
| `cap_max_amask_val(c)` | 48-53 | 最大地址掩码值 (for page-selective invalidation) |
| `cap_num_fault_regs(c)` | 40-47 | 故障记录寄存器数量 |
| `cap_pgsel_inv(c)` | 39 | Page-selective invalidation 支持 |
| `cap_super_page_val(c)` | 34-37 | 超级页位图 (bit0=4KB, bit1=2MB, bit2=1GB) |
| `cap_mgaw(c)` | 16-21 | 最大 Guest Address Width (MGAW = value + 1) |
| `cap_sagaw(c)` | 8-12 | 支持的 AGAW 位图 |
| `cap_caching_mode(c)` | 7 | Caching mode (需要 cache flush) |
| `cap_rwbf(c)` | 4 | Read/Write Buffer Flush 需要 |
| `cap_afl(c)` | 3 | Advanced Fault Logging 支持 |
| `cap_ndoms(c)` | 0-2 | 最大 domain 数量 = 2^(4+2×value) |

ECAP 寄存器（iommu.h:189-218）编码扩展能力：

| 宏 | ECAP 位 | 含义 |
|----|---------|------|
| `ecap_pms(e)` | 51 | Performance Monitoring Support |
| `ecap_rps(e)` | 49 | RID-PASID Support |
| `ecap_smpwc(e)` | 48 | Scalable Mode PASID Write Clear |
| `ecap_flts(e)` | 47 | First Level Translation Support |
| `ecap_slts(e)` | 46 | Second Level Translation Support |
| `ecap_smts(e)` | 43 | Scalable Mode Translation Support |
| `ecap_dit(e)` | 41 | Device IOTLB Throttling |
| `ecap_pds(e)` | 42 | Page-request Drain Support |
| `ecap_pasid(e)` | 40 | PASID Support |
| `ecap_pss(e)` | 35-39 | PASID Size Supported (2^pss bit = max PASID entries) |
| `ecap_prs(e)` | 29 | Page Request Support |
| `ecap_nest(e)` | 26 | Nested Translation Support |
| `ecap_mts(e)` | 25 | Memory Type Support |
| `ecap_coherent(e)` | 0 | Cache Coherency |
| `ecap_qis(e)` | 1 | Queued Invalidation Support |
| `ecap_dev_iotlb_support(e)` | 2 | Device IOTLB Support |
| `ecap_ir_support(e)` | 3 | Interrupt Remapping Support |
| `ecap_eim_support(e)` | 4 | Extended Interrupt Mode (x2APIC) |
| `ecap_pass_through(e)` | 6 | Pass Through Support |
| `ecap_sc_support(e)` | 7 | Snoop Control Support |
| `ecap_max_handle_mask(e)` | 20-23 | Max Invalidation Handle Mask |
| `ecap_iotlb_offset(e)` | 8-17 | IOTLB 寄存器偏移 |

---

## 6. Queued Invalidation 数据结构

### 6.1 `struct qi_desc` — 队列条目

```c
// drivers/iommu/intel/iommu.h:469-474
struct qi_desc {
    u64 qw0;    /* 类型 + 粒度 + DID + SID */
    u64 qw1;    /* 地址 + 粒度 */
    u64 qw2;    /* 保留 / PASID 扩展 */
    u64 qw3;    /* 保留 / 包间类数据 */
};
```

### 6.2 `struct q_inval` — QI 队列

```c
// drivers/iommu/intel/iommu.h:476-483
struct q_inval {
    raw_spinlock_t  q_lock;        /* 队列锁 */
    void            *desc;         /* 循环缓冲区 (物理连续性) */
    int             *desc_status;  /* 描述符完成状态数组 */
    int             free_head;     /* 第一个空闲槽 */
    int             free_tail;     /* 最后一个空闲槽 */
    int             free_cnt;      /* 空闲槽计数 */
};
```

### 6.3 `struct qi_batch` — 批量提交

```c
// drivers/iommu/intel/iommu.h:586-590
#define QI_MAX_BATCHED_DESC_COUNT 16

struct qi_batch {
    struct qi_desc descs[QI_MAX_BATCHED_DESC_COUNT];
    unsigned int index;
};
```

### 6.4 `struct iommu_flush` — IOMMU flush 方法

```c
// drivers/iommu/intel/iommu.h:512-517
struct iommu_flush {
    void (*flush_context)(struct intel_iommu *iommu, u16 did,
                         u16 sid, u8 fm, u64 type);
    void (*flush_iotlb)(struct intel_iommu *iommu, u16 did, u64 addr,
                       unsigned int size_order, u64 type);
};
```

---

## 7. 设备 Probe 全流程

### 7.1 `intel_iommu_probe_device` — 设备探测

```c
// drivers/iommu/intel/iommu.c:3229-3322
static struct iommu_device *intel_iommu_probe_device(struct device *dev)
{
    struct pci_dev *pdev = dev_is_pci(dev) ? to_pci_dev(dev) : NULL;
    struct device_domain_info *info;
    struct intel_iommu *iommu;
    u8 bus, devfn;
    int ret;

    // 1. 找到设备对应的 IOMMU
    iommu = device_lookup_iommu(dev, &bus, &devfn);
    if (!iommu || !iommu->iommu.ops)
        return ERR_PTR(-ENODEV);

    // 2. 分配 device_domain_info
    info = kzalloc_obj(*info);
    ...

    // 3. 设置 segment/bus/devfn
    if (dev_is_real_dma_subdevice(dev)) {
        info->bus = pdev->bus->number;
        info->devfn = pdev->devfn;
        info->segment = pci_domain_nr(pdev->bus);
    } else {
        info->bus = bus;
        info->devfn = devfn;
        info->segment = iommu->segment;
    }

    info->dev = dev;
    info->iommu = iommu;

    // 4. 探测 PCI 设备能力
    if (dev_is_pci(dev)) {
        // ATS Support
        if (ecap_dev_iotlb_support(iommu->ecap) &&
            pci_ats_supported(pdev) &&
            dmar_ats_supported(pdev, iommu)) {
            info->ats_supported = 1;
            info->dtlb_extra_inval = dev_needs_extra_dtlb_flush(pdev);
            if (ecap_dit(iommu->ecap))
                info->pfsid = pci_dev_id(pci_physfn(pdev));
            info->ats_qdep = pci_ats_queue_depth(pdev);
        }
        // PASID Support
        if (sm_supported(iommu)) {
            if (pasid_supported(iommu)) {
                int features = pci_pasid_features(pdev);
                if (features >= 0)
                    info->pasid_supported = features | 1;
            }
            // PRI Support
            if (info->ats_supported && ecap_prs(iommu->ecap) &&
                ecap_pds(iommu->ecap) && pci_pri_supported(pdev))
                info->pri_supported = 1;
        }
    }

    // 5. 将 info 注册到设备
    dev_iommu_priv_set(dev, info);

    // 6. 插入 device_rbtree
    if (pdev && pci_ats_supported(pdev)) {
        pci_prepare_ats(pdev, VTD_PAGE_SHIFT);
        ret = device_rbtree_insert(iommu, info);
    }

    // 7. 为 scalable mode 分配 PASID 表
    if (sm_supported(iommu) && !dev_is_real_dma_subdevice(dev)) {
        ret = intel_pasid_alloc_table(dev);
        ...
        if (!context_copied(iommu, info->bus, info->devfn)) {
            ret = intel_pasid_setup_sm_context(dev);
        }
    }

    intel_iommu_debugfs_create_dev(info);
    return &iommu->iommu;
}
```

### 7.2 `intel_iommu_probe_finalize` — 探测完成

```c
// drivers/iommu/intel/iommu.c:3324-3351
static void intel_iommu_probe_finalize(struct device *dev)
{
    ...
    // PCIe spec: 必须在 ATS 启用前启用 PASID
    if (info->pasid_supported &&
        !pci_enable_pasid(to_pci_dev(dev), info->pasid_supported & ~1))
        info->pasid_enabled = 1;

    // 启用 ATS + 分配 DEVTLB cache tag
    if (sm_supported(iommu) && !dev_is_real_dma_subdevice(dev)) {
        iommu_enable_pci_ats(info);
        if (info->ats_enabled && info->domain) {
            u16 did = domain_id_iommu(info->domain, iommu);
            cache_tag_assign(info->domain, did, dev,
                           IOMMU_NO_PASID, CACHE_TAG_DEVTLB);
        }
    }
    iommu_enable_pci_pri(info);
}
```

---

## 8. IOMMU 操作表 (ops vtable)

### 8.1 `intel_iommu_ops` — 全局 ops

```c
// drivers/iommu/intel/iommu.c:3913-3930
const struct iommu_ops intel_iommu_ops = {
    .blocked_domain         = &blocking_domain,
    .release_domain         = &blocking_domain,
    .identity_domain        = &identity_domain,
    .capable                = intel_iommu_capable,
    .hw_info                = intel_iommu_hw_info,
    .domain_alloc_paging_flags = intel_iommu_domain_alloc_paging_flags,
    .domain_alloc_sva       = intel_svm_domain_alloc,
    .domain_alloc_nested    = intel_iommu_domain_alloc_nested,
    .probe_device           = intel_iommu_probe_device,
    .probe_finalize         = intel_iommu_probe_finalize,
    .release_device         = intel_iommu_release_device,
    .get_resv_regions       = intel_iommu_get_resv_regions,
    .device_group           = intel_iommu_device_group,
    .is_attach_deferred     = intel_iommu_is_attach_deferred,
    .def_domain_type        = device_def_domain_type,
    .page_response          = intel_iommu_page_response,
};
```

### 8.2 `intel_fs_paging_domain_ops` — 第一级页表 ops

```c
// drivers/iommu/intel/iommu.c:3891-3900
const struct iommu_domain_ops intel_fs_paging_domain_ops = {
    IOMMU_PT_DOMAIN_OPS(x86_64),        // 宏展开: .map_pages, .unmap_pages, .iova_to_phys
    .attach_dev = intel_iommu_attach_device,
    .set_dev_pasid = intel_iommu_set_dev_pasid,
    .iotlb_sync_map = intel_iommu_iotlb_sync_map,
    .flush_iotlb_all = intel_flush_iotlb_all,
    .iotlb_sync = intel_iommu_tlb_sync,
    .free = intel_iommu_domain_free,
    .enforce_cache_coherency = intel_iommu_enforce_cache_coherency_fs,
};
```

### 8.3 `intel_ss_paging_domain_ops` — 第二级页表 ops

```c
// drivers/iommu/intel/iommu.c:3902-3911
const struct iommu_domain_ops intel_ss_paging_domain_ops = {
    IOMMU_PT_DOMAIN_OPS(vtdss),         // 宏展开: .map_pages, .unmap_pages, .iova_to_phys (vtdss 格式)
    .attach_dev = intel_iommu_attach_device,
    .set_dev_pasid = intel_iommu_set_dev_pasid,
    .iotlb_sync_map = intel_iommu_iotlb_sync_map,
    .flush_iotlb_all = intel_flush_iotlb_all,
    .iotlb_sync = intel_iommu_tlb_sync,
    .free = intel_iommu_domain_free,
    .enforce_cache_coherency = intel_iommu_enforce_cache_coherency_ss,
};
```

---

## 9. 页表 → Domain → Device 关系图

```
struct intel_iommu (iommu.h:682)
│
├── root_entry[] ──→ Root Table (物理连续)
│   │                   [bus 0] root_entry → context_table_0
│   │                   [bus 1] root_entry → context_table_1
│   │                   ...
│   │
│   └── context_entry per devfn
│       │
│       │  Translation type:
│       │  ├── TT_MULTI_LEVEL → dma_pte[] → 3/5-level PT
│       │  ├── TT_PASS_THROUGH → 1:1 mapping
│       │  └── CONTEXT_PASIDE → pasid_table
│       │
│       ├── device_domain_info.iommu = iommu
│       ├── device_domain_info.domain = dmar_domain
│       └── device_domain_info.bus/devfn = BDF identifier
│
├── device_rbtree → 所有 probed 设备 (by RID)
│
├── qi (q_inval) → IOTLB/devTLB/context cache flush
│
├── ir_table → Interrupt Remapping Table
│
└── prq → Page Request Queue (PRI)
```

---

## 10. Domain 生命周期

```
domain_alloc_paging_flags()
│
├── intel_iommu_domain_alloc_paging_flags()
│   ├── 分配 dmar_domain
│   ├── 选择 fspt (first-stage) 或 sspt (second-stage)
│   └── 初始化 xarray (iommu_array)
│
├── attach_dev()
│   ├── domain_attach_iommu() — 分配 per-IOMMU domain_id
│   ├── 设置 context_entry 的 address root → dma_pte
│   ├── cache_tag_assign() — IOTLB + DEVTLB 缓存标签
│   └── iommu_flush 刷新
│
├── map/unmap pages (IOMMU_PT_DOMAIN_OPS)
│   ├── 遍历/构建 dma_pte 层级
│   └── 标记 iotlb_sync_map = 1 (需要 flush)
│
├── iotlb_sync
│   └── cache_tag_flush_range() — 批量 flush
│
├── detach_dev()
│   └── domain_detach_iommu() + context_clear_entry()
│
└── domain_free()
    ├── 释放 dma_pte 页面
    └── 释放 domain struct
```

---

## 11. 关键行号速查

| 符号 | 文件 | 行号 | 说明 |
|------|------|------|------|
| `struct intel_iommu` | iommu.h | 682 | IOMMU 实例结构体 |
| `struct dmar_domain` | iommu.h | 592 | DMA 地址域 |
| `struct device_domain_info` | iommu.h | 740 | 设备-域绑定 |
| `struct root_entry` | iommu.h | 550 | 根表项 |
| `struct context_entry` | iommu.h | 566 | 上下文表项 |
| `struct dma_pte` | iommu.h | 841 | DMA 页表项 |
| `struct qi_desc` | iommu.h | 469 | QI 描述符 |
| `struct q_inval` | iommu.h | 476 | QI 队列 |
| `struct qi_batch` | iommu.h | 587 | 批量 QI |
| `struct iommu_flush` | iommu.h | 512 | Flush 方法 |
| `struct cache_tag` | iommu.h | 1214 | 缓存标签 |
| `cap_mgaw` | iommu.h | 177 | MGAW 解码 |
| `cap_sagaw` | iommu.h | 178 | SAGAW 解码 |
| `cap_caching_mode` | iommu.h | 179 | Caching mode |
| `cap_ndoms` | iommu.h | 184 | Max domains |
| `ecap_pasid` | iommu.h | 198 | PASID support |
| `ecap_smts` | iommu.h | 195 | Scalable mode |
| `ecap_nest` | iommu.h | 207 | Nested translation |
| `ecap_coherent` | iommu.h | 211 | Cache coherency |
| `ecap_ir_support` | iommu.h | 215 | IR support |
| `ecap_eim_support` | iommu.h | 214 | x2APIC |
| `DMAR_GCMD_REG` | iommu.h | 71 | GCMD 偏移 |
| `DMAR_GSTS_REG` | iommu.h | 72 | GSTS 偏移 |
| `DMAR_RTADDR_REG` | iommu.h | 73 | Root Table 地址 |
| `DMAR_IQH_REG` | iommu.h | 85 | QI 队列头 |
| `DMAR_IQA_REG` | iommu.h | 90 | QI 队列地址 |
| `intel_iommu_probe_device` | iommu.c | 3229 | 设备探测 |
| `intel_iommu_probe_finalize` | iommu.c | 3324 | 探测完成 |
| `intel_iommu_release_device` | iommu.c | 3353 | 设备释放 |
| `intel_fs_paging_domain_ops` | iommu.c | 3891 | 第一级页表 ops |
| `intel_ss_paging_domain_ops` | iommu.c | 3902 | 第二级页表 ops |
| `intel_iommu_ops` | iommu.c | 3913 | 全局 IOMMU ops |
| `to_dmar_domain` | iommu.h | 784 | domain 转换 |
| `domain_id_iommu` | iommu.h | 809 | Domain ID 获取 |
| `agaw_to_level` | iommu.h | 875 | AGAW → level |

---

## 下一篇文章

[第三篇：DMA 重映射 — 页表遍历、TLB 刷新与缓存标签](03-dma-remap.md)
