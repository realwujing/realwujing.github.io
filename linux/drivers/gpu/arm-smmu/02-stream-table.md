# 第二篇：流表、命令队列与事件队列 — SMMUv3 的三大核心数据结构

> 源码：`arm-smmu-v3.h` (STE/CD/Queue structs) | `arm-smmu-v3.c` (queue init, event handling)

系列目录：[ARM SMMU 内核源码深度解析](./README.md)

---

## 前言

SMMUv3 的三大核心内存数据结构——流表（Stream Table）、命令队列（Command Queue）和事件队列（Event Queue）构成了 SMMU 的运行时骨架。流表将 StreamID 映射到翻译上下文，命令队列承载所有软件到硬件的控制指令，事件队列上报硬件检测到的故障和异常。本章逐字段解析这些结构的硬件格式和软件管理代码。

## 1. Stream Table Entry (STE) (`arm-smmu-v3.h:219`)

### 1.1 硬件格式

STE 是 64 字节（8 个 u64）的硬件定义结构，SMMU 通过 StreamID 索引流表找到对应设备的所有翻译配置：

```c
// arm-smmu-v3.h:219-221
struct arm_smmu_ste {
    __le64 data[STRTAB_STE_DWORDS];   // 8 × u64 = 64 字节
};
```

静态断言验证大小（`arm-smmu-v3.c:53`）：

```c
static_assert(sizeof(struct arm_smmu_ste) == NUM_ENTRY_QWORDS * sizeof(u64));
```

**STE 字段布局**：

```
STE Word 0 (data[0]):                    STE Word 1 (data[1]):
┌───────────────────────────────┐        ┌───────────────────────────────┐
│63:59 S1CDMAX (max CD index)   │        │63:46  Reserved                 │
│51:6  S1ContextPtr             │        │45:44  SHCFG (shareability)     │
│ 5:4  S1FMT (CD table format)  │        │31:30  STRW (streamworld)       │
│ 3:1  CFG (STE config)         │        │29:28  EATS (ATS control)       │
│  0   V (Valid)                │        │  27   S1STALLD (stall disable) │
└───────────────────────────────┘        │  25   S2FWB (force WB)         │
                                         │  19   MEV (merge events)       │
STE Word 2 (data[2]):                    │ 7:6   S1CSH (inner share)      │
┌───────────────────────────────┐        │ 5:4   S1COR (outer cache)      │
│  58  S2S (stage 2 secure)     │        │ 3:2   S1CIR (inner cache)      │
│  57  S2R (stage 2 read alloc) │        │ 1:0   S1DSS (default substream)│
│  54  S2PTW (page table walk)  │        └───────────────────────────────┘
│  52  S2ENDI (endianness)      │
│  51  S2AA64 (AArch64)         │        STE Word 3 (data[3]):
│50:32  VTCR (S2 TCR fields)    │        ┌───────────────────────────────┐
│15:0   S2VMID                   │        │51:4  S2TTB (S2 页表基地址)    │
└───────────────────────────────┘        │ 3:0  Reserved                  │
                                         └───────────────────────────────┘
STE Words 4-7 (data[4..7]):
┌───────────────────────────────────────┐
│ Reserved / IMPLEMENTATION DEFINED     │
└───────────────────────────────────────┘
```

### 1.2 CFG 字段 — STE 的翻译模式 (`arm-smmu-v3.h:244-249`)

```c
#define STRTAB_STE_0_V         (1UL << 0)
#define STRTAB_STE_0_CFG       GENMASK_ULL(3, 1)
#define STRTAB_STE_0_CFG_ABORT     0    // 中止所有事务
#define STRTAB_STE_0_CFG_BYPASS    4    // 直通，不做翻译
#define STRTAB_STE_0_CFG_S1_TRANS  5    // Stage-1 翻译
#define STRTAB_STE_0_CFG_S2_TRANS  6    // Stage-2 翻译
#define STRTAB_STE_0_CFG_NESTED    7    // 嵌套翻译 (S1 + S2)
```

**CFG 字段的核心意义**：

每个 STE 的 CFG 字段决定了该 StreamID 对应的翻译模式。CFG 的选取在 `arm_smmu_make_s2_domain_ste` (`arm-smmu-v3.h:1005`)、`arm_smmu_make_cdtable_ste` (`arm-smmu-v3.h:1019`) 等函数中完成：

| CFG 值 | 语义 | 使用场景 |
|--------|------|----------|
| 0 (Abort) | 所有事务返回 abort | 设备未 attach 时的默认 STE |
| 4 (Bypass) | 1:1 直通，IOVA=PA | RMR (RAS Error Record) 所需 |
| 5 (S1_TRANS) | Stage-1: VA→IPA | 标准 DMA 映射 iommu domain |
| 6 (S2_TRANS) | Stage-2: IPA→PA | 虚拟机设备直通 |
| 7 (NESTED) | 嵌套: S1+S2 | iommufd 用户态驱动 |

### 1.3 S1FMT — Stage-1 CD 表格式 (`arm-smmu-v3.h:251-255`)

```c
#define STRTAB_STE_0_S1FMT         GENMASK_ULL(5, 4)
#define STRTAB_STE_0_S1FMT_LINEAR  0        // 线性 CD 表
#define STRTAB_STE_0_S1FMT_64K_L2  2        // 二级 CD 表 (每表 1024 个 CD)
#define STRTAB_STE_0_S1CTXPTR_MASK GENMASK_ULL(51, 6)
#define STRTAB_STE_0_S1CDMAX       GENMASK_ULL(63, 59)
```

