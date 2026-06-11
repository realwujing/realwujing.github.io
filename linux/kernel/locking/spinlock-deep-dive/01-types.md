# 第1篇：三层类型体系 — arch_spinlock_t → raw_spinlock_t → spinlock_t

> 源码：`include/linux/spinlock_types.h` (78行) | `include/linux/spinlock_types_raw.h` (74行) | 头文件：`include/asm-generic/qspinlock_types.h` (95行)

系列目录：[spin_lock 内核源码深度解析](./README.md)

---

## 1. 概述

Linux 内核的 spinlock 并非单一数据结构，而是精心设计的三层类型体系。这种分层将“用户接口”、“调试/校验层”、“硬件原语”彻底解耦，使得：

- **架构可移植**：替换底层 `arch_spinlock_t`（从 ticket lock 到 qspinlock）对上层完全透明
- **PREEMPT_RT 兼容**：实时内核下 `spinlock_t` 自动变成 `rt_mutex`，代码无需修改
- **调试可插拔**：lockdep、spinlock debug 通过编译选项注入，不影响发行内核性能

本文将逐层剖析这三层结构：从最深层的 qspinlock 位域布局，到中间层 raw_spinlock 的 lockdep 注解，再到外层 spinlock_t 如何根据 `CONFIG_PREEMPT_RT` 在两套语义之间切换。

---

## 2. 三层架构全景图

```
┌───────────────────────────────────────────────────────────────┐
│                    用户层 API                                  │
│  spin_lock / spin_unlock / spin_lock_irq / spin_lock_bh ...  │
│    include/linux/spinlock.h  (678行)                          │
└────────────────────────┬──────────────────────────────────────┘
                         │ 使用 spinlock_t
                         │
     ┌───────────────────┴──────────────────────┐
     │           spinlock_t                     │
     │    include/linux/spinlock_types.h         │
     │                                          │
     │  ┌─ 非 RT (CONFIG_PREEMPT_RT=n) ──────┐ │
     │  │  union {                           │ │
     │  │    struct raw_spinlock rlock;      │ │
     │  │    ... lockdep_map ...             │ │
     │  │  }                                 │ │
     │  │  line 17-30                        │ │
     │  └────────────────────────────────────┘ │
     │                                          │
     │  ┌─ RT (CONFIG_PREEMPT_RT=y) ─────────┐ │
     │  │  struct rt_mutex_base lock;        │ │
     │  │  ... lockdep_map ...               │ │
     │  │  line 46-57                        │ │
     │  └────────────────────────────────────┘ │
     └───────────────────┬──────────────────────┘
                         │ 非 RT 分支：包装 raw_spinlock_t
                         │
     ┌───────────────────▼──────────────────────┐
     │         raw_spinlock_t                   │
     │    include/linux/spinlock_types_raw.h     │
     │                                          │
     │  ┌─ 核心 ──────────────────────────────┐ │
     │  │  arch_spinlock_t raw_lock;          │ │
     │  │  line 15                            │ │
     │  └─────────────────────────────────────┘ │
     │                                          │
     │  ┌─ 调试 (CONFIG_DEBUG_SPINLOCK) ──────┐ │
     │  │  unsigned int magic; // 0xdead4ead  │ │
     │  │  unsigned int owner_cpu;            │ │
     │  │  void *owner;                       │ │
     │  │  line 17-18                         │ │
     │  └─────────────────────────────────────┘ │
     │                                          │
     │  ┌─ lockdep (CONFIG_DEBUG_LOCK_ALLOC) ─┐ │
     │  │  struct lockdep_map dep_map;        │ │
     │  │  line 21                            │ │
     │  └─────────────────────────────────────┘ │
     │                                          │
     │  line 14-24                              │
     └───────────────────┬──────────────────────┘
                         │
     ┌───────────────────▼──────────────────────┐
     │         arch_spinlock_t                  │
     │   include/asm-generic/qspinlock_types.h   │
     │                                          │
     │  ┌─ qspinlock (SMP 架构) ──────────────┐ │
     │  │  atomic_t val;   (32-bit word)      │ │
     │  │  line 16                            │ │
     │  │                                     │ │
     │  │  位域布局 (little endian):           │ │
     │  │  Bit [7:0]    locked (8 bit)        │ │
     │  │  Bit [15:8]   pending (8 bit)       │ │
     │  │  Bit [17:16]  tail_idx (2 bit)      │ │
     │  │  Bit [31:18]  tail_cpu (14-16 bit)  │ │
     │  │  line 24-31                        │ │
     │  └─────────────────────────────────────┘ │
     │                                          │
     │  line 14-44                              │
     └──────────────────────────────────────────┘
```

