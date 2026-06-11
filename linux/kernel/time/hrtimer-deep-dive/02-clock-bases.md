# 第2篇：时钟基准：MONOTONIC/REALTIME/BOOTTIME/TAI

> 源码：kernel/time/hrtimer.c (2503行) | 头文件：include/linux/hrtimer_defs.h (113行)

系列目录：[hrtimer 内核源码深度解析](./README.md)

---

## 1. 引言

上一篇我们剖析了 hrtimer 的三大核心数据结构。本篇聚焦 `hrtimer_cpu_base` 中的 `clock_base[8]` 数组——8 种时钟基准的语义差异、hard/soft 分流机制、时钟域转换，以及 `hrtimer_forward` 的实现。

每个 CPU 维持 8 个独立的 `hrtimer_clock_base`，映射到 4 种时间域 × 2 种回调上下文（hard/soft）：

```
               hrtimer_cpu_base.clock_base[8]
    ┌──────────────────────────────────────────────────────┐
    │                                                      │
    │  [0] HRTIMER_BASE_MONOTONIC      (HARD)             │
    │  [1] HRTIMER_BASE_REALTIME       (HARD)             │
    │  [2] HRTIMER_BASE_BOOTTIME       (HARD)             │
    │  [3] HRTIMER_BASE_TAI            (HARD)             │
    │                                                      │
    │  [4] HRTIMER_BASE_MONOTONIC_SOFT (SOFT)             │
    │  [5] HRTIMER_BASE_REALTIME_SOFT  (SOFT)             │
    │  [6] HRTIMER_BASE_BOOTTIME_SOFT  (SOFT)             │
    │  [7] HRTIMER_BASE_TAI_SOFT       (SOFT)             │
    │                                                      │
    └──────────────────────────────────────────────────────┘
```

---

## 2. 枚举定义

源码位置：`include/linux/hrtimer_defs.h:38-48`

```cpp
enum hrtimer_base_type {
    HRTIMER_BASE_MONOTONIC,        // 0
    HRTIMER_BASE_REALTIME,         // 1
    HRTIMER_BASE_BOOTTIME,         // 2
    HRTIMER_BASE_TAI,              // 3
    HRTIMER_BASE_MONOTONIC_SOFT,   // 4
    HRTIMER_BASE_REALTIME_SOFT,    // 5
    HRTIMER_BASE_BOOTTIME_SOFT,    // 6
    HRTIMER_BASE_TAI_SOFT,         // 7
    HRTIMER_MAX_CLOCK_BASES        // 8  (用作数组大小)
};
```

枚举值按 hard(0–3) → soft(4–7) 的顺序排列。这个顺序不是偶然的——它对应 active_bases 位掩码中的位布局，使 bit 0–3 代表 hard bases，bit 4–7 代表 soft bases。

内核定义了两个掩码常量为 `__hrtimer_run_queues()` 的传入参数（见 hrtimer.c 上部约行号 90 附近）：

```cpp
#define HRTIMER_ACTIVE_HARD  ((1U << HRTIMER_BASE_MONOTONIC)      | \
                              (1U << HRTIMER_BASE_REALTIME)       | \
                              (1U << HRTIMER_BASE_BOOTTIME)       | \
                              (1U << HRTIMER_BASE_TAI))
#define HRTIMER_ACTIVE_SOFT  ((1U << HRTIMER_BASE_MONOTONIC_SOFT) | \
                              (1U << HRTIMER_BASE_REALTIME_SOFT)  | \
                              (1U << HRTIMER_BASE_BOOTTIME_SOFT)  | \
                              (1U << HRTIMER_BASE_TAI_SOFT))
#define HRTIMER_ACTIVE_ALL   (HRTIMER_ACTIVE_HARD | HRTIMER_ACTIVE_SOFT)
```

---

## 3. Hard vs Soft：回调上下文的双重路径

### 3.1 设计动机

Linux 内核中，硬中断上下文的操作必须尽可能快——不能睡眠、不能持有过多锁。但有些定时器回调需要操作文件系统、内存分配等可能阻塞的操作。为此，hrtimer 引入了 hard/soft 双上下文设计：

