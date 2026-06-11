# Linux 上下文切换深度解析 — 四种切换各自保存什么

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源文件：`arch/x86/entry/entry_64.S` `arch/x86/include/asm/switch_to.h` `kernel/sched/core.c` `arch/x86/kvm/vmx/vmenter.S` `arch/arm64/kernel/entry.S` `arch/arm64/include/asm/ptrace.h`

---

Linux 中**不是只有一种**"上下文切换"。常见的 4 种切换保存的内容完全不同，由轻到重。

## 1. 四种上下文切换概览

| 类型 | 触发点 | 保存内容（x86） | 保存内容（arm64） | 代码位置 |
|------|--------|---------------|------------------|---------|
| **函数调用** | `call` 指令 | `rip` + 可选 `rbp` | `lr` (x30) + 可选 `fp` (x29) | ABI 约定 |
| **进程切换** | `schedule()` → `context_switch()` | callee-saved regs (rbp/rbx/r12-r15) + `rsp` + FPU | callee-saved regs (x19-x29) + `sp` + `fp` | `entry_64.S:177` / `entry.S:821` |
| **用户态↔内核态** | 系统调用/中断/异常 | **全部** GPR + rip/rsp/flags/cs/ss → **pt_regs** | **全部** GPR (x0-x30) + sp/pc/pstate → **pt_regs** | `entry_64.S:87` / `entry.S` |
| **VM Exit/Entry** | KVM 虚拟机退出 | 全部 GPR + 全段寄存器 + CR3 + MSR + FPU | 全部 GPR + sysregs (SCTLR_EL1/TTBR0_EL1/TTBR1_EL1等) + FPU | `vmenter.S:47` / `arch/arm64/kvm/hyp/` |

> **pt_regs**：**进程寄存器快照**。内核在每次用户→内核穿越时在栈顶构造这个结构体，保存全部通用寄存器和硬件上下文的快照——系统调用、中断、异常、信号处理全都依赖这个结构体来读取/修改用户态的寄存器状态。

---

## 2. caller-saved vs callee-saved — 为什么进程切换只保存部分寄存器

CPU 有 16 个（x86）或 31 个（arm64）通用寄存器，但进程切换只保存其中 7 个（x86）或 13 个（arm64）。**为什么不全保存？**

答案在于编译器遵循的 **ABI（Application Binary Interface）调用约定**，所有寄存器被分为两类：

```
┌─────────────────────────────────────────────────────────────────┐
│                     寄存器在函数调用中的角色                      │
│                                                                 │
│  caller-saved (调用者保存):                                      │
│    • "我不在乎它们的值"                                         │
│    • 调用者如果要在调用后继续用这些值 → 自己 push 到栈上         │
│    • 被调用者可以随意覆盖，无需恢复                              │
│    • ABI 不保证它们的值在函数调用后仍然有效                      │
│                                                                 │
│  callee-saved (被调用者保存):                                    │
│    • "我保证用完后还原"                                         │
│    • 被调用者如果要改这些寄存器 → 先 push 原值, 返回前 pop 回去 │
│    • 调用者可以放心地放持久数据到这些寄存器里，不用自己 push     │
│    • ABI 保证它们的值在函数调用前后不变                          │
└─────────────────────────────────────────────────────────────────┘
```

### 为什么这样设计？

**性能。** 如果所有寄存器全部 callee-saved，每个函数无论多短都要 push/pop 16 个寄存器——大部分函数根本不需要那么多寄存器。如果全部 caller-saved，调用者在调用前后要反复 push/pop，同样浪费。

折中方案：**传参/临时算的寄存器 caller-saved**（rax, rcx, rdx, rsi, rdi, r8-r11），调用者自己负责值是否穿梭过函数；**持久变量/长期计算的寄存器 callee-saved**（rbx, rbp, r12-r15），编译器把跨语句的变量放这里，不用每个函数做 push/pop。

### 进程切换和这个有什么关系？

**进程切换本质上是内核假装自己是"被调用者"**——它抢占了当前任务，然后"返回"到另一个任务。

