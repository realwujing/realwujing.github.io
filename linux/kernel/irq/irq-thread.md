# 中断线程化 — request_threaded_irq 与 force_irqthreads

> 源码：`kernel/irq/manage.c` `kernel/irq/handle.c` | 📌 同系列 [IRQ 系列](../irq/irq-deep-dive/README.md)

---

## 1. 为什么需要线程化中断

传统中断处理全在硬中断上下文：
- 关中断，不能睡眠
- 占用 CPU 时间不可控
- PREEMPT_RT 要求所有中断可被低延迟任务抢占

线程化中断将耗时处理移到内核线程，硬中断只做"ack + wake thread"：

```
传统: 硬中断 → 处理全部逻辑 → 返回
线程化: 硬中断 → mask + ack → 唤醒 irq 线程 → 返回
         irq/XX 线程：休眠等待 → 被唤醒 → 处理逻辑 → unmask
```

---

## 2. request_threaded_irq — 注册入口

```c
// kernel/irq/manage.c:2115-2117
int request_threaded_irq(unsigned int irq,
                         irq_handler_t handler,     // 硬中断 handler（可为 NULL）
                         irq_handler_t thread_fn,   // 线程 handler（可为 NULL）
                         unsigned long irqflags,
                         const char *devname, void *dev_id)
```

### 三种调用模式

| handler | thread_fn | 行为 |
|---------|-----------|------|
| 有效 | NULL | 传统模式，硬中断处理全部逻辑（如果设置了 IRQF_ONESHOT → 不 oneshot，只有 spurious 检测） |
| NULL | 有效 | 只用线程处理。IRQF_ONESHOT 必须设置 |
| 有效 | 有效 | 硬中断做快速处理，返回 IRQ_WAKE_THREAD → 唤醒线程做重量处理 |

### IRQF_ONESHOT 的作用

```c
// IRQF_ONESHOT 保证 handler 和 thread_fn 之间不重入:
// 1. handler 返回 IRQ_WAKE_THREAD
// 2. 中断线被 mask 住
// 3. 线程被唤醒 → thread_fn 执行
// 4. thread_fn 完成 → 中断线被 unmask
// 5. 如果期间又来了中断: 只记录 pending，等 unmask 后重新触发
```

---

## 3. irq_thread — 线程化的核心

```c
// kernel/irq/manage.c:1244
static int irq_thread(void *data)
{
    struct irqaction *action = data;
    struct irq_desc *desc = irq_to_desc(action->irq);
    irqreturn_t (*handler_fn)(struct irq_desc *desc, struct irqaction *action);

    // 设置调度策略: SCHED_FIFO, 优先级 50 (半实时, 不饿死 SCHED_FIFO 99)
    sched_set_fifo(current);                   // line ~1250

    while (!irq_wait_for_interrupt(action)) {  // line ~1260
        irq_thread_check_affinity(desc, action);

        if (!desc->threads_oneshot || !action->thread_fn) {
            handler_fn = irq_thread_fn;
        } else {
            // ONE SHOT 模式下: 等所有其他线程完成
            raw_spin_lock_irq(&desc->lock);
            while (irq_wait_for_oneshot(...))  // 等所有 ONE SHOT handler 完成
                ;
            raw_spin_unlock_irq(&desc->lock);
            handler_fn = irq_thread_fn;
        }

        // ★ 执行实际的处理函数
        irqreturn_t ret = handler_fn(desc, action);

        // ★ ONE SHOT 处理完成后 unmask 中断
        if (action->flags & IRQF_ONESHOT) {
            if (!desc->threads_oneshot)
                unmask_threaded_irq(desc);
        }
    }

    // 线程退出
    task_work_cancel(current, irq_thread_dtor);
    return 0;
}
```

---

## 4. force_irqthreads — 强制所有中断线程化

```c
// kernel/irq/manage.c:28-31
static bool force_irqthreads_key;           // line 28 — 是否强制中断线程化
module_param_named(threadirqs, force_irqthreads_key, bool, 0644); // line 29
```

通过内核参数 `threadirqs` 或 `/sys/module/irq/parameters/threadirqs` 控制。

当 `force_irqthreads_key = true` 时，所有中断（即使没有注册 thread_fn 的普通 request_irq）也会被强制在线程中处理：

```c
// __handle_irq_event_percpu 内部:
if (force_irqthreads()) {
    // 即使 action->handler 返回了 IRQ_HANDLED,
    // 也强制唤醒线程重新处理
    __irq_wake_thread(desc, action);
}

// irq_thread_fn 内部:
if (force_irqthreads()) {
    // ★ 被强制线程化: 在 irq_forced_thread_fn 中调用原始的 handler
    ret = action->handler(irq, action->dev_id);
}
else {
    // 普通线程化: 调用 thread_fn
    ret = action->thread_fn(irq, action->dev_id);
}
```

**关键**：强制线程化后，原始的硬中断 handler 变成了线程中调用的函数——**用户代码完全感知不到差异**，但上下文从硬中断变成了进程。

---

## 5. 唤醒路径

```c
// kernel/irq/handle.c:61-90
void __irq_wake_thread(struct irq_desc *desc, struct irqaction *action)
{
    // 设置 action->thread_flags |= IRQTF_RUNTHREAD
    // 确保线程不会被重复唤醒（原子操作）

    if (action->thread->__state != TASK_RUNNING)
        wake_up_process(action->thread);          // 唤醒休眠的 irq 线程
}
```

这个函数被调用的场景：
1. **IRQ_WAKE_THREAD**：硬中断 handler 返回 `IRQ_WAKE_THREAD` — 通用路径
2. **IRQF_ONESHOT**：硬中断 handler 完成后，即使 handler 为 NULL 也会唤醒线程
3. **force_irqthreads**：所有中断都被强制线程化——在 irq_forced_thread_fn 中被调用

---

## 6. 完整时序

```
设备产生中断
  → arch entry: 跳转到 handler
    → desc->handle_irq(desc)   // flow handler (handle_fasteoi_irq 等)
      → handle_irq_event(desc)
        → __handle_irq_event_percpu(desc)
          → action->handler(irq, dev_id)               // ★ 硬中断 handler
            → 返回 IRQ_WAKE_THREAD
          → __irq_wake_thread(desc, action)            // ★ 唤醒线程

[上下文切换: 硬中断 → irq 线程]

irq/XX 线程:
  → irq_thread(action)                                 // manage.c:1244
    → irq_wait_for_interrupt(action)                    // 等被唤醒
    → action->thread_fn(irq, action->dev_id)            // ★ 线程 handler
    → IRQF_ONESHOT: unmask_irq(desc)
    → 循环
```

---

## 7. 与 PREEMPT_RT 的关系

PREEMPT_RT 自动启用 `force_irqthreads`（无法关闭）。这对于 RT 内核是硬性需求：
- 硬中断仍然存在（用作快速 ack+mask），但立刻转给 irq 线程
- 所有 irq 线程以 `SCHED_FIFO 50` 运行，低于实时任务的 99
- 实时任务不会被"不可控"的硬中断阻塞

---

## 8. 总结

| 机制 | 硬中断部分 | 线程部分 | 关键标志 |
|------|----------|---------|---------|
| 传统中断 | handler(全部) | 无 | 无 |
| 普通线程化 | handler(快速) | thread_fn(重量) | IRQF_ONESHOT |
| 纯线程化 | 无（只有ack） | thread_fn(全部) | IRQF_ONESHOT, handler=NULL |
| 强制线程化 | 被移入线程 | 原 handler 在 irq_forced_thread_fn 中执行 | `threadirqs` 参数 |
