# 第四篇：负载均衡与调度域 — 多核均衡的艺术

> 源码：`kernel/sched/fair.c` (load balance), `kernel/sched/topology.c` (domain setup) | 头文件：`kernel/sched/sched.h`, `include/linux/sched/topology.h`

> 系列目录：[调度器 内核源码深度解析](./README.md)

## 一、为什么需要负载均衡

在 SMP/NUMA 多核系统中，每个 CPU 都有独立的运行队列 (rq)。如果任务只在单个 CPU 上运行，其他 CPU 就会空闲。负载均衡的目标是：

**让每个 CPU 上的任务负载尽可能均衡，最大化系统吞吐量。**

### 1.1 负载均衡的挑战

```
理想状态:
  CPU0: [A A A]  负载: 3
  CPU1: [B B B]  负载: 3
  CPU2: [C C C]  负载: 3

现实中的不均衡:
  CPU0: [A A A A A A A]  负载: 7
  CPU1: [B]              负载: 1
  CPU2: [空闲]           负载: 0

需要负载均衡!
  迁移 Task A 到 CPU1 和 CPU2
```

### 1.2 负载均衡的数学基础 (fair.c:9403-9459)

```c
// kernel/sched/fair.c:9403
/**************************************************
 * Fair scheduling class load-balancing methods.
 *
 * BASICS
 *
 * The purpose of load-balancing is to achieve the same basic fairness the
 * per-CPU scheduler provides, namely provide a proportional amount of compute
 * time to each task. This is expressed in the following equation:
 *
 *   W_i,n/P_i == W_j,n/P_j for all i,j                               (1)
 *
 * Where W_i,n is the n-th weight average for CPU i. The instantaneous weight
 * W_i,0 is defined as:
 *
 *   W_i,0 = \Sum_j w_i,j                                             (2)
 *
 * Where w_i,j is the weight of the j-th runnable task on CPU i. This weight
 * is derived from the nice value as per sched_prio_to_weight[].
 *
 * The weight average is an exponential decay average of the instantaneous
 * weight:
 *
 *   W'_i,n = (2^n - 1) / 2^n * W_i,n + 1 / 2^n * W_i,0               (3)
 *
 * C_i is the compute capacity of CPU i...
 *
 * To achieve this balance we define a measure of imbalance which follows
 * directly from (1):
 *
 *   imb_i,j = max{ avg(W/C), W_i/C_i } - min{ avg(W/C), W_j/C_j }    (4)
 */
```

**核心思想**：`Weight_i / Capacity_i = Weight_j / Capacity_j`，即每个 CPU 的负载/容量比应该相等。

## 二、调度域 (sched_domain) — 硬件拓扑的抽象

### 2.1 硬件拓扑 → sched_domain 映射

现代 CPU 的拓扑是多层的，sched_domain 对应每一层：

```
实测 AMD EPYC 系统拓扑:

┌──────────────────────────────────────────────────────────────────────┐
│  NUMA Node 0                          NUMA Node 1                    │
│  ┌──────────────┐  ┌──────────────┐   ┌──────────────┐              │
│  │ DIE (CCD)     │  │ DIE (CCD)    │   │ DIE (CCD)    │  ...         │
│  │ ┌───────────┐ │  │ ┌───────────┐│   │              │              │
│  │ │ MC (L3共享)│ │  │ │ MC (L3共享)││   │              │              │
│  │ │ ┌──┐ ┌──┐ │ │  │ │ ┌──┐ ┌──┐ ││   │              │              │
│  │ │ │SMT│ │SMT│ │ │  │ ││SMT│ │SMT│││   │              │              │
│  │ │ └──┘ └──┘ │ │  │ │└──┘ └──┘ ││   │              │              │
│  │ └───────────┘ │  │ └───────────┘│   │              │              │
│  └──────────────┘  └──────────────┘   └──────────────┘              │
│                                                                      │
│  NUMA Node 2                          NUMA Node 3                    │
└──────────────────────────────────────────────────────────────────────┘

对应的 sched_domain 层次:
  SD Level 3: NUMA (跨 NUMA 节点)
  SD Level 2: DIE  (跨 DIE/CCD)
  SD Level 1: MC   (跨 L3 共享域)
  SD Level 0: SMT  (同物理核的超线程)
```

