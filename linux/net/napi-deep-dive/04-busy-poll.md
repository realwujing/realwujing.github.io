# 第4篇：忙轮询与线程化 NAPI

> 源码：include/net/busy_poll.h net/core/dev.c include/linux/netdevice.h

系列目录：[NAPI 内核源码深度解析](./README.md)

---

## 1. 概述

前三篇文章覆盖了 NAPI 的标准路径：中断 → 调度 → 软中断 → poll → 完成。这条路径在大多数情况下工作良好，但有两个场景需要特殊处理：

1. **低延迟场景**：中断 + 软中断的上下文切换延迟可能达到几十微秒，对高频交易、实时应用不可接受
2. **CPU 隔离场景**：在 CPU 隔离（isolcpus）或 CONFIG_NO_HZ_FULL 环境中，软中断在 ksoftirqd 中运行，延迟不可控

Linux 提供了两种高级模式应对这些场景：
- **Busy Poll（忙轮询）**：用户态主动在进程上下文中轮询 NAPI，跳过软中断
- **Threaded NAPI（线程化）**：NAPI poll 在专用的内核线程中运行，与软中断解耦
- **IRQ Suspension（中断挂起）**：完成一次 poll 后延迟重新启用中断

本文将逐一分析这些机制的设计、实现和适用场景。

---

## 2. Busy Poll 忙轮询

### 2.1 设计理念

传统路径的问题在于：数据已到达网卡 Ring Buffer，但要经历 MSI-X 中断 → ISR → 软中断 → `net_rx_action` → 驱动 poll 才能拿到数据。这个链条的延迟在低负载时可能很短（<10µs），但在高负载或 CPU 竞争时可能很长。

Busy Poll 的思路：**用户态应用程序直接告诉内核"我在等数据，你帮我轮询一下网卡"**。内核在当前进程的上下文中直接调用驱动的 poll 函数，跳过中断和软中断。

### 2.2 核心接口 (busy_poll.h)

```c
// include/net/busy_poll.h:52
void napi_busy_loop(unsigned int napi_id,
                    bool (*loop_end)(void *, unsigned long),
                    void *loop_end_arg,
                    bool prefer_busy_poll,
                    u16 budget);

// include/net/busy_poll.h:117
static inline void sk_busy_loop(struct sock *sk, int nonblock)
{
#ifdef CONFIG_NET_RX_BUSY_POLL
    unsigned int napi_id = READ_ONCE(sk->sk_napi_id);

    if (napi_id_valid(napi_id))
        napi_busy_loop(napi_id,
                       nonblock ? NULL : sk_busy_loop_end, sk,
                       READ_ONCE(sk->sk_prefer_busy_poll),
                       READ_ONCE(sk->sk_busy_poll_budget)
                           ?: BUSY_POLL_BUDGET);
#endif
}
```

- `napi_busy_loop()`：轮询指定的 NAPI 实例
- `sk_busy_loop()`：通过 socket 找到关联的 NAPI 并轮询
- `BUSY_POLL_BUDGET` (busy_poll.h:32)：`8`，比标准 NAPI weight (64) 小得多

### 2.3 入口：从 socket 到 NAPI

```
  ┌────────────────────────────────────────────┐
  │  用户态应用程序                               │
  │  recvmsg() / epoll_wait() / select()        │
  │  sk->sk_ll_usec = 50  (SO_BUSY_POLL)       │
  └──────────────────┬─────────────────────────┘
                     │
                     ▼
  ┌────────────────────────────────────────────┐
  │  内核: tcp_recvmsg() / udp_recvmsg()       │
  │                                            │
  │  if (sk_can_busy_loop(sk)):                 │
  │    sk_busy_loop(sk, nonblock)   ←─ 关键     │
  │                                            │
  │  sk_can_busy_loop 检查:                     │
  │  - sk->sk_ll_usec > 0 (应用设置的低延迟超时)  │
  │  - !signal_pending(current) (无待处理信号)    │
  └──────────────────┬─────────────────────────┘
                     │
                     ▼
  ┌────────────────────────────────────────────┐
  │  __napi_busy_loop()          [dev.c:6935]   │
  │                                            │
  │  - 通过 napi_id 哈希查找 napi_struct        │
  │  - 设置 NAPI_STATE_IN_BUSY_POLL            │
  │  - 循环调用 napi->poll()                   │
  │  - 直到 timeout 或数据到达                  │
  │  - 恢复状态                                 │
  └────────────────────────────────────────────┘
```

