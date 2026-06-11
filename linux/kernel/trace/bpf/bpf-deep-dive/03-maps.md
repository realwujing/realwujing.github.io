# 第3篇：BPF Maps — BPF_MAP_CREATE、lookup/update 与 map 类型

> 源码：`kernel/bpf/syscall.c` `kernel/bpf/hashtab.c` `kernel/bpf/arraymap.c` | 头文件：`include/uapi/linux/bpf.h`

系列目录：[eBPF 内核源码深度解析](./README.md)

---

## 1. BPF Map — 内核常驻的键值存储

BPF map 是 BPF 程序和用户态之间共享数据的主要机制。它由内核管理，在程序加载后就一直存在在内核内存中。

```c
// include/linux/bpf.h
struct bpf_map {
    const struct bpf_map_ops  *ops;          // ★ map 类型的操作表
    struct bpf_map            *inner_map_meta; // 内嵌 map（用于 ARRAY_OF_MAPS/HASH_OF_MAPS）
    void                      *record;        // map 的设备特定记录
    enum bpf_map_type          map_type;      // BPF_MAP_TYPE_*
    u32                        key_size;      // 键的字节大小
    u32                        value_size;    // 值的字节大小
    u32                        max_entries;   // 最大条目数
    u64                        map_extra;     // 每 map 额外的参数
    u32                        map_flags;     // BPF_F_* flags
    atomic64_t                 usercnt;       // 用户态持有的引用计数
    struct work_struct         work;          // 延迟释放的工作
    char                       name[BPF_OBJ_NAME_LEN]; // map 名称
    struct btf_record          *record;       // BTF 记录
};
```

---

## 2. BPF_MAP_CREATE — 创建 map

```c
// kernel/bpf/syscall.c
static int map_create(union bpf_attr *attr)
{
    struct bpf_map *map;
    int err;

    // ① 验证: key_size, value_size 在合理范围
    // 检查 max_entries（不能超过 sysctl_max_memlock）
    // 检查 map_type 是否已注册

    // ② 找到 map_type 对应的 map_ops
    switch (attr->map_type) {
    case BPF_MAP_TYPE_HASH:
        ops = &hash_map_ops;
        break;
    case BPF_MAP_TYPE_ARRAY:
        ops = &array_map_ops;
        break;
    case BPF_MAP_TYPE_PERCPU_HASH:
        ops = &percpu_hash_map_ops;
        break;
    // ... 30+ map types ...
    }

    // ③ 分配 struct bpf_map + 初始化
    map = ops->map_alloc(attr);             // 特定类型的 init
    map->ops = ops;
    map->map_type = attr->map_type;
    map->key_size = attr->key_size;
    map->value_size = attr->value_size;
    map->max_entries = attr->max_entries;

    // ④ 分配文件描述符
    err = bpf_map_new_fd(map, ...);

    // ⑤ 注册到全局 IDR（允许用户态通过 fd 找到 map）
    map->id = idr_alloc(&bpf_map_idr, map, 1, INT_MAX, GFP_KERNEL);

    return fd;
}
```

---

## 3. Map 操作表 (bpf_map_ops)

```c
struct bpf_map_ops {
    int (*map_alloc_check)(union bpf_attr *attr);          // 验证创建参数
    struct bpf_map *(*map_alloc)(union bpf_attr *attr);    // 分配 map 内存
    void (*map_release)(struct bpf_map *map, struct file *filp); // 用户态 close fd
    void (*map_free)(struct bpf_map *map);                 // 最后一次引用释放

    int (*map_get_next_key)(struct bpf_map *map, void *key, void *next_key);

    void *(*map_lookup_elem)(struct bpf_map *map, void *key);      // ★ 查找
    long (*map_update_elem)(struct bpf_map *map, void *key,        // ★ 更新/插入
                            void *value, u64 flags);
    long (*map_delete_elem)(struct bpf_map *map, void *key);       // 删除
    int (*map_push_elem)(struct bpf_map *map, void *value, u64 flags);
    int (*map_pop_elem)(struct bpf_map *map, void *value);

    int (*map_lookup_batch)(struct bpf_map *map, const union bpf_attr *attr,
                            union bpf_attr __user *uattr);
    int (*map_update_batch)(struct bpf_map *map, struct file *map_file,
                            const union bpf_attr *attr,
                            union bpf_attr __user *uattr);
    int (*map_delete_batch)(struct bpf_map *map, const union bpf_attr *attr,
                            union bpf_attr __user *uattr);

    int (*map_lookup_and_delete_elem)(struct bpf_map *map, void *key,
                                      void *value, u64 flags);
    int (*map_lookup_and_delete_batch)(struct bpf_map *map, ...);
    int (*map_redirect)(struct bpf_map *map, u64 key, u64 flags);
};
```

