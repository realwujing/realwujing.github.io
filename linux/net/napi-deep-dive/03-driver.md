# 第3篇：驱动集成 — igb 与 NAPI 全程

> 源码：drivers/net/ethernet/intel/igb/igb_main.c net/core/dev.c | 头文件：include/linux/netdevice.h

系列目录：[NAPI 内核源码深度解析](./README.md)

---

## 1. 概述

前两篇分别分析了 NAPI 核心调度和 GRO 合并引擎。本文以 Intel igb 驱动（e1000 系列千兆网卡）为实例，完整走读驱动如何将硬件收包、中断处理、NAPI 调度、GRO 合并集成到一起。

igb 是 Linux 中最成熟的网卡驱动之一，同时支持 MSI-X（多队列）、INTx（传统中断）、ITR（中断调节）、XDP、AF_XDP 等高级特性。选择 igb 作为示例，因为它包含了一个 NAPI 驱动应有的所有典型模式。

---

## 2. igb 驱动架构概览

### 2.1 核心数据结构关系

```
struct igb_adapter                   顶层适配器
├── struct net_device *netdev
├── struct igb_q_vector *q_vector[N]  每个中断向量一个
├── int num_q_vectors
├── int num_rx_queues / num_tx_queues
├── struct e1000_hw hw
└── ...

struct igb_q_vector                   中断向量
├── struct napi_struct napi           内嵌 NAPI 上下文
├── struct igb_ring rx / tx           收发 Ring
├── struct igb_adapter *adapter
├── int cpu
└── ...

struct igb_ring                       收发 Ring
├── void *desc                        DMA 描述符区域
├── struct igb_tx_buffer / rx_buffer *buffer_info
├── u16 next_to_use / next_to_clean
└── ...
```

每个 `igb_q_vector` 对应一个中断向量和一个 NAPI 上下文。在多队列模式下，每个 MSI-X 向量处理一个收发队列对。

### 2.2 中断模式支持

| 模式 | 中断函数 | 行号 | NAPI 调度方式 |
|------|----------|------|--------------|
| **MSI-X (多队列)** | `igb_msix_ring` | igb_main.c:7153 | 每个队列独立 `napi_schedule` |
| **MSI (单向量)** | `igb_intr_msi` | igb_main.c:8174 | 单次 `napi_schedule` |
| **INTx (传统线中断)** | `igb_intr` | igb_main.c:8211 | 单次 `napi_schedule` |

---

## 3. NAPI 注册

### 3.1 netif_napi_add_weight_locked — NAPI 初始化 (dev.c:7558)

驱动初始化时（通常在 `probe` 中），通过此函数注册 NAPI：

```c
// net/core/dev.c:7558
void netif_napi_add_weight_locked(struct net_device *dev,
                                  struct napi_struct *napi,
                                  int (*poll)(struct napi_struct *, int),
                                  int weight)
{
    netdev_assert_locked(dev);
    if (WARN_ON(test_and_set_bit(NAPI_STATE_LISTED, &napi->state)))
        return;

    INIT_LIST_HEAD(&napi->poll_list);
    INIT_HLIST_NODE(&napi->napi_hash_node);
    hrtimer_setup(&napi->timer, napi_watchdog,
                  CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
    gro_init(&napi->gro);     // 初始化 GRO 节点（哈希表和 bitmask）
    napi->skb = NULL;
    napi->poll = poll;        // 保存驱动 poll 函数指针
    if (weight > NAPI_POLL_WEIGHT)
        netdev_err_once(dev, "%s() called with weight %d\n",
                        __func__, weight);
    napi->weight = weight;    // igb 使用 64
    napi->dev = dev;
    napi->list_owner = -1;
    set_bit(NAPI_STATE_SCHED, &napi->state);   // 注册期间的临时状态
    set_bit(NAPI_STATE_NPSVC, &napi->state);   // 防止 netpoll 干扰
    netif_napi_dev_list_add(dev, napi);
    // 添加到 NAPI hash 表（用于 busy poll 查找）
    // ...
}
```

