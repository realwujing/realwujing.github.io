# 第一篇：panic → kexec → vmcore 生成

> 内核源码：`kernel/panic.c` `kernel/crash_core.c` `kernel/kexec_internal.h` `arch/x86/kernel/crash.c` `arch/x86/kernel/machine_kexec_64.c` `kernel/vmcore_info.c`

系列目录：[vmcore 深度解析系列](./README.md)

---

## 一、整体流程概览

当内核触发 `panic()` 后，一条单向、不可逆的路径被激活，目标是：停止一切，保存现场，跳到 capture kernel。整个过程简化为：

```
panic()
  └─ __crash_kexec()
       ├─ kexec_trylock()           ← 原子 cmpxchg 抢锁，只允许一个 CPU 执行
       ├─ crash_save_vmcoreinfo()   ← 保存内核常量到 ELF note 页
       ├─ native_machine_crash_shutdown()
       │    ├─ nmi_shootdown_cpus(kdump_nmi_callback)
       │    │    └─ kdump_nmi_callback(cpu, regs)  ← 每个非崩溃 CPU 上执行
       │    │         ├─ crash_save_cpu(regs, cpu)  ← 保存各 CPU 寄存器到 ELF notes
       │    │         └─ disable_local_APIC()       ← 防止 IOAPIC 死锁
       │    ├─ lapic_shutdown()
       │    ├─ ioapic_zap_locks() / clear_IO_APIC()
       │    └─ crash_save_cpu(regs, smp_processor_id()) ← 保存崩溃 CPU 自身寄存器
       └─ machine_kexec(kexec_crash_image)
            ├─ load_segments()      ← 重载段寄存器
            ├─ relocate_kernel()    ← 设置 identity-mapped 页表
            └─ → 跳入 capture kernel 入口
```

本文按这个顺序，逐段拆解源码。

---

## 二、`vpanic()` — 内核恐慌的入口

`kernel/panic.c:576-634` 是 `vpanic()` 的主体。关键步骤：

**2.1 抢 panic_cpu**

```c
// kernel/panic.c:627-630
if (panic_try_start()) {
    /* go ahead */
} else if (panic_on_other_cpu())
    panic_smp_self_stop();
```

`panic_try_start()` (`kernel/panic.c:451-464`) 使用原子 cmpxchg 争抢一个全局变量：

```c
// kernel/panic.c:451-464
bool panic_try_start(void)
{
    int old_cpu, this_cpu;

    /*
     * Only one CPU is allowed to execute the crash_kexec() code as with
     * panic().  Otherwise parallel calls of panic() and crash_kexec()
     * may stop each other.
     */
    old_cpu = PANIC_CPU_INVALID;
    this_cpu = raw_smp_processor_id();

    return atomic_try_cmpxchg(&panic_cpu, &old_cpu, this_cpu);
}
```

只有第一个把 `panic_cpu` 从 `PANIC_CPU_INVALID` 改为自身 CPU ID 的 CPU 会继续执行；其余 CPU 全部调用 `panic_smp_self_stop()` 自旋等待。

**2.2 打印 panic 信息后调用 __crash_kexec**

`kernel/panic.c:661-696`，panic 信息输出后，立刻判断是否要触发 kdump：

```c
// kernel/panic.c:661-672
/*
 * If we have crashed and we have a crash kernel loaded let it handle
 * everything else.
 * If we want to run this after calling panic_notifiers, pass
 * the "crash_kexec_post_notifiers" option to the kernel.
 *
 * Bypass the panic_cpu check and call __crash_kexec directly.
 */
if (!_crash_kexec_post_notifiers)
    __crash_kexec(NULL);

panic_other_cpus_shutdown(_crash_kexec_post_notifiers);
```

默认情况下（`crash_kexec_post_notifiers=0`），在调用 panic notifiers 之前就立刻执行 `__crash_kexec(NULL)`。这是因为 notifiers 可能让已经崩溃的内核更不稳定。

如果内核启动时传了 `crash_kexec_post_notifiers`（`kernel/panic.c:63`），则先跑完 notifiers 和 kmsg_dump，再在 `kernel/panic.c:695-696` 调用：

```c
// kernel/panic.c:693-696
if (_crash_kexec_post_notifiers)
    __crash_kexec(NULL);
```

---

## 三、`__crash_kexec()` — kdump 的中央调度器

`kernel/crash_core.c:127-149` 是核心函数。它完成三项工作：

