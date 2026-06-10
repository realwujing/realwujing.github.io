# 第2篇：HMM 如何遍历 CPU 页表 — hmm_range_fault 内部全解

> 源码：`mm/hmm.c` | 对应头文件：`include/linux/hmm.h`

系列目录：[SoC 统一内存架构深度解析](./README.md)

---

## HMM 的角色

HMM（Heterogeneous Memory Management）是 Linux 内核中的一个**只读页表遍历器**。它不修改 CPU 页表内容（除非触发了 fault-in），也不分配新页面——它只是读取 CPU 页表的当前状态，把物理页帧号（PFN）连同权限标志填入 `hmm_range.pfns[]` 数组返回给调用者（通常是 GPU 驱动）。

HMM 与 GPU 统一内存的关系：
```
GPU 驱动调用 hmm_range_fault(range)
    │
    ▼
HMM 遍历 CPU 页表，收集 PFN
    │
    ▼
GPU 驱动获得物理地址 → 建立 GPU 页表（通过 SMMU 或本地映射）
    │
    ▼
GPU 可以直接访问 CPU 内存（双向可见）
```

---

## HMM PFN 标志位

`include/linux/hmm.h:38-58` 定义了 PFN 数组每一项的位域编码，使用 `unsigned long` 的高位做标志，低 53 位存 PFN：

```c
// include/linux/hmm.h:38-57
enum hmm_pfn_flags {
    /* Output fields and flags */
    HMM_PFN_VALID      = 1UL << (BITS_PER_LONG - 1),  // line 40 — PFN 有效
    HMM_PFN_WRITE      = 1UL << (BITS_PER_LONG - 2),  // line 41 — 页面可写
    HMM_PFN_ERROR      = 1UL << (BITS_PER_LONG - 3),  // line 42 — 无法访问

    /* Sticky flags, carried from input to output */
    HMM_PFN_DMA_MAPPED = 1UL << (BITS_PER_LONG - 4),  // line 47
    HMM_PFN_P2PDMA     = 1UL << (BITS_PER_LONG - 5),  // line 48
    HMM_PFN_P2PDMA_BUS = 1UL << (BITS_PER_LONG - 6),  // line 49

    HMM_PFN_ORDER_SHIFT = (BITS_PER_LONG - 11),        // line 51 — order 位起始

    /* Input flags (reuse output values) */
    HMM_PFN_REQ_FAULT   = HMM_PFN_VALID,               // line 54 — 请求 fault-in
    HMM_PFN_REQ_WRITE   = HMM_PFN_WRITE,               // line 55 — 请求可写

    HMM_PFN_FLAGS       = ~((1UL << HMM_PFN_ORDER_SHIFT) - 1), // line 57
};
```

关键设计：输入标志 `HMM_PFN_REQ_FAULT` 复用 `HMM_PFN_VALID` 的位值。调用者在 `pfns[i]` 中设置请求位，HMM 遍历后覆盖为结果位。`HMM_PFN_ORDER_SHIFT` 预留 10 位用于存储大页的 order（`hmm.c:181-184`）。

---

## walk_ops 注册

HMM 使用内核的 `walk_page_range()` 框架进行页表遍历，通过 `mm_walk_ops` 结构体注册各级回调。定义在 `mm/hmm.c:631-638`：

```c
// mm/hmm.c:631-638
static const struct mm_walk_ops hmm_walk_ops = {
    .pud_entry      = hmm_vma_walk_pud,            // line 632 — PUD 大页处理
    .pmd_entry      = hmm_vma_walk_pmd,            // line 633 — 主路径
    .pte_hole       = hmm_vma_walk_hole,           // line 634 — 空洞处理
    .hugetlb_entry  = hmm_vma_walk_hugetlb_entry,   // line 635 — HugeTLB
    .test_walk      = hmm_vma_walk_test,            // line 636 — VMA 过滤
    .walk_lock      = PGWALK_RDLOCK,                // line 637 — 只读锁
};
```

### test_walk 过滤

`hmm_vma_walk_test`（`hmm.c:597-629`）在每个 VMA 开始遍历前被调用：

```c
// hmm.c:597-606
static int hmm_vma_walk_test(unsigned long start, unsigned long end,
                             struct mm_walk *walk)
{
    // line 604: 合格条件 — 非 IO/非 PFNMAP 且可读
    if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)) &&
        vma->vm_flags & VM_READ)
        return 0;     // 通过，继续遍历

    // line 619-623: 不合格 — 如果请求了 fault 则 -EFAULT
    if (hmm_range_need_fault(...))
        return -EFAULT;

    // line 625: 否则填 HMM_PFN_ERROR 并跳过此 VMA
    hmm_pfns_fill(start, end, range, HMM_PFN_ERROR);
    return 1;         // 跳过该 VMA，继续下一个
}
```

