# 第2篇：qspinlock 慢路径 — pending、MCS 队列与锁传递

> 源码：`kernel/locking/qspinlock.c` (410行) | `kernel/locking/qspinlock.h` (201行) | 头文件：`include/asm-generic/qspinlock.h` (152行)

系列目录：[spin_lock 内核源码深度解析](./README.md)

---

## 1. 概述

本文聚焦于 qspinlock 最核心的部分——获取锁的完整流程。采用"自顶向下"的分析方式：先看 fast path（一行原子操作），再逐段拆解 slow path 的 7 步状态机，最后深入 MCS 队列与锁传递机制。

核心函数：`queued_spin_lock_slowpath()` (`kernel/locking/qspinlock.c:130`)。

---

## 2. 为什么需要 qspinlock？

### 2.1 ticket lock 的局限

Linux 长期使用 ticket lock。它在低竞争场景下表现良好，但有两个问题：

1. **O(N²) 缓存一致性流量**：每个等待者都在同一个 `lock->val` 上自旋。锁释放时所有 N 个等待者的 cacheline 都从 shared 变成 invalid，再依次获取独占
2. **严格 FIFO** 下前面的 CPU 慢，后面的全部等

### 2.2 MCS 锁的原理

MCS 锁（Mellor-Crummey & Scott, 1991）的核心思想：**每个等待者自旋在自己的本地变量上**。

```
ticket lock:                          MCS lock:
  CPU0 spin on lock->val               CPU0 spin on node0->locked
  CPU1 spin on lock->val               CPU1 spin on node1->locked
  CPU2 spin on lock->val               CPU2 spin on node2->locked
  所有 CPU 在同一行上竞争              每个 CPU 在自己的行上等待
  O(N²) 缓存流量                       O(1) 缓存流量
```

### 2.3 qspinlock 的压缩方案

MCS 锁需要 tail 指针（8字节）+ next 指针（8字节）= 16 字节。但内核的 `spinlock_t` 只有 4 字节。

qspinlock 将 `{tail, next->locked}` 压缩进一个 u32（`kernel/locking/qspinlock.c:46-57`）：

- **locked**（8 bit）：锁持有标志，替代 MCS 的 `next->locked` 通知
- **pending**（1-8 bit）：单个等待者的乐观自旋位，避免不必要的队列开销
- **tail**（剩余 14-23 bit）：编码 `(CPU编号+1, 嵌套深度)` 作为队列尾部指针

---

## 3. Fast Path：一行 CMPXCHG

`include/asm-generic/qspinlock.h:107-116`：

```c
static __always_inline void queued_spin_lock(struct qspinlock *lock)
{
    int val = 0;                                   // line 109
    if (likely(atomic_try_cmpxchg_acquire(&lock->val, &val, _Q_LOCKED_VAL)))
        return;                                    // line 111-112: fast path
    queued_spin_lock_slowpath(lock, val);           // line 114
}
```

Fast path 只做一件事：原子地将 `lock->val` 从 `0` 改为 `1`。

```
unlocked:                       locked:
┌────────┬──────┬──────┐      ┌────────┬──────┬──────┐
│ tail=0 │ pend │ lock │      │ tail=0 │ pend │ lock │
│   0    │  0   │  0   │ ───→ │   0    │  0   │  1   │
└────────┴──────┴──────┘      └────────┴──────┴──────┘
   val = 0x00000000               val = 0x00000001
```

如果 CMPXCHG 失败，`val` 被更新为 `lock->val` 的当前值，传递给 slow path。

**快速释放**（`include/asm-generic/qspinlock.h:123-130`）：

```c
static __always_inline void queued_spin_unlock(struct qspinlock *lock)
{
    smp_store_release(&lock->locked, 0);           // line 128
}
```

仅将 `locked` 字节置零。如果有等待者，pending 持有者或队列头正在轮询这个字节。

**trylock**（`include/asm-generic/qspinlock.h:90-98`）先读一次，非零直接返回，避免无意义的 CMPXCHG 开销。

---

## 4. Slow Path 状态机

`kernel/locking/qspinlock.c:116-129` 的注释状态图（`(tail, pending, locked)` 表示）：

```
          fast     :    slow                                  :    unlock
                   :                                          :
uncontended (0,0,0)-:--> (0,0,1) ----------------------------:--> (*,*,0)
                   :       | ^--------.------.             /  :
                   :       v           \      \            |  :
pending            :   (0,1,1) +--> (0,1,0)   \           |  :
                   :       | ^--'              |           |  :
                   :       v                   |           |  :
uncontended queue  :   (n,x,y) +--> (n,0,0) --'           |  :
                   :       | ^--'                          |  :
                   :       v                               |  :
contended queue    :   (*,x,y) +--> (*,0,0) ---> (*,0,1) -'  :
                   :         ^--'                             :
```