- **S1CTXPTR**：指向 CD 表的基物理地址（51:6 位对齐）
- **S1CDMAX**：log2(最大 CD 条目数)，用于硬件边界检查
- **S1FMT_LINEAR**：线性表，驱动分配 `CTXDESC_LINEAR_CDMAX`（`arm-smmu-v3.h:378`）个 CD
- **S1FMT_64K_L2**：二级表，L1 为 1024 个 `struct arm_smmu_cdtab_l1` 条目，每个 L1 条目指向一个包含 1024 个 CD 的 L2 表

### 1.4 S1DSS — 默认 Substream 行为 (`arm-smmu-v3.h:257-260`)

```c
#define STRTAB_STE_1_S1DSS           GENMASK_ULL(1, 0)
#define STRTAB_STE_1_S1DSS_TERMINATE 0x0  // 无 PASID 时 abort
#define STRTAB_STE_1_S1DSS_BYPASS    0x1  // 无 PASID 时 bypass
#define STRTAB_STE_1_S1DSS_SSID0     0x2  // 无 PASID 时使用 SSID=0 的 CD
```

在 `arm_smmu_make_cdtable_ste` 中，通过 `s1dss` 参数设置此字段，支持使用和不使用 PASID 的混合设备环境。

### 1.5 EATS — 设备侧 ATS 控制 (`arm-smmu-v3.h:274-277`)

```c
#define STRTAB_STE_1_EATS          GENMASK_ULL(29, 28)
#define STRTAB_STE_1_EATS_ABT      0UL    // 禁止 ATS 请求
#define STRTAB_STE_1_EATS_TRANS    1UL    // 允许 ATS 翻译请求
#define STRTAB_STE_1_EATS_S1CHK    2UL    // 允许 ATS + S1 权限检查
```

### 1.6 Stage-2 相关字段

```c
// arm-smmu-v3.h:286-301
#define STRTAB_STE_2_S2VMID     GENMASK_ULL(15, 0)     // Stage-2 VMID
#define STRTAB_STE_2_VTCR       GENMASK_ULL(50, 32)
#define STRTAB_STE_2_VTCR_S2T0SZ GENMASK_ULL(5, 0)     // T0SZ
#define STRTAB_STE_2_VTCR_S2SL0  GENMASK_ULL(7, 6)     // 起始 level
#define STRTAB_STE_2_VTCR_S2PS   GENMASK_ULL(18, 16)   // 物理地址大小
#define STRTAB_STE_2_S2AA64      (1UL << 51)            // AArch64 页表
#define STRTAB_STE_2_S2PTW       (1UL << 54)            // 硬件 PTW
#define STRTAB_STE_2_S2S         (1UL << 57)            // S2 secure
#define STRTAB_STE_2_S2R         (1UL << 58)            // S2 read allocate
#define STRTAB_STE_3_S2TTB_MASK  GENMASK_ULL(51, 4)     // S2 页表基地址
```

### 1.7 流表数据结构 (`arm-smmu-v3.h:224-231`)

流表本身可以是线性或二级结构：

```c
// Stream Table 二级结构
#define STRTAB_NUM_L2_STES  (1 << STRTAB_SPLIT)     // 256 (STRTAB_SPLIT=8)

struct arm_smmu_strtab_l2 {
    struct arm_smmu_ste stes[STRTAB_NUM_L2_STES];   // 256 个 STE
};

struct arm_smmu_strtab_l1 {
    __le64 l2ptr;                                    // 指向 L2 表的物理地址
};
#define STRTAB_MAX_L1_ENTRIES  (1 << 17)             // 最大 131072 L1 条目

// SID 到 L1/L2 索引的转换
static inline u32 arm_smmu_strtab_l1_idx(u32 sid)
{
    return sid / STRTAB_NUM_L2_STES;    // 高 SID bits
}
static inline u32 arm_smmu_strtab_l2_idx(u32 sid)
{
    return sid % STRTAB_NUM_L2_STES;    // 低 8 bits
}
```

### 1.8 STE 写入流程 (`arm-smmu-v3.c:1868`)

```c
// arm-smmu-v3.c:1868
static void arm_smmu_write_ste(struct arm_smmu_master *master, u32 sid,
                               struct arm_smmu_ste *ste)
{
    ...
    arm_smmu_write_entry(&writer, arm_smmu_get_step_for_sid(smmu, sid), ste);
}
```

`arm_smmu_entry_writer`（`arm-smmu-v3.h:992`）使用 hitsless update 算法，在不停止硬件的情况下安全更新 STE/CD（详见 `arm_smmu_write_entry`，`arm-smmu-v3.h:1014`）。

## 2. Context Descriptor (CD) (`arm-smmu-v3.h:326`)

### 2.1 硬件格式

CD 是 64 字节的硬件定义结构，每个 PASID (SSID) 对应一个 CD，描述该地址空间的页表基址和翻译参数：

```c
// arm-smmu-v3.h:326-328
struct arm_smmu_cd {
    __le64 data[CTXDESC_CD_DWORDS];  // 8 × u64 = 64 字节
};
```

**CD 字段布局**：

```
CD Word 0 (data[0]):                          CD Word 1 (data[1]):
┌─────────────────────────────────────┐       ┌──────────────────────────────────┐
│63:48 ASID                            │       │63:52  Reserved                    │
│  47  ASET (ASID ETM)                 │       │51:4   TTB0 (页表基址)             │
│  46  A (Access flag)                 │       │ 3:0   Reserved                    │
│  45  R (remote)                      │       └──────────────────────────────────┘
│  44  S (stall enable)                │
│  43  HA (hardware access flag)       │       CD Word 2 (data[2]):
│  42  HD (hardware dirty)             │       ┌──────────────────────────────────┐
│  41  AA64 (AArch64)                  │       │ Reserved                         │
│  38  TBI0 (top byte ignore)          │       └──────────────────────────────────┘
│34:32 IPS (IPA size)                  │
│  31  V (Valid)                       │       CD Word 3 (data[3]):
│  30  EPD1 (disable TTBR1 walks)      │       ┌──────────────────────────────────┐
│  15  ENDI (endianness)               │       │63:0  MAIR (内存属性间接寄存器)     │
│  14  EPD0 (disable TTBR0 walks)      │       └──────────────────────────────────┘
│13:12 SH0 (shareability)              │
│11:10 ORGN0 (outer cacheability)      │       CD Words 4-7 (data[4..7]):
│ 9:8  IRGN0 (inner cacheability)      │       ┌──────────────────────────────────┐
│ 7:6  TG0 (granule size)              │       │ Reserved / 用户态自定义            │
│ 5:0  T0SZ (TCR T0SZ)                 │       └──────────────────────────────────┘
└─────────────────────────────────────┘
```

