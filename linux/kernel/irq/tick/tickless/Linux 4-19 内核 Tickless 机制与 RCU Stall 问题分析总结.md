# Linux 4.19 内核 Tickless 机制与 RCU Stall 问题分析总结

## 问题描述

### 1. 内核版本及环境信息

```bash
uname -a

Linux gzinf-kunpeng-55e235e17e57 4.19.90-2102.2.0.0068.ctl2.aarch64 #1 SMP Tue Aug 22 10:53:53 UTC 2023 aarch64 aarch64 aarch64 GNU/Linux
```

---

### 2. 内核启动参数

```bash
cat /proc/cmdline

BOOT_IMAGE=(hd0,3)/vmlinuz-4.19.90-2102.2.0.0068.ctl2.aarch64 root=UUID=216fa54f-9345-4f9a-8fe2-dc3a44db7775 ro console=tty0 pci=realloc pciehp.pciehp_force=1 biosdevname=0 net.ifnames-0 console=ttyS0,115200n8 crashkernel=1024M,high smmu.bypassdev=0x1000:0x17 smmu.bypassdev=0x1000:0x15 video=efifb:off
```

---

### 3. 内核配置相关

```bash
grep -E "CONFIG_HZ|CONFIG_HIGH_RES_TIMERS|CONFIG_NO_HZ" /boot/config-$(uname -r)

# CONFIG_HZ_PERIODIC is not set
CONFIG_HIGH_RES_TIMERS=y
# CONFIG_HZ_100 is not set
CONFIG_HZ_250=y
# CONFIG_HZ_300 is not set
# CONFIG_HZ_1000 is not set
CONFIG_HZ=250
CONFIG_NO_HZ_COMMON=y
# CONFIG_NO_HZ_IDLE is not set
CONFIG_NO_HZ_FULL=y
CONFIG_NO_HZ=y
```

---

### 4. 当前时钟源

```bash
cat /sys/devices/system/clocksource/clocksource0/current_clocksource

arch_sys_counter
```

---

### 5. CPU Tickless 状态

```bash
grep 'tick_stopped' /proc/timer_list

#部分结果（部分截取）：

.tick_stopped   : 1
.tick_stopped   : 1
...
.tick_stopped   : 0
.tick_stopped   : 1
...
```

说明有些 CPU 已进入 tickless（tick_stopped:1），部分尚未停止调度 tick。

* 通过 `CONFIG_NO_HZ` 及 `CONFIG_NO_HZ_FULL` 启用，支持在 CPU 空闲时关闭周期调度 tick，减少无用中断，提高能效。
* **CPU idle 或运行非内核态任务时，调度 tick 会停止（tickless），只保留必要定时器。**
* 必要保留的定时器主要包括：

  * **hrtimer**：用于高精度定时任务。
  * **RCU 定时器**：保障内核同步和回调机制。
  * **watchdog 定时器**：系统看门狗，防止死锁。

---

### 6. 查看定时器详细状态（CPU0为例）

```bash
cat /proc/timer_list | grep -A10 'cpu: 0'
```

部分结果：

```
cpu: 0
 clock 0:
  .base:       0000000059048cdc
  .index:      0
  .resolution: 1 nsecs
  .get_time:   ktime_get
  .offset:     0 nsecs
active timers:
 #0: <00000000410c4969>, tick_sched_timer, S:01
 # expires at 9855148276000000-9855148276000000 nsecs [in 97383564 to 97383564 nsecs]
 #1: <00000000e2898855>, kvm_bg_timer_expire, S:01
```

其中 `tick_sched_timer` 是传统调度周期中断。

---

### 7. 现象与问题

* CPU idle 后进入 tickless，`.tick_stopped` 为 1。
* 但出现 RCU stall 和 rcu_sched 被饿死60秒，甚至 rcu hard lockup。
* 该现象可能由深度睡眠导致时钟源暂停或失效，hrtimer 无法触发软中断。
* 导致 RCU 软中断得不到执行，内核同步受阻。

---

### 8. 关闭 tickless 试验（临时）

* 在 grub 启动参数添加 `nohz=off`
* 运行后，`/proc/timer_list` 中 `.tick_stopped` 应全为 0，表示调度 tick 不被停止。
* 检查系统稳定性，若无 RCU stall，说明 tickless 相关导致问题。

---

### 9. 传统时钟中断与高精度定时器（hrtimer）配合

* `sched tick` 以固定频率产生调度中断，驱动软中断执行 RCU、调度等。
* `hrtimer` 用于 tickless 模式替代传统 tick，精准调度定时任务。
* 两者结合，在 idle 及用户态运行时降低中断频率，节省功耗。
* 但若硬件时钟源异常或深度睡眠影响 hrtimer，软中断无法及时触发，导致 RCU stall。

---

#### 关于 hrtimer 与传统调度 timer（sched_timer）的关系

* **hrtimer（高精度定时器）** 启用后，
  它会 **代替传统的低精度周期定时器（如 jiffies-based timer）来调度高精度任务**，
  例如精确的超时和延时需求。

* 但是，**sched_timer（调度周期 timer）并不会完全被移除**，
  它依然保留着调度器基本的调度节奏控制作用，
  特别是在非 tickless 模式或 CPU 活跃状态下的调度周期维护。

