# 第一篇：Dentry Cache — 目录项缓存与 RCU 无锁查找

> 源码：`fs/dcache.c` (3411行) | 头文件：`include/linux/dcache.h` (635行)

系列目录：[VFS 内核源码深度解析](./README.md)

---

## 1. 什么是 dentry

dentry（directory entry）是 VFS 层的**纯内存对象**，不存储于磁盘。它代表路径中的一个组件（component），连接"文件名"到"inode"。例如 `/home/user/file` 包含 4 个 dentry：`/`、`home`、`user`、`file`。

核心设计目标：
- **快速路径名解析**：字符串比较 → dentry 查找 → inode 访问
- **缓存"不存在"结果**（negative dentry，d_inode == NULL），避免查盘
- **RCU 无锁读**：通过 `d_seq` seqlock 检测并发修改，读方完全无锁
- **高效引用计数**：`d_lockref` 融合锁和引用计数，单次 cmpxchg 原子操作

```
路径 /home/user/file 的 dentry 树:

dentry("/")    d_inode → root inode
  │ d_children   (hlist_head)
  ├── dentry("home")   d_inode → inode of /home/
  │     │ d_children
  │     ├── dentry("user")  d_inode → inode of /home/user/
  │     │     │ d_children
  │     │     ├── dentry("file")  d_inode → inode for /home/user/file
  │     │     └── dentry(".bashrc")
  │     └── dentry("opt")
  │           ...
  └── dentry("etc")
        d_parent → dentry("/")
        ...
```

每个 dentry 通过 `d_sib`（兄弟指针）挂在父的 `d_children` 链表上。`d_parent` 指回父 dentry，`d_inode` 指向对应的 inode（NULL = negative）。

---

## 2. struct dentry 字段解析 (dcache.h:93-144)

```c
struct dentry {
    /* RCU lookup touched fields — 第一cacheline(64B)专用 */
    unsigned int d_flags;           // 95: 标志位，受 d_lock 保护
    seqcount_spinlock_t d_seq;      // 96: per-dentry seqlock! RCU查找核心
    struct hlist_bl_node d_hash;    // 97: 哈希桶链表节点
    struct dentry *d_parent;        // 98: 父目录 dentry
    union {
        struct qstr __d_name;       // 100: 内嵌名字(短名≤DNAME_INLINE_LEN-1)
        const struct qstr d_name;   // 101: qstr = {hash, len, name指针}
    };
    struct inode *d_inode;          // 103: 关联inode, NULL=negative dentry
    union shortname_store d_shortname; // 105: 内嵌名字缓冲区

    /* cacheline 1 boundary (64 bytes) */

    /* Ref lookup also touches following */
    const struct dentry_operations *d_op; // 109: 文件系统自定义钩子
    struct super_block *d_sb;       // 110: 所属超级块(dentry树的根)
    unsigned long d_time;           // 111: d_revalidate 时间戳
    void *d_fsdata;                 // 112: 文件系统私有数据(如NFS的fh)

    /* cacheline 2 boundary (128 bytes) */

    struct lockref d_lockref;       // 114: 融合锁+计数! 独立cacheline

    union {
        struct list_head d_lru;        // 120: LRU回收链表
        wait_queue_head_t *d_wait;     // 121: in-lookup等待队列
    };
    struct hlist_node d_sib;        // 123: 兄弟节点(父的d_children上)
    struct hlist_head d_children;   // 124: 子dentry链表头

    /* 以下成员共用内存，使用场景互斥 */
    union {                         // 129-143:
        struct hlist_node d_alias;          // 131: positive: inode别名链
        struct hlist_bl_node d_in_lookup_hash; // 132: in-lookup negative
        struct rcu_head d_rcu;              // 135: killed: RCU延迟释放
        struct completion_list *waiters;     // 142: live non-in-lookup negative
    };
};
```

### 关键字段深入

