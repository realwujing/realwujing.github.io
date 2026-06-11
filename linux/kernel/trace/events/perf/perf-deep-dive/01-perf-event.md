# struct perf_event 与 perf_event_open — 内核性能事件子系统深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`kernel/events/core.c` | 头文件：`include/linux/perf_event.h` `kernel/events/internal.h`

---

本专题直接解剖 Linux 内核的 perf_event 子系统。这是 `perf top`、`perf stat`、`perf record`、`perf_event_open()` 的用户态工具背后的内核引擎。

---

> 系列目录：perf 内核源码深度解析（本目录下 4 篇）

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | `01-perf-event.md` (本篇) | struct perf_event、perf_event_open、PMU 注册 |
| 2 | `02-ring-buffer.md` | Ring buffer、用户态 mmap、采样写出 |
| 3 | `03-pmu.md` | x86 PMU 驱动、硬件计数器、NMI 采样 |
| 4 | `04-tooling.md` | tracepoint、ftrace 集成、perf/kprobe/uprobe |

---

## 1. struct perf_event — 核心对象

```c
// include/linux/perf_event.h:765
struct perf_event {
    struct list_head                event_entry;    // 在 ctx->event_list 的链表节点
    struct list_head                sibling_list;   // 同组 sibling 的链表
    int                             cpu;            // 目标 CPU (-1 = 任意)
    struct perf_event               *group_leader;  // 组长（本事件 = 组长时指向自己）
    struct pmu                      *pmu;           // ★ 指向对应的 PMU 实例
    enum perf_event_state           state;          // PERF_EVENT_STATE_OFF/INACTIVE/ACTIVE
    local64_t                       count;          // ★ 硬件计数器的原始值
    atomic64_t                      child_count;    // 子事件合并的计数

    /*
     * These are the total time in nanoseconds that the event
     * has been enabled and running (respectively) for
     * this event:
     */
    u64                             total_time_enabled;
    u64                             total_time_running;

    struct perf_event_attr          attr;           // ★ 用户态传入的事件属性
    struct hw_perf_event            hw;             // 硬件特定的状态（PMU 寄存器地址等）

    struct perf_event_context       *ctx;           // 所属上下文
    struct file                     *mmap_file;     // mmap 时的文件对象
    atomic_long_t                   refcount;

    /* mmap 数据页 */
    struct ring_buffer              *rb;            // ★ AUX 或主 ring buffer
    struct list_head                rb_entry;

    /* 采样相关 */
    u64                             id;             // 事件的全局唯一 ID
    struct perf_addr_filters_head   addr_filters;   // 地址过滤

    /* cgroup 跟踪 */
    struct perf_cgroup              *cgrp;

    /* overflow handling */
    int                             pending_wakeup;
    int                             pending_kill;
    int                             pending_disable;
    int                             pending_trans;
    int                             pending;
    struct irq_work                 pending_irq;

    /* 事件的溢出次数 */
    atomic_t                        event_limit;

    /* 事件的时间戳和采样周期 */
    u64                             tstamp;
    u64                             shadow_ctx_time;

    struct perf_event               *parent_event;  // 父事件（指向继承的父 context 中的事件）

    struct callback_head            callback_head;
    struct pid_namespace            *ns;
    u64                             ns_pid;
    u64                             ns_tid;

    struct list_head                sb_list;

    /* 硬件 breakpoint 的附加信息 */
    struct arch_hw_breakpoint       hw_bp;
};
```

**核心字段说明**：

| 字段 | 含义 |
|------|------|
| `pmu` | 指向物理/逻辑 PMU 实例——决定事件如何被"打开"、"读"和"关闭" |
| `attr` | 用户态 `perf_event_open` 传入的精确副本：`.type`, `.config`, `.sample_period` 等 |
| `hw` | PMU 硬件特定的状态——x86 的 MSR 地址、Intel PT 的 ToPA 表指针等 |
| `count` | 被 PMU 硬件更新的计数器值——用户态通过 `read()` 或 `PERF_EVENT_IOC_PERIOD` 取得 |
| `ctx` | 上下文：per-cpu 或 per-task |

---

## 2. struct pmu — PMU 抽象层