* 在 **tickless 模式下（NO_HZ）**，sched_timer 可能会被停止（tick_stopped=1），
  但它仍作为“备胎”存在于内核中，方便恢复调度 tick，
  以及处理某些必须依赖传统周期 tick 的功能。

* hrtimer 和 sched_timer 是 **协同工作的**，
  保证调度的准确性和系统响应的灵活性。

---

## ⏱️ hrtimer 与传统 tick 定时器的协同机制（核心讲解）

### ✅ **一、传统低精度定时器机制（timer wheel）**

* 依赖 **周期性 tick 中断** 驱动（来源于 CONFIG_HZ 设置）。
* 每次 tick 会推进 `jiffies`，并扫描当前 slot 触发 callback。
* 特点：**粒度较粗（4ms）、延迟高、效率高（适合大批任务）**。

---

### ✅ **二、高精度定时器 hrtimer 的引入**

* 由每个 CPU 的 `hrtimer_cpu_base` 管理，用红黑树存储。
* 可以精确到纳秒级的定时中断。
* 定时器硬件只能同时服务一个 timeout → 选择最早的 hrtimer 事件作为定时触发点。

---

### ✅ **三、hrtimer 如何模拟“tick”以协同工作？**

> hrtimer 替代了部分传统 timer，但仍保留了 tick 的周期性角色：`sched_timer`。

#### 1. **sched_timer 是一个周期性的 hrtimer，用于模拟每个 tick**

* 其超时值设置为 `1 / CONFIG_HZ = 4ms`。
* 触发后会：

  * 调用 `update_process_times()`，驱动调度器、进程统计等。
  * `update_wall_time()`，驱动时钟更新。
  * 然后 **再次插入下一个 tick 的 hrtimer**（即周期性重插）。

#### 2. **如果系统空闲，可以关闭 tick（tickless 模式）**

* 在你系统中，大量 `.tick_stopped: 1` → 多核已启用 **动态 tick 停止**（NOHZ）。
* 系统会判断未来是否有近期任务要做，如果没有，就不设置下一个 sched_timer。
* 等到下一个明确事件（比如 hrtimer 或唤醒）再启动 tick。

#### 3. **如果有 hrtimer 到期时间更早，sched_timer 会被推后**

* hrtimer 的红黑树中调度的是“最早到期时间”的事件。
* 比如用户设定的 2ms 超时任务会抢占原本的 sched_timer 设置。

---

### ✅ **四、协同机制小结：**

| 类型                       | 驱动来源               | 精度              | 应用场景              |
| ------------------------ | ------------------ | --------------- | ----------------- |
| `jiffies` / `timer_list` | 周期 tick（低精度）       | 毫秒级（CONFIG_HZ） | 大量任务，性能友好         |
| `sched_timer`            | hrtimer 驱动的模拟 tick | 纳秒级，但周期性为 4ms   | 调度、统计、timekeeping |
| 其他 `hrtimer`             | 独立设置、单次/周期任务       | 纳秒级             | 睡眠唤醒、定时器精确唤醒      |

当前系统的行为是：

* 在不需要时**停掉 tick 中断**（省电、减少上下文切换）；
* 依赖 `hrtimer` 管理所有 timer，包括模拟 tick；
* 保留 `CONFIG_HZ=250`，tick 模拟周期为 4ms，用于 fallback 路径。

---

## 那当前的配置下cpu idle状态默认会进入tickless？

是的，根据提供的内核配置和系统信息，**当前 CPU 在 idle 状态下默认确实会进入 tickless 模式（即停止周期性 tick 中断）**。下面是分析依据：

---

### ✅ 一、配置项分析

```bash
CONFIG_NO_HZ_COMMON=y
CONFIG_NO_HZ_FULL=y
CONFIG_NO_HZ=y
# CONFIG_NO_HZ_IDLE is not set
```

* `CONFIG_NO_HZ=y`：说明启用了 tickless 支持；
* `CONFIG_NO_HZ_COMMON=y`：是 `NO_HZ_IDLE` 和 `NO_HZ_FULL` 的公共基础；
* `CONFIG_NO_HZ_FULL=y`：表示系统**启用了完整 tickless 模式**（NO_HZ_FULL），用于用户态；
* `# CONFIG_NO_HZ_IDLE is not set`：表示**没有使用 idle 专用 tickless 模式**，因为被 `FULL` 替代。

> ✅ `NO_HZ_IDLE` 是为了 idle 状态减少 tick；
>
> ✅ `NO_HZ_FULL` 是为了用户态也能停 tick，更彻底。

---

### ✅ 二、实际运行状态

```bash
.tick_stopped : 1  ← 代表该 CPU 停止 tick（tickless）
```

这表明大多数 CPU 已经**实际处于 tickless 状态**。

而 `.tick_stopped : 0` 的几个 CPU 可能当前正在内核态，或者运行着内核线程（如 ksoftirqd、rcu、kworker），需要保留调度 tick。

---

### ✅ 三、CPU idle 默认行为（NO_HZ_FULL 下）

> ✅ **在 NO_HZ_FULL 模式下，非内核态、idle CPU 会自动停掉调度 tick**，只保留必要的定时器（如 hrtimer、RCU、watchdog）。

除非：

1. 正在运行内核线程
2. 发生频繁中断
3. 有 `isolcpus` 没设置导致不允许 full tickless

---

### ✅ 四、总结

