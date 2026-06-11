# Linux 上下文切换深度解析 — 四种切换各自保存什么

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源文件：`arch/x86/entry/entry_64.S` `arch/x86/include/asm/switch_to.h` `kernel/sched/core.c` `arch/x86/kvm/vmx/vmenter.S`

---

Linux 中**不是只有一种**"上下文切换"。常见的 4 种切换保存的内容完全不同，由轻到重。

## 1. 四种上下文切换概览

| 类型 | 触发点 | 保存内容 | 代码位置 |
|------|--------|---------|---------|
| **函数调用** | `call` 指令 | `rip` (返回地址) + 可选 `rbp` | x86 ABI 约定 |
| **进程切换** | `schedule()` → `context_switch()` | callee-saved regs (6个) + `rsp` + FPU | `entry_64.S:177` `__switch_to_asm` |
| **用户态↔内核态** | 系统调用/中断/异常 | **全部** GPR (15个) + `rip/rsp/flags/cs/ss` | `entry_64.S:87` `entry_SYSCALL_64` |
| **VM Exit/Entry** | KVM 虚拟机退出 | 全部 GPR + sysregs + 段寄存器 + VMCS | `vmenter.S:47` `__vmx_vcpu_run` |

---

## 2. 函数调用 — 最轻量

```
call foo:
  push rip (隐式)        ← 只有这一个寄存器
  push rbp               ← 可选：编译器要不要保存 frame pointer
```

x86-64 ABI 规定 **caller-saved regs** (rax, rcx, rdx, rsi, rdi, r8-r11) 不需要保存——调用者自己负责。被调用者只保存 **callee-saved regs** (rbx, rbp, r12-r15) 如果要用的话。

```
一般函数调用只保存: rip (返回地址) + 可能 rbp
最多少量 callee-saved regs (6个 × 8B = 48B)
```

---

## 3. 进程切换 — 6 个寄存器 + FPU + 地址空间

### 3.1 context_switch 的流程

```c
// kernel/sched/core.c:5329
context_switch(rq, prev, next)
  │
  ├── prepare_task_switch(rq, prev, next)        // line 5333 — 锁准备
  ├── arch_start_context_switch(prev)             // line 5340
  │
  ├── 地址空间切换 (line 5346-5367):
  │     if (!next->mm) → kernel thread: lazy tlb
  │     else → switch_mm_irqs_off(prev, next->mm) // 写 CR3!
  │
  └── switch_to(prev, next, prev)                 // ★ 汇编寄存器切换
        ＝ __switch_to_asm(prev, next)             // 在 entry_64.S:177
```

### 3.2 __switch_to_asm — 实际保存的寄存器

```asm
# arch/x86/entry/entry_64.S:177-217
SYM_FUNC_START(__switch_to_asm)
    # ① 保存 callee-saved 寄存器到 prev 的栈上
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    # ② ★ 切换栈指针
    movq    %rsp, TASK_threadsp(%rdi)   # prev->thread.sp = rsp
    movq    TASK_threadsp(%rsi), %rsp   # rsp = next->thread.sp

    # ③ FILL_RETURN_BUFFER — 清除间接分支预测器 (Spectre v2 缓解)

    # ④ 恢复 callee-saved 寄存器
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp

    jmp     __switch_to        # C 代码继续处理 FPU 等
SYM_FUNC_END(__switch_to_asm)
```

### 3.3 struct inactive_task_frame

```c
// arch/x86/include/asm/switch_to.h:23-42
struct inactive_task_frame {
    unsigned long r15;
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long bx;        // rbx
    unsigned long bp;        // rbp
    unsigned long ret_addr;  // 返回地址
};
```

**这 7 个字是进程切换保存的最小集合**。切换后新进程恢复时直接从栈上 pop 出这些值，最后一个 `pop rbp` 让栈帧恢复到新进程的上下文。

### 3.4 还要做什么：FPU 和 CR3

这些不是 `__switch_to_asm` 做的，但在 `context_switch` 的整体流程中：

| 组件 | 函数 | 何时做 |
|------|------|--------|
| **FPU 状态** | `switch_fpu_prepare()` / `switch_fpu_finish()` | 如果 prev 用了 FPU → 保存到 prev→thread.fpu，恢复 next→thread.fpu |
| **CR3** | `switch_mm_irqs_off(prev, next)` | 不同进程 → 写 CR3 = 新页表基址 |
| **TLS** | `__switch_to` (C 函数后半部) | 设置 FS/GS 基址（glibc 线程的 TLS） |
| **调试寄存器** | `__switch_to` | 切换 debug registers (dr0-dr7) |