**cacheline 布局至关重要**。前64字节精心排列——`d_flags -> d_seq -> d_hash -> d_parent -> d_name -> d_inode -> d_shortname`——确保RCU查找仅触及一个cacheline（加上 d_name 名字数据在另一个地址）。`d_lockref` 独占 cacheline 2（128B边界），避免与RCU读区产生 false sharing。

**d_seq (line 96)**：`seqcount_spinlock_t` 类型，每个 dentry 一个序列锁。RCU 查找时：
1. 调用 `raw_seqcount_begin(&dentry->d_seq)` 获取当前序列号
2. 读取 `d_parent`、`d_name`、`d_inode` 等字段
3. 调用 `read_seqcount_retry(&dentry->d_seq, seq)`：如果序列号变化→重试
4. 写方在修改任何RCU可见字段前：`spin_lock(&dentry->d_lock)` + `write_seqcount_begin` + 修改 + `write_seqcount_end` + `spin_unlock`

**d_lockref (line 114)**：`struct lockref` 将自旋锁和引用计数融合进一个 64 位变量。低 32 位为 spinlock（值 0 或 1），高 32 位为计数。`lockref_get_not_zero()` 只需一次 cmpxchg（如果锁未持有），路径查找中极为高效。

**d_alias (line 131)**：硬链接场景——多个 dentry 指向同一个 inode。`d_alias` 将 dentry 链入 `inode->i_dentry` 的 hlist 中。

**d_flags 常用标志**：
- `DCACHE_LRU_LIST` — 在 LRU 链表上
- `DCACHE_PAR_LOOKUP` — 正在并行查找中（d_alloc_parallel）
- `DCACHE_SHRINK_LIST` — 在 shrinker 收缩列表上
- `DCACHE_DISCONNECTED` — 未连接到根的匿名 dentry
- `DCACHE_OP_HASH` — 使用文件系统自定义哈希
- `DCACHE_OP_COMPARE` — 使用文件系统自定义比较
- `DCACHE_DENTRY_KILLED` — 已被杀死，等待 RCU 释放
- `DCACHE_NORCU` — 不使用 RCU 路径释放

---

## 3. Dentry 状态机

```
                    __d_alloc() slab分配
                          │
                          ▼
                   ┌─────────────┐
                   │  ALLOCATED   │ d_parent=self, d_inode=NULL
                   │  游离状态     │ lockref_init → count=1
                   └──────┬──────┘
                          │ d_add() / d_instantiate()
                          │ d_alloc() + hash插入
                          ▼
                   ┌─────────────┐
                   │   HASHED     │ 已在 dentry_hashtable 中
                   │  可被查找     │
                   └──────┬──────┘
                     ┌────┴────┐
               d_inode!=NULL   d_inode==NULL
                     │              │
                     ▼              ▼
              ┌──────────┐  ┌──────────────┐
              │ POSITIVE │  │  NEGATIVE     │ 缓存"文件不存在"
              │ (in use) │  │ d_inode=NULL  │ 快速返回 ENOENT
              └────┬─────┘  └──────┬────────┘
                   │               │
                   │ 最后一个dput()  │ d_delete() 或 淘汰
                   │ d_delete()     │
                   ▼               ▼
              ┌────────────────────────────┐
              │        UNHASHED            │  __d_drop() 移出哈希表
              │  DCACHE_DENTRY_KILLED      │
              └────────────┬───────────────┘
                           │ dentry_kill() → 等待 RCU grace period
                           ▼
              ┌────────────────────────────┐
              │         FREED              │  kmem_cache_free(dentry_cache, ...)
              └────────────────────────────┘
```

几个关键转换函数：
- `__d_alloc` (dcache.c:1801)：slab 分配 → lockref_init → seqcount_init → d_parent=self → d_inode=NULL
- `d_add(dentry, inode)`：将 dentry 与 inode 关联，加入哈希表
- `d_instantiate(dentry, inode)`：仅关联 inode，不加入哈希表（需后续 d_rehash）
- `__d_drop(dentry)`：从哈希桶移除（unhash），d_inode 保持不变
- `d_delete(dentry)`：d_inode → NULL，再 __d_drop
- `dentry_kill(dentry)`：设置 KILLED 标志，等待 RCU → 释放内存

