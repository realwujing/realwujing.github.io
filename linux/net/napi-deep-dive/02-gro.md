# 第2篇：GRO — 通用接收卸载

> 源码：net/core/gro.c include/net/gro.h | 头文件：include/linux/netdevice.h

系列目录：[NAPI 内核源码深度解析](./README.md)

---

## 1. 概述

上一篇分析了 NAPI 的核心调度机制，驱动在 poll 过程中通过 `napi_gro_receive()` 将 skb 送入 GRO。GRO (Generic Receive Offload) 是 Linux 内核的软件包合并引擎——它检查同一流的连续数据包，将它们合并为一个大的"超级包"，再送入协议栈。

**为什么要合并？**
- 减少协议栈处理次数：TCP/IP 头部解析、Netfilter、路由查找只执行一次
- 提高 GRO→GSO (Generic Segmentation Offload) 的配对效率：合并后的大包可直接 DMA 转发
- 减少 sk_buff 分配/释放开销

本文深入分析 GRO 的哈希查找、流匹配、数据合并和 flush 到协议栈的完整流程。

---

## 2. GRO 的数据结构

### 2.1 struct gro_node — GRO 节点 (netdevice.h 内嵌于 napi_struct)

```c
// 在 napi_struct 中嵌入 (netdevice.h:403)
struct gro_node     gro;

// struct gro_node 包含:
// - struct gro_list hash[GRO_HASH_BUCKETS]  (通常 8 个桶)
// - unsigned long bitmask                    标记哪些桶非空
// - u32 cached_napi_id                      缓存的 NAPI ID (用于 busy poll)
```

每个 napi_struct 实例拥有一个 GRO 节点，构成一个完全独立的合并上下文。这意味着：
- 不同 NAPI 上下文之间的包不会交叉合并
- 同一个流的包必须到达同一个 NAPI（由硬件 RPS/RFS/队列分配保证）
- GRO 内部不需要锁（单线程在 NAPI poll 上下文中运行）

### 2.2 struct gro_list — 每个哈希桶的链表

```c
struct gro_list {
    struct list_head list;
    int count;             // 此桶中的包数量（最多 MAX_GRO_SKBS = 8）
};
```

### 2.3 struct napi_gro_cb — 控制块 (嵌入 skb->cb)

```c
// gro 把元数据编码在 skb->cb[] 中
struct napi_gro_cb {
    // ...
    int same_flow;         // 与链表中的包属于同一流？
    int flush;             // 强制 flush（不分流合并）
    int free;              // 释放模式（GRO_MERGED_FREE 相关）
    int count;             // 合并的包数 (用于 GSO)
    int age;               // 包的 "年龄" (jiffies)，用于旧包 flush
    struct sk_buff *last;  // 流中最后一个 skb
    // ...(更多字段用于检查和协议特定数据)
};
```

### 2.4 struct packet_offload — 协议处理注册

```c
struct packet_offload {
    __be16 type;           // 协议类型 (ETH_P_IP, ETH_P_IPV6...)
    u16 priority;
    struct offload_callbacks callbacks;  // gro_receive 和 gro_complete
    struct list_head list;
};
```

通过 `dev_add_offload()` (gro.c:25) 注册协议处理器。inet 初始化时注册 TCP over IPv4/IPv6 的 GRO callbacks。

---

## 3. 注册协议处理器：dev_add_offload

### 3.1 dev_add_offload — 注册 GRO 协议回调 (gro.c:25)

```c
// net/core/gro.c:25
void dev_add_offload(struct packet_offload *po)
{
    struct packet_offload *elem;

    spin_lock(&offload_lock);
    list_for_each_entry(elem, &net_hotdata.offload_base, list) {
        if (po->priority < elem->priority)
            break;
    }
    list_add_rcu(&po->list, elem->list.prev);
    spin_unlock(&offload_lock);
}
```

在 TCP/IPv4 初始化时调用：

