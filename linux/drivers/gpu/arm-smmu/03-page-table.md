# 第三篇：页表引擎 — LPAE 格式、Stage 1/2 与域终结

> 源码：`io-pgtable-arm.c` (1267 行) | `arm-smmu-v3.c` (`domain_finalise`, `map/unmap ops`)

系列目录：[ARM SMMU 内核源码深度解析](./README.md)

---

## 前言

SMMUv3 的页表引擎基于 ARM LPAE (Large Physical Address Extension) 格式实现，与 ARM CPU 的 VMSA 页表格式高度同源。`io-pgtable-arm.c` 是一个 CPU 无关的通用页表分配器（1267 行），同时服务于 SMMUv1/v2/v3 三个世代的驱动。本章从 PTE 位定义出发，追踪完整的页表构造、遍历和域终结流程。

## 1. io-pgtable-arm.c 架构概览

### 1.1 组成定位

```
io-pgtable-arm.c (1267 行)
├── 页表格式定义 (行 10-144)
│   ├── ARM_LPAE_MAX_ADDR_BITS = 52
│   ├── ARM_LPAE_MAX_LEVELS = 4
│   └── PTE bit 定义 (TYPE, AF, DBM, XN, nG, ...)
├── 核心数据结构和 helper (行 145-200)
│   ├── struct arm_lpae_io_pgtable
│   ├── arm_lpae_iopte (typedef u64)
│   └── iopte_leaf / iopte_table helpers
├── 页表分配 (行 201-380)
│   ├── __arm_lpae_alloc_pages: 单页/多页分配
│   ├── __arm_lpae_free_pages:  单页/多页释放
│   └── arm_lpae_install_table: 安装中间级表
├── 页表遍历与映射 (行 381-730)
│   ├── __arm_lpae_map: 递归安装映射
│   ├── __arm_lpae_unmap: 递归解除映射
│   └── arm_lpae_iova_to_phys: 逆遍历查找 PA
├── 页表格式配置 (行 731-1000)
│   ├── arm_64_lpae_alloc_pgtable_s1: S1 配置
│   ├── arm_64_lpae_alloc_pgtable_s2: S2 配置
│   └── arm_32_lpae_alloc_pgtable_s1/s2: 兼容 v7
└── io_pgtable_init 注册 (行 1001-1267)
    └── ARM_64_LPAE_S1 / ARM_64_LPAE_S2 / ARM_MALI_LPAE 格式注册
```

### 1.2 核心数据结构 (`io-pgtable-arm.c:157`)

```c
// io-pgtable-arm.c:157-166
struct arm_lpae_io_pgtable {
    struct io_pgtable    iop;         // 通用 io_pgtable 基类

    int                  pgd_bits;    // PGD 索引位宽
    int                  start_level; // 起始遍历 level (0=PGD, 1=PUD, ...)
    int                  bits_per_level; // 每级页表的索引位宽

    void                 *pgd;        // PGD 表指针 (CPU VA)
};

typedef u64 arm_lpae_iopte;           // 每个 PTE 为 64 位
```

**关键宏定义** (`io-pgtable-arm.c:25-65`)：

