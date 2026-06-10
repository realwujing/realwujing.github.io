# 第3篇：MMU 虚拟化 — EPT/NPT、影子页表与 TDP MMU

> 源码：`arch/x86/kvm/mmu/mmu.c` `arch/x86/kvm/mmu/tdp_mmu.c` | 头文件：`arch/x86/include/asm/kvm_host.h`

系列目录：[KVM 内核源码深度解析](./README.md)

---

## 1. 内存虚拟化的核心问题

在没有硬件辅助虚拟化的时代，guest 物理地址 (GPA) 与 host 物理地址 (HPA) 之间存在额外一层映射。一个 guest 物理页可能位于 host 任意物理地址。这就引入了**两级地址转换**的问题：

```
Guest Virtual Address (GVA)
       │
       ▼ (Guest 页表: CR3_g → 4-level/5-level 页表遍历)
Guest Physical Address (GPA)
       │
       ▼ (Hypervisor 映射: EPT/NPT 或 影子页表)
Host Physical Address (HPA)
```

shadow paging 和 nested paging (EPT/NPT) 是解决这个问题的两种方案。

| 特性 | Shadow Paging | EPT/NPT (Two-Dimensional Paging) |
|------|--------------|----------------------------------|
| 硬件要求 | 无 (纯软件) | Intel EPT / AMD NPT |
| guest CR3 含义 | HPA → 影子页表根 | GPA → guest 页表根 |
| 额外页表 | KVM 维护影子页表 | 硬件维护 EPT/NPT 页表 |
| 页故障处理 | KVM 截获所有 #PF，模拟 guest 页表遍历 | VM-Exit (EPT violation) |
| TLB 支持 | 传统 INVLPG | EPT 感知的 INVEPT/INVVPID |
| 内存开销 | 高 (每 guest 进程一个影子页表) | 中 (单一 NPT/EPT 结构) |
| 当前状态 | 仅在无法使用 EPT/NPT 时回退 | 首选方案 |

---

## 2. 地址转换流程 (EPT/NPT 模式)

### 2.1 EPT 两级转换

```
     GVA (Guest Virtual Address)
           │
           ▼
 ┌── guest CR3 (GPA) ──────────────────────┐
 │  PML4                                     │
 │   │                                       │
 │   ▼                                       │
 │  PDPTR              Guest Page Tables    │
 │   │                 (GPA 域)             │
 │   ▼                                       │
 │  PDE                                     │
 │   │                                       │
 │   ▼                                       │
 │  PTE  ──→ GPA                            │
 └──────────────────────────────────────────┘
           │
           │ GPA                              ← guest 页表遍历完成
           ▼
 ┌── EPTP (EPT PML4 HPA) ──────────────────┐
 │  EPT PML4                                 │
 │   │                                       │
 │   ▼                                       │
 │  EPT PDPTR            EPT Page Tables    │
 │   │                   (HPA 域)           │
 │   ▼                                       │
 │  EPT PDE                                 │
 │   │                                       │
 │   ▼                                       │
 │  EPT PTE  ──→ HPA (host 物理页)         │
 └──────────────────────────────────────────┘
           │
           ▼
       HPA (最终物理地址)
```

### 2.2 EPT 页表条目结构

每个 EPT 条目 8 字节：

```
EPT PML4E / PDPTE / PDE / PTE (64-bit):
┌────────────────────────────────────────────────────────────────┬───┬───┬──┐
│ 物理地址基址 (51:12)                                             │ ...│RWX│0 │
└────────────────────────────────────────────────────────────────┴───┴───┴──┘
```

EPT 权限位：

| 位 | 名称 | 含义 |
|----|------|------|
| 0 | Read | 可读 |
| 1 | Write | 可写 |
| 2 | Execute (user) | 用户模式可执行 |
| 3-5 | Memory Type | 内存类型 (WB/WC/UC/WP/WT) |
| 6 | IPAT | 忽略 PAT |
| 7 | Super Page | 大页 (1G/2M) |
| 10 | Execute (supervisor) | 内核模式可执行 |
| 12-51 | Physical Address | 物理页基址 |

