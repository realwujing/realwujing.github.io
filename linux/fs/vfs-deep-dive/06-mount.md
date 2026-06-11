# 第六篇：超级块与挂载 — 从 register_filesystem 到用户态看见文件

> 源码：fs/super.c (2272 行), fs/namespace.c (6531 行), fs/filesystems.c ｜ 头文件：include/linux/fs/super_types.h, include/linux/fs.h

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 6.1 概述：文件系统如何变得"可见"

在之前的五篇文章中，我们讨论了 dentry、inode、页缓存、路径查找和文件操作——这些构成了 VFS 已挂载文件系统的运行态。但文件系统在被挂载前，只是一个在内核中注册的名字。本篇将追踪文件系统从注册到用户可见的完整路径：

```
register_filesystem("ext4")
    │
    ▼
用户可以执行: mount -t ext4 /dev/sda1 /mnt
    │
    ▼
内核创建 struct super_block
    │
    ▼
ext4 文件系统从磁盘读取超级块数据
    │
    ▼
root dentry 挂在 /mnt 目录树上
    │
    ▼
用户: ls /mnt → 看到文件!
```

## 6.2 struct super_block：文件系统的内核根基

`struct super_block` 定义在 `include/linux/fs/super_types.h:132`，是文件系统在内核中的"元对象"——每个挂载的文件系统有且仅有一个超级块：

```c
// super_types.h:132 开始
struct super_block {
    struct list_head        s_list;           // line 133  全局 super_blocks 链表节点
    dev_t                   s_dev;            // line 134  块设备号 (搜索索引)
    unsigned char           s_blocksize_bits; //          块大小 (位)
    unsigned long           s_blocksize;      // line 136  块大小 (字节)
    loff_t                  s_maxbytes;       // line 137  最大文件大小
    struct file_system_type *s_type;          // line 138  文件系统类型
    const struct super_operations *s_op;      // line 139  超级块操作表
    const struct dquot_operations *dq_op;
    const struct quotactl_ops *s_qcop;
    const struct export_operations *s_export_op;
    unsigned long           s_flags;          //          挂载标志
    unsigned long           s_iflags;         //          内部 SB_I_* 标志
    unsigned long           s_magic;          // line 145  幻数 (如 EXT4_SUPER_MAGIC=0xEF53)
    struct dentry           *s_root;          // line 146  文件系统根 dentry
    struct rw_semaphore     s_umount;         // line 147  卸载保护读写信号量
    int                     s_count;          //          使用计数
    atomic_t                s_active;         // line 149  活跃引用计数
    // ...
    struct hlist_bl_head    s_roots;          // line 165  NFS 备用根 dentry
    struct mount            *s_mounts;        // line 166  挂载实例链表
    struct block_device     *s_bdev;          // line 167  块设备
    struct file             *s_bdev_file;     // line 168  块设备文件
    struct backing_dev_info *s_bdi;
    // ...
    struct sb_writers       s_writers;        // line 175  freeze 写者计数
    void                    *s_fs_info;       // line 182  文件系统私有数据
    u32                     s_time_gran;      // line 185  时间戳粒度 (ns)
    time64_t                s_time_min;       //          最早可表示时间
    time64_t                s_time_max;       //          最晚可表示时间
    // ...
};
```

**关键字段的设计意图**：

- **`s_list`**（结构体第一个字段）：挂接到全局 `super_blocks` 链表（`super.c:46`），受 `sb_lock` 自旋锁保护。这个全局链表用于 `sync_filesystems`（sync 系统调用）遍历所有超级块。
- **`s_umount`**：读写信号量，保护超级块的整个生命周期。挂载时持有写锁初始化结构，卸载时持有写锁销毁结构，正常操作期间持有读锁。
- **`s_root`**：指向文件系统根目录的 dentry。这是"可见性"的锚点——通过这个 dentry，整个文件系统树可被遍历。
- **`s_active`**（原子计数器）：与 `s_count` 配合跟踪活跃引用。卸载时必须降为 0。
- **`s_fs_info`**：`void *` 类型，文件系统私有数据。ext4 将其转换为 `struct ext4_sb_info *`，包含 inode 表位置、块组描述符等信息。这是文件系统从超级块中分离出私有数据的标准模式。
- **`s_bdev`**：对于基于块设备的文件系统，指向 `struct block_device`。

### 6.2.1 超级块与 dentry 树的关系