```c
#define ARM_LPAE_MAX_ADDR_BITS    52          // 最大 52 位 VA (LPA2)
#define ARM_LPAE_S2_MAX_CONCAT_PAGES 16       // S2 最多 16 页连续 (初始化页表)
#define ARM_LPAE_MAX_LEVELS       4           // 最多 4 级页表

// 计算 level l 对应虚拟地址的右移量
#define ARM_LPAE_LVL_SHIFT(l,d)                     \
    (((ARM_LPAE_MAX_LEVELS - (l)) * (d)->bits_per_level) + \
     ilog2(sizeof(arm_lpae_iopte)))

// 页表颗粒度 (每级页表覆盖的地址范围)
#define ARM_LPAE_GRANULE(d)                         \
    (sizeof(arm_lpae_iopte) << (d)->bits_per_level)

// 每级页表的 PTE 条目数
#define ARM_LPAE_PTES_PER_TABLE(d)                  \
    (ARM_LPAE_GRANULE(d) >> ilog2(sizeof(arm_lpae_iopte)))

// level l 和虚拟地址 a 下的索引
#define ARM_LPAE_LVL_IDX(a,l,d)                     \
    (((u64)(a) >> ARM_LPAE_LVL_SHIFT(l,d)) &        \
     ((1 << ((d)->bits_per_level + ARM_LPAE_PGD_IDX(l,d))) - 1))

// level l 对应 block 映射的大小
#define ARM_LPAE_BLOCK_SIZE(l,d)  (1ULL << ARM_LPAE_LVL_SHIFT(l,d))
```

**bits_per_level 与页大小的关系**：

| 页大小 | bits_per_level | 每表条目数 | 颗粒度 (表大小) |
|--------|---------------|-----------|-----------------|
| 4KB    | 9             | 512       | 4KB             |
| 16KB   | 11            | 2048      | 16KB            |
| 64KB   | 13            | 8192      | 64KB            |

对于标准 4KB 页 + 48 位 VA：
- Level 0 (PGD): `ARM_LPAE_MAX_LEVELS - 1 = 3` 个 9-bit chunk + (pgd_bits - bits_per_level) 额外 bits
- Level 1 (PUD): 9 bits
- Level 2 (PMD): 9 bits
- Level 3 (PTE): 9 bits

## 2. PTE 位定义

### 2.1 基本 PTE 位 (`io-pgtable-arm.c:67-87`)

```c
// io-pgtable-arm.c:67-87
#define ARM_LPAE_PTE_TYPE_SHIFT    0
#define ARM_LPAE_PTE_TYPE_MASK     0x3
#define ARM_LPAE_PTE_TYPE_BLOCK    1       // 块映射 (非叶子级)
#define ARM_LPAE_PTE_TYPE_TABLE    3       // 页表引用 (中间级) ★
#define ARM_LPAE_PTE_TYPE_PAGE     3       // 页映射 (叶子级=最后级) ★

#define ARM_LPAE_PTE_ADDR_MASK     GENMASK_ULL(47,12)

#define ARM_LPAE_PTE_NSTABLE       (((arm_lpae_iopte)1) << 63) // 非安全表
#define ARM_LPAE_PTE_XN            (((arm_lpae_iopte)3) << 53)
#define ARM_LPAE_PTE_DBM           (((arm_lpae_iopte)1) << 51) // 硬件脏位
#define ARM_LPAE_PTE_AF            (((arm_lpae_iopte)1) << 10) // 访问标志
#define ARM_LPAE_PTE_SH_NS         (((arm_lpae_iopte)0) << 8)  // 非共享
#define ARM_LPAE_PTE_SH_OS         (((arm_lpae_iopte)2) << 8)  // 外部共享
#define ARM_LPAE_PTE_SH_IS         (((arm_lpae_iopte)3) << 8)  // 内部共享
#define ARM_LPAE_PTE_NS            (((arm_lpae_iopte)1) << 5)  // 非安全
#define ARM_LPAE_PTE_VALID         (((arm_lpae_iopte)1) << 0)

// 软件同步位 (解决 cache coherency race)
#define ARM_LPAE_PTE_SW_SYNC       (((arm_lpae_iopte)1) << 55)
```

**PTE TYPE 编码**：

| TYPE bits | 语义 | 说明 |
|-----------|------|------|
| 00 | 无效 | PTE 未被使用 |
| 01 | Block | 中间级 block 映射 |
| 11 | Table/Page | 指向下级页表 (中间级) 或页帧 (最后级) |