### 2.2 关键字段解析

```c
// arm-smmu-v3.h:348-372
#define CTXDESC_CD_0_TCR_T0SZ   GENMASK_ULL(5, 0)    // Size offset: 64 - T0SZ
#define CTXDESC_CD_0_TCR_TG0    GENMASK_ULL(7, 6)    // 0=4K, 1=64K, 2=16K
#define CTXDESC_CD_0_TCR_IRGN0  GENMASK_ULL(9, 8)    // Inner cacheability
#define CTXDESC_CD_0_TCR_ORGN0  GENMASK_ULL(11, 10)  // Outer cacheability
#define CTXDESC_CD_0_TCR_SH0    GENMASK_ULL(13, 12)  // Shareability
#define CTXDESC_CD_0_TCR_EPD0   (1ULL << 14)         // Disable TTBR0
#define CTXDESC_CD_0_TCR_EPD1   (1ULL << 30)         // Disable TTBR1

#define CTXDESC_CD_0_TCR_IPS    GENMASK_ULL(34, 32)  // IPA 大小

#define CTXDESC_CD_0_AA64        (1UL << 41)          // AArch64 页表格式
#define CTXDESC_CD_0_S           (1UL << 44)          // Stall 使能
#define CTXDESC_CD_0_R           (1UL << 45)          // Remote
#define CTXDESC_CD_0_A           (1UL << 46)          // Access flag
#define CTXDESC_CD_0_ASET        (1UL << 47)          // ASID ETM
#define CTXDESC_CD_0_ASID        GENMASK_ULL(63, 48)  // 16-bit ASID

#define CTXDESC_CD_0_TCR_HA      (1UL << 43)          // H/W access flag (HTTU)
#define CTXDESC_CD_0_TCR_HD      (1UL << 42)          // H/W dirty (HTTU)

#define CTXDESC_CD_1_TTB0_MASK   GENMASK_ULL(51, 4)   // TTBR0 物理地址
```

**TTB0 和 SMMU PTW**：CD 的 TTB0 字段指向 S1 页表的根（PGD）。SMMU 硬件页表遍历器（PTW）会在 TLB 未命中时自动从 TTB0 开始页表遍历。

**CD 与 CPU 页表寄存器对比**：

| CD 字段 | CPU 寄存器 | 描述 |
|---------|-----------|------|
| TTB0 | TTBR0_EL1 | 页表基址 |
| T0SZ | TCR_EL1.T0SZ | VA 大小 |
| TG0 | TCR_EL1.TG0 | 颗粒大小 |
| IRGN0/ORGN0/SH0 | TCR_EL1 | 缓存属性 |
| MAIR (data[3]) | MAIR_EL1 | 内存属性间接寄存器 |
| ASID | TTBR0_EL1.ASID | 地址空间标识符 |
| IPS | TCR_EL1.IPS | 中间物理地址大小 |

### 2.3 CD 表结构

```c
// arm-smmu-v3.h:319-346
#define CTXDESC_L2_ENTRIES      1024

struct arm_smmu_cdtab_l2 {
    struct arm_smmu_cd cds[CTXDESC_L2_ENTRIES];    // 1024 个 CD
};

struct arm_smmu_cdtab_l1 {
    __le64 l2ptr;                                   // 指向 L2 表
};

static inline unsigned int arm_smmu_cdtab_l1_idx(unsigned int ssid)
{
    return ssid / CTXDESC_L2_ENTRIES;   // 高 bits
}
static inline unsigned int arm_smmu_cdtab_l2_idx(unsigned int ssid)
{
    return ssid % CTXDESC_L2_ENTRIES;   // 低 10 bits
}
```

### 2.4 STE ↔ CD ↔ 页表关系图

```
StreamID (由 PCIe BDF 派生)
   │
   ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Stream Table                               │
│  ┌──────────────────────────────────────┐                       │
│  │ STE[SID]                              │                      │
│  │  V=1, CFG=S1_TRANS                    │                      │
│  │  S1FMT=0x2 (64K_L2)                   │                      │
│  │  S1CTXPTR ─────────────────────────┐  │                      │
│  │  S1CDMAX=10 (max 1024 CDs)         │  │                      │
│  │  S1DSS=SSID0 (default to SSID=0)   │  │                      │
│  │  EATS=0x1 (ATS Trans)              │  │                      │
│  └────────────────────────────────────┘  │                      │
└──────────────────────────────────────────┼──────────────────────┘
                                           │
                                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                     Context Descriptor Table                      │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ CD[SSID=0]  ← 默认 PASID (PASID=0 时使用)                    │ │
│  │   TTB0 ──────────────────────────────┐                      │ │
│  │   ASID=5                             │                      │ │
│  │   TCR: T0SZ=16, TG0=4K, SH0=IS      │                      │ │
│  │   MAIR: 0x000000FF440C0400           │                      │ │
│  │   V=1, AA64=1, A=1, S=0             │                      │ │
│  └──────────────────────────────────────┼──────────────────────┘ │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ CD[SSID=7]  ← 辅助 PASID                                    │ │
│  │   TTB0 ────────────────┐                                    │ │
│  │   ASID=12              │                                    │ │
│  │   ...                  │                                    │ │
│  └────────────────────────┼────────────────────────────────────┘ │
└───────────────────────────┼──────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────────┐
│                     Stage-1 Page Table (LPAE)                     │
│                                                                  │
│  Level 0 (PGD)  ───  512 entries, 512GB each                    │
│       │                                                          │
│  Level 1 (PUD)  ───  512 entries, 1GB each                      │
│       │                                                          │
│  Level 2 (PMD)  ───  512 entries, 2MB each                      │
│       │                                                          │
│  Level 3 (PTE)  ───  512 entries, 4KB each → Output PA           │
└──────────────────────────────────────────────────────────────────┘
```