---

## 3. 第二层：arch_spinlock_t — 硬件原语

### 3.1 qspinlock 结构体定义

`include/asm-generic/qspinlock_types.h` 定义了 qspinlock 的 C 语言布局：

```c
// include/asm-generic/qspinlock_types.h:14-44
typedef struct qspinlock {
    union {
        atomic_t val;                          // line 16: 整体作为 32 位原子变量访问

#ifdef __LITTLE_ENDIAN
        struct {
            u8  locked;                        // line 25: byte[0] - 锁持有标志
            u8  pending;                       // line 26: byte[1] - 待定位
        };
        struct {
            u16 locked_pending;                // line 29: 低 16 位整体
            u16 tail;                          // line 30: 高 16 位 - 队列尾部编码
        };
#else
        struct {
            u16 tail;                          // line 34: 大端序反向
            u16 locked_pending;                // line 35
        };
        struct {
            u8  reserved[2];                   // line 38
            u8  pending;                       // line 39
            u8  locked;                        // line 40
        };
#endif
    };
} arch_spinlock_t;
```

关键设计意图（`qspinlock_types.h:19-22` 注释）：
> *By using the whole 2nd least significant byte for the pending bit, we can allow better optimization of the lock acquisition for the pending bit holder.*

将 pending 设为完整字节而非单比特，是为了让 pending 持有者的锁获取操作可以利用原子字节写入进行优化。x86 上的 `clear_pending_set_locked` 就利用了这点：一次 `WRITE_ONCE(lock->locked_pending, _Q_LOCKED_VAL)` 即可同时清除 pending 并设置 locked。

### 3.2 32 位字位域布局

`qspinlock_types.h:52-66` 的注释精确描述了位域分布：

```
NR_CPUS < 16K 时 (典型 x86 配置)：

   Bit:  31..........18   17......16   15...........8   7...........0
       ┌──────────────┬───────────┬──────────────┬──────────────┐
       │  tail_cpu    │ tail_idx  │   pending    │    locked    │
       │  (14 bits)   │ (2 bits)  │  (8 bits)    │  (8 bits)    │
       └──────────────┴───────────┴──────────────┴──────────────┘
       高 16 位=tail             低 16 位=locked_pending

NR_CPUS ≥ 16K 时 (超大系统)：

   Bit:  31..........11   10......9   8  7...........0
       ┌──────────────┬───────────┬──┬──────────────┐
       │  tail_cpu    │ tail_idx  │P │   locked    │
       │  (21 bits)   │ (2 bits)  │1b│  (8 bits)    │
       └──────────────┴───────────┴──┴──────────────┘
```

**为什么需要 8 位 locked？**

`kernel/locking/qspinlock.c:56-58` 注释解释了：
> *Even though we only need 1 bit for the lock, we extend it to a full byte to achieve better performance for architectures that support atomic byte write.*

一个完整的字节可以：
- 使用 `smp_store_release(&lock->locked, 0)` 单字节 release 写代替读-改-写操作
- 避免与 tail 字段的 false sharing（同一字节内有多个字段会引发不必要竞争）

### 3.3 位域偏移量和掩码宏

`qspinlock_types.h:69-93` 定义了所有位域常量：

