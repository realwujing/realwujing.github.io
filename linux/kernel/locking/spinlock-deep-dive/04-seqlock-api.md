# 第4篇：seqlock 与全量 API 参考 — 序列锁、调用链与选择决策树

> 源码：`include/linux/seqlock.h` `include/linux/spinlock.h` `include/linux/spinlock_api_smp.h` `kernel/locking/spinlock.c` | 头文件：`include/linux/seqlock_types.h`

系列目录：[spin_lock 内核源码深度解析](./README.md)

---

## 1. seqlock — 序列锁

seqlock 是 `spinlock_t` + `seqcount_t` 的组合——写者拿 spinlock 后递增序号，读者不持任何锁，通过序号校验一致性。

### 1.1 数据结构

```c
// include/linux/seqlock_types.h:84-92
context_lock_struct(seqlock) {
    seqcount_spinlock_t seqcount;              // line 89 — 序号 + spinlock 指针
    spinlock_t lock;                           // line 90 — 真正自旋锁
};

// include/linux/seqlock_types.h:33-38
typedef struct seqcount {
    unsigned sequence;                         // line 34 — 奇 = 写者活跃, 偶 = 安全
} seqcount_t;
```

### 1.2 写者路径

```c
// include/linux/seqlock.h:877-882 — write_seqlock
#define write_seqlock(sl)                                     \
    do {                                                      \
        spin_lock(&(sl)->lock);                               \
        do_write_seqcount_begin(&(sl)->seqcount.seqcount);    \
    } while (0)

// do_write_seqcount_begin_nested 展开:
//   seqcount_acquire(&s->dep_map, 0, 0, ...)  [lockdep]
//   do_raw_write_seqcount_begin(s):
//     s->sequence++                             ★ 递增为奇数
//     smp_wmb()                                 ★ 写屏障
```

**关键**：sequence++ 后变成奇数，向所有读者宣告"写者活跃"。

```c
// include/linux/seqlock.h:891-896 — write_sequnlock
#define write_sequnlock(sl)                                   \
    do {                                                      \
        do_write_seqcount_end(&(sl)->seqcount.seqcount);      \
        spin_unlock(&(sl)->lock);                             \
    } while (0)

// do_write_seqcount_end 展开:
//   do_raw_write_seqcount_end(s):
//     smp_wmb()                                 ★ 写屏障
//     s->sequence++                             ★ 递增回偶数
//   seqcount_release(&s->dep_map, ...)
```

**关键**：两次 `smp_wmb()` 保证数据修改在 sequence 变化之间对读者可见。

### 1.3 读者路径

```c
// include/linux/seqlock.h:835 — read_seqbegin
#define read_seqbegin(sl)   read_seqcount_begin(&(sl)->seqcount)

// include/linux/seqlock.h:289-301 — read_seqcount_begin
static inline unsigned __read_seqcount_begin(const seqcount_t *s)
{
    unsigned ret;
repeat:
    ret = READ_ONCE(s->sequence);              // 读序号
    if (unlikely(ret & 1)) {                   // ★ 奇数 = 写者活跃 → 等一下
        cpu_relax();
        goto repeat;
    }
    smp_load_acquire(&s->sequence);            // acquire barrier
    return ret;                                // 返回偶数序号
}

// include/linux/seqlock.h:406-407 — 读者校验
static inline int read_seqcount_retry(const seqcount_t *s, unsigned start)
{
    smp_rmb();                                 // ★ 配对的读屏障
    return READ_ONCE(s->sequence) != start;    // 序号变了 = 写者介入了 = 重试
}
```

**读者使用范式**：

```c
do {
    seq = read_seqbegin(&seqlock);           // 读偶数序号
    // 读共享数据（无锁，很快）
} while (read_seqretry(&seqlock, seq));      // 序号变了？重来
```

### 1.4 时序图

