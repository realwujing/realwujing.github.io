# 第四篇：SoftIRQ 与 Tasklet — 下半部中断处理机制

> 源码：kernel/softirq.c | 头文件：include/linux/interrupt.h include/linux/preempt.h

系列目录：[IRQ 内核源码深度解析](./README.md)

---

## 1. 为什么需要下半部

硬中断处理禁止睡眠且关中断，必须快速返回。耗时工作通过下半部延迟执行：

| 机制 | 上下文 | 并发性 | 动态注册 | 开销 |
|------|--------|--------|---------|------|
| **SoftIRQ** | 软中断 | 同类型多 CPU 并发 | ❌ | 极低 |
| **Tasklet** | 软中断 | 同一 tasklet 串行 | ✅ | 低 |
| **Workqueue** | 进程 | 可睡眠 | ✅ | 中等 |

---

## 2. SoftIRQ 类型枚举 (interrupt.h:550-563)

10 种，按优先级从高到低：

```c
// include/linux/interrupt.h:550
enum {
    HI_SOFTIRQ=0,    TIMER_SOFTIRQ,   NET_TX_SOFTIRQ,  NET_RX_SOFTIRQ,
    BLOCK_SOFTIRQ,   IRQ_POLL_SOFTIRQ,TASKLET_SOFTIRQ, SCHED_SOFTIRQ,
    HRTIMER_SOFTIRQ, RCU_SOFTIRQ,     /* RCU should always be the last */
    NR_SOFTIRQS     // = 10
};
```

```c
// kernel/softirq.c:60
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

// kernel/softirq.c:64
const char * const softirq_to_name[NR_SOFTIRQS] = {
    "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "IRQ_POLL",
    "TASKLET", "SCHED", "HRTIMER", "RCU"
};
```

---

## 3. 核心数据结构

### 3.1 struct softirq_action (interrupt.h:590)

```c
// include/linux/interrupt.h:590
struct softirq_action {
    void (*action)(void);
};
```

### 3.2 struct tasklet_struct (interrupt.h:691)

```c
// include/linux/interrupt.h:691
struct tasklet_struct {
    struct tasklet_struct *next;   // 链表指针
    unsigned long state;           // TASKLET_STATE_SCHED / TASKLET_STATE_RUN
    atomic_t count;                // 0=启用, 非0=禁用
    bool use_callback;
    union {
        void (*func)(unsigned long data);         // 旧接口
        void (*callback)(struct tasklet_struct *t); // 新接口
    };
    unsigned long data;
};

// include/linux/interrupt.h:733
enum { TASKLET_STATE_SCHED, TASKLET_STATE_RUN };

// include/linux/interrupt.h:704, 721 — DECLARE_TASKLET / DECLARE_TASKLET_OLD
```

### 3.3 tasklet_head (softirq.c:814)

```c
// kernel/softirq.c:814
struct tasklet_head {
    struct tasklet_struct *head;
    struct tasklet_struct **tail;    // O(1) 追加: *tail = t; tail = &t->next;
};
// kernel/softirq.c:819
static DEFINE_PER_CPU(struct tasklet_head, tasklet_vec);
```

---

## 4. SoftIRQ 的注册与触发

### 4.1 open_softirq (softirq.c:806)

```c
// kernel/softirq.c:806
void open_softirq(int nr, void (*action)(void)) {
    softirq_vec[nr].action = action;
}
```

典型调用：`open_softirq(TASKLET_SOFTIRQ, tasklet_action)` 等，由 `softirq_init` 和各子系统注册。

### 4.2 pending 位图 (interrupt.h:528-530)

```c
// include/linux/interrupt.h:528-530
#define local_softirq_pending()   (__this_cpu_read(local_softirq_pending_ref))
#define set_softirq_pending(x)    (__this_cpu_write(local_softirq_pending_ref, (x)))
#define or_softirq_pending(x)     (__this_cpu_or(local_softirq_pending_ref, (x)))
```

`local_softirq_pending_ref` 映射到 per-CPU `irq_stat.__softirq_pending`：

```
__softirq_pending bit 布局 (32 bits):
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬────────────────────┐
│RCU│HRT│SCH│TSK│IRQ│BLK│NRX│NTX│TMR│ HI│         0           │
│ 9 │ 8 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │                     │
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴────────────────────┘
```

