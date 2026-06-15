# 第二篇：capture kernel 启动与 /proc/vmcore

> 内核源码：`fs/proc/vmcore.c` `include/linux/crash_dump.h` `kernel/crash_core.c`

系列目录：[vmcore 深度解析系列](./README.md)

---

## 一、从跳转点到用户空间 — capture kernel 启动全景

上一篇讲到 `machine_kexec()` 执行 `relocate_kernel()` 后，CPU 跳入 capture kernel 的入口点。自此，以下事件依次发生：

```
relocate_kernel() 完成
  │
  ├─ CPU 跳入 capture kernel 的 startup_64 (arch/x86/kernel/head_64.S)
  │    ├─ 设置初始页表 (identity-mapped)
  │    ├─ 设置 GDT / IDT / TSS
  │    ├─ 清除 CR4.PGE（已在 relocate_kernel 中完成）
  │    └─ 跳入 x86_64_start_kernel → start_kernel → rest_init → kernel_init
  │
  ├─ 启动过程识别自身为 capture kernel
  │    ├─ 检测 elfcorehdr= 内核参数 → is_kdump_kernel() == true
  │    ├─ 只使用 crashkernel= 保留的内存区域
  │    └─ 不接触任何生产内核的物理内存页面
  │
  ├─ 挂载 initramfs (最小化 rootfs)
  │    ├─ 通常通过 dracut/mkinitrd 打包
  │    ├─ 包含 makedumpfile / crash / vmcore-dmesg
  │    └─ 启动到 rescue shell 或自动 dump 脚本
  │
  └─ 用户空间工具读取 /proc/vmcore
       ├─ makedumpfile 压缩并写入文件
       └─ vmcore-dmesg 快速提取 panic 日志
```

---

## 二、capture kernel 的关键特征

### 2.1 `is_kdump_kernel()` — 如何判断自己是 capture kernel

`include/linux/crash_dump.h:68-71`：

```c
// include/linux/crash_dump.h:57-71
#ifndef is_kdump_kernel
/*
 * is_kdump_kernel() checks whether this kernel is booting after a panic of
 * previous kernel or not. This is determined by checking if previous kernel
 * has passed the elf core header address on command line.
 *
 * This is not just a test if CONFIG_CRASH_DUMP is enabled or not. It will
 * return true if CONFIG_CRASH_DUMP=y and if kernel is booting after a panic
 * of previous kernel.
 */

static inline bool is_kdump_kernel(void)
{
    return elfcorehdr_addr != ELFCORE_ADDR_MAX;
}
#endif
```

生产内核在 `__crash_kexec()` 中通过 kexec 机制将 `elfcorehdr_addr`（ELF 头文件的物理地址）传给 capture kernel 作为内核命令行参数。如果 capture kernel 发现 `elfcorehdr_addr` 不为 `ELFCORE_ADDR_MAX`（即 ELFCORE_ADDR_MAX 是一个无效值，通常是 `~0ULL`），就确认自己处于 kdump 模式。

### 2.2 内存隔离 — 绝不动生产内核的内存

capture kernel 启动时使用 crashkernel= 保留的物理内存区域。这段区域是在生产内核启动时通过以下方式保留的：

```
生产内核启动参数: crashkernel=256M@128M
  → memblock 保留了 [128M, 384M) 这一段物理内存
  → 生产内核无法使用这段内存（从 buddy allocator 中移除）
  → kexec 加载 capture kernel 时将内核镜像、initramfs 放入这段保留区域
  → capture kernel 启动后只看到这段保留内存
```

这意味着除了保留区域外，所有生产内核的物理内存对 capture kernel 来说是"不可分配但可读取"的状态。`read_from_oldmem()` 通过映射 `pfn_is_ram()` 判定为 RAM 的页面，逐页读取旧内核的物理内存。

### 2.3 `is_vmcore_usable()` — 确认 dump 有效

`include/linux/crash_dump.h` 中还有一个辅助函数：

```c
// include/linux/crash_dump.h:74
/* is_vmcore_usable() checks if the kernel is booting after a panic and
 * the kdump kernel image is using a reliable method for dumping the
 * vmcore.
 */
```

它确保 elfcorehdr 地址合法，并且 kdump 过程没有发生异常。

---

## 三、`/proc/vmcore` — 内核模块如何暴露崩溃内存