**三个核心操作**：
- `map_lookup_elem`：BPF 程序 `bpf_map_lookup_elem` 的实际执行——返回一个指针到 map 的值
- `map_update_elem`：BPF 程序 `bpf_map_update_elem` 的实际执行——更新或插入一个键值对
- `map_delete_elem`：BPF 程序 `bpf_map_delete_elem` 的实际执行

---

## 4. 哈希表映射 (BPF_MAP_TYPE_HASH)

```c
// kernel/bpf/hashtab.c

struct bpf_htab {
    struct bpf_map map;
    struct bucket *buckets;             // 哈希桶数组 (n_buckets 个)
    int n_buckets;                      // 桶数（2的幂）
    int elem_size;                     // 每个元素的大小（含 key + value + 头部）
    struct pcpu_freelist freelist;      // ★ 每 CPU 的空闲链表（用于快速分配）
    struct bpf_mem_alloc *ma;           // 内存分配器
    // ... 锁 ...
};

// 分配：
static struct bpf_map *htab_map_alloc(union bpf_attr *attr)
{
    // ① 计算桶数（max_entries 向上取 2 的幂）
    // ② 分配 htab 结构
    // ③ 预分配元素: 从 pcpu_freelist 分配 max_entries 个元素
    // ④ 返回 map
}

// 查找：
static void *htab_map_lookup_elem(struct bpf_map *map, void *key)
{
    struct bpf_htab *htab = container_of(map, struct bpf_htab, map);
    struct htab_elem *l;

    hash = htab_map_hash(key, key_size, htab->hashrnd);   // 计算哈希
    head = &htab->buckets[hash & (htab->n_buckets - 1)];  // 桶头
    hlist_for_each_entry_rcu(l, head, hash_node) {         // 遍历链表
        if (!memcmp(l->key, key, key_size)) {
            return l->key + round_up(key_size, 8);         // 跳到值的位置
        }
    }
    return NULL;  // 未找到
}

// 更新/插入：
static long htab_map_update_elem(struct bpf_map *map, void *key,
                                 void *value, u64 map_flags)
{
    // ① 查找已有条目
    l_old = lookup_elem_raw(head, hash, key, key_size);
    if (l_old) {
        if (map_flags == BPF_NOEXIST)
            return -EEXIST;
        // 复制新值
        copy_map_value(map, l_old->key + round_up(key_size, 8), value);
        return 0;
    }

    // ② 不存在，从 freelist 分配新元素
    l_new = alloc_htab_elem(htab, ...);
    memcpy(l_new->key, key, key_size);
    copy_map_value(map, l_new->key + round_up(key_size, 8), value);

    // ③ 插入到桶的链表
    hlist_add_head_rcu(&l_new->hash_node, head);
}
```

---

## 5. 数组映射 (BPF_MAP_TYPE_ARRAY)

```c
// kernel/bpf/arraymap.c

struct bpf_array {
    struct bpf_map map;
    u32 elem_size;                  // 每元素大小
    u32 index_mask;                 // max_entries - 1
    struct bpf_array_aux *aux;
    char value[];                   // ★ 扁平数组: 所有值连续存储
};

// 预分配——所有 max_entries 在创建时立即分配
static struct bpf_map *array_map_alloc(union bpf_attr *attr)
{
    elem_size = round_up(attr->value_size, 8);
    array_size = sizeof(*array) + (u64)attr->max_entries * elem_size;
    // kmalloc 全量分配
    array = bpf_map_area_alloc(array_size, numa_node);
    return &array->map;
}

// 查找:
static void *array_map_lookup_elem(struct bpf_map *map, void *key)
{
    struct bpf_array *array = container_of(map, struct bpf_array, map);
    u32 index = *(u32 *)key;
    if (index >= array->map.max_entries)
        return NULL;
    return array->value + (u64)index * array->elem_size;  // O(1)
}

// 更新:
static long array_map_update_elem(struct bpf_map *map, void *key,
                                  void *value, u64 map_flags)
{
    struct bpf_array *array = container_of(map, struct bpf_array, map);
    u32 index = *(u32 *)key;
    if (index >= array->map.max_entries)
        return -E2BIG;
    memcpy(array->value + (u64)index * array->elem_size, value, map->value_size);
    return 0;
}
```

