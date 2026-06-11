# 第4篇：缺页处理与 mmap 生命周期 — do_anonymous_page、COW 与 VMA 操作

> 源码：`mm/memory.c` `mm/mmap.c` | 头文件：`include/linux/mm.h`

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. struct vm_fault — 缺页上下文

```c
// include/linux/mm.h:715-759
struct vm_fault {
    const struct {
        struct vm_area_struct *vma;          // 出错的 VMA
        gfp_t gfp_mask;                      // 分配掩码
        pgoff_t pgoff;                       // 页偏移
        unsigned long address;                // 虚拟地址
        unsigned long real_address;           // 未对齐的真实地址
    };
    enum fault_flag flags;                   // FAULT_FLAG_WRITE/FAULT_FLAG_USER 等
    pmd_t *pmd;
    pud_t *pud;
    union { pte_t orig_pte; pmd_t orig_pmd; };
    struct page *cow_page;                   // ★ COW 时分配的新页
    struct page *page;                       // 映射到进程的页
    pte_t *pte;                              // 页表项指针
    spinlock_t *ptl;                         // 页表锁
};
```

---

## 2. 缺页入口 — handle_mm_fault

```c
// mm/memory.c:6699
vm_fault_t handle_mm_fault(struct vm_area_struct *vma,
                           unsigned long address, unsigned int flags,
                           struct pt_regs *regs)
{
    // ① 检查 arch_vma_access_permitted
    // ② hugetlb? → hugetlb_fault
    // ③ 正常 → __handle_mm_fault(vma, address, flags)
}
```

```c
// mm/memory.c:6465
static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
                                    unsigned long address, unsigned int flags)
{
    // ① 逐级分配页表: pgd_offset → p4d_alloc → pud_alloc → pmd_alloc
    // ② 检查 PMD 级别的 THP: pmd_trans_huge → handle PTE 级别 fallback
    // ③ 最终调用: handle_pte_fault(&vmf)
}
```

---

## 3. 四种缺页分支 — handle_pte_fault

```
handle_pte_fault(vmf)
  │
  ├── pte_none?
  │     └── do_pte_missing(vmf)          [memory.c:4561]
  │           ├── vma->vm_ops 有 fault? → do_fault(vmf)   [memory.c:6013]
  │           └── 无 → do_anonymous_page(vmf)              [memory.c:5337]
  │
  ├── !pte_present?
  │     └── do_swap_page(vmf)            ← 从 swap 换入
  │
  ├── pte_protnone && vma_is_accessible?
  │     └── do_numa_page(vmf)            ← NUMA 自动迁移
  │
  └── FAULT_FLAG_WRITE && !pte_write?
        └── do_wp_page(vmf)              ← Copy-on-Write  [memory.c:4244]
```

---

## 4. do_anonymous_page — 匿名页分配

```c
// memory.c:5337
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;

    // 从 buddy 分配一个 zeroed 4KB 页
    folio = vma_alloc_zeroed_movable_folio(vma, vmf->address);   // line 5372

    // 设置 pte: 写权限 + 脏标志
    entry = mk_pte(&folio->page, vma->vm_page_prot);
    if (vmf->flags & FAULT_FLAG_WRITE)
        entry = pte_mkwrite(pte_mkdirty(entry), vma);

    // ★ 安装页表项
    set_pte_at(vma->vm_mm, vmf->address, vmf->pte, entry);
    return 0;
}
```

**这是最常见的缺页路径**：malloc/fork/stack 扩展等第一次访问内存时走这里。

---

## 5. do_fault — 文件映射缺页

```c
// memory.c:6013
static vm_fault_t do_fault(struct vm_fault *vmf)
{
    // ① fault-around: 预取相邻页面（提高顺序读取性能）
    do_fault_around(vmf);                                    // memory.c:5843

    // ② 从 page cache 读取
    folio = __filemap_get_folio(vmf->vma->vm_file->f_mapping,
                                vmf->pgoff, ...);
    if (!folio) {
        // page cache miss → 从磁盘读
        ret = vmf->vma->vm_ops->fault(vmf);
    }

    // ③ 安装 PTE
    entry = mk_pte(&folio->page, vma->vm_page_prot);
    set_pte_at(vma->vm_mm, vmf->address, vmf->pte, entry);
}
```

---

## 6. do_wp_page — Copy-on-Write

