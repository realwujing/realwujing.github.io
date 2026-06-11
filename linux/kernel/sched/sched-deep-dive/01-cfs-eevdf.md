# 第一篇：CFS 与 EEVDF — 公平调度与 vruntime

> 源码：`kernel/sched/fair.c` | 头文件：`kernel/sched/fair.c`, `kernel/sched/sched.h`, `include/linux/sched.h`

> 系列目录：[调度器 内核源码深度解析](./README.md)

## 一、CFS 核心思想

CFS (Completely Fair Scheduler) 是 Linux 内核默认的进程调度器，自 2.6.23 引入。它的核心思想极其简洁：**让每个任务获得公平的 CPU 时间**。

传统调度器按优先级赋予固定时间片（如 O(1) 调度器的 `timeslice`），而 CFS 转而追踪每个任务已经消耗了多少 CPU 时间，始终选择消耗最少的任务来运行。

### 1.1 三个关键概念

| 概念 | 变量 | 含义 |
|------|------|------|
| 实际运行时间 | `sum_exec_runtime` | 任务在 CPU 上实际运行的纳秒数 |
| 权重 (weight) | `se->load.weight` | 由 nice 值决定，nice 越低（优先级越高）权重越大 |
| 虚拟运行时间 | `vruntime` | 实际运行时间按权重归一化后的值 = `delta_exec * NICE_0_LOAD / weight` |

**直观理解**：如果一个任务权重是另一个的两倍（更高优先级），那它获得同样的实际运行时间时，vruntime 只增长一半，从而在 rbtree 中保持较小的虚拟时间值，被选择的机会更多。

### 1.2 核心算法

```
每次 tick 或调度事件发生时：
  1. update_curr()   → 增加当前任务的 vruntime
  2. pick_next_task() → 从 rbtree 中选取 vruntime 最小的任务
  3. context_switch() → 切换到该任务
```

## 二、核心数据结构

### 2.1 struct sched_entity (include/linux/sched.h:575-621)

```c
// include/linux/sched.h:575
struct sched_entity {
	/* For load-balancing: */
	struct load_weight		load;           // 权重 (weight + inv_weight)
	struct rb_node			run_node;       // 在 cfs_rq 的 rbtree 中的节点
	u64				deadline;       // EEVDF 虚拟截止时间
	u64				min_vruntime;   // EEVDF: 子树最小 vruntime (augmented rbtree)
	u64				min_slice;      // 最小调度粒度
	u64				max_slice;      // 最大调度粒度

	struct list_head		group_node;     // 连接到 cfs_tasks 链表
	unsigned char			on_rq;          // 是否在运行队列上
	unsigned char			sched_delayed;
	unsigned char			rel_deadline;
	unsigned char			custom_slice;
					/* hole */

	u64				exec_start;             // 本次执行开始时间
	u64				sum_exec_runtime;       // 累计实际运行时间
	u64				prev_sum_exec_runtime;  // 本次调度前累计
	u64				vruntime;               // 虚拟运行时间 ★核心★
	/* Approximated virtual lag: */
	s64				vlag;                   // EEVDF: 虚拟滞后量
	/* 'Protected' deadline, to give out minimum quantums: */
	u64				vprot;
	u64				slice;                  // EEVDF: 本次分配的时间片

	u64				nr_migrations;

#ifdef CONFIG_FAIR_GROUP_SCHED
	int				depth;
	struct sched_entity		*parent;
	struct cfs_rq			*cfs_rq;
	struct cfs_rq			*my_q;
	unsigned long			runnable_weight;
#endif

	/*
	 * Per entity load average tracking.
	 *
	 * Put into separate cache line so it does not
	 * collide with read-mostly values above.
	 */
	struct sched_avg		avg;            // PELT 负载跟踪
};
```

**关键字段解读**：
- `load`: 包含 `weight` 和预先计算好的 `inv_weight`，用于快速除法
- `vruntime`: 任务的虚拟运行时间，决定在 rbtree 中的位置
- `deadline`: EEVDF 中每个任务的虚拟截止时间 (vruntime + slice)
- `vlag`: 虚拟滞后量，用于衡量任务相对公平的偏差
- `slice`: EEVDF 分配给任务的运行时间片
- `exec_start` / `sum_exec_runtime`: 分别记录本次和累计运行时间
- `avg`: PELT 负载跟踪的平均值

