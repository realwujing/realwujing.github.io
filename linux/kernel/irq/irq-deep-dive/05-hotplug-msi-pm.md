# 第五篇：CPU Hotplug、MSI 与电源管理 — IRQ 的跨 CPU 迁移与睡眠唤醒

> 源码：kernel/irq/cpuhotplug.c kernel/irq/msi.c kernel/irq/pm.c | 头文件：include/linux/irqflags.h

系列目录：[IRQ 内核源码深度解析](./README.md)

---

## 1. 概述

前四篇假设了稳态运行。现实中系统会遇到：

- **CPU 热插拔**：CPU offline 时挂在上面的 IRQ 怎么办？
- **MSI 中断**：现代 PCIe 设备使用 MSI/MSI-X 替代 INTx，内核如何描述？
- **系统睡眠/唤醒**：suspend to RAM 期间中断如何处理？唤醒后如何恢复？

本文聚焦这三个高级话题。

---

## 2. 中断上下文开关 (irqflags.h:200-212, 237-238)

```c
// include/linux/irqflags.h:200-204 (CONFIG_TRACE_IRQFLAGS=y)
#define local_irq_enable()              \
    do {                                \
        trace_hardirqs_on();            \
        raw_local_irq_enable();         \
    } while (0)

// include/linux/irqflags.h:206-212 (CONFIG_TRACE_IRQFLAGS=y)
#define local_irq_disable()             \
    do {                                \
        bool was_disabled = raw_irqs_disabled(); \
        raw_local_irq_disable();        \
        if (!was_disabled)              \
            trace_hardirqs_off();       \
    } while (0)

// include/linux/irqflags.h:237-238 (CONFIG_TRACE_IRQFLAGS=n)
#define local_irq_enable()  do { raw_local_irq_enable(); } while (0)
#define local_irq_disable() do { raw_local_irq_disable(); } while (0)
```

`raw_local_irq_disable → arch_local_irq_disable`: x86 → `cli`, arm64 → `msr DAIFSet`.

---

## 3. CPU Hotplug 与 IRQ 迁移

### 3.1 问题场景

```
   CPU2 offline:
   ┌────────┐     ┌────────┐     ┌────────┐     ┌────────┐
   │ CPU0   │     │ CPU1   │     │ CPU2   │     │ CPU3   │
   │ IRQ 32 │     │ IRQ 33 │     │ 离线 ↓  │     │ IRQ 35 │
   │ IRQ 36 │     │ IRQ 37 │     │ IRQ 34 │     │ IRQ 39 │
   └────────┘     └────────┘     └────┬───┘     └────────┘
                              irq_migrate_all
                              _off_this_cpu()
                              ┌──────┼──────┐
                              ▼      ▼      ▼
                           CPU0   CPU1   CPU3
```

### 3.2 irq_migrate_all_off_this_cpu (cpuhotplug.c:171)

```c
// kernel/irq/cpuhotplug.c:171
void irq_migrate_all_off_this_cpu(void)
{
    struct irq_desc *desc;
    unsigned int irq;

    for_each_active_irq(irq) {          // 遍历所有活跃 IRQ
        bool affinity_broken;

        desc = irq_to_desc(irq);
        scoped_guard(raw_spinlock, &desc->lock) {
            affinity_broken = migrate_one_irq(desc);
            if (affinity_broken && desc->affinity_notify)
                irq_affinity_schedule_notify_work(desc);
        }
        if (affinity_broken)
            pr_debug_ratelimited("IRQ %u: no longer affine to CPU%u\n",
                                irq, smp_processor_id());
    }
}
```

### 3.3 migrate_one_irq — 核心迁移逻辑 (cpuhotplug.c:53)