```
┌──────────────────────────────────────────────────────┐
│ struct super_block (一个挂载点一个)                    │
│                                                      │
│  s_root ───────→ /mnt           ← 根 dentry          │
│                   │                                  │
│                   ├── home/                          │
│                   │    ├── alice/                    │
│                   │    └── bob/                      │
│                   ├── etc/                           │
│                   └── var/                           │
│                                                      │
│  s_bdev  → /dev/sda1         ← 底层块设备            │
│  s_fs_info → ext4_sb_info    ← 文件系统私有元数据    │
│  s_op → ext4_sops            ← 超级块操作表          │
└──────────────────────────────────────────────────────┘
```

每个 dentry 都通过 `dentry->d_sb` 反向指向所属的超级块：

```
dentry->d_sb → super_block
dentry->d_inode → inode   → inode->i_sb → super_block
                          → inode->i_op → 文件系统提供的 inode 操作
```

## 6.3 struct super_operations：超级块操作表

`struct super_operations` 定义在 `include/linux/fs/super_types.h:83`，是文件系统必须实现的核心操作集：

```c
struct super_operations {  // super_types.h:83
    struct inode *(*alloc_inode)(struct super_block *sb);     // line 84
    void (*destroy_inode)(struct inode *inode);               // line 85
    void (*evict_inode)(struct inode *inode);                 // line 90
    void (*put_super)(struct super_block *sb);                // line 91
    int  (*sync_fs)(struct super_block *sb, int wait);        // line 92
    int  (*freeze_super)(struct super_block *, enum freeze_holder, const void *); // line 93
    int  (*freeze_fs)(struct super_block *sb);                // line 95
    int  (*thaw_super)(struct super_block *, enum freeze_holder, const void *); // line 96
    int  (*statfs)(struct dentry *, struct kstatfs *);        // line 99
    void (*umount_begin)(struct super_block *sb);             // line 100
    int  (*show_options)(struct seq_file *, struct dentry *); // line 102
    int  (*show_devname)(struct seq_file *, struct dentry *);
};
```

**各回调的触发时机**：

| 回调 | 触发场景 | ext4 实现 |
|------|---------|-----------|
| `alloc_inode` | 创建新 inode 时分配内存 | 分配 `ext4_inode_info`（扩展结构） |
| `evict_inode` | inode 最后一个引用释放且无链接 | 写入元数据，释放数据块 |
| `put_super` | 卸载时释放超级块 | 释放 `ext4_sb_info`，关闭日志 |
| `sync_fs` | sync/fsync 系统调用 | 提交日志事务 |
| `statfs` | statfs/df 命令 | 返回可用块/总块数 |
| `umount_begin` | 强制卸载 (MNT_FORCE) | 中止所有待处理操作 |
| `show_options` | mount 选项显示 (/proc/mounts) | 输出 "data=ordered,errors=remount-ro" |
| `freeze_super` / `freeze_fs` | fsfreeze ioctl | 停止写操作，准备快照 |
| `thaw_super` / `unfreeze_fs` | fsfreeze 解冻 | 恢复写操作 |

**`alloc_inode` 的特殊性**：VFS 的 `inode` 是固定大小的，但文件系统需要附加私有数据（如 ext4 的 `ext4_inode_info`）。文件系统通过在 `alloc_inode` 中分配一个更大的结构体并嵌入 `struct inode` 来解决：

```
ext4 在 alloc_inode 中:
  ┌───────────────────────────┐
  │ struct ext4_inode_info    │  ← 分配这个 (更大)
  │  ┌─────────────────────┐ │
  │  │ struct inode        │ │  ← VFS 看到的 inode
  │  │  i_ino, i_mode, ... │ │
  │  └─────────────────────┘ │
  │  ext4 私有字段...        │  ← ext4 私有数据
  └───────────────────────────┘
  通过 container_of() 从 inode 得到 ext4_inode_info
```

## 6.4 文件系统注册

### 6.4.1 register_filesystem

文件系统模块加载时调用 `register_filesystem()`，将 `struct file_system_type` 注册到全局链表。该函数位于 `fs/filesystems.c:72`：