### 2.2 sched_domain 的标志位

| Flag | 含义 | 示例 |
|------|------|------|
| `SD_BALANCE_NEWIDLE` | 新空闲时立即进行负载均衡 | MC/DIE 级 |
| `SD_BALANCE_EXEC` | execve() 后进行负载均衡 | SMT 级 |
| `SD_BALANCE_FORK` | fork() 后进行负载均衡 | SMT 级 |
| `SD_BALANCE_WAKE` | 唤醒时进行负载均衡 | 所有级别 |
| `SD_WAKE_AFFINE` | 唤醒时优先选 cache 亲和 CPU | MC/SMT |
| `SD_SHARE_CPUCAPACITY` | 同组 CPU 共享计算能力 | SMT |
| `SD_SHARE_PKG_RESOURCES` | 同组 CPU 共享 package 资源 | MC |
| `SD_SHARE_POWERDOMAIN` | 同组 CPU 共享电力域 | MC |
| `SD_ASYM_CPUCAPACITY` | 非对称 CPU 容量 (big.LITTLE) | DIE |
| `SD_ASYM_PACKING` | 非对称打包 (优先用高性能核心) | DIE |
| `SD_PREFER_SIBLING` | 优先在同级兄弟 CPU 上运行 | MC/DIE |
| `SD_NUMA` | NUMA 感知 | NUMA |
| `SD_SERIALIZE` | 串行化负载均衡 | NUMA |

### 2.3 sched_group — 调度域内的 CPU 分组

每个 sched_domain 由多个 sched_group 组成。每个 group 代表一组地位等同的 CPU。

```
SD = MC (4 个 L3 共享组):

  sched_group[0]: {CPU0, CPU1}  ← SMT siblings
  sched_group[1]: {CPU2, CPU3}
  sched_group[2]: {CPU4, CPU5}
  sched_group[3]: {CPU6, CPU7}

负载均衡时:
  for_each_cpu_and(cpu, sd_span, cpu_active_mask):
    sg = cpu_rq(cpu)->sd->groups  ← 该 CPU 所在 group 的视角
```

## 三、核心数据结构

### 3.1 struct lb_env (fair.c:9581-9595)

```c
// kernel/sched/fair.c:9581
struct lb_env {
	struct sched_domain	*sd;

	struct rq		*src_rq;        // 源 CPU (最忙)
	int			src_cpu;

	int			dst_cpu;        // 目标 CPU (需要更多负载)
	struct rq		*dst_rq;

	struct cpumask		*dst_grpmask;
	int			new_dst_cpu;    // 备选目标 CPU
	enum cpu_idle_type	idle;           // CPU_{NOT_IDLE, NEWLY_IDLE, IDLE}
	long			imbalance;      // 不均衡量
	struct cpumask		*cpus;
	unsigned int		flags;

	unsigned int		loop;           // 已执行循环数
	unsigned int		loop_break;
	unsigned int		loop_max;       // 最大循环数

	enum fbq_type		fbq_type;
	struct list_head	tasks;          // 要迁移的任务链表
};
```

### 3.2 struct sg_lb_stats (fair.c:10262-10284)

```c
// kernel/sched/fair.c:10262
struct sg_lb_stats {
	unsigned long avg_load;            /* 组内平均负载 */
	unsigned long group_load;          /* 组内总负载 */
	unsigned long group_capacity;      /* 组内总能力 */
	unsigned long group_util;          /* 组内总利用率 */
	unsigned long group_runnable;      /* 组内总可运行时间 */
	unsigned int sum_nr_running;       /* 所有任务数 (含 RT/DL) */
	unsigned int sum_h_nr_running;     /* CFS 任务数 */
	unsigned int idle_cpus;            /* 空闲 CPU 数 */
	unsigned int group_weight;         /* 组内 CPU 数 */
	enum group_type group_type;        /* 组类型 */
	int group_no_capacity;             /* 无能力的 CPU 数 */
	unsigned long group_asym_packing;  /* 非对称打包状态 */
	unsigned long group_misfit_task_load; /* 不合适的任务负载 */
	unsigned long group_has_spare;     /* 有富余能力 */
};
```