```c
// kernel/crash_core.c:127-149
void __noclone __crash_kexec(struct pt_regs *regs)
{
    /* Take the kexec_lock here to prevent sys_kexec_load
     * running on one cpu from replacing the crash kernel
     * we are using after a panic on a different cpu.
     *
     * If the crash kernel was not located in a fixed area
     * of memory the xchg(&kexec_crash_image) would be
     * sufficient.  But since I reuse the memory...
     */
    if (kexec_trylock()) {
        if (kexec_crash_image) {
            struct pt_regs fixed_regs;

            crash_setup_regs(&fixed_regs, regs);
            crash_save_vmcoreinfo();
            machine_crash_shutdown(&fixed_regs);
            crash_cma_clear_pending_dma();
            machine_kexec(kexec_crash_image);
        }
        kexec_unlock();
    }
}
STACK_FRAME_NON_STANDARD(__crash_kexec);
```

逐行解读：

### 3.1 `kexec_trylock()` — 允许多 CPU 同时 panic 的互斥锁

`kernel/kexec_internal.h:23-28`：

```c
// kernel/kexec_internal.h:23-28
extern atomic_t __kexec_lock;
static inline bool kexec_trylock(void)
{
    int old = 0;
    return atomic_try_cmpxchg_acquire(&__kexec_lock, &old, 1);
}
```

为什么不用 spinlock？注释写得很清楚：`__crash_kexec()` 可能在 NMI 上下文中被调用（详见 `nmi_panic()`，`kernel/panic.c:510-517`）。NMI 中 spinlock 可能已经死锁，因此用最简单的原子变量 + cmpxchg。

`atomic_try_cmpxchg_acquire` 的语义：如果 `__kexec_lock` 的值是 `old`（即 0），就把它设为 1，返回 true；否则把当前值写回 `old`，返回 false。这保证了同一时刻只有一个 CPU 走完 crash_kexec 流程。

解锁是简单的 `atomic_set_release(&__kexec_lock, 0)`，位于 `kernel/kexec_internal.h:29-32`。

### 3.2 `crash_save_vmcoreinfo()` — 保存内核常量

`kernel/vmcore_info.c:84-95`：

```c
// kernel/vmcore_info.c:84-95
void crash_save_vmcoreinfo(void)
{
    if (!vmcoreinfo_note)
        return;

    /* Use the safe copy to generate vmcoreinfo note if have */
    if (vmcoreinfo_data_safecopy)
        vmcoreinfo_data = vmcoreinfo_data_safecopy;

    vmcoreinfo_append_str("CRASHTIME=%lld\n", ktime_get_real_seconds());
    update_vmcoreinfo_note();
}
```

`vmcoreinfo_append_str()` (`kernel/vmcore_info.c:97-115`) 在运行时阶段初始化时已经填入了大量内容（详见第六节），崩溃时刻只追加一行 CRASHTIME：

```c
// kernel/vmcore_info.c:97-115
void vmcoreinfo_append_str(const char *fmt, ...)
{
    va_list args;
    char buf[0x50];
    size_t r;

    va_start(args, fmt);
    r = vscnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    r = min(r, (size_t)VMCOREINFO_BYTES - vmcoreinfo_size);

    memcpy(&vmcoreinfo_data[vmcoreinfo_size], buf, r);

    vmcoreinfo_size += r;

    WARN_ONCE(vmcoreinfo_size == VMCOREINFO_BYTES,
              "vmcoreinfo data exceeds allocated size, truncating");
}
```

每次追加限制总大小不超过 `VMCOREINFO_BYTES`（4096），超过时打印 WARNING 并截断。

### 3.3 `machine_crash_shutdown()` → `native_machine_crash_shutdown()`

对于 x86_64，这个函数定义在 `arch/x86/kernel/crash.c:100-145`：

```c
// arch/x86/kernel/crash.c:100-145
void native_machine_crash_shutdown(struct pt_regs *regs)
{
    /* This function is only called after the system
     * has panicked or is otherwise in a critical state.
     * The minimum amount of code to allow a kexec'd kernel
     * to run successfully needs to happen here.
     *
     * In practice this means shooting down the other cpus in
     * an SMP system.
     */
    /* The kernel is broken so disable interrupts */
    local_irq_disable();

    crash_smp_send_stop();

    x86_virt_emergency_disable_virtualization_cpu();

    /*
     * Disable Intel PT to stop its logging
     */
    cpu_emergency_stop_pt();

#ifdef CONFIG_X86_IO_APIC
    /* Prevent crash_kexec() from deadlocking on ioapic_lock. */
    ioapic_zap_locks();
    clear_IO_APIC();
#endif
    lapic_shutdown();
    restore_boot_irq_mode();
#ifdef CONFIG_HPET_TIMER
    hpet_disable();
#endif

    /*
     * Non-crash kexec calls enc_kexec_begin() while scheduling is still
     * active. This allows the callback to wait until all in-flight
     * shared<->private conversions are complete. In a crash scenario,
     * enc_kexec_begin() gets called after all but one CPU have been shut
     * down and interrupts have been disabled. This allows the callback to
     * detect a race with the conversion and report it.
     */
    x86_platform.guest.enc_kexec_begin();
    x86_platform.guest.enc_kexec_finish();

    crash_save_cpu(regs, smp_processor_id());
}
```

