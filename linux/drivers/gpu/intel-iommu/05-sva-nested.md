# 第五篇：SVA、嵌套翻译与 IOMMUFD — VT-d 的未来

> 源码：`drivers/iommu/intel/svm.c` `drivers/iommu/intel/nested.c` `drivers/iommu/intel/prq.c` `drivers/iommu/intel/perfmon.c` | 系列目录：[Intel IOMMU 内核源码深度解析](./README.md)

---

## 第一部分：SVA (Shared Virtual Addressing)

## 1. SVA 概念

SVA（Shared Virtual Memory）允许设备使用与进程相同的虚拟地址空间进行 DMA，省去传统的 pin 内存 + 驱动管理 IOVA 的复杂度。GPU 计算（CUDA/OpenCL）、RDMA 网卡（HPC）是主要的应用场景。

```
传统 DMA (无 SVA)：
  进程 VA (0x7f1234567000) → 驱动 pin → PA → IOMMU 映射 → IOVA → 设备访问

SVA 模式：
  进程 VA (0x7f1234567000) → CPU PT → PA ←─ IOMMU FL PT ← 设备使用同一 VA
                       ↘    → 设备共享同一个进程页表
```

---

## 2. SVM 兼容性检查

```c
// drivers/iommu/intel/svm.c:28-48 — intel_svm_check()
void intel_svm_check(struct intel_iommu *iommu)
{
    if (!pasid_supported(iommu))
        return;

    // CPU 支持 1GB 大页 → IOMMU 必须也支持
    if (cpu_feature_enabled(X86_FEATURE_GBPAGES) &&
        !cap_fl1gp_support(iommu->cap)) {
        pr_err("%s SVM disabled, incompatible 1GB page capability\n",
               iommu->name);
        return;
    }

    // CPU 支持 LA57 (5-level) → IOMMU 必须也支持
    if (cpu_feature_enabled(X86_FEATURE_LA57) &&
        !cap_fl5lp_support(iommu->cap)) {
        pr_err("%s SVM disabled, incompatible paging mode\n",
               iommu->name);
        return;
    }

    iommu->flags |= VTD_FLAG_SVM_CAPABLE;
}
```

SVM 的三个前置条件：
1. **PASID 支持**（前文）
2. **1GB 大页兼容**：CPU 和 IOMMU 双方都必须支持 `GBPAGES`
3. **5-level paging (LA57) 兼容**：CPU 和 IOMMU 双方页表级数必须一致

如果条件不满足，`VTD_FLAG_SVM_CAPABLE` 不会被设置，后续所有 SVA 路径都会返回 `-ENODEV`。

---

## 3. MMU Notifier 集成

SVMMMU notifier 是 SVA 的核心组件 — 当进程页表变更（munmap/mprotect/madvise）时，IOMMU 的 IOTLB 必须同步刷新。

### 3.1 TLB 失效通知

```c
// drivers/iommu/intel/svm.c:51-68 — intel_arch_invalidate_secondary_tlbs()
/*
 * Pages have been freed at this point
 */
static void intel_arch_invalidate_secondary_tlbs(struct mmu_notifier *mn,
                                                 struct mm_struct *mm,
                                                 unsigned long start,
                                                 unsigned long end)
{
    struct dmar_domain *domain =
        container_of(mn, struct dmar_domain, notifier);

    if (start == 0 && end == ULONG_MAX) {
        cache_tag_flush_all(domain);       // 全局刷新
        return;
    }

    // 将 mm 的 [start, end) 转换为 IOMMU 的 [start, end-1]
    cache_tag_flush_range(domain, start, end - 1, 0);
}
```

关键设计点：
- `start=0, end=ULONG_MAX` 表示进程退出，需要全局刷新
- mm 的 `vm_end` 是 **开区间** 的末尾，IOMMU 使用 **闭区间**：需要 `end - 1`

### 3.2 进程退出回掉

```c
// drivers/iommu/intel/svm.c:70-97 — intel_mm_release()
static void intel_mm_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
    struct dmar_domain *domain =
        container_of(mn, struct dmar_domain, notifier);
    struct dev_pasid_info *dev_pasid;
    struct device_domain_info *info;
    unsigned long flags;

    /*
     * This might end up being called from exit_mmap(), *before* the page
     * tables are cleared. And __mmu_notifier_release() will delete us from
     * the list of notifiers so that our invalidate_range() callback doesn't
     * get called when the page tables are cleared. So we need to protect
     * against hardware accessing those page tables.
     *
     * We do it by clearing the entry in the PASID table and then flushing
     * the IOTLB and the PASID table caches. This might upset hardware;
     * perhaps we'll want to point the PASID to a dummy PGD (like the zero
     * page) so that we end up taking a fault that the hardware really
     * *has* to handle gracefully without affecting other processes.
     */
    spin_lock_irqsave(&domain->lock, flags);
    list_for_each_entry(dev_pasid, &domain->dev_pasids, link_domain) {
        info = dev_iommu_priv_get(dev_pasid->dev);
        intel_pasid_tear_down_entry(info->iommu, dev_pasid->dev,
                                    dev_pasid->pasid, true);
    }
    spin_unlock_irqrestore(&domain->lock, flags);
}
```