### 2.2 struct cfs_rq (kernel/sched/sched.h:678-773)

```c
// kernel/sched/sched.h:678
struct cfs_rq {
	struct load_weight	load;
	unsigned int		nr_queued;              // 入队任务数
	unsigned int		h_nr_queued;            // 层次入队数
	unsigned int		h_nr_runnable;
	unsigned int		h_nr_idle;

	s64			sum_w_vruntime;         // 加权 vruntime 总和
	u64			sum_weight;             // 权重总和
	u64			zero_vruntime;          // 基准 vruntime
	unsigned int		sum_shift;

#ifdef CONFIG_SCHED_CORE
	unsigned int		forceidle_seq;
	u64			zero_vruntime_fi;
#endif

	struct rb_root_cached	tasks_timeline;         // ★ rbtree: 按 deadline 排序的任务树 ★

	struct sched_entity	*curr;                  // 当前运行的实体
	struct sched_entity	*next;                  // 下一个要运行的实体 (buddy)

	struct sched_avg	avg;                    // CFS 队列的 PELT 统计
#ifndef CONFIG_64BIT
	u64			last_update_time_copy;
#endif
	struct {
		raw_spinlock_t	lock ____cacheline_aligned;
		int		nr;
		unsigned long	load_avg;
		unsigned long	util_avg;
		unsigned long	runnable_avg;
	} removed;

#ifdef CONFIG_FAIR_GROUP_SCHED
	// ... 组调度相关字段 ...
	struct task_group	*tg;
#endif
};
```

### 2.3 任务与队列的关系

```
rq (每 CPU 运行队列, sched.h:1131)
├── cfs   (struct cfs_rq)      ← CFS 的 runqueue
│   ├── tasks_timeline         ← rb_root_cached, EEVDF rbtree
│   │   ├── sched_entity (task A, vruntime=1000)
│   │   ├── sched_entity (task B, vruntime=1500)
│   │   └── sched_entity (task C, vruntime=2000)
│   └── curr → task A
├── rt    (struct rt_rq)
├── dl    (struct dl_rq)
├── nr_running                 ← 总运行任务数
└── __lock                     ← rq 的自旋锁
```

## 三、vruntime 的递进机制 — update_curr

### 3.1 update_curr (fair.c:1407-1453)

```c
// kernel/sched/fair.c:1407
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;
	struct rq *rq = rq_of(cfs_rq);
	s64 delta_exec;
	bool resched;

	if (unlikely(!curr))
		return;

	delta_exec = update_se(rq, curr);       // 计算本次执行的实际时间
	if (unlikely(delta_exec <= 0))
		return;

	curr->vruntime += calc_delta_fair(delta_exec, curr);   // ★ 核心: vruntime 递进 ★
	resched = update_deadline(cfs_rq, curr);                // EEVDF: 更新 deadline

	if (entity_is_task(curr)) {
		dl_server_update(&rq->fair_server, delta_exec);
	}

	account_cfs_rq_runtime(cfs_rq, delta_exec);

	if (cfs_rq->nr_queued == 1)
		return;

	if (resched || !protect_slice(curr)) {
		resched_curr_lazy(rq);                // 设置 need_resched 标志
		clear_buddies(cfs_rq, curr);
	}
}
```

**update_curr 做了三件事**：
1. 计算本次执行的 delta_exec（实际纳秒数）
2. 将 `delta_exec` 转换为 `vruntime` 增量：`calc_delta_fair(delta_exec, curr)`
3. EEVDF 阶段：检查 deadline 是否到期，必要时设置 resched

### 3.2 calc_delta_fair (fair.c:294-303)

```c
// kernel/sched/fair.c:294
/*
 * delta /= w
 */
static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
	if (unlikely(se->load.weight != NICE_0_LOAD))
		delta = __calc_delta(delta, NICE_0_LOAD, &se->load);

	return delta;
}
```

**公式**：

```
vruntime_delta = delta_exec * NICE_0_LOAD / weight
```

