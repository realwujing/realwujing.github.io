# 第4篇：ATS/PRI — 页表共享的最后一块拼图

> 源码：`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c`, `drivers/pci/ats.c`, `drivers/iommu/io-pgfault.c`, `include/linux/iommu.h`

系列目录：[SoC 统一内存架构深度解析](./README.md)

---

## 1. 问题：没有 ATS/PRI 会怎样？

想象 SoC 上的 GPU 每做一次 DMA 访问：

1. GPU 发出 IOVA 地址
2. SMMU 进行 Stage-1 页表遍历（可能 4 级页表，16 次内存访问）
3. SMMU 将 IOVA 翻译为 PA，返回给 GPU
4. GPU 用 PA 访问内存

没有 ATS 时，**每次内存访问都要经过完整的 SMMU 页表遍历**。这对 GPU（大量并发内存访问）来说是不可接受的延迟。

GPU 需要两样东西：
- **本地 TLB 缓存**：把翻译结果缓存在 GPU 自己的 ATC（Address Translation Cache）里，避免重复走 SMMU
- **缺页请求能力**：当 ATC miss 且 SMMU 页表也没有时，GPU 能主动通知 OS 来填充页表

ATS 解决第一个问题，PRI 解决第二个。

---

## 2. 三个协议

### 2.1 ATS — Address Translation Service

ATS 是 PCIe 协议扩展。启用后：

- GPU 内部维护 **ATC**（Address Translation Cache），缓存 IOVA→PA 的翻译条目
- GPU 在做 DMA 之前，先查 ATC。如果命中（HIT），直接用缓存的 PA 访问内存
- 如果 ATC miss，GPU 向 SMMU 发送 **Translation Request**，SMMU 遍历页表后返回翻译结果，GPU 缓存在 ATC
- 当 OS 修改页表（如 mprotect、munmap）时，OS 必须发送 **ATC Invalidation** 通知 GPU 刷新对应缓存行

关键参数 **STU**（Smallest Translation Unit）：告诉 PCIe 设备 SMMU 支持的最小页大小。GPU 据此决定 ATC 条目的覆盖范围。

### 2.2 PRI — Page Request Interface

PRI 是 PCIe 协议扩展，处理 **SMMU 页表也没有这个映射** 的情况：

1. GPU ATC miss
2. GPU 向 SMMU 发送 Translation Request
3. SMMU 页表遍历也 miss（该 IOVA 确实没映射）
4. 如果 SMMU 启用了 PRI，GPU 发送 **Page Request**（不是 Translation Request），说"我需要这个页"
5. SMMU 通过 IOPF 框架通知 OS
6. OS 缺页处理 handler 填充页表（调用 `hmm_range_fault`）
7. OS 通过 `arm_smmu_page_response` 发送 **Page Response** 给 GPU
8. GPU 重试，这次 ATC 命中，成功

区分两种失败：
- **Translation Request 失败**：页表存在但出了别的问题（如权限位不对），PRI 不触发
- **页表没有映射**：这是 PRI 的用武之地

### 2.3 PASID — Process Address Space ID

PRI 引入一个关键概念：当 GPU 被多个进程共享时（每个进程有独立地址空间），Page Request 需要带上 **PASID** 来标识是哪个进程缺了页。

- 类似 CPU 的 ASID，但用于 PCIe 设备
- SMMU 用 PASID 选择对应的 Stage-1 页表（即对应进程的 CPU 页表）
- `iommu_fault_page_request` 结构体（`include/linux/iommu.h:75`）中的 `pasid` 字段承载这个 ID

---

## 3. ATS 启用：arm-smmu-v3 实现

### 3.1 能力检查 — `arm_smmu_ats_supported`

`arm-smmu-v3.c:3039-3051`：

```c
static bool arm_smmu_ats_supported(struct arm_smmu_master *master)
{
	struct device *dev = master->dev;
	struct arm_smmu_device *smmu = master->smmu;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (!(smmu->features & ARM_SMMU_FEAT_ATS))        // line 3045
		return false;

	if (!(fwspec->flags & IOMMU_FWSPEC_PCI_RC_ATS))    // line 3048
		return false;

	return dev_is_pci(dev) && pci_ats_supported(to_pci_dev(dev)); // line 3051
}
```

