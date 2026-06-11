# 第三篇：/proc/interrupts 与 虚假中断检测 — show_interrupts 与 note_interrupt

> 源码：kernel/irq/proc.c kernel/irq/spurious.c | 头文件：include/linux/interrupt.h

系列目录：[IRQ 内核源码深度解析](./README.md)

---

## 1. 概述

前两篇分析了中断描述符结构和流控处理机制。当 ISR 被调用后，有两个关键的后处理步骤：
1. **统计计数**：累加 per-CPU 中断计数，供 `/proc/interrupts` 展示
2. **虚假中断检测**：判断中断是否被正确处理，如果没有设备认领则启动诊断机制

本文将深入 `show_interrupts()` 的数据来源和 `note_interrupt()` 的虚假检测算法 — 这是内核最优雅的防御机制之一。

---

## 2. per-CPU 中断计数 — kstat_irqs

### 2.1 数据结构

```c
// include/linux/kernel_stat.h
struct kernel_stat {
    unsigned int irqs_sum;             // 所有 IRQ 的计数之和 (快速检查用)
    unsigned int softirqs_sum;
};

DECLARE_PER_CPU(struct kernel_stat, kstat);

// irq_desc 中的计数指针
struct irq_desc {
    unsigned int __percpu *kstat_irqs;  // per-CPU 中断计数数组
    // ...
};
```

每个 IRQ 有独立的 per-CPU 数组，记录了每个 CPU 上该中断的触发次数。

### 2.2 累积计数

```c
// kernel/irq/internal.h (内联)
static inline void kstat_incr_irqs_this_cpu(struct irq_desc *desc)
{
    __this_cpu_inc(*desc->kstat_irqs);
    __this_cpu_inc(kstat.irqs_sum);
}
```

每次中断进入 `__handle_irq_event_percpu()` 时，先累加此计数器 (无论 ISR 是否真的处理了该中断)：

```c
// kernel/irq/handle.c:185
irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc)
{
    // ★ 第一行: 无条件累加计数
    __kstat_incr_irqs_this_cpu(desc);

    for_each_action_of_desc(desc, action) {
        res = action->handler(irq, action->dev_id);
        // ...
    }
    note_interrupt(irq, desc, retval);
    return retval;
}
```

---

## 3. /proc/interrupts — show_interrupts (proc.c:453)

### 3.1 输出格式

```text
           CPU0       CPU1       CPU2       CPU3
  1:     123456      10000     200000     150000  IR-IO-APIC  1-edge      i8042
  7:          2          0          0          0  IR-IO-APIC  1-edge      parport0
 12:          3          0          0          0  IR-IO-APIC  1-edge      i8042
 16:     123456     100000     200000     100000  IR-IO-APIC  16-fasteoi  ehci_hcd:usb1
 23:     432109     200000     100000     300000  IR-IO-APIC  23-fasteoi  ehci_hcd:usb2
 24:          0          0          0          0  DMAR-MSI    0-edge      dmar0
 25:          0          0          0          0  DMAR-MSI    1-edge      dmar1
 26:          0          0          0          0  IR-PCI-MSIX 0000:00:1f.3 0-edge
 NMI:      12345      10000       5000      15000   Non-maskable interrupts
 LOC:   12345678   10000000    5000000   15000000   Local timer interrupts
 SPU:          0          0          0          0   Spurious interrupts
 PMI:      12345      10000       5000      15000   Performance monitoring interrupts
 IWI:          0          0          0          0   IRQ work interrupts
 RTR:          0          0          0          0   APIC ICR read retry
 RES:    1234567     500000     600000     450543   Rescheduling interrupts
 CAL:    1234567     600000     500000     350543   Function call interrupts
 TLB:    1234567     400000     300000     450543   TLB shootdowns
 TRM:      12345      10000       5000      15000   Thermal event interrupts
 THR:          0          0          0          0   Threshold APIC interrupts
 DFR:          0          0          0          0   Deferred Error APIC interrupts
 MCE:          0          0          0          0   Machine check exceptions
 MCP:        123        456        789        123   Machine check polls
 ERR:          0
 MIS:          0
 PIN:          0          0          0          0   Posted-interrupt notification event
 NPI:          0          0          0          0   Nested posted-interrupt event
 PIW:          0          0          0          0   Posted-interrupt wakeup event
```