| 特性 | Hard Timer | Soft Timer |
|------|-----------|------------|
| 回调执行上下文 | 硬中断 (hrtimer_interrupt) | softirq (hrtimer_run_softirq) |
| IRQ 状态 | 禁用 | 启用 |
| 可阻塞操作 | 不允许 | 允许 |
| 适用场景 | 周期 tick、clocksource watchdog、实时任务 | 磁盘 I/O 超时、网络超时、用户态 sleep |
| 注册方式 | HRTIMER_MODE_ABS_HARD / HRTIMER_MODE_REL_HARD | HRTIMER_MODE_ABS_SOFT / HRTIMER_MODE_REL_SOFT |
| PER-CPU flag | is_hard = true | is_soft = true |

### 3.2 Hard Timer 路径

```
hrtimer_interrupt (hrtimer.c:2083)
    │
    ├─ raw_spin_lock_irqsave(&cpu_base->lock)
    ├─ no:
    │   检查 softirq_expires_next
    │   → 如到期则 raise_timer_softirq(HRTIMER_SOFTIRQ)
    │
    └─ __hrtimer_run_queues(cpu_base, now, flags, HRTIMER_ACTIVE_HARD)
        │
        ├─ 只遍历 active_bases & HRTIMER_ACTIVE_HARD 中设置的 base
        │   (即索引 0-3: MONOTONIC/REALTIME/BOOTTIME/TAI 的 HARD 版本)
        │
        └─ 对每个到期的 timer:
            └─ 在硬中断上下文中直接调用 timer->function(timer)
```

### 3.3 Soft Timer 路径

```
hrtimer_interrupt (hrtimer.c:2083)
    │
    ├─ 检查 softirq_expires_next:
    │   if (now >= cpu_base->softirq_expires_next)
    │       raise_timer_softirq(HRTIMER_SOFTIRQ)
    │
    ...

HRTIMER_SOFTIRQ 触发 (由 __do_softirq 调度)
    │
    ▼
hrtimer_run_softirq (hrtimer.c:2001)
    │
    ├─ hrtimer_cpu_base_lock_expiry(cpu_base)  // PREEMPT_RT 专用锁
    ├─ raw_spin_lock_irqsave(&cpu_base->lock)
    ├─ now = hrtimer_update_base(cpu_base)
    │
    └─ __hrtimer_run_queues(cpu_base, now, flags, HRTIMER_ACTIVE_SOFT)
        │
        ├─ 只遍历 active_bases & HRTIMER_ACTIVE_SOFT 中设置的 base
        │   (即索引 4-7: MONOTONIC/REALTIME/BOOTTIME/TAI 的 SOFT 版本)
        │
        └─ 对每个到期的 timer:
            └─ 在 softirq 上下文中调用 timer->function(timer)
                (此时 IRQ 已启用，可睡眠/分配内存)
    │
    ├─ cpu_base->softirq_activated = false
    ├─ hrtimer_update_softirq_timer(cpu_base, true)
    │   └─ 重新计算 softirq_expires_next
    │
    ├─ raw_spin_unlock_irqrestore
    └─ hrtimer_cpu_base_unlock_expiry(cpu_base)
```

### 3.4 softirq_activated 标记

源码位置：`hrtimer_defs.h:60-61`

`cpu_base->softirq_activated` 防止对同一个 CPU 重复触发 `HRTIMER_SOFTIRQ`。当 `hrtimer_interrupt` 或 `hrtimer_run_queues` 检测到 soft timer 到期后设置此标记，后续的 `hrtimer_update_softirq_timer` 检查此标记，避免在 softirq 尚未执行前再次触发。

### 3.5 PREEMPT_RT 下的差异

在 `CONFIG_PREEMPT_RT` 配置下：

- Hard timer 可能被降级为 soft timer（因为 RT 内核上硬中断是线程化的）。
- 增加了 `softirq_expiry_lock` (spinlock_t)（见 `hrtimer_defs.h:100`），用于在 hard 和 soft 过期路径之间提供额外同步。
- 增加了 `timer_waiters` (atomic_t) 计数（见 `hrtimer_defs.h:101`），用于 `hrtimer_cancel_wait_running` 的 wait 机制。
- Mode 检查改为校验 `HARD` bit 而非 `SOFT` bit（见 `hrtimer_start_range_ns:1484-1487`）。

---

## 4. 四种时间域详解

