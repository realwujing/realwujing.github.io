# 第五篇：sched_ext — BPF 可扩展调度器

> 源码：kernel/sched/ext.c + kernel/sched/sched.h | 头文件：include/linux/sched/ext.h

系列目录：[调度器 内核源码深度解析](./README.md)

---

## 1. 什么是 sched_ext

sched_ext（SCX）是 Linux 6.12 合并的**BPF 可扩展调度类**，允许用户通过 BPF 程序**完全替换 CFS 调度策略**，实现自定义的 CPU 调度算法。它不是一个独立的调度策略，而是一个**调度框架**：BPF 程序通过实现 `struct sched_ext_ops` 回调接管核心调度决策。

```
  ┌────────────────────────────────────────────────────┐
  │                  sched_class 优先级链                │
  │                                                      │
  │  stop_sched_class     ← 最高（per-CPU stopper）      │
  │       ↑                                              │
  │  dl_sched_class       ← Deadline 实时               │
  │       ↑                                              │
  │  rt_sched_class       ← RT 实时                     │
  │       ↑                                              │
  │  ext_sched_class      ← ★ sched_ext (BPF)           │
  │       ↑   当 scx_switched_all() 时替换 fair          │
  │  fair_sched_class     ← CFS/EEVDF                    │
  │       ↑                                              │
  │  idle_sched_class     ← 最低（idle 任务）            │
  └────────────────────────────────────────────────────┘
```

核心文件 `kernel/sched/ext.c` 长达 9986 行，实现了一个完整的调度类：

```c
// kernel/sched/ext.c:1
/*
 * BPF extensible scheduler class: Documentation/scheduler/sched-ext.rst
 *
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
```

### 1.1 为什么需要 sched_ext？

传统内核调度器（CFS）是通用设计，难以满足所有场景：

| 场景 | CFS 局限 | sched_ext 方案 |
|------|----------|----------------|
| 游戏（反作弊） | 无法区分作弊进程 | 自定义优先级，限制特定进程 CPU |
| Meta 数据中心 | 需要应用感知调度 | 按服务优先级排队 |
| 延迟敏感应用 | CFS 时间片固定 | 自定义抢占策略 |
| 研究实验 | 修改内核成本高 | BPF 动态加载，无需重启 |
| 特定拓扑优化 | 通用 NUMA 均衡 | BPF 可感知硬件拓扑定制 |

---

## 2. sched_ext 在调度类层次中的位置

### 2.1 ext_sched_class 声明

```c
// kernel/sched/sched.h:1840
#ifdef CONFIG_SCHED_CLASS_EXT
extern const struct sched_class ext_sched_class;
```

```c
// kernel/sched/sched.h:1842-1846
DECLARE_STATIC_KEY_FALSE(__scx_enabled);     /* SCX BPF scheduler loaded */
DECLARE_STATIC_KEY_FALSE(__scx_switched_all); /* all fair class tasks on SCX */

#define scx_enabled()       static_branch_unlikely(&__scx_enabled)
#define scx_switched_all()  static_branch_unlikely(&__scx_switched_all)
```

两个关键状态：
- `scx_enabled()`：有 BPF 调度器已加载（可能只部分任务在 SCX 上）
- `scx_switched_all()`：**所有** CFS 任务已切换到 SCX，此时 CFS 被完全绕过

### 2.2 next_active_class — 动态跳过类

```c
// kernel/sched/sched.h:2747-2757
static inline const struct sched_class *next_active_class(const struct sched_class *class)
{
    class++;
#ifdef CONFIG_SCHED_CLASS_EXT
    if (scx_switched_all() && class == &fair_sched_class)
        class++;                    // SCX 接管全部 → 跳过 CFS
    if (!scx_enabled() && class == &ext_sched_class)
        class++;                    // SCX 未加载 → 跳过 SCX
#endif
    return class;
}
```

### 2.3 for_active_class_range