```c
// include/asm-generic/qspinlock_types.h:69-93

#define _Q_LOCKED_OFFSET   0          // line 69: locked 起始于 bit 0
#define _Q_LOCKED_BITS     8          // line 70: locked 占 8 位
#define _Q_LOCKED_MASK     _Q_SET_MASK(LOCKED)  // line 71: 0x000000FF

#define _Q_PENDING_OFFSET  (_Q_LOCKED_OFFSET + _Q_LOCKED_BITS)  // line 73: = 8
#if CONFIG_NR_CPUS < (1U << 14)
#define _Q_PENDING_BITS    8          // line 75: NR_CPUS < 16384 时 pending 占 8 位
#else
#define _Q_PENDING_BITS    1          // line 77: 超大系统 pending 仅占 1 位
#endif
#define _Q_PENDING_MASK    _Q_SET_MASK(PENDING)  // line 79: 0x0000FF00 或 0x00000100

#define _Q_TAIL_IDX_OFFSET (_Q_PENDING_OFFSET + _Q_PENDING_BITS)  // line 81
#define _Q_TAIL_IDX_BITS   2          // line 82: tail_idx 占 2 位，编码 4 个嵌套上下文
#define _Q_TAIL_IDX_MASK   _Q_SET_MASK(TAIL_IDX)

#define _Q_TAIL_CPU_OFFSET (_Q_TAIL_IDX_OFFSET + _Q_TAIL_IDX_BITS)  // line 85
#define _Q_TAIL_CPU_BITS   (32 - _Q_TAIL_CPU_OFFSET)  // line 86: 剩余位全给 CPU 编号
#define _Q_TAIL_CPU_MASK   _Q_SET_MASK(TAIL_CPU)

#define _Q_TAIL_OFFSET     _Q_TAIL_IDX_OFFSET  // line 89
#define _Q_TAIL_MASK       (_Q_TAIL_IDX_MASK | _Q_TAIL_CPU_MASK)  // line 90: 高 16 位

#define _Q_LOCKED_VAL      (1U << _Q_LOCKED_OFFSET)  // line 92: 0x00000001
#define _Q_PENDING_VAL     (1U << _Q_PENDING_OFFSET)  // line 93: 0x00000100 或 0x00000100
```

`_Q_SET_MASK` 宏展开（`qspinlock_types.h:67-68`）：

```c
#define _Q_SET_MASK(type) (((1U << _Q_ ## type ## _BITS) - 1) << _Q_ ## type ## _OFFSET)
// 例如 _Q_SET_MASK(LOCKED) → ((1U << 8) - 1) << 0) → (256 - 1) << 0 → 0xFF
// 例如 _Q_SET_MASK(PENDING) → ((1U << 8) - 1) << 8) → 0xFF << 8 → 0xFF00
```

### 3.4 初始值

```c
// include/asm-generic/qspinlock_types.h:49
#define __ARCH_SPIN_LOCK_UNLOCKED  { { .val = ATOMIC_INIT(0) } }
```

解锁状态下所有位都是 0：`locked=0, pending=0, tail_idx=0, tail_cpu=0`。

### 3.5 典型锁状态值

```
状态                  val 值    二进制 (低 32 位)              含义
──────────────────────────────────────────────────────────────────
unlocked              0x00000000  ...0000 0000 0000 0000 0000  无持有者
locked (fast)         0x00000001  ...0000 0000 0000 0000 0001  bit0=1
pending only          0x00000100  ...0000 0000 0001 0000 0000  bit8=1
locked+pending        0x00000101  ...0000 0000 0001 0000 0001  bit0+bit8
queued (tail set)     0xNNNN0001  ...NNNN NNNN NNNN 0000 0001  队列中有等待者
```

---

## 4. 第三层：raw_spinlock_t — 真正的自旋锁

### 4.1 结构体定义

`include/linux/spinlock_types_raw.h:14-24`：

