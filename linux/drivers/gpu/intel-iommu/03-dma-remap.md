# 第三篇：DMA 重映射 — 页表遍历、TLB 刷新与缓存标签

> 源码：`drivers/iommu/intel/iommu.c` `drivers/iommu/intel/cache.c` `drivers/iommu/intel/dmar.c` | 系列目录：[Intel IOMMU 内核源码深度解析](./README.md)

---

## 1. DMA 重映射概述

DMA 重映射是 VT-d 最核心的功能：将设备发出的 DMA 地址（IOVA）转换为主机物理地址（HPA）。内核通过构建多级页表（page table walk）完成此转换。

```
设备 DMA 请求 (Bus:Dev.Fn + IOVA)
        │
        ▼
┌───────────────────────────────────────────────────┐
│  1. Root Table lookup                              │
│     root_entry[bus#] → context table physical addr │
│                                                    │
│  2. Context Table lookup                           │
│     context_entry[devfn] → DMA PT root + domain_id │
│                                                    │
│  3. DMA Page Table Walk (3 或 5 级)               │
│     IOVA[bits] → L2 PTE → L1 PTE → L0 PTE         │
│                                                    │
│  4. Physical Address Construction                  │
│     pte[12:63] | IOVA[11:0] → HPA                 │
│                                                    │
│  5. Access Check                                   │
│     pte.read (bit 0) / pte.write (bit 1)           │
│                                                    │
│  6. Cache Lookup (IOTLB / devTLB)                  │
│     hit → skip walk;  miss → walk → fill cache     │
└───────────────────────────────────────────────────┘
```

---

## 2. DMA PTE 位编码

```c
// drivers/iommu/intel/iommu.h:832-843
/*
 * 0: readable
 * 1: writable
 * 2-6: reserved
 * 7: super page
 * 8-10: available
 * 11: snoop behavior
 * 12-63: Host physical address
 */
struct dma_pte {
    u64 val;
};
```

### 2.1 位字段详解

| 位 | 名称 | 含义 |
|----|------|------|
| 0 | Read (R) | 允许 DMA 读 |
| 1 | Write (W) | 允许 DMA 写 |
| 2-6 | Reserved | 保留，必须为 0 |
| 7 | SP (Super Page) | 1 表示大页（跳过下一级页表） |
| 8-10 | Available | 软件可用位 |
| 11 | Snoop | 1 表示 snoop CPU cache |
| 12-63 | PFN | HPA 页框号 (4KB 对齐) → 实际地址 = PFN << 12 |

### 2.2 PTE 访问宏

```c
// iommu.h:845-853 — 读取物理地址
static inline u64 dma_pte_addr(struct dma_pte *pte)
{
#ifdef CONFIG_64BIT
    return pte->val & VTD_PAGE_MASK;
#else
    /* 32-bit: 必须原子读取 64-bit 值 */
    return  __cmpxchg64(&pte->val, 0ULL, 0ULL) & VTD_PAGE_MASK;
#endif
}

// iommu.h:855-858 — 检查 present (bit 0 或 bit 1 置位)
static inline bool dma_pte_present(struct dma_pte *pte)
{
    return (pte->val & 3) != 0;
}

// iommu.h:860-863 — 检查大页
static inline bool dma_pte_superpage(struct dma_pte *pte)
{
    return (pte->val & DMA_PTE_LARGE_PAGE);
}
```

### 2.3 超级页支持

VT-d 支持在页表中使用超级页（super page），减少页表层级：

```
VT-d Super Page Size:
│
├── 4KB page:   标准页表走到底层 (level 0 PTE)
│
├── 2MB page:   level 1 PTE 设置 bit 7 (SP=1)
│               IOVA 低 21 位 = offset
│               HPA = (level1_PTE.pfn << 12) | (IOVA[20:0])
│
├── 1GB page:   level 2 PTE 设置 bit 7 (SP=1) — 需 cap_fl1gp_support
│               IOVA 低 30 位 = offset
│               HPA = (level2_PTE.pfn << 12) | (IOVA[29:0])
│
└── 512GB page: level 3 PTE 设置 bit 7 (仅 5-level PT 且 cap_fl5lp_support)
```