`fs/proc/vmcore.c` 是整个 kdump 体系的用户空间接口。它提供 1756 行代码，实现了一个读-only 的 proc 文件，将生产内核的全部物理内存以 ELF 格式呈现给用户空间。

### 3.1 模块初始化 — `vmcore_init()`

`fs/proc/vmcore.c:1720-1741`：

```c
// fs/proc/vmcore.c:1720-1741
// 在 vmcore_init() 函数中
    if (!(is_vmcore_usable()))
        return rc;
    rc = parse_crash_elf_headers();
    if (rc) {
        elfcorehdr_free(elfcorehdr_addr);
        pr_warn("not initialized\n");
        return rc;
    }
    elfcorehdr_free(elfcorehdr_addr);
    elfcorehdr_addr = ELFCORE_ADDR_ERR;

    proc_vmcore = proc_create("vmcore", S_IRUSR, NULL, &vmcore_proc_ops);
    if (proc_vmcore)
        proc_vmcore->size = vmcore_size;
    return 0;
}
fs_initcall(vmcore_init);
```

初始化流程：
1. `is_vmcore_usable()` — 确认这是 kdump 内核且 dump 有效
2. `parse_crash_elf_headers()` — 解析生产内核传递过来的 ELF 头（物理地址 → 虚拟地址映射）
3. `proc_create("vmcore", S_IRUSR, NULL, &vmcore_proc_ops)` — 创建 `/proc/vmcore` 节点
4. 设置文件大小为 `vmcore_size`，供 `stat()` / `ls -l` 显示

注意第 3 步中 `S_IRUSR` 权限是 0400（只读，仅 owner）。普通用户无法读取 `/proc/vmcore`。

### 3.2 proc_ops 函数表

`fs/proc/vmcore.c:710-716`：

```c
// fs/proc/vmcore.c:710-716
static const struct proc_ops vmcore_proc_ops = {
    .proc_open      = open_vmcore,
    .proc_release   = release_vmcore,
    .proc_read_iter = read_vmcore,
    .proc_lseek     = default_llseek,
    .proc_mmap      = mmap_vmcore,
};
```

五个操作：
- **open** — 打开 vmcore 文件
- **release** — 关闭 vmcore 文件
- **read_iter** — 迭代读取 vmcore 数据（现代内核用 read_iter 替代传统 read）
- **lseek** — 定位读取位置（使用内核默认的 `default_llseek`）
- **mmap** — 部分平台支持 mmap 读取（性能优化）

---

## 四、`open_vmcore()` — 控制并发访问

`fs/proc/vmcore.c:135-147`：

```c
// fs/proc/vmcore.c:135-147
static int open_vmcore(struct inode *inode, struct file *file)
{
    mutex_lock(&vmcore_mutex);
    vmcore_opened = true;
    if (vmcore_open + 1 == 0) {
        mutex_unlock(&vmcore_mutex);
        return -EBUSY;
    }
    vmcore_open++;
    mutex_unlock(&vmcore_mutex);

    return 0;
}
```

`open_vmcore()` 的核心作用是防止溢出。`vmcore_open` 变量记录当前打开 vmcore 的进程数。如果 `vmcore_open` 已经是最大值（`unsigned int` 溢出到 0），返回 `-EBUSY`。

**为什么不限制只能开一次？** 多个进程可以同时读取 `/proc/vmcore`，比如同时运行 `makedumpfile` 和 `vmcore-dmesg`。每个进程有独立的文件描述符和偏移位置。

### 4.1 `release_vmcore()` — 释放文件引用

`fs/proc/vmcore.c:149-156`：

```c
// fs/proc/vmcore.c:149-156
static int release_vmcore(struct inode *inode, struct file *file)
{
    mutex_lock(&vmcore_mutex);
    vmcore_open--;
    mutex_unlock(&vmcore_mutex);

    return 0;
}
```

简洁地递减引用计数。capture kernel 本身是一个最简内核，`/proc/vmcore` 是它最重要的功能，因此关闭后的清理工作非常少。

---

## 五、`read_vmcore()` → `__read_vmcore()` — 核心数据读取

`fs/proc/vmcore.c:430-433`：

```c
// fs/proc/vmcore.c:430-433
static ssize_t read_vmcore(struct kiocb *iocb, struct iov_iter *iter)
{
    return __read_vmcore(iter, &iocb->ki_pos);
}
```

`read_vmcore` 是 `read_iter` 的包装，转发到 `__read_vmcore()`。它使用 `iocb->ki_pos` 作为当前文件偏移量，支持 lseek 后读取任意位置。

