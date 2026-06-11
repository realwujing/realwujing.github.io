# 第四篇：Hard Lockup (Buddy) — CPU 互检与 miss 阈值

> 源码：`kernel/watchdog_buddy.c` `kernel/watchdog.c` | 头文件：`include/linux/nmi.h`

系列目录：[Hard/Soft Lockup 内核源码深度解析](./README.md)

---

## 1. Buddy 检测器概述

当系统**没有**可用的硬件 PMU 用于 perf NMI hardlockup 检测时，内核回退到纯软件的 **Buddy 检测器**。Buddy 的核心思想：

> **CPU 组成一个环形检测链，每个 CPU 通过 hrtimer 心跳检查下一个 CPU 是否还活着。**

```
检测环 (4 CPU 示例):

  CPU0 ──检查──→ CPU1
   ↑              │
   │              │ 检查
   │              ↓
  CPU3 ←──检查── CPU2
```

- `watchdog_buddy.c` 仅 103 行——极简设计
- 无硬件依赖，纯软件实现
- `miss_thresh = 3`（需要连续 3 次 hrtimer 中断 missed 才报 hard lockup）
- 检测在 `watchdog_timer_fn()` 的 hrtimer 上下文中完成（非 NMI）
- 没有 `regs` 可用——因为不是 NMI 中断

---

## 2. 全局状态

```c
// [watchdog_buddy.c:9]
static cpumask_t __read_mostly watchdog_cpus;

// [watchdog.c:145-150]
static DEFINE_PER_CPU(atomic_t, hrtimer_interrupts);       // hrtimer 心跳计数
static DEFINE_PER_CPU(int, hrtimer_interrupts_saved);      // 上次快照
static DEFINE_PER_CPU(int, hrtimer_interrupts_missed);     // 连续 missed
static DEFINE_PER_CPU(bool, watchdog_hardlockup_warned);   // 去重标志
static DEFINE_PER_CPU(bool, watchdog_hardlockup_touched);  // touch 标志
static unsigned long hard_lockup_nmi_warn;                 // 全CPU BT 互斥
```

`watchdog_cpus` 是 buddy 检测器维护的**"在线的受监控 CPU 集合"**。只有在此集合中的 CPU 才参与互检。这个集合与 `watchdog_cpumask` 不同——后者是用户配置的期望 CPU，而 `watchdog_cpus` 是 buddy 运行时维护的实际在线 CPU。

---

## 3. 初始化 — probe

```c
// [watchdog_buddy.c:22-26]
int __init watchdog_hardlockup_probe(void)
{
    watchdog_hardlockup_miss_thresh = 3;
    return 0;
}
```

与 perf 探测不同，buddy 的 probe **永远成功**。它做的唯一事情是将 `watchdog_hardlockup_miss_thresh` 设为 3——表示需要连续 missed 3 次 `hrtimer_interrupts` 递增才算 hard lockup。

比例关系：

```
hrtimer 每 4s 触发一次
miss_thresh = 3 → 3 × 4s = 12s 后才判定 hard lockup

对比 perf: miss_thresh = 1，但 NMI 周期 ≈ 10s
总体检测延迟相近 (~10-12s)
```

---

## 4. CPU 环形索引

```c
// [watchdog_buddy.c:11-20]
static unsigned int watchdog_next_cpu(unsigned int cpu)
{
    unsigned int next_cpu;

    next_cpu = cpumask_next_wrap(cpu, &watchdog_cpus);
    if (next_cpu == cpu)
        return nr_cpu_ids;   // 自己是唯一在线的CPU → 无可检查对象

    return next_cpu;
}
```

`cpumask_next_wrap` 从 `cpu` 开始，在 `watchdog_cpus` 中找到**下一个**已设置的位。如果 next_cpu 绕回到 `cpu` 自身（说明 `watchdog_cpus` 中只有这个 CPU），返回 `nr_cpu_ids`。

**示例**：

```
watchdog_cpus = {0b1011} = CPU0, CPU1, CPU3 在线

watchdog_next_cpu(CPU0) → CPU1
watchdog_next_cpu(CPU1) → CPU3  (跳过 CPU2)
watchdog_next_cpu(CPU3) → CPU0  (wrap)
watchdog_next_cpu(CPU2) → nr_cpu_ids (CPU2 不在 mask 里)
```

---

## 5. 启用 — watchdog_hardlockup_enable()