```c
static inline bool iopte_leaf(arm_lpae_iopte pte, int lvl,
                              enum io_pgtable_fmt fmt)
{
    if (lvl == (ARM_LPAE_MAX_LEVELS - 1) && fmt != ARM_MALI_LPAE)
        return iopte_type(pte) == ARM_LPAE_PTE_TYPE_PAGE;
    return iopte_type(pte) == ARM_LPAE_PTE_TYPE_BLOCK;
}
```

### 2.2 Stage-1 特有 PTE 位 (`io-pgtable-arm.c:90-97`)

```c
#define ARM_LPAE_PTE_AP_UNPRIV  (((arm_lpae_iopte)1) << 6)  // 非特权访问
#define ARM_LPAE_PTE_AP_RDONLY  (((arm_lpae_iopte)1) << 7)  // 只读
#define ARM_LPAE_PTE_nG         (((arm_lpae_iopte)1) << 11) // 非全局 (进程级)
#define ARM_LPAE_PTE_ATTRINDX_SHIFT  2
```

**AP (Access Permission) 编码**：

| AP[2:1] | 含义 | 说明 |
|---------|------|------|
| 00 | EL1: RW, EL0: 无访问 | |
| 01 | EL1: RW, EL0: RW | 全权限 |
| 10 | EL1: RO, EL0: 无访问 | 内核只读 |
| 11 | EL1: RO, EL0: RO | 全只读 |

**DBM (Dirty Bit Management)** 组合 (`io-pgtable-arm.c:151`):

```c
#define ARM_LPAE_PTE_AP_WR_CLEAN_MASK (ARM_LPAE_PTE_AP_RDONLY | \
                                       ARM_LPAE_PTE_DBM)
// AP[2]=1 (read-only) + DBM=1 → 硬件将 AP[2] 当作"脏"
// 即: PTE 初始时 AP[2]=1, 硬件写入时清除 AP[2]
static inline bool iopte_writeable_dirty(pte)
{
    return ((pte) & ARM_LPAE_PTE_AP_WR_CLEAN_MASK) == ARM_LPAE_PTE_DBM;
}
```

### 2.3 Stage-2 特有 PTE 位 (`io-pgtable-arm.c:100-117`)

```c
#define ARM_LPAE_PTE_HAP_FAULT  (((arm_lpae_iopte)0) << 6)
#define ARM_LPAE_PTE_HAP_READ   (((arm_lpae_iopte)1) << 6)
#define ARM_LPAE_PTE_HAP_WRITE  (((arm_lpae_iopte)2) << 6)

// 内存属性编码 (S2)
#define ARM_LPAE_PTE_MEMATTR_FWB_WB  (((arm_lpae_iopte)0x6) << 2)
#define ARM_LPAE_PTE_MEMATTR_OIWB    (((arm_lpae_iopte)0xf) << 2)
#define ARM_LPAE_PTE_MEMATTR_NC      (((arm_lpae_iopte)0x5) << 2)
#define ARM_LPAE_PTE_MEMATTR_DEV     (((arm_lpae_iopte)0x1) << 2)
```

S2 的内存属性机制与 S1 不同：S2 可以超控 S1 的内存类型（通过 `ARM_SMMU_FEAT_S2FWB`），确保虚拟机无法将设备内存重映射为 cacheable。

### 2.4 MAIR 编码 (`io-pgtable-arm.c:127-136`)

```c
#define ARM_LPAE_MAIR_ATTR_SHIFT(n)   ((n) << 3)
#define ARM_LPAE_MAIR_ATTR_MASK       0xff
#define ARM_LPAE_MAIR_ATTR_DEVICE     0x04     // nGnRE
#define ARM_LPAE_MAIR_ATTR_NC         0x44     // Non-cacheable
#define ARM_LPAE_MAIR_ATTR_INC_OWBRWA 0xf4     // Inner cacheable, Outer WB-RWA
#define ARM_LPAE_MAIR_ATTR_WBRWA      0xff     // Inner WB-RWA, Outer WB-RWA
#define ARM_LPAE_MAIR_ATTR_IDX_NC     0
#define ARM_LPAE_MAIR_ATTR_IDX_CACHE  1
#define ARM_LPAE_MAIR_ATTR_IDX_DEV    2
```