### 3.3 struct sd_lb_stats (fair.c:10286-10296)

```c
// kernel/sched/fair.c:10286
struct sd_lb_stats {
	struct sched_group *busiest;       /* 最忙的组 */
	struct sched_group *local;         /* 本地组 */
	unsigned long total_load;          /* 域内总负载 */
	unsigned long total_capacity;      /* 域内总能力 */
	unsigned long avg_load;            /* 域内平均负载 */
	unsigned int prefer_sibling;       /* 是否优先兄弟 CPU */

	struct sg_lb_stats busiest_stat;   /* 最忙组的统计 */
	struct sg_lb_stats local_stat;     /* 本组的统计 */
};
```

## 四、负载均衡触发时机

### 4.1 三个触发时机

```
1. 周期均衡 (scheduler_tick):
   每个 tick → trigger_load_balance() → raise_softirq(SCHED_SOFTIRQ)
   每 1ms 最多触发一次 (在 tick 中设置标志)
   周期性执行 sched_balance_softirq()

2. 新空闲均衡 (CPU_NEWLY_IDLE):
   当 CPU 刚变为空闲时 (没有可运行任务)
   pick_next_task_fair() → newidle_balance()
   快速检查同域内是否有任务可迁移

3. 繁忙均衡 (CPU_NOT_IDLE):
   仅通过周期均衡触发
   检查当前 CPU 是否过载，是否需要从其他 CPU 拉取任务
```

### 4.2 idle 类型

```c
enum cpu_idle_type {
	CPU_NOT_IDLE,      // CPU 繁忙
	CPU_NEWLY_IDLE,    // CPU 刚变为空闲
	CPU_IDLE           // CPU 持续空闲 (NO_HZ)
};
```

## 五、周期性负载均衡：sched_balance_rq (fair.c:12101)

这是最核心的负载均衡函数：

```c
// kernel/sched/fair.c:12101
static int sched_balance_rq(int this_cpu, struct rq *this_rq,
			struct sched_domain *sd, enum cpu_idle_type idle,
			int *continue_balancing)
{
	int ld_moved, cur_ld_moved, active_balance = 0;
	struct sched_domain *sd_parent = sd->parent;
	struct sched_group *group;
	struct rq *busiest;
	struct rq_flags rf;
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(load_balance_mask);
	struct lb_env env = {
		.sd		= sd,
		.dst_cpu	= this_cpu,
		.dst_rq		= this_rq,
		.dst_grpmask    = group_balance_mask(sd->groups),
		.idle		= idle,
		.loop_break	= SCHED_NR_MIGRATE_BREAK,
		.cpus		= cpus,
		.fbq_type	= all,
		.tasks		= LIST_HEAD_INIT(env.tasks),
	};

	cpumask_and(cpus, sched_domain_span(sd), cpu_active_mask);

	schedstat_inc(sd->lb_count[idle]);

redo:
	if (!should_we_balance(&env)) {
		*continue_balancing = 0;
		goto out_balanced;
	}

	// ... SD_SERIALIZE 串行化检查 ...

	group = sched_balance_find_src_group(&env);    // ★ 1. 找最忙的组
	if (!group) {
		schedstat_inc(sd->lb_nobusyg[idle]);
		goto out_balanced;
	}

	busiest = sched_balance_find_src_rq(&env, group); // ★ 2. 找组内最忙的 CPU
	if (!busiest) {
		schedstat_inc(sd->lb_nobusyq[idle]);
		goto out_balanced;
	}

	env.src_cpu = busiest->cpu;
	env.src_rq = busiest;

	ld_moved = 0;
	if (busiest->nr_running > 1) {
		env.loop_max = min(sysctl_sched_nr_migrate, busiest->nr_running);

more_balance:
		rq_lock_irqsave(busiest, &rf);
		update_rq_clock(busiest);

		cur_ld_moved = detach_tasks(&env);      // ★ 3. 拆除任务

		rq_unlock(busiest, &rf);

		if (cur_ld_moved) {
			attach_tasks(&env);              // ★ 4. 附加到目标 CPU
			ld_moved += cur_ld_moved;
		}

		// ... 处理 pin 的任务、重试等 ...
	}
}
```