EPT violation 时，硬件提供 `exit_qualification` 字段，包含：
- 访问类型 (read/write/execute)
- 是否因 guest 页表 entry 权限引起 (NMI unblocking)
- Gla (guest linear address) 有效性

---

## 3. KVM MMU 框架架构

KVM x86 MMU 子系统位于 `arch/x86/kvm/mmu/` 目录：

```
arch/x86/kvm/mmu/
├── mmu.c             — TDP MMU 和 Legacy MMU 通用逻辑
├── tdp_mmu.c         — TDP MMU 实现 (当前主流，约 60KB)
├── mmu_internal.h    — MMU 内部函数声明
├── paging.h          — 页表遍历辅助
├── paging_tmpl.h     — 页表遍历模板代码 (通过宏实例化)
├── spte.h            — Shadow PTE 格式定义
├── tdp_iter.h/c      — TDP MMU 页表迭代器
└── page_track.c      — 页面跟踪 (写保护/脏页)
```

### 3.1 核心函数路径

```
KVM_RUN → VM-Entry
    │
    ▼
Guest 访问 GPA (EPT violation 触发 VM-Exit)
    │
    ▼
handle_ept_violation()
    │
    ▼
kvm_mmu_page_fault()  ──────────────┐
    │                                │
    ├──→ kvm_tdp_mmu_map()          │ ← TDP MMU (当前主流)
    │    (arch/x86/kvm/mmu/tdp_mmu.c)│
    │                                │
    └──→ direct_page_fault()        │ ← Legacy MMU (fallback)
         (arch/x86/kvm/mmu/mmu.c)   │
```

---

## 4. TDP MMU: Two-Dimensional Paging MMU

### 4.1 设计动机

TDP MMU (arch/x86/kvm/mmu/tdp_mmu.c) 是 KVM 为 EPT/NPT 场景设计的新 MMU 实现，解决传统 MMU 的几个痛点：

| 传统 MMU 问题 | TDP MMU 解决方案 |
|-------------|---------------|
| mmu_lock 粗粒度，多 vCPU 争用 | 细粒度锁：每个 SPTE 可独立锁定 |
| 页表遍历和修改耦合 | 使用 TDP iterator 统一遍历 |
| ROLE-based lookup 效率低 | 直接 GPA→SPTE 查找 |
| 需要维护 rmap (反向映射) | 通过 page table walk 追踪 |

### 4.2 TDP MMU 锁设计

TDP MMU 最关键的设计是细粒度锁定，允许不同的 vCPU 并行修改不相交的 TDP 页表区域：

```
传统 MMU:
    kvm_mmu_page_fault()
        spin_lock(&kvm->mmu_lock)      // 整个 VM 的页表操作串行化
            ... 处理页故障 ...
        spin_unlock(&kvm->mmu_lock)

TDP MMU:
    kvm_tdp_mmu_map()
        tdp_iter_for_each_pte(iter, ...)
            // 遍历过程中按需锁定每个 SPTE
            tdp_mmu_set_spte_atomic()
                spin_lock(&iter->sptep_lock)  // 仅锁这个 SPTE
                WRITE_ONCE(*sptep, new_spte)
                spin_unlock(&iter->sptep_lock)
```

### 4.3 TDP Iterator (tdp_iter.h/c)

TDP iterator 提供统一的页表遍历接口：

```c
struct tdp_iter {
    u64 *pt_path[5];            // 从 root 到当前 PTE 的路径
    u64 *sptep;                 // 当前 SPTE 指针
    u64 old_spte;               // 修改前的值
    gfn_t gfn;                  // 当前 GFN
    u64 goal_level;             // 目标页表级别
    int root_level;             // root 级别 (4 或 5)
    int level;                  // 当前级别
    int min_level;              // 最小可映射级别
    unsigned long yield_safe;   // 安全 yield 点
    u64 *root;                  // 根 SPTE
    bool valid;                 // 迭代器是否有效
};
```

遍历示例：

