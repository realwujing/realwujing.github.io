# 第二篇：IRQ Chip 与 Flow Handlers — Level/Edge/FASTEOI 三种处理方式

> 源码：kernel/irq/chip.c kernel/irq/handle.c kernel/irq/irqdesc.c | 头文件：include/linux/irq.h include/linux/irqhandler.h

系列目录：[IRQ 内核源码深度解析](./README.md)

---

## 1. 概述

第一篇我们分析了 `irq_desc` 数据结构以及 `request_irq` 的注册过程。当硬件中断到达 CPU 时，内核通过 `desc->handle_irq` 指向的流控处理器 (Flow Handler) 来决定如何调用 ISR。流控处理器的选择取决于硬件中断控制器的类型 (Level/Edge/FASTEOI) 和芯片方法表 (`irq_chip`)。

**核心问题**：为什么需要不同的流控处理器？
- **Level 触发**：中断线保持高电平，直到设备清除条件。如果处理完不清除 (ack) 也不 mask，会立即再次触发，导致中断风暴。
- **Edge 触发**：中断线产生一个脉冲。如果脉冲到达时被 mask，则会丢失。
- **FASTEOI**：现代 MSI/MSI-X 的中断默认处理方式，发送 eoi 后控制器自动重装。

本文将逐行分析三个核心流控处理器和 `irq_chip` 方法表。

---

## 2. struct irq_chip — 中断控制器方法表 (irq.h:498)

```c
// include/linux/irq.h:498
struct irq_chip {
    const char      *name;            // 控制器名称 (例如 "GICv3", "IO-APIC")
    unsigned int    (*irq_startup)(struct irq_data *data);   // 启动中断
    void            (*irq_shutdown)(struct irq_data *data);  // 关闭中断
    void            (*irq_enable)(struct irq_data *data);    // 使能中断
    void            (*irq_disable)(struct irq_data *data);   // 禁用中断

    void            (*irq_ack)(struct irq_data *data);       // 应答 (acknowledge)
    void            (*irq_mask)(struct irq_data *data);      // 掩码 (mask)
    void            (*irq_mask_ack)(struct irq_data *data);  // mask + ack 组合
    void            (*irq_unmask)(struct irq_data *data);    // 解掩码 (unmask)
    void            (*irq_eoi)(struct irq_data *data);       // 中断结束 (end of interrupt)

    int             (*irq_set_affinity)(struct irq_data *data,
                                        const struct cpumask *dest, bool force);
    int             (*irq_retrigger)(struct irq_data *data);  // 重新触发
    int             (*irq_set_type)(struct irq_data *data, unsigned int flow_type);
                                                   // 设置触发类型 (level/edge/极性)
    int             (*irq_set_wake)(struct irq_data *data, unsigned int on);
                                                   // 设置为唤醒源
    void            (*irq_bus_lock)(struct irq_data *data);   // 总线锁
    void            (*irq_bus_sync_unlock)(struct irq_data *data);
                                                // 总线同步解锁，发送挂起的寄存器写

    int             (*irq_suspend)(struct irq_data *data);    // PM: 挂起
    int             (*irq_resume)(struct irq_data *data);     // PM: 恢复

    void            (*irq_compose_msi_msg)(struct irq_data *data, struct msi_msg *msg);
                                               // MSI: 组合 MSI 消息
    // ...更多字段...
};
```

