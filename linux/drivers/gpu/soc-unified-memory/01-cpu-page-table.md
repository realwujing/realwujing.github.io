# 第1篇：CPU 页表深度解析 — 一个虚拟地址的物理之旅

> 源码：`arch/arm64/mm/fault.c` `mm/memory.c` | 对应头文件：`arch/arm64/include/asm/pgtable-hwdef.h` `include/linux/mm_types.h`

系列目录：[SoC 统一内存架构深度解析](./README.md)

---

## 为什么要讲 CPU 页表？

因为 HMM 遍历的就是 CPU 页表，SMMU 把 CPU 页表翻译给 GPU 用。不懂页表就看不懂统一内存。

## ARM64 四级页表拓扑

ARM64 在 48-bit VA 配置（`CONFIG_ARM64_VA_BITS=48`）下使用 4 级页表：

```
Virtual Address (48-bit)
│ [47:39] [38:30] [29:21] [20:12] [11:0]
│  L0       L1      L2      L3     offset
▼
TTBR0_EL1 → PGD → PUD → PMD → PTE → Physical Page
```

页表级数由 `ARM64_HW_PGTABLE_LEVELS` 宏动态计算，位于 `arch/arm64/include/asm/pgtable-hwdef.h:31-32`：

```c
#define ARM64_HW_PGTABLE_LEVELS(va_bits) \
	(((va_bits) - PTDESC_ORDER - 1) / PTDESC_TABLE_SHIFT)
```

其中 `PTDESC_TABLE_SHIFT` 为 `PAGE_SHIFT - PTDESC_ORDER`（`page.h:13` → `pgtable-hwdef.h:13`），对于 4K 页表为 `12 - 3 = 9`。代入 48-bit VA：

- `levels = (48 - 3 - 1) / 9 = 44 / 9 ≈ 4.88` → `levels = 4`（天花板除法）

对于 39-bit VA 则是 3 级页表，所以 ARM64 的页表级数是运行时确定的。

## 各级页表 Shift 定义

各级页表映射的地址宽度定义在 `arch/arm64/include/asm/pgtable-hwdef.h`：

| 级别 | 宏 | 行号 | 条件 | 含义 |
|------|-----|------|------|------|
| PGD | `PGDIR_SHIFT` | **82** | `4 - CONFIG_PGTABLE_LEVELS` | 顶级页表入口覆盖的VA位数 |
| PUD | `PUD_SHIFT` | **65** | `CONFIG_PGTABLE_LEVELS > 3` | Level-1 入口覆盖的VA位数 |
| PMD | `PMD_SHIFT` | **55** | `CONFIG_PGTABLE_LEVELS > 2` | Level-2 入口覆盖的VA位数 |
| PAGE | `PAGE_SHIFT` | **13** (`include/vdso/page.h`) | 通用 | 页内偏移位数 |

```c
// pgtable-hwdef.h:82
#define PGDIR_SHIFT     ARM64_HW_PGTABLE_LEVEL_SHIFT(4 - CONFIG_PGTABLE_LEVELS)
```

其中 `ARM64_HW_PGTABLE_LEVEL_SHIFT(n) = PTDESC_TABLE_SHIFT * (4 - n) + PTDESC_ORDER`（`pgtable-hwdef.h:47`）。

对于 48-bit / 4K 页表：
- `PGDIR_SHIFT` = `9 * (4 - 2) + 3` = `9 * 2 + 3` = **21** — PGD 每个 entry 映射 2MB
- `PUD_SHIFT` = `9 * (4 - 1) + 3` = **30** — PUD 每个 entry 映射 1GB
- `PMD_SHIFT` = `9 * (4 - 0) + 3` = **39** — PMD 每个 entry 映射 512GB

> Note: 虽然 PMD_SHIFT=39 理论上可以映射 512GB，但在常规 Walk 中，PMD 入口指向的是下一级 PTE 表，而非直接映射。Block mapping（`PMD_TYPE_SECT`）选项可以将 PMD 入口配置为直接映射 2MB 大页。