### 2.4 核心实现：__napi_busy_loop (dev.c:6935)

```c
// net/core/dev.c:6935
static void __napi_busy_loop(unsigned int napi_id,
        bool (*loop_end)(void *, unsigned long),
        void *loop_end_arg, unsigned flags, u16 budget)
{
    unsigned long start_time = loop_end ? busy_loop_current_time() : 0;
    int (*napi_poll)(struct napi_struct *napi, int budget);
    struct napi_struct *napi;

restart:
    napi_poll = NULL;

    rcu_read_lock();
    napi = napi_by_id(napi_id);     // 通过 NAPI ID 在 hash 表中查找
    if (!napi)
        goto out;

    if (!IS_ENABLED(CONFIG_PREEMPT_RT))
        preempt_disable();

    for (;;) {
        int work = 0;

        local_bh_disable();
        // 原子设置 IN_BUSY_POLL 标志，通知 NAPI 调度器和网络设备
        if (!test_and_set_bit(NAPI_STATE_IN_BUSY_POLL, &napi->state))
            ++napi->napi_rmap_idx;

        work = napi_poll(napi, budget);
        trace_napi_poll(napi, work, budget);

        gro_normal_list(&napi->gro);
        local_bh_enable();

        if (work > 0)
            goto count;

        if (loop_end(loop_end_arg, start_time))
            break;

        busy_wait(1);  // 延迟 1µs 再试
    }

    // ...
out:
    rcu_read_unlock();
}
```

关键设计：
1. **`NAPI_STATE_IN_BUSY_POLL`**：设置后，硬件中断不会重新调度 NAPI（因为进程正在主动轮询）
2. **`busy_wait(1)`**：如果没有数据，短暂等待 1µs 再试
3. **`loop_end` 回调**：由 `sk_busy_loop_end` (busy_poll.h:50) 实现，检查是否超时
4. **直接调用 napi_poll**：完全绕过软中断路径

### 2.5 Busy Poll 的完成：busy_poll_stop (dev.c:6886)

```c
// net/core/dev.c:6886
static void busy_poll_stop(struct napi_struct *napi,
                           void *have_poll_lock,
                           unsigned flags, u16 budget)
{
    unsigned long timeout = 0;
    int rc;

    // 清除 MISSED 标志（busy poll 本来就在处理中）
    clear_bit(NAPI_STATE_MISSED, &napi->state);
    clear_bit(NAPI_STATE_IN_BUSY_POLL, &napi->state);

    local_bh_disable();

    if (flags & NAPI_F_PREFER_BUSY_POLL) {
        napi->defer_hard_irqs_count = napi_get_defer_hard_irqs(napi);
        if (napi->defer_hard_irqs_count) {
            timeout = napi_get_gro_flush_timeout(napi);
        }
    }

    // 再调用一次 poll 确保没有遗漏
    rc = napi->poll(napi, budget);

    netpoll_poll_unlock(have_poll_lock);

    if (rc == budget)
        __busy_poll_stop(napi, timeout);    // 还有工作 → 重新调度
    local_bh_enable();
}
```

### 2.6 Busy Poll 配置参数

| 参数 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `SO_BUSY_POLL` | socket option | 0 (禁用) | 每个 socket 的 busy poll 超时 (µs) |
| `sysctl_net_busy_read` | sysctl | 0 | 全局 busy read 超时 |
| `sysctl_net_busy_poll` | sysctl | 0 | 全局 busy poll 超时 (用于 epoll/select) |
| `sk->sk_ll_usec` | per-socket | 0 | 低延迟超时 (µs)，0=禁用 |
| `sk->sk_prefer_busy_poll` | per-socket | false | 优先使用 busy poll 而非 softirq |
| `sk->sk_busy_poll_budget` | per-socket | BUSY_POLL_BUDGET (8) | busy poll 的每轮预算 |