进程退出时通过 `intel_pasid_tear_down_entry` 清除所有关联的 PASID entry，防止设备继续访问已释放的进程页表。

### 3.3 MMU Notifier 操作表

```c
// drivers/iommu/intel/svm.c:107-111
static const struct mmu_notifier_ops intel_mmuops = {
    .release = intel_mm_release,
    .arch_invalidate_secondary_tlbs = intel_arch_invalidate_secondary_tlbs,
    .free_notifier = intel_mm_free_notifier,
};
```

---

## 4. SVA Domain 分配与附加

### 4.1 SVA Domain 分配

```c
// drivers/iommu/intel/svm.c:207-234 — intel_svm_domain_alloc()
struct iommu_domain *intel_svm_domain_alloc(struct device *dev,
                                            struct mm_struct *mm)
{
    struct dmar_domain *domain;
    int ret;

    ret = intel_iommu_sva_supported(dev);  // PASID+ATS+?PRI 全部检查
    if (ret)
        return ERR_PTR(ret);

    domain = kzalloc_obj(*domain);

    domain->domain.ops = &intel_svm_domain_ops;
    INIT_LIST_HEAD(&domain->dev_pasids);
    INIT_LIST_HEAD(&domain->cache_tags);
    spin_lock_init(&domain->cache_lock);
    spin_lock_init(&domain->lock);

    domain->notifier.ops = &intel_mmuops;
    ret = mmu_notifier_register(&domain->notifier, mm);
    // 注册到目标 mm 的 notifier 链表中
    ...

    return &domain->domain;
}
```

### 4.2 附加 PASID

```c
// drivers/iommu/intel/svm.c:148-192 — intel_svm_set_dev_pasid()
static int intel_svm_set_dev_pasid(struct iommu_domain *domain,
                                   struct device *dev, ioasid_t pasid,
                                   struct iommu_domain *old)
{
    struct device_domain_info *info = dev_iommu_priv_get(dev);
    struct intel_iommu *iommu = info->iommu;
    struct mm_struct *mm = domain->mm;

    ret = intel_iommu_sva_supported(dev);       // 再次验证

    dev_pasid = domain_add_dev_pasid(domain, dev, pasid);  // 设备→PASID→Domain 绑定

    // 如果设备支持 PRI，设置 PRI 页面故障框架
    if (info->pri_supported) {
        ret = iopf_for_domain_replace(domain, old, dev);
    }

    // 设置 PASID entry 为 first-level 模式
    sflags = cpu_feature_enabled(X86_FEATURE_LA57) ? PASID_FLAG_FL5LP : 0;
    sflags |= PASID_FLAG_PWSNP;          // 写允许+非snoop
    ret = __domain_setup_first_level(iommu, dev, pasid,
                                     FLPT_DEFAULT_DID, __pa(mm->pgd),  // 使用进程 PGD!
                                     sflags, old);

    domain_remove_dev_pasid(old, dev, pasid);
    return 0;
}
```

核心步骤：
1. **验证设备兼容性**：PASID + ATS 已启用
2. **将进程 PGD (Page Global Directory) 填入 PASID entry** 的 `flptr` 字段
3. **设置 flags**：`PASID_FLAG_FL5LP`（如果需要 LA57）、`PASID_FLAG_PWSNP`
4. **IOPF (I/O Page Fault) 框架**：如果设备支持 PRI，注册故障处理

### 4.3 SVA 附加条件检查

```c
// drivers/iommu/intel/svm.c:113-146 — intel_iommu_sva_supported()
static int intel_iommu_sva_supported(struct device *dev)
{
    struct device_domain_info *info = dev_iommu_priv_get(dev);
    struct intel_iommu *iommu;

    iommu = info->iommu;
    if (!iommu) return -EINVAL;

    if (!(iommu->flags & VTD_FLAG_SVM_CAPABLE))
        return -ENODEV;

    if (!info->pasid_enabled || !info->ats_enabled)
        return -EINVAL;

    // 设备驱动的非PRI IOPF（设备自己对故障处理）不需要硬件 PRI
    if (!info->pri_supported)
        return 0;

    // 支持 PRI 的设备必须启用 PRI
    if (!info->pri_enabled)
        return -EINVAL;

    return 0;
}
```

