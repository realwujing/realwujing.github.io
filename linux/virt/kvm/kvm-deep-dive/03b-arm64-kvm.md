# 第3.2篇：ARM64 KVM 虚拟化全景 — VHE/nVHE、vGIC 与系统寄存器

> 源码：`arch/arm64/kvm/arm.c` `arch/arm64/kvm/hyp/vhe/switch.c` `arch/arm64/kvm/hyp/nvhe/switch.c` `arch/arm64/kvm/sys_regs.c` `arch/arm64/kvm/vgic/` | 头文件：`arch/arm64/include/asm/kvm_host.h`

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 1. ARM64 KVM 架构全景

```
┌──────────────────────────────────────────────────────────────┐
│                     ARM64 KVM                                │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                arch/arm64/kvm/arm.c                   │   │
│  │  kvm_arm_init() [2982]  ← module_init                 │   │
│  │  kvm_arch_vcpu_ioctl_run() [1243]  ← KVM_RUN 循环      │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │          Hyp 模式切换（VHE vs nVHE）                    │   │
│  │  ┌───────────────────┐  ┌────────────────────────┐   │   │
│  │  │ hyp/vhe/switch.c  │  │ hyp/nvhe/switch.c      │   │   │
│  │  │ VHE guest switch  │  │ nVHE guest switch      │   │   │
│  │  │ __kvm_vcpu_run_vhe│  │ __kvm_vcpu_run (nHE)   │   │   │
│  │  └───────────────────┘  └────────────────────────┘   │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  handle_exit.c [376]  arm_exit_handlers 表（退出分发） │   │
│  │  sys_regs.c           系统寄存器模拟（5869 行）         │   │
│  │  hypercalls.c         HVC/SMC 处理                     │   │
│  │  mmu.c (74KB)         Stage-2 MMU                     │   │
│  │  vgic/ (17 files)     vGIC (GICv2/v3/v4/v5)          │   │
│  │  pvtime.c             ARM64 半虚拟化时间               │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

ARM64 和 x86 KVM 最根本的架构差异在 **Hyp 模式**：x86 始终运行在 root 模式（VMX root），ARM64 则有两条路径——VHE 和 nVHE。

---

## 2. ARM64 四级异常等级 — EL0 ~ EL3

ARM64 有四种异常等级，数字越大权限越高，互不重叠：

```
┌─────────────────────────────────────────────────────────────┐
│  EL3 — Secure Monitor                                       │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  固件（ARM Trusted Firmware / TF-A）                   │  │
│  │  SDEI（Software Delegated Exception Interface）       │  │
│  │  PSCI（Power State Coordination Interface）           │  │
│  │  安全世界 ↔ 非安全世界切换（SMC 指令）                  │  │
│  └───────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  EL2 — Hypervisor                                           │
│  ┌─────────────── VHE 模式 ──────────────────────────────┐  │
│  │  Host OS（Linux 内核）直接运行在 EL2                    │  │
│  │  HCR_EL2 控制：E2H=1, TGE=1                           │  │
│  │  Guest 通过 HCR_EL2 配置从 EL0/EL1 trap 上来            │  │
│  ├─────────────── nVHE 模式 ─────────────────────────────┤  │
│  │  Host OS 运行在 EL1，KVM（hyp）运行在 EL2               │  │
│  │  Guest trap 到 EL2，处理完再切回 EL1 的 host           │  │
│  └───────────────────────────────────────────────────────┘  │
│  职责：Stage-2 MMU、vGIC、系统寄存器 trap、VM 调度          │
├─────────────────────────────────────────────────────────────┤
│  EL1 — 操作系统内核                                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Guest OS kernel（Linux / Windows）                    │  │
│  │  TTBR0_EL1/TTBR1_EL1：页表翻译                          │  │
│  │  SCTLR_EL1：系统控制                                    │  │
│  │  HVC 指令 → trap 到 EL2（guest 向 KVM 请求服务）         │  │
│  │  SMC 指令 → trap 到 EL3（向 TrustZone 请求服务）         │  │
│  └───────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  EL0 — 用户态应用                                           │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Guest 用户的进程（在虚拟机里运行的 .so/.exe）           │  │
│  │  SVC 指令 → trap 到 EL1（系统调用）                      │  │
│  │  直接由 Guest OS 内核处理，KVM 不介入                    │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**关键原则**：异常只能 trap 到同级或更高异常等级，不能向下。EL0 的系统调用（`SVC`）trap 到 EL1 的 guest 内核，HVC trap 到 EL2 的 KVM，SMC trap 到 EL3 的 Secure Monitor。KVM 运行在 EL2，它**截获 guest 的 EL0→EL1 异常**（通过 Stage-2 abort）和**EL1 发的 HVC 指令**（直接 trap 到 EL2）。