### 2.7 Busy Poll 中的中断处理

当 Busy Poll 调用 `napi->poll()` 时，如果有新数据到达：
- `napi_schedule_prep()` 检测到 `NAPI_STATE_IN_BUSY_POLL` → 不设置 `SCHED`
- 新数据由当前 busy poll 循环直接处理
- 忙轮询结束后，`busy_poll_stop()` 清理状态，决定是否需要重新调度

---

## 3. 线程化 NAPI

### 3.1 设计动机

传统 NAPI 依赖软中断在 hardirq 退出时执行。但在以下场景中，软中断路径有局限：

1. **CPU 隔离 (isolcpus + NO_HZ_FULL)**：软中断可能在 ksoftirqd 中延迟执行
2. **实时内核 (PREEMPT_RT)**：软中断被线程化，增加延迟
3. **调度灵活性**：专用线程可以设置调度策略和优先级
4. **可预测性**：线程化 NAPI 提供更可预测的执行延迟

### 3.2 线程化 NAPI 的调度 (dev.c:4957)

当 NAPI 设置了 `NAPI_STATE_THREADED` 标志时，`____napi_schedule()` 采用不同的路径：

```c
// net/core/dev.c:4957 (相关片段)
static inline void ____napi_schedule(struct softnet_data *sd,
                                     struct napi_struct *napi)
{
    // ...
    if (test_bit(NAPI_STATE_THREADED, &napi->state)) {
        thread = READ_ONCE(napi->thread);
        if (thread) {
            if (use_backlog_threads() &&
                thread == raw_cpu_read(backlog_napi))
                goto use_local_napi;

            set_bit(NAPI_STATE_SCHED_THREADED, &napi->state);
            wake_up_process(thread);      // 唤醒专用线程
            return;
        }
    }

use_local_napi:
    // 标准路径：加入 poll_list
    list_add_tail(&napi->poll_list, &sd->poll_list);
    // ...
}
```

与标准路径的区别：
- **不加入 poll_list**：线程独立于 softirq_data 的 NAPI 链表
- **不触发 NET_RX_SOFTIRQ**：线程自己就是执行上下文
- **设置 `SCHED_THREADED`**：标记线程已被调度

### 3.3 NAPI 线程流程：napi_threaded_poll (dev.c:7887)

```c
// net/core/dev.c:7887
static int napi_threaded_poll(void *data)
{
    struct napi_struct *napi = data;
    unsigned long last_qs = jiffies;
    bool want_busy_poll;
    bool in_busy_poll;
    unsigned long val;

    while (!napi_thread_wait(napi)) {
        val = READ_ONCE(napi->state);

        want_busy_poll = val & NAPIF_STATE_THREADED_BUSY_POLL;
        in_busy_poll = val & NAPIF_STATE_IN_BUSY_POLL;

        if (unlikely(val & NAPIF_STATE_DISABLE))
            want_busy_poll = false;

        if (want_busy_poll != in_busy_poll)
            assign_bit(NAPI_STATE_IN_BUSY_POLL, &napi->state,
                       want_busy_poll);

        napi_threaded_poll_loop(napi,
                                want_busy_poll ? &last_qs : NULL);
    }

    return 0;
}
```

线程的主循环：
1. **`napi_thread_wait()`**：等待 `SCHED_THREADED` 被设置（即 `____napi_schedule` 被调用）
2. 如果启用了 `THREADED_BUSY_POLL`，保持 `IN_BUSY_POLL` 状态
3. 调用 `napi_threaded_poll_loop()` 执行实际的 poll 工作

### 3.4 线程化 NAPI 的状态标志

| 标志 | 含义 | 设置者 |
|------|------|--------|
| `NAPI_STATE_THREADED` | NAPI 使用线程模式 | 驱动初始化时设置 |
| `NAPI_STATE_SCHED_THREADED` | 线程已被调度（等待被唤醒） | `____napi_schedule` |
| `NAPI_STATE_THREADED_BUSY_POLL` | 线程应 busy poll | 用户空间配置 |

---

