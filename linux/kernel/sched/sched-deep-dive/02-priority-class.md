# 第二篇：优先级与调度类链 — 从 prio.h 到 sched_class 的完整体系

> 源码：`kernel/sched/core.c`, `kernel/sched/sched.h` | 头文件：`include/linux/sched/prio.h`, `include/linux/sched.h`, `kernel/sched/sched.h`

> 系列目录：[调度器 内核源码深度解析](./README.md)

## 一、Linux 优先级全景

Linux 内核的优先级体系是一个**140 级（0~139）的全局优先级空间**，值越小优先级越高。整个空间被切分为三个区域。

### 1.1 优先级定义 (include/linux/sched/prio.h:5-28)

```c
// include/linux/sched/prio.h:5
#define MAX_NICE	19
#define MIN_NICE	-20
#define NICE_WIDTH	(MAX_NICE - MIN_NICE + 1)   // = 40

#define MAX_RT_PRIO		100                    // RT 优先级上限 (0..99)
#define MAX_DL_PRIO		0                      // Deadline 优先级 (始终为 0)

#define MAX_PRIO		(MAX_RT_PRIO + NICE_WIDTH)  // = 140
#define DEFAULT_PRIO		(MAX_RT_PRIO + NICE_WIDTH / 2) // = 120

/*
 * Convert user-nice values [ -20 ... 0 ... 19 ]
 * to static priority [ MAX_RT_PRIO..MAX_PRIO-1 ],
 * and back.
 */
#define NICE_TO_PRIO(nice)	((nice) + DEFAULT_PRIO)
#define PRIO_TO_NICE(prio)	((prio) - DEFAULT_PRIO)
```

### 1.2 优先级范围全景

```
优先级数值: 0 ───────────── 99 ───────────── 100 ───────────── 139
 越小越高    ▲                ▲                ▲                 ▲
            │                │                │                 │
            RT 专区         RT 上限      NORMAL 下线      NORMAL 上限
          (SCHED_FIFO     MAX_RT_PRIO    MAX_RT_PRIO      MAX_PRIO-1
           SCHED_RR)                       = 100

 nice 映射:  ─               ─             -20  ...  0  ...  19
                                           │         │         │
                                          prio 100  prio 120  prio 139
                                              │         │         │
                                          weight 88761 1024      15
```

**三个调度策略域**：

| 策略 | 优先级范围 | nice 映射 | 说明 |
|------|-----------|-----------|------|
| SCHED_DEADLINE | prio = 0 (内部按 deadline 排序) | 不适用 | EDF 实时调度 |
| SCHED_FIFO / SCHED_RR | prio = 1..99 | 不适用 | RT 实时调度 |
| SCHED_NORMAL / SCHED_BATCH / SCHED_IDLE | prio = 100..139 | nice = prio - 120 | CFS 公平调度 |

### 1.3 task_struct 中的优先级字段 (include/linux/sched.h:866-878)

```c
// include/linux/sched.h:866
	int				prio;           // 运行时有效优先级 (考虑 PI 提升等)
	int				static_prio;    // 静态优先级 = NICE_TO_PRIO(nice)
	int				normal_prio;    // 正常优先级 (CFS 缓存值)
	unsigned int			rt_priority;    // RT 任务的优先级 (0..99)

	struct sched_entity		se;
	struct sched_rt_entity		rt;
	struct sched_dl_entity		dl;
	struct sched_dl_entity		*dl_server;
#ifdef CONFIG_SCHED_CLASS_EXT
	struct sched_ext_entity		scx;
#endif
	const struct sched_class	*sched_class;   // ★ 指向调度类 vtable ★
```

**三个优先级的区别**：

| 字段 | 含义 | 何时变化 |
|------|------|----------|
| `static_prio` | 用户设置的静态优先级 (nice 值映射) | set_user_nice() / sched_setattr() |
| `normal_prio` | 正常优先级，CFS 用于权重计算 | 根据 static_prio 和调度策略计算 |
| `prio` | 运行时有效优先级 | 考虑 PI (Priority Inheritance) 提升，临时提高 |
| `rt_priority` | RT 任务的优先级 (1..99) | sched_setattr() 设置 SCHED_FIFO/RR |

