# 第1篇：NAPI 核心 — napi_struct、napi_schedule 与 net_rx_action

> 源码：net/core/dev.c include/linux/netdevice.h | 头文件：include/linux/netdevice.h

系列目录：[NAPI 内核源码深度解析](./README.md)

---

## 1. 概述

NAPI (New API) 是 Linux 网络栈的核心接收机制。它的设计目标是用**中断 + 轮询混合模型**替代传统的纯中断模型：收到第一个包时触发硬中断，随后通过软中断批量轮询，在高负载下以轮询避免中断风暴，在低负载下自动回退到中断驱动。

本文逐一剖析 NAPI 的核心数据结构、调度入口和软中断处理函数，覆盖从硬件中断到数据包被送入协议栈的完整路径。

---

## 2. 核心数据结构

### 2.1 struct napi_struct — NAPI 上下文 (netdevice.h:381)

每个网络设备的每个接收队列都有一个 `napi_struct`，它封装了该队列的轮询状态和调度信息：

```c
// include/linux/netdevice.h:381
struct napi_struct {
    unsigned long       state;              // NAPI_STATE_* 状态标志
    struct list_head    poll_list;          // 挂入 softnet_data.poll_list 的链表节点
    int                 weight;             // 每次 poll 调用的处理预算（通常 64）
    u32                 defer_hard_irqs_count;  // 延迟恢复 IRQ 的计数器
    int                (*poll)(struct napi_struct *, int);  // 驱动提供的轮询函数
    int                 list_owner;         // 调度此 NAPI 的 CPU
    struct net_device  *dev;               // 所属网络设备
    struct sk_buff     *skb;               // GRO 复用的 skb (napi_get_frags)
    struct gro_node     gro;               // GRO 节点（hash 表 + bitmask）
    struct hrtimer      timer;             // GRO 延迟刷新定时器 / busy poll 定时器
    struct task_struct *thread;            // 线程化 NAPI 的 kthread
    unsigned long       gro_flush_timeout;  // GRO 刷新超时 (ns)
    unsigned long       irq_suspend_timeout;// 中断挂起超时
    u32                 defer_hard_irqs;    // 完成 NAPI 后延迟启 IRQ 的轮次
    u32                 napi_id;            // NAPI 唯一 ID（用于 busy poll 索引）
    struct list_head    dev_list;           // 设备链表节点
    struct hlist_node   napi_hash_node;     // NAPI hash 表节点
    int                 irq;                // 关联的中断号
    struct irq_affinity_notify notify;      // IRQ 亲和性通知
    int                 napi_rmap_idx;      // CPU reverse map 索引
    int                 index;              // NAPI 索引
    struct napi_config *config;            // NAPI 配置（新接口）
};
```

关键字段解析：
- **`state`**：原子位图，控制 NAPI 的调度、完成、禁用等状态
- **`poll_list`**：NAPI 通过此节点挂入 `softnet_data.poll_list`
- **`weight`**：驱动处理的权重，igb 通常设为 64
- **`poll`**：驱动实现的轮询函数，返回已处理的帧数
- **`gro`**：嵌入的 GRO 节点，在 NAPI 上下文中进行包合并
- **`timer`**：复用于 GRO 延迟刷新和 busy poll 超时

### 2.2 NAPI State 标志位 (netdevice.h:421-448)