**步骤拆解：**

1. **`local_irq_disable()`** — 内核已经崩了，关中断，不接受任何打扰。

2. **`crash_smp_send_stop()`** (`arch/x86/kernel/crash.c:78-91`) — 调用 `kdump_nmi_shootdown_cpus()` 通过 NMI 停掉所有其他 CPU：

```c
// arch/x86/kernel/crash.c:70-75
void kdump_nmi_shootdown_cpus(void)
{
    nmi_shootdown_cpus(kdump_nmi_callback);

    disable_local_APIC();
}
```

3. **`ioapic_zap_locks()` / `clear_IO_APIC()`** — 注释非常直白：「Prevent crash_kexec() from deadlocking on ioapic_lock」。如果不清理 IOAPIC 锁，kexec 中可能会死锁在 IOAPIC 中断上。

4. **`lapic_shutdown()`** — 禁用 local APIC，防止本地中断干扰 kexec 搬运过程。

5. **`restore_boot_irq_mode()`** — 将中断模式恢复到启动时的 8259 PIC 模式，因为 capture kernel 也是从 8259 模式开始工作的。

6. **`crash_save_cpu(regs, smp_processor_id())`** — 最后保存崩溃 CPU 自身的寄存器到 ELF notes。

### 3.4 `kdump_nmi_callback()` — 每个非崩溃 CPU 上执行的 NMI 回调

`arch/x86/kernel/crash.c:56-68`：

```c
// arch/x86/kernel/crash.c:56-68
static void kdump_nmi_callback(int cpu, struct pt_regs *regs)
{
    crash_save_cpu(regs, cpu);

    /*
     * Disable Intel PT to stop its logging
     */
    cpu_emergency_stop_pt();

    kdump_sev_callback();

    disable_local_APIC();
}
```

NMI 是不可屏蔽的，无论目标 CPU 正在执行什么（持自旋锁、关中断、甚至处于 MWAIT），NMI 都能打断它。这就是为什么 NMI 是唯一安全的方式去"停掉"其他 CPU。

三个动作：
- `crash_save_cpu(regs, cpu)` — 把该 CPU 的寄存器状态存到 ELF notes，这样 crash 工具就能看到每个 CPU 在崩溃瞬间的调用栈。
- `cpu_emergency_stop_pt()` — 关闭 Intel Processor Trace 日志。
- `disable_local_APIC()` — 禁用本地的 APIC，防止后续 kexec 执行中被中断打断。

### 3.5 `machine_kexec()` — 跳入 capture kernel

`arch/x86/kernel/machine_kexec_64.c:400-481` 是整个流程的终点：

```c
// arch/x86/kernel/machine_kexec_64.c:396-399
/*
 * Do not allocate memory (or fail in any way) in machine_kexec().
 * We are past the point of no return, committed to rebooting now.
 */

// arch/x86/kernel/machine_kexec_64.c:400-481
void __nocfi machine_kexec(struct kimage *image)
{
    unsigned long reloc_start = (unsigned long)__relocate_kernel_start;
    relocate_kernel_fn *relocate_kernel_ptr;
    unsigned int relocate_kernel_flags;
    int save_ftrace_enabled;
    void *control_page;

#ifdef CONFIG_KEXEC_JUMP
    if (image->preserve_context)
        save_processor_state();
#endif

    save_ftrace_enabled = __ftrace_enabled_save();

    /* Interrupts aren't acceptable while we reboot */
    local_irq_disable();
    hw_breakpoint_disable();
    cet_disable();

    if (image->preserve_context) {
#ifdef CONFIG_X86_IO_APIC
        /*
         * We need to put APICs in legacy mode so that we can
         * get timer interrupts in second kernel. kexec/kdump
         * paths already have calls to restore_boot_irq_mode()
         * in one form or other. kexec jump path also need one.
         */
        clear_IO_APIC();
        restore_boot_irq_mode();
#endif
    }

    control_page = page_address(image->control_code_page);

    /*
     * Allow for the possibility that relocate_kernel might not be at
     * the very start of the page.
     */
    relocate_kernel_ptr = control_page +
        (unsigned long)relocate_kernel - reloc_start;

    relocate_kernel_flags = 0;
    if (image->preserve_context)
        relocate_kernel_flags |= RELOC_KERNEL_PRESERVE_CONTEXT;

    /*
     * This must be done before load_segments() since it resets
     * GS to 0 and percpu data needs the correct GS to work.
     */
    if (this_cpu_read(cache_state_incoherent))
        relocate_kernel_flags |= RELOC_KERNEL_CACHE_INCOHERENT;

    /*
     * The segment registers are funny things, they have both a
     * visible and an invisible part.  Whenever the visible part is
     * set to a specific selector, the invisible part is loaded
     * with from a table in memory.  At no other time is the
     * descriptor table in memory accessed.
     *
     * Take advantage of this here by force loading the segments,
     * before the GDT is zapped with an invalid value.
     *
     * load_segments() resets GS to 0.  Don't make any function call
     * after here since call depth tracking uses percpu variables to
     * operate (relocate_kernel() is explicitly ignored by call depth
     * tracking).
     */
    load_segments();

    /* now call it */
    image->start = relocate_kernel_ptr((unsigned long)image->head,
                       virt_to_phys(control_page),
                       image->start,
                       relocate_kernel_flags);

#ifdef CONFIG_KEXEC_JUMP
    if (image->preserve_context)
        restore_processor_state();
#endif

    __ftrace_enabled_restore(save_ftrace_enabled);
}
ANNOTATE_NOCFI_SYM(machine_kexec);
```