`cap_super_page_val(c)` (iommu.h:169) 返回一个 bitmap 标记支持的超级页：
- bit 0: 4KB
- bit 1: 2MB
- bit 2: 1GB
- bit 3: 512GB (5-level only)

---

## 3. 3-Level 页表走查

标准 48-bit AGAW 使用 3 级页表，每级 9 位索引（LEVEL_STRIDE=9, iommu.h:870）：

```
IOVA (48-bit 示例：0x0000_0123_4567_8000)
│
│  [47:39] 9 bits  →  L2 index = 0
│  [38:30] 9 bits  →  L1 index = 1
│  [29:21] 9 bits  →  L0 index = 0x123
│  [20:12] 9 bits  →  4KB page offset (IGNORED in PTE walk)
│  [11:0]  12 bits →  byte offset within 4KB page
│
└── PTE levels:

root_entry[bus].lo → context_table_PA
    │
    └── context_entry[devfn]
        │
        ├── domain_id (hi bits)
        ├── address_width (hi bits)
        └── address_space_root → L2 page table PA
            │
            ├── L2 PTE[index=0] — 通过 IOVA[47:39] 索引
            │   ├── present? yes → continue
            │   ├── superpage? no
            │   └── pfn → L1 page table PA
            │       │
            │       ├── L1 PTE[index=1] — 通过 IOVA[38:30] 索引
            │       │   ├── present? yes
            │       │   ├── superpage? no
            │       │   └── pfn → L0 page table PA
            │       │       │
            │       │       └── L0 PTE[index=0x123] — 通过 IOVA[29:21] 索引
            │       │           ├── present? yes
            │       │           ├── read? yes / write? yes
            │       │           ├── snoop? hardware specific
            │       │           └── pfn → HPA frame
            │       │                   │
            │       │                   └── HPA = (pfn << 12) | (IOVA & 0xFFF)
            │       │                       = (pfn << 12) | 0x000
            │       │
            │       └── L1 PTE 超级页 shortcut (SP=1):
            │           HPA = (pfn << 12) | (IOVA[20:12] << 12) | (IOVA & 0xFFF)
            │
            └── L2 PTE 超级页 shortcut (SP=1, 1GB page):
                HPA = (pfn << 12) | (IOVA[29:0])
```

---

## 4. 5-Level 页表 (LA57)

当 `cap_fl5lp_support` (iommu.h:160) 为 1 且 AGAW=57 时，使用 5 级页表：

```c
// iommu.h:57-58
#define ADDR_WIDTH_5LEVEL  (57)
#define ADDR_WIDTH_4LEVEL  (48)
```

```
IOVA (57-bit 示例)
│
│  [56:48] 9 bits → L4 index
│  [47:39] 9 bits → L3 index (= 原来的 L2)
│  [38:30] 9 bits → L2 index (= 原来的 L1)
│  [29:21] 9 bits → L1 index (= 原来的 L0)
│  [20:0]  21 bits → offset
│
└── 5级层次:

L4 PTE → L3 PTE → L2 PTE → L1 PTE → 4KB page
                                    ↑
                              L1 SP → 2MB page
                         ↑
                    L2 SP → 1GB page
                ↑
           L3 SP → 512GB page
      ↑
 L4 SP → 256TB page
```

页表级数计算 (iommu.h:875-878)：

```c
static inline int agaw_to_level(int agaw)
{
    return agaw + 2;
}
// agaw=30 (32-bit): level=32 → 4级 (实际上使用 2 级 DMA PT + root + context)
// agaw=39 (48-bit): level=41 → 5级 (root + context + 3 级 DMA PT)
// agaw=48 (57-bit): level=50 → 6级 (root + context + 4 级 DMA PT)
```

---

## 5. DMA 映射操作