```c
// include/linux/spinlock_types_raw.h:14-24
context_lock_struct(raw_spinlock) {
    arch_spinlock_t raw_lock;                  // line 15: 核心——架构相关锁
#ifdef CONFIG_DEBUG_SPINLOCK
    unsigned int magic, owner_cpu;             // line 17
    void *owner;                               // line 18
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;                // line 21: lockdep 状态跟踪
#endif
};
typedef struct raw_spinlock raw_spinlock_t;    // line 24
```

`context_lock_struct` 宏来自 `tools/include/linux/compiler-context-analysis.h:13`：
```c
#define context_lock_struct(name, ...) struct __VA_ARGS__ name
```
直接展开为 `struct raw_spinlock { ... };`，仅为静态分析工具提供附加信息。

### 4.2 raw_spinlock 的定位

`raw_spinlock_t` 是内核中**真正意义上的自旋锁**：
- 永远自旋等待，不会睡眠
- 关抢占（`preempt_disable`）
- 在中断保护场景中进一步关中断
- 被 `spinlock_t`（非 RT）包装，也被内核核心代码直接使用

### 4.3 调试字段

**SPINLOCK_MAGIC**（`spinlock_types_raw.h:26`）：
```c
#define SPINLOCK_MAGIC  0xdead4ead    // line 26
```

`0xdead4ead` 是一个经典内核魔数（"dead for read" 的变体）。当 `CONFIG_DEBUG_SPINLOCK=y` 时：
- 初始化时写入 `SPINLOCK_MAGIC`
- 使用时检查魔数，防止对未初始化或已破坏锁的操作
- `owner_cpu` 跟踪锁的持有 CPU，用于检测同一 CPU 的重复加锁

`owner` 初始化为特殊哨兵值（`spinlock_types_raw.h:28`）：
```c
#define SPINLOCK_OWNER_INIT  ((void *)-1L)    // line 28
```

### 4.4 静态初始化器

```c
// include/linux/spinlock_types_raw.h:63-70
#define __RAW_SPIN_LOCK_INITIALIZER(lockname)  \
{                                              \
    .raw_lock = __ARCH_SPIN_LOCK_UNLOCKED,     \  // line 65: arch_spinlock_t 初始值
    SPIN_DEBUG_INIT(lockname)                  \  // line 66: 调试字段
    RAW_SPIN_DEP_MAP_INIT(lockname)               // line 67: lockdep 字段
}

#define __RAW_SPIN_LOCK_UNLOCKED(lockname)         \
    (raw_spinlock_t) __RAW_SPIN_LOCK_INITIALIZER(lockname)  // line 70

#define DEFINE_RAW_SPINLOCK(x)  raw_spinlock_t x = __RAW_SPIN_LOCK_UNLOCKED(x)  // line 72
```

`SPIN_DEBUG_INIT` 展开（`spinlock_types_raw.h:55-58`）：
```c
// #ifdef CONFIG_DEBUG_SPINLOCK
#define SPIN_DEBUG_INIT(lockname)            \
    .magic = SPINLOCK_MAGIC,                 \
    .owner_cpu = -1,                         \
    .owner = SPINLOCK_OWNER_INIT,
```

### 4.5 lockdep wait_type 注解

lockdep 通过 `wait_type_inner` 标记锁的等待类型，用于检测死锁（如持有自旋锁时获取可睡眠锁）。`spinlock_types_raw.h:34-47`：

```c
// include/linux/spinlock_types_raw.h:34-47

// raw_spinlock → LD_WAIT_SPIN：永远自旋，不能在任何持有自旋锁的上下文中睡眠
#define RAW_SPIN_DEP_MAP_INIT(lockname)         \
    .dep_map = {                                \
        .name = #lockname,                      \
        .wait_type_inner = LD_WAIT_SPIN,        \  // line 34: 自旋等待
    }

// spinlock → LD_WAIT_CONFIG：非RT自旋，RT睡眠（依赖配置）
#define SPIN_DEP_MAP_INIT(lockname)             \
    .dep_map = {                                \
        .name = #lockname,                      \
        .wait_type_inner = LD_WAIT_CONFIG,      \  // line 39: 配置依赖等待
    }

// local spinlock → LD_WAIT_CONFIG + per-CPU 标记
#define LOCAL_SPIN_DEP_MAP_INIT(lockname)       \
    .dep_map = {                                \
        .name = #lockname,                      \
        .wait_type_inner = LD_WAIT_CONFIG,      \
        .lock_type = LD_LOCK_PERCPU,            \  // line 46: per-CPU 锁标记
    }
```