### 3.2 源码分析

```c
// kernel/irq/proc.c:453
int show_interrupts(struct seq_file *p, void *v)
{
    unsigned long flags, any_count = 0;
    int i = *(loff_t *) v, j;
    struct irq_desc *desc;
    struct irqaction *action;

    // ───── 第一遍: 输出标题行 ─────
    if (i == 0) {
        seq_puts(p, "           ");
        for_each_online_cpu(j)
            seq_printf(p, "CPU%-8d", j);
        seq_putc(p, '\n');
    }

    // ───── 第二遍: 遍历所有 IRQ ─────
    if (i < nr_irqs) {
        desc = irq_to_desc(i);
        if (!desc)
            return 0;

        raw_spin_lock_irqsave(&desc->lock, flags);

        // (A) 打印每个 CPU 的计数值
        // kstat_irqs 是 per-CPU 的 unsigned int 数组
        for_each_online_cpu(j)
            any_count |= data_race(*per_cpu_ptr(desc->kstat_irqs, j));

        if (any_count || !irq_settings_is_hidden(desc)) {
            seq_printf(p, "%3d: ", i);

            // 输出每个 CPU 的计数值
            for_each_online_cpu(j)
                seq_printf(p, "%10u ",
                          data_race(*per_cpu_ptr(desc->kstat_irqs, j)));

            // (B) 打印 chip 名称和触发类型
            // desc->irq_data.chip->name → "IR-IO-APIC", "GICv3", ...
            // irq_get_trigger_type() → "edge" / "level"
            seq_printf(p, " %-14s", desc->irq_data.chip->name
                       ? : "None");

            seq_printf(p, " %-10s",
                       desc->irq_data.domain ? "edge" : desc->irq_data.hwirq);

            // (C) 打印所有注册的 ISR 名称
            // action->name → "i8042", "ehci_hcd:usb1", ...
            action = desc->action;
            if (action) {
                seq_printf(p, "  %s", action->name);
                while ((action = action->next) != NULL)
                    seq_printf(p, ", %s", action->name);
            }

            seq_putc(p, '\n');
        }

        raw_spin_unlock_irqrestore(&desc->lock, flags);
    }

    return 0;
}
```

### 3.3 数据流动图

```
硬件中断到达
    │
    ▼
__handle_irq_event_percpu()
    │
    ├── ★ __kstat_incr_irqs_this_cpu(desc)
    │      │
    │      └── per_cpu_ptr(desc->kstat_irqs)[cpu]++
    │           (per-CPU 的无锁自增)
    │
    └── handle ISR
         │
         ▼
    note_interrupt(irq, desc, retval)
         │
         ▼
    /proc/interrupts 读取:
         │
         ▼
    show_interrupts(seq_file *p, void *v)
         │
         ├── 遍历 nr_irqs
         ├── 对每个 irq: 读 kstat_irqs per-CPU 数组
         ├── 打印 chip->name
         ├── 打印 hwirq / trigger type
         └── 遍历 action 链表, 打印 action->name
```

**性能考量**：
- `kstat_incr_irqs_this_cpu()` 使用 `__this_cpu_inc()` — 单 CPU 上无锁
- `show_interrupts()` 使用 `data_race()` 读取 — 不需要精确一致性，允许短暂的不准确

---

## 4. /proc/irq/N/ 目录结构 (proc.c:331)

```c
// kernel/irq/proc.c:331
void register_irq_proc(unsigned int irq, struct irq_desc *desc)
{
    char name [16];
    struct proc_dir_entry *entry;

    // ... 如果 /proc/irq 不存在则创建 /proc/irq

    sprintf(name, "%d", irq);

    // 创建 /proc/irq/N/ 目录
    desc->dir = proc_mkdir(name, root_irq_dir);

    // 创建子文件:
    // /proc/irq/N/smp_affinity         — 位掩码，设置 CPU affinity
    // /proc/irq/N/smp_affinity_list    — 列表格式，设置 CPU affinity
    // /proc/irq/N/affinity_hint        — 建议的 affinity
    // /proc/irq/N/spurious             — 虚假中断统计
    // /proc/irq/N/node                 — NUMA node
    // /proc/irq/N/effective_affinity   — 实际生效的 affinity
    // /proc/irq/N/effective_affinity_list
}
```

### 各文件用途