DMA 映射通过 `IOMMU_PT_DOMAIN_OPS(x86_64)` / `IOMMU_PT_DOMAIN_OPS(vtdss)` 宏自动生成 `.map_pages` 和 `.unmap_pages` 回调。核心路径：

### 5.1 Map 流程

```
dma_map_sg()/iommu_map()
│
├── domain->ops->map_pages(domain, iova, phys, pgsize, pgcount, prot, gfp)
│   │
│   ├── IOVA → PTE level 计算
│   │   └── 根据 AGAW 确定起始层级
│   │
│   ├── 逐页表项分配 / 填充
│   │   ├── 检查 PTE present
│   │   │   ├── missing → alloc empty page table
│   │   │   │   └── 设置父级 PTE 指向新表
│   │   │   └── present → 继续
│   │   │
│   │   ├── 如果支持 super page (pgsize >= 2MB/1GB)
│   │   │   └── 设置 PTE.SP=1, 直接写入 PFN
│   │   │
│   │   ├── 设置 PTE.read/write/snoop
│   │   │
│   │   └── clflush_cache_range (如果 ecap_coherent=0)
│   │
│   ├── domain->iotlb_sync_map = 1  (标记需要 flush)
│   │
│   └── 返回成功 / -ENOMEM
```

### 5.2 Unmap 流程

```
dma_unmap_sg()/iommu_unmap()
│
├── domain->ops->unmap_pages(domain, iova, pgsize, pgcount, gather)
│   │
│   ├── IOVA → PTE level 计算
│   ├── 遍历页表找到对应 PTE
│   ├── 清除 PTE (PTE.val = 0)
│   │   └── clflush (如果非 coherent)
│   │
│   ├── 累计 unmapped 大小 (gather)
│   └── 如果是大页，可能释放中间页表
```

---

## 6. IOTLB 刷新

### 6.1 刷新粒度

VT-d IOTLB 支持三种刷新粒度（iommu.h:240-251）：

```c
// iommu.h:240-251
#define DMA_TLB_GLOBAL_FLUSH  (((u64)1) << 60)  // 全局刷新
#define DMA_TLB_DSI_FLUSH     (((u64)2) << 60)  // Domain-selective 刷新
#define DMA_TLB_PSI_FLUSH     (((u64)3) << 60)  // Page-selective 刷新
#define DMA_TLB_DID(id)       (((u64)((id) & 0xffff)) << 32)  // Domain ID
#define DMA_TLB_IVT           (((u64)1) << 63)  // Invalidate 标记
#define DMA_TLB_IH_NONLEAF    (((u64)1) << 6)   // 仅刷新非叶子
#define DMA_TLB_MAX_SIZE      (0x3f)            // 最大 AM 值
```

| 粒度 | QI 类型 | 场景 |
|------|---------|------|
| **Global** | `DMA_TLB_GLOBAL_FLUSH` | IOMMU 重置、初始化 |
| **Domain-selective** | `DMA_TLB_DSI_FLUSH` | 整个 domain 页表变更 |
| **Domain-selective + PASID** | `QI_GRAN_NONG_PASID` | Per-PASID IOTLB 刷新 |
| **Page-selective** | `DMA_TLB_PSI_FLUSH` | 单页 unmap |
| **Page-selective + PASID** | `QI_GRAN_PSI_PASID` | Per-PASID per-page 刷新 |

### 6.2 IOTLB descriptor 构建

```c
// iommu.h:1040-1059 — IOTLB invalidation descriptor
static inline void qi_desc_iotlb(struct intel_iommu *iommu, u16 did, u64 addr,
                                 unsigned int size_order, u64 type,
                                 struct qi_desc *desc)
{
    u8 dw = 0, dr = 0;

    if (cap_write_drain(iommu->cap))
        dw = 1;   // Write drain

    if (cap_read_drain(iommu->cap))
        dr = 1;   // Read drain

    desc->qw0 = QI_IOTLB_DID(did) | QI_IOTLB_DR(dr) | QI_IOTLB_DW(dw)
              | QI_IOTLB_GRAN(type) | QI_IOTLB_TYPE;
    desc->qw1 = QI_IOTLB_ADDR(addr) | QI_IOTLB_IH(ih)
              | QI_IOTLB_AM(size_order);
    desc->qw2 = 0;
    desc->qw3 = 0;
}
```