| 条目                       | 当前配置是否满足                   |
| ------------------------ | -------------------------- |
| `CONFIG_NO_HZ_FULL`      | ✅ 启用                       |
| `sched_tick` 是否默认关闭      | ✅ 在 idle CPU 和用户态 CPU 默认关闭 |
| `.tick_stopped=1` 多数 CPU | ✅ 是                        |
| idle 是否默认进入 tickless     | ✅ 是（只保留 hrtimer/RCU）       |

---

如需进一步验证，可以运行：

```bash
cat /proc/timer_list | grep -A10 'cpu: 0'
```

观察是否只有极少数定时器（如 `tick_sched_timer` 和 `rcu`）在活动，说明 CPU 正在运行于 tickless 状态。

---

## 当前的系统在调度器选中idle时，也会进入tickless?

**当前系统在调度器选中 idle 任务时，会自动进入 tickless 模式**（也称为 *idle tickless*）。

---

### 🔍 依据如下

#### ✅ 当前的内核配置（来自 `/boot/config-$(uname -r)`）

```bash
CONFIG_NO_HZ_COMMON=y
# CONFIG_NO_HZ_IDLE is not set
CONFIG_NO_HZ_FULL=y
CONFIG_NO_HZ=y
```

这说明：

* 系统启用了 `NO_HZ_FULL` 模式；
* `NO_HZ_IDLE` 虽未单独启用，但 `NO_HZ_FULL` 包含了对 idle tickless 的支持；
* **只要某个 CPU 没有被 `nohz_full=` 参数隔离出去，它就默认工作在 idle tickless 模式**。

---

### 🧠 内核逻辑说明

内核中 tickless 支持主要分为两部分：

| 模式               | 启动条件                      | 应用范围                 |
| ---------------- | ------------------------- | -------------------- |
| **NO_HZ_IDLE** | 默认对所有 CPU 启用              | CPU 进入 idle 时停止 tick |
| **NO_HZ_FULL** | 需要配置 `nohz_full=` 并隔离 CPU | CPU 在用户态时也可停 tick    |

你的内核配置中启用了 `NO_HZ_FULL`，则说明：

* **没有被隔离的 CPU 默认运行在 idle tickless 模式（与 `NO_HZ_IDLE` 行为一致）**；
* 被隔离的 CPU（通过内核命令行 `nohz_full=` 配置）则支持用户态停 tick。

---

### ✅ 举个例子

之前运行的：

```bash
cat /proc/timer_list | grep 'tick_stopped'

# 输出中大多数 CPU 是：

.tick_stopped : 1
```

说明当前这些 CPU 已经进入 tickless 状态，而这在没有 `nohz_full=` 参数的情况下，通常就是因为这些 CPU 当前 idle，调度器选择了 idle 任务。

---

### ✅ 结论

> **是的，系统中未被 `nohz_full` 隔离的 CPU，只要调度器选择了 idle 任务（即进入空闲），就会自动停止调度用 tick，进入 tickless 模式。**

这正是 idle tickless 的标准行为。

如需进一步分析哪些 CPU 被 nohz_full 隔离，可以查看：

```bash
cat /sys/devices/system/cpu/nohz_full
```

或者确认内核启动参数是否包含：

```bash
nohz_full=1-15  # 举例
```

## 只保留必要的定时器（如 hrtimer、RCU、watchdog），这三种保留的定时器讲一下？

在 `NO_HZ_FULL` 模式下，系统尽量停掉周期性调度 tick，但仍**必须保留少量关键定时器**，以保障内核基本功能的正常运行。你提到的 `hrtimer`、`RCU`、`watchdog` 是典型的三种**不能省略**的定时器，下面分别讲清楚它们的作用和运行机制：

---

### ① **hrtimer（高精度定时器）**

#### 🔹 作用

* 用于替代旧的 `timer_list`，提供纳秒级定时精度。
* 支撑周期性任务（如 `sched_tick`）、单次定时器、延迟唤醒等功能。
* 是 tickless 模式中**最核心的定时器框架**，用于调度精度更高的事件。

#### 🔹 特点

* 每个 CPU 有独立的 `hrtimer_cpu_base`。
* 使用红黑树管理定时器，按时间排序，优先触发最近的定时器。
* 硬件层通过 `clockevent` 设置唤醒时间。

#### 🔹 举例

* 睡眠中的进程设置了 `nanosleep()` 或 `clock_nanosleep()`。
* 用户态运行时需要周期性唤醒，如 perf sampling、周期 timerfd。
* 替代 `sched_tick`，精确控制重新调度时间点。

---

### ② **RCU（Read-Copy Update）定时器**

#### 🔹 作用

* 确保并发读写情况下内核数据结构的一致性。
* 在写操作时，RCU 需要等待所有 CPU 经过一个 *“RCU grace period”*，即所有旧版本读操作完成之后，才能释放旧数据。

#### 🔹 tickless 下的难点

* 如果一个 CPU 一直在用户态、无中断、无调度，RCU 就**无法确认它是否已经完成了读操作**。
* 所以，即使在 `NO_HZ_FULL` 模式下，RCU 也必须设置**周期性定时器**，适时检查 CPU 是否“Quiescent”（安静态）。

#### 🔹 技术手段

* 使用 **RCU callback timer** 和 `rcu_sched_clock_irq()` 来主动触发检查。
* 或者在 kernel exit to user 模式下调用 `rcu_user_exit()` 让 RCU 知道可以推进 grace period。

