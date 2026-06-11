# Linux 内核 hrtimer 深度解析系列

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)

---

## 系列概览

hrtimer（高精度定时器）是 Linux 内核中精度可达纳秒级的定时器子系统，与传统的 timer wheel（jiffies 粒度）互为补充。本系列从数据结构、时钟基准、低精度定时器、动态时钟（dyntick/noHZ）到 clocksource 硬件抽象层，逐层深入解析。

```
                        ┌──────────────────────────────────────────────────┐
                        │                  hrtimer 子系统架构                │
                        └──────────────────────────────────────────────────┘

   用户态 sleep/nanosleep/timerfd         内核态 schedule_timeout/hrtimer_*

           │                                      │
           ▼                                      ▼
   ┌───────────────────────────────────────────────────────────────┐
   │                     hrtimer API                               │
   │  hrtimer_start / hrtimer_start_range_ns / hrtimer_cancel      │
   │  hrtimer_forward / hrtimer_restart / hrtimer_try_to_cancel    │
   └───────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
   ┌───────────────────────────────────────────────────────────────┐
   │              struct hrtimer_cpu_base (per-CPU)                 │
   │              ┌──────────────────────────────────────┐         │
   │              │  lock, cpu, active_bases, hres_active │         │
   │              │  expires_next, softirq_expires_next   │         │
   │              │  next_timer, softirq_next_timer       │         │
   │              └──────────────────────────────────────┘         │
   │                             │                                 │
   │         clock_base[0]  clock_base[1]  ...  clock_base[7]     │
   │        (MONO_HARD)     (REAL_HARD)           (TAI_SOFT)       │
   └───────────┬───────────────┬───────────────────┬───────────────┘
               │               │                   │
               ▼               ▼                   ▼
   ┌───────────────────────────────────────────────────────────────┐
   │              struct hrtimer_clock_base                         │
   │  ┌─────────────────────────────────────────────────┐         │
   │  │  cpu_base, index, clockid, seq, offset           │         │
   │  │  running, expires_next                          │         │
   │  │  active (timerqueue_linked_head = RB-tree root)  │         │
   │  └─────────────┬───────────────────────────────────┘         │
   │                │                                              │
   │    ┌───────────▼───────────┐                                  │
   │    │     Red-Black Tree    │  ← 按 expires 排序               │
   │    │  ┌──────┐ ┌──────┐   │     O(log n) 插入/删除           │
   │    │  │ node │ │ node │   │                                  │
   │    │  └──┬───┘ └──┬───┘   │                                  │
   │    │  ┌──▼──┐ ┌──▼──┐     │                                  │
   │    │  │left │ │right│ ... │                                  │
   │    │  └─────┘ └─────┘     │                                  │
   │    └──────────────────────┘                                  │
   └───────────────────┬───────────────────────────────────────────┘
                       │
          expires_next (最左节点过期时间)
                       │
                       ▼
   ┌───────────────────────────────────────────────────────────────┐
   │              时钟事件设备 (clock_event_device)                 │
   │      程序下一次中断 → hrtimer_interrupt (硬 IRQ)               │
   └───────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
   ┌───────────────────────────────────────────────────────────────┐
   │              hrtimer_interrupt (hrtimer.c:2083)                │
   │                                                                 │
   │  1. 获取当前时间 (hrtimer_update_base)                         │
   │  2. 检查 softirq_expires_next → raise HRTIMER_SOFTIRQ          │
   │  3. __hrtimer_run_queues(HRTIMER_ACTIVE_HARD) → 过期 hard timer│
   │  4. 重试循环 (最多3次) 防止挂死                                │
   │  5. 重新编程时钟事件设备 (rearm)                                │
   └───────────────────────┬───────────────────────────────────────┘
                           │
              ┌────────────▼────────────┐
              │                         │
              ▼                         ▼
   ┌──────────────────────┐  ┌──────────────────────────┐
   │   Hard timer 回调     │  │  HRTIMER_SOFTIRQ          │
   │   (hrtimer_interrupt) │  │  → hrtimer_run_softirq    │
   │   上下文中直接执行     │  │    (hrtimer.c:2001)       │
   │   IRQ 禁用            │  │   softirq 上下文          │
   └──────────────────────┘  │   过期 soft timer 回调     │
                              └──────────────────────────┘
```

## 文章目录

| 篇目 | 标题 | 文件 | 核心内容 |
|------|------|------|----------|
| 第1篇 | 核心数据结构与红黑树操作 | [01-core.md](./01-core.md) | struct hrtimer, clock_base, cpu_base, RB-tree 增删改查, 中断处理 |
| 第2篇 | 时钟基准：MONOTONIC/REALTIME/BOOTTIME/TAI | [02-clock-bases.md](./02-clock-bases.md) | 8 种 clock base, hard vs soft, 时钟域转换, hrtimer_forward |
| 第3篇 | Timer Wheel：传统低精度定时器 | [03-timer-wheel.md](./03-timer-wheel.md) | 9 级轮盘, 无级联设计, HZ 粒度表, 与 hrtimer 对比 |
| 第4篇 | NO_HZ 与动态时钟 (tick-sched) | [04-nohz-tick.md](./04-nohz-tick.md) | dyntick/idle, tick 模拟, sched_timer, clocksource 选择与监控 |

## 关键源文件索引

| 文件 | 描述 | 行数 |
|------|------|------|
| `kernel/time/hrtimer.c` | hrtimer 核心实现 | 2503 |
| `kernel/time/timer.c` | 传统 timer wheel | 2580 |
| `kernel/time/tick-sched.c` | 动态时钟调度 | 1716 |
| `kernel/time/clocksource.c` | clocksource 管理 | 1600 |
| `include/linux/hrtimer_types.h` | hrtimer 类型定义 | 53 |
| `include/linux/hrtimer_defs.h` | 时钟基准与 CPU 基准定义 | 113 |
| `include/linux/hrtimer.h` | hrtimer API 与 mode 枚举 | 357 |
| `include/linux/timer.h` | timer wheel API | 200 |

## 阅读建议

1. **先读第1篇**：理解核心数据结构，这是所有后续内容的基础。
2. **第2篇**：理解 8 种时钟基准的区别和 hard/soft 分流机制。
3. **第3篇**：对比理解 timer wheel 的设计哲学（空间换时间 vs 精确排序）。
4. **第4篇**：理解内核如何在 idle 时停止 tick，以及 clocksource 的选优与监控。
5. 建议边读边在源码中跟踪函数调用链，使用 `git grep` 和 `cscope` 辅助。

## 相关文档

- [Documentation/timers/hrtimers.rst](https://www.kernel.org/doc/Documentation/timers/hrtimers.rst)
- [Documentation/timers/highres.rst](https://www.kernel.org/doc/Documentation/timers/highres.rst)
- [Documentation/timers/NO_HZ.rst](https://www.kernel.org/doc/Documentation/timers/NO_HZ.rst)
- LWN: "A new approach to high-resolution timers" (2006)
- LWN: "The high-resolution timer API" (2006)

---

*本系列基于 Linux v7.1-rc5-26 (commit eb3f4b7426cf)，所有行号均来自真实源码。*