### 6.3 devTLB descriptor 构建

```c
// iommu.h:1061-1078 — Device TLB invalidation
static inline void qi_desc_dev_iotlb(u16 sid, u16 pfsid, u16 qdep, u64 addr,
                                     unsigned int mask, struct qi_desc *desc)
{
    if (mask) {
        addr |= (1ULL << (VTD_PAGE_SHIFT + mask - 1)) - 1;
        desc->qw1 = QI_DEV_IOTLB_ADDR(addr) | QI_DEV_IOTLB_SIZE;
    } else {
        desc->qw1 = QI_DEV_IOTLB_ADDR(addr);
    }

    if (qdep >= QI_DEV_IOTLB_MAX_INVS)
        qdep = 0;

    desc->qw0 = QI_DEV_IOTLB_SID(sid) | QI_DEV_IOTLB_QDEP(qdep) |
               QI_DIOTLB_TYPE | QI_DEV_IOTLB_PFSID(pfsid);
    desc->qw2 = 0;
    desc->qw3 = 0;
}
```

PFSID (PF Source ID, iommu.h:3271) 在 **ecap_dit**（Device IOTLB Throttling）场景中用于聚合 VF 的 devTLB invalidation 请求到 PF，避免单个 VF 过度消耗 QI 带宽。

### 6.4 PASID-selective IOTLB descriptor

```c
// iommu.h:1081-1100 — PASID-selective IOTLB invalidation
static inline void qi_desc_piotlb_all(u16 did, u32 pasid, struct qi_desc *desc)
{
    desc->qw0 = QI_EIOTLB_PASID(pasid) | QI_EIOTLB_DID(did) |
                QI_EIOTLB_GRAN(QI_GRAN_NONG_PASID) | QI_EIOTLB_TYPE;
    desc->qw1 = 0;
}

static inline void qi_desc_piotlb(u16 did, u32 pasid, u64 addr,
                                  unsigned int size_order, bool ih,
                                  struct qi_desc *desc)
{
    desc->qw0 = QI_EIOTLB_PASID(pasid) | QI_EIOTLB_DID(did) |
                QI_EIOTLB_GRAN(QI_GRAN_PSI_PASID) | QI_EIOTLB_TYPE;
    desc->qw1 = QI_EIOTLB_ADDR(addr) | QI_EIOTLB_IH(ih) |
                QI_EIOTLB_AM(size_order);
}
```

---

## 7. QI 提交机制

### 7.1 `qi_submit_sync` — 同步提交

```c
// drivers/iommu/intel/dmar.c:1368-1487 — qi_submit_sync()
int qi_submit_sync(struct intel_iommu *iommu, struct qi_desc *desc,
                   unsigned int count, unsigned long options)
{
    struct q_inval *qi = iommu->qi;
    struct qi_desc wait_desc;
    int wait_index, index;

    if (!qi)
        return 0;

restart:
    rc = 0;

    // 1. 获取 QI 队列锁
    raw_spin_lock_irqsave(&qi->q_lock, flags);

    // 2. 等待足够空闲槽: count + 1(wait descriptor) + 1(gap)
    while (qi->free_cnt < count + 2) {
        raw_spin_unlock_irqrestore(&qi->q_lock, flags);
        cpu_relax();
        raw_spin_lock_irqsave(&qi->q_lock, flags);
    }

    // 3. 写入描述符到循环缓冲区
    index = qi->free_head;
    wait_index = (index + count) % QI_LENGTH;
    shift = qi_shift(iommu);

    for (i = 0; i < count; i++) {
        offset = ((index + i) % QI_LENGTH) << shift;
        qi->desc[offset]     = desc[i].qw0;
        qi->desc[offset + 8] = desc[i].qw1;
        qi->desc[offset + 16]= desc[i].qw2;
        qi->desc[offset + 24]= desc[i].qw3;
        qi->desc_status[index + i] = QI_IN_FLIGHT;
    }

    // 4. 写入 wait descriptor
    qi->desc[wait_offset] = QI_WAIT_TYPE;
    qi->desc_status[wait_index] = QI_WAIT;

    // 5. 推进 tail 寄存器（通知硬件）
    qi->free_head = (index + count + 1) % QI_LENGTH;
    qi->free_cnt -= count + 1;
    writel(qi->free_head << shift, iommu->reg + DMAR_IQT_REG);

    raw_spin_unlock_irqrestore(&qi->q_lock, flags);

    // 6. 等待硬件完成 (poll wait descriptor status)
    ...
}
```