```c
// include/linux/netdevice.h:421
enum {
    NAPI_STATE_SCHED,             // Poll 已调度（核心标志）
    NAPI_STATE_MISSED,            // 需要重新调度
    NAPI_STATE_DISABLE,           // 待禁用
    NAPI_STATE_NPSVC,             // netpoll 使用中——不要从 poll_list 出队
    NAPI_STATE_LISTED,            // 已添加到系统链表中
    NAPI_STATE_NO_BUSY_POLL,      // 不在 napi_hash 中，不支持 busy polling
    NAPI_STATE_IN_BUSY_POLL,      // 当前正在 busy poll（不要重新启用中断）
    NAPI_STATE_PREFER_BUSY_POLL,  // 优先使用 busy polling 而非 softirq
    NAPI_STATE_THREADED,          // 在独立线程中执行 poll
    NAPI_STATE_SCHED_THREADED,    // 线程化 NAPI 已调度（等待线程被唤醒）
    NAPI_STATE_HAS_NOTIFIER,      // 有 IRQ 亲和性通知者
    NAPI_STATE_THREADED_BUSY_POLL,// 线程化 NAPI 应 busy poll
};

// 对应的 BIT 宏 (netdevice.h:436)
enum {
    NAPIF_STATE_SCHED               = BIT(NAPI_STATE_SCHED),
    NAPIF_STATE_MISSED              = BIT(NAPI_STATE_MISSED),
    NAPIF_STATE_DISABLE             = BIT(NAPI_STATE_DISABLE),
    NAPIF_STATE_NPSVC               = BIT(NAPI_STATE_NPSVC),
    NAPIF_STATE_LISTED              = BIT(NAPI_STATE_LISTED),
    NAPIF_STATE_NO_BUSY_POLL        = BIT(NAPI_STATE_NO_BUSY_POLL),
    NAPIF_STATE_IN_BUSY_POLL        = BIT(NAPI_STATE_IN_BUSY_POLL),
    NAPIF_STATE_PREFER_BUSY_POLL    = BIT(NAPI_STATE_PREFER_BUSY_POLL),
    NAPIF_STATE_THREADED            = BIT(NAPI_STATE_THREADED),
    NAPIF_STATE_SCHED_THREADED      = BIT(NAPI_STATE_SCHED_THREADED),
    NAPIF_STATE_HAS_NOTIFIER        = BIT(NAPI_STATE_HAS_NOTIFIER),
    NAPIF_STATE_THREADED_BUSY_POLL  = BIT(NAPI_STATE_THREADED_BUSY_POLL),
};
```

核心状态转换：

```
初始状态: state = 0
    │
    ├── napi_schedule_prep() 设置 SCHED
    │   state = SCHED
    │
    ├── net_rx_action() 正在 poll
    │   state = SCHED（保持不变）
    │
    ├── 如果在 poll 期间又有包到达
    │   napi_schedule_prep() 设置 SCHED | MISSED
    │
    └── napi_complete_done()
        成功: state = 0（清除 SCHED）
        有 MISSED: state = SCHED（重新调度）
```

---

## 3.调度路径：从硬件中断到软中断

### 3.1 napi_schedule — 内联快速路径 (netdevice.h:558)

驱动 ISR 中调用的标准入口：

```c
// include/linux/netdevice.h:558
static inline bool napi_schedule(struct napi_struct *n)
{
    if (napi_schedule_prep(n)) {
        __napi_schedule(n);
        return true;
    }
    return false;
}
```

两步操作：
1. **`napi_schedule_prep()`**：执行原子"test-and-set SCHED"操作
2. **`__napi_schedule()`**：将 NAPI 加入 poll_list 并触发软中断

如果 NAPI 已经在 SCHED 状态，`napi_schedule_prep()` 返回 false，但同时会设置 `MISSED` 标志——这告诉 `net_rx_action` 在本次 poll 完成后不要清除 SCHED，必须再轮询一轮。

### 3.2 napi_schedule_prep — 原子检查与调度 (dev.c:6729)

```c
// net/core/dev.c:6729
bool napi_schedule_prep(struct napi_struct *n)
{
    unsigned long new, val = READ_ONCE(n->state);

    do {
        if (unlikely(val & NAPIF_STATE_DISABLE))
            return false;
        new = val | NAPIF_STATE_SCHED;

        // 巧妙：如果 SCHED 已经设置，同时设置 MISSED
        // 使用位运算而非条件判断以获得更好的指令生成
        new |= (val & NAPIF_STATE_SCHED) / NAPIF_STATE_SCHED *
                                           NAPIF_STATE_MISSED;
    } while (!try_cmpxchg(&n->state, &val, new));

    return !(val & NAPIF_STATE_SCHED);
}
```