```c
// kernel/sched/sched.h:2765-2769
#define for_active_class_range(class, _from, _to)               \
    for (class = (_from); class != (_to); class = next_active_class(class))

#define for_each_active_class(class)                            \
    for_active_class_range(class, __sched_class_highest, __sched_class_lowest)
```

`for_each_active_class` 遍历所有**活跃**调度类，根据 SCX 状态决定是否包含 `ext_sched_class` 或 `fair_sched_class`。

### 2.4 sched_class_above

```c
// kernel/sched/sched.h:2771
#define sched_class_above(_a, _b)    ((_a) < (_b))
```

因为类在数组中按优先级排列（高优先级的指针值较小），简单比较指针即可。

RT 和 DL 类**始终**在 SCX 之上执行。SCX 不能覆盖 `stop_sched_class`（per-CPU stopper）：

```c
// kernel/sched/core.c:8885-8886
BUG_ON(!sched_class_above(&fair_sched_class, &ext_sched_class));
BUG_ON(!sched_class_above(&ext_sched_class, &idle_sched_class));
```

### 2.5 类间切换

当任务策略设为 `SCHED_EXT` 时，直接切换类：

```c
// kernel/sched/core.c:4737
p->sched_class = &ext_sched_class;
```

当内核 `pick_next_task` 时，检测到 `ext_sched_class`：

```c
// kernel/sched/ext.c:3035
if (sched_class_above(&ext_sched_class, next_class))
    // 有更高优先级任务要运行，SCX 让步
```

---

## 3. sched_ext_ops — BPF 到内核的契约

### 3.1 完整的 ops 回调集

`sched_ext_ops` 结构体定义了 BPF 程序必须或可选实现的回调，在 `kernel/sched/ext.c:7731-7771` 注册为 `struct_ops`：

```c
// kernel/sched/ext.c:7731
static struct sched_ext_ops __bpf_ops_sched_ext_ops = {
    .select_cpu         = sched_ext_ops__select_cpu,    // ★ 选择目标 CPU
    .enqueue            = sched_ext_ops__enqueue,       // ★ 任务入队
    .dequeue            = sched_ext_ops__dequeue,       // 任务出队
    .dispatch           = sched_ext_ops__dispatch,      // ★ 分发任务
    .tick               = sched_ext_ops__tick,          // 周期性 tick
    .runnable           = sched_ext_ops__runnable,      // 任务变为可运行
    .running            = sched_ext_ops__running,       // 任务开始运行
    .stopping           = sched_ext_ops__stopping,      // 任务停止运行
    .quiescent          = sched_ext_ops__quiescent,     // CPU 空闲
    .yield              = sched_ext_ops__yield,         // 自愿让出 CPU
    .core_sched_before  = sched_ext_ops__core_sched_before, // Core scheduling
    .set_weight         = sched_ext_ops__set_weight,    // 设置权重
    .set_cpumask        = sched_ext_ops__set_cpumask,   // 设置 CPU 亲和性
    .update_idle        = sched_ext_ops__update_idle,   // 更新 idle 状态
    .cpu_acquire        = sched_ext_ops__cpu_acquire,   // CPU 被 SCX 获取
    .cpu_release        = sched_ext_ops__cpu_release,   // CPU 被 SCX 释放
    .init_task          = sched_ext_ops__init_task,     // 任务初始化
    .exit_task          = sched_ext_ops__exit_task,     // 任务退出
    .enable             = sched_ext_ops__enable,        // SCX 被启用
    .disable            = sched_ext_ops__disable,       // SCX 被禁用
    // ... cgroup 相关、sub_sched 相关省略
    .init               = sched_ext_ops__init,
    .exit               = sched_ext_ops__exit,
    .dump               = sched_ext_ops__dump,
    .dump_cpu           = sched_ext_ops__dump_cpu,
    .dump_task          = sched_ext_ops__dump_task,
};
```

### 3.2 bpf_struct_ops 注册