```c
// [watchdog_buddy.c:28-60]
void watchdog_hardlockup_enable(unsigned int cpu)
{
    unsigned int next_cpu;

    /*
     * 新 CPU 在在线之前被标记 online，但在 hrtimer 中断首次
     * 运行之前，其他 CPU 检查它会导致误报。
     * 在新 CPU 上 touch watchdog，延迟检查至少 3 个采样周期。
     */
    watchdog_hardlockup_touch_cpu(cpu);

    /*
     * 我们要检查下一个 CPU。如果该 CPU 之前已经在线过，
     * 它的 hrtimer_interrupts_saved 可能不为 0。
     * 在下一个 CPU 上也 touch，避免误报。
     */
    next_cpu = watchdog_next_cpu(cpu);
    if (next_cpu < nr_cpu_ids)
        watchdog_hardlockup_touch_cpu(next_cpu);

    /*
     * 内存屏障：确保在此 CPU 被标记之前，touch 对其他 CPU 可见。
     * 对应端在 watchdog_buddy_check_hardlockup() 中的 smp_rmb()。
     */
    smp_wmb();

    cpumask_set_cpu(cpu, &watchdog_cpus);
}
```

### 为什么要双重 touch？

考虑以下场景：

```
初始状态: watchdog_cpus = {CPU0, CPU1}
检测链: CPU0 → 检查 → CPU1

现在 CPU2 hotplug 上线:

            CPU0                  CPU1                  CPU2
            ====                  ====                  ====
hrtimer:    [kick]                [kick]                [hrtimer 尚未启动]
                                  检查 CPU0             
                                  ↑ 如果此时 CPU2 加入...
```

如果 CPU2 立即被加入 `watchdog_cpus`，CPU1 的 `watchdog_next_cpu(1)` 可能返回 CPU2。但 CPU2 的 `hrtimer_interrupts_saved` 还是初始值 0（尚未有任何 hrtimer tick），CPU1 看到 `hrtimer_interrupts` 未增长 → 误报。

**解决方案**：
1. `watchdog_hardlockup_touch_cpu(cpu)` — 在 CPU2（新 CPU）自身 touch，使其 `watchdog_hardlockup_touched = true`。任何检查 CPU2 的 CPU 看到此标志 → 跳过检查
2. `watchdog_hardlockup_touch_cpu(next_cpu)` — 在 CPU3（CPU2 的下一个 CPU，如果存在）也 touch，防止 CPU2 加入后立即检查 CPU3 时产生误报
3. `smp_wmb()` 后 `cpumask_set_cpu()` — 保证 touch 写入在 CPU 标记为在线之前对其他 CPU 可见

### 内存屏障成对使用

| 写入端 (enable/disable) | 读取端 (buddy_check) |
|--------------------------|----------------------|
| `smp_wmb()` (写屏障) | `smp_rmb()` (读屏障) |

```
enable 端:                          check 端:
  watchdog_hardlockup_touch_cpu()      next_cpu = watchdog_next_cpu()
  smp_wmb()           ────────→        smp_rmb()
  cpumask_set_cpu()                    watchdog_hardlockup_check(next_cpu)
```

保证：如果 `smp_rmb()` 之后 `watchdog_next_cpu()` 返回了新 CPU，那之前必然能看到 `watchdog_hardlockup_touched` 已被设置。

---

## 6. 禁用 — watchdog_hardlockup_disable()

```c
// [watchdog_buddy.c:62-84]
void watchdog_hardlockup_disable(unsigned int cpu)
{
    unsigned int next_cpu = watchdog_next_cpu(cpu);

    /*
     * CPU 下线会改变检测链：之前检查这个 CPU 的 CPU 现在需要跳过它
     * 去检查 next_cpu。如果这个 CPU 刚完成了对 next_cpu 的检查并
     * 更新了 hrtimer_interrupts_saved，然后前一个 CPU 在一个采样
     * 周期内检查它，会触发误报。
     * 在 next_cpu 上 touch 来防止这个。
     */
    if (next_cpu < nr_cpu_ids)
        watchdog_hardlockup_touch_cpu(next_cpu);

    smp_wmb();

    cpumask_clear_cpu(cpu, &watchdog_cpus);
}
```

### 下线时序分析

