# 第五篇：目录操作与 HTree 索引 — 从线性搜索到 B 树

> 源码：fs/ext4/namei.c (4249 lines) · fs/ext4/ialloc.c (1626 lines)
> 头文件：fs/ext4/ext4.h · fs/ext4/fsmap.h

系列目录：[ext4 内核源码深度解析](./README.md)

---

## 1. 概述

目录是文件系统的命名空间基石。小型 ext4 目录（< 2 个块）用简单的线性链表存储条目，而较大的目录则使用 **HTree（Hashed Tree）** 索引——一种基于 B 树变体的索引结构，将 O(n) 的线性查找优化为 O(log n)。

HTree 是 ext3 时代引入的关键创新，让 ext4 在处理数百万文件级别的目录时依然保持高效的查找性能。核心思路：对文件名做哈希，用哈希值构建一棵深度为 1 或 2 的 B 树索引。

本篇将逐层剖析 ext4 目录操作的完整实现：

| 函数 | 文件:行号 | 功能 |
|------|-----------|------|
| `ext4_lookup` | namei.c:1760 | 目录查找入口 |
| `ext4_create` | namei.c:2813 | 创建普通文件 |
| `ext4_mkdir` | namei.c:2995 | 创建子目录 |
| `ext4_append` | namei.c:53 | 扩展目录块 |
| `ext4_dir_inode_operations` | namei.c:4222 | VFS 操作表 |

---

## 2. ext4 目录项结构

ext4 的目录项是变长结构，每个条目代表一个文件或子目录：

```
ext4_dir_entry_2 结构 (磁盘格式):

┌─────────┬─────────┬──────────┬─────────┬────────┬──────────┐
│ inode   │ rec_len │ name_len │file_type│ name[] │  padding │
│ (4bytes)│(2bytes) │ (1byte)  │ (1byte) │(变长)  │ (变长)   │
└─────────┴─────────┴──────────┴─────────┴────────┴──────────┘

字段说明:
  inode     — 32位 inode 号
  rec_len   — 整个条目长度（包含 padding），用于遍历链表
  name_len  — 文件名字节数
  file_type — 文件类型代码: 1=regular, 2=dir, 3=char, 4=block,
              5=fifo, 6=socket, 7=symlink
  name[]    — 文件名，最多 EXT4_NAME_LEN (255) 字节
```

**删除条目的处理**：删除一个条目时，`rec_len` 会合并到前一个条目中，形成"空洞"。这是 ext2 时代的经典设计，简单高效。

**file_type 字段**是 ext4 的特性，让 `readdir()` 不必为每个条目调用 `stat()` 来获取类型——这对 `ls -F` 和 `find` 等操作有显著的性能提升。

---

## 3. 线性目录 vs HTree 索引

### 3.1 线性查找

小目录（≤ 2 个块）直接使用线性搜索：逐字节扫描 `ext4_dir_entry_2` 链表，比对文件名。

```c
// 伪代码：线性搜索
for (offset = 0; offset < dir->i_size; offset += rec_len) {
    de = (ext4_dir_entry_2 *)(bh->b_data + offset);
    if (de->inode && match_name(de->name, de->name_len, target))
        return de;
    // 下一个条目: offset = de->rec_len
}
```

即使只有 2 个块（8KB with 4KB blocks），也能存放数百个短文件名。

### 3.2 HTree 索引

当目录增大超过 2 个块时，ext4 会启用 HTree 索引。HTree 在目录数据块中嵌入索引块（dx_node），形成如下结构：

```
HTree 目录布局:

  ┌─────────────────────┐
  │   block 0: dx_root  │  ← 根索引块 (也包含 "." 和 "..")
  │   hash=0  entries:  │
  │   count, limit,     │
  │   dx_entry[0..N]    │
  └──────┬──────────────┘
         │
    ┌────┴─────────────┐
    ▼                  ▼
┌────────┐          ┌────────┐
│ dx_node│          │ dx_node│  ← 中间索引块
│ block 1│          │ block 2│
│ hash   │          │ hash   │
│ entries│          │ entries│
└───┬────┘          └───┬────┘
    │                   │
    ▼                   ▼
┌────────┐          ┌────────┐
│ leaf   │          │ leaf   │  ← 叶块: 含 ext4_dir_entry_2
│ block 3│          │ block 4│     线性扫描
│ de[0]  │          │ de[0]  │
│ de[1]  │          │ de[1]  │
│ de[2]  │          │ de[2]  │
│ ...    │          │ ...    │
└────────┘          └────────┘
```