### 4.3 raise_softirq / __raise_softirq_irqoff (softirq.c:790, 799)

```c
// kernel/softirq.c:790
void raise_softirq(unsigned int nr) {
    unsigned long flags;
    local_irq_save(flags);
    raise_softirq_irqoff(nr);
    local_irq_restore(flags);
}

// kernel/softirq.c:799
void __raise_softirq_irqoff(unsigned int nr) {
    lockdep_assert_irqs_disabled();
    trace_softirq_raise(nr);
    or_softirq_pending(1UL << nr);   // 设置 pending bit
}
```

### 4.4 wakeup_softirqd (softirq.c:75)

```c
// kernel/softirq.c:75
static void wakeup_softirqd(void) {
    struct task_struct *tsk = __this_cpu_read(ksoftirqd);
    if (tsk) wake_up_process(tsk);
}
```

---

## 5. do_softirq (softirq.c:510)

```c
// kernel/softirq.c:510
asmlinkage __visible void do_softirq(void) {
    __u32 pending;
    unsigned long flags;

    if (in_interrupt())          // 防止重入
        return;

    local_irq_save(flags);
    pending = local_softirq_pending();
    if (pending)
        do_softirq_own_stack();  // 切栈调用 __do_softirq
    local_irq_restore(flags);
}
```

---

## 6. handle_softirqs — 核心执行 (softirq.c:579)

```c
// kernel/softirq.c:543-544
#define MAX_SOFTIRQ_TIME  msecs_to_jiffies(2)
#define MAX_SOFTIRQ_RESTART 10

// kernel/softirq.c:579
static void handle_softirqs(bool ksirqd) {
    unsigned long end = jiffies + MAX_SOFTIRQ_TIME;
    unsigned long old_flags = current->flags;
    int max_restart = MAX_SOFTIRQ_RESTART;
    struct softirq_action *h;
    bool in_hardirq;
    __u32 pending;
    int softirq_bit;

    current->flags &= ~PF_MEMALLOC;
    pending = local_softirq_pending();

    softirq_handle_begin();
    in_hardirq = lockdep_softirq_start();
    account_softirq_enter(current);

restart:
    set_softirq_pending(0);       // ★ 先清零 pending，再开中断
    local_irq_enable();           // ★ 此时可被硬件中断抢占

    h = softirq_vec;
    while ((softirq_bit = ffs(pending))) {  // ffs 从低到高找第一个 1
        unsigned int vec_nr;
        int prev_count;

        h += softirq_bit - 1;
        vec_nr = h - softirq_vec;
        prev_count = preempt_count();

        kstat_incr_softirqs_this_cpu(vec_nr);
        trace_softirq_entry(vec_nr);
        h->action();              // ★ 执行 handler
        trace_softirq_exit(vec_nr);

        if (unlikely(prev_count != preempt_count())) {
            pr_err("huh, entered softirq %u %s %p with "
                   "preempt_count %08x, exited with %08x?\n",
                   vec_nr, softirq_to_name[vec_nr], h->action,
                   prev_count, preempt_count());
            preempt_count_set(prev_count);
        }
        h++;
        pending >>= softirq_bit;
    }

    if (!IS_ENABLED(CONFIG_PREEMPT_RT) && ksirqd)
        rcu_softirq_qs();

    local_irq_disable();
    pending = local_softirq_pending();

    if (pending) {
        if (time_before(jiffies, end) && !need_resched() && --max_restart)
            goto restart;          // 还有 pending 且未超限 → 重试
        wakeup_softirqd();         // 超限 → 交给 ksoftirqd
    }

    account_softirq_exit(current);
    lockdep_softirq_end(in_hardirq);
    softirq_handle_end();
    current_restore_flags(old_flags, PF_MEMALLOC);
}
```

### 6.1 关键设计：先清零再开中断

```
正确:  set_pending(0) → local_irq_enable → action
       开中断后新 IRQ 进来 → raise_softirq → 设 pending bit
       → 下一轮 restart 会处理新 pending ✅

错误:  local_irq_enable → action → set_pending(0)
       新 IRQ 设置 pending bit → 当前轮结束清零 → 新 softirq 丢失 ❌
```

