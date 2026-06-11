# spin_lock + local_bh_disable 配合使用 — 锁与下半部的协同

> 源码：`include/linux/spinlock.h` `kernel/softirq.c` | 📌 同系列 [spin_lock 系列](../locking/spinlock-deep-dive/README.md) [IRQ 系列](../irq/irq-deep-dive/README.md)

---

## 1. 问题：进程上下文 vs softirq 的竞争

假设一个数据被进程上下文和 NET_RX softirq 同时访问：

```
进程上下文（可抢占）：
  spin_lock(&lock)
  // 修改共享数据
  spin_unlock(&lock)

NET_RX_SOFTIRQ（同 CPU 中断上下文）：
  spin_lock(&lock)     // ← 如果同 CPU 正在持有锁 → 死锁！
  // 修改共享数据
  spin_unlock(&lock)
```

`spin_lock` 只禁止抢占（`preempt_disable`），但 softirq 在当前 CPU 上可以在 `irq_exit` 中执行，它可以抢占任何未关 softirq 的内核路径。

---

## 2. spin_lock_bh — 标准解法

```c
// include/linux/spinlock.h:345-349
#define spin_lock_bh(lock)                  \
do {                                        \
    local_bh_disable();                     \   // ★ 先关 BH
    raw_spin_lock(&(lock)->rlock);          \   // ★ 再拿自旋锁
} while (0)

// include/linux/spinlock.h:393-397
#define spin_unlock_bh(lock)                \
do {                                        \
    raw_spin_unlock(&(lock)->rlock);        \   // ★ 先放自旋锁
    local_bh_enable();                      \   // ★ 再开 BH
} while (0)
```

锁叠加顺序：

```
spin_lock_bh()  = local_bh_disable() + spin_lock()
spin_lock_irq() = local_irq_disable() + spin_lock()
spin_lock_irqsave() = local_irq_save() + spin_lock()
```

---

## 3. local_bh_disable 的实现

```c
// include/linux/bottom_half.h:8-17
static inline void local_bh_disable(void)
{
    __local_bh_disable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
}

// kernel/softirq.c:150-175
void __local_bh_disable_ip(unsigned long ip, unsigned int cnt)
{
    unsigned long flags;
    WARN_ON_ONCE(in_hardirq());                 // ★ 不能在硬中断中禁用 BH

    raw_local_irq_save(flags);
    __preempt_count_add(cnt);                   // ★ 递增 preempt_count 中的 BH 位
    raw_local_irq_restore(flags);
}
```

`preempt_count` 的位字段：

```
Bit:  31..20        19..16        15..8         7..0
    ┌──────────────┬────────────┬──────────┬──────────┐
    │  PREEMPT     │  SOFTIRQ   │  HARDIRQ │  NMI     │
    │  (preempt)   │  (BH)      │  (irq)   │          │
    └──────────────┴────────────┴──────────┴──────────┘
```

- `BH 位`（bit 16-19）：`local_bh_disable` 递增这里
- `irq_exit` 在检查 softirq pending 时，先看 `in_interrupt()` 是否返回 false → BH 被禁时 softirq 不会在当前中断返回路径中执行

---

## 4. local_bh_enable 的延迟执行

```c
// kernel/softirq.c:195-210
void __local_bh_enable_ip(unsigned long ip, unsigned int cnt)
{
    WARN_ON_ONCE(in_hardirq());

    preempt_count_sub(cnt - 1);              // 递减 BH 计数，剩 1

    if (unlikely(!in_interrupt() && local_softirq_pending())) {
        // ★ BH 重新启用 + 有 pending softirq → 立即处理！
        do_softirq();
    }

    preempt_count_dec();                     // 最后 1 个 BH 计数
}
```

**关键**：`local_bh_enable` 不是简单地清除 BH 禁止标志。如果在这个过程中发现有 pending 的 softirq 需要处理，会**立即调用 `do_softirq()` 在当前上下文中处理**。这就是为什么 `spin_unlock_bh` 的释放顺序是先解锁再开 BH——避免了拿着锁处理 softirq 的可能性。

---

## 5. 何时用 spin_lock，何时用 spin_lock_bh

```
┌─────────────────────────────┐
│ 进程上下文 vs 进程上下文          │ → spin_lock 就够了
├─────────────────────────────┤
│ 进程上下文 vs softirq/tasklet │ → ★ spin_lock_bh
├─────────────────────────────┤
│ 进程上下文 vs 硬中断           │ → spin_lock_irqsave
├─────────────────────────────┤
│ softirq vs softirq (同 CPU)    │ → spin_lock 就够了（同类型 softirq 不抢占同 CPU）
├─────────────────────────────┤
│ softirq vs 硬中断             │ → spin_lock_irqsave
├─────────────────────────────┤
│ 硬中断 vs 硬中断               │ → spin_lock_irqsave
└─────────────────────────────┘
```

---

## 6. 典型场景

### NET_RX softirq 中的锁

```c
// 网络驱动 napi poll 函数（softirq 上下文）
static int driver_poll(struct napi_struct *napi, int budget)
{
    // 不需要 spin_lock_bh，因为已经在 softirq 上下文中
    spin_lock(&ring->lock);          // ★ 保护 ring buffer
    // ... 处理数据包 ...
    spin_unlock(&ring->lock);
}

// 进程上下文（如 ethtool 或 ifconfig）
static void driver_update_stats(struct net_device *dev)
{
    spin_lock_bh(&ring->lock);       // ★ 关 BH 防止 softirq 抢占
    // ... 读统计数据 ...
    spin_unlock_bh(&ring->lock);
}
```

### 定时器回调中的锁

```c
// 定时器回调（softirq 上下文——TIMER_SOFTIRQ）
static void my_timer_callback(struct timer_list *t)
{
    spin_lock(&lock);                // ★ 已经在 softirq 中，不需要 _bh
    // ... 修改数据 ...
    spin_unlock(&lock);
}

// 用户态 ioctl（进程上下文）
static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    spin_lock_bh(&lock);             // ★ 关 BH 防止定时器抢占
    // ... 修改数据 ...
    spin_unlock_bh(&lock);
}
```

---

## 7. 与 PREEMPT_RT 的交互

PREEMPT_RT 下，`local_bh_disable` 的行为不变——仍递增 `preempt_count`。但因为 hardirq 已经完全线程化，`_irq` 变体不再关中断（它们只是 spin_lock）。`_bh` 变体仍然本质上是安全的，因为在 RT 中 softirq 通常在 `ksoftirqd` 线程中运行。

---

## 8. 总结

| 保护对象 | 推荐锁 | 原因 |
|---------|--------|------|
| 进程 vs 进程 | `spin_lock` | 抢占已禁，不会死锁 |
| 进程 vs softirq | `spin_lock_bh` | 关 softirq 防止同 CPU 抢锁 |
| 进程 vs 硬中断 | `spin_lock_irqsave` | 关硬中断 |
| softirq vs 硬中断 | `spin_lock_irqsave` | softirq 可被硬中断抢占 |
| 硬中断 vs 硬中断 | `spin_lock_irqsave` | 同类中断不会抢占但可能在不同 CPU |

**根本原则**：只关"比你强"的中断——进程关 BH，BH 关硬中断。同等优先级只靠锁本身的并发保护。
