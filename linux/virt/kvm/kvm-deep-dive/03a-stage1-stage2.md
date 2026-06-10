# KVM Stage 1 vs Stage 2 页表 — 两级地址转换深度解析

> 源码：`arch/arm64/kvm/mmu.c` `arch/x86/kvm/mmu/mmu.c` `arch/x86/kvm/mmu/tdp_mmu.c` `arch/arm64/kvm/hyp/vhe/switch.c` | 头文件：`include/asm/kvm_host.h` `include/asm/kvm_mmu.h`

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 1. 前置概念：为什么需要两级页表

在虚拟化中，guest 看不到 host 物理地址。guest 的"物理地址"(GPA)对 host 来说只是另一个虚拟地址。这需要**两层翻译**：

```
Guest VA (GVA)  ──→  Guest PA (GPA)  ──→  Host PA (HPA)
                  Stage-1 (Guest 页表)     Stage-2 (Hypervisor 页表)
                  由 guest OS 管理         由 KVM 管理，guest 不可见
```

这个 Stage-1/Stage-2 的概念和 IOMMU 系列中的 SMMU Stage-1/Stage-2 是同一个概念——都是"两层翻译"。在 x86 上叫 EPT/NPT，在 ARM64 上叫 Stage-2，本质上做的事情完全一样：

| | x86 (Intel VMX) | x86 (AMD SVM) | ARM64 VHE/nVHE |
|---|---|---|---|
| **Stage-1** | Guest CR3 → GPA | Guest CR3 → GPA | Guest TTBR0_EL1 → IPA |
| **Stage-2** | EPT (Extended Page Tables) | NPT (Nested Page Tables) | VTTBR_EL2 → PA |
| **Stage-2 基址寄存器** | EPT pointer (VMCS) | nCR3 (VMCB) | VTTBR_EL2 |
| **Stage-2 缺页异常** | EPT Violation → VM-Exit | #NPF → VM-Exit | Stage-2 fault → trap to EL2 |

---

## 2. ARM64 Stage-2 实现

### 2.1 数据结构：`struct kvm_s2_mmu`

ARM64 的 Stage-2 MMU 是一个独立的 `struct kvm_s2_mmu`（`arch/arm64/include/asm/kvm_host.h:153-235`），嵌入在 `kvm_arch` 中：

```c
// kvm_host.h:153-235
struct kvm_s2_mmu {
    struct kvm_vmid vmid;                    // 虚拟 VMID
    phys_addr_t pgd_phys;                    // Stage-2 PGD 物理地址
    struct kvm_pgtable *pgt;                 // 指向 page table 结构的指针
    u64 vtcr;                                // VTCR_EL2 值：页大小、IPA 宽度等
    int __percpu *last_vcpu_ran;             // 每 CPU 上次运行的 vCPU
    struct kvm_mmu_memory_cache split_page_cache;  // 用于 eager split 的页缓存
    struct kvm_arch *arch;                   // 回指 kvm_arch
    u64 tlb_vttbr;                           // TLB 无效化用的缓存 VTTBR
    u64 tlb_vtcr;                            // TLB 无效化用的缓存 VTCR
    bool nested_stage2_enabled;              // 嵌套 S2 是否启用
    atomic_t refcnt;                         // 引用计数
};
```

VM 创建时通过 `kvm_arch.kvm_arch_init_vm()` → `kvm_init_stage2_mmu()` 分配（`arm.c:208 → arm.c:237 → mmu.c:954`）：

```c
// arm.c:208-237 — VM 创建初始化
int kvm_arch_init_vm(struct kvm *kvm, unsigned long type)
{
    ...
    ret = kvm_init_stage2_mmu(kvm, &kvm->arch.mmu, type);  // line 237
    ...
}

// mmu.c:954-1021 — 分配并初始化 Stage-2 MMU
int kvm_init_stage2_mmu(struct kvm *kvm, struct kvm_s2_mmu *mmu, unsigned long type)
{
    // 分配 VMID
    ret = kvm_arm_vmid_alloc_init(&mmu->vmid);      // line 969
    ...
    // ★ 调用 stage2 page table 初始化
    ret = kvm_pgtable_stage2_init(&pgt, &mmu->pgt,  // line 986
                                  kvm->arch, &kvm_s2_mm_ops);
    ...
    mmu->pgd_phys = virt_to_phys(pgt->pgd);         // line 1000
    mmu->arch = kvm->arch;
}
```

