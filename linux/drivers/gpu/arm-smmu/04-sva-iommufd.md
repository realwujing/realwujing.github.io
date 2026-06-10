# 第四篇：SVA 与 IOMMUFD — 共享虚拟地址与嵌套翻译

> 源码：`arm-smmu-v3-sva.c` (348 行) | `arm-smmu-v3-iommufd.c` (487 行) | `tegra241-cmdqv.c` (1293 行)

系列目录：[ARM SMMU 内核源码深度解析](./README.md)

---

## 前言

SVA（Shared Virtual Addressing）和 IOMMUFD（IOMMU userspace file descriptor）是 SMMUv3 最前沿的两大功能。SVA 允许设备 DMA 直接使用进程虚拟地址，共享 CPU 页表，消除数据拷贝。IOMMUFD 通过将 SMMU 控制权安全地暴露给用户态，使虚拟机监控器（VMM）可以将 Stage-1 页表管理下放给客户机内核。本章深入解析这两个子系统的完整实现。

## 1. SVA 子系统：arm-smmu-v3-sva.c

### 1.1 什么是 SVA？

SVA（Shared Virtual Addressing，也称 SVM/PASID）允许 PCIe 设备通过 PASID 前缀直接使用进程的 CPU 页表进行 DMA。核心效果：

```
传统 DMA (无 SVA):
    CPU进程            IOMMU
    VA → PA            IOVA → PA
    独立页表           独立页表
    需要 pin/映射      需要 pin/映射
    ❌ 两套地址空间    ❌ 额外拷贝开销

SVA:
    CPU进程            IOMMU
    VA → PA            VA → PA  ← 共享同一页表！
    共享页表           共享页表
    系统交换           设备 TLB miss → PRI → 缺页处理
    ✅ 零拷贝          ✅ 数据直接可用
```

### 1.2 构建 SVA CD：arm_smmu_make_sva_cd (`arm-smmu-v3-sva.c:51`)

SVA 的核心是将 CPU 进程的页表参数复制到 SMMU 的 Context Descriptor 中：

```c
// arm-smmu-v3-sva.c:51-122
VISIBLE_IF_KUNIT
void arm_smmu_make_sva_cd(struct arm_smmu_cd *target,
                          struct arm_smmu_master *master,
                          struct mm_struct *mm,
                          u16 asid)
{
    u64 par;
    memset(target, 0, sizeof(*target));

    // 从 CPU PARANGE 寄存器获取物理地址大小
    par = cpuid_feature_extract_unsigned_field(
        read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1),
        ID_AA64MMFR0_EL1_PARANGE_SHIFT);

    // 构造 CD word 0
    target->data[0] = cpu_to_le64(
        CTXDESC_CD_0_TCR_EPD1 |        // 禁用 TTBR1 (SMMU 仅用 TTBR0)
        CTXDESC_CD_0_V |               // CD 有效
        FIELD_PREP(CTXDESC_CD_0_TCR_IPS, par) |  // PA 大小
        CTXDESC_CD_0_AA64 |            // AArch64 页表
        (master->stall_enabled ? CTXDESC_CD_0_S : 0) |
        CTXDESC_CD_0_R |               // Remote
        CTXDESC_CD_0_A |               // Access flag
        CTXDESC_CD_0_ASET |            // ASID ETM
        FIELD_PREP(CTXDESC_CD_0_ASID, asid));

    // 如果有 mm_struct (进程存在) → 填充页表参数
    if (mm) {
        target->data[0] |= cpu_to_le64(
            FIELD_PREP(CTXDESC_CD_0_TCR_T0SZ, 64ULL - vabits_actual) |
            FIELD_PREP(CTXDESC_CD_0_TCR_TG0, page_size_to_cd()) |
            FIELD_PREP(CTXDESC_CD_0_TCR_IRGN0, ARM_LPAE_TCR_RGN_WBWA) |
            FIELD_PREP(CTXDESC_CD_0_TCR_ORGN0, ARM_LPAE_TCR_RGN_WBWA) |
            FIELD_PREP(CTXDESC_CD_0_TCR_SH0, ARM_LPAE_TCR_SH_IS));

        // TTB0 = 进程页表基址直接映射
        target->data[1] = cpu_to_le64(
            virt_to_phys(mm->pgd) & CTXDESC_CD_1_TTB0_MASK);
    } else {
        // mm==NULL → 创建"全部错误"的 CD (用于 detach 场景)
        target->data[0] |= cpu_to_le64(CTXDESC_CD_0_TCR_EPD0);
        if (!(master->smmu->features & ARM_SMMU_FEAT_STALL_FORCE))
            target->data[0] &= ~cpu_to_le64(CTXDESC_CD_0_S | CTXDESC_CD_0_R);
    }

    // MAIR 直接复制 CPU MAIR_EL1
    target->data[3] = cpu_to_le64(read_sysreg(mair_el1));
}
```

**关键设计点**：

1. **TTB0 = `mm->pgd`**：设备 DMA 直接使用进程页表根，无需创建 mirror 页表
2. **T0SZ**：`64 - vabits_actual`，例如 48-bit VA → T0SZ=16
3. **MAIR**：直接复制 `mair_el1`，确保设备 DMA 的内存属性与 CPU 一致
4. **mm==NULL 场景**：用于进程退出时的 detach，CD 设为 EPD0（禁止翻译），使后续 DMA 立即 abort
5. **S1PIE/POE/GCS 不在 SMMU 支持范围内**：行 120-121 处的 `dev_warn_once` 提醒用户这些 CPU 安全特性不会在设备侧生效

### 1.3 page_size_to_cd (`arm-smmu-v3-sva.c:39`)