### 5.1 `__read_vmcore()` — 全量读取逻辑

`fs/proc/vmcore.c:333-428`，这是整个文件的精华：

```c
// fs/proc/vmcore.c:330-332
/* Read from the ELF header and then the crash dump. On error, negative value is
 * returned otherwise number of bytes read are returned.
 */

// fs/proc/vmcore.c:333-428
static ssize_t __read_vmcore(struct iov_iter *iter, loff_t *fpos)
{
    struct vmcore_range *m = NULL;
    ssize_t acc = 0, tmp;
    size_t tsz;
    u64 start;

    if (!iov_iter_count(iter) || *fpos >= vmcore_size)
        return 0;

    iov_iter_truncate(iter, vmcore_size - *fpos);

    /* Read ELF core header */
    if (*fpos < elfcorebuf_sz) {
        tsz = min(elfcorebuf_sz - (size_t)*fpos, iov_iter_count(iter));
        if (copy_to_iter(elfcorebuf + *fpos, tsz, iter) < tsz)
            return -EFAULT;
        *fpos += tsz;
        acc += tsz;

        /* leave now if filled buffer already */
        if (!iov_iter_count(iter))
            return acc;
    }
    // ... 继续 ELF notes 和 PT_LOAD 段
```

`__read_vmcore()` 按顺序返回三个区域的数据：

1. **ELF 头 (Ehdr + Phdrs)** — 从 `elfcorebuf` 中读取
2. **ELF PT_NOTE 段** — 包含各 CPU 的寄存器快照、VMCOREINFO note
3. **ELF PT_LOAD 段** — 生产内核的物理 RAM 数据

每一步都检查 `iov_iter_count(iter)` 是否为 0，一旦用户缓冲区满了就返回已读的字节数。

### 5.2 第1段：ELF 头部的读取

```c
    /* Read ELF core header */
    if (*fpos < elfcorebuf_sz) {
        tsz = min(elfcorebuf_sz - (size_t)*fpos, iov_iter_count(iter));
        if (copy_to_iter(elfcorebuf + *fpos, tsz, iter) < tsz)
            return -EFAULT;
        *fpos += tsz;
        acc += tsz;

        /* leave now if filled buffer already */
        if (!iov_iter_count(iter))
            return acc;
    }
```

`elfcorebuf` 在 `parse_crash_elf_headers()` 中构造，内容是一个标准的 ELF64 头（`Elf64_Ehdr`）+ 一组程序头（`Elf64_Phdr[]`）。这些信息描述：
- 有多少个 PT_LOAD 段（对应物理 RAM 内存区间）
- 每个段的物理地址范围
- PT_NOTE 段的位置和大小
- 机器类型（EM_X86_64）、字节序（ELFDATA2LSB）

`copy_to_iter()` 是内核的高效批量拷贝函数，将内核缓冲区数据复制到用户空间 `struct iov_iter`（iovec 迭代器）中。

### 5.3 第2段：ELF notes 的读取

`fs/proc/vmcore.c:358-404`：

```c
    /* Read ELF note segment */
    if (*fpos < elfcorebuf_sz + elfnotes_sz) {
        void *kaddr;

        /* We add device dumps before other elf notes because the
         * other elf notes may not fill the elf notes buffer
         * completely and we will end up with zero-filled data
         * between the elf notes and the device dumps. Tools will
         * then try to decode this zero-filled data as valid notes
         * and we don't want that. Hence, adding device dumps before
         * the other elf notes ensure that zero-filled data can be
         * avoided.
         */
#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
        /* Read device dumps */
        if (*fpos < elfcorebuf_sz + vmcoredd_orig_sz) {
            tsz = min(elfcorebuf_sz + vmcoredd_orig_sz -
                  (size_t)*fpos, iov_iter_count(iter));
            start = *fpos - elfcorebuf_sz;
            if (vmcoredd_copy_dumps(iter, start, tsz))
                return -EFAULT;

            *fpos += tsz;
            acc += tsz;

            /* leave now if filled buffer already */
            if (!iov_iter_count(iter))
                return acc;
        }
#endif /* CONFIG_PROC_VMCORE_DEVICE_DUMP */

        /* Read remaining elf notes */
        tsz = min(elfcorebuf_sz + elfnotes_sz - (size_t)*fpos,
              iov_iter_count(iter));
        kaddr = elfnotes_buf + *fpos - elfcorebuf_sz - vmcoredd_orig_sz;
        if (copy_to_iter(kaddr, tsz, iter) < tsz)
            return -EFAULT;

        *fpos += tsz;
        acc += tsz;

        /* leave now if filled buffer already */
        if (!iov_iter_count(iter))
            return acc;

        cond_resched();
    }
```