```c
// kernel/irq/cpuhotplug.c:53
static bool migrate_one_irq(struct irq_desc *desc)
{
    struct irq_data *d = irq_desc_get_irq_data(desc);
    struct irq_chip *chip = irq_data_get_irq_chip(d);
    bool maskchip = !irq_can_move_pcntxt(d) && !irqd_irq_masked(d);
    const struct cpumask *affinity;
    bool brokeaff = false;
    int err;

    // 1. chip 不存在或无 set_affinity → 无法迁移
    if (!chip || !chip->irq_set_affinity) {
        pr_debug("IRQ %u: Unable to migrate away\n", d->irq);
        return false;
    }

    // 2. 完成 pending 中断移动清理
    irq_force_complete_move(desc);

    // 3. per-CPU / 未启动 / 不需要修复 → 跳过
    if (irqd_is_per_cpu(d) || !irqd_is_started(d) ||
        !irq_needs_fixup(d)) {
        irq_fixup_move_pending(desc, false);
        return false;
    }

    // 4. 确定目标 CPU mask
    if (irq_fixup_move_pending(desc, true))
        affinity = irq_desc_get_pending_mask(desc);
    else
        affinity = irq_data_get_affinity_mask(d);

    // 5. 不能进程迁移的 IRQ 先 mask
    if (maskchip && chip->irq_mask)
        chip->irq_mask(d);

    // 6. affinity 与在线 CPU 无交集
    if (!cpumask_intersects(affinity, cpu_online_mask)) {
        if (irqd_affinity_is_managed(d)) {
            irqd_set_managed_shutdown(d);
            irq_shutdown_and_deactivate(desc);
            return false;
        }
        affinity = cpu_online_mask;
        brokeaff = true;
    }

    // 7. 执行迁移 (不强制，保留 offline CPU 屏蔽)
    err = irq_do_set_affinity(d, affinity, false);

    // 8. ENOSPC → 放宽到全部在线 CPU 重试
    if (err == -ENOSPC && !irqd_affinity_is_managed(d) &&
        affinity != cpu_online_mask) {
        affinity = cpu_online_mask;
        brokeaff = true;
        err = irq_do_set_affinity(d, affinity, false);
    }

    if (err) {
        pr_warn_ratelimited("IRQ%u: set affinity failed(%d).\n",
                           d->irq, err);
        brokeaff = false;
    }

    if (maskchip && chip->irq_unmask)
        chip->irq_unmask(d);

    return brokeaff;
}
```

决策树：

```
migrate_one_irq(desc)
 ├─ chip 不存在? → return false
 ├─ 完成 pending move cleanup
 ├─ per-CPU / !started / 不需要修复? → return false
 ├─ 确定目标 affinity mask
 ├─ 目标 ∩ online_CPU == ∅?
 │    ├─ Managed → shutdown, return false
 │    └─ 普通 → affinity=cpu_online_mask, brokeaff=true
 ├─ irq_do_set_affinity()
 │    └─ ENOSPC? → 放宽到 cpu_online_mask 重试
 └─ return brokeaff
```

### 3.4 Managed 中断

Managed 中断 (`IRQD_AFFINITY_MANAGED`) 由内核管理 affinity。典型场景：PCIe MSI/MSI-X 多队列网卡、IOMMU 设备中断。

```
Managed 中断保证:
  1. 内核自动选择目标 CPU
  2. CPU offline: 已启动 → 迁移; 已 shutdown → 等待 CPU online
  3. CPU online: irq_affinity_online_cpu() 恢复被 shutdown 的中断
```

### 3.5 CPU 上线：irq_affinity_online_cpu (cpuhotplug.c:233)

```c
// kernel/irq/cpuhotplug.c:233
int irq_affinity_online_cpu(unsigned int cpu)
{
    struct irq_desc *desc;
    unsigned int irq;

    irq_lock_sparse();
    for_each_active_irq(irq) {
        desc = irq_to_desc(irq);
        scoped_guard(raw_spinlock_irq, &desc->lock)
            irq_restore_affinity_of_irq(desc, cpu);
    }
    irq_unlock_sparse();
    return 0;
}
```

`irq_restore_affinity_of_irq` (cpuhotplug.c:206)：仅处理 managed 中断且新 CPU 在 affinity 范围内；如果之前是 managed_shutdown → 重新启动；执行 `irq_set_affinity_locked` 迁移。

---

## 4. MSI 中断基础设施

### 4.1 MSI vs INTx

```
传统 INTx:
  PCI 设备 → IO-APIC → CPU LAPIC
  共享物理线、4 条线限制、需轮询 ISR

MSI:
  PCIe 设备 ──内存写入(TLP)──→ CPU LAPIC
  无需 IO-APIC 中转、每向量独立、MSI-X 最多 2048 向量、更低的延迟
```

### 4.2 核心数据结构

#### struct msi_device_data (msi.c:32)

```c
// kernel/irq/msi.c:32
struct msi_device_data {
    unsigned long           properties;
    struct mutex            mutex;
    struct msi_dev_domain   __domains[MSI_MAX_DEVICE_IRQDOMAINS];
    unsigned long           __iter_idx;
};
```

每个 MSI 设备通过 `dev->msi.data` 维护。`__domains[]` 支持一个设备与多个 MSI 域关联。

#### struct msi_ctrl (msi.c:47)

```c
// kernel/irq/msi.c:47
struct msi_ctrl {
    unsigned int            domid;   // MSI 域 ID
    unsigned int            first;   // 硬件起始槽位
    unsigned int            last;    // 硬件结束槽位
    unsigned int            nirqs;   // Linux IRQ 数量 (可能 > 范围: PCI/multi-MSI)
};
```

