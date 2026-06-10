# 第2篇：RCU API — rcu_read_lock、synchronize_rcu、call_rcu 与指针宏

> 源码：`include/linux/rcupdate.h` `kernel/rcu/tree.c` | 头文件：`include/linux/rcupdate.h`

系列目录：[RCU 内核源码深度解析](./README.md)

---

## 1. RCU 的"读者-写者"模型

RCU 的核心思想：**读者不阻塞，写者等读者**。

```
读者:
  rcu_read_lock()
  p = rcu_dereference(gp)      ← 无锁读取
  use(p)
  rcu_read_unlock()
                                  ← 读者完全不受写者影响

写者:
  p = kmalloc(...)
  p->data = new_value
  old = gp
  rcu_assign_pointer(gp, p)     ← 原子替换指针
  synchronize_rcu()              ← ★ 等所有读者退出
  kfree(old)                     ← 安全释放旧数据
```

关键在于 `synchronize_rcu()`：它担保调用返回后，所有在调用**之前**进入 `rcu_read_lock()` 的读者都已经调用了 `rcu_read_unlock()`。

---

## 2. rcu_read_lock / rcu_read_unlock

### 2.1 标准 RCU 读锁

```c
// include/linux/rcupdate.h:833-841
static __always_inline void rcu_read_lock(void) __acquires_shared(RCU)
{
    __rcu_read_lock();
    __acquire_shared(RCU);
    rcu_lock_acquire(&rcu_lock_map);
    RCU_LOCKDEP_WARN(!rcu_is_watching(),
        "rcu_read_lock() used illegally while idle");
}

// include/linux/rcupdate.h:864-872
static inline void rcu_read_unlock(void) __releases_shared(RCU)
{
    RCU_LOCKDEP_WARN(!rcu_is_watching(),
        "rcu_read_unlock() used illegally while idle");
    rcu_lock_release(&rcu_lock_map);
    __release_shared(RCU);
    __rcu_read_unlock();
}
```

底层实现在非 PREEMPT_RT 内核中：

```c
// include/linux/rcupdate.h:101-111
static inline void __rcu_read_lock(void)   { preempt_disable(); }             // line 101
static inline void __rcu_read_unlock(void) {                                    // line 106
    if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
        rcu_read_unlock_strict();
    preempt_enable();
}
```

**本质**：`rcu_read_lock()` = `preempt_disable()`，`rcu_read_unlock()` = `preempt_enable()`。RCU 的"读者"就是一个禁止抢占的内核代码段。并发 CPU 不进入这个段了（不发生 context switch 经过了），grace period 结束。

### 2.2 RCU BH 读锁

```c
// include/linux/rcupdate.h:888-897
static inline void rcu_read_lock_bh(void)
{
    local_bh_disable();          // 禁止 softirq（bottom half）
    __acquire_shared(RCU);
    rcu_lock_acquire(&rcu_bh_lock_map);
}
```

`rcu_read_lock_bh()` = `local_bh_disable()`。用于保护被 softirq 访问的数据结构。

### 2.3 RCU Sched 读锁

```c
// include/linux/rcupdate.h:930-939
static inline void rcu_read_lock_sched(void)
{
    preempt_disable();
    __acquire_shared(RCU);
    rcu_lock_acquire(&rcu_sched_lock_map);
}
```

和标准 RCU 一样是 `preempt_disable()`，但语义不同——用于保护被调度器（任意上下文）访问的数据。

### 2.4 三类锁对比

| 锁 | 实现 | 保护的数据访问场景 |
|---|---|---|
| `rcu_read_lock()` | `preempt_disable()` | 任意上下文 |
| `rcu_read_lock_bh()` | `local_bh_disable()` | 进程上下文 + softirq |
| `rcu_read_lock_sched()` | `preempt_disable()` | 调度器相关 |

---

## 3. 指针操作宏

### 3.1 rcu_assign_pointer — 写者发布新指针

```c
// include/linux/rcupdate.h:558-567
#define rcu_assign_pointer(p, v)                                    \
    context_unsafe(                                                  \
        uintptr_t _r_a_p__v = (uintptr_t)(v);                       \
        rcu_check_sparse(p, __rcu);                                  \
        if (__builtin_constant_p(v) && (_r_a_p__v) == (uintptr_t)NULL) \
            WRITE_ONCE((p), (typeof(p))(_r_a_p__v));                \
        else                                                         \
            smp_store_release(&p, RCU_INITIALIZER((typeof(p))_r_a_p__v)); \
    )
```

- 写到 NULL：用 `WRITE_ONCE`（不需要 barrier，NULL 是安全的读出）
- 写有效指针：用 `smp_store_release`——保证指针写入之前的所有初始化在读者看到指针**之后**一定可见

### 3.2 rcu_dereference — 读者获取指针

