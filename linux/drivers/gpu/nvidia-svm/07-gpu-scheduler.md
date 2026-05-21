# 第7篇：GPU 命令提交调度：DRM Scheduler

> 源码：`drivers/gpu/drm/scheduler/sched_main.c` + `sched_entity.c` | 头文件：`include/drm/gpu_scheduler.h`
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. 问题背景：GPU 作业调度为何复杂

GPU 不像 CPU——它不是简单的"取指令→执行→写回"。GPU 同时运行数百个线程、数千个 wavefront，命令是**异步提交**的，执行延迟不可预测。内核需要：

1. **流控**：防止用户态无限提交作业耗尽内核资源
2. **优先级**：KERNEL 级命令优先于 USER 级命令
3. **超时检测**：GPU 挂死时踢出 offending job 恢复可用性
4. **依赖管理**：作业之间有依赖关系（fence）
5. **多引擎调度**：GFX（渲染）、Compute、DMA、Video 各自有独立调度器

DRM Scheduler 是 Linux 内核中解决这些问题的通用框架。它不绑定特定硬件——amdgpu、nouveau、xe、panthor 以及所有 accel 子系统的驱动都在用它。

## 2. 架构总览

`drivers/gpu/drm/scheduler/sched_main.c:25-49`（DOC 注释）：

```
1. 每个硬件运行队列 → 一个 drm_gpu_scheduler
2. 每个 scheduler  → 多个 run queue（HIGH_HW, HIGH_SW, KERNEL, NORMAL）
3. 每个 run queue  → 多个 drm_sched_entity 排队
4. 每个 entity     → SPSC 队列存储 drm_sched_job
```

```
┌─────────────────────────────────────────────────────────┐
│                    drm_gpu_scheduler                    │
│  ops: backend_ops (prepare_job, run_job, timedout_job)  │
│  credit_limit: 64  credit_count: atomic_t              │
│                                                         │
│  ┌─────────────────────────────────────────────────────┐│
│  │  submit_wq: ordered_workqueue("amdgpu-submit")      ││
│  │                                                      ││
│  │  sched_rq[DRM_SCHED_PRIORITY_HIGH_HW]   ← 最高优先 ││
│  │  sched_rq[DRM_SCHED_PRIORITY_HIGH_SW]              ││
│  │  sched_rq[DRM_SCHED_PRIORITY_KERNEL]   ← 内核命令  ││
│  │  sched_rq[DRM_SCHED_PRIORITY_NORMAL]   ← 用户命令  ││
│  │    ┌──────────┐ ┌──────────┐ ┌──────────┐          ││
│  │    │ entity A │ │ entity B │ │ entity C │    ...    ││
│  │    │ SPSC Q   │ │ SPSC Q   │ │ SPSC Q   │          ││
│  │    │ [job1][  │ │ [job3][  │ │ [job5]   │          ││
│  │    │  job2][  │ │  job4]   │ │          │          ││
│  │    └──────────┘ └──────────┘ └──────────┘          ││
│  └─────────────────────────────────────────────────────┘│
│                                                         │
│  delayed_work → timeout handler (TDR)                  │
└─────────────────────────────────────────────────────────┘
```

## 3. 核心数据结构

### 3.1 drm_sched_job — GPU 作业

每个提交到硬件的 GPU 命令被打包成 `drm_sched_job`：

```c
// include/drm/gpu_scheduler.h
struct drm_sched_job {
    struct dma_fence          *s_fence;      // 调度器信号量 fence
    struct dma_fence          *hw_fence;     // 硬件完成 fence（run_job 返回）
    u32                        credits;      // 占用的信用额度
    struct drm_sched_entity   *entity;       // 所属实体
    struct list_head           list;         // 调度器内部链表
    // ...
};
```

关键字段：
- `s_fence`：scheduled fence——作业被**调度到硬件**时 signal
- `hw_fence`：hardware fence——硬件**执行完成**时 signal（由驱动的 `run_job` 回调返回）
- `credits`：占用多少信用。复杂作业（大 buffer）可以占 2+ 信用点，简单作业占 1

### 3.2 drm_sched_entity — 用户上下文

`drm_sched_entity` 代表一个**用户态上下文**（比如一个 OpenGL context 或 CUDA stream）：

