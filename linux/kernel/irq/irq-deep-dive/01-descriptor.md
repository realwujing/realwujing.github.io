# 第一篇：IRQ 描述符与 request_irq — 中断注册的完整生命周期

> 源码：kernel/irq/manage.c kernel/irq/irqdesc.c | 头文件：include/linux/irqdesc.h include/linux/irq.h include/linux/interrupt.h

系列目录：[IRQ 内核源码深度解析](./README.md)

---

## 1. 概述

中断是操作系统与外设交互的核心机制。当网卡收到数据包、磁盘完成 DMA 传输、定时器到期时，硬件通过中断通知 CPU。Linux 内核中，每个中断都由一个 `struct irq_desc` 描述符管理系统，涉及描述符分配、中断注册、线程化处理、程序清理四大阶段。

本文将深入分析 `request_irq()` 从调用到 ISR 执行的完整生命周期，覆盖数据结构、每个阶段的核心源码，并用 ASCII 流程图串联整个调用路径。

---

## 2. 核心数据结构

### 2.1 struct irq_desc — 中断描述符 (irqdesc.h:80)

```c
// include/linux/irqdesc.h:80
struct irq_desc {
    struct irq_common_data    irq_common_data;   // 通用状态、affinity、MSI 元数据
    struct irq_data           irq_data;          // 芯片级抽象：irq、hwirq、chip、domain
    unsigned int __percpu    *kstat_irqs;        // per-CPU 中断计数
    irq_flow_handler_t        handle_irq;        // 流控处理函数 (level/edge/fasteoi)
    struct irqaction         *action;            // ISR 链表头
    unsigned int              status_use_accessors;  // IRQ_* 状态标志
    unsigned int              core_internal_state__do_not_mess_with_it;  // IRQS_*
    unsigned int              depth;             // 0=已启用, >0=已禁用 (嵌套计数)
    unsigned int              wake_depth;         // wakeup 源计数
    unsigned int              tot_count;          // deprecated
    unsigned int              irq_count;          // 总中断次数 (记录用)
    unsigned long             last_unhandled;     // 最后一次 IRQ_NONE 的时间
    unsigned int              irqs_unhandled;     // 未处理中断计数
    raw_spinlock_t            lock;              // 保护本描述符
    struct cpumask           *percpu_enabled;    // per-CPU 启用状态
#ifdef CONFIG_SMP
    const struct cpumask     *affinity;           // 原有 affinity, 下次设置时使用
    struct irq_affinity_notify *affinity_notify;
    unsigned long             affinity_force[4];
#endif
#ifdef CONFIG_GENERIC_PENDING_IRQ
    cpumask_var_t             pending_mask;
#endif
    unsigned long             threads_oneshot;   // 已触发等待线程的 oneshot 位图
    atomic_t                  threads_active;    // 正在运行的线程数
    wait_queue_head_t         wait_for_threads;  // 等待所有线程退出的等待队列
#ifdef CONFIG_PM
    unsigned int              nr_actions;        // 已注册的 irqaction 数量
    struct irqaction         *pm_action;         // PM 专用 irqaction
#endif
    struct irq_data          *parent_data;       // 父域 irq_data 指针
    struct irq_desc          *parent_desc;       // 父域 irq_desc 指针
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry    *dir;               // /proc/irq/N/ 目录
#endif
    // ... 更多字段
};
```

**字段说明：**

| 字段 | 作用 |
|------|------|
| `irq_common_data` | 状态位 (`IRQD_*`)、NUMA node、chip handler_data、MSI 描述符、affinity |
| `irq_data` | `mask`、`irq` (Linux 号)、`hwirq` (硬件号)、`chip` (控制器)、`domain` (中断域) |
| `handle_irq` | 函数指针，指向 `handle_level_irq` / `handle_edge_irq` / `handle_fasteoi_irq` 之一 |
| `action` | 用户注册的 ISR 链表头，共享中断时链式存储 |
| `depth` | 嵌套禁用计数器；0 表示 IRQ 启用，1 表示第一次禁用的层级 |
| `lock` | 保护 desc 的自旋锁 (`raw_spinlock_t`)，不能进睡眠上下文 |
| `wait_for_threads` | ONESHOT 场景下，kernel 等待所有线程退出后才会再次使能 IRQ |