```
检测链: CPU0 → CPU1 → CPU2 → CPU0

CPU1 即将下线:

                       CPU0                  CPU1                  CPU2
                       ====                  ====                  ====
hrtimer tick:          [kick]                [kick]                
                                             检查 CPU2:
                                             save hrtimer_int(C2)=N
                                             
                        [kick] ← 下一 tick
                        watchdog_next_cpu(0)
                        此时 CPU1 已不在 mask
                        → 返回 CPU2
                        检查 CPU2:
                        hrtimer_int(C2) = N (未变!)
                        → 误报! (因为 CPU1 刚检查过并保存)
```

CPU1 刚对 CPU2 做了 `watchdog_hardlockup_update_reset()`，把 `hrtimer_interrupts_saved(CPU2) = N`。然后 CPU1 下线，CPU0 的检查目标从 CPU1 变为 CPU2，CPU0 读取 `hrtimer_interrupts_saved(CPU2) = N`，当前 `hrtimer_interrupts(CPU2) = N`（还没到下一个 tick）→ 认为 missed → **误报**。

**防护**：`watchdog_hardlockup_touch_cpu(next_cpu)` 在 CPU2 上设置 `touched` 标志，CPU0 检查时发现 `touched` → 跳过。

---

## 7. 互检核心 — watchdog_buddy_check_hardlockup()

```c
// [watchdog_buddy.c:86-103]
void watchdog_buddy_check_hardlockup(int hrtimer_interrupts)
{
    unsigned int next_cpu;

    /* 检查下一个 CPU 是否 hard lockup */
    next_cpu = watchdog_next_cpu(smp_processor_id());
    if (next_cpu >= nr_cpu_ids)
        return;

    /*
     * 确保由于 watchdog_hardlockup_enable()/disable() 引起的
     * watchdog_next_cpu() 更改，能在检查前看到 touch。
     */
    smp_rmb();

    watchdog_hardlockup_check(next_cpu, NULL);
}
```

### 调用链

```
watchdog_timer_fn()              [watchdog.c:795]
  └─ watchdog_hardlockup_kick()  [watchdog.c:199]
       ├─ atomic_inc_return(hrtimer_interrupts)  // 本 CPU 心跳+1
       └─ watchdog_buddy_check_hardlockup()      // 检查下一个 CPU
            [watchdog_buddy.c:86]
            └─ watchdog_hardlockup_check(next_cpu, NULL)
                 [watchdog.c:207]
                 └─ is_hardlockup(next_cpu)      // 下一CPU停跳了?
```

**注意参数**：Buddy 传入的 `regs = NULL`，因为 buddy 检查在 hrtimer 上下文（软中断），不是 NMI。这意味着如果本 CPU 自己发生 hard lockup，`show_regs()` 拿不到精确的寄存器上下文，只能 `dump_stack()`。

---

## 8. 完整判定链路

### 8.1 检测流程图

```
本 CPU hrtimer tick (每 4s) ─── watchdog_timer_fn()
                                     │
                                     ▼
                           watchdog_hardlockup_kick()  [watchdog.c:199]
                                     │
                    ┌────────────────┼────────────────┐
                    │ 本 CPU 心跳+1  │    buddy 检查   │
                    │ atomic_inc(    │                 │
                    │ hrtimer_ints)  │                 │
                    └────────────────┘                 │
                                                       ▼
                                    watchdog_buddy_check_hardlockup()
                                    [watchdog_buddy.c:86]
                                                       │
                                            smp_rmb()   │
                                                       ▼
                                    watchdog_hardlockup_check(next_cpu, NULL)
                                    [watchdog.c:207]
                                                       │
                                           ┌───────────┴────────────┐
                                           │ touched?               │
                                           ├── 是 → reset → return  │
                                           ├── 否                   │
                                           │    is_hardlockup()?    │
                                           │    ├── 否 → return     │
                                           │    └── 是              │
                                           │         pr_emerg      │
                                           │         show_regs/    │
                                           │         dump_stack    │
                                           │         panic?        │
                                           └───────────────────────┘
```

### 8.2 is_hardlockup() 回顾

```c
// [watchdog.c:183-197]
static bool is_hardlockup(unsigned int cpu)
{
    int hrint = atomic_read(&per_cpu(hrtimer_interrupts, cpu));

    // 计数器增长 → CPU 还能处理 hrtimer 中断 → 活着
    if (per_cpu(hrtimer_interrupts_saved, cpu) != hrint) {
        watchdog_hardlockup_update_reset(cpu);
        return false;
    }

    // 计数器未增长 → 累计 missed
    per_cpu(hrtimer_interrupts_missed, cpu)++;
    if (per_cpu(hrtimer_interrupts_missed, cpu) % watchdog_hardlockup_miss_thresh)
        return false;

    return true;  // missed 累计到 miss_thresh=3 → HARD LOCKUP!
}
```