```c
// include/linux/perf_event.h (约 line 280+)
struct pmu {
    struct list_head                entry;           // 在全局 pmu 链表中的节点
    struct module                   *module;         // PMU 所属模块（如果是可卸载的）
    const struct attribute_group    **attr_groups;   // sysfs 属性组
    const struct attribute_group    **attr_update;   // 设备特定的动态属性
    const char                      *name;           // PMU 名称（如 "cpu", "power", "intel_pt"）
    int                             type;            // PERF_TYPE_*（仅对软件 PMU 有效）

    /*
     * PMU 的能力和能力标志
     */
    int                             capabilities;    // PERF_PMU_CAP_* 标志

    int * __percpu                  pmu_disable_count;  // PMU 被禁用的次数
    atomic_t                        exclusive_cnt;      // 独占计数

    /*
     * PMU 提供的方法（实现 p 是 PMU 驱动）
     */
    int (*event_init)               (struct perf_event *);     // ★ 验证 attr 和分配资源
    void (*event_mapped)            (struct perf_event *event, struct mm_struct *mm);
    void (*event_unmapped)          (struct perf_event *event, struct mm_struct *mm);
    int (*add)                      (struct perf_event *event, int flags);     // 激活事件
    int (*del)                      (struct perf_event *event, int flags);     // 停用事件
    void (*start)                   (struct perf_event *event, int flags);     // 开始计数
    void (*stop)                    (struct perf_event *event, int flags);     // 停止计数
    void (*read)                    (struct perf_event *event);                // 读取计数器
    int (*start_txn)                (struct pmu *pmu, unsigned int txn_flags);
    int (*commit_txn)               (struct pmu *pmu);
    int (*cancel_txn)               (struct pmu *pmu);
    int (*event_idx)                (struct perf_event *event);

    void (*sched_task)              (struct perf_event *event, bool sched_in);

    /* 过滤 */
    int (*addr_filters_validate)    (struct list_head *filters);
    void (*addr_filters_sync)       (struct perf_event *event);
    int (*aux_output_match)         (struct perf_event *event);

    /* 设置 */
    void (*set_aux_output)          (struct perf_event *event);
    void (*get_aux_output)          (struct perf_event *event);
    void (*check_period)            (struct perf_event *event, u64 value);

    /* AUX buffer 管理 */
    void *(*setup_aux)              (struct perf_event *event, void **pages, int nr_pages, bool overwrite);
    void (*free_aux)                (void *aux);
    int (*snapshot_aux)             (struct perf_event *event, void *data, long size);
};
```

**三个核心方法**：
- `event_init`：验证 `attr`，如果无效返回错误
- `add`：分配硬件计数器，连接到 PMU——之后事件开始计数
- `del`：释放硬件计数器，断开与 PMU 的连接

---

## 3. perf_event_open — 系统调用入口

```c
// kernel/events/core.c:13844
SYSCALL_DEFINE5(perf_event_open,
                struct perf_event_attr __user *, attr_uptr,
                pid_t, pid, int, cpu, int, group_fd, unsigned long, flags)
{
    struct perf_event *group_leader = NULL, *output_event = NULL;
    struct perf_event *event, *sibling;
    struct perf_event_attr attr;
    struct perf_event_context *ctx;
    struct file *event_file = NULL;
    struct fd group = {NULL, 0};
    struct task_struct *task = NULL;
    struct pmu *pmu;
    int event_fd;
    int move_group = 0;
    int err;

    // ① 验证 attr（从用户态拷贝并检查有效性）
    if (copy_from_user(&attr, attr_uptr, sizeof(attr)) != 0)
        return -EFAULT;
    if (perf_event_attr_verify(&attr))       // 验证 type/config/sample_period 等
        return -EINVAL;

    // ② 获取目标 task / CPU
    if (pid == 0) {
        task = current;
    } else if (pid == -1) {
        // -1 表示 per-CPU 事件，cpu >= 0
    } else {
        task = find_task_by_vpid(pid);       // 按 pid 找目标 task
    }

    // ③ 如果提供了 group_fd，找到组长事件
    if (group_fd != -1) {
        group = fdget(group_fd);
        group_leader = group.file->private_data;
    }

    // ④ 找到 PMU 并分配 perf_event
    pmu = perf_init_event(&attr);             // 根据 attr.type 找到 PMU
    event = perf_event_alloc(&attr, cpu, task, group_leader, NULL, NULL, NULL, pmu->type);

    // ⑤ ★ pmu->event_init(event) — 验证具体参数，分配 PMU 资源
    err = pmu->event_init(event);

    // ⑥ 获取上下文（per-task 或 per-CPU）
    ctx = find_get_context(task, event);       // 每个 task 或每个 CPU 的 perf_event_context

    // ⑦ 安装到文件描述符
    event_file = anon_inode_getfile("[perf_event]", &perf_fops, event, O_RDWR);
    event->mmap_file = event_file;

    // ⑧ add to context
    perf_install_in_context(ctx, event, cpu);

    // ⑨ 如果提供 group_fd，把 event 加入该 group
    if (group_fd != -1)
        perf_group_attach(event);

    fd_install(event_fd, event_file);
    return event_fd;
}
```

**调用完成后发生了什么**：
1. `event_fd` 返回给用户态——这就是 `perf stat -e cycles` 或 `perf record` 持有的 fd
2. `pmu->event_init` 已调用——此时验证通过，但硬件计数器尚未激活
3. 事件通过 `perf_install_in_context` 安装到目标上下文，等待 `perf_event_enable()` 或 `ioctl(PERF_EVENT_IOC_ENABLE)`

---

## 4. PMU 注册 — perf_pmu_register

