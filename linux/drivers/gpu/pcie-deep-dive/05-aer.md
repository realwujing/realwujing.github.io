# 第五篇：AER — 高级错误报告

> 源码：`drivers/pci/pcie/aer.c` (1740 lines) | `drivers/pci/pcie/err.c` (299 lines) | `drivers/pci/pcie/dpc.c` (536 lines)
> 头文件：`include/linux/aer.h` | `drivers/pci/pcie/portdrv.h`

系列目录：[PCIe 内核源码深度解析](./README.md)

---

## 1. AER 能力初始化

AER (Advanced Error Reporting) 是 PCIe 规范定义的可选扩展能力，提供比传统 PCI 奇偶校验更细粒度的错误检测和报告机制。内核在设备枚举阶段通过 `pci_init_capabilities` 统一初始化所有 PCIe 能力，其中 AER 的初始化入口在 `probe.c:2674`：

```c
// drivers/pci/probe.c:2664-2681
pci_imm_ready_init(dev);        /* Immediate Readiness */
pci_pm_init(dev);               /* Power Management */
pci_vpd_init(dev);              /* Vital Product Data */
pci_configure_ari(dev);         /* Alternative Routing-ID Forwarding */
pci_iov_init(dev);              /* Single Root I/O Virtualization */
pci_ats_init(dev);              /* Address Translation Services */
pci_pri_init(dev);              /* Page Request Interface */
pci_pasid_init(dev);            /* Process Address Space ID */
pci_acs_init(dev);              /* Access Control Services */
pci_ptm_init(dev);              /* Precision Time Measurement */
pci_aer_init(dev);              /* Advanced Error Reporting */    // <-- line 2674
pci_dpc_init(dev);              /* Downstream Port Containment */
```

`pci_aer_init` 的实现位于 `aer.c:385-419`，主要做四件事：

```c
// drivers/pci/pcie/aer.c:385-419
void pci_aer_init(struct pci_dev *dev)
{
    int n;

    dev->aer_cap = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
    if (!dev->aer_cap)
        return;

    dev->aer_info = kzalloc_obj(*dev->aer_info);
    if (!dev->aer_info) {
        dev->aer_cap = 0;
        return;
    }

    ratelimit_state_init(&dev->aer_info->correctable_ratelimit,
                 DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);
    ratelimit_state_init(&dev->aer_info->nonfatal_ratelimit,
                 DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);

    n = pcie_cap_has_rtctl(dev) ? 5 : 4;
    pci_add_ext_cap_save_buffer(dev, PCI_EXT_CAP_ID_ERR, sizeof(u32) * n);

    pci_aer_clear_status(dev);

    if (pci_aer_available())
        pci_enable_pcie_error_reporting(dev);

    pcie_set_ecrc_checking(dev);
}
```

步骤拆解：

1. **查找 AER 扩展能力** (`aer.c:389`)：`pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR)` 扫描 PCIe 扩展能力链表，找到 AER 能力的位置，存入 `dev->aer_cap`
2. **分配 `aer_info` 结构** (`aer.c:393`)：为设备分配错误计数器和速率限制状态
3. **初始化速率限制** (`aer.c:399-402`)：防止错误日志刷屏，分别对 Correctable 和 Non-Fatal 错误做速率限制
4. **分配 suspend/resume 保存缓冲区** (`aer.c:410-411`)：Root Port 额外需要保存 `PCI_ERR_ROOT_COMMAND`，所以 5 个 dword；普通设备 4 个 dword（`UNCOR_MASK`, `UNCOR_SEVER`, `COR_MASK`, `ERR_CAP`）
5. **清除 stale 状态，启用 reporting** (`aer.c:413-416`)

---

## 2. 三类 PCIe 错误

PCIe 规范定义了三种错误严重级别：

```
Severity 级别        硬件行为                       内核处理策略
──────────────────────────────────────────────────────────────────
Correctable (CE)   硬件自动纠正，发送 ERR_COR 消息    日志记录，速率限制
                    事务可能被重试，对软件透明

Non-Fatal (NFE)    事务失败，发送 ERR_NONFATAL 消息    Link 可能仍可用
                    链路保持 up，设备可继续工作         通知驱动恢复

Fatal (FE)         不可恢复，发送 ERR_FATAL 消息       Link 可能降级
                    链路可能不可用                      完整 reset 流程
```

AER 扩展能力中的关键寄存器：