## PTE 硬件位定义

每个 PTE 是一个 64-bit 的硬件描述符，位定义在 `arch/arm64/include/asm/pgtable-hwdef.h`：

| 位域 | 宏 | 行号 | 说明 |
|------|-----|------|------|
| bit[0] | `PTE_VALID` | **164** | 该 PTE 有效，MMU 可以将其用于翻译 |
| bits[1:0]=0b11 | `PTE_TYPE_PAGE` | **166** | 这是一个 Page 类型的描述符（leaf entry） |
| bit[6] | `PTE_USER` | **167** | AP[1]，EL0 可以访问 |
| bit[7] | `PTE_RDONLY` | **168** | AP[2]，只读 |
| bits[9:8] | `PTE_SHARED` | **169** | SH[1:0]，Inner Shareable |
| bit[10] | `PTE_AF` | **170** | **Access Flag**：MMU 硬件自动设置，OS 定期清除以跟踪页面访问 |
| bit[11] | `PTE_NG` | **171** | nG（non-Global），用于 ASID 区分 |
| bit[50] | `PTE_GP` | **172** | BTI Guarded Page |
| bit[51] | `PTE_DBM` | **173** | **Dirty Bit Management**：硬件脏页跟踪（FEAT_HA/HD） |
| bit[52] | `PTE_CONT` | **174** | Contiguous range，连续页合并 TLB 入口 |
| bit[53] | `PTE_PXN` | **175** | Privileged Execute Never |
| bit[54] | `PTE_UXN` | **176** | User Execute Never |

### 关键硬件机制详解

**Access Flag (`PTE_AF`)**：当 MMU 遍历页表进行地址翻译时，会自动将对应表项的 AF 位设置为 1。OS 定期扫描并清除 AF 位，下一次访问又会触发 AF 硬件置位。OS 通过检查 AF 状态判断页面是否被最近访问过——这是 LRU 页面回收的基础。

**Dirty Bit Management (`PTE_DBM`)**：传统的 ARM 架构没有硬件脏位，OS 需要用 `PTE_RDONLY` 模拟写保护来跟踪脏页（写时触发行保护 fault）。`FEAT_HA` + `FEAT_HD` 硬件扩展引入 DBM，让 MMU 在首次写入时自动设置 DBM 位（硬件等价于 PTE_WRITE），消除了额外的页错误中断。对应的 fault_info 表中有 access flag fault（`fault.c:917-920`）和 permission fault（`fault.c:921-924`）不同路径。

**Block Mapping (`PMD_TYPE_SECT`)**：PMD 级别的 section/block 映射（`pgtable-hwdef.h:138`），可以直接将 2MB 的 PMD entry 指向物理内存，跳过 PTE 级。HMM 遍历时通过 `pmd_trans_huge()` 检测这一情况。

## 软件 PTE 位定义

软件层使用硬件未定义的位来存储额外信息，定义在 `arch/arm64/include/asm/pgtable-prot.h`：

| 宏 | 硬件位 | 行号 | 用途 |
|----|--------|------|------|
| `PTE_WRITE` | `=PTE_DBM` (bit 51) | **16** | 软件可写标志，直接复用硬件 DBM 位 |
| `PTE_DIRTY` | bit 55 | **18** | 软件脏位（当硬件 HD 不可用时） |
| `PTE_SPECIAL` | bit 56 | **19** | Special 映射（vDSO、gate area 等） |
| `PTE_PRESENT_INVALID` | `=PTE_NG` (bit 11) | **26** | PTE 有效但不可被硬件访问，软件解释内容 |