### 4.1 CLOCK_MONOTONIC — 单调时钟

```
特性：从系统启动开始单调递增，不受系统时间调整影响
用途：内核内部定时器、调度周期、超时检测
offset: 0 (expires 直接以 ktime_t 存储)
获取时间: ktime_get()  (hrtimer.c:1744-1745)
```

CLOCK_MONOTONIC 是最常用的 hrtimer 时钟源。它从系统启动开始的纳秒数，不受 `settimeofday()` 等系统时间调整操作的影响。绝大多数内核定时器使用此时钟域。

在低精度模式下，`ktime_get()` 基于 jiffies 计算；在高精度模式下，从 clocksource 直接读取当前时间。

### 4.2 CLOCK_REALTIME — 实时时钟（墙上时钟）

```
特性：受 settimeofday()、NTP 调整影响
用途：用户态定时器接口 (timer_create/setitimer)、文件时间戳
offset: 从 monotonic 到 wall clock 的偏移量
获取时间: ktime_get_real()  (hrtimer.c:1746-1747)
```

CLOCK_REALTIME 的最大挑战是 `settimeofday()` 会改变墙上时间。如果 REALTIME 定时器使用绝对过期时间，系统时间被拨快或拨慢时会导致所有 REALTIME 定时器立即到期或延迟。

内核的解决方案是 **offset 机制**（见 `hrtimer_clock_base.offset`）：

```
REALTIME timer 的 expires 存储的是相对于 monotonic 的值
真实的墙上过期时间 = expires + base->offset
```

当 `settimeofday()` 被调用时，只需更新 `base->offset`（使用 `seqcount` 保护），无需遍历 RB-tree 更新所有 REALTIME 定时器的过期时间。这是一个经典的高效设计：

```
用户调用 settimeofday() 将时钟拨快 3600 秒
    │
    ▼
更新 REALTIME base->offset += 3600秒
    │ (使用 raw_write_seqcount_begin/end 保护写者)
    │
    │ 读取方 (hrtimer_get_softexpires_tv64 等):
    │   do { seq = read_seqcount_begin(&base->seq);
    │        expires = node->expires + base->offset;
    │   } while (read_seqcount_retry(&base->seq, seq));
    │
    ▼
所有 REALTIME 定时器的有效过期时间自动偏移
```

**读取 offset 的锁机制示例**（hrtimer.c 相关代码）：

```cpp
static inline s64 hrtimer_get_expires_tv64(const struct hrtimer *timer)
{
    struct hrtimer_clock_base *base = timer->base;
    s64 expires = timer->node.expires;

    // 使用 seqcount 读取 offset
    do {
        unsigned int seq = raw_read_seqcount_begin(&base->seq);
        expires += base->offset;
    } while (raw_read_seqcount_retry(&base->seq, seq));

    return expires;
}
```

### 4.3 CLOCK_BOOTTIME — 启动时间（含休眠）

```
特性：CLOCK_MONOTONIC + 系统休眠时间
用途：需要不受休眠影响的定时器、电源管理相关超时
offset: 系统休眠累计时间
获取时间: ktime_get_boottime()  (hrtimer.c:1748-1749)
```

CLOCK_BOOTTIME 在 CLOCK_MONOTONIC 基础上累加了系统休眠（suspend）时间。当系统恢复时，BOOTTIME 的值不会跳变——休眠时间被平滑地累积到 BOOTTIME 中。

这使得 BOOTTIME 适合作为"绝对时间"引用，用于需要跨越休眠周期的定时器（例如：闹钟、定时关机）。

**实现原理（简化）：**

```
ktime_get_boottime()
    │
    ├─ monotonic = ktime_get()
    │   (从 clocksource 或 jiffies 读取系统运行时间)
    │
    ├─ sleep_time = timekeeping 子系统维护的累计休眠时间
    │
    └─ return monotonic + sleep_time
```

### 4.4 CLOCK_TAI — 国际原子时

```
特性：CLOCK_REALTIME 去掉闰秒调整
用途：需要严格单调递增且与墙上时间相关的科学/金融计算
offset: 类似 REALTIME，但补偿闰秒
获取时间: ktime_get_clocktai()  (hrtimer.c:1750-1751)
```