**wait_type_inner 层级关系**（lockdep 死锁检测基础）：

```
LD_WAIT_SPIN (最内层)
   ↓  嵌套时只能获取同为 LD_WAIT_SPIN 的锁
LD_WAIT_CONFIG
   ↓  可以获取 LD_WAIT_SPIN 锁，但不能反过来
LD_WAIT_SLEEP (最外层)
   可以获取 LD_WAIT_CONFIG 和 LD_WAIT_SPIN 锁
```

raw_spinlock 标记为 `LD_WAIT_SPIN` 意味着：在持有 raw_spinlock 时，不允许获取任何 `LD_WAIT_CONFIG` 或 `LD_WAIT_SLEEP` 级别的锁——这自然会因为自旋时睡眠而死锁。

---

## 5. 第一层：spinlock_t — 用户接口

`spinlock_t` 是用户代码直接声明的锁类型。它的内部结构根据 `CONFIG_PREEMPT_RT` 有**两种截然不同的实现**——但用户 API 完全统一。

### 5.1 非 RT 路径（默认）

`include/linux/spinlock_types.h:17-30`：

```c
// include/linux/spinlock_types.h:17-30
context_lock_struct(spinlock) {
    union {
        struct raw_spinlock rlock;             // line 19: 包装 raw_spinlock_t

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define LOCK_PADSIZE (offsetof(struct raw_spinlock, dep_map))  // line 22
        struct {
            u8 __padding[LOCK_PADSIZE];        // line 24: 填充至 dep_map 偏移
            struct lockdep_map dep_map;        // line 25: lockdep 映射
        };
#endif
    };
};
```

**巧妙之处在于 union**：

- `rlock` 用于实际的加锁/解锁操作
- `dep_map` 通过 union 的第二个视图覆盖 `rlock.dep_map` 位置
- `LOCK_PADSIZE` 计算出 `dep_map` 在 `raw_spinlock` 中的偏移量，使得两个视图中的 `dep_map` 指向同一内存位置

这样在非 lockdep 配置下 `dep_map` 根本不存在，union 也不会浪费空间；在 lockdep 配置下，`dep_map` 通过 padding 精确对齐到 `raw_spinlock.dep_map` 的位置。

**静态初始化器**（`spinlock_types.h:32-44`）：

```c
// include/linux/spinlock_types.h:32-44
#define ___SPIN_LOCK_INITIALIZER(lockname)    \
    {                                         \
    .raw_lock = __ARCH_SPIN_LOCK_UNLOCKED,    \   // line 34
    SPIN_DEBUG_INIT(lockname)                 \   // line 35
    SPIN_DEP_MAP_INIT(lockname) }                 // line 36

#define __SPIN_LOCK_INITIALIZER(lockname) \
    { { .rlock = ___SPIN_LOCK_INITIALIZER(lockname) } }   // line 39

#define __SPIN_LOCK_UNLOCKED(lockname) \
    (spinlock_t) __SPIN_LOCK_INITIALIZER(lockname)        // line 42

#define DEFINE_SPINLOCK(x)  spinlock_t x = __SPIN_LOCK_UNLOCKED(x)  // line 44
```

展开 `DEFINE_SPINLOCK(my_lock)` 为：