返回值 1 表示 "跳过该 VMA"，返回值 0 表示 "正常遍历该 VMA"。`VM_IO` 和 `VM_PFNMAP` 的 VMA 没有 `struct page` 支撑，HMM 无法从中提取 PFN。

---

## hmm_range_fault 入口

`mm/hmm.c:659-685`：

```c
// hmm.c:659-685
int hmm_range_fault(struct hmm_range *range)
{
    struct hmm_vma_walk hmm_vma_walk = {
        .range = range,
        .last  = range->start,
    };
    struct mm_struct *mm = range->notifier->mm;
    int ret;

    mmap_assert_locked(mm);               // line 668 — 必须持有 mmap_lock

    do {
        // line 672-673: 检查 invalidation sequence
        if (mmu_interval_check_retry(range->notifier,
                                     range->notifier_seq))
            return -EBUSY;

        // line 675-676: 用 mm_walk_ops 遍历页表
        ret = walk_page_range(mm, hmm_vma_walk.last, range->end,
                              &hmm_walk_ops, &hmm_vma_walk);
        // -EBUSY 时 hmm_vma_walk.last 指向未处理地址，重新开始
    } while (ret == -EBUSY);

    return ret;
}
```

关键循环：`walk_page_range()` 返回 `-EBUSY` 表示发生了 fault-in（触发 `handle_mm_fault`）或遇到了迁移中的 PTE，此时 `hmm_vma_walk.last` 指向尚未处理的地址，do-while 循环重启遍历。`mmu_interval_check_retry` 检测是否有并发的 mmu notifier invalidation 发生（如页面被 swap out）。三者共同保证遍历结果的一致性和正确性。

---

## PMD 级遍历 — 主路径

`hmm_vma_walk_pmd`（`hmm.c:396-471`）是 HMM 最核心的函数，处理 PMD 级别的所有情况：

```
hmm_vma_walk_pmd(start, end, walk)
    │
    ├── pmd_none → hmm_vma_walk_hole()             // line 412-413
    │     └── need_fault? → hmm_vma_fault()         // line 73-94
    │         └── handle_mm_fault() × N pages       // line 90
    │         └── 返回 -EBUSY 触发重走
    │
    ├── pmd_is_migration_entry                     // line 415-422
    │     └── need_fault? → pmd_migration_entry_wait() → -EBUSY
    │
    ├── !pmd_present → hmm_vma_handle_absent_pmd() // line 424-426
    │     └── DEVICE_PRIVATE? → 直接返回 PFN (不迁移!)
    │     └── 否则: need_fault? → fault 或 -EFAULT
    │
    ├── pmd_trans_huge → hmm_vma_handle_pmd()      // line 428-442
    │     └── 大页: 提取 PFN + order 标志
    │     └── need_fault? → 返回 -EFAULT (THP 不拆分)
    │
    ├── pmd_bad → -EFAULT 或 ERROR                  // line 451-455
    │
    └── 正常 PTE 表 → 逐条遍历 PTE                  // line 457-468
          │
          └── for each PTE:
                hmm_vma_handle_pte(walk, addr, end, pmdp, ptep, hmm_pfn)
```

遍历 PMD 的核心代码（`hmm.c:457-468`）：

```c
// hmm.c:457-468
ptep = pte_offset_map(pmdp, addr);
if (!ptep)
    goto again;
for (; addr < end; addr += PAGE_SIZE, ptep++, hmm_pfns++) {
    int r;
    r = hmm_vma_handle_pte(walk, addr, end, pmdp, ptep, hmm_pfns);
    if (r) {
        /* hmm_vma_handle_pte() did pte_unmap() */
        return r;
    }
}
pte_unmap(ptep - 1);
return 0;
```

---

## PTE 级处理

`hmm_vma_handle_pte`（`hmm.c:235-332`）处理单个 PTE：