### 2.2 struct irq_common_data — 通用中断数据 (irq.h:149)

```c
// include/linux/irq.h:149
struct irq_common_data {
    unsigned int          state_use_accessors;   // IRQD_* 位图
#ifdef CONFIG_NUMA
    unsigned int          node;
#endif
    void                 *handler_data;          // chip 控制器私有数据
    struct msi_desc      *msi_desc;             // MSI 描述符 (SPI 时为 NULL)
    cpumask_var_t         affinity;              // SMP 亲和性
#ifdef CONFIG_GENERIC_IRQ_EFFECTIVE_AFF_MASK
    cpumask_var_t         effective_affinity;   // 调度实际使用的 affinity
#endif
#ifdef CONFIG_GENERIC_IRQ_IPI
    unsigned int          ipi_offset;
#endif
};
```

**关键字段：**

- `state_use_accessors`：通过 `irqd_*()` 辅助宏操作，常用标志：

  | 标志 (IRQD_) | 含义 |
  |--------------|------|
  | `IRQD_TRIGGER_RISING/FALLING/HIGH/LOW` | 硬件触发类型 |
  | `IRQD_LEVEL` | Level 触发 |
  | `IRQD_ACTIVATED` | 已 activate |
  | `IRQD_IRQ_STARTED` | 已启用 (unmasked) |
  | `IRQD_IRQ_DISABLED` | 已禁用 |
  | `IRQD_IRQ_MASKED` | 已 mask |
  | `IRQD_PER_CPU` | per-CPU 中断 |
  | `IRQD_WAKEUP_STATE` | 唤醒源状态 |
  | `IRQD_MOVE_PCNTXT` | 禁止 IRQ 迁移 |

- `node`：NUMA node ID，分配描述符和 per-CPU 数据时使用
- `msi_desc`：MSI/MSI-X 消息接口描述符

### 2.3 struct irq_data — 芯片级抽象 (irq.h:181)

```c
// include/linux/irq.h:181
struct irq_data {
    u32               mask;
    unsigned int      irq;
    unsigned long     hwirq;
    struct irq_common_data *common;
    struct irq_chip   *chip;
    struct irq_domain *domain;
#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
    struct irq_data   *parent_data;
#endif
    void             *chip_data;
};
```

这是连接 Linux IRQ 编号和硬件中断的桥梁：

- `irq`：Linux IRQ 号 (0 ~ nr_irqs-1)
- `hwirq`：硬件 IRQ 号 (GIC 上的中断 ID、MSI 消息编码等)
- `chip`：`irq_chip` 方法表，操作硬件寄存器
- `domain`：所属中断域，负责 `hwirq ↔ Linux irq` 的映射

```
  Linux IRQ 12  ──►  irq_data.irq = 12
                     irq_data.hwirq = 44          (GIC 中断 ID)
                     irq_data.chip  = &gic_chip   (GIC 寄存器操作)
                     irq_data.domain = &gic_domain
```

### 2.4 struct irqaction — 用户注册的 ISR (interrupt.h:123)

```c
// include/linux/interrupt.h:123
struct irqaction {
    irq_handler_t       handler;       // ISR 函数 (在硬中断上下文执行)
    void               *dev_id;        // 设备 ID，共享中断时用于区分设备
    void               *percpu_dev_id;
    void __percpu      *percpu_dev_id;
    irq_handler_t       thread_fn;     // 线程处理函数 (IRQF_ONESHOT)
    struct task_struct  *thread;       // 内核线程 task_struct
    struct irqaction    *next;         // 链表中下一个 action (共享中断)
    unsigned int        irq;           // Linux IRQ 号
    unsigned int        flags;         // IRQF_* 标志
    unsigned long       thread_flags;  // IRQTF_* 标志
    unsigned long       thread_mask;   // 线程状态的位掩码
    const char         *name;          // 设备名称
    struct proc_dir_entry *dir;
};
```

**链表结构 (共享中断)：**

```
  desc->action → [irqaction #1, handler=eth0_intr, dev_id=eth0]  → [irqaction #2, handler=eth1_intr, dev_id=eth1]  → NULL
                                        ↑ next                                                ↑ next
```

---

## 3. IRQ 标志 (interrupt.h:31-88)

