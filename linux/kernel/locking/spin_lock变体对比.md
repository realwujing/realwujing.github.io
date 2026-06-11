# spin_lock 内核实现全景 — 从 fast path 到 PREEMPT_RT

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`include/linux/spinlock*.h` `kernel/locking/spinlock.c` `kernel/locking/qspinlock.c` `arch/x86/include/asm/qspinlock.h`

---

## 1. 三层类型体系

Linux 自旋锁有三层：`arch_spinlock_t` → `raw_spinlock_t` → `spinlock_t`。

```
┌─────────────────────────────────────┐
│          spinlock_t                 │  ← 用户使用
│   union { struct raw_spinlock; }    │     PREEMPT_RT: struct rt_mutex_base
├─────────────────────────────────────┤
│        raw_spinlock_t               │  ← 调试/追踪层
│   arch_spinlock_t raw_lock          │     + debug (magic, owner_cpu, dep_map)
├─────────────────────────────────────┤
│       arch_spinlock_t               │  ← 硬件原子操作
│   qspinlock { val/locked/pending    │     x86/arm64 都用 qspinlock
│               /tail }               │
└─────────────────────────────────────┘
```

```c
// include/linux/spinlock_types.h:17-30 — non-RT spinlock_t
context_lock_struct(spinlock) {
    union {
        struct raw_spinlock rlock;                         // line 19
        struct { u8 __padding[...]; struct lockdep_map; };  // lines 24-25
    };
};
typedef struct spinlock spinlock_t;

// include/linux/spinlock_types_raw.h:14-24 — raw_spinlock_t
context_lock_struct(raw_spinlock) {
    arch_spinlock_t raw_lock;                               // line 15
#ifdef CONFIG_DEBUG_SPINLOCK
    unsigned int magic, owner_cpu;                          // lines 17-18
    void *owner;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;                             // line 21
#endif
};
typedef struct raw_spinlock raw_spinlock_t;

// include/asm-generic/qspinlock_types.h:14-20 — arch_spinlock_t (qspinlock)
typedef struct qspinlock {
    union {
        atomic_t val;                                       // 整个 32-bit 原子字
        struct { u8 locked; u8 pending; };                 // 直接访问低 16 位
        struct { u16 locked_pending; u16 tail; };          // tail = 高 16 位
    };
} arch_spinlock_t;
```

**qspinlock bit 字段**（`include/asm-generic/qspinlock_types.h:67-93`）：

```
Bit:   31..18        17..16      15..8        7..0
    ┌──────────────┬──────────┬──────────┬──────────┐
    │  tail-cpu    │ tail-idx │ pending  │  locked  │
    │  (14-16 bit) │  (2 bit) │ (1-8 bit)│  (8 bit) │
    └──────────────┴──────────┴──────────┴──────────┘
```

- `_Q_LOCKED_OFFSET=0, _Q_LOCKED_BITS=8`（line 67-69）
- `_Q_PENDING_OFFSET=8, _Q_PENDING_BITS=8`（line 73-75，≥16K CPUs 时 1 bit）
- `_Q_TAIL_IDX_OFFSET=16, _Q_TAIL_IDX_BITS=2`（line 81-83）
- `_Q_TAIL_CPU_OFFSET=18`（line 85）
- `_Q_LOCKED_VAL = 1 << 0`，`_Q_PENDING_VAL = 1 << 8`（line 92-93）

---

## 2. 全量 API 一览

### 2.1 spin_lock 变体（`include/linux/spinlock.h`）