| 文件 | 权限 | 用途 |
|------|------|------|
| `smp_affinity` | rw | 设置/查看 CPU affinity 位掩码 |
| `smp_affinity_list` | rw | 设置/查看 CPU affinity 列表 |
| `affinity_hint` | rw | 驱动程序建议的 best CPU affinity |
| `spurious` | r | 虚假中断计数和最后未处理时间 |
| `node` | r | NUMA node ID |
| `effective_affinity` | r | 实际起效的 affinity (硬件限制后) |
| `effective_affinity_list` | r | 列表格式的实际 affinity |

---

## 5. 虚假中断检测 — note_interrupt (spurious.c:222)

`note_interrupt()` 是内核的多层防御机制，用于检测"没有设备认领"的中断。如果检测到长期未处理的中断，内核会禁用该中断线并启动轮询。

### 5.1 完整的决策树

```
note_interrupt(irq, desc, action_ret)
│
├── action_ret == IRQ_HANDLED?
│   │
│   ├── YES: 一切正常，直接返回
│   │
│   └── NO: 继续检测
│
├── action_ret == IRQ_WAKE_THREAD?
│   │
│   └── 启动 SPURIOUS_DEFERRED 延迟检测
│       (在线程完成后由 irq_finalize_oneshot 继续检测)
│
├── action_ret == IRQ_NONE
│   │
│   ├── 如果是轮询中断 (poll_spurious_irqs): 不检测
│   │
│   ├── 如果是 IRQS_POLL_INPROGRESS: 不检测
│   │
│   ├── 增加 irqs_unhandled 计数
│   │
│   ├── 检查 100ms 的老化窗口
│   │   └── 如果距上一次 IRQ_NONE 超过 100ms:
│   │       对 irqs_unhandled 进行衰减
│   │
│   ├── 尝试错误路由 IRQ (irqfixup 内核参数)
│   │
│   ├── 检查 "999/1000 规则"
│   │   └── 如果最近 100,000 次中断中 99,900 次没处理:
│   │       设置 IRQS_SPURIOUS_DISABLED → 启动轮询
│   │
│   └── 如果轮询成功 (有 handler 响应):
│       清除 SPURIOUS, 恢复正常处理
│
└── action_ret 无效值 (不是任何 IRQ 返回值)
    └── 报告 BAD_IRQ (严重错误, 可能是内核 bug)
```

### 5.2 源码分析

```c
// kernel/irq/spurious.c:222
void note_interrupt(unsigned int irq, struct irq_desc *desc,
                    irqreturn_t action_ret)
{
    // ───── 情况1: 正常处理 ─────
    if (action_ret == IRQ_HANDLED) {
        // 一切正常, 清除未处理状态
        desc->irqs_unhandled = 0;
        return;
    }

    // ───── 情况2: 线程被唤醒 ─────
    if (action_ret == IRQ_WAKE_THREAD) {
        // 将在线程完成后再检测 (SPURIOUS_DEFERRED)
        desc->irqs_unhandled = 0;
        desc->threads_handled |= SPURIOUS_DEFERRED;
        return;
    }

    // ───── 情况3: 轮询中断跳过 ─────
    if (irq_settings_is_polled(desc)) {
        desc->irqs_unhandled = 0;
        return;
    }

    if (desc->istate & IRQS_POLL_INPROGRESS)
        return;

    // ───── 情况4: 无效的 action_ret ─────
    if (action_ret == IRQ_NONE) {
        /*
         * 100ms 的老化窗口
         *
         * 如果两次 IRQ_NONE 之间的时间超过 100ms:
         *   将 irqs_unhandled 除以 2 (衰减)
         *
         * 原因: 短时间的突发 IRQ_NONE 可能是共享线上的正常现象
         *      但持续的大量 IRQ_NONE 说明中断线有问题
         */
        time_after(desc->last_unhandled + HZ / 10, jiffies)
            ? desc->irqs_unhandled++         // 100ms 内: 累积
            : desc->irqs_unhandled >>= 1;     // 超过 100ms: 衰减一半

        desc->last_unhandled = jiffies;

        /*
         * 错误路由中断修复 (irqfixup 内核参数)
         *
         * 如果检测到中断线可能是错误路由的 (例如 PCI IRQ steering 失败):
         *   试一下所有共享中断的其他注册设备
         */
        if (unlikely(irqfixup)) {
            if (try_one_irq(desc, true))
                return;
        }
    } else {
        /*
         * action_ret 是无效返回值 (不是 IRQ_HANDLED / IRQ_NONE / IRQ_WAKE_THREAD)
         * → 这是一个严重的内核 bug!
         */
        __report_bad_irq(desc, action_ret);
    }

    /*
     * 999/1000 规则: 虚假中断检测
     *
     * 检查多少个中断没有被处理
     * 阈值: irqs_unhandled > 99900
     * → 意味着在最近约 100,000 次中断中，最多只有 100 次被处理
     * → 显然这条中断线有问题
     */
    if (unlikely(irqs_unhandled > 99900)) {
        /*
         * 设置 SPURIOUS_DISABLED 标志
         * 启动轮询 (poll_spurious_irqs)
         */
        __report_bad_irq(desc, action_ret);
        desc->istate |= IRQS_SPURIOUS_DISABLED;
        desc->depth++;
        irq_disable(desc);              // 禁止中断

        mod_timer(&poll_spurious_irq_timer,
                  jiffies + POLL_SPURIOUS_IRQ_INTERVAL);  // 1 tick
        desc->irqs_unhandled = 0;
    }
}
```