## 4. IRQ Suspension 中断挂起

### 4.1 设计原理

传统 NAPI 模式：poll 完成 → `napi_complete_done()` → 驱动重新启用中断。下一次包到达时，又要经历一次中断 → 调度的延迟。

IRQ Suspension 允许驱动在完成一次 poll 后**不立即重新启用中断**，而是通过 `defer_hard_irqs` 和 `irq_suspend_timeout` 延迟中断恢复。这意味着：

```
传统模式:
  poll完成 → 开中断 → 等包到达 → 中断 → 调度 → poll

IRQ Suspension 模式:
  poll完成 → [延迟开中断] → 可能有更多包到达（但没中断）
            → 超时后重新调度 → poll（批量处理更多包）
```

### 4.2 实现 (napi_complete_done 中, dev.c:6791)

```c
// net/core/dev.c:6771 (相关片段)
// 在 napi_complete_done() 中:
if (n->defer_hard_irqs_count > 0) {
    n->defer_hard_irqs_count--;            // 递减计数
    timeout = napi_get_gro_flush_timeout(n);
    if (timeout)
        ret = false;                       // 告诉调用者：不要开中断
}
// ...
if (timeout)
    hrtimer_start(&n->timer, ns_to_ktime(timeout),
                  HRTIMER_MODE_REL_PINNED);  // 设置定时器
```

`defer_hard_irqs_count` 的 decrement 意味着每次 poll 完成只减 1，直到计数器归零才真正启用中断。

### 4.3 Busy Poll 中的 IRQ Suspension (dev.c:6908)

```c
// busy_poll_stop() 中:
if (flags & NAPI_F_PREFER_BUSY_POLL) {
    napi->defer_hard_irqs_count = napi_get_defer_hard_irqs(napi);
    if (napi->defer_hard_irqs_count) {
        timeout = napi_get_gro_flush_timeout(napi);
    }
}
```

在 prefer_busy_poll 模式下，`busy_poll_stop()` 也使用 `defer_hard_irqs_count` 来延迟中断恢复。

---

## 5. 三种处理路径对比

### 5.1 核心对比表

| 特性 | 标准 NAPI (softirq) | Busy Poll | Threaded NAPI |
|------|---------------------|-----------|---------------|
| **执行上下文** | 软中断 (hardirq 退出) | 用户进程 | 内核线程 |
| **谁触发** | 硬件中断 | 用户态 recv/read/epoll | 硬件中断 |
| **延迟** | 中 (~10-50µs) | 极低 (~<5µs) | 低 (~5-20µs) |
| **CPU 占用** | 低 | 极高（100% 忙等待） | 中 |
| **适用场景** | 通用 | 极低延迟（金融/游戏） | CPU 隔离/RT 环境 |
| **中断参与** | 中断驱动 | 中断可选（可挂起） | 中断触发线程唤醒 |
| **批处理** | 是 (budget=64-300) | 少量 (budget=8) | 是 (budget=64) |
| **GRO** | NAPI 中的 GRO | 需在 busy_poll_stop 中 flush | NAPI 中的 GRO |
| **Budget** | 64 (NAPI weight) | 8 (BUSY_POLL_BUDGET) | 64 (NAPI weight) |
| **关键配置** | netdev_budget | SO_BUSY_POLL, sk_ll_usec | threaded napi, irq-suspend |