| 转换 | 触发条件 | 含义 |
|------|----------|------|
| `(0,0,0)` → `(0,0,1)` | fast path CMPXCHG | 无竞争直接获取 |
| `(0,1,1)` → `(0,1,0)` | 持有者释放 | 锁释放，pending 者等待 |
| `(0,1,0)` → `(0,0,1)` | pending 看到 locked=0 | pending 晋升 |
| `(n,x,y)` → `(n,0,0)` | 持有者释放 | 队列头等待 locked+pending 清零 |
| `(n,0,0)` → `(0,0,1)` | 队列头 CMPXCHG | 唯一队列成员获取，清除 tail |
| `(*,0,0)` → `(*,0,1)` | 队列头 set_locked | 有其他成员，仅设 locked |
| `(*,*,1)` → `(*,*,0)` | unlock | locked=0，唤醒下一个 |

---

## 5. Slow Path 逐段解析

函数入口 `kernel/locking/qspinlock.c:130-134`：

```c
void __lockfunc queued_spin_lock_slowpath(struct qspinlock *lock, u32 val)
{
    struct mcs_spinlock *prev, *next, *node;
    u32 old, tail;
    int idx;

    BUILD_BUG_ON(CONFIG_NR_CPUS >= (1U << _Q_TAIL_CPU_BITS));  // line 136

    if (pv_enabled())                                // line 138: 半虚拟化路径
        goto pv_queue;
    if (virt_spin_lock(lock))                        // line 141: 虚拟机 hypercall
        return;
```

### Step 1: Pending 过渡期等待（line 150-154）

```c
    /*
     * Wait for in-progress pending->locked hand-overs.
     * 0,1,0 -> 0,0,1
     */
    if (val == _Q_PENDING_VAL) {                     // val == 0x100
        int cnt = _Q_PENDING_LOOPS;                  // x86: 512, 默认: 1
        val = atomic_cond_read_relaxed(&lock->val,
                               (VAL != _Q_PENDING_VAL) || !cnt--);
    }
```

**场景**：上一等待者的 pending 正处于 `0,1,0→0,0,1` 过渡中。短暂自旋而非立即排队——pending 持有者通常极短时间内完成。x86 上 `_Q_PENDING_LOOPS = 512`（`arch/x86/include/asm/qspinlock.h:14`）。

### Step 2: 竞争检测 — 是否必须排队？（line 159-160）

```c
    if (val & ~_Q_LOCKED_MASK)              // pending 或 tail 有非零位
        goto queue;
```

只有在纯粹的 `0x0`（空闲）或 `0x1`（仅 locked）时才走 pending 快速路径。有任何其他等待者就直接排队。

### Step 3: 设置 Pending 位（line 167）

```c
    val = queued_fetch_set_pending_acquire(lock);
```

x86 实现（`arch/x86/include/asm/qspinlock.h:17-31`）：

```c
static __always_inline u32 queued_fetch_set_pending_acquire(struct qspinlock *lock)
{
    u32 val;
    val = GEN_BINARY_RMWcc(LOCK_PREFIX "btsl", lock->val.counter, c,
                           "I", _Q_PENDING_OFFSET) * _Q_PENDING_VAL;
    val |= atomic_read(&lock->val) & ~_Q_PENDING_MASK;
    return val;
}
```

展开为 x86 指令：
```asm
    lock btsl $8, (%rdi)     ; 原子地设置 bit 8，CF=旧值
    setc   %al               ; CF → %al
    movzbl %al, %eax
    shll   $8, %eax          ; val = (old_bit8 << 8)
    mov    (%rdi), %edx      ; 读取修改后的锁值
    andl   $0xffff00ff, %edx ; 清除 pending 位
    orl    %edx, %eax        ; val |= (新值 & ~pending)
```

返回值 val 中的 pending 位是**设置之前的旧值**。

通用实现（`kernel/locking/qspinlock.h:184-188`）使用 `atomic_fetch_or_acquire`。

### Step 3.5: Pending 后的竞争回退（line 176-183）

```c
    if (unlikely(val & ~_Q_LOCKED_MASK)) {   // 设置瞬间出现并发
        if (!(val & _Q_PENDING_MASK))         // pending 是我们设置的
            clear_pending(lock);             // 撤销：WRITE_ONCE(lock->pending, 0)
        goto queue;
    }
```

### Step 4: 自旋等待锁释放（line 196-197）