**关键细节：**

1. **注释「Do not allocate memory (or fail in any way)」** — 此函数绝不能失败。已过"不归点"（point of no return），必须完成跳转，否则系统卡死。

2. **`load_segments()`** (`arch/x86/kernel/machine_kexec_64.c:313-322`) — 在所有段寄存器中强制加载 `__KERNEL_DS`。注释解释得很清楚：GDT 即将被 relocate_kernel 破坏（zap with an invalid value），但段寄存器的不可见部分（invisible part / cached descriptor）只在 `mov %sel, %seg` 时从 GDT 加载。因此必须在此刻提前加载好，之后 GDT 覆盖了也无所谓。

3. **`relocate_kernel()`** 是一段位置无关的汇编代码（位于 `arch/x86/kernel/relocate_kernel_64.S`），它负责：
   - 设置 identity-mapped 页表（让物理地址等于虚拟地址）
   - 清除 CR4.PGE（防止 stale TLB 条目导致地址映射错误）
   - 设置新的 CR3
   - 跳转到 capture kernel 入口点

4. **`ANNOTATE_NOCFI_SYM(machine_kexec)`** — 禁用 CFI (Control Flow Integrity) 检查。因为 `relocate_kernel` 是直接函数指针调用，且它永远不会"返回"到正常代码路径。

---

## 四、`crash_setup_regs()` — 寄存器修正

`kernel/crash_core.c:141` 调用了 `crash_setup_regs(&fixed_regs, regs)`。如果 panic 是由 `die()` / oops 触发的，`regs` 指针可能为 NULL（来自 `panic("...")` 直接调用），也可能指向有 bug 的寄存器快照。`crash_setup_regs()` 的职责是：如果 regs 不可用，则通过 `__builtin_frame_address(0)` 和当前栈帧构造一个"当前"的寄存器上下文。

---

## 五、`nmi_panic()` — NMI 上下文的 panic

`kernel/panic.c:510-517`：

```c
// kernel/panic.c:510-517
void nmi_panic(struct pt_regs *regs, const char *msg)
{
    if (panic_try_start())
        panic("%s", msg);
    else if (panic_on_other_cpu())
        nmi_panic_self_stop(regs);
}
```

当 NMI watchdog 或 MCE (Machine Check Exception) 检测到致命错误时调用。它在 NMI 上下文中：关中断、不可屏蔽、不能再拿锁。因此 `kexec_trylock()` 被设计为 NMI-safe 的原子 cmpxchg。

---

## 六、VMCOREINFO — 内核常量的"密码本"

capture kernel 和 crash 工具需要对生产内核的数据结构有精确认知，但这些信息可能在崩溃前后不一致（ASLR 偏移、编译选项等）。解决方案：在生产内核的正常运行阶段，把关键常量写入一个物理页，该页在 reserved crash 区域中，能在 kexec 之后存活。

### 6.1 初始化阶段 — 填充常量

`kernel/vmcore_info.c:140-157` 在系统启动时（`subsys_initcall`，`kernel/vmcore_info.c:253`）分配页面并填充：