- **PCI_ERR_COR_STATUS**：16 个 Correctable 错误位 (Receiver Error, Bad TLP, Bad DLLP, REPLAY_NUM Rollover, Replay Timer Timeout, Advisory Non-Fatal Error 等)
- **PCI_ERR_UNCOR_STATUS**：32 个 Uncorrectable 错误位 (Data Link Protocol Error, Surprise Down, Poisoned TLP, Flow Control Protocol Error, Completion Timeout, Completer Abort, Unexpected Completion, ECRC Error, Unsupported Request, ACS Violation, Uncorrectable Internal Error, MC Blocked TLP 等)

Root Port 额外有：
- **PCI_ERR_ROOT_STATUS**：记录收到的 ERR_COR / ERR_FATAL / ERR_NONFATAL / Multiple ERR_COR
- **PCI_ERR_ROOT_ERR_SRC**：包含 `ERR_COR_ID` 和 `ERR_SRC_ID`，用于定位错误源

---

## 3. 核心数据结构

### 3.1 `struct aer_err_source` — 错误源 (`aer.c:49-52`)

```c
// drivers/pci/pcie/aer.c:49-52
struct aer_err_source {
    u32 status;         /* PCI_ERR_ROOT_STATUS */
    u32 id;             /* PCI_ERR_ROOT_ERR_SRC */
};
```

Root Port 的中断处理程序从 `PCI_ERR_ROOT_STATUS` 和 `PCI_ERR_ROOT_ERR_SRC` 寄存器中读取这两个 32-bit 值，打包后推入 kfifo。

### 3.2 `struct aer_rpc` — Root Port 控制块 (`aer.c:54-57`)

```c
// drivers/pci/pcie/aer.c:44-57
#define AER_ERROR_SOURCES_MAX       128      // 最多缓冲 128 个错误源

struct aer_rpc {
    struct pci_dev *rpd;                     /* Root Port device */
    DECLARE_KFIFO(aer_fifo, struct aer_err_source, AER_ERROR_SOURCES_MAX);
};
```

`aer_rpc` 是每个 Root Port 服务实例的私有数据。`rpd` 指向所属的 Root Port 设备，`aer_fifo` 是一个无锁环形缓冲区 (kfifo)，容量为 128 个 `aer_err_source`。hardIRQ 和 threaded handler 之间通过这个 FIFO 传递错误信息。

### 3.3 `struct aer_info` — 设备级错误统计 (`aer.c:60-97`)

```c
// drivers/pci/pcie/aer.c:59-97
/* AER info for the device */
struct aer_info {
    /*
     * Counters for all AER capable devices. They indicate the errors
     * "as seen by this device". Note that this may mean that if an
     * Endpoint is causing problems, the AER counters may increment
     * at its link partner (e.g. Root Port) because the errors will be
     * "seen" by the link partner and not the problematic Endpoint
     * itself.
     */
    u64 dev_cor_errs[AER_MAX_TYPEOF_COR_ERRS];       // 16 种 CE
    u64 dev_fatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS];    // 32 种 FE
    u64 dev_nonfatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS]; // 32 种 NFE
    u64 dev_total_cor_errs;        /* 本设备发送的 ERR_COR 总数 */
    u64 dev_total_fatal_errs;      /* 本设备发送的 ERR_FATAL 总数 */
    u64 dev_total_nonfatal_errs;   /* 本设备发送的 ERR_NONFATAL 总数 */

    /* Root Port / RCEC only: 收到的错误消息总数（含自身产生） */
    u64 rootport_total_cor_errs;
    u64 rootport_total_fatal_errs;
    u64 rootport_total_nonfatal_errs;

    /* 速率限制，防止日志刷屏 */
    struct ratelimit_state correctable_ratelimit;
    struct ratelimit_state nonfatal_ratelimit;
};
```

关键注释 (`aer.c:62-69`) 指出一个重要的语义：错误计数器记录的并非"谁产生的错误"，而是"谁看到的错误"。如果 Endpoint 产生错误，其上游的 Root Port 会先看到并记录，而肇事 Endpoint 自己的计数器可能全是 0（因为它从未意识到出错了）。

### 3.4 AER Log TLP 掩码 (`aer.c:99-109`)