### 2.1 Hyp 模式：VHE vs nVHE

上面这个 EL0~EL3 架构在不同硬件上实际运作时有两条路径：VHE 和 nVHE。

ARMv8.1 引入 **VHE**（Virtualization Host Extensions），让 host OS 直接运行在 EL2，消除传统的 EL1→EL2 world switch 开销。

| | **VHE** (ARMv8.1+) | **nVHE** (传统) |
|---|---|---|
| **Host 运行级别** | EL2（直接） | EL1 |
| **Guest 运行级别** | EL0/EL1（HCR_EL2 控制） | EL0/EL1 |
| **Guest SVC (EL0→EL1)** | 直接 trap 到 EL1（guest 内核） | 直接 trap 到 EL1（guest 内核） |
| **Guest HVC (EL1→EL2)** | trap 到 EL2（KVM） | trap 到 EL2（nVHE hyp） |
| **Guest 内存 abort** | EL2 收到 Stage-2 fault | EL2 收到 Stage-2 fault |
| **切换文件** | `hyp/vhe/switch.c` (689行) | `hyp/nvhe/switch.c` (409行) |
| **World Switch 开销** | 低（host 已在 EL2） | 高（EL1↔EL2 切换） |
| **CPU 要求** | ARMv8.1-VHE | ARMv8.0 |
| **Host 内核看到的** | 以为自己在 EL1（HCR_EL2.E2H 把 EL2 伪装成 EL1） | 确实在 EL1 |

**EL3 始终独立运行**：无论 VHE 还是 nVHE，EL3（Secure Monitor）都不参与 KVM 的日常调度。PSCI 调用（如 CPU 上下电）才需要 SMC 转发到 EL3。KVM 不直接访问 EL3，通过 `handle_smc`（`handle_exit.c` 分发到 `hypercalls.c`）转发。

### 2.2 VHE Guest Switch

```c
// hyp/vhe/switch.c:572-620 — VHE guest 运行循环
static int __kvm_vcpu_run_vhe(struct kvm_vcpu *vcpu)
{
    // ① 保存 host 上下文（callee-saved regs + sysregs）
    host_ctxt = &this_cpu_ptr(&kvm_host_data)->host_ctxt;
    __host_enter(vcpu, host_ctxt);    // 保存 host TCR/MAIR/TTBR等
    
    // ② 激活 guest 陷阱
    vcpu->arch.hcr_el2 = compute_hcr(vcpu);     // line 52, __compute_hcr
    write_sysreg(vcpu->arch.hcr_el2, hcr_el2);
    
    // ③ 调整 PC（KVM 需要 PC 指向 exception return 点）
    *vcpu_pc(vcpu) = *vcpu_cpsr(vcpu) & ...;
    
    // ④ 恢复 guest sysregs
    __sysreg_restore_el1_state(guest_ctxt);
    
    // ⑤ ★ 加载 Stage-2
    __load_stage2(vcpu->arch.hw_mmu, ...);       // line 222
    
    // ⑥ ★ 进入 guest (__guest_enter 循环)
    do {
        __guest_enter(vcpu);                     // ERET to EL1/EL0
        fixup_guest_exit(vcpu, &exit_code);      // line 537
    } while (fixup_guest_exit returns true);     // 重新进入 guest
    
    // ⑦ 保存 guest sysregs
    __sysreg_save_el1_state(guest_ctxt);
    
    // ⑧ 恢复 host sysregs
    __sysreg_restore_el2_state(host_ctxt);
    
    return exit_code;  // 返回给 handle_exit() 分发
}
```