- **进程中 caller-saved 寄存器的值**：早在编译器调用 `switch_to` 之前，调用链上的函数已经把需要的数据 push 到栈上了。进程切换不需要帮它们兜底
- **进程中 callee-saved 寄存器的值**：被抢占的任务可能有重要数据（循环变量、指针、中间结果）就在这些寄存器里，编译器认为它们在整个函数内有效，**从未 push 过**。如果切换时丢了这些值，恢复执行时拿到的就是垃圾

这就是 **`__switch_to_asm` 只保存 rbp/rbx/r12-r15 而不管 rax/rcx/rdx/rsi/rdi/r8-r11 的原因**——后者已经有编译器"代劳"了。

### arm64 的分布

| 分类 | 寄存器 | 为什么 |
|------|--------|--------|
| caller-saved | x0-x18 | 传参 (x0-x7)、临时使用 (x9-x15)、内联调用 (x16-x17) |
| callee-saved | x19-x29 | 持久变量——编译器把跨语句存活的值放这里 |
| 特殊 | x18 | 平台寄存器（内核中是 `current` 指针，不参与 ABI） |

---

## 3. 寄存器在上下文中的角色速查

### 2.1 x86-64（16 个 GPR）

| 寄存器 | 在上下文切换中的角色 | ABI 分类 |
|--------|-------------------|---------|
| **rax** | 函数返回值、系统调用号/返回值 | caller-saved |
| **rcx** | 第 4 个函数参数；`SYSCALL` 指令自动把 `rip` 存入 `rcx` | caller-saved |
| **rdx** | 第 3 个函数参数 | caller-saved |
| **rsi** | 第 2 个函数参数 | caller-saved |
| **rdi** | 第 1 个函数参数 | caller-saved |
| **r8-r11** | 第 5-8 个函数参数；`r11` 在 `SYSCALL` 中被硬件覆盖为 `rflags` | caller-saved |
| **rbx** | 通用 callee-saved，**进程切换必须保存** | callee-saved |
| **rbp** | 栈帧基址指针（frame pointer），**进程切换必须保存** | callee-saved |
| **r12-r15** | 通用 callee-saved，**进程切换必须保存** | callee-saved |
| **rsp** | 栈指针——**进程切换核心：`prev->rsp ↔ next->rsp` 是切换的本质** | — |
| **rip** | 指令指针——内核栈上的 pt_regs.rip 是"用户态恢复后从哪里继续执行" | — |
| **rflags** | CPU 标志寄存器（IF/DF/AC等） | — |

### 2.2 arm64（31 个 GPR）

| 寄存器 | 在上下文切换中的角色 | ABI 分类 |
|--------|-------------------|---------|
| **x0-x7** | 函数参数/返回值；x8 是间接结果寄存器 | caller-saved |
| **x9-x15** | 临时寄存器 | caller-saved |
| **x16-x17** | 内联调用临时寄存器 | caller-saved |
| **x18** | 平台寄存器（内核中是 `current` 指针） | 特殊 |
| **x19-x28** | 通用 callee-saved，**进程切换必须保存** | callee-saved |
| **x29 (fp)** | 栈帧指针，**进程切换必须保存** | callee-saved |
| **x30 (lr)** | 链接寄存器（返回地址），**进程切换必须保存** | callee-saved |
| **sp** | 栈指针——**进程切换核心：`prev->sp ↔ next->sp`** | — |
| **pc** | 程序计数器——pt_regs.pc 是异常返回地址 | — |
| **pstate** | 处理器状态（NZCV + DAIF + 异常级别等） | — |

---

## 4. 函数调用 — 最轻量

### 4.1 x86-64

```
call foo:
  push rip (隐式)        ← 只有这一个寄存器
  push rbp               ← 可选：编译器要不要保存 frame pointer
```

**x86-64 ABI**：caller-saved (rax, rcx, rdx, rsi, rdi, r8-r11) 由调用者负责，被调者不用管。被调者只保存 callee-saved (rbx, rbp, r12-r15) 如果用到了。

