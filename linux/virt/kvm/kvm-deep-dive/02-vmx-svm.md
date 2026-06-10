# 第2篇：x86 虚拟化 — VMX/VMCS 与 SVM/VMCB，VM-Entry/Exit 全解析

> 源码：`arch/x86/kvm/vmx/vmx.c` `arch/x86/kvm/svm/svm.c` | 头文件：`arch/x86/include/asm/kvm_host.h`

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 1. struct kvm_x86_ops：vendor 抽象层 (kvm_host.h:1762)

`struct kvm_x86_ops` 是 KVM x86 架构最核心的抽象接口，定义了 VMX (Intel) 和 SVM (AMD) 共用的操作集，定义在 `arch/x86/include/asm/kvm_host.h:1762`。

两个 vendor 实例：
- Intel: `vt_x86_ops` — `arch/x86/kvm/vmx/main.c:872`
- AMD:   `svm_x86_ops` — `arch/x86/kvm/svm/svm.c:5270`

```c
// arch/x86/include/asm/kvm_host.h:1762
struct kvm_x86_ops {
    // ========== vCPU 生命周期 ==========
    int (*vcpu_create)(struct kvm_vcpu *vcpu);
    void (*vcpu_free)(struct kvm_vcpu *vcpu);
    void (*vcpu_reset)(struct kvm_vcpu *vcpu, bool init_event);

    // ========== vCPU 执行 ==========
    fastpath_t (*vcpu_run)(struct kvm_vcpu *vcpu, u64 run_flags);      // VM-Entry
    int (*handle_exit)(struct kvm_vcpu *vcpu, int reason);            // VM-Exit 分发
    void (*handle_exit_irqoff)(struct kvm_vcpu *vcpu);                // 关中断的 exit 处理
    void (*request_immediate_exit)(struct kvm_vcpu *vcpu);            // 请求立即退出

    // ========== 寄存器虚拟化 ==========
    int (*get_segment)(struct kvm_vcpu *vcpu, struct kvm_segment *var, int seg);
    void (*set_segment)(struct kvm_vcpu *vcpu, const struct kvm_segment *var, int seg);
    void (*get_cs_db_l_bits)(struct kvm_vcpu *vcpu, int *db, int *l);
    void (*set_cr0)(struct kvm_vcpu *vcpu, unsigned long cr0);
    void (*set_cr3)(struct kvm_vcpu *vcpu, unsigned long cr3);
    void (*set_cr4)(struct kvm_vcpu *vcpu, unsigned long cr4);
    void (*set_efer)(struct kvm_vcpu *vcpu, u64 efer);
    u64 (*get_segment_base)(struct kvm_vcpu *vcpu, int seg);

    // ========== MSR 虚拟化 ==========
    int (*get_msr)(struct kvm_vcpu *vcpu, struct msr_data *msr);
    int (*set_msr)(struct kvm_vcpu *vcpu, struct msr_data *msr);
    int (*get_msr_feature)(struct kvm_msr_entry *entry);

    // ========== 中断虚拟化 ==========
    void (*inject_irq)(struct kvm_vcpu *vcpu, bool reinjected);
    void (*inject_nmi)(struct kvm_vcpu *vcpu);
    void (*enable_nmi_window)(struct kvm_vcpu *vcpu);
    void (*enable_irq_window)(struct kvm_vcpu *vcpu);
    bool (*apic_init_signal_blocked)(struct kvm_vcpu *vcpu);
    bool (*apicv_pre_state_acceptable)(struct kvm_vcpu *vcpu);

    // ========== TLB / MMU ==========
    int (*tlb_flush)(struct kvm_vcpu *vcpu);
    int (*tlb_flush_guest)(struct kvm_vcpu *vcpu);
    void (*flush_tlb_all)(struct kvm_vcpu *vcpu);
    void (*flush_tlb_current)(struct kvm_vcpu *vcpu);
    void (*load_mmu_pgd)(struct kvm_vcpu *vcpu, hpa_t root_hpa, int root_level);

    // ========== 嵌套虚拟化 ==========
    struct kvm_x86_nested_ops nested_ops;

    // ========== APIC 虚拟化 ==========
    bool (*has_virtual_apic)(struct kvm_vcpu *vcpu);
    void (*refresh_apicv_exec_ctrl)(struct kvm_vcpu *vcpu);
    void (*hwapic_irr_update)(struct kvm_vcpu *vcpu, int max_irr);
    void (*hwapic_isr_update)(int isr);
    u64 (*apic_sync_pir_to_irr)(struct kvm_vcpu *vcpu);

    // ========== PMU ==========
    const struct kvm_pmu_ops *pmu_ops;

    // ========== VM 生命周期 ==========
    int (*vm_init)(struct kvm *kvm);
    void (*vm_destroy)(struct kvm *kvm);
    int (*vm_move_enc_context_from)(struct kvm *kvm, unsigned int source_fd);

    // ========== 安全检查 ==========
    bool (*has_wbinvd_exit)(void);
    int (*check_processor_compatibility)(void);

    // ========== 内存加密 ==========
    int (*mem_enc_ioctl)(struct kvm *kvm, void __user *argp);
    int (*mem_enc_register_region)(struct kvm *kvm, struct kvm_enc_region *argp);
    int (*mem_enc_unregister_region)(struct kvm *kvm, struct kvm_enc_region *argp);
};
```