```c
// kernel/sched/ext.c:7773
static struct bpf_struct_ops bpf_sched_ext_ops = {
    .verifier_ops = &bpf_scx_verifier_ops,
    .reg          = bpf_scx_reg,
    .unreg        = bpf_scx_unreg,
    .check_member = bpf_scx_check_member,
    .init_member  = bpf_scx_init_member,
    .init         = bpf_scx_init,
    .update       = bpf_scx_update,
    .validate     = bpf_scx_validate,
    .name         = "sched_ext_ops",
    .owner        = THIS_MODULE,
    .cfi_stubs    = &__bpf_ops_sched_ext_ops
};
```

### 3.3 三大核心回调

SCX 调度器必须实现的关键回调：

| 回调 | 触发时机 | 职责 |
|------|----------|------|
| `select_cpu(p, prev_cpu, wake_flags)` | 任务被唤醒 | 返回应在此运行的 CPU |
| `enqueue(p, enq_flags)` | 任务进入 SCX runqueue | 将任务加入调度队列 |
| `dispatch(cpu, prev)` | CPU 需要下一个任务 | 将任务从调度队列分发到 CPU 的本地 DSQ |

此外：
- `running(p)`：任务开始运行时通知 BPF
- `stopping(p, runnable)`：任务停止运行时通知 BPF
- `tick(p)`：周期性 tick，BPF 可据此调整 slice 或触发抢占
- `cpu_acquire(cpu)` / `cpu_release(cpu)`：CPU 被 SCX 控制/释放时通知

### 3.4 DEFINE_SCHED_CLASS(ext)

```c
// kernel/sched/ext.c:4544
DEFINE_SCHED_CLASS(ext) = {
    .enqueue_task       = enqueue_task_scx,
    .dequeue_task       = dequeue_task_scx,
    .yield_task         = yield_task_scx,
    .yield_to_task      = yield_to_task_scx,
    .wakeup_preempt     = wakeup_preempt_scx,
    .pick_task          = pick_task_scx,
    .put_prev_task      = put_prev_task_scx,
    .set_next_task      = set_next_task_scx,
    .select_task_rq     = select_task_rq_scx,
    .task_woken         = task_woken_scx,
    .set_cpus_allowed   = set_cpus_allowed_scx,
    .rq_online          = rq_online_scx,
    .rq_offline         = rq_offline_scx,
    .task_tick          = task_tick_scx,
    .switching_to       = switching_to_scx,
    .switched_from      = switched_from_scx,
    .switched_to        = switched_to_scx,
    .reweight_task      = reweight_task_scx,
    .prio_changed       = prio_changed_scx,
    .update_curr        = update_curr_scx,
#ifdef CONFIG_UCLAMP_TASK
    .uclamp_enabled     = 1,
#endif
};
```

这是一个标准的 `sched_class` 实现，通过宏嵌入类链中。

---

## 4. Dispatch Queue（DSQ）

### 4.1 DSQ 概念

DSQ（Dispatch Queue）是 sched_ext 的核心数据结构，用于管理任务的排队和分发：

```c
// include/linux/sched/ext.h:81
struct scx_dispatch_q {
    raw_spinlock_t              lock;
    struct task_struct __rcu    *first_task;   /* lockless peek at head */
    struct list_head            list;           /* FIFO 任务链表 */
    struct rb_root              priq;           /* vtime 优先级红黑树 */
    u32                         nr;
    u32                         seq;
    u64                         id;
    struct rhash_head           hash_node;
    struct llist_node           free_node;
    struct scx_sched            *sched;
    struct scx_dsq_pcpu __percpu *pcpu;       /* per-CPU 状态 */
    struct rcu_head             rcu;
};
```

### 4.2 DSQ 类型