### 5.3 SPURIOUS_DEFERRED — 延迟检测 (spurious.c:220)

```c
// kernel/irq/spurious.c:220
// SPURIOUS_DEFERRED 宏
#define SPURIOUS_DEFERRED   0x80000000
```

当线程化 IRQ 的 PRIMARY handler 返回 `IRQ_WAKE_THREAD` 时：
1. PRIMARY handler 传递了事件给线程
2. 线程可能会处理事件或报告 IRQ_NONE
3. 延迟到线程完成后才做 spurious 检测

```c
// 在线程完成后的 irq_finalize_oneshot 中:
void irq_finalize_oneshot(struct irq_desc *desc, struct irqaction *action)
{
    /*
     * 线程完成后:
     *   如果 threads_handled 设置了 SPURIOUS_DEFERRED:
     *     调用 handle_irq_surplus_thread() 检查
     *     确认线程是否真的处理了中断
     */
    if (desc->istate & IRQS_ONESHOT) {
        if (!(desc->istate & IRQS_ONESHOT_ACTIVE) && thread_handler_done) {
            // 延迟检测: 线程是否处理了中断?
            handle_irq_surplus_thread(desc, action);
        }
    }
}
```

---

## 6. "nobody cared" — __report_bad_irq (spurious.c:152)

当内核检测到持续的虚假中断时，通过 `__report_bad_irq()` 输出诊断信息。

```c
// kernel/irq/spurious.c:152
static void __report_bad_irq(struct irq_desc *desc, irqreturn_t action_ret)
{
    struct irqaction *action;

    // 防止日志洪泛: 仅输出一次
    if (desc->istate & IRQS_REPORTED)
        return;
    desc->istate |= IRQS_REPORTED;

    printk(KERN_ERR
           "irq %d: nobody cared (try booting with "
           "the \"irqpoll\" option)\n", desc->irq_data.irq);

    dump_stack();

    // 打印所有注册的 ISR 名称
    printk(KERN_ERR "handlers:\n");
    action = desc->action;
    while (action) {
        printk(KERN_ERR "[<%p>] %pf%s",
               action->handler, action->handler,
               (action->thread_fn ? " threaded " : " "));
        printk(KERN_CONT "%s", action->name ? : "???");
        action = action->next;
    }
}
```

**典型输出：**

```
[ERR] irq 16: nobody cared (try booting with the "irqpoll" option)
...
[ERR] handlers:
[ERR] [<ffffffffc0367000>] ehci_hcd:usb1
```

这意味着中断 16 持续触发，但 `ehci_hcd:usb1` 的 ISR 返回了 `IRQ_NONE` (设备不认领)。

---

## 7. 轮询机制 — poll_spurious_irqs (spurious.c:106)

当中断被标记为 `IRQS_SPURIOUS_DISABLED` 后，内核启动定时器轮询以检查中断线是否恢复。

