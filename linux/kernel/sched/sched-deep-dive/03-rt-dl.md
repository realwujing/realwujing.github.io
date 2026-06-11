# 第三篇：RT 与 Deadline — 实时调度的两种面孔

> 源码：`kernel/sched/rt.c`, `kernel/sched/deadline.c`, `kernel/sched/cpupri.c` | 头文件：`kernel/sched/sched.h`

> 系列目录：[调度器 内核源码深度解析](./README.md)

## 一、实时调度概述

Linux 提供两种实时调度策略，保证高优先级任务获得确定的调度延迟：

| 策略 | 调度类 | 排队算法 | 时间片 | 优先级 |
|------|--------|----------|--------|--------|
| SCHED_DEADLINE | dl_sched_class | EDF + CBS | 由用户指定 runtime/period | (in-band 最高) |
| SCHED_FIFO | rt_sched_class | 按优先级 FIFO | 无限 (直到阻塞) | 1..99 |
| SCHED_RR | rt_sched_class | 按优先级 RR | `sched_rr_timeslice` | 1..99 |

**关键原则**：RT 和 DL 任务优先级始终高于 CFS 任务。只要有一个 RT/DL 任务可运行，任何 CFS 任务都无法获得 CPU。

## 二、RT 调度器 — SCHED_FIFO / SCHED_RR

### 2.1 源文件头注释 (rt.c:1-5)

```c
// kernel/sched/rt.c:1
/*
 * Real-Time Scheduling Class (mapped to the SCHED_FIFO and SCHED_RR
 * policies)
 */
```

### 2.2 核心数据结构

#### struct rt_prio_array (sched.h:311-314)

RT 调度器的核心数据结构，用 **bitset + 链表** 实现 O(1) 查找和 O(1) 出队。

```c
// kernel/sched/sched.h:311
struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); /* include 1 bit for delimiter */
	struct list_head queue[MAX_RT_PRIO];
};
```

#### struct rt_rq (sched.h:838-865)

```c
// kernel/sched/sched.h:838
struct rt_rq {
	struct rt_prio_array	active;
	unsigned int		rt_nr_running;
	unsigned int		rr_nr_running;
	struct {
		int		curr; /* highest queued rt task prio */
		int		next; /* next highest */
	} highest_prio;
	bool			overloaded;
	struct plist_head	pushable_tasks;

	int			rt_queued;

#ifdef CONFIG_RT_GROUP_SCHED
	int			rt_throttled;
	u64			rt_time;
	u64			rt_runtime;
	raw_spinlock_t		rt_runtime_lock;
	unsigned int		rt_nr_boosted;
	struct rq		*rq;
#endif
#ifdef CONFIG_CGROUP_SCHED
	struct task_group	*tg;
#endif
};
```

**`rt_prio_array` 结构示意**：

```
bitmap:  [0][1][2]...[49][50][51]...[98][99][100]   ← 100 bits + 1 delimiter
            │  │         │   │                    │
            │  │         │   │                    └─ MAX_RT_PRIO = delimiter
            │  │         │   └─ 优先级 50 有任务?  → if (test_bit(50, bitmap))
            │  │         └── 优先级 99 有任务?  → if (test_bit(99, bitmap))
            │  └─ 优先级 2 有任务?  → if (test_bit(2, bitmap))
            └─ 优先级 0..1 无任务

queue[0]:  → [FIFO task A, prio=1] → [RR task B, prio=1]
queue[1]:  → [FIFO task C, prio=2]
queue[2]:  → (empty)
...
queue[49]: → [FIFO task D, prio=50] → [RR task E, prio=50]
queue[50]: → (empty)
...
queue[99]: → (empty)
```

**查找最高优先级任务 = O(1)**：`ffz()` / `find_first_bit()` 查找 bitmap 第一个已设置的位。

#### struct sched_rt_entity (include/linux/sched.h:623-629)

```c
// include/linux/sched.h:623
struct sched_rt_entity {
	struct list_head		run_list;
	unsigned long			timeout;
	unsigned long			watchdog_stamp;
	unsigned int			time_slice;
	unsigned short			on_rq;
	unsigned short			on_list;
	// ...
};
```

### 2.3 SCHED_FIFO vs SCHED_RR

