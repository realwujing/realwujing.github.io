# 第4篇：crash 工具的内存读取 — readmem、uvtop 与栈回溯

> crash 源码：`memory.c` `defs.h` | vmcore 源码：`fs/proc/vmcore.c`

系列目录：[vmcore 深度解析系列](./README.md)

---

## 1. readmem — 整个 crash 工具的核心

crash 工具所有命令（`bt`、`log`、`kmem`、`ps`）最终都调用 `readmem` 读取内核内存。这是唯一的 IO 路径。

```c
// memory.c:2307 — the workhorse
readmem(ulonglong addr, int memtype, void *buffer, long size,
        char *type, ulong error_handle)
{
    physaddr_t paddr;
    long cnt, orig_size;
    char *bufptr = (char *)buffer;

    // ① 验证 memtype
    switch (memtype) {
    case UVADDR:                           // 用户态虚拟地址
        if (!CURRENT_CONTEXT()) {          // 需要当前进程上下文
            error(INFO, "no current user process\n");
            goto readmem_error;
        }
        if (!IS_UVADDR(addr, CURRENT_CONTEXT())) {
            error(INFO, INVALID_UVADDR, addr, type);
            goto readmem_error;
        }
        break;

    case KVADDR:                           // 内核虚拟地址
        if (!IS_KVADDR(addr)) {
            error(INFO, INVALID_KVADDR, addr, type);
            goto readmem_error;
        }
        break;

    case PHYSADDR:                         // 物理地址 (直接)
        break;
    }

    // ② ★ 核心循环: 每次处理一个页面
    while (size > 0) {
        switch (memtype) {
        case UVADDR:
            // ★ 用户态: uvtop 遍历 page table
            if (!uvtop(CURRENT_CONTEXT(), addr, &paddr, 0)) {
                cnt = PAGESIZE() - PAGEOFFSET(addr);
                if (cnt > size) cnt = size;
                cnt = readswap(paddr, bufptr, cnt, addr);
                if (cnt) { bufptr += cnt; addr += cnt; size -= cnt; continue; }
            }
            goto readmem_error;

        case KVADDR:
            // ★ 内核态: VTOP (虚拟到物理转换)
            if (!kvtop(CURRENT_CONTEXT(), addr, &paddr, 0)) {
                cnt = PAGESIZE() - PAGEOFFSET(addr);
                if (cnt > size) cnt = size;
                cnt = read_vmcore_at_phys(paddr, bufptr, cnt);
                if (cnt) { bufptr += cnt; addr += cnt; size -= cnt; continue; }
            }
            goto readmem_error;

        case PHYSADDR:
            // ★ 直接物理地址 → 查找对应的 PT_LOAD segment → 读 vmcore 文件
            cnt = read_vmcore_at_phys(addr, bufptr, size);
            if (cnt) { bufptr += cnt; addr += cnt; size -= cnt; continue; }
            goto readmem_error;
        }
    }
}
```

**三种地址模式的核心转换函数**：

| 模式 | 输入 | 转换 | 输出 | 使用场景 |
|------|------|------|------|---------|
| KVADDR | 内核虚拟地址 (0xffff...) | `kvtop()` — 内核页表遍历 | PHYSADDR | `bt`, `log`, `kmem -i`, 符号读取 |
| UVADDR | 用户虚拟地址 (0x7f...) | `uvtop()` — 进程页表遍历 | PHYSADDR | `bt -p`, 用户态栈回溯, `vtop` |
| PHYSADDR | 物理地址 | 直接查找 PT_LOAD | 文件偏移 → `read()` | `rd -p`, `/proc/kcore` 读取 |

---

## 2. 物理地址到 vmcore 文件偏移的映射

```c
// 核心查找: 在 nd->load64[] 数组中查找物理地址对应的 vmcore 文件偏移
static long long physaddr_to_file_offset(physaddr_t paddr)
{
    struct vmcore_data *nd = &vmcore_data;

    for (int i = 0; i < nd->num_load; i++) {
        Elf64_Phdr *phdr = nd->load64[i];

        // ★ 物理地址在这个 segment 的范围内?
        if (paddr >= phdr->p_paddr &&
            paddr < phdr->p_paddr + phdr->p_memsz) {

            // ★ 文件偏移 = p_offset + (paddr - p_paddr)
            return phdr->p_offset + (paddr - phdr->p_paddr);
        }
    }
    return -1;  // 不在 vmcore 中 (可能是设备 MMIO 区域等)
}
```

**PT_LOAD 段的含义**：

```
Elf64_Phdr 1 (内核代码 + 数据):
  p_offset = 0x1000000 (16MB from start of vmcore file)
  p_vaddr  = 0xffff888000000000 (内核虚拟地址)
  p_paddr  = 0x10000000 (256MB physical RAM)
  p_filesz = p_memsz = 512MB (内核占用的 RAM 量)

Elf64_Phdr 2 (direct mapping 区域):
  p_offset = 0x21000000
  p_vaddr  = 0xffff888000000000 + 512MB
  p_paddr  = 0x10000000 + 512MB
  p_filesz = 512MB

Elf64_Phdr 3 (vmalloc/ioremap):
  p_offset = 0x42000000
  p_vaddr  = 0xffffc90000000000
  p_paddr  = (no physical — virtual only page tables)
  p_filesz = 128MB
```

---

## 3. kvtop — 内核虚拟地址到物理地址