关键点：
- QI 队列是**循环缓冲区**，硬件通过 head/tail 指针消费
- 每次提交附加一个 **wait descriptor**，用于同步等待完成
- `QI_IN_FLIGHT` / `QI_WAIT` / `QI_DONE` / `QI_ABORT` 四个状态

### 7.2 批量提交

```c
// iommu.h:586-590 — batch 机制
#define QI_MAX_BATCHED_DESC_COUNT 16

struct qi_batch {
    struct qi_desc descs[QI_MAX_BATCHED_DESC_COUNT];
    unsigned int index;
};
```

每个 `dmar_domain` 有一个 `qi_batch`（iommu.h:617），用于积累 deffered invalidation：

```c
// drivers/iommu/intel/cache.c:295 — cache_tag_flush_range 中的批量提交
qi_submit_sync(iommu, batch->descs, batch->index, 0);
```

---

## 8. Cache Tag 系统

### 8.1 架构动机

在多设备共享 Domain、多 PASID 共享 IOMMU 的场景下，重复的 IOTLB invalidation 会成为性能瓶颈。Cache tag 系统（cache.c, 534 行）通过**去重和批量合并**避免冗余刷新。

### 8.2 Cache tag 类型

```c
// drivers/iommu/intel/iommu.h:1207-1212
enum cache_tag_type {
    CACHE_TAG_IOTLB,            // IOMMU 端 IOTLB
    CACHE_TAG_DEVTLB,           // 设备端 TLB
    CACHE_TAG_NESTING_IOTLB,    // 嵌套翻译 IOTLB
    CACHE_TAG_NESTING_DEVTLB,   // 嵌套翻译 devTLB
};
```

### 8.3 `struct cache_tag`

```c
// drivers/iommu/intel/iommu.h:1214-1228
struct cache_tag {
    struct list_head node;
    enum cache_tag_type type;
    struct intel_iommu *iommu;
    /*
     * IOTLB: dev 指向 IOMMU 设备
     * DevTLB: dev 指向 PCIe 端点
     */
    struct device *dev;
    u16 domain_id;
    ioasid_t pasid;
    unsigned int users;   /* 引用计数：多个设备可共享同一 tag */
};
```

### 8.4 `cache_tag_assign` — 标签分配

```c
// drivers/iommu/intel/cache.c:43-90
int cache_tag_assign(struct dmar_domain *domain, u16 did, struct device *dev,
                     ioasid_t pasid, enum cache_tag_type type)
{
    struct device_domain_info *info = dev_iommu_priv_get(dev);
    struct intel_iommu *iommu = info->iommu;
    struct cache_tag *tag, *temp;
    struct list_head *prev;

    tag = kzalloc_obj(*tag);
    ...
    tag->type = type;
    tag->iommu = iommu;
    tag->domain_id = did;
    tag->pasid = pasid;
    tag->users = 1;   // 初始引用计数 = 1

    if (type == CACHE_TAG_DEVTLB || type == CACHE_TAG_NESTING_DEVTLB)
        tag->dev = dev;
    else
        tag->dev = iommu->iommu.dev;

    // 搜索已有 tag: 如果 domain_id+pasi+type 相同则复用
    spin_lock_irqsave(&domain->cache_lock, flags);
    prev = &domain->cache_tags;
    list_for_each_entry(temp, &domain->cache_tags, node) {
        if (cache_tage_match(temp, did, iommu, dev, pasid, type)) {
            temp->users++;       // 引用计数+1, 不分配新 tag
            kfree(tag);
            return 0;            // 重用成功
        }
        if (temp->iommu == iommu)
            prev = &temp->node;  // 同 IOMMU 分组
    }
    // 新 tag 插入同 IOMMU group 末尾
    list_add(&tag->node, prev);
    spin_unlock_irqrestore(&domain->cache_lock, flags);
    return 0;
}
```