**HTree 属性**：
- 深度 1（root→leaves）或 2（root→interior nodes→leaves）
- 每个索引块包含 `count` 个 `dx_entry`，每个 dx_entry 指向一个子块
- 叶块包含普通的 `ext4_dir_entry_2`，线性搜索查找目标条目
- 哈希碰撞通过将冲突条目放入同一叶块或分裂块来处理

---

## 4. 目录块类型提示：dirblock_type_t

`dirblock_type_t`（namei.c:117-119）是一个枚举，在读取目录块时提供类型提示，用于安全检查：

```c
// namei.c:117-119
typedef enum {
    EITHER,         // 不确定是索引块还是数据块
    INDEX,          // 预期是 HTree 索引块 (dx_root/dx_node)
    DIRENT,         // 预期是普通目录项块
    DIRENT_HTREE    // HTree 叶块，有额外的校验
} dirblock_type_t;
```

通过 `ext4_read_dirblock` 宏使用：
```c
// namei.c:124-128
static struct buffer_head *__ext4_read_dirblock(struct inode *inode,
    ext4_lblk_t block, dirblock_type_t type,
    const char *func, unsigned int line)
```

类型提示影响校验方式：
- `INDEX` 块 → 验证 dx_root/dx_node 的幻数和校验和
- `DIRENT` 块 → 验证目录项的 rec_len 链完整性
- `DIRENT_HTREE` → 额外验证 fscrypt 密文/明文一致性

---

## 5. 目录查找：ext4_lookup

`ext4_lookup`（namei.c:1760）是 VFS `.lookup` 操作的实现，每当内核需要解析路径中一个分量时调用。

```c
// namei.c:1760
static struct dentry *ext4_lookup(struct inode *dir,
    struct dentry *dentry, unsigned int flags)
```

**完整流程**：

```
ext4_lookup(dir, dentry, flags)
  │
  ├─[1] 检查文件名长度 <= EXT4_NAME_LEN (255)             (line 1766)
  │     超长 → -ENAMETOOLONG
  │
  ├─[2] ext4_lookup_entry(dir, dentry, &de)              (line 1769)
  │     ├─ ext4_find_entry(dir, name, &de) — 核心查找
  │     │   ├─ [linear]  小目录: 逐块线性扫描
  │     │   ├─ [HTree]   大目录: hash→dx_root→dx_node→leaf
  │     │   └─ [inline]  inline data: 在 inode 内查找
  │     └─ 返回 buffer_head 和目录项指针
  │
  ├─[3] 提取 inode 号: ino = le32_to_cpu(de->inode)      (line 1774)
  │
  ├─[4] 验证: ino 合法性 (ext4_valid_inum)                (line 1776)
  │     ino 不能等于 dir 自身（循环链接检测）              (line 1780)
  │
  ├─[5] ext4_iget(sb, ino, EXT4_IGET_NORMAL)              (line 1785)
  │     从磁盘加载 inode 到内存
  │     处理 ESTALE（陈旧的删除 inode 引用）               (line 1786)
  │
  ├─[6] fscrypt 检查: 加密目录与子 inode 上下文一致性      (line 1792-1800)
  │     IS_ENCRYPTED(dir) && (S_ISDIR|S_ISLNK)
  │     → fscrypt_has_permitted_context(dir, inode)
  │     不一致 → -EPERM
  │
  ├─[7] Unicode 大小写折叠检查 (IS_CASEFOLDED)             (line 1803-1810)
  │     未找到 inode 且目录支持 casefold:
  │     返回 NULL 防止负 dentry 缓存
  │     （未来会调用 d_add_ci 做更精确的缓存）
  │
  └─[8] d_splice_alias(inode, dentry)                     (line 1812)
        将找到的 inode 挂接到 dentry 上
        处理别名（硬链接）情况
```

**HTree 查找过程**：