igb 的调用（在 `igb_alloc_q_vector` 中，igb_main.c:1200）：

```c
netif_napi_add_config(adapter->netdev, &q_vector->napi,
                      igb_poll, 64);
```

**igb 的 weight=64**，这是驱动自己决定的——表示每次 poll 最多处理 64 个包（实际包含 TX clean + RX 收包）。

---

## 4. 中断处理路径

### 4.1 MSI-X 路径：igb_msix_ring (igb_main.c:7153)

```c
// drivers/net/ethernet/intel/igb/igb_main.c:7153
static irqreturn_t igb_msix_ring(int irq, void *data)
{
    struct igb_q_vector *q_vector = data;

    // 写入 ITR (Interrupt Throttle Rate)，为下次中断设置时间间隔
    igb_write_itr(q_vector);

    // 关键调用：调度 NAPI
    napi_schedule(&q_vector->napi);

    return IRQ_HANDLED;
}
```

这是最简洁的中断处理——只有两步：
1. **ITR 更新**：根据当前负载动态调整中断频率
2. **napi_schedule**：触发软中断处理

**为什么不需要显式 mask 中断？** igb 硬件支持 Auto-Mask 特性：读取 ICR（中断原因寄存器）时硬件自动屏蔽中断。对于 MSI-X，由于每个队列有独立的中断向量，设计上只需要依赖此硬件特性。

### 4.2 INTx 路径：igb_intr (igb_main.c:8211)

```c
// drivers/net/ethernet/intel/igb/igb_main.c:8211
static irqreturn_t igb_intr(int irq, void *data)
{
    struct igb_adapter *adapter = data;
    struct igb_q_vector *q_vector = adapter->q_vector[0];
    struct e1000_hw *hw = &adapter->hw;
    u32 icr = rd32(E1000_ICR);      // 读取 ICR（硬件自动 mask）

    if (!(icr & E1000_ICR_INT_ASSERTED))
        return IRQ_NONE;            // 不是我们的中断

    igb_write_itr(q_vector);        // 更新 ITR

    // 处理各种 interrupt cause
    if (icr & E1000_ICR_DRSTA)
        schedule_work(&adapter->reset_task);

    if (icr & E1000_ICR_DOUTSYNC)
        adapter->stats.doosync++;

    if (icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
        hw->mac.get_link_status = 1;
        if (!test_bit(__IGB_DOWN, &adapter->state))
            mod_timer(&adapter->watchdog_timer, jiffies + 1);
    }

    if (icr & E1000_ICR_TS)
        igb_tsync_interrupt(adapter);

    // 调度 NAPI
    napi_schedule(&q_vector->napi);

    return IRQ_HANDLED;
}
```

INTx 模式下只有 1 个中断向量，因此需要读取 ICR 来判断中断原因：
- `E1000_ICR_INT_ASSERTED`：确认中断来自本设备（共享中断线判断）
- `E1000_ICR_RXSEQ / LSC`：接收序列错误或链路状态变化
- `E1000_ICR_TS`：时间戳中断（PTP）

### 4.3 中断屏蔽与恢复模式