`kvm_x86_ops` 使得 `arch/x86/kvm/x86.c` 中的通用代码可以直接调用 vendor 特定实现，而不需要 #ifdef 条件编译。

---

## 2. Intel VMX 实现

### 2.1 struct vcpu_vmx (vmx/vmx.h:200)

```c
// arch/x86/kvm/vmx/vmx.h:200
struct vcpu_vmx {
    struct kvm_vcpu       vcpu;            // 通用 vCPU (必须是第一个成员)

    // VMCS 管理
    struct loaded_vmcs    vmcs01;          // 主 VMCS (L1 guest)
    struct loaded_vmcs   *loaded_vmcs;     // 当前使用的 VMCS 指针 (L1 或 L2)
    struct loaded_vmcs   *loaded_vmcs_on_cpu; // 当前 pCPU 上已加载的 VMCS
    bool                  __launched;      // 首次用 VMLAUNCH, 之后用 VMRESUME

    // 嵌套虚拟化
    struct nested_vmx     nested;

    // VPID 和 EPT
    u16                   vpid;            // Virtual Processor ID

    // Posted Interrupt
    struct pi_desc        pi_desc ____cacheline_aligned;
    struct pi_desc        *pid_table;      // Posted Interrupt Descriptor

    // 寄存器缓存
    unsigned long         host_debugctlmsr;

    // MSR 自动加载
    struct vmx_msrs       msr_autoload;

    // 统计信息
    u32                   exit_reason;     // 上一次 VM-Exit 原因
    u32                   exit_intr_info;  // 中断/异常信息
    u32                   exit_qualification;

    // IRQ/NMI window
    u32                   idt_vectoring_info;
    ...
};
```

### 2.2 VMCS 布局

Intel VMX 使用 VMCS (Virtual Machine Control Structure) 数据结构控制 VM 切换：