```c
static struct packet_offload ipv4_packet_offload = {
    .type = cpu_to_be16(ETH_P_IP),
    .priority = 0,
    .callbacks = {
        .gro_receive = inet_gro_receive,
        .gro_complete = inet_gro_complete,
    },
};
```

`offload_base` 链表按 priority 排序，GRO 处理时遍历此链表找到匹配协议的回调。

---

## 4. 数据合并路径

### 4.1 入口：gro_receive_skb (gro.c:631)

由驱动的 NAPI poll 函数调用：

```c
// net/core/gro.c:631
gro_result_t gro_receive_skb(struct gro_node *gro, struct sk_buff *skb)
{
    gro_result_t ret;

    __skb_mark_napi_id(skb, gro);
    trace_napi_gro_receive_entry(skb);

    skb_gro_reset_offset(skb, 0);

    ret = gro_skb_finish(gro, skb, dev_gro_receive(gro, skb));
    trace_napi_gro_receive_exit(ret);

    return ret;
}
```

### 4.2 dev_gro_receive — 核心合并逻辑 (gro.c:469)

```c
// net/core/gro.c:469
static enum gro_result dev_gro_receive(struct gro_node *gro,
                                       struct sk_buff *skb)
{
    u32 bucket = skb_get_hash_raw(skb) & (GRO_HASH_BUCKETS - 1);
    struct list_head *head = &net_hotdata.offload_base;
    struct gro_list *gro_list = &gro->hash[bucket];
    struct packet_offload *ptype;
    __be16 type = skb->protocol;
    struct sk_buff *pp = NULL;
    int same_flow;

    if (netif_elide_gro(skb->dev))
        goto normal;

    // Step 1: 预处理——标记链表中的 same_flow 关系
    gro_list_prepare(&gro_list->list, skb);

    // Step 2: 查找匹配的协议处理器
    rcu_read_lock();
    list_for_each_entry_rcu(ptype, head, list) {
        if (ptype->type == type && ptype->callbacks.gro_receive)
            goto found_ptype;
    }
    rcu_read_unlock();
    goto normal;

found_ptype:
    // Step 3: 设置 GRO 控制块
    skb_set_network_header(skb, skb_gro_offset(skb));
    NAPI_GRO_CB(skb)->count = 1;
    // ... checksum 验证 ...

    // Step 4: 调用协议特定的 gro_receive 回调
    pp = INDIRECT_CALL_INET(ptype->callbacks.gro_receive,
                            ipv6_gro_receive, inet_gro_receive,
                            &gro_list->list, skb);
    rcu_read_unlock();

    // Step 5: 处理协议回调的结果
    same_flow = NAPI_GRO_CB(skb)->same_flow;
    ret = NAPI_GRO_CB(skb)->free ? GRO_MERGED_FREE : GRO_MERGED;

    if (pp) {
        skb_list_del_init(pp);
        gro_complete(gro, pp);            // 老包被标记完成，送入协议栈
        gro_list->count--;
    }

    if (same_flow)
        goto ok;

    if (NAPI_GRO_CB(skb)->flush)
        goto normal;

    // Step 6: 添加到哈希桶
    if (unlikely(gro_list->count >= MAX_GRO_SKBS))
        gro_flush_oldest(gro, &gro_list->list);
    else
        gro_list->count++;

    gro_try_pull_from_frag0(skb);
    NAPI_GRO_CB(skb)->age = jiffies;
    NAPI_GRO_CB(skb)->last = skb;
    list_add(&skb->list, &gro_list->list);
    ret = GRO_HELD;

ok:
    // 更新 bitmask
    if (gro_list->count) {
        if (!test_bit(bucket, &gro->bitmask))
            __set_bit(bucket, &gro->bitmask);
    } else if (test_bit(bucket, &gro->bitmask)) {
        __clear_bit(bucket, &gro->bitmask);
    }

    return ret;

normal:
    ret = GRO_NORMAL;
    gro_try_pull_from_frag0(skb);
    goto ok;
}
```