其中 `PTE_PRESENT_INVALID` 是一个巧妙的设计：硬件侧 `PTE_VALID=0`（bit[0] 清零），MMU 遇到此 PTE 会触发 fault；但软件侧 `pte_present()` 返回 true，OS 可以直接读取 PTE 内容。这使得可以通过设置 PTE_NG 来标记一个 "对软件 present 但对硬件 invalid" 的状态。

```c
// PTE_WRITE 复用硬件 DBM 位，意味着写权限和脏位是同一个物理位
#define PTE_WRITE   (PTE_DBM)         /* same as DBM (51) */

// PTE_DIRTY 使用 sw bit 55，当硬件不支持 HD 时用于软件脏页跟踪
#define PTE_DIRTY   (_AT(pteval_t, 1) << 55)

// PTE_PRESENT_INVALID = PTE_NG 且 V=0，硬件无法访问但软件读得到
#define PTE_PRESENT_INVALID (PTE_NG)  /* only when !PTE_VALID */
```

## 关键数据结构

### PGD 指针 — `mm_struct.pgd`

```c
// include/linux/mm_types.h:1199
pgd_t * pgd;
```

每个 `mm_struct` 的 `pgd` 字段指向进程页表的 PGD（Page Global Directory）基地址。在上下文切换时写入 TTBR0_EL1（用户态）或 TTBR1_EL1（内核态）。

### VMA — `vm_area_struct`

```c
// include/linux/mm_types.h:932
struct vm_area_struct {
    unsigned long vm_start;     // line 938
    unsigned long vm_end;       // line 939
    struct mm_struct *vm_mm;    // line 948
    pgprot_t vm_page_prot;      // line 949 — 访问权限
    vm_flags_t vm_flags;        // line 958 — VM_READ/WRITE/EXEC/SHARED 等
};
```

VMA 描述了进程虚拟地址空间的一个连续区间及其属性。HMM 的 `test_walk` 回调（`hmm.c:597`）会检查 `vm_flags` 中是否设置了 `VM_IO` 或 `VM_PFNMAP` 来跳过无法处理的 VMA。

## 缺页中断调用链

### fault_info 调度表

ARM64 通过 ESR_ELx 寄存器中的 Fault Status Code (FSC) 字段分发到对应的处理函数。调度表定义在 `arch/arm64/mm/fault.c:908-972`：

```c
// fault.c:908
static const struct fault_info fault_info[] = {
    { do_bad,               SIGKILL, SI_KERNEL, "ttbr address size fault"   },  // 0
    ...
    { do_translation_fault, SIGSEGV, SEGV_MAPERR, "level 0 translation fault" }, // 4
    { do_translation_fault, SIGSEGV, SEGV_MAPERR, "level 1 translation fault" }, // 5
    { do_translation_fault, SIGSEGV, SEGV_MAPERR, "level 2 translation fault" }, // 6
    { do_translation_fault, SIGSEGV, SEGV_MAPERR, "level 3 translation fault" }, // 7
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 0 access flag fault"  }, // 8
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 1 access flag fault"  }, // 9
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 2 access flag fault"  }, // 10
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 3 access flag fault"  }, // 11
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 0 permission fault"   }, // 12
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 1 permission fault"   }, // 13
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 2 permission fault"   }, // 14
    { do_page_fault,        SIGSEGV, SEGV_ACCERR, "level 3 permission fault"   }, // 15
    ...
};
```

关键路径的 FSC 编码与处理：
- **FSC 0x04-0x07** → translation fault → `do_translation_fault` → `do_page_fault`（无 PTE 存在）
- **FSC 0x08-0x0B** → access flag fault → `do_page_fault`（PTE 存在但 AF=0，MMU 硬件未置位因为 OS 清过）
- **FSC 0x0C-0x0F** → permission fault → `do_page_fault`（权限不匹配：写只读页、执行 NX 页等）

### do_translation_fault vs do_page_fault — 本质区别

这两个 handler 是 ARM64 缺页处理中最容易混淆的概念：