**总结**：进程切换实际保存的内容比表面上看的多：
```
保存: rbp + rbx + r12-r15 + rsp (7 个寄存器, 56B)
FPU: 如果 active → 512B (XSAVE)
CR3: 如果不同进程 → 页表切换
TLS: FS/GS base (glibc 线程)
```

---

## 4. 用户态↔内核态 — 完整 GPR + 硬件上下文

### 4.1 系统调用 — entry_SYSCALL_64

```asm
# arch/x86/entry/entry_64.S:87-106
SYM_CODE_START(entry_SYSCALL_64)
    swapgs                   # ① 切换到内核 GS base
    movq  %rsp, PER_CPU_VAR(cpu_tss_rw + TSS_sp2)  # ② 保存用户栈
    SWITCH_TO_KERNEL_CR3     # ③ 切换到内核页表
    movq  PER_CPU_VAR(cpu_current_top_of_stack), %rsp  # ④ 切换到内核栈

    # ★ ⑤ 构造 struct pt_regs — 硬件自动 push 的部分
    pushq  $__USER_DS               # ss
    pushq  PER_CPU_VAR(..., sp)     # sp (用户栈指针)
    pushq  %r11                     # rflags (SYSCALL 把 rflags 保存到 r11)
    pushq  $__USER_CS               # cs
    pushq  %rcx                     # rip (SYSCALL 把 rip 保存到 rcx)
    pushq  %rax                     # orig_ax

    # ⑥ PUSH_AND_CLEAR_REGS — push 所有 GPR (15 个) 并清零
    PUSH_AND_CLEAR_REGS rax=$-ENOSYS
```

**用户↔内核切换保存的寄存器（按 `struct pt_regs` 顺序）**：

```c
struct pt_regs {
    unsigned long r15;        // ← PUSH_AND_CLEAR_REGS push 的
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long rbp;
    unsigned long rbx;
    unsigned long r11;        // 被覆盖为栈上的 r11 值
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long rax;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long orig_rax;   // ← pushq %rax 推入的 syscall 号
    unsigned long rip;        // ← 硬件自动 push 的
    unsigned long cs;
    unsigned long rflags;
    unsigned long rsp;
    unsigned long ss;
};
```

### 4.2 中断/异常 — 相同的 pt_regs 结构

中断进入和系统调用进入保存**相同的寄存器集**——都是 `struct pt_regs`。唯一的区别是中断进入路径可能还需要 `error_code`（异常会 push 在 `orig_rax` 和 `rip` 之间）。

```
entry_SYSCALL_64:           没有额外的 error_code
error_entry (from 中断):    pt_regs 之外还 push error_code
                            如果是 NMI → struct pt_regs  + NMI 额外信息
```

**核心**：无论是系统调用还是中断，所有的 15 个 GPR 都保存到 `struct pt_regs` 中。内核代码依赖这个结构来访问用户态寄存器。

---

## 5. VM Exit — 最重的上下文切换

```c
// arch/x86/kvm/vmx/vmenter.S:47
SYM_FUNC_START(__vmx_vcpu_run)
    # ① 保存 host callee-saved regs (同进程切换)
    push %rbp / push %rbx / push %r12-r15

    # ② 加载 guest 的 callee-saved regs
    pop %r15 / ... / pop %rbp

    # ③ ★ VMRESUME — 所有其他寄存器通过 VMCS 自动处理
    vmresume

    # ④ ★ vmx_vmexit — 硬件自动还原 host 状态
SYM_INNER_LABEL_ALIGN(vmx_vmexit)
    # 保存 guest 通用寄存器 + 段寄存器到 host 栈
    # 恢复 host 的 callee-saved regs
    # 返回 C 代码 (vmx_vcpu_run)
SYM_FUNC_END(__vmx_vcpu_run)
```

### 5.1 VMCS 中自动切换的内容

VM Exit 时，VMCS 中预先配置的**host-state 区域**自动被硬件加载：

```
VMCS host-state area 自动还原:
  - CR0, CR3, CR4               ← 控制寄存器
  - RSP, RIP, RFLAGS            ← 栈、指令、标志
  - ES, CS, SS, DS, FS, GS, TR, LDTR  ← 全线寄存器
  - IA32_SYSENTER_CS/EIP/ESP    ← 系统调用 MSR
  - IA32_EFER, IA32_PAT          ← 扩展功能 MSR

VMCS guest-state area 自动保存:
  - 同上所有字段的反向
```