| API | Line | 中断 | BH | 保存标志 | 调试 | 非RT实现 |
|-----|------|------|-----|---------|------|---------|
| `spin_lock(l)` | 339 | ❌ | ❌ | ❌ | ✅ | `raw_spin_lock(&l->rlock)` |
| `spin_lock_irq(l)` | 369 | ✅关 | ❌ | ❌ | ✅ | `raw_spin_lock_irq(&l->rlock)` |
| `spin_lock_irqsave(l,f)` | 375 | ✅关 | ❌ | ✅ | ✅ | `raw_spin_lock_irqsave(spinlock_check(l),f)` |
| `spin_lock_bh(l)` | 345 | ❌ | ✅关 | ❌ | ✅ | `raw_spin_lock_bh(&l->rlock)` |
| `spin_trylock(l)` | 351 | ❌ | ❌ | ❌ | ✅ | `raw_spin_trylock(&l->rlock)` |
| `spin_unlock(l)` | 387 | ❌ | ❌ | ❌ | ✅ | `raw_spin_unlock(&l->rlock)` |
| `spin_unlock_irq(l)` | 399 | ✅开 | ❌ | ❌ | ✅ | `raw_spin_unlock_irq(&l->rlock)` |
| `spin_unlock_irqrestore(l,f)` | 405 | ✅恢复 | ❌ | ✅ | ✅ | 内联 `raw_spin_unlock_irqrestore` |
| `spin_unlock_bh(l)` | 393 | ❌ | ✅开 | ❌ | ✅ | `raw_spin_unlock_bh(&l->rlock)` |
| `spin_is_locked(l)` | 448 | ❌ | ❌ | ❌ | ✅ | `raw_spin_is_locked(&l->rlock)` |

### 2.2 raw_spin_lock 变体（`include/linux/spinlock.h`）

与上表等价，只是**不含 lockdep 调试**，底层调用 `_raw_spin_*` 系列。

| API | Line | 实现 |
|-----|------|------|
| `raw_spin_lock(l)` | 218 | `_raw_spin_lock(l)` |
| `raw_spin_lock_irqsave(l,f)` | 242-273 | `f = _raw_spin_lock_irqsave(l)` |
| `raw_spin_lock_irq(l)` | 275 | `_raw_spin_lock_irq(l)` |
| `raw_spin_lock_bh(l)` | 276 | `_raw_spin_lock_bh(l)` |
| `raw_spin_trylock(l)` | 216 | `_raw_spin_trylock(l)` |

---

## 3. 实现细节 — `_raw_spin_lock` 的全调用链

### 3.1 Non-debug SMP 路径

```c
// kernel/locking/spinlock.c:156-160 — 外部实现
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock)
{
    __raw_spin_lock(lock);
}
EXPORT_SYMBOL(_raw_spin_lock);

// include/linux/spinlock_api_smp.h:154-162 — lockdep 关闭时内联
static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
    preempt_disable();                                      // line 157
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);           // line 158 — lockdep
    LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock); // line 159-160
}

// include/linux/spinlock.h:184-189 — 直接调用 arch 层
static inline void do_raw_spin_lock(raw_spinlock_t *lock) __acquires(lock)
{
    __acquire(lock);                                        // line 186 — sparse
    arch_spin_lock(&lock->raw_lock);                        // line 187
    mmiowb_spin_lock();                                     // line 189 — mmiowb
}
```

### 3.2 spin_lock_irqsave 完整路径

```c
// include/linux/spinlock_api_smp.h:125-135
static inline unsigned long __raw_spin_lock_irqsave(raw_spinlock_t *lock)
{
    unsigned long flags;
    local_irq_save(flags);                                  // ★ 关中断 + 保存
    preempt_disable();                                      // ★ 关抢占
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
    LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
    return flags;                                           // 返回保存的中断标志
}
```

### 3.3 spin_lock_bh 完整路径

```c
// include/linux/spinlock_api_smp.h:146-152
static inline void __raw_spin_lock_bh(raw_spinlock_t *lock)
{
    __local_bh_disable_ip(_RET_IP_, SOFTIRQ_LOCK_OFFSET);   // ★ 关 softirq
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
    LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}
```

`local_bh_disable()` 通过递增 per-CPU 的 `__softirq_pending` 计数器禁止 softirq 在当前 CPU 上执行，但**不屏蔽硬中断**。

### 3.4 trylock 路径

```c
// include/linux/spinlock_api_smp.h:86-96
static inline int __raw_spin_trylock(raw_spinlock_t *lock)
{
    preempt_disable();
    if (do_raw_spin_trylock(lock)) {                         // 原子 try
        spin_acquire(&lock->dep_map, 0, 1, _RET_IP_);        // lockdep
        return 1;
    }
    preempt_enable();
    return 0;
}
```

---

## 4. qspinlock 慢路径 — `queued_spin_lock_slowpath`

### 4.1 Fast path（`include/asm-generic/qspinlock.h`）