### 2.3 nVHE Guest Switch

```c
// hyp/nvhe/switch.c:258-315 — nVHE guest 运行循环
static int __kvm_vcpu_run(struct kvm_vcpu *vcpu)
{
    // ① 保存 host 上下文（在 EL1）
    host_ctxt = &this_cpu_ptr(&kvm_host_data)->host_ctxt;
    __host_enter(vcpu, host_ctxt);
    
    // ② PMU 切换（nVHE 需要完整 PMU 状态切换）
    kvm_vcpu_pmu_switch_sw(vcpu);  // 计数器和事件
    
    // ③ 保存 host debug 状态
    kvm_vcpu_debug_switch_sw(vcpu, NULL);
    
    // ④ 恢复 guest sysregs
    __sysreg_restore_el1_state(guest_ctxt);
    
    // ⑤ ★ 加载 Stage-2
    __load_stage2(vcpu->arch.hw_mmu, ...);
    
    // ⑥ 激活 traps（nVHE 需要完整陷阱配置）
    __activate_traps(vcpu);          // HCR/MDCR/CPTR/VBAR
    
    // ⑦ 进入 guest
    do {
        __guest_enter(vcpu);
        fixup_guest_exit(vcpu, &exit_code);
    } while (...);
    
    // ⑧ ★ 停用 traps（nVHE 需要停用以防 host 被 trap）
    __deactivate_traps(vcpu);        // 恢复 host HCR/MDCR
    
    // ⑨ 恢复 host sysregs
    __sysreg_restore_el1_state(host_ctxt);
    
    return exit_code;
}
```

核心差异：nVHE 需要显式的 `__activate_traps / __deactivate_traps` 对，而 VHE 的陷阱在 HCR_EL2 中激活后一直生效。

---

## 3. VM-Exit 处理

### 3.1 arm_exit_handlers 分发表

`handle_exit.c:376-405` 定义了完整的 Exit Handler 分发表，按 ESR_ELx.EC（Exception Class）索引：

```c
// handle_exit.c:376-405
static exit_handle_fn arm_exit_handlers[] = {
    [0 ... ESR_ELx_EC_MAX]    = kvm_handle_unknown_ec,   // 未识别的异常
    
    // 系统寄存器访问（绝大多数 VM-Exit 原因）
    [ESR_ELx_EC_SYS64]         = kvm_handle_sys_reg,      // MSR/MRS to sysreg
    
    // 内存访问
    [ESR_ELx_EC_IABT_LOW]      = kvm_handle_guest_abort,  // 指令 abort
    [ESR_ELx_EC_DABT_LOW]      = kvm_handle_guest_abort,  // 数据 abort
    
    // 系统调用（Hypercall / Secure call）
    [ESR_ELx_EC_HVC32]         = handle_hvc,              // HVC 32-bit
    [ESR_ELx_EC_HVC64]         = handle_hvc,              // HVC 64-bit
    [ESR_ELx_EC_SMC32]         = handle_smc,              // SMC 32-bit
    [ESR_ELx_EC_SMC64]         = handle_smc,              // SMC 64-bit
    
    // 协处理器访问
    [ESR_ELx_EC_CP15_32]       = kvm_handle_cp15_32,
    [ESR_ELx_EC_CP15_64]       = kvm_handle_cp15_64,
    
    // 浮点/SIMD
    [ESR_ELx_EC_FP_ASIMD]      = kvm_handle_fpasimd,      // 浮点寄存器访问
    [ESR_ELx_EC_SVE]           = kvm_handle_sve,           // SVE 寄存器
    
    // Wait-for-event/interrupt
    [ESR_ELx_EC_WFx]           = kvm_handle_wfx,           // WFI/WFE
    
    // 调试
    [ESR_ELx_EC_BRK64]         = kvm_handle_guest_debug,
    ...
};
```

### 3.2 handle_exit 顶层分发

