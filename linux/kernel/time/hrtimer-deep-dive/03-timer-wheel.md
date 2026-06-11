# 第3篇：Timer Wheel：传统低精度定时器

> 源码：kernel/time/timer.c (2580行) | 头文件：include/linux/timer.h (200行), include/linux/timer_types.h (23行)

系列目录：[hrtimer 内核源码深度解析](./README.md)

---

## 1. 引言

前两篇我们深入分析了 hrtimer（高精度定时器）子系统。但在许多场景下（网络超时、磁盘 I/O 超时、协议栈保活），微秒/纳秒级精度并不需要，更关键的是**低开销**。Linux 内核为此保留了传统的 timer wheel（时间轮盘），运行在 jiffies (tick) 粒度上。

本篇聚焦 timer wheel 的核心设计：9 级免级联轮盘、`struct timer_list`、`struct timer_base`、add/mod/del 操作，并与 hrtimer 做全面对比。

---

## 2. 设计哲学

### 2.1 历史演进

- **1991**：Linus Torvalds 最初实现简单的超时链。
- **1997**：Finn Arne Gangstad 改进了可扩展性（`timer.c:7`）。
- **2002**：Ingo Molnar 实现了可伸缩的 SMP per-CPU 定时器。
- **2016**：Thomas Gleixner 重写了 timer wheel，去掉了传统的级联（cascading）机制。

### 2.2 核心思想

源码位置：`kernel/time/timer.c:64-97`

```
/*
 * The timer wheel has LVL_DEPTH array levels. Each level provides an array of
 * LVL_SIZE buckets. Each level is driven by its own clock and therefore each
 * level has a different granularity.
 *
 * The level granularity is:		LVL_CLK_DIV ^ level
 * The level clock frequency is:	HZ / (LVL_CLK_DIV ^ level)
 *
 * The array level of a newly armed timer depends on the relative expiry
 * time. The farther the expiry time is away the higher the array level and
 * therefore the granularity becomes.
 *
 * Contrary to the original timer wheel implementation, which aims for 'exact'
 * expiry of the timers, this implementation removes the need for recascading
 * the timers into the lower array levels. The previous 'classic' timer wheel
 * implementation of the kernel already violated the 'exact' expiry by adding
 * slack to the expiry time to provide batched expiration. The granularity
 * levels provide implicit batching.
 *
 * This is an optimization of the original timer wheel implementation for the
 * majority of the timer wheel use cases: timeouts.
 */
```

关键设计决策：

1. **无级联（No Cascading）**：旧设计中，定时器随着时间推进从高级别桶逐级向下"瀑布"迁移。新设计直接在入队时确定桶的级别，定时器在其生命周期内不移动（除非被 mod）。
2. **隐式批处理**：每个级别有固定的粒度，同一桶内的所有定时器被视为"同时"到期——误差在级别粒度以内的延迟是可接受的。
3. **空间换时间**：64 × 9 = 576 个桶，O(1) 插入与取出。

### 2.3 为什么不需要精确到期

源码注释明确指出（timer.c:83-87）：

```
 * The vast majority of timeout timers (networking, disk I/O ...) are canceled
 * before expiry. If the timeout expires it indicates that normal operation is
 * disturbed, so it does not matter much whether the timeout comes with a slight
 * delay.
```

绝大多数超时定时器**根本不会到期**——它们被取消（cancel）。如果到期了，说明系统已经不正常，几毫秒的延迟无所谓。

---

## 3. 数据结构

### 3.1 struct timer_list — 定时器本体

源码位置：`include/linux/timer_types.h:8-21`

