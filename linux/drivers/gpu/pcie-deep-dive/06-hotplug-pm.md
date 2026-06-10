# 第六篇：热插拔与电源管理

> 源码：`drivers/pci/hotplug/pciehp_core.c` (384 lines) | `drivers/pci/hotplug/pciehp_hpc.c` (1124 lines) | `drivers/pci/pcie/aspm.c` (1786 lines)
> 头文件：`drivers/pci/hotplug/pciehp.h` | `include/linux/pci.h` | `include/uapi/linux/pci_regs.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. PCIe 热插拔的三种实现

Linux 内核支持三种热插拔方式，对应不同的固件/硬件模型：

| 热插拔类型 | 源码位置 | 控制方式 | 使用场景 |
|-----------|----------|---------|---------|
| **ACPI Hotplug** | `drivers/pci/hotplug/acpi_pcihp.c` <br> `drivers/pci/hotplug/acpiphp_glue.c` | ACPI 固件 (_OST, _RMV 方法) | 服务器/笔记本，BIOS 控制 |
| **Native PCIe Hotplug** | `drivers/pci/hotplug/pciehp_core.c` <br> `drivers/pci/hotplug/pciehp_hpc.c` | PCIe Slot Capability 寄存器 | 原生 PCIe 插槽，OS 直接控制 |
| **SHPC Hotplug** | `drivers/pci/hotplug/shpchp_core.c` | Standard Hot-Plug Controller | 传统 PCI/PCI-X 遗留 |

本文聚焦 **Native PCIe Hotplug** (`pciehp`)，这是现代系统中 GPU/NVMe SSD 热插拔的主要实现方式。

---

## 2. pciehp 核心架构

pciehp 作为 PCIe Port 服务驱动运行，注册在 portdrv 框架下 (`pciehp_core.c:353-373`)：

```c
// drivers/pci/hotplug/pciehp_core.c:353-373
static struct pcie_port_service_driver hpdriver_portdrv = {
    .name       = "pciehp",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_HP,

    .probe      = pciehp_probe,
    .remove     = pciehp_remove,

#ifdef  CONFIG_PM
#ifdef  CONFIG_PM_SLEEP
    .suspend    = pciehp_suspend,
    .resume_noirq   = pciehp_resume_noirq,
    .resume     = pciehp_resume,
#endif
    .runtime_suspend = pciehp_runtime_suspend,
    .runtime_resume  = pciehp_runtime_resume,
#endif  /* PM */

    .slot_reset = pciehp_slot_reset,
};

