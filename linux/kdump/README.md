# vmcore 深度解析系列 — 内核与 crash 工具的协作全链路

> 📌 内核源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` | crash 源码：crash-8.0.5

---

## 目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [内核篇: panic → kexec → vmcore 生成](./01-kernel-panic-kexec.md) | `crash_save_cpu`、`machine_crash_shutdown`、`vmcoreinfo`、ELF 头生成 |
| 2 | [capture kernel 启动与 /proc/vmcore](./02-capture-kernel.md) | capture kernel 启动路径、`fs/proc/vmcore.c` 完整实现、用户态读取 vmcore 的内核路径 |
| 3 | [crash 工具加载 vmcore — ELF 解析与初始化](./03-crash-load.md) | `is_netdump` 流程、ELF 头解析、PT_NOTE 和 PT_LOAD 段的处理、`resize_elf_header` |
| 4 | [crash 工具的内存读取 — readmem 与 ptov 实现](./04-crash-readmem.md) | `readmem` 核心函数完整分析、KVADDR/PHYSADDR/UVADDR 的分发逻辑、`uvtop` 页表遍历 |
| 5 | [crash 工具的 VMCOREINFO 键值数据库](./05-vmcoreinfo.md) | `vmcoreinfo_read_string`、`kernel_init` 启动时如何查询内核版本/PAGE_OFFSET/log_buf 等关键信息 |

## 前置知识

- 了解 kdump/kexec 基本概念和 `crashkernel=` 参数
- 掌握 ELF 文件格式基础 (Ehdr/Phdr/Nhdr)
- 了解 Linux 内核的 panic 机制
- 可搭配 coredump 分析经验更佳

## 架构总览

```
正常内核 panic()
    │
    ▼
crash_save_cpu (per-CPU registers) → crash_save_vmcoreinfo (关键常量) → machine_kexec
    │
    ▼
capture kernel 启动
    │
    ▼
/proc/vmcore 暴露 ELF 段 (VM core 内容) → 用户态 crash 工具
    │
    ▼
crash 工具: is_netdump → resize_elf_header → dump_Elf64_Ehdr/Phdr → readmem

                    crash 工具通过 VMCOREINFO 键值对获取内核版本/物理基址/
                    日志缓冲区地址/PAGE_OFFSET 等关键信息

→ bt (栈回溯) / log (dmesg) / kmem -i (内存统计) / ps (进程列表)
```