PTE 中的 AttrIndx[2:0] 字段索引到这 8 个预定义的 MAIR 条目，分别编码内/外部缓存属性。

## 3. LPAE 页表遍历

### 3.1 4 级页表遍历图

```
虚拟地址 (48-bit VA, 4KB 页):
┌──────┬──────┬──────┬──────┬───────────┐
│ 47:39│ 38:30│ 29:21│ 20:12│   11:0    │
│ PGD  │ PUD  │ PMD  │ PTE  │ 偏移量     │
│ 9bit │ 9bit │ 9bit │ 9bit │  12bit     │
└──────┴──────┴──────┴──────┴───────────┘

               ┌──────────────────────────┐
               │  TTB0 (页表基址寄存器)     │
               └────────────┬─────────────┘
                            │
    ┌───────────────────────▼───────────────────────┐
    │  Level 0 (PGD) — 512 entries, 512GB each      │
    │  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐   │
    │  │ 0 │ 1 │ 2 │...│n  │   │   │   │   │511│   │
    │  └───┴───┴───┴───┴─┬─┴───┴───┴───┴───┴───┘   │
    │                     │ PGD[n] = PUD 物理地址     │
    └─────────────────────┼─────────────────────────┘
                          │
    ┌─────────────────────▼─────────────────────────┐
    │  Level 1 (PUD) — 512 entries, 1GB each        │
    │  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐   │
    │  │ 0 │ 1 │ 2 │...│m  │   │   │   │   │511│   │
    │  └───┴───┴───┴───┴─┬─┴───┴───┴───┴───┴───┘   │
    │                     │ PUD[m] = PMD 物理地址     │
    └─────────────────────┼─────────────────────────┘
                          │
    ┌─────────────────────▼─────────────────────────┐
    │  Level 2 (PMD) — 512 entries, 2MB each        │
    │  ┌───────┬───────┬───────┬───────┬───────┐    │
    │  │  B0   │  B1   │ ...   │  Bk   │  B511 │    │
    │  │ 2MB   │ 2MB   │       │ 2MB   │ 2MB   │    │
    │  │ block │ block │       │block  │ block │    │
    │  └───────┴───────┴───────┴───┬───┴───────┘    │
    │                              │                 │
    │              PMD[k] ────────▶│ PTE 表物理地址    │
    └──────────────────────────────┼─────────────────┘
                                   │
    ┌──────────────────────────────▼─────────────────┐
    │  Level 3 (PTE) — 512 entries, 4KB each         │
    │  ┌───────┬───────┬───────┬───────┬───────┐     │
    │  │ P0    │ P1    │ ...   │ Pp    │ P511  │     │
    │  │ 4KB   │ 4KB   │       │ 4KB   │ 4KB   │     │
    │  │ page  │ page  │       │ page  │ page  │     │
    │  └───────┴───────┴───────┴───────┴───────┘     │
    │              PTE[p] → 物理页帧号                  │
    └─────────────────────────────────────────────────┘

中间级可选 block 映射:
  Level 1 PUD block: 1GB 直接映射 (无需 Level 2, 3)
  Level 2 PMD block: 2MB 直接映射 (无需 Level 3)
```

### 3.2 __arm_lpae_map — 递归页表映射 (`io-pgtable-arm.c:422`)

