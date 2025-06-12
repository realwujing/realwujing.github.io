# Linux 4.19 内核 Tickless 机制与 RCU Stall 问题分析总结

## 问题描述

### 1. 内核版本及环境信息

```bash
uname -a
```

结果：

```
Linux gzinf-kunpeng-55e235e17e57 4.19.90-2102.2.0.0068.ctl2.aarch64 #1 SMP Tue Aug 22 10:53:53 UTC 2023 aarch64 aarch64 aarch64 GNU/Linux
```

---

### 2. 内核启动参数

```bash
cat /proc/cmdline
```

结果：

```
BOOT_IMAGE=(hd0,3)/vmlinuz-4.19.90-2102.2.0.0068.ctl2.aarch64 root=UUID=216fa54f-9345-4f9a-8fe2-dc3a44db7775 ro console=tty0 pci=realloc pciehp.pciehp_force=1 biosdevname=0 net.ifnames-0 console=ttyS0,115200n8 crashkernel=1024M,high smmu.bypassdev=0x1000:0x17 smmu.bypassdev=0x1000:0x15 video=efifb:off
```

---

### 3. 内核配置相关

```bash
grep -E "CONFIG_HZ|CONFIG_HIGH_RES_TIMERS|CONFIG_NO_HZ" /boot/config-$(uname -r)
```

结果：

```
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
```

结果：

```
arch_sys_counter
```

---

### 5. CPU Tickless 状态

```bash
grep 'tick_stopped' /proc/timer_list
```

部分结果（部分截取）：

```
.tick_stopped   : 1
.tick_stopped   : 1
...
.tick_stopped   : 0
.tick_stopped   : 1
...
```

说明有些 CPU 已进入 tickless（tick\_stopped:1），部分尚未停止调度 tick。

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
* 但出现 RCU stall 和 rcu\_sched 被饿死60秒，甚至 rcu hard lockup。
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

#### 关于 hrtimer 与传统调度 timer（sched\_timer）的关系

* **hrtimer（高精度定时器）** 启用后，
  它会 **代替传统的低精度周期定时器（如 jiffies-based timer）来调度高精度任务**，
  例如精确的超时和延时需求。

* 但是，**sched\_timer（调度周期 timer）并不会完全被移除**，
  它依然保留着调度器基本的调度节奏控制作用，
  特别是在非 tickless 模式或 CPU 活跃状态下的调度周期维护。

* 在 **tickless 模式下（NO\_HZ）**，sched\_timer 可能会被停止（tick\_stopped=1），
  但它仍作为“备胎”存在于内核中，方便恢复调度 tick，
  以及处理某些必须依赖传统周期 tick 的功能。

* hrtimer 和 sched\_timer 是 **协同工作的**，
  保证调度的准确性和系统响应的灵活性。

---

## ⏱️ hrtimer 与传统 tick 定时器的协同机制（核心讲解）

### ✅ **一、传统低精度定时器机制（timer wheel）**

* 依赖 **周期性 tick 中断** 驱动（来源于 CONFIG\_HZ 设置）。
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

#### 1. **sched\_timer 是一个周期性的 hrtimer，用于模拟每个 tick**

* 其超时值设置为 `1 / CONFIG_HZ = 4ms`。
* 触发后会：

  * 调用 `update_process_times()`，驱动调度器、进程统计等。
  * `update_wall_time()`，驱动时钟更新。
  * 然后 **再次插入下一个 tick 的 hrtimer**（即周期性重插）。

#### 2. **如果系统空闲，可以关闭 tick（tickless 模式）**

* 在你系统中，大量 `.tick_stopped: 1` → 多核已启用 **动态 tick 停止**（NOHZ）。
* 系统会判断未来是否有近期任务要做，如果没有，就不设置下一个 sched\_timer。
* 等到下一个明确事件（比如 hrtimer 或唤醒）再启动 tick。

#### 3. **如果有 hrtimer 到期时间更早，sched\_timer 会被推后**

* hrtimer 的红黑树中调度的是“最早到期时间”的事件。
* 比如用户设定的 2ms 超时任务会抢占原本的 sched\_timer 设置。

---

### ✅ **四、协同机制小结：**

| 类型                       | 驱动来源               | 精度              | 应用场景              |
| ------------------------ | ------------------ | --------------- | ----------------- |
| `jiffies` / `timer_list` | 周期 tick（低精度）       | 毫秒级（CONFIG\_HZ） | 大量任务，性能友好         |
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
* `CONFIG_NO_HZ_FULL=y`：表示系统**启用了完整 tickless 模式**（NO\_HZ\_FULL），用于用户态；
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