```
┌─────────────────────────────────────┐
│          VMCS (每个 vCPU 一份)        │
├─────────────────────────────────────┤
│  Host-State Area                    │
│    ├── CR0, CR3, CR4               │
│    ├── RIP (vmx_vmexit 入口)        │
│    ├── RSP                          │
│    ├── CS/DS/ES/SS/FS/GS/TR selec  │
│    ├── FS/GS/TR base               │
│    └── MSR: EFER, SYSENTER, PAT    │
├─────────────────────────────────────┤
│  Guest-State Area                   │
│    ├── CR0, CR3, CR4, DR7          │
│    ├── RIP, RSP, RFLAGS            │
│    ├── CS/DS/ES/SS selector+base   │
│    ├── LDTR, GDTR, IDTR, TR        │
│    ├── MSR: EFER, SYSENTER         │
│    └── Activity State              │
├─────────────────────────────────────┤
│  VM-Execution Control Fields        │
│    ├── Pin-based controls           │
│    │   └── NMI exiting, virtual NMI│
│    ├── Processor-based controls    │
│    │   ├── Primary: int window,    │
│    │   │   use TSC offsetting,     │
│    │   │   CR3-load exiting, ...   │
│    │   └── Secondary: EPT, VPID,   │
│    │       RDTSCP, XSAVES, ...     │
│    └── Exception Bitmap            │
├─────────────────────────────────────┤
│  VM-Exit Control Fields             │
│    ├── Host address space size     │
│    ├── Load MSRs on exit           │
│    └── Save MSRs on exit           │
├─────────────────────────────────────┤
│  VM-Entry Control Fields            │
│    ├── Load MSRs on entry          │
│    └── Entry to SMM control        │
└─────────────────────────────────────┘
```

VMREAD/VMWRITE 指令用于读写 VMCS 字段。KVM 使用 `vmcs_read(field)` / `vmcs_write(field, value)` 封装。

### 2.3 VM-Entry: __vmx_vcpu_run (vmenter.S:47)

核心汇编函数 `__vmx_vcpu_run` 实现寄存器级别的世界切换：

```c
// arch/x86/kvm/vmx/vmenter.S:47
SYM_FUNC_START(__vmx_vcpu_run)
    // 保存 host 寄存器到栈
    push %_ASM_BP
    mov  %_ASM_SP, %_ASM_BP

    push %r15
    push %r14
    push %r13
    push %r12
    push %rbx

    // 加载 guest 寄存器 (RAX, RCX, RDX, ... → VMCS)
    // ...
    // VMLAUNCH 或 VMRESUME
    // ...
    // VM-Exit 后: 保存 guest 寄存器 → VMCS, 恢复 host 寄存器
    // ...
    // 返回
SYM_FUNC_END(__vmx_vcpu_run)
```

C 层包装 `vmx_vcpu_run` (`arch/x86/kvm/vmx/vmx.c:7515`)：

```c
// arch/x86/kvm/vmx/vmx.c:7515
fastpath_t vmx_vcpu_run(struct kvm_vcpu *vcpu, u64 run_flags)
{
    struct vcpu_vmx *vmx = to_vmx(vcpu);

    // 1. 准备 VM-entry: 写 VMCS 各字段
    vmx_vcpu_prepare(vcpu);                         // line ~7530

    // 2. 允许进入 guest mode
    kvm_guest_enter_irqoff();

    // 3. 汇编 VM-Entry world switch
    vmx->fail = __vmx_vcpu_run(vmx,                 // line 7490
                                (unsigned long *)&vcpu->arch.regs,
                                __vmx_vcpu_run_flags(vmx));

    // 4. 退出 guest mode
    kvm_guest_exit_irqoff();

    // 5. 获取 VM-Exit reason, 尝试快手处理
    vmx->exit_reason = vmcs_read32(VM_EXIT_REASON);

    // 6. 如果可能, 在内核态直接处理 exit (fast path)
    if (is_guest_mode(vcpu))
        return EXIT_FASTPATH_NONE;

    // 尝试 handle_exit_fastpath:
    //   - EXIT_REASON_MSR_WRITE (EXIT_FASTPATH_MSR_WRITE_12)
    //   - EXIT_REASON_PREEMPTION_TIMER (reschedule)
    return vmx_handle_exit_fastpath(vcpu, run_flags);
}
```

### 2.4 VM-Exit 分发

