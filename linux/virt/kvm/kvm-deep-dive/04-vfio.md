# 第4篇：设备直通 — VFIO、IOMMU 与中断重映射

> 源码：virt/kvm/vfio.c (387行) | drivers/vfio/vfio_main.c (1856行) | vmx/posted_intr.c (319行) | svm/avic.c (1321行)

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 一、设备直通总体架构

设备直通让虚拟机直接访问物理 PCIe 设备，需要三大组件协同：

```
    Guest VM
   ┌─────────┐
   │ Driver  │ → DMA (GPA) ──┐     MSI/MSI-X ──┐
   └─────────┘               │                 │
                              ▼                 ▼
   ┌──────────┐  ┌──────────────┐  ┌────────────────┐
   │  VFIO    │  │  IOMMU DMA   │  │ 中断重映射       │
   │ 设备驱动  │  │ GPA → HPA    │  │ MSI → vCPU     │
   └────┬─────┘  └──────┬───────┘  └───────┬────────┘
        └───────────────┼──────────────────┘
                        ▼
              ┌──────────────────┐
              │  Physical Device │
              └──────────────────┘
```

---

## 二、VFIO 核心架构

### 2.1 VFIO 与 KVM 的模块化绑定

VFIO 使用 `symbol_get` 机制避免与 KVM 的编译时强依赖，允许两者作为独立模块加载：

```c
// vfio_main.c:445-475
void vfio_device_get_kvm_safe(struct vfio_device *device, struct kvm *kvm)
{
    void (*pfn)(struct kvm *kvm);
    bool (*fn)(struct kvm *kvm);
    bool ret;

    lockdep_assert_held(&device->dev_set->lock);
    if (!kvm)
        return;

    pfn = symbol_get(kvm_put_kvm);          // 动态获取释放函数
    fn = symbol_get(kvm_get_kvm_safe);      // 动态获取获取函数
    if (WARN_ON(!pfn)) return;
    ret = fn(kvm);
    symbol_put(kvm_get_kvm_safe);
    if (!ret) { symbol_put(kvm_put_kvm); return; }

    device->put_kvm = pfn;
    device->kvm = kvm;
}
```

```c
// vfio_main.c:477-493
void vfio_device_put_kvm(struct vfio_device *device)
{
    lockdep_assert_held(&device->dev_set->lock);
    if (!device->kvm) return;
    device->put_kvm(device->kvm);
    device->put_kvm = NULL;
    symbol_put(kvm_put_kvm);
    device->kvm = NULL;
}
```

### 2.2 文件级别 KVM 绑定

在 cdev 路径中，KVM 先绑定到文件，设备 open 时再传播到 `vfio_device::kvm`：

```c
// vfio_main.c:1516-1528
static void vfio_device_file_set_kvm(struct file *file, struct kvm *kvm)
{
    struct vfio_device_file *df = file->private_data;
    spin_lock(&df->kvm_ref_lock);
    df->kvm = kvm;
    spin_unlock(&df->kvm_ref_lock);
}
```

公共入口同时处理 group 和 cdev 两种路径：

```c
// vfio_main.c:1538-1549
void vfio_file_set_kvm(struct file *file, struct kvm *kvm)
{
    struct vfio_group *group;
    group = vfio_group_from_file(file);
    if (group) vfio_group_set_kvm(group, kvm);
    if (vfio_device_from_file(file))
        vfio_device_file_set_kvm(file, kvm);
}
EXPORT_SYMBOL_GPL(vfio_file_set_kvm);
```

---

## 三、KVM-VFIO 桥接层

`virt/kvm/vfio.c` (387行) 在 KVM 框架内注册一个伪设备类型。

### 3.1 核心数据结构

```c
// virt/kvm/vfio.c:24-36
struct kvm_vfio_file {
    struct list_head node;
    struct file *file;
};

struct kvm_vfio {
    struct list_head file_list;
    struct mutex lock;
    bool noncoherent;
};
```

### 3.2 Device Ops 与创建

```c
// virt/kvm/vfio.c:347-377
static const struct kvm_device_ops kvm_vfio_ops = {
    .name = "kvm-vfio",
    .create = kvm_vfio_create,
    .release = kvm_vfio_release,
    .set_attr = kvm_vfio_set_attr,
    .has_attr = kvm_vfio_has_attr,
};

static int kvm_vfio_create(struct kvm_device *dev, u32 type)
{
    lockdep_assert_held(&dev->kvm->lock);
    /* Only one VFIO "device" per VM */
    list_for_each_entry(tmp, &dev->kvm->devices, vm_node)
        if (tmp->ops == &kvm_vfio_ops)
            return -EBUSY;
    kv = kzalloc_obj(*kv, GFP_KERNEL_ACCOUNT);
    INIT_LIST_HEAD(&kv->file_list);
    mutex_init(&kv->lock);
    dev->private = kv;
    return 0;
}
```

