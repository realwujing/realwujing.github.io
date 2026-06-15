# 第5篇：crash 工具的 VMCOREINFO 键值数据库

> crash 源码：`kernel.c` `netdump.c` | vmcore 源码：`kernel/vmcore_info.c`

系列目录：[vmcore 深度解析系列](./README.md)

---

## 1. VMCOREINFO 是什么

VMCOREINFO 是崩溃内核在 panic 之前保存在**保留物理页**中的一段文本。它包含 crash 工具运行所需的所有关键信息——不需要 vmlinux 带完整 debug 信息（数百 MB），只需要这 4KB。

```
/etc/kdump/scripts/local/makedumpfile ...
  → 读取 /proc/vmcore
  → 解压 → 保存为 /var/crash/<timestamp>/vmcore

crash /usr/lib/debug/lib/modules/$(uname -r)/vmlinux /var/crash/vmcore
  → crash 读取 vmcore → 从中提取 ELF PT_NOTE "VMCOREINFO" → 获取内核信息
```

---

## 2. VMCOREINFO 在 vmcore 文件中的位置

```
ELF Header
  ...
  
  Program Headers:
    PT_NOTE (offset=0x1000, filesz=4KB)
      ├─ note: "VMCOREINFO\0" — 内核关键常量
      │   OSRELEASE=6.6.0-1.ctl4
      │   PAGESIZE=4096
      │   SYMBOL(log_buf)=ffff888000000000
      │   ...
      ├─ note: "CORE\0" — CPU0 的 elf_prstatus
      ├─ note: "CORE\0" — CPU1 的 elf_prstatus
      └─ ...

    PT_LOAD (物理内存段)
      p_offset=0x100000 (vmcore 文件中的位置)
      p_paddr=0x10000000 (物理地址)
      p_vaddr=0xffff888010000000 (内核虚拟地址)
      p_filesz=0x20000000 (512MB)
```

---

## 3. 内核侧：crash_save_vmcoreinfo 生成 VMCOREINFO

```c
// kernel/vmcore_info.c:84-93
void crash_save_vmcoreinfo(void)
{
    // 第一行：panic 的时间戳
    vmcoreinfo_append_str("CRASHTIME=%lld\n", ktime_get_real_seconds());

    // ★ 关键架构常量
    VMCOREINFO_NUMBER(PAGE_SHIFT);          // 12
    VMCOREINFO_NUMBER(PAGE_SIZE);           // 4096
    VMCOREINFO_NUMBER(PAGE_OFFSET);         // 直接映射的虚拟基址

    // ★ SLUB 分配器常量 — crash 需要读 kmalloc caches
    VMCOREINFO_NUMBER(KMALLOC_MIN_SIZE);    // 最小分配大小
    VMCOREINFO_NUMBER(KMALLOC_SHIFT_HIGH);  // 大分配的对齐

    // ★ 内核链接偏移 (KASLR)
    VMCOREINFO_NUMBER(KERNELOFFSET);        // 随机化偏移

    // ★ 内存布局常量
    VMCOREINFO_NUMBER(VMALLOC_START);       // vmalloc 区域开始
    VMCOREINFO_NUMBER(VMEMMAP_START);       // struct page 数组开始

    // ★ 关键全局符号
    vmcoreinfo_append_str("SYMBOL(init_uts_ns)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(node_online_map)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(swapper_pg_dir)=%lx\n", ...);

    // ★ printk 日志缓冲区信息
    vmcoreinfo_append_str("SYMBOL(log_buf)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(log_end)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(log_buf_len)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(logged_chars)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(log_first_idx)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(log_next_idx)=%lx\n", ...);
}
```

**VMCOREINFO 文本格式示例** (前 20 行)：