```c
static u64 page_size_to_cd(void)
{
    static_assert(PAGE_SIZE == SZ_4K || PAGE_SIZE == SZ_16K ||
                  PAGE_SIZE == SZ_64K);
    if (PAGE_SIZE == SZ_64K)
        return ARM_LPAE_TCR_TG0_64K;
    if (PAGE_SIZE == SZ_16K)
        return ARM_LPAE_TCR_TG0_16K;
    return ARM_LPAE_TCR_TG0_4K;
}
```

TCR.TG0 编码：0 = 4KB，1 = 64KB，2 = 16KB。内核编译时根据 `CONFIG_ARM64_*K_PAGES` 确定。

### 1.4 SVA 支持检查：arm_smmu_sva_supported (`arm-smmu-v3-sva.c:187`)

```c
// arm-smmu-v3-sva.c:187-237
bool arm_smmu_sva_supported(struct arm_smmu_device *smmu)
{
    u32 feat_mask = ARM_SMMU_FEAT_COHERENCY; // 必需: 缓存一致性

    // LPA2 (52-bit VA + 4K/16K 页) 不被 SMMU 支持
    if (vabits_actual == 52) {
        if (PAGE_SIZE != SZ_64K)
            return false;
        feat_mask |= ARM_SMMU_FEAT_VAX;
    }

    if (system_supports_bbml2_noabort())
        feat_mask |= ARM_SMMU_FEAT_BBML2;

    // 检查 SMMU 特性
    if ((smmu->features & feat_mask) != feat_mask)
        return false;

    // SMMU 必须支持当前 CPU 的页大小
    if (!(smmu->pgsize_bitmap & PAGE_SIZE))
        return false;

    // CPU 和 SMMU 的 PA 大小必须兼容
    reg = read_sanitised_ftr_reg(SYS_ID_AA64MMFR0_EL1);
    fld = cpuid_feature_extract_unsigned_field(reg, ID_AA64MMFR0_EL1_PARANGE_SHIFT);
    oas = id_aa64mmfr0_parange_to_phys_shift(fld);
    if (smmu->oas < oas)     // SMMU 的 PA 不能小于 CPU 的 PA
        return false;

    // ASID 位宽: SMMU 至少要有 CPU 那么多
    fld = cpuid_feature_extract_unsigned_field(reg, ID_AA64MMFR0_EL1_ASIDBITS_SHIFT);
    asid_bits = fld ? 16 : 8;
    if (smmu->asid_bits < asid_bits)
        return false;

    return true;
}
```

**必需特性条件清单**：

| 条件 | 标志 | 说明 |
|------|------|------|
| IO-Coherent | `ARM_SMMU_FEAT_COHERENCY` | SMMU 页表遍历必须 cache coherent |
| VAX (52-bit) | `ARM_SMMU_FEAT_VAX` | 仅 52-bit VA + 64K 页支持 |
| BBML2 | `ARM_SMMU_FEAT_BBML2` | 如果 CPU 支持 BBML2 无中断 |
| 页大小匹配 | `pgsize_bitmap & PAGE_SIZE` | SMMU 支持当前内核页大小 |
| PA 兼容 | `smmu->oas >= cpu_oas` | SMMU 输出地址不能小于 CPU |
| ASID 兼容 | `smmu->asid_bits >= cpu_asid_bits` | SMMU ASID 空间足够 |

### 1.5 SVA 域分配：arm_smmu_sva_domain_alloc (`arm-smmu-v3-sva.c:304`)

```c
// arm-smmu-v3-sva.c:304-347
struct iommu_domain *arm_smmu_sva_domain_alloc(struct device *dev,
                                               struct mm_struct *mm)
{
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    struct arm_smmu_device *smmu = master->smmu;
    struct arm_smmu_domain *smmu_domain;
    u32 asid;
    int ret;

    smmu_domain = arm_smmu_domain_alloc();
    smmu_domain->domain.type = IOMMU_DOMAIN_SVA;
    smmu_domain->domain.ops = &arm_smmu_sva_domain_ops;
    smmu_domain->stage = ARM_SMMU_DOMAIN_SVA;
    smmu_domain->smmu = smmu;

    // 分配 ASID
    ret = xa_alloc(&arm_smmu_asid_xa, &asid, smmu_domain,
                   XA_LIMIT(1, (1 << smmu->asid_bits) - 1), GFP_KERNEL);
    smmu_domain->cd.asid = asid;

    // 注册 mmu_notifier: 当 CPU 修改页表时通知 SMMU 无效化 TLB
    smmu_domain->mmu_notifier.ops = &arm_smmu_mmu_notifier_ops;
    ret = mmu_notifier_register(&smmu_domain->mmu_notifier, mm);

    return &smmu_domain->domain;
}
```

**SVA 域的生命周期与普通域的关键区别**：
- 使用 `mmu_notifier` 监听 CPU 侧的页表变更
- ASID 不随 domain 释放而立即回收，而是等到 `mmu_notifier->free_notifier` 回调
- 域类型是 `IOMMU_DOMAIN_SVA` 而非 `IOMMU_DOMAIN_UNMANAGED`

### 1.6 绑定 PASID：arm_smmu_sva_set_dev_pasid (`arm-smmu-v3-sva.c:248`)

```c
// arm-smmu-v3-sva.c:248-273
static int arm_smmu_sva_set_dev_pasid(struct iommu_domain *domain,
                                      struct device *dev, ioasid_t id,
                                      struct iommu_domain *old)
{
    struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    struct arm_smmu_cd target;

    if (!(master->smmu->features & ARM_SMMU_FEAT_SVA))
        return -EOPNOTSUPP;

    // 获取 mm 引用，防止进程在附着期间退出
    if (!mmget_not_zero(domain->mm))
        return -EINVAL;

    arm_smmu_make_sva_cd(&target, master, domain->mm,
                         smmu_domain->cd.asid);
    ret = arm_smmu_set_pasid(master, smmu_domain, id, &target, old);
    mmput(domain->mm);
    return ret;
}
```

### 1.7 MMU Notifier 钩子 (`arm-smmu-v3-sva.c:125-185`)