```
ext4_find_entry(dir, name, &de, NULL)
  │
  ├─[HTree + 加密] fscrypt → 密文名
  │  ext4_fname_prepare(dir, name, &fname)
  │
  ├─[HTree 目录]
  │  ext4_dx_find_entry():
  │  1. 读取 dx_root (block 0)
  │  2. hash = ext4fs_dirhash(name, fname, s_hash_seed)
  │  3. 在 dx_root->entries[] 中二分查找
  │     entries[n].hash <= hash < entries[n+1].hash
  │     → 定位到 entries[n].block
  │  4. 如果 entries[n] 指向 dx_node:
  │     读取该 dx_node
  │     在 entries[] 中再次二分 → 定位叶块
  │  5. 读取叶块
  │  6. 在叶块中线性扫描 ext4_dir_entry_2
  │     buf = bh->b_data;
  │     while (offset < blocksize) {
  │         de = de_buf + offset;
  │         if (match) → found
  │         offset += de->rec_len;
  │     }
  │
  └─[加密比较] ext4_fname_diskcmp(dir, de, fname)
     匹配密文或解密后明文
```

---

## 6. 创建普通文件：ext4_create

`ext4_create`（namei.c:2813）处理 `creat()` / `open(O_CREAT)` 系统调用：

```c
// namei.c:2813
static int ext4_create(struct mnt_idmap *idmap, struct inode *dir,
    struct dentry *dentry, umode_t mode, bool excl)
```

**流程**：

```
ext4_create(dir, dentry, mode, excl)
  │
  ├─[1] dquot_initialize(dir) — 初始化 quota              (line 2820)
  │
  ├─[2] 计算事务 credit:
  │     DATA_TRANS_BLOCKS + INDEX_EXTRA_TRANS_BLOCKS + 3  (line 2824)
  │
  ├─[3] ext4_new_inode_start_handle(idmap, dir, mode,     (line 2827)
  │         &dentry->d_name, 0, NULL, EXT4_HT_DIR, credits)
  │     └── 调用 __ext4_new_inode (ialloc.c:931)
  │         ├─分配 inode
  │         ├─初始化 extent/xattr/加密
  │         └─返回新 inode
  │
  ├─[4] 设置 inode 操作函数:
  │     inode->i_op = &ext4_file_inode_operations         (line 2832)
  │     inode->i_fop = &ext4_file_operations              (line 2833)
  │     ext4_set_aops(inode)                              (line 2834)
  │
  ├─[5] ext4_add_nondir(handle, dentry, &inode)           (line 2835)
  │     ├─ext4_add_entry(handle, dentry, inode)
  │     │   向父目录中添加目录项 (可能触发 HTree split)
  │     ├─ext4_mark_inode_dirty(handle, inode)
  │     └─d_instantiate_new(dentry, inode)
  │
  ├─[6] ext4_fc_track_create(handle, dentry)              (line 2837)
  │     记录到 fast commit
  │
  ├─[7] ext4_journal_stop(handle)                         (line 2840)
  │
  └─[8] 重试: ENOSPC && ext4_should_retry_alloc           (line 2843)
         重新开始整个操作（给 allocator 另一次机会）
```

**`ext4_add_nondir` 流程图**：

```
ext4_add_nondir(handle, dentry, inode)
  │
  ├─ ext4_add_entry(handle, dentry, inode)
  │   │
  │   ├─[linear] make_indexed_dir()?
  │   │   NO  → ext4_add_dirent_to_buf():
  │   │         在指定块中插入目录项
  │   │         如果块满 → ext4_append() 新块
  │   │         NO  → 分配新块，设置 rec_len
  │   │
  │   ├─[HTree] ext4_dx_add_entry():
  │   │          hash 文件名 → 定位叶块
  │   │          → ext4_add_dirent_to_buf() 插入
  │   │          如果叶块满 → do_split() 分裂块
  │   │          如果 dx_root 满 → ext4_dx_rehash() 重建树
  │   │
  │   └─ ext4_mark_inode_dirty(handle, dir)
  │
  ├─ ext4_mark_inode_dirty(handle, inode) — 更新 inode 的 mtime/ctime
  │
  └─ d_instantiate_new(dentry, inode) — 建立 dentry↔inode 关联
```