```cpp
struct timer_list {
    struct hlist_node    entry;        // 哈希链表节点，链入桶中
    unsigned long        expires;      // 过期时间 (jiffies)
    void               (*function)(struct timer_list *);  // 回调函数
    u32                  flags;        // 标志位
#ifdef CONFIG_LOCKDEP
    struct lockdep_map   lockdep_map;  // 锁依赖跟踪
#endif
};
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| entry | hlist_node | 桶内的单链表节点。每个桶是一个 `hlist_head` 开头的单向链表 |
| expires | unsigned long | 以 jiffies 为单位的绝对过期时间。在 mod_timer 时传入，并可能根据桶粒度向上对齐 |
| function | 函数指针 | 定时器到期后执行的回调。新接口：`void (*)(struct timer_list *)`；旧接口使用 `data` 字段传递参数，现已废弃 |
| flags | u32 | 包含 CPU 编号、TIMER_DEFERRABLE、TIMER_PINNED、TIMER_IRQSAFE 等标志 |

**标志位定义**（timer.h:44-50）：

```cpp
#define TIMER_CPUMASK    0x0003FFFF     // CPU 编号掩码
#define TIMER_MIGRATING  0x00040000     // 正在跨 CPU 迁移
#define TIMER_BASEMASK   (TIMER_CPUMASK | TIMER_MIGRATING)
#define TIMER_DEFERRABLE 0x00080000     // 可延迟定时器（不将 CPU 从 idle 中唤醒）
#define TIMER_PINNED     0x00100000     // 固定在当前 CPU
#define TIMER_IRQSAFE    0x00200000     // IRQ 安全回调
```

### 3.2 struct timer_base — Per-CPU 基准

源码位置：`kernel/time/timer.c:250-265`

```cpp
struct timer_base {
    raw_spinlock_t          lock;                 // 保护此基准的锁
    struct timer_list       *running_timer;       // 正在执行回调的定时器
#ifdef CONFIG_PREEMPT_RT
    spinlock_t              expiry_lock;          // RT 内核过期锁
    atomic_t                timer_waiters;        // cancel 等待者计数
#endif
    unsigned long           clk;                  // 当前时钟 (jiffies)
    unsigned long           next_expiry;          // 下一个到期时间
    unsigned int            cpu;                  // 所属 CPU
    bool                    next_expiry_recalc;   // 需要重算 next_expiry
    bool                    is_idle;              // CPU 是否处于 idle
    bool                    timers_pending;       // 是否有待处理定时器
    DECLARE_BITMAP(pending_map, WHEEL_SIZE);     // 位图：哪些桶非空
    struct hlist_head       vectors[WHEEL_SIZE];  // WHEEL_SIZE 个桶
} ____cacheline_aligned;
```

**Per-CPU 实例化**（timer.c:267）：

```cpp
static DEFINE_PER_CPU(struct timer_base, timer_bases[NR_BASES]);
```

每个 CPU 拥有 `NR_BASES` 个 `timer_base`。在 NO_HZ_COMMON 配置下：
- `timer_bases[BASE_LOCAL]`：本地（固定）定时器
- `timer_bases[BASE_GLOBAL]`：全局（可迁移）定时器
- `timer_bases[BASE_DEF]`：可延迟（deferrable）定时器

### 3.3 轮盘的数学参数

源码位置：`kernel/time/timer.c` 约第 200 行附近

```cpp
#define LVL_SIZE        64            // 每级 64 个桶
#define LVL_DEPTH       9             // 共 9 级
#define WHEEL_SIZE      (LVL_SIZE * LVL_DEPTH)  // 576 个桶
#define LVL_CLK_DIV     8             // 各级间时钟分频因子
#define LVL_CLK_SHIFT   3             // log2(LVL_CLK_DIV)

#define NR_BASES        3             // LOCAL + GLOBAL + DEF
#define BASE_LOCAL      0
#define BASE_GLOBAL     1
#define BASE_DEF        2
```

**轮盘结构示意：**

```
Level 0:  桶[ 0.. 63]    粒度 = 1 jiffy    索引 = 0..63
Level 1:  桶[64..127]    粒度 = 8 jiffies  索引 = 64..127
Level 2:  桶[128..191]   粒度 = 64 jiffies 索引 = 128..191
Level 3:  桶[192..255]   粒度 = 512 jiffies索引 = 192..255
Level 4:  桶[256..319]   粒度 ~4 jiffies  索引 = 256..319  (HZ=1000: 4096 jiffies)
Level 5:  桶[320..383]   粒度 ~32 jiffies 索引 = 320..383
Level 6:  桶[384..447]   粒度 ~256 jiffies索引 = 384..447
Level 7:  桶[448..511]   粒度 ~2048 jiffies索引 = 448..511
Level 8:  桶[512..575]   粒度 ~16384 jiffies索引 = 512..575