```c
// arm-smmu-v3-sva.c:125-142
static void arm_smmu_mm_arch_invalidate_secondary_tlbs(
    struct mmu_notifier *mn, struct mm_struct *mm,
    unsigned long start, unsigned long end)
{
    struct arm_smmu_domain *smmu_domain =
        container_of(mn, struct arm_smmu_domain, mmu_notifier);

    size = end - start;   // mm_types 使用 [start, end) 半开区间
    arm_smmu_domain_inv_range(smmu_domain, start, size, PAGE_SIZE, false);
}

// arm-smmu-v3-sva.c:144-173
static void arm_smmu_mm_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
    // 进程退出: 禁用翻译但保留 CD 有效 (防止 C_BAD_CD)
    //   → 对每个 master 写入 mm==NULL 的 CD (EPD0=1)
    //   → 最终所有 DMA 将 abort
    list_for_each_entry(master_domain, &smmu_domain->devices, devices_elm) {
        arm_smmu_make_sva_cd(&target, master, NULL, smmu_domain->cd.asid);
        arm_smmu_write_cd_entry(master, master_domain->ssid, cdptr, &target);
    }
    arm_smmu_domain_inv(smmu_domain);
}

static void arm_smmu_mmu_notifier_free(struct mmu_notifier *mn)
{
    arm_smmu_domain_free(
        container_of(mn, struct arm_smmu_domain, mmu_notifier));
}
```

**关键安全设计**：`mm_release` 不直接释放 ASID 或 CD，而是先将 CD 设为 EPD0（禁止翻译）状态，确保飞行中的 DMA 仍能完成（以 abort 方式），然后在 `free_notifier` 回调中才释放 domain。

### 1.8 SVA 域释放的 ASID 竞态处理 (`arm-smmu-v3-sva.c:275`)

```c
static void arm_smmu_sva_domain_free(struct iommu_domain *domain)
{
    // 先将 SMMU 中该 ASID 的 TLB 全部无效化，防止 ASID 重用后命中旧缓存
    arm_smmu_domain_inv(smmu_domain);

    // 此时 arm_smmu_mm_arch_invalidate_secondary_tlbs 可能仍在运行
    // 允许 ASID 立即重用 — 如果发生竞态，至多产生无害的额外无效化
    xa_erase(&arm_smmu_asid_xa, smmu_domain->cd.asid);

    // 实际释放延迟到 SRCU 回调: arm_smmu_mmu_notifier_free()
    mmu_notifier_put(&smmu_domain->mmu_notifier);
}
```

**消除 ASID 重用后的 TLB 污染**：通过在回收前发出全局域无效化，确保任何飞行中的页表遍历在 ASID 重用前完成。

## 2. IOMMUFD 嵌套翻译：arm-smmu-v3-iommufd.c

### 2.1 嵌套翻译架构

iommufd 将 SMMUv3 的 Stage-1 页表管理暴露给用户态（如 QEMU/VMM），内核只控制 Stage-2：

```
┌─────────────────────────────────────────────────────────┐
│  用户态 (QEMU/VMM)                                       │
│  ┌───────────────────────────────────────────────────┐  │
│  │  虚拟机 (Guest VM)                                 │  │
│  │  ┌────────┐                                       │  │
│  │  │ 客户机  │ ─── 管理 S1 页表 (GVA→GPA)            │  │
│  │  │ 内核    │     通过 IOMMUFD INVALIDATE 命令      │  │
│  │  │        │     通过 IOMMUFD VEVENTQ 接收事件      │  │
│  │  └────────┘                                       │  │
│  └───────────────────────────────────────────────────┘  │
│                         │                                │
│     iommufd /dev/iommu  │                                │
│                         │                                │
├─────────────────────────┼────────────────────────────────┤
│  内核态                 │                                │
│  ┌──────────────────────▼────────────────────────────┐  │
│  │  arm_smmu_vsmmu                                     │  │
│  │  ┌───────────────────────────────────────────────┐ │  │
│  │  │  S1 页表 (用户态管理)                          │ │  │
│  │  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐                │ │  │
│  │  │  │PGD │→│PUD │→│PMD │→│PTE │→ IPA           │ │  │
│  │  │  └────┘ └────┘ └────┘ └────┘                │ │  │
│  │  └──────────────────────┬────────────────────────┘ │  │
│  │                         │ GPA                      │  │
│  │                         ▼                           │  │
│  │  ┌───────────────────────────────────────────────┐ │  │
│  │  │  S2 页表 (内核管理)                            │ │  │
│  │  │  IPA → PA     (VMID 隔离)                     │ │  │
│  │  └───────────────────────────────────────────────┘ │  │
│  └────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 2.2 硬件信息暴露：arm_smmu_hw_info (`arm-smmu-v3-iommufd.c:10`)

```c
// arm-smmu-v3-iommufd.c:10-40
void *arm_smmu_hw_info(struct device *dev, u32 *length,
                       enum iommu_hw_info_type *type)
{
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    struct iommu_hw_info_arm_smmuv3 *info;
    u32 __iomem *base_idr;
    unsigned int i;

    // 厂商特定类型? 委托给 impl_ops->hw_info
    if (*type != IOMMU_HW_INFO_TYPE_DEFAULT &&
        *type != IOMMU_HW_INFO_TYPE_ARM_SMMUV3) {
        if (!impl_ops || !impl_ops->hw_info)
            return ERR_PTR(-EOPNOTSUPP);
        return impl_ops->hw_info(master->smmu, length, type);
    }

    info = kzalloc_obj(*info);

    // 读取 SMMU ID 寄存器 (IDR0-5, IIDR, AIDR)
    base_idr = master->smmu->base + ARM_SMMU_IDR0;
    for (i = 0; i <= 5; i++)
        info->idr[i] = readl_relaxed(base_idr + i);
    info->iidr = readl_relaxed(master->smmu->base + ARM_SMMU_IIDR);
    info->aidr = readl_relaxed(master->smmu->base + ARM_SMMU_AIDR);