### 2.1 方法分类

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                      irq_chip 方法分类                                         │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  【启动/关闭】                                                                 │
│  ┌─────────────────────┬──────────────────────────────────────────┐          │
│  │ irq_startup()       │ 初始化硬件，使能中断。默认: irq_enable     │          │
│  │ irq_shutdown()      │ 关闭硬件中断。默认: irq_disable           │          │
│  │ irq_enable()        │ 使能中断。默认: irq_unmask                │          │
│  │ irq_disable()       │ 禁用中断。默认: irq_mask                  │          │
│  └─────────────────────┴──────────────────────────────────────────┘          │
│                                                                               │
│  【应答/掩码】                                                                 │
│  ┌─────────────────────┬──────────────────────────────────────────┐          │
│  │ irq_ack()          │ 应答 (告诉控制器"我已收到")                  │          │
│  │ irq_mask()         │ 掩码 (阻止更多中断到达)                     │          │
│  │ irq_mask_ack()     │ mask + ack 组合 (level 中断常用)           │          │
│  │ irq_unmask()       │ 解掩码 (允许中断到达)                      │          │
│  │ irq_eoi()          │ 中断结束 (告诉控制器"我已处理完毕")          │          │
│  └─────────────────────┴──────────────────────────────────────────┘          │
│                                                                               │
│  【触发类型】                                                                  │
│  ┌─────────────────────┬──────────────────────────────────────────┐          │
│  │ irq_set_type()      │ 设置触发类型 (低电平/高电平/上升沿/下降沿)    │          │
│  │ irq_retrigger()     │ 重新触发中断 (用于边沿检测和 poll)          │          │
│  └─────────────────────┴──────────────────────────────────────────┘          │
│                                                                               │
│  【其他】                                                                      │
│  ┌─────────────────────┬──────────────────────────────────────────┐          │
│  │ irq_set_affinity()  │ 设置 CPU affinity                        │          │
│  │ irq_set_wake()      │ 设置 wakeup 源                            │          │
│  │ irq_bus_lock()      │ 总线访问锁                                 │          │
│  │ irq_bus_sync_unlock() │ 发送挂起的寄存器写                      │          │
│  │ irq_compose_msi_msg()│ 组合 MSI 消息                            │          │
│  │ irq_suspend/resume()│ PM 挂起/恢复                              │          │
│  └─────────────────────┴──────────────────────────────────────────┘          │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 默认实现

如果 chip 未提供某些方法，内核使用 irq_chip 的默认实现：

```c
// 默认 startup: 调用 enable
irq_startup(struct irq_data *data) {
    irq_enable(data);
    return 0;
}

// 默认 enable: 调用 unmask
irq_enable(struct irq_data *data) {
    irq_unmask(data);
}

// 默认 disable: 调用 mask
irq_disable(struct irq_data *data) {
    irq_mask(data);
}
```

---

## 3. irq_flow_handler_t — 流控处理器类型 (irqhandler.h)

```c
// include/linux/irqhandler.h:12
typedef void (*irq_flow_handler_t)(struct irq_desc *desc);
```

流控处理器是一个函数指针，存储在 `desc->handle_irq` 中。内核预定义了多种流控处理器：

| 流控处理器 | 用途 | 源码位置 |
|-----------|------|---------|
| `handle_level_irq` | Level 触发中断 | chip.c:687 |
| `handle_fasteoi_irq` | 现代 MSI/MSI-X, GIC | chip.c:738 |
| `handle_edge_irq` | Edge 触发中断 | chip.c:825 |
| `handle_simple_irq` | 无需 ack/eoi 的简单中断 | chip.c:630 |
| `handle_percpu_irq` | per-CPU 中断 | chip.c:975 |
| `handle_bad_irq` | 不处理 (占位/错误) | handle.c:33 |

---

## 4. 架构入口 → 流控处理器的完整链路

### 4.1 ARM64 上的中断入口 (GICv3)

```
硬件中断到达 PE
  │
  ▼
ARM64 异常向量表 — el1_irq
  │
  ▼
arch/arm64/kernel/entry.S:  el1_irq_handler
  │
  ▼
kernel/irq/handle.c:  handle_arch_irq(regs)
  │
  ▼
set_handle_irq() 注册的函数
  │   (arm64 GIC: gic_handle_irq)
  ▼
drivers/irqchip/irq-gic-v3.c:  gic_handle_irq(void *regs)
  │
  ├── 读取 GICC_IAR (Interrupt Acknowledge Register)
  │     └── hwirq = 中断 ID (intid)
  │
  ├── 如果 intid < 1020 (PPI/SPI): 普通中断
  │     │
  │     └── generic_handle_domain_irq(gic_data.domain, hwirq)  // irqdesc.c:729
  │
  └── 如果 intid >= 1020: 特殊中断 (IPI, 维护等)
```

### 4.2 generic_handle_domain_irq → generic_handle_irq_desc

```c
// kernel/irq/irqdesc.c:729
int generic_handle_domain_irq(struct irq_domain *domain, unsigned int hwirq)
{
    // 通过 irq_domain 的映射表查找 Linux IRQ 号
    // 再调用 generic_handle_irq
    return handle_irq_desc(irq_find_mapping(domain, hwirq));
}
```

```c
// kernel/irq/irqdesc.c:688
int generic_handle_irq(unsigned int irq)
{
    // 用 Linux IRQ 号在 radix tree 中查找 irq_desc
    return handle_irq_desc(irq);
}
```