其中：
- `NICE_0_LOAD` = `1L << NICE_0_LOAD_SHIFT`，64 位下为 `1 << 20` = 1048576，对应 nice=0 的权重
- `weight` 来自 `sched_prio_to_weight[40]`，nice=0 时 weight=1024 (未缩放) 或 1048576 (缩放后)

**示例**：假设 nice=0 (weight=1024, NICE_0_LOAD=1048576)
- delta_exec = 1ms = 1,000,000 ns
- vruntime_delta = 1,000,000 * 1048576 / 1048576 = 1,000,000

假设 nice=5 (weight=335):
- vruntime_delta = 1,000,000 * 1048576 / 335 ≈ 3,130,000

**nice 越大，vruntime 增长越快，被选中的机会越少！**

### 3.3 sched_class->update_curr 的调用链

```
tick 中断
  └→ scheduler_tick()          (core.c)
       └→ task_tick()           (core.c)
            └→ sched_class->task_tick()
                 └→ fair_sched_class.task_tick = task_tick_fair()
                      └→ update_curr()          ← 递进当前任务 vruntime
```

同时，在出队/入队/调度选择时也会调用 update_curr 以保证 vruntime 及时更新。

## 四、EEVDF — Earliest Eligible Virtual Deadline First

Linux 6.6 引入了 EEVDF，替换了纯粹的 CFS（按最小 vruntime 选择）。EEVDF 的核心改进是：**不仅考虑 vruntime，还考虑 deadline**。

### 4.1 为什么需要 EEVDF

CFS 只比较 vruntime，但一个轻量级任务可能在极短时间内获得大量调度机会，导致整体延迟波动。EEVDF 引入了 **虚拟截止时间 (deadline)** 的概念：

```
deadline = vruntime + slice
```

其中 `slice` 是该任务在本次调度周期中被分配的时间片。

**选择规则**：
1. 首先确定"合格" (eligible) 的任务集合：`vruntime <= cfs_rq->zero_vruntime`
2. 从合格任务中选择 deadline 最小的

### 4.2 pick_eevdf — 核心选择函数 (fair.c:1136-1205)

```c
// kernel/sched/fair.c:1136
static struct sched_entity *pick_eevdf(struct cfs_rq *cfs_rq, bool protect)
{
	struct rb_node *node = cfs_rq->tasks_timeline.rb_root.rb_node;
	struct sched_entity *se = __pick_first_entity(cfs_rq);  // rbtree 最左节点
	struct sched_entity *curr = cfs_rq->curr;
	struct sched_entity *best = NULL;

	/*
	 * We can safely skip eligibility check if there is only one entity
	 * in this cfs_rq, saving some cycles.
	 */
	if (cfs_rq->nr_queued == 1)
		return curr && curr->on_rq ? curr : se;

	/*
	 * Picking the ->next buddy will affect latency but not fairness.
	 */
	if (sched_feat(PICK_BUDDY) && protect &&
	    cfs_rq->next && entity_eligible(cfs_rq, cfs_rq->next)) {
		WARN_ON_ONCE(cfs_rq->next->sched_delayed);
		return cfs_rq->next;
	}

	if (curr && (!curr->on_rq || !entity_eligible(cfs_rq, curr)))
		curr = NULL;

	if (curr && protect && protect_slice(curr))
		return curr;

	/* Pick the leftmost entity if it's eligible */
	if (se && entity_eligible(cfs_rq, se)) {
		best = se;
		goto found;
	}

	/* Heap search for the EEVD entity */
	while (node) {
		struct rb_node *left = node->rb_left;

		/*
		 * Eligible entities in left subtree are always better
		 * choices, since they have earlier deadlines.
		 */
		if (left && vruntime_eligible(cfs_rq,
					__node_2_se(left)->min_vruntime)) {
			node = left;
			continue;
		}

		se = __node_2_se(node);

		/*
		 * The left subtree either is empty or has no eligible
		 * entity, so check the current node since it is the one
		 * with earliest deadline that might be eligible.
		 */
		if (entity_eligible(cfs_rq, se)) {
			best = se;
			break;
		}

		node = node->rb_right;
	}
found:
	if (!best || (curr && entity_before(curr, best)))
		best = curr;

	return best;
}
```

