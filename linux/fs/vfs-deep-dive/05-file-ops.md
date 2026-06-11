# 第五篇：文件操作与文件描述符表 — open 到 close 的完整生命周期

> 源码：fs/open.c (1581 行), fs/file.c (1533 行), fs/file_table.c (665 行), fs/read_write.c (1823 行) ｜ 头文件：include/linux/fs.h, include/linux/fdtable.h

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 5.1 概述：从用户态 fd 到内核 struct file

对于用户态开发人员来说，文件描述符（file descriptor, fd）只是一个整数——`open()` 返回 3、`read(3, ...)` 读取、`close(3)` 释放。但在这简单的接口背后，内核维护了一个多层结构：

```
用户态                         内核态
───┬───                    ───┬───
   │                          │
  fd=3     ─────────→    files_struct.fd_array[3]   (RCU 指针数组)
                                │
                                ▼
                          struct file               (文件实例)
                           ├─ f_op      → file_operations (操作表)
                           ├─ f_inode   → struct inode    (inode 缓存)
                           ├─ f_path    → struct path     (dentry+mount)
                           ├─ f_pos     → 当前文件偏移
                           ├─ f_flags   → O_RDONLY/O_RDWR 等
                           └─ f_ref     → 引用计数 (file_ref_t)
```

本篇将追踪一个文件从 `open()` 到 `close()` 的完整生命周期，深入每一层数据结构。

## 5.2 open() 系统调用链

### 5.2.1 调用流程图

```
用户态: open("/home/file.txt", O_RDWR | O_CREAT, 0644)
   │
   ▼
do_sys_open() (open.c:1367)
   │  struct open_how how = build_open_how(flags, mode);
   │
   ▼
do_sys_openat2() (open.c:1355)
   │  build_open_flags(&how, &op);
   │
   ▼
do_file_open() (namei.c:4877)
   │  path_openat(&nd, op, flags | other);
   │    ├─ path_init()          → 初始化 nameidata
   │    ├─ link_path_walk()     → 路径查找 (见第4篇)
   │    └─ do_open()            → 实际打开
   │       ├─ vfs_open()        → 调用文件系统 open
   │       └─ 如果是 O_CREAT → lookup_open()
   │
   ▼
FD_ADD(how->flags, do_file_open(...)) (open.c:1364)
   │  fd = alloc_fd()           → 分配 fd 号
   │  fd_install(fd, file)      → 安装到 fd table
   │
   ▼
返回值: 3  (用户态得到的 fd)
```

### 5.2.2 do_sys_openat2

```c
// open.c:1355-1365
static int do_sys_openat2(int dfd, const char __user *filename,
                          struct open_how *how)
{
    struct open_flags op;
    int err = build_open_flags(how, &op);   // 翻译 flags
    if (unlikely(err))
        return err;

    CLASS(filename, name)(filename);       // 拷贝路径到内核空间
    return FD_ADD(how->flags, do_file_open(dfd, name, &op));
}
```

`FD_ADD` 是一个巧妙的设计：它的第二个参数 `do_file_open(dfd, name, &op)` 返回一个 `struct file *`，宏将其转换为 fd 编号。

### 5.2.3 build_open_flags

`build_open_flags()` 将用户态的 `open_how`（支持 `openat2()` 的扩展参数结构）转换为内核内部的 `struct open_flags`。这一步处理：

- `O_RDONLY / O_WRONLY / O_RDWR` → `FMODE_READ / FMODE_WRITE` 的精确模式位
- `O_CREAT → open_flag |= O_CREAT`
- `O_TRUNC → open_flag |= O_TRUNC`
- `O_APPEND → FMODE_APPEND`
- `O_DIRECT → FMODE_DIRECT_IO`
- `O_SYNC / O_DSYNC` → 等价的 FMODE 位
- `openat2` 的 `resolve` 字段 → `LOOKUP_*` 标志位

## 5.3 struct file：内核文件对象

`struct file` 定义在 `include/linux/fs.h:1260`，是已打开文件的内核表示。每个 `open()` 调用都分配一个新的 `struct file`：