#### msi_alloc_desc (msi.c:76)

```c
// kernel/irq/msi.c:76
static struct msi_desc *msi_alloc_desc(struct device *dev, int nvec,
                                       const struct irq_affinity_desc *affinity)
{
    struct msi_desc *desc = kzalloc_obj(*desc);
    if (!desc) return NULL;

    desc->dev = dev;
    desc->nvec_used = nvec;
    if (affinity) {
        desc->affinity = kmemdup_array(affinity, nvec,
                                      sizeof(*desc->affinity), GFP_KERNEL);
        if (!desc->affinity) { kfree(desc); return NULL; }
    }
    return desc;
}
```

#### msi_insert_desc (msi.c:102)

描述符存储在 xarray 中，支持自动分配和指定索引两种模式。

### 4.3 MSI 整体架构

```
pci_alloc_irq_vectors()       ← PCI 层 API
       │
       ▼
msi_domain_alloc_irqs()       ← MSI 域层
       │
   ┌───┼───┐
   ▼       ▼               ▼
msi_alloc  irq_domain     irq_create
_desc()    _alloc_irqs()  _fwspec_mapping()
   │                       │
   ▼                       ▼
                    struct irq_desc
                    irq_common_data.msi_desc → struct msi_desc
```

---

## 5. 中断电源管理 (IRQ PM)

### 5.1 关键标志

| 标志 | 来源 | 含义 |
|------|------|------|
| `IRQF_NO_SUSPEND` | pm.c:39 | suspend 期间不禁用 |
| `IRQF_COND_SUSPEND` | pm.c:41 | 无其他用户时才 suspend |
| `IRQF_FORCE_RESUME` | pm.c:33 | resume 时强制恢复 |
| `IRQF_EARLY_RESUME` | pm.c:59 附近 | 设备 resume 前就在 syscore 阶段恢复 |

### 5.2 状态标志

| 标志 | 含义 |
|------|------|
| `IRQS_SUSPENDED` (pm.c:96) | 描述符已挂起 |
| `IRQD_WAKEUP_ARMED` (pm.c:75) | 此 IRQ 被设定为唤醒源 |
| `IRQD_IRQ_ENABLED_ON_SUSPEND` (pm.c:85) | suspend 期间被启用的唤醒中断 |

### 5.3 suspend_device_irqs (pm.c:126)

```c
// kernel/irq/pm.c:126
void suspend_device_irqs(void)
{
    struct irq_desc *desc;
    int irq;

    for_each_irq_desc(irq, desc) {
        bool sync;
        if (irq_settings_is_nested_thread(desc))
            continue;
        scoped_guard(raw_spinlock_irqsave, &desc->lock)
            sync = suspend_device_irq(desc);
        if (sync)
            synchronize_irq(irq);
    }
}
```

单 IRQ 处理 `suspend_device_irq` (pm.c:65)：

```c
// kernel/irq/pm.c:65 (简化)
static bool suspend_device_irq(struct irq_desc *desc)
{
    // 跳过: 无 action、级联中断、no_suspend_depth > 0
    if (!desc->action || irq_desc_is_chained(desc) ||
        desc->no_suspend_depth)
        return false;

    // ★ 唤醒源处理
    if (irqd_is_wakeup_set(irqd)) {
        irqd_set(irqd, IRQD_WAKEUP_ARMED);
        if ((chipflags & IRQCHIP_ENABLE_WAKEUP_ON_SUSPEND) &&
            irqd_irq_disabled(irqd)) {
            __enable_irq(desc);              // 保持唤醒源活跃
            irqd_set(irqd, IRQD_IRQ_ENABLED_ON_SUSPEND);
        }
        return true;   // 需要 synchronize_irq
    }

    // ★ 非唤醒源处理
    desc->istate |= IRQS_SUSPENDED;
    __disable_irq(desc);
    if (chipflags & IRQCHIP_MASK_ON_SUSPEND)
        mask_irq(desc);   // chip 级 mask
    return true;
}
```

### 5.4 唤醒处理：irq_pm_handle_wakeup (pm.c:16)

```c
// kernel/irq/pm.c:16
void irq_pm_handle_wakeup(struct irq_desc *desc)
{
    irqd_clear(&desc->irq_data, IRQD_WAKEUP_ARMED);
    desc->istate |= IRQS_SUSPENDED | IRQS_PENDING;
    desc->depth++;
    irq_disable(desc);
    pm_system_irq_wakeup(irq_desc_get_irq(desc));   // 通知 PM 核心
}
```