```c
// io-pgtable-arm.c:422 — 核心映射递归函数
static int __arm_lpae_map(struct arm_lpae_io_pgtable *data,
                          unsigned long iova, phys_addr_t paddr,
                          size_t size, size_t pgcount,
                          arm_lpae_iopte prot, int lvl,
                          arm_lpae_iopte *ptep, gfp_t gfp,
                          size_t *mapped)
{
    arm_lpae_iopte *cptep, pte;
    size_t block_size = ARM_LPAE_BLOCK_SIZE(lvl, data);
    size_t tblsz = ARM_LPAE_GRANULE(data);
    struct io_pgtable_cfg *cfg = &data->iop.cfg;

    // 如果当前 level 可以包含整个映射范围，安装 block/page 映射
    if (size >= block_size && !(cfg->quirks & IO_PGTABLE_QUIRK_NO_BLOCK_OPS)) {
        ...
        __arm_lpae_sync_pte(ptep, pte, cfg);  // 写 PTE
        *mapped += min(block_size, size);
        return 0;
    }

    // 需要更深层级 → 检查/安装下级表
    pte = READ_ONCE(*ptep);
    if (!iopte_valid(pte)) {
        cptep = __arm_lpae_alloc_pages(tblsz, gfp, cfg);
        ...
        __arm_lpae_init_pte(data, iova, virt_to_phys(cptep),
                            ARM_LPAE_PTE_TYPE_TABLE, lvl, cfg, ptep);
    } else {
        cptep = iopte_deref(pte, data);  // 解引用到 CPU VA
    }

    // 递归映射到更深层级
    return __arm_lpae_map(data, iova, paddr, size, pgcount,
                          prot, lvl + 1, cptep, gfp, mapped);
}
```

**关键分解**：

1. **Block 映射**: 当 `size >= block_size` 且无 `BLOCK_OPS` quirk 时，在当前 level 直接安装 block PT
2. **Table 分配**: 如果下一级页表不存在，`__arm_lpae_alloc_pages` 分配并初始化新的页表
3. **递归**: 调用自身继续到 `lvl + 1` 更深层级
4. **同步**: `__arm_lpae_sync_pte` 设置 `SW_SYNC` 位 + DSB + 清除 `SW_SYNC` 位，确保硬件可见

### 3.3 __arm_lpae_unmap — 递归页表解除映射

unmap 操作是 map 的逆过程，同样递归遍历页表：

1. 定位目标 level 的 block/page 映射
2. 清除 PTE（写零）
3. 检查是否可以回收空闲的页表页（当所有 PTE 为空时，释放该表并清除上级 PTE 的指针）
4. `__arm_lpae_sync_pte` 确保硬件 TLB 失效后再回收

### 3.4 arm_lpae_iova_to_phys — 逆遍历 (`io-pgtable-arm.c:734`)

```c
// io-pgtable-arm.c:734 — 将 IOVA 逆查找为 PA
static phys_addr_t arm_lpae_iova_to_phys(struct io_pgtable_ops *ops,
                                         unsigned long iova)
{
    struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
    arm_lpae_iopte pte;
    int lvl = data->start_level;  // 起始 level

    do {
        // 计算当前 level 的 PTE 指针
        ptep = iopte_deref(pte, data) + ARM_LPAE_LVL_IDX(iova, lvl, data);
        pte = READ_ONCE(*ptep);

        // block 映射 → 直接计算 PA:
        //    PA = PTE_addr + (iova & (block_size - 1))
        if (iopte_leaf(pte, lvl, data->iop.fmt)) {
            ...
            return iopte_to_paddr(pte, data) + (iova & (block_size - 1));
        }

        // table → 继续下一级遍历
        ...
    } while (++lvl < ARM_LPAE_MAX_LEVELS);

    return 0;  // 未映射
}
```

## 4. Stage-1 / Stage-2 配置差异

### 4.1 arm_smmu_domain_finalise (`arm-smmu-v3.c:2928`)

这是 SMMUv3 域初始化的关键函数，将 IOMMU 域与 LPAE 页表引擎连接：