```
┌──────────────────────────────────────────────────────────────┐
│                  igb 中断屏蔽/恢复时序                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  IRQ 到达 (MSI-X)                                             │
│    │                                                         │
│    │ [硬件] 读取 ICR → Auto-Mask (中断被屏蔽)                  │
│    │                                                         │
│    ▼                                                         │
│  igb_msix_ring()                         [igb_main.c:7153]   │
│    ├── igb_write_itr()                  更新中断调节           │
│    └── napi_schedule()                  调度 NAPI             │
│          │                                                    │
│          ▼                                                    │
│  NET_RX_SOFTIRQ → net_rx_action()                             │
│    └── igb_poll()                         [igb_main.c:8278]   │
│          ├── igb_clean_tx_irq()         清理 TX 完成           │
│          ├── igb_clean_rx_irq()         收包 + GRO            │
│          └── napi_complete_done()                             │
│                └── igb_ring_irq_enable() [igb_main.c:8252]    │
│                      重新启用该队列的中断                       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 4.4 igb_ring_irq_enable — 重新启用中断 (igb_main.c:8252)

```c
// drivers/net/ethernet/intel/igb/igb_main.c:8252
static void igb_ring_irq_enable(struct igb_q_vector *q_vector)
{
    struct igb_adapter *adapter = q_vector->adapter;
    struct e1000_hw *hw = &adapter->hw;

    if ((q_vector->rx.ring && (adapter->rx_itr_setting & 3)) ||
        (!q_vector->rx.ring && (adapter->tx_itr_setting & 3))) {
        if ((adapter->num_q_vectors == 1) && !adapter->vf_data)
            igb_set_itr(q_vector);
        else
            igb_update_ring_itr(q_vector);
    }

    if (!test_bit(__IGB_DOWN, &adapter->state)) {
        if (adapter->flags & IGB_FLAG_HAS_MSIX) {
            // MSI-X: 写入 EIMS (Extended Interrupt Mask Set/ Clear)
            wr32(E1000_EIMS, q_vector->eims_value);
        }
        // ...
    }
}
```

关键点：只有在 `napi_complete_done()` 返回 true 后才会调用此函数（见 igb_poll 返回值逻辑）。

---

## 5. Poll 函数：igb_poll

### 5.1 igb_poll — NAPI 轮询入口 (igb_main.c:8278)

```c
// drivers/net/ethernet/intel/igb/igb_main.c:8278
static int igb_poll(struct napi_struct *napi, int budget)
{
    struct igb_q_vector *q_vector =
        container_of(napi, struct igb_q_vector, napi);
    struct xsk_buff_pool *xsk_pool;
    bool clean_complete = true;
    int work_done = 0;

#ifdef CONFIG_IGB_DCA
    if (q_vector->adapter->flags & IGB_FLAG_DCA_ENABLED)
        igb_update_dca(q_vector);         // Direct Cache Access 更新
#endif

    // Step 1: TX 清理（回收已发送的包）
    if (q_vector->tx.ring)
        clean_complete = igb_clean_tx_irq(q_vector, budget);

    // Step 2: RX 收包
    if (q_vector->rx.ring) {
        int cleaned;

        xsk_pool = READ_ONCE(q_vector->rx.ring->xsk_pool);
        cleaned = xsk_pool ?
            igb_clean_rx_irq_zc(q_vector, xsk_pool, budget) :
            igb_clean_rx_irq(q_vector, budget);

        work_done += cleaned;
        if (cleaned >= budget)
            clean_complete = false;
    }

    // Step 3: 决定是否完成
    if (!clean_complete)
        return budget;                    // 还有工作 → 继续 poll

    // Step 4: napi_complete → 重新启用中断
    if (likely(napi_complete_done(napi, work_done)))
        igb_ring_irq_enable(q_vector);

    return work_done;
}
```

三个退出路径：
1. **`work_done` (正常完成)**：TX 和 RX 都处理完毕，调用 `napi_complete_done` → 如果返回 true → 重新启用中断
2. **`budget` (有剩余工作)**：TX 或 RX 处理到 budget 上限，返回 budget 告诉 `net_rx_action` 不要清除 SCHED → 下一轮继续
3. **`napi_complete_done` 返回 false**：即使 work < budget，但由于 MISSED 标志（意味着 poll 期间有新包到达），也不启用中断 → 重新调度

注意：TX clean 总是完成（不消耗 budget 计数），budget 只用于 RX 收包。work_done 只累计 RX 处理的包数。

### 5.2 igb_clean_rx_irq — 核心收包循环

虽然本文不展开此函数的完整代码，但其核心流程是：

```
循环直到 budget:
    1. 从 RX ring 读取下一个描述符
    2. 检查 DD (Descriptor Done) 位
    3. 如果未完成 → 跳出循环
    4. dma_sync_single_for_cpu (DMA 工程)
    5. 构建 sk_buff
    6. 设置协议类型 (eth_type_trans)
    7. napi_gro_receive(&q_vector->napi, skb)  ← GRO 入口
    8. 返回新的空 buffer 给硬件
    9. work_done++