**PI 提升示例**：
```
Task A (SCHED_NORMAL, prio=120) 持有互斥锁
Task B (SCHED_FIFO, prio=50) 等待该锁
→ Task A 被 PI 提升到 prio=50 (临时)
→ 释放锁后 prio 恢复到 120
```

### 1.4 优先级关系总结

```
static_prio   ──→ normal_prio  ──→ prio (可能被 PI 提升)
    │                  │               │
    │ (nice 映射)       │ (缓存)        │ (运行时)
    │                  │               │
    ▼                  ▼               ▼
 设置入口            权重计算         调度决策
 sched_setattr()    set_load_weight()  pick_next_task()
```

## 二、调度类体系 — sched_class

### 2.1 五大调度类 (kernel/sched/sched.h:2737-2741)

```c
// kernel/sched/sched.h:2737
extern const struct sched_class stop_sched_class;      // 最高优先级
extern const struct sched_class dl_sched_class;        // Deadline
extern const struct sched_class rt_sched_class;        // RT
extern const struct sched_class fair_sched_class;      // CFS
extern const struct sched_class idle_sched_class;      // 最低优先级 (idle 任务)
```

**优先级排序（高→低）**：

```
stop > dl > rt > fair > idle
  │     │    │     │      │
  最高   │    │     │    永远有 idle 任务运行
        Deadline   │   SCHED_{NORMAL,BATCH,IDLE}
               SCHED_{FIFO,RR}
```

### 2.2 链接器魔法 — DEFINE_SCHED_CLASS (sched.h:2728-2731)

```c
// kernel/sched/sched.h:2728
#define DEFINE_SCHED_CLASS(name) \
const struct sched_class name##_sched_class \
	__aligned(__alignof__(struct sched_class)) \
	__section("__" #name "_sched_class")
```

这利用了链接器脚本的特殊 section：
- `__stop_sched_class` → `__dl_sched_class` → `__rt_sched_class` → `__fair_sched_class` → `__idle_sched_class`
- 链接器按字母序排列（巧合！`stop` < `dl` < `rt` < `fair` < `idle` 恰好是优先级顺序）
- `__sched_class_highest` 和 `__sched_class_lowest` 标记边界

### 2.3 遍历宏 (sched.h:2759-2771)

```c
// kernel/sched/sched.h:2759
#define for_class_range(class, _from, _to) \
	for (class = (_from); class < (_to); class++)

#define for_each_class(class) \
	for_class_range(class, __sched_class_highest, __sched_class_lowest)

#define for_active_class_range(class, _from, _to)				\
	for (class = (_from); class != (_to); class = next_active_class(class))

#define for_each_active_class(class)						\
	for_active_class_range(class, __sched_class_highest, __sched_class_lowest)

#define sched_class_above(_a, _b)	((_a) < (_b))
```

**关键点**：
- `for_each_class` 从最高到最低遍历
- `sched_class_above(a, b)` 使用**指针比较**：因为按 section 顺序排列，地址小的优先级高
- `for_each_active_class` 跳过被 sched_ext 完全接管时的 fair 类

### 2.4 pick_next_task 流程

```
schedule()
  └→ __schedule()
       └→ pick_next_task(rq, prev, rf)
            │
            for_each_class(class):               ← 从 stop 开始
                p = class->pick_next_task(rq)
                if (p) return p                   ← 第一个有 runnable 任务的类获胜
            │
            return idle_task                      ← 默认返回 idle
```

这个遍历确保了：如果一个 RT 任务和 10 个 CFS 任务同时处于 runnable 状态，RT 任务一定被选中。

### 2.5 判断各类是否有 runnable 任务 (sched.h:2784-2802)

```c
// kernel/sched/sched.h:2784
static inline bool sched_stop_runnable(struct rq *rq)
{
	return rq->stop && task_on_rq_queued(rq->stop);
}

static inline bool sched_dl_runnable(struct rq *rq)
{
	return rq->dl.dl_nr_running > 0;
}

static inline bool sched_rt_runnable(struct rq *rq)
{
	return rq->rt.rt_queued > 0;
}

static inline bool sched_fair_runnable(struct rq *rq)
{
	return rq->cfs.nr_queued > 0;
}
```