ELF notes 段按以下顺序排列：
1. **Device dumps**（如果 `CONFIG_PROC_VMCORE_DEVICE_DUMP` 开启）— 设备驱动注册的额外 dump 数据（如固件日志）
2. **VMCOREINFO note** — 第一篇详述的内核常量 key=value 文本
3. **Per-CPU 寄存器 notes** — 每个 CPU 的 `pt_regs` 快照，以 `NT_PRSTATUS` note 表示
4. **VM 区域 notes** — 每个线程的用户态寄存器快照，以 `NT_PRPSINFO` + `NT_PRSTATUS` 表示

`cond_resched()` 在数据分段之间被调用，这是一个调度点（scheduling point）。内核注释明确：在 `__read_vmcore()` 的循环中必须 `cond_resched()`，因为它可能在非抢占内核上运行且执行大量内存拷贝。

### 5.4 第3段：PT_LOAD 段 — 物理 RAM 数据

`fs/proc/vmcore.c:406-425`：

```c
    list_for_each_entry(m, &vmcore_list, list) {
        if (*fpos < m->offset + m->size) {
            tsz = (size_t)min_t(unsigned long long,
                        m->offset + m->size - *fpos,
                        iov_iter_count(iter));
            start = m->paddr + *fpos - m->offset;
            tmp = read_from_oldmem(iter, tsz, &start,
                    cc_platform_has(CC_ATTR_MEM_ENCRYPT));
            if (tmp < 0)
                return tmp;
            *fpos += tsz;
            acc += tsz;

            /* leave now if filled buffer already */
            if (!iov_iter_count(iter))
                return acc;
        }

        cond_resched();
    }
```

`vmcore_list` 是一个链表，每个元素描述一段连续的物理内存区间（`struct vmcore_range`）：

```c
// fs/proc/vmcore.c:35-38
/* List representing chunks of contiguous memory areas and their offsets in
 * vmcore file.
 */
static LIST_HEAD(vmcore_list);
```

每个 `vmcore_range` 包含：
- `paddr` — 物理起始地址
- `size` — 区间大小
- `offset` — 在 vmcore 文件中的偏移

遍历链表时，`read_from_oldmem()` 被调用来从生产内核的物理内存中读取数据。注意 `start` 是物理地址（因为 `read_from_oldmem()` 内部用 `*ppos / PAGE_SIZE` 得到 PFN）。

---

## 六、`read_from_oldmem()` — 从旧内核物理内存读取

`fs/proc/vmcore.c:159-206`，这个函数是 kdump 的核心 I/O 引擎：

```c
// fs/proc/vmcore.c:158-159
/* Reads a page from the oldmem device from given offset. */

// fs/proc/vmcore.c:159-206
ssize_t read_from_oldmem(struct iov_iter *iter, size_t count,
             u64 *ppos, bool encrypted)
{
    unsigned long pfn, offset;
    ssize_t nr_bytes;
    ssize_t read = 0, tmp;
    int idx;

    if (!count)
        return 0;

    offset = (unsigned long)(*ppos % PAGE_SIZE);
    pfn = (unsigned long)(*ppos / PAGE_SIZE);

    idx = srcu_read_lock(&vmcore_cb_srcu);
    do {
        if (count > (PAGE_SIZE - offset))
            nr_bytes = PAGE_SIZE - offset;
        else
            nr_bytes = count;

        /* If pfn is not ram, return zeros for sparse dump files */
        if (!pfn_is_ram(pfn)) {
            tmp = iov_iter_zero(nr_bytes, iter);
        } else {
            if (encrypted)
                tmp = copy_oldmem_page_encrypted(iter, pfn,
                                 nr_bytes,
                                 offset);
            else
                tmp = copy_oldmem_page(iter, pfn, nr_bytes,
                               offset);
        }
        if (tmp < nr_bytes) {
            srcu_read_unlock(&vmcore_cb_srcu, idx);
            return -EFAULT;
        }

        *ppos += nr_bytes;
        count -= nr_bytes;
        read += nr_bytes;
        ++pfn;
        offset = 0;
    } while (count);
    srcu_read_unlock(&vmcore_cb_srcu, idx);

    return read;
}
```