```c
// drivers/pci/pcie/aer.c:99-109
#define AER_LOG_TLP_MASKS      (PCI_ERR_UNC_POISON_TLP|    \
                    PCI_ERR_UNC_POISON_BLK |                \
                    PCI_ERR_UNC_ECRC|                       \
                    PCI_ERR_UNC_UNSUP|                      \
                    PCI_ERR_UNC_COMP_ABORT|                 \
                    PCI_ERR_UNC_UNX_COMP|                   \
                    PCI_ERR_UNC_ACSV |                      \
                    PCI_ERR_UNC_MCBTLP |                    \
                    PCI_ERR_UNC_ATOMEG |                    \
                    PCI_ERR_UNC_DMWR_BLK |                  \
                    PCI_ERR_UNC_XLAT_BLK |                  \
                    PCI_ERR_UNC_PTLP_REQUEST)
```

当这些错误位被置位时，内核会额外记录 TLP Header（事务层包头部，16 字节），帮助定位问题事务的来源地址、请求者 ID 和 Tag。

### 3.5 AER Agent 分类 (`aer.c:427-443`)

```c
// drivers/pci/pcie/aer.c:427-443
#define AER_AGENT_RECEIVER      0
#define AER_AGENT_REQUESTER     1
#define AER_AGENT_COMPLETER     2
#define AER_AGENT_TRANSMITTER   3

#define AER_AGENT_REQUESTER_MASK(t) \
    ((t == AER_CORRECTABLE) ? 0 : (PCI_ERR_UNC_COMP_TIME|PCI_ERR_UNC_UNSUP))
#define AER_AGENT_COMPLETER_MASK(t) \
    ((t == AER_CORRECTABLE) ? 0 : PCI_ERR_UNC_COMP_ABORT)
#define AER_AGENT_TRANSMITTER_MASK(t) \
    ((t == AER_CORRECTABLE) ? \
     (PCI_ERR_COR_REP_ROLL|PCI_ERR_COR_REP_TIMER) : 0)

#define AER_GET_AGENT(t, e) \
    ((e & AER_AGENT_COMPLETER_MASK(t)) ? AER_AGENT_COMPLETER :  \
     (e & AER_AGENT_REQUESTER_MASK(t)) ? AER_AGENT_REQUESTER :  \
     (e & AER_AGENT_TRANSMITTER_MASK(t)) ? AER_AGENT_TRANSMITTER : \
     AER_AGENT_RECEIVER)
```

根据错误位判断错误发生在事务的哪个阶段：
- **RECEIVER**：接收方检测到物理层/链路层错误
- **REQUESTER**：请求方收到 Unsupported Request 或 Completion Timeout
- **COMPLETER**：完成方发出 Completer Abort
- **TRANSMITTER**：发送方检测到 Replay Rollover 或 Replay Timer Timeout

---

## 4. 中断处理流水线

AER 的中断处理采用 **两阶段** (hardIRQ + threaded IRQ) 模型：

```
硬件 AER 中断
     │
     ▼
aer_irq()  [hardIRQ, aer.c:1470]
  ├─ 读取 PCI_ERR_ROOT_STATUS
  ├─ 读取 PCI_ERR_ROOT_ERR_SRC
  ├─ 清除 Root Port 中的状态位
  ├─ 推入 aer_rpc->aer_fifo (kfifo)
  └─ 返回 IRQ_WAKE_THREAD
     │
     ▼
aer_isr()  [threaded handler, aer.c:1449]
  ├─ 从 kfifo 中逐个取出 aer_err_source
  └─ 每个调用 aer_isr_one_error()
```

### 4.1 aer_irq — hardIRQ 上下文 (`aer.c:1470-1489`)

```c
// drivers/pci/pcie/aer.c:1470-1489
static irqreturn_t aer_irq(int irq, void *context)
{
    struct pcie_device *pdev = (struct pcie_device *)context;
    struct aer_rpc *rpc = get_service_data(pdev);
    struct pci_dev *rp = rpc->rpd;
    int aer = rp->aer_cap;
    struct aer_err_source e_src = {};

    pci_read_config_dword(rp, aer + PCI_ERR_ROOT_STATUS, &e_src.status);
    if (!(e_src.status & AER_ERR_STATUS_MASK))
        return IRQ_NONE;         // 假中断，不是我们的

    pci_read_config_dword(rp, aer + PCI_ERR_ROOT_ERR_SRC, &e_src.id);
    pci_write_config_dword(rp, aer + PCI_ERR_ROOT_STATUS, e_src.status);

    if (!kfifo_put(&rpc->aer_fifo, e_src))
        return IRQ_HANDLED;      // FIFO 满了，丢弃但返回已处理

    return IRQ_WAKE_THREAD;      // 唤醒线程化处理函数
}
```