## 3. 命令队列 (Command Queue) (`arm-smmu-v3.h:631`)

### 3.1 核心数据结构

```c
// arm-smmu-v3.h:594-622
struct arm_smmu_ll_queue {
    union {
        u64         val;
        struct {
            u32     prod;       // 生产者索引
            u32     cons;       // 消费者索引
        };
        struct {
            atomic_t prod;      // 原子生产者
            atomic_t cons;      // 原子消费者
        } atomic;
        u8      __pad[SMP_CACHE_BYTES];
    } ____cacheline_aligned_in_smp;
    u32     max_n_shift;        // 队列大小的 log2
};

struct arm_smmu_queue {
    struct arm_smmu_ll_queue llq;
    int     irq;                // 有线中断号 (EVTQ/PRIQ)
    __le64  *base;              // 队列环基地址 (CPU VA)
    dma_addr_t base_dma;        // 队列环基地址 (DMA)
    u64     q_base;             // 写入硬件寄存器的队列基址
    size_t  ent_dwords;         // 每条目 dword 数
    u32 __iomem *prod_reg;      // 硬件 prod 寄存器地址
    u32 __iomem *cons_reg;      // 硬件 cons 寄存器地址
};

struct arm_smmu_cmdq {
    struct arm_smmu_queue   q;          // 基础队列结构
    atomic_long_t           *valid_map; // Consume 有效性位图
    atomic_t                owner_prod; // 当前 owner 的 prod
    atomic_t                lock;       // 混合型 rwlock
    bool                    (*supports_cmd)(struct arm_smmu_cmdq_ent *ent);
};
```

### 3.2 命令编码 (`arm-smmu-v3.h:516-592`)

`struct arm_smmu_cmdq_ent` 是驱动内部的命令抽象，通过 `arm_smmu_cmdq_build_cmd` (`arm-smmu-v3.c:271`) 转换为硬件 64 字节命令：

```c
// arm-smmu-v3.h:516-592
struct arm_smmu_cmdq_ent {
    u8      opcode;
    bool    substream_valid;
    union {
        #define CMDQ_OP_PREFETCH_CFG   0x1     // 预取配置
        struct { u32 sid; } prefetch;

        #define CMDQ_OP_CFGI_STE       0x3     // 无效化特定 STE
        #define CMDQ_OP_CFGI_ALL       0x4     // 无效化所有 STE 缓存
        #define CMDQ_OP_CFGI_CD        0x5     // 无效化特定 CD
        #define CMDQ_OP_CFGI_CD_ALL    0x6     // 无效化所有 CD 缓存
        struct { u32 sid; u32 ssid; union { bool leaf; u8 span; }; } cfgi;

        #define CMDQ_OP_TLBI_NH_ALL    0x10    // 无效化所有非安全 TLB
        #define CMDQ_OP_TLBI_NH_ASID   0x11    // 无效化特定 ASID 的 TLB
        #define CMDQ_OP_TLBI_NH_VA     0x12    // 无效化特定 VA 的 TLB
        #define CMDQ_OP_TLBI_NH_VAA    0x13    // 无效化特定 VA (忽略 ASID)
        #define CMDQ_OP_TLBI_EL2_ALL   0x20    // EL2 TLB 无效化
        #define CMDQ_OP_TLBI_EL2_ASID  0x21
        #define CMDQ_OP_TLBI_EL2_VA    0x22
        #define CMDQ_OP_TLBI_S12_VMALL 0x28    // Stage-1+2 VM 全部 TLB
        #define CMDQ_OP_TLBI_S2_IPA    0x2a    // Stage-2 特定 IPA TLB
        #define CMDQ_OP_TLBI_NSNH_ALL  0x30    // 非安全非超管全部
        struct { u8 num; u8 scale; u16 asid; u16 vmid; bool leaf;
                 u8 ttl; u8 tg; u64 addr; } tlbi;

        #define CMDQ_OP_ATC_INV        0x40    // ATC 无效化
        struct { u32 sid; u32 ssid; u64 addr; u8 size;
                 bool global; } atc;

        #define CMDQ_OP_PRI_RESP       0x41    // PRI 响应
        struct { u32 sid; u32 ssid; u16 grpid;
                 enum pri_resp resp; } pri;

        #define CMDQ_OP_RESUME         0x44    // 恢复 stalled 事务
        struct { u32 sid; u16 stag; u8 resp; } resume;

        #define CMDQ_OP_CMD_SYNC       0x46    // 命令同步屏障
        struct { u64 msiaddr; } sync;
    };
};
```

### 3.3 命令队列的并发控制 (`arm-smmu-v3.c:550`)

CMDQ 在所有 CPU 间共享，驱动实现了一种富有创意的并发控制算法：