```c
// filesystems.c:34-35
static struct file_system_type *file_systems;
static DEFINE_RWLOCK(file_systems_lock);

// filesystems.c:72-94
int register_filesystem(struct file_system_type *fs)
{
    struct file_system_type **p;

    // 禁止挂载检测 (LKM 标志位)
    BUG_ON(strchr(fs->name, '.'));
    if (fs->next)
        return -EBUSY;

    write_lock(&file_systems_lock);
    p = find_filesystem(fs->name, strlen(fs->name));
    if (*p)
        BUG();   // 不应该出现重名
    *p = fs;     // 插入链表
    write_unlock(&file_systems_lock);
    return 0;
}
EXPORT_SYMBOL(register_filesystem);
```

**`file_systems` 全局链表**：

```
file_systems
  │
  ├─ ext4_fs_type  → next → xfs_fs_type  → next → btrfs_fs_type → ...
  │    .name="ext4"
  │    .fs_flags=FS_REQUIRES_DEV
  │    .init_fs_context=ext4_init_fs_context
  │    .kill_sb=kill_block_super
  │
  ├─ proc_fs_type  → next → sysfs_fs_type → ...
  │    .name="proc"  (虚拟文件系统, FS_USERNS_MOUNT)
  │
  └─ (以 file_systems_lock 读写锁保护)
```

### 6.4.2 get_fs_type：查找文件系统类型

在挂载时，`get_fs_type("ext4")` 遍历该链表找到对应的 `file_system_type`：

```c
struct file_system_type *get_fs_type(const char *name)
{
    struct file_system_type *fs;
    read_lock(&file_systems_lock);             // 持读锁
    fs = __get_fs_type(name, strlen(name));
    if (fs && !try_module_get(fs->owner))     // 锁定模块 (防止 rmmod)
        fs = NULL;                             // 模块正在卸载
    read_unlock(&file_systems_lock);
    return fs;
}
```

## 6.5 挂载流程：从 mount 系统调用到 s_root

### 6.5.1 完整调用链

```
用户态: mount -t ext4 /dev/sda1 /mnt
           │
           ▼
      mount 系统调用
           │
           ▼
    path_mount() (namespace.c:4079)
      │
      ├─ 参数解析 + 权限检查
      │   security_sb_mount() → LSM 钩子
      │   may_mount() → 检查是否允许挂载 (CAP_SYS_ADMIN)
      │
      └─ do_new_mount() (namespace.c:3787)
           │
           ├─ get_fs_type("ext4")  → 找到 ext4_fs_type
           ├─ alloc_fs_context()   → 创建 fs_context
           │
           ├─ fs_type->init_fs_context(fc)  (或 fc->ops->parse_param 等)
           │   ext4: ext4_init_fs_context()
           │   → 设置 fc->ops = &ext4_context_ops
           │
           ├─ parse_monolithic_mount_data()  (旧式 mount 数据)
           │   或 vfs_parse_fs_param()       (新式 fsconfig)
           │
           └─ do_new_mount_fc() (namespace.c:3754)
                │
                ├─ fc_mount(fc)  → 内部调用 vfs_get_tree()
                │    │
                │    └─ vfs_get_tree() (super.c:1743)
                │         │
                │         ├─ fc->ops->get_tree(fc)
                │         │    │
                │         │    ├─ 基于块设备: get_tree_bdev()
                │         │    │    → fc->fs_type->get_tree_bdev 不存在时
                │         │    │    → sget_fc(fc, super_s_dev_test, set)
                │         │    │    → fill_super → sb->s_root = d_make_root()
                │         │    │
                │         │    └─ 虚拟文件系统: get_tree_nodev()
                │         │         → 不关联块设备
                │         │         → fill_super 直接创建 root inode+dentry
                │         │
                │         ├─ super_wake() → SB_BORN 标志可见
                │         │
                │         └─ fc->root = sb->s_root  (设置根路径)
                │
                ├─ security_sb_kern_mount() → LSM 钩子
                ├─ mount_too_revealing() → 安全沙箱检查
                │
                └─ do_add_mount() → 挂到挂载树
                     │
                     └─ graft_tree() → 挂到 /mnt 所在父挂载点
```

### 6.5.2 path_mount

`path_mount()` 位于 `namespace.c:4079`，是所有新挂载的中央入口：