```
SCHED_FIFO:
  任务 A (prio=80)
  任务 B (prio=60)
  任务 C (prio=60)

  运行顺序: A 运行直到阻塞/让出 → B 或 C 中选一个 (谁在前面)
           FIFO 同优先级之间无时间片轮转
           ───────────────────────────
           只有在不同优先级之间才有抢占

SCHED_RR:
  任务 A (prio=80)
  任务 B (prio=60, time_slice=100ms)
  任务 C (prio=60, time_slice=100ms)

  运行顺序: A 运行直到阻塞 → B 运行 100ms → C 运行 100ms → B 运行 100ms → ...
           ───────────────────────────
           RR 同优先级之间按时间片轮转
```

### 2.4 init_rt_rq 初始化 (rt.c:68-95)

```c
// kernel/sched/rt.c:68
void init_rt_rq(struct rt_rq *rt_rq)
{
	struct rt_prio_array *array;
	int i;

	array = &rt_rq->active;
	for (i = 0; i < MAX_RT_PRIO; i++) {
		INIT_LIST_HEAD(array->queue + i);
		__clear_bit(i, array->bitmap);
	}
	/* delimiter for bitsearch: */
	__set_bit(MAX_RT_PRIO, array->bitmap);

	rt_rq->highest_prio.curr = MAX_RT_PRIO-1;
	rt_rq->highest_prio.next = MAX_RT_PRIO-1;
	rt_rq->overloaded = 0;
	plist_head_init(&rt_rq->pushable_tasks);
	rt_rq->rt_queued = 0;
	// ... 组调度初始化 ...
}
```

**delimiter 的作用**：`__set_bit(MAX_RT_PRIO, array->bitmap)` 设置第 100 位作为 "标记"，这样 `find_first_bit()` 在找不到 runnable 任务时返回 100，而不是返回一个无意义的极大值。

### 2.5 入队 / 出队

#### enqueue_task_rt (rt.c:1435-1453)

```c
// kernel/sched/rt.c:1435
static void enqueue_task_rt(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_rt_entity *rt_se = &p->rt;

	if (flags & ENQUEUE_WAKEUP)
		rt_se->timeout = 0;

	check_schedstat_required();
	update_stats_wait_start_rt(rt_rq_of_se(rt_se), rt_se);

	enqueue_rt_entity(rt_se, flags);

	if (task_is_blocked(p))
		return;

	if (!task_current(rq, p) && p->nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
}
```

#### dequeue_task_rt (rt.c:1455-1465)

```c
// kernel/sched/rt.c:1455
static bool dequeue_task_rt(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_rt_entity *rt_se = &p->rt;

	update_curr_rt(rq);
	dequeue_rt_entity(rt_se, flags);

	dequeue_pushable_task(rq, p);

	return true;
}
```

**入队/出队的关键操作**：
1. 从 `active.queue[prio]` 链表中添加/移除 `sched_rt_entity`
2. 更新 `active.bitmap`：如果该优先级队列从空变为不空 → set bit；从不空变为空 → clear bit
3. 更新 `rt_rq->highest_prio` 缓存
4. 如果任务可以在多个 CPU 上运行，加入 `pushable_tasks` 列表

### 2.6 pick_task_rt (rt.c:1709-1719)

```c
// kernel/sched/rt.c:1709
static struct task_struct *pick_task_rt(struct rq *rq, struct rq_flags *rf)
{
	struct task_struct *p;

	if (!sched_rt_runnable(rq))
		return NULL;

	p = _pick_next_task_rt(rq);

	return p;
}
```

内部 `_pick_next_task_rt` 的逻辑：
1. `sched_find_first_bit(rt_rq->active.bitmap)` → 找到最高优先级 `idx`
2. 从 `active.queue[idx]` 中取头结点
3. 如果是 RR 任务且时间片用完，移到队列尾部并重置时间片

### 2.7 RT 带宽控制

```c
// kernel/sched/rt.c:10-24
int sched_rr_timeslice = RR_TIMESLICE;

/*
 * period over which we measure -rt task CPU usage in us.
 * default: 1s
 */
int sysctl_sched_rt_period = 1000000;

/*
 * part of the period that we allow rt tasks to run in us.
 * default: 0.95s
 */
int sysctl_sched_rt_runtime = 950000;
```