```
struct file {
    spinlock_t              f_lock;
    fmode_t                 f_mode;          // line 1262  读写模式
    const struct file_operations *f_op;       // line 1263  操作表指针
    struct address_space    *f_mapping;       // line 1264  页缓存映射
    void                    *private_data;    // line 1265  文件系统私有数据
    struct inode            *f_inode;         // line 1266  指向 inode
    unsigned int            f_flags;          // line 1267  O_* 标志位
    unsigned int            f_iocb_flags;
    const struct cred       *f_cred;          // 打开时的凭证
    struct fown_struct      *f_owner;         // SIGIO 所有者
    
    /* --- cacheline 1 boundary (64 bytes) --- */
    union {
        const struct path   f_path;           // line 1273  dentry + mount
        struct path         __f_path;
    };
    union {
        struct mutex        f_pos_lock;       // 位置锁 (普通文件/目录)
        u64                 f_pipe;           // 管道用
    };
    loff_t                  f_pos;            // line 1282  当前文件偏移

    /* --- cacheline 2 boundary (128 bytes) --- */
    errseq_t                f_wb_err;         // writeback 错误
    errseq_t                f_sb_err;         // superblock 错误
#ifdef CONFIG_EPOLL
    struct hlist_head       *f_ep;            // epoll 链
#endif
    union {
        struct callback_head f_task_work;     // RCU 回调 (fput 延迟)
        struct llist_node    f_llist;         // 延迟释放链表
        struct file_ra_state f_ra;            // 预读状态
        freeptr_t            f_freeptr;       // slab 空闲链表
    };
    file_ref_t              f_ref;            // line 1298  新引用计数
} __randomize_layout __attribute__((aligned(4)));
```

**关键设计要点**：

- **`f_ref` 使用 `file_ref_t`**：这是内核 6.x 新引入的类型（替代了 `atomic_long_t`），专为 `struct file` 引用计数优化，注意与 `inode->i_count` 和 `dentry->d_lockref` 的计数体系不耦合
- **`f_path` 是 `const`**：文件一旦打开，路径不可变——即使目录树重组、挂载点变化、文件被 rename，`struct file` 仍通过 dentry 和 vfsmount 精确指向打开时的 inode 副本
- **`__randomize_layout`**：内核安全加固，随机化结构体布局以防止结构体布局已知导致的攻击
- **cacheline 对齐**：3 个 64 字节的 cacheline 划分，将热路径字段（f_mode, f_op, f_mapping）与冷路径字段分离

## 5.4 struct file_operations：文件操作表

`struct file_operations` 定义在 `include/linux/fs.h:1926`，是 VFS 面向对象设计的核心——每个文件系统提供自己的操作函数集，VFS 层通过函数指针统一调用：

```c
struct file_operations {  // include/linux/fs.h:1926
    loff_t (*llseek)(struct file *, loff_t, int);          // line 1929
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);     // line 1930
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *); // line 1931
    ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);    // line 1932
    ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);   // line 1933
    int (*iterate_shared)(struct file *, struct dir_context *); // line 1936
    __poll_t (*poll)(struct file *, struct poll_table_struct *); // line 1937
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long); // line 1938
    int (*mmap)(struct file *, struct vm_area_struct *);        // line 1940
    int (*open)(struct inode *, struct file *);                 // line 1941
    int (*release)(struct inode *, struct file *);              // line 1943
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);  // line 1944
    int (*fasync)(int, struct file *, int);                     // line 1945
    int (*lock)(struct file *, int, struct file_lock *);        // line 1946
    ssize_t (*splice_write)(...);                               // line 1950
    ssize_t (*splice_read)(...);                                // line 1951
    long (*fallocate)(struct file *, int, loff_t, loff_t);      // line 1954
};
```

**关键回调的使用方式**：

| 回调 | 触发时机 | 典型实现 |
|------|---------|---------|
| `open` | `dentry_open()` 返回前 | ext4 初始化预读窗口, xfs 初始化锁状态 |
| `release` | `__fput()` 中最后一次引用释放 | ext4 清理私有数据, 最终 iput |
| `read_iter` | `vfs_read()` 主路径 | ext4_file_read_iter → generic_file_read_iter |
| `write_iter` | `vfs_write()` 主路径 | ext4_file_write_iter → __generic_file_write_iter |
| `iterate_shared` | `getdents64()` | ext4_readdir → 目录迭代 |
| `mmap` | `mmap()` 系统调用 | ext4_file_mmap → generic_file_mmap |
| `unlocked_ioctl` | `ioctl()` | ext4_ioctl → 处理 FS_IOC_* 命令 |
| `llseek` | `lseek()` | generic_file_llseek |
| `poll` | `select/poll/epoll` | 检查文件是否就绪 |
| `fsync` | `fsync()/fdatasync()` | ext4_sync_file |

