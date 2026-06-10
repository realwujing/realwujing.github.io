# 第一篇：架构全景：SMMUv3 的硬件模型与初始化

> 源码：`arm-smmu-v3.c` (5625 行) | 头文件：`arm-smmu-v3.h` (1229 行)

系列目录：[ARM SMMU 内核源码深度解析](./README.md)

---

## 前言

SMMUv3 是 ARM 体系下 IOMMU 的第三代架构规范。相比前两代，v3 引入了流表（Stream Table）优化、命令队列（CMDQ）、MSI 中断和共享虚拟地址（SVA）等关键特性。本章从架构全景出发，逐一解析 `arm_smmu_device`、`arm_smmu_master`、`arm_smmu_domain` 三大核心数据结构，以及 `arm_smmu_device_probe` 的完整初始化流程。

## 1. ARM SMMU 三代架构演进

Linux 内核维护了两套 SMMU 驱动，对应不同世代：

```
drivers/iommu/arm/
├── arm-smmu/                   # SMMUv1/v2 驱动
│   ├── arm-smmu.c              # 驱动主体
│   ├── arm-smmu-qcom.c         # Qualcomm 变体 (838 行)
│   └── arm-smmu-qcom-debug.c   # Qualcomm debug 接口
├── arm-smmu-v3/                # SMMUv3 驱动
│   ├── arm-smmu-v3.c           # 驱动主体 (5625 行)
│   ├── arm-smmu-v3.h           # 数据结构与寄存器定义 (1229 行)
│   ├── arm-smmu-v3-sva.c       # SVA 支持 (348 行)
│   ├── arm-smmu-v3-iommufd.c   # iommufd 嵌套翻译 (487 行)
│   └── tegra241-cmdqv.c        # Tegra241 虚拟命令队列 (1293 行)
└── io-pgtable-arm.c            # 通用 LPAE 页表分配器 (1267 行)
```

### 1.1 SMMUv1 — 流匹配入门

SMMUv1 只支持基于 StreamID 的流匹配，通过一组 Stream Mapping 寄存器将 StreamID 映射到上下文（Context Bank）。每个 Context Bank 包含独立的 TTBR0/TTBR1 指向 Stage-1 页表。不支持 Stage-2 翻译、PRI 和 ATS。

### 1.2 SMMUv2 — 加入 Stage-2 与虚拟化

SMMUv2 在 v1 基础上新增：

- **Stage-2 翻译**：支持 IPA→PA 映射，为虚拟机直通设备提供二阶段地址翻译
- **PRI (Page Request Interface)**：设备可通过 PRI 请求宿主映射尚未驻留的页
- **ATOS (Address Translation Operations)**：软件可显式发起地址翻译请求
- **Stream Match 寄存器**：支持掩码匹配，比 v1 的精确匹配更灵活

### 1.3 SMMUv3 — 全面升级的性能与虚拟化

SMMUv3 是一次重大架构升级，核心改进入下：

| 特性 | 描述 | 对应 feature flag |
|------|------|-------------------|
| 流表 (Stream Table) | 内存中的多级表结构，替代 v1/v2 的寄存器映射 | `ARM_SMMU_FEAT_2_LVL_STRTAB` |
| 命令队列 (CMDQ) | 环形缓冲队列，批量提交 TLB/ATC 无效化命令 | `CR0_CMDQEN` |
| MSI 中断 | 基于 MSI 的事件上报，替代传统线中断 | `ARM_SMMU_FEAT_MSI` |
| 上下文描述符表 (CD) | 支持 PASID 的多上下文结构 | `ARM_SMMU_FEAT_2_LVL_CDTAB` |
| SVA/PASID | 共享 CPU 页表，支持进程级地址空间 | `ARM_SMMU_FEAT_SVA` |
| 嵌套翻译 | S1+S2 组合翻译，iommufd 用户态控制 | `ARM_SMMU_FEAT_NESTING` |
| 范围无效化 | 单条命令无效化地址范围 | `ARM_SMMU_FEAT_RANGE_INV` |

## 2. Qualcomm 变体：arm-smmu-qcom.c

Qualcomm 在标准 SMMU 基础上扩展了多个 SoC 特定特性，`arm-smmu-qcom.c` (838 行) 通过 `struct arm_smmu_device` 的 `qcom` 字段注入平台逻辑：