```c
// qspinlock.h:107-116
static __always_inline void queued_spin_lock(struct qspinlock *lock)
{
    u32 val = 0;
    if (likely(atomic_try_cmpxchg_acquire(&lock->val, &val, _Q_LOCKED_VAL)))
        return;   // ★ fast: 无人持锁, 直接设为 0x01 返回值了
    queued_spin_lock_slowpath(lock, val);   // ★ slow
}

// qspinlock.h:123-130
static __always_inline void queued_spin_unlock(struct qspinlock *lock)
{
    smp_store_release(&lock->locked, 0);   // ★ 写 0 释放锁
}
```

### 4.2 Slow path 状态机

锁字状态转换（`kernel/locking/qspinlock.c:116-129`）：

```
uncontended:  (0,0,0) → fast path → (0,0,1)
pending:      (0,1,1) → 自旋等锁释放 → (0,1,0) → 设 locked → (0,0,1)
uncontended-q:(n,x,y) → 队头醒来 → (n,0,0) → fast → (0,0,1)
contended-q:  (*,x,y) → 队头醒来 → (*,0,0) → set_locked → (*,0,1)
```

### 4.3 Slow path 步骤（`kernel/locking/qspinlock.c:130-371`）

**步骤 1**：如果锁字为 `_Q_PENDING_VAL`（即 (0,1,0)，pending 位在等待 locked 位释放），spin 等待最多 `_Q_PENDING_LOOPS` 次（x86 上 512 次，`arch/x86/include/asm/qspinlock.h:14`）：

```c
if (val == _Q_PENDING_VAL) {                                 // line 150
    int cnt = _Q_PENDING_LOOPS;
    val = atomic_cond_read_relaxed(&lock->val, (VAL != _Q_PENDING_VAL) || !cnt--);
}
```

**步骤 2**：如果锁被争抢 (`val & ~_Q_LOCKED_MASK`)，直接进入 MCS 队列（line 159-160）。

**步骤 3**：否则尝试设置 pending 位 — `queued_fetch_set_pending_acquire(lock)`（line 167-168）：
- x86 上用 `LOCK btsl` 原子设置 bit 8（`arch/x86/include/asm/qspinlock.h:16-31`）

**步骤 4**：如果 locked 位还在（(0,1,1)），等它释放（line 196-197）：
```c
smp_cond_load_acquire(&lock->locked, !VAL);
```

**步骤 5**：`clear_pending_set_locked(lock)` — 清除 pending 位同时设置 locked 位（line 204），完成 (0,1,0 → 0,0,1)。

**步骤 6（队列路径）**：如果第三步发现 pending 已经被别人占了，走 MCS 队列：
- 分配 per-CPU MCS node（line 215-218）
- `old = xchg_tail(lock, tail)` — 原子替换 tail 指针进入队列（line 277）
- 如果队列非空：`WRITE_ONCE(prev->next, node)` 链接到前驱（line 289），然后 `arch_mcs_spin_lock_contended(&node->locked)` 等待前驱释放（line 301）
- 队列头部醒来后：`atomic_cond_read_acquire(&lock->val, !(VAL & _Q_LOCKED_PENDING_MASK))` 等 locked+pending 都清零（line 328）
- 争抢锁：`atomic_try_cmpxchg(lock->val, &val, _Q_LOCKED_VAL)`（line 361）
- 如果有后续节点：`smp_cond_load_relaxed(&node->next, VAL)` 等它出现（line 367），然后 `arch_mcs_spin_unlock_contended(&next->locked)` 传递锁（line 371）

### 4.4 qspinlock vs ticket spinlock

| | ticket spinlock | qspinlock |
|---|---|---|
| **公平性** | FIFO (ticket 号) | MCS 队列 + pending 位 |
| **cache 伸缩性** | 差（所有 CPU 读同一个 `head`） | 好（每个 CPU 自旋自己的 `node->locked`） |
| **NUMA 效率** | 低 | 高（MCS node per-CPU） |
| **实现复杂度** | 低（<50 行） | 高（350+ 行 slow path） |
| **Linux 引入** | 2.6.25 | 4.2 |

---

## 5. PREEMPT_RT — spin_lock 变身 rt_mutex

### 5.1 spinlock_t 结构变化