### 5.1 负载均衡步骤

```
sched_balance_rq 流程:

1. should_we_balance(&env)
   └→ 是否应该由本 CPU 执行均衡? (多 CPU group 中只有一个 CPU 执行)

2. sched_balance_find_src_group(&env)
   └→ 在 sched_domain 的所有 group 中找最忙的

3. sched_balance_find_src_rq(&env, group)
   └→ 在最忙的 group 中找最忙的 CPU

4. detach_tasks(&env)
   └→ 从源 CPU 拆除合适的任务

5. attach_tasks(&env)
   └→ 将任务附加到目标 CPU

6. active_balance (如果普通负载均衡不成功)
   └→ 强制进行活动迁移
```

### 5.2 任务迁移数量限制

```c
env.loop_max = min(sysctl_sched_nr_migrate, busiest->nr_running);
```

每次负载均衡最多迁移 32 个任务（`SCHED_NR_MIGRATE_BREAK`）。

## 六、寻找最忙组：sched_balance_find_src_group

### 6.1 组类型判定

```c
enum group_type {
	group_has_spare,           // 有富余能力
	group_fully_busy,          // 刚好忙
	group_misfit_task,         // 有不适合当前 CPU 的任务
	group_asym_packing,        // 非对称打包不均衡
	group_imbalanced,          // 调度组不均衡
	group_overloaded,          // 过载
};
```

**优先级**：过载 > 不均衡 > misfit > 非对称打包 > 有富余

```
判断哪个组是"最忙":

  for each group:
    update_sg_lb_stats(env, group, &sgs)
    update_sd_pick_busiest(env, sds, group, &sgs)
      └→ 比较 group_type, avg_load, group_util 等

  返回 busiest group
```

### 6.2 判断 group 是否更忙 (fair.c:10800)

```c
// kernel/sched/fair.c:10800
static bool update_sd_pick_busiest(...)
{
	// 先比较 group_type (过载 > 不均衡 > misfit > 富余)
	if (sgs->group_type > busiest->group_type)
		return true;
	if (sgs->group_type < busiest->group_type)
		return false;

	// group_type 相同 → 比较负载
	switch (sgs->group_type) {
	case group_has_spare:
		// 比较平均负载
		if (sgs->avg_load > busiest->avg_load)
			return true;
		break;
	case group_fully_busy:
	case group_overloaded:
		// 比较不均衡量: 负载 * 对方容量 > 对方负载 * 本方容量
		break;
	// ... 其他类型 ...
	}

	return false;
}
```

## 七、寻找最忙 CPU：sched_balance_find_src_rq (fair.c 附近)

根据迁移类型选择最忙的 CPU：

```c
switch (env->migration_type) {
case migrate_load:
	// 比较 CPU 负载: load_i * capacity_j > load_j * capacity_i
	// 优先选负载/容量比最高的

case migrate_util:
	// 比较 CPU 利用率
	busiest_util < cpu_util_cfs_boost(i)

case migrate_task:
	// 比较 nr_running
	busiest_nr < nr_running

case migrate_misfit:
	// misfit 任务的负载
	busiest_load < rq->misfit_task_load
}
```

## 八、detach_tasks — 拆除任务

从最忙 CPU 上拆除最合适的任务。选择标准：

1. **负载检查**：`task_h_load(task) <= imbalance`（不拆除过重的任务）
2. **亲和性检查**：`cpumask_test_cpu(env->dst_cpu, task->cpus_ptr)`（任务允许在目标 CPU 上运行）
3. **cache 热检查**：如果不是刚运行的任务，可能不适合迁移
4. **一次可以拆除多个任务**，直到不均衡被消除