```c
// include/linux/interrupt.h:31-88

#define IRQF_TRIGGER_NONE   0x00000000
#define IRQF_TRIGGER_RISING 0x00000001      // 上升沿触发
#define IRQF_TRIGGER_FALLING  0x00000002    // 下降沿触发
#define IRQF_TRIGGER_HIGH   0x00000004      // 高电平触发
#define IRQF_TRIGGER_LOW    0x00000008      // 低电平触发
#define IRQF_SHARED         0x00000080      // 允许多个设备共享此 IRQ
#define IRQF_PROBE_SHARED   0x00000100      // 共享 IRQ 探测用
#define __IRQF_TIMER        0x00000200      // 标记为定时器中断
#define IRQF_PERCPU         0x00000400      // per-CPU 中断 (无需锁保护)
#define IRQF_NOBALANCING    0x00000800      // 禁止 irqbalance 迁移此中断
#define IRQF_IRQPOLL        0x00001000      // 轮询模式
#define IRQF_ONESHOT        0x00002000      // 一次性中断: 完成线程后 unmask
#define IRQF_NO_SUSPEND     0x00004000      // 系统挂起时不 disable 此中断
#define IRQF_FORCE_RESUME   0x00008000      // 恢复时强制重新 enable
#define IRQF_NO_THREAD      0x00010000      // 禁止强制线程化
#define IRQF_EARLY_RESUME   0x00020000      // 早期恢复 (早于 syscore)
#define IRQF_COND_SUSPEND   0x00040000      // 条件挂起
```

**重要标志详细说明：**

| 标志 | 作用 |
|------|------|
| `IRQF_SHARED` | 多个设备共享一个 IRQ 线。每个 ISR 必须检查 `dev_id` |
| `IRQF_ONESHOT` | 执行 handler 后保持 IRQ 被 mask，直到线程完成再 unmask |
| `IRQF_PERCPU` | per-CPU 中断，只在本 CPU 上执行，无需自旋锁 |
| `IRQF_NO_SUSPEND` | 系统挂起后此 IRQ 仍使能，通常用于唤醒源 |
| `IRQF_NO_THREAD` | 即使内核启用了 `threadirqs=`，此 IRQ 也不强制线程化 |
| `IRQF_EARLY_RESUME` | 在 syscore 恢复早期阶段重新启用此 IRQ |

---

## 4. request_irq — 中断注册入口

### 4.1 request_irq 包装函数 (interrupt.h:172)

```c
// include/linux/interrupt.h:172
static inline int __must_check
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
            const char *name, void *dev)
{
    return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}
```

`request_irq` 是 `request_threaded_irq` 的简易包装，将 `thread_fn` 设为 NULL 表示不创建线程。

### 4.2 request_threaded_irq (manage.c:2115)

```c
// kernel/irq/manage.c:2115
int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn, unsigned long irqflags,
                         const char *devname, void *dev_id)
{
    struct irqaction *action;
    struct irq_desc *desc;
    int retval;

    // ... 参数校验 ...

    // (1) 查找中断描述符
    desc = irq_to_desc(irq);
    if (!desc)
        return -EINVAL;

    // ... 休眠标志校验 (不能在原子上下文中调用) ...

    // (2) 分配 irqaction
    action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
    if (!action)
        return -ENOMEM;

    // (3) 初始化 irqaction
    action->handler  = handler;
    action->thread_fn = thread_fn;
    action->flags    = irqflags;
    action->name     = devname;
    action->dev_id   = dev_id;
    action->irq      = irq;

    // (4) 核心安装
    retval = __setup_irq(irq, desc, action);

    if (retval)
        kfree(action);

    return retval;
}
```

**调用流程：**

```
request_irq(irq, handler, flags, name, dev)
  └── request_threaded_irq(irq, handler, NULL, flags, name, dev)
        ├── irq_to_desc(irq)          // 查找描述符
        ├── kzalloc(irqaction)        // 分配 action
        ├── 初始化 action 字段
        └── __setup_irq(irq, desc, action)   // 核心安装逻辑
```

---

## 5. __setup_irq — 核心安装逻辑 (manage.c:1470)

这是中断注册最复杂的函数，约 500 行，处理共享、ONESHOT、PM、线程化等所有场景。

### 5.1 整体流程