## 5.5 struct inode_operations：inode 操作表

`struct inode_operations` 定义在 `include/linux/fs.h:2001`，它在 inode 级别提供目录/文件操作。与 `file_operations` 不同，`inode_operations` 不需要文件处于"已打开"状态——它处理的是命名空间层级的操作：

```c
struct inode_operations {  // include/linux/fs.h:2001
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int); // line 2002
    int (*permission)(struct mnt_idmap *, struct inode *, int);   // line 2004
    int (*create)(struct mnt_idmap *, struct inode *, struct dentry*, umode_t, bool); // line 2009
    int (*link)(struct dentry *, struct inode *, struct dentry *); // line 2011
    int (*unlink)(struct inode *, struct dentry *);               // line 2012
    struct dentry *(*mkdir)(struct mnt_idmap *, struct inode *, struct dentry*, umode_t); // line 2015
    int (*rmdir)(struct inode *, struct dentry *);                // line 2017
    int (*rename)(struct mnt_idmap *, struct inode *, struct dentry*,
                   struct inode *, struct dentry *, unsigned int); // line 2020
    int (*setattr)(struct mnt_idmap *, struct dentry *, struct iattr *); // line 2022
    int (*getattr)(struct mnt_idmap *, const struct path*, struct kstat*, u32, unsigned); // line 2023
    int (*update_time)(struct inode *, enum fs_update_type, unsigned int); // line 2028
    int (*atomic_open)(struct inode *, struct dentry*, struct file*, unsigned, umode_t);
};
```

**`file_operations` vs `inode_operations` 的区别**：

```
                    file_operations                 inode_operations
取决于           打开的文件实例                   inode (可以不打开)
典型调用方        vfs_read/vfs_write              namei (路径查找)
主要操作          read/write/ioctl/mmap           lookup/create/mkdir/rename
偏移量            有 (f_pos)                     无
生命周期          从 open 到 close                从 inode 创建到销毁
并发              f_pos_lock 保护                  inode_lock 保护
```

## 5.6 文件描述符表 (fd table)

### 5.6.1 struct fdtable

`struct fdtable` 定义在 `include/linux/fdtable.h:26`，是描述符数组的核心：

```c
#define NR_OPEN_DEFAULT BITS_PER_LONG             // 64 (在 64 位系统)

struct fdtable {
    unsigned int max_fds;                         // line 27  最大 fd 数
    struct file __rcu **fd;                       // line 28  RCU 保护的指针数组
    unsigned long *close_on_exec;                 // line 29  执行时关闭位图
    unsigned long *open_fds;                      // line 30  已打开 fd 位图
    unsigned long *full_fds_bits;                 // line 31  分配加速位图
    struct rcu_head rcu;                          //          释放时的 RCU 延迟
};
```

**位图的作用**：

```
open_fds[0] = 0b...10101
                    │││
                    ││└─ fd 0 已打开
                    │└── fd 1 未打开
                    └─── fd 2 已打开

close_on_exec[0]:   fork()+exec() 时关闭的 fd 集合
full_fds_bits[N]:   第 N 组 64 个 fd 已全部占满 → 跳过该组，加速分配
```

### 5.6.2 struct files_struct

`struct files_struct` 定义在 `include/linux/fdtable.h:38`：