| DSQ 类型 | ID 前缀 | 描述 |
|----------|---------|------|
| 本地 DSQ | `SCX_DSQ_LOCAL` | 每个 CPU 一个私有的 FIFO 队列 |
| 全局 DSQ | `SCX_DSQ_GLOBAL` | 所有 CPU 共享的 FIFO 队列 |
| 用户 DSQ | BPF 创建 | 自定义的 FIFO 或 vtime 优先队列（带红黑树） |

任务的排队顺序：
- **FIFO**：通过 `list` 链表，按插入顺序出队
- **vtime**：通过 `priq` 红黑树，按 `p->scx.dsq_vtime` 值排序

### 4.3 scx_entity — 任务的 SCX 状态

```c
// include/linux/sched/ext.h:170
struct sched_ext_entity {
    // ... 
    u64         ddsp_dsq_id;        // 直接分发的目标 DSQ ID
    u64         ddsp_enq_flags;     // 直接分发的入队标志
    struct scx_dsq_list_node dsq_list; // 在 DSQ 链表中的节点
    struct rb_node          dsq_priq;  // 在 DSQ 红黑树中的节点
    u32                     dsq_seq;
    u32                     dsq_flags;
    u32                     flags;
    u32                     weight;
    s32                     sticky_cpu;   // 粘性 CPU
    s32                     holding_cpu;  // 当前持有 CPU
    s32                     selected_cpu; // 选择的 CPU
    
    struct list_head        runnable_node; // 在 rq->scx.runnable_list 中
    
    u64         slice;          /* BPF 可控: 时间片预算(ns) */
    u64         dsq_vtime;      /* BPF 可控: vtime 排序值 */
    bool        disallow;       /* 拒绝切换到 SCX */
};
```

### 4.4 dispatch 两阶段模型

sched_ext 将 dispatch 分为两阶段（`ext.c:2672-2674` 注释）：

```
  阶段 1: scx_bpf_dsq_insert()          ← 在 ops.dispatch() 中调用
           └─ 记录任务和 qseq 到 dsp_ctx
  
  阶段 2: finish_dispatch()             ← ops.dispatch() 返回后
           └─ 批量将记录的任务真正插入 DSQ
```

这样避免了在 `ops.dispatch()` 持锁时操作 DSQ 的锁序反转问题。

---

## 5. 核心 kfunc：BPF 可调用的内核函数

sched_ext 暴露了一套 kfunc 供 BPF 程序调用。

### 5.1 scx_bpf_dsq_insert — 插入任务到 DSQ

```c
// kernel/sched/ext.c:8218
__bpf_kfunc void scx_bpf_dsq_insert(struct task_struct *p, u64 dsq_id,
                                     u64 slice, u64 enq_flags, u64 *aux)
```

```c
// kernel/sched/ext.c:8191
__bpf_kfunc bool scx_bpf_dsq_insert___v2(struct task_struct *p, u64 dsq_id,
                                          u64 slice, u64 enq_flags, u64 *aux)
```

**调用上下文**（`ext.c:8160-8177`）：

| 调用位置 | 行为 |
|----------|------|
| `ops.select_cpu()` | **直接分发**：任务绕过 DSQ，直接到目标 CPU |
| `ops.enqueue()` | **直接分发**：同上，`@p` 入列到 `selected_cpu` 的本地 DSQ |
| `ops.dispatch()` | **间接分发**：放入 `SCX_DSQ_LOCAL`，等待被 pick；或放入其他 DSQ |

`v2` 版本新增了 `enq_flags` 参数支持，`v1` 兼容调用。

### 5.2 scx_bpf_dsq_insert_vtime — vtime 排序插入

```c
// kernel/sched/ext.c:8302
__bpf_kfunc void scx_bpf_dsq_insert_vtime(struct task_struct *p, u64 dsq_id,
                                           u64 slice, u64 vtime, u64 enq_flags,
                                           u64 *aux)
```

通过 `__scx_bpf_dsq_insert_vtime`（`ext.c:8283`）实现，将任务加入 DSQ 的 `priq` 红黑树，按 `vtime` 值排序（类似 CFS 的 virtual runtime）。