```
__setup_irq(irq, desc, action)
│
├── 1. 参数和 sanity 校验
│     ├── 检查 ONESHOT + SHARED 冲突
│     ├── 检查 PERCPU 与线程化冲突
│     └── 验证触发类型一致性
│
├── 2. 初次安装 (action 链表为空)
│     ├── 分配 per-CPU kstat_irqs
│     ├── irq_request_resources()     // 请求硬件资源
│     ├── irq_setup_affinity()        // 设置初始 CPU affinity
│     ├── irq_activate()              // activate (设置 irq_set_type 等)
│     └── irq_startup()               // unmask → 开始接收中断
│
├── 3. 共享中断验证
│     ├── 遍历 action 链，检查 flags 兼容性
│     └── 检查初级 handler 兼容性
│
├── 4. 启动线程 (如果需要)
│     └── setup_irq_thread()
│
├── 5. 链接 action 到链表
│     ├── 共享: 追加到尾部
│     └── 非共享: 替换链表头
│
├── 6. 注册 proc 文件系统 (/proc/irq/N/)
│
└── 7. 唤醒卡住的线程 (如果被轮询)
```

### 5.2 ONESHOT 关键逻辑

```c
// manage.c — 在 __setup_irq 中
if (new->flags & IRQF_ONESHOT) {
    /*
     * 检查条件:
     * 1. 如果 IRQF_ONESHOT 且非共享: 允许
     * 2. 如果 IRQF_ONESHOT 且共享: 共享的所有 action 必须都提供 thread_fn
     * 3. ONESHOT 不能和没有 thread_fn 的 action 共享
     *
     * 原因: ONESHOT 要求所有 ISR 完成后才 unmask。
     *       如果有 shared action 没有 thread_fn, 中断无法重新使能。
     */
    if (!new->thread_fn) {
        ret = -EINVAL;               // ONESHOT 必须有 thread_fn
        goto out_mask;
    }
}
```

### 5.3 CPU 亲和性设置

```c
// manage.c — __setup_irq 中
/*
 * 对于非 per-CPU 中断，设置 CPU affinity。
 * 默认 affinity mask 由 irq_default_affinity 定义。
 * 用户空间可通过 /proc/irq/N/smp_affinity 修改。
 */
irq_setup_affinity(desc);
```

**Affinity 默认值的决定因素：**

```
irq_setup_affinity(desc)
  ├── 如果 irq_affinity_auto_calc 为真:
  │     └── irq_do_set_affinity() 使用 irq_default_affinity
  └── 如果有 irq_default_affinity:
        └── 写入 desc->irq_common_data.affinity
```

---

## 6. 中断线程化

### 6.1 setup_irq_thread (manage.c)

当 `thread_fn` 不为 NULL 时，`__setup_irq` 会调用 `setup_irq_thread()` 创建内核线程。

```
setup_irq_thread(action, dev_name)
  │
  ├── kthread_create(irq_thread, action, "%s-irq/%d-%s", ...)
  │     创建内核线程，入口函数是 irq_thread
  │
  ├── 设置线程为 SCHED_FIFO 实时调度 + 中等优先级 (irq_default_prio = 50)
  │
  ├── wake_up_process(new->thread)    // 启动线程
  │
  └── 线程进入 irq_thread() 主循环
```

### 6.2 irq_thread — 线程处理循环 (manage.c:1244)

```c
// kernel/irq/manage.c:1244
static int irq_thread(void *data)
{
    struct irqaction *action = data;
    irq_handler_t handler_fn = action->thread_fn;
    struct irq_desc *desc = irq_to_desc(action->irq);
    irqreturn_t ret;

    sched_set_fifo(current);  // 设置为 FIFO 实时调度

    while (!kthread_should_stop()) {
        /*
         * 设置当前状态为 TASK_INTERRUPTIBLE
         * 等待 PRIMARY handler 完成并通过 irq_wake_thread() 唤醒
         */
        set_current_state(TASK_INTERRUPTIBLE);

        if (kthread_should_stop()) {
            __set_current_state(TASK_RUNNING);
            break;
        }

        schedule();  // 睡眠，等待唤醒

        // 被唤醒后: 执行线程处理函数
        ret = handler_fn(action->irq, action->dev_id);

        if (ret == IRQ_HANDLED) {
            // 回调流控处理器的 secondary 完成处理
            // 通常在 handle_level_irq/handle_fasteoi_irq 中 unmask
            irq_finalize_oneshot(desc);
        }
    }
    return 0;
}
```