每个桶 → hlist_head (单向链表)
每个桶 → pending_map 中的一位 (1 = 非空, 0 = 空)
```

---

## 4. 粒度与覆盖范围

### 4.1 HZ=1000 下的精确数值表

源码位置：`kernel/time/timer.c:104-114`

```
HZ 1000 steps (1 jiffy = 1 ms)
Level Offset  Granularity            Range
  0      0         1 ms                0 ms -         63 ms
  1     64         8 ms               64 ms -        511 ms
  2    128        64 ms              512 ms -       4095 ms (512ms - ~4s)
  3    192       512 ms             4096 ms -      32767 ms (~4s - ~32s)
  4    256      4096 ms (~4s)      32768 ms -     262143 ms (~32s - ~4m)
  5    320     32768 ms (~32s)    262144 ms -    2097151 ms (~4m - ~34m)
  6    384    262144 ms (~4m)    2097152 ms -   16777215 ms (~34m - ~4h)
  7    448   2097152 ms (~34m)  16777216 ms -  134217727 ms (~4h - ~1d)
  8    512  16777216 ms (~4h)  134217728 ms - 1073741822 ms (~1d - ~12d)

LVL_DEPTH = 9, LVL_SIZE = 64, LVL_CLK_DIV = 8
最大范围：64 + 64*8 + 64*8^2 + ... + 64*8^8 = 64*(8^9-1)/(8-1) ≈ 1.07×10^9 jiffies ≈ 12.4 天
```

### 4.2 HZ=300 下的数值表

源码位置：`kernel/time/timer.c:116-126`

```
HZ 300 steps (1 jiffy ≈ 3.33 ms)
Level Offset  Granularity            Range
  0      0         3 ms                0 ms -        210 ms
  1     64        26 ms              213 ms -       1703 ms (213ms - ~1s)
  ...
  8    512  55924053 ms (~15h) 447392426 ms - 3579139406 ms (~5d - ~41d)
```

### 4.3 HZ=250 下的数值表

源码位置：`kernel/time/timer.c:128-138`

```
HZ 250 steps (1 jiffy = 4 ms)
Level Offset  Granularity            Range
  0      0         4 ms                0 ms -        255 ms
  1     64        32 ms              256 ms -       2047 ms (256ms - ~2s)
  ...
  8    512  67108864 ms (~18h) 536870912 ms - 4294967288 ms (~6d - ~49d)
```

### 4.4 HZ=100 下的数值表

源码位置：`kernel/time/timer.c:140-143`

```
HZ 100 steps (1 jiffy = 10 ms)
Level Offset  Granularity            Range
  0      0         10 ms               0 ms -        630 ms
  1     64         80 ms             640 ms -       5110 ms (640ms - ~5s)
  ...
```

### 4.5 桶索引计算

给定过期时间 `expires`（jiffies），计算其应该在哪个桶：

```cpp
// 伪代码，核心逻辑在 timer.c 的 calc_wheel_index 及相关宏中
static int calc_wheel_index(unsigned long expires, unsigned long clk)
{
    unsigned long delta = expires - clk;

    if (delta < LVL_SIZE)          // Level 0: 0..63 jiffies
        return expires & (LVL_SIZE - 1);  // bucket 0..63
    if (delta < 1 << (LVL_CLK_SHIFT + 6)) // Level 1: 64..511
        return 64 + ((expires >> LVL_CLK_SHIFT) & (LVL_SIZE - 1));
    // ... 依次类推每级
}
```

**图解：Level 0 vs Level 1 的桶索引：**

```
时间轴 (jiffies):  |----- 0 -----|----- 1 -----|----- 2 -----| ...
                      clk=now

Level 0 (0-63):       index = expires & 0x3f
  桶0: jiffy 0, 64, 128, ...
  桶1: jiffy 1, 65, 129, ...
  ...
  桶63: jiffy 63, 127, 191, ...

Level 1 (64-127):    index = 64 + ((expires >> 3) & 0x3f)
  桶64: jiffies 0-7, 512-519, 1024-1031, ...
  桶65: jiffies 8-15, 520-527, 1032-1039, ...
  ...
  桶127: jiffies 504-511, 1016-1023, ...