**算法逻辑**：
1. 只有一个任务 → 直接返回
2. 如果 `->next` buddy 是 eligible 的，优先选它（减少延迟）
3. 如果当前任务受 slice 保护且 eligible，继续运行
4. 如果 rbtree 最左节点是 eligible 的，选它
5. 否则在树中搜索：从根往下走，优先左子树（deadline 更早），检查当前节点是否 eligible
6. 当前运行中的任务如果 deadline 更早，优先考虑

### 4.3 实体合格性检查

```
entity_eligible(se):
    se->vruntime <= cfs_rq->zero_vruntime
```

`zero_vruntime` 是队列的"虚拟时间零点"，类似于 CFS 中的 `min_vruntime`。

## 五、Nice 值与权重

### 5.1 优先级定义 (include/linux/sched/prio.h:1-46)

```c
// include/linux/sched/prio.h:5
#define MAX_NICE	19
#define MIN_NICE	-20
#define NICE_WIDTH	(MAX_NICE - MIN_NICE + 1)  // = 40

#define MAX_RT_PRIO		100
#define MAX_PRIO		(MAX_RT_PRIO + NICE_WIDTH)  // = 140
#define DEFAULT_PRIO		(MAX_RT_PRIO + NICE_WIDTH / 2) // = 120

#define NICE_TO_PRIO(nice)	((nice) + DEFAULT_PRIO)    // nice + 120
#define PRIO_TO_NICE(prio)	((prio) - DEFAULT_PRIO)    // prio - 120
```

**优先级映射关系**：

```
nice:       -20  -19  ...   0  ...  19
             │    │         │        │
prio:       100  101  ...  120  ... 139
             │    │         │        │
weight:   88761 71755 ... 1024 ...  15
             │    │         │        │
CPU share:  ~90%  ~70%     ~1%    ~0.01%
```

### 5.2 权重表 (core.c:10569-10578)

```c
// kernel/sched/core.c:10569
const int sched_prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};
```

每降低一个 nice 级别（-1），权重增加约 25%（乘以 ~1.25）。

### 5.3 NICE_0_LOAD 和缩放 (sched.h:148-175)

```c
// kernel/sched/sched.h:148
#ifdef CONFIG_64BIT
# define NICE_0_LOAD_SHIFT	(SCHED_FIXEDPOINT_SHIFT + SCHED_FIXEDPOINT_SHIFT)
# define scale_load(w)		((w) << SCHED_FIXEDPOINT_SHIFT)
#else
# define NICE_0_LOAD_SHIFT	(SCHED_FIXEDPOINT_SHIFT)
# define scale_load(w)		(w)
#endif

#define NICE_0_LOAD		(1L << NICE_0_LOAD_SHIFT)
```

64 位：`NICE_0_LOAD = 1 << 20 = 1048576`, 32 位：`NICE_0_LOAD = 1 << 10 = 1024`

## 六、PELT — Per-Entity Load Tracking

### 6.1 设计目标

PELT (Per-Entity Load Tracking) 为每个 sched_entity 维护一个指数衰减的负载平均值，帮助负载均衡和 CPU 频率调节做出更准确的决策。

**衰减公式**：

```
L = L_0 + L_1 * y + L_2 * y^2 + L_3 * y^3 + ...
```

其中 `y` ≈ 0.5^(1/32ms)，即**半衰期为 32ms**。

### 6.2 struct sched_avg

```c
// sched_entity 末尾 (include/linux/sched.h:620)
struct sched_avg		avg;
```

`sched_avg` 包含：
- `load_avg` — 负载平均值（考虑权重）
- `runnable_avg` — 可运行状态的平均值
- `util_avg` — 利用率平均值（不考虑权重，只考虑 CPU 占用比例）
- `last_update_time` — 上次更新时间戳

**`util_avg`** 常用于 EAS (Energy-Aware Scheduling) 和 CPU 频率选择。

## 七、入队/出队流程

### 7.1 enqueue_task_fair (fair.c:6144)