### 6.2 防止饥饿

两个停止条件确保软中断不能无限运行：时间 > 2ms 或重试 > 10 次 → 唤醒 ksoftirqd。

---

## 7. ksoftirqd — 守护线程

```c
// kernel/softirq.c:62
DEFINE_PER_CPU(struct task_struct *, ksoftirqd);

// kernel/softirq.c:1063
static int ksoftirqd_should_run(unsigned int cpu) {
    return local_softirq_pending();   // 有 pending 就唤醒
}

// kernel/softirq.c:1068
static void run_ksoftirqd(unsigned int cpu) {
    ksoftirqd_run_begin();
    if (local_softirq_pending()) {
        handle_softirqs(true);    // ksirqd=true
        ksoftirqd_run_end();
        cond_resched();
        return;
    }
    ksoftirqd_run_end();
}
```

每个 CPU 一个 `ksoftirqd/N` 线程，SCHED_OTHER 策略，不抢占实时任务。

---

## 8. irq_exit → 下半部入口

```
do_IRQ() → handle_irq_event → irq_exit():
    if (!in_interrupt() && local_softirq_pending()) {
#ifdef CONFIG_HAVE_IRQ_EXIT_ON_IRQ_STACK
        __do_softirq();          // 中断栈几乎为空 → 直接执行
#else
        do_softirq_own_stack();  // 任务栈可能很深 → 切栈
#endif
    } else {
        wakeup_softirqd();       // 已在中斷上下文 → 委托
    }
```

---

## 9. Tasklet：建立在 TASKLET_SOFTIRQ 之上

### 9.1 调度 (interrupt.h:758)

```c
// include/linux/interrupt.h:758
static inline void tasklet_schedule(struct tasklet_struct *t) {
    if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state))
        __tasklet_schedule(t);   // 防重入: 同一 tasklet 只调度一次
}
```

### 9.2 执行 (softirq.c:916)

```c
// kernel/softirq.c:916
static void tasklet_action_common(struct tasklet_head *tl_head,
                                  unsigned int softirq_nr) {
    struct tasklet_struct *list;

    local_irq_disable();
    list = tl_head->head;
    tl_head->head = NULL;              // ★ 原子摘取整条链表
    tl_head->tail = &tl_head->head;
    local_irq_enable();

    tasklet_lock_callback();
    while (list) {
        struct tasklet_struct *t = list;
        list = list->next;

        if (tasklet_trylock(t)) {           // ★ test_and_set RUN → 串行
            if (!atomic_read(&t->count)) {   // count==0 → 已启用
                if (tasklet_clear_sched(t)) {
                    if (t->use_callback)
                        t->callback(t);      // 新接口
                    else
                        t->func(t->data);    // 旧接口
                }
                tasklet_unlock(t);
                tasklet_callback_sync_wait_running();
                continue;
            }
            tasklet_unlock(t);
        }
        // trylock 失败 (正被其他 CPU 执行) → 重新挂回链表
        local_irq_disable();
        t->next = NULL;
        *tl_head->tail = t;
        tl_head->tail = &t->next;
        __raise_softirq_irqoff(softirq_nr);    // 重新触发
        local_irq_enable();
    }
    tasklet_unlock_callback();
}
```

### 9.3 状态机

```
              tasklet_schedule()
  [IDLE] ──────────────────────────► [SCHED]
    ▲                                    │
    │                softirq 触发         │
    │                tasklet_trylock()    │
    │                         ┌──────────┘
    │                         ▼
    │                    [RUNNING] (TASKLET_STATE_RUN set)
    │                         │
    │           ┌─────────────┤
    │           │ 执行完成     │ 被其他 CPU 重新调度
    │           ▼             ▼
    │     tasklet_unlock → [IDLE] 或 → [SCHED] (requeue + raise_softirq)
    │
    └── trylock 失败 → requeue 回链表 → 重新 raise TASKLET_SOFTIRQ
```

---

## 10. 上下文判断宏 (preempt.h:127-141)

