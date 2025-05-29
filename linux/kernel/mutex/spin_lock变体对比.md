# spin_lock变体对比

## 自旋锁spin_lock和raw_spin_lock

- [自旋锁spin_lock和raw_spin_lock](https://blog.csdn.net/DroidPhone/article/details/7395983)

### 1. **背景解析：`raw_spin_lock` 的起源**

- **历史背景（2004 年，PREEMPT_RT 的引入）**：
  - 2004 年，MontaVista Software 提出了实时 Linux 内核模型，目标是提升 Linux 的实时性，由 Ingo Molnar 实现为 PREEMPT_RT 补丁。
  - PREEMPT_RT 的核心理念是允许临界区被抢占，并将自旋锁（spinlock）的忙等待机制改为基于信号量（`rt_mutex`）的睡眠锁，以减少延迟，提高实时性。
  - 当时内核中有约 10,000 处使用 `spin_lock`，若全部改为睡眠锁，改动过于庞大，且可能影响性能敏感的代码（如硬中断处理、调度器等）。

- **解决方案**：
  - 为避免大规模修改，开发者决定只在必须保持忙等待（不允许抢占或休眠）的约 100 处使用新的锁机制，命名为 `raw_spin_lock`。
  - 同时，Linus Torvalds 建议调整命名空间以减少混淆：
    - 原有的 `raw_spin_lock`（体系结构相关的原子操作实现）改为 `arch_spin_lock`。
    - 原有的 `spin_lock` 改为 `raw_spin_lock`，用于忙等待锁。
    - 引入新的 `spin_lock`，在 PREEMPT_RT 下映射为睡眠锁。

- **结果**：
  - 在 Linux 2.6.33 及之后的版本中，`spin_lock` 成为通用接口，优先使用，允许在 PREEMPT_RT 下转换为睡眠锁。
  - `raw_spin_lock` 用于性能敏感或硬实时场景，始终保持忙等待。

### 2. **`raw_spin_lock` 和 `spin_lock` 的区别（Linux 4.19 上下文）**

结合提供的信息和 Linux 4.19 的实现，区别如下：

| **特性**                | **`spin_lock`**                                              | **`raw_spin_lock`**                                          |
|------------------------|-------------------------------------------------------------|-------------------------------------------------------------|
| **定义**               | 高级接口，基于 `raw_spin_lock` 封装，可能包含调试逻辑。       | 低级接口，直接操作 `arch_spin_lock`，无额外封装。            |
| **抢占行为**           | 禁用抢占（通过 `preempt_disable()`）；PREEMPT_RT 下为睡眠锁。 | 禁用抢占（通过 `preempt_disable()`），始终忙等待。           |
| **PREEMPT_RT 行为**    | 映射为 `rt_mutex` 睡眠锁，允许抢占和休眠。                   | 保持忙等待，不允许抢占或休眠。                              |
| **调试支持**           | 支持 lockdep 死锁检测（若启用 `CONFIG_DEBUG_LOCK_ALLOC`）。   | 无调试逻辑，性能更高。                                      |
| **适用场景**           | 通用场景，临界区可稍长，优先使用。                          | 硬中断、调度器等性能敏感场景，临界区必须极短。              |
| **调用路径**           | 调用 `raw_spin_lock`，增加调试和 PREEMPT_RT 逻辑。            | 直接调用 `arch_spin_lock`，减少开销。                       |

### 3. **为何内核中同时使用 `spin_lock` 和 `raw_spin_lock`**

- **历史遗留与兼容性**：
  - 在 PREEMPT_RT 补丁引入之前，内核只有 `spin_lock`，用于所有忙等待场景。引入 PREEMPT_RT 后，`spin_lock` 被改造为支持睡眠锁，但某些场景（硬中断、调度器等）不能接受休眠，因此需要 `raw_spin_lock` 保留原始忙等待行为。
  - 内核开发者选择保留两套接口，允许逐步迁移，而不是一次性替换所有锁。

- **性能与实时性权衡**：
  - **`spin_lock`**：在 PREEMPT_RT 下，`spin_lock` 转换为睡眠锁，适合实时系统，减少忙等待带来的延迟，但可能增加上下文切换开销。
  - **`raw_spin_lock`**：保持忙等待，适用于性能敏感场景（如调度器、时钟中断），但不适合实时性要求高的场景。

- **代码分布**：
  - 内核中约 99% 的锁使用 `spin_lock`，因为它们在进程上下文或软中断中，允许抢占或休眠。
  - 约 1% 的锁使用 `raw_spin_lock`，集中在硬中断、调度器或核心代码（如 `kernel/sched/` 或 `kernel/time/`），这些地方禁止休眠。

### 4. **关于显式调用 `preempt_disable()`**

- 前一个问题提到 `raw_spin_lock` 是否需要显式调用 `preempt_disable()`：
  - **实际实现**：在 Linux 4.19 中，`raw_spin_lock` 的调用路径（如 `__raw_spin_lock`）通常显式调用 `preempt_disable()`，以确保抢占被禁用（如前所述，位于 `include/linux/spinlock_api_smp.h`）。
  - **理论可能性**：`arch_spin_lock` 的忙等待和原子操作可能隐式防止抢占，但显式调用 `preempt_disable()` 是为了跨架构一致性和调试支持（如 lockdep）。
  - **PREEMPT_RT 影响**：`raw_spin_lock` 不受 PREEMPT_RT 影响，始终忙等待，因此 `preempt_disable()` 确保调度器不会干预。

### 5. **文章中的建议解读**

文章建议：

- **尽可能使用 `spin_lock`**：因为它是通用接口，支持 PREEMPT_RT 的睡眠锁，适合大多数场景。
- **仅在绝对不允许抢占或休眠的地方使用 `raw_spin_lock`**：如硬中断、调度器或极短的临界区。
- **临界区极小时使用 `raw_spin_lock`**：因为忙等待锁的性能开销在短临界区中更可控。

这些建议与 Linux 4.19 的实现一致：

- `spin_lock` 是默认选择，适用于进程上下文或软中断，兼容 PREEMPT_RT。
- `raw_spin_lock` 用于硬中断或性能敏感场景，如 `kernel/sched/core.c` 中的运行队列保护。

### 6. **Linux 4.19 的具体实现**

- **架构实现**：在 x86_64 上，`raw_spin_lock` 使用 queued spinlock（`qspinlock`），定义在 `arch/x86/include/asm/qspinlock.h`，通过原子操作（如 `cmpxchg`）实现高效锁。
- **调用路径**：

  ```c
  #define raw_spin_lock(lock) _raw_spin_lock(lock)
  static inline void __raw_spin_lock(raw_spinlock_t *lock)
  {
      preempt_disable(); // 显式禁用抢占
      spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
      arch_spin_lock(&lock->raw_lock); // 调用架构特定的锁
  }
  ```

- **PREEMPT_RT**：`spin_lock` 在 PREEMPT_RT 下映射为 `rt_mutex`（定义在 `include/linux/spinlock_rt.h`），而 `raw_spin_lock` 保持不变。

### 7. **总结**

- **为何同时存在**：`spin_lock` 和 `raw_spin_lock` 的并存源于 PREEMPT_RT 的需求。`spin_lock` 优先用于支持实时性的场景，`raw_spin_lock` 用于必须忙等待的硬实时场景。
- **抢占禁用**：`raw_spin_lock` 通常显式调用 `preempt_disable()`，以确保一致性和调试支持，尽管 `arch_spin_lock` 的忙等待可能隐式防止抢占。
- **使用建议**：
  - 默认使用 `spin_lock`，除非明确需要忙等待（如硬中断或极短临界区）。
  - 在 Linux 4.19 中，`raw_spin_lock` 适用于核心代码（如调度器），而 `spin_lock` 适用于通用场景。

---

## 🔒 自旋锁变体对比表

| 锁类型                     | 中断控制               | 抢占控制   | 调试支持  | 适用场景                   | 性能开销 | 示例代码                                                                                                      |
| ----------------------- | ------------------ | ------ | ----- | ---------------------- | ---- | --------------------------------------------------------------------------------------------------------- |
| `spin_lock`             | ❌ 不关中断             | 隐式关闭抢占 | ✅ 支持  | 通用非中断上下文，需调试检测死锁       | 较高   | `spin_lock(&lock); ... spin_unlock(&lock);`                                                               |
| `raw_spin_lock`         | ❌ 不关中断             | 隐式关闭抢占 | ❌ 不支持 | 内核核心路径（调度器/RCU），极致性能需求 | 最低   | `raw_spin_lock(&lock); ... raw_spin_unlock(&lock);`                                                       |
| `spin_lock_irq`         | ✅ 关中断（不保存状态）       | 隐式关闭抢占 | ✅ 支持  | 简化中断已关闭场景              | 中    | `spin_lock_irq(&lock); ... spin_unlock_irq(&lock);`                                                       |
| `spin_lock_irqsave`     | ✅ 关中断 + 保存状态       | 隐式关闭抢占 | ✅ 支持  | 通用中断上下文锁，需要恢复原始中断状态    | 中    | `unsigned long flags; spin_lock_irqsave(&lock, flags); ... spin_unlock_irqrestore(&lock, flags);`         |
| `raw_spin_lock_irqsave` | ✅ 关中断 + 保存状态       | 隐式关闭抢占 | ❌ 不支持 | 中断上下文 + 高性能需求          | 低    | `unsigned long flags; raw_spin_lock_irqsave(&lock, flags); ... raw_spin_unlock_irqrestore(&lock, flags);` |
| `spin_lock_bh`          | ❌ 不关硬件中断<br>✅ 关软中断 | 隐式关闭抢占 | ✅ 支持  | 防止软中断并发（网络、定时器）        | 中    | `spin_lock_bh(&lock); ... spin_unlock_bh(&lock);`                                                         |
| `raw_spin_lock_bh`      | ❌ 不关硬件中断<br>✅ 关软中断 | 隐式关闭抢占 | ❌ 不支持 | 高性能软中断保护（如网络驱动）        | 低    | `raw_spin_lock_bh(&lock); ... raw_spin_unlock_bh(&lock);`                                                 |

---

### 📌 关键维度说明

#### 🔁 中断控制

* **关中断**：防止硬中断期间产生死锁。
* **关软中断**：防止软中断上下文中并发访问共享资源（如网络子系统）。

#### 🚫 抢占控制

* 所有自旋锁在加锁期间 **自动关闭抢占（preempt\_disable）**。
* 若需显式关闭抢占（例如临界区内调用非锁操作），可用 `preempt_disable()` 搭配使用。

#### 🧪 调试支持

* `spin_lock` 系列支持 **LOCKDEP 死锁检测**，便于开发调试。
* `raw_spin_lock` 系列去除了调试逻辑，追求 **最小开销**。

---

### 📋 锁选择指南

| 使用场景     | 推荐锁类型                   | 理由                  |
| -------- | ----------------------- | ------------------- |
| 普通内核模块   | `spin_lock`             | 调试支持完善，适合大多数使用情境    |
| 中断上下文    | `raw_spin_lock_irqsave` | 安全地关闭中断，适用于硬件中断处理程序 |
| 高性能内核路径  | `raw_spin_lock`         | 追求极致性能，避免调试代码影响     |
| 防止软中断并发  | `spin_lock_bh`          | 屏蔽软中断足够，无需关闭硬件中断    |
| 实时内核（RT） | `raw_spin_lock`         | 避免被自动替换为可睡眠锁，确保实时响应 |

---

### ⚠️ 注意事项

* **不要混用不同变体**：如 `spin_lock` 加锁但 `raw_spin_unlock` 解锁，行为未定义。
* **避免长临界区操作**：阻塞其他 CPU，影响系统实时性。
* **实时内核兼容性**：RT patch 中部分 `spin_lock` 可能被转换为可睡眠锁，应优先使用 `raw_spin_lock` 保持行为一致。
* **中断状态恢复务必对称**：使用 `irqsave/irqrestore` 时确保成对使用。

## 也就是说带raw的唯一区别就是不支持调试？

是的，理解得基本正确：

> **带 `raw_` 前缀的自旋锁（如 `raw_spin_lock`）的主要区别** 是：
> **移除了调试/追踪功能**（如 `lockdep`、锁验证、调试信息记录），
> **以最小开销、最直接的方式操作锁**，适用于 **内核核心路径或性能关键场景**。

---

## ✅ `raw_spin_lock` vs `spin_lock` 核心对比总结

| 特性           | `spin_lock`                                   | `raw_spin_lock`         |
| ------------ | --------------------------------------------- | ----------------------- |
| **调试支持**     | ✅ 支持 `LOCKDEP`（锁依赖、死锁检测）                      | ❌ 不支持，避免调试逻辑（性能最优）      |
| **中断控制**     | ❌ 默认不关中断（需使用 `_irqsave` 等变体）                  | ❌ 同上                    |
| **抢占控制**     | ✅ 隐式关闭（`preempt_disable()`）                   | ✅ 同上                    |
| **性能开销**     | 较高（额外调试逻辑）                                    | 更低（裸操作）                 |
| **适用场景**     | 普通内核模块、调试阶段                                   | 性能关键路径、锁实现内部、实时内核       |
| **替换为睡眠锁风险** | ✅ 在 RT 内核中可能被替换为 `rt_mutex`                   | ❌ 始终为真正的自旋锁             |
| **底层实现**     | `__spin_lock()` → 调试逻辑 + `do_raw_spin_lock()` | 直接 `do_raw_spin_lock()` |

---

### 🔍 为什么需要 `raw_spin_lock`

#### 1. **绕过调试逻辑，极致性能**

* `raw_spin_lock()` 直接使用底层原子指令。
* 避免调用 `LOCKDEP` 的开销，例如 `spin_acquire()`、`lockdep_invariant_state()` 等。

#### 2. **避免锁实现中的递归调用**

* 比如实现 `qspinlock` 自旋锁算法时自身就使用锁，必须避免递归触发 `LOCKDEP`。

#### 3. **实时内核场景中避免潜在睡眠**

* `spin_lock` 在 `CONFIG_PREEMPT_RT` 下会变为 `rt_mutex`（可睡眠），不再是真正的自旋锁。
* 而 `raw_spin_lock` 永远不会被替换为 `rt_mutex`，仍保持忙等行为。

---

### 💡 底层实现简析

#### `spin_lock()` 逻辑（带调试）

```c
void spin_lock(spinlock_t *lock) {
    __spin_lock(lock);                 // 调用 LOCKDEP 调试逻辑
    do_raw_spin_lock(&lock->rlock);   // 真正原子加锁
}
```

* `__spin_lock()` 内部调用 `spin_acquire()`、打标记、检查递归等。

#### `raw_spin_lock()` 逻辑（无调试）

```c
void raw_spin_lock(raw_spinlock_t *lock) {
    do_raw_spin_lock(lock);           // 直接原子操作，最小指令路径
}
```

---

### 🛠️ 选择指南

| 使用场景           | 推荐锁类型                   | 原因                      |
| -------------- | ----------------------- | ----------------------- |
| 普通驱动/模块开发      | `spin_lock`             | 锁依赖检查有助于调试死锁            |
| 调度器、RCU、软中断    | `raw_spin_lock`         | 极致性能、避免调试开销             |
| 硬中断上下文（需关中断）   | `raw_spin_lock_irqsave` | 不能使用 `spin_lock`，也无需调试  |
| 实时内核中要求强制自旋的代码 | `raw_spin_lock`         | 防止被替换为 `rt_mutex`（可睡眠锁） |
| 锁实现自身          | `raw_spin_lock`         | 避免递归调用死锁检测逻辑            |

---

### ⚠️ 注意事项

* 🔁 **不要混用 `raw_spin_lock` 和 `spin_unlock`**：必须成对使用。
* ⏳ **避免在临界区内执行耗时操作**：尤其是 `raw_spin_lock`，会长时间占用 CPU。
* 🧠 **明确的锁所处上下文（中断/软中断/抢占）**，选择正确的变体。

---

### ✅ 总结一句话

> 除非明确知道自己**需要跳过调试或运行于实时/中断/调度路径中**，否则请默认使用 `spin_lock` —— 它安全，有调试支持，适合大多数场景。

---

## 🔍 进入底层看实现：`__spin_lock()` 和 `do_raw_spin_lock()`

这些函数是自旋锁的核心实现，下面以 `x86_64` 为例（以 Linux 6.x 为基础，结构不会变太大）：

---

### 🔹 `__spin_lock()` 是带调试支持的封装

```c
void __spin_lock(raw_spinlock_t *lock)
{
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_); // LOCKDEP 追踪
    do_raw_spin_lock(lock);
}
```

* `spin_acquire()`：用于 Lockdep 死锁检测系统，记录谁在什么时候拿了锁。
* 然后调用 `do_raw_spin_lock()`，实际执行原子操作。

---

### 🔹 `do_raw_spin_lock()` 是真正执行原子加锁逻辑的底层实现

在 `include/linux/spinlock_api_smp.h` 或 `arch/x86/include/asm/spinlock.h` 中，最终调用的是如下代码：

```c
static __always_inline void do_raw_spin_lock(raw_spinlock_t *lock)
{
    preempt_disable();                         // 关闭抢占
    arch_spin_lock(&lock->raw_lock);           // 硬件架构原子加锁
}
```

其中：

* `preempt_disable()` 防止自旋过程中被调度切走（必须保证线程持续运行以完成锁操作）。
* `arch_spin_lock()` 是体系结构相关的实现，例如 x86 是用 `xchg` 或 `cmpxchg`：

---

### 🔹 x86 架构下的 `arch_spin_lock()` 示例实现

```c
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
    while (unlikely(!arch_spin_trylock(lock))) {
        cpu_relax(); // 减少总线压力，避免忙等
    }
}
```

* `arch_spin_trylock()` 尝试原子加锁，失败就循环；
* `cpu_relax()` 是一个 hint，比如 `pause` 指令，用于减少忙等时的功耗和提升性能。

---

### 🔹 `raw_spin_lock()` 和 `spin_lock()` 的关系

```c
#define raw_spin_lock(lock)         _raw_spin_lock(lock)
#define spin_lock(lock)             _spin_lock(lock)

#define _spin_lock(lock)            __spin_lock(lock)  // 有调试支持
#define _raw_spin_lock(lock)        do_raw_spin_lock(lock) // 纯粹原子操作
```

也就是说：

| 接口                | 调试支持 | 调用路径                                     |
| ----------------- | ---- | ---------------------------------------- |
| `spin_lock()`     | ✅ 有  | → `__spin_lock()` → `do_raw_spin_lock()` |
| `raw_spin_lock()` | ❌ 无  | → `do_raw_spin_lock()` 直接执行              |

---

### ✅ 总结

| 比较项  | `spin_lock()`                      | `raw_spin_lock()`    |
| ---- | ---------------------------------- | -------------------- |
| 调试功能 | ✅ 支持 lockdep 等                     | ❌ 无                  |
| 开销   | 略高（主要是调试追踪）                        | 最小                   |
| 使用建议 | 一般内核模块/驱动                          | 性能关键路径，如调度器、RCU、硬中断  |
| 内部实现 | `__spin_lock` + `do_raw_spin_lock` | 仅 `do_raw_spin_lock` |

## 关中断、关抢占调度器还会切换进程吗？

在 Linux 内核中，如果**关闭中断**和**关闭抢占（preemption）**，调度器**一般不会切换进程**，但这要看具体的上下文和操作行为。我们来详细分情况说明：

---

### ✅ 一、先说结论

| 状态         | 是否可能进程切换            |
| ---------- | ------------------- |
| 关中断 + 关抢占  | ❌ **一般不会**切换进程      |
| 仅关中断       | ✅ 可能被调度器抢占（内核态非抢占）  |
| 仅关抢占       | ❌ 不会调度其他任务，但可能被中断打断 |
| 用户态        | ✅ 会被抢占调度            |
| 内核态默认      | ✅ 在抢占内核配置下可被抢占      |
| NMI（非屏蔽中断） | ✅ 即使关中断也可能被 NMI 打断  |

---

### 🧠 二、抢占与调度的关键逻辑

#### 1. `preempt_disable()` / `local_irq_disable()`

* `preempt_disable()`：防止当前代码被**内核抢占**（即当前线程不会被调度走）。
* `local_irq_disable()`：关闭**本 CPU 的中断响应**，防止中断打断当前执行。

#### 2. 调度器的前提条件

调度器必须满足两个条件才会切换进程：

* 当前允许被抢占；
* 存在更高优先级可运行进程。

而如果同时执行了：

```c
local_irq_disable();      // 关中断
preempt_disable();        // 关内核抢占
```

就相当于屏蔽了：

* 硬件中断（无法进入调度的时钟中断等）；
* 抢占调度点（不响应 `schedule()` 时机）；

**因此当前进程不会被换出，直到手动恢复中断或抢占。**

---

### 🔄 三、什么时候会切换进程？

以下情况**即使关闭中断/抢占，仍可能切进程**（罕见但必须知道）：

| 情况                | 是否可能调度 | 原因                          |
| ----------------- | ------ | --------------------------- |
| 主动调用 `schedule()` | ✅ 会切换  | 即使关抢占，调度器照样切走               |
| NMI 非屏蔽中断         | ✅ 会打断  | 不受 `local_irq_disable()` 影响 |
| 配置错误或 bug         | ✅ 风险   | 内核调度器 BUG 或 driver 抢占逻辑错误   |

例如：

```c
local_irq_disable();
preempt_disable();
schedule();  // 主动进入调度器，此时会切换线程
```

这种写法通常是错误的：因为 schedule 需要抢占或自愿睡眠，而却关了抢占。

---

### 🧪 四、在 spinlock 中的表现

`spin_lock()` 内部执行了：

```c
preempt_disable();        // 禁止抢占
lock();                   // 实际自旋加锁
```

如果使用 `spin_lock_irqsave()` 或 `raw_spin_lock_irqsave()`，还会额外关闭中断。

此时：

* 当前 CPU 不会响应软/硬中断；
* 当前任务不会被调度器切走；
* 直到调用 `spin_unlock_irqrestore()` 恢复现场；

---

### 🔚 五、总结

#### ❓“关中断 + 关抢占还会切换进程吗？”

> ✅ **一般不会。**

这正是我们使用这些手段的原因 —— 为了临界区不被中断、不被调度，保证上下文一致性（如锁操作、原子操作等）。

#### ⚠️ 但要注意

* 如果主动调用 `schedule()`，就算关了抢占，中断，也会被调度；
* NMI 无法屏蔽，因此极端情况下仍有不可预测行为；
* 永远不要让关中断+关抢占的临界区太大，否则会影响系统响应性能；

---

## 内核线程为什么默认“不被抢占”？

内核线程在默认情况下表现出“不被抢占”行为，这一特性并非来自单一机制，而是由 **内核抢占模型**、**调度策略** 和 **关抢占机制** 三者共同作用的结果。具体实现如下：

---

### 一、内核抢占模型（Kernel Preemption Model）

Linux 支持不同的抢占模式，通过编译选项控制：

#### 1. CONFIG\_PREEMPT\_NONE（非抢占内核）

* **行为**：内核线程不会被强制抢占，除非：

  * 主动调用 `schedule()` / `cond_resched()`
  * 发生阻塞（如等待锁或 I/O）
* **适用场景**：追求吞吐性能，如服务器场景

#### 2. CONFIG\_PREEMPT\_VOLUNTARY（自愿抢占）

* **行为**：仅在显式调度点（如 `might_sleep()`）可能让出 CPU，不会强制抢占

#### 3. CONFIG\_PREEMPT（完全抢占内核）

* **行为**：允许更高优先级任务抢占正在运行的内核线程
* **限制**：

  * **关抢占区域**（如持自旋锁、RCU 临界区、中断上下文）仍不可被抢占

---

### 二、调度策略（Scheduling Class）

#### 1. SCHED\_NORMAL（CFS，完全公平调度）

* 内核线程通常不会被强制抢占，原因包括：

  * 无主动调度点就不会被 CFS 抢占
  * 高优先级（如 `nice = -19`）降低被抢占概率

#### 2. SCHED\_FIFO / SCHED\_RR（实时调度）

* **行为**：只会被优先级更高的实时任务抢占
* **应用**：如 `migration/`、`watchdog/`、线程化中断（`irq/XX`）

---

### 三、关键路径中的“关抢占”机制

即使启用 CONFIG\_PREEMPT，内核也会在关键区域手动关闭抢占，保护一致性：

| 机制                | 效果                          |
| ----------------- | --------------------------- |
| `spin_lock()`     | 隐含 `preempt_disable()`，禁止抢占 |
| `rcu_read_lock()` | 保证 RCU 区间内不可被抢占             |
| 中断上下文             | 默认不可被抢占                     |

示例：

```c
spin_lock(&mylock);   // 禁止抢占
... 临界区 ...
spin_unlock(&mylock); // 恢复抢占
```

---

### 四、用户进程 vs. 内核线程的抢占时机

* **用户进程**：在系统调用或中断返回前检查 `need_resched`，若为 1 可立即被抢占
* **内核线程**：不会自动响应 `need_resched`，除非主动进入调度点（如 `cond_resched()`）

---

### 五、如何让内核线程可以被抢占？

* **启用 CONFIG\_PREEMPT**
* **主动让出 CPU**（在循环中加入 `cond_resched()`）：

  ```c
  while (!kthread_should_stop()) {
      do_work();
      cond_resched(); // 允许调度器调度其他任务
  }
  ```

* **避免使用实时调度类（SCHED\_FIFO）或适当降低优先级**

---

### 六、总结：为何内核线程默认“不被抢占”？

| 机制/配置                      | 抢占行为               |
| -------------------------- | ------------------ |
| CONFIG\_PREEMPT\_NONE      | 除非主动让出，否则不抢占       |
| CONFIG\_PREEMPT\_VOLUNTARY | 仅在调度点可能被抢占         |
| CONFIG\_PREEMPT            | 可被抢占，但关键路径除外       |
| SCHED\_NORMAL              | 依赖主动调度点，不会强制剥夺 CPU |
| SCHED\_FIFO                | 仅被更高优先级实时线程抢占      |
| preempt\_disable()         | 明确禁止抢占             |

**最终结论**：
内核线程的“不被抢占”并非天然特性，而是由 **内核编译配置 + 调度类 + 手动关抢占** 多重机制保障的运行策略。

---