### 2.2 加载 Stage-2 到硬件

每次 vCPU 进入 guest 前，必须把 Stage-2 页表基地址写入硬件寄存器 VTTBR_EL2：

```c
// include/asm/kvm_mmu.h:321 — 硬件切换宏
static inline void __load_stage2(struct kvm_s2_mmu *mmu, struct kvm_arch *arch)
{
    write_sysreg(mmu->pgd_phys, vttbr_el2);  // 写 VTTBR_EL2
    write_sysreg(mmu->vtcr, vtcr_el2);       // 写 VTCR_EL2（页大小等配置）
    isb();
}
```

**VHE 路径**中由 `kvm_vcpu_load_vhe()` 调用（`hyp/vhe/switch.c:216-222`）：

```c
// hyp/vhe/switch.c:216-224
void kvm_vcpu_load_vhe(struct kvm_vcpu *vcpu)
{
    ...
    __load_stage2(vcpu->arch.hw_mmu, vcpu->arch.hw_mmu->arch);  // line 222
    ...
}
```

**nVHE 路径**中由 `__kvm_vcpu_run()` 调用（`hyp/nvhe/switch.c:258-318`），在 restore guest sysregs 阶段加载 Stage-2。

### 2.3 Stage-2 Fault 处理

当 guest 访问一个还没有 Stage-2 映射的 GPA（或权限不足），硬件触发 abort，trap 到 EL2：

```
Guest 访问 GPA
  → MMU 查 Stage-1 (guest CR3) → 得到 IPA
  → MMU 查 Stage-2 (VTTBR_EL2) → IPA 无映射 → Abort
  → EL2 trap → handle_exit.c → kvm_handle_guest_abort
```

```c
// handle_exit.c:394-395 — Abort 分发
[ESR_ELx_EC_IABT_LOW] = kvm_handle_guest_abort,  // 指令 abort
[ESR_ELx_EC_DABT_LOW] = kvm_handle_guest_abort,  // 数据 abort
```

`kvm_handle_guest_abort` 在 `mmu.c:2215`，核心逻辑：

```c
// mmu.c:2215—2330 — 顶层 abort 处理
int kvm_handle_guest_abort(struct kvm_vcpu *vcpu)
{
    ...
    // 检查是否 IPA 还没有 Stage-2 映射
    if (fault_status == FSC_PERM && stage2_fault_handled) {
        ret = io_mem_abort(vcpu);       // MMIO 访问 (设备)
    } else {
        ret = user_mem_abort(vcpu);      // 普通内存 (RAM)
    }
    ...
}
```

`user_mem_abort`（`mmu.c:2072`）负责为 Stage-2 缺页分配 host 物理页并建立映射：

```c
// mmu.c:2072-2110 — Stage-2 缺页处理
static int user_mem_abort(struct kvm_vcpu *vcpu, ...)
{
    gfn = fault_ipa >> PAGE_SHIFT;           // 故障 GPA → GFN
    ...
    // 查找 host 物理页
    ret = kvm_s2_fault_pin_pfn(vcpu, fault_ipa, &pfn, ...);   // line 1871
    // 计算保护位
    prot = kvm_s2_fault_compute_prot(vcpu, fault_ipa, ...);   // line 1933
    // 建立 Stage-2 映射
    ret = kvm_s2_fault_map(vcpu, fault_ipa, pfn, prot, ...);  // line 1984
    ...
}
```

### 2.4 Stage-2 页表操作

ARM64 KVM 提供了完整的 Stage-2 管理 API（均在 `mmu.c`）：

| 函数 | 行号 | 用途 |
|------|------|------|
| `kvm_stage2_unmap_range()` | 340 | 解除 Stage-2 映射 |
| `kvm_stage2_flush_range()` | 349 | 刷新（clean+invalidate）Stage-2 范围 |
| `kvm_stage2_wp_range()` | 1222 | 写保护 Stage-2 内存范围 |
| `__unmap_stage2_range()` | 328 | 内部实现：清除 PTE |
| `kvm_free_stage2_pgd()` | 1098 | 释放整个 Stage-2 PGD |
| `stage2_unmap_vm()` | 1077 | 解除 VM 所有 Stage-2 RAM 映射 |
| `kvm_uninit_stage2_mmu()` | 1022 | 析构 Stage-2 MMU |