```c
spinlock_t my_lock = (spinlock_t) { {
    .rlock = {
        .raw_lock = __ARCH_SPIN_LOCK_UNLOCKED,  // → { .val = ATOMIC_INIT(0) }
        .magic = SPINLOCK_MAGIC,                // DEBUG
        .owner_cpu = -1,                        // DEBUG
        .owner = (void *)-1L,                   // DEBUG
        .dep_map = { .name = "my_lock", .wait_type_inner = LD_WAIT_SPIN }
    }
} };
```

注意：非 RT 路径下 `spinlock_t` 的初始化器使用了 `RAW_SPIN_DEP_MAP_INIT` 中的 `LD_WAIT_SPIN`，说明即使是用户层 spinlock，在非 RT 下也被当作自旋锁来检测锁依赖。

### 5.2 PREEMPT_RT 路径

`include/linux/spinlock_types.h:46-57`：

```c
// include/linux/spinlock_types.h:46-57
/* PREEMPT_RT kernels map spinlock to rt_mutex */
#include <linux/rtmutex.h>                         // line 49

context_lock_struct(spinlock) {
    struct rt_mutex_base    lock;                  // line 52: 直接是 rt_mutex!
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map      dep_map;               // line 54
#endif
};
typedef struct spinlock spinlock_t;                // line 57
```

在 PREEMPT_RT 下，`spinlock_t` **不再包含任何自旋锁**，而是直接嵌入 `rt_mutex_base`：

```
非 RT:  spinlock_t → raw_spinlock_t → arch_spinlock_t → 原子操作自旋
  RT:  spinlock_t → rt_mutex_base → 睡眠锁（PI 感知的实时互斥锁）
```

这意味着：
- 在 RT 下 `spin_lock()` 会睡眠，而非自旋
- 持锁期间允许抢占（这就是 RT 的关键优势）
- 带有优先级继承（PI），防止优先级反转
- **但 `spin_lock_irqsave()` 在 RT 下仍然关中断**——因为中断上下文本身不能睡眠

**RT 下的静态初始化器**（`spinlock_types.h:59-73`）：

```c
// include/linux/spinlock_types.h:59-73
#define __SPIN_LOCK_UNLOCKED(name)                              \
    {                                                           \
        .lock = __RT_MUTEX_BASE_INITIALIZER(name.lock),         \  // line 61
        SPIN_DEP_MAP_INIT(name)                                 \  // line 62
    }

#define __LOCAL_SPIN_LOCK_UNLOCKED(name)                        \
    {                                                           \
        .lock = __RT_MUTEX_BASE_INITIALIZER(name.lock),         \  // line 67
        LOCAL_SPIN_DEP_MAP_INIT(name)                           \  // line 68
    }

#define DEFINE_SPINLOCK(name)                                   \
    spinlock_t name = __SPIN_LOCK_UNLOCKED(name)                // line 71-72
```

注意 RT 路径使用 `SPIN_DEP_MAP_INIT`（`LD_WAIT_CONFIG`）而非 `RAW_SPIN_DEP_MAP_INIT`（`LD_WAIT_SPIN`）：因为 spinlock 在 RT 下可能睡眠，所以 wait type 是配置依赖的。

---

## 6. 从宏到用户 API 的完整链条

### 6.1 头文件包含关系

```
include/linux/spinlock.h (678行)        ← 用户 #include <linux/spinlock.h>
├── include/linux/spinlock_types.h       ← spinlock_t 定义
│   ├── include/linux/spinlock_types_raw.h   ← raw_spinlock_t 定义
│   │   └── include/asm-generic/qspinlock_types.h  ← arch_spinlock_t = qspinlock
│   ├── (PREEMPT_RT 时) include/linux/rtmutex.h
│   └── include/linux/rwlock_types.h     ← rwlock_t 定义 (line 76)
├── asm/spinlock.h                       ← 架构相关的 fast path 实现
│   └── include/asm-generic/qspinlock.h  ← queued_spin_lock/unlock 内联函数
├── include/linux/spinlock_api_smp.h     ← SMP 下的 _raw_spin_lock 等声明
└── ...                                  ← lockdep、其它锁 API
```

### 6.2 spin_lock 宏展开（非 RT，SMP）