关键逻辑：
- **DISABLE 检查**：如果 NAPI 正在被禁用，拒绝调度
- **SCHED → MISSED 自动转换**：`(val & SCHED) / SCHED * MISSED` 利用 C 整数运算实现——如果 SCHED 已置位，结果为 `1 * MISSED = MISSED`；否则为 0
- **返回值语义**：仅当 SCHED 之前未设置（即首次调度）时返回 true
- **原子性**：`try_cmpxchg` 保证在竞争条件下只有一个 CPU 能成功设置

### 3.3 __napi_schedule — 软中断触发 (dev.c:6710)

```c
// net/core/dev.c:6710
void __napi_schedule(struct napi_struct *n)
{
    unsigned long flags;

    local_irq_save(flags);
    ____napi_schedule(this_cpu_ptr(&softnet_data), n);
    local_irq_restore(flags);
}
```

关中断保护是因为接下来的操作涉及 per-CPU 数据结构和 softirq 触发。

### 3.4 ____napi_schedule — 核心调度 (dev.c:4957)

```c
// net/core/dev.c:4957
static inline void ____napi_schedule(struct softnet_data *sd,
                                     struct napi_struct *napi)
{
    struct task_struct *thread;

    lockdep_assert_irqs_disabled();

    if (test_bit(NAPI_STATE_THREADED, &napi->state)) {
        thread = READ_ONCE(napi->thread);
        if (thread) {
            if (use_backlog_threads() &&
                thread == raw_cpu_read(backlog_napi))
                goto use_local_napi;

            set_bit(NAPI_STATE_SCHED_THREADED, &napi->state);
            wake_up_process(thread);
            return;
        }
    }

use_local_napi:
    DEBUG_NET_WARN_ON_ONCE(!list_empty(&napi->poll_list));
    list_add_tail(&napi->poll_list, &sd->poll_list);
    WRITE_ONCE(napi->list_owner, smp_processor_id());
    if (!sd->in_net_rx_action)
        raise_softirq_irqoff(NET_RX_SOFTIRQ);
}
```

完整调度逻辑：
1. **线程化 NAPI 检查**：如果启用了 `NAPI_STATE_THREADED`，唤醒专用的 kthread 而非加入 poll_list
2. **加入 poll_list**：标准路径——将 napi_struct 加入当前 CPU 的 `softnet_data.poll_list` 尾部
3. **触发软中断**：如果不在 `net_rx_action` 上下文中（防止递归），调用 `raise_softirq_irqoff(NET_RX_SOFTIRQ)`

---

## 4. 软中断处理：net_rx_action

### 4.1 注册 (dev.c:13289)

```c
// net/core/dev.c:13289
open_softirq(NET_RX_SOFTIRQ, net_rx_action);
```

在 `net_dev_init()` 中注册。当 `NET_RX_SOFTIRQ` 被触发时，`net_rx_action()` 将被调用。

### 4.2 net_rx_action — THE 处理函数 (dev.c:7914)

```c
// net/core/dev.c:7914
static __latent_entropy void net_rx_action(void)
{
    struct softnet_data *sd = this_cpu_ptr(&softnet_data);
    unsigned long time_limit = jiffies +
        usecs_to_jiffies(READ_ONCE(net_hotdata.netdev_budget_usecs));
    struct bpf_net_context __bpf_net_ctx, *bpf_net_ctx;
    int budget = READ_ONCE(net_hotdata.netdev_budget);
    LIST_HEAD(list);
    LIST_HEAD(repoll);

    bpf_net_ctx = bpf_net_ctx_set(&__bpf_net_ctx);
start:
    sd->in_net_rx_action = true;
    local_irq_disable();
    list_splice_init(&sd->poll_list, &list);    // 原子取出所有待处理的 NAPI
    local_irq_enable();

    for (;;) {
        struct napi_struct *n;

        skb_defer_free_flush();

        if (list_empty(&list)) {
            if (list_empty(&repoll)) {
                sd->in_net_rx_action = false;
                barrier();
                // 检查是否在 in_net_rx_action=false 之前又有新调度
                if (!list_empty(&sd->poll_list))
                    goto start;
                if (!sd_has_rps_ipi_waiting(sd))
                    goto end;
            }
            break;
        }

        n = list_first_entry(&list, struct napi_struct, poll_list);
        budget -= napi_poll(n, &repoll);

        // 预算耗尽或时间超过限制 → 挂起剩余工作
        if (unlikely(budget <= 0 ||
                     time_after_eq(jiffies, time_limit))) {
            WRITE_ONCE(sd->time_squeeze, sd->time_squeeze + 1);
            break;
        }
    }

    local_irq_disable();
    list_splice_tail_init(&sd->poll_list, &list);
    list_splice_tail(&repoll, &list);
    list_splice(&list, &sd->poll_list);
    if (!list_empty(&sd->poll_list))
        __raise_softirq_irqoff(NET_RX_SOFTIRQ);  // 重新触发
    else
        sd->in_net_rx_action = false;

    net_rps_action_and_irq_enable(sd);
end:
    bpf_net_ctx_clear(bpf_net_ctx);
}
```