### 5.2 三种处理循环

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Hardirq → Softirq 路径（标准）                      │
│                                                                     │
│  中断到达                                                             │
│    │  ISR: napi_schedule()                                           │
│    ▼                                                                 │
│  NET_RX_SOFTIRQ                                                      │
│    │  net_rx_action()                                                │
│    │  budget=300, time_limit=2 jiffies                               │
│    ▼                                                                 │
│  napi->poll(budget=64)                                               │
│    │  work < 64 → napi_complete_done → 开中断                         │
│    │  work == 64 → 返回 budget → 继续                                 │
│    ▼                                                                 │
│  [下一轮软中断]                                                        │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    Timer → Softirq 路径（延迟/超时）                    │
│                                                                     │
│  GRO flush 定时器到期 / irq suspend 定时器到期                         │
│    │  hrtimer 回调: napi_schedule()                                  │
│    ▼                                                                 │
│  NET_RX_SOFTIRQ                                                      │
│    │  net_rx_action()                                                │
│    ▼                                                                 │
│  napi->poll(budget=64) → flush 旧 GRO 包                              │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    Epoll → Busy Poll 路径（低延迟）                     │
│                                                                     │
│  用户态 epoll_wait() / recvmsg()                                      │
│    │  sk_can_busy_loop() → sk_busy_loop()                            │
│    ▼                                                                 │
│  进程上下文（非软中断）                                                  │
│    │  napi_busy_loop()                                               │
│    │  budget=8 (BUSY_POLL_BUDGET)                                   │
│    │  循环 napi_poll() 直到数据或超时                                   │
│    │  busy_wait(1µs) 空闲等待                                         │
│    ▼                                                                 │
│  数据到达 → process_backlog / 或直接返回                                │
│  超时 → 返回 EAGAIN                                                  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 6. 内核配置与 sysfs

### 6.1 内核配置选项

```makefile
CONFIG_NET_RX_BUSY_POLL=y    # 启用 busy poll 支持
```

### 6.2 关键 sysfs/ethtool 接口

```bash
# Busy Poll 全局设置
sysctl net.core.busy_read=50     # 全局 busy read 超时 (µs)
sysctl net.core.busy_poll=50     # 全局 busy poll 超时 (µs)

# 线程化 NAPI
ethtool -C eth0 rx-usecs 30      # 中断聚合超时
ethtool -C eth0 adaptive-rx off  # 关闭自适应 ITR（使用固定值）

# IRQ Suspend
# 通过 napi_defer_hard_irqs 和 gro_flush_timeout 设置
```

### 6.3 Socket 级别控制

```c
// 在应用程序中
int val = 50;  // 50µs
setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &val, sizeof(val));

// 或通过 SO_PREFER_BUSY_POLL 偏向 busy poll
int prefer = 1;
setsockopt(fd, SOL_SOCKET, SO_PREFER_BUSY_POLL, &prefer, sizeof(prefer));

// 设置 busy poll budget
int budget = 16;
setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL_BUDGET, &budget, sizeof(budget));
```

---

## 7. 经典 NAPI vs Busy Poll vs Threaded NAPI 模式对比

### 7.1 模式演进图

```
                    ┌─────────────────┐
                    │  传统中断模式     │
                    │  (无 NAPI)       │
                    │  每包一个中断     │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  NAPI 标准模式   │
                    │  中断 + 软中断   │
                    │  批量处理 64 包  │
                    │  自动中断聚合    │
                    └───┬─────┬──────┘
                        │     │
           ┌────────────▼┐    └────────────┐
           │ Busy Poll   │                 │
           │ 用户态驱动   │       ┌────────▼────────┐
           │ 0 中断延迟  │       │ Threaded NAPI   │
           │ 100% CPU    │       │ 内核线程驱动     │
           │ budget=8    │       │ 实时调度        │
           └─────────────┘       │ CPU 亲和性      │
                                 └─────────────────┘
```

### 7.2 应用场景决策树

```
  你的应用需要极低延迟 (< 10µs) 吗?
    ├── 是 → 使用 Busy Poll
    │        - 设置 SO_BUSY_POLL
    │        - 考虑 SO_PREFER_BUSY_POLL
    │        - 准备好消耗 CPU（100% 占用）
    │        - 预算：低流量 (< 100K pps)
    │
    └── 否 → 标准 NAPI (大多数场景)
             - 自动工作，无需配置
             - 低流量时自动中断驱动
             - 高流量时自动批量处理

  你的环境有 CPU 隔离需求吗?
    ├── 是 (isolcpus / NO_HZ_FULL / PREEMPT_RT)
    │   → 考虑 Threaded NAPI
    │      - 设置 napi threaded=1
    │      - 设置线程 CPU 亲和性和调度策略
    │
    └── 否 → 标准 NAPI 足够
```

---

## 8. Busy Poll + Threaded NAPI 组合