---

### ③ **watchdog（看门狗定时器）**

#### 🔹 作用

* 检测 CPU 是否卡死或长时间无响应。
* 常用于软锁死（soft lockup）、硬锁死（hard lockup）等 bug 的自动触发检测。

#### 🔹 特点

* 定时器周期性触发 watchdog 检查函数（如 `watchdog_timer_fn()`）。
* 检查当前 CPU 的调度器心跳、软中断等活动迹象。
* 如果发现某个 CPU 长时间没有响应（如 N 秒无调度），就触发 BUG/WARNING。

#### 🔹 在 NO_HZ_FULL 模式中

* 某些 watchdog 功能会主动在用户态暂停；
* 但 CPU 一旦进入内核态或中断上下文，watchdog 会继续运作；
* 可以通过 `watchdog_thresh` 配置超时时间。

---

### ✅ 总结表

| 定时器      | 用途                          | 是否每 CPU 保留 | 是否依赖 hrtimer | 与 tickless 的关系                   |
| -------- | --------------------------- | ---------- | ------------ | -------------------------------- |
| hrtimer  | 纳秒级通用定时器，支撑所有延时/周期事件        | 是          | 本身就是         | Tickless 的核心，替代 tick             |
| RCU      | 管理内核并发数据一致性，推进 grace period | 是          | 是            | 为推进 RCU 状态，tickless 中必须保留        |
| watchdog | 防止软/硬死锁，保障内核可靠性             | 是          | 是            | Tickless 中 watchdog 会适度放宽但保留监控能力 |

---

## rcu_needs_cpu() 函数

```c
// vim kernel/rcu/tree_plugin.h +1476

1450 /*
1451  * This code is invoked when a CPU goes idle, at which point we want        // 这段代码在 CPU 进入空闲状态时调用
1452  * to have the CPU do everything required for RCU so that it can enter      // 目标是让 CPU 完成所有 RCU 所需操作，从而可以安全进入
1453  * the energy-efficient dyntick-idle mode.  This is handled by a            // 高能效的 dyntick-idle 模式（tick 停止）
1454  * state machine implemented by rcu_prepare_for_idle() below.               // 这一行为由 rcu_prepare_for_idle() 实现的状态机控制
1455  *
1456  * The following three proprocessor symbols control this state machine:     // 以下三个预处理符号控制该状态机
1457  *
1458  * RCU_IDLE_GP_DELAY gives the number of jiffies that a CPU is permitted    // RCU_IDLE_GP_DELAY 表示 CPU 在有挂起回调的情况下
1459  *  to sleep in dyntick-idle mode with RCU callbacks pending.  This         // 允许处于 dyntick-idle 模式的最大 jiffies 数
1460  *  is sized to be roughly one RCU grace period.  Those energy-efficiency   // 通常设置为一个 RCU 宽限期的时长
1461  *  benchmarkers who might otherwise be tempted to set this to a large      // 警告：不要将其设置得过大（为了省电）
1462  *  number, be warned: Setting RCU_IDLE_GP_DELAY too high can hang your     // 否则可能导致系统 hang 住
1463  *  system.  And if you are -that- concerned about energy efficiency,       // 如果你极度关注能效
1464  *  just power the system down and be done with it!                         // 那直接关机可能更有效 :)
1465  * RCU_IDLE_LAZY_GP_DELAY gives the number of jiffies that a CPU is         // RCU_IDLE_LAZY_GP_DELAY 表示仅存在 lazy 回调时的最大空闲时间
1466  *  permitted to sleep in dyntick-idle mode with only lazy RCU              // lazy 回调可以推迟执行，但不能无限制
1467  *  callbacks pending.  Setting this too high can OOM your system.          // 设置过大可能导致内存耗尽（OOM）
1468  *
1469  * The values below work well in practice.  If future workloads require      // 下面的默认值在实际中表现良好
1470  * adjustment, they can be converted into kernel config parameters, though   // 将来如需调整可改为内核配置参数
1471  * making the state machine smarter might be a better option.               // 当然改进状态机可能是更好的方案
1472  */
1473 #define RCU_IDLE_GP_DELAY 4     /* Roughly one grace period. */             // 非 lazy 回调允许 idle 的时间（单位：jiffies，约等于一个 RCU 宽限期）
1474 #define RCU_IDLE_LAZY_GP_DELAY (6 * HZ) /* Roughly six seconds. */          // lazy 回调允许 idle 的时间（约 6 秒）
1475
1476 static int rcu_idle_gp_delay = RCU_IDLE_GP_DELAY;           // 非 lazy 回调 idle 延迟值（可通过模块参数设置）
1477 module_param(rcu_idle_gp_delay, int, 0644);                 // 注册为模块参数，可在运行时读取/设置
1478 static int rcu_idle_lazy_gp_delay = RCU_IDLE_LAZY_GP_DELAY; // lazy 回调 idle 延迟值
1479 module_param(rcu_idle_lazy_gp_delay, int, 0644);            // 注册为模块参数

```