```c
// arch/x86/kvm/vmx/vmx.c: ~6980
static int vmx_handle_exit(struct kvm_vcpu *vcpu, fastpath_t exit_fastpath)
{
    struct vcpu_vmx *vmx = to_vmx(vcpu);

    if (exit_fastpath != EXIT_FASTPATH_NONE)
        return 1;   // fastpath 已经处理

    return kvm_vmx_exit_handlers[exit_reason](vcpu);
}

// 部分 exit handler:
static int (*kvm_vmx_exit_handlers[])(struct kvm_vcpu *vcpu) = {
    [EXIT_REASON_EXCEPTION_NMI]           = handle_exception_nmi,
    [EXIT_REASON_EXTERNAL_INTERRUPT]      = handle_external_interrupt,
    [EXIT_REASON_TRIPLE_FAULT]            = handle_triple_fault,
    [EXIT_REASON_EPT_VIOLATION]           = handle_ept_violation,
    [EXIT_REASON_EPT_MISCONFIG]           = handle_ept_misconfig,
    [EXIT_REASON_CPUID]                   = handle_cpuid,
    [EXIT_REASON_HLT]                     = handle_hlt,
    [EXIT_REASON_INVD]                    = handle_invd,
    [EXIT_REASON_INVLPG]                  = handle_invlpg,
    [EXIT_REASON_RDPMC]                   = handle_rdpmc,
    [EXIT_REASON_VMCALL]                  = handle_vmcall,
    [EXIT_REASON_VMCLEAR]                 = handle_vmclear,
    [EXIT_REASON_VMLAUNCH]                = handle_vmlaunch,
    [EXIT_REASON_VMPTRLD]                 = handle_vmptrld,
    [EXIT_REASON_VMPTRST]                 = handle_vmptrst,
    [EXIT_REASON_VMREAD]                  = handle_vmread,
    [EXIT_REASON_VMRESUME]                = handle_vmresume,
    [EXIT_REASON_VMWRITE]                 = handle_vmwrite,
    [EXIT_REASON_VMOFF]                   = handle_vmoff,
    [EXIT_REASON_VMON]                    = handle_vmon,
    [EXIT_REASON_CR_ACCESS]               = handle_cr,
    [EXIT_REASON_DR_ACCESS]               = handle_dr,
    [EXIT_REASON_IO_INSTRUCTION]          = handle_io,
    [EXIT_REASON_MSR_READ]                = handle_rdmsr,
    [EXIT_REASON_MSR_WRITE]               = handle_wrmsr,
    [EXIT_REASON_PENDING_INTERRUPT]       = handle_interrupt_window,
    [EXIT_REASON_NMI_WINDOW]              = handle_nmi_window,
    [EXIT_REASON_TASK_SWITCH]             = handle_task_switch,
    [EXIT_REASON_MCE_DURING_VMENTRY]      = handle_machine_check,
    [EXIT_REASON_GDTR_IDTR]               = handle_desc,
    [EXIT_REASON_LDTR_TR]                 = handle_desc,
    [EXIT_REASON_APIC_ACCESS]             = handle_apic_access,
    [EXIT_REASON_APIC_WRITE]              = handle_apic_write,
    [EXIT_REASON_EPT_ACCESS]              = handle_wbinvd,
    [EXIT_REASON_XSETBV]                  = handle_xsetbv,
    [EXIT_REASON_XSAVES]                  = handle_xsaves,
    [EXIT_REASON_XRSTORS]                 = handle_xrstors,
    [EXIT_REASON_PML_FULL]                = handle_pml_full,
    [EXIT_REASON_PREEMPTION_TIMER]        = handle_preemption_timer,
    [EXIT_REASON_NOTIFY]                  = handle_notify,
};
```

### 2.5 VPID: 虚拟处理器标识

VPID (Virtual Processor Identifier) 避免 TLB flush on VM-entry/exit：

| VPID 值 | 含义 |
|--------|------|
| 0 | 共享物理 TLB (legacy) |
| 1+ | 独立 TLB 标签 |

KVM 在 `vmx_vcpu_create` 中分配 VPID：`vmx->vpid = allocate_vpid()`。VPID 值写入 VMCS guest-state area 的 VPID 字段，配合 EPT 实现 TLB 隔离。