```c
struct drm_sched_entity {
    struct drm_sched_rq       *rq;          // 挂载的运行队列
    struct spsc_queue          job_queue;   // SPSC 无锁作业队列
    u32                        priority;    // 优先级
    struct rb_node             rb_tree_node; // RB 树节点（挂到 run queue）
    // ...
};
```

**SPSC 队列**（Single-Producer-Single-Consumer）：用户态只有一个 writer（push_job），内核调度器只有一个 reader（pop_job）。锁开销为零。

### 3.3 drm_sched_backend_ops — 驱动回调

`include/drm/gpu_scheduler.h`：

```c
struct drm_sched_backend_ops {
    struct dma_fence *(*prepare_job)(struct drm_sched_job *job,
                                     struct drm_sched_entity *entity);
    struct dma_fence *(*run_job)(struct drm_sched_job *job);
    void (*timedout_job)(struct drm_sched_job *job);
    void (*free_job)(struct drm_sched_job *job);
};
```

| 回调 | 调用时机 | 含义 |
|------|---------|------|
| `prepare_job` | job 从 entity 取出后 | 准备依赖，做最后的 job 处理 |
| `run_job` | 硬件可以接纳新作业时 | 真正提交到 GPU 硬件队列 |
| `timedout_job` | job 超时 | 该作业已挂死，驱动负责恢复 |
| `free_job` | job 生命周期结束 | 释放驱动私有数据 |

### 3.4 drm_sched_init_args — 初始化参数

`include/drm/gpu_scheduler.h`：

```c
struct drm_sched_init_args {
    const struct drm_sched_backend_ops *ops;
    u32         credit_limit;    // 最大并行作业数（典型 64）
    const char *name;            // 调度器名称（如 "amdgpu"）
    unsigned    num_rqs;         // 运行队列数量
    u32         hang_limit;      // TDR 容忍次数
    long        timeout;         // 超时时间（jiffies）
    struct workqueue_struct *timeout_wq;
    struct workqueue_struct *submit_wq;
    struct device *dev;
    // ...
};
```

## 4. 信用制流控

这是调度器最核心的**反压机制**。想象用户态疯狂提交作业——如果没有流控，内核会耗尽内存。

### 4.1 drm_sched_available_credits

`sched_main.c:96-105`

```c
static u32 drm_sched_available_credits(struct drm_gpu_scheduler *sched)
{
    u32 credits;
    WARN_ON(check_sub_overflow(sched->credit_limit,
                               atomic_read(&sched->credit_count),
                               &credits));
    return credits;
}
```

`credit_limit - credit_count = available_credits`。初始 `credit_count=0`，最大 64。

### 4.2 drm_sched_can_queue

`sched_main.c:115-134`

```c
static bool drm_sched_can_queue(struct drm_gpu_scheduler *sched,
                                struct drm_sched_entity *entity)
{
    struct drm_sched_job *s_job;
    s_job = drm_sched_entity_queue_peek(entity);
    if (!s_job)
        return false;

    /* If a job exceeds the credit limit, truncate it to the credit limit
     * itself to guarantee forward progress.
     */
    if (s_job->credits > sched->credit_limit) {
        dev_WARN(sched->dev,
                 "Jobs may not exceed the credit limit, truncate.\n");
        s_job->credits = sched->credit_limit;
    }

    return drm_sched_available_credits(sched) >= s_job->credits;
}
```

**关键保护**：即使某个 job 声称 credits 超过 credit_limit，调度器也将其截断为 limit。保证**永远有前向进展**——不会出现"需要 65 个 credit 但最多只有 64"的死锁。

### 4.3 信用流动

```
用户 push_job → entity SPSC 队列
    ↓
调度器 wake_up → 查看队首 job 的 credits
    ↓
available_credits >= credits? 
    ├─ YES → 从 entity 弹出 job，credit_count += credits
    │        调用 prepare_job → run_job → 提交给硬件
    │        硬件完成后 → credit_count -= credits（回收）
    └─ NO  → 等待（调度器休眠，某个 job 完成时唤醒）
```

```
信用水位:
  credit_limit (64)
  ┌────────────────────────────────────────┐
  │████████████████████████░░░░░░░░░░░░░░░░│ ← 已用 30 信用
  └────────────────────────────────────────┘
  credit_count = 30,  available = 34

  新 job 需要 10 credits → OK
  新 job 需要 50 credits → 等待
```

## 5. 优先级调度

### 5.1 优先级定义