```c
// vim kernel/rcu/tree_plugin.h +1520

1520 /*
1521  * Allow the CPU to enter dyntick-idle mode unless it has callbacks ready // 允许 CPU 进入 dyntick-idle 模式，除非有待处理的回调任务
1522  * to invoke.  If the CPU has callbacks, try to advance them.  Tell the  // 如果有回调任务，尝试推进它们的状态
1523  * caller to set the timeout based on whether or not there are non-lazy  // 根据是否存在非延迟回调，通知调用者设置定时器超时时间
1524  * callbacks.                                                            //
1525  *                                                                      //
1526  * The caller must have disabled interrupts.                            // 调用该函数前必须关闭中断
1527  */
1528 int rcu_needs_cpu(u64 basemono, u64 *nextevt)                     // 判断当前 CPU 是否仍然需要执行 RCU 工作
1529 {
1530     struct rcu_dynticks *rdtp = this_cpu_ptr(&rcu_dynticks);      // 获取当前 CPU 上的 rcu_dynticks 结构体指针
1531     unsigned long dj;                                             // 存储接下来需要等待的时间（以 jiffies 为单位）
1532
1533     lockdep_assert_irqs_disabled();                               // 断言中断已关闭，防止竞态条件
1534
1535     /* Snapshot to detect later posting of non-lazy callback. */   // 快照当前是否存在非 lazy 回调，用于后续判断是否有新增
1536     rdtp->nonlazy_posted_snap = rdtp->nonlazy_posted;             // 记录当前的 nonlazy_posted 状态
1537
1538     /* If no callbacks, RCU doesn't need the CPU. */              // 如果当前没有回调任务，RCU 就不需要使用这个 CPU
1539     if (!rcu_cpu_has_callbacks(&rdtp->all_lazy)) {               // 检查是否有任何回调挂起（lazy 或非 lazy）
1540         *nextevt = KTIME_MAX;                                     // 设置下一次事件时间为最大值（表示不需要定时器）
1541         return 0;                                                 // 返回 0 表示 RCU 当前不需要 CPU
1542     }
1543
1544     /* Attempt to advance callbacks. */                           // 尝试推进回调链表状态（比如从等待状态变为可执行）
1545     if (rcu_try_advance_all_cbs()) {                              // 如果推进后发现有可执行的回调
1546         /* Some ready to invoke, so initiate later invocation. */ // 有可执行回调，唤起 RCU 核心线程来处理
1547         invoke_rcu_core();                                        // 调用 rcu_core 进行回调处理
1548         return 1;                                                 // 返回 1 表示 CPU 还需要执行 RCU 工作
1549     }
1550     rdtp->last_accelerate = jiffies;                              // 记录最近一次尝试推进回调的时间
1551
1552     /* Request timer delay depending on laziness, and round. */   // 根据是否 lazy 回调决定下次检查时间，并向 jiffies 对齐
1553     if (!rdtp->all_lazy) {                                        // 如果存在非 lazy 回调（必须尽快执行）
1554         dj = round_up(rcu_idle_gp_delay + jiffies,                // 使用非 lazy 延迟时间，并向上对齐到 delay 的整数倍
1555             ¦   ¦  rcu_idle_gp_delay) - jiffies;                  // 计算实际延迟（单位：jiffies）
1556     } else {                                                      // 否则（所有回调都是 lazy 的）
1557         dj = round_jiffies(rcu_idle_lazy_gp_delay + jiffies) - jiffies; // 使用 lazy 延迟时间，并向 jiffies 轮整
1558     }
1559     *nextevt = basemono + dj * TICK_NSEC;                         // 设置下一次定时器触发时间（从单调时钟起点）
1560     return 0;                                                     // 返回 0 表示 CPU 可以进入 idle
1561 }

```

这段代码是 Linux 内核中 RCU（Read-Copy-Update）机制的一部分，主要功能是判断当前 CPU 是否需要继续处理 RCU 回调，或者是否可以进入空闲状态。以下是详细解析：

---

### **函数功能**

`rcu_needs_cpu()` 用于检查当前 CPU 是否需要继续处理 RCbackups，并计算下一次可能的回调触发时间（`nextevt`）。

**参数：**

* `basemono`：基准时间（通常是当前时间）
* `nextevt`：输出参数，存储下一次需要处理回调的时间

**返回值：**

* `1`：当前有回调需要立即处理，CPU 不能进入空闲状态
* `0`：当前没有紧急回调，可以进入空闲状态（`nextevt` 会告知下次可能的唤醒时间）

---

### **代码逻辑分析**

#### **1. 检查中断是否已禁用**

```c
lockdep_assert_irqs_disabled();
```

* RCU 要求调用此函数时必须禁用中断，否则会触发内核锁调试警告。

#### **2. 记录非延迟回调的快照**

```c
rdtp->nonlazy_posted_snap = rdtp->nonlazy_posted;
```

* 保存当前非延迟回调（`nonlazy`）的数量，用于后续检测是否有新回调到达。

#### **3. 如果没有回调，直接允许空闲**

```c
if (!rcu_cpu_has_callbacks(&rdtp->all_lazy)) {
    *nextevt = KTIME_MAX;  // 无需唤醒
    return 0;              // 不需要CPU
}
```

* 如果当前 CPU 没有待处理的回调（无论延迟或非延迟），设置下次唤醒时间为无限远（`KTIME_MAX`），并返回 `0`，表示 CPU 可以进入空闲状态。

#### **4. 尝试推进回调处理**