关键操作：
1. 读取 Root Port 的 `PCI_ERR_ROOT_STATUS` 和 `PCI_ERR_ROOT_ERR_SRC` (aer.c:1478-1482)
2. **立即清除** Root Port 中的状态位 (aer.c:1483)，防止中断反复触发
3. 将错误源信息推入 kfifo (aer.c:1485)
4. 如果 FIFO 满了就静默丢弃，返回 `IRQ_HANDLED` 防止中断风暴
5. 返回 `IRQ_WAKE_THREAD` 唤醒线程化 handler

### 4.2 aer_isr — threaded handler (`aer.c:1449-1461`)

```c
// drivers/pci/pcie/aer.c:1449-1461
static irqreturn_t aer_isr(int irq, void *context)
{
    struct pcie_device *dev = (struct pcie_device *)context;
    struct aer_rpc *rpc = get_service_data(dev);
    struct aer_err_source e_src;

    if (kfifo_is_empty(&rpc->aer_fifo))
        return IRQ_NONE;

    while (kfifo_get(&rpc->aer_fifo, &e_src))
        aer_isr_one_error(rpc->rpd, &e_src);
    return IRQ_HANDLED;
}
```

非常简单：如果 kfifo 为空则返回 `IRQ_NONE`，否则逐个取出并处理。真正的复杂逻辑在 `aer_isr_one_error` 中。

### 4.3 aer_isr_one_error — 单个错误处理 (`aer.c:1405-1439`)

```c
// drivers/pci/pcie/aer.c:1405-1439 (关键部分)
static void aer_isr_one_error(struct pci_dev *root,
                  struct aer_err_source *e_src)
{
    u32 status = e_src->status;

    pci_rootport_aer_stats_incr(root, e_src);

    /*
     * 有可能同时记录了 CE 和 UE，先报告 CE
     */
    if (status & PCI_ERR_ROOT_COR_RCV) {
        int multi = status & PCI_ERR_ROOT_MULTI_COR_RCV;
        struct aer_err_info e_info = {
            .id = ERR_COR_ID(e_src->id),
            .error_dev_num = 0,
            .multi_error_valid = multi,
        };
        aer_isr_one_error_type(root, &e_info);    // 处理 CE
    }

    if (status & PCI_ERR_ROOT_UNCOR_RCV) {         // 处理 UE (NFE/FE)
        // ... 构建 aer_err_info ...
        aer_isr_one_error_type(root, &e_info);
    }
}
```

注意两点：
1. CE 优先于 UE 处理 — 如果一个中断同时报告了 CE 和 UE，先处理 CE
2. `multi_error_valid` 标记表示是否有多个 CE 等待处理

### 4.4 aer_isr_one_error_type — 查找源设备并处理 (`aer.c:1373-1397`)

```c
// drivers/pci/pcie/aer.c:1373-1397
static void aer_isr_one_error_type(struct pci_dev *root,
                   struct aer_err_info *info)
{
    bool found;

    found = find_source_device(root, info);

    if (info->root_ratelimit_print ||
        (!found && aer_ratelimit(root, info->severity)))
        aer_print_source(root, info, found);

    if (found)
        aer_process_err_devices(info);    // trigger recovery chain
}
```

`find_source_device` 从 `PCI_ERR_ROOT_ERR_SRC` 中提取 Requester ID（Bus:Device.Function），在 PCI 拓扑中查找对应的 `struct pci_dev`。找到后调用 `aer_process_err_devices`，这会将错误信息传递给下游设备，并**根据错误严重性触发恢复链**。

---

## 5. 错误恢复链 — err.c

当 AER 检测到 Non-Fatal 或 Fatal 错误并找到错误源设备后，就进入了 PCIe 错误恢复链。这个过程由 `err.c` 中的 `pcie_do_recovery` 函数统一编排，类似交响乐团的指挥。

```
错误恢复流程（err.c）：

                 error_detected 广播 (err.c:49-87)
                        │
                        ▼
                   检查结果 status
                   ├─ CAN_RECOVER  ──►  mmio_enabled 广播 (err.c:129-146)
                   ├─ NEED_RESET   ──►  reset_subordinates (err.c:252-256)
                   └─ DISCONNECT   ──►  失败，放弃
                        │
                        ▼
                  slot_reset 广播 (err.c:148-166)
                        │
                        ▼
                  resume 广播 (err.c:168-185)
                        │
                        ▼
                  清理 AER 状态，发送 uevent
```

### 5.1 pcie_do_recovery — 总指挥 (`err.c:210-299`)