```
共享 CMDQ 插入算法的核心步骤：

1. 分配队列空间 (owner 机制)
   ┌────────────────────────────────────┐
   │ arm_smmu_cmdq_shared_lock          │
   │  ├─ atomic_fetch_inc_relaxed ↑     │ ← 共享锁 (允许多个 CPU 并发写入)
   │  └─ EXCLUSIVE: INT_MIN             │
   └────────────────────────────────────┘
       │
       ▼
2. 写入命令
   ┌────────────────────────────────────┐
   │ arm_smmu_cmdq_write_entries        │
   │  将命令写入队列 slot               │
   └────────────────────────────────────┘
       │
       ▼
3. 标记有效性
   ┌────────────────────────────────────┐
   │ arm_smmu_cmdq_set_valid_map        │
   │  在 valid_map 位图中标记 slot 有效  │
   └────────────────────────────────────┘
       │
       ▼
4a. 如果是 OWNER: 推进 PROD
   ┌────────────────────────────────────┐
   │ arm_smmu_cmdq_poll_valid_map       │
   │  等待自己范围内的所有 slot 变为有效  │
   │ writel(prod, PROD_REG)             │
   └────────────────────────────────────┘

4b. 如果是 CMD_SYNC: 等待完成
   ┌────────────────────────────────────┐
   │ arm_smmu_cmdq_poll_until_sync      │
   │  ├─ MSI polling: cmd[0]==0 表示完成 │
   │  └─ Busy polling: cons > sync_idx   │
   └────────────────────────────────────┘
```

锁的类型：
- **Shared lock** (`arm_smmu_cmdq_shared_lock`, `arm-smmu-v3.c:490`): 普通命令插入者持有的读锁
- **Exclusive lock** (`arm_smmu_cmdq_exclusive_trylock_irqsave`, `arm-smmu-v3.c:528`): 错误处理时的独占锁

### 3.4 命令同步 (CMD_SYNC)

CMD_SYNC 是命令队列中的同步屏障。SMMUv3 通过 MSI 或轮询两种方式检测 CMD_SYNC 完成：

```c
// arm-smmu-v3.c:393-413
static void arm_smmu_cmdq_build_sync_cmd(u64 *cmd, struct arm_smmu_device *smmu,
                                         struct arm_smmu_cmdq *cmdq, u32 prod)
{
    struct arm_smmu_cmdq_ent ent = { .opcode = CMDQ_OP_CMD_SYNC };
    if (smmu->options & ARM_SMMU_OPT_MSIPOLL) {
        // MSI polling: SMMU 完成 sync 后会写 0 到 msiaddr
        ent.sync.msiaddr = q->base_dma + Q_IDX(&q->llq, prod) *
                           q->ent_dwords * 8;
    }
    arm_smmu_cmdq_build_cmd(cmd, &ent);
}
```

### 3.5 批处理优化 (`arm-smmu-v3.c:951`)

```c
// arm-smmu-v3.c:951-990
static void arm_smmu_cmdq_batch_init(...);
static void arm_smmu_cmdq_batch_add(...);   // 批量添加命令
static int arm_smmu_cmdq_batch_submit(...); // 提交批次

// 典型使用: 一次性提交多个 TLBI 命令
struct arm_smmu_cmdq_batch batch;
arm_smmu_cmdq_batch_init(smmu, &batch);
arm_smmu_cmdq_batch_add(smmu, &batch, &ent_tlbi_1);
arm_smmu_cmdq_batch_add(smmu, &batch, &ent_tlbi_2);
arm_smmu_cmdq_batch_add(smmu, &batch, &ent_sync);
arm_smmu_cmdq_batch_submit(smmu, &batch);
```

批处理减少了对 CMDQ 锁的获取次数和 WRITE_ONCE 的 barrier 开销。

### 3.6 无效化类型 (`arm-smmu-v3.h:655-675`)

```c
enum arm_smmu_inv_type {
    INV_TYPE_S1_ASID,             // ASID-based S1 invalidation
    INV_TYPE_S2_VMID,             // VMID-based S2 invalidation
    INV_TYPE_S2_VMID_S1_CLEAR,    // S2 VMID + clear S1 for this VMID
    INV_TYPE_ATS,                 // ATC invalidation
    INV_TYPE_ATS_FULL,            // Full ATC invalidation
};

struct arm_smmu_inv {
    struct arm_smmu_device *smmu;
    u8  type;
    u8  size_opcode;              // Range inv opcode
    u8  nsize_opcode;             // Non-range inv opcode
    u32 id;                       // ASID or VMID or SID
    union {
        size_t pgsize;            // Range inv page size
        u32 ssid;                 // ATS ssid
    };
    int users;                    // Refcount, 0 = trash
};
```

## 4. 事件队列 (Event Queue) (`arm-smmu-v3.h:738`)

### 4.1 数据结构

```c
// arm-smmu-v3.h:738-742
struct arm_smmu_evtq {
    struct arm_smmu_queue   q;          // 基础队列
    struct iopf_queue       *iopf;      // I/O Page Fault 队列 (SVA)
    u32                     max_stalls; // 最大同时 stall 数
};
```

### 4.2 事件结构体 (`arm-smmu-v3.h:906-924`)

```c
struct arm_smmu_event {
    u8  stall : 1,                    // 事务被 stalled
        ssv : 1,                      // SubstreamID 有效
        privileged : 1,               // 特权访问
        instruction : 1,              // 指令访问
        s2 : 1,                       // Stage-2 故障
        read : 1,                     // 读事务
        ttrnw : 1,                    // TT 读写 (仅 CLASS=TT 时)
        class_tt : 1;                 // CLASS=TT
    u8  id;                           // 事件 ID
    u8  class;                        // 事件类别
    u16 stag;                         // Stall tag
    u32 sid;                          // StreamID
    u32 ssid;                         // SubstreamID
    u64 iova;                         // 故障 IOVA
    u64 ipa;                          // 故障 IPA
    u64 fetch_addr;                   // 表遍历的物理地址
    struct device *dev;               // 对应的设备
};
```

### 4.3 事件 ID 定义 (`arm-smmu-v3.c:88-100`)