```
detach_tasks 过程:

  for each task in cfs_rq (从 rb_last 开始):
    ├→ 检查 task affinity (cpus_ptr)
    ├→ 检查 task_h_load(task) <= imbalance
    ├→ 检查 task 是否可以被迁移 (nr_cpus_allowed > 1)
    ├→ deactivate_task() → 标记 TASK_ON_RQ_MIGRATING
    ├→ list_add(&p->se.group_node, &env->tasks)
    ├→ imbalance -= task_h_load(task)
    └→ 如果 imbalance <= 0 → break
```

## 九、NOHZ 负载均衡

当 CPU 进入 NO_HZ 空闲状态（关闭 tick），它无法执行周期性负载均衡。内核通过两种机制解决：

### 9.1 NOHZ 状态跟踪 (fair.c:7461-7471)

```c
// kernel/sched/fair.c:7461
static struct {
	cpumask_var_t idle_cpus_mask;
	int has_blocked_load;		/* Idle CPUS has blocked load */
	int needs_update;		/* Newly idle CPUs need their next_balance collated */
	unsigned long next_balance;     /* in jiffy units */
	unsigned long next_blocked;	/* Next update of blocked load in jiffies */
} nohz ____cacheline_aligned;
```

### 9.2 繁忙 CPU 代行均衡

繁忙的 CPU 会检查 NOHZ 空闲 CPU 是否有阻塞负载，并刷新它们的 PELT 统计：

```c
// kernel/sched/fair.c:10108
static bool __update_blocked_others(struct rq *rq, bool *done)
{
	bool updated;

	/*
	 * update_load_avg() can call cpufreq_update_util(). Make sure that RT,
	 * DL and IRQ signals have been updated before updating CFS.
	 */
	updated = update_other_load_avgs(rq);

	if (others_have_blocked(rq))
		*done = false;

	return updated;
}
```

## 十、负载均衡的层次遍历

负载均衡自底向上逐层执行：

```
rebalance_domains(rq, idle):
  │
  for each sched_domain (从底层 SMT 到顶层 NUMA):
    │
    if (!(sd->flags & idle_type_flag))
        continue;                     ← 不是所有域都在所有 idle 类型下均衡
    │
    ├→ should_we_balance(env)?
    │    └→ 由 first_idle_cpu 执行
    │
    ├→ sched_balance_rq(this_cpu, this_rq, sd, idle, &continue_balancing)
    │    ├→ 找最忙组
    │    ├→ 找最忙 CPU
    │    ├→ detach_tasks
    │    └→ attach_tasks
    │
    └→ 如果 continue_balancing = 0 且无其他任务进入
         → 停止更高层的均衡
```

**层次设计**：
- SMT 层均衡：频繁且快速（只涉及共享核）
- MC 层均衡：中等频率（涉及共享 L3）
- DIE 层均衡：低频率（跨 L3 缓存昂贵）
- NUMA 层均衡：最低频率（跨 NUMA 节点最昂贵）

## 十一、非对称 CPU (big.LITTLE) 的负载均衡

### 11.1 CPU capacity 跟踪

```c
// kernel/sched/fair.c:104
#define fits_capacity(cap, max)	((cap) * 1280 < (max) * 1024)

// kernel/sched/fair.c:112
#define capacity_greater(cap1, cap2) ((cap1) * 1024 > (cap2) * 1078)
```

`fits_capacity`: 当 capacity 低于 max 的 80% 时，认为任务"不适合"（misfit）。

### 11.2 misfit 任务迁移

当一个小核心 (capacity=256) 上的任务利用率超过其容量时，它会被标记为 misfit：

```
big.LITTLE 系统:
  大核: capacity=1024
  小核: capacity=256

  小核上的任务 util=300 → misfit! → 迁移到大核
  大核上的任务 util=100 → 浪费 → 迁移到小核 (EAS)
```

## 十二、CPU 负载计算 (cpu_load, fair.c:7473)

```c
// kernel/sched/fair.c:7473
static unsigned long cpu_load(struct rq *rq)
{
	return cfs_rq_load_avg(&rq->cfs);
}
```

`cfs_rq_load_avg` 返回 CFS 运行队列的 PELT 平均负载，考虑了任务权重和衰减。

**CPU 容量计算 (scale_rt_capacity, fair.c:10319)**：

