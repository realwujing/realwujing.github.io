# 第四篇：路径查找 — link_path_walk、RCU 快速路径与符号链接解析

> 源码：fs/namei.c (6427 行) ｜ 头文件：include/linux/namei.h, include/linux/dcache.h

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 4.1 什么是路径查找

路径查找（path lookup）是 VFS 层最核心、最频繁调用的操作。用户态每一个文件操作——`open("/home/user/file.txt")`、`stat()`、`unlink()` 等——都必须先将字符串形式的路径翻译为内核中的 `struct dentry` 对象。这个翻译过程每秒可能执行数十万次，因此内核专门设计了一套精巧的多模式查找引擎。

核心问题可以概括为：给定一个路径字符串 `"/home/user/file.txt"`，如何在内存中找到对应的目录项缓存（dcache），或者驱动实际文件系统去磁盘上找到它？

```
用户态: open("/home/user/file.txt") 
              │
              ▼
         ┌──────────┐
         │ do_sys_open│ (open.c:1367)
         └──────────┘
              │
              ▼
         ┌──── nameidata ─────────────────────┐
         │  pathname: "/home/user/file.txt"   │
         │  逐组件遍历:                        │
         │    "/" → "home" → "user" → "file"   │
         └────────────────────────────────────┘
              │
              ▼
   ┌──────────────────────────────┐
   │  link_path_walk() (namei.c:2574) │
   │  对每个组件调用 walk_component() │
   └──────────────────────────────┘
              │
              ▼
         ┌─────────────────────────┐
         │ 最终 dentry 指向目标文件 │
         └─────────────────────────┘
```

## 4.2 struct nameidata：路径遍历的状态载体

`struct nameidata`（定义于 `namei.c` 文件顶部附近）是整个路径查找过程的状态对象。每次路径查找开始时创建，贯穿于 `link_path_walk` 的整个生命周期，承载了所有中间上下文：

```
struct nameidata {
    struct path     path;          /* 当前遍历位置: dentry + vfsmount */
    struct qstr     last;          /* 当前组件的名字字符串 */
    struct path     root;          /* 进程的根目录 (chroot 生效时不同于 "/") */
    struct inode    *inode;        /* path.dentry->d_inode 的缓存 */
    unsigned int    seq;           /* 父 dentry 的序列号 (RCU 验证用) */
    unsigned int    next_seq;      /* 当前 dentry 的序列号 */
    unsigned int    m_seq;         /* mount_lock 序列号 */
    unsigned int    r_seq;         /* rename_lock 序列号 */
    unsigned int    depth;         /* 符号链接嵌套深度 */
    int             total_link_count; /* 符号链接总数 */
    struct saved    *stack;        /* 符号链接跳转栈 */
    unsigned        flags;         /* LOOKUP_* 标志位 */
    int             last_type;     /* 组件类型: LAST_NORM/LAST_DOT/LAST_DOTDOT */
    // ...
};
```

关键字段说明：

- **`path`**：当前已经解析到的位置，包含 `dentry`（目录项）和 `mnt`（挂载点结构）
- **`seq` / `next_seq`**：用于 RCU 模式的序列验证，是避免 TOCTOU 竞态的核心机制
- **`depth` / `stack`**：符号链接嵌套保存栈，防止无限递归
- **`total_link_count`**：累计跟踪的符号链接数量，超过 `MAXSYMLINKS` 则返回 `-ELOOP`

## 4.3 LOOKUP 标志位体系

路径查找的行为由 `nd->flags` 中的位掩码精确控制，定义在 `include/linux/namei.h` 中：

