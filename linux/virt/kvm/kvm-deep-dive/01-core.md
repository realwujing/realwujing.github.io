# 第1篇：KVM 核心 — VM 生命周期、内存槽与 vCPU 调度

> 源码：`virt/kvm/kvm_main.c` | 头文件：`include/linux/kvm_host.h`

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 1. /dev/kvm 字符设备：一切从这里开始

KVM 以字符设备的方式暴露给 userspace，设备文件 `/dev/kvm` 的 file_operations 定义在 `kvm_main.c:5558`：

```c
// virt/kvm/kvm_main.c:5558
static const struct file_operations kvm_chardev_ops = {
    .unlocked_ioctl = kvm_dev_ioctl,       // line 5522
    .llseek          = noop_llseek,
    KVM_COMPAT(kvm_dev_ioctl),
};
```

### 1.1 kvm_dev_ioctl 入口 (kvm_main.c:5522)

```c
// virt/kvm/kvm_main.c:5522
static long kvm_dev_ioctl(struct file *filp,
                          unsigned int ioctl, unsigned long arg)
{
    long r = -EINVAL;

    switch (ioctl) {
    case KVM_GET_API_VERSION:
        ...
    case KVM_CREATE_VM:                              // line 5533
        r = kvm_dev_ioctl_create_vm(arg);            // line 5534
        if (r < 0)
            return r;
        file = fget(r);
        ...
    case KVM_GET_MSR_INDEX_LIST:
    case KVM_CHECK_EXTENSION:
    case KVM_GET_VCPU_MMAP_SIZE:
        ...
    }
}
```

### 1.2 KVM_CREATE_VM: 创建虚拟机 (kvm_main.c:5479)

```c
// virt/kvm/kvm_main.c:5479
static int kvm_dev_ioctl_create_vm(unsigned long type)
```

关键流程：

1. **分配 struct kvm** — kvm_arch_alloc_vm() 分配并初始化架构相关字段
2. **kvm_create_vm()** — 分配 memslot、IO bus、debugfs 节点、mmu_notifier 等
3. **kvm_arch_post_init_vm()** — x86 架构完成 VM 创建后初始化
4. **anon_inode_getfd()** — 创建匿名 inode fd 返回给 userspace
5. `type` 参数用于区分普通 VM / SEV / TDX 等机密计算类型

userspace 获得一个代表了该 VM 的文件描述符 fd，后续所有 VM 级 ioctl 都通过此 fd 发出。

---

## 2. struct kvm：VM 的核心数据结构 (kvm_host.h:770)

```c
// include/linux/kvm_host.h:770
struct kvm {
    struct mm_struct *mm;                   // 关联的进程地址空间
    spinlock_t mmu_lock;                    // MMU 操作锁
    struct mutex slots_lock;                // memslot 操作锁
    struct mutex slots_arch_lock;           // x86 专用 memslot 锁

    struct xarray vcpu_array;               // vCPU ID → struct kvm_vcpu * 映射

    struct kvm_memslots __rcu *memslots[KVM_MAX_NR_ADDRESS_SPACES];  // line ~790
    // KVM_MAX_NR_ADDRESS_SPACES = 2 (普通 + SMM) — arch/x86/include/asm/kvm_host.h:2425

    struct kvm_io_bus __rcu *buses[KVM_NR_BUS_TYPES];
    // KVM_MMIO_BUS, KVM_PIO_BUS, KVM_VIRTIO_CCW_NOTIFY_BUS, KVM_FAST_MMIO_BUS

    struct kvm_io_bus *arch_buses[KVM_ARCH_NR_BUS_TYPES];

    // 统计信息: dirty ring, MMU pages, remote TLB flushes
    unsigned int nr_mmu_pages;
    ...
    struct kvm_stat_data **debugfs_stat_data;
    struct srcu_struct srcu;                // SRCU 保护 memslot 读取
    struct srcu_struct irq_srcu;            // SRCU 保护 IRQ 注入

    struct kvm_irq_routing_table __rcu *irq_routing;   // 中断路由表

    wait_queue_head_t mn_invalidate_in_progress;       // MMU notifier

    struct kvm_dirty_ring dirty_ring;                  // dirty ring for live migration

    struct list_head devices;              // 注册的设备列表
    struct kvm_arch arch;                  // x86 架构私有数据
};
```