```c
// hmm.c:235-332
static int hmm_vma_handle_pte(struct mm_walk *walk, unsigned long addr,
                              unsigned long end, pmd_t *pmdp, pte_t *ptep,
                              unsigned long *hmm_pfn)
{
    pte_t pte = ptep_get(ptep);
    uint64_t pfn_req_flags = *hmm_pfn;

    // Case 1: pte_none — 无映射 (line 252)
    if (pte_none(pte) || pte_is_uffd_wp_marker(pte)) {
        required_fault = hmm_pte_need_fault(...);  // 检查是否请求 fault
        if (required_fault)
            goto fault;    // → hmm_vma_fault → handle_mm_fault
        goto out;          // 不请求 fault，返回 0
    }

    // Case 2: !pte_present — swap/migration/device (line 260)
    if (!pte_present(pte)) {
        softleaf_t entry = softleaf_from_pte(pte);

        // ★ 关键: DEVICE_PRIVATE 检查 (line 267-275)
        if (softleaf_is_device_private(entry) &&
            page_pgmap(softleaf_to_page(entry))->owner ==
            range->dev_private_owner) {
            // 属于调用者自己的设备内存 → 直接返回 PFN，不迁移!
            new_pfn_flags = softleaf_to_pfn(entry) | HMM_PFN_VALID;
            goto out;
        }

        // 别人的 DEVICE_PRIVATE → fault (line 285-286)
        if (softleaf_is_device_private(entry))
            goto fault;

        // migration entry → wait (line 291-295)
        if (softleaf_is_migration(entry)) {
            migration_entry_wait(walk->mm, pmdp, addr);
            return -EBUSY;
        }

        // swap entry → fault (line 282-283)
        if (softleaf_is_swap(entry))
            goto fault;

        return -EFAULT;
    }

    // Case 3: normal present PTE (line 303)
    cpu_flags = pte_to_hmm_pfn_flags(range, pte);
    required_fault = hmm_pte_need_fault(...);
    if (required_fault)
        goto fault;    // 需要写但不可写 → fault

    // 非 struct page 内存检查 (line 313)
    if (!vm_normal_page(walk->vma, addr, pte) && !is_zero_pfn(...))
        new_pfn_flags = HMM_PFN_ERROR;
    else
        new_pfn_flags = pte_pfn(pte) | cpu_flags;

out:
    *hmm_pfn = (*hmm_pfn & HMM_PFN_INOUT_FLAGS) | new_pfn_flags;
    return 0;

fault:
    pte_unmap(ptep);
    return hmm_vma_fault(addr, end, required_fault, walk);  // → -EBUSY
}
```

### hmm_pte_need_fault 决策逻辑

`hmm.c:96-128` 判断是否需要触发缺页中断：

```c
// hmm.c:96-128
static unsigned int hmm_pte_need_fault(const struct hmm_vma_walk *hmm_vma_walk,
                                       unsigned long pfn_req_flags,
                                       unsigned long cpu_flags)
{
    // 合并 per-page 请求和 range 级默认标志
    pfn_req_flags &= range->pfn_flags_mask;
    pfn_req_flags |= range->default_flags;

    // 未请求 fault → 不处理
    if (!(pfn_req_flags & HMM_PFN_REQ_FAULT))
        return 0;

    // 请求写权限但页面不可写 → WRITE_FAULT
    if ((pfn_req_flags & HMM_PFN_REQ_WRITE) &&
        !(cpu_flags & HMM_PFN_WRITE))
        return HMM_NEED_FAULT | HMM_NEED_WRITE_FAULT;

    // 页面无效 → FAULT
    if (!(cpu_flags & HMM_PFN_VALID))
        return HMM_NEED_FAULT;

    return 0;  // 不需要 fault
}
```

---

## hmm_vma_fault — 触发缺页中断

`hmm.c:73-94` 是唯一修改 CPU 页表的地方：

```c
// hmm.c:73-94
static int hmm_vma_fault(unsigned long addr, unsigned long end,
                         unsigned int required_fault, struct mm_walk *walk)
{
    struct vm_area_struct *vma = walk->vma;
    unsigned int fault_flags = FAULT_FLAG_REMOTE;

    if (required_fault & HMM_NEED_WRITE_FAULT) {
        if (!(vma->vm_flags & VM_WRITE))
            return -EPERM;
        fault_flags |= FAULT_FLAG_WRITE;
    }

    for (; addr < end; addr += PAGE_SIZE)
        if (handle_mm_fault(vma, addr, fault_flags, NULL) &
            VM_FAULT_ERROR)
            return -EFAULT;

    return -EBUSY;  // ← 总是返回 EBUSY，让主循环重新遍历!
}
```

注意：即使 `handle_mm_fault` 成功，也返回 `-EBUSY` 而非 0。这是因为 fault-in 后 CPU 页表已被修改，之前获取的锁和上下文可能已经释放（mmap_lock 可能在 `__handle_mm_fault` 中被 drop/reacquire），必须重新从头遍历以保证一致性。

---

## DEVICE_PRIVATE 的特殊处理

HMM 内核中最精妙的设计之一是对 DEVICE_PRIVATE 页面的处理。

### PTE 级别的 DEVICE_PRIVATE

`hmm.c:260-275`：当 PTE 为 `!pte_present` 且 entry 类型为 DEVICE_PRIVATE 时：

```c
// hmm.c:267-275
if (softleaf_is_device_private(entry) &&
    page_pgmap(softleaf_to_page(entry))->owner ==
    range->dev_private_owner) {
    // 这是调用者自己的 DEVICE_PRIVATE 页面
    // → 直接返回 PFN，零拷贝，不触发迁移！
    cpu_flags = HMM_PFN_VALID;
    if (softleaf_is_device_private_write(entry))
        cpu_flags |= HMM_PFN_WRITE;
    new_pfn_flags = softleaf_to_pfn(entry) | cpu_flags;
    goto out;
}
```