```c
// drivers/iommu/arm/arm-smmu/arm-smmu-qcom.c: implementation specific tuning
struct qcom_smmu {
    struct arm_smmu_device smmu;
    bool bypass_quirk;
    u8 bypass_cbndx;
    u32 stall_enabled;
    ...
};
```

**关键功能**：

1. **Prefetch 调优**：通过 `qcom,no-dynamic-asid` 等 DT 属性控制预取行为
2. **CMTLB/CPRE**：Qualcomm 特有的 TLB 维护和功耗管理
3. **Per-SoC match table**：精确匹配 Adreno GPU、MDSS (显示子系统)、Venus (视频编码) 等不同 IP 的 SMMU 实例
4. **Bypass quirk**：某些 SoC 需要在特定 context bank 上配置 bypass 模式

```c
// 典型的 per-SoC 匹配结构
static const struct of_device_id qcom_smmu_client_of_match[] = {
    { .compatible = "qcom,adreno" },
    { .compatible = "qcom,mdp4" },
    { .compatible = "qcom,mdss" },
    { .compatible = "qcom,venus" },
    ...
};
```

## 3. 核心数据结构全景

### 3.1 arm_smmu_device — SMMU 实例 (`arm-smmu-v3.h:824`)

`struct arm_smmu_device` 是 SMMUv3 驱动的核心数据结构，表示一个物理 SMMU 实例（一个 SoC 上可能有多个 SMMU，例如 TCU 独立于 TBU）：

```c
// arm-smmu-v3.h:824-893
struct arm_smmu_device {
    struct device           *dev;           // SMMU 平台设备
    struct device           *impl_dev;      // 厂商实现扩展设备
    const struct arm_smmu_impl_ops *impl_ops; // 厂商实现钩子

    void __iomem            *base;          // MMIO page0 基地址 (64KB)
    void __iomem            *page1;         // MMIO page1 基地址 (队列寄存器)

#define ARM_SMMU_FEAT_2_LVL_STRTAB   (1 << 0)
#define ARM_SMMU_FEAT_2_LVL_CDTAB    (1 << 1)
#define ARM_SMMU_FEAT_TT_LE          (1 << 2)
#define ARM_SMMU_FEAT_TT_BE          (1 << 3)
#define ARM_SMMU_FEAT_PRI            (1 << 4)
#define ARM_SMMU_FEAT_ATS            (1 << 5)
#define ARM_SMMU_FEAT_SEV            (1 << 6)
#define ARM_SMMU_FEAT_MSI            (1 << 7)
#define ARM_SMMU_FEAT_COHERENCY      (1 << 8)
#define ARM_SMMU_FEAT_TRANS_S1       (1 << 9)
#define ARM_SMMU_FEAT_TRANS_S2       (1 << 10)
#define ARM_SMMU_FEAT_STALLS         (1 << 11)
#define ARM_SMMU_FEAT_HYP            (1 << 12)
#define ARM_SMMU_FEAT_STALL_FORCE    (1 << 13)
#define ARM_SMMU_FEAT_VAX            (1 << 14)    // 52-bit VA
#define ARM_SMMU_FEAT_RANGE_INV      (1 << 15)
#define ARM_SMMU_FEAT_BTM            (1 << 16)
#define ARM_SMMU_FEAT_SVA            (1 << 17)
#define ARM_SMMU_FEAT_E2H            (1 << 18)
#define ARM_SMMU_FEAT_NESTING        (1 << 19)
#define ARM_SMMU_FEAT_ATTR_TYPES_OVR (1 << 20)
#define ARM_SMMU_FEAT_HA             (1 << 21)    // 硬件访问标志
#define ARM_SMMU_FEAT_HD             (1 << 22)    // 硬件脏标志
#define ARM_SMMU_FEAT_S2FWB          (1 << 23)    // Stage-2 强制写回
#define ARM_SMMU_FEAT_BBML2          (1 << 24)    // BBM level 2
    u32                     features;       // 24 个硬件 feature flags (832-857 行)

    struct arm_smmu_cmdq    cmdq;           // 主命令队列
    struct arm_smmu_evtq    evtq;           // 事件队列 (fault 上报)
    struct arm_smmu_priq    priq;           // PRI 请求队列 (ATS)

    int                     gerr_irq;       // 全局错误中断
    int                     combined_irq;   // 组合中断 (Cavium 等)

    unsigned long           oas;            // 输出地址大小 (PA bits)
    unsigned long           pgsize_bitmap;  // 支持的页大小位图

    unsigned int            asid_bits;      // 8 或 16 (ARM_SMMU_MAX_ASIDS)
    unsigned int            vmid_bits;      // 8 或 16 (ARM_SMMU_MAX_VMIDS)
    unsigned int            ssid_bits;      // SubstreamID 位宽
    unsigned int            sid_bits;       // StreamID 位宽

    struct arm_smmu_strtab_cfg strtab_cfg;  // 流表配置 (线性或 2 级)

    struct iommu_device     iommu;          // IOMMU core 句柄
    struct rb_root          streams;        // StreamID → master 的红黑树
    struct mutex            streams_mutex;
};
```