```c
// kernel/irq/spurious.c:106
static void poll_spurious_irqs(struct timer_list *unused)
{
    struct irq_desc *desc;
    int i;

    for_each_irq_desc(i, desc) {
        // 遍历所有 SPURIOUS_DISABLED 的中断
        if (!(desc->istate & IRQS_SPURIOUS_DISABLED))
            continue;

        // 设置 POLL_INPROGRESS 防止并发
        desc->istate |= IRQS_POLL_INPROGRESS;

        do {
            if (desc->istate & IRQS_PENDING)
                irq_startup(desc);

            // 尝试发送中断
            if (irq_settings_can_autoenable(desc))
                irq_startup(desc);

            // 为每个 action 创建一个线程来检查
            try_one_irq(desc, true);

        } while (desc->istate & IRQS_PENDING);

        desc->istate &= ~IRQS_POLL_INPROGRESS;
    }

    // 如果还有 SPURIOUS 中断，重新设置定时器
    mod_timer(&poll_spurious_irq_timer,
              jiffies + POLL_SPURIOUS_IRQ_INTERVAL);
}
```

**定时器参数：**
```c
#define POLL_SPURIOUS_IRQ_INTERVAL (HZ / 10)  // 100ms
```

每 100ms 检查一次是否有中断线恢复。如果设备再次正确响应，内核将重新启用该中断。

### 7.1 try_one_irq — 尝试触发单次中断 (spurious.c:28)

```c
// kernel/irq/spurious.c:28
static int try_one_irq(struct irq_desc *desc, bool force)
{
    struct irqaction *action;
    int ret = 0;

    // ... 如果 IRQ 已被关闭则拒绝 ...

    action = desc->action;
    if (!action)
        goto out;

    /*
     * 遍历所有 action, 调用它们的 handler
     * 如果有任何一个返回 IRQ_HANDLED:
     *   说明中断线恢复了 → 清除 SPURIOUS 状态
     */
    for (; action; action = action->next) {
        irqreturn_t res;

        res = action->handler(desc->irq_data.irq, action->dev_id);
        if (res == IRQ_HANDLED) {
            ret = 1;
            break;
        }
    }

    if (ret) {
        // 中断线恢复: 重新启用
        desc->istate &= ~IRQS_SPURIOUS_DISABLED;
        desc->depth--;
        irq_startup(desc);
        desc->irqs_unhandled = 0;
    }

out:
    return ret;
}
```

---

## 8. 完整决策流程图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        note_interrupt 决策树                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  handle_irq_event_percpu()                                                 │
│       │                                                                      │
│       ▼                                                                      │
│  ┌─────────────────────┐                                                    │
│  │ ISR 返回值 action_ret│                                                    │
│  └──────┬──────────────┘                                                    │
│         │                                                                     │
│     ┌───┼─────────────┬─────────────────┐                                   │
│     │   │             │                 │                                    │
│  IRQ_ IRQ_          IRQ_            无效值                                   │
│  HAND WAKETHREAD    NONE            (bug)                                   │
│  LED  │             │                 │                                      │
│       │             │                 ▼                                      │
│       │             │        ┌─────────────────┐                            │
│       │             │        │ __report_bad_irq │  严重: ISR 返回损坏值       │
│       │             │        └──────┬──────────┘                            │
│       │             │               │                                        │
│       │             │               ▼                                        │
│       │             │        ┌─────────────────┐                            │
│       │             │        │ dump_stack()    │                            │
│       │             │        └─────────────────┘                            │
│       │             │                                                        │
│       │             ▼                                                        │
│       │     ┌──────────────────┐                                             │
│       │     │ 跳过轮询中断?     │                                             │
│       │     │ (polled irq or   │──YES──► return                              │
│       │     │  POLL_INPROGRESS)│                                             │
│       │     └────┬──┬──────────┘                                             │
│       │          │  │                                                        │
│       │          │  NO                                                       │
│       │          ▼                                                           │
│       │     ┌──────────────────┐  100ms 老化窗口                             │
│       │     │ 距离上一次 IRQ_NONE│                                            │
│       │     │ 超过 100ms?       │                                            │
│       │     └──┬──────────┬────┘                                             │
│       │        │ YES      │ NO                                               │
│       │        ▼          ▼                                                  │
│       │  ┌──────────┐ ┌──────────┐                                          │
│       │  │ 衰减一半  │ │ 累积 +1  │                                          │
│       │  │ >>= 1    │ │ ++       │                                          │
│       │  └────┬─────┘ └────┬─────┘                                          │
│       │       └──────┬─────┘                                                 │
│       │              ▼                                                       │
│       │     ┌──────────────────┐                                             │
│       │     │ try_one_irq()    │  (如果 irqfixup 参数启用)                   │
│       │     │ 尝试错误路由修复  │                                             │
│       │     └──────┬───────────┘                                             │
│       │            │                                                          │
│       │            ▼                                                          │
│       │     ┌──────────────────┐                                             │
│       │     │ irqs_unhandled   │                                             │
│       │     │ > 99900?         │                                             │
│       │     └──┬───────┬───────┘                                             │
│       │        │ YES   │ NO                                                  │
│       │        ▼       ▼                                                     │
│       │  ┌──────────┐  return (continue tracking)                           │
│       │  │ disabled  │                                                        │
│       │  │ + polling │                                                        │
│       │  └──────────┘                                                        │
│       │                                                                       │
│       ▼                                                                       │
│  ┌──────────┐                                                                │
│  │ clear     │                                                                │
│  │ unhandled │                                                                │
│  └──────────┘                                                                │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. 内核启动参数 (spurious.c:17, 382)