```c
// kernel/irq/irqdesc.h:184 (内联函数)
static inline int handle_irq_desc(struct irq_desc *desc)
{
    if (!desc)
        return -EINVAL;
    generic_handle_irq_desc(desc);
}
```

### 4.3 generic_handle_irq_desc — 调用流控处理器

```c
// kernel/irq/irqdesc.h:193 (内联函数)
static inline void generic_handle_irq_desc(struct irq_desc *desc)
{
    /*
     * desc->handle_irq() 指向:
     *   handle_level_irq()
     *   handle_edge_irq()
     *   handle_fasteoi_irq()
     *   handle_bad_irq()
     *   ...
     */
    desc->handle_irq(desc);
}
```

### 4.4 x86 入口 (APIC)

```
x86 中断门
  │
  ▼
arch/x86/kernel/irq.c:  common_interrupt(regs)
  │
  ▼
kernel/irq/handle.c:  handle_arch_irq(regs) = do_IRQ(regs)
  │
  ▼
do_IRQ(): 读取 ISA IRQ 或通过 IO-APIC / MSI 消息向量查找 irq_desc
  │
  ▼
generic_handle_irq(irq)
  │
  ▼
desc->handle_irq(desc)           // → handle_fasteoi_irq / handle_edge_irq
```

### 4.5 handle_arch_irq 注册 (全局变量)

```c
// kernel/irq/handle.c:285
void (*handle_arch_irq)(struct pt_regs *) __ro_after_init;

void __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
    handle_arch_irq = handle_irq;
}
```

各架构在初始化时注册自己的中断处理入口：

```c
// arm64
set_handle_irq(gic_handle_irq);

// x86 默认
handle_arch_irq = do_IRQ;
```

---

## 5. handle_level_irq — Level 触发 (chip.c:687)

### 5.1 Level 触发的物理特性

```
            中断信号
                ┌─────────────────────────────────────────────────┐
                │  高电平: 中断持续                                │
                │                                                  │
  level      ────┤                                                  ├────
  inactive  ──  │                                                  │
                │                                                  │
                └──┬──┬─────────┬─────────────────────────────────┘
                   │  设备置起中断  设备清除中断条件
                   │
                   └─── interrupt signaled ────────────────────────
```

**Level 触发的关键特性**：
- 中断线保持断言 (asserted) 状态直到设备寄存器的中断标志被清除
- 如果不在处理前 mask 中断，会在 ISR 返回后立即再次触发 → **中断风暴**
- 处理策略：**先 mask，再 ack 设备，然后调用 ISR，最后 unmask**

### 5.2 源码分析

```c
// kernel/irq/chip.c:687
void handle_level_irq(struct irq_desc *desc)
{
    raw_spin_lock(&desc->lock);

    // ───── 第一步: mask + ack (mask_ack) ─────
    // mask: 阻止更多中断到达 (防止重入)
    // ack: 通知控制器已收到中断
    //
    // 使用 mask_ack_irq 统一调用 desc->irq_data.chip->irq_mask_ack
    // 如果 chip 未提供 mask_ack, 先调 mask 再调 ack
    mask_ack_irq(desc);

    // ───── 第二步: 检查 IRQ 状态 ─────
    if (!irq_may_run(desc))
        goto out_unlock;

    // ───── 第三步: 设置 INPROGRESS ─────
    // 在调用 ISR 前标记 "正在处理中"
    // 如果 INPROGRESS 已被设置 (嵌套/并发 场景), 直接退出
    if (unlikely(irqd_irq_inprogress(&desc->irq_data)))
        if (!irq_check_poll(desc))
            goto out_unmask;

    desc->istate |= IRQS_INPROGRESS;

    do {
        // ───── 第四步: 清除 "REPLAY" 标志 ─────
        // 如果由于某些原因 (如 spurious detection polling) 需要
        // 重新处理: 清除标志并继续
        if (unlikely(desc->istate & IRQS_REPLAY))
            desc->istate &= ~IRQS_REPLAY;

        // ───── 第五步: 调用 ISR 链 ─────
        handle_irq_event(desc);

        // ───── 第六步: 检查 IRQ 是否仍然有效 ─────
        // level 中断在 ISR 执行后, 硬件仍然可能保持断言状态
        // 如果 ISR 成功处理 (设备清除条件), 循环结束
        // 如果 ISR 未处理 (设备未清除), 再做一次
        cond_unmask_irq(desc);

    } while (irqd_irq_inprogress(&desc->irq_data));

    // ───── 第七步: 清除 INPROGRESS ─────
    desc->istate &= ~IRQS_INPROGRESS;

out_unmask:
    // ───── 第八步: unmask ─────
    // 如果依然要执行 (retrigger): 跳过 unmask (交由 retrigger 处理)
    // 否则: 常态 unmask → 重新接收中断
    if (unlikely(!idec->on | irqd_irq_inprogress(...)))
        goto out_unlock;
    unmask_irq(desc);

out_unlock:
    raw_spin_unlock(&desc->lock);
}
```