### 2.6 TDX: Trust Domain Extensions (vmx/tdx.c)

Intel TDX 提供硬件级机密运算。KVM 通过 `vmx/tdx.c` 支持 TDX guest：

- SEAM (Secure Arbitration Mode) 模块处理 TD VM 的进入和退出
- SEAMCALL 指令替代 VMLAUNCH/VMRESUME
- SEAMRET 返回替代 VM-Exit
- Guest 私有内存通过 SEPTs (Secure EPT) 管理
- `vt_x86_ops` 在 `vmx/tdx.c:3416` 被 TDX 覆盖若干指针

---

## 3. AMD SVM 实现

### 3.1 struct vcpu_svm (svm/svm.h:272)

```c
// arch/x86/kvm/svm/svm.h:272
struct vcpu_svm {
    struct kvm_vcpu vcpu;                 // 通用 vCPU

    struct vmcb *vmcb;                    // 当前活跃 VMCB (vmcb01 或 vmcb02)
    struct vmcb *vmcb01;                  // L1 guest 的 VMCB

    u64 vmcb_pa;                          // VMCB 物理地址

    struct svm_cpu_data *svm_data;        // per-CPU SVM 数据

    // SEV
    void __user *ghcb_sa;                 // GHCB scratch area
    u64 ghcb_reg;                         // GHCB 寄存器
    u64 ghcb_sa_len;

    // 嵌套虚拟化
    struct nested_state nested;

    // ASID
    u32 asid;                             // Address Space ID
    u16 asid_generation;

    // MSR 权限位图
    unsigned long *msrpm;                 // MSR Permission Map (2K x 2)

    // AVIC 相关
    struct page *avic_backing_page;       // AVIC 后备页
    u64 *avic_physical_id_cache;          // vCPU → pCPU 映射
    bool avic_is_running;

    // SEV-ES / SEV-SNP
    struct vmcb_save_area *vmsa;          // VMSA (VM Save Area)

    bool guest_state_loaded;              // guest state 是否已加载
};
```

### 3.2 VMCB 布局

AMD 使用 VMCB (Virtual Machine Control Block) 替代 Intel 的 VMCS：

```
┌──────────────────────────────────────┐
│              VMCB (4KB)              │
├──────────────────────────────────────┤
│  Save Area (偏移 0x000)              │
│    ├── ES/CS/SS/DS selector + attr  │
│    ├── CPL, CR0, CR2, CR3, CR4     │
│    ├── RIP (nRIP)                   │
│    ├── RSP                          │
│    ├── RAX                          │
│    ├── RFLAGS                       │
│    ├── DR6, DR7                     │
│    ├── EFER, STAR, LSTAR, CSTAR    │
│    ├── GDTR, IDTR, TR               │
│    └── Efer, PAT                    │
├──────────────────────────────────────┤
│  Control Area (偏移 0x400)           │
│    ├── Intercept vectors            │
│    │   ├── INTERCEPT_CR_READ/WRITE  │
│    │   ├── INTERCEPT_MSR_PROT       │
│    │   ├── INTERCEPT_IOIO_PROT      │
│    │   ├── INTERCEPT_INTR, NMI      │
│    │   ├── INTERCEPT_VMRUN, VMLOAD  │
│    │   ├── INTERCEPT_HLT, PAUSE     │
│    │   └── INTERCEPT_EXCEPTION_OFF  │
│    │                                │
│    ├── IOPM_BASE_PA  (IO权限位图)   │
│    ├── MSRPM_BASE_PA  (MSR权限位图) │
│    ├── TSC_OFFSET                   │
│    ├── ASID                         │
│    ├── V_TPR, V_IRQ                 │
│    ├── V_INTR_VECTOR                │
│    ├── NPT (Nested Page Table)      │
│    ├── nested_ctl                   │
│    ├── EVENTINJ (事件注入)           │
│    ├── EXITCODE, EXITINFO1/2       │
│    └── VMCB Clean Bits              │
└──────────────────────────────────────┘
```

