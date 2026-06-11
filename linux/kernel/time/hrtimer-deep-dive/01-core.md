# 第1篇：核心数据结构与红黑树操作

> 源码：kernel/time/hrtimer.c (2503行) | 头文件：include/linux/hrtimer_types.h (53行), include/linux/hrtimer_defs.h (113行), include/linux/hrtimer.h (357行)

系列目录：[hrtimer 内核源码深度解析](./README.md)

---

## 1. 引言与设计目标

hrtimer（high-resolution timer）子系统诞生于 2005–2006 年，由 Thomas Gleixner 和 Ingo Molnar 主导开发，是 Linux 内核从毫秒级（jiffies）跨入纳秒级定时的关键基础设施。其设计目标包括：

1. **纳秒级分辨率**：受限于 clocksource 硬件，理想可达 1ns 精度。
2. **高精度模式下通过 one-shot 时钟事件设备触发中断**：不再是周期性 tick。
3. **保持与低精度模式（jiffies 粒度）的兼容**：在无高精度时钟事件设备的平台上自动回退。
4. **per-CPU 时钟基准与全局时钟域分离**：每个 CPU 独立管理 8 个时钟基准。
5. **hard/soft 双上下文回调**：关键路径在硬中断中完成，非关键路径延后到 softirq。

本篇文章聚焦三大核心数据结构（`struct hrtimer`、`struct hrtimer_clock_base`、`struct hrtimer_cpu_base`）及其操作接口，并在最后剖析一次完整的中断处理流程。

---

## 2. 核心数据结构

### 2.1 struct hrtimer — 定时器本体

源码位置：`include/linux/hrtimer_types.h:41-51`

```cpp
struct hrtimer {
    struct timerqueue_linked_node    node;       // RB-tree 节点
    struct hrtimer_clock_base       *base;       // 所属时钟基准
    bool                             is_queued;  // 是否加入 RB-tree
    bool                             is_rel;     // 相对时间标记
    bool                             is_soft;    // softirq 过期标记
    bool                             is_hard;    // hard IRQ 过期标记
    bool                             is_lazy;    // 延迟重编程标记
    ktime_t                          _softexpires; // 软件过期时间
    enum hrtimer_restart            (*__private function)(struct hrtimer *); // 回调函数
};
```

**字段详解：**

- **node**：嵌入的 `timerqueue_linked_node`，包含 `struct rb_node` 和 `ktime_t expires`。插入 RB-tree 时按 `expires` 排序，形成按过期时间递增的红黑树。
- **base**：指向此定时器当前所在的 `hrtimer_clock_base`。可用于获取 `clockid`、`cpu_base` 等信息。跨 CPU 迁移时此指针会变化。
- **is_queued**：标记定时器是否已入队。取值为 `HRTIMER_STATE_ENQUEUED`（已入队）或 `HRTIMER_STATE_INACTIVE`（未入队）。通过 `WRITE_ONCE/READ_ONCE` 实现无锁查询。
- **is_rel**：标记过期时间为相对时间（从调用时刻开始计算）。
- **is_soft / is_hard**：指示回调函数在哪个上下文中执行。hard 在 `hrtimer_interrupt` 硬中断中直接调用；soft 由 `hrtimer_run_softirq` 在 `HRTIMER_SOFTIRQ` softirq 中调用。
- **is_lazy**：标记此定时器允许延迟重编程，避免虚拟机中的频繁重编程。
- **_softexpires**：软过期时间（相对于 monotonic 基准）。用于判断 soft timer 是否到期，以及确定 `interrupt` 中是否要触发 softirq。
- **function**：回调函数指针，原型为 `enum hrtimer_restart (*)(struct hrtimer *)`。返回 `HRTIMER_NORESTART` 或 `HRTIMER_RESTART`（周期定时器用后者）。

### 2.2 struct hrtimer_clock_base — 时钟基准

源码位置：`include/linux/hrtimer_defs.h:27-36`

```cpp
struct hrtimer_clock_base {
    struct hrtimer_cpu_base         *cpu_base;    // 所属 per-CPU 基准
    const unsigned int               index;       // 在 clock_base[] 数组中的索引
    const clockid_t                  clockid;     // CLOCK_MONOTONIC/REALTIME/BOOTTIME/TAI
    seqcount_raw_spinlock_t          seq;         // 顺序锁，保护 offset 读
    ktime_t                          expires_next;// 下一个到期时间（最左节点）
    struct hrtimer                  *running;     // 正在运行中的定时器
    struct timerqueue_linked_head    active;      // RB-tree 根节点
    ktime_t                          offset;      // 时钟偏移量
} __hrtimer_clock_base_align;
```