### 4.3 流匹配：gro_list_prepare (gro.c:350)

在调用协议层合并之前，先标记同一流的关系：

```c
// net/core/gro.c:350
static void gro_list_prepare(const struct list_head *head,
                             const struct sk_buff *skb)
{
    unsigned int maclen = skb->dev->hard_header_len;
    u32 hash = skb_get_hash_raw(skb);
    struct sk_buff *p;

    list_for_each_entry(p, head, list) {
        unsigned long diffs;

        if (hash != skb_get_hash_raw(p)) {
            NAPI_GRO_CB(p)->same_flow = 0;
            continue;
        }

        // 比较设备、VLAN、元数据
        diffs = (unsigned long)p->dev ^ (unsigned long)skb->dev;
        diffs |= p->vlan_all ^ skb->vlan_all;
        diffs |= skb_metadata_differs(p, skb);

        // 比较 MAC 头
        if (maclen == ETH_HLEN)
            diffs |= compare_ether_header(skb_mac_header(p),
                                          skb_mac_header(skb));
        else if (!diffs)
            diffs = memcmp(skb_mac_header(p),
                           skb_mac_header(skb), maclen);

        // 慢路径检查（非 TCP 才进入）
        if (!diffs && unlikely(skb->slow_gro | p->slow_gro)) {
            diffs |= p->sk != skb->sk;
            diffs |= skb_metadata_dst_cmp(p, skb);
            diffs |= skb_get_nfct(p) ^ skb_get_nfct(skb);
            // ...
        }

        NAPI_GRO_CB(p)->same_flow = !diffs;
    }
}
```

same_flow 的判断基于：
1. **哈希值相同**（相同流 → 相同哈希桶 → 相同哈希值）
2. **设备相同**（同一网络设备）
3. **VLAN 相同**
4. **MAC 地址相同**
5. **慢路径**（非 TCP）：还要检查 socket、目的地、conntrack 等

### 4.4 skb_gro_receive — 物理合并 (gro.c:92)

协议层回调调用此函数将新包的数据合并到已有的 skb：

```c
// net/core/gro.c:92
int skb_gro_receive(struct sk_buff *p, struct sk_buff *skb)
{
    struct skb_shared_info *pinfo, *skbinfo = skb_shinfo(skb);
    unsigned int offset = skb_gro_offset(skb);
    unsigned int headlen = skb_headlen(skb);
    unsigned int len = skb_gro_len(skb);
    // ...

    // Case 1: 数据完全合并到已有的 page frag
    if (headlen <= offset) {
        // copy to frag
    }

    // Case 2: 当前 skb 的 headroom 足够，直接合并
    if (skb_headroom(p) >= headlen) {
        // copy headers
    }

    // Case 3: 使用 frag_list（零拷贝合并）
    // 将 skb 添加到 p 的 frag_list 中
    // ...
}
```

核心思想：将多个小 skb 的数据合并到一个 **"head skb"** 中（通过 page frags 或 frag_list），协议栈后续只处理 head skb 一次。

### 4.5 GRO 返回值 (netdevice.h:451)

```c
enum gro_result {
    GRO_MERGED,        // 合并成功，head skb 仍在 GRO 中
    GRO_MERGED_FREE,   // 合并成功，新 skb 的数据已移入 head skb，新 skb 被释放
    GRO_HELD,          // 新 skb 被添加到 GRO 链表（成为新的 head skb 候选）
    GRO_NORMAL,        // 无法合并，直接送给协议栈（__netif_receive_skb）
    GRO_CONSUMED,      // 已被完全消费（由协议回调内部处理）
};
```

---

## 5. GRO 完成路径

### 5.1 gro_complete — 标记完成并设 GSO 字段 (gro.c:259)

当一个包链被 flush 时，每个 skb 需要设置 GSO 字段：