```c
int path_mount(const char *dev_name, const struct path *path,
               const char *type_page, unsigned long flags, void *data_page)
{
    unsigned int mnt_flags = 0, sb_flags;
    int ret;

    // 丢弃兼容的魔数
    if ((flags & MS_MGC_MSK) == MS_MGC_VAL)
        flags &= ~MS_MGC_MSK;

    if (data_page)
        ((char *)data_page)[PAGE_SIZE - 1] = 0;  // 强制 null 终止

    if (flags & MS_NOUSER)
        return -EINVAL;                         // 只能内部使用的挂载

    ret = security_sb_mount(dev_name, path, type_page, flags, data_page);
    if (ret)
        return ret;
    if (!may_mount())
        return -EPERM;                          // 不具备挂载能力

    if (flags & SB_MANDLOCK)
        warn_mandlock();                        // 强锁已废弃警告

    // 默认 relatime
    if (!(flags & MS_NOATIME))
        mnt_flags |= MNT_RELATIME;

    // 分离 per-mountpoint 标志
    if (flags & MS_NOSUID)
        mnt_flags |= MNT_NOSUID;
    if (flags & MS_NODEV)
        mnt_flags |= MNT_NODEV;
    if (flags & MS_NOEXEC)
        mnt_flags |= MNT_NOEXEC;
    // ...

    sb_flags = flags & (SB_RDONLY | SB_SYNCHRONOUS | SB_MANDLOCK |
                        SB_DIRSYNC | SB_SILENT | SB_LAZYTIME);

    return do_new_mount(path, type_page, sb_flags, mnt_flags, 
                        dev_name, data_page);
}
```

### 6.5.3 do_new_mount

`do_new_mount()` 位于 `namespace.c:3787`，处理新式挂载。核心流程：`get_fs_type("ext4")` 查找文件系统类型 → `fs_context_for_mount()` 创建 fs_context → 解析挂载选项 → `do_new_mount_fc()` 完成实际挂载。

### 6.5.4 do_new_mount_fc

`do_new_mount_fc()` 位于 `namespace.c:3754`：调用 `fc_mount(fc)`（内部触发 `vfs_get_tree()` 创建超级块），然后进行 LSM 检查、安全沙箱检查，最后 `do_add_mount()` 将新挂载加入挂载树。

## 6.6 vfs_get_tree：创建或获取超级块

`vfs_get_tree()` 位于 `super.c:1743`，是超级块获取的核心函数。名称中的"tree"指挂载树的根（root）：

```c
int vfs_get_tree(struct fs_context *fc)
{
    struct super_block *sb;
    int error;

    if (fc->root)
        return -EBUSY;                         // 已经调用了 get_tree

    // 1) 调用文件系统的 get_tree (ext4: get_tree_bdev)
    error = fc->ops->get_tree(fc);
    if (error < 0)
        return error;

    if (!fc->root) {
        pr_err("Filesystem %s get_tree() didn't set fc->root\n",
               fc->fs_type->name);
        BUG();
    }

    // 2) 设置 SB_BORN 标志
    sb = fc->root->d_sb;
    WARN_ON(!sb->s_bdi);

    // 放置内存屏障确保所有超级块设置可见
    smp_mb();
    sb->s_flags |= SB_BORN;                     // 文件系统正式生效

    return 0;
}
EXPORT_SYMBOL(vfs_get_tree);
```

**`SB_BORN` 标志的意义**：这个标志是超级块"正式活过来"的信号。在 `SB_BORN` 设置之前，超级块虽然在内存中存在，但其他内核组件不能使用它。设置 `SB_BORN` 后，超级块才正式进入全局可访问状态。

### 6.6.1 get_tree_bdev：基于块设备的文件系统

对于 ext4/xfs/btrfs 等基于块设备的文件系统，`get_tree` 最终走到 `get_tree_bdev()`（在 `super.c` 的辅助函数中）：

```
get_tree_bdev()
  │
  ├─ sget_fc(fc, super_s_dev_test, super_s_dev_set)
  │    │
  │    ├─ 查找已有超级块: 按块设备号在 super_blocks 链表搜索
  │    │   if (found && sb->s_type == fc->fs_type)
  │    │       return sb;  // 复用
  │    │
  │    └─ 没找到: 分配新超级块
  │         alloc_super() → kmem_cache_alloc + 初始化
  │
  ├─ 打开块设备: bdev_file_open_by_dev()
  │
  └─ fill_super()  (ext4: ext4_fill_super)
       │
       ├─ 从磁盘读取超级块数据
       ├─ 验证幻数 ext4  = 0xEF53
       ├─ 解析块组描述符
       ├─ 设置 sb->s_op = &ext4_sops
       ├─ 设置 sb->s_root = d_make_root(root_inode)
       │    └─ root inode = ext4_iget(sb, EXT4_ROOT_INO)
       ├─ 初始化日志 (jbd2)
       ├─ 初始化 inode 表
       └─ 返回 0
```

