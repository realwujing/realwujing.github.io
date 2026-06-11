# Linux 内核调度器源码深度解析

📌 **源码**：`git@github.com:torvalds/linux.git`, `torvalds/master`, `eb3f4b7426cf` (v7.1-rc5-26)

---

## 系列文章

| 序号 | 标题 | 核心内容 |
|------|------|----------|
| 1 | [CFS 与 EEVDF — 公平调度与 vruntime](./01-cfs-eevdf.md) | sched_entity, cfs_rq, vruntime, EEVDF, PELT, nice 权重 |
| 2 | [优先级与调度类链 — 从 prio.h 到 sched_class 的完整体系](./02-priority-class.md) | 140级优先级, 5 大调度类, sched_class vtable, pick_next_task 链 |
| 3 | [RT 与 Deadline — 实时调度的两种面孔](./03-rt-dl.md) | SCHED_FIFO/SCHED_RR, EDF+CBS, push/pull migration, 带宽控制 |
| 4 | [负载均衡与调度域 — 多核均衡的艺术](./04-load-balance.md) | sched_domain, sched_group, CPU 拓扑, LB 算法, NOHZ |
| 5 | [sched\_ext — BPF 可扩展调度类](./05-sched-ext.md) | scx, BPF scheduler, dispatch queue, ops 回调 (待完成) |

---

## 调度器架构总览

```
用户态            内核态                                    硬件
  │                │                                        │
  │  syscall / return-to-user                                │
  │                │                                        │
  │         ┌──────▼──────┐                                  │
  │         │  schedule() │  ← 主动让出/被动抢占入口           │
  │         └──────┬──────┘                                  │
  │                │                                        │
  │         ┌──────▼────────┐                                │
  │         │  __schedule() │  ← 核心调度逻辑                  │
  │         └──────┬────────┘                                │
  │                │                                        │
  │         ┌──────▼───────────┐                             │
  │         │ pick_next_task() │  ← 遍历 sched_class 链       │
  │         └──────┬───────────┘                             │
  │                │                                        │
  │    ┌───────────┼───────────┐                             │
  │    │           │           │                             │
  │    ▼           ▼           ▼                             │
  │ stop_sched   dl_sched   rt_sched   fair_sched   idle     │
  │    │           │           │           │          │       │
  │    └───────────┴───────────┴───────────┴──────────┘       │
  │                │                                        │
  │         ┌──────▼──────┐                                  │
  │         │ context_switch│  ← 切换地址空间 + 寄存器          │
  │         └──────┬──────┘                                  │
  │                │                                        │
  │                └──────────────────────────────────────────► CPU
```

**核心路径**：
```
schedule() → __schedule() → pick_next_task() → sched_class 链 → context_switch()
```

---

## 前置知识

阅读本系列需要了解以下基础概念：

| 概念 | 说明 | 相关头文件 |
|------|------|-----------|
| `struct task_struct` | 进程描述符，所有调度信息的容器 | `include/linux/sched.h` |
| `spin_lock` | 自旋锁，调度器中最核心的同步原语 | `include/linux/spinlock.h` |
| RCU | Read-Copy-Update，调度器大量使用 RCU 保护读端 | `include/linux/rcupdate.h` |
| `struct rq` | 每 CPU 的运行队列，调度器操作的根数据结构 | `kernel/sched/sched.h:1131` |
| per-CPU 变量 | 每个 CPU 独立的数据副本 | `include/linux/percpu.h` |

---

## 关键源文件索引

| 文件 | 行数 | 内容 |
|------|------|------|
| `kernel/sched/core.c` | ~11,236 | 调度入口 schedule(), __schedule(), sched_setattr() |
| `kernel/sched/fair.c` | ~14,312 | CFS+EEVDF 完全公平调度 |
| `kernel/sched/rt.c` | ~2,939 | SCHED_FIFO/SCHED_RR 实时调度 |
| `kernel/sched/deadline.c` | ~3,879 | SCHED_DEADLINE EDF+CBS |
| `kernel/sched/sched.h` | ~4,139 | 内部数据结构和宏 (rq, cfs_rq, dl_rq, rt_rq, sched_class) |
| `include/linux/sched.h` | ~2,524 | 公开的 task_struct, sched_entity, sched_attr |
| `include/linux/sched/prio.h` | 46 | MAX_PRIO, MAX_RT_PRIO, NICE_TO_PRIO |
| `kernel/sched/cpupri.c` | ~220 | RT push/pull 用的 CPU 优先级位图 |
| `kernel/sched/topology.c` | ~2,800 | 调度域拓扑构建 |
| `kernel/sched/ext.c` | ~6,000+ | sched_ext BPF 可扩展调度器 |
