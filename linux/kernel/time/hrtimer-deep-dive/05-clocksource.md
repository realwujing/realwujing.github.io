# 第5篇：时钟源 — 从 TSC 到 HPET 的选择与验证

> 源码：`kernel/time/clocksource.c` `kernel/time/timekeeping.c` | 头文件：`include/linux/clocksource.h`

系列目录：[hrtimer 内核源码深度解析](./README.md)

---

## 1. clocksource — 时间的"原始材料"

hrtimer 依赖硬件计数器提供时间。`struct clocksource` 封装了一个硬件计数器及其 `cycles → ns` 的转换参数。

```c
// include/linux/clocksource.h:98-132
struct clocksource {
    u64     (*read)(struct clocksource *cs);     // ★ 读硬件计数器的函数
    u64     mask;                                 // 计数器的位宽掩码
    u32     mult;                                 // 乘数因子
    u32     shift;                                // 右移位数
    u64     max_idle_ns;                          // ★ NO_HZ idle 最大时长限制
    u32     maxadj;                               // NTP 调整的最大范围
    u64     max_cycles;                           // 允许读取的最大 cycle 值
    const char *name;
    struct list_head list;                        // 全局 clocksource 链表节点
    u32     freq_khz;                             // 工作频率（kHz）
    int     rating;                               // ★ 质量评分（越高越好）
    enum clocksource_ids id;
    enum vdso_clock_mode vdso_clock_mode;        // 是否可通过 vDSO 访问
    unsigned long flags;
};
```

**核心公式**：`ns = (cycles * mult) >> shift`

这个乘-移位组合避免了昂贵的整数除法，是实现纳秒级时间读取的关键。`clocks_calc_mult_shift`（`clocksource.c:60`）根据 `freq_khz` 计算最优的 `mult/shift` 组合。

---

## 2. 评分体系 — 内核如何选择最优时钟源

```c
// 评分从高到低（典型值）:
TSC      (Time Stamp Counter)       rating = 300  // x86 CPU 内置，最高精度最低延迟
HPET     (High Precision Event Timer) rating = 250  // 主板自带，稳定但访问慢
ACPI_PM  (ACPI Power Management Timer) rating = 200  // 老旧但通用
jiffies                             rating = 1    // 最后选择，精度仅达 jiffy
```

**选择策略**：内核总是选择 `rating` 最高的可用时钟源。`clocksource_enqueue`（`clocksource.c:25`）将时钟源按 rating 降序插入全局链表，`timekeeping_notify` 自动切换。

---

## 3. 关键 flags

| Flag | clocksource.h line | 含义 |
|------|-------------------|------|
| `CLOCK_SOURCE_VALID_FOR_HRES` | 142 | 支持高精度 hrtimer（纳秒级） |
| `CLOCK_SOURCE_UNSTABLE` | 143 | 时间不稳定（如 TSC 在某些 CPU 上跨核不一致） |
| `CLOCK_SOURCE_WATCHDOG` | 141 | 被其他时钟源监控中 |
| `CLOCK_SOURCE_MUST_VERIFY` | 144 | 必须在 registration 时进行 watchdog 检查 |
| `CLOCK_SOURCE_CONTINUOUS` | 151 | 连续计数器（不会 wrap） |

如果 `CLOCK_SOURCE_UNSTABLE` 被设置，内核会降级到次优时钟源（通常是 HPET）。TSC 在较新的 x86 CPU 上标记为 `CLOCK_SOURCE_STABLE`（稳定的），此时 `max_idle_ns` 可以很大。

---

## 4. Watchdog — 时钟源的"保镖"

TSC 曾经有严重缺陷（跨核漂移、变频不稳定等）。内核的 `clocksource watchdog` 周期性将当前时钟源与另一个独立时钟源交叉验证：

```
每 ~0.5s (WATCHDOG_INTERVAL) → 
  读 TSC → ns1
  读 HPET → ns2
  |ns1 - ns2| > threshold → 标记 TSC 为 UNSTABLE → 降级到 HPET
```

Watchdog 通过 `WATCHDOG_MAX_SKEW`（62.5µs）判断时钟源是否漂移超标。如果连续多次超标，时钟源被标记为不稳定，系统切换到次优时钟源。

---

## 5. max_idle_ns — NO_HZ 的"安全锁"

`max_idle_ns` 限制了 CPU 在 NO_HZ idle 状态（tick 被停止）下可以睡眠的最长时间。如果睡太久，硬件计数器可能 wrap，时间信息丢失。

| 时钟源 | max_idle_ns | 典型限制 |
|--------|-------------|---------|
| TSC (stable) | ~10 小时 | 几乎不限 |
| HPET | ~500ms | 必须在 500ms 内醒来 |
| ACPI_PM | ~300ms | 更短 |