```c
// kernel/sched/fair.c:10319
static unsigned long scale_rt_capacity(int cpu)
{
	unsigned long max = get_actual_cpu_capacity(cpu);
	unsigned long used, free;
	unsigned long irq;

	// 减去 IRQ 使用的容量
	// 减去 RT/DL 使用的容量
	// CFS 可用的 = max - IRQ - RT - DL
}
```

## 十三、综合流程

```
tick 中断 (每 1/HZ 秒)
  │
  ├→ scheduler_tick()
  │    ├→ update_rq_clock()      // 更新时间戳
  │    ├→ curr->sched_class->task_tick()  // 类相关处理
  │    │    └→ fair: update_curr()         // 递进 vruntime
  │    └→ trigger_load_balance(rq)        // 触发负载均衡
  │         └→ raise_softirq(SCHED_SOFTIRQ)
  │
  ▼
SCHED_SOFTIRQ
  │
  └→ sched_balance_softirq()
       │
       if (!idle_cpu(cpu))
           rebalance_domains(rq, CPU_NOT_IDLE)
       │   │
       │   └→ for each sd (SMT → MC → DIE → NUMA):
       │        ├→ should_we_balance()
       │        └→ sched_balance_rq()
       │             ├→ sched_balance_find_src_group()
       │             ├→ sched_balance_find_src_rq()
       │             ├→ detach_tasks()
       │             └→ attach_tasks()
       │
       if (idle_cpu(cpu))
           nohz.next_balance = ...
           (由其他繁忙 CPU 帮助均衡)
```

**pick_next_task_fair 中的 newidle balance**：

```
pick_next_task_fair()
  │
  if (!p)
      goto idle
        │
        └→ new_tasks = sched_balance_newidle(rq, rf)
             │
             └→ newidle_balance(rq, rf)
                  │
                  for_each_domain(this_cpu, sd):
                    ├→ if (need_idle_balance(sd))
                    │     sd->balance(rq, CPU_NEWLY_IDLE)
                    │
                    └→ if (rq->nr_running)
                          break;  ← 有任务进入, 停止
```

## 十四、总结

```
┌──────────────────────────────────────────────────────────────────┐
│                       负载均衡全景                                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  触发时机:                                                        │
│    ├── 周期性 (tick)  → sched_balance_softirq()                   │
│    ├── 新空闲 (newidle) → newidle_balance()                       │
│    └── NOHZ           → nohz_balancer_kick()                     │
│                                                                  │
│  核心函数:                                                        │
│    ├── sched_balance_rq()        : 主入口                         │
│    ├── sched_balance_find_src_group() : 找最忙组                   │
│    ├── sched_balance_find_src_rq()    : 找组内最忙 CPU            │
│    ├── detach_tasks()            : 拆除任务                       │
│    └── attach_tasks()            : 附加任务                       │
│                                                                  │
│  数据结构:                                                        │
│    ├── sched_domain              : 硬件拓扑抽象 (SMT/MC/DIE/NUMA)  │
│    ├── sched_group               : domain 内的 CPU 分组            │
│    ├── lb_env                    : 负载均衡环境                    │
│    ├── sg_lb_stats               : 组统计                         │
│    └── sd_lb_stats               : 域统计                         │
│                                                                  │
│  负载衡量:                                                        │
│    ├── load_avg (PELT)           : 考虑权重的负载                  │
│    ├── util_avg (PELT)           : 不考虑权重的利用率              │
│    ├── nr_running                : 简单任务数                     │
│    └── cpu_capacity              : CPU 计算能力 (大核/小核)        │
│                                                                  │
│  特殊场景:                                                        │
│    ├── big.LITTLE (ASYM_CPUCAPACITY) : misfit 迁移                │
│    ├── NOHZ                      : 关闭 tick 依旧均衡              │
│    ├── SD_SERIALIZE              : 大 NUMA 系统串行化均衡          │
│    └── NUMA balancing            : NUMA 感知的页面迁移             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

## 下一篇文章

[第五篇：sched_ext — BPF 可扩展调度类](./05-sched-ext.md)