```c
static const char * const event_str[] = {
    [EVT_ID_BAD_STREAMID_CONFIG]   = "C_BAD_STREAMID",
    [EVT_ID_STE_FETCH_FAULT]       = "F_STE_FETCH",
    [EVT_ID_BAD_STE_CONFIG]        = "C_BAD_STE",
    [EVT_ID_STREAM_DISABLED_FAULT] = "F_STREAM_DISABLED",
    [EVT_ID_BAD_SUBSTREAMID_CONFIG]= "C_BAD_SUBSTREAMID",
    [EVT_ID_CD_FETCH_FAULT]        = "F_CD_FETCH",
    [EVT_ID_BAD_CD_CONFIG]         = "C_BAD_CD",
    [EVT_ID_TRANSLATION_FAULT]     = "F_TRANSLATION",       // TLB miss, no mapping
    [EVT_ID_ADDR_SIZE_FAULT]       = "F_ADDR_SIZE",          // Address out of range
    [EVT_ID_ACCESS_FAULT]          = "F_ACCESS",             // Access flag fault
    [EVT_ID_PERMISSION_FAULT]      = "F_PERMISSION",         // Permission fault
    [EVT_ID_VMS_FETCH_FAULT]       = "F_VMS_FETCH",
    ...
};
```

命名约定：
- **C_** = Configuration error (配置错误)
- **F_** = Fault (运行时故障)

### 4.4 事件解码与处理 (`arm-smmu-v3.c:2103-2136`)

```c
// arm-smmu-v3.c:2103-2134
static void arm_smmu_decode_event(struct arm_smmu_device *smmu, u64 *raw,
                                  struct arm_smmu_event *event)
{
    event->id   = FIELD_GET(EVTQ_0_ID, raw[0]);
    event->sid  = FIELD_GET(EVTQ_0_SID, raw[0]);
    event->ssv  = FIELD_GET(EVTQ_0_SSV, raw[0]);
    event->ssid = event->ssv ? FIELD_GET(EVTQ_0_SSID, raw[0]) : IOMMU_NO_PASID;
    event->privileged = FIELD_GET(EVTQ_1_PnU, raw[1]);
    event->instruction= FIELD_GET(EVTQ_1_InD, raw[1]);
    event->s2    = FIELD_GET(EVTQ_1_S2, raw[1]);
    event->read  = FIELD_GET(EVTQ_1_RnW, raw[1]);
    event->stag  = FIELD_GET(EVTQ_1_STAG, raw[1]);
    event->stall = FIELD_GET(EVTQ_1_STALL, raw[1]);
    event->class = FIELD_GET(EVTQ_1_CLASS, raw[1]);
    event->iova  = FIELD_GET(EVTQ_2_ADDR, raw[2]);
    event->ipa   = raw[3] & EVTQ_3_IPA;
    ...
    // 通过红黑树查找 master 获取设备指针
    mutex_lock(&smmu->streams_mutex);
    master = arm_smmu_find_master(smmu, event->sid);
    if (master)
        event->dev = get_device(master->dev);
    mutex_unlock(&smmu->streams_mutex);
}
```

### 4.5 handle_event 分发逻辑 (`arm-smmu-v3.c:2136-2201`)

```c
// arm-smmu-v3.c:2136-2201
static int arm_smmu_handle_event(struct arm_smmu_device *smmu, u64 *evt,
                                 struct arm_smmu_event *event)
{
    // 只处理可恢复的事件
    switch (event->id) {
    case EVT_ID_BAD_STE_CONFIG:
    case EVT_ID_STREAM_DISABLED_FAULT:
    case EVT_ID_BAD_SUBSTREAMID_CONFIG:
    case EVT_ID_BAD_CD_CONFIG:
    case EVT_ID_TRANSLATION_FAULT:
    case EVT_ID_ADDR_SIZE_FAULT:
    case EVT_ID_ACCESS_FAULT:
    case EVT_ID_PERMISSION_FAULT:
        break;
    default:
        return -EOPNOTSUPP;
    }
    // 如果是 stall 事件，构造 IOMMU_FAULT_PAGE_REQ 并上报
    if (event->stall) {
        flt->type = IOMMU_FAULT_PAGE_REQ;
        flt->prm = (struct iommu_fault_page_request){
            .grpid = event->stag,
            .addr  = event->iova,
            ...
        };
    }
    // 路由：stall → iommu_report_device_fault
    //      vmaster → arm_vmaster_report_event
    if (event->stall)
        ret = iommu_report_device_fault(master->dev, &fault_evt);
    else if (master->vmaster && !event->s2)
        ret = arm_vmaster_report_event(master->vmaster, evt);
    ...
}
```

### 4.6 EVTQ 中断线程 (`arm-smmu-v3.c:2262`)

```c
// arm-smmu-v3.c:2262-2293
static irqreturn_t arm_smmu_evtq_thread(int irq, void *dev)
{
    u64 evt[EVTQ_ENT_DWORDS];
    struct arm_smmu_event event = {0};
    struct arm_smmu_device *smmu = dev;
    struct arm_smmu_queue *q = &smmu->evtq.q;

    do {
        while (!queue_remove_raw(q, evt)) {     // 从队列读取事件
            arm_smmu_decode_event(smmu, evt, &event);
            if (arm_smmu_handle_event(smmu, evt, &event))
                arm_smmu_dump_event(smmu, evt, &event, &rs);
            put_device(event.dev);
            cond_resched();
        }
        // 处理溢出
        if (queue_sync_prod_in(q) == -EOVERFLOW)
            dev_err(smmu->dev, "EVTQ overflow detected -- events lost\n");
    } while (!queue_empty(llq));

    queue_sync_cons_ovf(q);     // 同步溢出标志
    return IRQ_HANDLED;
}
```

### 4.7 事件日志输出 (`arm-smmu-v3.c:2218-2260`)