**字段详解：**

- **cpu_base**：反向指针，指向包含此 base 的 `hrtimer_cpu_base`。
- **index**：在 `hrtimer_cpu_base.clock_base[]` 数组中的位置索引（0–7）。
- **clockid**：标识时钟类型。`CLOCK_MONOTONIC` / `CLOCK_REALTIME` / `CLOCK_BOOTTIME` / `CLOCK_TAI` 四种，每种分为 hard 和 soft 两个实例。
- **seq**：顺序锁。使用 `raw_spinlock_t` 作为序列号保护（`seqcount_raw_spinlock_t`）。读写 `offset` 字段时需要持有此锁。读取方使用 `seqcount` 的重试机制避免加锁。
- **expires_next**：RB-tree 中最左节点的过期时间，即下一个到期定时器的时间戳。若树为空则为 `KTIME_MAX`。
- **running**：指向当前正在执行回调的定时器。跨 CPU 取消操作（`hrtimer_cancel`）依赖此字段判断定时器是否正在被某个 CPU 执行。
- **active**：`timerqueue_linked_head` 结构体，包含 RB-tree 根节点 `struct rb_root_cached`。所有在此基准上排队的定时器通过 `node` 字段链接成红黑树。
- **offset**：时钟偏移量。对于 `CLOCK_MONOTONIC` 和 `CLOCK_BOOTTIME`，offset 为 0（expires 直接以 ktime_t 存储）。对于 `CLOCK_REALTIME` 和 `CLOCK_TAI`，expires 存储相对值，offset 存储相对 monotonic 的偏移，用于支持 `settimeofday()` 调整系统时间时高效更新所有 REALTIME 定时器。

### 2.3 struct hrtimer_cpu_base — Per-CPU 基准

源码位置：`include/linux/hrtimer_defs.h:82-110`

```cpp
struct hrtimer_cpu_base {
    raw_spinlock_t              lock;                   // 保护此 CPU 上所有 base 和定时器的锁
    unsigned int                cpu;                    // 所属 CPU 编号
    unsigned int                active_bases;           // 位域，标记有活跃定时器的 base
    unsigned int                clock_was_set_seq;      // 时钟变更序列号
    bool                        hres_active;            // 高精度模式是否激活
    bool                        deferred_rearm;         // 延迟重新编程标记
    bool                        deferred_needs_update;  // 延迟重编程需要重新评估首个定时器
    bool                        hang_detected;          // 上次中断检测到挂起
    bool                        softirq_activated;      // softirq 已触发标记
    bool                        online;                 // CPU 在 hrtimer 视角下是否在线
#ifdef CONFIG_HIGH_RES_TIMERS
    unsigned int                nr_events;              // 中断事件总数
    unsigned short              nr_retries;             // 中断重试次数
    unsigned short              nr_hangs;               // 挂起次数
    unsigned int                max_hang_time;          // 最长挂起时间
#endif
#ifdef CONFIG_PREEMPT_RT
    spinlock_t                  softirq_expiry_lock;    // softirq 过期锁
    atomic_t                    timer_waiters;          // cancel 等待计数
#endif
    ktime_t                     expires_next;           // 下一个硬过期时间
    struct hrtimer             *next_timer;             // 首个过期定时器指针
    ktime_t                     softirq_expires_next;   // 下一个软过期时间
    struct hrtimer             *softirq_next_timer;     // 首个软过期定时器指针
    ktime_t                     deferred_expires_next;  // 缓存的延迟过期时间
    struct hrtimer_clock_base   clock_base[HRTIMER_MAX_CLOCK_BASES]; // 8个时钟基准
    call_single_data_t          csd;                    // 跨CPU回调数据
} ____cacheline_aligned;
```

**关键字段说明：**