---

## 4. Dentry 哈希表 (dcache.c:112-131)

```c
// fs/dcache.c:113-121
static unsigned int d_hash_shift __ro_after_init __used;
static struct hlist_bl_head *dentry_hashtable __ro_after_init __used;

static inline struct hlist_bl_head *d_hash(unsigned long hashlen)
{
    return runtime_const_ptr(dentry_hashtable) +
        runtime_const_shift_right_32(hashlen, d_hash_shift);
}
```

初始化在 `dcache_init_early` / `dcache_init` 中：
- 表大小 = `1 << d_hash_shift`，根据系统内存动态计算（`d_hash_shift = 32 - order_base_2(total_pages)`）
- 使用 `hlist_bl_head`（bit-lock 哈希链表头）。每个桶有一个独立的位自旋锁，嵌入在链表头指针的最低位——获取桶锁时只需 `hlist_bl_lock(head)`，它位原子地设置锁位
- 哈希输入是 `name->hash_len`（qstr 的 hash+len 打包为 u64）
- `runtime_const_ptr` / `runtime_const_shift_right_32` 是内核性能优化：根据架构不同，可能用立即数编码 shift 和 base 来避免 load

### in_lookup 辅助哈希表

```c
// fs/dcache.c:123-131
#define IN_LOOKUP_SHIFT 10
static struct hlist_bl_head in_lookup_hashtable[1 << IN_LOOKUP_SHIFT];

static inline struct hlist_bl_head *in_lookup_hash(const struct dentry *parent,
                                                   unsigned int hash)
{
    hash += (unsigned long) parent / L1_CACHE_BYTES;
    return in_lookup_hashtable + hash_32(hash, IN_LOOKUP_SHIFT);
}
```

1024 个桶的辅助哈希表，专用于 `d_alloc_parallel` 并发控制。哈希键 = 名字hash + 父指针/L1_CACHE_BYTES，确保同一目录中不同父对象（硬链接）获得不同桶。

---

## 5. 锁定规则 (dcache.c:40-75)

```
使用规则:
  dentry->d_inode->i_lock 保护:
    — i_dentry, d_alias, d_inode of aliases
  dcache_hash_bucket lock 保护:
    — the dcache hash table (dentry_hashtable)
  s_roots bl list spinlock 保护:
    — the s_roots 链表 (see __d_drop)
  dentry->d_sb->s_dentry_lru_lock 保护:
    — LRU 链表和计数器 (d_lru)
  d_lock 保护:
    — d_flags, d_name, d_lru, d_count
    — d_unhashed()
    — d_parent 和 d_children
    — children 的 d_sib 和 d_parent
    — d_alias, d_inode

锁的顺序:
  dentry->d_inode->i_lock
    dentry->d_lock
      dentry->d_sb->s_dentry_lru_lock
      dcache_hash_bucket lock
      s_roots lock

  祖先关系(同一路径链):
    dentry->d_parent->...->d_parent->d_lock
      ...
        dentry->d_parent->d_lock
          dentry->d_lock
  无祖先关系:
    由 rename_lock 序列化，顺序随意
```

核心要点：
- **i_lock 在最外层**：先锁 inode，再锁 dentry，最后锁哈希桶或 LRU
- **祖先关系保证**：从根到叶按序加锁，父锁先于子锁
- **无祖先关系**：由 `rename_lock` seqlock 完全序列化，任意顺序都安全
- **d_lock 保护最多字段**：d_flags, d_name, d_lru, d_count, d_parent, d_children, d_sib, d_alias, d_inode

---

## 6. rename_lock 序列锁 (dcache.c:85-87)

```c
__cacheline_aligned_in_smp DEFINE_SEQLOCK(rename_lock);
EXPORT_SYMBOL(rename_lock);
```

全局 seqlock，序列化跨目录 rename 操作。在 `d_path()` 等路径反解函数中的使用模式：