这个结构体几乎包含了虚拟机运行所需要的全部信息：内存布局 (memslots)、IO 地址空间 (io buses)、vCPU 列表、中断路由和统计信息。理解 `struct kvm` 是理解 KVM 源码的第一步。

---

## 3. KVM_CREATE_VCPU: 创建虚拟 CPU (kvm_main.c:4151)

创建 vCPU 是 VM 运行的前提，userspace 调用 `KVM_CREATE_VCPU`：

```c
// virt/kvm/kvm_main.c:4151
static int kvm_vm_ioctl_create_vcpu(struct kvm *kvm, unsigned long id)
```

`kvm_vm_ioctl` 在 `kvm_main.c:5147` 分发：

```c
// virt/kvm/kvm_main.c:5147
static long kvm_vm_ioctl(struct file *filp,
                         unsigned int ioctl, unsigned long arg)
{
    ...
    case KVM_CREATE_VCPU:                                    // line 5157
        r = kvm_vm_ioctl_create_vcpu(kvm, arg);              // line 5158
        ...
}
```

### 3.1 创建流程

1. **xarray 容量检查** — 最多 KVM_MAX_VCPUS (默认 1024)
2. **preempt_notifier_init** — 初始化抢占通知器（steal time 计算用）
3. **kvm_arch_vcpu_create()** — 架构相关：分配 VMCS/VMCB、MTRR、FPU 状态
4. **kvm_arch_vcpu_postcreate()** — 架构创建后初始化
5. **create_vcpu_fd()** — 创建 vCPU fd，返回给 userspace
6. **kvm_create_vcpu_debugfs()** — 创建 debugfs 文件

### 3.2 struct kvm_vcpu (kvm_host.h:325)

```c
// include/linux/kvm_host.h:325
struct kvm_vcpu {
    struct kvm *kvm;                       // 所属 VM

    int vcpu_id;                           // 用户侧 vCPU ID
    int vcpu_idx;                          // vCPU 数组索引 (可能不同)

    unsigned long requests;                // KVM_REQUEST_* 位图

    struct kvm_run *run;                   // 共享内存区域 (与 userspace 映射)
    unsigned int run_size;                 // run 区域大小

    struct swait_queue_head wqh;           // vCPU 等待队列 (HALT/休眠时)

    struct kvm_vcpu_arch arch;             // x86 架构状态: regs, VMCS, mmu
    struct kvm_vcpu_stat stat;             // vCPU 统计信息

    struct mutex mutex;                    // vCPU 保护锁

    struct kvm_dirty_ring dirty_ring;      // per-vCPU dirty ring

    int mode;                              // GUEST_MODE / IN_GUEST_MODE 等
};
```

### 3.3 vCPU 的 file_operations (kvm_main.c:4098)

```c
// virt/kvm/kvm_main.c:4098
static const struct file_operations kvm_vcpu_fops = {
    .unlocked_ioctl = kvm_vcpu_ioctl,      // line 4100
    .mmap            = kvm_vcpu_mmap,
    .release         = kvm_vcpu_release,
};
```

---

## 4. KVM_RUN: 进入 vCPU 执行循环 (kvm_main.c:4405)

```c
// virt/kvm/kvm_main.c:4405
static long kvm_vcpu_ioctl(struct file *filp,
                           unsigned int ioctl, unsigned long arg)
{
    case KVM_RUN: {                        // line 4440
        ...
        r = __kvm_vcpu_run(vcpu);          // 进入 kvm_arch_vcpu_ioctl_run
        ...
    }
}
```

内部流程：