- **lock**：`raw_spinlock_t`，保护当前 CPU 上所有时钟基准及其定时器的唯一锁。采用哈希锁方案：持有 `per_cpu(hrtimer_bases)[n].lock` 即可锁定绑定到该 base 的所有定时器。
- **active_bases**：位掩码，每位对应一个时钟基准。定时器入队时设置对应位，最后一个定时器出队时清除对应位。中断处理使用此字段跳过空基准。
- **hres_active**：表示当前 CPU 是否启用了高精度模式。为 false 时回退到 `hrtimer_run_queues()` 在每个 jiffy 中运行。
- **deferred_rearm**：当为 true 时，`hrtimer_start_range_ns` 不会重新编程时钟事件设备，而是通过 `TIF_HRTIMER_REARM` 线程标志延后到 IRQ 退出或 `__hrtimer_rearm_deferred` 执行。
- **hang_detected**：当 `hrtimer_interrupt` 重试 3 次后 `expires_next` 仍小于 `now` 时设置。下次重编程时强制添加 100ms 延迟，防止中断风暴。
- **expires_next**：全局最早的 hard timer 过期时间（考虑所有 clock_base）。设置为 `KTIME_MAX` 可阻止远程 CPU 在本地 CPU 正在处理过期定时器期间向本地排队新定时器。
- **next_timer**：指向首个即将过期的定时器。这是 `__remove_hrtimer()` 的优化用指针（避免遍历树查最左节点）。**注意**：跨 CPU 删除时此指针不可靠，不可解引用。
- **softirq_expires_next**：全局最早的 soft timer 过期时间。当 `now >= softirq_expires_next` 时，中断处理函数会触发 `HRTIMER_SOFTIRQ`。
- **softirq_activated**：标记 softirq 已被触发，避免重复触发。
- **clock_base[HRTIMER_MAX_CLOCK_BASES]**：8 个时钟基准数组。索引 0–3 为 MONOTONIC/REALTIME/BOOTTIME/TAI 的 hard 版本，索引 4–7 为对应的 soft 版本。
- **csd**：用于向其他 CPU 发送 IPI 请求重新编程时钟事件设备。

### 2.4 Per-CPU 实例化

源码位置：`kernel/time/hrtimer.c:106-120`

```cpp
DEFINE_PER_CPU(struct hrtimer_cpu_base, hrtimer_bases) =
{
    .lock = __RAW_SPIN_LOCK_UNLOCKED(hrtimer_bases.lock),
    .clock_base = {
        BASE_INIT(HRTIMER_BASE_MONOTONIC,     CLOCK_MONOTONIC),
        BASE_INIT(HRTIMER_BASE_REALTIME,      CLOCK_REALTIME),
        BASE_INIT(HRTIMER_BASE_BOOTTIME,      CLOCK_BOOTTIME),
        BASE_INIT(HRTIMER_BASE_TAI,           CLOCK_TAI),
        BASE_INIT(HRTIMER_BASE_MONOTONIC_SOFT, CLOCK_MONOTONIC),
        BASE_INIT(HRTIMER_BASE_REALTIME_SOFT, CLOCK_REALTIME),
        BASE_INIT(HRTIMER_BASE_BOOTTIME_SOFT, CLOCK_BOOTTIME),
        BASE_INIT(HRTIMER_BASE_TAI_SOFT,      CLOCK_TAI),
    },
    .csd = CSD_INIT(retrigger_next_event, NULL)
};
```

`DEFINE_PER_CPU` 宏为每个 CPU 创建一个 `hrtimer_cpu_base` 实例，命名为 `hrtimer_bases`。初始化的时钟基准按 hard→soft 顺序排列，每种时间域各有一个 hard 和一个 soft 版本。

`BASE_INIT` 是一个宏，初始化 `clock_base[i]` 的各个字段：

```cpp
#define BASE_INIT(type, clock) \
    { .cpu_base = &hrtimer_bases, .index = type, .clockid = clock }

// 实际展开要复杂得多（见 hrtimer.c:80-101），需要设置 seq 和 offset
```

在 UP 系统中，`lock_hrtimer_base` 的实现很简单（hrtimer.c:317-325），直接返回 `timer->base` 并加锁。但在 SMP 系统中引入了 migration_base 机制和 hashed locking。

---

## 3. 锁机制与跨 CPU 迁移

### 3.1 Hashed Locking

源码位置：`kernel/time/hrtimer.c:171-198`

hrtimer 采用哈希锁方案：`per_cpu(hrtimer_bases)[n].lock` 保护所有绑定到该 base 上所有定时器的操作。这种设计的优势在于：