### 3.3 文件管理与缓存一致性

`kvm_vfio_file_add` (行143-186) 将 VFIO 文件加入 VM 的文件列表，调用 `kvm_vfio_file_set_kvm` 通过 symbol_get 跨模块绑定：

```c
// virt/kvm/vfio.c:38-49
static void kvm_vfio_file_set_kvm(struct file *file, struct kvm *kvm)
{
    void (*fn)(struct file *file, struct kvm *kvm);
    fn = symbol_get(vfio_file_set_kvm);
    if (!fn) return;
    fn(file, kvm);
    symbol_put(vfio_file_set_kvm);
}
```

`kvm_vfio_update_coherency` (行120-141) 遍历所有设备检查是否需要 noncoherent DMA，按需调用 `kvm_arch_register_noncoherent_dma`。

### 3.4 IOCTL 接口

用户态通过 `KVM_SET_DEVICE_ATTR` 操作：

```c
// virt/kvm/vfio.c:266-290
static int kvm_vfio_set_file(struct kvm_device *dev, long attr,
                             void __user *arg)
{
    switch (attr) {
    case KVM_DEV_VFIO_FILE_ADD:
        return kvm_vfio_file_add(dev, fd);
    case KVM_DEV_VFIO_FILE_DEL:
        return kvm_vfio_file_del(dev, fd);
    }
    return -ENXIO;
}
```

设备释放时 (`kvm_vfio_release`, 行324-343) 遍历 file_list 逐一解除 KVM 绑定、释放引用。

---

## 四、IOMMU DMA 重映射

### 4.1 两层翻译路径

```
  Device DMA Request
        │
        ▼
  ┌──────────────┐
  │  IOVA        │  ← 设备"看到"的地址（由 VFIO IOMMU 域管理）
  └──────┬───────┘
         │ IOMMU 页表（iommufd/type1 维护）
         ▼
  ┌──────────────┐
  │  GPA         │  ← Guest 物理地址
  └──────┬───────┘
         │ EPT/NPT 页表（KVM MMU 维护）
         ▼
  ┌──────────────┐
  │  HPA         │  ← 最终物理地址
  └──────────────┘
```

### 4.2 现代路径：iommufd

VFIO 现推荐使用 iommufd 而非 legacy type1 IOMMU 接口。iommufd 支持：
- IOAS (I/O Address Space) 多设备共享
- 硬件嵌套分页（guest-managed IOMMU 页表）
- 统一 hardware-agnostic 接口

VFIO 设备打开时自动绑定 iommufd，通过 `iommufd_access_create` 建立访问通道，随后调用设备驱动的 `attach_ioas` 建立映射。

### 4.3 内存变更同步

KVM memslot 变更时，mmu_notifier 链通知 IOMMU 层同步映射。关键路径：
- KVM MMU notifier → VFIO IOMMU notifier
- 新内存自动建立 IOVA→HPA 映射
- Balloon inflate / memory hot-unplug 时解除映射

---

## 五、中断重映射：Intel Posted Interrupts

### 5.1 核心理念

Posted Interrupts 允许设备中断通过硬件直接投递到 vCPU 的 Posted Interrupt Descriptor (PID)，无需 VM-exit。

PID 是 64 字节对齐的内存结构，地址写入 VMCS 的 `posted-interrupt descriptor address` 字段：

```
  Offset  Size  Field
  ──────  ────  ────────────────────────────────
  0       32    PIR  (Posted Interrupt Requests) — 256-bit 位图
  32      2     ON   (Outstanding Notification)   — bit 256
  34      2     SN   (Suppress Notification)      — bit 257
  36      2     Reserved
  38      2     NV   (Notification Vector 0-15)
  40      4     NDST (Notification Destination)
```

### 5.2 PI Load 流程