```c
// arm-smmu-v3.c:2218-2260
static void arm_smmu_dump_event(struct arm_smmu_device *smmu, u64 *raw,
                                struct arm_smmu_event *evt,
                                struct ratelimit_state *rs)
{
    switch (evt->id) {
    case EVT_ID_TRANSLATION_FAULT:
    case EVT_ID_ADDR_SIZE_FAULT:
    case EVT_ID_ACCESS_FAULT:
    case EVT_ID_PERMISSION_FAULT:
        dev_err(smmu->dev, "event: %s client: %s sid: %#x ssid: %#x "
                "iova: %#llx ipa: %#llx", ...);
        dev_err(smmu->dev, "%s %s %s %s \"%s\"%s%s stag: %#x",
                evt->privileged ? "priv" : "unpriv",
                evt->instruction ? "inst" : "data",
                str_read_write(evt->read),
                evt->s2 ? "s2" : "s1", event_class_str[evt->class],
                ...);
        break;
    case EVT_ID_STE_FETCH_FAULT:
    case EVT_ID_CD_FETCH_FAULT:
    case EVT_ID_VMS_FETCH_FAULT:
        dev_err(smmu->dev, "event: %s client: %s sid: %#x ssid: %#x "
                "fetch_addr: %#llx", ...);
        break;
    }
}
```

## 5. PRI 队列 (`arm-smmu-v3.h:744`)

```c
// arm-smmu-v3.h:744-746
struct arm_smmu_priq {
    struct arm_smmu_queue q;
};
```

### 5.1 PRI 处理 (`arm-smmu-v3.c:2295-2333`)

```c
// arm-smmu-v3.c:2295-2333
static void arm_smmu_handle_ppr(struct arm_smmu_device *smmu, u64 *evt)
{
    u32 sid, ssid;
    u16 grpid;
    bool ssv, last;

    sid = FIELD_GET(PRIQ_0_SID, evt[0]);
    ssv = FIELD_GET(PRIQ_0_SSV, evt[0]);
    ssid = ssv ? FIELD_GET(PRIQ_0_SSID, evt[0]) : IOMMU_NO_PASID;
    grpid = FIELD_GET(PRIQ_1_PRG_IDX, evt[1]);
    last = FIELD_GET(PRIQ_1_PRG_LAST, evt[1]);
    ...
    // 构造 PRI 响应发给设备
    cmd.opcode = CMDQ_OP_PRI_RESP;
    cmd.substream_valid = ssv;
    cmd.pri.sid = sid;
    cmd.pri.ssid = ssid;
    cmd.pri.grpid = grpid;
    cmd.pri.resp = PRI_RESP_SUCC;
    arm_smmu_cmdq_issue_cmd(smmu, &cmd);
}
```

PRI (Page Request Interface) 允许设备请求 IOMMU 驱动映射尚未驻留的页。PRIQ 线程读取 PRI 事件，然后通过 CMDQ 发送 `PRI_RESP` 响应。

### 5.2 PRIQ 中断线程 (`arm-smmu-v3.c:2333`)

```c
// arm-smmu-v3.c:2333-2351
static irqreturn_t arm_smmu_priq_thread(int irq, void *dev)
{
    struct arm_smmu_device *smmu = dev;
    struct arm_smmu_queue *q = &smmu->priq.q;
    u64 evt[PRIQ_ENT_DWORDS];

    do {
        while (!queue_remove_raw(q, evt))
            arm_smmu_handle_ppr(smmu, evt);
        if (queue_sync_prod_in(q) == -EOVERFLOW)
            dev_err(smmu->dev, "PRIQ overflow detected -- requests lost\n");
    } while (!queue_empty(llq));
    queue_sync_cons_ovf(q);
    return IRQ_HANDLED;
}
```

## 6. 队列初始化 (`arm-smmu-v3.c:4410-4503`)

### 6.1 单队列初始化 (`arm-smmu-v3.c:4410`)

```c
// arm-smmu-v3.c:4410-4449
int arm_smmu_init_one_queue(struct arm_smmu_device *smmu,
                            struct arm_smmu_queue *q, void __iomem *page,
                            unsigned long prod_off, unsigned long cons_off,
                            size_t dwords, const char *name)
{
    size_t qsz;
    do {
        qsz = ((1 << q->llq.max_n_shift) * dwords) << 3;
        q->base = dmam_alloc_coherent(smmu->dev, qsz, &q->base_dma, GFP_KERNEL);
        if (q->base || qsz < PAGE_SIZE)
            break;
        q->llq.max_n_shift--;   // 失败时回退大小
    } while (1);
    ...
    q->prod_reg = page + prod_off;
    q->cons_reg = page + cons_off;
    q->ent_dwords = dwords;
    q->q_base = Q_BASE_RWA | (q->base_dma & Q_BASE_ADDR_MASK)
              | FIELD_PREP(Q_BASE_LOG2SIZE, q->llq.max_n_shift);
    q->llq.prod = q->llq.cons = 0;
    return 0;
}
```

### 6.2 CMDQ 特别初始化 (`arm-smmu-v3.c:4451-4465`)

```c
// arm-smmu-v3.c:4451-4465
int arm_smmu_cmdq_init(struct arm_smmu_device *smmu,
                       struct arm_smmu_cmdq *cmdq)
{
    unsigned int nents = 1 << cmdq->q.llq.max_n_shift;
    atomic_set(&cmdq->owner_prod, 0);
    atomic_set(&cmdq->lock, 0);
    cmdq->valid_map = (atomic_long_t *)devm_bitmap_zalloc(smmu->dev, nents,
                                                          GFP_KERNEL);
    if (!cmdq->valid_map)
        return -ENOMEM;
    return 0;
}
```

### 6.3 所有队列的统一初始化 (`arm-smmu-v3.c:4467`)