三个条件缺一不可：
1. **SMMU 硬件支持 ATS**：`ARM_SMMU_FEAT_ATS` 在 SMMU 初始化时从 IDR 寄存器读取
2. **Root Complex 支持 ATS**：`IOMMU_FWSPEC_PCI_RC_ATS` 由固件（ACPI/DT）声明
3. **PCI 设备支持 ATS**：通过 `pci_ats_supported()` 检查 PCIe 扩展能力寄存器

### 3.2 ATS 启用 — `arm_smmu_enable_ats`

`arm-smmu-v3.c:3054-3070`：

```c
static void arm_smmu_enable_ats(struct arm_smmu_master *master)
{
	size_t stu;
	struct pci_dev *pdev;
	struct arm_smmu_device *smmu = master->smmu;

	/* Smallest Translation Unit: log2 of the smallest supported granule */
	stu = __ffs(smmu->pgsize_bitmap);                  // line 3061
	pdev = to_pci_dev(master->dev);

	/*
	 * ATC invalidation of PASID 0 causes the entire ATC to be flushed.
	 */
	arm_smmu_atc_inv_master(master, IOMMU_NO_PASID);   // line 3067
	if (pci_enable_ats(pdev, stu))                      // line 3068
		dev_err(master->dev, "Failed to enable ATS (STU %zu)\n", stu);
}
```

关键步骤：
1. **计算 STU**（line 3061）：`__ffs(smmu->pgsize_bitmap)` 取 `pgsize_bitmap` 最低置位的位置。例如 SMMU 支持 4K/64K，最低是 4K → STU=12
2. **清空 ATC**（line 3067）：在启用 ATS 之前，通过 CMDQ 发送 ATC_INV 命令（PASID=0 表示刷全部），确保没有脏缓存
3. **写 PCIe 配置空间**（line 3068）：`pci_enable_ats` 设置 `PCI_ATS_CTRL_ENABLE` 位 + STU 值

---

## 4. PCI ATS/PRI 核心实现

### 4.1 `pci_enable_ats` — `drivers/pci/ats.c:90-121`

```c
int pci_enable_ats(struct pci_dev *dev, int ps)
{
	u16 ctrl;

	if (!pci_ats_supported(dev))      // line 95: 能力寄存器检查
		return -EINVAL;
	if (WARN_ON(dev->ats_enabled))    // line 98: 不能重复使能
		return -EBUSY;
	if (ps < PCI_ATS_MIN_STU)         // line 101: STU 不能小于 PCI ATS 最小值(12)
		return -EINVAL;

	ctrl = PCI_ATS_CTRL_ENABLE;       // line 108: 设置使能位
	if (!dev->is_virtfn) {
		dev->ats_stu = ps;            // line 114: 记录 STU
		ctrl |= PCI_ATS_CTRL_STU(dev->ats_stu - PCI_ATS_MIN_STU); // line 115
	}
	pci_write_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, ctrl); // line 117

	dev->ats_enabled = 1;             // line 119
	return 0;
}
```

核心操作就是写 PCIe ATS Capability 的 Control 寄存器（line 117）。`PCI_ATS_CTRL_STU` 字段告诉设备 SMMU 的最小支持粒度。

### 4.2 `pci_disable_ats` — `drivers/pci/ats.c:128-141`

```c
void pci_disable_ats(struct pci_dev *dev)
{
	u16 ctrl;
	if (WARN_ON(!dev->ats_enabled))
		return;

	pci_read_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, &ctrl);  // line 135
	ctrl &= ~PCI_ATS_CTRL_ENABLE;                                    // line 136
	pci_write_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, ctrl);  // line 137

	dev->ats_enabled = 0;                                            // line 139
}
```

清零 `PCI_ATS_CTRL_ENABLE` 位。

### 4.3 `pci_enable_pri` — `drivers/pci/ats.c:230-268`

```c
int pci_enable_pri(struct pci_dev *pdev, u32 reqs)
{
	u16 control, status;
	u32 max_requests;
	int pri = pdev->pri_cap;

	if (pdev->is_virtfn) {                         // line 241: VF 共享 PF 的 PRI
		if (pci_physfn(pdev)->pri_enabled)
			return 0;
		return -EINVAL;
	}
	if (WARN_ON(pdev->pri_enabled))                // line 247
		return -EBUSY;
	if (!pri)                                      // line 250: 无 PRI capability
		return -EINVAL;

	pci_read_config_word(pdev, pri + PCI_PRI_STATUS, &status);        // line 253
	if (!(status & PCI_PRI_STATUS_STOPPED))                           // line 254
		return -EBUSY;  // PRI 必须处于 STOPPED 状态才能启动

	pci_read_config_dword(pdev, pri + PCI_PRI_MAX_REQ, &max_requests);// line 257
	reqs = min(max_requests, reqs);                                   // line 258
	pdev->pri_reqs_alloc = reqs;
	pci_write_config_dword(pdev, pri + PCI_PRI_ALLOC_REQ, reqs);      // line 260

	control = PCI_PRI_CTRL_ENABLE;                                    // line 262
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);         // line 263

	pdev->pri_enabled = 1;                                            // line 265
	return 0;
}
```