```c
// handle_exit.c:446-489
static int handle_exit(struct kvm_vcpu *vcpu, int exception_index)
{
    switch (exception_index) {
    case ARM_EXCEPTION_IRQ:
        return 1;                              // 正常中断 → 继续
    
    case ARM_EXCEPTION_EL1_SERROR:
        kvm_inject_vabt(vcpu);                // SError 注入到 guest
        return 1;
    
    case ARM_EXCEPTION_TRAP:
        // ★ 核心：查找异常类 → 调用对应 handler
        return handle_trap_exceptions(vcpu);   // line 484
    
    case ARM_EXCEPTION_IL:
        run->exit_reason = KVM_EXIT_FAIL_ENTRY;
        return 0;                              // 非法指令长度 → 退出到用户态
    }
}
```

---

## 4. 系统寄存器模拟 — sys_regs.c (5869 行)

ARM64 有数百个系统寄存器，KVM 必须模拟 guest 对这些寄存器的访问（通过 trap-and-emulate 或直接映射）。

### 4.1 系统寄存器描述符表

```c
// sys_regs.c — sys_reg_descs[] 表定义（位于文件末尾的 4700 行起）
// 每个描述符包含：
// - 寄存器编号（Op0, Op1, CRn, CRm, Op2）
// - access 函数：处理 guest 读/写
// - reset 函数：初始化 guest 可见值
// - visibility 函数：控制 guest 是否能看到
```

常见的系统寄存器模拟类：

| 寄存器类型 | 示例 | 处理方式 |
|-----------|------|---------|
| **ID 寄存器** | MIDR_EL1, ID_AA64ISAR0_EL1 | `set_id_reg`：从 `kvm_arch.id_regs[]` 返回过滤后的值 |
| **控制寄存器** | SCTLR_EL1, TCR_EL1 | 需要 Stage-1 上下文的复杂处理 |
| **性能计数** | PMCR_EL0, PMCNTENSET_EL0 | 通过 `kvm_pmu` 虚拟 PMU |
| **FP/SIMD/SVE** | ZCR_EL1, FPCR | SVE 状态管理 |

### 4.2 ID 寄存器虚拟化

```c
// sys_regs.c:49 — 设置 ID 寄存器到 guest 可见值
static void set_id_reg(struct kvm_vcpu *vcpu, const struct sys_reg_desc *rd,
                       u64 val)
{
    // 通过 PMU 过滤或掩码某些位
    val = arm64_ftr_apply_relaxed(...);
    __vcpu_sys_reg(vcpu, rd->reg) = val;
}

// arm.c:373 — kvm_arch 的 vCPU 特性位图
DECLARE_BITMAP(vcpu_features, KVM_VCPU_MAX_FEATURES);  // VM 级特性
```

---

## 5. HVC/SMC 处理 — hypercalls.c

Guest 通过 `HVC` 指令发出 hypercall，KVM 在 EL2 截获后处理：

```c
// hypercalls.c:21-45 — PTP 时钟 hypercall
int kvm_ptp_get_time(struct kvm_vcpu *vcpu, u64 *val)
{
    // 向 guest 提供虚拟/物理计数器值（用于精确计时）
    val[0] = kvm_ptp_get_time_ns(...);
    val[1] = 0;
    val[2] = 0;
    return 0;
}

// hypercalls.c:70-80 — 默认允许的 SMCCC 函数
static bool kvm_smccc_default_allowed(u32 func_id)
{
    switch (func_id) {
    case ARM_SMCCC_VERSION_FUNC_ID:
    case ARM_SMCCC_ARCH_FEATURES_FUNC_ID:
        return true;
    }
    return kvm_arm_smccc_filter_allowed(vcpu, func_id);
}
```

---

## 6. vGIC — 虚拟通用中断控制器

ARM64 KVM 的 vGIC 目录（`arch/arm64/kvm/vgic/`）包含 17 个文件，支持 GICv2/v3/v4/v5：