### 5.2 VM Exit 的总开销

| 组件 | 操作 | 来源 |
|------|------|------|
| **全部 GPR** | 同进程切换 (6个) + VMCS 控制寄存器 | ~56B + arch |
| **段寄存器** | 全部 7 个 (主机侧自动加载) | VMCS |
| **CR0/CR3/CR4** | 包括页表基址 | VMCS |
| **MSR** | SYSENTER_CS/EIP/ESP, EFER, PAT | VMCS |
| **FPU** | XSAVE/XRSTOR | kvm_fpu 层 |
| **vmsave/vmload** | AMD SVM 用这两个指令代替 VMCS | VMCB |
| **VMCS read/write** | VMREAD/VMWRITE 指令 | Intel VMX |

**总计**：一次 VM Exit 保存和恢复数百字节的状态，是四种切换中最重的。

---

## 6. 四者对比表

| | 函数调用 | 进程切换 | 用户↔内核 | VM Exit |
|---|---|---|---|---|
| **触发器** | `call` 指令 | `schedule()` → `context_switch()` | 系统调用/中断/异常 | VMRUN/VMRESUME |
| **保存的寄存器** | rip + 可选 rbp | rbp/rbx/r12-r15 (6个) | **全部** GPR (15个) + rip/rsp/flags/cs/ss | 全部 GPR + 全段寄存器 + CR0/3/4 + MSR |
| **FPU** | 无 | 可能 (如果被用) | 无 (硬件不碰) | 完整切换 (kvm_fpu) |
| **页表 (CR3)** | 无 | 是 (不同进程) | 是 (KPTI) | 是 (VMCS) |
| **栈切换** | 无 | 是 (prev→next) | 是 (user→kernel) | 是 (guest→host) |
| **TLS (FS/GS)** | 无 | 是 (glibc 线程) | 是 (swapgs) | 是 (VMCS) |
| **代码位置** | ABI 约定 | `entry_64.S:177` | `entry_64.S:87` | `vmenter.S:47` |
| **状态大小** | ~16B | ~128B + FPU | ~160B + KPTI | ~KB + FPU |
| **开销 (周期)** | 微 (流水线刷新) | 低 (~1000-2000) | 中 (~500-1000) | 高 (~5000-10000+) |

---

## 7. 典型场景中的组合

**一次 `read()` 系统调用**经历了两次上下文切换：

```
用户态程序调用 read():
  ① 用户→内核: entry_SYSCALL_64 → 保存 15个GPR + rip/rsp/flags/cs/ss
  ② 内核执行中，需要等磁盘: schedule()
       → context_switch() → __switch_to_asm → 保存 rbp/rbx/r12-r15 + rsp + FPU
  ③ IO 完成: task 被唤醒
       → __switch_to_asm 恢复 rbp/rbx/r12-r15 + rsp + FPU (反向)
  ④ 返回用户态: sysretq → 恢复 15个GPR + rip/rsp/flags/cs/ss (反向)
```

**一个 KVM VM中运行的应用调用 read()** 经历了四次：

```
guest 应用 → read()
  ① guest 用户→guest 内核 (entry_SYSCALL_64 in guest) — 保存 guest GPRs
  ② guest read → 访问 "PCI 设备" → NPF (Nested Page Fault)
       → VM Exit → 保存全部 guest sysregs + GPRs + 段寄存器 (VMCS)
  ③ KVM host -> vhost thread → schedule() → context_switch (进程切换)
  ④ host 恢复 vhost 状态 → vhost IRQ → 唤醒 kvm thread → context_switch
       → VM Entry → 恢复全部 guest sysregs + GPRs (VMCS)
```

---

## 8. 总结

| 切换类型 | 保存原则 | 恢复策略 |
|---------|---------|---------|
| **函数调用** | 只保存 caller 需要恢复的 regs | 退出时 `ret` 从栈拿 rip |
| **进程切换** | 保存 callee-saved regs + rsp + FPU | `__switch_to_asm` 栈切换 + pop |
| **用户↔内核** | 保存全部 GPR + 硬件上下文 | `struct pt_regs` 完整保存/恢复 |
| **VM Exit** | 保存所有上述 + VMCS 宿主区域 | 硬件自动加载 + C 代码补充 |