```c
// include/linux/spinlock_types.h:46-57 — PREEMPT_RT
context_lock_struct(spinlock) {
    struct rt_mutex_base lock;                              // line 52
};
// spinlock_t 变成了 rt_mutex_base 的薄包装
```

### 5.2 spin_lock 实现（`kernel/locking/spinlock_rt.c`）

```c
// spinlock_rt.c:54-58 — RT spin_lock
void __sched rt_spin_lock(spinlock_t *lock)
{
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);           // line 56 — lockdep
    __rt_spin_lock(lock);                                    // line 57
}

// spinlock_rt.c:46-52 — __rt_spin_lock
static __always_inline void __rt_spin_lock(spinlock_t *lock)
{
    rtlock_might_resched();                                 // line 48
    rtlock_lock(&lock->lock);                               // line 49
    rcu_read_lock();                                        // line 50
    migrate_disable();                                      // line 51
}
```

**核心差异**：
- **`rcu_read_lock()`**：PREEMPT_RT 下 spin_lock 隐式进入 RCU 读临界区，保证数据一致性
- **`migrate_disable()`**：禁止线程迁移到其他 CPU（替代 preempt_disable 的不可抢占语义）
- **`rtlock_lock()`**：实际用 `rt_mutex_cmpxchg_acquire(rtm, NULL, current)` 尝试获取，失败走 `rtlock_slowlock`

### 5.3 中断标志被忽略

```c
// include/linux/spinlock_rt.h:99-104 — RT spin_lock_irqsave
#define spin_lock_irqsave(lock, flags)   \
    do {                                 \
        flags = 0;                       \   // ★ 标志永远为 0
        spin_lock(lock);                  \   // ★ 不关中断
    } while (0)
```

PREEMPT_RT 下的 spin_lock 永远**不关中断**——因为自旋锁已经变成睡眠锁（`rt_mutex`），线程化中断处理程序可以独立运行。硬中断保留直通路径，只使用 `raw_spin_lock`。

---

## 6. rwlock — 读写锁

公平读写锁（qrwlock）用 `arch_spinlock_t wait_lock` 保证写者不饿死。

```c
// include/asm-generic/qrwlock_types.h:13-27
typedef struct qrwlock {
    union {
        atomic_t cnts;                     // 组合 reader count + writer state
        struct { u8 wlocked; u8 __lstate[3]; };
    };
    arch_spinlock_t wait_lock;             // 公平锁
} arch_rwlock_t;
```

**状态编码**（`include/asm-generic/qrwlock.h:27-31`）：
- `_QW_WAITING = 0x100` — 写者在等待
- `_QW_LOCKED = 0x0ff` — 写者持有锁
- `_QW_WMASK  = 0x1ff` — 写者掩码
- `_QR_SHIFT  = 9` — 读者计数器偏移
- `_QR_BIAS   = 1U << 9` — 基本读者计数单位

**rwlock API**（`include/linux/rwlock.h`）：

| API | Line | 行为 |
|-----|------|------|
| `read_lock(l)` | 66 | `_raw_read_lock(l)` — preempt_disable + 递增读者计数 |
| `write_lock(l)` | 65 | `_raw_write_lock(l)` — 先拿 wait_lock，设 _QW_LOCKED |
| `read_lock_irqsave(l,f)` | 76 | `f = _raw_read_lock_irqsave(l)` |
| `write_lock_bh(l)` | 105 | `_raw_write_lock_bh(l)` |

---

## 7. seqlock — 序列锁

seqlock 是 `spinlock_t` + `seqcount_t` 的组合——写者拿 spinlock 后递增序号，读者不持锁，通过序号校验一致性。

```c
// include/linux/seqlock_types.h:84-92
context_lock_struct(seqlock) {
    seqcount_spinlock_t seqcount;                             // line 89
    spinlock_t lock;                                          // line 90
};

// include/linux/seqlock_types.h:33-38
typedef struct seqcount {
    unsigned sequence;                                        // line 34
} seqcount_t;
```

### 7.1 写者路径