### arm64

```
bl foo:
  lr (x30) = 返回地址     ← 硬件自动
  stp fp, lr, [sp, #-16]! ← 可选：保存 frame pointer + 返回地址
```

**arm64 ABI**：caller-saved (x0-x18) 由调用者负责。被调者保存 callee-saved (x19-x30) 如果用到了。

**总结**：函数调用只保存返回值/返回地址和可选的栈帧链。不保存通用数据。

---

## 4. 进程切换 — callee-saved regs + FPU + 地址空间

### 4.1 x86-64 — `__switch_to_asm`

```c
// arch/x86/include/asm/switch_to.h:23-42
// 这 7 个字段 = 进程切换保存的最小寄存器集
struct inactive_task_frame {
    unsigned long r15;        // ★ callee-saved — 通用
    unsigned long r14;        // ★ callee-saved — 通用
    unsigned long r13;        // ★ callee-saved — 通用
    unsigned long r12;        // ★ callee-saved — 通用
    unsigned long bx;         // ★ callee-saved — rbx, 通用
    unsigned long bp;         // ★ callee-saved — rbp, 栈帧基址
    unsigned long ret_addr;   // ★ 返回地址 — 恢复后 `ret` 到这里
};
```

```asm
# arch/x86/entry/entry_64.S:177-217
__switch_to_asm:
    pushq   %rbp       # 保存 prev 的栈帧基址
    pushq   %rbx       # 保存 prev 的通用 callee-saved 寄存器
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, TASK_threadsp(%rdi)   # ★ prev->thread.sp = rsp (核心!)
    movq    TASK_threadsp(%rsi), %rsp   # ★ rsp = next->thread.sp (核心!)
    # 恢复 next 的 callee-saved regs (逆向 pop)
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    jmp     __switch_to    # C 函数继续处理 FPU / TLS / debug regs
```

### 4.2 arm64 — `cpu_switch_to`

```asm
# arch/arm64/kernel/entry.S:821-849
cpu_switch_to:
    save_and_disable_daif x11            # 关中断
    mov     x10, #THREAD_CPU_CONTEXT     # CPU context 在 task_struct 中的偏移
    add     x8, x0, x10                  # x8 = &prev->thread.cpu_context
    mov     x9, sp                       # ★ 保存 prev 的栈指针
    stp     x19, x20, [x8], #16          # 保存 2 个 callee-saved regs
    stp     x21, x22, [x8], #16
    stp     x23, x24, [x8], #16
    stp     x25, x26, [x8], #16
    stp     x27, x28, [x8], #16
    stp     x29, x9,  [x8], #16          # ★ fp (x29) + sp (x9 = prev 的 sp)
    str     lr, [x8]                      # ★ lr (x30) = 返回地址
    # --- 加载 next 的 CPU context ---
    add     x8, x1, x10                  # x8 = &next->thread.cpu_context
    ldp     x19, x20, [x8], #16          # 恢复 callee-saved regs
    ldp     x21, x22, [x8], #16
    ldp     x23, x24, [x8], #16
    ldp     x25, x26, [x8], #16
    ldp     x27, x28, [x8], #16
    ldp     x29, x9,  [x8], #16          # ★ fp (x29) + sp (x9 = next 的 sp)
    ldr     lr, [x8]                      # ★ lr (x30) = next 的返回地址
    mov     sp, x9                        # ★ 切换到 next 的栈
    msr     sp_el0, x1                    # 设置 EL0 栈指针
    ptrauth_keys_install_kernel x1        # 恢复指针认证密钥
    scs_save x0                           # Shadow Call Stack: 保存 prev
    scs_load_current                      # Shadow Call Stack: 加载 current
    restore_irq x11                       # 恢复中断
    ret                                   # 返回到 cpu_switch_to 被调用的地方
```

### 4.3 还要做什么：FPU、CR3/TTBR、TLS

这些不是汇编层做的，在高级层 `context_switch()` 中处理：