### 5.3 Level 流程图

```
  Level 中断到达
       │
       ▼
  ┌─────────────┐
  │ 获取锁       │  raw_spin_lock(&desc->lock)
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐
  │ mask + ack   │  chip->irq_mask_ack() → 阻止重入 + 应答
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐ 是
  │ INPROGRESS? ├──→ goto out_unmask  (重入保护)
  └──┬──────────┘
     │ 否
     ▼
  ┌─────────────┐
  │ 设置          │  desc->istate |= IRQS_INPROGRESS
  │ INPROGRESS    │
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐
  │ ISR 执行     │  handle_irq_event(desc)
  │ 遍历 action   │  → 逐个调用 action->handler()
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐ 否
  │ 仍在 active  │────→ 清除 INPROGRESS → unmask → unlock → 返回
  │ (IRQ 条件还未 │
  │  被设备清除?)  │
  └──┬──────────┘
     │ 是: 重新执行一遍 ISR
     │ (设备清除条件耗时较长)
     ▼
  ┌─────────────┐
  │ unmask       │  chip->irq_unmask() → 恢复接收中断
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐
  │ 释放锁       │  raw_spin_unlock(&desc->lock)
  └─────────────┘
```

---

## 6. handle_fasteoi_irq — 现代 MSI/MSI-X (chip.c:738)

### 6.1 FASTEOI 的物理特性

**MSI/MSI-X 中断**不是引脚电平，而是写入特定内存地址的消息。一旦写入，中断消息被边缘触发，但可以通过 EOI (End Of Interrupt) 来通知控制器已完成处理。GIC v2/v3 的 SPI (Shared Peripheral Interrupts) 也使用此模式。

与 Level/Edge 的关键区别：
- Level: mask → ISR → unmask (处理期间短暂屏蔽)
- Edge: ack → ISR (处理前 ack 防止错过)
- **FASTEOI: ISR → eoi** (先处理再 eoi, 更简化)

### 6.2 源码分析

```c
// kernel/irq/chip.c:738
void handle_fasteoi_irq(struct irq_desc *desc)
{
    raw_spin_lock(&desc->lock);

    // ───── 第一步: 快速状态检查 ─────
    if (!irq_may_run(desc) || irqd_irq_polled(&desc->irq_data))
        goto out_eoi;

    // ───── 第二步: 检查 ONESHOT 状态 ─────
    // 如果 IRQF_ONESHOT, threads_oneshot 不为零表示
    // 上一个中断的线程还未完成 → mask 等待线程完成
    if (desc->istate & IRQS_ONESHOT)
        mask_irq(desc);

    // ───── 第三步: 再次检查 ─────
    if (!irq_may_run(desc))
        goto out_eoi;

    // ───── 第四步: 调用 ISR ─────
    handle_irq_event(desc);

out_eoi:
    // ───── 第五步: 检查是否需要 eoi ─────
    // 如果 ISR 返回 IRQ_WAKE_THREAD (启动了线程化 handler):
    //   不发送 eoi (让线程完成后在 irq_finalize_oneshot 中发送)
    // 否则: 立即发送 eoi
    if (!(desc->istate & IRQS_ONESHOT_ACTIVE))
        irq_eoi(desc);

    raw_spin_unlock(&desc->lock);
}
```

### 6.3 FASTEOI 流程图