```c
struct files_struct {
    /* read mostly part */
    atomic_t count;                                    // 引用计数
    bool resize_in_progress;                          // 扩容进行中
    wait_queue_head_t resize_wait;                    // 扩容等待队列

    struct fdtable __rcu *fdt;                        // line 46  RCU 指针
    struct fdtable fdtab;                             // 嵌入的 fdtable (默认大小)

    /* written part (separate cache line in SMP) */
    spinlock_t file_lock ____cacheline_aligned_in_smp; // line 51
    unsigned int next_fd;                             // line 52  快速分配提示
    unsigned long close_on_exec_init[1];              // 位图初始值 (嵌入)
    unsigned long open_fds_init[1];                   // 位图初始值 (嵌入)
    unsigned long full_fds_bits_init[1];              // 位图初始值 (嵌入)
    struct file __rcu * fd_array[NR_OPEN_DEFAULT];   // line 56  初始 64 槽
};
```

**设计细节**：

- **初始嵌入 64 个槽**：对于绝大多数进程（打开文件数 < 64），所有数据结构都在 `files_struct` 自身的内存中，无需额外分配
- **RCU 保护**：`fdt` 通过 RCU 指针访问，`fd` 数组使用 `__rcu` 标注。这意味着 `fget()` 可以在不持锁的情况下通过 RCU 安全读取 fd 表
- **扩容机制**：当打开 fd 数超过当前容量时，`expand_files()` 分配更大的 `fdtable`，设置 `resize_in_progress`，准备就绪后通过 RCU 替换 `fdt` 指针

```
初始态 (fd < 64):                  扩容后 (fd >= 64):

  files_struct                       files_struct
  ┌──────────────┐                  ┌──────────────┐
  │ fdt ─────────┼────┐             │ fdt ─────────┼─────────┐
  │ fdtab        │    │             │ fdtab        │         │
  │  fd ────────┐│    │             │  fd ────────┐│         │
  └──────────────┘    │             └──────────────┘         │
                      ▼                                      ▼
               fd_array[64]                      新分配的 fdtable
              ┌──────────┐                     ┌──────────────────┐
              │ [0] file*│                     │ max_fds = 256    │
              │ [1] NULL │                     │ fd ────────────> array[256]
              │ [2] NULL │                     │ open_fds         │
              │ ...      │                     │ close_on_exec    │
              │ [63] NULL│                     │ full_fds_bits    │
              └──────────┘                     └──────────────────┘
```

## 5.7 alloc_fd：文件描述符分配

`alloc_fd()` 位于 `fs/file.c:570`，在 fd 表中查找最小的可用描述符：

```c
static int alloc_fd(unsigned start, unsigned end, unsigned flags)
{
    struct files_struct *files = current->files;
    unsigned int fd;
    int error;
    struct fdtable *fdt;

    spin_lock(&files->file_lock);          // 持有 file_lock
repeat:
    fdt = files_fdtable(files);
    fd = start;
    if (fd < files->next_fd)
        fd = files->next_fd;               // 使用 hint 加速

    if (likely(fd < fdt->max_fds))
        fd = find_next_fd(fdt, fd);        // 在位图中找下一个空闲

    error = -EMFILE;
    if (unlikely(fd >= end))               // 超过进程限制
        goto out;

    if (unlikely(fd >= fdt->max_fds)) {    // 需要扩容
        error = expand_files(files, fd);
        if (error < 0)
            goto out;
        goto repeat;                       // 重新尝试
    }

    if (start <= files->next_fd)
        files->next_fd = fd + 1;           // 更新 hint

    __set_open_fd(fd, fdt, flags & O_CLOEXEC);  // 设置位图
    error = fd;
    VFS_BUG_ON(rcu_access_pointer(fdt->fd[fd]) != NULL);  // 确保槽为空
    // ...
out:
    spin_unlock(&files->file_lock);
    return error;
}
```

**关键特性**：

1. **位图 + hint 加速**：`next_fd` 缓存了上次分配的下一个位置，`full_fds_bits` 按 64 个一组判断是否全满来跳过已满区域
2. **`rlimit(NOFILE)` 限制**：进程的 `nofile` 限制（默认 1024，可由 `sysctl_nr_open` 调高，上限约 1M）
3. **PM_EMFILE 错误**：当 `fd >= end`（即超过了进程的文件打开限制）时返回

## 5.8 fd_install：安装文件描述符

`fd_install()` 位于 `fs/file.c:680`，将 `struct file *` 安装到 fd 数组中：