| 标志位 | 含义 |
|--------|------|
| `LOOKUP_FOLLOW` | 最后一个路径组件如果是符号链接则跟踪 |
| `LOOKUP_DIRECTORY` | 要求最终目标必须是目录 |
| `LOOKUP_AUTOMOUNT` | 触发自动挂载点 |
| `LOOKUP_EMPTY` | 允许路径为空（由特定调用方设置） |
| `LOOKUP_DOWN` | 仅向下的路径遍历（openat2 限制） |
| `LOOKUP_MORE` | 后面还有组件（非最后一个组件） |
| `LOOKUP_REVAL` | 强制重新验证所有组件 |
| `LOOKUP_RCU` | 使用 RCU 快速路径（无锁遍历） |
| `LOOKUP_NO_XDEV` | 不允许跨越挂载点边界 |
| `LOOKUP_NO_MAGICLINKS` | 不允许跟随 /proc/self/fd 这类魔数符号链接 |
| `LOOKUP_NO_SYMLINKS` | 完全禁止跟随符号链接 |
| `LOOKUP_BENEATH` | openat2 的 RESOLVE_BENEATH — 不离开起始目录 |
| `LOOKUP_IN_ROOT` | openat2 的 RESOLVE_IN_ROOT — 把 dfd 当作根 |

## 4.4 符号链接解析规则

内核对于"何时跟踪符号链接"有一套精确的规则，定义在 `namei.c:100-113` 的注释块中：

```
/* [Feb-Apr 2000 AV] Complete rewrite. Rules for symlinks:
 *	inside the path - always follow.
 *	in the last component in creation/removal/renaming - never follow.
 *	if LOOKUP_FOLLOW passed - follow.
 *	if the pathname has trailing slashes - follow.
 *	otherwise - don't follow.
 * (applied in that order).
 */
```

这五条规则按优先级排列，解释了各种边界情况：

```
例: /a/b/c   (c 是符号链接)
  ├─ open("/a/b/c")          → 不跟踪 c (无 LOOKUP_FOLLOW)
  ├─ open("/a/b/c/")         → 必须跟踪 (末尾有 '/')
  ├─ stat("/a/b/c")          → 跟踪 (stat 设置 LOOKUP_FOLLOW)
  ├─ unlink("/a/b/c")        → 不跟踪 (删除操作)
  ├─ /a/b/c/d   (c 在中间)    → 总是跟踪 c (路径内部)
  └─ open("/a/b/c/../d")     → 跟踪 c (路径内部)
```

## 4.5 link_path_walk：主循环

`link_path_walk()` 位于 `namei.c:2574`，是路径遍历的核心循环：

```c
static int link_path_walk(const char *name, struct nameidata *nd)
```

### 4.5.1 函数流程图

```
link_path_walk(name, nd)
│
├─ 跳过连续 '/'  (namei.c:2583-2587)
│    while (*name == '/') name++;
│
├─ 空路径检查 (namei.c:2588-2591)
│    if (!*name) return 0;  // 根目录
│
└─ 主循环 for(;;) (namei.c:2594-2669)
    │
    ├─ may_lookup() 权限检查 (namei.c:2600)
    │
    ├─ hash_name() 计算组件名哈希，跳过当前组件 (namei.c:2605)
    │   └─ 同时将 nd->last_type 设为:
    │      LAST_DOT, LAST_DOTDOT, 或 LAST_NORM
    │
    ├─ 如果特殊组件 (namei.c:2607-2627)
    │    "."  → LAST_DOT, ND_JUMPED 置位
    │    ".." → LAST_DOTDOT 
    │
    ├─ DCACHE_OP_HASH: 文件系统自定义哈希 (namei.c:2621-2626)
    │
    ├─ 如果 !*name → 到达末尾，跳转到 OK (namei.c:2629-2630)
    │   如果末尾是 '/' → 继续跳过 (namei.c:2635-2637)
    │
    ├─ 确定调用 walk_component (namei.c:2639-2653)
    │   ├─ 最后一个组件 → walk_component(nd, 0)
    │   └─ 非最后一个   → walk_component(nd, WALK_MORE)
    │
    ├─ 如果 walk_component 返回符号链接内容: (namei.c:2654-2661)
    │   nd->stack[depth++].name = name  保存返回点
    │   name = link                    继续遍历符号链接目标
    │   continue
    │
    └─ 检查是否目录 (namei.c:2662-2668)
       └─ !d_can_lookup → ENOTDIR
```

### 4.5.2 核心流程详解

整段主循环的精妙之处在于：

1. **组件切分由 `hash_name()` 完成**：它既是名称哈希又是组件边界检测。函数扫描字符串直到遇到 `/`，计算哈希到 `nd->last`，返回指向 `/` 后第一个字符的指针。同时还判断是否为 `.` 或 `..`。

