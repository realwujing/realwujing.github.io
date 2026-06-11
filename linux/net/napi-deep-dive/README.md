# NAPI 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)
> 源文件：`net/core/dev.c` `net/core/gro.c` | 头文件：`include/linux/netdevice.h` `include/net/busy_poll.h`

---

## 系列目录

| 篇 | 标题 | 文件 |
|---:|------|------|
| 1 | NAPI 核心 — napi_struct、napi_schedule 与 net_rx_action | [01-napi-core.md](./01-napi-core.md) |
| 2 | GRO — 通用接收卸载 | [02-gro.md](./02-gro.md) |
| 3 | 驱动集成 — igb 与 NAPI 全程 | [03-driver.md](./03-driver.md) |
| 4 | 忙轮询与线程化 NAPI | [04-busy-poll.md](./04-busy-poll.md) |

---

## 架构总览

```
设备硬件中断
    │
    │  (MSI-X / INTx)
    ▼
┌──────────────────────────┐
│  网卡驱动 ISR              │  igb_msix_ring() [igb_main.c:7153]
│  - 读取 ICR (中断原因)     │  igb_intr() [igb_main.c:8211]
│  - 屏蔽中断 (mask)         │
│  - 更新 ITR                │
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  napi_schedule()          │  netdevice.h:558
│  - napi_schedule_prep()   │  原子设置 NAPI_STATE_SCHED
│  - __napi_schedule()      │  net/core/dev.c:6710
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  ____napi_schedule()      │  net/core/dev.c:4957
│  - 加入 poll_list          │
│  - raise_softirq_irqoff   │
│    (NET_RX_SOFTIRQ)       │
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  net_rx_action()          │  net/core/dev.c:7914
│  - splice poll_list       │  NET_RX_SOFTIRQ 的处理函数
│  - 遍历 NAPI，调用 poll    │  open_softirq(NET_RX_SOFTIRQ, net_rx_action)
│  - budget 控制             │  net/core/dev.c:13289
│  - 时间限制检查            │
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│  napi->poll()             │  igb_poll() [igb_main.c:8278]
│  - igb_clean_rx_irq()    │  驱动硬件收包
│  - gro_receive_skb()      │  → 进入 GRO
│  - napi_complete_done()   │  完成本轮处理
└──────────┬───────────────┘
           │
     ┌─────┴─────┐
     │           │
     ▼           ▼
 工作完成      工作未完成
     │           │
     ▼           ▼
 napi_complete  返回 budget（触发重新调度）
     │
     ▼
 重新启用中断 (unmask IRQ)
```

---

## 核心数据结构关系

```
struct napi_struct (netdevice.h:381)
├── state             // NAPI_STATE_* 标志位 (SCHED, MISSED, THREADED...)
├── poll_list         // 挂接到 softnet_data.poll_list
├── weight            // 每次 poll 调用的处理预算
├── poll              // 驱动提供的回调函数 (int (*poll)(struct napi_struct *, int))
├── gro               // GRO 节点 (附有 hash 表和 bitmask)
├── timer             // GRO 延迟刷新的 hrtimer
├── thread            // 线程化 NAPI 的 kthread
├── dev               // 所属的网络设备
├── skb               // GRO 的复用 skb (napi_get_frags)
└── ...
        │
        │ 挂接到
        ▼
struct softnet_data
├── poll_list         // 待处理的 NAPI 链表 (由 net_rx_action 消费)
├── backlog           // 非 NAPI 设备的回退 NAPI (process_backlog)
├── input_pkt_queue   // 非 NAPI 设备的输入队列
├── process_queue     // process_backlog 的处理队列
└── ...
```

---

## 文件索引

| 文件 | 用途 |
|------|------|
| `include/linux/netdevice.h` (5718 行) | napi_struct 定义、状态标志、napi_schedule 内联函数 |
| `net/core/dev.c` (13317 行) | NAPI 调度核心：net_rx_action, ____napi_schedule, napi_complete_done |
| `net/core/gro.c` (842 行) | GRO 完整实现：dev_gro_receive, gro_complete, skb_gro_receive |
| `include/net/busy_poll.h` (188 行) | 忙轮询接口：napi_busy_loop, sk_busy_loop, BUSY_POLL_BUDGET |
| `drivers/net/ethernet/intel/igb/igb_main.c` (10314 行) | igb 驱动 NAPI 集成：igb_msix_ring, igb_poll |

---

## 前置知识

阅读本系列前，建议掌握：

1. **Linux 网络栈基础**：sk_buff 结构、网络设备模型、数据包收发流程
2. **中断子系统**：硬中断 (hardirq)、软中断 (softirq)、中断亲和性
3. **内核并发**：per-CPU 变量、local_bh_disable、原子操作
4. **网卡工作原理**：Ring Buffer、DMA、中断聚合 (ITR)
5. **TCP/IP 基础**：TCP 分段卸载 (TSO)、校验和卸载

---

## 三种数据路径

```
路径 1: 传统非 NAPI (process_backlog)
  IRQ → 屏蔽中断 → netif_rx() → 入队 input_pkt_queue
  → NET_RX_SOFTIRQ → process_backlog() → __netif_receive_skb()

路径 2: NAPI (标准方式，所有现代驱动)
  IRQ → 屏蔽中断 → napi_schedule() → NET_RX_SOFTIRQ
  → net_rx_action() → napi->poll() → napi_complete_done() → 开中断

路径 3: 忙轮询 (Busy Poll) + 线程化 NAPI
  用户态 epoll/busy_read → napi_busy_loop()
  → 绕过 softirq，直接在进程上下文中 poll
  → 或：NAPI kthread (THREADED) → 独立上下文 polling
```