PRI 启用的关键约束：
- line 254：PRI 状态寄存器必须处于 **STOPPED**（停止），否则说明上一个使用者的 Page Request 还没清干净
- line 258：请求数不能超过硬件能力（`max_requests`），取最小值
- line 260：`PCI_PRI_ALLOC_REQ` 写入申请的 outstanding request 数量
- line 263：设置 `PCI_PRI_CTRL_ENABLE` 启动 PRI

### 4.4 `pci_reset_pri` — `drivers/pci/ats.c:329-347`

```c
int pci_reset_pri(struct pci_dev *pdev)
{
	u16 control;
	int pri = pdev->pri_cap;

	if (pdev->is_virtfn)                           // line 334
		return 0;
	if (WARN_ON(pdev->pri_enabled))                // line 337: PRI 必须先 disable
		return -EBUSY;
	if (!pri)                                      // line 340
		return -EINVAL;

	control = PCI_PRI_CTRL_RESET;                  // line 343
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);  // line 344

	return 0;
}
```

PRI 复位写 `PCI_PRI_CTRL_RESET`（line 343），必须在 PRI 已停止状态下操作（line 337）。

---

## 5. IOPF 框架

### 5.1 核心数据结构（`include/linux/iommu.h`）

**`iommu_fault_page_request` — line 75**：

```c
struct iommu_fault_page_request {
#define IOMMU_FAULT_PAGE_REQUEST_PASID_VALID   (1 << 0)  // line 76
#define IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE     (1 << 1)  // line 77
#define IOMMU_FAULT_PAGE_RESPONSE_NEEDS_PASID  (1 << 2)  // line 78
    u32 flags;       // PASID 是否有效、是否是最后一页、响应是否需要 PASID
    u32 pasid;       // 进程地址空间 ID
    u32 grpid;       // Page Request Group 索引
    u32 perm;        // 请求的权限（读/写）
    u64 addr;        // 缺页地址
    u64 private_data[2]; // 设备私有数据
};
```

**`iopf_group` — line 130**：

```c
struct iopf_group {
    struct iopf_fault last_fault;      // 这组的最后一个故障
    struct list_head faults;           // 故障链表
    size_t fault_count;                // 故障计数
    struct list_head pending_node;     // 挂入 iommu_fault_param::faults
    struct work_struct work;           // 工作队列
    struct iommu_attach_handle *attach_handle; // 指向 domain 的句柄
    struct iommu_fault_param *fault_param;    // 设备故障参数
    struct list_head node;             // handler 的自定义链表
    u32 cookie;
};
```

**`iopf_queue` — line 151**：

```c
struct iopf_queue {
    struct workqueue_struct *wq;   // 故障处理工作队列
    struct list_head devices;      // 挂靠此队列的设备列表
    struct mutex lock;
};
```

**`iommu_fault_param` — line 819**：

```c
struct iommu_fault_param {
    struct mutex lock;
    refcount_t users;
    struct rcu_head rcu;

    struct device *dev;
    struct iopf_queue *queue;          // line 825: 关联的 IOPF 队列
    struct list_head queue_list;       // 设备在队列中的链表节点

    struct list_head partial;          // line 828: 未完成的 partial faults
    struct list_head faults;           // line 829: 等待响应的故障
};
```

关键理解：`partial` 和 `faults` 的区别：
- 硬件可能把一批相关的缺页请求（同 `grpid`）分成多次报告，`partial` 暂存尚不完整的批次
- 当标记 `PAGE_REQUEST_LAST_PAGE` 的请求到达时，整个批次从 `partial` 移到 `faults`，触发 handler

### 5.2 `find_fault_handler` — `io-pgfault.c:118-156`