**features 位掩码的设计意图**：

- 前 24 位 (0-23) 来自 ID 寄存器硬件探测（`arm_smmu_device_hw_probe`，`arm-smmu-v3.c:5012`）
- 第 25+ 位留给厂商扩展（如 Tegra241 的 `ARM_SMMU_OPT_TEGRA241_CMDQV`）
- 每个 feature 决定了驱动程序能使用哪些代码路径：
  - `ARM_SMMU_FEAT_COHERENCY` → IOMMU page table walker 与 CPU 共享 cache
  - `ARM_SMMU_FEAT_SVA` → 启用 SVA 代码路径（`CONFIG_ARM_SMMU_V3_SVA`）
  - `ARM_SMMU_FEAT_NESTING` → 启用 iommufd 嵌套域支持
  - `ARM_SMMU_FEAT_HD + ARM_SMMU_FEAT_COHERENCY` → 启用 DBM (Dirty Bit Management)

**options 位掩码**（不同于 features，options 是软件级别的配置）：

```c
#define ARM_SMMU_OPT_SKIP_PREFETCH   (1 << 0)   // Hisilicon: 跳过预取命令
#define ARM_SMMU_OPT_PAGE0_REGS_ONLY (1 << 1)   // Cavium CN9900: 无 page1
#define ARM_SMMU_OPT_MSIPOLL         (1 << 2)   // MSI polling for CMD_SYNC
#define ARM_SMMU_OPT_CMDQ_FORCE_SYNC (1 << 3)   // 强制同步
#define ARM_SMMU_OPT_TEGRA241_CMDQV  (1 << 4)   // Tegra241 虚拟 CMDQ
```

### 3.2 arm_smmu_master — 设备端结构 (`arm-smmu-v3.h:927`)

每个挂载到 SMMU 上的设备（如 PCIe Endpoint）都有一个 `arm_smmu_master` 实例：

```c
// arm-smmu-v3.h:927-948
struct arm_smmu_master {
    struct arm_smmu_device      *smmu;          // 所属 SMMU 实例
    struct device               *dev;           // 设备 struct device
    struct arm_smmu_stream      *streams;       // 多 StreamID 数组
    struct arm_smmu_invs        *build_invs;    // 附着时构建 invalidation 数组的临时内存
    struct arm_smmu_vmaster     *vmaster;       // 虚拟 master (iommufd 嵌套)
    struct arm_smmu_ctx_desc_cfg cd_table;      // Context Descriptor 表 (PASID 目录)
    unsigned int                num_streams;    // StreamID 数量
    bool                        ats_enabled :1;
    bool                        ste_ats_enabled :1;
    bool                        stall_enabled;
    unsigned int                ssid_bits;      // 此设备支持的 SubstreamID 位宽
    unsigned int                iopf_refcount;  // IOPF (I/O Page Fault) 引用计数
};
```

**关键设计点**：

1. **streams 数组**：一个 PCIe 设备可能通过别名（alias）被赋予多个 StreamID，因此需要数组来维护所有相关的 STE 索引
2. **cd_table**：Context Descriptor 表，支持 PASID（进程地址空间 ID），每个 SSID 对应一个 CD
3. **build_invs**：预分配的内存用于在 `arm_smmu_attach_prepare_invs`（`arm-smmu-v3.c:3368`）期间构建无锁的 invalidation 数组

### 3.3 arm_smmu_domain — IOMMU 域 (`arm-smmu-v3.h:957`)

域（domain）是 IOMMU API 的核心抽象，表示一组设备共享的地址空间：