```c
// kernel/vmcore_info.c:140-157
static int __init crash_save_vmcoreinfo_init(void)
{
    int order;
    order = get_order(VMCOREINFO_BYTES);
    vmcoreinfo_data = (unsigned char *)__get_free_pages(
        GFP_KERNEL | __GFP_ZERO, order);
    if (!vmcoreinfo_data) {
        pr_warn("Memory allocation for vmcoreinfo_data failed\n");
        return -ENOMEM;
    }

    vmcoreinfo_note = alloc_pages_exact(VMCOREINFO_NOTE_SIZE,
                        GFP_KERNEL | __GFP_ZERO);
    if (!vmcoreinfo_note) {
        free_pages((unsigned long)vmcoreinfo_data, order);
        vmcoreinfo_data = NULL;
        pr_warn("Memory allocation for vmcoreinfo_note failed\n");
        return -ENOMEM;
    }

    VMCOREINFO_OSRELEASE(init_uts_ns.name.release);
    // ... 后续填充大量内容
```

`VMCOREINFO_BYTES` 定义为 4096（一个页面），`get_order(4096)` = 0（只分配一页）。`VMCOREINFO_NOTE_SIZE` 也对应一个页面。注意 `alloc_pages_exact()` 返回的是精确大小分配的页面指针，这个页面将永久保留，直到系统崩溃。

### 6.2 运行时填充的内容

`kernel/vmcore_info.c:159-248` 通过一系列宏填入了 `vmcoreinfo_data[]`。以下是关键常量的完整列表：

**结构化宏定义：**

```c
// kernel/vmcore_info.c:159-170
VMCOREINFO_OSRELEASE(init_uts_ns.name.release);  // 内核版本字符串
VMCOREINFO_BUILD_ID();                            // 编译 ID (链接器的 build-id)
VMCOREINFO_PAGESIZE(PAGE_SIZE);                   // 页面大小

VMCOREINFO_SYMBOL(init_uts_ns);                   // init_uts_ns 符号地址
VMCOREINFO_OFFSET(uts_namespace, name);           // uts_namespace.name 偏移
VMCOREINFO_SYMBOL(node_online_map);               // NUMA node 在线位图地址
#ifdef CONFIG_MMU
VMCOREINFO_SYMBOL_ARRAY(swapper_pg_dir);          // 内核页表根目录地址
#endif
VMCOREINFO_SYMBOL(_stext);                        // 内核正文段起始
```

**VMALLOC 和内存布局：**

```c
// kernel/vmcore_info.c:170
vmcoreinfo_append_str("NUMBER(VMALLOC_START)=0x%lx\n",
    (unsigned long) VMALLOC_START);
```

**NUMA/SPARSEMEM 信息：**

```c
// kernel/vmcore_info.c:172-186
#ifndef CONFIG_NUMA
VMCOREINFO_SYMBOL(mem_map);                       // FLATMEM 的 mem_map 数组
VMCOREINFO_SYMBOL(contig_page_data);              // 连续内存的 pg_data_t
#endif
#ifdef CONFIG_SPARSEMEM_VMEMMAP
VMCOREINFO_SYMBOL_ARRAY(vmemmap);                 // vmemmap 地址
#endif
#ifdef CONFIG_SPARSEMEM
VMCOREINFO_SYMBOL_ARRAY(mem_section);             // 内存段数组
VMCOREINFO_LENGTH(mem_section, NR_SECTION_ROOTS);
VMCOREINFO_STRUCT_SIZE(mem_section);              // struct mem_section 大小
VMCOREINFO_OFFSET(mem_section, section_mem_map); // 偏移
VMCOREINFO_NUMBER(SECTION_SIZE_BITS);             // 段大小
VMCOREINFO_NUMBER(MAX_PHYSMEM_BITS);             // 最大物理地址位宽
#endif
```

**`struct page` / zone / pglist_data 的布局信息：**

```c
// kernel/vmcore_info.c:187-216
VMCOREINFO_STRUCT_SIZE(page);                     // sizeof(struct page)
VMCOREINFO_STRUCT_SIZE(pglist_data);              // sizeof(struct pglist_data)
VMCOREINFO_STRUCT_SIZE(zone);                     // sizeof(struct zone)
VMCOREINFO_STRUCT_SIZE(free_area);                // sizeof(struct free_area)
VMCOREINFO_STRUCT_SIZE(list_head);                // sizeof(struct list_head)
VMCOREINFO_SIZE(nodemask_t);                      // sizeof(nodemask_t)
VMCOREINFO_OFFSET(page, flags);                   // page->flags 偏移
VMCOREINFO_OFFSET(page, _refcount);               // page->_refcount 偏移
VMCOREINFO_OFFSET(page, mapping);                 // page->mapping 偏移
VMCOREINFO_OFFSET(page, lru);                     // page->lru 偏移
VMCOREINFO_OFFSET(page, _mapcount);               // page->_mapcount 偏移
VMCOREINFO_OFFSET(page, private);                 // page->private 偏移
VMCOREINFO_OFFSET(page, compound_info);           // page->compound_info 偏移
VMCOREINFO_OFFSET(pglist_data, node_zones);       // pg_data_t->node_zones
VMCOREINFO_OFFSET(pglist_data, nr_zones);         // pg_data_t->nr_zones
VMCOREINFO_OFFSET(pglist_data, node_start_pfn);   // pg_data_t->node_start_pfn
VMCOREINFO_OFFSET(pglist_data, node_spanned_pages);
VMCOREINFO_OFFSET(pglist_data, node_id);
VMCOREINFO_OFFSET(zone, free_area);               // zone->free_area
VMCOREINFO_OFFSET(zone, vm_stat);                 // zone->vm_stat
VMCOREINFO_OFFSET(zone, spanned_pages);           // zone->spanned_pages
VMCOREINFO_OFFSET(free_area, free_list);          // free_area->free_list
VMCOREINFO_OFFSET(list_head, next);               // list_head->next
VMCOREINFO_OFFSET(list_head, prev);               // list_head->prev
VMCOREINFO_LENGTH(zone.free_area, NR_PAGE_ORDERS);
log_buf_vmcoreinfo_setup();                       // 内核日志缓冲区位置
VMCOREINFO_LENGTH(free_area.free_list, MIGRATE_TYPES);
```