```c
static struct iommu_attach_handle *find_fault_handler(struct device *dev,
                                                       struct iopf_fault *evt)
{
    struct iommu_fault *fault = &evt->fault;

    if (fault->prm.flags & IOMMU_FAULT_PAGE_REQUEST_PASID_VALID) {  // line 124
        // 有 PASID：查找对应 PASID 的 attach_handle
        attach_handle = iommu_attach_handle_get(dev->iommu_group,
                fault->prm.pasid, 0);                               // line 125
        if (IS_ERR(attach_handle)) {
            const struct iommu_ops *ops = dev_iommu_ops(dev);
            if (!ops->user_pasid_table)
                return NULL;
            // 用户态管理的 PASID table → 用 NESTED domain
            attach_handle = iommu_attach_handle_get(
                    dev->iommu_group, IOMMU_NO_PASID,
                    IOMMU_DOMAIN_NESTED);                            // line 138
        }
    } else {
        // 无 PASID：用 RID（设备本身）查找
        attach_handle = iommu_attach_handle_get(dev->iommu_group,
                IOMMU_NO_PASID, 0);                                 // line 145
    }

    if (!attach_handle->domain->iopf_handler)                        // line 152
        return NULL;

    return attach_handle;
}
```

两层查找策略（line 124-152）：
1. **有 PASID**：以 `(group, pasid, 0)` 查找——这是 SVA 场景，每个进程有独立地址空间
2. **无 PASID**：以 `(group, IOMMU_NO_PASID, 0)` 查找——RID 级别的 handler
3. 特殊路径（line 130-138）：如果 IOMMU 驱动支持用户态管理 PASID table，则 fallback 到 NESTED domain 的 RID handler
4. **必须检查 `iopf_handler` 存在**（line 152），否则返回 NULL，故障被丢弃

### 5.3 `iommu_report_device_fault` — `io-pgfault.c:214-283`

这是 IOPF 框架的核心入口，被 SMMU 驱动的故障处理程序调用。

```c
int iommu_report_device_fault(struct device *dev, struct iopf_fault *evt)
{
    struct iommu_fault *fault = &evt->fault;
    struct iommu_fault_param *iopf_param;
    struct iopf_group abort_group = {};
    struct iopf_group *group;

    // Step 1: 找到对应的 handler
    attach_handle = find_fault_handler(dev, evt);           // line 222
    if (!attach_handle)
        goto err_bad_iopf;

    // Step 2: 获取设备的 fault_param
    iopf_param = iopf_get_dev_fault_param(dev);             // line 230
    if (WARN_ON(!iopf_param))
        goto err_bad_iopf;

    // Step 3: 不是最后一页 → 暂存到 partial 列表
    if (!(fault->prm.flags & IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE)) {  // line 234
        ret = report_partial_fault(iopf_param, fault);      // line 237
        iopf_put_dev_fault_param(iopf_param);
        return ret;   // 不发送响应，等待最后一条
    }

    // Step 4: 最后一条 → 组建完整的 group，调用 handler
    group = iopf_group_alloc(iopf_param, evt, &abort_group);  // line 252
    if (group == &abort_group)
        goto err_abort;

    group->attach_handle = attach_handle;                   // line 256

    // Step 5: 调用 domain 的 iopf_handler
    if (group->attach_handle->domain->iopf_handler(group))  // line 262
        goto err_abort;

    return 0;

err_abort:
    iopf_group_response(group, IOMMU_PAGE_RESP_FAILURE);    // line 270
    ...
err_bad_iopf:
    if (fault->type == IOMMU_FAULT_PAGE_REQ)
        iopf_error_response(dev, evt);                      // line 280
    return -EINVAL;
}
```

流程图解：

```
SMMU 故障处理 ──→ iommu_report_device_fault
                      │
                      ├─ find_fault_handler (PASID 或 RID 查找)
                      │     └─ 找不到 → err_bad_iopf → iopf_error_response
                      │
                      ├─ iopf_get_dev_fault_param
                      │     └─ 没有 param → err_bad_iopf
                      │
                      ├─ PAGE_REQUEST_LAST_PAGE?
                      │     ├─ N: report_partial_fault → 暂存，直接返回
                      │     └─ Y: iopf_group_alloc → 组装完整 group
                      │
                      └─ domain->iopf_handler(group)  ← 这就是实际缺页处理
                            ├─ 成功 → 返回 0
                            └─ 失败 → iopf_group_response(FAILURE)
```

关键行：
- **line 234**：检查 `IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE` 标志位，判断是否需要等待更多 partial faults
- **line 262**：调用 `domain->iopf_handler(group)` — 这是 driver 注册的回调（例如 GPU SVM 的 handler，内部会调用 `hmm_range_fault`）