```c
// include/linux/rcupdate.h:649-764
#define rcu_dereference_check(p, c) \
    __rcu_dereference_check((p), __UNIQUE_ID(rcu), (c) || rcu_read_lock_held(), __rcu)

#define rcu_dereference(p)  rcu_dereference_check(p, 0)       // line 740
```

`rcu_dereference()` 等价于 `rcu_dereference_check(p, 0)`，底层使用 `READ_ONCE()` 读指针，并附加 `lockdep` 检查（是否在 `rcu_read_lock()` 内）。`smp_read_barrier_depends()`（ARMv7）或直接 `READ_ONCE()`（x86/ARMv8 的依赖排序保证）。

### 3.3 RCU_INIT_POINTER — 初始化专用

```c
// include/linux/rcupdate.h:1029-1033
#define RCU_INIT_POINTER(p, v)          \
    context_unsafe(                      \
        rcu_check_sparse(p, __rcu);     \
        WRITE_ONCE(p, RCU_INITIALIZER(v)); \
    )
```

没有 ordering 保证，只在初始化阶段使用（没有并发 reader 时）。

### 3.4 发布-消费模型总结

```
写者                              读者
────────────────────────────       ────────────────────────
p = kmalloc                       rcu_read_lock()
p->a = 1                          q = rcu_dereference(gp)
p->b = 2                          // 此时如果 gp 已指向新 p，
p->x = 3                          // 读者一定能看到 p->a=1, p->b=2, p->x=3
rcu_assign_pointer(gp, p)          // 因为 smp_store_release
// ↑ 这里保证上面所有修改
//   在读者看到新指针后可见         rcu_read_unlock()
```

---

## 4. call_rcu — 异步等待 Grace Period

```c
// kernel/rcu/tree.c:3243-3261
void call_rcu(struct rcu_head *head, rcu_callback_t func)
{
    __call_rcu_common(head, func, false);
}
```

`call_rcu()` 注册一个回调 `func`，在下一个 grace period 结束后调用。不阻塞调用者：

```
Writer call_rcu(head, func):
    → __call_rcu_common       // tree.c:3102
      → 把 head 挂到 per-CPU segmented callback 链表
      → 启动 grace period（如果还没启动）
      → 立即返回！

rcu_gp_kthread:
    → grace period 完成
      → rcu_do_batch
        → 遍历回调链表
          → 调用 func(head)   ← 此时回调真正执行
```

### 4.1 回调分段链表

每个 CPU 的 `rcu_data` 有一个 `struct rcu_segcblist cblist`：

```
     [DONE] → [NEXT] → [WAITING]  → ...
     ↑ 已过GP    ↑ 当前GP   ↑ 等下一GP
     └─ rcu_do_batch 从这里取
```

回调分段的关键：RCU 需要知道哪些回调已经在已完成的 GP 中（DONE 段），哪些还在等待当前/下一轮 GP。`__call_rcu_common` 将回调加到 WAITING 段，GP 推进时搬到 NEXT → DONE。

---

## 5. synchronize_rcu — 同步等待 Grace Period

```c
// kernel/rcu/tree.c:3349-3379
void synchronize_rcu(void)
{
    RCU_LOCKDEP_WARN(lock_is_held(&rcu_bh_lock_map) ||
                     lock_is_held(&rcu_lock_map) ||
                     lock_is_held(&rcu_sched_lock_map),
                     "Illegal synchronize_rcu() in RCU read-side critical section");

    if (rcu_blocking_is_gp())    // 如果当前已经在 GP 中（单 CPU），直接返回
        return;

    // 正常路径：等待一次完整 grace period
    if (rcu_gp_is_expedited())
        synchronize_rcu_expedited();
    else
        wait_rcu_gp(call_rcu_hurry);
}
```

`wait_rcu_gp()` 内部使用 `completion` 机制：在里面调用 `call_rcu_hurry()` 注册一个 `wakeme_after_rcu()` 回调，等 GP 完成时触发 completion → `synchronize_rcu()` 返回。

```c
// tree.c:3277-3315 — synchronize_rcu_normal flow
static void synchronize_rcu_normal(void)
{
    struct rcu_synchronize rcu;
    init_completion(&rcu.completion);        // 创建 completion
    // 注册 wakeme_after_rcu 回调
    call_rcu_hurry(&rcu.head, wakeme_after_rcu);
    // 阻塞等待 completion
    wait_for_completion(&rcu.completion);
    // GP 已过，回调已执行 → 返回
    destroy_rcu_head_on_stack(&rcu.head);
}
```

`call_rcu_hurry`（tree.c:3183）是 `call_rcu` 的"紧急"版本——它告诉 RCU 回调需要尽快（即不让系统推迟到 lazy 模式）。

---

