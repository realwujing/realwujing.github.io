# 第3篇：crash 工具加载 vmcore — ELF 解析与初始化

> crash 源码：`netdump.c` `main.c` | vmcore 源码：`fs/proc/vmcore.c`

系列目录：[vmcore 深度解析系列](./README.md)

---

## 1. crash 工具的启动 — 三种 vmcore 格式的识别

crash 工具支持三种 vmcore 来源，在 `main.c` 中按优先级判断：

```c
// main.c:444 — 第一步: 本地 kdump 压缩格式
if (is_kdump(pc->dumpfile, KDUMP_LOCAL)) {
    // kdump compressed format (.dmp files)
}

// main.c:545 — 第二步: ELF vmcore (netdump/kdump)
else if (is_netdump(argv[optind], NETDUMP_LOCAL)) {
    // ELF vmcore format from /proc/vmcore or disk
}

// main.c:607 — 第三步: diskdump 压缩格式
else if (is_diskdump(argv[optind])) {
    // diskdump LKCD compressed format
}
```

**三种格式的 ELF 处理**：
- ELF vmcore (netdump/kdump): 直接解析 ELF header + program headers + notes
- kdump 压缩格式: 解压后还是标准 ELF，走同样的 netdump 路径
- diskdump LKCD 格式: 走独立的 `diskdump.c` 路径，压缩块逐块解压

**核心结论：所有路径最终都转换成 ELF 格式在内存中操作。**

---

## 2. is_netdump — ELF vmcore 的入口

```c
// netdump.c:119 — 判断是否是 ELF vmcore 并完整解析
is_netdump(char *file, ulong source_query)
{
    struct vmcore_data *nd = &vmcore_data;
    int fd, size;
    char *eheader, *sect0;

    // ① 打开 vmcore 文件
    fd = open(file, O_RDONLY);

    // ② ★ 读取和重排 ELF header
    size = resize_elf_header(fd, file, &eheader, &sect0, format);  // line 386

    // ③ 判断 ELF 类别 (32-bit or 64-bit)
    if (eheader[EI_CLASS] == ELFCLASS32) {
        nd->elf32 = (Elf32_Ehdr *)eheader;

        // ★ 解析 ELF header
        dump_Elf32_Ehdr(nd->elf32);                    // line 411
        // ★ 解析 PT_NOTE segment
        dump_Elf32_Phdr(nd->notes32, ELFREAD);         // line 412

        // ★ 解析每个 PT_LOAD segment
        for (int i = 0; i < nd->num_load; i++) {
            dump_Elf32_Phdr(nd->load32 + i, ELFSTORE + i);  // line 414
        }

        // ★ 解析 ELF notes (VMCOREINFO + per-CPU registers)
        for (Elf32_Off offset32 = nd->notes32->p_offset; ...) {
            if (!(len = dump_Elf32_Nhdr(offset32, ELFSTORE)))  // line 417
                break;
            offset32 += len;
        }
    } else {
        // ★ 64-bit ELF: 完全相同的流程
        nd->elf64 = (Elf64_Ehdr *)eheader;
        dump_Elf64_Ehdr(nd->elf64);                    // line 449
        dump_Elf64_Phdr(nd->notes64, ELFREAD);         // line 450
        for (int i = 0; i < nd->num_load; i++)
            dump_Elf64_Phdr(nd->load64 + i, ELFSTORE + i);  // line 452
        for (Elf64_Off offset64 = nd->notes64->p_offset; ...)
            if (!(len = dump_Elf64_Nhdr(offset64, ELFSTORE)))  // line 455
                break;
    }

    // ★ 注册 VMCOREINFO 查询回调
    pc->read_vmcoreinfo = vmcoreinfo_read_string;      // line 465

    return TRUE;
}
```

---

## 3. resize_elf_header — ELF header 的读取和内存布局