```c
// dcache.c 中 d_path 路径反解:
read_seqbegin_or_lock(&rename_lock, &seq);
// ... 遍历 dentry → d_parent 链构建路径 ...
// 如果遍历中发生了 rename:
if (need_seqretry(&rename_lock, seq)) {
    // 获取写锁，重新遍历
}
```

- **读方**（d_path, __d_lookup_rcu 等调用者）：使用 `read_seqbegin` 乐观读
- **写方**（d_move）：`write_seqlock(&rename_lock)` 阻止并发
- 这是"无祖先关系 dentry 锁顺序随意"的根本原因——它们被 rename_lock 序列化了

---

## 7. d_alloc 系列：dentry 分配

### __d_alloc (dcache.c:1801-1873)

所有公开分配接口的核心。关键实现步骤：

```c
static struct dentry *__d_alloc(struct super_block *sb, const struct qstr *name)
{
    // 1. 从 dentry_cache slab 分配，直接关联到 sb 的 LRU
    dentry = kmem_cache_alloc_lru(dentry_cache, &sb->s_dentry_lru, GFP_KERNEL);

    // 2. 名字处理三态:
    //    - name == NULL:  使用 slash_name ("/"), 内嵌存储
    //    - len ≤ DNAME_INLINE_LEN-1: 短名, 内嵌在 d_shortname 中
    //    - len > DNAME_INLINE_LEN-1: 长名, 额外分配 external_name
    //      (包含引用计数 atomic_t count, 多个 dentry 可共享)

    // 3. smp_store_release 保证名字指针和 NUL 对其它 CPU 可见
    smp_store_release(&dentry->__d_name.name, dname);

    // 4. 初始化
    dentry->d_flags = 0;
    lockref_init(&dentry->d_lockref);
    seqcount_spinlock_init(&dentry->d_seq, &dentry->d_lock);
    dentry->d_inode = NULL;           // 初始为 negative
    dentry->d_parent = dentry;        // 初始指向自己（后续修正）
    dentry->d_sb = sb;
    dentry->d_op = sb->__s_d_op;      // 继承 superblock 的操作表
    dentry->d_flags = sb->s_d_flags;  // 继承 superblock 的标志
    INIT_HLIST_HEAD(&dentry->d_children);
    INIT_LIST_HEAD(&dentry->d_lru);
    INIT_HLIST_NODE(&dentry->d_sib);

    // 5. 文件系统自定义初始化
    if (dentry->d_op && dentry->d_op->d_init) {
        err = dentry->d_op->d_init(dentry);
        if (err) { /* 清理, 返回 NULL */ }
    }
    this_cpu_inc(nr_dentry);
    return dentry;
}
```

### d_alloc (dcache.c:1884-1899)

```c
struct dentry *d_alloc(struct dentry *parent, const struct qstr *name)
{
    struct dentry *dentry = __d_alloc(parent->d_sb, name);
    if (!dentry) return NULL;

    spin_lock(&parent->d_lock);
    dentry->d_parent = dget_dlock(parent);        // 原子引用+父指针
    hlist_add_head(&dentry->d_sib, &parent->d_children); // 加入兄弟链表
    spin_unlock(&parent->d_lock);
    return dentry;
}
```

`dget_dlock` 是 lockref 优化：在持父 d_lock 时安全增加引用计数。

### d_alloc_parallel (dcache.c:2664-2743)

这是路径查找中**最关键**的创建函数，处理两个 CPU 同时查找同一名字的竞态：

```
CPU 0: lookup("file")          CPU 1: lookup("file")
  __d_lookup_rcu → NULL          __d_lookup_rcu → NULL
  d_alloc_parallel:              d_alloc_parallel:
    __d_alloc(name="file")         __d_alloc(name="file")
    设DCACHE_PAR_LOOKUP            retry循环:
    挂入parent->d_children           RCU查找 → 找到CPU0的dentry!
    retry循环:                       lockref_get_not_dead → 获取引用
      RCU查找 → NULL                 等待CPU0的查找完成(如果还在进行)
      检查rename_lock                返回同一个dentry ←── 避免了磁盘I/O!
      检查i_dir_seq为偶数
      锁in_lookup_hash桶
      再次检查 → 无人查找
      插入in_lookup_hashtable
      返回new(含PAR_LOOKUP)
    → 调用者执行文件系统lookup
    → 获得inode, d_splice_alias
    → 清除DCACHE_PAR_LOOKUP
    → 唤醒CPU 1上的等待者
```