    *length = sizeof(*info);
    *type = IOMMU_HW_INFO_TYPE_ARM_SMMUV3;
    return info;
}
```

**IOMMUFD hw_info 的作用**：将 SMMU 的 ID 寄存器值原样暴露给用户态，使得 VMM 可以了解 SMMU 支持的页大小、ASID 位宽、Stage-1 能力等信息。

### 2.3 虚拟 SMMU 初始化：arm_vsmmu_init (`arm-smmu-v3-iommufd.c:445`)

```c
// arm-smmu-v3-iommufd.c:445-468
int arm_vsmmu_init(struct iommufd_viommu *viommu,
                   struct iommu_domain *parent_domain,
                   const struct iommu_user_data *user_data)
{
    struct arm_vsmmu *vsmmu = container_of(viommu, struct arm_vsmmu, core);
    struct arm_smmu_device *smmu =
        container_of(viommu->iommu_dev, struct arm_smmu_device, iommu);
    struct arm_smmu_domain *s2_parent = to_smmu_domain(parent_domain);

    if (s2_parent->smmu != smmu)
        return -EINVAL;

    vsmmu->smmu = smmu;
    vsmmu->s2_parent = s2_parent;
    vsmmu->vmid = s2_parent->s2_cfg.vmid;  // 复用 S2 父域的 VMID

    if (viommu->type == IOMMU_VIOMMU_TYPE_ARM_SMMUV3) {
        viommu->ops = &arm_vsmmu_ops;
        return 0;
    }

    // 厂商特定类型 (如 Tegra241): 委托给 impl_ops->vsmmu_init
    return smmu->impl_ops->vsmmu_init(vsmmu, user_data);
}
```

### 2.4 嵌套域分配：arm_vsmmu_alloc_domain_nested (`arm-smmu-v3-iommufd.c:243`)

```c
// arm-smmu-v3-iommufd.c:243-276
struct iommu_domain *
arm_vsmmu_alloc_domain_nested(struct iommufd_viommu *viommu, u32 flags,
                              const struct iommu_user_data *user_data)
{
    struct arm_vsmmu *vsmmu = container_of(viommu, struct arm_vsmmu, core);
    struct arm_smmu_nested_domain *nested_domain;
    struct iommu_hwpt_arm_smmuv3 arg;
    bool enable_ats = false;
    int ret;

    // 从用户态接收虚拟 STE (仅 STE[0] 和 STE[1])
    ret = iommu_copy_struct_from_user(&arg, user_data,
                                      IOMMU_HWPT_DATA_ARM_SMMUV3, ste);

    // 验证虚拟 STE 合法性
    ret = arm_smmu_validate_vste(&arg, &enable_ats);

    nested_domain = kzalloc_obj(*nested_domain, GFP_KERNEL_ACCOUNT);
    nested_domain->domain.type = IOMMU_DOMAIN_NESTED;
    nested_domain->domain.ops = &arm_smmu_nested_ops;
    nested_domain->enable_ats = enable_ats;      // 用户态提供的 EATS
    nested_domain->vsmmu = vsmmu;
    nested_domain->ste[0] = arg.ste[0];          // 存储用户态虚拟 STE
    nested_domain->ste[1] = arg.ste[1] & ~cpu_to_le64(STRTAB_STE_1_EATS);

    return &nested_domain->domain;
}
```

### 2.5 虚拟 STE 验证：arm_smmu_validate_vste (`arm-smmu-v3-iommufd.c:207`)

```c
// arm-smmu-v3-iommufd.c:207-240
static int arm_smmu_validate_vste(struct iommu_hwpt_arm_smmuv3 *arg,
                                  bool *enable_ats)
{
    // 不合法 STE (V=0) → 自动转为 abort
    if (!(arg->ste[0] & cpu_to_le64(STRTAB_STE_0_V))) {
        memset(arg->ste, 0, sizeof(arg->ste));
        return 0;
    }

    // 检查保留位: 仅允许 NESTING_ALLOWED 掩码中的位
    if ((arg->ste[0] & ~STRTAB_STE_0_NESTING_ALLOWED) ||
        (arg->ste[1] & ~STRTAB_STE_1_NESTING_ALLOWED))
        return -EIO;

    // 只允许 ABORT / BYPASS / S1_TRANS 三种 CFG
    cfg = FIELD_GET(STRTAB_STE_0_CFG, le64_to_cpu(arg->ste[0]));
    if (cfg != STRTAB_STE_0_CFG_ABORT &&
        cfg != STRTAB_STE_0_CFG_BYPASS &&
        cfg != STRTAB_STE_0_CFG_S1_TRANS)
        return -EIO;

    // EATS: 仅允许 ABT 或 TRANS
    eats = FIELD_GET(STRTAB_STE_1_EATS, le64_to_cpu(arg->ste[1]));
    if (eats != STRTAB_STE_1_EATS_ABT && eats != STRTAB_STE_1_EATS_TRANS)
        return -EIO;

    if (cfg == STRTAB_STE_0_CFG_S1_TRANS)
        *enable_ats = (eats == STRTAB_STE_1_EATS_TRANS);
    return 0;
}
```

**允许的用户态 STE 字段** (`arm-smmu-v3.h:304-310`)：

```c
#define STRTAB_STE_0_NESTING_ALLOWED \
    cpu_to_le64(STRTAB_STE_0_V | STRTAB_STE_0_CFG | STRTAB_STE_0_S1FMT | \
                STRTAB_STE_0_S1CTXPTR_MASK | STRTAB_STE_0_S1CDMAX)
#define STRTAB_STE_1_NESTING_ALLOWED \
    cpu_to_le64(STRTAB_STE_1_S1DSS | STRTAB_STE_1_S1CIR | \
                STRTAB_STE_1_S1COR | STRTAB_STE_1_S1CSH | \
                STRTAB_STE_1_S1STALLD | STRTAB_STE_1_EATS)