---

## 第二部分：嵌套翻译

## 5. 嵌套翻译概念

嵌套翻译（Nested Translation）在虚拟化场景中至关重要：**guest 的第一级页表（GVA→GPA）嵌套在 host 的第二级页表（GPA→HPA）之上**。

```
嵌套翻译两层走查:
│
├── 第一级 (Stage-1):  guest 控制
│   └── GVA → GPA   (guest page table, first-level format)
│       └── 由 guest VM 管理, host 不干预内容
│
├── 第二级 (Stage-2):  host 控制
│   └── GPA → HPA   (VT-d second-level page table)
│       └── 由 host/VMM 管理
│
└── 最终结果: GVA → HPA = S1_walk(S2_walk(GVA))
```

```c
// drivers/iommu/intel/iommu.h:631 — s2_domain 指向第二级父域
struct dmar_domain {
    ...
    /* 嵌套用户域 */
    struct {
        struct dmar_domain *s2_domain;        /* 第二级父域 */
        struct iommu_hwpt_vtd_s1 s1_cfg;       /* 第一级配置 */
        struct list_head s2_link;              /* 父域子域链接 */
    };
    ...
};
```

---

## 6. 嵌套域操作

### 6.1 域分配

```c
// drivers/iommu/intel/nested.c:196 — intel_iommu_domain_alloc_nested()
intel_iommu_domain_alloc_nested(struct device *dev,
                                struct iommu_domain *parent,
                                u32 flags,
                                const struct iommu_user_data *user_data)
{
    // 检查 ecap_nest 能力
    // 从 user_data 复制 s1_cfg (first-level 配置)
    // 绑定 parent (s2_domain)
    ...
}
```

### 6.2 设备附加

```c
// drivers/iommu/intel/nested.c:21-77 — intel_nested_attach_dev()
static int intel_nested_attach_dev(struct iommu_domain *domain,
                                   struct device *dev,
                                   struct iommu_domain *old)
{
    struct device_domain_info *info = dev_iommu_priv_get(dev);
    struct dmar_domain *dmar_domain = to_dmar_domain(domain);
    struct intel_iommu *iommu = info->iommu;
    unsigned long flags;
    int ret = 0;

    device_block_translation(dev);

    // 1. 验证 S2 domain 兼容性
    ret = paging_domain_compatible(&dmar_domain->s2_domain->domain, dev);
    if (ret) return ret;

    // 2. 分配 domain_id
    ret = domain_attach_iommu(dmar_domain, iommu);
    if (ret) return ret;

    // 3. 分配 cache tag (嵌套类型)
    ret = cache_tag_assign_domain(dmar_domain, dev, IOMMU_NO_PASID);
    if (ret) goto detach_iommu;

    // 4. 设置 IOPF
    ret = iopf_for_domain_set(domain, dev);
    if (ret) goto unassign_tag;

    // 5. 设置 PASID entry 为嵌套模式
    ret = intel_pasid_setup_nested(iommu, dev,
                                   IOMMU_NO_PASID, dmar_domain);
    if (ret) goto disable_iopf;

    // 6. 将设备加入 domain 设备链表
    info->domain = dmar_domain;
    info->domain_attached = true;
    list_add(&info->link, &dmar_domain->devices);

    return 0;
    ...
}
```

### 6.3 Cache Invalidation (用户态)

```c
// drivers/iommu/intel/nested.c:91-120 — intel_nested_cache_invalidate_user()
static int intel_nested_cache_invalidate_user(struct iommu_domain *domain,
                                              struct iommu_user_data_array *array)
{
    struct dmar_domain *dmar_domain = to_dmar_domain(domain);
    struct iommu_hwpt_vtd_s1_invalidate inv_entry;
    u32 index;

    if (array->type != IOMMU_HWPT_INVALIDATE_DATA_VTD_S1)
        return -EINVAL;

    for (index = 0; index < array->entry_num; index++) {
        ret = iommu_copy_struct_from_user_array(&inv_entry, array,
                                                IOMMU_HWPT_INVALIDATE_DATA_VTD_S1,
                                                index, __reserved);
        ...
        if (!IS_ALIGNED(inv_entry.addr, VTD_PAGE_SIZE) ||
            ((inv_entry.npages == U64_MAX) && inv_entry.addr))
            return -EINVAL;
        ...
    }
}
```