DRM scheduler 定义了多个优先级级别：

```c
enum drm_sched_priority {
    DRM_SCHED_PRIORITY_MIN,
    DRM_SCHED_PRIORITY_NORMAL = DRM_SCHED_PRIORITY_MIN,
    DRM_SCHED_PRIORITY_HIGH_SW,
    DRM_SCHED_PRIORITY_KERNEL,
    DRM_SCHED_PRIORITY_HIGH_HW,
    DRM_SCHED_PRIORITY_COUNT
};
```

数字越大优先级越高：`HIGH_HW > KERNEL > HIGH_SW > NORMAL`。

### 5.2 Round-Robin 同一优先级内调度

`sched_main.c:136-139`：

```c
static __always_inline bool drm_sched_entity_compare_before(
    struct rb_node *a, const struct rb_node *b)
{
    struct drm_sched_entity *ent_a = rb_entry((a), struct drm_sched_entity, rb_tree_node);
    // ...
```

当多个 entity 在同一优先级 run queue 中时，调度器使用 RB 树实现 Round-Robin：每次从当前 entity 取一个 job，然后移动到下一个 entity。防止某个 entity 饿死其他 entity。

### 5.3 调度循环

```
for priority in [HIGH_HW, KERNEL, HIGH_SW, NORMAL]:
    if sched_rq[priority] 有 entity 且 can_queue:
        从当前 entity 取 job
        调用 run_job()
        return

如果所有优先级都无可提交的 job → 调度器睡眠
```

## 6. Job 生命周期

### 6.1 drm_sched_job_init — 初始化

`sched_main.c:800-829`

```c
int drm_sched_job_init(struct drm_sched_job *job,
                       struct drm_sched_entity *entity,
                       u32 credits, void *owner,
                       uint64_t drm_client_id)
{
    if (!entity->rq) {
        dev_err(job->sched->dev,
                "%s: entity has no rq!\n", __func__);
        return -ENOENT;
    }
    if (unlikely(!credits)) {
        pr_err("*ERROR* %s: credits cannot be 0!\n", __func__);
        return -EINVAL;
    }

    memset(job, 0, sizeof(*job));    // 关键安全性：全零
    job->entity = entity;
    job->credits = credits;
    job->s_fence = drm_sched_fence_alloc(entity, owner, drm_client_id);
```

初始化流程：
1. 检查 entity 是否有有效 run queue
2. 检查 credits 不为 0
3. **memset 清零**整个 job 结构体——这是防御性编程
4. 分配 `s_fence`（scheduled fence）和 `hw_fence`

### 6.2 drm_sched_entity_push_job — 提交

`sched_entity.c:576`

```c
void drm_sched_entity_push_job(struct drm_sched_job *sched_job)
{
    struct drm_sched_entity *entity = sched_job->entity;
    bool first;
    ktime_t submit_ts;

    trace_drm_sched_job_queue(sched_job, entity);
    // ...
    atomic_inc(entity->rq->sched->score);
    WRITE_ONCE(entity->last_user, current->group_leader);
```

**关键**: `push_job` 只是把 job 放入 entity 的 SPSC 队列。实际的 `run_job` 发生在调度器 workqueue 线程中。

### 6.3 run_job — 硬件提交

驱动实现的 `run_job` 回调将 job 写入 GPU 寄存器/命令环缓冲区（ring buffer）。返回一个 `dma_fence`，表示硬件完成信号。

```
用户态               内核态                         GPU 硬件
   │                   │                              │
   ├─ ioctl(提交job) ──→│                              │
   │                   ├─ push_job → SPSC queue       │
   │                   ├─ wake_up scheduler           │
   │                   ├─ prepare_job                 │
   │                   ├─ run_job ──────────────────────→│
   │                   │   (写寄存器/ring buffer)       │
   │                   │                              ├─ 执行命令
   │                   │                              ├─ 完成!
   │   ← fence signal ─┤  ← hw_fence signal ───────────┤
   │                   ├─ free_job（释放资源）         │
```

## 7. 超时处理与 TDR

### 7.1 Timeout Detection and Recovery

`sched_main.c:1317` — `drm_sched_init` 设置 `sched->timeout`（jiffies），然后启动一个延迟 workqueue：