### 8.3 检测时间线 (Buddy)

```
假设 CPU1 在 T=0 之后锁死：

本CPU tick:   T=0    T=4     T=8     T=12    T=16
hrtimer_int   CPU1=5  CPU1=5  CPU1=5  CPU1=5  CPU1=5
                ↓                  ↓
CPU0→CPU1:    check   check   check   check   check
missed:        0→1     1→2     2→0!    0→1     1→2
             %3=1≠0  %3=2≠0  判定！  %3=1≠0  %3=2≠0
                                   ↑ BUG!

判定时间：从 CPU1 锁死开始需 3×4=12s
```

`miss%3==0` 时返回 `true`——3 次连续 missed。如果中途 `hrtimer_interrupts` 增长（CPU 恢复），`watchdog_hardlockup_update_reset` 将 missed 重置为 0。

---

## 9. 多种 CPU 拓扑下的检测行为

### 9.1 正常 4 CPU 环

```
watchdog_cpus = {0b1111} = {CPU0, CPU1, CPU2, CPU3}

检测链: CPU0 → CPU1 → CPU2 → CPU3 → CPU0 (wrap)

每 4s:
  CPU0 hrtimer: inc(CPU0.ints), buddy_check(CPU1)
  CPU1 hrtimer: inc(CPU1.ints), buddy_check(CPU2)
  CPU2 hrtimer: inc(CPU2.ints), buddy_check(CPU3)
  CPU3 hrtimer: inc(CPU3.ints), buddy_check(CPU0)
```

### 9.2 奇数 CPU：最后一个检查谁？

```
watchdog_cpus = {0b111} = {CPU0, CPU1, CPU2}

检测链: CPU0 → CPU1 → CPU2 → CPU0 (wrap)
```

每个 CPU 都有且只有一个"被检查对象"。环形结构天然处理奇数和偶数 CPU。

### 9.3 单 CPU

```
watchdog_cpus = {0b1} = {CPU0}

watchdog_next_cpu(CPU0) = nr_cpu_ids
→ buddy_check_hardlockup: return (无可检查对象)
→ 单 CPU 系统下 buddy 不检测 hard lockup (但也无所谓——无法调度就是 soft lockup 了)
```

### 9.4 CPU 下线中间状态

```
初始: {CPU0, CPU1, CPU2}, CPU1 正在下线

时刻1: CPU1 disable → touch(CPU2) → smp_wmb() → clear_cpu(CPU1)
时刻2: CPU0 tick → watchdog_next_cpu(0) → CPU2 → check CPU2
        看到 touched → reset → 不报

此后:
  检测链: CPU0 → CPU2 → CPU0 (跳过 CPU1)
```

---

## 10. arch_touch_nmi_watchdog()

```c
// [watchdog.c:152-163]
notrace void arch_touch_nmi_watchdog(void)
{
    /*
     * 使用 __raw 因为某些调用路径允许抢占。
     * 如果抢占可用，中断也应该可用——此时不需要关心
     * watchdog 是否会超时。
     */
    raw_cpu_write(watchdog_hardlockup_touched, true);
}
EXPORT_SYMBOL(arch_touch_nmi_watchdog);
```

这是在**本 CPU** 上设置 `touched` 标志，表示"我知道我要做长时间操作，别误报"。常用于：
- 内核调试器入口
- kdb/kgdb 断点
- 长时间 disable interrupt 的代码段

```c
// [watchdog.c:165-168]
void watchdog_hardlockup_touch_cpu(unsigned int cpu)
{
    per_cpu(watchdog_hardlockup_touched, cpu) = true;
}
```

跨 CPU 版本——在**指定的其他 CPU** 上设置 `touched` 标志。被 enable/disable 路径使用。

---

## 11. 硬锁报告 (watchdog.c:207-294 对 buddy 的执行路径)