1. 每个 CPU 的 8 个 clock_base 共用同一把锁（`hrtimer_cpu_base.lock`）。
2. 持有 `per_cpu(hrtimer_bases)[cpu].lock` 即锁定了该 CPU 上所有定时器。
3. `__run_timers` / `migrate_timers` 可以安全地修改队列上所有定时器。

```cpp
// hrtimer.c:183-199
static struct hrtimer_clock_base *lock_hrtimer_base(const struct hrtimer *timer,
                                                     unsigned long *flags)
{
    for (;;) {
        struct hrtimer_clock_base *base = READ_ONCE(timer->base);

        if (likely(base != &migration_base)) {
            raw_spin_lock_irqsave(&base->cpu_base->lock, *flags);
            if (likely(base == timer->base))
                return base;
            /* timer 已迁移到另一个 CPU */
            raw_spin_unlock_irqrestore(&base->cpu_base->lock, *flags);
        }
        cpu_relax();
    }
}
```

加锁-重读（lock-and-recheck）模式：在加锁之前用 `READ_ONCE` 读取 base 指针，加锁后检查 base 指针是否未变。如果已变（定时器被迁移），则释放锁重试。

### 3.2 Migration Base

源码位置：`kernel/time/hrtimer.c:159-169`

```cpp
static struct hrtimer_cpu_base migration_cpu_base = {
    .clock_base = {
        [0] = {
            .cpu_base = &migration_cpu_base,
            .seq      = SEQCNT_RAW_SPINLOCK_ZERO(migration_cpu_base.seq,
                                                  &migration_cpu_base.lock),
        },
    },
};

#define migration_base  migration_cpu_base.clock_base[0]
```

`migration_base` 是一个特殊的占位基准，用于在定时器跨 CPU 迁移过程中标记"正在搬家"状态。当 `timer->base == &migration_base` 时：

- `lock_hrtimer_base` 不会尝试加锁，而是自旋等待（`cpu_relax()`）。
- `hrtimer_callback_running()` 无条件返回 true（因为 migration_base 上没有 running 定时器）。

### 3.3 switch_hrtimer_base — 跨 CPU 迁移

源码位置：`kernel/time/hrtimer.c:269-313`

当定时器需要迁移到其他 CPU 时（由 `get_target_base` 决定目标 CPU），执行以下步骤：

```
write_lock(timer->base)           ← 锁住原 base
    ↓
timer->base = &migration_base     ← 标记"正在搬家"
    ↓
unlock(原 base)                   ← 释放原 base 锁
    ↓
lock(新 base)                     ← 获取新 base 锁
    ↓
timer->base = new_base            ← 指到新 base
    ↓
return new_base
```

如果定时器的回调函数正在执行（`hrtimer_callback_running(timer)` 返回 true），则保持原 base 不变，防止在回调进行中迁移。

---

## 4. hrtimer_mode 枚举

源码位置：`include/linux/hrtimer.h:43-65`

```cpp
enum hrtimer_mode {
    HRTIMER_MODE_ABS        = 0x00,   // 绝对时间
    HRTIMER_MODE_REL        = 0x01,   // 相对时间
    HRTIMER_MODE_PINNED     = 0x02,   // 固定在当前 CPU
    HRTIMER_MODE_SOFT       = 0x04,   // softirq 上下文回调
    HRTIMER_MODE_HARD       = 0x08,   // hard IRQ 上下文回调
    HRTIMER_MODE_LAZY_REARM = 0x10,   // 延迟重编程

    HRTIMER_MODE_ABS_PINNED         = HRTIMER_MODE_ABS | HRTIMER_MODE_PINNED,
    HRTIMER_MODE_REL_PINNED         = HRTIMER_MODE_REL | HRTIMER_MODE_PINNED,
    HRTIMER_MODE_ABS_SOFT           = HRTIMER_MODE_ABS | HRTIMER_MODE_SOFT,
    HRTIMER_MODE_REL_SOFT           = HRTIMER_MODE_REL | HRTIMER_MODE_SOFT,
    HRTIMER_MODE_ABS_PINNED_SOFT    = HRTIMER_MODE_ABS_PINNED | HRTIMER_MODE_SOFT,
    HRTIMER_MODE_REL_PINNED_SOFT    = HRTIMER_MODE_REL_PINNED | HRTIMER_MODE_SOFT,
    HRTIMER_MODE_ABS_HARD           = HRTIMER_MODE_ABS | HRTIMER_MODE_HARD,
    HRTIMER_MODE_REL_HARD           = HRTIMER_MODE_REL | HRTIMER_MODE_HARD,
    HRTIMER_MODE_ABS_PINNED_HARD    = HRTIMER_MODE_ABS_PINNED | HRTIMER_MODE_HARD,
    HRTIMER_MODE_REL_PINNED_HARD    = HRTIMER_MODE_REL_PINNED | HRTIMER_MODE_HARD,
};
```