```c
// fault.c:833-844 — do_translation_fault 是一个轻量包装
static int __kprobes do_translation_fault(unsigned long far,
                                          unsigned long esr,
                                          struct pt_regs *regs)
{
    unsigned long addr = untagged_addr(far);

    if (is_ttbr0_addr(addr))           // ← 用户态地址？
        return do_page_fault(far, esr, regs);  // 转给 do_page_fault

    do_bad_area(far, esr, regs);       // 内核地址+translation fault→kill
    return 0;
}
```

`do_translation_fault` **只是一个门卫**，它不是在处理 translation fault，而是在判断 **谁发的**：
- 用户态地址（TTBR0，`is_ttbr0_addr`）→ 转给 `do_page_fault`，正常缺页
- 内核态地址（TTBR1）→ 直接 `do_bad_area`，kill 进程

而 `do_page_fault`（`fault.c:596`）才是真正干活的那个——分配 `vm_flags`、查 VMA、调用 `handle_mm_fault`。

```c
// fault.c:654-662 — kernel uaccess 检查
if (is_ttbr0_addr(addr) && is_el1_permission_fault(addr, esr, regs)) {
    if (is_el1_instruction_abort(esr))
        die_kernel_fault("execution of user memory", addr, esr, regs);
    if (!insn_may_access_user(regs->pc, esr))
        die_kernel_fault("access to user memory outside uaccess routines",
                         addr, esr, regs);
}
```

为什么需要这种区分？ARM64 的 TTBR0_EL1 指向用户态页表，TTBR1_EL1 指向内核态页表。如果内核代码访问用户态地址时没走 `copy_from_user` 等安全路径，就需要 `die_kernel_fault` 而不是静默处理。

**signal code 的区别**也值得注意（`fault.c:908-924`）：
- translation fault → `SEGV_MAPERR`：**页根本不存在**（`fault.c:913-916`）
- access flag / permission fault → `SEGV_ACCERR`：**页存在但不允许访问**（`fault.c:917-924`）

这两个信号码和 VMA 的存在性检测形成闭环（`fault.c:687-735`）：
```c
// fault.c:687 — 查 VMA 权限
if (!(vma->vm_flags & vm_flags)) {
    si_code = SEGV_ACCERR;      // VMA 存在但权限不匹配
    ...
}
// fault.c:728 — VMA 不存在
si_code = SEGV_MAPERR;          // 根本没有 VMA
```

### 调用链详解

```
异常入口（硬件）
  │
  ▼
do_mem_abort(far, esr, regs)                      // fault.c:975
  │ esr_to_fault_info(esr) → fault_info[index]     // fault.c:58-60
  │ inf->fn(far, esr, regs)                        // 分发到具体 handler
  │
  ├── do_translation_fault (FSC=4-7)               // fault.c:833
  │     └── do_page_fault                          // fault.c:596
  │
  └── do_page_fault (FSC=8-15)                     // fault.c:596
        │
        │ 1. 根据 ESR 解析 fault 类型：
        │    - is_el0_instruction_abort → vm_flags=VM_EXEC
        │    - is_write_abort           → vm_flags=VM_WRITE
        │    - else                     → vm_flags=VM_READ
        │ 2. user_mode? → FAULT_FLAG_USER
        │ 3. lock_vma_under_rcu() or lock_mm_and_find_vma()
        │
        ├── handle_mm_fault(vma, addr, flags, regs)   // memory.c:6699
        │     │
        │     ├── sanitize_fault_flags()
        │     ├── arch_vma_access_permitted() 检查
        │     ├── hugetlb? → hugetlb_fault()
        │     └── else → __handle_mm_fault(vma, addr, flags)  // memory.c:6465
        │           │
        │           ├── pgd_offset(mm, addr)  — 从 mm->pgd 计算 PGD 入口
        │           ├── p4d_alloc(mm, pgd, addr)  — 分配 P4D（如果折叠则为 no-op）
        │           ├── pud_alloc(mm, p4d, addr)  — 分配 PUD
        │           ├── 检查 PUD 大页 (create_huge_pud/pud_trans_huge)
        │           ├── pmd_alloc(mm, pud, addr)  — 分配 PMD
        │           ├── 检查 PMD 大页 (create_huge_pmd/pmd_trans_huge)
        │           └── handle_pte_fault(vmf)          // memory.c:6383
        │                 │
        │                 ├── pte_none? → do_pte_missing(vmf)     // 缺页分配
        │                 ├── !pte_present? → do_swap_page(vmf)   // Swap in
        │                 ├── pte_protnone? → do_numa_page(vmf)   // NUMA 迁移
        │                 └── FAULT_FLAG_WRITE && !pte_write?
        │                       → do_wp_page(vmf)                  // 写时复制
        │
        └── 返回后: 检查 VM_FAULT_RETRY，处理信号
```