```c
// arm-smmu-v3.c:2928-2993
static int arm_smmu_domain_finalise(struct arm_smmu_domain *smmu_domain,
                                    struct arm_smmu_device *smmu, u32 flags)
{
    int ret;
    enum io_pgtable_fmt fmt;
    struct io_pgtable_cfg pgtbl_cfg;
    struct io_pgtable_ops *pgtbl_ops;
    bool enable_dirty = flags & IOMMU_HWPT_ALLOC_DIRTY_TRACKING;

    pgtbl_cfg = (struct io_pgtable_cfg) {
        .pgsize_bitmap  = smmu->pgsize_bitmap,
        .coherent_walk  = smmu->features & ARM_SMMU_FEAT_COHERENCY,
        .tlb            = &arm_smmu_flush_ops,
        .iommu_dev      = smmu->dev,
    };

    switch (smmu_domain->stage) {
    case ARM_SMMU_DOMAIN_S1: {
        // VA 大小: 有 VAX → 52 bit, 否则 48 bit
        unsigned long ias = (smmu->features &
                            ARM_SMMU_FEAT_VAX) ? 52 : 48;
        pgtbl_cfg.ias = min_t(unsigned long, ias, VA_BITS);
        pgtbl_cfg.oas = smmu->oas;            // PA 大小

        if (enable_dirty)
            pgtbl_cfg.quirks |= IO_PGTABLE_QUIRK_ARM_HD;

        fmt = ARM_64_LPAE_S1;
        finalise_stage_fn = arm_smmu_domain_finalise_s1;
        break;
    }
    case ARM_SMMU_DOMAIN_S2:
        if (enable_dirty)
            return -EOPNOTSUPP;   // S2 不支持 dirty tracking
        pgtbl_cfg.ias = smmu->oas;    // S2: IAS = OAS (IPA 大小 = PA 大小)
        pgtbl_cfg.oas = smmu->oas;
        fmt = ARM_64_LPAE_S2;
        finalise_stage_fn = arm_smmu_domain_finalise_s2;

        // S2FWB: 强制 write-back (嵌套场景)
        if ((smmu->features & ARM_SMMU_FEAT_S2FWB) &&
            (flags & IOMMU_HWPT_ALLOC_NEST_PARENT))
            pgtbl_cfg.quirks |= IO_PGTABLE_QUIRK_ARM_S2FWB;
        break;
    }

    // 分配 LPAE 页表操作接口
    pgtbl_ops = alloc_io_pgtable_ops(fmt, &pgtbl_cfg, smmu_domain);
    ...
    smmu_domain->domain.pgsize_bitmap = pgtbl_cfg.pgsize_bitmap;
    smmu_domain->domain.geometry.aperture_end = (1UL << pgtbl_cfg.ias) - 1;
    smmu_domain->domain.geometry.force_aperture = true;

    // Stage 特定资源分配 (ASID/VMID)
    ret = finalise_stage_fn(smmu, smmu_domain);
    ...
    smmu_domain->pgtbl_ops = pgtbl_ops;
    smmu_domain->smmu = smmu;
    return 0;
}
```

### 4.2 Stage-1 Finalise (`arm-smmu-v3.c:2896`)

```c
// arm-smmu-v3.c:2896-2910
static int arm_smmu_domain_finalise_s1(struct arm_smmu_device *smmu,
                                       struct arm_smmu_domain *smmu_domain)
{
    u32 asid = 0;
    struct arm_smmu_ctx_desc *cd = &smmu_domain->cd;

    mutex_lock(&arm_smmu_asid_lock);
    ret = xa_alloc(&arm_smmu_asid_xa, &asid, smmu_domain,
                   XA_LIMIT(1, (1 << smmu->asid_bits) - 1), GFP_KERNEL);
    cd->asid = (u16)asid;
    mutex_unlock(&arm_smmu_asid_lock);
    return ret;
}
```

ASID 从全局 XArray (`arm_smmu_asid_xa`, `arm-smmu-v3.c:79`) 中分配，范围 [1, 2^asid_bits - 1]。