| 组件 | x86-64 (`core.c:5329`) | arm64 |
|------|----------------------|-------|
| **FPU** | `switch_fpu_prepare()` / `switch_fpu_finish()` | `fpsimd_thread_switch()` |
| **页表** | `switch_mm_irqs_off()` → 写 CR3 | `check_and_switch_context()` → 写 TTBR0_EL1 |
| **TLS** | `__switch_to` 后段 → 写 FS/GS base | `tls_thread_switch()` → 写 TPIDRRO_EL0 |
| **调试寄存器** | `__switch_to` → 切换 dr0-dr7 | `hw_breakpoint_thread_switch()` |
| **指针认证** | 无 | `ptrauth_keys_install_kernel` — 每个进程独立 PAC 密钥 |
| **Shadow Call Stack** | 无 | `scs_save` / `scs_load_current` — 防止 ROP 攻击 |

### 4.4 进程切换保存内容的总结

| 架构 | 保存的寄存器 | 数量 | 大小 | rsp/sp 的角色 |
|------|------------|------|------|--------------|
| **x86-64** | rbp, rbx, r12-r15, rsp | 7 个 | ~56B | `prev->thread.sp = rsp; rsp = next->thread.sp` — **核心切换** |
| **arm64** | x19-x29, sp, lr (x30) | 13 个 | ~104B | `stp x29, x9 (sp) ... mov sp, x9` — flow 同 x86 |

---

## 5. 用户态↔内核态 — 完整 pt_regs + 硬件上下文

### 5.1 x86-64 — `entry_SYSCALL_64`

```asm
# arch/x86/entry/entry_64.S:87-109
entry_SYSCALL_64:
    swapgs                     # ① 切换内核 GS base
    movq  %rsp, PER_CPU_VAR(cpu_tss_rw + TSS_sp2) # ② 保存用户 sp
    SWITCH_TO_KERNEL_CR3       # ③ 切换用户页表→内核页表 (KPTI)
    movq  PER_CPU_VAR(cpu_current_top_of_stack), %rsp # ④ 切到内核栈
    pushq  $__USER_DS          # ⑤ 构造 pt_regs: ss
    pushq  PER_CPU_VAR(..., sp)  # rsp (用户栈指针)
    pushq  %r11                # rflags (SYSCALL 把原 rflags 存到 r11)
    pushq  $__USER_CS          # cs
    pushq  %rcx                # rip (SYSCALL 把原 rip 存到 rcx)
    pushq  %rax                # orig_ax (系统调用号)
    PUSH_AND_CLEAR_REGS rax=$-ENOSYS  # ⑥ push 全部 15 个 GPR 并清零
```

**构造出的 pt_regs 结构体 (进程寄存器快照)**：

```c
// x86-64 struct pt_regs — 用户态寄存器在内核栈上的完整快照
struct pt_regs {
    unsigned long r15;        // ← PUSH_AND_CLEAR_REGS push 的第一个
    unsigned long r14;        //     通用寄存器
    unsigned long r13;
    unsigned long r12;
    unsigned long rbp;        //     用户态栈帧基址
    unsigned long rbx;        //     用户态通用寄存器
    unsigned long r11;        //     用户态 r11 值
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long rax;        //     用户态 rax 值 (syscall 返回后覆盖)
    unsigned long rcx;        //     用户态 rcx 值 (SYSCALL 硬件覆盖后才 push)
    unsigned long rdx;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long orig_rax;   // ← pushq %rax — 保存系统调用号
    unsigned long rip;        // ← 硬件自动 push — 用户态恢复后的指令地址
    unsigned long cs;         // ← 硬件自动 push — 用户态代码段
    unsigned long rflags;     // ← 硬件自动 push — 用户态标志寄存器 (IF/DF/AC等)
    unsigned long rsp;        // ← 硬件间接 push — 用户态栈指针
    unsigned long ss;         // ← 硬件间接 push — 用户态栈段
};
```

### 5.2 arm64 — `kernel_ventry`