以 x86 qspinlock 为例，`spin_lock()` 的调用链为：

```
spin_lock(&my_lock)                             // include/linux/spinlock.h
  → raw_spin_lock(&my_lock->rlock)
    → _raw_spin_lock(&my_lock->rlock)           // include/linux/spinlock_api_smp.h
      → __raw_spin_lock(&my_lock->rlock)
        → do_raw_spin_lock(&my_lock->rlock)     // 关抢占、lockdep 检查后 →
          → arch_spin_lock(&lock->raw_lock)
            → queued_spin_lock(&lock->raw_lock) // include/asm-generic/qspinlock.h:107
              → atomic_try_cmpxchg_acquire()    // fast path
              → queued_spin_lock_slowpath()     // kernel/locking/qspinlock.c:130
```

spin_unlock 同理：

```
spin_unlock(&my_lock)
  → arch_spin_unlock(&lock->raw_lock)
    → queued_spin_unlock(&lock->raw_lock)       // include/asm-generic/qspinlock.h:123
      → smp_store_release(&lock->locked, 0)     // 单字节 release 写
```

### 6.3 PREEMPT_RT 展开

```
spin_lock(&my_lock)                             // 同一 API
  → rt_spin_lock(&my_lock)
    → rt_mutex_lock(&my_lock->lock)             // 睡眠，带 PI
```

这就是 RT 设计的精髓：**换锁不换 API**。

---

## 7. 完整的类型层次结构 ASCII 图

```
                        ┌─────────────────────────────────────┐
                        │          spin_lock(&lock)            │
                        │          spin_unlock(&lock)          │
                        │          DEFINE_SPINLOCK(name)        │
                        │         (include/linux/spinlock.h)   │
                        └─────────────────┬───────────────────┘
                                          │
                        ┌─────────────────▼───────────────────┐
                        │           spinlock_t                 │
                        │   include/linux/spinlock_types.h     │
                        │                                      │
                        │  ┌───────── CONFIG_PREEMPT_RT ────┐ │
                        │  │                                 │ │
                        │  │  n (默认)           y (RT)      │ │
                        │  │  ┌─────────┐    ┌─────────┐    │ │
                        │  │  │ union { │    │ struct  │    │ │
                        │  │  │  rlock  │    │  rt_    │    │ │
                        │  │  │  dep_   │    │  mutex_ │    │ │
                        │  │  │  map    │    │  base   │    │ │
                        │  │  │ }       │    │ +dep_map│    │ │
                        │  │  └────┬────┘    └────┬────┘    │ │
                        │  │       │              │         │ │
                        │  └───────┼──────────────┼─────────┘ │
                        └──────────┼──────────────┼───────────┘
                                   │              │
              ┌────────────────────┘              └──────────────────────┐
              │                                                          │
              ▼                                                          ▼
┌─────────────────────────┐                              ┌─────────────────────────┐
│    raw_spinlock_t       │                              │   rt_mutex_base         │
│ spinlock_types_raw.h    │                              │ include/linux/rtmutex.h  │
│                         │                              │                          │
│  ┌───────────────────┐  │                              │  - waiter tree          │
│  │ raw_lock          │  │                              │  - owner                │
│  │ (arch_spinlock_t) │  │                              │  - wait_lock            │
│  ├───────────────────┤  │                              │  - PI chain             │
│  │ magic (debug)     │  │                              │                          │
│  │ owner_cpu (debug) │  │                              │  等待时睡眠，支持 PI     │
│  │ owner (debug)     │  │                              │  spin_lock 语义等价于    │
│  ├───────────────────┤  │                              │  mutex_lock              │
│  │ dep_map (lockdep) │  │                              └─────────────────────────┘
│  └────────┬──────────┘  │
│           │              │
└───────────┼──────────────┘
            │
            ▼
┌─────────────────────────────────────────────────────────┐
│              arch_spinlock_t (= qspinlock)               │
│     include/asm-generic/qspinlock_types.h                │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  atomic_t val;  // 32-bit word                    │   │
│  │                                                   │   │
│  │  ┌─────────┬───────────┬──────────┬──────────┐   │   │
│  │  │ bits     │  31...18  │ 17...16  │ 15...8   │...│   │
│  │  │          │  │         │         │          │   │   │
│  │  │ field    │  tail_cpu │ tail_idx│ pending  │   │   │
│  │  │          │  (14-16)  │  (2 bit) │ (1-8 bit)│...│   │
│  │  └─────────┴───────────┴──────────┴──────────┴───┘   │
│  │                                                     │   │
│  │  7...0: locked (8 bit)                              │   │
│  │  locked=0 表示未锁定，locked!=0 表示有持有者         │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  fast path:  atomic_try_cmpxchg_acquire(&val, 0→1)       │
│  slow path:  kernel/locking/qspinlock.c:130              │
│  unlock:     smp_store_release(&locked, 0)               │
└─────────────────────────────────────────────────────────┘
```