```
CRASHTIME=1718428800
PAGESIZE=4096
SYMBOL(init_uts_ns)=ffffffff81c3a2a0
SYMBOL(node_online_map)=ffffffff81c3a300
SYMBOL(swapper_pg_dir)=ffffffff81c3b000
SYMBOL(_stext)=ffffffff81000000
SYMBOL(vmlist)=ffffffff81c40128
SYMBOL(mem_section)=ffff888000000000
LENGTH(mem_section)=4096
SIZE(mem_section)=16
OFFSET(mem_section.section_mem_map)=0
SIZE(page)=64
SIZE(pglist_data)=32768
SIZE(zone)=1536
SIZE(free_area)=16
SIZE(list_head)=16
SIZE(nodemask_t)=16
OFFSET(page.flags)=0
OFFSET(page._refcount)=52
...
```

---

## 4. crash 工具侧：vmcoreinfo_read_string

```c
// netdump.c:52 — 被注册为 pc->read_vmcoreinfo (line 465)
static char *vmcoreinfo_read_string(const char *key)
{
    struct vmcore_data *nd = &vmcore_data;
    char *vmcoreinfo = nd->vmcoreinfo;   // ★ 完整的 4KB 文本

    // ① 搜索 "KEY=" 前缀
    char *start = strstr(vmcoreinfo, key);
    if (!start) return NULL;
    start += strlen(key) + 1;            // 跳过 "KEY="

    // ② 提取值直到 '\n'
    char *end = strchr(start, '\n');
    if (!end) return NULL;

    // ③ 返回 malloca 分配的结果
    char *result = malloc(end - start + 1);
    strncpy(result, start, end - start);
    result[end - start] = '\0';
    return result;
}
```

---

## 5. kernel_init — crash 启动时如何查询 VMCOREINFO

```c
// kernel.c — 所有 vmcoreinfo_read_string 调用点

static void kernel_init(void)
{
    char *string;

    // ① ★ 内核版本 — 最关键的查询
    if ((string = pc->read_vmcoreinfo("OSRELEASE"))) {     // line 11026
        pc->kernel_version = string;
        // "6.6.0-1.ctl4"
    }

    // ② ★ 页大小
    if ((string = pc->read_vmcoreinfo("PAGESIZE"))) {      // line 11041
        machdep->pagesize = atol(string);                   // 4096 or 65536
    }

    // ③ ★ printk 日志缓冲区 — 5 个日志相关的查询
    // log_buf 的虚拟地址
    if ((string = pc->read_vmcoreinfo("SYMBOL(log_buf)"))) { // line 11050
        machdep->kvbase = htol(string, QUIET, NULL);
    }
    // log_end 的虚拟地址
    if ((string = pc->read_vmcoreinfo("SYMBOL(log_end)"))) { // line 11057
        machdep->log_end = htol(string, QUIET, NULL);
    }
    // log_buf_len — 缓冲区大小
    if ((string = pc->read_vmcoreinfo("SYMBOL(log_buf_len)"))) { // line 11064
        machdep->log_buf_len = htol(string, QUIET, NULL);
    }
    // logged_chars — 实际日志的字节数, 含 wrap
    if ((string = pc->read_vmcoreinfo("SYMBOL(logged_chars)"))) { // line 11071
        machdep->logged_chars = htol(string, QUIET, NULL);
    }
    // log_first_idx / log_next_idx — 环形缓冲区的读写指针
    if ((string = pc->read_vmcoreinfo("SYMBOL(log_first_idx)"))) { // line 11078
        machdep->log_first_idx = htol(string, QUIET, NULL);
    }
    if ((string = pc->read_vmcoreinfo("SYMBOL(log_next_idx)"))) { // line 11085
        machdep->log_next_idx = htol(string, QUIET, NULL);
    }

    // ④ ★ 物理基址
    if ((string = pc->read_vmcoreinfo("SYMBOL(phys_base)"))) { // line 11092
        machdep->phys_base = htol(string, QUIET, NULL);
    }

    // ⑤ ★ 内核文本起始地址
    if ((string = pc->read_vmcoreinfo("SYMBOL(_stext)"))) { // line 11099
        machdep->stext = htol(string, QUIET, NULL);
    }

    // ⑥ ★ printk 日志的时间戳字段偏移 (ts_nsec)
    if ((string = pc->read_vmcoreinfo("OFFSET(log.ts_nsec)"))) { // line 11106
        machdep->log_ts_offset = atoi(string);
    } else if ((string = pc->read_vmcoreinfo("OFFSET(printk_log.ts_nsec)"))) { // line 11112
        machdep->log_ts_offset = atoi(string);
    }

    // ⑦ ★ printk 日志的长度/文本偏移
    if ((string = pc->read_vmcoreinfo("OFFSET(log.len)"))) { // line 11119
        machdep->log_len_offset = atoi(string);
    }
    if ((string = pc->read_vmcoreinfo("OFFSET(log.text_len)"))) { // line 11132
        machdep->log_text_len_offset = atoi(string);
    }
}
```