2. **`last_type` 的区别对待**：`. 和 ..` 不进入标准 dcache 查找流程，而是由 `handle_dots()` 直接处理——`walk_component` 入口处第一条就是判断 `nd->last_type != LAST_NORM`。

3. **符号链接的嵌套处理**：`walk_component` 返回非 NULL 表示当前组件是一个符号链接，`step_into` 已经将链接内容返回。此时主循环将当前位置保存到 `nd->stack[]`，然后用链接内容替代 `name` 继续遍历，`depth` 递增。

4. **末尾组件的特殊逻辑**：当路径末尾没有更多 `/` 时（`!*name`），跳到 `OK` 标签。如果此前有符号链接栈（`depth > 0`），说明是嵌套符号链接的最后一个组件，弹出保存的位置继续；否则设置目录属性后返回 0。

## 4.6 walk_component：处理单个路径组件

`walk_component()` 位于 `namei.c:2261`，是单个组件的统一入口：

```c
static __always_inline const char *walk_component(struct nameidata *nd, int flags)
```

### 4.6.1 代码流程

```c
// namei.c:2261-2285
static __always_inline const char *walk_component(struct nameidata *nd, int flags)
{
    struct dentry *dentry;

    // 1) 处理 "." 和 ".."          (namei.c:2269-2273)
    if (unlikely(nd->last_type != LAST_NORM)) {
        if (unlikely(nd->depth) && !(flags & WALK_MORE))
            put_link(nd);
        return handle_dots(nd, nd->last_type);
    }

    // 2) 尝试 RCU 快速查找         (namei.c:2274-2276)
    dentry = lookup_fast(nd);
    if (IS_ERR(dentry))
        return ERR_CAST(dentry);

    // 3) 快速查找失败 → 慢速查找   (namei.c:2277-2281)
    if (unlikely(!dentry)) {
        dentry = lookup_slow(&nd->last, nd->path.dentry, nd->flags);
        if (IS_ERR(dentry))
            return ERR_CAST(dentry);
    }

    // 4) 释放嵌套符号链接的引用    (namei.c:2282-2283)
    if (unlikely(nd->depth) && !(flags & WALK_MORE))
        put_link(nd);

    // 5) 进入 dentry               (namei.c:2284)
    return step_into(nd, flags, dentry);
}
```

### 4.6.2 三种情况的处理

**情况 A：`.` 和 `..`**

由 `handle_dots()` 处理，不经过 dcache 查找：

```
".":
  → 当前目录的 dentry 就是 nd->path.dentry 本身
  → 直接返回 NULL (即不是符号链接，继续遍历)

"..":
  → 确保不越过当前根目录 (nd->root)
  → 如果当前 dentry 是挂载点的根 → 沿 mount 树向上
  → 获取 d_parent
  → 更新 nd->path, nd->inode, nd->seq
```

**情况 B：正则组件，RCU 快速路径**

调用 `lookup_fast()`（详见 §4.7），在 RCU 读锁保护下尝试无锁查找。如果 dentry 已在 dcache 中命中且有效，直接返回——这是最常见的高性能路径。

**情况 C：正则组件，ref-walk 慢速路径**

当 `lookup_fast()` 返回 NULL（dcache 未命中）时，调用 `lookup_slow()`（详见 §4.8），获取 inode 共享锁后调用实际文件系统的 `->lookup()`。

## 4.7 RCU-walk：无锁快速路径

RCU-walk 是内核路径查找的性能杀手锏。其核心思想是：**在不持有任何锁、不修改引用计数、不阻塞的情况下完成查找**。由于绝大多数路径查找命中 dcache，RCU-walk 可以消除锁竞争开销。

### 4.7.1 lookup_fast

`lookup_fast()` 位于 `namei.c:1838`，是 RCU 查找的主入口：

