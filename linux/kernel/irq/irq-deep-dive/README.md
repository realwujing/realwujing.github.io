# IRQ 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)

## 系列目录

| 篇 | 标题 | 文件 |
|---:|------|------|
| 1 | IRQ 描述符与 request_irq — 中断注册的完整生命周期 | [01-descriptor.md](./01-descriptor.md) |
| 2 | IRQ Chip 与 Flow Handlers — Level/Edge/FASTEOI 三种处理方式 | [02-chip-flow.md](./02-chip-flow.md) |
| 3 | /proc/interrupts 与虚假中断检测 — show_interrupts 与 note_interrupt | [03-proc-spurious.md](./03-proc-spurious.md) |
| 4 | 软中断与 Tasklet — 从中断上下文到进程上下文 | [04-softirq-tasklet.md](./04-softirq-tasklet.md) |
| 5 | CPU Hotplug、MSI 与电源管理 — 现代中断管理的进阶话题 | [05-hotplug-msi-pm.md](./05-hotplug-msi-pm.md) |

## 架构总览

```
┌───────────────────────────────────────────────────────────────────────────┐
│                         Linux IRQ 处理全流程                               │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  硬件中断                                                                    │
│     │                                                                      │
│     ▼                                                                      │
│  ┌──────────────┐     CPU 架构入口 (arch_entry)                             │
│  │ arch entry   │     例如 x86: common_interrupt → do_IRQ                   │
│  │              │     arm64: el1_irq → gic_handle_irq                       │
│  └──────┬───────┘                                                          │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  中断域分发                                             │
│  │ handle_arch_irq  │  set_handle_irq() 设置的架构级回调                      │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌────────────────────────┐  通用中断进入点                                  │
│  │ generic_handle_irq_desc│  根据 irq 号在稀疏 radix tree 中查找 irq_desc    │
│  │ → generic_handle_irq   │  irqdesc.h:184, irqdesc.c:688                   │
│  └──────┬─────────────────┘                                                │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  流控处理 (Flow Handler)                              │
│  │ desc->handle_irq  │  根据触发类型分发：                                    │
│  │                   │  handle_level_irq  → mask_ack → ISR → unmask        │
│  │                   │  handle_edge_irq   → ack → ISR                     │
│  │                   │  handle_fasteoi_irq → ISR → eoi                    │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  中断事件处理                                          │
│  │ handle_irq_event │  遍历 action 链表，调用每个 handler                    │
│  │                  │  + add_interrupt_randomness()                         │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ├──► action->handler()     用户注册的 ISR                           │
│         ├──► action->thread_fn()   线程化中断 (如果 IRQF_ONESHOT)            │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  虚假中断检测                                          │
│  │ note_interrupt() │  统计 IRQ_NONE 次数，超过阈值则禁用+轮询                │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐                                                       │
│  │ 设置软中断标志     │  raise_softirq_irqoff(TIMER_SOFTIRQ /               │
│  │                  │  NET_RX_SOFTIRQ / TASKLET_SOFTIRQ / ...)             │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  irq_exit() 中检查                                    │
│  │ irq_exit →       │  if (in_interrupt()) return;                          │
│  │ invoke_softirq() │  if (pending softirqs) → __do_softirq()              │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  软中断处理 (softirq processing)                      │
│  │ __do_softirq()   │  遍历 softirq_vec[], 最多处理 max_restart=10 轮       │
│  │                  │  → net_rx_action                                    │
│  │                  │  → tasklet_action → 执行 tasklet 链表                 │
│  │                  │  → timer_handler (TIMER_SOFTIRQ)                    │
│  │                  │  → rcu_process_callbacks (RCU_SOFTIRQ)              │
│  └──────┬───────────┘                                                      │
│         │                                                                   │
│         ▼                                                                   │
│  ┌──────────────────┐  ksoftirqd 守护线程                                   │
│  │ ksoftirqd/n      │  如果 __do_softirq 处理不完，唤醒 ksoftirqd 继续       │
│  └──────────────────┘                                                      │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

## 核心数据结构关系

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         struct irq_desc                                 │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ struct irq_common_data    (irq.h:149)                             │  │
│  │   unsigned int state_use_accessors; // IRQD_* 状态位               │  │
│  │   unsigned int node;                // NUMA node                   │  │
│  │   void *handler_data;              // chip 私有数据                │  │
│  │   struct msi_desc *msi_desc;       // MSI 描述符                   │  │
│  │   cpumask_var_t affinity;          // CPU 亲和性                   │  │
│  ├───────────────────────────────────────────────────────────────────┤  │
│  │ struct irq_data             (irq.h:181)                           │  │
│  │   u32 mask;                  // 寄存器访问掩码                     │  │
│  │   unsigned int irq;          // Linux IRQ 号                       │  │
│  │   unsigned long hwirq;       // 硬件 IRQ 号                        │  │
│  │   struct irq_chip *chip;     // 底层控制器                        │  │
│  │   struct irq_domain *domain; // 所属中断域                        │  │
│  ├───────────────────────────────────────────────────────────────────┤  │
│  │ unsigned int irq_count;       // 中断计数 (deprecated)             │  │
│  │ struct irqaction *action;     // ISR 链表头                        │  │
│  │ unsigned int depth;           // disable 嵌套深度                  │  │
│  │ unsigned int wake_depth;      // wake depth                        │  │
│  │ raw_spinlock_t lock;          // 自旋锁保护                        │  │
│  │ wait_queue_head_t wait_for_threads; // 线程等待队列                │  │
│  │ unsigned int threads_oneshot; // oneshot 线程计数                  │  │
│  │ struct mutex request_mutex;   // request/free 互斥锁              │  │
│  │ irq_flow_handler_t handle_irq; // 流控处理函数指针                 │  │
│  │ struct proc_dir_entry *dir;   // /proc/irq/N/ 目录                │  │
│  │ struct kstat_irqs *kstat_irqs; // per-CPU 统计                    │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

## 前置知识

- 中断基础知识：硬件中断、中断向量、中断控制器 (PIC/APIC/GIC)
- 内核模块开发基础：ISR 编写、内核线程
- Linux 内核基础：自旋锁、原子操作、per-CPU 变量

## 关键源文件索引

| 文件 | 内容 |
|------|------|
| `include/linux/interrupt.h` | irqaction, request_irq/free_irq, IRQF_* flags |
| `include/linux/irq.h` | irq_common_data, irq_data, irq_chip, IRQD_* 状态位 |
| `include/linux/irqdesc.h` | struct irq_desc, irq_to_desc, 稀疏树 |
| `include/linux/irqhandler.h` | irq_flow_handler_t |
| `kernel/irq/manage.c` | request_threaded_irq, __setup_irq, free_irq, irq_thread |
| `kernel/irq/chip.c` | handle_level_irq, handle_edge_irq, handle_fasteoi_irq |
| `kernel/irq/handle.c` | __handle_irq_event_percpu, handle_irq_event |
| `kernel/irq/irqdesc.c` | irq_to_desc, alloc_desc, generic_handle_irq |
| `kernel/irq/proc.c` | show_interrupts, /proc/irq/ 文件系统 |
| `kernel/irq/spurious.c` | note_interrupt, poll_spurious_irqs |
| `kernel/softirq.c` | __do_softirq, tasklet_action, raise_softirq |
| `kernel/irq/cpuhotplug.c` | irq_calc_affinity_vectors 等 CPU 热插拔 |
| `kernel/irq/msi.c` | MSI 域操作 |
| `kernel/irq/pm.c` | 中断电源管理 |