这允许 **用户态 VMM** 直接通过 IOMMUFD 向硬件提交 cache invalidation 命令。

---

## 第三部分：Page Request Queue (PRI)

## 7. PRQ 硬件设计

PRQ（Page Request Queue）是 VT-d 侧实现 PCIe PRI 协议的核心组件。当设备通过 ATS 缓存了一个无效的地址转换，硬件通过 PRI 向 IOMMU 发送 "page request"。

### 7.1 `struct page_req_dsc` — 页面请求描述符

```c
// drivers/iommu/intel/prq.c:17-43
struct page_req_dsc {
    union {
        struct {
            u64 type:8;
            u64 pasid_present:1;
            u64 rsvd:7;
            u64 rid:16;           // Requestor ID (BDF)
            u64 pasid:20;         // Process Address Space ID
            u64 exe_req:1;        // Execute request
            u64 pm_req:1;         // Privileged mode request
            u64 rsvd2:10;
        };
        u64 qw_0;
    };
    union {
        struct {
            u64 rd_req:1;        // Read request
            u64 wr_req:1;        // Write request
            u64 lpig:1;          // Last Page in Group
            u64 prg_index:9;      // Page Request Group index (0-511)
            u64 addr:52;          // Request address (4KB aligned)
        };
        u64 qw_1;
    };
    u64 qw_2;
    u64 qw_3;
};
```

### 7.2 硬件 PRQ 队列

```c
// iommu.h:486-489 — PRQ 配置
#define PRQ_ORDER       4           // 2^4 = 16 pages = 64KB
#define PRQ_SIZE        (SZ_4K << PRQ_ORDER)
#define PRQ_RING_MASK   (PRQ_SIZE - 0x20)
#define PRQ_DEPTH       (PRQ_SIZE >> 5)  // 每个描述符 32 字节 → 2048 entries
```

硬件通过以下寄存器操作 PRQ（iommu.h:92-99）：
- `DMAR_PQH_REG (0xC0)` — 队列头（硬件读取）
- `DMAR_PQT_REG (0xC8)` — 队列尾（硬件写入）
- `DMAR_PQA_REG (0xD0)` — 队列地址（软件配置）
- `DMAR_PRS_REG (0xDC)` — 页面请求状态
- `DMAR_PECTL_REG (0xE0)` — 页面请求事件控制

### 7.3 PRQ 中断处理

```c
// drivers/iommu/intel/prq.c:300-347 — PRQ 初始化
int intel_iommu_init_prq(struct intel_iommu *iommu)
{
    // 1. 分配 16-page 对齐的 DMA 区域
    iommu->prq = iommu_alloc_pages_node_sz(...)

    // 2. 分配硬件 irq
    irq = dmar_alloc_hwirq(IOMMU_IRQ_ID_OFFSET_PRQ + iommu->seq_id, ...)
    iommu->pr_irq = irq;

    // 3. 创建 IOPF 队列
    iopfq = iopf_queue_alloc(iommu->iopfq_name);
    iommu->iopf_queue = iopfq;

    // 4. 注册中断线程处理程序
    ret = request_threaded_irq(irq, NULL, prq_event_thread,
                               IRQF_ONESHOT, iommu->prq_name, iommu);

    // 5. 配置硬件寄存器
    writeq(0ULL, iommu->reg + DMAR_PQH_REG);
    writeq(0ULL, iommu->reg + DMAR_PQT_REG);
    writeq(virt_to_phys(iommu->prq) | PRQ_ORDER, iommu->reg + DMAR_PQA_REG);

    init_completion(&iommu->prq_complete);
    return 0;
}
```

---

## 8. 页面故障处理流程

```
设备发出 PRI Page Request
│
├── 硬件: 设备 → PCIe PRI TLP → IOMMU PRQ 队列
│   │
│   ├── PRQ 尾指针推进 (DMAR_PQT_REG 更新)
│   └── 中断触发 (pr_irq)
│
├── 软件: prq_event_thread (prq.c)
│   │
│   ├── 读取 PRQ 条目 (page_req_dsc)
│   ├── 解析 PASID, RID, 地址, 权限
│   ├── 查找 domain (通过 RID → device_rbtree → device_domain_info.domain)
│   ├── 创建 iopf_fault 结构
│   ├── 提交到 iopf_queue
│   └── 推进头指针 (writeq → DMAR_PQH_REG)
│
├── IOPF 框架处理
│   │
│   ├── domain->iopf_handler (domain, fault)
│   │   ├── SVA: 处理进程页表缺失 (mapping 页)
│   │   └── Nested: 传递给 guest VM
│   │
│   └── page_response (成功/失败)
│       └── intel_iommu_page_response (prq.c:372)
│           ├── 构建 Page Response 描述符
│           ├── qi_submit_sync → 发送到设备
│           └── 设备通过 ATS 更新 devTLB
│
└── 设备重试 DMA 请求
```

