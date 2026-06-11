# VFS 内核源码深度解析

> VFS (Virtual File System Switch) 是 Linux 内核中承上启下的核心层——对上向用户态暴露统一的文件和目录操作接口，对下衔接具体文件系统实现。本系列从内核源码出发，逐层剖析 VFS 的六大核心模块。

📌 **源码**：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf

---

## 架构概览

```
用户态                            内核态
┌───────────┐                ┌─────────────────────────────────────────┐
│  open()   │ ───────────►   │         VFS (Virtual Filesystem)        │
│  read()   │                │                                         │
│  write()  │                │  ┌─────────────────────────────────┐    │
│  close()  │                │  │         path lookup             │    │
└───────────┘                │  │    (第4篇：路径名查找)            │    │
                             │  │         dir → dentry            │    │
                             │  └──────────────┬──────────────────┘    │
                             │                 │                       │
                             │  ┌──────────────▼──────────────────┐    │
                             │  │         dentry cache             │    │
                             │  │    (第1篇：目录项缓存)            │    │
                             │  │    dcache.h:93 struct dentry     │    │
                             │  └──────────────┬──────────────────┘    │
                             │                 │ d_inode               │
                             │  ┌──────────────▼──────────────────┐    │
                             │  │         inode lifecycle          │    │
                             │  │    (第2篇：inode 生命周期)        │    │
                             │  │    fs.h:767 struct inode         │    │
                             │  └──────────────┬──────────────────┘    │
                             │                 │ i_mapping             │
                             │  ┌──────────────▼──────────────────┐    │
                             │  │    page cache + buffer IO        │    │
                             │  │    (第3篇：页缓存与缓冲区IO)       │    │
                             │  │    fs.h:473 address_space        │    │
                             │  └──────────────┬──────────────────┘    │
                             │                 │                       │
                             │  ┌──────────────▼──────────────────┐    │
                             │  │         block layer              │    │
                             │  │         bio → disk               │    │
                             │  └─────────────────────────────────┘    │
                             │                                         │
                             │  ┌─────────────────────────────────┐    │
                             │  │      file ops & fd table        │    │
                             │  │    (第5篇：文件操作与fd表)         │    │
                             │  └─────────────────────────────────┘    │
                             │                                         │
                             │  ┌─────────────────────────────────┐    │
                             │  │     mount & superblock          │    │
                             │  │    (第6篇：挂载与超级块)          │    │
                             │  └─────────────────────────────────┘    │
                             └─────────────────────────────────────────┘
```

## 系列目录

| 篇目 | 标题 | 核心文件 |
|------|------|----------|
| [01](./01-dentry.md) | Dentry Cache — 目录项缓存与 RCU 无锁查找 | `include/linux/dcache.h`, `fs/dcache.c` |
| [02](./02-inode.md) | Inode 生命周期 — 创建、查找、引用计数与回收 | `include/linux/fs.h`, `fs/inode.c` |
| [03](./03-page-cache.md) | 页缓存与缓冲区 I/O — 从 folio 到 bio | `include/linux/fs.h`, `mm/filemap.c`, `fs/buffer.c` |
| [04](./04-path-lookup.md) | 路径名查找 — 从 open() 到 inode | `fs/namei.c` |
| [05](./05-file-ops.md) | 文件操作与文件描述符表 — 进程视角 | `include/linux/fs.h`, `fs/file_table.c`, `fs/open.c` |
| [06](./06-mount.md) | 挂载与超级块 — 文件系统实例的生命周期 | `include/linux/fs.h`, `fs/super.c`, `fs/namespace.c` |

## 阅读前提

- 理解基本的文件系统概念（目录、inode、数据块）
- 了解 Linux 内核基础（进程模型、系统调用、锁机制）
- 推荐先阅读 `Documentation/filesystems/vfs.rst`

---

所有文章均为原创，基于 Linux 内核主线源码逐行分析，标记实际行号。欢迎提交 issue 或 PR 纠正错误。