```

用户态可以通过 `S1CTXPTR` 指向它自己的 CD 表，从而完全自主地管理 Stage-1 页表。

### 2.6 物理 STE 构造：arm_smmu_make_nested_domain_ste (`arm-smmu-v3-iommufd.c:67`)

```c
// arm-smmu-v3-iommufd.c:67-97
static void arm_smmu_make_nested_domain_ste(
    struct arm_smmu_ste *target, struct arm_smmu_master *master,
    struct arm_smmu_nested_domain *nested_domain, bool ats_enabled)
{
    unsigned int cfg =
        FIELD_GET(STRTAB_STE_0_CFG, le64_to_cpu(nested_domain->ste[0]));

    if (!(nested_domain->ste[0] & cpu_to_le64(STRTAB_STE_0_V)))
        cfg = STRTAB_STE_0_CFG_ABORT;

    switch (cfg) {
    case STRTAB_STE_0_CFG_S1_TRANS:
        // 用户态请求翻译: 设置 S1FMT/S1CTXPTR + S2
        arm_smmu_make_nested_cd_table_ste(target, master,
                                          nested_domain, ats_enabled);
        break;
    case STRTAB_STE_0_CFG_BYPASS:
        // 用户态请求 bypass: 只安装 S2 (直通到 PA)
        arm_smmu_make_s2_domain_ste(target, master,
                                    nested_domain->vsmmu->s2_parent,
                                    ats_enabled);
        break;
    case STRTAB_STE_0_CFG_ABORT:
    default:
        // 用户态请求 abort: 安装 abort STE
        arm_smmu_make_abort_ste(target);
        break;
    }
}
```

### 2.7 嵌套 CD 表 STE 构造 (`arm-smmu-v3-iommufd.c:42`)

```c
static void arm_smmu_make_nested_cd_table_ste(
    struct arm_smmu_ste *target, struct arm_smmu_master *master,
    struct arm_smmu_nested_domain *nested_domain, bool ats_enabled)
{
    // 先设置 S2 参数 (来自 vsmmu->s2_parent)
    arm_smmu_make_s2_domain_ste(
        target, master, nested_domain->vsmmu->s2_parent, ats_enabled);

    // 覆盖 CFG 为 NESTED + 使用用户态的 S1 参数
    target->data[0] = cpu_to_le64(STRTAB_STE_0_V |
                                  FIELD_PREP(STRTAB_STE_0_CFG,
                                             STRTAB_STE_0_CFG_NESTED));
    target->data[0] |= nested_domain->ste[0] & ~cpu_to_le64(STRTAB_STE_0_CFG);
    target->data[1] |= nested_domain->ste[1];
    target->data[1] |= cpu_to_le64(STRTAB_STE_1_MEV);  // 合并事件
}
```

**MEV（Merge Events）的作用**：防止 guest 恶意发送大量 event 导致 EVTQ 溢出 DoS 攻击。合并相同 SID 的连续事件。

### 2.8 嵌套 attach：arm_smmu_attach_dev_nested (`arm-smmu-v3-iommufd.c:151`)

```c
static int arm_smmu_attach_dev_nested(struct iommu_domain *domain,
                                      struct device *dev,
                                      struct iommu_domain *old_domain)
{
    struct arm_smmu_nested_domain *nested_domain =
        to_smmu_nested_domain(domain);
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    struct arm_smmu_attach_state state = { ... };
    struct arm_smmu_ste ste;

    // 安全检查: nVIommu 的 SMMU 必须与设备的 SMMU 一致
    if (nested_domain->vsmmu->smmu != master->smmu)
        return -EINVAL;

    // 不能与使用 SSID 的 S1 attach 共存
    if (arm_smmu_ssids_in_use(&master->cd_table))
        return -EBUSY;

    mutex_lock(&arm_smmu_asid_lock);

    // ATS 状态由用户态 EATS 字段决定
    if (FIELD_GET(STRTAB_STE_0_CFG, le64_to_cpu(nested_domain->ste[0])) ==
        STRTAB_STE_0_CFG_S1_TRANS)
        state.disable_ats = !nested_domain->enable_ats;

    ret = arm_smmu_attach_prepare(&state, domain);

    // 构造物理 STE
    arm_smmu_make_nested_domain_ste(&ste, master, nested_domain,
                                    state.ats_enabled);
    arm_smmu_install_ste_for_dev(master, &ste);
    arm_smmu_attach_commit(&state);
    mutex_unlock(&arm_smmu_asid_lock);
    return 0;
}
```

### 2.9 虚拟事件上报：arm_vmaster_report_event (`arm-smmu-v3-iommufd.c:470`)

```c
int arm_vmaster_report_event(struct arm_smmu_vmaster *vmaster, u64 *evt)
{
    struct iommu_vevent_arm_smmuv3 vevt;
    int i;

    lockdep_assert_held(&vmaster->vsmmu->smmu->streams_mutex);

    // 将物理 SID 替换为虚拟 SID (vsid)
    vevt.evt[0] = cpu_to_le64((evt[0] & ~EVTQ_0_SID) |
                              FIELD_PREP(EVTQ_0_SID, vmaster->vsid));
    for (i = 1; i < EVTQ_ENT_DWORDS; i++)
        vevt.evt[i] = cpu_to_le64(evt[i]);

    return iommufd_viommu_report_event(&vmaster->vsmmu->core,
                                       IOMMU_VEVENTQ_TYPE_ARM_SMMUV3,
                                       &vevt, sizeof(vevt));
}
```

**关键转换**：物理 SID 被替换为 vsid（虚拟 StreamID），确保虚拟化环境中的安全隔离。

### 2.10 缓存无效化：arm_vsmmu_cache_invalidate (`arm-smmu-v3-iommufd.c:353`)

```c
int arm_vsmmu_cache_invalidate(struct iommufd_viommu *viommu,
                               struct iommu_user_data_array *array)
{
    struct arm_vsmmu *vsmmu = container_of(viommu, struct arm_vsmmu, core);
    struct arm_smmu_device *smmu = vsmmu->smmu;