### 5.3 BTF ID 注册

```c
// kernel/sched/ext.c:8330-8333
BTF_ID_FLAGS(func, scx_bpf_dsq_insert, KF_IMPLICIT_ARGS | KF_RCU)
BTF_ID_FLAGS(func, scx_bpf_dsq_insert___v2, KF_IMPLICIT_ARGS | KF_RCU)
BTF_ID_FLAGS(func, __scx_bpf_dsq_insert_vtime, KF_IMPLICIT_ARGS | KF_RCU)
BTF_ID_FLAGS(func, scx_bpf_dsq_insert_vtime, KF_RCU)
```

### 5.4 其他关键 kfunc

| kfunc | 行号 | 功能 |
|-------|------|------|
| `scx_bpf_select_cpu_dfl` | `ext.c:8822` | 使用内核默认算法选择 CPU |
| `scx_bpf_select_cpu_and` | `ext.c:8820` | 约束 CPU 选择到指定掩码 |
| `scx_bpf_dsq_move_to` | `ext.c:8379` | 在 `dispatch()` 中将所有任务从一个 DSQ 移到另一个 |
| `scx_bpf_dsq_nr_queued` | `ext.c:8444` | 查询 DSQ 中排队的任务数 |
| `scx_bpf_consume` | `ext.c:8464` | 从 DSQ 中消费一个任务 |
| `scx_bpf_kick_cpu` | `ext.c:8613` | 唤醒指定 CPU 触发调度 |

---

## 6. scx_enabled() 与 scx_switched_all()

### 6.1 两个关键状态

```c
// kernel/sched/ext.c:52,57-58
DEFINE_STATIC_KEY_FALSE(__scx_enabled);        // SCX BPF 调度器已加载
DEFINE_STATIC_KEY_FALSE(__scx_switched_all);   // 所有 CFS 任务已切换到 SCX
```

- `scx_enabled()`：至少有一个 SCX 调度器实例在运行
- `scx_switched_all()`：**所有**可运行任务都在 SCX 调度下（CFS 完全闲置）

### 6.2 状态的影响

| 状态 | pick_next_task 行为 | 负载均衡 | CFS tick |
|------|---------------------|----------|----------|
| 都 false | 正常，SCX 类被跳过 | 正常 | 正常 |
| enabled, !switched_all | SCX 和 CFS 共存 | 正常（CFS 任务仍需均衡） | 各管各的 |
| 都 true | CFS 被完全绕过 | 不执行 CFS 均衡 | CFS tick 无操作 |

### 6.3 CPU acquire/release

当 `scx_switched_all()` 后，CFS 放弃对 CPU 的控制：

```c
// kernel/sched/ext.c:3047-3056
if (!rq->scx.cpu_released) {
    // ... CPU 尚未被释放，SCX 尝试释放
    rq->scx.cpu_released = true;
}
```

`cpu_release` 回调通知 BPF 调度器该 CPU 不再被内核管理，此时 BPF 可通过 kfunc 激活另一个 CPU。

---

## 7. 调度执行流程

### 7.1 pick_task_scx 核心路径

当 CPU 需要选择下一个任务运行时：

```
  __schedule()
    └─ pick_next_task(rq)
         └─ for_each_active_class(class):
              └─ class->pick_task(rq)  或 class->pick_next_task(rq)
                   └─ pick_task_scx(rq)
```

`pick_task_scx` 流程：

```
  pick_task_scx(rq)
    │
    ├─ 检查 rq->scx.local_dsq (本地 DSQ)
    │   └─ 有任务 → 直接返回
    │
    ├─ 调用 ops.dispatch(cpu, prev)     ← BPF 决定放什么任务到本地
    │   └─ finish_dispatch()
    │
    ├─ 再次检查 local_dsq
    │   └─ 有任务 → 返回
    │
    ├─ 遍历 global DSQ → 尝试偷任务
    │
    └─ 无可运行任务 → 返回 idle
```