内部 retry 循环的竞争检测：
```c
retry:
    rcu_read_lock();
    seq = smp_load_acquire(&parent->d_inode->i_dir_seq); // 目录版本号
    r_seq = read_seqbegin(&rename_lock);
    dentry = __d_lookup_rcu(parent, name, &d_seq);
    if (dentry) {                    // 已有dentry！放弃新创建的
        lockref_get_not_dead(...);
        read_seqcount_retry(...) → 重试;
        dput(new);
        return dentry;
    }
    if (read_seqretry(&rename_lock, r_seq)) goto retry;
    if (seq & 1) goto retry;         // 目录在写锁中
```

### d_alloc_anon (dcache.c:1902)

为 NFS 文件句柄查找等场景创建"悬浮"dentry：无父节点，`DCACHE_DISCONNECTED`。通过 `__d_instantiate_anon` 绑定到 inode 的别名链表。

### d_alloc_pseudo (dcache.c:1933)

为 pipefs、sockfs、anon_inodefs 的内部对象创建 dentry。不加哈希表（不可被路径查找发现），d_parent 指向自己，设 `DCACHE_NORCU`。

---

## 8. __d_lookup_rcu — RCU 无锁查找 (dcache.c:2366-2441)

```c
struct dentry *__d_lookup_rcu(const struct dentry *parent,
        const struct qstr *name, unsigned *seqp)
{
    u64 hashlen = name->hash_len;
    struct hlist_bl_head *b = d_hash(hashlen);
    struct dentry *dentry;

    // 文件系统自定义比较(如大小写不敏感)→走慢路径
    if (unlikely(parent->d_flags & DCACHE_OP_COMPARE))
        return __d_lookup_rcu_op_compare(parent, name, seqp);

    hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
        unsigned seq;

        // ★ raw_seqcount_begin: 不等待稳定！如果正在写(奇数)直接跳过
        seq = raw_seqcount_begin(&dentry->d_seq);
        if (dentry->d_parent != parent)      // 快速父指针检查
            continue;
        if (dentry->d_name.hash_len != hashlen) // hash+len相等?
            continue;
        if (unlikely(dentry_cmp(dentry, str, hashlen_len(hashlen)) != 0))
            continue;                        // 精确字符串比较
        if (unlikely(d_unhashed(dentry)))    // 已被移出哈希表?
            continue;
        *seqp = seq;                         // 返回d_seq给调用者
        return dentry;
    }
    return NULL;
}
```

**调用者的正确用法**：

```c
rcu_read_lock();
seq = read_seqbegin(&rename_lock);
dentry = __d_lookup_rcu(parent, name, &d_seq);
if (dentry) {
    inode = dentry->d_inode;
    // ... 使用 inode ...
    if (read_seqcount_retry(&dentry->d_seq, d_seq))
        goto retry;                          // d_seq 变了!
    // inode 仍然有效（有RCU保护）
}
if (read_seqretry(&rename_lock, seq))
    goto retry;                              // rename 发生了
rcu_read_unlock();
```

关键设计选择：
- **`raw_seqcount_begin` (不等待)**：如果 seqlock 正在写（奇数），立即放弃此 dentry 继续遍历。这比 `read_seqcount_begin`（等待偶数）更高效，因为在 RCU 遍历中遇到锁冲突直接跳过就行
- **`hlist_bl_for_each_entry_rcu`**：RCU 安全的遍历，链表的插入和删除通过 rcu_assign_pointer 和 call_rcu 保证可见性
- **调用者负责验证 d_seq**：不验证的 dentry 可能已被 d_move 修改了 d_name 或 d_parent
- **最快路径**：常见场景下整个 lookup 不获取任何锁、不写任何 cacheline——仅仅是一些 RCU load + seqlock read