**逐页读取逻辑**：
1. 将物理地址 `ppos` 转换为 `(pfn, offset)` 对
2. 对每个页面调用 `pfn_is_ram(pfn)` — 如果该 PFN 不是 RAM（例如设备 MMIO 区域），直接返回零填充
3. 如果是 RAM，调用 `copy_oldmem_page()` 或 `copy_oldmem_page_encrypted()`（AMD SEV/SME 加密内存）
4. 逐页循环，直到读完所有请求的字节

**SRCU 保护**：`srcu_read_lock(&vmcore_cb_srcu)` 保护 `vmcore_cb_list` 链表，允许已注册的回调函数在读取过程中不被移除。

### 6.1 `pfn_is_ram()` — 判断物理页是否为 RAM

内核在启动时构建了 `iomem_resource` 资源树（通过解析 E820/BIOS/EFI 的物理地址映射）。`pfn_is_ram()` 查询该树，返回该 PFN 是否属于 "System RAM" 类型。稀疏 dump 文件用零填充非 RAM 区域以保持 ELF 文件结构完整。

### 6.2 `copy_oldmem_page()` — 架构特定的页面拷贝

`include/linux/crash_dump.h:31-32` 声明了此函数：

```c
// include/linux/crash_dump.h:31-32
ssize_t copy_oldmem_page(struct iov_iter *i, unsigned long pfn,
    size_t csize, unsigned long offset);
```

对于 x86_64，该函数在 `arch/x86/kernel/crash_dump_64.c` 中实现。基本流程是：
1. 通过 PFN 计算物理地址 → 使用 `ioremap_cache()` 映射到临时内核虚拟地址
2. 将数据从临时映射拷贝到 `iov_iter`（用户空间缓冲区）
3. 使用 `iounmap()` 解除映射

因为 capture kernel 的页表只映射了自己保留的内存区域，生产内核的物理内存不在其直接映射范围内，所以每次读取都需要临时 ioremap。

---

## 七、ELF 头部的构建 — `parse_crash_elf_headers()`

在生产内核的 `native_machine_crash_shutdown()` 之前，kexec 框架已经构建好了 ELF core 头部（通过 `fill_up_crash_elf_data()` 和 `prepare_elf_headers()`）。capture kernel 通过 `elfcorehdr=` 参数接收其在物理内存中的位置，然后用 `parse_crash_elf_headers()` 解析它：

1. 使用 `elfcorehdr_read()` 从旧内核物理地址读取 ELF header
2. 解析 Phdr 表中的 PT_NOTE 和 PT_LOAD 段
3. 将 PT_LOAD 段信息填入 `vmcore_list` 链表
4. 计算 `vmcore_size`（总文件大小）

`elfcorehdr_read()` 有两个 weak 默认实现和架构 override：

```c
// fs/proc/vmcore.c:225-233
ssize_t __weak elfcorehdr_read(char *buf, size_t count, u64 *ppos)
{
    struct kvec kvec = { .iov_base = buf, .iov_len = count };
    struct iov_iter iter;

    iov_iter_kvec(&iter, ITER_DEST, &kvec, 1, count);

    return read_from_oldmem(&iter, count, ppos, false);
}
```

默认实现也是通过 `read_from_oldmem()` 读取旧内核的 `elfcorehdr` 区域。某些架构（如 ppc64）可以 override 这个函数，因为它们的 crash 过程会在 2nd kernel 中分配新的 elfcorehdr 内存。

---

## 八、Device Dump — 设备驱动额外数据

如果配置了 `CONFIG_PROC_VMCORE_DEVICE_DUMP`，设备驱动可以在 panic 期间通过 `vmcoredd_update_program_headers()` 注册额外的 dump 数据：

```c
// fs/proc/vmcore.c:55-66
#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
struct vmcoredd_node {
    struct list_head list;  /* List of dumps */
    void *buf;              /* Buffer containing device's dump */
    unsigned int size;      /* Size of the buffer */
};

/* Device Dump list and mutex to synchronize access to list */
static LIST_HEAD(vmcoredd_list);
```

`vmcoredd_copy_dumps()` (`fs/proc/vmcore.c:270-295`) 遍历该链表，将每个设备的 dump 数据拷贝到用户空间：