---

## 7. 创建子目录：ext4_mkdir

`ext4_mkdir`（namei.c:2995）不仅创建目录 inode，还初始化一个目录所需的所有基础设施：

```c
// namei.c:2995
static struct dentry *ext4_mkdir(struct mnt_idmap *idmap,
    struct inode *dir, struct dentry *dentry, umode_t mode)
```

**完整流程**：

```
ext4_mkdir(dir, dentry, mode)
  │
  ├─[1] 检查硬链接上限: EXT4_DIR_LINK_MAX                (line 3002)
  │     超限 → -EMLINK
  │
  ├─[2] dquot_initialize(dir)                            (line 3005)
  │
  ├─[3] ext4_new_inode_start_handle(dir, S_IFDIR|mode)   (line 3012)
  │     └── 调用 __ext4_new_inode → 选择块组 → 分配 inode
  │         特别注意: S_IFDIR 触发 Orlov 顶层目录分散
  │
  ├─[4] 设置 inode 操作函数:
  │     inode->i_op = &ext4_dir_inode_operations         (line 3020)
  │     inode->i_fop = &ext4_dir_operations              (line 3021)
  │
  ├─[5] ext4_init_new_dir(handle, dir, inode)            (line 3022)
  │     ├─ 创建 "." 条目: de->inode = inode->i_ino
  │     │    de->name = ".", de->name_len = 1
  │     │
  │     ├─ 创建 ".." 条目: de->inode = dir->i_ino
  │     │    de->name = "..", de->name_len = 2
  │     │
  │     ├─ 初始化 HTree 根: dx_root 块
  │     │    dx_root.info.hash_version = DX_HASH_HALF_MD4
  │     │    dx_root.info.info_length = sizeof(dx_root_info)
  │     │    dx_root.info.indirect_levels = 0
  │     │    dx_root.limit = 块中可容纳的最大 dx_entry 数
  │     │    dx_root.count = 0
  │     │
  │     └─ ext4_mark_inode_dirty(handle, inode)
  │
  ├─[6] ext4_mark_inode_dirty(handle, inode)             (line 3025)
  │
  ├─[7] ext4_add_entry(handle, dentry, inode)            (line 3027)
  │     向父目录注册 "." 条目指向新目录
  │
  ├─[8] ext4_inc_count(dir) — 父目录 nlink++              (line 3040)
  │
  ├─[9] ext4_update_dx_flag(dir) — 更新 HTree 标志位      (line 3042)
  │     决定父目录是否需要切换到 HTree 模式
  │
  ├─[10] ext4_mark_inode_dirty(handle, dir)              (line 3043)
  │
  ├─[11] d_instantiate_new(dentry, inode)                (line 3046)
  │
  ├─[12] ext4_fc_track_create(handle, dentry)            (line 3047)
  │
  └─[13] IS_DIRSYNC(dir)? ext4_handle_sync(handle)       (line 3049)
          目录同步模式: 强制 journal 提交
```

**错误恢复路径**（line 3029-3037）：

```
ext4_init_new_dir 或 ext4_add_entry 失败:
  │
  ├─ clear_nlink(inode) — 将 nlink 置 0
  ├─ ext4_orphan_add(handle, inode) — 加入 orphan 链表
  │   确保崩溃后 orphan cleanup 能回收
  ├─ unlock_new_inode(inode)
  ├─ ext4_mark_inode_dirty(handle, inode)
  └─ iput(inode) — 释放 inode
```

Orphan 链表是 JBD2 日志的辅助机制：如果文件删除过程中崩溃，日志重放时 orphan cleanup 会释放这些"半完成"的 inode。

---

## 8. 目录扩展：ext4_append

`ext4_append`（namei.c:53）负责向目录追加新的数据块：

```c
// namei.c:53
static struct buffer_head *ext4_append(handle_t *handle,
    struct inode *inode, ext4_lblk_t *block)
```

**流程**：