**`PG_*` 标志位的数值（`page->flags` 中）：**

```c
// kernel/vmcore_info.c:217-237
VMCOREINFO_NUMBER(NR_FREE_PAGES);
VMCOREINFO_NUMBER(PG_lru);
VMCOREINFO_NUMBER(PG_private);
VMCOREINFO_NUMBER(PG_swapcache);
VMCOREINFO_NUMBER(PG_swapbacked);
#define PAGE_SLAB_MAPCOUNT_VALUE   (PGTY_slab << 24)
VMCOREINFO_NUMBER(PAGE_SLAB_MAPCOUNT_VALUE);
#ifdef CONFIG_MEMORY_FAILURE
VMCOREINFO_NUMBER(PG_hwpoison);
#endif
VMCOREINFO_NUMBER(PG_head_mask);
#define PAGE_BUDDY_MAPCOUNT_VALUE  (PGTY_buddy << 24)
VMCOREINFO_NUMBER(PAGE_BUDDY_MAPCOUNT_VALUE);
#define PAGE_HUGETLB_MAPCOUNT_VALUE (PGTY_hugetlb << 24)
VMCOREINFO_NUMBER(PAGE_HUGETLB_MAPCOUNT_VALUE);
#define PAGE_OFFLINE_MAPCOUNT_VALUE (PGTY_offline << 24)
VMCOREINFO_NUMBER(PAGE_OFFLINE_MAPCOUNT_VALUE);
```

这些 `PGTY_*` 常量编码在 page->_mapcount 的高 8 位（24-31），crash 工具根据它们判断页面类型（slab、buddy、hugetlb 等）。

**KALLSYMS 信息：**

```c
// kernel/vmcore_info.c:239-245
#ifdef CONFIG_KALLSYMS
VMCOREINFO_SYMBOL(kallsyms_names);                // 符号名字符串表
VMCOREINFO_SYMBOL(kallsyms_num_syms);             // 符号数量
VMCOREINFO_SYMBOL(kallsyms_token_table);          // token 压缩表
VMCOREINFO_SYMBOL(kallsyms_token_index);          // token 索引
VMCOREINFO_SYMBOL(kallsyms_offsets);              // 符号偏移数组
#endif
```

### 6.3 封装为 ELF note

`kernel/vmcore_info.c:43-58` 的 `append_elf_note()` 将上面的 `key=value` 文本封装为标准 ELF note 结构：

```c
// kernel/vmcore_info.c:43-58
Elf_Word *append_elf_note(Elf_Word *buf, char *name,
    unsigned int type, void *data, size_t data_len)
{
    struct elf_note *note = (struct elf_note *)buf;

    note->n_namesz = strlen(name) + 1;
    note->n_descsz = data_len;
    note->n_type   = type;
    buf += DIV_ROUND_UP(sizeof(*note), sizeof(Elf_Word));
    memcpy(buf, name, note->n_namesz);
    buf += DIV_ROUND_UP(note->n_namesz, sizeof(Elf_Word));
    memcpy(buf, data, data_len);
    buf += DIV_ROUND_UP(data_len, sizeof(Elf_Word));

    return buf;
}
```

`update_vmcoreinfo_note()` (`kernel/vmcore_info.c:65-74`) 在每次追加新内容后重新生成整个 ELF note：

```c
// kernel/vmcore_info.c:65-74
static void update_vmcoreinfo_note(void)
{
    u32 *buf = vmcoreinfo_note;

    if (!vmcoreinfo_size)
        return;
    buf = append_elf_note(buf, VMCOREINFO_NOTE_NAME, 0,
                  vmcoreinfo_data, vmcoreinfo_size);
    final_note(buf);
}
```