```c
// namei.c:1838-1885 (简化)
static struct dentry *lookup_fast(struct nameidata *nd)
{
    struct dentry *dentry, *parent = nd->path.dentry;
    int status = 1;

    if (nd->flags & LOOKUP_RCU) {
        // RCU 快速路径
        dentry = __d_lookup_rcu(parent, &nd->last, &nd->next_seq);
        if (unlikely(!dentry)) {
            // 未命中 → 尝试 "unlazy"：退出 RCU 转为 ref-walk
            if (!try_to_unlazy(nd))
                return ERR_PTR(-ECHILD);  // 失败，让调用方重试
            return NULL;  // 已转为 ref-walk，走慢速路径
        }

        // 验证父 dentry 序列号是否变化
        if (read_seqcount_retry(&parent->d_seq, nd->seq))
            return ERR_PTR(-ECHILD);

        status = d_revalidate(nd->inode, &nd->last, dentry, nd->flags);
        if (likely(status > 0))
            return dentry;  // 成功！
        // ... 验证失败，尝试 unlazy
    } else {
        // 已在 ref-walk 模式，使用带锁查找
        dentry = __d_lookup(parent, &nd->last);
        if (unlikely(!dentry))
            return NULL;
        status = d_revalidate(nd->inode, &nd->last, dentry, nd->flags);
    }
    // ...
}
```

### 4.7.2 序列号验证机制

RCU-walk 的核心安全保障是 **seqlock 风格的序列号验证**。每个 dentry 都有一个 `d_seq` 序列计数器，在结构被修改时递增。

```
RCU-walk 三步曲:

 Step 1: 读取父dentry的序列号  → nd->seq
          read_seqcount_begin(&parent->d_seq)

 Step 2: 执行无锁查找
          __d_lookup_rcu()  → 在 dcache 哈希表中查找
          同时获取子dentry的序列号 → nd->next_seq

 Step 3: 验证序列号
          read_seqcount_retry(&parent->d_seq, nd->seq)
          → 如果序列号变了, 说明有并发修改, 结果不可靠, 返回 -ECHILD

 Step 4: (可选) 再次验证子dentry
          read_seqcount_retry(&dentry->d_seq, nd->next_seq)
          → 在 step_into 中执行 (namei.c:2138)
```

**关键原则**：RCU-walk 允许偶尔失败（返回 `-ECHILD`），此时调用方会从 RCU 模式退出（"unlazy" 或完全重试），转为持锁的 ref-walk。这种"乐观并发控制"在争用低时几乎零开销。

### 4.7.3 RCU-walk 的失败与重试

失败路径有三种典型场景：

```
场景 1: dcache 未命中 (lookup_fast 返回 NULL)
  → try_to_unlazy(nd) 尝试退出 RCU 模式
  → 成功 → lookup_slow() 持锁查文件系统
  → 失败 → 返回 -ECHILD, 调用方从头重试

场景 2: 序列号不匹配 (read_seqcount_retry 为真)
  → 直接返回 -ECHILD
  → 调用方: unlazy_walk() → 从 link_path_walk 入口重试

场景 3: 遇到挂载点/符号链接/托管 dentry
  → step_into() 中进入 step_into_slowpath()
  → 可能 unlazy, 可能保持 RCU
  → 符号链接: 必须 unlazy
```

### 4.7.4 __d_lookup_rcu 与 d_seq

`__d_lookup_rcu` 在 `dcache.c` 中实现，其核心是**无锁地遍历 dcache 哈希桶**。由于 dentry 从哈希表中删除使用 `dentry->d_seq` 的写锁保护，读取方只需在查找后验证序列号即可确保一致性。

## 4.8 ref-walk：持锁慢速路径

当 RCU-walk 未能在 dcache 中找到目标 dentry 时，必须退化为 ref-walk（持锁查找），这意味着进入实际文件系统。

### 4.8.1 __lookup_slow

`__lookup_slow()` 位于 `namei.c:1888`，是实际文件系统查找的核心：