int __init pcie_hp_init(void)
{
    int retval = 0;

    retval = pcie_port_service_register(&hpdriver_portdrv);
    pr_debug("pcie_port_service_register = %d\n", retval);
    if (retval)
        pr_debug("Failure to register service\n");

    return retval;
}
```

关键点：`slot_reset` 回调 — pciehp 参与了 AER/DPC 错误恢复链 (见第五篇)，在 slot reset 后重新初始化热插拔控制器。

### 2.1 init_slot — 建立 hotplug_slot_ops (`pciehp_core.c:51-89`)

```c
// drivers/pci/hotplug/pciehp_core.c:51-89
static int init_slot(struct controller *ctrl)
{
    struct hotplug_slot_ops *ops;
    char name[SLOT_NAME_SIZE];
    int retval;

    /* Setup hotplug slot ops */
    ops = kzalloc_obj(*ops);
    if (!ops)
        return -ENOMEM;

    ops->enable_slot         = pciehp_sysfs_enable_slot;
    ops->disable_slot        = pciehp_sysfs_disable_slot;
    ops->get_power_status    = get_power_status;
    ops->get_adapter_status  = get_adapter_status;
    ops->reset_slot          = pciehp_reset_slot;
    if (MRL_SENS(ctrl))
        ops->get_latch_status = get_latch_status;
    if (ATTN_LED(ctrl)) {
        ops->get_attention_status = pciehp_get_attention_status;
        ops->set_attention_status = set_attention_status;
    } else if (ctrl->pcie->port->hotplug_user_indicators) {
        ops->get_attention_status = pciehp_get_raw_indicator_status;
        ops->set_attention_status = pciehp_set_raw_indicator_status;
    }

    /* register this slot with the hotplug pci core */
    ctrl->hotplug_slot.ops = ops;
    snprintf(name, SLOT_NAME_SIZE, "%u", PSN(ctrl));

    retval = pci_hp_initialize(&ctrl->hotplug_slot,
                   ctrl->pcie->port->subordinate,
                   PCI_SLOT_ALL_DEVICES, name);
    if (retval) {
        ctrl_err(ctrl, "pci_hp_initialize failed: error %d\n", retval);
        kfree(ops);
    }
    return retval;
}
```

热插拔 sysfs 接口操作表：

| 操作 | 函数 | 用户态触发方式 |
|------|------|---------------|
| 上电/启用插槽 | `pciehp_sysfs_enable_slot` | `echo 1 > /sys/bus/pci/slots/X/power` |
| 断电/禁用插槽 | `pciehp_sysfs_disable_slot` | `echo 0 > /sys/bus/pci/slots/X/power` |
| 查询电源状态 | `get_power_status` | `cat /sys/bus/pci/slots/X/power` |
| 查询设备在位 | `get_adapter_status` | `cat /sys/bus/pci/slots/X/attention` |
| 重置插槽 | `pciehp_reset_slot` | 内核内部调用 / 错误恢复 |
| 注意指示灯 | `set_attention_status` | 通过 sysfs/setpci 控制 |

### 2.2 pciehp_probe — 驱动加载入口 (`pciehp_core.c:185-243`)

```c
// drivers/pci/hotplug/pciehp_core.c:185-240
static int pciehp_probe(struct pcie_device *dev)
{
    int rc;
    struct controller *ctrl;

    if (dev->service != PCIE_PORT_SERVICE_HP)
        return -ENODEV;

    if (!dev->port->subordinate) {
        pci_err(dev->port,
            "Hotplug bridge without secondary bus, ignoring\n");
        return -ENODEV;
    }

    ctrl = pcie_init(dev);                    // 初始化 controller 数据结构
    if (!ctrl) {
        pci_err(dev->port, "Controller initialization failed\n");
        return -ENODEV;
    }
    set_service_data(dev, ctrl);

    rc = init_slot(ctrl);                     // 创建 hotplug_slot_ops
    if (rc) {
        if (rc == -EBUSY)
            ctrl_warn(ctrl, "Slot already registered by another hotplug driver\n");
        else
            ctrl_err(ctrl, "Slot initialization failed (%d)\n", rc);
        goto err_out_release_ctlr;
    }

    rc = pcie_init_notification(ctrl);        // 注册中断处理
    if (rc) {
        ctrl_err(ctrl, "Notification initialization failed (%d)\n", rc);
        goto err_out_free_ctrl_slot;
    }

    /* Publish to user space */
    rc = pci_hp_add(&ctrl->hotplug_slot);     // 导出到 sysfs
    if (rc) {
        ctrl_err(ctrl, "Publication to user space failed (%d)\n", rc);
        goto err_out_shutdown_notification;
    }

    pciehp_check_presence(ctrl);              // 检查当前插槽状态
    return 0;

err_out_shutdown_notification:
    pcie_shutdown_notification(ctrl);
err_out_free_ctrl_slot:
    cleanup_slot(ctrl);
err_out_release_ctlr:
    pciehp_release_ctrl(ctrl);
    return -ENODEV;
}
```

probe 的四个阶段：
1. **pcie_init** — 从 PCIe 配置空间读取 Slot Capabilities（插槽支持能力）
2. **init_slot** — 构建 hotplug_slot_ops 回调表，注册到 hotplug 核心
3. **pcie_init_notification** — 注册中断处理程序 (ISR/IST)
4. **pci_hp_add** — 创建 sysfs 节点 (`/sys/bus/pci/slots/`)

---

## 3. pciehp 模块参数

```c
// drivers/pci/hotplug/pciehp_core.c:34-44
bool pciehp_poll_mode;
int pciehp_poll_time;

