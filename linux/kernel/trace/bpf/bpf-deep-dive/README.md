# eBPF 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`kernel/bpf/syscall.c` `kernel/bpf/verifier.c` | 头文件：`include/linux/bpf.h` `include/uapi/linux/bpf.h`

---

eBPF 允许用户态程序在内核中安全地执行沙箱化代码。本系列解剖 bpf 程序加载、verifier 验证和 map 管理的内核实现。

## 文章目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [01-prog-load.md](./01-prog-load.md) | BPF 程序加载 — struct bpf_prog、BPF_PROG_LOAD 系统调用 |
| 2 | [02-verifier.md](./02-verifier.md) | Verifier — 循环检测、类型追踪、边界检查 |
| 3 | [03-maps.md](./03-maps.md) | BPF Maps — BPF_MAP_CREATE、lookup/update 与 map 类型 |

## 前置知识

- 了解 `bpftool prog load` 或 `bpftrace` 基本用法
- 理解 eBPF 指令集基础（64-bit RISC，11 个寄存器）
- 熟悉链表、红黑树、哈希表内核数据结构的用法

## 架构总览

```
  用户态                     内核态
  ──────                    ──────

  BPF 字节码 (.o 文件)
       │
       │ bpf(BPF_PROG_LOAD)
       ▼
  ┌─────────────────┐
  │ syscall.c        │  bpf_prog_load  → bpf_prog_alloc
  │ bpf_prog_load    │                   → bpf_check (verifier)
  └────────┬────────┘                    → bpf_prog_kallsyms_add
           │
  ┌────────▼────────┐
  │ verifier.c       │  do_check  → 模拟每条指令
  │ bpf_check        │            → 跟踪寄存器类型 + 栈状态
  │                  │            → 拒绝无限循环 (CFG 分析)
  └────────┬────────┘
           │
  ┌────────▼────────┐
  │ 每个 prog_type   │  程序附加点 (XDP, tc, kprobe, tracepoint)
  │ 的特定 attach   │  bpf_prog_put  → 程序释放
  └─────────────────┘
```