```

---

## 6. 完整数据流：从网线到协议栈

### 6.1 硬件到驱动的路径

```
  ┌───────────────┐
  │  物理网络       │
  │  数据包到达     │
  └───────┬───────┘
          │
          ▼
  ┌───────────────────────────────────────────────┐
  │  igb 网卡硬件                                  │
  │  - DMA 帧到 RX Ring Buffer                     │
  │  - 更新描述符 (设置 DD 位)                      │
  │  - 触发 MSI-X 中断 (如果 ITR 允许)              │
  └───────────────────┬───────────────────────────┘
                      │
                      ▼
  ┌───────────────────────────────────────────────┐
  │  igb_msix_ring()               [igb_main.c:7153]│
  │  - igb_write_itr()            更新中断节流       │
  │  - napi_schedule(&q_vector->napi)               │
  └───────────────────┬───────────────────────────┘
                      │
                      ▼
  ┌───────────────────────────────────────────────┐
  │  ____napi_schedule()            [dev.c:4957]    │
  │  - list_add_tail → sd->poll_list               │
  │  - raise_softirq_irqoff(NET_RX_SOFTIRQ)        │
  └───────────────────┬───────────────────────────┘
                      │
                      ▼
  ┌───────────────────────────────────────────────┐
  │  net_rx_action()                [dev.c:7914]    │
  │  - splice poll_list                             │
  │  - 遍历 NAPI                                    │
  └───────────────────┬───────────────────────────┘
                      │
                      ▼
  ┌───────────────────────────────────────────────┐
  │  igb_poll()                     [igb_main.c:8278]│
  │                                                │
  │  ┌─ igb_clean_tx_irq()         清理 TX 完成     │
  │  │                                             │
  │  └─ igb_clean_rx_irq()         收包循环          │
  │       │                                         │
  │       ├── 读取描述符 (检查 DD 位)                 │
  │       ├── dma_unmap / sync                      │
  │       ├── 构建 sk_buff                           │
  │       ├── eth_type_trans()                      │
  │       └── napi_gro_receive()   → GRO 合并        │
  │             │                                   │
  │             ├── GRO_MERGED     合并到已有包        │
  │             ├── GRO_HELD       保留在 GRO 中      │
  │             └── GRO_NORMAL     直接送入协议栈      │
  │                                                │
  │  ┌─ napi_complete_done(napi, work_done)         │
  │  │    └── gro_flush_normal()  刷新 GRO 中剩余包   │
  │  └─ igb_ring_irq_enable()    重新启用中断         │
  └───────────────────────────────────────────────┘