**RT 带宽机制**：
- 每 1s 周期内，RT 任务最多使用 0.95s 的 CPU 时间
- 保留 5% 给 CFS 任务（防止 RT 任务完全饿死 CFS）
- 如果 RT 运行时间超过限制，RT runqueue 被 **throttle**（节流）
- `struct rt_bandwidth` 管理 RT 带宽(sched.h:316-323)

```c
// kernel/sched/sched.h:316
struct rt_bandwidth {
	/* nests inside the rq lock: */
	raw_spinlock_t		rt_runtime_lock;
	ktime_t			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_timer;
	unsigned int		rt_period_active;
};
```

### 2.8 Push/Pull 迁移

RT 调度器支持 **push/pull** 机制，用于将 RT 任务迁移到合适的 CPU：

```
push_rt_tasks(rq):                     pull_rt_task(this_rq):
  ┌──────────────────────────────┐      ┌──────────────────────────────┐
  │ 当前 CPU 有多个 RT 任务       │      │ 当前 CPU 空闲，但有 RT 任务    │
  │ 无法全部运行                 │      │ 在别的 CPU 上等待             │
  │                             │      │                             │
  │ push_rt_task() 循环:         │      │ 遍历所有 overloaded CPU       │
  │   find_lock_lowest_rq()     │      │   锁定源 rq                  │
  │   → 找一个允许运行的最低优先级 │      │   取最高优先级的 pushable 任务 │
  │     CPU                     │      │   迁移到 this_rq             │
  │   deactivate_task() +       │      │                             │
  │   activate_task() 迁移      │      │                             │
  └──────────────────────────────┘      └──────────────────────────────┘
```

**push_rt_tasks 源码定位**：

```c
// kernel/sched/rt.c:2071
static void push_rt_tasks(struct rq *rq)
{
	/* push_rt_task will return true if it moved an RT */
	while (push_rt_task(rq, false))
		;
}

// kernel/sched/rt.c:1953
static int push_rt_task(struct rq *rq, bool pull)

// kernel/sched/rt.c:1890
static struct rq *find_lock_lowest_rq(struct task_struct *task, struct rq *rq)

// kernel/sched/rt.c:2254
static void pull_rt_task(struct rq *this_rq)
```

**balance_rt (rt.c:1599)** — 在 pick_next_task 前调用：

```c
// kernel/sched/rt.c:1599
static int balance_rt(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
{
	if (!on_rt_rq(&p->rt) && need_pull_rt_task(rq, p)) {
		rq_unpin_lock(rq, rf);
		pull_rt_task(rq);
		// ...
	}
	// ...
}
```

### 2.9 RT 抢占规则

```
新任务被唤醒:
  if (新任务.prio < 当前任务.prio)
      设置 TIF_NEED_RESCHED 抢占

RR 同优先级时间片用完:
  requeue_task_rt() → 移到队尾
  设置 TIF_NEED_RESCHED
```

## 三、Deadline 调度器 — SCHED_DEADLINE

### 3.1 源文件头注释 (deadline.c:1-17)

```c
// kernel/sched/deadline.c:1
/*
 * Deadline Scheduling Class (SCHED_DEADLINE)
 *
 * Earliest Deadline First (EDF) + Constant Bandwidth Server (CBS).
 *
 * Tasks that periodically executes their instances for less than their
 * runtime won't miss any of their deadlines.
 * Tasks that are not periodic or sporadic or that tries to execute more
 * than their reserved bandwidth will be slowed down (and may potentially
 * miss some of their deadlines), and won't affect any other task.
 */
```

### 3.2 SCHED_DEADLINE 参数

每个 DL 任务有三个参数：

| 参数 | 含义 | 约束 |
|------|------|------|
| `sched_runtime` | 每个周期内的执行预算 | runtime ≤ deadline ≤ period |
| `sched_deadline` | 每周期内的截止时间 | |
| `sched_period` | 任务周期 | 100μs ≤ period ≤ 4s |

```
时间轴示例 (runtime=5ms, deadline=15ms, period=20ms):
  0ms    5ms        15ms    20ms    25ms       35ms    40ms
  ├──────┼──────────┼───────┼───────┼──────────┼───────┤
  │ run  │   sleep  │       │ run   │   sleep  │       │
  │< 5ms>│          │       │< 5ms> │          │       │
  │      └─deadline─┘       │       └─deadline─┘       │
  └────────── period ──────┘└────────── period ───────┘
```