## 完整页表遍历示意

```
                    ┌──────────────────────────────────────────────────────┐
                    │               Virtual Address (48-bit)                │
                    │   [47:39]  [38:30]  [29:21]  [20:12]    [11:0]      │
                    │    L0 idx   L1 idx   L2 idx   L3 idx    offset       │
                    │    9 bits   9 bits   9 bits   9 bits    12 bits      │
                    └──────┬───────┬───────┬───────┬───────────┬───────────┘
                           │       │       │       │           │
    ┌──────────────────────┘       │       │       │           │
    ▼                              │       │       │           │
┌─────────┐  TTBR0_EL1            │       │       │           │
│ mm->pgd │──────────────────────┐ │       │       │           │
│ (line   │    PGD[L0] ──► ┌─────▼─┐     │       │           │
│  1199)  │                │  PUD  │     │       │           │
└─────────┘                │table  │     │       │           │
                           │       │     │       │           │
                           │PUD    │     │       │           │
                           │[L1]───┼──┐  │       │           │
                           └───────┘ │  │       │           │
                                     │  │       │           │
                                     ▼  │       │           │
                                 ┌───────┐     │           │
                                 │  PMD  │     │           │
                                 │ table │     │           │
                                 │       │     │           │
                                 │PMD    │     │           │
                                 │[L2]───┼──┐  │           │
                                 └───────┘ │  │           │
                                           │  │           │
                                           ▼  │           │
                                       ┌───────┐         │
                                       │  PTE  │         │
                                       │ table │         │
                                       │       │         │
                                       │PTE    │         │
                                       │[L3]───┼──┐      │
                                       └───────┘ │      │
                                                 │      │
                                                 ▼      ▼
                                            ┌──────────────┐
                                            │  Physical    │
                                            │  Page Frame  │
                                            │  (4KB)       │
                                            └──────────────┘
```

每条 PGD/PUD/PMD/PTE entry 占用 64 bits。低 `PTDESC_TABLE_SHIFT`（9 或与页大小相关）位作为 flags，高位存放下一级页表的物理地址。

**块映射快捷路径**：当 PMD 入口配置为 `PMD_TYPE_SECT` 时，跳过 PTE 级，直接映射 2MB 大页。

## 总结

CPU 页表的核心是一个树形数据结构，根在 `mm->pgd`（`mm_types.h:1199`），通过 `ARM64_HW_PGTABLE_LEVELS`（`pgtable-hwdef.h:31-32`）确定级数。页错误通过 `fault_info[]` 表（`fault.c:908`）分派，最后到达 `handle_pte_fault`（`memory.c:6383`）进行 4 路分支处理。

理解这套机制后，HMM 就是在此之上建立一个只读的页表遍历器——不修改 PTE，只读取 PFN 并把结果填回 `hmm_range.pfns[]` 数组。

## 下一篇文章

→ [第2篇：HMM 如何遍历 CPU 页表 — hmm_range_fault 内部全解](./02-hmm-walk.md)