```c
// file.c:673-702
/**
 * fd_install - install a file pointer in the fd array
 * @fd:  file descriptor to install into
 * @file: file pointer to install
 *
 * After this, the fd array entry is visible to lockless lookups.
 */
void fd_install(unsigned int fd, struct file *file)
{
    struct files_struct *files = current->files;
    struct fdtable *fdt;

    rcu_read_lock_sched();
    fdt = files_fdtable(files);
    BUG_ON(fdt->fd[fd] != NULL);
    // 确保任何正在 resize 的 fdtable 已完成替换
    smp_rmb();
    rcu_assign_pointer(fdt->fd[fd], file);     // line 680 附近
    rcu_read_unlock_sched();
}
```

**关键的 `smp_rmb()` 屏障**：当另一个线程正在扩容 fdtable 时，`fd_install` 必须确保读取到最新的 `fdt` 指针。这个读屏障与 `expand_files` 中的写屏障配对，防止读到正在被释放的旧 fdtable。

## 5.9 fget/fput：引用计数操作

### 5.9.1 fget/fget_raw

`fget()` 位于 `fs/file.c:1111`，通过 fd 号获取文件引用——这是无锁的（RCU 保护）：

```c
// file.c:1094-1114
static struct file *__fget_files(struct files_struct *files, 
                                 unsigned int fd, fmode_t mask)
{
    struct file *file;

    rcu_read_lock();
    file = __fget_files_rcu(files, fd, mask);   // 无锁查找
    rcu_read_unlock();

    return file;
}

static inline struct file *__fget(unsigned int fd, fmode_t mask)
{
    return __fget_files(current->files, fd, mask);
}

struct file *fget(unsigned int fd)
{
    return __fget(fd, FMODE_PATH);     // 允许仅持有路径的文件
}
EXPORT_SYMBOL(fget);
```

**RCU-safe 无锁查找的工作原理**：

```
fget(3):
  │
  ├─ rcu_read_lock()
  │
  ├─ 读取 files_struct->fdt → fdtable
  │     └─ rcu_dereference (由 files_fdtable 宏完成)
  │
  ├─ 读取 fdtable->fd[3] → struct file *
  │     └─ smp_load_acquire (由 rcu_dereference 提供)
  │
  ├─ file_ref_inc(&file->f_ref)  → 增加引用
  │
  ├─ 验证 file 仍然在 fd 表中 (重新读取 fd[3], 比较指针)
  │     └─ 如果变了 → fput 回退, 返回 NULL
  │
  └─ rcu_read_unlock()
```

这个流程的关键漏洞是 **TOCTOU**（Time of Check, Time of Use）：在 `fget` 和实际使用之间，另一个线程可能执行 `close()` + `open()`，导致同一个 fd 号指向了不同的文件。内核通过 `fget` 内部的双重检查（拿到引用后重新确认 fd 表位置依旧）来解决这个问题。但调用方必须注意：**fd 到文件的映射随时可能变化，只有通过 fget 拿到的 `struct file *` 才是稳定的**。

### 5.9.2 fput 延迟释放

`fput()` 位于 `fs/file_table.c:584`：

```c
// file_table.c:584-589
void fput(struct file *file)
{
    if (unlikely(file_ref_put(&file->f_ref)))   // 引用计数降到 0
        __fput_deferred(file);
}
EXPORT_SYMBOL(fput);
```

当最后一个引用被释放时，`__fput()`（`file_table.c:484`）不是直接在 `fput` 调用者的上下文执行，而是通过以下三种方式之一延迟执行：

```
fput(file) → 引用计数归零
   │
   ├─ 方式 1: task_work (file_table.c:570-572)
   │    init_task_work(&file->f_task_work, ____fput);
   │    task_work_add(current, ...)    ← 在返回到用户态时执行
   │
   ├─ 方式 2: delayed_work (file_table.c:578-581)
   │    llist_add(&file->f_llist, &delayed_fput_list)
   │    schedule_delayed_work(...)     ← 调度到 worker 线程
   │
   └─ 方式 3: __fput_sync
       直接同步调用 __fput()           ← 仅内核线程特殊场景
```

三种方式的选择逻辑（在 `__fput_deferred` 中）：