```
  MSI 中断到达 (消息写入)
       │
       ▼
  ┌────────────────────────┐
  │ CORE: 获取锁            │  raw_spin_lock
  └──────────┬─────────────┘
             │
             ▼
  ┌────────────────────────┐ 是
  │ irq_may_run()?         ├──────────► goto out_eoi → eoi → unlock
  └──────┬─────────────────┘
         │ 否
         ▼
  ┌────────────────────────┐ 是
  │ ONESHOT active?        ├──────────► mask_irq(desc)  (等待上一个线程完成)
  └──────┬─────────────────┘
         │ 否
         ▼
  ┌────────────────────────┐
  │ handle_irq_event(desc) │  遍历 action 链表, 调用每个 handler
  └──────────┬─────────────┘
             │
             ├── 返回 NOT oneshot active:
             │     └──► irq_eoi(desc) → unlock
             │
             └── 返回 ONESHOT_ACTIVE:
                   └──► 跳过 eoi (等待线程完成后 irq_finalize_oneshot 发送)
                          unlock
```

---

## 7. handle_edge_irq — Edge 触发 (chip.c:825)

### 7.1 Edge 触发的物理特性

```
            中断信号
  inactive ────┐                              ┌────────────────
  (低电平)      │                              │
               │                              │
              ─┼──────────────┐               │
               │   上升沿脉冲   │               │
  active      │   (短脉冲)     │               │
               │               │               │
               └───────────────┘               │
               └──── interrupt signaled ───────┘
```

**Edge 触发的关键特性**：
- 中断只是一个**短暂的脉冲**，不会保持
- 如果脉冲到达时 IRQ 被 mask，脉冲会丢失 (不能恢复)
- 处理策略：**不能 mask！必须先 ack (锁存)，然后调用 ISR**

由于边缘触发的中断是瞬时的，一旦 ISR 完成处理后，硬件自动重新使能 (re-ARM)，不需要显式 unmask。但需要在处理期间暂时屏蔽以避免重入。

### 7.2 源码分析

```c
// kernel/irq/chip.c:825
void handle_edge_irq(struct irq_desc *desc)
{
    raw_spin_lock(&desc->lock);

    // ───── 第一步: 检查 IRQ 是否正在处理中 ─────
    //   如果已经在处理: goto out_unlock
    //
    //   但如果 REPLAY 标志已设置: 重入检查被跳过
    //   说明: REPLAY 表示中断可能被错过了 (例如之前被 mask)，
    //   现在需要重新投递。
    if (irqd_irq_disabled(&desc->irq_data) ||
        irq_settings_can_handle_polling(desc))
        desc->istate |= IRQS_REPLAY;
    else if (!(desc->istate & IRQS_INPROGRESS))
        desc->istate |= IRQS_INPROGRESS;
    else
        goto out_unlock;

    // ───── 第二步: ack ─────
    // 向控制器确认已收到中断 (锁存中断状态)
    // 对于 GIC, ack 本质上读取 GICC_IAR 并转换为 hwirq
    if (desc->irq_data.chip->irq_ack)
        desc->irq_data.chip->irq_ack(&desc->irq_data);

    // ───── 第三步: 调用 ISR ─────
    handle_irq_event(desc);

    // ───── 第四步: 检查 MISSED ─────
    // 在 ISR 执行期间，可能有新的边沿到达
    // 如果中断在 ISR 执行时仍然 pending (又触发了):
    //   设置 REPLAY 标志 → 下次再进来
    if (irqd_irq_masked(&desc->irq_data)) {
        if (!(desc->istate & IRQS_INPROGRESS)) {
            desc->istate &= ~IRQS_INPROGRESS;
            irq_release_resources(desc);
        }
    } else {
        irq_enable(desc);
    }

    desc->istate &= ~IRQS_INPROGRESS;

out_unlock:
    raw_spin_unlock(&desc->lock);
}
```

### 7.3 Edge 流程图