    // 从用户态获取无效化命令数组
    cmds = kzalloc_objs(*cmds, array->entry_num);
    ret = iommu_copy_struct_from_full_user_array(
        cmds, sizeof(*cmds), array,
        IOMMU_VIOMMU_INVALIDATE_DATA_ARM_SMMUV3);

    while (cur != end) {
        arm_vsmmu_convert_user_cmd(vsmmu, cur);

        cur++;
        if (cur != end && (cur - last) != CMDQ_BATCH_ENTRIES - 1)
            continue;

        // 批量提交到 SMMU CMDQ
        ret = arm_smmu_cmdq_issue_cmdlist(smmu, &smmu->cmdq,
                                          last->cmd, cur - last, true);
        last = cur;
    }
    ...
}
```

**命令转换函数** `arm_vsmmu_convert_user_cmd` (`arm-smmu-v3-iommufd.c:315`):

```c
static int arm_vsmmu_convert_user_cmd(struct arm_vsmmu *vsmmu,
                                      struct arm_vsmmu_invalidation_cmd *cmd)
{
    cmd->cmd[0] = le64_to_cpu(cmd->ucmd.cmd[0]);
    cmd->cmd[1] = le64_to_cpu(cmd->ucmd.cmd[1]);

    switch (cmd->cmd[0] & CMDQ_0_OP) {
    case CMDQ_OP_TLBI_NSNH_ALL:
        // 转换为 NH_ALL 并注入 VMID
        cmd->cmd[0] = CMDQ_OP_TLBI_NH_ALL |
                      FIELD_PREP(CMDQ_TLBI_0_VMID, vsmmu->vmid);
        cmd->cmd[1] = 0;
        break;
    case CMDQ_OP_TLBI_NH_VA:
    case CMDQ_OP_TLBI_NH_VAA:
    case CMDQ_OP_TLBI_NH_ALL:
    case CMDQ_OP_TLBI_NH_ASID:
        // 强制注入正确 VMID
        cmd->cmd[0] &= ~CMDQ_TLBI_0_VMID;
        cmd->cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, vsmmu->vmid);
        break;
    case CMDQ_OP_ATC_INV:
    case CMDQ_OP_CFGI_CD:
    case CMDQ_OP_CFGI_CD_ALL:
        // 转换虚拟 SID 为物理 SID
        arm_vsmmu_vsid_to_sid(vsmmu, vsid, &sid);
        cmd->cmd[0] &= ~CMDQ_CFGI_0_SID;
        cmd->cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SID, sid);
        break;
    default:
        return -EIO;
    }
    return 0;
}
```

## 3. Tegra241 CMDQ-V：虚拟命令队列

### 3.1 架构概述

NVIDIA Tegra241 提供了 CMDQV (Command Queue Virtualization) 扩展，允许为每个虚拟机分配独立的虚拟命令队列（VCMDQ）：

```
┌─────────────────────────────────────────────────────────┐
│              Tegra241 CMDQV 架构                          │
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ VINTF 0  │  │ VINTF 1  │  │ VINTF N  │  ← 虚拟接口  │
│  │ (内核)   │  │ (VM 1)   │  │ (VM N)   │              │
│  │          │  │          │  │          │              │
│  │ LVCMDQ_0 │  │ LVCMDQ_0 │  │ LVCMDQ_0 │  ← 逻辑 VCMDQ│
│  │ LVCMDQ_1 │  │ LVCMDQ_1 │  │ LVCMDQ_1 │              │
│  │ ...      │  │ ...      │  │ ...      │              │
│  └──────────┘  └──────────┘  └──────────┘              │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │              物理 SMMU CMDQ                         │  │
│  │  (用于不支持 VCMDQ 的命令类型)                       │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 3.2 虚拟接口结构：linux/iommufd.h 集成

```c
// tegra241-cmdqv.c: tegra241_vintf 结构解析
// 每个 VINTF 对应一个 VM (物理上映射到 PCIe 虚拟函数)
// VINTF 内包含:
//   - SID match/replace 寄存器 (将虚拟 SID 映射到物理 SID)
//   - 多个 LVCMDQ (逻辑虚拟命令队列)
//   - VINTF 级别的错误报告

// 命令分发: tegra241_cmdqv_get_cmdq (行 382)
static struct arm_smmu_cmdq *
tegra241_cmdqv_get_cmdq(struct arm_smmu_device *smmu,
                        struct arm_smmu_cmdq_ent *ent)
{
    struct tegra241_cmdqv *cmdqv =
        container_of(smmu, struct tegra241_cmdqv, smmu);
    struct tegra241_vintf *vintf = cmdqv->vintfs[0];

    if (!READ_ONCE(vintf->enabled))
        return NULL;

    // 基于 CPU ID 负载均衡到不同 LVCMDQ
    lidx = raw_smp_processor_id() % cmdqv->num_lvcmdqs_per_vintf;
    vcmdq = vintf->lvcmdqs[lidx];

    // 不支持的 CMD → 回退到主 SMMU CMDQ
    if (!arm_smmu_cmdq_supports_cmd(&vcmdq->cmdq, ent))
        return NULL;
    return &vcmdq->cmdq;
}
```

### 3.3 VCMDQ 支持的命令 (`tegra241-cmdqv.c:370`)

```c
static bool tegra241_guest_vcmdq_supports_cmd(struct arm_smmu_cmdq_ent *ent)
{
    switch (ent->opcode) {
    case CMDQ_OP_TLBI_NH_ASID:   // TLB invalidate by ASID
    case CMDQ_OP_TLBI_NH_VA:     // TLB invalidate by VA
    case CMDQ_OP_ATC_INV:        // ATC invalidate
        return true;
    default:
        return false;
    }
}
```