```c
// arch/arm64/include/asm/ptrace.h:156-170
struct pt_regs {
    union {
        struct user_pt_regs user_regs;   // x0-x30 + sp + pc + pstate
        struct {
            u64 regs[31];                // x0-x30 ★ 全部 31 个 GPR
            u64 sp;                      // ★ 用户态栈指针
            u64 pc;                      // ★ 异常返回地址
            u64 pstate;                  // ★ 处理器状态 (NZCV/DAIF/EL)
        };
    };
    u64 orig_x0;                         // 原始 x0 (系统调用号)
    s32 syscallno;                       // 内核解码后的系统调用号
    u32 pmr;                             // GIC 中断优先级掩码
    u64 sdei_ttbr1;                      // SDEI 异常时的 TTBR1 值
};
#define PT_REGS_SIZE  ((int)sizeof(struct pt_regs))  // 约 288 字节
```

**arm64 异常入口构造 pt_regs**：

```asm
# entry.S: kernel_ventry 宏
sub  sp, sp, #PT_REGS_SIZE          # ① 在内核栈上预留 288 字节
stp  x0, x1,   [sp, #16 * 0]        # ② 保存全部 31 个 GPR
stp  x2, x3,   [sp, #16 * 1]
# ... 
stp  x28, x29, [sp, #16 * 14]
str  x30,      [sp, #16 * 15]       # lr (x30)
# x29 不是正式的"寄存器 29"——它就是 frame pointer
```

**arm64 和 x86-64 在进入内核时都保存完整的 pt_regs。唯一的区别是 arm64 不需要 `swapgs`（用 `sp_el0`/`sp_el1` 切换栈，用 `tpidrro_el0` 代替 GS），不需要硬件 push（全部由软件 `stp` 指令完成）。**

### 5.3 中断/异常 — 相同的 pt_regs 结构

中断/异常进入路径和系统调用进入路径保存**相同的寄存器集**——都是 `struct pt_regs`。唯一的额外项是异常可能在 `orig_rax` 和 `rip` 之间 push 一个 `error_code`。

```
entry_SYSCALL_64:       没有额外的 error_code
error_entry (from 中断):  pt_regs 之外还 push error_code
NMI:                     pt_regs + NMI 额外信息
arm64 kernel_ventry:     全部 stp 指令保存到栈上
```

---

## 6. VM Exit — 最重的上下文切换

### 6.1 x86-64 — `__vmx_vcpu_run`

```c
// arch/x86/kvm/vmx/vmenter.S:47
__vmx_vcpu_run:
    push %rbp / push %rbx / push %r12-r15    # ① 保存 host callee-saved (同进程切换)
    pop %r15 / ... / pop %rbp                # ② 加载 guest callee-saved
    vmresume                                  # ③ ★ 进入 guest
vmx_vmexit:                                   # ④ ★ 硬件自动还原 host 上下文
    # 保存 guest 通用寄存器→host 栈
    # 恢复 host callee-saved
```

**VMCS 自动切换的内容**：

```
VMCS host-state area → 硬件在 VM Exit 时自动加载:
  CR0, CR3, CR4             ← 控制寄存器（含页表基址）
  RSP, RIP, RFLAGS          ← 栈、指令、标志
  ES, CS, SS, DS, FS, GS    ← 全 7 个段寄存器
  TR, LDTR, GDTR, IDTR      ← 任务/中断
  IA32_SYSENTER_CS/EIP/ESP  ← 系统调用 MSR
  IA32_EFER, IA32_PAT        ← 扩展功能

VMCS guest-state area → 硬件在 VM Exit 时自动保存:
  同上所有字段的反向
```

### 6.2 arm64 — `__kvm_vcpu_run_vhe`

arm64 VHE 模式下的 guest switch（`arch/arm64/kvm/hyp/vhe/switch.c:572`）不通过 VMCS，而是直接**显式保存/恢复 sysregs**：