```c
if (rcu_try_advance_all_cbs()) {
    invoke_rcu_core();  // 触发RCU核心处理
    return 1;           // 需要CPU立即处理
}
```

* 如果有回调已就绪（例如 GP（Grace Period）结束），调用 `invoke_rcu_core()` 触发软中断处理，并返回 `1`，表示 CPU 不能空闲。

#### **5. 计算下次唤醒时间**

```c
if (!rdtp->all_lazy) {
    // 非延迟回调：按固定间隔（rcu_idle_gp_delay）向上取整
    dj = round_up(rcu_idle_gp_delay + jiffies, rcu_idle_gp_delay) - jiffies;
} else {
    // 延迟回调：按宽松间隔（rcu_idle_lazy_gp_delay）取整
    dj = round_jiffies(rcu_idle_lazy_gp_delay + jiffies) - jiffies;
}
*nextevt = basemono + dj * TICK_NSEC;  // 转换为纳秒时间
return 0;  // 允许空闲，但需在nextevt时唤醒
```

* **非延迟回调**：严格按 `rcu_idle_gp_delay` 周期唤醒（例如 4 个 jiffies）。

* **延迟回调**：按更宽松的 `rcu_idle_lazy_gp_delay` 周期唤醒（例如 6 个 jiffies），并可能对齐到时钟滴答边界。
* 最终时间 `nextevt` 是基于基准时间 `basemono` 和计算出的延迟 `dj` 得出的绝对时间（纳秒）。

---

#### **关键点总结**

1. **回调分类**：
   * **非延迟回调（non-lazy）**：高优先级，需尽快处理。
   * **延迟回调（lazy）**：低优先级，可延迟处理。
2. **唤醒策略**：
   * 有立即回调 → 禁止空闲，触发处理（`return 1`）。
   * 无回调 → 允许空闲，无需唤醒（`KTIME_MAX`）。
   * 有待处理但未就绪的回调 → 允许空闲，但设置下次唤醒时间。
3. **时间计算**：
   * 使用 `round_up` 或 `round_jiffies` 对齐时间，避免频繁唤醒。

---

#### **典型应用场景**

此函数被调用在 CPU 即将进入空闲状态（如 `tick_nohz_stop_tick`）时，用于决定：

* 是否需阻止空闲（有紧急回调）。
* 若允许空闲，何时需唤醒（例如 4ms 后检查延迟回调）。

![rcu_needs_cpu](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/rcu_needs_cpu.png)

## 有待处理但未就绪的回调 → 允许空闲，但设置下次唤醒时间，这里展开讲讲？

在 Linux 内核的 **RCU（Read-Copy-Update）机制** 中，当 CPU 进入空闲状态时，`rcu_needs_cpu()` 函数会判断当前是否有待处理的 RCU 回调，并根据回调的状态决定是否允许 CPU 进入空闲模式，以及何时唤醒它。  

**关键场景**：  
**“有待处理但未就绪的回调”**（即回调已注册，但尚未达到 GP（Grace Period）结束条件）时，RCU 允许 CPU 进入空闲状态，但会设置一个 **下次唤醒时间（`nextevt`）**，以便在合适的时机检查回调是否就绪。  

---

### **1. 代码逻辑回顾**

在 `rcu_needs_cpu()` 中，相关逻辑如下：

```c
if (!rcu_cpu_has_callbacks(&rdtp->all_lazy)) {
    *nextevt = KTIME_MAX;  // 无回调 → 无需唤醒
    return 0;
}

if (rcu_try_advance_all_cbs()) {
    invoke_rcu_core();     // 有就绪回调 → 立即处理
    return 1;
}

/* 有待处理但未就绪的回调 */
if (!rdtp->all_lazy) {
    dj = round_up(rcu_idle_gp_delay + jiffies, rcu_idle_gp_delay) - jiffies;
} else {
    dj = round_jiffies(rcu_idle_lazy_gp_delay + jiffies) - jiffies;
}
*nextevt = basemono + dj * TICK_NSEC;  // 设置下次唤醒时间
return 0;  // 允许空闲，但需在 `nextevt` 时唤醒
```

---

### **2. 什么是“待处理但未就绪的回调”？**

#### **(1) 回调的两种状态**

| **回调类型**       | **状态**                                                                 | **示例**                                                                 |
|--------------------|--------------------------------------------------------------------------|--------------------------------------------------------------------------|
| **已注册但未就绪** | 已通过 `call_rcu()` 注册，但 GP（Grace Period）尚未结束，不能立即执行。 | 例如：某个数据结构被删除，但可能仍有读者在访问，需等待所有读者退出。       |
| **已就绪可执行**   | GP 已结束，可以安全执行回调（如内存释放）。                              | 例如：所有读者已确认退出，RCU 可以安全调用 `kfree()`。                   |

#### **(2) 为什么不能立即执行？**

- RCU 的核心思想是 **“等所有读者退出后再回收资源”**，因此回调必须等待 GP 结束。
* 如果 GP 未结束，强制执行回调可能导致 **数据竞争** 或 **内存访问越界**。

---

### **3. 如何计算下次唤醒时间？**

由于回调尚未就绪，RCU 需要 **预测一个合理的时间点** 来检查 GP 是否结束。这个时间取决于：