TAI (Temps Atomique International) 与 UTC 的差异在于闰秒。UTC 会插入闰秒来对齐天文观测，而 TAI 不会。`CLOCK_TAI = CLOCK_REALTIME - 闰秒偏移`。

与 REALTIME 一样，TAI 的 `expires` 也使用 offset 机制来高效处理 `settimeofday()` 修改。

---

## 5. 时钟域映射

### 5.1 hrtimer_clockid_to_base

源码位置：`kernel/time/hrtimer.c:1724-1739`

```cpp
static inline int hrtimer_clockid_to_base(clockid_t clock_id)
{
    switch (clock_id) {
    case CLOCK_MONOTONIC:
        return HRTIMER_BASE_MONOTONIC;
    case CLOCK_REALTIME:
        return HRTIMER_BASE_REALTIME;
    case CLOCK_BOOTTIME:
        return HRTIMER_BASE_BOOTTIME;
    case CLOCK_TAI:
        return HRTIMER_BASE_TAI;
    default:
        WARN(1, "Invalid clockid %d. Using MONOTONIC\n", clock_id);
        return HRTIMER_BASE_MONOTONIC;
    }
}
```

此函数将 POSIX `clockid_t` 映射到 `hrtimer_base_type` 枚举值。注意它返回的是 **HARD** 版本（索引 0–3）。soft 版本在此基础上加 4：

```cpp
// hrtimer_setup 内部 (hrtimer.c:1790~):
base += hrtimer_clockid_to_base(clock_id);
if (timer->is_soft)
    base += HRTIMER_MAX_CLOCK_BASES / 2;  // 跳到 SOFT 区域 (索引 4-7)
```

### 5.2 __hrtimer_cb_get_time — 获取当前时间

源码位置：`kernel/time/hrtimer.c:1741-1756`

```cpp
static ktime_t __hrtimer_cb_get_time(clockid_t clock_id)
{
    switch (clock_id) {
    case CLOCK_MONOTONIC:
        return ktime_get();
    case CLOCK_REALTIME:
        return ktime_get_real();
    case CLOCK_BOOTTIME:
        return ktime_get_boottime();
    case CLOCK_TAI:
        return ktime_get_clocktai();
    default:
        WARN(1, "Invalid clockid %d. Using MONOTONIC\n", clock_id);
        return ktime_get();
    }
}
```

此函数在 `__hrtimer_start_range_ns` 中用于将相对时间转换为绝对时间（`hrtimer.c:1376-1377`）：

```cpp
if (mode & HRTIMER_MODE_REL)
    tim = ktime_add_safe(tim, __hrtimer_cb_get_time(base->clockid));
```

### 5.3 hrtimer_cb_get_time — 定时器回调获取时间

源码位置：`kernel/time/hrtimer.c:1758-1762`

```cpp
ktime_t hrtimer_cb_get_time(const struct hrtimer *timer)
{
    return __hrtimer_cb_get_time(timer->base->clockid);
}
```

在回调函数内部，可以通过 `hrtimer_cb_get_time(timer)` 获取与自身时钟域一致的当前时间，用于计算下一次过期时间。

---

## 6. hrtimer_start_range_ns 的内部机制

### 6.1 __hrtimer_start_range_ns

源码位置：`kernel/time/hrtimer.c:1355-1455`（大致范围）

此函数是 `hrtimer_start_range_ns` 的核心实现。关键步骤：

```
__hrtimer_start_range_ns (hrtimer.c:1355)
    │
    ├─ 获取 was_first = (cpu_base->next_timer == timer)
    │   用于判断移除前该定时器是否为首个到期定时器
    │
    ├─ 调用 hrtimer_keep_base 决定是否保留在当前 CPU
    │   (hrtimer.c:1322-1353)
    │   │
    │   ├─ ON = 同一 CPU
    │   ├─ pin = PINNED 模式
    │   ├─ 首选本地: is_local && is_first && !is_pinned
    │   └─ 如果 ONLINE && 本地 && 非首 && 非 PIN:
    │       随机分配 (hrtimer_prefer_random)
    │
    ├─ 如果是 REL 模式:
    │   tim = ktime_add_safe(tim, __hrtimer_cb_get_time(base->clockid))
    │   将相对时间转为绝对时间
    │
    ├─ hrtimer_update_lowres:
    │   低精度模式窗口补偿 (使过期时间落在 tick 窗口内)
    │
    ├─ 如果 keep_base (保持在同一个 base):
    │   └─ remove_and_enqueue_same_base (hrtimer.c:1282-1319)
    │       在同一个 base 上完成出队→设时间→入队的操作
    │       避免了跨 CPU 锁迁移的开销
    │
    ├─ 否则 (需要迁移或首次入队):
    │   ├─ remove_hrtimer (如果已在队列中)
    │   ├─ hrtimer_set_expires_range_ns
    │   ├─ switch_hrtimer_base → 迁移到目标 CPU
    │   └─ enqueue_hrtimer
    │
    ├─ 如果 deferred_rearm 被设置:
    │   ├─ deferred_needs_update = true
    │   └─ return false (不在此处重编程)
    │
    └─ return first (是否成为新基的首个到期定时器)
```