当 `NAPI_STATE_THREADED_BUSY_POLL` 被设置时，线程化 NAPI 以 busy poll 方式运行：

```c
// napi_threaded_poll() 中的相关逻辑 (dev.c:7898)
want_busy_poll = val & NAPIF_STATE_THREADED_BUSY_POLL;
in_busy_poll = val & NAPIF_STATE_IN_BUSY_POLL;

if (want_busy_poll != in_busy_poll)
    assign_bit(NAPI_STATE_IN_BUSY_POLL, &napi->state,
               want_busy_poll);

napi_threaded_poll_loop(napi,
                        want_busy_poll ? &last_qs : NULL);
```

这种组合模式的优点：
- **线程可以一直保持 `IN_BUSY_POLL` 状态**，防止中断干扰
- **结合了线程化的可控性 + busy poll 的低延迟**
- 适用于需要极低延迟但还想要调度灵活性的场景

---

## 9. 调试与观测

### 9.1 Busy Poll 统计

```bash
# 查看 busy poll 命中率
cat /proc/net/softnet_stat
# 字段解释: processed, dropped, time_squeeze, cpu_collision,
#           received_rps, flow_limit_count
```

### 9.2 bpftrace 追踪 busy poll

```bpftrace
# 追踪 busy poll 调用
kprobe:napi_busy_loop
{
    printf("busy_poll: napi_id=%d\n", arg0);
}

# 追踪线程化 NAPI 唤醒
kprobe:____napi_schedule
{
    $n = (struct napi_struct *)arg1;
    if ($n->state & (1 << 9)) {  // NAPI_STATE_THREADED = 9
        printf("threaded napi scheduled\n");
    }
}
```

### 9.3 性能调优清单

| 检查项 | 工具 | 目标 |
|--------|------|------|
| softirq 时间占比 | `top`, `mpstat -I SCPU` | < 10% |
| net_rx_action time_squeeze | `/proc/net/softnet_stat` | 越小越好 |
| NAPI 线程的调度延迟 | `perf sched` | < 50µs |
| busy poll CPU 占用 | `perf top -C <cpu>` | 如果 100%，考虑降低 busy poll 时间 |
| 中断频率 | `/proc/interrupts` | 取决于流量 |

---

## 10. 系列结语

### 系列文章总结

| 篇 | 主题 | 核心内容 | 文件 |
|---:|------|----------|------|
| 1 | **NAPI 核心** | napi_struct，napi_schedule，net_rx_action，状态机 | `01-napi-core.md` |
| 2 | **GRO** | 哈希桶合并，流匹配，协议回调，GSO 配对 | `02-gro.md` |
| 3 | **驱动集成** | igb 实例：MSI-X，ITR，poll 函数，中断模式 | `03-driver.md` |
| 4 | **高级模式** | Busy Poll，线程化 NAPI，IRQ Suspension | `04-busy-poll.md` |

### NAPI 设计哲学

1. **中断 + 轮询混合**：低负载用中断（低延迟），高负载自动转轮询（高吞吐）
2. **per-CPU 无锁**：每个 CPU 独立管理 NAPI，避免跨 CPU 竞争
3. **预算控制**：budget + time_limit 防止单次处理过多
4. **可扩展性**：标准 softirq → busy poll → threaded，不同场景选不同路径
5. **GRO 深度集成**：NAPI 中的 GRO 在产生开销最小的位置进行包合并

### 关键文件速查

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `include/linux/netdevice.h` | ~5718 | napi_struct 定义、状态标志、napi_schedule |
| `net/core/dev.c` | ~13317 | NAPI 核心：net_rx_action, ____napi_schedule, napi_complete_done, busy poll |
| `net/core/gro.c` | ~842 | GRO：dev_gro_receive, gro_complete, skb_gro_receive |
| `include/net/busy_poll.h` | ~188 | Busy poll 接口：napi_busy_loop, sk_busy_loop, BUSY_POLL_BUDGET |
| `drivers/net/ethernet/intel/igb/igb_main.c` | ~10314 | igb 驱动：igb_msix_ring, igb_poll, igb_intr |

---

[⬆ 返回系列目录](./README.md)