```c
// fs/proc/vmcore.c:270-295
static int vmcoredd_copy_dumps(struct iov_iter *iter, u64 start, size_t size)
{
    struct vmcoredd_node *dump;
    u64 offset = 0;
    size_t tsz;
    char *buf;

    list_for_each_entry(dump, &vmcoredd_list, list) {
        if (start < offset + dump->size) {
            tsz = min(offset + (u64)dump->size - start, (u64)size);
            buf = dump->buf + start - offset;
            if (copy_to_iter(buf, tsz, iter) < tsz)
                return -EFAULT;

            size -= tsz;
            start += tsz;

            /* Leave now if buffer filled already */
            if (!size)
                return 0;
        }
        offset += dump->size;
    }

    return 0;
}
```

使用场景包括：固件日志、存储控制器的内部状态、网络控制器的芯片寄存器快照等。

---

## 九、`vmcore_cb` 回调机制 — 模块化物理内存读取

`fs/proc/vmcore.c:74-99` 定义了 vmcore callback 机制：

```c
// fs/proc/vmcore.c:74-76
DEFINE_STATIC_SRCU(vmcore_cb_srcu);
/* List of registered vmcore callbacks. */
static LIST_HEAD(vmcore_cb_list);
```

```c
// fs/proc/vmcore.c:84-99
void register_vmcore_cb(struct vmcore_cb *cb)
{
    INIT_LIST_HEAD(&cb->next);
    mutex_lock(&vmcore_mutex);
    list_add_tail(&cb->next, &vmcore_cb_list);
    /*
     * Registering a vmcore callback after the vmcore was opened is
     * very unusual (e.g., manual driver loading).
     */
    if (vmcore_opened)
        pr_warn_once("Unexpected vmcore callback registration\n");
    if (!vmcore_open && cb->get_device_ram)
        vmcore_process_device_ram(cb);
    mutex_unlock(&vmcore_mutex);
}
EXPORT_SYMBOL_GPL(register_vmcore_cb);
```

这个机制允许其他内核模块在 `read_from_oldmem()` 的执行路径上插入回调。回调可以在特定 PFN 范围内替换或增强数据流。典型用途包括：
- 为加密内存提供解密后的数据
- 为特定设备内存区域提供合成数据（synthetic data）
- 过滤敏感信息

回调注册必须在 `/proc/vmcore` 被打开之前完成（`vmcore_opened == false`）。

---

## 十、用户空间工具交互全景

capture kernel 在运行 initramfs 中的 `/init` 脚本后，用户空间工具开始与 `/proc/vmcore` 交互：

### 10.1 `makedumpfile` — 压缩和写入 dump 文件

`makedumpfile` 读取 `/proc/vmcore`，执行以下操作：
1. 解析 ELF header，获取所有 PT_LOAD 段的物理地址范围
2. 通过 `read()` 系统调用（即 `/proc/vmcore` 的 `read_vmcore()`）逐块读取数据
3. 过滤不需要的页面：
   - **零页** — 全零的页面（非常常见，大部分内存未使用）
   - **cache 页** — 文件缓存页面可被丢弃
   - **user data 页** — 用户进程内存，通常无关
   - **free 页** — buddy allocator free list 中的页面
4. 压缩剩余页面（支持 lzo、snappy、zstd 等算法）
5. 写入目标文件（通常为 `/var/crash/127.0.0.1-YYYY-MM-DD-HH:MM:SS/vmcore`）

过滤逻辑依赖 VMCOREINFO 中的 `NR_FREE_PAGES`、`PAGE_BUDDY_MAPCOUNT_VALUE`、`PG_lru` 等常量来判断页面类型。

### 10.2 `crash` 工具 — 在线/离线 vmcore 分析

crash 工具支持两种模式：
- **在线模式** — 直接读取 `/proc/vmcore`（在 capture kernel 中运行）
- **离线模式** — 读取 makedumpfile 保存的压缩文件

它使用 VMCOREINFO + debuginfo vmlinux 来：
- 解析内核数据结构（`task_struct`、`mm_struct`、`page` 等）
- 回溯所有 CPU 的调用栈
- 检查每个进程的状态
- 分析内存泄漏（通过 page flags 遍历）

典型的 crash 操作：
```
crash> bt          # 打印崩溃线程的调用栈
crash> bt -a       # 打印所有 CPU 的调用栈
crash> log         # 打印内核日志缓冲区
crash> ps          # 列出所有进程
crash> kmem -i     # 显示内存使用统计
crash> vm          # 显示虚拟内存映射
```