```
ext4_append(handle, inode, &block)
  │
  ├─[1] 检查目录大小上限:
  │     s_max_dir_size_kb → 目录最大大小                   (line 61-64)
  │     超限 → -ENOSPC
  │
  ├─[2] *block = inode->i_size >> blocksize_bits           (line 66)
  │     新块位于当前 i_size 末尾
  │
  ├─[3] ext4_map_blocks(NULL, inode, &map, 0)              (line 75)
  │     检查该逻辑块是否已分配
  │     已分配 → -EFSCORRUPTED (目录块不应被重复分配)
  │
  ├─[4] ext4_bread(handle, inode, *block, GET_BLOCKS_CREATE) (line 83)
  │     创建块 + 读取（实际是分配 + 预读）
  │     GET_BLOCKS_CREATE → 调用 ext4_mb_new_blocks
  │
  ├─[5] inode->i_size += blocksize                        (line 86)
  │     更新 inode 大小（目录变大了一个块）
  │
  ├─[6] EXT4_I(inode)->i_disksize = inode->i_size         (line 87)
  │     同步磁盘大小
  │
  ├─[7] ext4_mark_inode_dirty(handle, inode)              (line 88)
  │
  └─[8] ext4_journal_get_write_access(handle, bh)         (line 92)
        获取 buffer_head 的写访问权限用于日志
```

**将目录从线性切换到 HTree**：

当目录大小超过 2 个块时，`ext4_update_dx_flag` 会设置 `EXT4_INODE_INDEX` 标志。下一次 `ext4_add_entry` 调用将检测到这个标志，触发 `make_indexed_dir()`：

```
make_indexed_dir():
  │
  ├─ 读取 block 0 (当前的目录项)
  ├─ 在 block 0 中创建 dx_root:
  │   在 "." 和 ".." 之后放置 dx_root_info + dx_entry 数组
  ├─ 扫描所有现有目录项，哈希文件名
  ├─ 按哈希值排序条目
  ├─ 将排序后的条目分发到叶块
  ├─ 在 dx_root.entries[] 中创建索引
  └─ 标记目录为 HTree (EXT4_INODE_INDEX)
```

---

## 9. HTree 内部机制

### 9.1 哈希种子

ext4 在每个超级块中存储 4 个 32 位哈希种子：

```c
__le32 s_hash_seed[4];  // 共 128 位熵
```

哈希种子在 `mke2fs` 时随机生成，存储在 superblock 的 `s_hash_seed` 字段。每次文件系统挂载时读取到 `sbi->s_hash_seed`。

**为什么需要种子？**
- 防止 HTree 碰撞攻击：攻击者构造大量相同哈希的文件名导致目录查找退化为 O(n)
- 不同文件系统有不同的种子，同一文件名映射到不同位置
- 这也是 HTree 不是真正"通用 B 树"的原因——它是有种子的哈希 B 树

### 9.2 哈希函数

ext4 支持多种哈希函数，默认使用 `DX_HASH_HALF_MD4`：

```c
ext4fs_dirhash(parent, qstr->name, qstr->len, &hinfo);
// hinfo.hash = 32位哈希值
// hinfo.minor_hash = 额外的 32位值，用于碰撞处理
```

`half-MD4` 对文件名计算 MD4 哈希，取结果的一半作为 32 位哈希值，这比完整的 MD5 更快，同时提供足够的分散性。

### 9.3 dx_root 结构

```
dx_root (存储在 block 0):

  ┌─────────────────────────────────────┐
  │  "." 目录项                          │  ← inode=自身
  │  ".." 目录项                         │  ← inode=父目录
  ├─────────────────────────────────────┤
  │  dotdot.rec_len                      │  ← 指向 dx_root_info
  ├─────────────────────────────────────┤
  │  dx_root_info:                       │
  │    reserved_zero    (4 bytes)        │
  │    hash_version     (1 byte)         │  ← DX_HASH_HALF_MD4
  │    info_length      (1 byte)         │  = 8
  │    indirect_levels  (1 byte)         │  = 0: 只有根和叶
  │                                       │  = 1: 根→dx_node→叶
  │    unused_flags     (1 byte)         │
  ├─────────────────────────────────────┤
  │   limit            (2 bytes)         │  ← 最大 dx_entry 数
  │   count            (2 bytes)         │  ← 当前 dx_entry 数
  │   block            (4 bytes)         │  ← 根节点自身块号
  ├─────────────────────────────────────┤
  │  dx_entry[0]: hash=0, block=N        │  ← hash≤0 的条目
  │  dx_entry[1]: hash=H1, block=M       │  ← 0<hash≤H1 的条目
  │  dx_entry[2]: hash=H2, block=P       │  ← H1<hash≤H2 的条目
  │  ...                                 │
  │  dx_entry[count-1]: hash=Hmax, block │
  └─────────────────────────────────────┘
```