### 3.3 VMCB Clean Bits 优化

AMD 提供 VMCB Clean Bits 机制标记哪些字段已被 KVM 修改，避免不必要的 VMRUN 时刷新：

```c
#define VMCB_CLEAN_INTERCEPTS    (1 << 0)
#define VMCB_CLEAN_IOPM_PA       (1 << 1)
#define VMCB_CLEAN_ASID          (1 << 2)
#define VMCB_CLEAN_TPR           (1 << 3)
#define VMCB_CLEAN_NPT           (1 << 4)
#define VMCB_CLEAN_CR2           (1 << 5)
#define VMCB_CLEAN_LBR           (1 << 6)
#define VMCB_CLEAN_AVIC          (1 << 7)
```

KVM 使用 `vmcb_mark_dirty(vmcb, bits)` 标记脏字段，VMRUN 时硬件只加载被标记的字段。

### 3.4 世界切换：VMRUN / #VMEXIT

SVM 的世界切换流程：

```
host kernel                    guest
    │                            │
    │ 保存 host 寄存器            │
    │                            │
    │ 设置 VMCB 各字段            │
    │                            │
    │ VMRUN (vmcb_pa)            │
    │ ──────────────────────────►│ #VMEXIT 返回后第一条指令
    │                            │ guest 上下文自动加载
    │                            │
    │                            │ (guest 执行用户代码)
    │                            │
    │                            │ 触发拦截 (IO/MSR/CR access...)
    │                            │
    │ #VMEXIT ◄──────────────────│
    │                            │
    │ 读取 EXITCODE              │
    │ 分发 handle_exit           │
    │                            │
    │ 再次 VMRUN ...             │
```

与 Intel 不同，SVM 没有独立的 Enter/Exit 指令，而是统一的 VMRUN，自动处理 guest state 的加载和保存。AMD 使用 `VMRUN vmcb_pa` 一条指令完成切换。

### 3.5 SEV / SEV-ES / SEV-SNP (svm/sev.c)

AMD SEV 系列技术提供不同级别的机密运算：

| 技术 | 特性 | 内存加密 | 寄存器保护 |
|------|------|---------|-----------|
| SEV | 基本内存加密 | 每个 VM 独立密钥 | 无 |
| SEV-ES | 加密状态 | guest state 加密 | 是 (VMSA) |
| SEV-SNP | 安全嵌套分页 | 完整性保护 | 是 + RMP 校验 |

SEV 的关键流程：

```
KVM_SEV_INIT              — 初始化 SEV 固件
KVM_SEV_LAUNCH_START      — 开始启动加密 guest
KVM_SEV_LAUNCH_UPDATE_DATA — 加载加密数据 (firmware, kernel)
KVM_SEV_LAUNCH_MEASURE    — 获取启动度量
KVM_SEV_LAUNCH_FINISH     — 完成启动
```

SEV-ES 扩展引入 GHCB (Guest-Hypervisor Communication Block)，为 guest 和 hypervisor 之间的通信提供协议。

### 3.6 AVIC: Advanced Virtual Interrupt Controller (svm/avic.c)

AMD AVIC 提供硬件级中断虚拟化，避免 VM-Exit 处理中断：

```
Guest writes to APIC TPR/EOI
       │
       ▼
Hardware AVIC table update (no VM-Exit!)
       │
       ▼
Hardware 评估: 是否有 pending 中断 > TPR?
       │
       ├── 是 → 直接注入 (no VM-Exit)
       │
       └── 否 → 继续 guest 执行

外部中断到达 (IPI / device interrupt)
       │
       ▼
IOMMU 查 AVIC 表 → 发送 Posted Interrupt
       │
       ▼
目标 vCPU 收到通知 → 硬件注入中断 (no VM-Exit!)
```

AVIC 使用物理 pCPU 门铃机制和 doorbell 页完成中断加速。`svm_x86_ops` 在 `svm/avic.c:1299` 启用 AVIC 相关特性。