### 4.3 Stage-2 Finalise (`arm-smmu-v3.c:2912`)

```c
// arm-smmu-v3.c:2912-2926
static int arm_smmu_domain_finalise_s2(struct arm_smmu_device *smmu,
                                       struct arm_smmu_domain *smmu_domain)
{
    int vmid;
    struct arm_smmu_s2_cfg *cfg = &smmu_domain->s2_cfg;

    // VMID 0 预留给 Stage-2 bypass STE
    vmid = ida_alloc_range(&smmu->vmid_map, 1,
                           (1 << smmu->vmid_bits) - 1, GFP_KERNEL);
    ...
    cfg->vmid = (u16)vmid;
    return 0;
}
```

VMID 从 IDA (`smmu->vmid_map`, `arm-smmu-v3.c:881`) 中分配，VMID=0 保留给 bypass。

### 4.4 S1 vs S2 页表格式对比

| 属性 | Stage-1 (ARM_64_LPAE_S1) | Stage-2 (ARM_64_LPAE_S2) |
|------|-------------------------|-------------------------|
| 输入地址类型 | VA (虚拟地址) | IPA (中间物理地址) |
| 输出地址类型 | PA 或 IPA | PA (物理地址) |
| 输入地址大小 (IAS) | 48-bit (或 52-bit VAX) | OAS (32-52 bit) |
| 输出地址大小 (OAS) | 硬件 OAS (32-52 bit) | 硬件 OAS |
| 权限控制 | AP[2:1] (EL0/EL1 权限) | HAP[1:0] (读/写/执行) |
| 内存属性 | MAIR 索引 | S2 内存类型编码 |
| ASID/VMID | ASID (最多 16 位) | VMID (最多 16 位) |
| 脏位 (Dirty Bit) | DBM (bit 51) | 不支持 |
| 全局/进程级 | nG bit | 无 (S2 是全局的) |
| 执行权限 | XN, PXN | S2XN |
| Contiguous bit | bit 52 | bit 52 |
| 块映射粒度 | 1GB / 2MB | 1GB / 2MB |

## 5. Quirks 机制

### 5.1 IO_PGTABLE_QUIRK_ARM_HD (Hardware Dirty)

当 `ARM_SMMU_FEAT_HD` 和 `ARM_SMMU_FEAT_COHERENCY` 都满足时（`arm_smmu_v3.c:2802` `arm_smmu_dbm_capable`），启用硬件脏位管理：

```c
// io-pgtable-arm.c: 启用 DBM 的 PTE 编码
// PTE 初始: AP[2]=1, DBM=1 → 硬件"读"但不可"写"
// 硬件写入时自动清除 AP[2] → 表示页面已脏
// 软件可通过读取 AP[2] 确认页面是否脏 (无需软件写保护)
```

### 5.2 IO_PGTABLE_QUIRK_ARM_S2FWB (Stage-2 Force Write-Back)

启用后，S2 页表可以将所有 S1 映射的内存属性超控为强制写回：

```c
// 仅在 IOMMU_HWPT_ALLOC_NEST_PARENT 时启用
pgtbl_cfg.quirks |= IO_PGTABLE_QUIRK_ARM_S2FWB;
```

### 5.3 IO_PGTABLE_QUIRK_NO_BLOCK_OPS

禁用 block 映射，强制所有映射使用最小页粒度。用于某些不支持大页的 SMMU 实现。

## 6. Map/Unmap 操作到 io-pgtable 的委托链

### 6.1 arm_smmu_map_pages (`arm-smmu-v3.c:4025`)

```c
// arm-smmu-v3.c:4025 — IOMMU API .map_pages 回调
static int arm_smmu_map_pages(struct iommu_domain *domain,
                              unsigned long iova, phys_addr_t paddr,
                              size_t pgsize, size_t pgcount, int prot,
                              gfp_t gfp, size_t *mapped)
{
    struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
    struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
    ...
    return ops->map_pages(ops, iova, paddr, pgsize, pgcount,
                          prot, gfp, mapped);
    // ↓ 最终调用 io-pgtable-arm.c 的 arm_lpae_map_pages
}
```