## 三、struct sched_class vtable (sched.h:2519-2680)

这是调度器的**多态核心**——每个调度类实现自己的方法，通过函数指针调用。

```c
// kernel/sched/sched.h:2519
struct sched_class {
	// ── 入队 / 出队 ──
	void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
	bool (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);

	// ── 主动让出 ──
	void (*yield_task)   (struct rq *rq);
	bool (*yield_to_task)(struct rq *rq, struct task_struct *p);

	// ── 唤醒抢占检查 ──
	void (*wakeup_preempt)(struct rq *rq, struct task_struct *p, int flags);

	// ── 负载均衡 ──
	int  (*balance)(struct rq *rq, struct task_struct *prev, struct rq_flags *rf);

	// ── 选择任务 (核心) ──
	struct task_struct *(*pick_task)(struct rq *rq, struct rq_flags *rf);
	struct task_struct *(*pick_next_task)(struct rq *rq, struct task_struct *prev,
					      struct rq_flags *rf);

	// ── 调度状态切换 ──
	void (*put_prev_task)(struct rq *rq, struct task_struct *p, struct task_struct *next);
	void (*set_next_task)(struct rq *rq, struct task_struct *p, bool first);

	// ── 任务迁移 ──
	int  (*select_task_rq)(struct task_struct *p, int task_cpu, int flags);
	void (*migrate_task_rq)(struct task_struct *p, int new_cpu);
	void (*task_woken)(struct rq *this_rq, struct task_struct *task);
	void (*set_cpus_allowed)(struct task_struct *p, struct affinity_context *ctx);

	// ── CPU 上线/下线 ──
	void (*rq_online)(struct rq *rq);
	void (*rq_offline)(struct rq *rq);

	// ── Push/Pull 迁移 ──
	struct rq *(*find_lock_rq)(struct task_struct *p, struct rq *rq);

	// ── 周期调度 (tick) ──
	void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);

	// ── 任务生命周期 ──
	void (*task_fork)(struct task_struct *p);
	void (*task_dead)(struct task_struct *p);

	// ── 调度类切换 ──
	void (*switching_from)(struct rq *this_rq, struct task_struct *task);
	void (*switched_from) (struct rq *this_rq, struct task_struct *task);
	void (*switching_to)  (struct rq *this_rq, struct task_struct *task);
	void (*switched_to)   (struct rq *this_rq, struct task_struct *task);
	u64  (*get_prio)     (struct rq *this_rq, struct task_struct *task);
	void (*prio_changed) (struct rq *this_rq, struct task_struct *task,
			      u64 oldprio);

	// ── 权重变更 ──
	void (*reweight_task)(struct rq *this_rq, struct task_struct *task,
			      const struct load_weight *lw);

	// ── 策略查询 ──
	unsigned int (*get_rr_interval)(struct rq *rq,
					struct task_struct *task);
	void (*update_curr)(struct rq *rq);

#ifdef CONFIG_FAIR_GROUP_SCHED
	void (*task_change_group)(struct task_struct *p);
#endif
#ifdef CONFIG_SCHED_CORE
	int (*task_is_throttled)(struct task_struct *p, int cpu);
#endif
};
```

### 3.1 关键方法解释

| 方法 | 调用时机 | 关键实现 |
|------|----------|----------|
| `enqueue_task` | 任务变为 runnable，加入运行队列 | fair.c:6144, rt.c:1436, dl.c:2293 |
| `dequeue_task` | 任务变为 non-runnable，移出运行队列 | fair.c:7431, rt.c:1455, dl.c:2353 |
| `pick_next_task` | __schedule() 选择下一个任务 | fair.c:9233, rt.c:1709, dl.c:2630 |
| `task_tick` | 每次调度 tick 触发 | 各类更新 vruntime / 时间片 / 检查抢占 |
| `update_curr` | 更新当前任务的统计信息 | fair.c:1407, rt.c, dl.c |
| `wakeup_preempt` | 任务被唤醒时检查是否需要抢占 | CFS: wakeup_preempt, RT: 更高优先级即抢占 |
| `balance` | pick_next_task 前进行负载均衡 | RT: balance_rt, CFS: 拉取任务 |
| `select_task_rq` | 任务创建/唤醒时选择目标 CPU | 考虑 affinity, 负载, 功耗等 |
| `switched_from/to` | 任务改变调度类时通知 | fair.c, dl.c:1030 |