设计为位掩码，可以 OR 组合：
- `ABS` / `REL`：绝对/相对时间模式（互斥）。
- `HARD` / `SOFT`：回调执行上下文（互斥）。
- `PINNED`：禁止定时器迁移。
- `LAZY_REARM`：允许延迟重新编程时钟事件设备。

---

## 5. 核心 API 实现

### 5.1 hrtimer_start_range_ns — 启动/重启定时器

源码位置：`kernel/time/hrtimer.c:1471-1496`

```cpp
void hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim, u64 delta_ns,
                            const enum hrtimer_mode mode)
{
    struct hrtimer_clock_base *base;
    unsigned long flags;

    debug_hrtimer_assert_init(timer);

    // PREEMPT_RT=n 时检查 SOFT bit 与 is_soft 一致
    // PREEMPT_RT=y 时检查 HARD bit 与 is_hard 一致
    if (!IS_ENABLED(CONFIG_PREEMPT_RT))
        WARN_ON_ONCE(!(mode & HRTIMER_MODE_SOFT) ^ !timer->is_soft);
    else
        WARN_ON_ONCE(!(mode & HRTIMER_MODE_HARD) ^ !timer->is_hard);

    base = lock_hrtimer_base(timer, &flags);

    if (__hrtimer_start_range_ns(timer, tim, delta_ns, mode, base))
        hrtimer_reprogram(timer, true);

    unlock_hrtimer_base(timer, &flags);
}
```

**执行流程：**

```
hrtimer_start_range_ns (hrtimer.c:1471)
    │
    ├─ lock_hrtimer_base (hrtimer.c:183)
    │   └─ 获取 timer->base->cpu_base->lock
    │
    ├─ __hrtimer_start_range_ns (hrtimer.c:1355)
    │   │
    │   ├─ 判断是否为当前 CPU (is_local)
    │   ├─ hrtimer_keep_base: 决定是否需要迁移
    │   │   └─ 考虑了 local/pinned/keep_base 等因素
    │   │
    │   ├─ 如果是 REL 模式: tim = ktime_add_safe(tim, current_time)
    │   │
    │   ├─ hrtimer_update_lowres: 低精度时间窗补偿
    │   │
    │   ├─ 如果 keep_base 为 true:
    │   │   └─ remove_and_enqueue_same_base
    │   │       先移除，设置新的过期时间，重新入队
    │   │       跳过重新编程（如果仍是首个到期定时器）
    │   │
    │   ├─ 否则: (需要迁移)
    │   │   ├─ remove_hrtimer: 从原 base 移除
    │   │   ├─ hrtimer_set_expires_range_ns: 设置过期时间
    │   │   ├─ switch_hrtimer_base: 切换到目标 CPU 的 base
    │   │   └─ enqueue_hrtimer: 在新 base 入队
    │   │
    │   └─ 返回是否为首个到期定时器 (first)
    │
    └─ 如果 first && cpu_base == this_cpu_base:
        └─ hrtimer_reprogram: 重新编程时钟事件设备
```

### 5.2 enqueue_hrtimer — 入队 (RB-tree 插入)

源码位置：`kernel/time/hrtimer.c:1104-1122`

```cpp
static bool enqueue_hrtimer(struct hrtimer *timer,
                            struct hrtimer_clock_base *base,
                            enum hrtimer_mode mode, bool was_armed)
{
    // 设置 active_bases 位
    base->cpu_base->active_bases |= 1 << base->index;

    // 标记定时器为 ENQUEUED
    WRITE_ONCE(timer->is_queued, HRTIMER_STATE_ENQUEUED);

    // 插入 RB-tree，该函数返回是否为新的最左节点（即首个到期定时器）
    if (!timerqueue_linked_add(&base->active, &timer->node))
        return false;

    // 更新 expires_next 为新的最小过期时间
    base->expires_next = hrtimer_get_expires(timer);
    return true;
}
```