```c
// netdump.c:496 — 读取 ELF header 并为其在 crash 工具内存中重新分配空间
static size_t resize_elf_header(int fd, char *file,
                                char **eheader_ptr, char **sect0_ptr, ...)
{
    Elf64_Ehdr ehdr64;
    Elf32_Ehdr ehdr32;

    // ① 读 ELF magic (前 4 字节)
    read(fd, &ehdr32, sizeof(ehdr32));

    // ② 判断 32/64-bit
    if (ehdr32.e_ident[EI_CLASS] == ELFCLASS64) {
        // ★ 完整读取 64-bit ELF header
        memcpy(&ehdr64, &ehdr32, sizeof(ehdr32));        // 复用已读的前 64 字节
        read(fd, (char *)&ehdr64 + sizeof(ehdr32),
             sizeof(ehdr64) - sizeof(ehdr32));           // 读剩余部分

        // ★ 读取所有 program headers (e_phnum 个, 每个 e_phentsize 字节)
        // 分配新内存: header + e_phnum * sizeof(Elf64_Phdr)
        memsize = ehdr64.e_phoff + ehdr64.e_phnum * sizeof(Elf64_Phdr);
        *eheader_ptr = malloc(memsize);
        lseek(fd, 0, SEEK_SET);
        read(fd, *eheader_ptr, memsize);
    } else {
        // 32-bit: 同样的流程
        memsize = ehdr32.e_phoff + ehdr32.e_phnum * sizeof(Elf32_Phdr);
        *eheader_ptr = malloc(memsize);
        lseek(fd, 0, SEEK_SET);
        read(fd, *eheader_ptr, memsize);
    }
}
```

**为什么需要 "resize"**：vmcore 文件中的 ELF header 包含完整的 program header 表，但 crash 工具需要额外空间存储自己的元数据。所以它把整个 ELF header + program headers 读到一块连续内存，再把 program header 指针数组 (notes/load32/load64) 指定为这块内存中的对应位置。

---

## 4. dump_Elf64_Ehdr — 解析 64-bit ELF header

```c
// netdump.c:1532 — 解析 64-bit ELF header
static void dump_Elf64_Ehdr(Elf64_Ehdr *elf)
{
    struct vmcore_data *nd = &vmcore_data;

    nd->num_notes = 0;
    nd->num_load = 0;

    // ★ 遍历所有 program headers
    for (int i = 0; i < elf->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)elf + elf->e_phoff
                                          + i * elf->e_phentsize);

        switch (phdr->p_type) {
        case PT_NOTE:
            nd->notes64 = phdr;              // 第一个 PT_NOTE segment
            break;
        case PT_LOAD:
            // ★ 存储每个 PT_LOAD segment 的虚拟/物理/文件偏移
            nd->load64[nd->num_load] = phdr;
            nd->num_load++;
            break;
        }
    }
}
```

**解析结果**：
- `nd->notes64` → 指向 VMCOREINFO + 每颗 CPU 的 `elf_prstatus` 所在的 PT_NOTE segment
- `nd->load64[]` → 每个 PT_LOAD segment (物理内存的映射)——告诉 crash 工具 "虚拟地址 X 的物理内存位于 vmcore 文件的偏移 Y"
- `nd->num_load` → PT_LOAD 段的数量（通常 3-5 个，对应内核代码/数据、direct mapping、vmalloc 等区域）

---

## 5. dump_Elf64_Phdr — 存储 segment 信息

```c
// netdump.c:450
static void dump_Elf64_Phdr(Elf64_Phdr *phdr, int idx)
{
    struct vmcore_data *nd = &vmcore_data;

    if (idx == ELFREAD) {
        // ★ 读模式: 保存 PT_NOTE 的引用
        nd->notes64 = phdr;
    } else {
        // ★ 存储模式: 保存 PT_LOAD 到 load64 数组
        // idx = ELFSTORE + i (i = 0, 1, 2, ...)
        nd->load64[idx - ELFSTORE] = phdr;
    }
}
```