```c
// arm-smmu-v3.c:4467-4503
static int arm_smmu_init_queues(struct arm_smmu_device *smmu)
{
    int ret;

    // CMDQ (从 page0 寄存器读取 prod/cons)
    ret = arm_smmu_init_one_queue(smmu, &smmu->cmdq.q, smmu->base,
                                  ARM_SMMU_CMDQ_PROD, ARM_SMMU_CMDQ_CONS,
                                  CMDQ_ENT_DWORDS, "cmdq");
    ret = arm_smmu_cmdq_init(smmu, &smmu->cmdq);

    // EVTQ (从 page1 寄存器读取 prod/cons)
    ret = arm_smmu_init_one_queue(smmu, &smmu->evtq.q, smmu->page1,
                                  ARM_SMMU_EVTQ_PROD, ARM_SMMU_EVTQ_CONS,
                                  EVTQ_ENT_DWORDS, "evtq");

    // SVA 场景: 分配 IOPF 队列
    if ((smmu->features & ARM_SMMU_FEAT_SVA) &&
        (smmu->features & ARM_SMMU_FEAT_STALLS)) {
        smmu->evtq.iopf = iopf_queue_alloc(dev_name(smmu->dev));
    }

    // PRIQ (仅在 FEAT_PRI 时分配)
    if (!(smmu->features & ARM_SMMU_FEAT_PRI))
        return 0;
    return arm_smmu_init_one_queue(smmu, &smmu->priq.q, smmu->page1,
                                   ARM_SMMU_PRIQ_PROD, ARM_SMMU_PRIQ_CONS,
                                   PRIQ_ENT_DWORDS, "priq");
}
```

## 7. CMDQ 故障处理

### 7.1 CMDQ 错误类型 (`arm-smmu-v3.h:386-389`)

```c
#define CMDQ_ERR_CERROR_NONE_IDX  0    // 无错误
#define CMDQ_ERR_CERROR_ILL_IDX   1    // 非法命令
#define CMDQ_ERR_CERROR_ABT_IDX   2    // 命令获取中止
#define CMDQ_ERR_CERROR_ATC_INV_IDX 3  // ATC 无效化超时
```

### 7.2 错误跳过机制 (`arm-smmu-v3.c:415-471`)

`__arm_smmu_cmdq_skip_err` 读取错误的命令位置，将其替换为 `CMD_SYNC`，并重新排队继续：

```c
// arm-smmu-v3.c:415-471
void __arm_smmu_cmdq_skip_err(struct arm_smmu_device *smmu,
                              struct arm_smmu_cmdq *cmdq)
{
    u32 cons = readl_relaxed(q->cons_reg);
    u32 idx = FIELD_GET(CMDQ_CONS_ERR, cons);

    switch (idx) {
    case CMDQ_ERR_CERROR_ABT_IDX:
        dev_err(smmu->dev, "retrying command fetch\n");
        return;          // 重试
    case CMDQ_ERR_CERROR_NONE_IDX:
        return;          // 无错误
    case CMDQ_ERR_CERROR_ATC_INV_IDX:
        return;          // ATC 超时，不 skip
    case CMDQ_ERR_CERROR_ILL_IDX:
        // 将错误命令替换为 CMD_SYNC 继续
        arm_smmu_cmdq_build_cmd(cmd, &cmd_sync);
        queue_write(Q_ENT(q, cons), cmd, q->ent_dwords);
        break;
    }
}
```

## 8. 流表初始化 (`arm-smmu-v3.c:2032-2072`)

### 8.1 所有 STE 初始化为 Abort (`arm-smmu-v3.c:2032`)

```c
// arm-smmu-v3.c:2032-2041
static void arm_smmu_init_initial_stes(struct arm_smmu_ste *strtab,
                                       unsigned int nent)
{
    unsigned int i;
    for (i = 0; i < nent; ++i)
        arm_smmu_make_abort_ste(strtab + i);
}
```

所有未使用的 SID 默认指向 abort STE，确保未配置的设备 DMA 会被中止。

## 9. 整体数据流总结

```
       ┌──────────────────────────────────────────┐
       │         设备 DMA 请求                      │
       │  StreamID + SubstreamID + IOVA            │
       └──────────────┬───────────────────────────┘
                      │
       ┌──────────────▼───────────────────────────┐
       │         Stream Table Lookup               │
       │  STE = STR_TAB_BASE + StreamID * 64       │
       │  (支持两级表: L1 → L2)                     │
       └──────────────┬───────────────────────────┘
                      │ CFG 字段决定翻译策略
       ┌──────────────▼───────────────────────────┐
       │  ┌─────────┐  ┌─────────┐  ┌──────────┐  │
       │  │ CFG=0   │  │ CFG=4   │  │ CFG=5/7  │  │
       │  │ ABORT   │  │ BYPASS  │  │TRANSLATE │  │
       │  │ 中止    │  │ 直通    │  │          │  │
       │  └─────────┘  └─────────┘  └────┬─────┘  │
       └─────────────────────────────────┼────────┘
                                         │
                     ┌───────────────────▼──────────┐
                     │   Context Descriptor Lookup   │
                     │   CD = CD_BASE + SSID * 64    │
                     │   TTB0, TCR, ASID, MAIR       │
                     └───────────────┬──────────────┘
                                     │
                     ┌───────────────▼──────────────┐
                     │     Page Table Walk (PTW)     │
                     │     TTB0 → PGD → PUD → PMD    │
                     │     → PTE → Output PA         │
                     └───────────────┬──────────────┘
                                     │
                     ┌───────────────▼──────────────┐
                     │      TLB 命中 / 未命中        │
                     │  命中: 返回缓存的 PA           │
                     │  未命中: 触发 HW PTW           │
                     └──────────────────────────────┘
```

## 下一篇文章

[第三篇：页表引擎 — LPAE 格式、Stage 1/2 与域终结](./03-page-table.md)