- **task_work**：首选方式。将 `__fput` 挂到 `current` 进程的 task_work 队列，在返回到用户态前执行。这样避免在深度调用栈中执行繁重的释放操作。
- **delayed_fput**：fallback。当 task_work_add 失败时（如进程正在退出），加入全局 `delayed_fput_list` 链表，由 worker 线程 `delayed_fput` 异步处理。
- **sync**：仅用于 `__fput_sync()`，特供给需要同步等待释放完成的内核线程。

这种延迟设计的原因是 `__fput()` 可能执行大量工作：写入脏数据、关闭网络套接字、发送通知、释放锁等——在随机上下文中执行不可预测。

## 5.10 __fput：最后的释放

`__fput()` 位于 `fs/file_table.c:484`，是文件释放的"清算中心"：执行 fsnotify_close、eventpoll_release、locks_remove_file、security_file_release、fasync 关闭、`->release` 回调、cdev_put、fops_put、dput、mntput，最后 `file_free` 释放内存。

**释放步骤的严格顺序**：

```
__fput 执行顺序及其原因:

 1  fsnotify_close()      ← 必须最先: 后续操作可能触发新的通知, 但文件已逻辑关闭
 2  eventpoll_release()   ← 必须在 __fput 中第一个清理: 防止 epoll 回调竞争
 3  locks_remove_file()   ← 释放 POSIX 锁, 唤醒阻塞的锁请求
 4  security_file_release() ← LSM (SELinux/AppArmor) 释放安全上下文
 5  fasync(-1)            ← 关闭 FASYNC 通知, 中止 SIGIO 发送
 6  ->release()           ← 文件系统的最终清理: 写回脏数据, 关闭后端
 7  cdev_put()            ← 字符设备引用-1, 最后一次时关闭设备
 8  fops_put()            ← 释放文件系统模块的引用计数
 9  dput()                 ← dentry 引用-1
10  mntput()               ← vfsmount 引用-1
11  file_free()            ← 释放 struct file 自身内存
```

## 5.11 read/write 路径

### 5.11.1 vfs_read

`vfs_read()` 位于 `fs/read_write.c:554`，是 `read()` 系统调用进入 VFS 层后的主入口。它检查 `FMODE_READ` 权限、验证 `access_ok`、调用 `rw_verify_area` 进行范围检查，然后分发到 `f_op->read` 或 `f_op->read_iter`。现代文件系统（ext4、xfs）实现 `read_iter`，通过 `kiocb` + `iov_iter` 统一向量 I/O 和 O_DIRECT 路径。

**传统 read vs read_iter**：

```
传统 read(file, buf, count, pos): 一次性传入用户态缓冲区
现代 read_iter(kiocb, iov_iter):  基于 kiocb + iov_iter, 统一 readv/writev, O_DIRECT, splice

vfs_read 的兼容逻辑:
  if (file->f_op->read)          → 用传统路径
  else if (file->f_op->read_iter) → new_sync_read() ⇒ 构造 kiocb + iov_iter ⇒ 调用 read_iter
```

### 5.11.2 kernel_read / kernel_write

内核内部的读写使用 `kernel_read()` 和 `kernel_write()`，它们绕过了用户态检查：

```c
// read_write.c:543-552
ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
    return __kernel_read(file, buf, count, pos);
}

// read_write.c:652-666
ssize_t kernel_write(struct file *file, const void *buf, size_t count,
                     loff_t *pos)
{
    ret = __kernel_write(file, buf, count, pos);
    return ret;
}
```

这两个函数在内核内部被广泛使用：模块签名验证读取签名文件、内核从 initramfs 加载二进制、文件系统从其块设备读取数据等。

## 5.12 open 到 close 完整生命周期

```
用户态: open("/home/file", O_RDWR)
  → sys_open() → getname() → alloc_fd(3) → path_openat() → link_path_walk → do_open/vfs_open
    → do_dentry_open: file->f_op=inode->i_fop, f_op->open(inode,file)
  → fd_install(3, file) → return 3

用户态: read(3, buf, 4096)
  → ksys_read() → fdget_pos(3)=fget(3) → vfs_read(file,buf,...)
    → rw_verify_area → f_op->read_iter (ext4: ext4_file_read_iter → generic_file_read_iter → filemap_read)
  → fsnotify_access → return 4096

用户态: close(3)
  → __close_fd(): fdt->fd[3]=NULL → filp_close(file) → fput(file)
    → refcount→0 → __fput_deferred → task_work_add(____fput)
      → __fput: fsnotify_close, eventpoll_release, locks_remove,
        ->release, dput, mntput, file_free → kmem_cache_free
```