module_param(pciehp_poll_mode, bool, 0644);
module_param(pciehp_poll_time, int, 0644);
MODULE_PARM_DESC(pciehp_poll_mode, "Using polling mechanism for hot-plug events or not");
MODULE_PARM_DESC(pciehp_poll_time, "Polling mechanism frequency, in seconds");
```

- **pciehp_poll_mode**：当硬件中断不可用时（如 INTx 路由问题），可降级为轮询模式
- **pciehp_poll_time**：轮询间隔（默认 2 秒，有效范围 1-60 秒）
- 轮询线程实现在 `pciehp_hpc.c:800-819`，每 `pciehp_poll_time` 秒手动调用一次 `pciehp_isr(IRQ_NOTCONNECTED, ctrl)`

---

## 4. Slot Control 寄存器与事件源

PCIe 规范的 Slot Control/Status 寄存器定义了四类主要事件：

```
PCI_EXP_SLTCTL / PCI_EXP_SLTSTA 位定义：

  BIT  名称                         含义
  ───────────────────────────────────────────────
  0    PCI_EXP_SLTSTA_ABP    Attention Button Pressed    (注意按钮按下)
  1    PCI_EXP_SLTSTA_PFD    Power Fault Detected        (电源故障检测到)
  2    PCI_EXP_SLTSTA_MRLSC  MRL Sensor Changed           (MRL 传感器变化)
  3    PCI_EXP_SLTSTA_PDC    Presence Detect Changed      (设备插拔检测变化)
  4    PCI_EXP_SLTSTA_CC     Command Completed             (命令完成)
  5    PCI_EXP_SLTSTA_DLLSC  Data Link Layer State Changed (链路状态变化)

  (MRL = Manually-operated Retention Latch，物理锁扣)
```

内核通过 `PCI_EXP_SLTCTL_HPIE` 位 (Hot-Plug Interrupt Enable) 控制是否允许这些事件产生中断。

---

## 5. 热插拔中断处理流水线

pciehp 同样采用两阶段中断处理模型：

```
硬件事件 (插入/拔出/按钮/电源故障)
     │
     ▼
pciehp_isr()  [hardIRQ, pciehp_hpc.c:623]
  ├─ 读 PCI_EXP_SLTSTA
  ├─ 滤出事件位: ABP | PFD | PDC | CC | DLLSC
  ├─ 写回 SLTSTA 清除事件
  ├─ MSI 模式下重读确认无遗漏
  ├─ Command Completed 立即处理 (唤醒等待者)
  ├─ 其余事件保存到 pending_events
  └─ 返回 IRQ_WAKE_THREAD
     │
     ▼
pciehp_ist()  [threaded handler, pciehp_hpc.c:728]
  ├─ atomic_xchg 取出 pending_events
  ├─ ABP → pciehp_handle_button_press()       (5 秒内确认)
  ├─ PFD → 设置指示灯为故障状态
  ├─ PDC/DLLSC → pciehp_handle_presence_or_link_change()
  │    ├─ 有设备在位 → 上电、枚举、绑定驱动
  │    └─ 设备拔出 → 离线驱动、断电
  └─ 发送 KOBJ_CHANGE uevent