```

每进入下一级，粒度乘以 8，每个桶覆盖的时间范围也乘以 8。

---

## 5. pending_map 位图优化

`timer_base.pending_map` 是一个 `DECLARE_BITMAP` 位图，每个 bit 对应 `vectors` 中的一个桶。如果桶非空，对应位置 1。

当一个级别的时钟指针移动时，只需检查 `pending_map` 中该级的 64 个 bit，**跳过空桶**，直接找到有定时器的桶。

```
vectors[0..575]: [ ][ ][x][ ][ ][x][ ]...[ ][ ][x][ ]
pending_map:     0  0  1  0  0  1  0 ... 0  0  1  0

过期处理流程：
   1. 获取当前 clk
   2. 计算当前 level 中需要处理哪些桶 (clk 的哪些 bit 决定了桶)
   3. 在位图中查找这些桶 → 跳过空桶
   4. 对有定时器的桶：
      - 遍历 hlist
      - 收集过期定时器到本地列表
      - 调用 timer->function(timer)
```

这种设计在定时器稀疏的场景下（大多数桶为空时）非常高效。

---

## 6. 核心 API

### 6.1 add_timer — 添加定时器

源码位置：`kernel/time/timer.c:1245-1251`

```cpp
void add_timer(struct timer_list *timer)
{
    if (WARN_ON_ONCE(timer_pending(timer)))
        return;
    __mod_timer(timer, timer->expires, MOD_TIMER_NOTPENDING);
}
```

`add_timer` 只能用于未入队的定时器（`timer_pending(timer)` 返回 false）。它使用定时器自带的 `expires` 字段（需在调用前设置），并将 `timer->function` 设为回调。

`__mod_timer` (timer.c 内部) 执行以下步骤：

```
__mod_timer(timer, expires, MOD_TIMER_NOTPENDING)
    │
    ├─ 计算定时器应放入哪个桶 (calc_wheel_index)
    ├─ 锁定对应 timer_base
    ├─ 如果 hashed locking 需要迁移:
    │   └─ 更新 timer->flags 中的 CPU 编号
    ├─ 如果定时器已在队列: detach_timer
    ├─ internal_add_timer:
    │   ├─ 计算桶索引 = calc_index(expires, base->clk)
    │   ├─ hlist_add_head(&timer->entry, &base->vectors[idx])
    │   ├─ base->pending_map 置位对应的 bit
    │   └─ 更新 base->next_expiry (如果需要)
    └─ 解锁