```

### 6.2 完整 ASCII 流程图

```
  ┌──────────────────────────────────────────────────────────────────────────┐
  │                    igb 驱动: NAPI + GRO 全链路                              │
  ├──────────────────────────────────────────────────────────────────────────┤
  │                                                                          │
  │  ┌─────────────────────┐                                                 │
  │  │ 硬件: 数据包到达      │                                                 │
  │  │ DMA → RX Ring       │                                                 │
  │  │ 触发 MSI-X 中断      │                                                 │
  │  └──────────┬──────────┘                                                 │
  │             │                                                             │
  │             ▼                                                             │
  │  ┌──────────────────────────────────────┐                                 │
  │  │ igb_msix_ring()   [igb_main.c:7153] │                                 │
  │  │                                      │                                 │
  │  │   igb_write_itr(q_vector)            │  动态中断调节                     │
  │  │   napi_schedule(&q_vector->napi)     │  调度 NAPI                       │
  │  │                                      │                                 │
  │  │   [硬件自动 mask 中断]                 │  ICR 读取 → Auto-Mask           │
  │  └──────────────┬───────────────────────┘                                 │
  │                 │                                                         │
  │                 ▼                                                         │
  │  ┌──────────────────────────────────────┐                                 │
  │  │ ____napi_schedule() [dev.c:4957]     │                                 │
  │  │                                      │                                 │
  │  │   list_add_tail(&napi->poll_list,    │  加入 per-CPU poll_list          │
  │  │                 &sd->poll_list)      │                                 │
  │  │   raise_softirq_irqoff               │  触发 NET_RX_SOFTIRQ             │
  │  │           (NET_RX_SOFTIRQ)           │                                 │
  │  └──────────────┬───────────────────────┘                                 │
  │                 │                                                         │
  │                 ▼                                                         │
  │  ┌──────────────────────────────────────┐                                 │
  │  │ net_rx_action()   [dev.c:7914]       │                                 │
  │  │                                      │                                 │
  │  │   list_splice_init → 取出所有 NAPI    │                                 │
  │  │   budget = 300 (全局限制)             │                                 │
  │  │                                      │                                 │
  │  │   for each napi:                     │                                 │
  │  │     budget -= napi_poll(n, &repoll)  │                                 │
  │  └──────────────┬───────────────────────┘                                 │
  │                 │                                                         │
  │                 ▼                                                         │
  │  ┌──────────────────────────────────────┐                                 │
  │  │ igb_poll()  [igb_main.c:8278]       │                                 │
  │  │                                      │                                 │
  │  │   ┌──────────────────────────────┐   │                                 │
  │  │   │ igb_clean_tx_irq()           │   │  回收已发送的 buffer              │
  │  │   │  (不消耗 budget)              │   │                                 │
  │  │   └──────────────────────────────┘   │                                 │
  │  │                                      │                                 │
  │  │   ┌──────────────────────────────┐   │                                 │
  │  │   │ igb_clean_rx_irq()           │   │                                 │
  │  │   │                              │   │                                 │
  │  │   │  while (work < budget):      │   │  budget = 64 (NAPI weight)      │
  │  │   │    rx_desc = ring[next]      │   │                                 │
  │  │   │    if !(DD) break             │   │  没有新包 → 退出                │
  │  │   │    skb = build_skb(rx_desc)  │   │  构建 sk_buff                  │
  │  │   │    napi_gro_receive(napi,skb)│   │  → GRO 合并                     │
  │  │   │    work_done++               │   │                                 │
  │  │   └──────────────────────────────┘   │                                 │
  │  │                                      │                                 │
  │  │   if (!clean_complete)               │                                 │
  │  │     return budget;  // 继续 poll     │                                 │
  │  │                                      │                                 │
  │  │   if (napi_complete_done(napi, work))│                                 │
  │  │     igb_ring_irq_enable(q_vector);   │  重新启用中断                    │
  │  │                                      │                                 │
  │  │   return work_done;                  │                                 │
  │  └──────────────────────────────────────┘                                 │
  │                                                                          │
  └──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. ITR：中断调节

### 7.1 igb_write_itr (igb_main.c:7144)

```c
// drivers/net/ethernet/intel/igb/igb_main.c:7144
static void igb_write_itr(struct igb_q_vector *q_vector)
{
    u32 itr_val = q_vector->itr_val & ...;
    // ...
    writel(itr_val, ...);  // 写入 EITR 寄存器
}
```

ITR (Interrupt Throttle Rate) 控制中断的触发频率：
- **低 ITR**：中断频率高 → 延迟低 → CPU 占用高
- **高 ITR**：中断频率低 → 延迟高 → CPU 占用低（批量处理更多包）

igb 支持多种 ITR 模式：
- **动态模式**：根据 RX/TX 包速率自适应调整
- **固定模式**：用户通过 ethtool 设置固定值
- **"自适应"模式**：根据包大小、中断间隔动态微调