```c
// drivers/pci/pcie/err.c:210-240
pci_ers_result_t pcie_do_recovery(struct pci_dev *dev,
        pci_channel_state_t state,
        pci_ers_result_t (*reset_subordinates)(struct pci_dev *pdev))
{
    int type = pci_pcie_type(dev);
    struct pci_dev *bridge;
    pci_ers_result_t status = PCI_ERS_RESULT_CAN_RECOVER;
    struct pci_host_bridge *host = pci_find_host_bridge(dev->bus);

    /* 确定恢复的边界 (bridge) */
    if (type == PCI_EXP_TYPE_ROOT_PORT ||
        type == PCI_EXP_TYPE_DOWNSTREAM ||
        type == PCI_EXP_TYPE_RC_EC ||
        type == PCI_EXP_TYPE_RC_END)
        bridge = dev;                        // Port 本身
    else
        bridge = pci_upstream_bridge(dev);   // Endpoint → 上游桥

    pci_walk_bridge(bridge, pci_pm_runtime_get_sync, NULL);
```

恢复的边界 (`bridge`) 决定了哪些设备受影响：
- 如果错误由 Port 检测到 → bridge = Port 自身，波及所有下游设备
- 如果错误由 Endpoint 检测到 → bridge = 上游桥，只影响同一桥下的设备

然后对所有设备调用 `pm_runtime_get_sync` 防止设备在恢复过程中进入 runtime suspend。

### 5.2 report_error_detected — 第一阶段 (`err.c:49-87`)

```c
// drivers/pci/pcie/err.c:49-87
static int report_error_detected(struct pci_dev *dev,
                 pci_channel_state_t state,
                 enum pci_ers_result *result)
{
    struct pci_driver *pdrv;
    pci_ers_result_t vote;
    const struct pci_error_handlers *err_handler;

    device_lock(&dev->dev);
    pdrv = dev->driver;
    if (pci_dev_is_disconnected(dev)) {
        vote = PCI_ERS_RESULT_DISCONNECT;
    } else if (!pci_dev_set_io_state(dev, state)) {
        pci_info(dev, "can't recover (state transition %u -> %u invalid)\n",
            dev->error_state, state);
        vote = PCI_ERS_RESULT_NONE;
    } else if (!pdrv || !pdrv->err_handler ||
           !pdrv->err_handler->error_detected) {
        /*
         * 如果子树中任何设备没有 error_detected 回调，
         * PCI_ERS_RESULT_NO_AER_DRIVER 会阻止后续所有回调。
         */
        if (dev->hdr_type != PCI_HEADER_TYPE_BRIDGE) {
            vote = PCI_ERS_RESULT_NO_AER_DRIVER;
            pci_info(dev, "can't recover (no error_detected callback)\n");
        } else {
            vote = PCI_ERS_RESULT_NONE;
        }
    } else {
        err_handler = pdrv->err_handler;
        vote = err_handler->error_detected(dev, state);    // 调用驱动回调
    }
    pci_uevent_ers(dev, vote);
    *result = merge_result(*result, vote);
    device_unlock(&dev->dev);
    return 0;
}
```

对 bridge 下每个设备：
1. 检查设备是否已断开
2. 尝试状态转移（`pci_channel_io_normal` → `pci_channel_io_frozen`）
3. 如果驱动注册了 `err_handler->error_detected` 回调，则调用它
4. 对所有投票结果做 `merge_result` 汇总

**投票优先级**：`NO_AER_DRIVER > DISCONNECT > NEED_RESET > CAN_RECOVER > NONE > RECOVERED`

若任何设备没有 `error_detected` 回调，整个恢复链终止（`NO_AER_DRIVER` 一票否决）。

### 5.3 report_mmio_enabled — 第二阶段 (`err.c:129-146`)

```c
// drivers/pci/pcie/err.c:129-146
static int report_mmio_enabled(struct pci_dev *dev, void *data)
{
    struct pci_driver *pdrv;
    pci_ers_result_t vote, *result = data;
    const struct pci_error_handlers *err_handler;

    device_lock(&dev->dev);
    pdrv = dev->driver;
    if (!pdrv || !pdrv->err_handler || !pdrv->err_handler->mmio_enabled)
        goto out;

    err_handler = pdrv->err_handler;
    vote = err_handler->mmio_enabled(dev);
    *result = merge_result(*result, vote);
out:
    device_unlock(&dev->dev);
    return 0;
}
```