```
KVM_RUN ioctl
    │
    ▼
kvm_vcpu_ioctl — 检查信号、清理状态
    │
    ▼
kvm_arch_vcpu_ioctl_run — x86 特定入口
    │
    ├── vcpu->mode = IN_GUEST_MODE
    │
    ▼
kvm_x86_ops->vcpu_run(vcpu) — 调用 VMX 或 SVM 的 vcpu_run
    │
    ├── VM-Entry (VMLAUNCH/VMRESUME 或 VMRUN)
    │   └── guest 代码执行
    │
    ├── VM-Exit (硬件触发)
    │   └── handle_exit 分发
    │       ├── EXIT_REASON_EXCEPTION_NMI
    │       ├── EXIT_REASON_EXTERNAL_INTERRUPT
    │       ├── EXIT_REASON_EPT_VIOLATION
    │       ├── EXIT_REASON_IO_INSTRUCTION
    │       ├── EXIT_REASON_MSR_READ / WRITE
    │       ├── EXIT_REASON_CPUID
    │       └── EXIT_REASON_HLT / PAUSE / ...
    │
    └── vcpu->mode = OUTSIDE_GUEST_MODE
```

KVM_RUN 是 vCPU 执行的核心循环。userspace (QEMU) 通过多次调用 KVM_RUN ioctl，让 vCPU 在 VM-entry 与 VM-exit 之间不断切换。

---

## 5. 内存槽 (memslot)：GPA → HPA 映射

### 5.1 KVM_SET_USER_MEMORY_REGION (kvm_main.c:2140)

userspace 通过此 ioctl 为 guest 物理地址空间注册内存区域：

```c
// virt/kvm/kvm_main.c:2140
static int kvm_vm_ioctl_set_memory_region(struct kvm *kvm,
                                          struct kvm_userspace_memory_region *mem)
```

KVM 内部维护 `struct kvm_memory_slot`：

```c
struct kvm_memory_slot {
    gfn_t base_gfn;              // guest frame number 起始
    unsigned long npages;        // 槽大小 (页数)
    unsigned long *dirty_bitmap; // 脏页跟踪位图
    struct kvm_arch_memory_slot arch;  // x86: rmap (反向映射)
    unsigned long userspace_addr;      // QEMU 进程中的 HVA
    u32 flags;                  // KVM_MEM_READONLY / KVM_MEM_LOG_DIRTY_PAGES
    short id;                   // 槽 ID
};
```

### 5.2 双 memslot 机制（RCU 更新）

KVM 使用 active/inactive 双 memslot 设计：

```
struct kvm_memslots {
    int generation;
    short used_slots;
    struct kvm_memory_slot memslots[KVM_MEM_SLOTS_NUM];  // 实际数组
};
```

**更新流程**：
1. 持有 `slots_lock` mutex
2. 在 inactive memslot 数组上修改
3. 交换 active/inactive 指针 (通过 RCU)
4. `synchronize_srcu(&kvm->srcu)` — 等待所有 reader 完成
5. 释放旧 memslot

reader 侧无需锁：`srcu_read_lock(&kvm->srcu)` → 读取 memslot → `srcu_read_unlock()`。

**memslot 分配策略**：memslot 数组定长（KVM_MEM_SLOTS_NUM，默认 509 个插槽）。`kvm_vm_ioctl_set_memory_region` 传入 `slot` ID 指定位置。KVM 通过 `id_to_index` 将 slot ID 映射到数组索引。

**大页支持**：在 memslot 创建时，KVM 扫描 HVA 区域检测 THP (transparent huge pages)，记录大页信息。后续 MMU 映射时优先使用 2M/1G 大页以获得更好的 TLB 效率。

**memslot 的 x86 扩展** (`struct kvm_arch_memory_slot`)：
- `rmap` — 每个 GFN 的反向映射表 (GFN→SPTE 列表)，传统 MMU 使用
- `lpage_info` — 大页跟踪信息 (dirty logging 拆分状态)
- `arch_flags` — x86 架构专用标志

### 5.3 gfn_to_pfn: GPA → HPA 的核心转换