核心流程解析：

**Phase 1: 准备阶段**
```
local_irq_disable()
list_splice_init(&sd->poll_list, &list)    ← 原子取出整个链表
local_irq_enable()
sd->in_net_rx_action = true                ← 防止 ____napi_schedule 递归触发软中断
```

**Phase 2: 轮询循环**
```
for each NAPI in list:
    budget -= napi_poll(n, &repoll)         ← 调用驱动的 poll 函数
    if budget <= 0 or time_limit exceeded:
        break                                ← 软中断处理被打断
```

**Phase 3: 清理与重新调度**
```
将 list 中未完成的 NAPI + repoll 链表合并回 sd->poll_list
if poll_list 非空:
    __raise_softirq_irqoff(NET_RX_SOFTIRQ)  ← 重新触发软中断
else:
    sd->in_net_rx_action = false            ← 没有剩余工作
```

### 4.3 双重限制：Budget + Time

| 限制 | 来源 | 默认值 | 作用 |
|------|------|--------|------|
| **Budget** | `net_hotdata.netdev_budget` | 300 | 防止单次软中断处理过多包 |
| **Time Limit** | `net_hotdata.netdev_budget_usecs` | jiffies + N | 防止软中断占用 CPU 过久 |

两者任何一个超限，`net_rx_action` 都会中断处理，将剩余 NAPI 留在 poll_list 中，通过重新触发 `NET_RX_SOFTIRQ` 让下一轮处理。

`sd->time_squeeze` 记录了被打断的次数——如果此值持续增加，说明网络负载超出了单次软中断的处理能力。

### 4.4 repoll 链表的作用

`repoll` 链表用于收集 `napi_complete_done()` 返回 false（即需要重新 poll）的 NAPI。这些 NAPI 在下一轮继续处理。设计原因：

```
场景：NAPI poll 返回 budget 但 napi_complete_done 因 MISSED 标志返回 false
→ 不能简单地放回 poll_list（会导致重复处理）
→ 由 napi_poll() 内部放入 repoll 链表
→ 在下一轮处理
```

---

## 5. NAPI 完成路径

### 5.1 napi_complete_done — 标记完成 (dev.c:6771)