`final_note()` (`kernel/vmcore_info.c:60-63`) 向 note 末尾写入一个全零的 Elf_Nhdr（type=0, namesz=0, descsz=0），作为 ELF note 段的终止标记。

### 6.4 物理页面的保存 — 为什么能跨 kexec 存活

`kernel/crash_core.c:43-84` 的 `kimage_crash_copy_vmcoreinfo()`：

```c
// kernel/crash_core.c:43-84
int kimage_crash_copy_vmcoreinfo(struct kimage *image)
{
    struct page *vmcoreinfo_base;
    struct page *vmcoreinfo_pages[DIV_ROUND_UP(VMCOREINFO_BYTES, PAGE_SIZE)];
    unsigned int order, nr_pages;
    int i;
    void *safecopy;

    nr_pages = DIV_ROUND_UP(VMCOREINFO_BYTES, PAGE_SIZE);
    order = get_order(VMCOREINFO_BYTES);
    // ...
    vmcoreinfo_base = kimage_alloc_control_pages(image, order);
    // ...
    safecopy = vmap(vmcoreinfo_pages, nr_pages, VM_MAP, PAGE_KERNEL);
    // ...
    image->vmcoreinfo_data_copy = safecopy;
    crash_update_vmcoreinfo_safecopy(safecopy);
```

逻辑是：
1. `kimage_alloc_control_pages()` 从 crash kernel 的保留内存区域（`crashkernel=`）中分配页面。
2. 用 `vmap()` 将物理页面映射到内核虚拟地址空间，得到 `safecopy` 指针。
3. `crash_update_vmcoreinfo_safecopy(safecopy)` 将 vmcoreinfo_data 内容复制到 safecopy。
4. kexec 加载完成后，`arch_kexec_protect_crashkres()` 通过页表保护这片内存（禁止 kernel direct mapping 读写），但 `vmap` 的映射仍有效。
5. 崩溃时，`crash_save_vmcoreinfo()` 先将 `vmcoreinfo_data` 指向 safecopy（见 `kernel/vmcore_info.c:90-91`），然后追加 CRASHTIME。

因为 safecopy 的物理页在 `crashkernel=` 保留区域中，capture kernel 在自己的地址空间内可以访问到它，这就是它能跨 kexec 传递的关键。

---

## 七、`crash_smp_send_stop()` — 架构特定的停止方式

```c
// arch/x86/kernel/crash.c:78-91
void crash_smp_send_stop(void)
{
    static int cpus_stopped;

    if (cpus_stopped)
        return;

    if (smp_ops.crash_stop_other_cpus)
        smp_ops.crash_stop_other_cpus();
    else
        smp_send_stop();

    cpus_stopped = 1;
}
```

普通 panic 使用 `smp_send_stop()`（IPI-based），但 kdump 使用 `kdump_nmi_shootdown_cpus()`（NMI-based）。区别在于：IPI 可能被屏蔽（关中断的 CPU 收不到），但 NMI 一定能送达。

`static int cpus_stopped` 防止重复调用 — 如果 `panic_other_cpus_shutdown()` 和 `native_machine_crash_shutdown()` 都试图停 CPU，第二次调用直接返回。

---

## 八、`panic_other_cpus_shutdown()` — 两条停止路径

```c
// kernel/panic.c:550-567
static void panic_other_cpus_shutdown(bool crash_kexec)
{
    if (panic_print & SYS_INFO_ALL_BT)
        panic_trigger_all_cpu_backtrace();

    /*
     * Note that smp_send_stop() is the usual SMP shutdown function,
     * which unfortunately may not be hardened to work in a panic
     * situation. If we want to do crash dump after notifier calls
     * and kmsg_dump, we will need architecture dependent extra
     * bits in addition to stopping other CPUs, hence we rely on
     * crash_smp_send_stop() for that.
     */
    if (!crash_kexec)
        smp_send_stop();
    else
        crash_smp_send_stop();
}
```

两条分支：
- `crash_kexec=false`（普通 panic 无 kdump）：用 `smp_send_stop()` 通过 IPI 停掉其他 CPU。
- `crash_kexec=true`（有 kdump）：用 `crash_smp_send_stop()` 即 NMI shootdown。虽然 `native_machine_crash_shutdown()` 里还会再调一次 `crash_smp_send_stop()`，但 `static int cpus_stopped` 保证不会重复。

---

## 九、完整调用链总结