```c
// namei.c:1888-1922 (简化)
static struct dentry *__lookup_slow(const struct qstr *name,
                                    struct dentry *dir,
                                    unsigned int flags)
{
    struct dentry *dentry, *old;
    struct inode *inode = dir->d_inode;
    DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);

    if (unlikely(IS_DEADDIR(inode)))
        return ERR_PTR(-ENOENT);

again:
    // 1) 调用 d_alloc_parallel (namei.c:1900)
    //    并发安全的 dentry 分配——多线程查同一名字时只有一个赢
    dentry = d_alloc_parallel(dir, name, &wq);

    if (unlikely(!d_in_lookup(dentry))) {
        // 2a) 已经有其他线程完成了查找，直接验证
        int error = d_revalidate(inode, name, dentry, flags);
        // ...
    } else {
        // 2b) 我们是 "赢家"——调用实际文件系统的 lookup
        old = inode->i_op->lookup(inode, dentry, flags);  // namei.c:1915
        d_lookup_done(dentry);  // 通知等待线程
        if (unlikely(old)) {
            dput(dentry);
            dentry = old;  // 文件系统返回了一个已有 dentry
        }
    }
    return dentry;
}
```

### 4.8.2 lookup_slow / lookup_slow_killable

`lookup_slow()` 在 `__lookup_slow` 外层包了一层 `inode_lock_shared()`（共享锁），位于 `namei.c:1925`：

```c
static noinline struct dentry *lookup_slow(const struct qstr *name,
                                  struct dentry *dir,
                                  unsigned int flags)
{
    struct inode *inode = dir->d_inode;
    struct dentry *res;
    inode_lock_shared(inode);           // 持共享锁 namei.c:1931
    res = __lookup_slow(name, dir, flags);
    inode_unlock_shared(inode);         // namei.c:1933
    return res;
}
```

还有一个可中断的版本 `lookup_slow_killable()`（`namei.c:1937-1948`），用于需要考虑信号的情况。

### 4.8.3 d_alloc_parallel：并发查找的协作机制

`d_alloc_parallel()` 位于 `dcache.c:2664`，是并发路径查找的关键——当多个线程同时查找同一个名字时，避免重复劳动和竞争：

```
线程 A                        线程 B
  │                             │
  ├─ d_alloc_parallel(dir,name) ├─ d_alloc_parallel(dir,name)
  │  → 创建 dentry               │  → 发现 dentry 已在 "in_lookup" 状态
  │  → 标记 d_in_lookup          │  → 等待 wq (wait_queue_head)
  │                             │
  ├─ inode->i_op->lookup()      │  [睡眠等待]
  │  → 文件系统执行实际查找      │
  │                             │
  ├─ d_lookup_done(dentry)      │
  │  → 清除 d_in_lookup          │  → 被唤醒
  │  → 唤醒等待线程              │  → 调用 d_revalidate()
  │                             │  → 返回同一个 dentry
  │                             │
  ▼                             ▼
 同一个 dentry, 共享引用
```

这种模式在高度并发的文件服务器（如 NFS 服务器、Web 服务器）中极为重要——它避免了昂贵的文件系统操作被重复执行。

## 4.9 step_into：进入 dentry 的最终步骤

`step_into()` 位于 `namei.c:2126`，负责将遍历上下文切换到一个新的 dentry。它的返回值决定了路径遍历的下一步行为：

```c
// namei.c:2126-2149
static __always_inline const char *step_into(struct nameidata *nd, int flags,
                    struct dentry *dentry)
{
    // 快速路径: RCU 模式, 非托管dentry, 非符号链接
    if (likely((nd->flags & LOOKUP_RCU) &&
        !d_managed(dentry) && !d_is_symlink(dentry))) {
        struct inode *inode = dentry->d_inode;
        // 验证序列号
        if (read_seqcount_retry(&dentry->d_seq, nd->next_seq))
            return ERR_PTR(-ECHILD);
        if (unlikely(!inode))
            return ERR_PTR(-ENOENT);       // 负 dentry 返回 ENOENT
        nd->path.dentry = dentry;
        nd->inode = inode;
        nd->seq = nd->next_seq;
        return NULL;  // 不是符号链接, 继续遍历
    }
    return step_into_slowpath(nd, flags, dentry);
}
```

`step_into_slowpath()` 位于 `namei.c:2093`，处理更复杂的情况：