```c
    if (val & _Q_LOCKED_MASK)
        smp_cond_load_acquire(&lock->locked, !VAL);  // 自旋在 locked 字节
```

只读 `lock->locked` 单个字节（而非整个 `lock->val`），减少缓存行竞争范围。

### Step 5: Pending 晋升为持有者（line 204-206）

```c
    clear_pending_set_locked(lock);            // (0,1,0) → (0,0,1)
    return;
```

`clear_pending_set_locked`（`kernel/locking/qspinlock.h:99-102`，`_Q_PENDING_BITS == 8` 时）：

```c
static __always_inline void clear_pending_set_locked(struct qspinlock *lock)
{
    WRITE_ONCE(lock->locked_pending, _Q_LOCKED_VAL);  // line 101
}
```

**一次 16 位写入完成两个操作**：

```
操作前: locked_pending = 0x0100 (pending=1, locked=0)
WRITE_ONCE(lock->locked_pending, 0x0001)
操作后: locked_pending = 0x0001 (pending=0, locked=1)
```

无需 LOCK 前缀——此时我们是唯一的等待者，没有并发修改者。

---

## 6. 进入 MCS 队列

### Step 6a: 获取 per-CPU 节点（line 212-217）

```c
queue:
    lockevent_inc(lock_slowpath);
pv_queue:
    node = this_cpu_ptr(&qnodes[0].mcs);           // line 215
    idx = node->count++;                           // line 216
    tail = encode_tail(smp_processor_id(), idx);   // line 217
```

**Per-CPU 节点数组**（`kernel/locking/qspinlock.c:80`）：

```c
static DEFINE_PER_CPU_ALIGNED(struct qnode, qnodes[_Q_MAX_NODES]);
```

- `_Q_MAX_NODES = 4`（`kernel/locking/qspinlock.h:16`）：task, softirq, hardirq, nmi
- 4 个 qnode 精确占满一个 64 字节 cacheline（每个 mcs 16B × 4）

`struct qnode`（`kernel/locking/qspinlock.h:40-45`）：

```c
struct qnode {
    struct mcs_spinlock mcs;
#ifdef CONFIG_PARAVIRT_SPINLOCKS
    long reserved[2];                              // PV 扩展数据
#endif
};
```

**tail 编码**（`kernel/locking/qspinlock.h:52-60`）：

```c
static inline __pure u32 encode_tail(int cpu, int idx)
{
    u32 tail;
    tail  = (cpu + 1) << _Q_TAIL_CPU_OFFSET;      // cpu+1 左移 18 位 (NR_CPUS<16K)
    tail |= idx << _Q_TAIL_IDX_OFFSET;             // idx 左移 16 位
    return tail;
}
```

`cpu + 1` 避免与 `tail=0`（空队列）混淆。

**tail 解码**（`kernel/locking/qspinlock.h:62-69`）：

```c
static inline __pure struct mcs_spinlock *decode_tail(u32 tail,
                                                      struct qnode __percpu *qnodes)
{
    int cpu = (tail >> _Q_TAIL_CPU_OFFSET) - 1;
    int idx = (tail & _Q_TAIL_IDX_MASK) >> _Q_TAIL_IDX_OFFSET;
    return per_cpu_ptr(&qnodes[idx].mcs, cpu);
}
```

### Step 6b: 嵌套溢出回退（line 230-235）

```c
    if (unlikely(idx >= _Q_MAX_NODES)) {           // 超过 4 层嵌套
        lockevent_inc(lock_no_node);
        while (!queued_spin_trylock(lock))
            cpu_relax();
        goto release;
    }
```

极罕见的嵌套 NMI 情况，退化为直接自旋 trylock。

### Step 6c: 节点初始化与发布（line 237-277）

```c
    node = grab_mcs_node(node, idx);

    barrier();                                     // 保证 count++ 先于节点初始化
    node->locked = 0;
    node->next = NULL;
    pv_init_node(node);

    if (queued_spin_trylock(lock))                 // 趁初始化耗时再试一次
        goto release;

    smp_wmb();                                     // 保证节点初始化先于 tail 发布

    /*
     * Publish the updated tail.
     * p,*,* -> n,*,*
     */
    old = xchg_tail(lock, tail);
    next = NULL;
```

`xchg_tail`（`kernel/locking/qspinlock.h:114-122`，`_Q_PENDING_BITS == 8` 时）：

```c
static __always_inline u32 xchg_tail(struct qspinlock *lock, u32 tail)
{
    return (u32)xchg_relaxed(&lock->tail,
                     tail >> _Q_TAIL_OFFSET) << _Q_TAIL_OFFSET;
}
```