### d_lookup (dcache.c:2445)

持锁版本：获取哈希桶 bit-lock 确保精确（不会被 rename 干扰），用于需要百分之百确定性的场景。

---

## 9. dput — 引用计数释放 (dcache.c:966-978)

```c
void dput(struct dentry *dentry)
{
    if (!dentry) return;
    might_sleep();
    rcu_read_lock();
    if (likely(fast_dput(dentry))) {    // 快速路径: count > 1
        rcu_read_unlock();
        return;
    }
    finish_dput(dentry);                // 慢速路径: count == 1
}
EXPORT_SYMBOL(dput);
```

**fast_dput**：`lockref_put_return` 原子递减计数，如果结果 > 0 直接返回
**finish_dput**：最后一个引用释放 → `retain_dentry` 判断保留还是杀死 → 如果需要 kill → 经 RCU grace period → `__d_free`

### dput_to_list (dcache.c:1001-1011)

批量回收版本：收集待回收 dentry 到链表，批量调用 `shrink_dentry_list`，减少锁获取次数。

### dget_parent (dcache.c:1013)

安全获取父 dentry 引用（父可能正在被 rename 替换）：
```c
// 乐观 RCU 路径:
rcu_read_lock();
seq = raw_seqcount_begin(&dentry->d_seq);
ret = READ_ONCE(dentry->d_parent);
gotref = lockref_get_not_zero(&ret->d_lockref);
rcu_read_unlock();
if (likely(gotref)) {
    if (!read_seqcount_retry(&dentry->d_seq, seq))
        return ret;           // 成功！父未变
    dput(ret);                // seq变了，父可能已换
}
// 保守路径: spin_lock(d_lock) + 重检查
```

---

## 10. d_move — 重命名核心 (dcache.c)

`d_move` 处理文件重命名的核心逻辑：
```c
void d_move(struct dentry *dentry, struct dentry *target)
{
    write_seqlock(&rename_lock);        // 全局排他
    // 从旧哈希桶删除 → 更新 d_parent → 加入新父的 d_children
    // → 重新哈希 → 递增 d_seq → write_sequnlock(&rename_lock)
}
```

写方必须：持 `d_lock` → 修改字段 → `write_seqcount_begin` → `write_seqcount_end` → （读方通过 d_seq retry 重新读取）

---

## 11. Negative dentry

`d_inode == NULL` 的 dentry，表示"此名字对应的文件不存在"（dcache.h:103）。

三种子状态：
1. **hashed negative**：在哈希表中，lookup 能命中，快速返回 -ENOENT
2. **in-lookup negative**：DCACHE_PAR_LOOKUP 标志，正在等待文件系统创建 inode
3. **killed negative**：DCACHE_DENTRY_KILLED，等待 RCU 释放

重要性：
- stat("/nonexistent") → 创建 negative dentry → 下次 stat 直接在缓存返回 ENOENT，不查盘
- 统计：`nr_dentry_negative` per-CPU 计数器（dcache.c:144）
- `dentry_negative_policy`（dcache.c:145）控制是否积极回收
- `dentry_stat_t`（dcache.c:133-140）导出全局统计到 /proc/sys/fs/dentry-state

---

## 12. LRU 回收与缓存压力 (dcache.c:76-82)

```c
static int sysctl_vfs_cache_pressure __read_mostly = 100;
static int sysctl_vfs_cache_pressure_denom __read_mostly = 100;

unsigned long vfs_pressure_ratio(unsigned long val)
{
    return mult_frac(val, sysctl_vfs_cache_pressure,
                     sysctl_vfs_cache_pressure_denom);
}
EXPORT_SYMBOL_GPL(vfs_pressure_ratio);
```

- `vfs_cache_pressure`：默认 100。值越大越积极回收（如 200 = 扫描量翻倍）
- `vfs_pressure_ratio(val) = val * pressure / 100`
- 注册为 slab shrinker，内存压力时内核调用回调
- `d_lru_add` / `d_lru_del` / `d_shrink_add` 管理 dentry 在 LRU/shrink 列表间的移动
- `dentry_stat.age_limit`：默认 45 秒，越老的 dentry 越可能被回收