```
enqueue_task_fair(rq, p, flags):
  └→ for_each_sched_entity(se)         // 遍历调度实体层级
       ├→ enqueue_entity(cfs_rq, se, flags)
       │    ├→ update_curr()           // 更新 vruntime
       │    ├→ account_entity_enqueue() // 更新数量统计
       │    ├→ __enqueue_entity()      // 插入 rbtree
       │    │     └→ rb_add_augmented_cached() // 插入 augmented rbtree
       │    └→ hrtick_update()         // 如果需要设置高精度定时器
       └→ 如果 flags & ENQUEUE_WAKEUP:
             check_preempt_wakeup()    // 检查是否需要抢占
```

### 7.2 dequeue_task_fair (fair.c:7431)

```c
// kernel/sched/fair.c:7431
static bool dequeue_task_fair(struct rq *rq, struct task_struct *p, int flags)
{
	if (task_is_throttled(p)) {
		dequeue_throttled_task(p, flags);
		return true;
	}

	if (!p->se.sched_delayed)
		util_est_dequeue(&rq->cfs, p);

	util_est_update(&rq->cfs, p, flags & DEQUEUE_SLEEP);
	if (dequeue_entities(rq, &p->se, flags) < 0)
		return false;

	return true;
}
```

出队流程：
1. 如果是被限流的任务，特殊处理
2. 更新 PELT 的 util_est
3. 调用 `dequeue_entities` 从 rbtree 中删除

### 7.3 入队后 rbtree 状态示意

```
入队前:
  tasks_timeline:
       (empty)

入队 Task A (nice=0, vruntime=1000):
  tasks_timeline:
       [A: vr=1000, deadline=1700]

入队 Task B (nice=5, vruntime=1500):
  tasks_timeline:
       [A: vr=1000, d=1700]
         \
         [B: vr=1500, d=2200]

入队 Task C (nice=-5, vruntime=800):
  tasks_timeline:                     ← min_vruntime 作为 augmented 信息维护
       [A: vr=1000, d=1700, min=800]
       /                           \
  [C: vr=800, d=1200, min=800]  [B: vr=1500, d=2200, min=1500]
```

pick_eevdf 会选出 eligible 且 deadline 最小的：
- zero_vruntime = 900 时：C(800 ≤ 900) 是 eligible 的，deadline=1200 → 选 C
- zero_vruntime = 600 时：C(800 > 600) 不 eligible → 在树中搜索 → 选第一个 eligible 的

## 八、pick_next_task_fair — 选择下一个任务 (fair.c:9233-92xx)

```c
// kernel/sched/fair.c:9233
struct task_struct *
pick_next_task_fair(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
	__must_hold(__rq_lockp(rq))
{
	struct sched_entity *se;
	struct task_struct *p;
	int new_tasks;

again:
	p = pick_task_fair(rq, rf);       // 实际选取逻辑
	if (!p)
		goto idle;
	se = &p->se;

#ifdef CONFIG_FAIR_GROUP_SCHED
	if (prev->sched_class != &fair_sched_class)
		goto simple;
	// ... 组调度处理 ...
#endif

	put_prev_task(rq, prev);          // 把上一个任务放回队列
	set_next_task_fair(rq, p, true);  // 设置新任务为当前

	return p;

idle:
	// 没有可运行的 fair 任务
	new_tasks = sched_balance_newidle(rq, rf);
	// 尝试空闲负载均衡，可能拉取到新的任务
	if (new_tasks)
		goto again;

	return NULL;
}
```

**pick_task_fair (fair.c:9197)** 的实际逻辑：

```c
// kernel/sched/fair.c:9197
static struct task_struct *pick_task_fair(struct rq *rq, struct rq_flags *rf)
{
	// ...
	cfs_rq = &rq->cfs;
	if (!cfs_rq->nr_queued)
		return NULL;

	do {
		if (cfs_rq->curr && cfs_rq->curr->on_rq)
			update_curr(cfs_rq);

		se = pick_next_entity(rq, cfs_rq, true);  // 内部调用 pick_eevdf()
		if (!se)
			goto again;
		cfs_rq = group_cfs_rq(se);
	} while (cfs_rq);

	p = task_of(se);
	return p;
}
```

## 九、调度参数配置

### 9.1 sysctl 可调参数