### 6.2 hrtimer_start 便利函数

在 `hrtimer.h` 约第 217 行位置有内联的 `hrtimer_start` 函数：

```cpp
static inline void hrtimer_start(struct hrtimer *timer, ktime_t tim,
                                 const enum hrtimer_mode mode)
{
    hrtimer_start_range_ns(timer, tim, 0, mode);
}
```

`hrtimer_start` 是对 `hrtimer_start_range_ns` 的简单包装，将 `delta_ns` 设为 0（无松弛范围）。

---

## 7. hrtimer_forward — 周期定时器的前进

源码位置：`kernel/time/hrtimer.c:1061-1093`

```cpp
u64 hrtimer_forward(struct hrtimer *timer, ktime_t now, ktime_t interval)
{
    ktime_t delta;
    u64 orun = 1;

    delta = ktime_sub(now, hrtimer_get_expires(timer));

    if (delta < 0)
        return 0;  // 尚未过期

    if (WARN_ON(timer->is_queued))
        return 0;  // 定时器不应在队列中

    if (interval < hrtimer_resolution)
        interval = hrtimer_resolution;

    if (unlikely(delta >= interval)) {
        s64 incr = ktime_to_ns(interval);

        orun = ktime_divns(delta, incr);
        hrtimer_add_expires_ns(timer, incr * orun);
        if (hrtimer_get_expires(timer) > now)
            return orun;
        orun++;  // 精确校正
    }
    hrtimer_add_expires(timer, interval);

    return orun;
}
```

`hrtimer_forward` 是周期定时器的核心函数。它将定时器的过期时间向前推进，避免多次重复到期（overrun）问题。

**工作原理（图解）：**

```
时间线:  |-----|-----|-----|-----|-----|-----|---→
          ^           ^                       ^
        last_exp    interval              now

第一次检查:
    delta = now - last_exp
    如果 delta >= interval:
        跳过多个 interval
        orun = delta / interval  (超量计数)
        expires += interval * orun
        
    确保 expires > now (否则再加一个 interval)
```

**使用场景示例（tick_nohz_handler, tick-sched.c:306-334）：**

```cpp
static enum hrtimer_restart tick_nohz_handler(struct hrtimer *timer)
{
    struct tick_sched *ts = container_of(timer, struct tick_sched, sched_timer);
    ktime_t now = ktime_get();

    tick_sched_do_timer(ts, now);
    if (regs)
        tick_sched_handle(ts, regs);

    if (unlikely(tick_sched_flag_test(ts, TS_FLAG_STOPPED)))
        return HRTIMER_NORESTART;  // tick 已停止

    hrtimer_forward(timer, now, TICK_NSEC);  // 前进一个 tick 周期
    return HRTIMER_RESTART;  // 自动重新入队
}
```

---

## 8. hrtimer 定时器初始化

### 8.1 hrtimer_setup

源码位置：`kernel/time/hrtimer.c:1764-1795`（大致范围）