```c
// net/core/gro.c:259
static void gro_complete(struct gro_node *gro, struct sk_buff *skb)
{
    struct list_head *head = &net_hotdata.offload_base;
    struct packet_offload *ptype;
    __be16 type = skb->protocol;
    int err = -ENOENT;

    if (NAPI_GRO_CB(skb)->count == 1) {
        skb_shinfo(skb)->gso_size = 0;
        goto out;
    }

    skb->encapsulation = 0;
    rcu_read_lock();
    list_for_each_entry_rcu(ptype, head, list) {
        if (ptype->type != type || !ptype->callbacks.gro_complete)
            continue;

        err = INDIRECT_CALL_INET(ptype->callbacks.gro_complete,
                                 ipv6_gro_complete, inet_gro_complete,
                                 skb, 0);
        break;
    }
    rcu_read_unlock();

    if (err) {
        WARN_ON(&ptype->list == head);
        kfree_skb(skb);
        return;
    }

out:
    gro_normal_one(gro, skb, NAPI_GRO_CB(skb)->count);
}
```

`gro_complete` 调用协议层回调（如 `inet_gro_complete`）设置合并后 skb 的 GSO 字段：
- `gso_size`：MSS（最大分段大小）
- `gso_type`：SKB_GSO_TCPV4 或 SKB_GSO_TCPV6
- `gso_segs`：合并的段数

这样协议栈就知道这是一个 "GSO 包"——如果需要发送或分段，GSO 可以高效地将其切回原始大小。

### 5.2 __gro_flush — 刷新所有未完成的 GRO 包 (gro.c:319)

```c
// net/core/gro.c:319
void __gro_flush(struct gro_node *gro, bool flush_old)
{
    unsigned long bitmask = gro->bitmask;
    unsigned int i, base = ~0U;

    while ((i = ffs(bitmask)) != 0) {
        bitmask >>= i;
        base += i;
        __gro_flush_chain(gro, base, flush_old);
    }
}
```

用 `ffs()` 高效遍历 bitmask，对每个非空哈希桶调用 `__gro_flush_chain()`。

### 5.3 __gro_flush_chain — 刷新单个哈希桶 (gro.c:297)

```c
// net/core/gro.c:297
static void __gro_flush_chain(struct gro_node *gro,
                              u32 index, bool flush_old)
{
    struct list_head *head = &gro->hash[index].list;
    struct sk_buff *skb, *p;

    // 反向遍历（最新的在先），减少延迟
    list_for_each_entry_safe_reverse(skb, p, head, list) {
        if (flush_old && NAPI_GRO_CB(skb)->age == jiffies)
            return;     // 旧包模式：遇到同 jiffy 的包就停止
        skb_list_del_init(skb);
        gro_complete(gro, skb);     // 设置 GSO 字段，送入协议栈
        gro->hash[index].count--;
    }

    if (!gro->hash[index].count)
        __clear_bit(index, &gro->bitmask);
}
```

反向遍历的原因：越新的包排在链表尾部，反向遍历按从旧到新的顺序处理，这对 TCP ACK 处理更友好。

---

## 6. GRO flush 时机

GRO 保存合并的包直到以下时机之一触发 flush：

| 触发条件 | 代码位置 | 说明 |
|----------|----------|------|
| NAPI poll 完成 | `napi_complete_done()` → `gro_flush_normal()` (dev.c:6803) | 本轮 poll 结束 |
| NAPI 预算耗尽 (work==budget) | `napi_complete_done()` 中 `work_done` 导致 | 仍有工作但预算用完 |
| GRO 超时 | `napi_struct.timer` (hrtimer) | 超时后强制刷新 |
| 非同一流的包到达 | `dev_gro_receive()` 中 `pp` 非空 | 新流包到达，旧流头包被弹出 |
| 哈希桶满 (≥MAX_GRO_SKBS) | `gro_flush_oldest()` (gro.c:450) | 桶中最多 8 个包 |
| defer_hard_irqs 计数 | `napi_complete_done()` 中的 `defer_hard_irqs_count` | 延迟中断恢复 |