---

## 4. 嵌套虚拟化

### 4.1 VMX Nested (vmx/nested.c, ~237KB)

当 L1 hypervisor 运行在 KVM 内部时：

```
L2 Guest (nested VM)
       │
    VMLAUNCH/VMRESUME (由 L1 发出)
       │
       ▼
KVM intercept (L1 的 VM-entry 尝试)
       │
       ▼
nested_vmx_run() — 合并 L1 VMCS12 + L0 VMCS01 → 构造 VMCS02 (shadow VMCS)
       │
       ▼
VMRESUME (执行 L2, 使用 VMCS02)
       │
       ▼
L2 执行... → VM-Exit
       │
       ▼
如果 exit 需要 L1 处理 → inject 到 L1 → VM-Entry L1
如果 exit 可以被 L0 处理 → KVM 直接处理并重新进入 L2
```

关键状态结构：

```c
struct vmcs12 {
    // L1 设置的 VMCS fields (guest 视角)
    gva_t virtual_processor_id;
    u64 io_bitmap_a, io_bitmap_b;
    u64 msr_bitmap;
    u64 vm_exit_msr_store_addr, vm_exit_msr_load_addr;
    u64 vm_entry_msr_load_addr;
    u64 tsc_offset;
    u64 guest_ia32_pat;
    u64 guest_tr_base, guest_gdtr_base, guest_idtr_base;
    struct vmcs12_host_state host_state;      // L2 exit → L1 的状态
    struct vmcs12_guest_state guest_state;     // L2 的 guest state
    ...
};
```

### 4.2 SVM Nested (svm/nested.c, ~60KB)

SVM 嵌套相对简单，因为 `VMRUN` 本身是递归可嵌套的：

```c
struct vmcb *vmcb02;  // 合并 vmcb12 + L1 约束 → 硬件可用格式
```

L1 VMRUN 尝试 → KVM 拦截 → 合并 vmcb12 到 vmcb02 → 设置 vmcb02 的 clean bits → 硬件 VMRUN vmcb02 → #VMEXIT → 如果 L1 需要处理则写回 vmcb12 → VMRUN vmcb01 (返回 L1)。

---

## 5. VM-Entry / VM-Exit 完整生命周期