```c
// include/linux/preempt.h:127
#define in_hardirq()        (hardirq_count())

// include/linux/preempt.h:130
#ifdef CONFIG_PREEMPT_RT
# define in_task()          (!((preempt_count() & (NMI_MASK | HARDIRQ_MASK)) |
                               in_serving_softirq()))
#else
# define in_task()          (!(preempt_count() & (NMI_MASK | HARDIRQ_MASK |
                                                  SOFTIRQ_OFFSET)))
#endif

// include/linux/preempt.h:140 (deprecated)
#define in_softirq()        (softirq_count())
// include/linux/preempt.h:141 (deprecated)
#define in_interrupt()      (irq_count())
```

速查表：

```
┌────────────────────┬──────────┬───────────┬──────────┬──────────┐
│ 执行环境            │ in_task  │ in_hardirq│ in_softirq│in_interrupt│
├────────────────────┼──────────┼───────────┼──────────┼──────────┤
│ 进程上下文          │  true    │   false   │  false   │  false   │
│ 硬中断 ISR          │  false   │   true    │  false   │  true    │
│ 软中断 handler      │  false   │   false   │  true    │  true    │
│ BH 禁用区           │  false   │   false   │  true    │  true    │
└────────────────────┴──────────┴───────────┴──────────┴──────────┘
```

---

## 11. local_bh_enable/disable

```c
local_bh_disable()
  → __local_bh_disable_ip()  // 递增 preempt_count SOFTIRQ 部分, 防同 CPU 软中断重入

local_bh_enable()
  → __local_bh_enable_ip()
    → 递减计数; if (降到0 && local_softirq_pending())
        → __do_softirq()  // 或 wakeup_softirqd
```

注意：`local_bh_disable` 只保护同一 CPU，不防其他 CPU 的 softirq。跨 CPU 保护需要 spin_lock。

---

## 12. 完整流程图

```
        硬件设备
           │
           ▼
    ┌─────────────────┐
    │ arch_entry       │ x86: common_interrupt
    │ → handle_irq_desc│ arm64: el1_irq
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ handle_irq_event │ 上半部 ISR: raise_softirq_irqoff() 设 pending
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │ irq_exit()       │
    │ → invoke_softirq │
    └───┬────┬─────────┘
        │    │
  in_interrupt│ !in_interrupt && pending
        │    │
        ▼    ▼
    [返回] ┌──────────────────────┐
           │ handle_softirqs()    │ softirq.c:579
           │                      │
           │ set_pending(0)       │ 先清零
           │ local_irq_enable     │ 开中断
           │                      │
           │ while ffs(pending):  │ 按优先级遍历
           │  HI → TIMER →        │
           │  NET_TX → NET_RX →   │
           │  BLOCK → IRQ_POLL →  │
           │  TASKLET → SCHED →   │
           │  HRTIMER → RCU       │
           │                      │
           │ still pending?       │
           │  ├─ yes (<max): goto │
           │  │         restart   │
           │  └─ no/超限:         │
           │     wakeup_softirqd  │
           └──────────────────────┘
                     │
                     ▼
           ┌──────────────────┐
           │ ksoftirqd/N       │ 进程上下文继续处理
           └──────────────────┘

TASKLET_SOFTIRQ 触发时:
    ┌───────────────────────┐
    │ tasklet_action_common │ softirq.c:916
    │   摘取 per-CPU 链表    │
    │   遍历: trylock       │
    │     count==0 → execute│
    │     trylock失败→requeue│
    └───────────────────────┘
```

---

## 13. 调试与选择

```bash
cat /proc/softirqs                    # SoftIRQ per-CPU 统计
ps aux | grep ksoftirqd               # 守护线程
echo 1 > /sys/kernel/debug/tracing/events/irq/softirq_entry/enable
cat /sys/kernel/debug/tracing/trace_pipe  # tracepoint
watch -n1 cat /proc/softirqs          # 检测饥饿
```

选择决策：**要睡眠**→Workqueue; **不要睡眠+需多CPU并发+极致延迟**→SoftIRQ; 其余→Tasklet.

---

## 下一篇文章

[第五篇：CPU Hotplug、MSI 与电源管理 — IRQ 的跨 CPU 迁移与睡眠唤醒](./05-hotplug-msi-pm.md)