**批量策略**：
1. 同一 IOMMU 的所有 cache tag 在链表中**物理相邻**
2. Flush 同一 IOMMU 的所有 tag 时可以只提交**一次** QI 命令（合并同类型）
3. DEVTLB + IOTLB 分别对待：设备的 devTLB 和 IOMMU 的 IOTLB 是两个独立的缓存

### 8.5 批量 flush 流程图

```
cache_tag_flush_range(domain, start, end, ih)
│
├── 遍历 domain->cache_tags
│   │
│   ├── 同一 IOMMU 的所有 CACHE_TAG_IOTLB 合并
│   │   └── qi_desc_iotlb(did, addr, size, GRAN_DSI) × 1
│   │
│   └── 同一设备的所有 CACHE_TAG_DEVTLB 逐个提交
│       └── qi_desc_dev_iotlb(sid, pfsid, qdep, addr, mask) × N
│
├── qi_batch 积累到 QI_MAX_BATCHED_DESC_COUNT (16)
│
└── qi_submit_sync(iommu, batch->descs, batch->index, 0)
    │
    ├── write batch → 硬件 QI queue
    ├── append wait descriptor
    ├── advance tail register
    └── poll wait status → DONE
```

### 8.6 IOTLB/DevTLB 刷新路径

```c
// iommu.c:3891-3900 — 从 domain ops 调用的 TLB 刷新
const struct iommu_domain_ops intel_fs_paging_domain_ops = {
    ...
    .iotlb_sync_map = intel_iommu_iotlb_sync_map,   // map 后的同步刷新
    .flush_iotlb_all = intel_flush_iotlb_all,         // 全部刷新
    .iotlb_sync      = intel_iommu_tlb_sync,          // unmap 后的同步刷新
};
```

| 回调 | 触发场景 | 粒度 |
|------|---------|------|
| `iotlb_sync_map` | DMA map 完成时 | 仅新映射的页范围 |
| `flush_iotlb_all` | domain detach / 销毁 | 整个 domain |
| `iotlb_sync` | DMA unmap 完成后 | 累计的 unmap 范围 |

---

## 9. Context Cache 刷新

除了 IOTLB，VT-d 还有 **context entry cache**：

```c
// iommu.h:253-280 — CCMD (Context Command) 寄存器用法
#define DMA_CCMD_INVL_GRANU_OFFSET  61
#define DMA_CCMD_GLOBAL_INVL   (((u64)1) << 61)
#define DMA_CCMD_DOMAIN_INVL   (((u64)2) << 61)
#define DMA_CCMD_DEVICE_INVL   (((u64)3) << 61)
```

Context cache 刷新在以下场景必须执行：
- 修改 context_entry 后（present/translation type/domain ID 变更）
- 设备从 domain 分离时
- IOMMU 初始化设置 root table 时

---

## 10. 完整 DMA Remapping 事务示例