### 10.3 `vmcore-dmesg` — 快速 panic 日志提取

`vmcore-dmesg` 专门提取内核日志缓冲区（`log_buf`）。它不需要 VMCOREINFO 中的复杂数据结构信息，只需知道：
- `log_buf` 的虚拟地址或物理地址
- `log_buf_len` 的大小
- 日志缓冲区的环形缓冲结构

这使得它比 `makedumpfile` 快数百倍，适合在第一现场快速判断 panic 原因。

它的实现依赖 VMCOREINFO 中的 `log_buf_vmcoreinfo_setup()`（`kernel/vmcore_info.c:215`），该函数在生产内核运行时将内核日志缓冲区的物理地址和大小写入 VMCOREINFO。

---

## 十一、`mmap_vmcore()` — 零拷贝读取（可选）

`fs/proc/vmcore.c` 还提供了 `mmap_vmcore()` 接口（通过 `proc_mmap`）。对于支持 mmap 的 PT_LOAD 段，用户空间可以直接 mmap 物理内存到自己的地址空间，避免 `read()` + `copy_to_iter()` 的拷贝开销。

但 mmap 有局限性 — 它不能提供解密、设备 dump、回调转换等功能。因此大多数工具仍使用 `read()` 方式。

---

## 十二、完整数据流图

```
                      capture kernel 用户空间
┌─────────────────────────────────────────────────────────┐
│                                                          │
│  ┌──────────────┐  read(/proc/vmcore)  ┌──────────────┐ │
│  │ makedumpfile ├─────────────────────▶│ /proc/vmcore │ │
│  └──────┬───────┘                      └──────┬───────┘ │
│         │ 压缩写入                            │          │
│  ┌──────▼───────┐                    ┌────────▼───────┐ │
│  │ /var/crash/  │                    │ read_vmcore()  │ │
│  │   vmcore     │                    │ (read_iter)    │ │
│  └──────────────┘                    └────────┬───────┘ │
│                                               │          │
│  ┌──────────────┐                    ┌────────▼───────┐ │
│  │ crash 工具   ◀────────────────────│ __read_vmcore()│ │
│  │ (离线分析)   │                    └────────┬───────┘ │
│  └──────────────┘                             │          │
│                                  ┌────────────▼───────┐ │
│  ┌──────────────┐               │ 分段读取:           │ │
│  │ vmcore-dmesg │               │ 1. ELF header      │ │
│  │(快速日志提取)│               │ 2. ELF notes       │ │
│  └──────────────┘               │ 3. PT_LOAD 段      │ │
│                                  └────────┬───────────┘ │
└──────────────────────────────────────────┼─────────────┘
                                           │
                              ┌────────────▼───────────┐
                              │  read_from_oldmem()    │
                              │  逐页读取旧内核内存    │
                              └────────┬───────────────┘
                                       │
                              ┌────────▼───────────────┐
                              │  copy_oldmem_page()    │
                              │  PFN→ioremap→拷贝→释放 │
                              └────────┬───────────────┘
                                       │
                              ┌────────▼───────────────┐
                              │  生产内核物理内存      │
                              │  (reserved 区域之外)   │
                              └───────────────────────┘
```

---

## 十三、`/proc/vmcore` 的 ELF 文件结构

从用户空间的视角，`/proc/vmcore` 是一个标准的 ELF64 core dump 文件，结构如下：