---

## 8. 类型设计的工程智慧

### 8.1 分层隔离

| 层次 | 职责 | 可替换性 |
|------|------|----------|
| spinlock_t | 用户接口、PREEMPT_RT 抽象 | 通过 `#ifdef CONFIG_PREEMPT_RT` 切换 |
| raw_spinlock_t | 调试/校验层、lockdep 集成 | 编译选项注入/摘除 |
| arch_spinlock_t | 硬件算法原语 | 架构切换（qspinlock ↔ ticket lock） |

### 8.2 为什么需要 raw_spinlock_t？

直接使用 `arch_spinlock_t` 不是更简单吗？内核需要 raw_spinlock_t 作为中间层的原因：

1. **lockdep 注解不能挂在 `arch_spinlock_t` 上**：架构代码不应该知道 lockdep 的存在
2. **调试魔数 `0xdead4ead`**：帮助快速发现内存破坏，但不应该放在架构层
3. **owner 跟踪**：`owner_cpu` 和 `owner` 用于 `CONFIG_DEBUG_SPINLOCK` 运行时检测
4. **raw_spinlock 语义独立**：`raw_spinlock_t` 有明确定义——总是自旋，总是关抢占。这使得直接使用 `raw_spinlock_t` 的内核核心代码（如调度器、RCU）行为可预测

### 8.3 spinlock_debug 运行时检查

当 `CONFIG_DEBUG_SPINLOCK=y` 时，每次 `spin_lock` 操作都会执行额外的运行时检查：

```c
// include/linux/spinlock_debug.h (逻辑示意)
static inline void debug_spin_lock_before(raw_spinlock_t *lock) {
    SPIN_BUG_ON(lock->magic != SPINLOCK_MAGIC, lock, "bad magic");
    SPIN_BUG_ON(lock->owner == current, lock, "recursion");     // 重入检测
    SPIN_BUG_ON(lock->owner_cpu == raw_smp_processor_id(), lock, "CPU recursion");
}

static inline void debug_spin_lock_after(raw_spinlock_t *lock) {
    lock->owner_cpu = raw_smp_processor_id();
    lock->owner = current;
}
```

这些检查不会出现在发行内核中，但开发阶段极其有用。

---

## 9. 总结

Linux spinlock 的类型体系展示了内核代码设计的核心哲学：

1. **分层抽象**：每一层只关心自己职责范围内的事
2. **编译时多态**：用 `#ifdef` 而非虚函数表，零运行时开销
3. **渐进式调试**：通过编译选项注入不同粒度的调试信息
4. **API 稳定性**：底层实现从 ticket lock 变为 qspinlock、支持 PREEMPT_RT，用户代码一条都不需要改

下一篇文章将深入 `kernel/locking/qspinlock.c`，追踪 qspinlock 获取锁的完整慢路径流程。

---

## 下一篇文章

➡️ [第2篇：qspinlock 慢路径 — pending、MCS 队列与锁传递](./02-slowpath.md)