```c
hva_t gfn_to_hva(struct kvm *kvm, gfn_t gfn)
{
    // 查找 gfn 所在的 memslot
    slot = gfn_to_memslot(kvm, gfn);
    // 计算 HVA = slot->userspace_addr + offset
    return slot->userspace_addr + (gfn - slot->base_gfn) * PAGE_SIZE;
}

kvm_pfn_t __gfn_to_pfn_memslot(struct kvm *kvm_t, ...)
{
    hva = gfn_to_hva_memslot(slot, gfn);

    // 对普通 shared 内存:
    return hva_to_pfn(hva, ...);
    // hva_to_pfn → get_user_pages() / follow_page() → struct page → PFN

    // 对 private (guest_memfd) 内存:
    return kvm_gmem_get_pfn(kvm, slot, gfn);
    // 直接从 guest_memfd inode 获取 PFN, 不经过 HVA
}
```

**转换链 (shared)**：`GPA → memslot lookup → HVA → get_user_pages() → struct page → PFN`
**转换链 (private)**：`GPA → memslot lookup → guest_memfd inode → folio → PFN`

---

## 6. KVM IO 总线 (io_bus)

KVM 实现了轻量级的 IO 总线层，用于将 MMIO/PIO 请求路由到注册的设备：

```c
struct kvm_io_bus {
    int dev_count;
    struct kvm_io_device *range[];
};

struct kvm_io_device_ops {
    int (*read)(struct kvm_vcpu *vcpu, struct kvm_io_device *dev,
                gpa_t addr, int len, void *val);
    int (*write)(struct kvm_vcpu *vcpu, struct kvm_io_device *dev,
                 gpa_t addr, int len, const void *val);
};
```

VM-exit 时，KVM 对 MMIO/PIO 访问执行：
```c
kvm_io_bus_write(vcpu, KVM_MMIO_BUS, gpa, len, &val);
```

io_bus 同样使用 RCU 保护读写，设备通过 `kvm_io_bus_register_dev()` 注册。

---

## 7. eventfd 集成：高性能 I/O 路径

KVM eventfd 模块 (`virt/kvm/eventfd.c`) 提供了两种重要的 I/O 加速机制。

### 7.1 ioeventfd — MMIO/PIO 快速路径

ioeventfd 允许 userspace 为特定 MMIO/PIO 地址注册 eventfd。当 guest 访问该地址时，KVM 绕过 QEMU 的完整模拟，直接触发 eventfd 通知，避免用户态上下文切换。

```
Guest MMIO write to addr X
       │
       ▼
KVM VM-Exit → handle_exit → EXIT_REASON_EPT_VIOLATION/IO
       │
       ▼
查找 ioeventfd 表 (RCU 保护)
       │
       ├── 命中 → eventfd_signal(&ioeventfd->eventfd)
       │         返回 guest (无需返回用户态)
       │
       └── 未命中 → 返回用户态 → QEMU 模拟
```

### 7.2 irqfd — 从中断信号注入虚拟中断

irqfd 将 eventfd 信号映射到 guest 中断线，允许内核态直接注入中断：

```c
// 内核态某设备驱动
eventfd_signal(irqfd->eventfd);
    → irqfd_wakeup()
        → kvm_set_irq(kvm, irqfd->gsi, 1)
            → kvm_arch_set_irq()  // x86: kvm_set_ioapic_irq / kvm_set_pic_irq
                → 重新评估 APIC/PIC 状态
                → 如果 vCPU 可以接收中断 → kvm_vcpu_kick()
```

整个路径完全在内核态完成，无需返回 QEMU。

---

## 8. Dirty Ring：原地脏页跟踪 (virt/kvm/dirty_ring.c)

Dirty ring 是为实时迁移设计的脏页跟踪机制，每个 vCPU 拥有独立的环形缓冲区：

```c
struct kvm_dirty_ring {
    u32 dirty_index;                      // 生产者和消费者的索引
    u32 reset_index;
    u32 size;
    struct kvm_dirty_gfn *dirty_gfns;     // 环形缓冲区
};

struct kvm_dirty_gfn {
    __u32 flags;
    __u32 slot;                           // memslot ID
    __u64 offset;                         // 页偏移
};
```

