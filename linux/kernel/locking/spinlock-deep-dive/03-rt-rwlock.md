# 第3篇：PREEMPT_RT 与 rwlock — spin_lock 变身 rt_mutex + 公平读写锁

> 源码：`include/linux/spinlock_rt.h` `kernel/locking/spinlock_rt.c` `include/linux/rwlock.h` `include/asm-generic/qrwlock.h` | 头文件：`include/linux/rwlock_types.h`

系列目录：[spin_lock 内核源码深度解析](./README.md)

---

## 1. PREEMPT_RT 下的 spin_lock — 从自旋到睡眠

在 PREEMPT_RT 内核中，`spin_lock` 不再是忙等待，而是变成了 `rt_mutex`——一个带优先级继承的睡眠锁。`spin_lock_t` 的结构也随之改变：

```c
// include/linux/spinlock_types.h:46-57 — PREEMPT_RT 下的 spinlock_t
context_lock_struct(spinlock) {
    struct rt_mutex_base lock;              // line 52 — ★ 变成了 rt_mutex!
};
```

对比非 RT 版本的 `struct spinlock` 里是 `raw_spinlock rlock`，RT 版本直接换成 `rt_mutex_base`。

### 1.1 spin_lock 的 RT 实现

```c
// include/linux/spinlock_rt.h:42-46
#define spin_lock(lock)    rt_spin_lock(lock)

// kernel/locking/spinlock_rt.c:54-58
void __sched rt_spin_lock(spinlock_t *lock)
{
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);    // line 56 — lockdep
    __rt_spin_lock(lock);                              // line 57
}

// kernel/locking/spinlock_rt.c:46-52
static __always_inline void __rt_spin_lock(spinlock_t *lock)
{
    rtlock_might_resched();                             // line 48
    rtlock_lock(&lock->lock);                           // line 49
    rcu_read_lock();                                    // line 50 — ★ 隐式 RCU
    migrate_disable();                                  // line 51 — ★ 替代 preempt_disable
}
```

三条关键的"非自旋锁"行为：
- **`rtlock_lock()`**：尝试 `rt_mutex_cmpxchg_acquire(rtm, NULL, current)` 快速获取，失败进入 `rtlock_slowlock` — **当前任务会睡眠**
- **`rcu_read_lock()`**：PREEMPT_RT 下 spin_lock 隐式进入 RCU 读临界区，保证数据一致性
- **`migrate_disable()`**：禁止线程迁移到其他 CPU（替代 `preempt_disable` 的不可抢占语义，但允许睡眠）

### 1.2 spin_unlock 的 RT 实现

```c
// kernel/locking/spinlock_rt.c:78-86
void __sched rt_spin_unlock(spinlock_t *lock)
{
    spin_release(&lock->dep_map, _RET_IP_);              // lockdep
    migrate_enable();                                    // ★ 恢复迁移
    rcu_read_unlock();                                   // ★ 退出 RCU
    if (unlikely(!rt_mutex_cmpxchg_release(&lock->lock, current, NULL)))
        rt_mutex_slowunlock(&lock->lock);                // slow path
}
```

### 1.3 中断标志被忽略 — 最反直觉的一点

```c
// include/linux/spinlock_rt.h:99-104
#define spin_lock_irqsave(lock, flags)   \
    do {                                 \
        flags = 0;                       \   // ★ 标志永远为 0！
        spin_lock(lock);                  \   // ★ 不关中断！
    } while (0)

// include/linux/spinlock_rt.h:119-123
#define spin_unlock_irq(lock)                              \
    do {                                                   \
        rt_spin_unlock(lock);                              \   // ★ 不开中断！
    } while (0)
```

为什么能这么干？因为 PREEMPT_RT 下**硬中断全部线程化了**——中断处理程序变成 `irq/XX` 内核线程，正常睡眠锁不会阻塞它们。只有真正需要忙等待的地方才用 `raw_spin_lock`。

### 1.4 rtlock_lock 的快速/慢速路径

```c
// kernel/locking/spinlock_rt.c:38-44
static __always_inline void rtlock_lock(struct rt_mutex_base *rtm)
{
    if (likely(rt_mutex_cmpxchg_acquire(rtm, NULL, current)))  // fast
        return;
    rtlock_slowlock(rtm);                                       // slow → sleep
}
```

快速路径：原子 CAS 将锁的 owner 设为自己。慢速路径：入队 + 睡眠 + 等 wakeup。

---

## 2. rwlock — 公平读写锁（qrwlock）

### 2.1 数据结构

```c
// include/asm-generic/qrwlock_types.h:13-27
typedef struct qrwlock {
    union {
        atomic_t cnts;                     // 组合 reader count + writer state
        struct { u8 wlocked; u8 __lstate[3]; };
    };
    arch_spinlock_t wait_lock;             // 写者排队的自旋锁
} arch_rwlock_t;
```