仅当 `status == CAN_RECOVER` 时调用。此时 MMIO 事务可以重试，驱动可以恢复寄存器状态或做一些轻量级恢复。`mmio_enabled` 回调是可选的，大多数驱动不实现它。

### 5.4 report_slot_reset — 第三阶段 (`err.c:148-166`)

```c
// drivers/pci/pcie/err.c:148-166
static int report_slot_reset(struct pci_dev *dev, void *data)
{
    struct pci_driver *pdrv;
    pci_ers_result_t vote, *result = data;
    const struct pci_error_handlers *err_handler;

    device_lock(&dev->dev);
    pdrv = dev->driver;
    if (!pci_dev_set_io_state(dev, pci_channel_io_normal) ||
        !pdrv || !pdrv->err_handler || !pdrv->err_handler->slot_reset)
        goto out;

    err_handler = pdrv->err_handler;
    vote = err_handler->slot_reset(dev);
    *result = merge_result(*result, vote);
out:
    device_unlock(&dev->dev);
    return 0;
}
```

在 `reset_subordinates` 成功后调用。驱动在此回调中重新初始化设备硬件状态（重新写入 BAR、恢复配置空间寄存器、重新分配 DMA 缓冲区等）。

### 5.5 report_resume — 第四阶段 (`err.c:168-185`)

```c
// drivers/pci/pcie/err.c:168-185
static int report_resume(struct pci_dev *dev, void *data)
{
    struct pci_driver *pdrv;
    const struct pci_error_handlers *err_handler;

    device_lock(&dev->dev);
    pdrv = dev->driver;
    if (!pci_dev_set_io_state(dev, pci_channel_io_normal) ||
        !pdrv || !pdrv->err_handler || !pdrv->err_handler->resume)
        goto out;

    err_handler = pdrv->err_handler;
    err_handler->resume(dev);
out:
    pci_uevent_ers(dev, PCI_ERS_RESULT_RECOVERED);
    device_unlock(&dev->dev);
    return 0;
}
```

恢复链的最后一站。驱动在此完成最后的恢复操作（重新启用中断、重新开始 I/O 等），内核随后发送 `PCI_ERS_RESULT_RECOVERED` uevent 通知用户态。

### 5.6 恢复链主循环 (`err.c:238-298`)

```c
// drivers/pci/pcie/err.c:238-298 (pcie_do_recovery 的后半段)
    pci_dbg(bridge, "broadcast error_detected message\n");
    if (state == pci_channel_io_frozen)
        pci_walk_bridge(bridge, report_frozen_detected, &status);
    else
        pci_walk_bridge(bridge, report_normal_detected, &status);

    if (status == PCI_ERS_RESULT_CAN_RECOVER) {
        status = PCI_ERS_RESULT_RECOVERED;
        pci_dbg(bridge, "broadcast mmio_enabled message\n");
        pci_walk_bridge(bridge, report_mmio_enabled, &status);
    }

    if (status == PCI_ERS_RESULT_NEED_RESET ||
        state == pci_channel_io_frozen) {
        if (reset_subordinates(bridge) != PCI_ERS_RESULT_RECOVERED) {
            pci_warn(bridge, "subordinate device reset failed\n");
            goto failed;
        }
    }

    if (status == PCI_ERS_RESULT_NEED_RESET) {
        status = PCI_ERS_RESULT_RECOVERED;
        pci_dbg(bridge, "broadcast slot_reset message\n");
        pci_walk_bridge(bridge, report_slot_reset, &status);
    }

    if (status != PCI_ERS_RESULT_RECOVERED)
        goto failed;

    pci_dbg(bridge, "broadcast resume message\n");
    pci_walk_bridge(bridge, report_resume, &status);

    if (host->native_aer || pcie_ports_native) {
        pcie_clear_device_status(dev);
        pci_aer_clear_nonfatal_status(dev);
    }

    pci_walk_bridge(bridge, pci_pm_runtime_put, NULL);
    pci_info(bridge, "device recovery successful\n");
    return status;

failed:
    pci_walk_bridge(bridge, pci_pm_runtime_put, NULL);
    pci_walk_bridge(bridge, report_perm_failure_detected, NULL);
    pci_info(bridge, "device recovery failed\n");
    return status;
```

关键决策点：
- `state == pci_channel_io_frozen` → Fatal Error → 必须 reset
- `status == PCI_ERS_RESULT_NEED_RESET` → 驱动投票要求 reset
- `status == PCI_ERS_RESULT_CAN_RECOVER` → 不需要 reset，走 mmio_enabled 路径