## 四、sched_class 实例

### 4.1. stop_sched_class

```c
// 定义在 kernel/sched/stop_task.c
DEFINE_SCHED_CLASS(stop)
```

- **最高优先级**，用于停止所有其他调度
- 仅用于内核内部操作，如 `migration` 内核线程
- 每个 CPU 只有一个 stop 任务，通过 `sched_set_stop_task()` 设置
- 不会被抢占（最高优先级）

### 4.2 fair_sched_class (fair.c:5)

```c
// kernel/sched/fair.c:305
const struct sched_class fair_sched_class;
```

- **CFS + EEVDF** 实现，见第一篇
- 处理 SCHED_NORMAL, SCHED_BATCH, SCHED_IDLE
- 使用 vruntime rbtree 组织任务
- 支持 cgroup 组调度和 CFS 带宽控制

### 4.3 idle_sched_class

```c
// 定义在 kernel/sched/idle.c
DEFINE_SCHED_CLASS(idle)
```

- **最低优先级**，每个 CPU 有一个 idle 任务 (PID 0)
- 只有没有其他任务可运行时才调度 idle
- idle 任务进入 CPU 空闲状态（如 HLT 指令）

## 五、调度策略映射

### 5.1 策略定义

```c
#define SCHED_NORMAL    0   // → fair_sched_class
#define SCHED_FIFO      1   // → rt_sched_class
#define SCHED_RR        2   // → rt_sched_class
#define SCHED_BATCH     3   // → fair_sched_class
#define SCHED_IDLE      5   // → fair_sched_class
#define SCHED_DEADLINE  6   // → dl_sched_class
#define SCHED_EXT       7   // → ext_sched_class (sched_ext)
```

### 5.2 策略 → 类的映射关系

```
SCHED_DEADLINE  ──→ dl_sched_class
SCHED_FIFO      ──→ rt_sched_class
SCHED_RR        ──→ rt_sched_class
SCHED_NORMAL    ──→ fair_sched_class
SCHED_BATCH     ──→ fair_sched_class
SCHED_IDLE      ──→ fair_sched_class (内部用 sched_idle_policy 区分)
SCHED_EXT       ──→ ext_sched_class
```

## 六、综合调度流程

```
schedule() 调用流程图:

schedule()
  │
  ├─→ pick_next_task(rq, prev, rf)
  │     │
  │     ├─→ sched class 链遍历 (从 stop 到 idle):
  │     │
  │     │   stop_sched_class->pick_next_task():  有 stop 任务? → return it
  │     │   dl_sched_class->pick_next_task():     有 dl 任务?  → return it (EDF)
  │     │   rt_sched_class->pick_next_task():     有 rt 任务?  → return it (最高 prio)
  │     │   fair_sched_class->pick_next_task():   有 CFS 任务? → return it (EEVDF)
  │     │   idle_sched_class->pick_next_task():   返回 idle 任务
  │     │
  │     └─→ 如果没有任务 → idle task
  │
  ├─→ context_switch(rq, prev, next)
  │     ├─→ prepare_task_switch()     // 准备切换
  │     ├─→ switch_mm_irqs_off()      // 切换地址空间 (如果不同进程)
  │     └─→ switch_to()               // 切换寄存器上下文 (汇编)
  │
  └─→ finish_task_switch()
        ├─→ 清理 prev 的资源
        └─→ 返回
```

## 七、优先级对比的多态实现

### 7.1 用 sched_class_above 比较优先级

```c
// kernel/sched/sched.h:2771
#define sched_class_above(_a, _b)	((_a) < (_b))
```

由于 sched_class 实例按 section 顺序排列，**指针地址越小 = 优先级越高**。