### 8.1 页面响应

```c
// drivers/iommu/intel/prq.c:372 — intel_iommu_page_response()
void intel_iommu_page_response(struct device *dev, struct iopf_fault *evt,
                               struct iommu_page_response *msg)
{
    struct device_domain_info *info = dev_iommu_priv_get(dev);
    ...

    // 构造 Page Group Response 描述符
    desc.qw0 = QI_PGRP_PASID(pasid) | QI_PGRP_DID(did)
             | QI_PGRP_PASID_P(pasid_present) | QI_PGRP_RESP_CODE(resp_code)
             | QI_PGRP_RESP_TYPE | QI_PGRP_PRGI(prg_index);
    desc.qw1 = QI_PGRP_LPIG(result_in_group)
             | QI_PGRP_ADDR(addr);

    qi_submit_sync(iommu, &desc, 1, 0);

    if (result_in_group)
        intel_iommu_drain_pasid_prq(dev, pasid);
}
```

响应码：
- `PAGE_RESP_SUCCESS (0x00)` — 页面已映射
- `PAGE_RESP_INVALID (0x01)` — 无效请求
- `PAGE_RESP_FAILURE (0x0F)` — 无法响应

---

## 9. PASID 排水

```c
// drivers/iommu/intel/prq.c:60-99 — intel_iommu_drain_pasid_prq()
void intel_iommu_drain_pasid_prq(struct device *dev, u32 pasid)
{
    struct device_domain_info *info;
    struct dmar_domain *domain;
    struct intel_iommu *iommu;
    struct qi_desc desc[3];
    int head, tail;
    u16 sid, did;

    info = dev_iommu_priv_get(dev);
    if (!info->iopf_refcount)
        return;

    iommu = info->iommu;
    domain = info->domain;
    sid = PCI_DEVID(info->bus, info->devfn);
    did = domain ? domain_id_iommu(domain, iommu) : FLPT_DEFAULT_DID;

prq_retry:
    reinit_completion(&iommu->prq_complete);
    tail = readq(iommu->reg + DMAR_PQT_REG) & PRQ_RING_MASK;
    head = readq(iommu->reg + DMAR_PQH_REG) & PRQ_RING_MASK;

    // 扫描 PRQ，等待所有本设备的页面请求完成
    while (head != tail) {
        struct page_req_dsc *req;

        req = &iommu->prq[head / sizeof(*req)];
        if (req->rid != sid ||
            (req->pasid_present && pasid != req->pasid) ||
            (!req->pasid_present && pasid != IOMMU_NO_PASID)) {
            head = (head + sizeof(*req)) & PRQ_RING_MASK;
            continue;
        }

        wait_for_completion(&iommu->prq_complete);
        goto prq_retry;
    }

    // VT-d spec CH7.10: 排水流程
    // step 1: 等待软件队列清空
    // step 2: 发送排水 QI 命令
    // step 3: 等待硬件 PRQ 完全清空
}
```

---

## 第四部分：IOMMUFD — 用户态 IOMMU 控制面

## 10. IOMMUFD 架构

IOMMUFD (/dev/iommu) 是 Jason Gunthorpe 主导设计的用户态 IOMMU API，目标是将设备直通（device assignment）的安全隔离推入用户空间。Intel VT-d 的 nested translation 为 IOMMUFD 提供了硬件基石。

### 10.1 核心抽象

```
IOMMUFD 对象模型:
│
├── IOMMUFD_OBJ_IOAS (I/O Address Space)
│   ├── 用户态创建的 IOVA 空间
│   ├── IOAS_MAP: 用户添加的映射
│   └── IOAS_COPY: IOVA 空间拷贝
│
├── IOMMUFD_OBJ_HWPT_PAGING (Hardware Page Table — Paging)
│   ├── 直接操作的硬件页表
│   ├── 全由用户态 iopt 系统管理
│   └── 对应 Intel: first-level (GVA→HPA) 或 second-level (IOVA→HPA)
│
├── IOMMUFD_OBJ_HWPT_NESTED (Hardware Page Table — Nested)
│   ├── Stage-1 嵌套在 Stage-2 上
│   ├── 对应 Intel: PASID_ENTRY_PGTT_NESTED
│   └── S1 由用户管理, S2 由 host/VMM 管理
│
├── IOMMUFD_OBJ_VIOMMU (Virtual IOMMU)
│   ├── 安全的、虚拟化的 IOMMU 接口
│   └── 允许 guest VM 直接管理其 Stage-1 页表
│
└── IOMMUFD_OBJ_DEVICE (Device Handle)
    ├── 将 PCIe 设备绑定到 IOMMUFD
    ├── IOMMUFD_DEVICE_ATTACH: 绑定设备到 hwpt
    └── PASID: 支持 per-process 绑定
```