## 6.7 sget_fc：查找或创建超级块

`sget_fc()` 位于 `super.c:734`，是超级块重用的关键机制。它在 `sb_lock` 保护下遍历文件系统类型的 `fs_supers` 链表，使用 `test` 回调（如 `super_s_dev_test` 按块设备号匹配）查找已有超级块。如果找到则复用，否则 `alloc_super()` 分配新的。

**全局超级块链表**（`super.c:46-47`）：

```c
static LIST_HEAD(super_blocks);        // 全局链表 (所有文件系统类型)
static DEFINE_SPINLOCK(sb_lock);       // 保护该链表的自旋锁
```

对于基于块设备的文件系统，`test` 函数是 `super_s_dev_test`，按块设备号匹配：

```c
static int super_s_dev_test(struct super_block *s, struct fs_context *fc)
{
    return s->s_dev == fc->s_dev;    // 块设备号匹配
}
```

这确保了 **同一块设备上同一类型的文件系统只被挂载一次**——再次 `mount /dev/sda1 /mnt2` 时复用已有超级块。

## 6.8 超级块生命周期

```
  register_filesystem("ext4")
        │
        ▼
    mount("/dev/sda1", "/mnt")
        │
        ├─ sget_fc() → alloc_super()      [创建]
        │     sb->s_count = 1
        │     sb = 全新超级块
        │
        ├─ fill_super()                    [初始化]
        │     从磁盘读超级块
        │     验证 ext4 幻数
        │     创建 root inode + dentry
        │     sb->s_op = &ext4_sops
        │
        ├─ vfs_get_tree()                  [激活]
        │     SB_BORN ← true
        │     全局可见
        │
        ▼
    [活跃状态] ← 用户创建/删除文件
        │        sb->s_count > 0
        │
        ▼
    umount("/mnt") → kill_block_super()
        │
        ├─ generic_shutdown_super()
        │     │
        │     ├─ SB_BORN → 首先清除
        │     │
        │     ├─ shrink_dcache_sb(sb)
        │     │    释放所有 dentry
        │     │
        │     ├─ invalidate_inodes(sb)
        │     │    驱逐所有 inode
        │     │
        │     ├─ put_super(sb)
        │     │    回调 ext4: ext4_put_super()
        │     │    → 写回脏元数据, 关闭日志
        │     │
        │     └─ sb->s_active 降为 0
        │
        └─ destroy_super(sb)
              │
              ├─ spin_lock(&sb_lock)
              ├─ list_del(&sb->s_list)      ← 从全局链表移除
              ├─ spin_unlock(&sb_lock)
              │
              └─ destroy_super_work → kmem_cache_free
```

**SB_ACTIVE 和 SB_DYING 标志**：

```
SB_BORN   ← vfs_get_tree() 设置, 文件系统"活"了
SB_ACTIVE ← 使用中的超级块, 被 generic_shutdown_super 先清除
SB_DYING  ← 正在消亡, 阻止新用户获取引用
```

## 6.9 挂载的命名空间隔离

Linux 的挂载命名空间（mount namespace）允许不同进程看到不同的文件系统树：

```
全局视角:                     容器视角:
  /                              /
  ├── bin                        ├── bin
  ├── etc                        ├── etc  (来自镜像层)
  ├── home                       ├── home
  ├── mnt                        ├── proc
  │   └── usb                    ├── sys
  └── var/lib/docker/overlay2/   └── app
       └── container_root/
```

挂载命名空间通过 `struct mnt_namespace` 管理，其中包含一棵挂载树。每次 `mount()` 默认在当前命名空间中创建新挂载，除非使用了共享子树（shared subtrees）传播。

## 6.10 文件系统冻结/解冻

FIFREEZE ioctl 用于在文件系统快照（LVM, device mapper）前停止写操作。内核定义了三级冻结状态：

```
SB_FREEZE_WRITE      ← 首先进入: 停止用户写入
SB_FREEZE_PAGEFAULT  ← 其次:   停止页错误 (mmap 的写入)
SB_FREEZE_FS         ← 最后:   文件系统内部冻结 (日志, 事务)
THAW (解冻):          反向逐步解除
```