```

时间复杂度：O(1)（哈希链表头部插入）

### 6.2 mod_timer — 修改定时器

源码位置：`kernel/time/timer.c:1193-1196`

```cpp
int mod_timer(struct timer_list *timer, unsigned long expires)
{
    return __mod_timer(timer, expires, 0);
}
```

`mod_timer` 可以修改已入队或未入队的定时器。如果定时器已入队，先从原桶中删除，然后按新的 `expires` 重新入队。

返回值：
- **0**：定时器已在队列中，且新的 expires 与旧的等效（无需改变位置）
- **1**：定时器已从队列中移除并重新入队（因为 expires 变化改变了桶的位置）

### 6.3 timer_delete — 删除定时器

源码位置：`timer.h:167`

```cpp
extern int timer_delete(struct timer_list *timer);
```

函数实现（timer.c 内部）：

```cpp
int timer_delete(struct timer_list *timer)
{
    struct timer_base *base;
    unsigned long flags;
    int ret = -1;

    base = lock_timer_base(timer, &flags);

    if (timer_pending(timer)) {
        detach_timer(timer, true);
        ret = 1;
    }

    unlock_timer_base(timer, &flags);
    return ret;
}
```

### 6.4 timer_delete_sync — 同步删除

源码位置：`timer.h:166`

```cpp
extern int timer_delete_sync(struct timer_list *timer);
```

类似于 hrtimer 的 `hrtimer_cancel`，`timer_delete_sync` 确保删除返回后定时器回调不再运行。如果回调正在执行，它会等待回调完成（通过自旋等待 `base->running_timer != timer`）。

---

## 7. 过期处理路径

### 7.1 触发源

在低精度 tick 模式下，每个 jiffy tick 会调用 `run_local_timers()`：

```cpp
// timer.c:2416
static void run_local_timers(void)
{
    struct timer_base *base = this_cpu_ptr(&timer_bases[BASE_LOCAL]);

    hrtimer_run_queues();  // 同时运行到期的 hrtimer

    for (int i = 0; i < NR_BASES; i++, base++) {
        // 如果 next_expiry <= jiffies, raise TIMER_SOFTIRQ
        ...
    }
}
```

### 7.2 run_timer_softirq — Softirq 处理

源码位置：`kernel/time/timer.c:2401-2411`

```cpp
static __latent_entropy void run_timer_softirq(void)
{
    run_timer_base(BASE_LOCAL);   // 处理本地定时器
    if (IS_ENABLED(CONFIG_NO_HZ_COMMON)) {
        run_timer_base(BASE_GLOBAL);  // 处理全局定时器
        run_timer_base(BASE_DEF);     // 处理可延迟定时器

        if (is_timers_nohz_active())
            tmigr_handle_remote();    // 处理远程 CPU 上的定时器
    }
}
```

在 `open_softirq(TIMER_SOFTIRQ, run_timer_softirq)` (timer.c:2579) 中注册。

**run_timer_base 内部逻辑（简化）：**

```cpp
static void __run_timer_base(struct timer_base *base)
{
    // 1. 推进 base->clk (可能多级)
    while (需要推进时钟) {
        // 2. 确定当前级别需要处理的桶
        unsigned long idx = base->clk & LVL_MASK;
        struct hlist_head *vec = &base->vectors[idx];

        // 3. 收集过期定时器
        //    如果 pending_map 对应的组为 0，跳过
        if (base->pending_map & bit_for(idx))
            // 将整个桶的定时器取出到本地链表
            collect_expired_timers(base, vec);

        // 4. 执行回调
        for (each timer in expired_list) {
            base->running_timer = timer;
            raw_spin_unlock_irq(&base->lock);
            timer->function(timer);  // 回调！
            raw_spin_lock_irq(&base->lock);
            base->running_timer = NULL;
        }

        base->clk++;
    }
}
```

---

## 8. 与 hrtimer 的全面对比

### 8.1 架构对比

```
                    Timer Wheel                    vs           hrtimer
             ┌─────────────────────┐                    ┌─────────────────────┐
             │    timer_list        │                   │    hrtimer           │
             │  - expires (jiffies) │                   │ - node (RB-tree)     │
             │  - function           │                   │ - base (clock base)  │
             │  - flags (CPU编号等)  │                   │ - is_soft/is_hard    │
             │  - entry (hlist_node) │                   │ - _softexpires       │
             └─────────┬─────────────┘                   │ - function           │
                       │                                 └─────────┬───────────┘
                       ▼                                           ▼
             ┌─────────────────────┐                    ┌─────────────────────┐
             │    timer_base        │                   │ hrtimer_clock_base  │
             │  - vectors[576]      │                   │ - active (RB-root)  │
             │  - pending_map       │                   │ - expires_next      │
             │  - clk (jiffies)     │                   │ - offset            │
             │  - running_timer     │                   │ - running           │
             │  - next_expiry       │                   │ - clockid           │
             └─────────┬─────────────┘                   └─────────┬───────────┘
                       │                                           │
                       ▼                                           ▼
             ┌─────────────────────┐                    ┌─────────────────────┐
             │   数组: O(1) 查找    │                   │   RB-tree: O(log n) │
             │   桶: hlist (链表)   │                    │   按 expires 排序    │
             │   无级联: 原地到期   │                   │   精确排序          │
             └─────────────────────┘                   └─────────────────────┘
                       │                                           │
                       ▼                                           ▼
             ┌─────────────────────┐                    ┌─────────────────────┐
             │  TIMER_SOFTIRQ      │                    │ hrtimer_interrupt    │
             │  run_timer_softirq  │                    │ (hard) + HRTIMER_   │
             │                     │                    │  SOFTIRQ (soft)      │
             └─────────────────────┘                    └─────────────────────┘