```c
// posted_intr.c:57-145
void vmx_vcpu_pi_load(struct kvm_vcpu *vcpu, int cpu)
{
    struct pi_desc *pi_desc = vcpu_to_pi_desc(vcpu);
    struct pi_desc old, new;
    unsigned int dest;

    if (!enable_apicv || !lapic_in_kernel(vcpu))
        return;

    // 快速路径：vCPU 未迁移且未从 wakeup 恢复
    if (pi_desc->nv != POSTED_INTR_WAKEUP_VECTOR && vcpu->cpu == cpu) {
        if (pi_test_and_clear_sn(pi_desc))
            goto after_clear_sn;
        return;
    }

    // 慢速路径：处理迁移、从 wakeup list 移除
    if (pi_desc->nv == POSTED_INTR_WAKEUP_VECTOR) {
        raw_spin_lock(spinlock);
        list_del(&vt->pi_wakeup_list);
        raw_spin_unlock(spinlock);
    }

    dest = cpu_physical_id(cpu);
    // 原子更新 PID：设置 NDST、清除 SN、恢复 NV
    do {
        new.control = old.control;
        new.ndst = dest;
        __pi_clear_sn(&new);
        new.nv = POSTED_INTR_VECTOR;
    } while (pi_try_set_control(pi_desc, &old.control, new.control));

after_clear_sn:
    smp_mb__after_atomic();
    if (!pi_is_pir_empty(pi_desc))
        pi_set_on(pi_desc);
}
```

### 5.3 Wakeup Handler

当 vCPU 阻塞 (HLT) 时，KVM 将其加入 per-CPU wakeup 列表，PID 的 NV 改为 `POSTED_INTR_WAKEUP_VECTOR`（`pi_enable_wakeup_handler`, 行162-199）。设备中断到来时触发 IPI 唤醒 vCPU。

---

## 六、中断重映射：AMD AVIC

### 6.1 GATag 编码

AVIC 使用 32 位 GATag (Guest Address Tag) 编码 VM ID 和 vCPU index：

```c
// avic.c:47-66
#define AVIC_VCPU_IDX_MASK    AVIC_PHYSICAL_MAX_INDEX_MASK
#define AVIC_VM_ID_SHIFT      HWEIGHT32(AVIC_PHYSICAL_MAX_INDEX_MASK)
#define AVIC_VM_ID_MASK       (GENMASK(31, AVIC_VM_ID_SHIFT) >> AVIC_VM_ID_SHIFT)

#define AVIC_GATAG_TO_VMID(x)     ((x >> AVIC_VM_ID_SHIFT) & AVIC_VM_ID_MASK)
#define AVIC_GATAG_TO_VCPUIDX(x)  (x & AVIC_VCPU_IDX_MASK)

#define __AVIC_GATAG(vm_id, vcpu_idx) \
    ((((vm_id) & AVIC_VM_ID_MASK) << AVIC_VM_ID_SHIFT) | \
     ((vcpu_idx) & AVIC_VCPU_IDX_MASK))
```

### 6.2 硬件表

AVIC 依赖两块硬件表：

| 表名 | 描述 |
|------|------|
| Physical APIC ID Table | 每个物理 APIC ID → (VMCB addr, GATag) |
| Logical APIC ID Table | 逻辑 APIC ID → 物理 APIC ID，8 cluster × 16 APIC |

### 6.3 x2AVIC

Zen 4+ 支持 x2AVIC (x2APIC 模式的硬件虚拟化)。KVM 控制哪些 MSR 被截获：

```c
// avic.c:122-170
static const u32 x2avic_passthrough_msrs[] = {
    X2APIC_MSR(APIC_ID), X2APIC_MSR(APIC_LVR),
    X2APIC_MSR(APIC_TASKPRI), X2APIC_MSR(APIC_EOI),
    X2APIC_MSR(APIC_ISR), X2APIC_MSR(APIC_TMR),
    X2APIC_MSR(APIC_IRR), X2APIC_MSR(APIC_ICR),
    /* 注意！LVTT 始终截获，TSC-deadline 不被硬件虚拟化 */
    X2APIC_MSR(APIC_LVTTHMR),
    // ...
};
```

### 6.4 GALog 与 VM Hash Table

AVIC 无法直接投递时，硬件写入 Guest Activity Log (GALog)。KVM 维护 VM ID → kvm_svm 的哈希表来查找目标 VM：

```c
// avic.c:114-118
#define SVM_VM_DATA_HASH_BITS  8
static DEFINE_HASHTABLE(svm_vm_data_hash, SVM_VM_DATA_HASH_BITS);
static u32 next_vm_id = 0;
```

### 6.5 IPI 虚拟化

AMD IPIv 允许 vCPU 间 IPI 通过硬件直接投递（`enable_ipiv` 参数，avic.c:104）。

---

## 七、Posted Interrupts vs AVIC

```
Intel Posted Interrupts:
  Device MSI → VT-d IRTE → PID.PIR → vCPU
    • vCPU 运行中：硬件写 PIR，零 VM-Exit
    • vCPU 阻塞：NV=WAKEUP → IPI → KVM 处理
    • 数据结构：64B PID + VMCS 字段

AMD AVIC:
  Device MSI → AMD IOMMU DevTable → Physical APIC ID Table → vCPU
    • 查找物理 APIC ID Table → VMCB + GATag
    • 成功：直接投递到 guest vAPIC，零 VM-Exit
    • 失败：写入 GALog → KVM 事后处理
    • 数据结构：Physical/Logical APIC ID 表 + VMCB
```