```c
// net/core/dev.c:6771
bool napi_complete_done(struct napi_struct *n, int work_done)
{
    unsigned long flags, val, new, timeout = 0;
    bool ret = true;

    // 1. 安全检查：netpoll 或 busy poll 上下文中不处理
    if (unlikely(n->state & (NAPIF_STATE_NPSVC |
                             NAPIF_STATE_IN_BUSY_POLL)))
        return false;

    // 2. GRO flush 处理
    if (work_done) {
        if (n->gro.bitmask)
            timeout = napi_get_gro_flush_timeout(n);
        n->defer_hard_irqs_count = napi_get_defer_hard_irqs(n);
    }
    if (n->defer_hard_irqs_count > 0) {
        n->defer_hard_irqs_count--;
        timeout = napi_get_gro_flush_timeout(n);
        if (timeout)
            ret = false;                    // 需要重新 poll
    }

    gro_flush_normal(&n->gro, !!timeout);

    // 3. 从 poll_list 中移除
    if (unlikely(!list_empty(&n->poll_list))) {
        local_irq_save(flags);
        list_del_init(&n->poll_list);
        local_irq_restore(flags);
    }
    WRITE_ONCE(n->list_owner, -1);

    // 4. 原子清除 SCHED 标志
    val = READ_ONCE(n->state);
    do {
        WARN_ON_ONCE(!(val & NAPIF_STATE_SCHED));

        new = val & ~(NAPIF_STATE_MISSED | NAPIF_STATE_SCHED |
                      NAPIF_STATE_SCHED_THREADED |
                      NAPIF_STATE_PREFER_BUSY_POLL);

        // 如果 MISSED 被设置，保留 SCHED → 驱动需再次 poll
        new |= (val & NAPIF_STATE_MISSED) / NAPIF_STATE_MISSED *
                                            NAPIF_STATE_SCHED;
    } while (!try_cmpxchg(&n->state, &val, new));

    if (unlikely(val & NAPIF_STATE_MISSED)) {
        __napi_schedule(n);                 // 重新调度
        return false;
    }

    // 5. GRO 延迟刷新定时器
    if (timeout)
        hrtimer_start(&n->timer, ns_to_ktime(timeout),
                      HRTIMER_MODE_REL_PINNED);
    return ret;
}
```

关键步骤：
1. **`gro_flush_normal()`**：如果本轮有工作，刷新 GRO 中已合并的包到协议栈
2. **从 poll_list 移除**：清除 `SCHED` 前必须先出队
3. **处理 MISSED 标志**：如果 MISSED 被设置（说明在 poll 期间又有包到达），重新调用 `__napi_schedule()` 确保再轮询一轮
4. **延迟刷新定时器**：如果有 GRO 超时配置且还有未刷新数据，启动 hrtimer 保证后续刷新

### 5.2 napi_poll — net_rx_action 的工作循环 (dev.c:7850)

napi_poll 是 `net_rx_action` 轮询循环中调用的内部函数：

```c
static int napi_poll(struct napi_struct *n, struct list_head *repoll)
{
    int work, weight;

    weight = n->weight;

    work = n->poll(n, weight);     // 调用驱动 poll 函数
    trace_napi_poll(n, work, weight);

    if (unlikely(work > weight))
        pr_err_once("NAPI poll function %pS returned %d, exceeding its budget of %d.\n",
                    n->poll, work, weight);

    if (likely(work < weight))
        return work;

    // work == weight 且 napi_complete_done 返回 false
    if (napi_complete_done(n, work))
        return work;

    // 仍需继续处理：加入 repoll 链表
    list_add_tail(&n->poll_list, repoll);
    return work;
}
```

三种返回情况：
- **work < weight**：本轮处理完毕，NAPI 完成
- **work == weight + napi_complete_done 返回 true**：正常工作完成（处理了等于预算的包但没有 MISSED）
- **work == weight + napi_complete_done 返回 false**：还需处理，加入 repoll

---

## 6. 传统路径：process_backlog

### 6.1 process_backlog — 非 NAPI 设备的回退 (dev.c:6644)

不是所有设备都支持 NAPI。对于传统驱动或特殊的收包路径（如 loopback），使用 `process_backlog`：