```c
// crash 工具的内核页表遍历
int kvtop(struct task_context *tc, ulong kvaddr, physaddr_t *paddr, int verbose)
{
    // ① 通过 pgd_offset_k() 宏获取 PGD entry
    pgd = pgd_offset_k(kvaddr);

    // ② PGD → P4D → PUD → PMD → PTE
    p4d = p4d_offset(pgd, kvaddr);
    pud = pud_offset(p4d, kvaddr);
    pmd = pmd_offset(pud, kvaddr);
    pte = pte_offset_kernel(pmd, kvaddr);

    // ③ ★ 提取物理页帧号 + 页内偏移
    *paddr = (pte_val(pte) & PHYSICAL_PAGE_MASK) + PAGEOFFSET(kvaddr);
    return TRUE;
}
```

**crash 工具如何知道页表在哪里**：

1. VMCOREINFO 中的 `SYMBOL(swapper_pg_dir)` → 内核全局页表的虚拟地址
2. crash 读取 `swapper_pg_dir` 处的物理内存 → 得到 PGD 内容
3. 然后逐级遍历 PGD → PUD → PMD → PTE
4. 如果启用 5-level paging → 额外处理 P4D 级别
5. 如果页表项为零 (未映射) → 返回"无法读取"

---

## 4. uvtop — 用户态虚拟地址到物理地址

```c
// crash 工具的用户态页表遍历
int uvtop(struct task_context *tc, ulong uvaddr, physaddr_t *paddr, int verbose)
{
    // ① 获取当前进程的 mm_struct
    struct task_struct *task = tc->task;
    struct mm_struct *mm = task->mm;

    // ② 从 mm->pgd 获取进程的 PGD (用户态页表基址)
    ulong pgd_val = read_pointer(KVADDR, &mm->pgd);

    // ③ ★ 用户态 PGD → P4D → PUD → PMD → PTE
    // (和 kernel 的 kvtop 相同的逐级遍历，只是用的是 mm 的 pgd)
    pgd = pgd_offset(mm, uvaddr);
    // ... 逐级遍历 ...

    // ④ 提取物理页帧号
    *paddr = (pte_val(pte) & PHYSICAL_PAGE_MASK) + PAGEOFFSET(uvaddr);
    return TRUE;
}
```

**为什么 uvtop 需要 `CURRENT_CONTEXT()`**：用户态地址属于特定进程。crash 必须先通过 `set context <pid>` 选择一个进程，才能读取该进程的用户态内存。

---

## 5. 三个实例——从 readmem 到最终输出

### 5.1 `bt` (栈回溯)

```
bt → crash 获取当前上下文的 per-CPU elf_prstatus
    → regs->rip = read from PT_NOTE
    → regs->rsp = read from PT_NOTE

每帧栈:
  ① readmem(KVADDR, rsp + 8, &return_addr, sizeof(ulong))  // 读返回地址
  ② readmem(KVADDR, return_addr, &symbol, sizeof(char[128])) // 读符号名
  ③ rsp = rsp + frame_size  // 下一帧

最终输出:
  #0 [ffff888123456ab0] machine_kexec at ffffffff81123400
  #1 [ffff888123456b50] __crash_kexec at ffffffff81123500
  #2 [ffff888123456c00] panic at ffffffff81234000
```

### 5.2 `log` (dmesg)

```
log → crash 从 VMCOREINFO 读取:
    log_buf = 0xffff888000000000  (from SYMBOL(log_buf))
    log_buf_len = 2097152        (from SYMBOL(log_buf_len))
    log_first_idx = 1572864      (from SYMBOL(log_first_idx))
    log_next_idx = 524288        (from SYMBOL(log_next_idx))

  → readmem(KVADDR, log_buf, buf, log_buf_len)  // 一次性读 2MB
  → 环形缓冲区解码: buf[log_first_idx..log_buf_len] + buf[0..log_next_idx-1]
  → 输出到控制台
```

### 5.3 `kmem -i` (内存统计)

```
kmem -i → crash 读取全局:
    _totalram_pages = readmem(KVADDR, SYMBOL_VALUE(_totalram_pages), ...)
    zero_pfn = readmem(KVADDR, SYMBOL_VALUE(zero_pfn), ...)
    huge_zero_pfn = readmem(KVADDR, SYMBOL_VALUE(huge_zero_pfn), ...)

  然后遍历所有页面:
    对每个页面结构 struct page:
      readmem(KVADDR, &page[i], sizeof(struct page))
      if (page[i].flags & PG_free) free_pages++

  输出:
    TOTAL MEMORY:  64 GB
    TOTAL FREE:    48 GB
    TOTAL USED:    16 GB
```

---

## 6. 总结

| 操作 | 涉及的 readmem 调用 | 文件偏移源自 |
|------|-------------------|-----------|
| 读取符号值 | `readmem(KVADDR, SYMBOL_VALUE(name))` | kvtop → paddr → PT_LOAD 偏移 |
| 读取寄存器 | `readmem(PHYSADDR, PT_NOTE_offset)` | PT_NOTE 的 p_offset |
| 读取内核栈 | `readmem(KVADDR, rsp)` | kvtop → paddr → PT_LOAD 偏移 |
| 读取用户栈 | `readmem(UVADDR, rsp)` | uvtop → paddr → PT_LOAD 偏移 |
| 读取物理地址 | `readmem(PHYSADDR, addr)` | 直接 PT_LOAD 偏移 |

## 下一篇文章

→ [第5篇：crash 工具的 VMCOREINFO 键值数据库](./05-vmcoreinfo.md)