ITR 的工作原理是**在每次中断处理时设定下一次中断的最小间隔**，类似于 hrtimer 的 tick 合并。

---

## 8. 驱动开发要点

### 8.1 NAPI 驱动的标准模板

```c
// === 初始化阶段 (probe) ===
static int driver_probe(struct pci_dev *pdev, ...)
{
    // 1. 分配和初始化适配器
    struct driver_adapter *adapter = ...;

    // 2. 为每个队列初始化 NAPI
    for (i = 0; i < num_queues; i++) {
        netif_napi_add(dev, &adapter->q_vector[i].napi,
                       driver_poll, 64);
        // 或使用新的 netif_napi_add_config()
    }

    // 3. 注册中断处理函数
    request_irq(irq, driver_msix_ring, 0, name, q_vector);
}

// === 中断处理 (硬中断上下文) ===
static irqreturn_t driver_msix_ring(int irq, void *data)
{
    struct driver_q_vector *q_vector = data;

    // 屏蔽中断 (硬件 Auto-Mask 或显式 mask)
    napi_schedule(&q_vector->napi);
    return IRQ_HANDLED;
}

// === NAPI 轮询 (软中断上下文) ===
static int driver_poll(struct napi_struct *napi, int budget)
{
    struct driver_q_vector *q_vector =
        container_of(napi, struct driver_q_vector, napi);
    int work_done = 0;

    // TX clean (可选)
    driver_clean_tx(q_vector);

    // RX clean
    while (work_done < budget) {
        skb = driver_get_rx_packet(q_vector);
        if (!skb)
            break;
        napi_gro_receive(napi, skb);
        work_done++;
    }

    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        driver_enable_irq(q_vector);  // 重新启用中断
    }

    return work_done;
}
```

### 8.2 常见陷阱

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| **hung task / soft lockup** | poll 函数无限循环 | 确保在无包时 break，budget 检查 |
| **中断风暴** | Budget 总是用完 | 使用 ITR / 增大 weight |
| **包丢失** | Ring buffer 溢出 | 增大 ring size / 调整中断亲和性 |
| **NAPI 双重调度** | 竞态条件 | 正确使用 napi_schedule_prep 返回值 |
| **GRO 不工作** | skb 没有设置正确的 hash | 使用 ethtool 配置 RSS hash |
| **XDP 与 NAPI 冲突** | XDP 在 NAPI 上下文中运行 | 理解执行顺序：XDP → GRO → 协议栈 |

### 8.3 性能调优参数 (通过 ethtool)

| 参数 | ethtool 选项 | 默认 | 影响 |
|------|-------------|------|------|
| RX ring size | `-G rx N` | 256 | 更大的 ring 容忍突发流量 |
| TX ring size | `-G tx N` | 256 | 同上 |
| ITR | `-C rx-usecs N` | 动态 | 调节中断频率，平衡延迟/吞吐 |
| 中断聚合 | `-C rx-frames N` | 0 | 收到 N 帧后才中断 |
| 队列数 | `-L combined N` | 系统决定 | 更多队列 → 更好的并行性 |

---

## 9. 总结

以 igb 为例的 NAPI 驱动集成展示了标准模式：

1. **注册阶段**：`netif_napi_add_weight_locked()` 将 poll 函数与 NAPI 上下文绑定
2. **中断处理**：最简形式只有 `napi_schedule()` — 依赖硬件 Auto-Mask 和软中断处理
3. **Poll 函数**：TX clean + RX 收包 + GRO → `napi_complete_done()` → 重新启用中断
4. **budget 控制**：`work < budget` 正常完成；`work == budget` 继续轮询；`MISSED` 标志处理竞态
5. **ITR 调节**：在中断入口动态调整中断频率，平衡延迟和吞吐

下一篇将分析 NAPI 的高级模式：Busy Poll（忙轮询）和线程化 NAPI。

---

下一篇: [第4篇：忙轮询与线程化 NAPI](./04-busy-poll.md)