```
watchdog_hardlockup_check(cpu, regs=NULL)
  │
  ├─ touched? → 是 → reset → return
  │
  ├─ is_hardlockup(cpu)? → 否 → return
  │
  ├─ scx_hardlockup(cpu)? → 是 → return
  │
  ├─ watchdog_hardlockup_warned(cpu)? → 是 → return (去重)
  │
  ├─ pr_emerg("CPU%u: Watchdog detected hard LOCKUP on cpu %u")
  │   this_cpu=报告者, cpu=受害者
  │
  ├─ print_modules() + print_irqtrace_events(current)
  │
  ├─ cpu == this_cpu?
  │   ├─ 是: show_regs(regs)  // regs=NULL → dump_stack()
  │   └─ 否: trigger_single_cpu_backtrace(cpu)
  │
  ├─ 全 CPU backtrace (可选)
  │
  └─ hardlockup_panic? → nmi_panic(NULL, "Hard LOCKUP")
     注意: regs 为 NULL → nmi_panic 可能用其他方法获取上下文
```

Buddy 模式下 `regs = NULL` 的后果：
- `show_regs(regs)` 退化为 `dump_stack()`（只有调用栈，没有寄存器值）
- `nmi_panic(regs, ...)` 退化为普通 `panic()`
- 诊断信息比 perf 模式少——这是 buddy 的固有局限

---

## 12. 与 perf 检测器的交互

buddy 和 perf **互斥**——一个内核镜像只使用一种 hardlockup 检测器。它们都通过 `watchdog_hardlockup_probe()` weak 函数接入，但具体使用哪个由编译时 `CONFIG_HARDLOCKUP_DETECTOR_PERF` / `CONFIG_HARDLOCKUP_DETECTOR_BUDDY` Kconfig 决定。

共享的代码路径在 `watchdog.c`：

```c
// [watchdog.c:143] CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER
// 两个检测器共享此配置——都依赖 hrtimer_interrupts 计数

// [watchdog.c:309-311] weak 函数声明
void __weak watchdog_hardlockup_enable(unsigned int cpu) { }
void __weak watchdog_hardlockup_disable(unsigned int cpu) { }

// [watchdog.c:296-300] 当 CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER
// 未开启时，kick 也是空函数
#if !defined(CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER)
static inline void watchdog_hardlockup_kick(void) { }
#endif
```

编译时选择关系：

```
CONFIG_HARDLOCKUP_DETECTOR
  ├─ CONFIG_HARDLOCKUP_DETECTOR_PERF    → watchdog_perf.c   (启用)
  │   └─ watchdog_hardlockup_probe()   → perf 版本
  │   └─ watchdog_hardlockup_enable()  → perf 版本
  │   └─ watchdog_hardlockup_disable() → perf 版本
  │
  └─ CONFIG_HARDLOCKUP_DETECTOR_BUDDY   → watchdog_buddy.c  (启用)
      └─ watchdog_hardlockup_probe()   → buddy 版本
      └─ watchdog_hardlockup_enable()  → buddy 版本
      └─ watchdog_hardlockup_disable() → buddy 版本
```

---

## 13. 完整场景走查

### 场景：CPU2 跑飞，中断被关闭

```
初始: {CPU0, CPU1, CPU2, CPU3} 全部在线

时间轴:

T=0s  各 CPU hrtimer:
        CPU0: ints{0→1}, buddy_check CPU1 → CPU1 OK
        CPU1: ints{0→1}, buddy_check CPU2 → CPU2 OK
        CPU2: ints{0→1}, buddy_check CPU3 → CPU3 OK
        CPU3: ints{0→1}, buddy_check CPU0 → CPU0 OK

       ⚡ CPU2 进入 spin_lock_irqsave 死循环，中断被关闭

T=4s  各 CPU hrtimer:
        CPU0: ints{1→2}, buddy_check CPU1 → CPU1 OK
        CPU1: ints{1→2}, buddy_check CPU2
              → hrtimer_int(CPU2)=1, saved=1 → 未增长
              → missed(CPU2)=0→1, 1%3=1≠0 → 暂不报
        CPU2: **中断关闭，hrtimer 无法触发**
              → hrtimer_int(CPU2) 仍为 1
        CPU3: ints{1→2}, buddy_check CPU0 → CPU0 OK

T=8s  各 CPU hrtimer:
        CPU0: ints{2→3}, buddy_check CPU1 → CPU1 OK
        CPU1: ints{2→3}, buddy_check CPU2
              → hrtimer_int(CPU2)=1, saved=1 → 未增长
              → missed(CPU2)=1→2, 2%3=2≠0 → 暂不报
        CPU2: **仍卡死**
        CPU3: ints{2→3}, buddy_check CPU0 → CPU0 OK

T=12s 各 CPU hrtimer:
        CPU0: ints{3→4}, buddy_check CPU1 → CPU1 OK
        CPU1: ints{3→4}, buddy_check CPU2
              → hrtimer_int(CPU2)=1, saved=1 → 未增长
              → missed(CPU2)=2→3, 3%3=0 → BINGO!
              → 判定 HARD LOCKUP on CPU2
              → pr_emerg("CPU1: Watchdog detected hard LOCKUP on cpu 2")
              → trigger_single_cpu_backtrace(CPU2) // 尝试获取CPU2的栈
              → hardlockup_panic? (默认0，不 panic)
              → watchdog_hardlockup_warned(CPU2) = true
        CPU2: **仍卡死**
        CPU3: ints{3→4}, buddy_check CPU0 → CPU0 OK

T=16s 各 CPU hrtimer:
        CPU0: ints{4→5}, buddy_check CPU1 → CPU1 OK
        CPU1: ints{4→5}, buddy_check CPU2
              → is_hardlockup(CPU2): 仍卡死
              → 但 watchdog_hardlockup_warned(CPU2)=true → return (去重)
        ...
```