**关键区别**：数组在创建时一次性分配所有条目，查找 O(1)。哈希表只分配使用的条目，查找 O(1) 平均。

---

## 6. 其他常见 map 类型

| map_type | 文件名 | 结构 |
|----------|--------|------|
| PERCPU_HASH/PERCPU_ARRAY | `hashtab.c` / `arraymap.c` | 每 CPU 独立副本（无锁读） |
| LRU_HASH/LRU_PERCPU_HASH | `hashtab.c` | 带 LRU 淘汰的哈希 |
| STACK/QUEUE | `stackmap.c` | BPF_MAP_TYPE_STACK/BPF_MAP_TYPE_QUEUE |
| RINGBUF | `ringbuf.c` | BPF_MAP_TYPE_RINGBUF — 高吞吐用户态→BPF 管道 |
| BLOOM_FILTER | `bloom_filter.c` | 布隆过滤器 |
| ARENA | `arena.c` | 共享内存域（BPF 和用户态都可访问的 mmap 区域） |
| TASK_STORAGE | `bpf_task_storage.c` | 每 task 的本地存储 |
| CGROUP_STORAGE | `bpf_cgroup_storage.c` | 每 cgroup 的本地存储 |

---

## 7. BPF 系统调用的完整列表

```c
// 通过 bpf(2) 系统调用的命令
enum bpf_cmd {
    BPF_MAP_CREATE,
    BPF_MAP_LOOKUP_ELEM,
    BPF_MAP_UPDATE_ELEM,
    BPF_MAP_DELETE_ELEM,
    BPF_MAP_GET_NEXT_KEY,
    BPF_PROG_LOAD,
    BPF_OBJ_PIN,
    BPF_OBJ_GET,
    BPF_PROG_ATTACH,
    BPF_PROG_DETACH,
    BPF_PROG_TEST_RUN,
    BPF_PROG_RUN,
    BPF_PROG_GET_NEXT_ID,
    BPF_MAP_GET_NEXT_ID,
    BPF_PROG_GET_FD_BY_ID,
    BPF_MAP_GET_FD_BY_ID,
    BPF_OBJ_GET_INFO_BY_FD,
    BPF_PROG_QUERY,
    BPF_RAW_TRACEPOINT_OPEN,
    BPF_BTF_LOAD,
    BPF_BTF_GET_FD_BY_ID,
    BPF_TASK_FD_QUERY,
    BPF_MAP_LOOKUP_AND_DELETE_ELEM,
    BPF_MAP_FREEZE,
    BPF_BTF_GET_NEXT_ID,
    BPF_MAP_LOOKUP_BATCH,
    BPF_MAP_LOOKUP_AND_DELETE_BATCH,
    BPF_MAP_UPDATE_BATCH,
    BPF_MAP_DELETE_BATCH,
    BPF_LINK_CREATE,
    BPF_LINK_UPDATE,
    BPF_LINK_DETACH,
    BPF_ENABLE_STATS,
    BPF_ITER_CREATE,
};
```

---

## 8. BPF map 数据的并发访问

| map 类型 | 并发读 | 并发写 | RCU 机制 |
|---------|--------|--------|---------|
| ARRAY | 无锁（值直接内存读取，RCU 保护数组大小不变） | 无锁（单写者） | 数组大小不变 |
| HASH | 无锁（RCU `hlist_for_each_entry_rcu`） | 自旋锁保护每桶 | 链表更新通过 `call_rcu` 延迟释放 |
| PERCPU_ARRAY | 无锁（每 CPU 独立副本） | 无锁（关抢占即可） | 无 |
| LRU_HASH | 无锁读（RCU） | 自旋锁 + LRU 更新锁 | 同 HASH |

---

## 9. 系列结语

3 篇文章从 BPF 程序的加载 (`bpf_prog_load`)、verifier 验证 (`bpf_check` → `do_check_main` → `verifier_types`)、到 map 的创建和操作 (`map_create`、`htab_map_lookup_elem`、`array_map_update_elem`)，完整覆盖了 eBPF 的内核端实现。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-prog-load | struct bpf_prog + BPF_PROG_LOAD + prog_types | `syscall.c` |
| 02-verifier | bpf_check → do_check → reg_type 追踪 → 循环检测 | `verifier.c` |
| 03-maps | BPF_MAP_CREATE → map_ops → htab/array lookup/update | `syscall.c`, `hashtab.c`, `arraymap.c` |