```c
// net/core/dev.c:6644
static int process_backlog(struct napi_struct *napi, int quota)
{
    struct softnet_data *sd = container_of(napi, struct softnet_data, backlog);
    bool again = true;
    int work = 0;

    if (sd_has_rps_ipi_waiting(sd)) {
        local_irq_disable();
        net_rps_action_and_irq_enable(sd);
    }

    napi->weight = READ_ONCE(net_hotdata.dev_rx_weight);
    while (again) {
        struct sk_buff *skb;

        local_lock_nested_bh(&softnet_data.process_queue_bh_lock);
        while ((skb = __skb_dequeue(&sd->process_queue))) {
            local_unlock_nested_bh(&softnet_data.process_queue_bh_lock);
            rcu_read_lock();
            __netif_receive_skb(skb);       // 送入协议栈
            rcu_read_unlock();
            if (++work >= quota) {
                rps_input_queue_head_add(sd, work);
                return work;
            }
            local_lock_nested_bh(&softnet_data.process_queue_bh_lock);
        }
        local_unlock_nested_bh(&softnet_data.process_queue_bh_lock);

        backlog_lock_irq_disable(sd);
        if (skb_queue_empty(&sd->input_pkt_queue)) {
            napi->state &= NAPIF_STATE_THREADED;
            again = false;
        } else {
            local_lock_nested_bh(&softnet_data.process_queue_bh_lock);
            skb_queue_splice_tail_init(&sd->input_pkt_queue,
                                       &sd->process_queue);
            local_unlock_nested_bh(&softnet_data.process_queue_bh_lock);
        }
        backlog_unlock_irq_enable(sd);
    }

    if (work)
        rps_input_queue_head_add(sd, work);
    return work;
}
```

与传统 NAPI poll 的区别：
- **两级队列**：`input_pkt_queue`（中断上下文入队）→ `process_queue`（软中断出队处理）
- **无 GRO**：传统路径的包已经经过 `netif_rx()` 入队，GRO 无法介入
- **批量迁移**：使用 `skb_queue_splice_tail_init()` 一次性将整个 input_pkt_queue 迁移过来

---

## 7. 完整调度路径 ASCII 流程

```
  ┌─────────────────────────────────────────────────────────────────────────────┐
  │                            NAPI 收包全流程                                      │
  ├─────────────────────────────────────────────────────────────────────────────┤
  │                                                                             │
  │  硬件中断到达 (MSI-X / INTx)                                                   │
  │       │                                                                      │
  │       ▼                                                                      │
  │  ┌──────────────────────┐                                                    │
  │  │ 驱动 ISR               │  igb_msix_ring() [igb_main.c:7153]               │
  │  │ - 更新 ITR             │  或 igb_intr() [igb_main.c:8211]                  │
  │  │ - 硬件自动 mask IRQ    │                                                    │
  │  └──────────┬───────────┘                                                    │
  │             │                                                                 │
  │             ▼                                                                 │
  │  ┌──────────────────────┐                                                    │
  │  │ napi_schedule()       │  netdevice.h:558                                  │
  │  │  → napi_schedule_prep()│  原子设置 NAPI_STATE_SCHED                        │
  │  └──────────┬───────────┘                                                    │
  │             │                                                                 │
  │             ▼                                                                 │
  │  ┌──────────────────────┐                                                    │
  │  │ __napi_schedule()     │  net/core/dev.c:6710                              │
  │  │  → ____napi_schedule()│  net/core/dev.c:4957                              │
  │  │                       │                                                    │
  │  │  线程化 NAPI?          │                                                    │
  │  │  ├─ 是 → wake_up_process(thread) [返回]                                   │
  │  │  └─ 否 → list_add_tail(&napi->poll_list, &sd->poll_list)                 │
  │  │           raise_softirq_irqoff(NET_RX_SOFTIRQ)                           │
  │  └──────────┬───────────┘                                                    │
  │             │                                                                 │
  │             ▼                                                                 │
  │  ┌──────────────────────┐                                                    │
  │  │ NET_RX_SOFTIRQ 触发    │  由 open_softirq() 注册:                          │
  │  │                       │  net/core/dev.c:13289                             │
  │  └──────────┬───────────┘                                                    │
  │             │                                                                 │
  │             ▼                                                                 │
  │  ┌──────────────────────────────┐                                             │
  │  │ net_rx_action()              │  net/core/dev.c:7914                        │
  │  │                              │                                             │
  │  │ 1. list_splice_init poll_list│  原子取出所有待处理的 NAPI                    │
  │  │ 2. for each napi in list:    │                                             │
  │  │      napi_poll(n, &repoll)   │  调用 n->poll()                             │
  │  │ 3. budget ≤ 0 || time limit? │                                             │
  │  │    → break (重新触发软中断)    │                                             │
  │  │ 4. 合并回 poll_list           │                                             │
  │  │ 5. 如非空 → raise softirq    │                                             │
  │  └──────────┬───────────────────┘                                             │
  │             │                                                                 │
  │             ▼                                                                 │
  │  ┌──────────────────────┐                                                    │
  │  │ napi->poll()          │  igb_poll() [igb_main.c:8278]                     │
  │  │ - 驱动收包 (clean_rx) │  budget = 64                                      │
  │  │ - gro_receive_skb()   │  → 进入 GRO 合并                                  │
  │  │ - napi_complete_done()│                                                     │
  │  └──────────┬───────────┘                                                    │
  │             │                                                                 │
  │      ┌──────┴──────┐                                                         │
  │      │             │                                                         │
  │   work < budget   work == budget                                             │
  │  (本轮处理完毕)    (处理到预算上限)                                              │
  │      │             │                                                         │
  │      ▼             ▼                                                         │
  │  napi_complete   返回 budget (触发重新调度)                                     │
  │  _done()                                                                     │
  │      │                                                                       │
  │      ▼                                                                       │
  │  清除 SCHED                                                                  │
  │  重新启用 IRQ (unmask)                                                        │
  │                                                                             │
  └─────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. 关键设计要点

### 8.1 原子状态机

NAPI 使用 `try_cmpxchg` 原子操作管理状态转换，主要保证：
- 只有一个 CPU 能成功设置 SCHED 并调度 NAPI
- MISSED 标志的"自动"设置避免竞态——如果 poll 期间又有包到达，不会丢失

### 8.2 中断屏蔽模式

```
驱动 ISR 的标准模式 (以 igb 为例):

  IRQ 到达
    ├── 硬件自动屏蔽 (Auto-Mask on ICR read) 或驱动显式 mask
    ├── napi_schedule()
    └── ISR 返回
  
  ... (NET_RX_SOFTIRQ → net_rx_action → igb_poll)
  
  napi_complete_done() 返回 true
    └── 驱动重新启用 IRQ (igb_ring_irq_enable)