```
Writer:                    Reader:
                          seq = read_seqbegin() → 2 (偶数, OK)
                          // 开始读数据
  spin_lock()
  seqcount++ → 3 (奇数)
  smp_wmb()
  // 修改数据
  smp_wmb()
  seqcount++ → 4 (偶数)
  spin_unlock()
                          // 读数据结束
                          read_seqretry → seq(4) != start(2) → 重试!
                          seq = read_seqbegin() → 4 (偶数, OK)
                          // 再读已更新数据
                          read_seqretry → seq(4) == start(4) → 成功!
```

### 1.5 seqlock 适用场景

| ✅ 适合 | ❌ 不适合 |
|---------|---------|
| 大量读，极少写 | 写入频繁 |
| 读者临界区非常短（纳秒级） | 读者需要反复重试导致性能降级 |
| 不需要精确的 Read-Copy Update | 需要保证 strict 数据一致性 |
| 典型：jiffies, timekeeping | |

---

## 2. 全量 API 速查表

### 2.1 spin_lock 核心 API（spinlock.h）

| API | Line | 效果 |
|-----|------|------|
| `spin_lock_init(l)` | 321 | 初始化 spinlock_t |
| `spin_lock(l)` | 339 | preempt_disable + 自旋 |
| `spin_lock_irq(l)` | 369 | local_irq_disable + 自旋 |
| `spin_lock_irqsave(l,f)` | 375 | local_irq_save + 自旋, 返回 flags |
| `spin_lock_bh(l)` | 345 | local_bh_disable + 自旋 |
| `spin_trylock(l)` | 351 | 非阻塞尝试 |
| `spin_trylock_irqsave(l,f)` | 423 | 关中断 + 尝试 |
| `spin_unlock(l)` | 387 | preempt_enable |
| `spin_unlock_irqrestore(l,f)` | 405 | 恢复中断 + preempt_enable |
| `spin_unlock_bh(l)` | 393 | local_bh_enable + preempt_enable |
| `spin_is_locked(l)` | 448 | 查询锁状态 |
| `spin_is_contended(l)` | 453 | 是否有等待者 |

### 2.2 raw_spin_lock API（spinlock.h）

| API | Line | 效果 |
|-----|------|------|
| `raw_spin_lock_init(l)` | 104 | 初始化 |
| `raw_spin_lock(l)` | 218 | 自旋, 无 lockdep |
| `raw_spin_lock_irqsave(l,f)` | 242 | 关中断 + 自旋 |
| `raw_spin_lock_bh(l)` | 276 | 关 softirq + 自旋 |
| `raw_spin_trylock(l)` | 216 | 尝试 |
| `raw_spin_unlock(l)` | 277 | 释放 |
| `raw_spin_unlock_irqrestore(l,f)` | 280 | 恢复中断 + 释放 |

### 2.3 rwlock API（rwlock.h）

| API | Line | 效果 |
|-----|------|------|
| `read_lock(l)` | 66 | 读者锁 |
| `write_lock(l)` | 65 | 写者锁（等所有读者） |
| `read_trylock(l)` | 62 | 读者尝试 |
| `write_trylock(l)` | 63 | 写者尝试 |
| `read_lock_irqsave(l,f)` | 76 | 关中断 + 读者 |
| `write_lock_irqsave(l,f)` | 81 | 关中断 + 写者 |
| `read_lock_bh(l)` | 103 | 关 softirq + 读者 |
| `read_unlock(l)` | 106 | 读者释放 |
| `write_unlock(l)` | 107 | 写者释放 |

### 2.4 seqlock API（seqlock.h）

| API | Line | 效果 |
|-----|------|------|
| `seqlock_init(sl)` | 816 | spinlock_init + seqcount 初始化 |
| `read_seqbegin(sl)` | 835 | 读偶数序号 |
| `read_seqretry(sl, start)` | 852 | 序号变了? |
| `write_seqlock(sl)` | 877 | spin_lock + seqcount++ |
| `write_sequnlock(sl)` | 891 | seqcount++ + spin_unlock |
| `write_seqlock_irqsave(sl,f)` | 975 | 关中断 + 写锁 |
| `write_seqlock_bh(sl)` | 905 | 关 softirq + 写锁 |