1. **回调的类型**（非延迟 `!all_lazy` vs. 延迟 `all_lazy`）：
   * **非延迟回调**（高优先级）：使用较短的 `rcu_idle_gp_delay`（如 4 jiffies）。
   * **延迟回调**（低优先级）：使用较长的 `rcu_idle_lazy_gp_delay`（如 6 jiffies）。
2. **时间对齐**：
   * 非延迟回调：`round_up` 对齐到 `rcu_idle_gp_delay` 的整数倍，避免频繁唤醒。
   * 延迟回调：`round_jiffies` 对齐到时钟滴答（tick）边界，减少不必要的唤醒。

**示例计算**（假设 `jiffies=100`, `rcu_idle_gp_delay=4`）：

```c
dj = round_up(4 + 100, 4) - 100 = 104 - 100 = 4 jiffies
*nextevt = basemono + 4 * TICK_NSEC  // 4 jiffies 后唤醒
```

---

### **4. 唤醒后会发生什么？**

当 CPU 在 `nextevt` 时间被唤醒时，会再次调用 `rcu_needs_cpu()`，此时可能发生：

1. **GP 已结束** → 回调就绪，触发 `invoke_rcu_core()` 执行回调。
2. **GP 未结束** → 重新计算 `nextevt`，继续等待。

---

### **5. 对 `CONFIG_NO_HZ_FULL` 的影响**

在动态无时钟（`NO_HZ_FULL`）模式下：
* **默认行为**：无回调时彻底禁止唤醒（`KTIME_MAX`），最大化省电。
* **有待处理回调时**：即使允许空闲，也会设置精确的唤醒时间，确保回调能及时执行。
* **优化目标**：在 **低延迟** 和 **低功耗** 之间平衡：
  * 非延迟回调 → 短延迟（保证响应速度）。
  * 延迟回调 → 长延迟（减少唤醒次数）。

---

### **6. 实际应用案例**

### **(1) DPDK 隔离核心**

- 若 DPDK 线程不触发 RCU 回调 → `*nextevt = KTIME_MAX`，彻底无唤醒。
* 若使用 `KNI` 或内核内存分配 → 可能注册 RCU 回调，需设置唤醒时间。

### **(2) 高频交易系统**

- 非延迟回调确保快速释放内存，避免累积。
* 延迟回调合并处理，减少唤醒开销。

---

### **7. 如何监控 RCU 回调状态？**

```bash
# 查看待处理回调数量
cat /sys/kernel/debug/rcu/rcu*/rcu_pending

# 查看 GP 状态
cat /sys/kernel/debug/rcu/rcu*/grace_period
```

---

### **总结**

| **场景**               | 行为                                                                 |
|------------------------|----------------------------------------------------------------------|
| **无回调**             | `KTIME_MAX`，彻底无唤醒。                                           |
| **有就绪回调**         | 立即触发 `invoke_rcu_core()`，禁止空闲。                            |
| **有待处理但未就绪**   | 允许空闲，但设置 `nextevt` 定时唤醒，确保回调及时执行。             |
| **延迟 vs. 非延迟**    | 非延迟回调唤醒更快，延迟回调唤醒更宽松。                            |

这种机制保证了 RCU 在 **避免内存泄漏** 的同时，最小化对 CPU 空闲状态的干扰。

## 那也就是说在config_no_hz_full下，无回调 → 允许空闲，无需唤醒（KTIME_MAX）。彻底没有rcu设置下次唤醒时间？

是的，在 `CONFIG_NO_HZ_FULL`（动态无时钟模式）下，**如果当前 CPU 完全没有待处理的 RCU 回调（`!rcu_cpu_has_callbacks()` 返回 `true`）**，`rcu_needs_cpu()` 会直接将下次唤醒时间设为 `KTIME_MAX`，并返回 `0`。这意味着：

---

### **1. 无回调时的行为**

- **`*nextevt = KTIME_MAX`**  
  表示该 CPU **不需要被 RCU 唤醒**，可以长期保持空闲状态，直到其他事件（如中断、任务唤醒等）发生。
* **`return 0`**  
  通知调用者（如 `tick_nohz_stop_tick`）**当前 CPU 无需为 RCU 回调保持唤醒状态**。

---

### **2. 对 `CONFIG_NO_HZ_FULL` 的影响**

- **彻底无唤醒**  
  在无回调的情况下，RCU 不会强制设置任何唤醒时间，CPU 可以完全关闭时钟中断（Tickless），进入**深度空闲状态**，显著节省功耗。
* **依赖其他事件唤醒**  
  唤醒可能由以下事件触发：
  * 非 RCU 事件（如硬件中断、调度器任务唤醒）。
  * 后续新到达的 RCU 回调（通过 `rdtp->nonlazy_posted_snap` 检测变更）。

---

### **3. 与“有回调但未就绪”的区别**

如果存在回调但未就绪（如代码中的 `else` 分支），`rcu_needs_cpu()` 会计算一个明确的唤醒时间（`*nextevt`），此时行为不同：

| **场景**               | 唤醒时间（`*nextevt`） | 返回值 | 含义                          |
|------------------------|-----------------------|--------|-------------------------------|
| **无任何回调**          | `KTIME_MAX`           | `0`    | 彻底无需唤醒，长期空闲         |
| **有回调（非延迟/延迟）** | 未来时间（如 `basemono + dj`） | `0`    | 允许空闲，但需定时唤醒检查回调 |

---

### **4. 代码验证**