`freeze_super()` 使用 `sb->s_writers` 中的 per-CPU 计数器来同步：

```c
// 简化流程
freeze_super(sb, FREEZE_WRITE)
  → percpu_down_write(sb->s_writers + SB_FREEZE_WRITE - 1)
  → 等待所有正在进行的写操作完成
  → 禁止新写操作

thaw_super(sb)
  → percpu_up_write(sb->s_writers + SB_FREEZE_WRITE - 1)
  → 恢复写操作
```

文件系统可以实现 `freeze_fs` / `unfreeze_fs` 回调来进行额外的文件系统特定准备（如 xfs 的日志冻结）。

## 6.11 /proc/mounts：向用户态暴露挂载信息

`/proc/mounts` 的输出是通过遍历当前进程的挂载命名空间实现的：

```
读取 /proc/mounts:
  │
  ├─ mounts_open() → 遍历 mnt_namespace->list
  │
  └─ show_vfsmnt() 输出每行:
       dev_name mount_point fstype options freq passno
       
       例如:
       /dev/sda1 / ext4 rw,relatime 0 0
       
       其中 options 部分调用:
       → show_sb_opts()  (通用 VFS 选项: rw/ro, sync, noexec, ...)
       → sb->s_op->show_options()  (文件系统特定选项)
```

## 6.12 完整挂载流程 ASCII 图

```
┌─────────────────────────────────────────────────────────────────────┐
│                    mount -t ext4 /dev/sda1 /mnt                     │
└──────────────────────────────────┬──────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│  path_mount() (namespace.c:4079)                                    │
│  ├─ 解析 flags: SB_RDONLY? MS_NOSUID?                               │
│  ├─ security_sb_mount() → SELinux 检查                              │
│  └─ may_mount() → 检查 CAP_SYS_ADMIN                                │
└──────────────────────────────────┬──────────────────────────────────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│  do_new_mount() (namespace.c:3787)                                  │
│  ├─ get_fs_type("ext4") → 找到 ext4_fs_type                         │
│  ├─ fs_context_for_mount() → 创建 fs_context                        │
│  ├─ parse_monolithic_mount_data() → 解析 "defaults" 等              │
│  └─ do_new_mount_fc() (namespace.c:3754)                            │
│       │                                                             │
│       ├─ fc_mount(fc) ───────────────────────┐                      │
│       │                                       ▼                      │
│       │  ┌──────────────────────────────────────────────────────┐   │
│       │  │  vfs_get_tree() (super.c:1743)                       │   │
│       │  │  ├─ fc->ops->get_tree(fc)                             │   │
│       │  │  │    └─ get_tree_bdev()                              │   │
│       │  │  │         ├─ sget_fc() → alloc_super()               │   │
│       │  │  │         │    创建 struct super_block               │   │
│       │  │  │         │                                          │   │
│       │  │  │         └─ ext4_fill_super()                       │   │
│       │  │  │              ├─ 读取磁盘超级块 (ext4_super_block)  │   │
│       │  │  │              ├─ 验证幻数 0xEF53                    │   │
│       │  │  │              ├─ 解析块组描述符                     │   │
│       │  │  │              ├─ ext4_iget(EXT4_ROOT_INO) → root    │   │
│       │  │  │              ├─ d_make_root(root_inode) → s_root   │   │
│       │  │  │              └─ 初始化 jbd2 日志                   │   │
│       │  │  │                                                   │   │
│       │  │  └─ super_wake() → SB_BORN 置位                       │   │
│       │  └──────────────────────────────────────────────────────┘   │
│       │                                                             │
│       ├─ security_sb_kern_mount()                                    │
│       ├─ mount_too_revealing() → 沙箱检查                            │
│       │                                                             │
│       └─ do_add_mount()                                             │
│            └─ graft_tree() → 挂到 /mnt 所在父挂载点                  │
└─────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                         用户态: ls /mnt → 看到文件!
                                   │
                            通过 /mnt 的 dentry
                               ↓
                          sb->s_root
                               ↓
                          ext4 目录项
                               ↓
                          ls 列出文件
```

## 6.13 关键数据结构关系图