```
场景：NVMe 驱动对 SSD 发出 DMA 读请求
dma_addr = 0x1000_0000 (IOVA), size = 4KB

CPU 侧操作：
│
├── dma_map_single(dev, cpu_addr, size, DMA_FROM_DEVICE)
│   │
│   ├── 获取 dev→iommu→domain
│   ├── IOVA 分配: dma_addr = 0x1000_0000 (从 domain IOVA 空间)
│   ├── 构建 PTE:
│   │   pte.val = phys_addr & VTD_PAGE_MASK  // HPA
│   │           | BIT(0)                      // read enable
│   │
│   ├── clflush(pte)                          // 如果是 non-coherent
│   ├── iotlb_sync_map(iommu, domain, addr)
│   │   └── qi_desc_iotlb(DID, addr, 0, DSI)
│   └── 返回 dma_addr

设备侧操作：
│
├── NVMe 发起 DMA read @ IOVA = 0x1000_0000
│   │
│   ├── PCIe TLP: MRd { RequesterID=BDF, Address=0x1000_0000 }
│   │
│   ├── IOMMU 截获:
│   │   ├── root_entry[bus] → context_entry[devfn]
│   │   ├── domain_id = context.hi.domain_id
│   │   ├── IOTLB lookup: (domain_id, IOVA) → HPA?
│   │   │   ├── hit → 直接使用
│   │   │   └── miss → page table walk:
│   │   │       L2 PTE → L1 PTE → L0 PTE → HPA
│   │   │       → 填充 IOTLB
│   │   │
│   │   ├── 权限检查: pte.read = 1 ✓
│   │   ├── 转换为 HPA → MRd @ HPA
│   │   └── 转发到内存控制器
│   │
│   └── 内存控制器返回数据 → IOMMU → PCIe completion → NVMe

DMA 完成后：
│
├── dma_unmap_single(dev, dma_addr, size, DMA_FROM_DEVICE)
│   │
│   ├── PTE.val = 0                            // 清除
│   ├── iotlb_sync → 累计到 gather
│   │   └── qi_desc_iotlb(DID, addr, 0, PSI) // 单页刷新
│   └── 释放 IOVA
```

---

## 11. 关键行号速查

| 符号 | 文件 | 行号 | 说明 |
|------|------|------|------|
| `struct dma_pte` | iommu.h | 841 | DMA 页表项 |
| `dma_pte_addr` | iommu.h | 845 | 读取物理地址 |
| `dma_pte_present` | iommu.h | 855 | 检查 PT entry 存在 |
| `dma_pte_superpage` | iommu.h | 860 | 检查大页标志 |
| `DMA_TLB_GLOBAL_FLUSH` | iommu.h | 241 | 全局刷新 |
| `DMA_TLB_DSI_FLUSH` | iommu.h | 242 | 域选择性刷新 |
| `DMA_TLB_PSI_FLUSH` | iommu.h | 243 | 页选择性刷新 |
| `qi_desc_iotlb` | iommu.h | 1040 | IOTLB 描述符构建 |
| `qi_desc_dev_iotlb` | iommu.h | 1061 | devTLB 描述符构建 |
| `qi_desc_piotlb` | iommu.h | 1089 | PASID IOTLB 描述符 |
| `qi_desc_piotlb_all` | iommu.h | 1081 | PASID 全刷新 |
| `qi_submit_sync` | dmar.c | 1368 | QI 同步提交 |
| `cache_tag_assign` | cache.c | 43 | 缓存标签分配 |
| `cache_tage_match` | cache.c | 23 | 标签匹配（去重） |
| `cache_tag_unassign` | cache.c | 93 | 标签释放 |
| `cache_tag_flush_range` | cache.c | — | 范围刷新 |
| `cache_tag_flush_all` | cache.c | — | 全刷新 |
| `intel_fs_paging_domain_ops` | iommu.c | 3891 | 第一级 domain ops |
| `intel_ss_paging_domain_ops` | iommu.c | 3902 | 第二级 domain ops |
| `intel_iommu_iotlb_sync` | iommu.c | — | IOTLB 同步 |
| `intel_flush_iotlb_all` | iommu.c | — | IOTLB 全刷新 |

---

## 下一篇文章

[第四篇：中断重映射与 PASID — DMA 之外的能力](04-irq-pasid.md)