| 文件 | 用途 |
|------|------|
| `vgic-init.c` | 初始化 vGIC 分发器和 CPU 接口 |
| `vgic.c` | 核心分发逻辑 |
| `vgic-mmio-v2.c` | GICv2 MMIO 模拟 |
| `vgic-mmio-v3.c` | GICv3 MMIO 模拟（GICD, GICR） |
| `vgic-v3.c` | GICv3 分发器/重分发器状态 |
| `vgic-v4.c` | GICv4 直接虚拟中断注入 |
| `vgic-v5.c` | GICv5 支持 |
| `vgic-its.c` | ITS (Interrupt Translation Service) 模拟 |
| `vgic-irqfd.c` | IRQFD 支持（从内核注入虚拟中断） |

vGIC 的 MMIO 访问由 Stage-2 abort 触发（`io_mem_abort` 在 `mmu.c` 中），最终分发给 `vgic_mmio_read/write`。

---

## 7. 初始化流程

```c
// arm.c:2982-3062 — module_init 入口
static int __init kvm_arm_init(void)
{
    // ① 检查 hyp 模式是否可用
    int err = is_hyp_mode_available();        // line 2987
    kvm_get_mode();                           // line 2992: VHE / nVHE
    
    // ② 初始化系统寄存器描述符表
    kvm_sys_reg_table_init();                 // line 2997
    
    // ③ 设置 IPA 限制（Stage-2 IPA 宽度）
    kvm_set_ipa_limit();                      // line 3010
    
    // ④ 初始化 SVE 支持
    kvm_arm_init_sve();                       // line 3014
    
    // ⑤ 初始化 VMID 分配器
    kvm_arm_vmid_alloc_init();                // line 3018
    
    // ⑥ 初始化 hyp 模式（nVHE 需要建立 hyp 页表）
    init_hyp_mode();                          // line 3025
    
    // ⑦ 初始化子系统（vGIC, timer, hypercalls）
    init_subsystems();                        // line 3036
    
    // ⑧ 调用通用 KVM 初始化
    kvm_init(&kvm_ops, ...);                  // line 3051
    
    kvm_arm_initialised = true;               // line 3062
}
```

VM 创建时调用 `kvm_arch_init_vm`（`arm.c:208`），初始化 Stage-2 MMU、vGIC、timer。

---

## 8. ARM64 vs x86 KVM 核心差异对比

| | ARM64 KVM | x86 KVM |
|---|---|---|
| **初始化入口** | `kvm_arm_init()` (arm.c:2982) | `intel_iommu_init` + `vmx_init` |
| **Hyp/VMX 模式** | VHE (EL2直连) / nVHE (EL1↔EL2) | VMX root / non-root |
| **Guest 进入** | `__guest_enter()` + ERET (VHE) 或完整 world switch (nVHE) | `__vmx_vcpu_run` (vmenter.S:47) |
| **退出分发** | `arm_exit_handlers[EC]` (handle_exit.c:376) | VM-Exit reason dispatch |
| **MMU Stage-2** | `kvm_s2_mmu` → VTTBR_EL2 (arm64) | EPT/NPT → EPTP (VMCS) / nCR3 (VMCB) |
| **中断虚拟化** | vGIC (GICv2/v3/v4/v5) | LAPIC + IOAPIC |
| **系统寄存器** | sys_reg_descs[] (5869行) | MSR bitmap + MSR intercept |
| **Hypercall** | HVC 指令 (ARM SMCCC) | VMCALL 指令 |
| **半虚拟化时间** | `kvm_update_stolen_time()` (pvtime.c:13) | `record_steal_time()` (x86.c:3748) |

---

## 9. 总结

- ARM64 KVM 有两条路径：VHE（高效，EL2 直连）和 nVHE（传统，EL1↔EL2 切换）
- VM-Exit 通过 `arm_exit_handlers[]` 按 ESR_ELx.EC 分发
- 系统寄存器模拟 (`sys_regs.c`) 是 ARM64 KVM 最大的单一源文件 (5869 行)
- vGIC 支持 GICv2 到 GICv5，通过 Stage-2 abort → `io_mem_abort` → vgic MMIO 分发
- Stage-2 缺页是最频繁的 VM-Exit 原因，由 `user_mem_abort` (mmu.c:2072) 处理

## 下一篇文章

→ [第4篇：设备直通 — VFIO、IOMMU 与中断重映射](./04-vfio.md)