### ✅ 三、CPU idle 默认行为（NO\_HZ\_FULL 下）

> ✅ **在 NO\_HZ\_FULL 模式下，非内核态、idle CPU 会自动停掉调度 tick**，只保留必要的定时器（如 hrtimer、RCU、watchdog）。

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
| **NO\_HZ\_IDLE** | 默认对所有 CPU 启用              | CPU 进入 idle 时停止 tick |
| **NO\_HZ\_FULL** | 需要配置 `nohz_full=` 并隔离 CPU | CPU 在用户态时也可停 tick    |

你的内核配置中启用了 `NO_HZ_FULL`，则说明：

* **没有被隔离的 CPU 默认运行在 idle tickless 模式（与 `NO_HZ_IDLE` 行为一致）**；
* 被隔离的 CPU（通过内核命令行 `nohz_full=` 配置）则支持用户态停 tick。

---

### ✅ 举个例子

之前运行的：

```bash
cat /proc/timer_list | grep 'tick_stopped'
```

输出中大多数 CPU 是：

```
.tick_stopped : 1
```

说明当前这些 CPU 已经进入 tickless 状态，而这在没有 `nohz_full=` 参数的情况下，通常就是因为这些 CPU 当前 idle，调度器选择了 idle 任务。

---

### ✅ 结论

> **是的，系统中未被 `nohz_full` 隔离的 CPU，只要调度器选择了 idle 任务（即进入空闲），就会自动停止调度用 tick，进入 tickless 模式。**

这正是 idle tickless 的标准行为。

如需进一步分析哪些 CPU 被 nohz\_full 隔离，可以查看：

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

#### 🔹 在 NO\_HZ\_FULL 模式中

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

## tickless下的rcu会注册自己的htimer定时器，这时候怎么不起作用了？

很好的问题！既然 **tickless 模式下 RCU 会注册自己的 hrtimer 定时器**，理论上它应该可以独立驱动 RCU 任务的触发，为什么会出现 RCU stall 和 `rcu_sched` 被饿死的情况呢？

---

### 核心点：RCU 的 hrtimer 为什么不起作用？

#### 1. **hrtimer 依赖底层时钟事件驱动，但 tickless 模式下有可能 ClockEvent 设备进入停顿状态**

* 在 tickless idle 模式，内核会尝试关闭周期性 tick，底层 ClockEvent 硬件可能也进入“停止”或“单次触发”模式。
* 如果硬件定时器（ClockEvent device）没有正常设置下一次超时（例如，时钟事件停止或设置失败），hrtimer 的超时回调就不会被硬件触发，软中断不会被激活，导致 RCU 定时任务无法被执行。

#### 2. **hrtimer 的触发依赖于 CPU 不是处于深度睡眠且硬件定时器正常工作**

* CPU 如果处于较深的 idle 状态（如 C3、C6 等），硬件定时器可能被屏蔽或者不精准，导致 hrtimer 触发延迟或丢失。
* 因为 tickless idle 让 CPU 尽可能省电，部分深度睡眠模式对定时器有影响。

#### 3. **内核中 RCU hrtimer 定时器管理可能被长时间延迟**

* RCU 的 hrtimer 触发后会产生软中断，驱动 RCU 任务执行。
* 软中断处理依赖内核的调度机制，如果软中断处理被长时间阻塞（如高中断上下文、锁竞争），任务依然得不到执行。

#### 4. **竞争与优先级**

* 其他更高优先级的硬件中断或软中断抢占，导致 RCU hrtimer 软中断延迟。
* RCU hrtimer 超时后生成软中断，但软中断调度优先级不够高，长时间积压。

---

### 系统中的可能具体表现

* CPU 进入 tickless idle，硬件 ClockEvent 停止周期性 tick 以省电。
* RCU 注册的 hrtimer 依赖该 ClockEvent 来设置超时触发，但 ClockEvent 被停用或触发延迟。
* 因此，尽管 RCU 定时器存在，底层硬件或软中断机制没有及时触发对应回调。
* 导致 RCU 任务未被调度，最终被判定为 `rcu_sched` stall 和 hard lockup。

---

### 结论

**RCU 的 hrtimer 起作用的前提是底层时钟事件和软中断机制正常工作。tickless idle 模式下，如果 ClockEvent 设备停止或软中断处理被阻塞，hrtimer 就“看起来”不起作用了。**