```cpp
static void __hrtimer_setup(struct hrtimer *timer,
                             enum hrtimer_restart (*fn)(struct hrtimer *),
                             const clockid_t clock_id, enum hrtimer_mode mode)
{
    int base;

    memset(timer, 0, sizeof(*timer));

    timer->function = fn;
    timer->is_rel = !!(mode & HRTIMER_MODE_REL);
    timer->is_soft = !!(mode & HRTIMER_MODE_SOFT);
    timer->is_hard = !!(mode & HRTIMER_MODE_HARD);
    timer->is_lazy = !!(mode & HRTIMER_MODE_LAZY_REARM);

    // 映射 clockid → 基准索引
    base = hrtimer_clockid_to_base(clock_id);
    if (timer->is_soft)
        base += HRTIMER_MAX_CLOCK_BASES / 2;  // +4, 跳到 SOFT 区域

    timer->base = &per_cpu(hrtimer_bases, raw_smp_processor_id()).clock_base[base];
}
```

初始化的 timer 其 `base` 指针指向当前 CPU 的对应 clock_base，但定时器尚未入队（`is_queued` 为 0）。

### 8.2 hrtimer_setup 的对外接口

```cpp
// hrtimer.h 中
#define hrtimer_setup(t, cb, clock, mode)  __hrtimer_setup(t, cb, clock, mode)
```

典型用法：

```cpp
struct hrtimer my_timer;
hrtimer_setup(&my_timer, my_callback, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
```

---

## 9. active_bases 位掩码管理

`cpu_base->active_bases` 是一个 `unsigned int` 位掩码，用于快速跳过空的 clock_base。管理规则：

```
定时器入队 (enqueue_hrtimer, hrtimer.c:1112):
    active_bases |= 1 << base->index
    将 base->index 对应位设为 1

定时器出队 (__remove_hrtimer):
    如果 base->active (RB-tree) 为空:
        active_bases &= ~(1 << base->index)
        将 base->index 对应位清零

中断处理 (__hrtimer_run_queues):
    遍历 active_bases & active_mask 中每个设置的位
    跳过为 0 的位——该基准上没有定时器需要处理
```

这种设计使得中断处理函数的效率极高——在大多数空闲系统中，`active_bases` 为 0，中断处理几乎不花时间。

---

## 10. 性能分析

### 10.1 时钟域选择的影响

| 时钟域 | offset 开销 | settimeofday 影响 | 推荐场景 |
|--------|------------|-------------------|----------|
| MONOTONIC | 无 | 不受影响 | 内核内部、超时、调度 |
| REALTIME | seqcount 读 | 受影响 (通过 offset 处理) | 用户态定时器 |
| BOOTTIME | 睡眠累计 | 休眠累积 | 跨休眠定时器 |
| TAI | seqcount 读 | 受影响 | 科学计算 |

MONOTONIC 是最快的选择，因为没有 offset 的 seqcount 开销。REALTIME/TAI 基准读取 expires 时需要 seqcount 重试循环。

### 10.2 Hard vs Soft 的选择

选择 hard timer 可以减少延迟（直接在硬中断中执行）但必须保证回调函数快速且不会阻塞。选择 soft timer 有微小的延迟增加（需要等待 softirq 被调度），但回调可以做更多事情。

**延迟对比：**

```
Hard timer:
  时钟事件中断 → hrtimer_interrupt → 立即执行回调
  延迟: ~1-2 μs (取决于中断被禁止的时间)

Soft timer:
  时钟事件中断 → hrtimer_interrupt → raise HRTIMER_SOFTIRQ
     → 等待当前中断返回 → __do_softirq → hrtimer_run_softirq
  延迟: ~5-10 μs (取决于 softirq 调度延迟)
```

---

## 11. 总结

本文详细解析了 hrtimer 子系统中 8 种时钟基准的语义和机制：

| 概念 | 核心要点 |
|------|----------|
| Hard vs Soft | 通过不同的 active 掩码和两个独立的过期函数 (`hrtimer_interrupt` vs `hrtimer_run_softirq`) 实现双上下文回调 |
| 4 种时间域 | MONOTONIC（单调）、REALTIME（墙上）、BOOTTIME（含休眠）、TAI（原子时） |
| offset 机制 | REALTIME/TAI 的 expires 存储相对值，通过 base->offset 支持高效的 settimeofday 操作 |
| hrtimer_forward | 处理周期定时器的超量问题，一次跳过多个 interval |
| active_bases | 位掩码优化，使中断处理在无定时器时几乎零开销 |

---

## 下一篇文章

[第3篇：Timer Wheel：传统低精度定时器](./03-timer-wheel.md) — 深入 timer wheel 的 9 级轮盘结构、无级联设计、与 hrtimer 的架构对比。