```c
// kernel/events/core.c 中
// PMU 实例向 perf_event 子系统注册的函数
int perf_pmu_register(struct pmu *pmu, const char *name, int type)
{
    // ① 验证必要字段（name 和 event_init 是必须的）
    // ② 分配 IDA（id 分配器）——给每个 PMU 分配唯一的 ID
    // ③ 注册到全局 pmu_idr（按 ID 索引）
    // ④ 创建 sysfs 目录 /sys/bus/event_source/devices/<name>/
    // ⑤ 安装属性组（用户态 perf 工具通过 sysfs 读取能力信息）
    // ⑥ 调用 pmu_dev_alloc → 创建设备节点（用于 cgroup 隔离 / event fd 引用）
}
```

**现有的 PMU 实例示例**：

| PMU 名称 | type | 来源 | sysfs 路径 |
|----------|------|------|------------|
| `cpu` | PERF_TYPE_RAW | x86 PMU 驱动 | `/sys/bus/event_source/devices/cpu/` |
| `software` | PERF_TYPE_SOFTWARE | `kernel/events/core.c` | `/sys/bus/event_source/devices/software/` |
| `tracepoint` | PERF_TYPE_TRACEPOINT | `kernel/events/core.c` | `/sys/bus/event_source/devices/tracepoint/` |
| `power` | 动态分配 | `drivers/powercap/` | `/sys/bus/event_source/devices/power/` |
| `intel_pt` | 动态分配 | `arch/x86/events/intel/` | `/sys/bus/event_source/devices/intel_pt/` |
| `uncore` | 动态分配 | `arch/x86/events/intel/` | `/sys/bus/event_source/devices/uncore/` |

---

## 5. perf_event_context — 事件所属上下文

```c
// include/linux/perf_event.h 中的简化定义
struct perf_event_context {
    struct pmu                      *pmu;           // 上下文关联的 PMU
    raw_spinlock_t                  lock;           // 保护 event_list 的锁
    struct mutex                    mutex;          // 保护 pin_count + task 的大锁
    struct list_head                event_list;     // ★ 激活事件的链表
    int                             nr_events;      // 事件计数
    int                             nr_active;      // 当前活跃的事件
    int                             nr_stat;        // 有 stat 输出的事件
    int                             is_active;      // 上下文本是否活跃
    atomic_t                        refcount;       // 引用计数
    struct task_struct              *task;          // 拥有此上下文的 task（仅 per-task context）
    int                             cpu;            // 上下文所在的 CPU（仅 per-cpu context）
    struct perf_cgroup              *cgrp;          // cgroup 指针（仅 cgroup 事件）

    /* inherited 事件 */
    struct list_head                pinned_groups;  // 固定的事件组
    struct list_head                flexible_groups;// 灵活的事件组
};
```

每 task 一个 context 意味着**子进程通过 fork 继承 perf 事件**——这是 `perf stat -p <pid>` 能跟踪整个进程树的关键机制。

---

## 6. 从 attr 到 PMU 的查找 — perf_init_event

```
perf_event_open 调用
  → copy_from_user(&attr, ...)
  → perf_init_event(attr)
    → 通过 attr.type 查找 PMU：
      PERF_TYPE_HARDWARE → 查找 PMU 列表中的 pmu->type == PERF_TYPE_HARDWARE
        → 返回 &pmu
      PERF_TYPE_SOFTWARE → pmu = &perf_swevent_pmu
      PERF_TYPE_TRACEPOINT → pmu = &perf_tracepoint_pmu
      PERF_TYPE_BREAKPOINT → pmu = &perf_breakpoint_pmu
  → pmu->event_init(event) → 验证 attr.config + 分配 PMU 资源
```

---

## 7. 三个核心接口

用户态通过 3 个接口与 perf_event 交互：

| 接口 | 调用方式 | 含义 |
|------|---------|------|
| `ioctl(fd, PERF_EVENT_IOC_RESET/ENABLE/DISABLE)` | ioctl | 重置/启用/停用计数器 |
| `read(fd, &count)` | read | 读取当前 counter 值（`event->count`） |
| `mmap(fd, ...)` | mmap | 映射 ring buffer + 用户态读取采样数据 |

**mmap 的特殊之处**：用户态不只读取数据——`perf_event_update_userpage` 将 1 页的元数据（`perf_event_mmap_page`）映射到用户态，包含 `data_head`、`data_tail` 等指针，使用户态能**零系统调用**地轮询 ring buffer 中的采样记录。

---

## 8. 总结

| 组件 | 关键函数 | 核心作用 |
|------|---------|---------|
| perf_event_open | `core.c:13844` | 创建 perf_event，分配 fd |
| pmu->event_init | PMU 驱动 | 验证 attr、分配硬件资源 |
| perf_pmu_register | core.c | 注册到全局 PMU 列表 |
| perf_install_in_context | core.c | 将事件安装到 context |
| perf_event_mmap | core.c | 用户态 mmap ring buffer |
| perf_output_sample | core.c | 从 ring buffer 写出采样数据 |