```c
// kernel/irq/spurious.c:17
static bool irqfixup __read_mostly;

// 通过内核命令行启用错误路由修复
static int __init irqfixup_setup(char *str)
{
    irqfixup = 1;
    printk(KERN_INFO "Misrouted IRQ fixup support enabled.\n");
    return 1;
}
__setup("irqfixup", irqfixup_setup);
```

以及：

```c
static int __init irqpoll_setup(char *str)
{
    // 强制所有中断使用轮询模式
    irqfixup = 2;     // 完全轮询, 不是只在 IRQ_NONE 时检查
    printk(KERN_INFO "Misrouted IRQ fixup and polling support enabled\n");
    return 1;
}
__setup("irqpoll", irqpoll_setup);
```

以及：

```c
static int __init no_irq_debug_setup(char *str)
{
    noirqdebug = 1;
    return 1;
}
__setup("noirqdebug", no_irq_debug_setup);
```

**参数含义：**

| 参数 | 作用 |
|------|------|
| `irqfixup` | 对所有 IRQ_NONE 尝试错误路由修复 (调用全部共享 action) |
| `irqpoll` | 完全轮询模式，忽略硬件中断 |
| `noirqdebug` | 禁用虚假中断检测，不输出 "nobody cared" 信息 |

---

## 10. 100ms 老化窗口详解

`note_interrupt` 中的关键机制是 100ms 老化窗口：

```c
/*
 *     irqs_unhandled 随时间变化
 *
 *         ↑
 *         │
 *    99900├────────────────────────────── 阈值 ──────
 *         │                          ╱
 *         │                      ╱
 *         │                  ╱
 *   1000  │              ╱
 *         │          ╱
 *         │      ╱
 *     10  │  ╱
 *         └───────────────────────────────────────►  时间
 *            每个 IRQ_NONE 增加 +1
 *            每 100ms 衰减 (/2)
 *            持续高频 IRQ_NONE → 突破阈值 → 禁用
 */
```

**设计精妙之处：**
- 偶发 `IRQ_NONE` (例如共享中断正常情况) 不会触发阈值
- 衰减窗口确保短暂干扰自动恢复
- 只有**持续** (超过 100ms 窗口) **大量** (每窗口内 > 99900 次) 的虚假中断才会触发关闭

---

## 11. 虚假中断的调试

### 11.1 查看 spurious 统计

```bash
$ cat /proc/irq/16/spurious
count 0
unhandled 0
last_unhandled 0 ms

$ cat /proc/irq/23/spurious
count 123456              # 总共触发次数
unhandled 42              # 未处理计数
last_unhandled 12345 ms   # 距上次未处理已经过 xx ms
```

### 11.2 使用 tracepoint

```bash
# 启用 spurious 检测 tracepoint
echo 1 > /sys/kernel/debug/tracing/events/irq/irq_handler_exit/enable

# 查看 trace 输出, 找 IRQ_NONE 的 ISR
cat /sys/kernel/debug/tracing/trace | grep IRQ_NONE
```

### 11.3 检查 "nobody cared" 的遗留状态

```bash
# 查看禁用标志
cat /sys/kernel/debug/irq/irqs/16 | head
# 查看是否标记为 SPURIOUS
```