### 10.2 Intel 实现路径

```c
// ioctl 调用链:
ioctl(IOMMUFD, IOMMU_IOAS_MAP)
│
├── IOMMU_HWPT_ALLOC (IOMMU_HWPT_TYPE_VTD_S1 / IOMMU_HWPT_TYPE_VTD_S2)
│   ├── intel_iommu_domain_alloc_paging_flags
│   │   └── 返回 dmar_domain (fspt 或 sspt)
│   └── intel_iommu_domain_alloc_nested
│       └── 返回 dmar_domain (s2_domain 指向 parent)
│
├── IOMMU_HWPT_INVALIDATE
│   ├── intel_nested_cache_invalidate_user (nested.c:91)
│   │   └── 支持 IOMMU_HWPT_INVALIDATE_DATA_VTD_S1
│   └── qi_submit_sync 提交 QI invalidation
│
└── IOMMU_VIOMMU_ALLOC / IOMMU_VIOMMU_SET_DEV_ID
    └── 虚拟 IOMMU 设置 → guest 可以对 visiommu.vdevice_id 发起请求
```

---

## 第五部分：PerfMon

## 11. VT-d Performance Monitoring

### 11.1 `struct iommu_pmu` — 性能计数器

```c
// drivers/iommu/intel/iommu.h:658 + 736
struct iommu_pmu {
    struct pmu pmu;
    u64 filter;           /* 支持的过滤选项 */
    ...
};

struct intel_iommu {
    ...
    struct iommu_pmu *pmu;  /* iommu.h:736 */
};
```

### 11.2 PerfMon 事件

PerfMon (perfmon.c, 790 lines) 提供 Linux perf 子系统集成：

```c
// drivers/iommu/intel/perfmon.c:13-14
PMU_FORMAT_ATTR(event,          "config:0-27");   /* ES: Events Select */
PMU_FORMAT_ATTR(event_group,    "config:28-31");  /* EGI: Event Group Index */
```

PerfMon 支持的过滤器 (perfmon.c:77-79)：
- `filter_requester_id` — 按 PCIe BDF 过滤
- `filter_domain` — 按 Domain ID 过滤
- `filter_pasid` — 按 PASID 过滤
- `filter_ats` — 按 ATS 请求过滤

VT-d 4.0+ 的性能计数器允许度量：
- DMA 地址转换延迟
- IOTLB 命中率
- devTLB 命中率
- QI invalidation 延迟
- Page fault 频率

### 11.3 Hardware Register Layout

```c
// iommu.h:133-142 — PerfMon 寄存器
#define DMAR_PERFCAP_REG    0x300   /* 能力寄存器 */
#define DMAR_PERFCFGOFF_REG 0x310   /* 配置寄存器偏移 */
#define DMAR_PERFOVFOFF_REG 0x318   /* 溢出状态寄存器偏移 */
#define DMAR_PERFCNTROFF_REG 0x31c  /* 计数器偏移 */
#define DMAR_PERFINTRSTS_REG 0x324  /* 中断状态 */
#define DMAR_PERFINTRCTL_REG 0x328  /* 中断控制 */
#define DMAR_PERFEVNTCAP_REG 0x380  /* 事件能力 */
```

---

## 12. Intel VT-d vs ARM SMMUv3 功能矩阵