### 6.3 ONESHOT 生命周期

```
         时间线
─────┬────────┬──────────┬─────────────────┬─────────────────┬──────►
     │        │          │                 │                 │
     │        │          硬件中断到达                               │
     │        │          │                 │                 │
     ▼        ▼          ▼                 ▼                 ▼

  1. Mask      2. PRIMARY   3. Wake up       4. THREADED      5. Unmask
     IRQ          ISR runs       threaded          handler runs      IRQ
     (desc->      action->       handler           action->          (通过
      chip->      handler()      (irq_wake_        thread_fn()       eoi
      irq_mask)                  thread)                             或
                                                                      unmask)

  第1步: 硬件在触发 handler 之前自动 mask (或由 flow handler mask)
  第2步: action->handler() 在硬中断上下文执行 (快速路径, 睡眠无效)
  第3步: PRIMARY handler 返回 IRQ_WAKE_THREAD, 内核唤醒线程
  第4步: action->thread_fn() 在进程上下文执行 (可以睡眠, 使用互斥锁)
  第5步: THREADED handler 完成后 unmask, 准备接收下一个中断
```

---

## 7. 描述符管理

### 7.1 irq_to_desc — 根据 Linux IRQ 号查找描述符 (irqdesc.c:415)

```c
// kernel/irq/irqdesc.c:415
struct irq_desc *irq_to_desc(unsigned int irq)
{
    return radix_tree_lookup(&irq_desc_tree, irq);
}
```

`irq_desc_tree` 是一个 radix tree，存储所有已分配的 `irq_desc`。好处是不需要 `NR_IRQS` 大小的数组，稀疏的 IRQ 空间节省内存。

### 7.2 描述符分配

#### 7.2.1 早期初始化 — early_irq_init (irqdesc.c:550)

```c
// kernel/irq/irqdesc.c:550
int __init early_irq_init(void)
{
    int i, count;
    struct irq_desc *desc;

    count = ARRAY_SIZE(irq_desc);  // 对于非 sparse 配置，是预定义数组
    for (i = 0; i < count; i++) {
        desc = irq_desc + i;
        desc->irq_data.irq = i;
        desc->irq_data.hwirq = i;
        desc->handle_irq = handle_bad_irq;  // 初始化默认 handler
        // ...
        radix_tree_insert(&irq_desc_tree, i, desc);
    }
    return 0;
}
```

在启动早期，内核预分配 `NR_IRQS` 个描述符 (对于配置了 `CONFIG_SPARSE_IRQ=n` 架构)。

#### 7.2.2 动态分配 — alloc_desc (irqdesc.c:433)

```c
// kernel/irq/irqdesc.c:433
static int alloc_descs(unsigned int start, unsigned int cnt, int node,
                       const struct irq_affinity_desc *affinity,
                       struct module *owner)
{
    // ... 分配 irq_desc * descs[cnt]
    // 对每个 desc:
    //   1. 分配 kstat_irqs per-CPU 数组
    //   2. 分配 desc->irq_common_data.affinity
    //   3. 初始化锁、等待队列
    //   4. 设置 handle_irq = handle_bad_irq (占位)
    //   5. 插入 radix tree
    radix_tree_insert(&irq_desc_tree, irq, desc);
}
```

---

## 8. free_irq — 注销中断 (manage.c:2004)

```c
// kernel/irq/manage.c:2004
void free_irq(unsigned int irq, void *dev_id)
{
    struct irq_desc *desc = irq_to_desc(irq);
    struct irqaction *action, **action_ptr;

    // (1) 在 action 链表中查找匹配 dev_id
    action_ptr = &desc->action;
    for (;;) {
        action = *action_ptr;
        if (!action) {
            WARN(1, "Trying to free already-free IRQ %d\n", irq);
            return;
        }
        if (action->dev_id == dev_id)
            break;
        action_ptr = &action->next;
    }

    // (2) 禁用中断 (mask)
    if (irq_settings_can_disable(desc))
        irq_disable(desc);

    // (3) 从链表中移除 action
    *action_ptr = action->next;

    // (4) 如果没有更多 action, 关闭中断
    if (!desc->action) {
        irq_shutdown(desc);   // 调用 chip->irq_shutdown
    }

    // (5) 如果有 IRQF_ONESHOT 线程, 等待线程退出
    if (action->thread) {
        kthread_stop(action->thread);
        /*
         * 等待所有正在执行的 handler 完成
         * 包括: 硬中断 handler (synchronize_irq)
         *      线程 handler (kthread_stop)
         */
    }

    // (6) 等待所有飞行中中断完成
    synchronize_irq(irq);

    // (7) 释放 resources, 删除 proc 文件
    irq_release_resources(desc);
    unregister_handler_proc(irq, action);

    kfree(action);
}
```