```c
// include/linux/seqlock.h:877-882 — write_seqlock
#define write_seqlock(sl)                                     \
    do {                                                      \
        spin_lock(&(sl)->lock);                               \
        do_write_seqcount_begin(&(sl)->seqcount.seqcount);    \
    } while (0)

// 展开 do_write_seqcount_begin:
//   seqcount_acquire(); s->sequence++; smp_wmb();    ← 奇数号 = 写者活跃

// include/linux/seqlock.h:891-896 — write_sequnlock
#define write_sequnlock(sl)                                   \
    do {                                                      \
        do_write_seqcount_end(&(sl)->seqcount.seqcount);      \
        spin_unlock(&(sl)->lock);                             \
    } while (0)

// 展开 do_write_seqcount_end:
//   smp_wmb(); s->sequence++;                      ← 偶数号 = 写者结束
```

### 7.2 读者路径

```c
// include/linux/seqlock.h:289-301 — 读者读序号
static inline unsigned __read_seqcount_begin(const seqcount_t *s)
{
    unsigned ret;
repeat:
    ret = READ_ONCE(s->sequence);                // line 277
    if (unlikely(ret & 1)) {                     // ★ 奇数 = 写者活跃
        cpu_relax();
        goto repeat;
    }
    smp_load_acquire(&s->sequence);              // line 281 — acquire barrier
    return ret;                                  // 返回偶数序号
}

// include/linux/seqlock.h:406-407 — 读者校验
static inline int read_seqcount_retry(const seqcount_t *s, unsigned start)
{
    smp_rmb();                                   // ★ write_seqcount_end 的 smp_wmb 配对的 read barrier
    return READ_ONCE(s->sequence) != start;      // 序号变了 = 写者介入 = 重试
}
```

**简洁版**：读者循环直到读到**偶数**序号且重读不变。

---

## 8. 锁叠加规则速查

### 8.1 哪个锁关了什么

```
spin_lock()           → preempt_disable()
spin_lock_bh()        → local_bh_disable()  + preempt_disable()
spin_lock_irq()       → local_irq_disable() + preempt_disable()
spin_lock_irqsave()   → local_irq_save()    + preempt_disable()
```

### 8.2 raw 变体没有调试逻辑

```
raw_spin_lock()       → preempt_disable() + arch_spin_lock()
                        ← 没有 spin_acquire/lockdep
```

### 8.3 PREEMPT_RT 下行为变化

```
非RT                PREEMPT_RT
─────────────────   ─────────────────────────────────
spin_lock           rt_mutex（可睡眠）+ rcu_read_lock + migrate_disable
spin_lock_irqsave   spin_lock（忽略 flags，不关中断）
raw_spin_lock       preempt_disable + arch_spin_lock（不变）
```

---

## 9. 选择决策树

```
需要锁吗?
├─ 临界区可以睡眠?
│    └─ 用 mutex / rwsem
├─ 硬中断上下文?
│    ├─ 需要性能极致 → raw_spin_lock_irqsave
│    └─ 一般场景 → spin_lock_irqsave
├─ softirq/tasklet?
│    └─ spin_lock_bh  (防止同CPU softirq抢占)
├─ 进程上下文，无中断竞争?
│    └─ spin_lock
├─ 需要 NMI 安全?
│    └─ raw_spin_lock + 硬件原子操作
└─ PREEMPT_RT 且要求强制自旋?
     └─ raw_spin_lock (不会被替换为 rt_mutex)
```

---

## 10. 总结

| 层级 | 代表 | 核心行为 | 适用 |
|------|------|---------|------|
| `spin_lock` | 通用锁 | 非RT: preempt_disable + 自旋；RT: rt_mutex 睡眠 | 99% 场景 |
| `raw_spin_lock` | 无调试锁 | 同上但不走 lockdep，RT 也强制自旋 | 调度器、RCU、硬中断 |
| `arch_spin_lock` | 硬件锁 | qspinlock 原子操作 | 只被 raw_spin_lock 内部调用 |
| `spin_lock_irqsave` | 中断安全 | 关中断 + 保存标志 + 自旋 | 中断上下文 |
| `spin_lock_bh` | BH 安全 | 关 softirq + 自旋 | 网络层、定时器 |
| `seqlock` | 序列锁 | 读者无锁校验序号 | 大量读少量写的结构（jiffies） |
| `rwlock` | 读写锁 | 读者并行，写者独占 | 目标地读远多于写的场景 |