### 7.2 select_task_rq_scx — 唤醒选核

```c
// kernel/sched/ext.c:3343
if (ops_cpu_valid(sch, cpu, "from ops.select_cpu()"))
```

当任务被唤醒时的流程：

```
  try_to_wake_up(p)
    └─ select_task_rq(p, prev_cpu, wake_flags)
         └─ ext_sched_class.select_task_rq
              └─ 调用 ops.select_cpu(p, prev_cpu, wake_flags)  ← BPF 决策
                   │
                   ├─ BPF 返回 CPU → 使用该 CPU
                   │
                   └─ BPF 返回 -1 → SCX 遍历 idle CPUs 选择
```

### 7.3 enqueue_task_scx — 任务入队

```c
// kernel/sched/ext.c:6785-6788
/*
 * ops.enqueue() callback isn't implemented.
 */
scx_error(sch, "SCX_OPS_ENQ_LAST requires ops.enqueue() to be implemented");
```

流程：
1. `ops.enqueue(p, enq_flags)` 被调用
2. BPF 决定将任务放入哪个 DSQ（通过 `scx_bpf_dsq_insert`）
3. 如果 BPF 未做决定（未调度），SCX 将任务加入全局 DSQ

### 7.4 完整生命周期图示

```
    任务创建                        fork/clone
       │                               │
       ▼                               ▼
  ops.init_task()         ops.cgroup_init() (可选)
       │
       ▼
  ops.enable()   ←  调度器被加载
       │
       ├──────────────────────────────────────────────┐
       │              调度循环                         │
       │                                              │
       │  唤醒: ops.select_cpu() → CPU 选择           │
       │  入队: ops.enqueue()    → DSQ 排队           │
       │  Tick: ops.tick()       → 时间片检查          │
       │  分发: ops.dispatch()   → DSQ→local dsq      │
       │  运行: ops.running()    → 任务开始执行        │
       │  抢占: ops.stopping()   → 任务被抢占          │
       │  Yield: ops.yield()     → 自愿让出            │
       │                                              │
       └──────────────────────────────────────────────┘
       │
       ▼
  ops.disable()  ←  调度器被卸载
       │
       ▼
  ops.exit_task()  ← 任务退出/切换到其他类
```

---

## 8. 降级模式（Bypass Mode）

当 BPF 调度器挂起或出错时，sched_ext 自动进入**降级模式**：

```c
// kernel/sched/ext.c:5428-5433 注释
/*
 * - ops.select_cpu() is ignored and the default select_cpu() is used.
 * - ops.enqueue() is ignored and tasks are queued in simple global FIFO order.
 * - ops.dispatch() is ignored.
 */
```

降级模式下，所有 SCX 任务使用简单的全局 FIFO 排队，保证系统可用性。

触发降级的条件包括：
- BPF 程序运行超时（watchdog）
- `ops.dispatch()` 未产生任何任务（CPU 饥饿）
- BPF 程序调用 `scx_bpf_error()` 报告错误
- SysRq-S 手动触发（`sysrq_handle_sched_ext_reset`, `ext.c:7792`）

---

## 9. 基本 BPF 调度器示例

一个最小的 sched_ext 调度器结构：