```
┌───────────────────────────────────────────────────────┐
│ ELF Header (Elf64_Ehdr)                               │
│   e_type   = ET_CORE (4)                              │
│   e_machine = EM_X86_64 (62)                          │
│   e_phoff   → Program Header Table 偏移               │
│   e_phnum   = n                                       │
├───────────────────────────────────────────────────────┤
│ Program Header Table (Elf64_Phdr[])                   │
│   [0] p_type = PT_NOTE (4)    ← ELF notes 段          │
│       p_offset = elfcorebuf_sz  ← 在文件中的偏移       │
│       p_filesz = elfnotes_sz    ← 大小                │
│   [1] p_type = PT_LOAD (1)    ← 物理 RAM 段 #1        │
│       p_paddr  = physical_addr  ← 物理地址             │
│       p_offset = file_offset    ← 在文件中的偏移       │
│       p_filesz = size           ← 大小                │
│   [2] p_type = PT_LOAD (1)    ← 物理 RAM 段 #2        │
│       ...                                             │
│   [n-1] p_type = PT_LOAD (1)  ← 物理 RAM 段 #m        │
├───────────────────────────────────────────────────────┤
│ ELF Notes 段 (从 elfcorebuf_sz 偏移开始)               │
│   Note[0]: NT_PRSTATUS (registers for CPU 0)          │
│   Note[1]: NT_PRSTATUS (registers for CPU 1)          │
│   ...                                                 │
│   Note[N-1]: VMCOREINFO (type=0, name="VMCOREINFO")   │
│   Note[N]: NT_PRSTATUS (user tasks, 可选)              │
│   Note[最后]: 全零 Elf_Nhdr (终止标记)                 │
├───────────────────────────────────────────────────────┤
│ PT_LOAD #1 数据                                       │
│   生产内核物理内存 RAM[addr1 .. addr1+size1]           │
├───────────────────────────────────────────────────────┤
│ PT_LOAD #2 数据                                       │
│   生产内核物理内存 RAM[addr2 .. addr2+size2]           │
├───────────────────────────────────────────────────────┤
│ ... 更多 PT_LOAD 段 ...                               │
└───────────────────────────────────────────────────────┘
```

---

## 十四、关键数据结构 — 全局变量一览

```c
// fs/proc/vmcore.c:38-80
static LIST_HEAD(vmcore_list);                         // PT_LOAD 段链表
static char *elfcorebuf;                               // ELF header buffer
static size_t elfcorebuf_sz;                           // ELF header size
static size_t elfcorebuf_sz_orig;                      // 原始 header size
static char *elfnotes_buf;                             // ELF notes buffer
static size_t elfnotes_sz;                             // ELF notes total size
static size_t elfnotes_orig_sz;                        // 末添加设备dump前大小
static u64 vmcore_size;                                // 总文件大小
static struct proc_dir_entry *proc_vmcore;             // proc entry
static DEFINE_MUTEX(vmcore_mutex);                     // 互斥锁
DEFINE_STATIC_SRCU(vmcore_cb_srcu);                    // SRCU for callbacks
static LIST_HEAD(vmcore_cb_list);                      // 回调链表
static bool vmcore_opened;                             // 是否曾被打开
static unsigned int vmcore_open;                       // 当前打开计数
```

这些全局变量共同维护了 `/proc/vmcore` 文件的完整状态。`vmcore_mutex` 保护 `vmcore_list` 和 `vmcore_cb_list` 的修改。`vmcore_cb_srcu` 保护读取路径上的回调遍历。

---

## 十五、总结对比表

| 组件 | 文件 | 关键行 | 角色 |
|---|---|---|---|
| `/proc/vmcore` 创建 | `fs/proc/vmcore.c` | 1736 | `proc_create("vmcore", S_IRUSR, ...)` 创建 proc 文件 |
| 打开操作 | `fs/proc/vmcore.c` | 135 | `open_vmcore()` 管理并发访问 |
| 读取操作 | `fs/proc/vmcore.c` | 430-433 | `read_vmcore()` → `__read_vmcore()` |
| 分段读取 | `fs/proc/vmcore.c` | 333-428 | ELF header → notes → PT_LOAD 三段式 |
| 旧内存读取 | `fs/proc/vmcore.c` | 159-206 | `read_from_oldmem()` 逐页读取旧内核物理内存 |
| 页面拷贝 | `include/linux/crash_dump.h` | 31-32 | `copy_oldmem_page()` 架构特定实现 |
| ELF 头读取 | `fs/proc/vmcore.c` | 225-233 | `elfcorehdr_read()` 从旧内核读取 ELF header |
| 设备 dump | `fs/proc/vmcore.c` | 270-295 | `vmcoredd_copy_dumps()` 额外设备数据 |
| 回调注册 | `fs/proc/vmcore.c` | 84-98 | `register_vmcore_cb()` 模块化内存读取扩展 |
| `is_kdump_kernel()` | `include/linux/crash_dump.h` | 68-71 | 判断是否为 capture kernel |
| `makedumpfile` | 用户空间 | — | 压缩 /proc/vmcore → 保存 dump 文件 |
| `crash` 工具 | 用户空间 | — | 解析 vmcore 进行内核调试 |
| `vmcore-dmesg` | 用户空间 | — | 快速提取 log_buf 中的 panic 消息 |

---

## 下一篇文章

[第三篇：crash 工具加载与内存分析](./03-crash-load.md)