```c
// 从 root 级开始遍历 GPA=gfn 对应的页表
tdp_iter_start(&iter, root_spte, root_level, gfn);

for_each_tdp_pte(iter, root_spte, root_level, gfn) {
    spte = *iter.sptep;
    if (!is_shadow_present_pte(spte)) {
        // 非 present, 需要分配新页表
        sp = tdp_mmu_alloc_sp(vcpu);
        tdp_mmu_init_child_sp(sp, &iter);
        tdp_mmu_link_sp(kvm, &iter, sp, false);
    }
    // 继续遍历下一级
    if (iter.level == PG_LEVEL_4K) {
        // 设置最终映射
        new_spte = make_spte(kvm, pfn, ...);
        tdp_mmu_set_spte(kvm, &iter, new_spte);
        break;
    }
}
```

### 4.4 tdp_mmu_map (核心映射函数)

```
tdp_mmu_map(vcpu, fault)
{
    for_each_tdp_pte(iter, vcpu->arch.mmu->root.hpa, fault->gfn) {
        spte = tdp_iter_spte(&iter);

        if (!is_shadow_present_pte(spte)) {
            // 缺失 → 需要建立映射
            if (iter.level > fault->goal_level) {
                // 尚不是目标级别 → 分配中间页表
                sp = tdp_mmu_alloc_sp(vcpu);
                tdp_mmu_init_child_sp(sp, &iter);
                tdp_mmu_link_sp(kvm, &iter, sp, false);
                continue;  // 下一轮遍历子页表
            } else {
                // 达到目标级别 → 建立最终映射
                new_spte = kvm_tdp_mmu_map_spte(vcpu, fault, iter);
                tdp_mmu_set_spte_atomic(vcpu, &iter, new_spte);
                break;
            }
        }

        // 检查是否需要修改该 SPTE
        if (tdp_mmu_spte_changed(kvm, &iter, spte))
            break;

        // 如果当前级别就是目标级别, 处理大页重映射
        if (iter.level == fault->goal_level) {
            // build final SPTE
            break;
        }
    }
}
```

### 4.5 TDP MMU 的 split 与 collapse

**Split (大页拆分)**：
当大页 (2M/1G) 的某个 4K 子页需要不同权限时，将大页拆为多个 4K 页：

```
Before:                          After:
┌──────────────┐                ┌──────────────┐
│ 2M 大页 SPTE  │                │ PDE → 指向下一级表 │
│ (R/W/X=111)  │  ──split──→   │                 │
└──────────────┘                │  ├── 4K PTE (RO) │
                                │  ├── 4K PTE (RW) │
                                │  ├── 4K PTE (RW) │
                                      ... (512个)
```

**Collapse (合并)**：当拆分的 4K 页全部具有相同属性时，合并回大页。

### 4.6 Zap (回收/失效) 操作

TDP MMU 使用基于 generation 的快速回收：

```c
void kvm_tdp_mmu_zap_all(struct kvm *kvm)
{
    // 1. 递增 generation (无效化所有现有 SPTE)
    kvm->arch.tdp_mmu_zap_gen++;
    //   所有 reader 需要检查 generation

    // 2. 回收所有 page table
    for_each_tdp_mmu_root(kvm, root, root_idx) {
        tdp_mmu_zap_root(kvm, root);
    }
}
```

### 4.7 MMU Notifier 集成

KVM 通过 `mmu_notifier` 感知 host 内存管理变动：

```c
static const struct mmu_notifier_ops kvm_mmu_notifier_ops = {
    .invalidate_range_start = kvm_mmu_notifier_invalidate_range_start,
    .invalidate_range_end   = kvm_mmu_notifier_invalidate_range_end,
    .clear_flush_young      = kvm_age_gfn,
    .clear_young            = kvm_test_age_gfn,
    .change_pte             = kvm_set_spte_gfn,
};

int kvm_mmu_notifier_invalidate_range_start(struct mmu_notifier *mn,
                                            const struct mmu_notifier_range *range)
{
    // 1. 阻止新的页故障处理
    // 2. 写保护或回收范围内所有 SPTE
    // 3. 排空所有正在进行的 page fault
    // 4. 等待 → 返回
}
```

---

## 5. Shadow MMU (Legacy MMU)