1. 每个 job 提交后，记录 `job->submit_ts`
2. 调度器定期检查 `pending_list` 中最老的 job
3. 如果 `now - submit_ts > timeout` → 调用 `timedout_job` 回调
4. 驱动在 `timedout_job` 中重置 GPU 引擎或踢出 offending job

### 7.2 hang_limit

`sched_main.c:1325`：

```c
sched->hang_limit = args->hang_limit;
```

如果连续多次挂死（超过 `hang_limit`），调度器认为硬件不可恢复，标记整个 scheduler 不可用。

## 8. drm_sched_init 和 drm_sched_fini

### 8.1 初始化

`sched_main.c:1317-1359`

```c
int drm_sched_init(struct drm_gpu_scheduler *sched,
                   const struct drm_sched_init_args *args)
{
    int i;

    sched->ops = args->ops;
    sched->credit_limit = args->credit_limit;
    sched->name = args->name;
    sched->timeout = args->timeout;
    sched->hang_limit = args->hang_limit;
    sched->timeout_wq = args->timeout_wq ? args->timeout_wq : system_percpu_wq;
    sched->score = args->score ? args->score : &sched->_score;
    sched->dev = args->dev;

    if (args->num_rqs > DRM_SCHED_PRIORITY_COUNT) {
        dev_err(sched->dev,
                "%s: num_rqs cannot be greater than DRM_SCHED_PRIORITY_COUNT\n",
                __func__);
        return -EINVAL;
    }

    sched->sched_rq = kmalloc_objs(*sched->sched_rq, args->num_rqs,
                                   GFP_KERNEL | __GFP_ZERO);
```

关键步骤：
1. 从 `args` 复制所有参数
2. 校验 `num_rqs` 不超过 `DRM_SCHED_PRIORITY_COUNT`
3. 分配 `sched_rq[]` 数组，每个 run queue 用 RB 树管理 entity
4. 分配 `submit_wq`：`alloc_ordered_workqueue`（行 1349），有序保证 FIFO
5. 启动 timeout delayed work

### 8.2 销毁

`sched_main.c:1420-1448`

```c
void drm_sched_fini(struct drm_gpu_scheduler *sched)
{
    int i;

    drm_sched_wqueue_stop(sched);              // 停止调度 workqueue
    for (i = DRM_SCHED_PRIORITY_KERNEL; i < sched->num_rqs; i++)
        kfree(sched->sched_rq[i]);            // 释放各优先级 run queue

    wake_up_all(&sched->job_scheduled);        // 唤醒等待者
    cancel_delayed_work_sync(&sched->work_tdr); // 取消超时 work

    if (sched->ops->cancel_job)
        drm_sched_cancel_remaining_jobs(sched); // 取消残留 job

    if (sched->own_submit_wq)
        destroy_workqueue(sched->submit_wq);
    sched->ready = false;

    if (!list_empty(&sched->pending_list))
        dev_warn(sched->dev,
                 "Tearing down scheduler while jobs are pending!\n");
}
```

**防御性警告**（行 1445-1446）：如果销毁时 `pending_list` 非空，说明有 job 未完成——这通常是 bug。

## 9. NVIDIA AI Infra 中的角色

在 NVIDIA AI Infra 场景中，DRM Scheduler 直接管理 CUDA kernel launch 和 GPU 计算命令：

```
用户: cudaLaunchKernel(myKernel, grid, block)
  ↓
libcuda.so → ioctl 到内核
  ↓
drm_sched_job_init(job, entity, credits=1, ...)
  ↓
drm_sched_entity_push_job(job)   ← 放入 SPSC 队列
  ↓
调度器 workqueue:
  prepare_job → 检查 fence 依赖
  run_job     → 写入 GPU 命令环缓冲区
  ↓
GPU 执行 kernel 计算
  ↓
hw_fence signal → job 释放 → credit 回收
```

AI 训练中，数千个 kernel 在多个 CUDA streams 上并发。DRM Scheduler 确保：
- **Compute 优先级**高于 GFX 优先级
- **大 kernel**（长运行时间）不会阻塞小 kernel
- **超时 kernel**（死循环）被 TDR 踢出，不影响同一 GPU 上的其他进程

---

## 下一篇文章

[第8篇：GPU→RDMA 零拷贝桥梁：umem_dmabuf.c](08-umem-dmabuf.md)

简介：`ib_umem_dmabuf_get_pinned` 如何接收 GPU 的 dmabuf fd 并转换为 RDMA MR 所需的 SG table。