`timerqueue_linked_add` 的操作流程：

```
timerqueue_linked_add(&base->active, &timer->node)
    │
    ├─ rb_add_cached(&node->node, &head->rb_root, __timerqueue_less)
    │   │
    │   └─ 标准 RB-tree 插入算法
    │       - 从根节点开始比较 key (expires)
    │       - 新节点 < 当前节点 → 走左子树
    │       - 新节点 > 当前节点 → 走右子树
    │       - 找到适当位置后链接并重新平衡（旋转/染色）
    │       - 更新 cached 指针指向新的最左节点
    │
    └─ 返回 bool: 新节点是否为最左节点
```

时间复杂度：O(log n)，其中 n 是当前基准上的定时器数量。

### 5.3 __remove_hrtimer — 出队 (RB-tree 删除)

源码位置：`kernel/time/hrtimer.c:1139-1195`（大致位置）

出队操作执行以下步骤：

```cpp
static void __remove_hrtimer(struct hrtimer *timer,
                             struct hrtimer_clock_base *base,
                             u8 newstate, bool reprogram)
{
    // 1. 从 RB-tree 中删除节点
    timerqueue_linked_del(&base->active, &timer->node);

    // 2. 更新 expires_next
    base_update_next_timer(base);   // hrtimer.c:1124-1129

    // 3. 清除 active_bases 位（如果该基准已空）
    if (!timerqueue_linked_first(&base->active))
        base->cpu_base->active_bases &= ~(1 << base->index);

    // 4. 更新定时器状态
    WRITE_ONCE(timer->is_queued, newstate);

    // 5. 如果需要，重新编程时钟事件设备
    if (reprogram && timer == base->cpu_base->next_timer)
        hrtimer_force_reprogram(base->cpu_base);
}
```

`base_update_next_timer` (hrtimer.c:1124-1129)：

```cpp
static inline void base_update_next_timer(struct hrtimer_clock_base *base)
{
    struct timerqueue_linked_node *next = timerqueue_linked_first(&base->active);
    base->expires_next = next ? next->expires : KTIME_MAX;
}
```

### 5.4 hrtimer_cancel — 取消定时器

源码位置：`kernel/time/hrtimer.c:1643-1655`

```cpp
int hrtimer_cancel(struct hrtimer *timer)
{
    int ret;

    do {
        ret = hrtimer_try_to_cancel(timer);  // 尝试取消

        if (ret < 0)
            hrtimer_cancel_wait_running(timer);  // 等待回调执行完毕
    } while (ret < 0);
    return ret;
}
```

`hrtimer_try_to_cancel` (hrtimer.c:1509-1537) 的返回值：
- **0**：定时器未活跃（未入队且回调未在运行）
- **1**：定时器已从队列中成功移除
- **-1**：回调函数正在执行中，无法立即取消

当返回 -1 时，`hrtimer_cancel` 调用 `hrtimer_cancel_wait_running` 等待回调完成，然后重新尝试取消。

**执行流程：**

```
hrtimer_cancel (hrtimer.c:1643)
    │
    └─ loop:
        ├─ hrtimer_try_to_cancel (hrtimer.c:1509)
        │   │
        │   ├─ 快速检查: hrtimer_active(timer)?
        │   │   └─ 无锁读取 is_queued == ENQUEUED 或 callback_running
        │   │
        │   ├─ lock_hrtimer_base
        │   ├─ 如果回调未运行:
        │   │   └─ remove_hrtimer + 返回 1
        │   └─ 否则返回 -1
        │
        └─ 如果 ret < 0:
            └─ hrtimer_cancel_wait_running
                ├─ 在 PREEMPT_RT 下:
                │   ├─ atomic_inc(&cpu_base->timer_waiters)
                │   ├─ 释放 lock，获取 expiry_lock
                │   ├─ 等待回调释放 expiry_lock
                │   └─ atomic_dec(&cpu_base->timer_waiters)
                │
                └─ 非 PREEMPT_RT 下:
                    通过 schedule_hrtimer 在另一个 CPU 上等待
```

---

## 6. 中断处理路径

### 6.1 hrtimer_interrupt — 高精度中断处理

源码位置：`kernel/time/hrtimer.c:2083-2140`