---

## 3. spin_lock 完整调用链

```
spin_lock(lock)                                   [spinlock.h:339]
  → raw_spin_lock(&lock->rlock)                   [spinlock.h:339]
    → _raw_spin_lock(lock)                        [spinlock.c:156]
      → __raw_spin_lock(lock)                     [spinlock_api_smp.h:154]
        → preempt_disable()                        ★ 关抢占
        → spin_acquire(&lock->dep_map, ...)         ★ lockdep 记录
        → LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock)
          → do_raw_spin_lock(lock)                [spinlock.h:184]
            → arch_spin_lock(&lock->raw_lock)     [spinlock.h:187]
              → queued_spin_lock(lock)            [qspinlock.h:107]
                → atomic_try_cmpxchg_acquire(0→1)  ★ FAST PATH
                → queued_spin_lock_slowpath()     [qspinlock.c:130] ★ SLOW

spin_unlock(lock)                                 [spinlock.h:387]
  → __raw_spin_unlock(lock)                      [spinlock_api_smp.h:164]
    → spin_release(&lock->dep_map, ...)            ★ lockdep
    → do_raw_spin_unlock(lock)                   [spinlock.h:202]
      → arch_spin_unlock(&lock->raw_lock)         ★ qspinlock
        → smp_store_release(&lock->locked, 0)
    → preempt_enable()                             ★ 开抢占
```

---

## 4. 锁叠加规则

```
spin_lock()           → preempt_disable()
spin_lock_bh()        → local_bh_disable()  + preempt_disable()
spin_lock_irq()       → local_irq_disable() + preempt_disable()
spin_lock_irqsave()   → local_irq_save()    + preempt_disable()
```

---

## 5. 选择决策树

```
需要锁吗?
├─ 临界区可以睡眠?
│    └─ 用 mutex / rwsem
├─ 硬中断上下文?
│    ├─ 需要性能极致 → raw_spin_lock_irqsave
│    └─ 一般场景 → spin_lock_irqsave
├─ softirq / tasklet?
│    └─ spin_lock_bh (防止同 CPU softirq 抢占)
├─ 进程上下文, 无中断竞争?
│    └─ spin_lock
├─ 需要 NMI 安全?
│    └─ raw_spin_lock + 硬件原子操作
├─ 大量读, 极少写?
│    ├─ 读临界区很轻 → seqlock
│    └─ 读临界区较长 → rwlock
└─ PREEMPT_RT 且要求强制自旋?
     └─ raw_spin_lock (不会被替换为 rt_mutex)
```

---

## 6. 系列结语

4 篇文章从 `arch_spinlock_t` → `raw_spinlock_t` → `spinlock_t` 三层类型体系起步，深入 qspinlock 慢路径的 pending/MCS 队列/锁传递，再到 PREEMPT_RT 下 spinlock 变身 rt_mutex 的睡眠行为与 rwlock 公平设计，最后覆盖 seqlock 的序列一致性机制与全量 API 参考表。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-types | 三层类型体系 + 初始化 | `spinlock_types.h` `qspinlock_types.h` |
| 02-slowpath | qspinlock 慢路径 7 步 | `qspinlock.c:130-371` |
| 03-rt-rwlock | PREEMPT_RT 睡眠 + rwlock | `spinlock_rt.c:38-86` `qrwlock.h` |
| 04-seqlock-api | seqlock + 全量 API + 决策树 | `seqlock.h` `spinlock.h` |

配合 `spin_lock变体对比.md` 中的对比表和使用场景，现拥有完整的 Linux 自旋锁知识图谱。