x86 上为 `xchg` 指令，交换 `lock->tail`（16 bit）并返回旧值。

### Step 6d: 链接到前驱并自旋（line 284-302）

```c
    if (old & _Q_TAIL_MASK) {                      // 有前驱节点
        prev = decode_tail(old, qnodes);
        WRITE_ONCE(prev->next, node);               // 链接入队列

        arch_mcs_spin_lock_contended(&node->locked); // 自旋在本地 node->locked

        next = READ_ONCE(node->next);              // 检查是否有后继者
        if (next)
            prefetchw(next);                       // 预取，减少锁传递延迟
    }
```

`arch_mcs_spin_lock_contended`（`kernel/locking/mcs_spinlock.h:26-27`）：

```c
#define arch_mcs_spin_lock_contended(l)  smp_cond_load_acquire(l, VAL)
```

**在本地 `node->locked` 上自旋**——不会造成总线竞争。

队列链接示意图：

```
  持有者 (H)          等待者1 (W1=prev)     等待者2 (W2=node)
┌──────────┐       ┌──────────┐         ┌──────────┐
│ holds    │       │          │         │          │
│ lock     │       │ locked=0 │◄────────│ locked=0 │
│          │       │ next ────┼────────►│ next=NULL│
└──────────┘       └──────────┘         └──────────┘
                        ▲                     ▲
                  自旋在 node->locked   刚加入，即将自旋
```

---

## 7. Step 7: 队列头部获取锁

### 7a: 等待 locked + pending 清零（line 325-328）

```c
    if ((val = pv_wait_head_or_lock(lock, node)))
        goto locked;

    val = atomic_cond_read_acquire(&lock->val,
                           !(VAL & _Q_LOCKED_PENDING_MASK));
```

`_Q_LOCKED_PENDING_MASK = _Q_LOCKED_MASK | _Q_PENDING_MASK`（`kernel/locking/qspinlock.h:77`）。

等待 locked 和 pending 都清零——队列头必须等两者都释放：

```
(*,1,1) →  pending 获取锁 → (*,0,1) → 释放 → (*,0,0) → 队列头可以获取
(*,0,1) →  释放           → (*,0,0) → 队列头可以获取
(*,1,0) →  pending 获取锁 → (*,0,1) → 释放 → (*,0,0)
```

### 7b: 获取锁（line 352-369）

```c
locked:
    /*
     * n,0,0 -> 0,0,1 : lock, uncontended
     * *,*,0 -> *,*,1 : lock, contended
     */
    if ((val & _Q_TAIL_MASK) == tail) {            // 队列中仅我一人
        if (atomic_try_cmpxchg_relaxed(&lock->val, &val, _Q_LOCKED_VAL))
            goto release;                          // (n,0,0)→(0,0,1) 清除tail+获取锁
    }

    set_locked(lock);                              // (*,0,0)→(*,0,1) 保留tail

    if (!next)
        next = smp_cond_load_relaxed(&node->next, (VAL));  // 等待后继出现

    arch_mcs_spin_unlock_contended(&next->locked);  // 传递锁：next->locked=1
    pv_kick_node(lock, next);
```

**分支 1（无竞争）**：`tail == self` → 一次 CMPXCHG 同时清除 tail 并设置 locked：

```
修改前:                               修改后:
┌─────────────┬────┬────┐            ┌─────────────┬────┬────┐
│ tail = N    │ 0  │ 0  │            │ tail = 0    │ 0  │ 1  │
└─────────────┴────┴────┘            └─────────────┴────┴────┘
```

**分支 2（有竞争）**：仅 `set_locked(lock)`（`kernel/locking/qspinlock.h:196-199`）：

```c
static __always_inline void set_locked(struct qspinlock *lock)
{
    WRITE_ONCE(lock->locked, _Q_LOCKED_VAL);       // 保留 tail 给后继者
}
```

**锁传递**：`arch_mcs_spin_unlock_contended(&next->locked)`（`kernel/locking/mcs_spinlock.h:36-37`）：

```c
#define arch_mcs_spin_unlock_contended(l)  smp_store_release((l), 1)
```

将 `next->locked` 置 1，唤醒在 Step 6d 中自旋的后继者。

### 7c: 释放节点（line 373-380）

```c
release:
    trace_contention_end(lock, 0);
    __this_cpu_dec(qnodes[0].mcs.count);            // 递减嵌套计数
}
```

---

## 8. 完整流程图