### 9.4 哈希碰撞处理

HTree 的哈希碰撞策略是**合并**而非链式解决：

1. 如果两个文件名有相同的哈希值，它们必须放入同一个叶块
2. 如果叶块已满，分裂叶块时按次要哈希（minor_hash）排序
3. 极端情况下（所有文件哈希相同），整个目录退回为近似线性结构
4. 在 dx_entry 插入时，如果碰撞导致同一叶块条目过多，触发 `do_split()` 将块一分为二

### 9.5 树重建（Rehash）

当加入太多条目导致某个 dx_node 或 dx_root 溢出时，触发 `ext4_dx_rehash()`：

```
ext4_dx_rehash(dir)
  │
  ├─ 读取所有叶块的目录项
  ├─ 对所有条目按哈希值重新排序
  ├─ 将条目均匀分发到叶块
  ├─ 重建 dx_root 和 dx_node 的 dx_entry 数组
  └─ 可能增加 indirect_levels（加深树）
```

---

## 10. fscrypt 加密集成

ext4 原生支持 **fscrypt**（文件系统级加密），加密粒度是每个 inode。加密集成遍布目录操作的关键路径：

### 10.1 Inode 创建时的加密初始化

```c
// ialloc.c:994-998
if (!(i_flags & EXT4_EA_INODE_FL)) {
    err = fscrypt_prepare_new_inode(dir, inode, &encrypt);
    if (err)
        goto out;
}
```

`fscrypt_prepare_new_inode` 设置：
- 新 inode 的加密策略（继承自父目录）
- 派生出 per-inode 加密密钥
- 设置 `EXT4_INODE_FSCRYPT_MASK` 标志位

### 10.2 加密目录查找

加密目录中，文件名以密文形式存储。查找时的处理在 `ext4_find_entry` 中：

```c
// 查找加密目录
fscrypt_fname = ext4_fname_prepare(dir, dentry->d_name, &fname);
// fscrypt_fname.disk_name = 密文文件名
// 在 HTree 或线性搜索中使用密文名

// 找到条目后比较
ext4_fname_diskcmp(dir, de, fname);
// 比较 disk_name (密文) 或 no-key name
```

### 10.3 跨目录加密一致性

在 `ext4_lookup`（namei.c:1792-1800）中，如果目录被加密，查找出的子 inode 如果是目录或符号链接，必须验证加密上下文一致：

```c
if (!IS_ERR(inode) && IS_ENCRYPTED(dir) &&
    (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)) &&
    !fscrypt_has_permitted_context(dir, inode)) {
    ext4_warning(...);
    iput(inode);
    return ERR_PTR(-EPERM);
}
```

这防止了攻击将一个加密目录的 inode 通过硬链接方式"泄漏"到未加密目录中。

---

## 11. VFS 操作表

ext4 的目录 inode 操作接口在 namei.c:4222 定义：

```c
// namei.c:4222-4241
const struct inode_operations ext4_dir_inode_operations = {
    .create     = ext4_create,      // creat/open(O_CREAT)
    .lookup     = ext4_lookup,      // path 解析
    .link       = ext4_link,        // link() 硬链接
    .unlink     = ext4_unlink,      // unlink() 删除
    .symlink    = ext4_symlink,     // symlink() 符号链接
    .mkdir      = ext4_mkdir,       // mkdir()
    .rmdir      = ext4_rmdir,       // rmdir()
    .mknod      = ext4_mknod,       // mknod() 设备文件
    .tmpfile    = ext4_tmpfile,     // O_TMPFILE
    .rename     = ext4_rename2,     // rename()
    .setattr    = ext4_setattr,     // chmod/chown/truncate
    .getattr    = ext4_getattr,     // stat
    .listxattr  = ext4_listxattr,   // listxattr
    .get_inode_acl = ext4_get_acl,  // ACL 读取
    .set_acl    = ext4_set_acl,     // ACL 写入
    .fiemap     = ext4_fiemap,      // FS_IOC_FIEMAP
    .fileattr_get = ext4_fileattr_get, // FS_IOC_GETFLAGS
    .fileattr_set = ext4_fileattr_set, // FS_IOC_SETFLAGS
};
```