```c
// arm-smmu-v3.h:957-980
struct arm_smmu_domain {
    struct arm_smmu_device      *smmu;
    struct io_pgtable_ops       *pgtbl_ops;     // 页表操作接口
    atomic_t                    nr_ats_masters; // ATS 主设备计数

    enum arm_smmu_domain_stage  stage;          // S1 / S2 / SVA
    union {
        struct arm_smmu_ctx_desc cd;            // Stage-1: ASID
        struct arm_smmu_s2_cfg   s2_cfg;        // Stage-2: VMID
    };

    struct iommu_domain         domain;         // 核心 IOMMU domain

    struct arm_smmu_invs __rcu  *invs;          // RCU 保护的 invalidation 数组
    struct list_head            devices;        // arm_smmu_master_domain 链表
    spinlock_t                  devices_lock;
    bool                        enforce_cache_coherency :1;
    bool                        nest_parent :1;

    struct mmu_notifier         mmu_notifier;   // SVA 场景下的 mmu_notifier
};
```

**域类型** (`enum arm_smmu_domain_stage`, `arm-smmu-v3.h:951`)：

```c
enum arm_smmu_domain_stage {
    ARM_SMMU_DOMAIN_S1 = 0,   // Stage-1 翻译: VA → IPA (或 PA)
    ARM_SMMU_DOMAIN_S2,       // Stage-2 翻译: IPA → PA
    ARM_SMMU_DOMAIN_SVA,      // 共享虚拟地址 (S1，使用进程页表)
};
```

**invalidation 数组设计** (`arm_smmu-v3.h:715-723`)：

```c
struct arm_smmu_invs {
    size_t      max_invs;
    size_t      num_invs;
    size_t      num_trashes;    // 已标记删除的条目数
    rwlock_t    rwlock;         // 保护 ATS 操作的读写锁
    bool        has_ats;        // 是否包含 ATS 类型的 invalidation
    struct rcu_head rcu;
    struct arm_smmu_inv inv[];  // 柔性数组
};
```

`arm_smmu_invs` 是一个 RCU 数据结构。在 `->attach_dev` 回调期间，`arm_smmu_invs_merge()` (`arm-smmu-v3.c:1137`) 和 `arm_smmu_invs_unref()` (`arm-smmu-v3.c:1207`) 用于原子地修改域的 invalidation 数组。

### 3.4 arm_smmu_nested_domain — 嵌套域 (`arm-smmu-v3.h:982`)

嵌套域用于 iommufd 场景，允许用户态控制 Stage-1 页表而内核管理 Stage-2：

```c
// arm-smmu-v3.h:982-988
struct arm_smmu_nested_domain {
    struct iommu_domain domain;
    struct arm_vsmmu   *vsmmu;       // 虚拟 SMMU 实例
    bool               enable_ats :1;
    __le64             ste[2];       // 用户态提供的虚拟 STE
};
```

### 3.5 arm_vsmmu — 虚拟 SMMU (`arm-smmu-v3.h:1173`)

```c
// arm-smmu-v3.h:1173-1178
struct arm_vsmmu {
    struct iommufd_viommu  core;      // iommufd 框架的 viommu
    struct arm_smmu_device *smmu;     // 底层物理 SMMU
    struct arm_smmu_domain *s2_parent; // Stage-2 父域
    u16                    vmid;      // VMID
};
```

## 4. SMMUv3 架构数据流

以下 ASCII 图展示 SMMUv3 从 DMA 请求到物理地址的完整翻译路径：