当 EPT/NPT 不可用时，KVM 回退到 Shadow MMU。

### 5.1 工作原理

Shadow MMU 为 guest 页表维护一份 "影子" 拷贝。当 guest 设置其 CR3 指向某 guest 页表时，KVM 分配对应的影子页表，将其 HPA 写入 vCPU 的 CR3。

```
guest CR3 (GPA) → guest PML4 (GPA)
                         │
      KVM 影子 PML4 (HPA) ← 硬件 CR3
              │
      KVM 影子 PDPTR (HPA)
              │
      KVM 影子 PDE (HPA)
              │
      KVM 影子 PTE (HPA) → HPA
```

影子页表的同步通过以下机制：

1. **Guest 修改页表** → 触发写保护 fault → KVM 重新同步
2. **Guest 切换 CR3** → KVM 换影子页表根
3. **Guest INVLPG** → KVM 刷新对应影子页表条目

### 5.2 kvm_mmu_page 结构

```c
struct kvm_mmu_page {
    struct list_head link;       // 链表 (active/inactive)
    struct hlist_node hash_link; // 哈希表 (role + gfn)

    bool unsync;                 // 是否和 guest 页表不同步
    u8 unsync_children;          // 多少子页表未同步

    union kvm_mmu_page_role role; // 描述此页表角色:
        // .level        — 页表级别
        // .gpte_is_8_bytes — guest PTE 大小
        // .quadrant     — 页表象限
        // .direct       — 直接映射 (TDP)
        // .access       — 访问模式
        // .invalid      — 是否失效
        // .efer_nx      — NX 启用
        // .cr0_wp       — CR0.WP
        // .smep_andnot_wp — SMEP
        // .smap_andnot_wp — SMAP
        // .ad_disabled  — A/D bits 禁用
        // .guest_mode   — 是否嵌套 guest

    gfn_t gfn;                   // 此页表映射的 guest 页表 GFN

    u64 *spt;                    // 指向影子页表物理页
    unsigned long *unsync_child_bitmap;

    struct kvm_rmap_head parent_ptes; // 指向此页的父 SPTE (rmap)
    ...
};
```

### 5.3 反向映射 (rmap)

传统 MMU 维护反向映射表：给定 GFN，找到所有映射它的 SPTE。这在 MMU 回收和写保护场景必不可少。

```c
// 通过 rmap 快速定位所有引用指定 GFN 的 SPTE
struct kvm_rmap_head *rmap = gfn_to_rmap(kvm, gfn, sp);
for_each_rmap_spte(rmap, iter, sptep) {
    // sptep 指向一个映射 gfn 的 SPTE
    *sptep = set_spte_flags(*sptep, spte & ~PT_WRITABLE_MASK);
}
```

TDP MMU 不需要 rmap，因为可以通过 page table walk 找到所有引用。

---

## 6. 页故障处理全流程

```
                        Guest 访问内存
                              │
                              ▼
                      ┌───────────────┐
                      │ Hardware Page  │
                      │ Table Walk     │
                      └───────┬───────┘
                              │
              ┌───────────────┼────────────────┐
              │               │                │
       ┌──────▼──────┐ ┌─────▼─────┐  ┌───────▼───────┐
       │ EPT violation│ │ EPT       │  │ Guest #PF    │
       │ (no mapping) │ │ misconfig │  │ (EPT present │
       │              │ │           │  │ but guest     │
       │              │ │           │  │ page table    │
       │              │ │           │  │ fault)        │
       └──────┬──────┘ └─────┬─────┘  └───────┬───────┘
              │              │                │
              ▼              ▼                ▼
    handle_ept_violation  handle_ept_    kvm_handle_page_
                          misconfig      fault (注入 #PF)
              │              │                │
              ▼              ▼                │
         kvm_mmu_page_fault ←────────────────┘
              │
              ▼
         ┌─────────────────────────────────┐
         │ 1. 检查是否是快速 PF 可满足      │
         │    (不修改 guest 页表)           │
         │    - 仅需设置 SPTE               │
         │    - dirty logging 写保护        │
         └────────────┬────────────────────┘
                      │ 不是快速 PF
                      ▼
         ┌─────────────────────────────────┐
         │ 2. TDP MMU or Legacy MMU?       │
         │                                 │
         │    TDP: kvm_tdp_mmu_map()       │
         │    ├── 遍历 TDP 页表            │
         │    ├── 遇到 non-present:        │
         │    │   ├── 分配 struct page (SP)│
         │    │   ├── 链接到父 SPTE        │
         │    │   └── 继续遍历             │
         │    ├── 达到目标级别:            │
         │    │   ├── get_user_pages()     │
         │    │   ├── 计算 SPTE 权限和类型 │
         │    │   ├── make_spte()          │
         │    │   └── tdp_mmu_set_spte()   │
         │    └── 设置 dirty logging 等    │
         └─────────────────────────────────┘
```