```
                        fast path 失败
                             │
                             ▼
              ┌──────────────────────────┐
              │ Step 0: pv/virt 检查      │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 1: val==0x100?      │
              │ YES→自旋最多512次        │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 2: val 有竞争位?     │
              │ YES → 去 MCS 队列         │
              └────────────┬─────────────┘
                           │ NO
                           ▼
              ┌──────────────────────────┐
              │ Step 3: LOCK BTSL bit8   │
              │ 设置 pending 位          │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 3.5: 旧值有竞争?    │
              │ YES→撤销+去队列          │
              └────────────┬─────────────┘
                           │ NO
                           ▼
              ┌──────────────────────────┐
              │ Step 4: 等 locked 清零    │
              │ smp_cond_load_acquire    │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 5: pending→locked   │
              │ (0,1,0)→(0,0,1) DONE!   │
              └──────────────────────────┘


                   MCS 队列路径
              ┌──────────────────────────┐
              │ Step 6a: per-CPU 节点    │
              │ qnodes[idx], count++    │
              │ encode_tail(cpu,idx)    │
              │ idx≥4?→直接自旋         │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 6b: 初始化节点      │
              │ locked=0,next=NULL      │
              │ barrier, 再试 trylock   │
              │ smp_wmb, xchg_tail      │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 6c: 链接 & 自旋     │
              │ prev->next=node         │
              │ 自旋 node->locked       │
              │ 预取 next cacheline     │
              └────────────┬─────────────┘
                           │ node->locked=1
                           ▼
              ┌──────────────────────────┐
              │ Step 7a: 等 locked+      │
              │         pending 清零     │
              │ (*,x,y)→(*,0,0)         │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 7b: 获取锁          │
              │ tail==self?             │
              │ ├YES: CMPXCHG清tail+lock│
              │ └NO:  set_locked(lock)  │
              └────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │ Step 7c: 传递锁          │
              │ next->locked=1          │
              │ __this_cpu_dec(count)   │
              │ → DONE!                  │
              └──────────────────────────┘
```

---

## 9. MCS 锁通用实现

`kernel/locking/mcs_spinlock.h:56-87`：

```c
static inline
void mcs_spin_lock(struct mcs_spinlock **lock, struct mcs_spinlock *node)
{
    struct mcs_spinlock *prev;
    node->locked = 0;
    node->next   = NULL;

    prev = xchg(lock, node);
    if (likely(prev == NULL))              // 无人排队，直接获取
        return;
    WRITE_ONCE(prev->next, node);
    arch_mcs_spin_lock_contended(&node->locked);  // 自旋在本地
}
```

释放（`kernel/locking/mcs_spinlock.h:93-111`）：

```c
static inline
void mcs_spin_unlock(struct mcs_spinlock **lock, struct mcs_spinlock *node)
{
    struct mcs_spinlock *next = READ_ONCE(node->next);
    if (likely(!next)) {
        if (likely(cmpxchg_release(lock, node, NULL) == node))
            return;                        // 无后继者
        while (!(next = READ_ONCE(node->next)))
            cpu_relax();                   // 等后继者完成链接
    }
    arch_mcs_spin_unlock_contended(&next->locked);  // 传递锁
}
```

| 机制 | MCS lock | qspinlock |
|------|----------|-----------|
| 获取通知 | 前驱写 `node->locked=1` | 前驱写 `node->locked=1`（队列）或 `lock->locked`（pending） |
| 等待位置 | 本地 `node->locked` | 队列成员在本地 `node->locked`；队列头在 `lock->val` |
| 内存占用 | tail(8B) + 每个节点(12B+) | 总计 4B + per-CPU 节点 |

---

## 10. 核心设计权衡

1. **pending 位**：避免为每次竞争建立 MCS 队列。pending 路径仅操作 `locked` 字节，比完整排队快得多。

2. **tail 压缩**：`(cpu+1, idx)` 编码进 32 位字，限制每个 CPU 4 个嵌套上下文。

3. **locked 字节承接**：MCS 锁由释放者写 `next->locked=1` 传递。qspinlock 让第一个等待者在 `lock->locked` 上自旋——释放只需 `smp_store_release(&lock->locked, 0)`，与等待者数量无关。

4. **缓存友好**：每个等待者自旋在本地 per-CPU 的 `node->locked`；pending 者在 `lock->locked`（单字节）；仅队列头在 `lock->val` 上等待——最小化缓存一致性流量。

下一篇文章将分析 PREEMPT_RT 下 spinlock 如何变成 rt_mutex，以及 rwlock 的实现。

---

## 下一篇文章

➡️ [第3篇：PREEMPT_RT + rwlock 深度解析](./03-rt-rwlock.md)