只有可以与虚拟化环境安全执行的命令才下放到 VCMDQ。配置相关命令（CFGI_STE、CFGI_CD）始终由主 CMDQ 处理。

### 3.4 impl_ops 集成 (`tegra241-cmdqv.c:843`)

```c
static struct arm_smmu_impl_ops tegra241_cmdqv_impl_ops = {
    /* 内核使用 */
    .get_secondary_cmdq = tegra241_cmdqv_get_cmdq,  // CMDQ 分发
    .device_reset       = tegra241_cmdqv_hw_reset,
    .device_remove      = tegra241_cmdqv_remove,
    /* 用户态使用 */
    .hw_info            = tegra241_cmdqv_hw_info,      // 暴露 VCMDQ 参数
    .get_viommu_size    = tegra241_cmdqv_get_vintf_size,
    .vsmmu_init         = tegra241_cmdqv_init_vintf_user, // 为用户态初始化 VINTF
};
```

`tegra241_cmdqv_hw_info` (行 811) 向用户态暴露逻辑 VCMDQ 数量和虚拟 SID 数量，使 VMM 可以为每个 VM 分配 VINTF。

### 3.5 VCMDQ 超时刷新机制 (`tegra241-cmdqv.c:427`)

```c
static void tegra241_vcmdq_hw_flush_timeout(struct tegra241_vcmdq *vcmdq)
{
    // 当 VM 禁用了 VCMDQ 时，通过主 CMDQ 发送 CMD_SYNC 清理超时
    // 防止 ATC_INV 超时在新 VM 接管同一 VCMDQ 后误报
    arm_smmu_cmdq_issue_cmdlist(smmu, &smmu->cmdq, cmd_sync, 1, true);
}
```

### 3.6 Probe 入口 (`tegra241-cmdqv.c:893`)

```c
static struct arm_smmu_device *
__tegra241_cmdqv_probe(struct arm_smmu_device *smmu,
                       struct resource *res, int irq)
{
    static const struct arm_smmu_impl_ops init_ops = {
        .init_structures = tegra241_cmdqv_init_structures,
        .device_remove = tegra241_cmdqv_remove,
    };

    // 映射 CMDQV MMIO 空间
    base = ioremap(res->start, resource_size(res));

    // 读取 PARAM 获取 VINTF/VCMDQ 数量
    regval = readl_relaxed(REG_CMDQV(cmdqv, PARAM));
    cmdqv->num_vintfs = 1 << FIELD_GET(CMDQV_NUM_VINTF_LOG2, regval);
    cmdqv->num_vcmdqs = 1 << FIELD_GET(CMDQV_NUM_VCMDQ_LOG2, regval);
    cmdqv->num_lvcmdqs_per_vintf = cmdqv->num_vcmdqs / cmdqv->num_vintfs;

    // devm_krealloc: 将原始 arm_smmu_device 扩展为 tegra241_cmdqv
    cmdqv = devm_krealloc(smmu->dev, smmu, sizeof(*cmdqv), GFP_KERNEL);
    new_smmu = &cmdqv->smmu;

    // 设置初始 impl_ops (仅 init_structures)
    new_smmu->impl_ops = &init_ops;
    return new_smmu;
}
```

**`static_assert(offsetof(struct tegra241_cmdqv, smmu) == 0)`** 保证 `devm_krealloc` 安全——`arm_smmu_device` 必须是 `tegra241_cmdqv` 的第一个字段。

## 4. viommu 大小检查与安全限制 (`arm-smmu-v3-iommufd.c:408`)

```c
size_t arm_smmu_get_viommu_size(struct device *dev,
                                enum iommu_viommu_type viommu_type)
{
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    struct arm_smmu_device *smmu = master->smmu;

    // 必须支持 FEAT_NESTING
    if (!(smmu->features & ARM_SMMU_FEAT_NESTING))
        return 0;

    // FORCE_SYNC 不兼容嵌套
    if (WARN_ON(smmu->options & ARM_SMMU_OPT_CMDQ_FORCE_SYNC))
        return 0;

    // 安全条件: 必须能防止 VM 绕过 cache
    // canwbs: 设备完全 cache coherent, 无需维护
    // S2FWB:  S2 强制写回, S1 无法将内存改为 non-cacheable
    if (!arm_smmu_master_canwbs(master) &&
        !(smmu->features & ARM_SMMU_FEAT_S2FWB))
        return 0;

    if (viommu_type == IOMMU_VIOMMU_TYPE_ARM_SMMUV3)
        return VIOMMU_STRUCT_SIZE(struct arm_vsmmu, core);

    // 厂商特定类型
    if (!smmu->impl_ops || !smmu->impl_ops->get_viommu_size)
        return 0;
    return smmu->impl_ops->get_viommu_size(viommu_type);
}
```

## 5. SMMUv3 vs Intel VT-d 功能对比

| 特性 | ARM SMMUv3 | Intel VT-d |
|------|-----------|------------|
| **基本翻译** | StreamID → STE → CD → 页表 | Bus:Dev.Func → Context Entry → 页表 |
| **Stage-2** | 支持 (VMID) | 支持 (通过 EPT) |
| **SVA** | 支持 (PASID + CD) | 支持 (PASID + PASID Entry) |
| **嵌套翻译** | 支持 (iommufd, vSTE) | 支持 (iommufd) |
| **CMDQ** | 软件命令队列 | IOTLB 无效化寄存器 |
| **PRI/ATS** | 支持 | 支持 |
| **硬件脏位** | DBM (ARM_LPAE_PTE_DBM) | DBM (Access/Dirty for EPT) |
| **MSI** | 支持 | MSI 地址由 IOMMU 翻译 |
| **厂商扩展** | `impl_ops` + Tegra241 CMDQV | 无统一扩展接口 |
| **用户态控制** | IOMMUFD + vSTE | IOMMUFD + vContext |
| **S2FWB** | 支持 (force WB) | 无直接等价 |
| **BBML2** | 支持 (BBM level 2) | 无直接等价 |