```
step_into_slowpath 的工作:

 1. 托管 dentry (d_managed): 调用 ->d_manage() 回调
    → 可能返回 -EISDIR 拒绝自动挂载点遍历

 2. 负 dentry: 返回 -ENOENT

 3. 符号链接 + LOOKUP_FOLLOW: 
    → 递归解析
    → 返回符号链接内容字符串

 4. 符号链接 + !LOOKUP_FOLLOW:
    → 如果后续还有组件 → 返回 -ELOOP (中间组件必须跟踪)
    → 如果是最后一个组件 → 直接进入链接自身 (不跟踪)

 5. 挂载点穿越: 
    → __follow_mount_rcu() 或 __follow_mount()
    → 沿着 mounted 链表走到实际挂载的根 dentry

 6. 普通目录/文件: 
    → 更新 nd->path, nd->inode, nd->seq
    → 返回 NULL (继续遍历)
```

## 4.10 符号链接的递归解析

当 `step_into_slowpath()` 检测到当前 dentry 是符号链接且需要跟踪时，调用 `pick_link()`（位于 `step_into_slowpath` 的调用链中）。符号链接解析的深度受 `MAXSYMLINKS` 硬限制，定义在 `include/linux/namei.h:14`：

```c
#define MAXSYMLINKS 40
```

解析流程：

```
nd->path = /home/user/link  (link → /data/storage/file)
  │
  ├─ step_into() 检测到符号链接
  │   │
  │   ├─ nd->total_link_count++  (namei.c:1977)
  │   │   if (>= MAXSYMLINKS) → -ELOOP
  │   │
  │   ├─ inode->i_op->get_link() 获取链接内容
  │   │   → 返回 "/data/storage/file"
  │   │
  │   ├─ nd->stack[depth] 保存跳转前的 name
  │   │
  │   └─ 返回 "/data/storage/file" 给 link_path_walk
  │
  ├─ link_path_walk 主循环:
  │   if (link != NULL) {
  │       nd->stack[depth++].name = name;  // 保存返回点
  │       name = link;                     // 替换路径
  │       continue;                        // 重新开始遍历
  │   }
  │
  └─ 最终进入 /data/storage/file
```

**嵌套符号链接的栈管理**：`nd->stack[]` 是一个固定大小的数组（通常 40 个槽，对应 MAXSYMLINKS），保存每次跳转前的 `name` 指针位置。当遍历完符号链接内容到达末尾后，`link_path_walk` 的 `OK:` 标签弹出栈中的保存点继续遍历。

## 4.11 路径查找完整流程图

```
用户调用: path_lookupat("/home/user/file", ...)
           │
           ▼
      path_init()   ← 设置 nameidata, 从根/当前目录开始
           │
           ▼
   ┌─ link_path_walk() ─────────────────────────────────┐
   │                                                    │
   │  组件循环:                                          │
   │  ┌──────────────────────────────────────┐          │
   │  │ hash_name() → 切出 "home"            │          │
   │  │ walk_component("home")              │          │
   │  │   ├─ lookup_fast()  → RCU 查找      │          │
   │  │   │   └─ 命中 → step_into() → 更新 nd│          │
   │  │   └─ 未命中 → lookup_slow()         │          │
   │  │       └─ __lookup_slow()                     │  │
   │  │          └─ inode->i_op->lookup()            │  │
   │  └──────────────────────────────────────┘          │
   │  组件循环:                                          │
   │  ┌──────────────────────────────────────┐          │
   │  │ hash_name() → 切出 "user"            │          │
   │  │ walk_component("user")              │          │
   │  │   └─ lookup_fast() → 命中 ✓          │          │
   │  └──────────────────────────────────────┘          │
   │  组件循环:                                          │
   │  ┌──────────────────────────────────────┐          │
   │  │ hash_name() → 切出 "file"            │          │
   │  │ walk_component("file")              │          │
   │  │   └─ lookup_fast() → 命中 ✓          │          │
   │  │ (最后一个组件, flags=WALK_TRAILING)   │          │
   │  └──────────────────────────────────────┘          │
   └────────────────────────────────────────────────────┘
           │
           ▼
      complete_walk()  ← 完成检查, 处理末尾挂载点
           │
           ▼
      返回 path (包含 dentry + vfsmount)
```