### 6.1 fast_page_fault

fast_page_fault 是一种优化，在 GPF 不需要遍历 guest 页表时适用：

```c
static bool fast_page_fault(struct kvm_vcpu *vcpu, struct kvm_page_fault *fault)
{
    // 适用于:
    // 1. 只需修改 SPTE 权限位 (如写保护)
    // 2. 只需原子比较和交换 SPTE
    // 3. TDP 模式下 guest PT 不变
    // 不需要:
    //   - 分配新页表
    //   - get_user_pages()
    return true/false;
}
```

如果 fast_page_fault 成功，整个处理过程无需锁，延迟极低。

---

## 7. SPTE 格式 (spte.h)

`arch/x86/kvm/mmu/spte.h` 定义了 SPTE (Shadow PTE) 的格式。

### 7.1 Standard SPTE

```
64-bit Shadow PTE (TDP / Legacy):
┌──────────────────────────────────────────────┬──────┬──────┐
│ 物理页帧地址 (PFN)                            │ 属性 │ 标志 │
│ (51:12) = HPA >> 12                         │      │      │
├──────────────────────────────────────────────┼──────┼──────┤
│ Bit 63: suppress #VE (EPT only)             │      │      │
│ Bit 62: EPT ignore PAT                      │      │      │
│ Bit 61: EPT memory type                      │      │      │
│ Bit 60: EPT memory type                      │      │      │
│ Bit 59: EPT memory type                      │      │      │
│ Bit 58: PFN reserved                         │      │      │
│ ...                                          │      │      │
│ Bit 12: PFN bit 0 (4K aligned)              │      │      │
├──────────────────────────────────────────────┼──────┼──────┤
│ Bit 11:                                    │      │      │
│ Bit 10: EPT Execute (user mode)             │      │      │
│ Bit 9:  EPT Write                           │      │      │
│ Bit 8:  EPT Read or KVM directed mmio flag  │      │      │
│ Bit 7:  MMIO SPTE flag (direct mmio)        │      │      │
│ Bit 6:  Accessed (A) bit                     │      │      │
│ Bit 5:  Dirty (D) bit / Write protect       │      │      │
│ Bit 4:  Host writable (软件控制)              │      │      │
│ Bit 3:  MMU present / write tracking         │      │      │
│ Bit 2:  Shadow present (软件跟踪)             │      │      │
│ Bit 1:  Writable                            │      │      │
│ Bit 0:  Present or MMU spte present          │      │      │
└──────────────────────────────────────────────┴──────┴──────┘
```

### 7.2 SPTE 类型编码与 MMIO 检测

```c
// Non-present SPTE 值使用低 12 位编码特殊状态:
#define SPTE_MMU_PRESENT_MASK   BIT_ULL(0)
#define SPTE_MMU_WRITEABLE_MASK BIT_ULL(1)
#define SPTE_HOST_WRITABLE      BIT_ULL(4)
#define SPTE_MMU_WRITABLE       BIT_ULL(5)
#define SPTE_TDP_ACCESSED_MASK  BIT_ULL(6)
#define SPTE_MMIO_MASK          BIT_ULL(7)     // MMIO SPTE 标识
```