### 5.5 resume_device_irqs (pm.c:246)

```c
// kernel/irq/pm.c:246
void resume_device_irqs(void)
{
    resume_irqs(false);   // false = 非 early resume
}
```

恢复分两个阶段：
1. **Early resume** (syscore): `irq_pm_syscore_resume` → `resume_irqs(true)` — 仅恢复 `IRQF_EARLY_RESUME` 中断
2. **Late resume**: `resume_device_irqs` → `resume_irqs(false)` — 恢复其余中断

单 IRQ 恢复 (pm.c:144)：

```c
// kernel/irq/pm.c:144
static void resume_irq(struct irq_desc *desc)
{
    irqd_clear(irqd, IRQD_WAKEUP_ARMED);

    if (irqd_is_enabled_on_suspend(irqd)) {
        __disable_irq(desc);   // suspend 期间启用的 → 恢复禁用
        irqd_clear(irqd, IRQD_IRQ_ENABLED_ON_SUSPEND);
    }

    if (desc->istate & IRQS_SUSPENDED)
        goto resume;

    if (!desc->force_resume_depth)   // IRQF_FORCE_RESUME
        return;

    desc->depth++;
    irq_state_set_disabled(desc);
    irq_state_set_masked(desc);
resume:
    desc->istate &= ~IRQS_SUSPENDED;
    __enable_irq(desc);
}
```

### 5.6 rearm_wake_irq (pm.c:198)

```c
// kernel/irq/pm.c:198
void rearm_wake_irq(unsigned int irq)
{
    scoped_irqdesc_get_and_buslock(irq, IRQ_GET_DESC_CHECK_GLOBAL) {
        struct irq_desc *desc = scoped_irqdesc;

        if (!(desc->istate & IRQS_SUSPENDED) ||
            !irqd_is_wakeup_set(&desc->irq_data))
            return;

        desc->istate &= ~IRQS_SUSPENDED;
        irqd_set(&desc->irq_data, IRQD_WAKEUP_ARMED);
        __enable_irq(desc);
    }
}
```

下次 suspend 前重新武装唤醒 IRQ。

---

## 6. 完整 PM 流程图

### 6.1 Suspend 流程

```
系统 Suspend
      │
      ▼
suspend_device_irqs()     pm.c:126
      │
  ┌───┼───┐
  ▼   ▼   ▼
NO_    Wakeup   普通 IRQ
SUSPEND IRQ
  │      │         │
 跳过   ARM        IRQS_SUSPENDED
      保持unmask   __disable_irq()
  │      │         │
  │      │    mask_irq()
  │      │    (IRQCHIP_MASK_ON_SUSPEND)
  │      │         │
  └──────┴────┬────┘
              │
              ▼
      synchronize_irq()
              │
              ▼
      系统进入 Suspend
(仅 wakeup IRQ 仍在监听)
              │
         唤醒事件 (RTC/WoL等)
              │
              ▼
      wakeup IRQ handler 触发
      → irq_pm_handle_wakeup()      pm.c:16
      → pm_system_irq_wakeup()
```

### 6.2 Resume 流程

```
系统 Resume
      │
      ▼
irq_pm_syscore_resume()   syscore 阶段，最早执行
  → resume_irqs(true)     仅恢复 IRQF_EARLY_RESUME 中断
      │
      ▼
设备驱动 resume
      │
      ▼
resume_device_irqs()      pm.c:246
  → resume_irqs(false)    恢复全部非 early 中断
      │
  ┌───┼───┐
  ▼   ▼   ▼
 WAKEUP_  IRQS_      FORCE_
 ARMED    SUSPENDED  RESUME
  │         │          │
 清除ARMED 清除状态    强制启用
 如有ON_   __enable
 SUSPEND  _irq()
 则disable  │
  │         │          │
  └─────────┴────┬─────┘
                 │
                 ▼
          所有 IRQ 恢复正常
```

---

## 7. PM 状态变迁表

```
┌──────────────────────┬────────────────────────────┬──────────────────────────┐
│ 中断类型              │ Suspend 操作                │ Resume 操作               │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ IRQF_NO_SUSPEND      │ 跳过，保持活跃               │ 跳过，仍然活跃             │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ Wakeup + DISABLED    │ 启用 + ARM + ON_SUSPEND    │ 禁用 + CLEAR              │
│ (+ENABLE_ON_SUSPEND) │                            │                           │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ Wakeup + ENABLED     │ ARM (保持启用)              │ 清除 ARM                  │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ 普通 IRQ              │ SUSPENDED + disable        │ 清除 SUSPENDED + enable   │
│                      │ (+ MASK_ON_SUSPEND→mask)   │                           │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ IRQF_FORCE_RESUME    │ SUSPENDED + disable         │ 强制启用                   │
├──────────────────────┼────────────────────────────┼──────────────────────────┤
│ IRQF_EARLY_RESUME    │ (同上)                      │ Early phase (syscore) 恢复 │
└──────────────────────┴────────────────────────────┴──────────────────────────┘
```