`tick_nohz_stop_tick` 在停止 tick 前先查 `clocksource.max_idle_ns`，计算下次必须醒来的时间。

---

## 6. x86 的三大时钟源

### 6.1 TSC (Time Stamp Counter) — 主力

```c
// x86 的 TSC 实现
static u64 read_tsc(struct clocksource *cs) {
    return rdtsc_ordered();  // RDTSC 指令
}
```

- 延迟极低（~30ns 一次 RDTSC）
- 内置到 CPU，精度受 CPU 频率影响
- 现代 CPU（Intel Ivy Bridge+, AMD Zen+）：**invariant TSC** — 不随变频改变速率
- 仍可能在不同 socket 之间有轻微偏差，但整体上是最优选择

### 6.2 HPET — 备用

```c
// HPET 主计数器通过 MMIO 访问
// 每次读需 ~1µs（相比 TSC 的 30ns 慢 30 倍）
```

- 主板芯片组自带，不需要依赖 CPU
- 极其稳定，极少出现偏差
- 但通过 MMIO 读取，每次需要 PCIe 内存访问，显著高延迟
- 在 TSC 不稳定的系统上自动被选中

### 6.3 ACPI_PM — 遗留

- ACPI 规范定义的一个 32 位定时器
- 极其老旧（从 ACPI 1.0 开始存在）
- 访问方式是 IO 端口操作，极其慢
- 只在没有 TSC 和 HPET 的系统上使用

---

## 7. timekeeping — 从 counter 到"现在几点"

hrtimer 的 `_softexpires` 是 `ktime_t`（纳秒），这个值由 `timekeeping` 子系统维护。`timekeeping` 选一个主时钟源，定期读取其计数器，累加到核心的 `tk_core.timekeeper` 中：

```c
static inline u64 timekeeping_get_ns(void) {
    struct timekeeper *tk = &tk_core.timekeeper;
    u64 cycles = tk->tkr_mono.clock->read(tk->tkr_mono.clock);
    u64 nsec = timekeeping_cycles_to_ns(&tk->tkr_mono, cycles);
    return nsec + tk->tkr_mono.xtime_nsec + tk->xtime_sec * NSEC_PER_SEC;
}
```

`tk_core.timekeeper` 维护了从启动到现在的 MONOTONIC 时间、经 NTP 校正的 REALTIME 时间、BOOTTIME 时间等。`hrtimer_interrupt` 通过 `hrtimer_update_base` 间接调用 `timekeeping` 读取最新时间。

---

## 8. 完整关系图

```
                         ┌──────────────────┐
                         │   用户态/内核态  │
                         │   nanosleep等    │
                         └────────┬─────────┘
                                  │
                         ┌────────▼─────────┐
                         │   hrtimer API    │
                         │  (clock_base[]   │
                         │   RB-trees)      │
                         └────────┬─────────┘
                                  │
              ┌───────────────────┼───────────────────┐
              │                   │                   │
     ┌────────▼────────┐ ┌────────▼────────┐ ┌───────▼─────────┐
     │ hrtimer_interrupt│ │hrtimer_run_sofitrq│ │ tick-sched     │
     │ (硬中断到期)       │ │ (软中断到期)        │ │ (NO_HZ tick)   │
     └────────┬────────┘ └────────┬────────┘ └───────┬─────────┘
              │                   │                   │
              └───────────────────┼───────────────────┘
                                  │
                         ┌────────▼─────────┐
                         │   timekeeping     │
                         │ (tk_core.        │
                         │  timekeeper)     │
                         └────────┬─────────┘
                                  │
                         ┌────────▼─────────┐
                         │   clocksource     │
                         │  (TSC/HPET/PM)   │
                         │  RDTSC 指令/MMIO │
                         └──────────────────┘
```

---

## 9. 系列结语

5 篇文章从 hrtimer 核心数据结构（红黑树、clock_base、cpu_base）→ 4 种时基（MONOTONIC/REALTIME/BOOTTIME/TAI）→ Legacy 时间轮（timer wheel）→ NO_HZ 动态时钟调度 → clocksource 硬件抽象层，完整覆盖了 Linux 内核定时器子系统的设计实现。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-core | 数据结构 + 红黑树 + 中断派发 | `hrtimer_types.h:41`, `hrtimer.c:2083` |
| 02-clock-bases | 8 种时基 + 启动/取消/前推 | `hrtimer_defs.h:38`, `hrtimer.c:1471` |
| 03-timer-wheel | 9 级 64 桶无级联时间轮 | `timer.c:64-139` |
| 04-nohz-tick | NO_HZ + tick_sched 动态开关 | `tick-sched.c:1294/1491/1013` |
| 05-clocksource | TSC/HPET/ACPI_PM + watchdog | `clocksource.h:98`, `clocksource.c:25` |