```
  Edge 脉冲到达
       │
       ▼
  ┌────────────┐
  │ 获取锁       │  raw_spin_lock(&desc->lock)
  └──────┬─────┘
         │
         ▼
  ┌────────────┐ 是               ┌─────────────────┐
  │ INPROGRESS?├───────────────→ │ 设置 IRQS_REPLAY │  (中断可能丢失)
  └──┬─────────┘                  │ 下次再进          │
     │ 否                         └──────────┬────────┘
     ▼                                       │
  ┌─────────────────────┐                    │
  │ 设置 IRQS_INPROGRESS │                    │
  └──────────┬──────────┘                    │
             │                               │
             ▼                               │
  ┌─────────────────────┐                    │
  │ chip->irq_ack()     │  锁存中断状态        │
  └──────────┬──────────┘                    │
             │                               │
             ▼                               │
  ┌─────────────────────┐                    │
  │ handle_irq_event()  │  调用所有 ISR       │
  └──────────┬──────────┘                    │
             │                               │
             ▼                               │
  ┌─────────────────────┐                    │
  │ 中断在 ISR 期间     │                    │
  │ 又触发了吗?          │                    │
  └──┬─────────┬────────┘                    │
     │ 是       │ 否                         │
     ▼          ▼                            │
  ┌──────┐  ┌────────────┐                   │
  │ MASK  │  │ ENABLE     │                   │
  │ (掩码) │  │ (RE-ARM)   │                   │
  │       │  │            │  ◄────────────────┘
  └──────┘  └────────────┘
     │          │
     ▼          ▼
  ┌─────────────────────┐
  │ 清除 INPROGRESS      │
  │ 释放锁               │
  └─────────────────────┘
```

---

## 8. handle_irq_event — ISR 遍历机制 (handle.c:255)

`handle_irq_event` 是三个流控处理器的公共核心，负责遍历 action 链表。

```c
// kernel/irq/handle.c:255
irqreturn_t handle_irq_event(struct irq_desc *desc)
{
    irqreturn_t ret;

    // ───── 第一步: 清除 PENDING 标志 ─────
    desc->istate &= ~IRQS_PENDING;

    // ───── 第二步: 设置 INPROGRESS ─────
    irqd_set(&desc->irq_data, IRQD_INPROGRESS);

    // ───── 第三步: 调用 per-CPU ISR 遍历 ─────
    ret = __handle_irq_event_percpu(desc);

    // ───── 第四步: 清除 INPROGRESS ─────
    irqd_clear(&desc->irq_data, IRQD_INPROGRESS);

    // ───── 第五步: 贡献随机性熵 ─────
    add_interrupt_randomness(irq);

    return ret;
}
```

### 8.1 __handle_irq_event_percpu — 逐 action 调用 (handle.c:185)

```c
// kernel/irq/handle.c:185
irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc)
{
    irqreturn_t retval = IRQ_NONE;
    unsigned int irq = desc->irq_data.irq;
    struct irqaction *action;

    /*
     * 记录每个 CPU 上的中断次数
     * desc->kstat_irqs[] 是 per-CPU 计数的数组
     * 即 /proc/interrupts 的数据来源!
     */
    __kstat_incr_irqs_this_cpu(desc);

    /*
     * 遍历 action 链表
     * 共享中断: 调用所有 ISR, 返回值 OR 在一起
     */
    for_each_action_of_desc(desc, action) {
        irqreturn_t res;

        // ───── 追踪处理与否 ─────
        trace_irq_handler_entry(irq, action);

        // ───── 调用 ISR! ─────
        res = action->handler(irq, action->dev_id);

        trace_irq_handler_exit(irq, action, res);

        // ───── 合并返回值 ─────
        // 如果是共享中断, 所有 ISR 都必须返回 IRQ_NONE …
        // 如果有任何一个返回 IRQ_HANDLED, 整体就是 HANDLED
        switch (res) {
        case IRQ_WAKE_THREAD:
            // 标记需要唤醒线程, 但仍然 "HANDLED"
            __irq_wake_thread(desc, action);
            fallthrough;
        case IRQ_HANDLED:
            retval |= res;
            break;
        default:
            break;
        }
    }

    // ───── 第六步: 虚假中断检测 ─────
    // 如果没有任何 ISR 返回 HANDLED/WAKETHREAD
    // note_interrupt 进行虚假检测和 "nobody cared" 诊断 (详见第三篇)
    note_interrupt(irq, desc, retval);

    return retval;
}
```

**返回值组合逻辑：**

```
Action 1: IRQ_NONE          (设备不是这个 interrupt 的)
Action 2: IRQ_HANDLED       (设备处理了)
Action 3: IRQ_NONE          (设备也不是这个 interrupt 的)

最终 retval: IRQ_HANDLED
```

---