---

## 八、设备分配完整流程

```
QEMU/KVM Userspace
  │
  ├─ 1. open("/dev/vfio/devices/vfioX")  → VFIO cdev
  │     └─ vfio_df_device_first_open() → iommufd 绑定
  │
  ├─ 2. ioctl(VFIO_DEVICE_ATTACH_IOMMUFD_PT)
  │     └─ 建立 IOMMU 映射 (遍历 KVM memslots)
  │
  ├─ 3. KVM_CREATE_DEVICE(KVM_DEV_TYPE_VFIO)
  │     └─ kvm_vfio_create()  (virt/kvm/vfio.c:355)
  │
  ├─ 4. KVM_SET_DEVICE_ATTR(KVM_DEV_VFIO_FILE_ADD, fd)
  │     └─ kvm_vfio_file_add()  (virt/kvm/vfio.c:143)
  │        ├─ fget(fd) → 验证 VFIO 文件
  │        ├─ kvm_vfio_file_set_kvm() → 跨模块绑定 KVM
  │        └─ kvm_vfio_update_coherency()
  │
  ├─ 5. 配置 IRQ bypass
  │     └─ kvm_irqfd_assign() → irq_bypass_register_producer()
  │        └─ 桥接 VFIO IRQ → KVM posted interrupt
  │
  └─ 6. 运行时
        ├─ KVM mmu_notifier → IOMMU 映射同步
        └─ Memory hotplug → 增量映射
```

---
## 九、完整架构图

```
╔═══════════════════════ Guest VM ═══════════════════════╗
║  Guest Driver → DMA (GPA)          MSI/MSI-X → vCPU   ║
╚══════════════╤══════════════════════════╤═══════════════╝
               │                          │
  ╔════════════╪══════ Hypervisor ════════╪══════════════╗
  ║            ▼                          ▼              ║
  ║  ┌──────────────┐           ┌──────────────────┐    ║
  ║  │ VFIO-KVM桥   │           │ 中断重映射         │    ║
  ║  │ virt/kvm/    │           │  posted_intr.c    │    ║
  ║  │ vfio.c:379   │           │  avic.c           │    ║
  ║  └──────┬───────┘           └────────┬─────────┘    ║
  ║         ▼                            ▼              ║
  ║  ┌──────────────┐           ┌──────────────────┐    ║
  ║  │ VFIO Core    │           │ IOMMU Driver      │    ║
  ║  │ vfio_main.c  │◄─────────>│ VT-d / AMD IOMMU │    ║
  ║  │ • device ops │  iommufd  │ • DMA 重映射      │    ║
  ║  │ • KVM引用    │           │ • 中断重映射      │    ║
  ║  └──────────────┘           └────────┬─────────┘    ║
  ╚══════════════════════════════════════╪═══════════════╝
                                         │
                                ┌────────┴────────┐
                                │  Physical Device │
                                │  (NVMe/GPU/NIC)  │
                                └─────────────────┘

中断路径 (Posted Interrupt):
  Device MSI → VT-d IRTE → PID.PIR
    ├─ vCPU running → 硬件直接投递 (0 VM-Exit)
    └─ vCPU blocked → WAKEUP_VECTOR IPI → KVM 唤醒

DMA 路径:
  Device DMA (IOVA) → IOMMU 页表 → GPA
    ├─ 已映射  → 直接翻译为 HPA
    └─ 未映射  → IOMMU fault → mmu_notifier → 动态建立
```

---

## 十、关键文件索引

| 文件 | 行数 | 描述 |
|------|------|------|
| `virt/kvm/vfio.c` | 387 | KVM-VFIO 桥接层：伪设备注册、文件管理、缓存一致性 |
| `drivers/vfio/vfio_main.c` | 1856 | VFIO 核心：设备注册/注销、KVM 引用管理 |
| `drivers/vfio/device_cdev.c` | ~ | VFIO cdev 路径：设备 open/close、iommufd 绑定 |
| `arch/x86/kvm/vmx/posted_intr.c` | 319 | Intel Posted Interrupts：PID 管理、wakeup handler |
| `arch/x86/kvm/svm/avic.c` | 1321 | AMD AVIC：GATag 编码、APIC 表、x2AVIC、GALog |

---

## 下一篇文章

[第5篇: 半虚拟化 — pvclock、kvmclock、steal time 与 Hyper-V Enlightenment](./05-pv.md)