## 5.13 CLOSE-ON-EXEC 机制

`close_on_exec` 位图（`fdtable.h:29`）与 `O_CLOEXEC` 标志联动。当进程执行 `execve()` 时，内核遍历 `close_on_exec` 位图，关闭所有标记的 fd：

```
open("/tmp/secret", O_RDONLY | O_CLOEXEC)
   │
   ├─ alloc_fd() 中:
   │    __set_open_fd(fd, fdt, flags & O_CLOEXEC)
   │    → open_fds[fd] = 1
   │    → close_on_exec[fd] = 1   ← 设置 CLOEXEC
   │
   └─ 后续 execve("/bin/evil"):
        do_close_on_exec(files)
          │
          └─ 遍历 close_on_exec 位图
             → fd=3 的 close_on_exec bit = 1
             → __put_unused_fd(3)  ← 关闭
             → /bin/evil 看不到 /tmp/secret
```

`O_CLOEXEC` 是解决 TOCTOU 竞争的关键——在 `open()` 时设置 CLOEXEC 是原子的，而 `open() + fcntl(FD_CLOEXEC)` 之间有竞态窗口。

## 5.14 多进程共享 fd table

`struct files_struct` 可以在 `clone()` （`CLONE_FILES`）时共享：

```
父进程                              子进程 (CLONE_FILES)
  │                                   │
  ├───────────────────────────────────┤
  │       相同的 files_struct          │
  │     ┌────────────────┐            │
  │     │ count = 2      │            │
  │     │ fd_array:      │            │
  │     │  [0] → stdin   │            │
  │     │  [1] → stdout  │            │
  │     │  [3] → file    │            │
  │     └────────────────┘            │
  │                                   │
  ├─ read(3, ...)                      │
  │   → file->f_pos  变化 (共享偏移)   │
  │                                   ├─ write(3, ...)
  │                                   │   → 相同 file, 相同偏移!
  │                                   │
```

**关键影响**：
- `f_pos` 在两个进程/线程间共享（除非使用 `pread/pwrite`）
- `close()` 只移除当前进程的 fd 槽，不影响其他共享者的槽位
- `struct file` 的引用计数由所有共享者贡献，只有最后一个 `fput` 才释放

## 5.15 关键文件和函数索引

| 函数/定义 | 文件 | 行号 | 说明 |
|-----------|------|------|------|
| do_sys_open | fs/open.c | 1367 | open() 系统调用入口 |
| do_sys_openat2 | fs/open.c | 1355 | openat2 实现 |
| do_file_open | fs/namei.c | 4877 | 执行路径查找 + 打开 |
| struct file | include/linux/fs.h | 1260 | 内核文件对象 |
| struct file_operations | include/linux/fs.h | 1926 | 文件操作表 |
| struct inode_operations | include/linux/fs.h | 2001 | inode 操作表 |
| struct fdtable | include/linux/fdtable.h | 26 | fd 描述符表 |
| struct files_struct | include/linux/fdtable.h | 38 | 进程打开文件结构 |
| alloc_fd | fs/file.c | 570 | 分配 fd 号 |
| fd_install | fs/file.c | 680 | 安装 fd → file |
| __fget_files | fs/file.c | 1094 | RCU 安全获取文件引用 |
| fget | fs/file.c | 1111 | 通过 fd 获取文件引用 |
| alloc_file_pseudo | fs/file_table.c | 413 | 分配匿名文件 (pipe/socket) |
| __fput | fs/file_table.c | 484 | 最后的文件释放 |
| fput | fs/file_table.c | 584 | 释放文件引用 (可能延迟) |
| vfs_read | fs/read_write.c | 554 | read() VFS 入口 |
| kernel_read | fs/read_write.c | 543 | 内核空间读取 |
| kernel_write | fs/read_write.c | 652 | 内核空间写入 |

---

## 下一篇文章

[第六篇：超级块与挂载 — 从 register_filesystem 到用户态看见文件](./06-mount.md)