这是一个完整的 POSIX 目录操作实现，还包括 ext4 特有的扩展属性（ACL、fiemap、fileattr）。

---

## 12. HTree 查找完整流程图

```
用户: ls large_dir/file123
         │
         ▼
VFS: link_path_walk → walk_component
         │
         ▼
ext4_lookup(dir, dentry, flags)                    // namei.c:1760
         │
         ▼
ext4_lookup_entry(dir, dentry, &de)                // namei.c:1769
         │
         ▼
ext4_find_entry(dir, dentry, &de, NULL)
         │
         ├─ [inline dir] → ext4_find_inline_entry()
         │
         ├─ [线性目录] → search_dirblock(bh, dir, name, ...)
         │     │
         │     └── 扫描所有块:
         │         for (block=0; block < dir_blocks; block++)
         │            ext4_read_dirblock(dir, block, DIRENT)
         │            → 线性扫描 bh->b_data 中的 ext4_dir_entry_2
         │               de->name == name → 返回 &de
         │
         └─ [HTree 目录] → ext4_dx_find_entry()
               │
               ├─ hash = ext4fs_dirhash(name, s_hash_seed)
               │
               ├─ 读取 dx_root (block 0)
               │   ┌──────────────────────────────────┐
               │   │ dx_entry[0]: hash=0   → block 5  │
               │   │ dx_entry[1]: hash=100 → block 8  │
               │   │ dx_entry[2]: hash=500 → block 15 │  ← hash=320 命中
               │   │ dx_entry[3]: hash=900 → block 22 │
               │   └──────────────────────────────────┘
               │
               ├─ [indirect_levels==1] 读取 dx_node (block 15)
               │   ┌──────────────────────────────────┐
               │   │ dx_entry[0]: hash=250 → block 30 │  ← hash=320 命中
               │   │ dx_entry[1]: hash=350 → block 35 │
               │   │ dx_entry[2]: hash=450 → block 40 │
               │   └──────────────────────────────────┘
               │   读取叶块 (block 30)
               │
               └─ [indirect_levels==0] 直接读取叶块 (block 15)
                   │
                   ▼
               ┌──────────────────────────────────┐
               │ ext4_dir_entry_2 叶块 (block 30)  │
               │ ino=1234, "file001", type=1       │
               │ ino=5678, "file110", type=1       │
               │ ino=9012, "file123", type=1  ← 找到│
               │ ino=3456, "file199", type=1       │
               └──────────────────────────────────┘
                   │
                   ▼
               返回 ext4_dir_entry_2 *de → ext4_lookup → ext4_iget
```

---

## 13. 目录操作性能总结

| 操作 | 线性目录 | HTree 目录 | 复杂度 |
|------|---------|-----------|--------|
| lookup | O(n) 线性扫描 | O(log n) B 树 | n = 目录项数 |
| create | O(1) 插入块尾或 HTree 叶块 | O(log n) 定位叶块 + O(1) 插入 | |
| unlink | O(n) 查找 | O(log n) 查找 + O(1) 删除 | |
| readdir | O(n) 顺序扫描 | O(n) 顺序扫描 | 磁盘顺序无差别 |
| rename | O(n) 查找目标 | O(log n) 查找 + O(1) 更新 | |

**HTree 的限制**：
- `readdir()` 无法利用 HTree：目录项按创建顺序或哈希顺序存储，不是按文件名排序
- telldir/seekdir 兼容性：HTree 叶块间的逻辑块映射可能随 rehash 变化
- NFS 不兼容 HTree：NFS 导出的目录保持线性模式

---

## 下一篇文章

下一篇 [第六篇：在线扩容与 FSMap — 不停机扩展与块所有者查询](./06-resize-fsmap.md) 将深入 ext4 的在线 resize 机制和 FS_IOC_GETFSMAP 块所有者查询功能，以及整个系列的知识总结。