```

### 5.1 pciehp_isr — hardIRQ 上下文 (`pciehp_hpc.c:623-726`)

```c
// drivers/pci/hotplug/pciehp_hpc.c:623-700 (关键片段)
static irqreturn_t pciehp_isr(int irq, void *dev_id)
{
    struct controller *ctrl = (struct controller *)dev_id;
    struct pci_dev *pdev = ctrl_dev(ctrl);
    struct device *parent = pdev->dev.parent;
    u16 status, events = 0;

    /* D3cold 或未使能 HPIE → 忽略 */
    if (pdev->current_state == PCI_D3cold ||
        (!(ctrl->slot_ctrl & PCI_EXP_SLTCTL_HPIE) && !pciehp_poll_mode))
        return IRQ_NONE;

    /* 确保 port 可访问，持有 parent 的 runtime PM 引用 */
    if (parent) {
        pm_runtime_get_noresume(parent);
        if (!pm_runtime_active(parent)) {
            pm_runtime_put(parent);
            disable_irq_nosync(irq);
            atomic_or(RERUN_ISR, &ctrl->pending_events);
            return IRQ_WAKE_THREAD;
        }
    }

read_status:
    pcie_capability_read_word(pdev, PCI_EXP_SLTSTA, &status);
    if (PCI_POSSIBLE_ERROR(status)) {
        ctrl_info(ctrl, "%s: no response from device\n", __func__);
        if (parent)
            pm_runtime_put(parent);
        return IRQ_NONE;
    }

    /* 掩码出事件位 */
    status &= PCI_EXP_SLTSTA_ABP | PCI_EXP_SLTSTA_PFD |
              PCI_EXP_SLTSTA_PDC | PCI_EXP_SLTSTA_CC |
              PCI_EXP_SLTSTA_DLLSC;

    /* Power Fault 去重 */
    if (ctrl->power_fault_detected)
        status &= ~PCI_EXP_SLTSTA_PFD;
    else if (status & PCI_EXP_SLTSTA_PFD)
        ctrl->power_fault_detected = true;

    events |= status;
    if (!events) {
        if (parent)
            pm_runtime_put(parent);
        return IRQ_NONE;
    }

    if (status) {
        pcie_capability_write_word(pdev, PCI_EXP_SLTSTA, status);

        /* MSI 模式下必须重读：所有事件位清零后才发新中断 */
        if (pci_dev_msi_enabled(pdev) && !pciehp_poll_mode)
            goto read_status;
    }
```

ISR 中有几个关键细节：
1. **Runtime PM** (`pciehp_hpc.c:643-651`)：如果 port 的 parent 设备处于 suspended 状态，不能读配置空间。这时 `disable_irq_nosync` + `RERUN_ISR` 标记，让 threaded handler 先唤醒 parent，再重新跑 ISR。
2. **Power Fault 去重** (`pciehp_hpc.c:674-677`)：如果已经报告过电源故障，且在清除之前再次中断，不再重复报告。
3. **MSI 重读** (`pciehp_hpc.c:695-696`)：MSI 模式下，写 SLTSTA 清除事件后，如果有新事件在读写之间到达，需要立即重读。

### 5.2 pciehp_ist — threaded handler (`pciehp_hpc.c:728-798`)

```c
// drivers/pci/hotplug/pciehp_hpc.c:728-798
static irqreturn_t pciehp_ist(int irq, void *dev_id)
{
    struct controller *ctrl = (struct controller *)dev_id;
    struct pci_dev *pdev = ctrl_dev(ctrl);
    irqreturn_t ret;
    u32 events;

    ctrl->ist_running = true;
    pci_config_pm_runtime_get(pdev);

    /* 如果 port 在中断时不可访问，rerun ISR */
    if (atomic_fetch_and(~RERUN_ISR, &ctrl->pending_events) & RERUN_ISR) {
        ret = pciehp_isr(irq, dev_id);
        enable_irq(irq);
        if (ret != IRQ_WAKE_THREAD)
            goto out;
    }

    synchronize_hardirq(irq);
    events = atomic_xchg(&ctrl->pending_events, 0);
    if (!events) {
        ret = IRQ_NONE;
        goto out;
    }

    /* Check Attention Button Pressed */
    if (events & PCI_EXP_SLTSTA_ABP)
        pciehp_handle_button_press(ctrl);

    /* Check Power Fault Detected */
    if (events & PCI_EXP_SLTSTA_PFD) {
        ctrl_err(ctrl, "Slot(%s): Power fault\n", slot_name(ctrl));
        pciehp_set_indicators(ctrl, PCI_EXP_SLTCTL_PWR_IND_OFF,
                      PCI_EXP_SLTCTL_ATTN_IND_ON);
    }

    /* 过滤 DPC 恢复或 SBR 引起的 spurious link change */
    if ((events & (PCI_EXP_SLTSTA_PDC | PCI_EXP_SLTSTA_DLLSC)) &&
        (pci_dpc_recovered(pdev) || pci_hp_spurious_link_change(pdev)) &&
        ctrl->state == ON_STATE) {
        u16 ignored_events = PCI_EXP_SLTSTA_DLLSC;

        if (!ctrl->inband_presence_disabled)
            ignored_events |= PCI_EXP_SLTSTA_PDC;

        events &= ~ignored_events;
        pciehp_ignore_link_change(ctrl, pdev, irq, ignored_events);
    }

    /* 处理插拔事件 */
    down_read_nested(&ctrl->reset_lock, ctrl->depth);
    if (events & DISABLE_SLOT)
        pciehp_handle_disable_request(ctrl);
    else if (events & (PCI_EXP_SLTSTA_PDC | PCI_EXP_SLTSTA_DLLSC))
        pciehp_handle_presence_or_link_change(ctrl, events);
    up_read(&ctrl->reset_lock);

    ret = IRQ_HANDLED;
out:
    pci_config_pm_runtime_put(pdev);
    ctrl->ist_running = false;
    wake_up(&ctrl->requester);
    return ret;
}
```

事件处理优先级：**Disable Request > Presence/Link Change > Attention Button > Power Fault**

`pciehp_handle_presence_or_link_change` 是核心决策函数，它会：
- 检测到设备在位 + 当前状态 OFF → 上电 (`pciehp_power_on_slot`)，然后重新枚举 bus
- 检测到设备拔出 + 当前状态 ON → 先通知驱动 remove，再断电 (`pciehp_power_off_slot`)

---

## 6. ASPM — 主动状态电源管理

ASPM (Active State Power Management) 允许空闲链路进入低功耗状态，而无需软件干预。它是 PCIe 电源管理的核心机制。

### 6.1 ASPM 状态及延迟

```
状态      功耗等级        进入延迟        退出延迟        说明
──────────────────────────────────────────────────────────────────
L0        Fully ON        —              —             正常工作状态
L0s       Standby         ~50ns          1-10µs         单向低功耗，快速恢复
L1        Idle            ~10µs          10-100µs       双向低功耗，时钟可关闭
L1.1      Deeper Idle     ~10µs          10-100µs       L1 子状态，PLL 可关闭
L1.2      Deepest Idle    ~10µs          50-100µs       L1 子状态，参考时钟可关闭
```

L1 Substates (L1SS / L1.1 / L1.2) 在 PCIe 3.1+ 规范中定义，提供比 L1 更深的睡眠层次。

### 6.2 `struct pcie_link_state` (`aspm.c:228-248`)

```c
// drivers/pci/pcie/aspm.c:228-248
struct pcie_link_state {
    struct pci_dev *pdev;           /* Upstream component of the Link */
    struct pci_dev *downstream;     /* Downstream component, function 0 */
    struct pcie_link_state *root;   /* pointer to the root port link */
    struct pcie_link_state *parent; /* pointer to the parent Link state */
    struct list_head sibling;       /* node in link_list */

    /* ASPM state */
    u32 aspm_support:7;     /* Supported ASPM state */
    u32 aspm_enabled:7;     /* Enabled ASPM state */
    u32 aspm_capable:7;     /* Capable ASPM state with latency */
    u32 aspm_default:7;     /* Default ASPM state by BIOS or override */
    u32 aspm_disable:7;     /* Disabled ASPM state */

    /* Clock PM state */
    u32 clkpm_capable:1;    /* Clock PM capable? */
    u32 clkpm_enabled:1;    /* Current Clock PM state */
    u32 clkpm_default:1;    /* Default Clock PM state by BIOS */
    u32 clkpm_disable:1;    /* Clock PM disabled */
};
```

每个 PCIe 链路维护一个 `pcie_link_state`：
- **pdev** 指向链路的上游组件 (upstream component)
- **downstream** 指向链路的下游组件 (function 0)
- **root** 指向根端口的链路状态（用于确定最大端到端延迟）
- **parent** 指向父链路状态（层层向上，直到 root port）

五个 bitmask 的语义：
- `aspm_support`：硬件支持哪些 ASPM 状态
- `aspm_capable`：考虑延迟约束后允许哪些 ASPM 状态
- `aspm_enabled`：当前实际启用的 ASPM 状态
- `aspm_default`：BIOS 设置的默认 ASPM 状态
- `aspm_disable`：驱动/用户显式禁用的 ASPM 状态

### 6.3 ASPM 策略 (`aspm.c:255-307`)

```c
// drivers/pci/pcie/aspm.c:255-307
#define POLICY_DEFAULT 0       /* BIOS default setting */
#define POLICY_PERFORMANCE 1   /* high performance */
#define POLICY_POWERSAVE 2     /* high power saving */
#define POLICY_POWER_SUPERSAVE 3 /* possibly even more power saving */

#ifdef CONFIG_PCIEASPM_PERFORMANCE
static int aspm_policy = POLICY_PERFORMANCE;
#elif defined CONFIG_PCIEASPM_POWERSAVE
static int aspm_policy = POLICY_POWERSAVE;
#elif defined CONFIG_PCIEASPM_POWER_SUPERSAVE
static int aspm_policy = POLICY_POWER_SUPERSAVE;
#else
static int aspm_policy;
#endif

static const char *policy_str[] = {
    [POLICY_DEFAULT] = "default",
    [POLICY_PERFORMANCE] = "performance",
    [POLICY_POWERSAVE] = "powersave",
    [POLICY_POWER_SUPERSAVE] = "powersupersave"
};

static int policy_to_aspm_state(struct pcie_link_state *link)
{
    switch (aspm_policy) {
    case POLICY_PERFORMANCE:
        return 0;                                // L0 only
    case POLICY_POWERSAVE:
        return PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1;
    case POLICY_POWER_SUPERSAVE:
        return PCIE_LINK_STATE_ASPM_ALL;         // L0s + L1 + L1.1 + L1.2
    case POLICY_DEFAULT:
        return link->aspm_default;
    }
    return 0;
}
```

四种策略：
- **PERFORMANCE**：完全禁用 ASPM，链路始终在 L0
- **POWERSAVE**：启用 L0s 和 L1
- **POWER_SUPERSAVE**：启用所有状态（含 L1.1/L1.2）
- **DEFAULT**：跟随 BIOS 设置

可通过内核命令行 `pcie_aspm=off|performance|powersave|powersupersave` 或 sysfs `/sys/module/pcie_aspm/parameters/policy` 动态切换。

### 6.4 LTR — 延迟容忍度报告 (`aspm.c:30-68`)

```c
// drivers/pci/pcie/aspm.c:30-52
void pci_save_ltr_state(struct pci_dev *dev)
{
    int ltr;
    struct pci_cap_saved_state *save_state;
    u32 *cap;

    if (!pci_is_pcie(dev))
        return;

    ltr = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_LTR);
    if (!ltr)
        return;

    save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_LTR);
    if (!save_state) {
        pci_err(dev, "no suspend buffer for LTR; "
            "ASPM issues possible after resume\n");
        return;
    }

    /* Some broken devices only support dword access to LTR */
    cap = &save_state->cap.data[0];
    pci_read_config_dword(dev, ltr + PCI_LTR_MAX_SNOOP_LAT, cap);
}