每个 vCPU 拥有独立的 dirty ring，避免 vCPU 之间的 cache line false sharing。Dirty ring 满时，KVM 强制 vCPU 退出到用户态处理。

vCPU 创建 dirty entry 流程：
```
write to guest page → EPT violation (write protect)
    → kvm_mmu_write_protect_page()
    → mark_page_dirty_in_slot()
        → kvm_dirty_ring_push(vcpu, slot, offset)
            → ring[vcpu->dirty_index++] = {slot, offset}
            → smp_wmb() — 保证写入顺序
            → 如果 index == size → 设置 KVM_REQ_DIRTY_RING_FULL
```

| 维度 | Dirty Ring | Dirty Bitmap |
|------|-----------|-------------|
| 结构 | per-vCPU ring buffer | per-memslot bitmap |
| Cache 行为 | vCPU 独立, 无争用 | 多 vCPU 写同一 bitmap |
| 收集方式 | mmap ring 读取 | KVM_GET_DIRTY_LOG ioctl |
| 优势 | 低延迟, 无锁 | 简单, 兼容性好 |

---

## 9. Async PF：异步页面异常 (virt/kvm/async_pf.c)

当 guest 访问被换出到 swap 的页面时，async page fault 允许 vCPU 继续执行其他任务而非阻塞：

```
1. Guest 访问 GPA → EPT violation
2. KVM: get_user_pages() 发现需要读盘 → 不是立即等待
3. KVM 注入 page-not-present async PF (#PF 编码)
4. Guest: 收到 async PF, 将当前任务调度出, 执行其他任务
5. KVM: 磁盘 I/O 完成 → 注入 page-ready async PF
6. Guest: 收到通知, 将等待任务重新调度运行
```

KVM 使用 workqueue 异步执行页面换入：

```c
// virt/kvm/async_pf.c
kvm_async_pf_queue_work(cpu, gpa, gva, kvm, vcpu)
    → work = kzalloc(sizeof(*work), GFP_NOWAIT)
    → INIT_WORK(&work->work, async_pf_execute)
    → schedule_work_on(cpu, &work->work)
```

---

## 10. Guest Memory 类型：guest_memfd (virt/kvm/guest_memfd.c)

为支持 TDX 和 SEV-SNP 的私有内存，KVM 引入了 guest_memfd 机制。

**两种 guest 内存模式**：

| 内存类型 | 特征 | 支持技术 |
|---------|------|---------|
| Shared (普通) | userspace 映射，GPA→HVA→PFN 转换 | 所有 VM |
| Private (私有) | guest_memfd, inode 后端，不可从 host 读 | TDX, SEV-SNP, pKVM |

private 内存的 GPA→PFN 转换不经过 HVA：直接从 `kvm_gmem_get_pfn()` 映射到 guest_memfd 分配的物理页，避免了 host 访问 guest 私有内存的安全风险。

---

## 11. 完整 ioctl 交互流程

```
                 userspace (QEMU)
                      │
    ┌─────────────────┼──────────────────┐
    │                 │                  │
    ▼                 ▼                  ▼
open("/dev/kvm")  KVM_CREATE_VM     KVM_CREATE_VCPU
    │                 │                  │
    ▼                 ▼                  ▼
kvm_dev_ioctl   kvm_dev_ioctl    kvm_vm_ioctl
(kvm_main.c:   _create_vm        (kvm_main.c:
 5522)          (kvm_main.c:      5147)
                 5479)               │
    │                 │              ▼
    │                 ▼     kvm_vm_ioctl_create_vcpu
    │           struct kvm       (kvm_main.c:4151)
    │                 │              │
    │                 │              ▼
    │                 │        struct kvm_vcpu
    │                 │              │
    │                 │              │
    │       KVM_SET_USER_    KVM_RUN loop
    │       MEMORY_REGION       │
    │       (kvm_main.c:        │ kvm_vcpu_ioctl (4405)
    │        2140)              │ → kvm_x86_ops->vcpu_run
    │            │              │    ├── VM-Entry
    │            │              │    ├── Guest 执行
    │            │              │    ├── VM-Exit
    │            │              │    │   ├── EPT violation
    │            │              │    │   ├── MMIO/PIO
    │            │              │    │   │  → ioeventfd
    │            │              │    │   │    fast path
    │            │              │    │   └── handle_exit
    │            │              │    └── 返回用户态
    │            │              │         (如有需要)
    └────────────┴──────────────┴──────────→ return to QEMU
```