### 5.4 `iopf_group_response` — `io-pgfault.c:322-343`

```c
void iopf_group_response(struct iopf_group *group,
                         enum iommu_page_response_code status)
{
    struct iommu_fault_param *fault_param = group->fault_param;
    struct iopf_fault *iopf = &group->last_fault;
    struct device *dev = group->fault_param->dev;
    const struct iommu_ops *ops = dev_iommu_ops(dev);
    struct iommu_page_response resp = {
        .pasid = iopf->fault.prm.pasid,          // line 329
        .grpid = iopf->fault.prm.grpid,          // line 330
        .code = status,                           // line 331
    };

    mutex_lock(&fault_param->lock);
    if (!list_empty(&group->pending_node)) {
        ops->page_response(dev, &group->last_fault, &resp);  // line 338
        list_del_init(&group->pending_node);                 // line 339
    }
    mutex_unlock(&fault_param->lock);
}
```

这里的 `ops->page_response` 最终调用到 SMMU 驱动的 `arm_smmu_page_response`。

### 5.5 `arm_smmu_page_response` — `arm-smmu-v3.c:997-1029`

```c
static void arm_smmu_page_response(struct device *dev, struct iopf_fault *unused,
                                   struct iommu_page_response *resp)
{
    struct arm_smmu_cmdq_ent cmd = {0};
    struct arm_smmu_master *master = dev_iommu_priv_get(dev);
    int sid = master->streams[0].id;

    if (WARN_ON(!master->stall_enabled))                // line 1004
        return;

    cmd.opcode        = CMDQ_OP_RESUME;                 // line 1007
    cmd.resume.sid    = sid;                            // line 1008
    cmd.resume.stag   = resp->grpid;                    // line 1009

    switch (resp->code) {
    case IOMMU_PAGE_RESP_INVALID:
    case IOMMU_PAGE_RESP_FAILURE:
        cmd.resume.resp = CMDQ_RESUME_0_RESP_ABORT;     // line 1013
        break;
    case IOMMU_PAGE_RESP_SUCCESS:
        cmd.resume.resp = CMDQ_RESUME_0_RESP_RETRY;     // line 1016
        break;
    }

    arm_smmu_cmdq_issue_cmd(master->smmu, &cmd);        // line 1022
}
```

三种响应码的映射（line 1010-1017）：

| IOPF 响应码 | SMMU 命令 | 意义 |
|-------------|-----------|------|
| `IOMMU_PAGE_RESP_SUCCESS` | `CMDQ_RESUME_0_RESP_RETRY` | 页表已填充，GPU 重试，本次命中 |
| `IOMMU_PAGE_RESP_FAILURE` | `CMDQ_RESUME_0_RESP_ABORT` | 无法处理，终止事务 |
| `IOMMU_PAGE_RESP_INVALID` | `CMDQ_RESUME_0_RESP_ABORT` | 无效请求，终止 |

注释（line 1023-1028）说明：RESUME 命令不需要显式的 SYNC — RESUME 的消费本身保证了被暂停的事务最终会被终止。

---

## 6. IOPF 队列管理

### 6.1 `arm_smmu_enable_iopf` — `arm-smmu-v3.c:3159-3193`

```c
static int arm_smmu_enable_iopf(struct arm_smmu_master *master,
                                struct arm_smmu_master_domain *master_domain)
{
    if (!IS_ENABLED(CONFIG_ARM_SMMU_V3_SVA))            // line 3166
        return -EOPNOTSUPP;

    if (!master->stall_enabled)                         // line 3174
        return 0;

    if (master->num_streams != 1)                       // line 3178
        return -EOPNOTSUPP;

    if (master->iopf_refcount) {                        // line 3181: 引用计数
        master->iopf_refcount++;
        master_domain->using_iopf = true;
        return 0;
    }

    ret = iopf_queue_add_device(master->smmu->evtq.iopf, master->dev); // line 3187
    if (ret)
        return ret;
    master->iopf_refcount = 1;                          // line 3190
    master_domain->using_iopf = true;                   // line 3191
    return 0;
}
```

两个关键点：
- **line 3174**：只有启用了 stall 模式的设备才需要 IOPF。`stall_enabled` 意味着 SMMU 故障时不直接 abort 事务，而是暂停（stall）等 OS 响应
- **line 3187**：`iopf_queue_add_device` 将设备注册到 SMMU 事件队列的 IOPF queue 中