// drivers/pci/pcie/aspm.c:54-68
void pci_restore_ltr_state(struct pci_dev *dev)
{
    struct pci_cap_saved_state *save_state;
    int ltr;
    u32 *cap;

    save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_LTR);
    ltr = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_LTR);
    if (!save_state || !ltr)
        return;

    cap = &save_state->cap.data[0];
    pci_write_config_dword(dev, ltr + PCI_LTR_MAX_SNOOP_LAT, *cap);
}
```

LTR (Latency Tolerance Reporting) 允许设备向 Root Complex 报告其能容忍的最大延迟。如果设备能容忍的延迟大于 ASPM L1 exit 延迟，则可以安全进入 L1。suspend 时保存、resume 时恢复 LTR 值。

### 6.5 L1SS 配置 (`aspm.c:70-123`)

```c
// drivers/pci/pcie/aspm.c:70-81
void pci_configure_aspm_l1ss(struct pci_dev *pdev)
{
    int rc;

    pdev->l1ss = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);

    rc = pci_add_ext_cap_save_buffer(pdev, PCI_EXT_CAP_ID_L1SS,
                     2 * sizeof(u32));
    if (rc)
        pci_err(pdev, "unable to allocate ASPM L1SS save buffer (%pe)\n",
            ERR_PTR(rc));
}