### 3.3 带宽约束

DL 调度器有两个核心 sysctl 参数：

```c
// kernel/sched/deadline.c:31-32
static unsigned int sysctl_sched_dl_period_max = 1 << 22; /* ~4 seconds */
static unsigned int sysctl_sched_dl_period_min = 100;     /* 100 us */
```

**准入控制**：只有 `runtime / period ≤ 可用的 CPU 带宽` 时才允许创建 DL 任务：

```c
// kernel/sched/sched.h:348
struct dl_bw {
	raw_spinlock_t		lock;
	u64			bw;
	u64			total_bw;
};
```

```
CPU0 带宽: 100%
├── DL Task A: runtime=3ms, period=10ms → 利用 30%
├── DL Task B: runtime=2ms, period=20ms → 利用 10%
├── DL Task C: runtime=5ms, period=10ms → 利用 50%
│
├── 总 DL 带宽: 90% ← 可以接受
└── 剩余 10% → CFS/RT 任务
```

### 3.4 核心数据结构

#### struct dl_rq (sched.h:873-920)

```c
// kernel/sched/sched.h:873
struct dl_rq {
	/* runqueue is an rbtree, ordered by deadline */
	struct rb_root_cached	root;

	unsigned int		dl_nr_running;

	/*
	 * Deadline values of the currently executing and the
	 * earliest ready task on this rq. Caching these facilitates
	 * the decision whether or not a ready but not running task
	 * should migrate somewhere else.
	 */
	struct {
		u64		curr;
		u64		next;
	} earliest_dl;

	bool				overloaded;

	/*
	 * Tasks on this rq that can be pushed away. They are kept in
	 * an rb-tree, ordered by tasks' deadlines, with caching
	 * of the leftmost (earliest deadline) element.
	 */
	struct rb_root_cached		pushable_dl_tasks_root;

	/*
	 * "Active utilization" for this runqueue: increased when a
	 * task wakes up (becomes TASK_RUNNING) and decreased when a
	 * task blocks
	 */
	u64				running_bw;

	/*
	 * Utilization of the tasks "assigned" to this runqueue (including
	 * the tasks that are in runqueue and the tasks that executed on this
	 * CPU and blocked).
	 */
	u64				this_bw;
};
```

```
dl_rq 的结构:

  root (rb_root_cached):
       [D: deadline=5ms]
       /               \
  [A: d=3ms]     [F: d=8ms]
   /      \        /      \
[B: d=1ms] [C: d=4ms] [E: d=7ms] [G: d=10ms]
     ↑
  pick_next_task 选这个 (最早 deadline)
```

### 3.5 EDF + CBS 详解

#### EDF (Earliest Deadline First)

EDF 选择 deadline 最早的任务运行。这是最优的实时调度算法：只要能满足 deadline，EDF 就能调度。

#### CBS (Constant Bandwidth Server)

CBS 确保每个 DL 任务不会使用超过其预留的 CPU 带宽：

```
当任务消耗完 runtime:
  runtime = 0
  → throttling: dl_throttled = 1
  → 设置 replenishment timer → 在下一个 period 开始时补充 runtime

当任务提前完成:
  runtime 没用完，剩余部分可在当前 period 内继续使用
```

**EDF 抢占判断 (sched.h:301-306)**：

```c
// kernel/sched/sched.h:301
static inline bool dl_entity_preempt(const struct sched_dl_entity *a,
				     const struct sched_dl_entity *b)
{
	return dl_entity_is_special(a) ||
	       dl_time_before(a->deadline, b->deadline);
}
```

### 3.6 入队 / 出队

#### enqueue_task_dl (deadline.c:2293-2350)