---

## 14. Buddy 优缺点总结

| 方面 | 评价 |
|------|------|
| **硬件依赖** | 无——适合没有 PMU 或 NMI 的架构 (ARM, RISC-V 等) |
| **代码复杂度** | 极低——`watchdog_buddy.c` 仅 103 行 |
| **检测延迟** | 12s (默认)，比 perf 的 ~10s 略长 |
| **诊断精度** | 较低——`regs=NULL`，无寄存器快照 |
| **CPU 下线安全性** | 通过 smp_rmb/smp_wmb 精心处理竞态 |
| **单 CPU 系统** | 不检测 hard lockup（也无法检测——环破环） |
| **虚机误报** | 无此问题——基于 hrtimer 而非 PMU cycles |
| **多 CPU 一致性** | 环形互检，任意 CPU 卡死都能被另一个 CPU 查到 |
| **与 softlockup 耦合** | 在 `watchdog_timer_fn` 的 hrtimer 上下文中运行，与 softlockup 检测共享同一 tick |

---

## 15. 四篇文章总结

```
┌────────────────────────────────────────────────────────────────┐
│                     Lockup Detector 全景                        │
├────────────┬───────────────────┬────────────────────────────────┤
│  层次      │  机制              │  判定                          │
├────────────┼───────────────────┼────────────────────────────────┤
│ 初始化     │ lockup_detector_  │ 硬件探测 → 配置 → hrtimer启    │
│            │ init → setup()    │ 动 → sysctl 注册               │
├────────────┼───────────────────┼────────────────────────────────┤
│ Soft Lockup│ hrtimer →         │ softlockup_fn 能否被调度?      │
│            │ stop_one_cpu      │ touch_ts 是否过期?             │
│            │ _nowait(soft-     │ now - touch_ts > 2×thresh?     │
│            │ lockup_fn)        │                                │
├────────────┼───────────────────┼────────────────────────────────┤
│ Hard Lockup│ Perf NMI 溢出     │ hrtimer_interrupts 计数停止?   │
│ (Perf)     │ → callback        │ missed % 1 == 0?               │
│            │                   │                                │
│ Hard Lockup│ buddy 环形互检    │ hrtimer_interrupts 计数停止?   │
│ (Buddy)    │ → buddy_check     │ missed % 3 == 0?               │
├────────────┼───────────────────┼────────────────────────────────┤
│ CPU Hotplug│ lockup_detector_  │ 上线: enable, 下线: disable     │
│            │ online/offline_cpu │ buddy: 双重touch + smp_wmb     │
├────────────┼───────────────────┼────────────────────────────────┤
│ /proc 接口 │ sysctl             │ watchdog, nmi_watchdog,        │
│            │                   │ soft_watchdog, thresh, cpumask  │
└────────────┴───────────────────┴────────────────────────────────┘
```

### 关键源文件索引

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| `kernel/watchdog.c` | 1400 | lockup 检测核心：init、softlockup、is_hardlockup、sysctl |
| `kernel/watchdog_perf.c` | 314 | perf NMI hardlockup 检测器 |
| `kernel/watchdog_buddy.c` | 103 | buddy 环形互检 hardlockup 检测器 |
| `include/linux/nmi.h` | 233 | 公共 API、位掩码常量 |
| `init/main.c:1693` | — | lockup_detector_init() 调用点 |

---

## 下一篇文章

您已读完整个系列。返回 [目录](./README.md) 查看概览。