```

### 8.2 关键差异

| 维度 | Timer Wheel | hrtimer |
|------|-------------|---------|
| **时间精度** | jiffies (1ms~10ms) | ns 级 |
| **存储结构** | 576 个桶 (hlist) | RB-tree (平衡二叉搜索树) |
| **插入复杂度** | O(1) | O(log n) |
| **删除复杂度** | O(1) | O(log n) |
| **查找首个到期** | O(1) 位图扫描 + O(1) 桶遍历 | O(1) 缓存 (expires_next) |
| **过期处理** | 整桶收集 → 批量回调 | 逐个取出最左节点 → 单点回调 |
| **回调上下文** | softirq (TIMER_SOFTIRQ) | hard IRQ + HRTIMER_SOFTIRQ |
| **跨度范围** | ~12–49 天 (HZ 相关) | 无限制 (64-bit ktime_t) |
| **精确度** | 级别粒度误差 (1ms~4h) | 纳秒级精确 |
| **跨 CPU 迁移** | flags 存储 CPU 编号 | timer->base 指针 |
| **时钟域** | 仅 jiffies (MONOTONIC) | MONOTONIC/REALTIME/BOOTTIME/TAI × (HARD\|SOFT) = 8 |
| **空间开销/定时器** | ~40 B (hlist_node + 基础字段) | ~64 B (rb_node + 更多字段) |

### 8.3 使用场景选择

**选择 Timer Wheel 当：**
- 精度要求 ≤ jiffy 粒度（1ms 级别）
- 需要极低的插入/删除开销
- 网络超时、磁盘 I/O 超时
- 协议栈保活定时器
- 大多数定时器在到期前被取消

**选择 hrtimer 当：**
- 精度要求 > jiffy 粒度（微秒/纳秒）
- 需要 wallsclock (REALTIME) 定时
- 需要 clock_nanosleep 级别的 sleep
- 周期 tick 模拟 (tick_sched)
- POSIX 高精度定时器 (timer_create)

### 8.4 数学分析：为什么 Timer Wheel 的 O(1) 快？

Timer Wheel 的每个桶是一个 `hlist`（单向链表），插入总是在链表头部进行（`hlist_add_head`）。虽然最坏情况下一个桶可能包含大量定时器，但：

1. **9 级 × 64 桶 = 576 个独立桶**，碰撞概率低。
2. **大多数定时器在 Level 0–3 范围内**（短期超时），粒度细、分布均匀。
3. **长期定时器会被粗糙粒度聚拢**，但长期定时器数量少。

而 hrtimer 的 RB-tree 虽然插入是 O(log n)，但每次插入都涉及树旋转和重新着色，常数因子更高。在低精度场景下，O(1) 的 Timer Wheel 更优。

---

## 9. 时间推进与过期收集

### 9.1 时钟推进 (clock advance)

`base->clk` 是 `unsigned long` 类型，存储当前 jiffies。其低 6 位（bits 0–5）索引 Level 0 的桶；每 8 个 Level 0 桶的推进触发 Level 1 移动一次；依此类推。

```
base->clk 的位布局 (以 HZ=1000 为例):

bit:  ... 15 14 13 12 11 10 9 8 | 7 6 5 4 3 2 1 0
      ───────────────────────────┼─────────────────
         Level 3                  Level 2    Level 1  Level 0

每次 base->clk++ 触发 Level 0 的一个桶被处理
每当 base->clk 的 bit 3..8 全部回绕 (每 64 次计数)，触发 Level 1 的一个桶
每 8 个 Level 1 桶，触发 Level 2 ... 依此类推
```

### 9.2 过期定时器收集

```cpp
// 伪代码
struct hlist_head expired;

// Level 0: 每次 tick 处理 (clk & 0x3f) 指向的桶
idx = base->clk & LVL_MASK;
hlist_move_list(&base->vectors[idx], &expired);  // 整个桶拉出
base->pending_map_clear_bit(idx);