```
┌──────────────────────────────────────────────────────────────────────┐
│                         SMMUv3 翻译流水线                              │
│                                                                      │
│  ┌──────────────┐    StreamID          Stream Table                  │
│  │ PCIe Device  │────(SID)──────────▶ ┌─────────────┐                │
│  │ BDF=05:00.0  │                     │  STE[SID]   │                │
│  │ DMA IOVA     │                     │  ┌─────────┐│                │
│  │ PASID=0      │                     │  │V=1      ││                │
│  └──────────────┘                     │  │CFG=S1   ││                │
│                                       │  │S1FMT    ││                │
│   STRTAB_STE_0_CFG_S1_TRANS ─────────│  │CTXPTR───┼┼───┐            │
│                                       │  └─────────┘│   │            │
│                                       └─────────────┘   │            │
│                                                          ▼            │
│                                       Context Descriptor Table       │
│                                       ┌──────────────────┐           │
│                                        │ Linear or 2-level │          │
│                                        │  CD[SSID=0]       │          │
│                                        │  ┌──────────────┐ │          │
│                                        │  │ TCR  (T0SZ etc)│ │          │
│   STRTAB_STE_0_S1CTXPTR_MASK ────────│  │ TTB0 ─────────┼─┼───┐      │
│   CTXDESC_CD_0_AA64 ─────────────────│  │ ASID=3        │ │   │      │
│   CTXDESC_CD_0_ASID  ────────────────│  │ MAIR          │ │   │      │
│                                        │  └──────────────┘ │   │      │
│                                        └──────────────────┘   │      │
│                                                                ▼      │
│                                       Stage-1 Page Table (LPAE)      │
│                                       ┌──────────────────────┐       │
│                                        │ Level 0 (PGD)        │       │
│                                        │    │                 │       │
│                                        │ Level 1 (PUD)        │       │
│                                        │    │                 │       │
│                                        │ Level 2 (PMD)        │       │
│                                        │    │                 │       │
│                                        │ Level 3 (PTE)        │       │
│  CTXDESC_CD_1_TTB0_MASK ─────────────│         Output PA     │       │
│                                       └──────────────────────┘       │
│                                                                      │
│  ┌─────────────────────────────────────────────────────┐            │
│  │                  Stage-2 翻译 (可选)                  │            │
│  │                                                     │            │
│  │  STE.CFG=S2_TRANS ───▶ STRTAB_STE_3_S2TTB_MASK      │            │
│  │                        ┌──────────────────────┐      │            │
│  │                        │ S2 Page Table        │      │            │
│  │                        │ (IPA → PA)           │      │            │
│  │                        └──────────────────────┘      │            │
│  └─────────────────────────────────────────────────────┘            │
│                                                                      │
│  ┌─────────────────────────────────────────────────────┐            │
│  │                  TLB / Configuration Cache            │            │
│  │                                                     │            │
│  │  硬件自动缓存翻译结果，软件通过 CMDQ 发送无效化命令    │            │
│  │  CMD_TLBI_NH_ASID ─▶ 失效特定 ASID 的 TLB 条目      │            │
│  │  CMD_ATC_INV      ─▶ 失效设备侧 ATC 缓存             │            │
│  │  CMD_CFGI_STE     ─▶ 失效 STE 配置缓存               │            │
│  │  CMD_CFGI_CD      ─▶ 失效 CD 配置缓存                │            │
│  └─────────────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

## 5. 初始化流程

### 5.1 模块入口 (`arm-smmu-v3.c:5619`)

```c
// arm-smmu-v3.c:5602-5620
static struct platform_driver arm_smmu_driver = {
    .driver = {
        .name               = "arm-smmu-v3",
        .of_match_table     = arm_smmu_of_match,
        .suppress_bind_attrs = true,
    },
    .probe  = arm_smmu_device_probe,      // 入口: 第 5465 行
    .remove = arm_smmu_device_remove,
    .shutdown = arm_smmu_device_shutdown,
};
module_driver(arm_smmu_driver, platform_driver_register,
              arm_smmu_driver_unregister);