| 功能 | Intel VT-d | ARM SMMUv3 | 说明 |
|------|-----------|-----------|------|
| **发现方式** | ACPI DMAR | ACPI IORT / DT | VT-d 使用 DRHD, SMMUv3 使用 SMMU node |
| **DMA 页表** | 定制 x86 格式 | ARM 标准 VMSA | VT-d 使用 3/5-level x86 页表 |
| **PASID 支持** | 20-bit, VT-d 3.0+ | 20-bit SubStreamID | 功能等价 |
| **First-Level** | 与 CPU 页表兼容 | SMMU stage-1 | SVA 场景 |
| **Second-Level** | VT-d 专有格式 | SMMU stage-2 | IOVA 场景 |
| **Nested Translation** | ecap_nest (VT-d 3.4+) | Combined S1+S2 | VM guest 直通 |
| **中断重映射** | IRTE + x2APIC | GICv4 ITS | VT-d 有专用硬件 |
| **PRQ (PRI)** | Page Request Queue (VT-d 2.5+) | PRI Queue (SMMUv3.2+) | 功能等价 |
| **Qued Invl** | QI Queue | CMDQ (Command Queue) | VT-d 的 invalidation 队列 |
| **PerfMon** | VT-d 4.0+ | SMMU PMCG (SMMUv3.4+) | 性能计数器 |
| **IOMMUFD** | Nested + PRQ full support | SMMUv3 nested | 用户态控制面 |
| **Dirty Tracking** | slads (ecap bit 45) | DBM (dirty bit modifier) | 虚拟机迁移 |
| **Software Interface** | `intel_iommu_ops` (4227 LOC) | `arm_smmu_ops` | 代码量相当 |
| **总代码量** | ~11,400 LOC (12 files) | ~4,500 LOC (3 files) | VT-d 代码更大，功能更多 |

---

## 13. 完整 SVA 生命周期

```
应用启动:
│
├── 进程 A (pid=1234, mm=0xFFFFA...)
│   └── 打开 GPU 设备 (BDF=00:02.0) → fd=3
│
├── SVA 设置:
│   │
│   ├── ioctl(VFIO, VFIO_IOMMU_BIND, {fd=3, pasid=100, mm=0xFFFFA...})
│   │   │
│   │   ├── intel_svm_domain_alloc(dev, mm)
│   │   │   ├── intel_iommu_sva_supported()       ← PASID+ATS+?PRI ✓
│   │   │   ├── kzalloc(dmar_domain)
│   │   │   └── mmu_notifier_register(notifier, mm) ← 注册到进程 mm
│   │   │
│   │   ├── intel_svm_set_dev_pasid(domain, dev, 100, old)
│   │   │   ├── domain_add_dev_pasid()    ← dev+pasi→domain
│   │   │   ├── iopf_for_domain_replace()  ← 页面故障框架
│   │   │   └── __domain_setup_first_level(iommu,dev,100, __pa(mm->pgd))
│   │   │       └── intel_pasid_setup_first_level(iommu,dev, PGD,100,did,flags)
│   │   │           └── pasid_entry.flptr = PGD → PASID table update
│   │   │
│   │   └── 返回: pasid=100 绑定完成
│
├── 设备通过 VA=0x7F1234567000 发起 DMA:
│   │
│   ├── 设备 → ATS translation request (VA=0x7F...)
│   │   ├── IOMMU → First-Level PT walk (使用进程的 PGD)
│   │   │   ├── PGD → PUD → PMD → PTE → HPA
│   │   │   └── 填充 devTLB
│   │   └── ATS completion → 设备获得 HPA
│   │
│   ├── 如果 HPA 不在内存中（swap out）:
│   │   ├── IOMMU → PRQ entry (PASID=100, Addr=0x7F..., RID=BDF)
│   │   ├── prq_event_thread → iopf_queue → domain->iopf_handler
│   │   ├── SVA handler → 处理页缺失: swap in, alloc new page
│   │   ├── intel_iommu_page_response (PAGE_RESP_SUCCESS)
│   │   └── 设备重试 → 成功
│   │
│   └── DMA 完成 → 设备通过 devTLB 缓存后续访问
│
├── munmap(0x7F1234567000, ...) 或 mprotect
│   │
│   ├── mm → intel_arch_invalidate_secondary_tlbs
│   │   └── cache_tag_flush_range(domain, start, end-1, 0)
│   │       └── QI: IOTLB + devTLB invalidation for pasid=100
│
├── 进程退出 (exit/mmap_thread):
│   │
│   ├── mm → intel_mm_release
│   │   └── intel_pasid_tear_down_entry (所有 pasid=100 的 entry)
│   │       ├── PASID entry 清除
│   │       ├── IOTLB flush
│   │       ├── intel_iommu_drain_pasid_prq (等待所有 pending PRQ)
│   │       └── device_rbtree_remove (如果所有 PASID 都释放了)
│   │
│   └── 设备再访问 → 页面故障 (identity domain or blocked)
```

---

## 14. 关键行号速查