回到 `rcu_needs_cpu()` 的逻辑：

```c
if (!rcu_cpu_has_callbacks(&rdtp->all_lazy)) {
    *nextevt = KTIME_MAX;  // 无回调 → 无需唤醒
    return 0;
}
```

- `rcu_cpu_has_callbacks()` 返回 `false` 时，说明**既无延迟回调，也无非延迟回调**。
* 此时 `KTIME_MAX` 的语义是“无限远”，即**不主动设置 RCU 唤醒时间**。

---

### **5. 总结**

- **无回调时**：RCU 不会干扰 CPU 的空闲策略，`KTIME_MAX` 确保彻底无唤醒。
* **有回调时**：根据回调类型（延迟/非延迟）计算唤醒时间，平衡响应速度和功耗。
* **适用场景**：`CONFIG_NO_HZ_FULL` 下，RCU 对空闲 CPU 的干预最小化，适合低延迟、高能效场景（如 HPC 或实时系统）。

## 那dpdk跑在隔离核心上，且启用了config_no_hz_full，内核态会有rcu_callback吗？

在 **DPDK 运行于隔离核心（Isolated CPU）** 并启用 **`CONFIG_NO_HZ_FULL`** 的情况下，**内核态的 RCU 回调是否会存在**，取决于以下几个关键因素：

---

### **1. DPDK 的 CPU 隔离机制**

DPDK 通常通过以下方式隔离核心：
* **`isolcpus`** 内核启动参数（如 `isolcpus=2,3`）或 **`cgroups`** 隔离。
* **`taskset`** 或 **`sched_setaffinity`** 绑定 DPDK 线程到指定核心。
* **禁用内核调度干扰**（如 `nohz_full=2,3` 配合 `rcu_nocbs=2,3`）。

#### **对 RCU 的影响**

- **隔离核心不处理普通内核任务**，因此**常规内核 RCU 回调较少**。
* 但**某些特殊情况仍可能触发 RCU 回调**（见下文）。

---

### **2. 内核态 RCU 回调的来源**

即使 CPU 被隔离，以下情况仍可能导致内核态 RCU 回调：

#### **(1) DPDK 自身调用内核功能**

- **`rte_malloc` / `rte_free`**：依赖 `glibc` 或内核内存管理，可能触发 `call_rcu()`。
* **`rte_eth_dev_*`**：网卡驱动卸载或配置变更可能涉及 RCU。
* **`rte_timer`**：若使用内核定时器（非 DPDK 纯用户态定时器），可能依赖 RCU。

#### **(2) 共享数据结构**

- **网络协议栈旁路不完全**：如果 DPDK 仍与内核协议栈交互（如 `KNI` 或 `AF_XDP`），可能触发 RCU。
* **文件系统操作**：若 DPDK 访问 `/sys` 或 `/proc`，可能涉及 RCU 保护的数据结构。

#### **(3) 硬件中断处理**

- **网卡中断**（若未完全轮询模式）：中断处理可能调用 `call_rcu()`。
* **PMD 轮询模式**：通常无中断，但异常事件（如链路状态变更）可能触发 RCU。

#### **(4) 其他内核子系统**

- **`sysfs` / `procfs`**：DPDK 用户态工具查询信息时可能触发 RCU。
* **`perf` 或调试事件**：若启用性能监控，可能涉及 RCU。

---

### **3. `CONFIG_NO_HZ_FULL` 的影响**

启用 `NO_HZ_FULL` 后：
* **默认不强制唤醒空闲 CPU**（`rcu_needs_cpu()` 返回 `KTIME_MAX`）。
* **但若存在 RCU 回调**（如上述情况），仍会：
  * 通过 `invoke_rcu_core()` 触发软中断（如 `rcuc` 线程）。
  * 若隔离核心有回调，可能被唤醒处理。

---

### **4. 如何彻底避免 RCU 回调？**

如果希望 **完全消除隔离核心的 RCU 回调**，需：

1. **`rcu_nocbs` 参数**  
   启动时添加 `rcu_nocbs=<cpu-list>`，将 RCU 回调卸载到其他核心（如 `rcuc` 线程）。

   ```sh
   grub_cmdline_linux="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
   ```

2. **避免内核调用**  
   * 使用 **纯用户态 DPDK 驱动**（如 `vfio-pci` 而非 `igb_uio`）。
   * 禁用 `KNI`、`AF_XDP` 等内核交互组件。
   * 确保内存分配使用 `rte_malloc` 而非 `malloc`。
3. **监控 RCU 活动**  

   ```sh
   watch -n1 'cat /sys/kernel/debug/rcu/rcu*/rcu_pending'
   ```

   检查是否有回调累积。

---

### **5. 结论**

| **场景**                     | **是否可能触发 RCU 回调？** |
|------------------------------|---------------------------|
| **纯用户态 DPDK + 无内核调用** | ❌ 基本无 RCU 回调         |
| **使用内核功能（如 KNI）**     | ✅ 可能触发 RCU           |
| **启用 `rcu_nocbs`**          | ❌ 回调由其他核心处理      |

* **理想情况**：DPDK 运行在完全隔离的核心 + 无内核调用 + `rcu_nocbs` → **几乎无 RCU 回调**。
* **实际情况**：若涉及内核交互（如内存分配、网卡控制），**仍可能有少量 RCU 回调**，但可通过调优最小化。