```c
// kernel/sched/deadline.c:2293
static void enqueue_task_dl(struct rq *rq, struct task_struct *p, int flags)
{
	if (is_dl_boosted(&p->dl)) {
		// PI 提升的优先级覆盖 throttling
		if (p->dl.dl_throttled) {
			cancel_replenish_timer(&p->dl);
			p->dl.dl_throttled = 0;
		}
	} else if (!dl_prio(p->normal_prio)) {
		// 非 DL 任务被 deboost
		p->dl.dl_throttled = 0;
		if (!(flags & ENQUEUE_REPLENISH))
			printk_deferred_once("sched: DL de-boosted task PID %d: "
					     "REPLENISH flag missing\n", task_pid_nr(p));
		return;
	}

	check_schedstat_required();
	update_stats_wait_start_dl(dl_rq_of_se(&p->dl), &p->dl);

	if (p->on_rq == TASK_ON_RQ_MIGRATING)
		flags |= ENQUEUE_MIGRATING;

	enqueue_dl_entity(&p->dl, flags);

	if (dl_server(&p->dl))
		return;

	if (task_is_blocked(p))
		return;

	if (!task_current(rq, p) && !p->dl.dl_throttled && p->nr_cpus_allowed > 1)
		enqueue_pushable_dl_task(rq, p);
}
```

#### __enqueue_dl_entity / __dequeue_dl_entity (deadline.c:2164-2185)

```c
// kernel/sched/deadline.c:2164
static void __enqueue_dl_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = dl_rq_of_se(dl_se);

	WARN_ON_ONCE(!RB_EMPTY_NODE(&dl_se->rb_node));

	rb_add_cached(&dl_se->rb_node, &dl_rq->root, __dl_less);

	inc_dl_tasks(dl_se, dl_rq);
}

// kernel/sched/deadline.c:2175
static void __dequeue_dl_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = dl_rq_of_se(dl_se);

	if (RB_EMPTY_NODE(&dl_se->rb_node))
		return;

	rb_erase_cached(&dl_se->rb_node, &dl_rq->root);
	RB_CLEAR_NODE(&dl_se->rb_node);

	dec_dl_tasks(dl_se, dl_rq);
}
```

### 3.7 pick_task_dl (deadline.c:2603-2628)

```c
// kernel/sched/deadline.c:2603
static struct task_struct *__pick_task_dl(struct rq *rq, struct rq_flags *rf)
{
	struct sched_dl_entity *dl_se;
	struct dl_rq *dl_rq = &rq->dl;
	struct task_struct *p;

again:
	if (!sched_dl_runnable(rq))
		return NULL;

	dl_se = pick_next_dl_entity(dl_rq);     // 从 rbtree 取最左节点
	WARN_ON_ONCE(!dl_se);

	if (dl_server(dl_se)) {
		p = dl_se->server_pick_task(dl_se, rf);
		if (!p) {
			dl_server_stop(dl_se);
			goto again;
		}
		rq->dl_server = dl_se;
	} else {
		p = dl_task_of(dl_se);
	}

	return p;
}
```

**`pick_next_dl_entity` 就是取 rbtree 的最左节点（cached rb_root_cached）**：

```c
// kernel/sched/deadline.c:2587
static struct sched_dl_entity *pick_next_dl_entity(struct dl_rq *dl_rq)
{
	struct rb_node *left = rb_first_cached(&dl_rq->root);

	if (!left)
		return NULL;

	return __node_2_dle(left);
}
```

### 3.8 DL Server — 嵌套调度

DL Server 允许 DL 调度类"托管"其他调度类的任务：

```
DL Server 作为外层:
  dl_se->rq → 我们所在的 CPU
  dl_se->server_pick_task() → 委托给 fair/ext 类进行内层选择

当 CFS 任务在 DL Server 内运行时:
  1. DL scheduler tick 更新 dl_se 的 runtime
  2. dl_se runtime 耗尽 → dl_server_stop()
  3. dl_se deadline 未到 → defer 到下一个 period
```

**dl_server 相关 API (sched.h:411-427)**：

```c
extern void dl_server_update_idle(struct sched_dl_entity *dl_se, s64 delta_exec);
extern void dl_server_update(struct sched_dl_entity *dl_se, s64 delta_exec);
extern void dl_server_start(struct sched_dl_entity *dl_se);
extern void dl_server_stop(struct sched_dl_entity *dl_se);
extern void dl_server_init(struct sched_dl_entity *dl_se, struct rq *rq,
		    dl_server_pick_f pick_task);
extern void sched_init_dl_servers(void);
extern void fair_server_init(struct rq *rq);
```

### 3.9 DL Push/Pull

和 RT 类似，DL 也有 push/pull 迁移：

```c
// dl_rq 中的 pushable_dl_tasks_root (sched.h:897)
struct rb_root_cached	pushable_dl_tasks_root;
```