```
保存 host:
  __sysreg_save_el1_state(host_ctxt)  # SCTLR_EL1, TCR_EL1, TTBR0_EL1, MAIR_EL1, etc.
恢复 guest:
  __sysreg_restore_el1_state(guest_ctxt)
加载 Stage-2:
  __load_stage2(vcpu->arch.hw_mmu)   # 写 VTTBR_EL2 + VTCR_EL2
进入 guest:
  __guest_enter() → ERET to EL1
退出:
  fixup_guest_exit → 保存 guest sysregs → 恢复 host
```

### 6.3 VM Exit 总开销

| 组件 | x86-64 | arm64 |
|------|--------|-------|
| **全部 GPR** | callee-saved (6) + VMCS | 同进程切换 (13) |
| **段寄存器** | 全部 7 (VMCS) | 无（arm64 无段寄存器） |
| **CR 寄存器** | CR0/3/4 (VMCS) | SCTLR_EL1, TCR_EL1 (软件) |
| **页表** | CR3 (VMCS) | TTBR0_EL1 (软件) + VTTBR_EL2 (Stage-2) |
| **FPU** | XSAVE/XRSTOR | fpsimd_save/restore |
| **MSR/sysreg** | SYSENTER, EFER, PAT (VMCS) | MAIR_EL1, SP_EL0, TPIDRRO_EL0, etc. |
| **总大小** | ~KB + FPU | ~500B + FPU |

---

## 7. 四者对比表 + 一层比一层重的根本原因

### 7.1 为什么越往下保存越多？

答案不是"越往下越重要"，而是**越往下，切换前需要兜底的信息就越多**：

```
┌──────────────────────┬──────────────────────────────────────────────────────┐
│ 切换类型              │ 为什么只保存／全部保存这些                              │
├──────────────────────┼──────────────────────────────────────────────────────┤
│ 函数调用              │ 编译器知道调用发生在哪里。调用前 caller-saved 的值       │
│                      │ 如果后续还需要，编译器自己 push 了。共享栈，不用切栈。     │
├──────────────────────┼──────────────────────────────────────────────────────┤
│ 进程切换              │ 被抢占的任务不知道自己要"睡觉"——它可能刚执行到函数中间     │
│                      │ 的某条指令，callee-saved 寄存器里存着循环变量、指针等。     │
│                      │ caller-saved 的值早在调用 context_switch 之前已经被      │
│                      │ 上层函数调用链的编译器 push 到栈上了，不用再管。           │
│                      │ 不同进程 = 不同栈 + 不同页表 → 要切 rsp + CR3。          │
├──────────────────────┼──────────────────────────────────────────────────────┤
│ 用户态 ↔ 内核态       │ 中断/异常可能发生在用户态**任意一条指令**之后。CPU 不知道  │
│                      │ 用户在哪个寄存器里存了什么东西——没有调用链帮你 push 过     │
│                      │ 任何值。必须全部 GPR + rip/rsp/flags/cs/ss 完整快照，     │
│                      │ 一个都不能少。而且用户栈和内核栈是两块独立的物理内存，       │
│                      │ 切换时必须换 rsp + 页表。                              │
├──────────────────────────────────────────────────────────────────────┤
│ VM Exit              │ 硬件层面：CPU 不知道 guest 在哪个寄存器、哪个段、哪个      │
│                      │ 页表存了数据。中断+进程切换需要保存的所有东西（GPR、页表、   │
│                      │ FPU、栈），VM Exit **全都要**。还得额外管 guest 不能感知   │
│                      │ 到自己被暂停了——所以段寄存器、控制寄存器、MSR、sysreg，     │
│                      │ 一个都不能漏。                              │
│                      │ 逻辑上：VM Exit 需要用 VMCS/VMCB 保存 guest 状态、加载     │
│                      │ host 状态——它的"调用链"是 host→VMRUN→guest→vmexit→host，   │
│                      │ guest 完全没有参与这个协商，和中断一样是"硬抢"。            │
│                      │ 本质上：guest 的整个 CPU 状态 = 中断的 GPR + 进程切换的     │
│                      │ callee-saved + 额外的控制寄存器 + 段寄存器 + MSR。         │
└──────────────────────┴──────────────────────────────────────────────────────┘
```