```
vpanic()                                    kernel/panic.c:627-634
  │
  ├─[crash_kexec_post_notifiers=false]── __crash_kexec(NULL)
  │                                       kernel/panic.c:670
  │    ├─ kexec_trylock()                  kernel/kexec_internal.h:24-28
  │    ├─ crash_setup_regs()               kernel/crash_core.c:141
  │    ├─ crash_save_vmcoreinfo()          kernel/vmcore_info.c:84-95
  │    │    ├─ vmcoreinfo_append_str("CRASHTIME=...")  :93
  │    │    └─ update_vmcoreinfo_note()                :94
  │    │         └─ append_elf_note(...vmcoreinfo_data...)  :65-73
  │    ├─ native_machine_crash_shutdown()  arch/x86/kernel/crash.c:100-145
  │    │    ├─ crash_smp_send_stop()                   :113
  │    │    │    └─ kdump_nmi_shootdown_cpus()         :70-75
  │    │    │         └─ nmi_shootdown_cpus(kdump_nmi_callback)
  │    │    │              └─ kdump_nmi_callback(cpu, regs)      :56-68
  │    │    │                   ├─ crash_save_cpu(regs, cpu)     :58
  │    │    │                   ├─ cpu_emergency_stop_pt()       :63
  │    │    │                   └─ disable_local_APIC()          :67
  │    │    ├─ ioapic_zap_locks() / clear_IO_APIC()  :123-125
  │    │    ├─ lapic_shutdown()                       :127
  │    │    ├─ restore_boot_irq_mode()                :128
  │    │    └─ crash_save_cpu(regs, smp_processor_id()) :144
  │    └─ machine_kexec(kexec_crash_image)         :145
  │         └─ machine_kexec()   arch/x86/kernel/machine_kexec_64.c:400-481
  │              ├─ load_segments()                            :467
  │              └─ relocate_kernel_ptr(...)                   :470
  │                   └─ → capture kernel entry
  │
  ├─ panic_other_cpus_shutdown(true)  kernel/panic.c:672
  │    └─ crash_smp_send_stop()          kernel/panic.c:566
  │         └─ (static cpus_stopped 防止重复)
  │
  └─ [crash_kexec_post_notifiers=true]
       ├─ 先执行 panic_notifiers + kmsg_dump
       └─ 再 __crash_kexec(NULL)         kernel/panic.c:696
```

---

## 十、关键设计思想

| 设计点 | 实现 | 原因 |
|---|---|---|
| NMI 停止其他 CPU | `nmi_shootdown_cpus()` | 被停止的 CPU 可能关中断/持锁，IPI 收不到，NMI 一定能送达 |
| 原子 cmpxchg 互斥锁 | `kexec_trylock()` | panic 可能在 NMI 中触发，不能用 spinlock（可能死锁） |
| 不能在 `machine_kexec` 中失败 | 注释 "past the point of no return" | 已关中断灭九族，无法回退 |
| 段寄存器提前加载 | `load_segments()` 在 GDT 破坏前 | 段寄存器缓存不可见部分依赖 GDT，GDT 即将被覆盖 |
| VMCOREINFO 跨 kexec 存活 | 页面从 crashkernel 保留区域分配 | 那部分物理内存不被 capture kernel 覆盖 |
| CRASHTIME 最后追加 | 崩溃时刻调用 `crash_save_vmcoreinfo()` | 所有初始化常量早已写好，崩溃时只加时间戳 |
| `__noclone` 标记 | `__crash_kexec()` | 防止编译器将其内联克隆到多个地址，保证栈回溯准确 |
| CFI 绕过 | `ANNOTATE_NOCFI_SYM(machine_kexec)` | `relocate_kernel` 永不返回，CFI 无法验证 |

---

## 十一、数据流图

```
┌──────────────────────────────────────────────────┐
│                  生产内核 (panicked)               │
│                                                    │
│  ┌─────────────┐    寄存器快照     ┌────────────┐ │
│  │ crash_save_ │─────────────────▶│ ELF notes  │ │
│  │ cpu(regs,cpu)│  (所有CPU的)     │  (每CPU)   │ │
│  └─────────────┘                  └────────────┘ │
│                                                    │
│  ┌─────────────────┐        ┌────────────────────┐│
│  │ crash_save_     │──VTXT─▶│ vmcoreinfo_note    ││
│  │ vmcoreinfo()    │        │ (物理页,在crash区域)││
│  │ CRASHTIME=%lld  │        └────────────────────┘│
│  └─────────────────┘                               │
│                                                    │
│  ┌──────────────┐         ┌──────────────────────┐│
│  │ machine_kexec│────────▶│  capture kernel      ││
│  └──────────────┘         │  /proc/vmcore        ││
│                            └──────────────────────┘│
└──────────────────────────────────────────────────┘
```

---

## 下一篇文章

[第二篇：capture kernel 启动与 /proc/vmcore](./02-capture-kernel.md)