## 9. 三种处理器的对比

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    Level / FASTEOI / Edge 处理对比                               │
├─────────────┬─────────────────┬──────────────────┬──────────────────────────────┤
│ 特性        │ Level           │ FASTEOI          │ Edge                         │
├─────────────┼─────────────────┼──────────────────┼──────────────────────────────┤
│ 信号特性    │ 持续保持        │ 消息式/MSI       │ 脉冲式 (短暂)                │
│             │                 │                  │                              │
│ 中断丢失    │ 不可能          │ 不可能           │ 可能 (如果被 mask)            │
│             │                 │                  │                              │
│ 重入保护    │ mask → ISR      │ ONESHOT mask     │ INPROGRESS check             │
│             │                 │                  │                              │
│ ISR 前操作  │ mask_ack        │ (无)             │ ack (锁存)                   │
│             │                 │                  │                              │
│ ISR 后操作  │ unmask          │ eoi              │ 可能 unmask / 设置 REPLAY    │
│             │                 │                  │                              │
│ 并发控制    │ spin_lock       │ spin_lock        │ spin_lock                    │
│             │                 │                  │                              │
│ 典型硬件    │ 传统 PC/AT      │ GIC, ARM         │ 传统 PC/AT                   │
│             │ 8259, GPIO      │ ITS, MSI/MSI-X   │ 8259, 边沿 GPIO              │
│             │                 │                  │                              │
│ 中断风暴    │ 如果处理前后    │ 不需要 mask       │ 不需要 mask                  │
│             │ 不 mask 则可能  │                  │ 但需要 replay 防护           │
│             │                 │                  │                              │
│ 线程化适应  │ 直接支持        │ ONESHOT 机制     │ 小心 (线程化+边沿=挑战)      │
│             │                 │                  │                              │
│ 重入        │ 通过 mask 防止  │ ONESHOT 防止     │ INPROGRESS (+REPLAY) 防止    │
│             │                 │                  │                              │
└─────────────┴─────────────────┴──────────────────┴──────────────────────────────┘
```

### 时序对比图

```
Level:
  ──┬────mask─ack──┬───ISR───┬───unmask─────►
    │              │         │               │  中断可能仍 asserted
    │              │         │               │  第一个 unmask 后再次进入
    │              └─────────┘               │
    │                                        │
    └──── 中断持续保持 ──────────────────────┘

FASTEOI:
  ──┬─────┬───────ISR───────┬───eoi──►
    │     │                  │         │ 现代 MSI: 消息已发送
    │     │                  │         │ 仅在 ONESHOT 时 mask
    └─────┴──────────────────┘

Edge:
  ┌───┬──ack──┬───ISR───┬──CheckPending───►
  │   │       │         │          │ 如果在 ISR 期间又来了新 pulse
  │   │       │         │          │ 可能被 miss → 设置 REPLAY
  └───┘       └─────────┘          │ 重新调用 ISR
                                    │
                                    └─► IRQS_REPLAY → 下次再进入
```

---

## 10. handle_bad_irq — 错误占位处理器 (handle.c)

```c
// kernel/irq/handle.c:33
void handle_bad_irq(struct irq_desc *desc)
{
    // 简单打印错误并 ack
    // 通常在多个体系结构中用于 "不工作" 的中断
    kstat_incr_irqs_this_cpu(desc);
    ack_bad_irq(desc->irq_data.irq);
}
```

新的描述符初始化为 `handle_bad_irq`，直到通过 `irq_set_chip_and_handler_name` 等领域设置操作设置正确的流控处理器。

---

## 11. chip 和 handler 的绑定 — irq_set_chip_and_handler_name (chip.c:1037)

```c
// kernel/irq/chip.c:43
int irq_set_chip(unsigned int irq, struct irq_chip *chip)
{
    // 设置 irq_data->chip 指针和默认的 chip_data
    // 用于指向芯片的私有数据
    struct irq_desc *desc = irq_to_desc(irq);
    desc->irq_data.chip = chip;
    return 0;
}

// kernel/irq/chip.c:1037
irq_set_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
                               irq_flow_handler_t handle, const char *name)
{
    struct irq_desc *desc = irq_to_desc(irq);

    // 设置 chip 方法表
    desc->irq_data.chip = chip;

    // 设置流控处理器
    if (handle)
        __irq_set_handler(irq, handle, 0, name);

    // 确保 chip_data 指向有效数据 (通常是 irq_data)
    if (!desc->irq_data.chip_data && chip)
        desc->irq_data.chip_data = &desc->irq_data;
}
```

**典型调用 (arch/platform 代码中)：**

```c
// 为 GIC SPI 中断设置处理方式
irq_set_chip_and_handler_name(i, &gic_chip, handle_fasteoi_irq, NULL);