### synchronize_irq (manage.c:145)

```c
// kernel/irq/manage.c:145
void synchronize_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);

    if (WARN_ON(!desc))
        return;

    /*
     * 等待 desc->threads_active 变为 0
     * 即等待所有正在本 IRQ 上执行的 handler (硬+线程) 完成
     *
     * 这对 __setup_irq 非共享中断的场景至关重要:
     * 确保旧 handler 已经完成才启动新 handler
     */
    wait_event(desc->wait_for_threads, !atomic_read(&desc->threads_active));
}
```

---

## 9. 完整生命周期流程图

```
═══════════════════════════════════════════════════════════════════════════════
                request_irq → 中断执行 → free_irq 全流程
═══════════════════════════════════════════════════════════════════════════════

【阶段1: 注册】
─────────────
  模块加载
    │
    ▼
  request_irq(irq, handler, flags, name, dev)
    │
    ▼
  request_threaded_irq(irq, handler, NULL, flags, name, dev)
    │
    ├── irq_to_desc(irq)  ──► 在 irq_desc_tree 中查找 irq_desc
    │
    ├── kzalloc(sizeof(struct irqaction))
    │
    └── __setup_irq(irq, desc, action)
          │
          ├── 分配 kstat_irqs per-CPU 数组 (如果是首次安装)
          ├── irq_request_resources(desc)    // 请求硬件资源
          ├── irq_setup_affinity(desc)       // 设置 CPU affinity
          ├── irq_activate(desc)             // activate IRQ
          ├── irq_startup(desc)              // chip->irq_startup / unmask
          │     │
          │     └── desc->chip->irq_unmask(d)  → 硬件开始接收中断
          │
          ├── setup_irq_thread(action)       // 如果有 thread_fn
          │     │
          │     └── kthread_create(irq_thread, ...) → 创建内核线程
          │
          ├── 链接 action 到 desc->action 链表
          │
          └── register_handler_proc(irq, action)  // /proc/irq/N/
                                                        │
【阶段2: 中断触发执行】                                               │
─────────────────────                                               │
  硬件触发中断                                                       │
    │                                                               │
    ▼                                                               │
  CPU 架构入口 (x86: common_interrupt → do_IRQ)                     │
    │                                                               │
    ▼                                                               │
  generic_handle_arch_irq()                                         │
    │                                                               │
    ▼                                                               │
  generic_handle_irq_desc(desc)                                     │
    │                                                               │
    ▼                                                               │
  desc->handle_irq(desc)     // 流控处理器 (level/edge/fasteoi)     │
    │                                                               │
    ▼                                                               │
  handle_irq_event(desc)                                            │
    │                                                               │
    ▼                                                               │
  __handle_irq_event_percpu(desc)                                   │
    │                                                               │
    ├── 遍历 action 链表                                             │
    │     ├── action->handler(action->irq, action->dev_id)          │
    │     │     └── ISR 在硬中断上下文执行                            │
    │     │          返回: IRQ_HANDLED / IRQ_NONE / IRQ_WAKE_THREAD │
    │     │                                                        │
    │     └── do { ... } while (action)                             │
    │                                                               │
    └── add_interrupt_randomness()    // 贡献熵到 RNG               │
                                                                      │
【阶段3: 线程化处理 (如果 IRQF_ONESHOT)】                               │
─────────────────────────────────────────                               │
    │                                                               │
    ▼                                                               │
  primary handler 返回 IRQ_WAKE_THREAD                              │
    │                                                               │
    ▼                                                               │
  irq_wake_thread(desc, action)                                     │
    │                                                               │
    ├── wake_up_process(action->thread)  // 唤醒内核线程              │
    │                                                               │
    ▼                                                               │
  irq_thread() 循环                                                  │
    │                                                               │
    ├── set_current_state(TASK_INTERRUPTIBLE)                        │
    ├── schedule()  →  睡眠等待...                                   │
    ├── 被唤醒: thread_fn(irq, dev_id)                              │
    │      │                                                        │
    │      └── 可以睡眠! 使用 mutex_lock, msleep, kmalloc(GFP_KERNEL)│
    │                                                               │
    └── irq_finalize_oneshot(desc)  // unmask EOI                   │
                                                                      │
【阶段4: 注销】                                                       │
─────────────                                                        │
  模块卸载                                                           │
    │                                                               │
    ▼                                                               │
  free_irq(irq, dev_id)                                             │
    │                                                               │
    ├── 在 desc->action 链表中查找匹配 dev_id 的 action               │
    │                                                               │
    ├── irq_disable(desc)              // mask 中断                  │
    │                                                               │
    ├── 从链表中移除 action                                           │
    │                                                               │
    ├── 如果没有更多 action:                                          │
    │     └── irq_shutdown(desc)       // 关闭中断硬件                │
    │                                                               │
    ├── 如果有 thread:                                                │
    │     ├── kthread_stop(action->thread)  // 停止并等待线程退出     │
    │     └── synchronize_irq(irq)         // 等待所有 handler 完成  │
    │                                                               │
    ├── irq_release_resources(desc)     // 释放硬件资源               │
    │                                                               │
    ├── unregister_handler_proc(irq, action)  // 删除 /proc 条目    │
    │                                                               │
    └── kfree(action)                  // 释放 memory               │
                                                                      │
═══════════════════════════════════════════════════════════════════════════════
```