```c
// my_scheduler.bpf.c
#include <scx/common.bpf.h>

char _license[] SEC("license") = "GPL";

// 简单的全局 FIFO 调度器
s32 BPF_STRUCT_OPS(my_select_cpu, struct task_struct *p,
                   s32 prev_cpu, u64 wake_flags)
{
    return prev_cpu;  // 保持在唤醒 CPU 上运行
}

void BPF_STRUCT_OPS(my_enqueue, struct task_struct *p, u64 enq_flags)
{
    // 将所有任务放入全局 FIFO
    scx_bpf_dsq_insert(p, SCX_DSQ_GLOBAL, SCX_SLICE_DFL, 0, NULL);
}

void BPF_STRUCT_OPS(my_dispatch, s32 cpu, struct task_struct *prev)
{
    // 从全局 DSQ 中取一个任务分发到当前 CPU 的本地 DSQ
    scx_bpf_consume(SCX_DSQ_GLOBAL);
}

void BPF_STRUCT_OPS(my_running, struct task_struct *p)
{
    // 可选：记录任务开始运行
}

void BPF_STRUCT_OPS(my_stopping, struct task_struct *p, bool runnable)
{
    if (runnable)
        // 任务被抢占但仍有工作 → 重新入队
        scx_bpf_dsq_insert(p, SCX_DSQ_GLOBAL, SCX_SLICE_DFL, 0, NULL);
}

SEC(".struct_ops")
struct sched_ext_ops my_ops = {
    .select_cpu     = my_select_cpu,
    .enqueue        = my_enqueue,
    .dispatch       = my_dispatch,
    .running        = my_running,
    .stopping       = my_stopping,
    .name           = "my_simple_fifo",
};
```

编译和加载：
```bash
# 使用 bpftool 或 scx_loader 加载
sudo bpftool struct_ops register my_scheduler.bpf.o /sys/fs/bpf/my_scheduler
```

卸载：
```bash
sudo bpftool struct_ops unregister /sys/fs/bpf/my_scheduler
```

---

## 10. 安全与限制

### 10.1 Watchdog 机制

sched_ext 有内置的 watchdog 防止 BPF 调度器死锁或无限循环：

```c
// kernel/sched/ext.c:85
static unsigned long scx_watchdog_interval;

// kernel/sched/ext.c:93
static unsigned long scx_watchdog_timestamp = INITIAL_JIFFIES;
```

如果 BPF 调度器未在 `watchdog_timeout` 内完成调度，内核会：
1. 打印错误信息
2. 自动禁用 SCX 调度器
3. 触发 `SCX_EXIT_ERROR` 退出

### 10.2 调度延迟限制

- `ops.dispatch()` 有最大批量限制（`dispatch_max_batch`）
- `scx_bpf_dsq_insert` 在 `ops.dispatch()` 中有调用次数上限
- `CPU_NEWLY_IDLE` 场景下限制更严格

### 10.3 权限要求

加载 sched_ext 调度器需要 `CAP_SYS_ADMIN`（或等价权限）。

---

## 11. SysRq 支持

sched_ext 注册了两个 SysRq 处理函数：

```c
// kernel/sched/ext.c:7805
static const struct sysrq_key_op sysrq_sched_ext_reset_op = {
    .handler    = sysrq_handle_sched_ext_reset,
    .help_msg   = "reset-sched-ext(S)",
    .action_msg = "Disable sched_ext and revert all tasks to CFS",
    .enable_mask = SYSRQ_ENABLE_RTNICE,
};
```

- `SysRq+S`：立即禁用 sched_ext，所有任务回退到 CFS
- `SysRq+D`：触发 sched_ext 的调试 dump

---

## 12. 实际应用场景

### 12.1 Meta 内部调度器

Meta 在几十万台服务器上使用 sched_ext，用自定义 BPF 调度器实现：
- **延迟敏感 vs 批处理分离**：不同类型任务使用独立排队策略
- **NUMA 感知**：BPF 程序根据服务拓扑优化 CPU 选择
- **功耗优化**：将低优先级任务集中在少数 CPU 上

### 12.2 游戏反作弊

- 动态检测可疑进程
- 将可疑进程限制在特定 CPU 上
- 注入随机延迟扰乱作弊行为

### 12.3 学术研究

- 快速原型新型调度算法（无需重编译内核）
- 对比不同调度策略的性能（动态切换 BPF 程序）
- 在真实负载上实验论文算法

---

## 13. 源代码关键位置汇总