// drivers/pci/pcie/aspm.c:85-123
void pci_save_aspm_l1ss_state(struct pci_dev *pdev)
{
    struct pci_dev *parent = pdev->bus->self;
    struct pci_cap_saved_state *save_state;
    u32 *cap;

    /* Downstream Port 不直接恢复，而是由上游组件来恢复 */
    if (pcie_downstream_port(pdev) || !parent)
        return;

    if (!pdev->l1ss || !parent->l1ss)
        return;

    save_state = pci_find_saved_ext_cap(pdev, PCI_EXT_CAP_ID_L1SS);
    if (!save_state)
        return;

    cap = &save_state->cap.data[0];
    pci_read_config_dword(pdev, pdev->l1ss + PCI_L1SS_CTL2, cap++);
    pci_read_config_dword(pdev, pdev->l1ss + PCI_L1SS_CTL1, cap++);

    /* 同时保存 parent 的 L1SS 状态以便恢复 */
    save_state = pci_find_saved_ext_cap(parent, PCI_EXT_CAP_ID_L1SS);
    if (!save_state)
        return;

    cap = &save_state->cap.data[0];
    pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CTL2, cap++);
    pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CTL1, cap++);
}
```

L1SS (L1 Substates) 需要同时保存上行和下行两端的状态。因为 L1.1/L1.2 涉及双向协商，saving/restoring 必须成对进行。

---

## 7. PCI 电源状态

PCI/PCIe 规范定义了如下设备电源状态：

```
状态    名称      功耗        唤醒延迟       上下文保留
────────────────────────────────────────────────────
D0      ON        全功耗       N/A           全部保留
D1      轻睡眠    较低         较快           部分保留
D2      中睡眠    更低         较慢           部分保留
D3hot   热睡眠    极低         慢             几乎全部丢失
D3cold  冷睡眠    完全断电     最慢           全部丢失 (Vaux 可能保留)
```

- **D3hot**：设备仍由主电源供电，但时钟和大部分逻辑关闭
- **D3cold**：主电源完全移除，只保留 Vaux（辅助电源，如果存在）

PCIe 增加了：
- **PME (Power Management Event)**：设备在 D3cold 中可通过 Vaux 发送唤醒信号到 Root Complex
- **ASPM**：L0s/L1 是链路级的电源状态，与设备级的 D 状态正交

PME 的核心实现在 `drivers/pci/pcie/pme.c`，它也是作为 PCIe Port Service 驱动注册的，负责处理 PCIe 唤醒事件。

---

## 8. sysfs 接口全景

```
/sys/bus/pci/slots/
  └── <slot_number>/
        ├── address         — BDF 地址
        ├── attention       — 注意指示灯控制 (0=off, 1=on, 2=blink)
        ├── power           — 电源状态 (0=off, 1=on)
        └── adapter         — 设备在位 (1=present, 0=empty)