**逐层递增的本质**：每一层都是"前一层需要保存的全部内容，再加上这一层独有的信息"：

```
函数调用   =  返回地址
进程切换   =  返回地址 + callee-saved regs + 别人的栈 + 别人的页表
中断穿越   =  进程切换的 callee-saved + **全部** caller-saved (因为CPU不知道哪个重要)
              + 全部硬件上下文 (rip/rsp/flags/cs/ss)
VM Exit    =  中断穿越的全部 GPR + 段寄存器 + 控制寄存器 + MSR/sysreg + FPU
              + guest 无法参与协商 → 必须硬件完全隔离
```

### 7.2 对比表

| | 函数调用 | 进程切换 | 用户↔内核 | VM Exit |
|---|---|---|---|---|
| **保存策略** | 只保存必要的 | 只保存 callee-saved | **全部** GPR + 硬件上下文 | 全部 GPR + 控制寄存器 + 段 + MSR + FPU |
| **为什么这个策略** | 编译器已经 push 了 caller-saved | caller-saved 在上层调用链的栈上，不需要重复保存 | 中断发生在任意指令后，所有 15/31 个 GPR 都可能存着活数据 | guest 完全不参与协商，必须完整隔离 |
| **x86 保存寄存器** | rip + 可选 rbp | rbp/rbx/r12-r15 + rsp (7个) | **全部** 15 GPR + rip/rsp/flags/cs/ss → pt_regs | 全部 GPR + 段寄存器 + CR0/3/4 + MSR + FPU |
| **arm64 保存寄存器** | lr + 可选 fp | x19-x29 + sp + fp + lr (13个) | **全部** 31 GPR + sp/pc/pstate → pt_regs | 全部 GPR + SCTLR/TCR/TTBR + FPU + VTTBR |
| **保存大小** | ~16B | ~56B (x86) / ~104B (arm64) | ~160B (x86) / ~288B (arm64) + 页表 | ~KB + FPU |
| **页表** | 无 | 是 (CR3/TTBR0) | 是 (KPTI) | 是 (含 Stage-2) |
| **栈切换** | 无 | ★ 是 (核心机制) | 是 | 是 |
| **开销** | 微 | ~1-2K cycles | ~500-1K cycles | ~5-10K+ cycles |

---

## 8. 典型场景

**一次 `read()` 系统调用**经历 2 次穿越：

```
① 用户→内核: entry_SYSCALL_64 → 构造 pt_regs ← 保存 15 GPR
② 进程切换: schedule() → __switch_to_asm → 保存 rbp/rbx/r12-r15 + rsp + FPU
③ 唤醒: __switch_to_asm 恢复 rbp/rbx/r12-r15 + rsp + FPU (反向)
④ 内核→用户: sysretq → 从 pt_regs 恢复 15 GPR + rip/rflags/rsp/cs/ss
```

**KVM 中 guest 读盘**经历 4 次穿越：

```
① guest 用户→内核 (entry_SYSCALL_64 in guest) — pt_regs
② NPF → VM Exit (vmx_vmexit) — 全部 sysregs + VMCS
③ host io worker → context_switch — callee-saved regs + FPU
④ IO done → 唤醒 vCPU → VM Entry — 全部还原 → 回到 guest
```

---

## 9. 总结

| 切换类型 | 保存原则 | 恢复策略 |
|---------|---------|---------|
| **函数调用** | 只保存 caller 需要恢复的 regs | `ret` 从栈/lr 拿返回地址 |
| **进程切换** | 保存 callee-saved + rsp + FPU | `__switch_to_asm` / `cpu_switch_to` 栈切换 + pop/ldp |
| **用户↔内核** | 保存全部 GPR + 硬件上下文 → pt_regs | `struct pt_regs` 完整保存/恢复 |
| **VM Exit** | 保存以上 + VMCS/VTTBR | 硬件自动加载 (x86) / 软件保存恢复 (arm64) |