---

## 3. x86 EPT 实现对比

### 3.1 x86 Stage-1 vs Stage-2

在 x86 上，Stage-1 和 Stage-2 的使用情况取决于是否开启 TDP（Two-Dimensional Paging）：

| 模式 | Stage-1 | Stage-2 | 使用的硬件特性 |
|------|---------|---------|---------------|
| **Shadow Paging** | KVM 维护影子页表（GVA→HPA 直接映射） | 无 | CR3 (VMCS host-state) |
| **TDP (EPT)** | Guest CR3 → GPA | EPT (GPA → HPA) | EPT pointer (VMCS) |
| **TDP (NPT)** | Guest CR3 → GPA | NPT (GPA → HPA) | nCR3 (VMCB) |

### 3.2 EPT 页表结构

EPT 页表结构与 x86 64-bit 页表相同：4 级（5 级已支持 LA57），每个 entry 8 字节。EPT entry 包含：
- **物理地址**：HPA 的 bits [51:12]
- **权限位**：Read / Write / Execute（与常规页表不同，EPT 无 U/S 位）
- **内存类型**：PAT-like 属性（WB/WC）
- **Accessed / Dirty**：可选硬件跟踪

EPT Violation 触发 VM-Exit（错误码在 VMCS Exit Qualification 中），KVM 的 `tdp_page_fault` 处理流程：

```
Guest 访问 GPA → EPT 遍历 → Violation → VM-Exit
  → handle_ept_violation() [mmu.c]
    → kvm_mmu_page_fault()
      → try_async_pf()         // 尝试异步 page fault
      → direct_map()           // ★ 建立 EPT 映射
        → mmu_set_spte() [mmu.c:3051]
          → make_spte() [mmu.c:3100]  // 构造 SPTE
          → __set_spte() [mmu.c:339]  // 原子写入 SPTE
```

### 3.3 关键对比：ARM64 vs x86

| | ARM64 Stage-2 | x86 EPT |
|---|---|---|
| **初始化** | `kvm_init_stage2_mmu()` (mmu.c:954) | `init_kvm_tdp_mmu()` (tdp_mmu.c) |
| **基址寄存器** | VTTBR_EL2 + VTCR_EL2 | EPT pointer (VMCS) |
| **加载到硬件** | `__load_stage2()` (kvm_mmu.h:321) | VM-entry 时自动加载 |
| **缺页 entry** | `handle_exit` → `kvm_handle_guest_abort` (handle_exit.c:394) → `user_mem_abort` (mmu.c:2072) | `handle_ept_violation` → `kvm_mmu_page_fault` → `direct_map` |
| **映射安装** | `kvm_s2_fault_map` (mmu.c:1984) | `mmu_set_spte` → `__set_spte` (mmu.c:339/487) |
| **unmap** | `kvm_stage2_unmap_range` (mmu.c:340) | `tdp_mmu_zap_*` 系列 |
| **写保护** | `kvm_stage2_wp_range` (mmu.c:1222) | `tdp_mmu_wrprot_*` 系列 |
| **页表格式** | ARM LPAE (同 SMMU Stage-1) | x86 64-bit 页表 (同 CPU 页表) |

---

## 4. 完整的地址转换流程

以 ARM64 VHE 为例，一次 guest 内存访问的完整路径：

```
Guest 执行:  ldr x0, [x1]       ← 虚拟地址在 x1

  ① Stage-1 翻译 (Guest MMU):
     TTBR0_EL1 → PGD → PUD → PMD → PTE
     GVA → GPA (Intermediate Physical Address)

  ② Stage-2 翻译 (SMMU/KVM):
     VTTBR_EL2 → PGD → PUD → PMD → PTE  (LPAE 格式)
     GPA → HPA

  ┌── Stage-2 Hit (PTE 已有映射):
  │   直接访问 HPA → 完成
  │
  └── Stage-2 Miss (PTE 无映射):
       硬件 Abort → EL2 trap
         → ESR_EL2.EC = 0x24/0x25 (DABT_LOW/IABT_LOW)
         → arm_exit_handlers[EC] → kvm_handle_guest_abort
           → user_mem_abort (mmu.c:2072)
             → kvm_s2_fault_pin_pfn (mmu.c:1871)    // 锁定 host 物理页
             → kvm_s2_fault_compute_prot (mmu.c:1933) // 计算保护位
             → kvm_s2_fault_map (mmu.c:1984)           // 写入 Stage-2 PTE
               → 返回 guest → 重新执行 ldr → 此次命中
```