```cpp
void hrtimer_interrupt(struct clock_event_device *dev)
{
    struct hrtimer_cpu_base *cpu_base = this_cpu_ptr(&hrtimer_bases);
    ktime_t expires_next, now, entry_time, delta;
    unsigned long flags;
    int retries = 0;

    BUG_ON(!cpu_base->hres_active);
    cpu_base->nr_events++;
    dev->next_event = KTIME_MAX;

    raw_spin_lock_irqsave(&cpu_base->lock, flags);
    entry_time = now = hrtimer_update_base(cpu_base);

retry:
    cpu_base->deferred_rearm = true;
    // 阻止远程 CPU 在本地处理过程中排队新定时器
    cpu_base->expires_next = KTIME_MAX;

    // 检查 soft timer 是否到期 → 触发 HRTIMER_SOFTIRQ
    if (!ktime_before(now, cpu_base->softirq_expires_next)) {
        cpu_base->softirq_expires_next = KTIME_MAX;
        cpu_base->softirq_activated = true;
        raise_timer_softirq(HRTIMER_SOFTIRQ);
    }

    // 处理所有到期的 hard timer
    __hrtimer_run_queues(cpu_base, now, flags, HRTIMER_ACTIVE_HARD);

    // 重新评估：可能有长期运行的回调或虚拟化导致暂停
    now = hrtimer_update_base(cpu_base);
    expires_next = hrtimer_update_next_event(cpu_base);
    cpu_base->hang_detected = false;

    if (expires_next < now) {
        if (++retries < 3)
            goto retry;  // 再试一次

        // 挂起检测：3次重试后仍有过期定时器
        delta = ktime_sub(now, entry_time);
        cpu_base->max_hang_time = max_t(unsigned int,
                                         cpu_base->max_hang_time, delta);
        cpu_base->nr_hangs++;
        cpu_base->hang_detected = true;
    }

    hrtimer_interrupt_rearm(cpu_base, expires_next);
    raw_spin_unlock_irqrestore(&cpu_base->lock, flags);
}
```

**中断处理流程图：**

```
clock_event_device 触发中断
        │
        ▼
hrtimer_interrupt (hrtimer.c:2083)
        │
        ├─ raw_spin_lock(&cpu_base->lock)
        ├─ hrtimer_update_base(cpu_base) → now
        ├─ entry_time = now
        │
retry:  ←────────────────────────────────────────┐
        │                                          │
        ├─ deferred_rearm = true                   │
        ├─ expires_next = KTIME_MAX                │
        │   (阻止 remote CPU 入队)                 │
        │                                          │
        ├─ soft timer 到期?                        │
        │   YES → raise_timer_softirq(HRTIMER_SOFTIRQ)
        │   → softirq_activated = true             │
        │                                          │
        ├─ __hrtimer_run_queues(ACTIVE_HARD)       │
        │   │                                      │
        │   ├─ 遍历 active_bases 中每个有活跃     │
        │   │   定时器的 clock_base                 │
        │   │                                      │
        │   ├─ 对每个 base:                        │
        │   │   ├─ 获取最左节点 (expires_next)     │
        │   │   ├─ if expires_next <= now:         │
        │   │   │   ├─ 从树中移除该节点            │
        │   │   │   ├─ base->running = timer        │
        │   │   │   ├─ 释放锁，执行回调             │
        │   │   │   ├─ 重新获取锁                   │
        │   │   │   ├─ 如果回调返回 RESTART:       │
        │   │   │   │   └─ enqueue_hrtimer          │
        │   │   │   └─ base->running = NULL         │
        │   │   └─ else: break (base 内无更多到期) │
        │   │                                      │
        │   └─ 检查是否需要重新编程               │
        │                                          │
        ├─ hrtimer_update_base → now               │
        ├─ expires_next = hrtimer_update_next_event │
        ├─ hang_detected = false                    │
        │                                          │
        ├─ if expires_next < now:                   │
        │   ├─ if ++retries < 3: goto retry ───────┘
        │   └─ else: 记录 hang                      │
        │                                          │
        └─ hrtimer_interrupt_rearm                  │
           │                                        │
           ├─ HRTIMER_REARM_DEFERRED启用:           │
           │   └─ 缓存 expires_next, 设置 TIF_HRTIMER_REARM
           │                                        │
           └─ 否则:                                 │
               └─ hrtimer_rearm (hrtimer.c:2026-2040)
                   └─ tick_program_event(expires_next)
```