### 2.2 状态常量（qrwlock.h:27-31）

```c
#define _QW_WAITING    0x100    // 写者在排队等待
#define _QW_LOCKED     0x0ff    // 写者持有锁
#define _QW_WMASK      0x1ff    // 写者掩码（waiting | locked）
#define _QR_SHIFT      9        // reader count 起始位
#define _QR_BIAS       (1U << 9) // 每个读者的计数器单位
```

锁字编码：
```
Bit:   31..9      8          7..0
    ┌──────────┬────────┬───────────┐
    │ reader   │ waiting│  locked   │
    │ count    │ (写排)  │  (写持)   │
    └──────────┴────────┴───────────┘
```

- `locked` byte = 0xff → 写者持有锁
- `waiting` bit = 0x100 → 有写者在等
- reader bits 从 bit 9 开始，每个 reader +1 个单位

### 2.3 read_lock

```c
// include/linux/rwlock.h:66
#define read_lock(lock)    _raw_read_lock(lock)

// include/linux/rwlock_api_smp.h:159-165 — 展开
static inline void __raw_read_lock(rwlock_t *lock)
{
    preempt_disable();                           // 关抢占
    rwlock_acquire_read(&lock->dep_map, ...);    // lockdep
    LOCK_CONTENDED(lock, do_raw_read_trylock, do_raw_read_lock);
}
```

`do_raw_read_lock` 最终调用 `queued_read_lock_slowpath`——如果 `cnts > 0`（写者占锁或有写者在等）+ `cnts` 不变的条件不满足，读者递增 reader count。

### 2.4 write_lock

```c
// include/linux/rwlock.h:65
#define write_lock(lock)    _raw_write_lock(lock)

// 展开：preempt_disable + rwlock_acquire + do_raw_write_lock
// do_raw_write_lock:
//   1. 获取 wait_lock (arch_spin_lock)
//   2. 设 _QW_WAITING (告诉读者"有写者在等"，新读者不能进来)
//   3. 等 reader count 归零 (所有现有读者退出)
//   4. 设 _QW_LOCKED (写者获得锁)
//   5. 释放 wait_lock
```

关键设计：写者在获得 `wait_lock` 后先设 `_QW_WAITING` —— 这让后续读者看到 waiting 位就**不递增计数**，阻塞在 queued_read_lock_slowpath 中。这避免了写者饿死。

### 2.5 rwlock API 一览

| API | rwlock.h line | 行为 |
|-----|--------------|------|
| `read_lock(l)` | 66 | preempt_disable + 递增 reader count |
| `read_trylock(l)` | 62 | 非阻塞尝试 |
| `read_lock_irqsave(l,f)` | 76 | 关中断 + 发 |
| `read_lock_bh(l)` | 103 | 关 softirq |
| `write_lock(l)` | 65 | 设 _QW_WAITING → 等 reader 归零 → 设 _QW_LOCKED |
| `write_trylock(l)` | 63 | 非阻塞尝试 |
| `write_lock_irqsave(l,f)` | 81 | 关中断 + 写锁 |
| `read_unlock(l)` | 106 | 递减 reader count + preempt_enable |
| `write_unlock(l)` | 107 | 清 _QW_LOCKED + preempt_enable |

---

## 3. PREEMPT_RT 下的 rwlock

RT 内核中 `rwlock_t` 也变了：

```c
// include/linux/rwlock_types.h:58-65
context_lock_struct(rwlock) {
    struct rwbase_rt rwbase;         // 读写信号量 + 优先级继承
    atomic_t readers;                 // 读者计数
};
```

`rwbase_rt` 是一个带优先级继承的读写锁——多个读者可并发，写者互斥，和常规 rwlock 语义一致但允许睡眠。

---

## 4. 非 RT vs PREEMPT_RT 对比总结

| | 非 RT 内核 | PREEMPT_RT 内核 |
|---|---|---|
| `spin_lock` | `preempt_disable()` + 自旋 | `rt_mutex` 睡眠 |
| `spin_lock_irqsave` | 关中断 + 自旋 | spin_lock（无视中断掩码） |
| `raw_spin_lock` | 自旋 | **仍然是自旋**（不变！） |
| `rwlock` | 自旋公平读写锁 | `rwbase_rt` 睡眠读写锁 |
| 中断线程化 | 无 | 有（中断变成 `irq/XX` 线程） |
| 最大锁持有时间 | ≤ 微秒 | 可持有毫秒级 |

**核心原则**：PREEMPT_RT 下，**`spin_lock` 可以睡眠**，**`raw_spin_lock` 不可以**。默认使用 `spin_lock` 才能利用 RT 的低延迟特性。`raw_spin_lock` 只留给真正不能睡眠的上下文（硬中断、调度器、NMI）。

---

## 下一篇文章

→ [第4篇：seqlock 与全量 API 参考 — 序列锁、调用链与选择决策树](./04-seqlock-api.md)