// Level 1..8: 当时钟推进到该级的新组时处理
if ((base->clk & next_level_mask) == 0) {
    unsigned int lvl_clk = base->clk >> LVL_CLK_SHIFT;
    idx = LVL_OFFSET(1) + (lvl_clk & LVL_MASK);
    hlist_move_list(&base->vectors[idx], &expired);
    base->pending_map_clear_bit(idx);
}
```

收集到的所有过期定时器在同一个批量中逐个回调。

---

## 10. 特殊类型定时器

### 10.1 Deferrable Timer (可延迟定时器)

`TIMER_DEFERRABLE` (timer.h:47)：当 CPU 处于 idle 状态时，此类定时器不会将 CPU 从 idle 中唤醒。而是等待下一个非 deferrable 定时器激活 CPU 时一并处理。

这是节能（power saving）的关键机制——避免了为处理不重要的事件而频繁唤醒 CPU。

### 10.2 Pinned Timer (固定定时器)

`TIMER_PINNED` (timer.h:48)：定时器必须在其原始 CPU 上过期，不允许迁移。用于缓存局部性敏感的定时器。

### 10.3 IRQ Safe Timer

`TIMER_IRQSAFE` (timer.h:49)：回调函数在 IRQ 禁用的情况下执行。这使得 `timer_delete_sync` 可以与 IRQ 处理程序正确同步。专为解决 workqueue 锁定问题而设计。

---

## 11. 与 hrtimer 的互操作

Timer wheel 和 hrtimer 并非完全独立。在低精度模式下，`hrtimer_run_queues()` (hrtimer.c:2147) 由 `run_local_timers()` (timer.c:2416) 每个 jiffy 调用一次：

```cpp
static void run_local_timers(void)
{
    struct timer_base *base = this_cpu_ptr(&timer_bases[BASE_LOCAL]);

    hrtimer_run_queues();  // ← 在每个 jiffy 中运行到期的 hrtimer

    // 然后触发 TIMER_SOFTIRQ 运行 timer wheel
    ...
}
```

当所有 CPU 的 clocksource 可以支持高精度 one-shot 模式后，`hrtimer_switch_to_hres()` 启用高精度模式，`hrtimer_run_queues()` 变为空操作（因为 `hrtimer_hres_active(cpu_base)` 返回 true），timer wheel 的回调仍由 `TIMER_SOFTIRQ` 处理。

---

## 12. Timer Wheel 的优势与局限

### 优势

1. **极致的 enqueue/dequeue 速度**：O(1) 操作，链表头插入。
2. **内存局部性好**：桶和向量连续存储在 `timer_base` 中。
3. **免级联**：消除旧设计的级联延迟和 CPU 突发。
4. **位图加速**：`pending_map` 快速跳过空桶。
5. **隐式批处理**：粒度级别的定时器自然聚拢，减少回调次数抖动。

### 局限

1. **精度受限于 jiffies**：最多 ±(级别粒度) 的误差。
2. **时钟域单一**：只支持 MONOTONIC 时间。
3. **时间范围有限**：~12–49 天后溢出回到第一级。
4. **不精确的 ordering**：同一桶内的定时器到期顺序不可预测。

---

## 13. 总结

Timer wheel 是 Linux 内核"够用即可"设计哲学的典范：

| 概念 | 核心要点 |
|------|----------|
| 9 级轮盘 | 576 个桶，每级粒度递乘 8（LVL_CLK_DIV），无级联 |
| `struct timer_list` | 仅 4 个核心字段：entry, expires, function, flags |
| `struct timer_base` | 包含 vectors[576]、pending_map 位图、running_timer |
| O(1) 操作 | 链表头插入、位图快速跳过空桶、整桶收集回调 |
| TIMER_SOFTIRQ | run_timer_softirq → 按 clk 推进清理过期桶 |
| 精度误差 | 有意为之的隐式批处理，多数超时定时器在到期前被取消 |

hrtimer 和 timer wheel 各司其职：高精度场景用 hrtimer 的 RB-tree，低精度大批量场景用 timer wheel 的 O(1) 轮盘。两者共同构成了 Linux 内核完整的时间管理子系统。

---

## 下一篇文章

[第4篇：NO_HZ 与动态时钟 (tick-sched)](./04-nohz-tick.md) — 深入 dyntick/idle 机制、tick 模拟、sched_timer 的 hrtimer 实现，以及 clocksource 的选优与监控。