---

## 8. 全景图

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        Linux IRQ 子系统全景                                    │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  request_irq ─► irq_desc ─► handle_irq_event ─► irq_exit ─► __do_softirq    │
│  第1篇             第1篇         第2篇               第4篇        第4篇        │
│                                                                              │
│  note_interrupt ─► /proc/interrupts ─► tasklet_action ─► ksoftirqd          │
│  第3篇              第3篇                 第4篇             第4篇             │
│                                                                              │
│  migrate_one_irq ─► managed IRQ ─► msi_alloc_desc ─► suspend_device_irqs    │
│  第5篇               第5篇           第5篇               第5篇                 │
│                                                                              │
│  ┌────────────────────┐                                                      │
│  │  同步子系统         │  spin_lock(_irqsave) / local_bh_disable / RCU       │
│  │  与中断紧密耦合      │  → 理解中断上下文后才能正确使用锁                     │
│  └────────────────────┘                                                      │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. 实际场景示例

```bash
# CPU offline 跟踪
echo 0 > /sys/devices/system/cpu/cpu2/online
# 内核: take_cpu_down → fixup_irqs → irq_migrate_all_off_this_cpu
cat /proc/interrupts | awk '{print $1,$3,$4,$5,$6}'  # 查看 CPU2 列是否归零

# Suspend/Resume 跟踪
echo 1 > /sys/kernel/debug/tracing/events/irq/irq_handler_entry/enable
echo mem > /sys/power/state
# 唤醒后: 检查 trace 中的 handler 调用链

# MSI 信息
lspci -vvv | grep -A 20 "MSI"
cat /proc/interrupts | grep "PCI-MSI"
```

---

## 10. 系列结语

五篇文章完整覆盖 Linux 内核中断子系统的核心实现：

| 篇 | 主题 | 核心文件 | 关键知识点 |
|:-:|------|---------|-----------|
| 1 | IRQ 描述符与 request_irq | manage.c, irqdesc.c | irq_desc 结构、ISR 注册生命周期、ONESHOT |
| 2 | Flow Handlers | chip.c, handle.c | level/edge/fasteoi 三种流控模式、EOI 顺序 |
| 3 | /proc 与虚假中断 | proc.c, spurious.c | show_interrupts 统计、note_interrupt 检测 |
| 4 | SoftIRQ 与 Tasklet | softirq.c | pending 位图、MAX_RESTART 机制、ksoftirqd |
| 5 | CPU Hotplug/MSI/PM | cpuhotplug.c, msi.c, pm.c | IRQ 迁移、MSI 架构、suspend/resume |

### 知识图谱

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    内核中断 + 同步 知识体系                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌────────────────────┐                                                 │
│  │  IRQ 中断子系统      │  ← 本系列五篇                                 │
│  │  ├─ 描述符 & 注册    │                                                │
│  │  ├─ Flow Handler    │                                               │
│  │  ├─ 统计 & 检测     │                                                │
│  │  ├─ SoftIRQ/Tasklet │                                               │
│  │  └─ Hotplug/MSI/PM  │                                               │
│  └─────────┬──────────┘                                                │
│            │ 配合使用                                                    │
│            ▼                                                            │
│  ┌────────────────────┐                                                 │
│  │  内核同步机制        │  spin_lock(&_irqsave) / rwlock / RCU / mutex  │
│  │                     │  理解中断上下文才能正确使用锁                     │
│  └────────────────────┘                                                 │
└─────────────────────────────────────────────────────────────────────────┘
```

### 推荐下一步

1. **内核定时器** (`kernel/time/`): HRTIMER_SOFTIRQ 如何驱动高精度定时器
2. **NAPI 网络子系统** (`net/core/dev.c`): NET_RX_SOFTIRQ 如何实现 NAPI 轮询
3. **RCU 回调** (`kernel/rcu/`): RCU_SOFTIRQ 如何处理 RCU 回调
4. **内核锁机制** (`kernel/locking/`): spin_lock 中断变体、local_bh_disable 与锁的配合
5. **中断线程化** (`kernel/irq/manage.c`): IRQF_ONESHOT / force_irqthreads 实现

---
本系列所有源码引用基于 **Linux kernel v7.1-rc5-26 (eb3f4b7426cf)**，行号以该版本为基准。