x86 EPT 的等价流程：
```
Guest 执行:  mov rax, [rbx]

  ① Stage-1 (Guest CR3):
     GVA → GPA

  ② EPT (EPT pointer in VMCS):
     GPA → EPT 遍历 → Hit → HPA → Done
                     → Violation → VM-Exit
                       → handle_ept_violation
                         → kvm_mmu_page_fault
                           → direct_map → mmu_set_spte → __set_spte
                           → VM-Entry → 重新执行 mov → Hit
```

---

## 5. 嵌套 Stage-2：虚拟机嵌套场景

当 KVM 运行在另一个 hypervisor 之上时（如公有云中的嵌套虚拟化），需要**嵌套 Stage-2**：

```
L0 Hypervisor (云平台 KVM)
  L1 Guest (客户 KVM)
    L2 Guest (客户虚拟机)

L2 GVA → L2 Stage-1 → L2 GPA
       → L1 Stage-2 → L1 GPA (映射到 L0 的 HPA)
       → L0 Stage-2 → Host PA
```

ARM64 KVM 通过 `kvm_arch.nested_mmus` 数组支持多个嵌套 Stage-2 MMU（`kvm_host.h:325-326`）：

```c
struct kvm_arch {
    struct kvm_s2_mmu mmu;                  // 主 Stage-2 (L1 使用)
    struct kvm_s2_mmu *nested_mmus;         // 嵌套 Stage-2 数组 (L2 虚拟使用)
    size_t nested_mmus_size;
    int nested_mmus_next;
    ...
};
```

每个嵌套 MMU 有独立的 `pgd_phys`、`vtcr`，通过 `nested_stage2_enabled` 标志切换（`kvm_s2_mmu.nested_stage2_enabled`，`kvm_host.h:218`）。

---

## 6. 与 SMMU/IOMMU 系列的关联

本专栏和 `soc-unified-memory`、`arm-smmu` 系列中 Stage-1/Stage-2 的概念是同一套：

| 场景 | Stage-1 | Stage-2 |
|------|---------|---------|
| **SMMU 设备翻译** | VA → IPA（进程页表镜像） | IPA → PA（Hypervisor 级） |
| **KVM ARM64** | Guest CR3 → GPA（Guest 页表） | GPA → HPA（VTTBR_EL2） |
| **KVM x86 EPT** | Guest CR3 → GPA | EPT GPA → HPA |

SMMU 的 Stage-2 由 `io-pgtable-arm.c` 管理，格式为 `ARM_64_LPAE_S2`；KVM ARM64 的 Stage-2 由 `kvm_pgtable_stage2_init()` 管理，格式也是 ARM LPAE。两者用的是同一套页表格式标准。

---

## 7. 总结

- **Stage-1**：guest 自己的页表，guest OS 管理，将 GVA 翻译为 GPA
- **Stage-2**：KVM 管理的页表，将 GPA 翻译为 HPA，guest 不可见
- ARM64 用 `kvm_s2_mmu` 管理，通过 VTTBR_EL2 加载到硬件
- x86 用 EPT/NPT，通过 VMCS/VMCB 加载
- Stage-2 缺页是 KVM 内存虚拟化中最频繁的事件（每个 guest 页首次访问都会触发）
- 嵌套虚拟化需要链式 Stage-2（L0 S2 + L1 S2）

## 下一篇文章

→ [第3.2篇：ARM64 KVM 虚拟化全景](./03b-arm64-kvm.md) — Stage-2 只是 ARM64 KVM 的一部分，下一篇展开 ARM64 KVM 的完整架构：VHE/nVHE 两种 hyp 模式、vGIC 中断虚拟化、系统寄存器模拟、HVC/SMC 处理。