# perf 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`kernel/events/core.c` `arch/x86/events/intel/core.c` | 头文件：`include/linux/perf_event.h`

---

本系列解剖 Linux 内核的 **perf_event** 子系统——`perf top`、`perf stat`、`perf record` 背后的内核引擎。

## 文章目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [struct perf_event 与 perf_event_open](./01-perf-event.md) | 核心数据结构、系统调用、PMU 抽象 |
| 2 | [Ring Buffer](./02-ring-buffer.md) | 用户态 mmap、零拷贝轮询、AUX buffer |
| 3 | [x86 PMU 驱动](./03-pmu.md) | Intel 硬件计数器、NMI 采样路径 |
| 4 | [tracepoint/ftrace/bpf 集成](./04-tooling.md) | Tracepoint 桥接、perf record 完整数据流 |

## 前置知识

- 了解 `perf stat`、`perf record`、`perf report` 的用户态用法
- 了解 NMI（Non-Maskable Interrupt）概念
- 熟悉 mmap、ioctl、poll 等文件系统接口
- 基础的内核数据结构知识（list、rbtree、spinlock）

## 架构总览

```
              用户态 perf 工具
                    │
                    │ perf_event_open / ioctl / mmap / read
                    ▼
    ┌───────────────────────────────────────┐
    │     kernel/events/core.c              │
    │     struct perf_event                 │
    │     struct pmu (抽象层)              │
    │     perf_output_sample (写出采样)     │
    └───────────────┬───────────────────────┘
                    │
    ┌───────────────┼───────────────────────┐
    │               │                       │
    ▼               ▼                       ▼
┌───────────┐ ┌───────────┐ ┌───────────────────────┐
│ x86 PMU   │ │ Intel PT  │ │ tracepoint             │
│ 驱动      │ │ AUX Buf   │ │ perf 桥接              │
│ (NMI 采样)│ │ (大块trace)│ │ (ftrace/kprobe/bpf)   │
└───────────┘ └───────────┘ └───────────────────────┘
```