---

## 10. 重要辅助函数

### 10.1 disable_irq / enable_irq

```c
// manage.c
void disable_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    disable_irq_nosync(irq);
    synchronize_irq(irq);  // 等待当前进行中的 ISR 完成
}

void disable_irq_nosync(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    // 增加 depth 计数，调用 chip->irq_disable 或 irq_mask
    __disable_irq(desc);
}

void enable_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    // 减少 depth, 当 depth==0 时调用 chip->irq_enable 或 irq_unmask
    __enable_irq(desc);
}
```

**depth 嵌套机制：**

```
  disable_irq()     → depth = 1, 发送 mask
  disable_irq()     → depth = 2, 已 mask 无需再 mask
  enable_irq()      → depth = 1, 仍未 0, 不 unmask
  enable_irq()      → depth = 0, unmask!
```

### 10.2 irq_set_affinity

```c
// manage.c
int irq_set_affinity(unsigned int irq, const struct cpumask *mask)
{
    struct irq_desc *desc = irq_to_desc(irq);
    return irq_do_set_affinity(&desc->irq_data, mask, false);
}
```

通过 `/proc/irq/N/smp_affinity` 或低层 `irq_set_affinity()` API，用户可将某个中断绑定到特定 CPU。

---

## 11. 与后续文章的衔接

- **第一篇 (本文)**：`irq_desc` 结构体, `request_irq` → `__setup_irq`, `irq_thread`, `free_irq`
- **第二篇**：`irq_chip` 方法表, `handle_level_irq` / `handle_edge_irq` / `handle_fasteoi_irq` 如何操作硬件寄存器
- **第三篇**：`note_interrupt()` 虚假中断检测算法，`/proc/interrupts` 数据来源
- **第四篇**：`__do_softirq()` 软中断机制, tasklet 原理
- **第五篇**：CPU 热插拔对 IRQ 的影响, MSI/MSI-X 消息接口, 电源管理

---

## 12. 源码调试提示

```bash
# 查看所有中断描述符状态
cat /proc/interrupts

# 查看某中断详细信息
cat /proc/irq/32/smp_affinity
cat /proc/irq/32/spurious
cat /proc/irq/32/affinity_hint

# 使用 tracepoint 追踪
echo 1 > /sys/kernel/debug/tracing/events/irq/irq_handler_entry/enable
echo 1 > /sys/kernel/debug/tracing/events/irq/irq_handler_exit/enable
cat /sys/kernel/debug/tracing/trace

# 检查线程化中断
ps aux | grep "irq/"
```

---

## 下一篇文章

[第二篇：IRQ Chip 与 Flow Handlers — Level/Edge/FASTEOI 三种处理方式](./02-chip-flow.md)