```
                    ┌──────────────────────────────┐
                    │        KVM_RUN ioctl          │
                    │   (kvm_vcpu_ioctl:4405)       │
                    └──────────────┬───────────────┘
                                   │
                    ┌──────────────▼───────────────┐
                    │ kvm_arch_vcpu_ioctl_run      │
                    │   ├─ 处理 pending 信号        │
                    │   ├─ 处理 pending requests    │
                    │   └─ vcpu->mode =             │
                    │      IN_GUEST_MODE           │
                    └──────────────┬───────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
     ┌────────▼───────┐    ┌───────▼───────┐    ┌──────▼───────┐
     │ Intel VMX      │    │ Intel TDX     │    │ AMD SVM      │
     │ vmx_vcpu_run   │    │ tdx_vcpu_run  │    │ svm_vcpu_run │
     │ (vmx.c:7515)   │    │               │    │              │
     └────────┬───────┘    └───────┬───────┘    └──────┬───────┘
              │                    │                    │
              │ __vmx_vcpu_run     │ SEAMCALL           │ VMRUN
              │ (vmenter.S:47)     │                    │
              │                    │                    │
     ┌────────▼────────────────────────────────────────▼───────┐
     │                    VM-Entry (硬件)                        │
     │  ├─ 加载 guest CR0/CR3/CR4/RIP/RSP/RFLAGS/EFER         │
     │  ├─ 加载 EPT/NPT 指针                                    │
     │  ├─ 加载中断注入信息 (VM-entry interruption info)        │
     │  ├─ 设置 VPID/ASID                                       │
     │  └─ 跳转到 guest RIP                                     │
     └──────────────────────┬──────────────────────────────────┘
                            │
     ┌──────────────────────▼──────────────────────────────────┐
     │                   Guest 代码执行                          │
     │  ├─ 用户程序 / 内核代码                                   │
     │  ├─ 访问 MMIO → EPT/NPT violation                        │
     │  ├─ HLT / PAUSE / CPUID / RDMSR / WRMSR / IN/OUT       │
     │  ├─ 中断 / NMI / 异常                                     │
     │  └─ EOI / TPR 访问 (可硬件虚拟化)                         │
     └──────────────────────┬──────────────────────────────────┘
                            │
     ┌──────────────────────▼──────────────────────────────────┐
     │                    VM-Exit (硬件)                         │
     │  ├─ 保存 guest state → VMCS/VMCB                         │
     │  ├─ 加载 host state → CPU 寄存器                          │
     │  ├─ 写入 exit reason / qualification                     │
     │  └─ 跳转到 host RIP (vmx_vmexit入口 或 #VMEXIT后)       │
     └──────────────────────┬──────────────────────────────────┘
                            │
     ┌──────────────────────▼──────────────────────────────────┐
     │                    KVM Exit 处理                          │
     │  ├─ handle_exit_irqoff (关中断快速处理)                    │
     │  ├─ 更新统计计数器                                        │
     │  │                                                       │
     │  ├─ Fast Path (无锁, 无阻塞):                              │
     │  │   ├─ MSR_WRITE (WRMSR)                                │
     │  │   └─ PREEMPTION_TIMER                                 │
     │  │                                                       │
     │  └─ Slow Path (通过 handle_exit):                         │
     │      ├─ handle_exception_nmi                              │
     │      ├─ handle_external_interrupt                         │
     │      ├─ handle_io / handle_in/out                         │
     │      ├─ handle_cpuid                                      │
     │      ├─ handle_rdmsr / handle_wrmsr                       │
     │      ├─ handle_cr / handle_dr                             │
     │      ├─ handle_ept_violation → MMU page fault 处理       │
     │      ├─ handle_ept_misconfig                              │
     │      ├─ handle_hlt / handle_pause                         │
     │      ├─ handle_interrupt_window / handle_nmi_window       │
     │      └─ handle_apic_access / handle_apic_write            │
     └──────────────────────┬──────────────────────────────────┘
                            │
              ┌─────────────▼──────────────┐
              │ 是否可以在内核态继续运行？   │
              │                             │
              └──YES──┬────────NO──────────┐
                      │                    │
              ┌───────▼──────┐    ┌───────▼──────────────┐
              │ 准备下一轮     │    │ vcpu->run->exit_reason│
              │ VM-Entry      │    │ = KVM_EXIT_IO/MMIO/  │
              │               │    │   HLT/...            │
              │ 循环回到       │    │                      │
              │ kvm_x86_ops   │    │ 返回用户态 (QEMU)     │
              │ ->vcpu_run()  │    │                      │
              └───────────────┘    └──────────────────────┘
```

---

## 6. 性能对比：VMX vs SVM 关键差异

| 特性 | Intel VMX | AMD SVM |
|------|-----------|---------|
| 控制结构 | VMCS (VMREAD/VMWRITE) | VMCB (内存直接访问) |
| 世界切换 | VMLAUNCH/VMRESUME + VM-Exit | VMRUN (含 #VMEXIT) |
| TLB 管理 | VPID | ASID |
| 2D 页表 | EPT | NPT |
| 中断虚拟化 | Posted Interrupt | AVIC |
| VMCS 缓存 | VMCS shadowing (nested) | VMCB clean bits |
| CR 拦截 | VM-execution controls | INTERCEPT_CR 位图 |
| MSR 拦截 | MSR bitmap | MSRPM (MSR Permission Map) |
| 加密 | TDX (SEAM) | SEV/SEV-ES/SEV-SNP |

---

## 下一篇文章

[第3篇：MMU 虚拟化 — EPT/NPT、影子页表与 TDP MMU](./03-mmu.md)