MMIO SPTE 使用 PFN 域存储 GFN 而非 HPA。当 GPA 不在任何 memslot 中时，KVM 创建 MMIO SPTE，后续访问触发 `handle_ept_misconfig` 返回用户态处理。

### 7.3 Accessed / Dirty 跟踪

EPT/NPT 可选启用硬件 A/D 跟踪：启用时首次访问/写入触发 VM-Exit (KVM 设 A/D=1)，后续直接命中无 VM-Exit；禁用时每次首次访问/写入都触发 VM-Exit。

---

## 8. Dirty Logging 与 MMU 锁

### 8.1 脏页跟踪

实时迁移使用写保护：将全部 memslot SPTE 标记只读 → guest 首次写入触发 EPT violation → 标记 dirty 并解除写保护 → 迭代时重新写保护。每个 vCPU 独立 dirty ring 避免 cache line 争用，满时通过 `KVM_REQ_DIRTY_RING_FULL` 强制退出。

### 8.2 MMU 锁层次

```
mutex_lock(&kvm->slots_lock)       — 保护 memslot 更新
  spin_lock(&kvm->mmu_lock)         — 传统 MMU: shadow page hash, SPTE, rmap
    rcu_read_lock()                 — TDP MMU: page table 遍历 (无锁读)
      tdp_mmu_spte_lock(sptep)      — TDP MMU: per-SPTE 原子修改
    spin_lock(&tdp_mmu_pages_lock)  — TDP MMU page list
```

TDP MMU 将 mmu_lock 细化为 per-SPTE 锁，多 vCPU 可并行修改不相交页表区域。

---

## 9. MMU 页回收与 5 级页表

### 9.1 两阶段回收

Zap 使用 `kvm_zap_obsolete_pages()` 设置 invalid generation → 等待 SRCU reader → 释放页表物理页。内存压力时通过 shrinker 接口进行 LRU 回收：`mmu_shrink_scan()` → `kvm_mmu_zap_oldest_mmu_pages()`。

### 9.2 5 级页表

CR4.LA57 控制 guest 使用 48-bit (4级) 或 57-bit (5级) 虚拟地址。EPT 同样支持 5 级 (MAXPHYADDR 最大 52 位)：EPT PML5 → EPT PML4 → EPT PDPTR → EPT PDE → EPT PTE。

## 10. 总结

```
                      ┌─────────────────────────────────┐
                      │        KVM MMU 架构总览          │
                      └─────────────────────────────────┘

    guest 虚拟地址 (GVA)               guest 物理地址 (GPA)
          │                                  │
          ▼                                  ▼
    ┌──────────┐                      ┌──────────────┐
    │ Guest PT │    页故障 (VM-Exit)   │  EPT/NPT PT  │
    │ (GPA域)  │◄────────────────────►│  (HPA域)     │
    └──────────┘                      └──────────────┘
          │                                  │
          ▼                                  ▼
  ┌───────────────────────────────────────────────────┐
  │                  KVM MMU Layer                     │
  │                                                    │
  │   ┌──────────────────┐   ┌─────────────────────┐  │
  │   │  TDP MMU          │   │  Shadow MMU         │  │
  │   │  (for EPT/NPT)    │   │  (for legacy hw)    │  │
  │   │                    │   │                      │  │
  │   │  - SPTE细粒度锁    │   │  - mmu_lock 粗粒度  │  │
  │   │  - TDP iterator    │   │  - rmap反向映射     │  │
  │   │  - 无需rmap        │   │  - role+based hash  │  │
  │   │  - split/collapse  │   │  - unsync 跟踪      │  │
  │   └─────────┬──────────┘   └──────────┬──────────┘  │
  │             │                         │              │
  │             └──────────┬──────────────┘              │
  │                        │                             │
  │          ┌─────────────▼─────────────┐               │
  │          │    page fault handler     │               │
  │          │    fast_page_fault 优化   │               │
  │          │    make_spte / set_spte   │               │
  │          │    dirty logging / aging  │               │
  │          └───────────────────────────┘               │
  └───────────────────────────────────────────────────┘
```

---

## 下一篇文章

[第4篇：设备直通 — VFIO、IOMMU 与中断重映射](./04-vfio.md)