QEMU 收到 KVM_EXIT_IO / KVM_EXIT_MMIO 时执行设备模拟，然后再次调用 KVM_RUN 进入下一轮循环。

---

## 12. 其他核心模块概览

| 模块 | 文件 | 功能 |
|------|------|------|
| 中断路由 | `virt/kvm/irqchip.c` | GSI → irqchip 路由表设置（KVM_SET_GSI_ROUTING）|
| Coalesced MMIO | `virt/kvm/coalesced_mmio.c` | 合并相邻 MMIO 写入，减少 VM-Exit |
| Binary Stats | `virt/kvm/binary_stats.c` | 基于 debugfs 的二进制统计导出 |
| PFN cache | `virt/kvm/pfncache.c` | gfn→pfn 缓存（APIC 访问页等热路径）|
---

## 13. KVM io_bus 实现细节

KVM 维护 4 条 IO 总线 (KVM_MMIO_BUS, KVM_PIO_BUS, KVM_VIRTIO_CCW_NOTIFY_BUS, KVM_FAST_MMIO_BUS)。`struct kvm_io_bus` 包含按地址排序的 `kvm_io_device *` 数组，通过 RCU 更新。VM-exit 时 `kvm_io_bus_read/write()` 二分搜索设备，每个设备的回调返回 `-EOPNOTSUPP` 表示不认领则尝试下一个。

常见 io_bus 设备：ioeventfd (MMIO/PIO 快路径)、KVM APIC (0xFEE00000)、KVM IOAPIC (0xFEC00000)、KVM PIC (0x20/0xA0)、VFIO bus driver、coalesced MMIO。

---

## 14. irqfd 实现剖析 (virt/kvm/eventfd.c)

`struct kvm_kernel_irqfd` 关联 eventfd 与 GSI。eventfd signal 触发 `irqfd_wakeup()` → `kvm_set_irq()` (level-triggered) 或 `kvm_set_msi()` (MSI) → APIC 评估 → 如目标 vCPU 未运行则 `kvm_vcpu_kick()`。

`virt/kvm/irqbypass.c` 实现 IRQ Bypass：VFIO 设备通过 posted interrupts 直接将中断注入 guest，Device IRQ → posted interrupt descriptor 硬件更新 → 硬件注入中断 (无 VM-Exit)，完全绕过 KVM 软件栈。

---

## 15. vCPU 调度与控制

`kvm_vcpu_kick()` 向运行在其他 pCPU 上的 vCPU 发送 IPI `smp_send_reschedule()`，通过 preempt notifier 优雅退出 guest mode。

vCPU 状态机：OUTSIDE_GUEST_MODE → KVM_RUN → IN_GUEST_MODE → HLT → schedule() → wake_up → 重新 KVM_RUN。KVM 注册 preempt notifier (`kvm_sched_in/kvm_sched_out`) 感知 vCPU 被调度出入，`kvm_sched_out` 记录 steal time 使 guest 优化调度决策 (如 pv_ticket_lock)。

---

## 16. DebugFS 与统计信息

KVM 通过 `/sys/kernel/debug/kvm/<VM-ID>/` 暴露运行时信息：每 VM 有 vcpuN/stats、mmu_page_hash、io_bus 等节点。关键 per-vCPU 统计包括 exits、io_exits、mmio_exits、halt_exits、ept_violation_exits、irq_injections、pf_fixed 等，通过 debugfs 二进制接口高效导出。

---

## 下一篇文章

[第2篇：x86 虚拟化 — VMX/VMCS 与 SVM/VMCB，VM-Entry/Exit 全解析](./02-vmx-svm.md)