### 6.1 gro_flush_oldest — 桶满时驱逐最老包 (gro.c:450)

```c
// net/core/gro.c:450
static void gro_flush_oldest(struct gro_node *gro, struct list_head *head)
{
    struct sk_buff *oldest;

    oldest = list_last_entry(head, struct sk_buff, list);

    if (WARN_ON_ONCE(!oldest))
        return;

    skb_list_del_init(oldest);
    gro_complete(gro, oldest);
}
```

`MAX_GRO_SKBS=8` 的限制控制了单流在 GRO 中最多缓存的包数。超过后，最老（最早到达）的包被弹出并完成→送入协议栈。

### 6.2 GRO 超时定时器

```c
// napi_struct 中的相关字段:
unsigned long gro_flush_timeout;   // 超时时间 (ns)
struct hrtimer  timer;             // GRO 刷新定时器
```

在 `napi_complete_done()` 中：
```c
if (timeout)
    hrtimer_start(&n->timer, ns_to_ktime(timeout),
                  HRTIMER_MODE_REL_PINNED);
```

定时器回调在超时后调用 `napi_schedule()` 重新调度 NAPI，驱动 poll 再次被调用时，GRO 中的旧包被刷新。

### 6.3 defer_hard_irqs 机制

`defer_hard_irqs_count` 允许驱动延迟重新启用中断：
- 如果还有其他处理器要处理，暂时不重新启用中断
- 每次 `napi_complete_done` 递减计数器
- 超时也会重新调度 NAPI（即使中断未启用）

---

## 7. GRO 使用场景与驱动集成

### 7.1 标准驱动调用模式

```c
// 驱动 poll 函数中的典型模式
static int driver_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;

    while (work_done < budget) {
        skb = driver_receive_packet();     // 从硬件获取 skb
        if (!skb)
            break;

        // 关键调用：将 skb 送入 GRO
        if (napi_gro_receive(napi, skb) != GRO_DROP)
            work_done++;
    }

    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        enable_irq();
    }
    return work_done;
}
```

`napi_gro_receive()` 内部调用 `gro_receive_skb(&napi->gro, skb)`。

### 7.2 GRO vs 非 GRO 路径比较

```
非 GRO 路径:                     GRO 路径:
                                
  skb1 → __netif_receive_skb    skb1 → GRO hold
  skb2 → __netif_receive_skb    skb2 → 合并到 skb1
  skb3 → __netif_receive_skb    skb3 → 合并到 skb1
  skb4 → __netif_receive_skb    skb4 → 合并到 skb1

  开销: 4× 协议栈处理              skb1 (合并后) → __netif_receive_skb
  4× TCP处理                     开销: 1× 协议栈处理
  4× Netfilter                    1× TCP处理 (GSO aware)
  4× 路由查找                     1× Netfilter
                                  1× 路由查找
```

---

## 8. GRO 与 TSO/GSO 的关系

```
   发送端                        接收端
   
   应用层                        应用层
     │                             ▲
     ▼                             │
   TCP 发送                      TCP 接收 (1个合并包)
   (大数据块)                       │
     │                             │
     ▼                             ▼
   TSO 分段                     GRO 合并
     │                             ▲
     ▼                             │
   多个 skb                      多个 skb
     │                             ▲
     ▼                             │
   网卡 DMA                      网卡 DMA
     │                             ▲
     ▼                             │
   物理网络 ←──── 数据包 ────→ 物理网络
```

GRO 是 TSO 的逻辑逆操作：
- **TSO (发送端)**：将一个大数据块切分为多个小包发送
- **GRO (接收端)**：将多个小包合并回一个大包
- **配对效果**：当包被转发时，GRO→GSO 路径可以保持大包形式，避免中间的分段/重组

---

## 9. 完整 ASCII 流程