---

## 6. pci_driver 中的 err_handler

每个 PCI 设备驱动可以通过 `struct pci_driver` 的 `err_handler` 字段注册错误处理回调：

```c
// include/linux/pci.h
struct pci_error_handlers {
    pci_ers_result_t (*error_detected)(struct pci_dev *dev,
                                       pci_channel_state_t error);
    pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);
    pci_ers_result_t (*slot_reset)(struct pci_dev *dev);
    void (*resume)(struct pci_dev *dev);
};

// 典型驱动注册：
static struct pci_error_handlers my_err_handler = {
    .error_detected = my_error_detected,
    .slot_reset     = my_slot_reset,
    .resume         = my_resume,
};

static struct pci_driver my_driver = {
    .name         = "my_dev",
    .id_table     = my_id_table,
    .probe        = my_probe,
    .err_handler  = &my_err_handler,
};
```

回调返回值：
- `PCI_ERS_RESULT_NONE`：驱动不关心/不投票
- `PCI_ERS_RESULT_CAN_RECOVER`：MMIO 仍可用，可以恢复
- `PCI_ERS_RESULT_NEED_RESET`：需要 slot reset
- `PCI_ERS_RESULT_DISCONNECT`：设备已断开，无法恢复

---

## 7. DPC — 下游端口隔离 (`dpc.c`)

DPC (Downstream Port Containment) 是 PCIe 3.0+ 提供的一种硬件错误隔离机制。当 Downstream Port 检测到 Uncorrectable Error 时，DPC 会：

1. 立即禁用该 Port 下的链路（防止错误传播）
2. 触发中断通知软件
3. 软件执行恢复或移除受影响设备

### 7.1 DPC 控制定义 (`dpc.c:21-44`)

```c
// drivers/pci/pcie/dpc.c:21-22
#define PCI_EXP_DPC_CTL_EN_MASK    (PCI_EXP_DPC_CTL_EN_FATAL | \
                 PCI_EXP_DPC_CTL_EN_NONFATAL)

// drivers/pci/pcie/dpc.c:24-44
static const char * const rp_pio_error_string[] = {
    "Configuration Request received UR Completion",       /* Bit Position 0  */
    "Configuration Request received CA Completion",       /* Bit Position 1  */
    "Configuration Request Completion Timeout",           /* Bit Position 2  */
    NULL, NULL, NULL, NULL, NULL,
    "I/O Request received UR Completion",                 /* Bit Position 8  */
    "I/O Request received CA Completion",                 /* Bit Position 9  */
    "I/O Request Completion Timeout",                     /* Bit Position 10 */
    NULL, NULL, NULL, NULL, NULL,
    "Memory Request received UR Completion",              /* Bit Position 16 */
    "Memory Request received CA Completion",              /* Bit Position 17 */
    "Memory Request Completion Timeout",                  /* Bit Position 18 */
};
```

DPC 的 RP PIO (Root Port Programmed I/O) 扩展能进一步区分错误类型：Configuration/I/O/Memory Request 各自有 UR Completion、CA Completion、Timeout 三类错误。

### 7.2 DPC 中断处理 (`dpc.c:363-379`)

```c
// drivers/pci/pcie/dpc.c:363-379
static irqreturn_t dpc_handler(int irq, void *context)
{
    struct pci_dev *pdev = context;

    /* PCIe r6.0 sec 6.7.6: surprise removal 错误是正常副作用，应忽略 */
    if (dpc_is_surprise_removal(pdev)) {
        dpc_handle_surprise_removal(pdev);
        return IRQ_HANDLED;
    }

    pci_dev_get(pdev);
    dpc_process_error(pdev);

    /* DPC only triggers on ERR_FATAL → 执行完整恢复 */
    return IRQ_HANDLED;
}
```

DPC 中断有两种触发原因：
1. **Surprise Removal** — 设备被物理拔出，这不算真正的"错误"，只需清理状态
2. **真正的 Fatal Error** — 走 `dpc_process_error` → 最终调用 `pcie_do_recovery`

### 7.3 DPC 服务驱动注册 (`dpc.c:523-536`)

```c
// drivers/pci/pcie/dpc.c:523-536
static struct pcie_port_service_driver dpcdriver = {
    .name       = "dpc",
    .port_type  = PCIE_ANY_PORT,
    .service    = PCIE_PORT_SERVICE_DPC,
    .probe      = dpc_probe,
    .suspend    = dpc_suspend,
    .resume     = dpc_resume,
    .remove     = dpc_remove,
};

int __init pcie_dpc_init(void)
{
    return pcie_port_service_register(&dpcdriver);
}
```