---

## 12. 完整的中断状态转换

```
  正常处理流程
  ────────────
  [正常] ────► IRQ_HANDLED ────► [正常]   (计数器递增)

  虚假检测
  ────────
  [正常] ────► IRQ_NONE ────► irqs_unhandled++ ────► [检测中]

                              │ 衰减窗口
                              └─► 如果 < 阈值: [正常]

                              │ 超越阈值?
                              └─► IRQS_SPURIOUS_DISABLED ──► [禁用+轮询]

  轮询恢复
  ────────
  [禁用+轮询] ────► poll_spurious_irqs() ──► try_one_irq()

                   │ 如果 ISR 再次返回 IRQ_HANDLED:
                   └─► 清除 SPURIOUS_DISABLED ──► [恢复正常]
```

---

## 13. 虚假中断 vs. 共享中断的微妙关系

共享中断 (IRQF_SHARED) 是 `IRQ_NONE` 最常见的原因：

```
  中断线 16 上有两个设备:
    Device A: ehci_hcd:usb1,  ISR_a()
    Device B: i2c_designware, ISR_b()

  当中断触发时:
    isr_a() → 检查设备寄存器 → 我的设备没中断 → IRQ_NONE
    isr_b() → 检查设备寄存器 → 我的设备中断了 → IRQ_HANDLED

  note_interrupt 看到的 retval = IRQ_NONE | IRQ_HANDLED = IRQ_HANDLED
  → 正确处理! 虽然 ISR_a 返回 IRQ_NONE 但这在共享中断中是正常的
```

但如果是以下场景：

```
  中断线 16 上的 Device A 已经被拔出 (USB 热插拔)
  但 ISR 仍然注册 → isr_a() 仍然返回 IRQ_NONE

  如果中断线 16 保持活跃 (电噪声、浮空引脚):
  → 80% 的中断 isr_b 处理
  → 20% 的中断 无处理 (irqs_unhandled 累积)
  → 不会达到 999/1000 阈值
  → 正常! 内核不会误杀
```

**临界情况：**
```
  中断线 23 上的 PCI 设备已拔出:
  仍然有新设备占据了此中断线
  但此设备的 ISR 还在 → 所有中断都是 IRQ_NONE
  → irqs_unhandled 快速增长 → 超越 99900

  内核: "irq 23: nobody cared"
  该中断被禁用, 启动轮询
```

---

## 14. 性能影响

**虚假检测的性能开销：**

```
每个中断 (包括被 IRQ_HANDLED 的):
  __handle_irq_event_percpu:
    __kstat_incr_irqs_this_cpu     // per-CPU inc (快速)
    for_each_action_of_desc        // 遍历链表 (O(n_actions))
    note_interrupt                 // 如果不是 IRQ_NONE: return 0 (快速)
                                   // 如果是 IRQ_NONE: 累积+老化检查 (快速)
                                 total overhead: ~1-2 微秒
```

**对高频中断 (网络) 的影响极小：**
- 处理的返回 `IRQ_HANDLED` → 直接返回
- per-CPU 变量操作 (无锁)
- 无分配、无事件等待

---

## 15. 与后续文章的衔接

- **第一篇**：irq_desc 结构，kstat_irqs 在描述符中的位置
- **第二篇**：三个流控处理器 (level/edge/fasteoi) 的完整路径
- **第三篇 (本文)**：show_interrupts 数据来源，虚假中断检测算法，轮询恢复机制
- **第四篇**：irq_exit → __do_softirq → tasklet — 中断下半部处理
- **第五篇**：CPU 热插拔对 IRQ affinity 和 spurious 的影响，MSI 域，电源管理

---

## 16. 源码导航

```
kernel/irq/proc.c         — show_interrupts, register_irq_proc
kernel/irq/spurious.c     — note_interrupt, __report_bad_irq, poll_spurious_irqs
kernel/irq/handle.c       — __handle_irq_event_percpu (kstat_incr 调用点)
kernel/irq/manage.c       — irq_finalize_oneshot (SPURIOUS_DEFERRED 延迟检测)
include/linux/kernel_stat.h — kstat_irqs 定义
```

---

## 下一篇文章

[第四篇：软中断与 Tasklet — 从中断上下文到进程上下文](./04-softirq-tasklet.md)