```
地址空间:
低地址                                             高地址
stop_sched_class  dl_sched_class  rt_sched_class  fair_sched_class  idle_sched_class
      ▲               ▲               ▲               ▲                 ▲
      │               │               │               │                 │
   最高优先级                                                    最低优先级
```

### 7.2 类内优先级比较

```
不同类之间:
  sched_class_above(A, B) → 比较指针地址

同类内:
  CFS: 比较 vruntime / EEVDF deadline
  RT:  比较 rt_priority (1..99, 数字越小优先级越高)
  DL:  比较 dl_deadline (EDF)
```

## 八、rq 的结构概览 (sched.h:1131)

```c
// kernel/sched/sched.h:1131
struct rq {
	/* hot cacheline */
	unsigned int		nr_running;
	unsigned long		cpu_capacity;
	union {
		struct task_struct __rcu *donor; /* Scheduler context */
		struct task_struct __rcu *curr;  /* Execution context */
	};
	struct task_struct	*idle;

	u64			nr_switches	____cacheline_aligned;

	/* runqueue lock: */
	raw_spinlock_t		__lock;

	/* 三个调度类的运行队列 */
	struct cfs_rq		cfs;
	struct rt_rq		rt;
	struct dl_rq		dl;

	// ... 更多字段 ...
	struct task_struct	*stop;              // stop 任务 (每 CPU 一个)
	unsigned long		next_balance;
	struct mm_struct	*prev_mm;
	unsigned int		clock_update_flags;
	u64			clock;
	u64			clock_task;

	// ... 负载均衡相关字段 ...
	struct root_domain	*rd;
	struct sched_domain	*sd;
	// ...
};
```

**rq 锁规则**：
- 单 rq 操作：`rq_lock(rq)` → `raw_spin_lock(&rq->__lock)`
- 多 rq 操作：按 rq 地址升序加锁（避免死锁）
- 所有调度类操作都在 rq->lock 保护下进行

## 九、完整示例：任务从创建到运行的调度轨迹

```
1. fork() 创建新任务
   ├→ sched_fork()
   │    ├→ __sched_fork()     // 初始化 sched_entity, rt, dl 等
   │    └→ 设置 sched_class = &fair_sched_class (默认)
   │
2. wake_up_new_task()
   ├→ 设置 prio = normal_prio = NICE_TO_PRIO(nice)
   ├→ sched_class->enqueue_task(rq, p, flags)
   │    └→ fair_sched_class: enqueue_task_fair()
   │         └→ 插入 cfs_rq->tasks_timeline rbtree
   ├→ check_preempt_curr()
   │    └→ 检查是否需要抢占当前任务
   │
3. tick → scheduler_tick()
   ├→ curr->sched_class->task_tick()
   │    └→ fair: update_curr() → 递进 vruntime
   ├→ 检查是否需要设置 TIF_NEED_RESCHED
   │
4. schedule() 被调用 (因为 TIF_NEED_RESCHED)
   ├→ pick_next_task()
   │    └→ 遍历 sched_class 链，fair_sched_class 返回新任务
   ├→ context_switch()
   │    └→ 切换到新任务
   │
5. 任务开始运行
   ├→ exec_start = rq_clock()
   ├→ 每次 tick 更新 vruntime
   └→ 时间片用完 → 设置 TIF_NEED_RESCHED → schedule()
```

## 十、总结

```
        优先级     │  调度类      │  策略
───────────────────┼──────────────┼───────────────────
  prio 0   (最高)  │ stop_sched   │ (内部使用)
  prio 0   (隐式)  │ dl_sched     │ SCHED_DEADLINE
  prio 1..99       │ rt_sched     │ SCHED_FIFO, SCHED_RR
  prio 100..139    │ fair_sched   │ SCHED_{NORMAL,BATCH,IDLE}
  prio ~∞  (最低)  │ idle_sched   │ (per-CPU idle task)
───────────────────┴──────────────┴───────────────────

指针比较决定类优先级: sched_class_above(a, b) = (a < b)
pick_next_task: 从高到低遍历类，第一个有 runnable 任务的类获胜
```

## 下一篇文章

[第三篇：RT 与 Deadline — 实时调度的两种面孔](./03-rt-dl.md)