| 内容 | 文件 | 行号 |
|------|------|------|
| 主入口注释 | `kernel/sched/ext.c` | `1-8` |
| `__scx_enabled` / `__scx_switched_all` | `kernel/sched/ext.c` | `52,58` |
| 全局状态变量 | `kernel/sched/ext.c` | `50-93` |
| `scx_enabled()` / `scx_switched_all()` | `kernel/sched/sched.h` | `1845-1846` |
| `ext_sched_class` 声明 | `kernel/sched/sched.h` | `1840` |
| `next_active_class()` | `kernel/sched/sched.h` | `2747` |
| `for_active_class_range` | `kernel/sched/sched.h` | `2765` |
| `sched_class_above` | `kernel/sched/sched.h` | `2771` |
| `DEFINE_SCHED_CLASS(ext)` | `kernel/sched/ext.c` | `4544` |
| `struct sched_ext_ops` 模板 | `kernel/sched/ext.c` | `7731` |
| `bpf_sched_ext_ops` 注册 | `kernel/sched/ext.c` | `7773` |
| `sched_ext_entity` | `include/linux/sched/ext.h` | `170` |
| `scx_dispatch_q` (DSQ) | `include/linux/sched/ext.h` | `81` |
| `scx_bpf_dsq_insert` | `kernel/sched/ext.c` | `8218` |
| `scx_bpf_dsq_insert___v2` | `kernel/sched/ext.c` | `8191` |
| `scx_bpf_dsq_insert_vtime` | `kernel/sched/ext.c` | `8302` |
| BTF ID 声明 | `kernel/sched/ext.c` | `8330-8333` |
| `p->sched_class = &ext_sched_class` | `kernel/sched/core.c` | `4737` |
| 类顺序 BUG_ON 断言 | `kernel/sched/core.c` | `8885-8886` |
| `can_skip_idle_kick` | `kernel/sched/ext.c` | `7828` |
| SysRq reset | `kernel/sched/ext.c` | `7792` |

---

## 14. 系列结语

本系列共 5 篇文章，覆盖了 Linux 内核调度器的完整技术栈：

| 篇 | 主题 | 核心文件 |
|----|------|----------|
| **第一篇** | CFS 与 EEVDF — `vruntime`、红黑树与公平调度 | `fair.c:717-740`, `fair.c:442`, `fair.c:3826` |
| **第二篇** | 调度类优先级链 — `sched_class`、`pick_next_task` | `sched.h:1915-2118`, `sched.h:1810`, `core.c:5744` |
| **第三篇** | RT 与 Deadline 实时调度 — FIFO/RR/GEDF | `rt.c:1575`, `dl.c:1742`, `cpudeadline.c` |
| **第四篇** | 负载均衡与调度域 — SD 标志、`load_balance` 与 NUMA | `fair.c:9904-12101`, `topology.c:1803`, `sd_flags.h` |
| **第五篇** | sched_ext — BPF 可扩展调度器 | `ext.c`, `ext.h`, `sched.h:2747-2771` |

从 `fair_sched_class` 的 vruntime 公平调度，到五级 `sched_class` 优先级链（stop → dl → rt → ext → fair → idle），再到 RT/Deadline 的实时保证，再到多核负载均衡的调度域层次与 PELT 负载追踪，最后到 sched_ext 的 BPF 可扩展框架——Linux 调度器是一个历经 30 年演化、横跨 `kernel/sched/` 目录约 35000 行 C 代码的复杂系统，其设计思想深深影响了操作系统调度理论的发展。

继续探索的建议：
- 阅读 `Documentation/scheduler/` 下的官方文档
- 跟踪 `sched_tick()` → `scheduler_tick()` → `task_tick_fair()` → `entity_tick()` 的完整 tick 路径
- 研究 `CONFIG_CGROUP_SCHED` 下的组调度实现
- 尝试编写一个简单的 sched_ext BPF 调度器玩具