---

## 6. VMCOREINFO 的五种键类型

| 前缀 | 格式 | 示例 | crash 如何解析 |
|------|------|------|--------------|
| **直接键** | `KEY=VALUE` | `PAGESIZE=4096` | `strtoul(value)` 或 `atol(value)` |
| **SYMBOL** | `SYMBOL(NAME)=ADDR` | `SYMBOL(log_buf)=ffff888000000000` | `htol(value)` 转为 unsigned long |
| **OFFSET** | `OFFSET(STRUCT.FIELD)=N` | `OFFSET(log.len)=24` | `atoi(value)` |
| **SIZE** | `SIZE(TYPE)=N` | `SIZE(page)=64` | `atoi(value)` |
| **LENGTH** | `LENGTH(NAME)=N` | `LENGTH(mem_section)=4096` | `atoi(value)` |

**注意**：VMCOREINFO 也支持 `NUMBER` 前缀——`NUMBER(PAGE_SHIFT)=12`——它等同于直接键，只是语法略有不同。

---

## 7. 为什么 VMCOREINFO 存在而不直接包含 vmlinux 的 debug 信息

| | VMCOREINFO | vmlinux with debuginfo |
|---|---|---|
| **大小** | ~4KB | ~500MB-2GB |
| **存储位置** | 永远在 vmcore 文件内的 ELF note | 需要在 crash 工具命令行中单独指定 |
| **包含什么** | 只包含关键的 SYMBOL/OFFSET/SIZE/LENGTH | 包含完整的 DWARF 调试信息 |
| **如何获取** | crash 工具通过 `read_vmcoreinfo` 查询 | crash 工具通过 gdb 的 DWARF 解析 |
| **内核版本匹配**| 自动匹配 (VMCOREINFO 和 vmcore 在同一个 kernel 版本) | 必须和崩溃内核完全相同 |
| **KASLR 处理** | VMCOREINFO 包含 `KERNELOFFSET` 等 KASLR 偏移信息 | 不需要 |

**VMCOREINFO 是 crash 工具在不包含 vmlinux 的情况下工作的最小必要信息集。如果提供了 vmlinux，crash 会获得更详细的信息（源码行号、函数参数、复杂类型等），但 VMCOREINFO 足以完成基本的栈回溯、内存统计和内核日志提取。**

---

## 8. 系列结语

5 篇文章从内核 panic 触发 kdump，到 capture kernel 启动 `/proc/vmcore` 暴露 ELF 文件，再到 crash 工具如何通过 `resize_elf_header` + `dump_Elf64_Ehdr/Phdr/Nhdr` 解析 ELF 结构，再到 `readmem` 三种地址模式的内核/用户/物理内存读取，最终到 VMCOREINFO 键值数据库如何让 crash 工具在无 vmlinux debug info 的情况下完成分析。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-kernel-panic-kexec | panic → crash_save_cpu → VMCOREINFO → machine_kexec | `panic.c`, `crash.c:56-145`, `vmcore_info.c:84-155` |
| 02-capture-kernel | capture kernel 启动 + `/proc/vmcore` 暴露 | `fs/proc/vmcore.c` |
| 03-crash-load | crash 工具的 ELF 解析与初始化 | crash `netdump.c:119-465,496+,1379+,1532+` |
| 04-crash-readmem | readmem 三种地址模式 + kvtop/uvtop 页表遍历 | crash `memory.c:2307+` |
| 05-vmcoreinfo | VMCOREINFO 键值数据库 + kernel_init 启动查询 | crash `kernel.c:11026-11132`, `kernel/vmcore_info.c:84-93` |