```
                         ┌─────────────────┐
                         │ file_systems 链表 │
                         │ (filesystems.c)  │
                         └────────┬────────┘
                                  │
                        ┌─────────▼─────────┐
                        │file_system_type    │
                        │ .name = "ext4"    │
                        │ .init_fs_context  │
                        │ .kill_sb          │
                        │ .fs_supers ───┐   │
                        └───────────────┼───┘
                                        │
              ┌─────────────────────────┼───────────────────────┐
              │                         ▼                       │
              │              ┌──────────────────┐               │
              │              │ struct super_block│               │
              │              │ s_list ──→ 全局链表│              │
              │              │ s_type ──→ ext4   │              │
              │              │ s_op   ──→ ext4_sops            │
              │              │ s_root ──→ 根dentry             │
              │              │ s_bdev ──→ /dev/sda1            │
              │              │ s_fs_info → ext4_sb_info        │
              │              │ s_mounts ──→ mount 链表          │
              │              └────────┬─────────┘               │
              │                      │                          │
              │         ┌────────────┴────────────┐             │
              │         ▼                         ▼             │
              │  ┌──────────┐            ┌──────────┐          │
              │  │ mount    │            │ mount    │          │
              │  │ mnt_mountpoint  /mnt   │ mnt_mountpoint     │
              │  │ mnt_parent     /      │ mnt_parent         │
              │  │ mnt_sb ─────→ sb     │ mnt_sb ────→ sb    │
              │  └──────────┘            └──────────┘          │
              │  挂载点: /mnt            挂载点: /mnt2         │
              │                                                │
              └────────────────────────────────────────────────┘
```

## 6.14 关键文件和函数索引

| 函数/定义 | 文件 | 行号 | 说明 |
|-----------|------|------|------|
| register_filesystem | fs/filesystems.c | 72 | 注册文件系统类型 |
| file_systems 链表 | fs/filesystems.c | 34 | 全局文件系统类型链表 |
| struct super_block | include/linux/fs/super_types.h | 132 | 超级块结构定义 |
| struct super_operations | include/linux/fs/super_types.h | 83 | 超级块操作表 |
| super_blocks 链表 | fs/super.c | 46 | 全局超级块链表 |
| sb_lock 自旋锁 | fs/super.c | 47 | 超级块链表保护锁 |
| sget_fc | fs/super.c | 734 | 查找或创建超级块 |
| vfs_get_tree | fs/super.c | 1743 | 获取挂载树根 |
| path_mount | fs/namespace.c | 4079 | mount 系统调用入口 |
| do_new_mount | fs/namespace.c | 3787 | 创建新挂载 |
| do_new_mount_fc | fs/namespace.c | 3754 | 使用 fs_context 创建挂载 |

---

## 系列结语

自此，**VFS 内核源码深度解析**系列六篇文章，完整覆盖了 Linux 虚拟文件系统（VFS）的全栈实现：

| 篇目 | 核心主题 | 关键数据结构 |
|------|---------|-------------|
| 第一篇 | 目录项缓存 (dcache) | struct dentry, struct dentry_operations |
| 第二篇 | inode 与地址空间 | struct inode, struct address_space, page cache |
| 第三篇 | 页缓存与回写 | radix tree/xarray, writeback, readahead |
| 第四篇 | 路径查找 | struct nameidata, link_path_walk, RCU-walk |
| 第五篇 | 文件操作与 fd 表 | struct file, files_struct, fput 延迟释放 |
| 第六篇 | 超级块与挂载 | struct super_block, vfs_get_tree, register_filesystem |

这一系列从底层的数据缓存（dentry、inode、page cache），到中层的操作引擎（路径查找、文件操作），再到上层的系统接入（挂载、文件系统注册），完整呈现了 VFS 作为"一切皆文件"抽象的多层架构。

理解 VFS 的关键在于把握各层之间的解耦设计：

- **dentry 层**：纯粹的内存缓存，与底层文件系统无关，提供路径查找的性能
- **inode 层**：文件元数据的通用表示，通过操作表委托给文件系统实现
- **file 层**：打开文件的运行时状态，隔离了并发访问的上下文
- **super_block 层**：文件系统实例的"身份证"，管理整个文件系统的生命周期
- **挂载层**：将文件系统接入进程可见的命名空间树

Linux VFS 是内核中最优美、最复杂的子系统之一。从 Linus Torvalds 最初的设计到如今数十年数千开发者的贡献，VFS 证明了好的抽象不仅能持续演进，还能在面对 ext2→ext4、NFS→Ceph、本地→云原生的各种范式转变时保持核心设计的稳定。