DPC 作为 PCIe Port Service 驱动注册到 portdrv 框架，享有独立的 probe/remove/suspend/resume 生命周期。

### 7.4 DPC Surprise Removal 处理 (`dpc.c:328-347`)

```c
// drivers/pci/pcie/dpc.c:328-347
static void dpc_handle_surprise_removal(struct pci_dev *pdev)
{
    if (!pcie_wait_for_link(pdev, false)) {
        pci_info(pdev, "Data Link Layer Link Active not cleared in 1000 msec\n");
        goto out;
    }

    if (pdev->dpc_rp_extensions && dpc_wait_rp_inactive(pdev))
        goto out;

    pci_aer_raw_clear_status(pdev);
    pci_clear_surpdn_errors(pdev);

    pci_write_config_word(pdev, pdev->dpc_cap + PCI_EXP_DPC_STATUS,
                  PCI_EXP_DPC_STATUS_TRIGGER);

out:
    clear_bit(PCI_DPC_RECOVERED, &pdev->priv_flags);
    wake_up_all(&dpc_completed_waitqueue);
}
```

---

## 8. 总结：从 AER 中断到恢复完成的完整数据流

```
        ┌─────────────────────────────────────────┐
        │        硬件检测到 PCIe Error              │
        │        Root Port 触发 MSI-X 中断          │
        └──────────────────┬──────────────────────┘
                           │
        ╔══════════════════╧══════════════════════╗
        ║         aer_irq()  [hardIRQ]             ║  aer.c:1470
        ║  ┌─ 读 PCI_ERR_ROOT_STATUS               ║
        ║  ├─ 读 PCI_ERR_ROOT_ERR_SRC              ║
        ║  ├─ 写 PCI_ERR_ROOT_STATUS 清除状态       ║
        ║  ├─ kfifo_put → aer_fifo                 ║
        ║  └─ 返回 IRQ_WAKE_THREAD                 ║
        ╚══════════════════╤══════════════════════╝
                           │
        ╔══════════════════╧══════════════════════╗
        ║         aer_isr()  [threaded]            ║  aer.c:1449
        ║  ┌─ kfifo_get → aer_err_source           ║
        ║  └─ aer_isr_one_error()                  ║
        ║      ├─ aer_isr_one_error_type()         ║  aer.c:1373
        ║      │  ├─ find_source_device()          ║
        ║      │  └─ aer_process_err_devices()     ║
        ║      │     └─ 根据 severity 决定恢复等级   ║
        ╚══════════╤══════════════════════════════╝
                   │
        ╔══════════╧══════════════════════════════╗
        ║      pcie_do_recovery()  [err.c:210]     ║
        ║                                          ║
        ║  ┌─ Stage 1: error_detected 广播          ║  err.c:239
        ║  │  遍历 bridge 下所有 device              ║
        ║  │  调用 drv->err_handler->error_detected ║
        ║  │  收集投票结果 merge_result()            ║
        ║  │                                        ║
        ║  ├─ Stage 2: mmio_enabled 广播 (可选)     ║  err.c:247
        ║  │  仅当 status == CAN_RECOVER            ║
        ║  │                                        ║
        ║  ├─ Stage 3: reset_subordinates()         ║  err.c:252
        ║  │  执行硬件 reset (FLR/SBR)              ║
        ║  │                                        ║
        ║  ├─ Stage 4: slot_reset 广播              ║  err.c:266
        ║  │  驱动重新初始化硬件                      ║
        ║  │                                        ║
        ║  ├─ Stage 5: resume 广播                  ║  err.c:273
        ║  │  驱动恢复 I/O、重开中断                  ║
        ║  │  发送 KOBJ_CHANGE uevent               ║
        ║  │                                        ║
        ║  └─ 清理: 清除 AER 状态                    ║  err.c:281-283
        ║     pm_runtime_put                        ║
        ╚══════════════════════════════════════════╝
```

驱动要参与 AER 恢复只需实现 `struct pci_error_handlers` 中的回调并挂到 `pci_driver.err_handler`。内核已经做好了广播、投票、状态管理、uevent 通知的全部框架。

---

## 下一篇文章

热插拔与电源管理是 PCIe 的另一块核心基础设施——Native Hotplug 如何检测插拔事件、ASPM 如何在不同功耗状态间切换，详见[第六篇](./06-hotplug-pm.md)。