---

## 6. dump_Elf64_Nhdr — 解析 ELF notes

```c
// netdump.c:38 — 读取单个 ELF note header 并推进 offset
static size_t dump_Elf64_Nhdr(Elf64_Off offset, int store_flag)
{
    Elf64_Nhdr note;
    char *buf;

    // ① 读 ELF note header (4 bytes, 存储 n_namesz 和 n_descsz)
    read_at_offset(pc->dfd, offset, &note, sizeof(note));

    // ② 计算 note 的总长度 (对齐到 4 字节边界)
    size = sizeof(Elf64_Nhdr) +
           round_up(note.n_namesz, 4) +   // 名称 "VMCOREINFO" or "CORE"
           round_up(note.n_descsz, 4);    // 实际数据

    if (store_flag == ELFSTORE) {
        // ★ 读取完整的 note 数据
        buf = malloc(size);
        read_at_offset(pc->dfd, offset, buf, size);

        // 如果是 VMCOREINFO → 提供给 vmcoreinfo_read_string 查询
        // 如果是 CORE → 每个是 per-CPU elf_prstatus
        // 如果是 QEMU → 虚拟化特定的额外信息
    }

    return size;  // 调用者在循环中 offset += size 读取下一个 note
}
```

**ELF note 的类型**：

| n_type | 名称 | 内容 | 平均大小 |
|--------|------|------|---------|
| 0 | "VMCOREINFO" | 内核关键常量 (PAGE_SHIFT, SYMBOL, etc.) | ~4KB |
| NT_PRSTATUS | "CORE" | per-CPU `elf_prstatus` (寄存器: rip, rsp, flags, segments) | ~336B per CPU |
| NT_PRPSINFO | "CORE" | per-CPU 进程基本信息 (pid, comm) | ~136B per CPU |
| NT_TASKSTRUCT | "CORE" | `task_struct` 的完整副本 (qemu-only) | ~KB |

---

## 7. crash 初始化后的数据结构

加载完成后 crash 工具维护以下全局状态：

```c
// defs.h:7299 — crash tool 内部 struct
struct vmcore_data {
    union {
        Elf32_Ehdr *elf32;              // 32-bit ELF header
        Elf64_Ehdr *elf64;              // 64-bit ELF header
    };
    union {
        Elf32_Phdr *notes32, *load32;   // 32-bit program headers
        Elf64_Phdr *notes64, *load64;   // 64-bit program headers
    };
    int num_load;                        // PT_LOAD segment 的数量
    int num_notes;                       // PT_NOTE ent 的数量
    ulong flags;                         // NETDUMP_LOCAL | KDUMP_LOCAL | etc.
    char *vmcoreinfo;                   // 完整的 VMCOREINFO 字符串 (4KB text)
};
```

---

## 8. 总结

| 步骤 | 函数 | 行号 | 作用 |
|------|------|------|------|
| 格式判断 | `is_netdump` | netdump.c:119 | 识别 ELF vmcore |
| ELF header 读取 | `resize_elf_header` | netdump.c:496 | 读取 32/64-bit ELF header + 整个 program header 表 |
| header 解析 | `dump_Elf64_Ehdr` | netdump.c:1532 | 遍历 program headers → 分类为 PT_NOTE / PT_LOAD |
| segment 存储 | `dump_Elf64_Phdr` | netdump.c:450 | 存储每个 segment 的 offset/vaddr/paddr |
| note 解析 | `dump_Elf64_Nhdr` | netdump.c:38 | 提取 VMCOREINFO + per-CPU elf_prstatus |
| 回调注册 | `pc->read_vmcoreinfo` | netdump.c:465 | 注册 VMCOREINFO 字符串查询函数 |

## 下一篇文章

→ [第4篇：crash 工具的 readmem — KVADDR/PHYSADDR/UVADDR 三种地址模式](./04-crash-readmem.md)