回收判断 (`dentry_kill` 流程)：
1. `d_lockref.count != 0` → 有人还在用，不回收
2. `!hlist_empty(&dentry->d_children)` → 有子 dentry，不回收
3. `retain_dentry()` 返回 true → 保留（如被临时 pin 住）
4. 否则 → `__dentry_kill` → `dentry_unlist` → `dentry_free` (RCU 后)

---

## 13. d_walk — 子树遍历 (dcache.c)

`d_walk` 函数遍历 dentry 子树（递归处理），用于 shrink（`shrink_dcache_parent`）、flush 等批量操作。特点是：
- 通过序列号检测重命名，避免在 rename 中死锁
- 支持回调（`d_walk_fn`），每个 dentry 调用一次
- `shrink_dcache_parent` 常用场景：目录删除前先收缩子树

---

## 14. dentry_operations — 文件系统钩子 (dcache.h:163-174)

```c
struct dentry_operations {
    int (*d_revalidate)(struct inode *, const struct qstr *,
                        struct dentry *, unsigned int);
    int (*d_weak_revalidate)(struct dentry *, unsigned int);
    int (*d_hash)(const struct dentry *, struct qstr *);
    int (*d_compare)(const struct dentry *, unsigned int,
                     const char *, const struct qstr *);
    int (*d_delete)(const struct dentry *);
    int (*d_init)(struct dentry *);
    void (*d_release)(struct dentry *);
};
```

| 钩子 | 影响 | 典型用户 |
|------|------|----------|
| d_revalidate | 重新验证有效性（可触发网络I/O） | NFS 检查服务端状态 |
| d_weak_revalidate | 轻量验证，无网络I/O | NFS |
| d_hash | 自定义哈希计算 | FAT (大小写不敏感) |
| d_compare | 自定义名字比较 | FAT、CIFS |
| d_delete | 返回1就删除 dentry | — |
| d_init / d_release | 分配/释放时的回调 | — |

关键标志：
- `DCACHE_OP_HASH`：如果设置，d_hash 计算调用 `d_op->d_hash`
- `DCACHE_OP_COMPARE`：如果设置，名字比较走 `d_op->d_compare`。**这会绕开 `__d_lookup_rcu` 的快速路径**，转而走 `__d_lookup_rcu_op_compare`

---

## 15. 总结

| 概念 | 要点 |
|------|------|
| dentry | 纯内存对象，连接文件名→inode，不存盘 |
| d_seq | per-dentry seqlock，RCU 查找的核心保障 |
| d_lockref | 融合锁+引用计数，单次 cmpxchg 原子操作 |
| 哈希表 | dentry_hashtable，按 name->hash_len 索引，bit-lock 桶 |
| in_lookup 表 | 1024 桶辅助表，d_alloc_parallel 的并发控制 |
| 锁定顺序 | i_lock → d_lock → lru_lock / hash_bucket_lock |
| negative dentry | d_inode==NULL，缓存"不存在"，避免查盘 |
| rename_lock | 全局 seqlock，序列化跨目录 rename |
| __d_lookup_rcu | RCU 完全无锁，d_seq 验证一致性 |
| d_alloc_parallel | 两 CPU 同查一个名字→共享一个 dentry→一次磁盘读 |
| vfs_cache_pressure | sysctl 参数，控制 LRU 回收激进程度 |
| cacheline 布局 | RCU 字段 64B→操作 64B→lockref 64B，精心优化 |

dentry cache 是 VFS 第一道防线——在"缓存命中率"和"内存占用"之间取得平衡。RCU 无锁查找使其在高并发路径查找中表现出色（想象 `find /` 遍历百万文件），而 negative dentry 则避免了大量无意义的磁盘 I/O。

---

## 下一篇文章

➡️ [第二篇：Inode 生命周期 — 创建、查找、引用计数与回收](./02-inode.md)