## 6. rcu_barrier — 等待所有 CPU 的回调完成

```c
// kernel/rcu/tree.c:3817-3829
void rcu_barrier(void)
{
    struct rcu_data *rdp;
    int cpu;

    // 在每颗 CPU 上排队一个 barrier 回调
    for_each_possible_cpu(cpu) {                        // line 3824
        rdp = per_cpu_ptr(&rcu_data, cpu);
        if (rcu_segcblist_n_cbs(&rdp->cblist)) {       // line 3826
            // CPU 有挂起的回调 → 排队 barrier 回调
            __call_rcu_common((struct rcu_head *)&rdp->barrier_head,
                              rcu_barrier_callback, false);
        }
    }

    // 等所有排队的 barrier 回调完成
    wait_for_completion(&rcu_state.barrier_completion); // line 3832
}
```

`synchronize_rcu()` 等一个 GP，但**不保证** `call_rcu()` 排队的回调已经执行。`rcu_barrier()` 保证**所有 CPU** 上**所有之前排队的回调**都已执行完。典型用途：模块卸载时：

```c
my_module_exit() {
    // 1. 停止产生新的 call_rcu
    // 2. rcu_barrier() — 等所有 pending call_rcu 回调完成
    // 3. 安全释放模块数据
}
```

---

## 7. kfree_rcu — 最常见的 RCU 释放模式

```c
#define kfree_rcu(ptr, rhf)                         \
    kvfree_rcu_arg_2(ptr, rhf)

// 实际：
// 1. 直接从内存池分配 rcu_head，或用对象内部嵌入的 rcu_head
// 2. 注册回调：kfree(ptr)
// 3. 下个 GP 后自动释放
```

不需要包装函数，直接给对象的 `rcu_head` 字段名和对象指针。GP 完成后 `kfree()` 自动调用。

---

## 8. API 速查总结

```
写者 API:
  rcu_assign_pointer(p, v)            — 发布新指针，smp_store_release
  call_rcu(head, func)                — 异步：注册回调，GP 后回调执行
  call_rcu_hurry(head, func)          — 同上，但更急（不延迟）
  synchronize_rcu()                   — 同步：阻塞等一次 GP 完成
  kfree_rcu(ptr, rhf)                 — 常用：GP 后自动 kfree
  rcu_barrier()                       — 等所有 CPU 回调全部完成

读者 API:
  rcu_read_lock()                     — 进入读者段 = preempt_disable()
  rcu_read_unlock()                   — 退出读者段 = preempt_enable()
  rcu_dereference(p)                  — 读取 RCU 保护指针，READ_ONCE + lockdep
  rcu_read_lock_bh()                  — 读者 + 禁止 BH
  rcu_read_lock_sched()               — 读者调度器

初始化 API:
  RCU_INIT_POINTER(p, v)              — 初始化指针，无 barrier（无并发 reader 时）
```

---

## 9. 关键设计原则

**为什么 `rcu_read_lock()` 是 `preempt_disable()`？**

因为 RCU 的 GP 检测就是"看每颗 CPU 有没有发生 context switch"。如果 CPU 在 reader 段中被抢占（context switch 发生），GP 就不算完成；如果没有 context switch，说明读者已经离开了临界区。

**为什么 `synchronize_rcu()` 要等一次完整 GP？**

GP 开始时，所有正在 reader 段的 CPU 都被标记。等所有这些 CPU 至少发生一次 context switch，GP 结束。此时不会再有任何看得到"旧指针"的 reader。

**为什么 `rcu_barrier()` ≠ `synchronize_rcu()`？**

`synchronize_rcu()` 等一次 GP，但之前的 `call_rcu` 回调可能还没执行（因为回调在 GP 之后才在 `rcu_do_batch` 里跑）。`rcu_barrier()` 等所有回调都跑完。

---

## 10. 完整时序图

```
CPU0 (写者)                        CPU1 (读者)
──────                              ──────
                                    rcu_read_lock()
                                    p = rcu_dereference(gp)
                                    // p 指向旧数据
                                    
旧数据修改新数据
rcu_assign_pointer(gp, new) ─── smp_store_release ──→  ...
                                    读者如果此时读到 gp→new，
                                    保证能看见 new 的所有字段

synchronize_rcu()                   
  │                                 rcu_read_unlock()
  │ call_rcu_hurry(wakeme)          
  │ wait_for_completion()           ← GP 开始
  │                   GP 推进                         
  │                   context switch 发生
  │                   GP 完成 ←                   
  │ wakeme_after_rcu() 被调用  
  │ completion 触发
  │ 返回                             
kfree(old)   ← 安全释放旧数据
```

---

## 下一篇文章

→ [第3篇：RCU 变体 — SRCU、Tasks RCU 与 Tiny RCU](./03-variants.md)