// 为 GPIO 边沿中断
irq_set_chip_and_handler_name(i, &gpio_chip, handle_edge_irq, "edge");

// 为 legacy 8259 Level 中断
irq_set_chip_and_handler_name(i, &i8259A_chip, handle_level_irq, NULL);
```

---

## 12. 中断迁移与 CPU affinity — chip 的影响

当通过 `irq_set_affinity()` 改变 CPU 亲和性时，chip 的 `irq_set_affinity()` 方法必须对硬件寄存器做编程。

```c
// manage.c
int irq_do_set_affinity(struct irq_data *data, const struct cpumask *mask,
                        bool force)
{
    // 1. 更新 irq_common_data.affinity
    cpumask_copy(data->common->affinity, mask);

    // 2. 调用 chip 硬件操作
    ret = chip->irq_set_affinity(data, mask, force);

    // 3. 更新 effective_affinity
    // 记录实际可以设置的 CPU
    return ret;
}
```

---

## 13. MSI/MSI-X 与 FASTEOI

FASTEOI 是 MSI/MSI-X 中断的默认处理方式。MSI 中断的区别：
1. **没有物理引脚**，通过写入 PCI 配置空间中的 MSI 地址发送中断消息
2. **消息编码**包含硬件中断号 (hwirq)
3. **irq_compose_msi_msg** 是 chip 中的方法，组合 MSI 消息格式

```
  PCI 设备 MSI-X
    │
    ├── MSI-X 表项: 地址 hi + 地址 lo + 数据值 (hwirq)
    │
    ├── 设备写 MSI 消息到指定地址 (GIC ITS 或 IOMMU)
    │
    ├── 中断控制器译码消息, 提取 hwirq
    │
    ├── generic_handle_domain_irq(domain, hwirq)
    │
    └── handle_fasteoi_irq(desc) → ISR → eoi
```

---

## 14. 性能考量

| 场景 | 推荐的 handler | 原因 |
|------|---------------|------|
| MSI/MSI-X (NVMe、网络) | FASTEOI | 默认，无需 mask_ack |
| GPIO 电平中断 | Level | 需要 mask_ack 防止风暴 |
| GPIO 边沿中断 | Edge | 需要 REPLAY 防止丢失 |
| SGI/IPI | Percpu 或 FASTEOI | 每 CPU 私有中断 |
| 定时器 | 取决于架构 | 多数架构使用 per-IPI |

**FASTEOI 性能优势：**
- 减少寄存器操作次数 (无 mask, 无 ack, 仅 ISR + eoi)
- 对于高频中断 (网络 10Gbps → 每秒百万级中断)，每次节省 1-2 次外设寄存器访问

**Edge 中断捕获丢失：**
- 如果 ISR 执行时间过长，下一个边沿脉冲到达且处于 INPROGRESS 状态
- REPLAY 机制确保下次进入时重新调用 ISR
- 但可能仍然丢失某些硬件状态

---

## 15. 与后续文章的衔接

- **第一篇**：irq_desc, request_irq, irqaction 注册
- **第二篇 (本文)**：irq_chip 方法表, Level/Edge/FASTEOI 流控处理器
- **第三篇**：note_interrupt 虚假检测, /proc/interrupts 数据统计
- **第四篇**：raise_softirq, __do_softirq, ksoftirqd, tasklet
- **第五篇**：CPU 热插拔对 IRQ 的影响, MSI 域, 电源管理 PM

---

## 16. 源码导航

```
kernel/irq/chip.c          — 流控处理器实现
kernel/irq/handle.c        — handle_irq_event (ISR 链遍历)
kernel/irq/irqdesc.c       — generic_handle_irq, irq_to_desc
include/linux/irq.h        — irq_chip 结构体定义
include/linux/irqhandler.h — irq_flow_handler_t 类型定义
drivers/irqchip/           — 各厂商实现 (GIC, ITS, 8259, pl190, etc.)
arch/arm64/kernel/         — ARM64 中断入口 (gic_handle_irq)
arch/x86/kernel/irq.c      — x86 中断入口 (do_IRQ)
```

---

## 下一篇文章

[第三篇：/proc/interrupts 与 虚假中断检测 — show_interrupts 与 note_interrupt](./03-proc-spurious.md)