```c
// memory.c:4244
static vm_fault_t do_wp_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;

    // ① 如果该页只有一个映射者（!_mapcount）→ 可以直接重用
    if (folio_ref_count(folio) == 1) {
        // ★ 直接赋写权限，无需复制
        entry = pte_mkwrite(entry, vma);
        set_pte_at(vma->vm_mm, vmf->address, vmf->pte, entry);
        return 0;
    }

    // ② 分配新页
    new_folio = vmf->cow_page;

    // ③ 复制旧页内容到新页
    copy_user_highpage(&new_folio->page, &folio->page,
                       vmf->address, vma);

    // ④ 安装新 PTE（写权限）
    entry = pte_mkwrite(mk_pte(&new_folio->page, vma->vm_page_prot), vma);
    set_pte_at(vma->vm_mm, vmf->address, vmf->pte, entry);
}
```

**COW 是 fork 高效的关键**：fork 后父子进程共享所有页，只在第一次写入时才真正复制。这就是 `vfork` 比 `fork+exec` 快的原因——exec 直接替换页表，不需要 COW。

---

## 7. mmap — VMA 的创建

```c
// mm/mmap.c:336
unsigned long do_mmap(struct file *file, unsigned long addr,
                      unsigned long len, unsigned long prot,
                      unsigned long flags, vm_flags_t vm_flags,
                      unsigned long pgoff, unsigned long *populate,
                      struct once_per_mmap_args *args)
{
    // ① 验证: 检查 MAP_FIXED 冲突、地址对齐
    // ② 找间隙: 在 mm->mmap_tree 中找空闲 VMA 区域
    // ③ 分配 VMA: vm_area_alloc → 设置 vm_start/end/flags/ops
    // ④ 调用 mmap_region: 插入 VMA 到 mm->mmap_tree
    // ⑤ mm_populate: 如果 MAP_POPULATE → 预填充页表
}
```

`struct vm_operations_struct` (mm.h:768) 是 VMA 的操作表：

```c
struct vm_operations_struct {
    void (*open)(struct vm_area_struct *area);
    void (*close)(struct vm_area_struct *area);
    vm_fault_t (*fault)(struct vm_fault *vmf);      // ★ 缺页回调
    vm_fault_t (*huge_fault)(struct vm_fault *vmf); // THP 缺页回调
    vm_fault_t (*map_pages)(struct vm_fault *vmf, pgoff_t start, pgoff_t end);
    int (*mremap)(struct vm_area_struct *vma, struct vm_area_struct *new_vma);
    int (*mprotect)(struct vm_area_struct *vma, unsigned long start, unsigned long end, unsigned long newflags);
    // ...
};
```

---

## 8. mlock — 锁定内存

```c
// mm/mlock.c:618
static int do_mlock(unsigned long start, size_t len, vm_flags_t flags)
{
    // ① mlock_fixup: 调整 VMA flags (VM_LOCKED)
    // ② 遍历页表: 逐页调用 mlock_pte_range
    //     → 把 folio 从可回收 LRU 移到 unevictable LRU
    //     → 标记 VM_LOCKED
    // ③ 预填充: mm_populate → 防止后续的 mlock page fault
}
```

---

## 9. madvise — 内存使用建议

```c
// mm/madvise.c:2029
SYSCALL_DEFINE3(madvise, unsigned long, start, size_t, len_in, int, behavior)
```

关键 advice 参数：

| behavior | 含义 |
|----------|------|
| MADV_DONTNEED | 丢弃页内容（下次访问从 0 开始） |
| MADV_FREE | 标记页为"可回收"（有 swap 时先换出） |
| MADV_MERGEABLE | 允许 KSM 合并此区域 |
| MADV_HUGEPAGE | 建议使用 THP |
| MADV_NOHUGEPAGE | 禁止 THP |
| MADV_COLD | 标记页为"冷的"（优先被回收） |
| MADV_PAGEOUT | 立即换出这些页 |
| MADV_WILLNEED | 预读（populate page cache） |

---

## 10. 完整生命周期图

```
用户态: malloc / mmap
  → do_mmap → 创建 VMA（仅虚拟地址空间，无物理页）
  → 第一次访问 → 缺页 → handle_mm_fault → do_anonymous_page → 分配 4KB 页
  → 写入 → COW → do_wp_page → 分配新页+复制
  → fork → 子进程共享所有页(PROT_READ)
  → 子进程写入 → do_wp_page → 各自独立物理页

用户态: mmap(file)
  → 第一次读 → page cache miss → do_fault → 从磁盘读 → 安装 PTE
  → 修改 → mark dirty → writeback

用户态: munmap
  → 释放 PTE → TLB flush → refcount-- (folio可能回到buddy或file cache)
  → 释放 VMA 结构
```

---

## 下一篇文章

→ [第5篇：THP、Compaction 与 KSM — 大页、碎片整理与同页合并](./05-thp-compact-ksm.md)
