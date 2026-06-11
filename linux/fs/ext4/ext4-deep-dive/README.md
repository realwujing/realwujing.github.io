# ext4 内核源码深度解析 — 系列目录

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)

## 系列文章

| 篇目 | 标题 | 核心文件 |
|------|------|----------|
| 第一篇 | [磁盘布局 — 超级块、块组描述符与索引节点](./01-disk-layout.md) | `fs/ext4/ext4.h`, `fs/ext4/super.c` |
| 第二篇 | [范围树 — 二叉搜索、插入分裂与合并](./02-extent-tree.md) | `fs/ext4/ext4_extents.h`, `fs/ext4/extents.c` |
| 第三篇 | [日志 — JBD2 句柄、事务提交与快速提交](./03-journal.md) | `fs/ext4/ext4_jbd2.c`, `fs/ext4/fast_commit.c` |
| 第四篇 | [块与索引节点分配 — 多块分配器与 inode 分配策略](./04-alloc.md) | `fs/ext4/mballoc.c`, `fs/ext4/ialloc.c` |
| 第五篇 | [目录与 HTree — 哈希 B 树查找与索引目录](./05-htree.md) | `fs/ext4/namei.c`, `fs/ext4/dir.c` |
| 第六篇 | [在线扩缩容与 FSMAP — 块组扩展与空间映射](./06-resize-fsmap.md) | `fs/ext4/resize.c`, `fs/ext4/fsmap.c` |

## 架构概览

```
┌──────────────────────────────────────────────────────────────────┐
│                          Userspace                               │
│  open() / read() / write() / fsync() / fallocate() / ioctl()    │
└───────────────────────────────┬──────────────────────────────────┘
                                │  syscall
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                     VFS (Virtual File System)                    │
│  struct file_operations, struct inode_operations,                │
│  struct address_space_operations, struct super_operations        │
└───────────────────────────────┬──────────────────────────────────┘
                                │  ext4_sops (super.c:1659)
                                │  ext4_file_operations, ext4_dir_operations
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                         ext4 Core                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │  ext4 inode  │  │  Extent Tree │  │  Directory / HTree   │   │
│  │  operations  │  │  (extents.c) │  │  (namei.c, dir.c)    │   │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘   │
│         │                 │                      │               │
│         ▼                 ▼                      ▼               │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              JBD2 Journal Layer (ext4_jbd2.c)             │   │
│  │         ┌──────────────┐   ┌─────────────────────┐       │   │
│  │         │ Full Commit  │   │ Fast Commit (TLV)   │       │   │
│  │         └──────────────┘   └─────────────────────┘       │   │
│  └──────────────────────────────────────────────────────────┘   │
│         │                                                       │
│         ▼                                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │           Block / Inode Allocator                         │   │
│  │     ┌────────────────┐    ┌──────────────────────┐       │   │
│  │     │  mballoc.c     │    │  ialloc.c            │       │   │
│  │     │  (multi-block) │    │  (inode allocation)  │       │   │
│  │     └────────────────┘    └──────────────────────┘       │   │
│  └──────────────────────────────────────────────────────────┘   │
│         │                                                       │
└─────────┼───────────────────────────────────────────────────────┘
          │
          ▼
┌──────────────────────────────────────────────────────────────────┐
│                     Block Layer                                  │
│  submit_bio() → block device driver → disk                       │
└──────────────────────────────────────────────────────────────────┘
```

## 前置知识

建议先阅读 [VFS 内核源码深度解析](../vfs-deep-dive/) 系列，了解以下概念：

- `struct super_block` — 超级块对象，代表一个已挂载的文件系统
- `struct inode` — VFS 索引节点，文件系统通过 `ext4_alloc_inode` 内嵌 `ext4_inode_info`
- `struct file` / `struct dentry` — 文件对象与目录项缓存
- `struct address_space` — 页缓存与 `writepage` / `read_folio` 操作
- `struct buffer_head` — 块缓存，ext4 大量使用 `buffer_head` 接口与 JBD2 交互
- 文件系统挂载流程：`get_tree_bdev()` → `fill_super()` → 设置 `s_op`

## 缩写速查

| 缩写 | 全称 | 说明 |
|------|------|------|
| JBD2 | Journaling Block Device 2 | ext4 使用的日志层，`fs/jbd2/` |
| GDT | Group Descriptor Table | 块组描述符表 |
| HTree | Hash Tree | 目录项索引 B 树，替代线性扫描 |
| TLV | Tag-Length-Value | Fast Commit 日志条目格式 |
| mballoc | Multi-Block Allocator | 多块分配器，基于 buddy 位图 |
| sbi | SUPER_BLOCK_INFO | `struct ext4_sb_info`，ext4 私有超级块 |
| flex_bg | Flexible Block Group | 多组聚合的弹性块组 |
| BG | Block Group | 块组，ext4 空间管理基本单元 |
| FSMAP | Filesystem Space Map | `FS_IOC_GETFSMAP` ioctl 实现 |