```

这个 "mask → schedule → poll → complete → unmask" 模式确保了在软中断处理期间不会有新的中断干扰。

### 8.3 防止迷失的包

`net_rx_action` 末尾的 `barrier()` + `!list_empty(&sd->poll_list)` 检查解决了经典竞态：

```
场景:
  1. net_rx_action 几乎处理完所有 NAPI
  2. 设置 sd->in_net_rx_action = false  ← 现在可以接收新调度
  3. [中断到达] napi_schedule → 加入 poll_list
  4. barrier() → 然后检查 poll_list
  5. 发现非空 → goto start → 继续处理
```

如果没有 barrier()，步骤 3 的 poll_list 更新可能在步骤 4 的检查之前不可见（store forwarding / 乱序）。

---

## 9. 调优参数

| 参数 | 路径 | 默认值 | 含义 |
|------|------|--------|------|
| netdev_budget | sysctl | 300 | 每次 softirq 处理的最大包数 |
| netdev_budget_usecs | sysctl | 2000 | 每次 softirq 的最大时间 (µs) |
| weight | 驱动设定 | 64 | 单个 NAPI 每次 poll 的最大包数 |
| dev_weight_rx_bias | sysctl | 1 | process_backlog 的权重乘数 |

---

## 10. 总结

NAPI 核心通过精巧的原子状态机和软中断机制实现了高效收包：

1. **`napi_schedule()`** 通过 `try_cmpxchg` 原子设置 SCHED 标志，防止重复调度
2. **`____napi_schedule()`** 将 NAPI 加入 per-CPU poll_list 并触发 NET_RX_SOFTIRQ
3. **`net_rx_action()`** 批量取出 poll_list，逐个调用驱动的 poll 函数
4. **`napi_complete_done()`** 处理完成逻辑——清除 SCHED、刷新 GRO、处理 MISSED
5. **Budget + Time Limit** 双重限制防止软中断占满 CPU

下一篇将深入分析 GRO (Generic Receive Offload) 的实现——数据包如何被合并以减少协议栈开销。

---

下一篇: [第2篇：GRO — 通用接收卸载](./02-gro.md)