| 参数 | 默认值 | 文件/行号 | 说明 |
|------|--------|-----------|------|
| `sysctl_sched_base_slice` | 700000 ns (0.7ms) | fair.c:79 | 基准调度粒度 |
| `sysctl_sched_migration_cost` | 500000 ns (0.5ms) | fair.c:82 | 迁移开销估算 |
| `sysctl_sched_cfs_bandwidth_slice` | 5000 us (5ms) | fair.c:125 | CFS 带宽分配粒度 |
| `sysctl_sched_tunable_scaling` | LOG | fair.c:72 | 可调参数缩放策略 |

### 9.2 sched_tunable_scaling (fair.c:66-72)

```c
// kernel/sched/fair.c:66
/*
 * Options are:
 *
 *   SCHED_TUNABLESCALING_NONE  - unscaled, always *1
 *   SCHED_TUNABLESCALING_LOG   - scaled logarithmically, *1+ilog(ncpus)
 *   SCHED_TUNABLESCALING_LINEAR - scaled linear, *ncpus
 *
 * (default SCHED_TUNABLESCALING_LOG = *(1+ilog(ncpus))
 */
unsigned int sysctl_sched_tunable_scaling = SCHED_TUNABLESCALING_LOG;
```

这些参数在 `update_sysctl()` 中按缩放因子缩放：

```c
// kernel/sched/fair.c:213
static void update_sysctl(void)
{
	unsigned int factor = get_update_sysctl_factor();

	SET_SYSCTL(sched_base_slice);
	// ...
}
```

缩放因子取决于 CPU 数量：
- 4 CPUs: 缩放因子 = 1 + ilog2(4) = 3
- 8 CPUs: 缩放因子 = 1 + ilog2(8) = 4

## 十、组调度概览

CFS 支持 **cgroup** 组调度，将任务分组，然后在组内和组间分别进行公平调度。

```
           root_task_group
          /        |        \
    tg_A           tg_B       tg_C
    /   \           |
  T1   T2          T3

每 CPU 上:
  cfs_rq (root)
     ├── se_for_tg_A  → cfs_rq_of(tg_A)
     │                     ├── se for T1
     │                     └── se for T2
     ├── se_for_tg_B  → cfs_rq_of(tg_B)
     │                     └── se for T3
     └── se_for_tg_C  → cfs_rq_of(tg_C)
```

组调度允许管理员通过 `cpu.shares` 设置每个组的权重比例，实现多租户隔离。

## 十一、总结

```
┌─────────────────────────────────────────────────────────────┐
│                   CFS + EEVDF 调度流程                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  tick / wakeup / yield                                       │
│       │                                                      │
│       ▼                                                      │
│  update_curr(cfs_rq)                                         │
│   ├→ delta_exec = now - exec_start                           │
│   ├→ vruntime += calc_delta_fair(delta_exec, curr)           │
│   └→ update_deadline()  // EEVDF: 检查/更新 deadline         │
│       │                                                      │
│       ▼                                                      │
│  pick_next_task_fair(rq, prev, rf)                           │
│   ├→ pick_task_fair()                                        │
│   │    └→ pick_eevdf(cfs_rq)                                 │
│   │         ├→ 找出 eligible 的实体                           │
│   │         └→ 选择 deadline 最小的                           │
│   ├→ put_prev_task(prev)                                     │
│   └→ set_next_task(task)                                     │
│       │                                                      │
│       ▼                                                      │
│  context_switch() → 切换到新任务                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**核心公式总结**：

| 公式 | 说明 |
|------|------|
| `vruntime += delta_exec * NICE_0_LOAD / weight` | 虚拟时间递进 |
| `deadline = vruntime + slice` | EEVDF 截止时间 |
| `eligible: vruntime <= zero_vruntime` | 合格条件 |
| `weight = sched_prio_to_weight[prio - MAX_RT_PRIO]` | nice → weight 映射 |
| `PELT: L = L_0 + L_1*y + L_2*y^2 + ...` | 负载指数衰减 |

## 下一篇文章

[第二篇：优先级与调度类链 — 从 prio.h 到 sched_class 的完整体系](./02-priority-class.md)