## 4.12 路径查找中的权限检查

`may_lookup()` 在 `namei.c:1951` 定义，对每个路径组件执行权限检查：

```c
static inline int may_lookup(struct mnt_idmap *idmap,
                             struct nameidata *restrict nd)
```

它调用 `inode_permission(idmap, nd->inode, MAY_EXEC)`，检查当前进程是否有对目录的"执行"（搜索）权限。注意这里检查的是 `MAY_EXEC` 而非 `MAY_READ`——因为遍历目录需要执行权限 `x`，而非读权限 `r`。

## 4.13 RCU-walk 与 ref-walk 对比总结

```
┌──────────────────┬─────────────────────┬─────────────────────┐
│      特性        │     RCU-walk        │     ref-walk         │
├──────────────────┼─────────────────────┼─────────────────────┤
│ 锁              │ 无 (RCU 读锁)       │ inode_lock_shared   │
│ 引用计数         │ 不操作              │ dget/dput           │
│ 可睡眠           │ 否                  │ 是                  │
│ 文件系统交互      │ 仅 dcache           │ 可调用 ->lookup()   │
│ 符号链接         │ 可检测, 需 unlazy    │ 完全支持            │
│ 性能             │ ~0.3μs (L1 命中)    │ ~3-10μs (含 I/O)   │
│ 失败处理         │ -ECHILD → 重试      │ -errno 直接返回     │
│ 序列验证         │ d_seq 检查          │ 不需要 (已经持锁)    │
└──────────────────┴─────────────────────┴─────────────────────┘
```

现代 Linux 内核在 `link_path_walk` 中默认使用 RCU-walk（设置 `LOOKUP_RCU` 标志），只有在遇到以下情况时才退化为 ref-walk：

1. dcache 未命中（需要通过文件系统查找）
2. 遇到符号链接（必须停止 RCU 并设置引用计数）
3. d_seq 验证失败（并发修改导致结果不可靠）
4. 遇到托管 dentry（`d_managed` 为真，如自动挂载点）
5. 需要调用文件系统特定函数（如 `d_hash`、`d_revalidate`）

这种"乐观执行 + 失败回退"的模式使得内核在绝大多数日常场景中（dcache 热命中）享受了无锁遍历的性能优势。

## 4.14 关于 RO 初始化数据

在 `namei.c:128-129` 可以看到内核使用 `__ro_after_init` 来保护初始化后只读的数据：

```c
static struct kmem_cache *__names_cache __ro_after_init;
#define names_cache	runtime_const_ptr(__names_cache)
```

现代内核还引入了 `runtime_const_ptr()` 宏，这是一种减少指针间接访问的优化——将运行时确定的常量指针值直接嵌入代码段，避免从 `.data` 段加载。

## 4.15 关键文件和函数索引

| 函数/定义 | 文件 | 行号 | 说明 |
|-----------|------|------|------|
| symlink rules | fs/namei.c | 100-113 | 符号链接解析规则 |
| __names_cache | fs/namei.c | 129 | filename 对象的 SLAB 缓存 |
| lookup_fast | fs/namei.c | 1838 | RCU 快速查找入口 |
| __lookup_slow | fs/namei.c | 1888 | 实际文件系统查找 |
| d_alloc_parallel | fs/dcache.c | 2664 | 并发安全的 dentry 分配 |
| lookup_slow | fs/namei.c | 1925 | 持锁慢速查找 |
| lookup_slow_killable | fs/namei.c | 1937 | 可中断的慢速查找 |
| step_into_slowpath | fs/namei.c | 2093 | 带符号链接处理的方向进入 |
| step_into | fs/namei.c | 2126 | 进入 dentry 的内联函数 |
| walk_component | fs/namei.c | 2261 | 单组件处理入口 |
| link_path_walk | fs/namei.c | 2574 | 主路径遍历循环 |
| path_init | fs/namei.c | 2673 | 初始化 nameidata |
| MAXSYMLINKS | include/linux/namei.h | 14 | 符号链接最大深度=40 |

---

## 下一篇文章

[第五篇：文件操作与文件描述符表 — open 到 close 的完整生命周期](./05-file-ops.md)