### 6.2 hrtimer_run_softirq — Softirq 处理

源码位置：`kernel/time/hrtimer.c:2001-2018`

```cpp
static __latent_entropy void hrtimer_run_softirq(void)
{
    struct hrtimer_cpu_base *cpu_base = this_cpu_ptr(&hrtimer_bases);
    unsigned long flags;
    ktime_t now;

    hrtimer_cpu_base_lock_expiry(cpu_base);
    raw_spin_lock_irqsave(&cpu_base->lock, flags);

    now = hrtimer_update_base(cpu_base);
    __hrtimer_run_queues(cpu_base, now, flags, HRTIMER_ACTIVE_SOFT);

    cpu_base->softirq_activated = false;
    hrtimer_update_softirq_timer(cpu_base, true);

    raw_spin_unlock_irqrestore(&cpu_base->lock, flags);
    hrtimer_cpu_base_unlock_expiry(cpu_base);
}
```

此函数由 `HRTIMER_SOFTIRQ` 触发，在 `open_softirq(HRTIMER_SOFTIRQ, hrtimer_run_softirq)` (hrtimer.c:2502) 中注册。与 hard 路径不同的是：

- 使用 `HRTIMER_ACTIVE_SOFT` 作为掩码，只处理 soft clock base（索引 4–7）。
- 在 `__hrtimer_run_queues` 返回后更新 `softirq_expires_next`。
- 在 PREEMPT_RT 下使用专用的 `softirq_expiry_lock`。

### 6.3 hrtimer_run_queues — 低精度模式 tick 处理

源码位置：`kernel/time/hrtimer.c:2147-2179`

当 `hres_active` 为 false 时（低精度模式），此函数由 `run_local_timers()` (timer.c:2416) 在每个 jiffy tick 中调用，执行与 `hrtimer_interrupt` 类似的过期处理，同时检测是否满足切换到高精度模式的条件（`tick_check_oneshot_change → hrtimer_switch_to_hres`）。

---

## 7. 定时器生命周期状态机

```
            INIT (inactive)
               │
               │  hrtimer_start / hrtimer_start_range_ns
               ▼
            ENQUEUED (在 RB-tree 中)
               │
               ├──────────── 过期 ────────────┐
               │                              │
               │                              ▼
               │                      RUNNING (回调执行中)
               │                         │    │
               │    ┌────────────────────┘    │
               │    │ hrtimer_cancel          │
               │    │ (等待回调完成)          │
               │    ▼                         │
               │  INACTIVE                    │
               │                              │
               │                    回调返回 NORESTART
               │                              │
               └──────────────────────────────┘
                                              │
                           回调返回 RESTART   │
                                              │
                                              ▼
                                       ENQUEUED (自动重新入队)
```

---

## 8. 低精度模式兼容

当系统不支持高精度时钟事件设备时，hrtimer 在低精度模式下运行，通过 `hrtimer_update_lowres` (hrtimer.c:1359 附近调用) 将过期时间对齐到 jiffy 分辨率：

- `CONFIG_TIME_LOW_RES=y` 时，相对定时器添加一个 tick 的偏移。
- 每个 tick 中，`hrtimer_run_queues` 检查是否有 hrtimer 过期。
- 低精度模式下定时器过期时间被截断到 jiffy 边界，精度为 `1/HZ`。

---

## 9. 总结

本文深入剖析了 hrtimer 子系统的三大核心数据结构：

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| enqueue_hrtimer | O(log n) | RB-tree 插入 + 旋转/染色 |
| remove_hrtimer | O(log n) | RB-tree 删除 + 旋转/染色 |
| 获取 expires_next | O(1) | 缓存在 base->expires_next |
| hrtimer_cancel | O(log n) + 等待 | 删除 + 可能等待回调完成 |

调试统计可在 `/proc/timer_list` 中查看每个 CPU 的 nr_events、nr_hangs、max_hang_time 等计数器。

---

## 下一篇文章

[第2篇：时钟基准：MONOTONIC/REALTIME/BOOTTIME/TAI](./02-clock-bases.md) — 深入理解 8 种 clock base 的区别、hard/soft 分流机制、时钟域映射和 hrtimer_forward 的实现。
