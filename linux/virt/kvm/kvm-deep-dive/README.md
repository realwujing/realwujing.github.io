# KVM 内核源码深度解析

系列文章深入分析 Linux 内核 KVM (Kernel-based Virtual Machine) 子系统的核心实现。
基于 Linux 最新内核源码，涵盖从核心框架到 x86 硬件虚拟化的完整技术栈。

## 架构总览

```
userspace (QEMU / Cloud Hypervisor / crosvm)
         │
         │ ioctl(fd, KVM_CREATE_VM / KVM_CREATE_VCPU / KVM_RUN / ...)
         ▼
┌─────────────────────────────────────────────────────┐
│                  virt/kvm/kvm_main.c                 │
│  ┌──────────┐  ┌──────────┐  ┌────────────────────┐ │
│  │  VM 管理 │  │ vCPU 调度│  │ memslot / io_bus   │ │
│  └──────────┘  └──────────┘  └────────────────────┘ │
│  ┌──────────┐  ┌──────────┐  ┌────────────────────┐ │
│  │ eventfd  │  │ dirty log│  │ guest_memfd/pfn    │ │
│  └──────────┘  └──────────┘  └────────────────────┘ │
├─────────────────────────────────────────────────────┤
│              arch/x86/kvm/x86.c (x86 通用层)         │
│           struct kvm_x86_ops — vendor 抽象           │
├──────────────────────┬──────────────────────────────┤
│   vmx/vmx.c          │        svm/svm.c             │
│   Intel VMX/VMCS     │        AMD SVM/VMCB          │
│   ├─ nested.c        │        ├─ nested.c           │
│   ├─ tdx.c           │        ├─ sev.c              │
│   └─ posted_intr     │        └─ avic.c             │
├──────────────────────┴──────────────────────────────┤
│              arch/x86/kvm/mmu/ (MMU 虚拟化)          │
│   ┌───────────┐  ┌───────────┐  ┌────────────────┐  │
│   │ TDP MMU   │  │ Shadow MMU│  │ 5-level paging │  │
│   │ (EPT/NPT) │  │ (legacy)  │  │                │  │
│   └───────────┘  └───────────┘  └────────────────┘  │
└─────────────────────────────────────────────────────┘
```

## 前置知识

- 基本的虚拟化概念：hypervisor、VM、vCPU、内存虚拟化、I/O 虚拟化
- Linux 内核模块编程基础：字符设备、ioctl、文件操作
- x86 保护模式基础：页表、段、中断、MSR

## 文章目录

| 篇目 | 标题 | 重点内容 |
|------|------|----------|
| 第1篇 | [KVM 核心 — VM 生命周期、内存槽与 vCPU 调度](./01-core.md) | /dev/kvm、KVM_CREATE_VM/VCPU/RUN、memslot、ioeventfd/irqfd |
| 第2篇 | [x86 虚拟化 — VMX/VMCS 与 SVM/VMCB，VM-Entry/Exit 全解析](./02-vmx-svm.md) | struct kvm_x86_ops、VMCS/VMCB、世界切换、nested、SEV/TDX |
| 第3篇 | [MMU 虚拟化 — EPT/NPT、影子页表与 TDP MMU](./03-mmu.md) | GPA→HPA 两步转换、TDP MMU 锁设计、page fault 处理 |
| 第3.1篇 | [**KVM Stage 1 vs Stage 2 页表 — 两级地址转换深度解析**](./03a-stage1-stage2.md) | ARM64 Stage-2 (VTTBR_EL2) vs x86 EPT/NPT、缺页处理全链路对比 |
| 第3.2篇 | [**ARM64 KVM 虚拟化全景 — VHE/nVHE、vGIC 与系统寄存器**](./03b-arm64-kvm.md) | VHE/nVHE 双路径、arm_exit_handlers 分发、sys_regs.c (5869行)、vGIC、HVC |
| 第4篇 | [设备直通 — VFIO、IOMMU 与中断重映射](./04-vfio.md) | VFIO 框架、IOMMU DMA 重映射、INTx/MSI/MSI-X 直通、IRTE |
| 第5篇 | [半虚拟化 — pvclock、steal time、Hyper-V 启蒙](./05-pv.md) | kvmclock、pvclock 协议、steal time、Hyper-V synic/stimer |

## 源码版本

基于 upstream Linux kernel，关键文件：
- `virt/kvm/kvm_main.c` — KVM 核心框架
- `include/linux/kvm_host.h` — KVM 核心数据结构
- `arch/x86/include/asm/kvm_host.h` — x86 架构 KVM 声明
- `arch/x86/kvm/vmx/`, `arch/x86/kvm/svm/` — Intel/AMD vendor 实现
- `arch/x86/kvm/mmu/` — MMU 虚拟化
- `virt/kvm/eventfd.c`, `virt/kvm/async_pf.c`, `virt/kvm/dirty_ring.c` — 子模块