DL push/pull 逻辑类似于 RT，但按 deadline 排序（EDF）。

## 四、DL Server：fair_server 和 ext_server

在现代内核中，DL Server 用于实现两种嵌套调度：

```
rq
├── fair_server (一种 dl_server)
│   └── 托管 CFS 任务，提供定额的时间保证
│
└── ext_server (CONFIG_SCHED_CLASS_EXT)
    └── 托管 sched_ext 任务
```

**为什么需要 DL Server**：它能确保低 priority 的调度类（如 CFS）在 RT/DL 高负载下仍能获得最小时间保证，避免完全饿死。

## 五、cpupri.c — CPU 优先级跟踪 (kernel/sched/cpupri.c)

`cpupri` 是 RT push/pull 迁移的关键辅助模块。它维护一个全局的 CPU 优先级矩阵，用于快速找到能接受某个优先级任务的 CPU。

**核心思想**：
- 每个 CPU 维护其"最高非运行 RT 任务的优先级"
- `cpupri_find()` 快速查询哪些 CPU 能接受特定优先级的任务
- `find_lowest_rq()` 使用 cpupri 找到目标 CPU

## 六、RT 和 DL 的完整调度流程

```
schedule() 被调用
  │
  ├─→ pick_next_task()
  │     │
  │     ├─→ stop_sched_class->pick_next_task()    // 最高优先级
  │     │     └→ 有 stop 任务? → 直接返回
  │     │
  │     ├─→ dl_sched_class->pick_next_task()      // Deadline
  │     │     └→ pick_next_dl_entity() → EDF 选 deadline 最小的
  │     │         ├→ dl_server? → 委托 server_pick_task()
  │     │         └→ 直接返回 dl_task_of(dl_se)
  │     │     └→ CBS: 检查/更新 runtime, 必要时 throttle
  │     │
  │     ├─→ rt_sched_class->pick_next_task()      // RT
  │     │     └→ sched_find_first_bit(bitmap) → O(1) 找最高优先级
  │     │     └→ 从对应链表中取头结点
  │     │     └→ SCHED_RR: 时间片用完 → 移到队尾
  │     │
  │     ├─→ fair_sched_class->pick_next_task()    // CFS
  │     │     └→ pick_eevdf() → EEVDF 选 deadline 最小的
  │     │
  │     └─→ idle_sched_class->pick_next_task()    // idle
  │
  └─→ context_switch(prev, next)
```

## 七、总结

```
┌──────────────────────────────────────────────────────────────────┐
│                      实时调度全景                                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  SCHED_DEADLINE (dl_sched_class)                                 │
│  ├── 算法: EDF (Earliest Deadline First) + CBS                    │
│  ├── 数据结构: dl_rq → rbtree (按 deadline 排序)                   │
│  ├── 参数: {runtime, deadline, period}                            │
│  ├── 选择: pick_next_dl_entity() → rbtree 最左节点                 │
│  ├── 带宽: sysctl_sched_dl_period_{min,max}                       │
│  └── 抢占: dl_entity_preempt() → deadline 更早?                    │
│                                                                  │
│  SCHED_FIFO / SCHED_RR (rt_sched_class)                          │
│  ├── 算法: 按优先级排队 (FIFO/RR)                                  │
│  ├── 数据结构: rt_prio_array → bitmap + 100 list_heads             │
│  ├── 参数: rt_priority (1..99), sched_rr_timeslice                 │
│  ├── 选择: find_first_bit(bitmap) → 最高优先级链表头                │
│  ├── 带宽: sysctl_sched_rt_period_us / runtime_us                  │
│  └── 抢占: 更高优先级直接抢占                                      │
│                                                                  │
│  共同特性:                                                        │
│  ├── Push/Pull 迁移: 将 RT/DL 任务迁移到合适的 CPU                 │
│  ├── cpupri: CPU 优先级矩阵，辅助迁移决策                          │
│  ├── 带宽控制 (throttling): 防止 RT/DL 饿死 CFS                     │
│  ├── PI (priority inheritance): 防止优先级反转                     │
│  └── 优先级始终高于 CFS (除了 SCHED_DEADLINE = 0 有一个特殊位置)    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

## 下一篇文章

[第四篇：负载均衡与调度域 — 多核均衡的艺术](./04-load-balance.md)