```

### 5.2 Probe 流程 (`arm-smmu-v3.c:5465`)

```
arm_smmu_device_probe (第 5465 行)
│
├── devm_kzalloc: 分配 smmu 结构体
├── arm_smmu_device_dt_probe / arm_smmu_device_acpi_probe: 解析 DT/ACPI
│   ├── 读取 "msi-parent" 属性获取 ITS 节点
│   ├── 读取 "dma-coherent" 设置 ARM_SMMU_FEAT_COHERENCY
│   └── 读取 "hisilicon,broken-prefetch-cmd" 等 quirk 选项
│
├── arm_smmu_impl_probe (第 5431 行): 探测厂商实现扩展
│   ├── 检查 ARM_SMMU_OPT_TEGRA241_CMDQV → tegra241_cmdqv_probe
│   ├── 验证 impl_ops 一致性 (get_viommu_size ↔ vsmmu_init)
│   └── devm_add_action_or_reset: 注册 remove 回调
│
├── platform_get_resource: 获取 MMIO 基地址
├── arm_smmu_ioremap: 映射 page0 (64KB)
├── arm_smmu_ioremap: 映射 page1 (若资源 > 64KB)
│
├── platform_get_irq_byname_optional: 获取中断线
│   ├── "combined" → combined_irq (Cavium ThunderX2)
│   ├── "eventq"   → evtq.q.irq
│   ├── "priq"     → priq.q.irq
│   └── "gerror"   → gerr_irq
│
├── arm_smmu_device_hw_probe (第 5012 行): 硬件特性探测 ★
│   ├── IDR0: ST_LVL, CD2L, TTENDIAN, PRI, ATS, SEV, MSI, HYP
│   │         STALL_MODEL, S1P, S2P, TTF, ASID16, VMID16, HTTU, COHACC
│   ├── IDR1: TABLES_PRESET, QUEUES_PRESET, CMDQS, EVTQS, PRIQS
│   │         SSIDSIZE, SIDSIZE
│   ├── IDR3: RIL, BBM, FWB
│   └── IDR5: OAS, VAX, GRAN4K/16K/64K, STALL_MAX
│
├── arm_smmu_init_structures (第 4575 行): 初始化内存数据结构
│   ├── mutex_init(&smmu->streams_mutex)
│   ├── smmu->streams = RB_ROOT
│   ├── arm_smmu_init_queues (第 4467 行): 分配 CMDQ/EVTQ/PRIQ
│   │   ├── arm_smmu_init_one_queue: 分配 cmds 环
│   │   ├── arm_smmu_cmdq_init: 分配 valid_map 位图
│   │   ├── arm_smmu_init_one_queue: 分配 evtq
│   │   ├── iopf_queue_alloc: 若 SVA+STALLS 支持
│   │   └── arm_smmu_init_one_queue: 分配 priq (若 FEAT_PRI)
│   └── arm_smmu_init_strtab (第 4559 行): 初始化流表
│       ├── arm_smmu_init_strtab_2lvl: 二级表 (FEAT_2_LVL_STRTAB)
│       │   ├── 计算 L1 条目数: min(last_sid_idx+1, STRTAB_MAX_L1_ENTRIES)
│       │   ├── dmam_alloc_coherent: 分配 L1 表
│       │   └── devm_kcalloc: 分配 L2 指针数组
│       └── arm_smmu_init_strtab_linear: 线性表
│           ├── size = (1 << sid_bits) * sizeof(ste)
│           ├── dmam_alloc_coherent: 分配线性表
│           └── arm_smmu_init_initial_stes: 全部初始化为 abort
│
├── platform_set_drvdata: 设置驱动私有数据
├── arm_smmu_rmr_install_bypass_ste: 安装 RMR bypass STE
│
├── arm_smmu_device_reset (第 4809 行): 硬件复位与启动
│   ├── arm_smmu_device_disable: 清除 CR0_SMMUEN
│   ├── 写 CR1: 设置 table/queue 内存属性 (WB, ISH)
│   ├── 写 CR2: PTM, RECINVSID, E2H
│   ├── arm_smmu_write_strtab: 写 STRTAB_BASE 到硬件
│   ├── 写 CMDQ_BASE/PROD/CONS 到硬件寄存器
│   ├── arm_smmu_write_reg_sync: 使能 CR0_CMDQEN
│   ├── CMDQ_OP_CFGI_ALL: 无效化所有配置缓存
│   ├── CMDQ_OP_TLBI_EL2_ALL / CMDQ_OP_TLBI_NSNH_ALL: 无效化所有 TLB
│   ├── 写 EVTQ_BASE/PROD/CONS, 使能 CR0_EVTQEN
│   ├── 写 PRIQ_BASE/PROD/CONS, 使能 CR0_PRIQEN (若 FEAT_PRI)
│   └── arm_smmu_write_reg_sync: 使能 CR0_SMMUEN
│
├── arm_smmu_setup_irqs (第 4733 行): 设置中断
│   ├── 清除 IRQ_CTRL
│   ├── 注册 combined_irq 或 unique irq 处理器
│   └── 使能 IRQ_CTRL_EVTQ_IRQEN | GERROR_IRQEN | PRIQ_IRQEN
│
└── iommu_device_sysfs_add / iommu_device_register: 注册到 IOMMU 核心
```

### 5.3 硬件特性探测详解 (`arm-smmu-v3.c:5012`)

`arm_smmu_device_hw_probe` 读取 SMMU 的 ID 寄存器并填充 `smmu->features` 位掩码。以下是与 features 标志的完整映射关系（行 832-857）：

| Feature Flag | 行号 | 来源寄存器 | 含义 |
|---|---|---|---|
| `ARM_SMMU_FEAT_2_LVL_STRTAB` | 833 | IDR0 ST_LVL=1 | 支持二级流表 |
| `ARM_SMMU_FEAT_2_LVL_CDTAB` | 834 | IDR0 CD2L=1 | 支持二级 CD 表 |
| `ARM_SMMU_FEAT_TT_LE` | 835 | IDR0 TTENDIAN=LE | 小端页表 |
| `ARM_SMMU_FEAT_TT_BE` | 836 | IDR0 TTENDIAN=BE | 大端页表 |
| `ARM_SMMU_FEAT_PRI` | 837 | IDR0 PRI=1 | Page Request Interface |
| `ARM_SMMU_FEAT_ATS` | 838 | IDR0 ATS=1 | Address Translation Services |
| `ARM_SMMU_FEAT_SEV` | 839 | IDR0 SEV=1 | 发送 WFE 唤醒事件 |
| `ARM_SMMU_FEAT_MSI` | 840 | IDR0 MSI=1 | 支持 MSI 中断 |
| `ARM_SMMU_FEAT_COHERENCY` | 841 | DT dma-coherent | IO-coherent TCU |
| `ARM_SMMU_FEAT_TRANS_S1` | 842 | IDR0 S1P=1 | Stage-1 翻译 |
| `ARM_SMMU_FEAT_TRANS_S2` | 843 | IDR0 S2P=1 | Stage-2 翻译 |
| `ARM_SMMU_FEAT_STALLS` | 844 | IDR0 STALL_MODEL=0/2 | Stall 模式 |
| `ARM_SMMU_FEAT_HYP` | 845 | IDR0 HYP=1 | Hypervisor 模式 |
| `ARM_SMMU_FEAT_STALL_FORCE` | 846 | IDR0 STALL_MODEL=2 | 强制 Stall |
| `ARM_SMMU_FEAT_VAX` | 847 | IDR5 VAX=1 | 52-bit VA |
| `ARM_SMMU_FEAT_RANGE_INV` | 848 | IDR3 RIL=1 | 范围无效化 |
| `ARM_SMMU_FEAT_BTM` | 849 | IDR0 HTTU=2 | 广播 TLB 维护 |
| `ARM_SMMU_FEAT_SVA` | 850 | 组合条件 | SVA 支持 |
| `ARM_SMMU_FEAT_E2H` | 851 | HYP + VHE | VHE 宿主模式 |
| `ARM_SMMU_FEAT_NESTING` | 852 | 组合条件 | 嵌套翻译 |
| `ARM_SMMU_FEAT_ATTR_TYPES_OVR` | 853 | IDR1 ATTR_TYPES_OVR=1 | 属性类型覆盖 |
| `ARM_SMMU_FEAT_HA` | 854 | IDR0 HTTU=1/2 | 硬件访问标志 |
| `ARM_SMMU_FEAT_HD` | 855 | IDR0 HTTU=2 | 硬件脏标志 |
| `ARM_SMMU_FEAT_S2FWB` | 856 | IDR3 FWB=1 | Stage-2 强制写回 |
| `ARM_SMMU_FEAT_BBML2` | 857 | BBM=2 | 二级断点匹配 |

### 5.4 impl_ops 扩展机制 (`arm-smmu-v3.h:805`)

SMMUv3 驱动支持通过 `struct arm_smmu_impl_ops` 注入厂商特定的实现：

```c
// arm-smmu-v3.h:805-821
struct arm_smmu_impl_ops {
    int (*device_reset)(struct arm_smmu_device *smmu);
    void (*device_remove)(struct arm_smmu_device *smmu);
    int (*init_structures)(struct arm_smmu_device *smmu);
    struct arm_smmu_cmdq *(*get_secondary_cmdq)(
        struct arm_smmu_device *smmu, struct arm_smmu_cmdq_ent *ent);
    void *(*hw_info)(struct arm_smmu_device *smmu, u32 *length,
                     enum iommu_hw_info_type *type);
    size_t (*get_viommu_size)(enum iommu_viommu_type viommu_type);
    int (*vsmmu_init)(struct arm_vsmmu *vsmmu,
                      const struct iommu_user_data *user_data);
};
```

**钩子函数的作用**：

1. **device_reset**: 替代或增强标准硬件复位流程（第 4809 行）
2. **device_remove**: 清理厂商特定资源
3. **init_structures**: 初始化额外的内存结构（如 Tegra241 的 VCMDQ）
4. **get_secondary_cmdq**: 返回次级命令队列（如 Tegra241 基于命令类型分发到不同 VCMDQ）
5. **hw_info**: 向用户态暴露厂商特定的硬件信息（第 10 行 of iommufd.c）
6. **get_viommu_size / vsmmu_init**: 虚拟 IOMMU 支持

## 6. IOMMU 操作注册 (`arm-smmu-v3.c:4372`)

SMMUv3 驱动最终通过 `arm_smmu_ops` 向 IOMMU 核心注册操作接口：

```c
// arm-smmu-v3.c:4372-4402
static const struct iommu_ops arm_smmu_ops = {
    .identity_domain        = &arm_smmu_identity_domain,
    .blocked_domain         = &arm_smmu_blocked_domain,
    .release_domain         = &arm_smmu_blocked_domain,
    .capable                = arm_smmu_capable,
    .hw_info                = arm_smmu_hw_info,
    .domain_alloc_sva       = arm_smmu_sva_domain_alloc,
    .domain_alloc_paging_flags = arm_smmu_domain_alloc_paging_flags,
    .probe_device           = arm_smmu_probe_device,
    .release_device         = arm_smmu_release_device,
    .device_group           = arm_smmu_device_group,
    .of_xlate               = arm_smmu_of_xlate,
    .get_resv_regions       = arm_smmu_get_resv_regions,
    .page_response          = arm_smmu_page_response,
    .def_domain_type        = arm_smmu_def_domain_type,
    .get_viommu_size        = arm_smmu_get_viommu_size,
    .viommu_init            = arm_vsmmu_init,
    .user_pasid_table       = 1,
    .owner                  = THIS_MODULE,
    .default_domain_ops = &(const struct iommu_domain_ops) {
        .attach_dev             = arm_smmu_attach_dev,
        .enforce_cache_coherency = arm_smmu_enforce_cache_coherency,
        .set_dev_pasid          = arm_smmu_s1_set_dev_pasid,
        .map_pages              = arm_smmu_map_pages,
        .unmap_pages            = arm_smmu_unmap_pages,
        .flush_iotlb_all        = arm_smmu_flush_iotlb_all,
        .iotlb_sync             = arm_smmu_iotlb_sync,
        .iova_to_phys           = arm_smmu_iova_to_phys,
        .free                   = arm_smmu_domain_free_paging,
    }
};
```

重点关注：
- `.map_pages` / `.unmap_pages` 最终委托给 `smmu_domain->pgtbl_ops`，即 `io-pgtable-arm.c` 的 LPAE 页表引擎
- `.default_domain_ops` 采用匿名 struct 语法直接嵌入初始化，这是内核中常见的零分配优化模式

## 7. TBU 与 SMMU 拓扑

大型 SoC 中 SMMU 采用分布式拓扑：一个 TCU（Translation Control Unit）负责页表遍历，多个 TBU（Translation Buffer Unit）分布在各总线主设备附近提供本地 TLB 缓存：

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  TBU (GPU)   │  │ TBU (Display)│  │  TBU (DMA)   │
│  Local TLB   │  │  Local TLB   │  │  Local TLB   │
└──────┬───────┘  └──────┬───────┘  └──────┬───────┘
       │                 │                 │
       └────────────┬────┴────────────────┘
                    │ 分布式互连总线
               ┌────┴──────┐
               │ TCU (DTU) │ ← SMMUv3 驱动管理的核心
               │ Page Table│
               │  Walker   │
               │ CMDQ/E/P  │
               └───────────┘
```

TCU 通过 CMDQ 广播 TLB 无效化到各 TBU，确保所有 Translation Agent 一致。ARM SMMUv3 规范称之为"分布式翻译"架构，需要 `ARM_SMMU_FEAT_COHERENCY` 来保证 TCU 遍历页表时与 CPU 缓存一致。

## 下一篇文章

[第二篇：流表、命令队列与事件队列 — SMMUv3 的三大核心数据结构](./02-stream-table.md)