### 6.2 `arm_smmu_disable_iopf` — `arm-smmu-v3.c:3195-3209`

```c
static void arm_smmu_disable_iopf(struct arm_smmu_master *master,
                                  struct arm_smmu_master_domain *master_domain)
{
    if (!IS_ENABLED(CONFIG_ARM_SMMU_V3_SVA))            // line 3200
        return;

    if (!master_domain || !master_domain->using_iopf)   // line 3203
        return;

    master->iopf_refcount--;                            // line 3206
    if (master->iopf_refcount == 0)
        iopf_queue_remove_device(master->smmu->evtq.iopf, master->dev); // line 3208
}
```

引用计数管理：一个 master 可能被多个 domain 使用，只有当 `iopf_refcount` 归零时才真正从 IOPF queue 移除（line 3206-3208）。

---

## 7. 完整时序图

```
  GPU                SMMU              IOPF                OS Handler         CPU MMU
   │                  │                  │                    │                  │
   │  DMA Read(IOVA)  │                  │                    │                  │
   │─────────────────►│                  │                    │                  │
   │                  │                  │                    │                  │
   │  ATC miss        │                  │                    │                  │
   │  Translation Req │                  │                    │                  │
   │─────────────────►│                  │                    │                  │
   │                  │ 页表遍历 miss     │                    │                  │
   │                  │ (该 IOVA 无映射)  │                    │                  │
   │                  │                  │                    │                  │
   │  PRI Page Req    │                  │                    │                  │
   │─────────────────►│                  │                    │                  │
   │                  │  EVTQ 入队       │                    │                  │
   │                  │─────────────────►│                    │                  │
   │                  │                  │ find_fault_handler │                  │
   │                  │                  │──────────┐         │                  │
   │                  │                  │          │         │                  │
   │                  │                  │ LAST_PAGE? group成立                  │
   │                  │                  │          │         │                  │
   │                  │                  │ iopf_handler(group)                  │
   │                  │                  │─────────►│                            │
   │                  │                  │          │ hmm_range_fault()          │
   │                  │                  │          │───────────────────────────►│
   │                  │                  │          │          ◄───PFN[]─────────│
   │                  │                  │          │                            │
   │                  │                  │          │ arm_lpae_map_pages()       │
   │                  │                  │          │──→ 写 SMMU 页表            │
   │                  │                  │          │                            │
   │                  │                  │◄─────────│ iopf_group_response(OK)    │
   │                  │                  │          │                            │
   │                  │ arm_smmu_page_response(RETRY)                            │
   │                  │◄─────────────────│          │                            │
   │                  │                  │          │                            │
   │  CMDQ_RESUME     │                  │          │                            │
   │  RESP_RETRY      │                  │          │                            │
   │◄─────────────────│                  │          │                            │
   │                  │                  │          │                            │
   │  GPU 重试事务     │                  │          │                            │
   │─────────────────►│                  │          │                            │
   │                  │ 页表遍历 HIT      │          │                            │
   │  ◄── PA ─────────│                  │          │                            │
   │                  │                  │          │                            │
   │  ATC 缓存本次翻译 │                  │          │                            │
   │  DMA 成功完成     │                  │          │                            │
```

---

## 8. 配置依赖

PRI/IOPF 功能**没有独立的 Kconfig 选项**。一切由 `CONFIG_ARM_SMMU_V3_SVA` 控制：

- `CONFIG_ARM_SMMU_V3_SVA=y` → 编译 `arm_smmu_enable_iopf/disable_iopf`（line 3159, 3200）
- `CONFIG_ARM_SMMU_V3_SVA=n` → 这两个函数直接返回 `-EOPNOTSUPP`
- `CONFIG_PCI_PRI` 已在 `drivers/pci/Kconfig` 中定义，但通常被 PCI_ATS 隐式选中

验证方法：
```bash
grep CONFIG_ARM_SMMU_V3_SVA /boot/config-$(uname -r)
```

---

## 下一篇文章

[第5篇：SoC 统一内存完整时序 — 从 GPU 缺页到页表填充](./05-soc-timing.md)

在本文了解了 ATS/PRI/IOPF 协议层和框架层之后，下一篇文章将串联整个调用链：从 GPU 缺页到 SMMU 事件 → IOPF → HMM → 页表写回 → GPU 重试，并对比 discrete GPU 和 SoC 两种架构在每个环节的差异。