```
  ┌────────────────────────────────────────────────────────────────┐
  │                      GRO 合并全流程                               │
  ├────────────────────────────────────────────────────────────────┤
  │                                                                │
  │  驱动 poll 函数                                                 │
  │  napi_gro_receive(napi, skb)                                  │
  │       │                                                        │
  │       ▼                                                        │
  │  gro_receive_skb(&napi->gro, skb)           [gro.c:631]       │
  │       │                                                        │
  │       ▼                                                        │
  │  dev_gro_receive(&napi->gro, skb)            [gro.c:469]      │
  │       │                                                        │
  │       ├── 1. hash = skb_get_hash_raw(skb) % 8                  │
  │       │      bucket = gro->hash[hash]                           │
  │       │                                                        │
  │       ├── 2. gro_list_prepare(bucket, skb)   [gro.c:350]      │
  │       │      标记链表中每个 skb 的 same_flow                     │
  │       │                                                        │
  │       ├── 3. 查找协议处理器 (offload_base)                       │
  │       │      → inet_gro_receive (TCP/IPv4)                     │
  │       │                                                        │
  │       ├── 4. 协议层 gro_receive 回调                             │
  │       │      → tcp_gro_receive()                               │
  │       │      → 检查 TCP 序号是否连续                             │
  │       │      → 是: skb_gro_receive(old, skb) [gro.c:92]       │
  │       │          返回 pp = NULL 或 pp = 老 head skb            │
  │       │                                                        │
  │       ├── 5. 如果 pp 非空:                                      │
  │       │      gro_complete(gro, pp)           [gro.c:259]      │
  │       │      → 设置 gso_size, gso_type 等字段                   │
  │       │      → gro_normal_one() → 送入协议栈                    │
  │       │                                                        │
  │       └── 6. 返回结果:                                          │
  │            GRO_MERGED     合并到已有包                           │
  │            GRO_MERGED_FREE 合并+free 新 skb                      │
  │            GRO_HELD        新流，保留在 GRO 中                    │
  │            GRO_NORMAL      无法合并，直接上送                     │
  │                                                                │
  │  ┌─────────────────────────────────────────────────────────┐   │
  │  │ 触发 flush:                                              │   │
  │  │                                                          │   │
  │  │ napi_complete_done() → gro_flush_normal()                │   │
  │  │    ↓                                                     │   │
  │  │ __gro_flush(&napi->gro, flush_old)    [gro.c:319]       │   │
  │  │    ↓                                                     │   │
  │  │ 遍历 bitmask 中的每个桶                                    │   │
  │  │    ↓                                                     │   │
  │  │ __gro_flush_chain()                   [gro.c:297]       │   │
  │  │    ↓                                                     │   │
  │  │ 反向遍历链表中的每个 skb                                    │   │
  │  │    ↓                                                     │   │
  │  │ gro_complete() → inet_gro_complete()  [gro.c:259]       │   │
  │  │    ↓                                                     │   │
  │  │ 设置 GSO 字段，调用 gro_normal_one()                       │   │
  │  │    ↓                                                     │   │
  │  │ __netif_receive_skb() → 进入协议栈                        │   │
  │  └─────────────────────────────────────────────────────────┘   │
  │                                                                │
  └────────────────────────────────────────────────────────────────┘
```

---

## 10. 总结

GRO 通过精巧的流合并减少协议栈开销，核心设计要点：

1. **per-NAPI GRO**：每个 NAPI 上下文独立合并，无需锁
2. **哈希桶 + same_flow**：快速判断流匹配，8 桶 × 8 包 = 最多 64 个包同时在 GRO 中
3. **协议回调模式**：TCP/IPv4、TCP/IPv6、UDP 等协议通过 `dev_add_offload()` 注册各自的合并/完成逻辑
4. **延迟刷新**：GRO 超时定时器在 NAPI 完成后继续保存未发送的包
5. **GSO 配对**：合并后的包设置 GSO 字段，可被 GSO 直接分段

下一篇将以 igb 驱动为例，展示 NAPI + GRO 的完整集成流程。

---

下一篇: [第3篇：驱动集成 — igb 与 NAPI 全程](./03-driver.md)