/sys/devices/pci.../<dev>/
  └── remove               — 写 1 触发安全移除

/sys/module/pcie_aspm/parameters/
  └── policy               — ASPM 策略 (default/performance/powersave/powersupersave)
```

---

## 9. 总结：从插拔事件到用户态通知的数据流

```
        ╔═══════════════════════════════════════╗
        ║  物理事件：用户插入 PCIe 设备             ║
        ╚══════════════╤════════════════════════╝
                       │
        ╔══════════════╧════════════════════════╗
        ║  PCIe Slot Status: PRESENCE_DETECTED  ║
        ║  硬件置位 SLTSTA.PDC                   ║
        ╚══════════════╤════════════════════════╝
                       │
        ╔══════════════╧════════════════════════╗
        ║  pciehp_isr()  [pciehp_hpc.c:623]     ║
        ║  ├─ 读 SLTSTA: PDC=1                  ║
        ║  ├─ 写 SLTSTA 清除 PDC                ║
        ║  └─ atomic_or → pending_events        ║
        ╚══════════════╤════════════════════════╝
                       │
        ╔══════════════╧════════════════════════╗
        ║  pciehp_ist()  [pciehp_hpc.c:728]     ║
        ║  ├─ atomic_xchg → pending_events      ║
        ║  ├─ pciehp_handle_presence_or_link_   ║
        ║  │    change()                        ║
        ║  │  ├─ pciehp_power_on_slot()         ║
        ║  │  ├─ pciehp_link_enable()           ║
        ║  │  ├─ pci_rescan_bus()  重新枚举      ║
        ║  │  └─ driver core: probe 新设备       ║
        ║  └─ 发送 KOBJ_CHANGE uevent            ║
        ╚═══════════════════════════════════════╝
```

而 ASPM 独立于热插拔工作：只要链路 idle，ASPM 引擎在微秒级自动切换 L0s/L1，对设备驱动完全透明。内核策略层 (`aspm_policy`) 决定"允许进入多深"的状态，硬件自主决定"何时进入"。

---

## 下一篇文章

热插拔和电源管理的背后，是 PCIe Port 服务模型在支撑——一个 Root Port 的中断如何分发给 AER、Hotplug、PME、DPC、BWCTRL 五个独立服务？详见[第七篇](./07-port-service.md)。