### 6.2 完整调用链

```
用户态 / 内核 VFIO
   │
   ▼
iommu_map() / iommu_map_sg()
   │
   ▼
__iommu_map() → domain->ops->map_pages()
   │
   ▼
arm_smmu_map_pages()          ← arm-smmu-v3.c:4025
   │
   ▼
ops->map_pages()              ← 指针指向 io-pgtable-arm.c
   │
   ▼
arm_lpae_map_pages()          ← io-pgtable-arm.c
   │
   ▼
__arm_lpae_map()              ← 递归页表安装 + 同步
   │
   └── __arm_lpae_sync_pte()  ← 设置 SW_SYNC + DSB + 清 SW_SYNC
```

## 7. IOMMU 域完整生命周期

```
                    ┌───────────────────────┐
                    │   arm_smmu_domain_alloc │ arm-smmu-v3.c:2852
                    │   → 分配 smmu_domain     │
                    │   → 分配空 invs 数组     │
                    └───────────┬─────────────┘
                                │
                    ┌───────────▼─────────────┐
                    │ arm_smmu_domain_finalise  │ arm-smmu-v3.c:2928
                    │   → 确定 S1/S2            │
                    │   → alloc_io_pgtable_ops │ → io-pgtable-arm.c
                    │   → 分配 ASID 或 VMID    │
                    └───────────┬─────────────┘
                                │
                    ┌───────────▼─────────────┐
                    │   arm_smmu_attach_dev     │ arm-smmu-v3.c:3656
                    │   → 写 STE 到流表          │
                    │   → 写 CD 到 CD 表         │
                    │   → 构建 invs 数组        │
                    └───────────┬─────────────┘
                                │
                    ┌───────────▼─────────────┐
                    │   map_pages / unmap_pages  │
                    │   → pgtbl_ops → LPAE     │
                    │   → TLB 无效化 (CMDQ)     │
                    └───────────┬─────────────┘
                                │
                    ┌───────────▼─────────────┐
                    │ arm_smmu_domain_free_paging│ arm-smmu-v3.c:2874
                    │   → free_io_pgtable_ops  │ → 释放页表树
                    │   → 释放 ASID/VMID       │
                    └─────────────────────────┘
```

## 8. 与 CPU 页表的对比

```
          CPU VMSA 页表                  SMMU LPAE 页表
          ──────────────                 ──────────────
          TTBR0_EL1 / TTBR1_EL1         STE.S1CTXPTR → CD.TTB0
          TCR_EL1                        CD.TCR
          MAIR_EL1                       CD.MAIR
          ASID (TTBR0_EL1 upper)         CD.ASID

                          共同点:
          ──────────────────────────────────────────
          • 同为 4 级 / 3 级 (取决于 VA 大小)
          • 同用 ARM_LPAE PTE 格式
          • 同为 64 位 PTE
          • 同有 AF (Access Flag)、DBM (Dirty Bit)
          • 同有 block/page 映射

                          差异点:
          ──────────────────────────────────────────
          • CPU 支持 TTBR0/TTBR1 split → SMMU 仅用 CD.TTB0
          • CPU 支持 ASID → SMMU CD 中含 ASID
          • CPU 支持 PAN/POE/GCS → SMMU 不支持
          • SMMU 仅读 CD 不加锁 → hitsless update 协议
          • SMMU 支持 S2 页表 (IPA→PA) → CPU VHE 仅有 S1
          • 页表分配器: CPU 用伙伴分配器 → SMMU 用 io-pgtable-arm.c
```

## 下一篇文章

[第四篇：SVA 与 IOMMUFD — 共享虚拟地址与嵌套翻译](./04-sva-iommufd.md)