如果该 DEVICE_PRIVATE 页面的 owner 与 `range->dev_private_owner` 匹配，HMM 直接返回 PFN 而不需要迁移。这意味着 GPU 驱动可以**零拷贝**访问自己已经在 GPU 内存中的页面。

### PMD 级别的 DEVICE_PRIVATE

`hmm_vma_handle_absent_pmd`（`hmm.c:334-380`）处理 PMD 级别的 DEVICE_PRIVATE 条目（大页情况）：

```c
// hmm.c:346-367
if (softleaf_is_device_private(entry) &&
    softleaf_to_folio(entry)->pgmap->owner == range->dev_private_owner) {
    unsigned long cpu_flags = HMM_PFN_VALID |
        hmm_pfn_flags_order(PMD_SHIFT - PAGE_SHIFT);  // 大页 order
    unsigned long pfn = softleaf_to_pfn(entry);

    for (i = 0; addr < end; addr += PAGE_SIZE, i++, pfn++) {
        hmm_pfns[i] &= HMM_PFN_INOUT_FLAGS;
        hmm_pfns[i] |= pfn | cpu_flags;
    }
    return 0;
}
```

PMD 级别的 DEVICE_PRIVATE 使用 `hmm_pfn_flags_order()` 编码 order 信息，GPU 驱动可以通过 `hmm_pfn_to_map_order()` 恢复原始的大页 order。

---

## 完整 HMM 遍历流程图

```
hmm_range_fault(range)
    │
    ├── mmap_assert_locked(mm)                              // hmm.c:668
    │
    ▼
    do {
        mmu_interval_check_retry? → -EBUSY                  // hmm.c:672
        │
        walk_page_range(mm, start, end, &hmm_walk_ops)      // hmm.c:675
            │
            ├── test_walk: 过滤 VM_IO/VM_PFNMAP              // hmm.c:597
            │     └── 不过滤 → 返回 PFN_ERROR
            │
            ├── PGD/P4D/PUD 级别 (folded to pmd)
            │
            ├── .pud_entry = hmm_vma_walk_pud               // hmm.c:632
            │     └── pud_trans_huge → HMM_PFN_VALID + order
            │     └── !pud_present → hmm_vma_walk_hole
            │
            ├── .pmd_entry = hmm_vma_walk_pmd               // hmm.c:633 ★主路径
            │     │
            │     ├── pmd_none → hmm_vma_walk_hole
            │     ├── pmd_migration → wait → -EBUSY
            │     ├── !pmd_present → hmm_vma_handle_absent_pmd
            │     │     └── DEVICE_PRIVATE(own) → PFN 直接返回
            │     │     └── 其他 → fault 或 ERROR
            │     ├── pmd_trans_huge → hmm_vma_handle_pmd
            │     │     └── 大页 PFN + order
            │     └── 正常 PTE 表
            │           └── for each PTE:                    // hmm.c:460
            │                 hmm_vma_handle_pte()           // hmm.c:235
            │                   ├── pte_none → fault? 或 0
            │                   ├── DEVICE_PRIVATE(own) → PFN
            │                   ├── swap → fault
            │                   ├── migration → wait → -EBUSY
            │                   └── present → PFN + flags
            │
            ├── .pte_hole = hmm_vma_walk_hole               // hmm.c:634
            │     └── need_fault? → hmm_vma_fault → -EBUSY
            │
            └── .hugetlb_entry                              // hmm.c:635
                  └── need_fault? → fault 或 direct PFN
    } while (ret == -EBUSY);                                // hmm.c:683

    return 0;
```

## 总结

HMM 通过 `walk_page_range`（`hmm.c:675`）+ `hmm_walk_ops`（`hmm.c:631-638`）建立了一个只读的 CPU 页表遍历框架。其核心价值在于：

1. **只读语义**：正常情况下不修改 CPU 页表
2. **按需 fault-in**：通过 `hmm_vma_fault`（`hmm.c:73-94`）触发 `handle_mm_fault`
3. **DEVICE_PRIVATE 优化**：对于自己的 DEVICE_PRIVATE 页面（`hmm.c:267-275`），直接返回 PFN 而不迁移
4. **一致性保证**：通过 `mmu_interval_check_retry`（`hmm.c:672`）和 `-EBUSY` 重试循环保证并发安全
5. **大页支持**：PMD 级 THP（`hmm.c:428-442`）和 PUD 级大页（`hmm.c:484-529`）都通过 order 位编码返回

## 下一篇文章

→ [第3篇：ARM SMMU 页表镜像 — IOMMU 如何把 CPU 页表给 GPU 用](./03-smmu-page-table.md)