| 符号 | 文件 | 行号 | 说明 |
|------|------|------|------|
| `intel_svm_check` | svm.c | 28 | SVM 兼容性检查 |
| `intel_arch_invalidate_secondary_tlbs` | svm.c | 51 | MM 页面释放的 IOMMU TLB shootdown |
| `intel_mm_release` | svm.c | 70 | 进程退出时清除 PASID entries |
| `intel_mmuops` | svm.c | 107 | MMU notifier 操作表 |
| `intel_iommu_sva_supported` | svm.c | 113 | SVA 支持的设备条件 |
| `intel_svm_set_dev_pasid` | svm.c | 148 | 附加 PASID 到域 |
| `intel_svm_domain_ops` | svm.c | 202 | SVA domain 操作表 |
| `intel_svm_domain_alloc` | svm.c | 207 | SVA domain 分配 |
| `intel_nested_attach_dev` | nested.c | 21 | 嵌套域设备附加 |
| `intel_nested_cache_invalidate_user` | nested.c | 91 | 用户态 cache invalidation |
| `intel_nested_domain_ops` | nested.c | 188 | 嵌套域操作表 |
| `intel_iommu_domain_alloc_nested` | nested.c | 196 | 嵌套域分配 |
| `struct page_req_dsc` | prq.c | 17 | 页面请求描述符 |
| `intel_iommu_drain_pasid_prq` | prq.c | 60 | PASID 排水 |
| `intel_iommu_init_prq` | prq.c | 300 | PRQ 初始化 |
| `intel_iommu_page_response` | prq.c | 372 | 页面响应 |
| `intel_iommu_init` | iommu.c | 2552 | IOMMU 初始化入口 |
| `intel_iommu_ops` | iommu.c | 3913 | 全局 IOMMU ops |
| `IOMMU_PMU_ATTR` | perfmon.c | 52 | PerfMon 属性宏 |
| `DMAR_PERFCAP_REG` | iommu.h | 133 | PerfMon 能力寄存器 |
| `DMAR_PQH_REG` | iommu.h | 92 | PRQ 队列头 |
| `DMAR_PQA_REG` | iommu.h | 94 | PRQ 队列地址 |

---

## 15. 系列阅读顺序总结

```
01 DMAR 发现 ──→ 02 引擎与数据结构 ──→ 03 DMA 重映射
                                          │
                               ┌──────────┼──────────┐
                               ▼          ▼          ▼
                         04 中断重映射  04 PASID   05 SVA/嵌套/IOMMUFD
```

**内核代码演进**：
- VT-d 1.0 (2007): iommu.c 基础 DMA remapping
- VT-d 2.0 (2010): IRQ remapping, Queued Invalidation
- VT-d 2.5 (2013): PRI/PRQ, ATS 完整支持
- VT-d 3.0 (2016): PASID, scalable mode, SVM/SVA
- VT-d 3.4 (2020): Nested translation, IOMMUFD 基础
- VT-d 4.0 (2022): PerfMon, ECMD (Extended Commands)

**学习路径**：
1. 先读 01-03 掌握基本 DMA 重映射
2. 读 04 理解中断和 PASID 隔离
3. 读 05 掌握现代场景（GPU 计算、虚拟化、用户态控制面）
4. 对照源码 `drivers/iommu/intel/`，用 `git blame` 跟踪各功能的引入时间

---

## 系列结语

Intel IOMMU (VT-d) 是 x86 平台上 DMA 隔离和地址转换的核心组件。本系列从 ACPI DMAR 表的解析开始，逐步深入到 DMA 重映射、中断重映射、PASID 管理、SVA、嵌套翻译和 IOMMUFD 用户态 API。VT-d 驱动总共约 **11,400 行代码** 分布在 12 个文件中，是内核中最复杂的 IOMMU 驱动之一。

几个关键的未来方向：
- **IOMMUFD 全面取代 VFIO type1**：用户态控制面正在生产中取代原有的内核 DMA 映射
- **SVA 应用扩展**：GPU/加速器的 native SVM 支持更多设备
- **VT-d 5.0**：预计将引入更多虚拟化优化和安全性增强

建议继续阅读：
- [PCIe 内核源码深度解析](../pcie-deep-dive/README.md) — 理解 ATS/PRI 协议层
- [ARM SMMU 内核源码深度解析](../arm-smmu/README.md) — 对比理解另一大 IOMMU 架构
- [Intel VT-d 规范 v4.1](https://www.intel.com/content/www/us/en/content-details/790559/intel-virtualization-technology-for-directed-i-o-architecture-specification.html)
- [IOMMUFD 设计文档](https://lwn.net/Articles/943767/)