## 6. UAPI 结构体

### iommu_hwpt_arm_smmuv3 (用户态 vSTE)

```c
// uapi/linux/iommufd.h: IOMMU_HWPT_DATA_ARM_SMMUV3
struct iommu_hwpt_arm_smmuv3 {
    __aligned_u64 ste[2]; // 仅暴露 STE[0] 和 STE[1], S2 由内核控制
};
```

### iommu_viommu_arm_smmuv3_invalidate (用户态无效化)

```c
// uapi/linux/iommufd.h: IOMMU_VIOMMU_INVALIDATE_DATA_ARM_SMMUV3
struct iommu_viommu_arm_smmuv3_invalidate {
    __aligned_u64 cmd[2]; // 原始 CMDQ 命令 (LE64)
};
```

### iommu_vevent_arm_smmuv3 (虚拟事件)

```c
// uapi/linux/iommufd.h: IOMMU_VEVENTQ_TYPE_ARM_SMMUV3
struct iommu_vevent_arm_smmuv3 {
    __aligned_u64 evt[4]; // 4 个 u64 (EVTQ 条目格式)
};
```

## 7. 总体调用栈

```
┌──────────────────────────────────────────────────────────┐
│                    SVA 调用栈                              │
│                                                          │
│  VFIO / 用户态                 内核态                     │
│  ────────────                  ──────                     │
│  VFIO_IOMMU_BIND                                               │
│      │                                                     │
│      ▼                                                     │
│  iommufd_device_bind()                                     │
│      │                                                     │
│      ├─► arm_smmu_sva_domain_alloc()    ← sva.c:304       │
│      │       ├─ arm_smmu_domain_alloc()                    │
│      │       ├─ xa_alloc(&arm_smmu_asid_xa)                │
│      │       └─ mmu_notifier_register()                    │
│      │                                                     │
│      └─► arm_smmu_sva_set_dev_pasid()  ← sva.c:248        │
│              ├─ arm_smmu_make_sva_cd()  ← sva.c:51        │
│              │     ├─ cpuid_feature_extract (PARANGE, ASID)│
│              │     ├─ virt_to_phys(mm->pgd) → TTB0         │
│              │     └─ read_sysreg(mair_el1) → MAIR          │
│              └─ arm_smmu_set_pasid()                       │
│                    └─ arm_smmu_write_cd_entry()            │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│                  IOMMUFD 嵌套调用栈                         │
│                                                          │
│  VFIO / 用户态                 内核态                     │
│  ────────────                  ──────                     │
│  IOMMUFD_CMD_HWPT_ALLOC                                         │
│      │                                                     │
│      ▼                                                     │
│  arm_vsmmu_alloc_domain_nested()       ← iommufd.c:243   │
│      ├─ iommu_copy_struct_from_user()   (接收用户态 vSTE)  │
│      ├─ arm_smmu_validate_vste()        ← iommufd.c:207  │
│      └─ 存储 ste[0..1] 到 nested_domain                    │
│                                                           │
│  attach: VFIO_DEVICE_ATTACH_IOMMUFD_PT                     │
│      │                                                     │
│      ▼                                                     │
│  arm_smmu_attach_dev_nested()          ← iommufd.c:151   │
│      ├─ arm_smmu_attach_prepare_vmaster() ← iommufd.c:99 │
│      ├─ arm_smmu_make_nested_domain_ste() ← iommufd.c:67 │
│      ├─ arm_smmu_install_ste_for_dev()                     │
│      └─ arm_smmu_attach_commit()                           │
│                                                           │
│  invalidate: IOMMUFD_CMD_HWPT_INVALIDATE                    │
│      │                                                     │
│      ▼                                                     │
│  arm_vsmmu_cache_invalidate()          ← iommufd.c:353   │
│      ├─ iommu_copy_struct_from_full_user_array()           │
│      ├─ arm_vsmmu_convert_user_cmd()   ← iommufd.c:315   │
│      └─ arm_smmu_cmdq_issue_cmdlist()                      │
│                                                           │
│  event: SMMU EVTQ 中断                                      │
│      │                                                     │
│      ▼                                                     │
│  arm_smmu_evtq_thread()                                     │
│      └─ arm_smmu_handle_event()                             │
│            └─ arm_vmaster_report_event() ← iommufd.c:470  │
│                  └─ iommufd_viommu_report_event()           │
└──────────────────────────────────────────────────────────┘
```

## 系列结语

本系列四篇文章覆盖了 ARM SMMUv3 驱动从硬件架构到高级虚拟化特性的完整技术实现：

1. **架构全景**：SMMU 三代演进、`arm_smmu_device` 核心数据结构和 `arm_smmu_device_probe` 初始化流程
2. **流表与队列**：STE/CD 硬件格式、CMDQ 并发控制算法、EVTQ/PRIQ 事件处理
3. **页表引擎**：ARM LPAE 页表格式、4 级页表遍历、Stage-1/2 配置差异、Map/Unmap 委托链
4. **SVA 与 IOMMUFD**：SVA 共享页表的 CD 构造、嵌套翻译的虚拟 STE、Tegra241 CMDQ-V

ARM SMMUv3 驱动的设计展现了现代 IOMMU 驱动的几个核心原则：
- **硬件格式直映**：STE/CD 结构与 SMMU 规范一一对应，避免翻译开销
- **Hitsless 更新**：在不停止 DMA 的情况下安全更新 STE/CD
- **RCU + 引用计数**：`arm_smmu_invs` 使用 RCU 保护并发访问
- **命令批处理**：减少 CMDQ 锁竞争
- **厂商扩展**：`impl_ops` 机制提供干净的扩展点

系列源码总计约 11,000 行，涵盖 SMMUv3 驱动的所有核心路径。
