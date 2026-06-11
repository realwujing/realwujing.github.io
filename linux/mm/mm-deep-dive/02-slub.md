# 第二篇：SLUB 分配器 — sheaf 缓存、slab freelist 与 kmalloc

> 源码：mm/slub.c, mm/slab_common.c | 头文件：mm/slab.h, include/linux/slub_def.h

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. 概述

SLUB 是 Linux 内核自 v7.1 起默认的内核对象分配器，取代了此前的 SLAB 分配器。它提供两种核心服务：

1. **专用缓存 (dedicated cache)**：为固定大小的内核对象（如 `struct task_struct`、`struct inode`、`struct dentry`）创建专用 `kmem_cache`，复用已释放对象，避免频繁 buddy 分配
2. **通用分配 (kmalloc)**：类似用户态 `malloc()`，根据请求大小查找预定义的 size bucket cache，分配变长内存

SLUB v7.1 新架构引入 **sheaf** 概念（替代 v6.x 的 freelist），每个 CPU 持有 `slub_percpu_sheaves`，内核端以 `slab_sheaf` 管理空闲对象。

**核心文件**：
- `mm/slub.c` — SLUB 核心逻辑（~9950 行）
- `mm/slab.h` — kmem_cache、node_barn 等数据结构
- `mm/slab_common.c` — cache 创建/销毁的公共层
- `include/linux/slub_def.h` — 对外接口声明

---

## 2. 核心数据结构

### 2.1 struct kmem_cache — 缓存描述符

`mm/slab.h:198-249`

每个 kmem_cache 代表一类固定大小对象的"slab 工厂"：

```c
struct kmem_cache {
    struct slub_percpu_sheaves __percpu *cpu_sheaves;  // per-CPU sheaf 指针数组
    slab_flags_t flags;
    unsigned long min_partial;          // 保留的最少 partial slab 数
    unsigned int size;                  // 含 metadata 的对象总大小
    unsigned int object_size;           // 纯对象大小（不含 metadata）
    unsigned int offset;                // 空闲指针在对象内的偏移
    unsigned int sheaf_capacity;        // 每个 sheaf 容纳的对象数
    struct kmem_cache_order_objects oo; // {order, count}：首选 slab order 和对象数
    struct kmem_cache_order_objects min; // {order, count}：最小 slab order 和对象数
    gfp_t allocflags;                   // 向 buddy 申请 slab 页时的 gfp 标志
    int refcount;                       // 缓存引用计数
    void (*ctor)(void *object);         // 对象构造函数
    unsigned int inuse;                 // 含 metadata 的对象大小（用于确定 real size）
    unsigned int align;                 // 对齐要求
    const char *name;                   // 缓存名称（仅用于显示）
    struct list_head list;              // 全局 cache 链表节点
    unsigned int remote_node_defrag_ratio; // NUMA 远程节点碎片化比例
    ...
};
```

**关键字段 `oo` (order+objects)**：
```c
struct kmem_cache_order_objects {
    unsigned int order;   // slab 页的 order（如 order=2 → 4页）
    unsigned int objects; // 该 slab 能容纳的对象数
};
```

计算示例：假设 `object_size=192`，`PAGE_SIZE=4096`：
- order=0 (1页)：`(4096 - header) / 192 ≈ 21` 个对象
- order=1 (2页)：`(8192 - header) / 192 ≈ 42` 个对象
- SLUB 选择能容纳 1/4 个 freelist 最少页数的 order 作为 `oo`

### 2.2 struct slub_percpu_sheaves — per-CPU 缓存

`mm/slub.c:420-425`

```c
struct slub_percpu_sheaves {
    local_trylock_t lock;
    struct slab_sheaf *main;      // 主 sheaf，永不为 NULL（未锁定时）
    struct slab_sheaf *spare;     // 备用 sheaf，可能为 NULL
    struct slab_sheaf *rcu_free;  // RCU 延迟释放的批量队列
};
```

**per-CPU sheaves 架构图**：
```
CPU 0                               CPU 1
┌─────────────────────────┐        ┌─────────────────────────┐
│ slub_percpu_sheaves     │        │ slub_percpu_sheaves     │
│  lock: (trylock)        │        │  lock: (trylock)        │
│  ┌─────────────────┐    │        │  ┌─────────────────┐    │
│  │ main (sheaf*)   │    │        │  │ main (sheaf*)   │    │
│  │  ┌───────────┐  │    │        │  │  ┌───────────┐  │    │
│  │  │ objects[] │  │    │        │  │  │ objects[] │  │    │
│  │  │ [obj0]    │──┼────┼──pop──►│  │  │ [objA]    │──┼──► alloc
│  │  │ [obj1]    │  │    │        │  │  │ [objB]    │  │    │
│  │  │ [obj2]    │  │    │        │  │  │ [objC]    │  │    │
│  │  │ ...       │  │    │        │  │  │ ...       │  │    │
│  │  └───────────┘  │    │        │  │  └───────────┘  │    │
│  └─────────────────┘    │        │  └─────────────────┘    │
│  ┌─────────────────┐    │        │  ┌─────────────────┐    │
│  │ spare (sheaf*)  │    │        │  │ spare (sheaf*)  │    │
│  │  (may be NULL)  │    │        │  │  (may be NULL)  │    │
│  └─────────────────┘    │        │  └─────────────────┘    │
│  ┌─────────────────┐    │        │  ┌─────────────────┐    │
│  │ rcu_free (*)    │    │        │  │ rcu_free (*)    │    │
│  │  kfree_rcu 批量 │    │        │  │  kfree_rcu 批量 │    │
│  └─────────────────┘    │        │  └─────────────────┘    │
└─────────────────────────┘        └─────────────────────────┘
```

### 2.3 struct slab_sheaf — 内核侧 sheaf

`mm/slub.c:404-418`

```c
struct slab_sheaf {
    union {
        struct rcu_head rcu_head;       // RCU 释放头
        struct list_head barn_list;     // barn 链表节点（full/empty）
        struct {
            unsigned int capacity;      // sheaf 容量（对象数）
            bool pfmemalloc;            // 是否来自 PF_MEMALLOC 分配
        };
    };
    struct kmem_cache *cache;           // 所属 kmem_cache
    unsigned int size;                  // 当前 sheaf 中对象数
    int node;                           // NUMA node（仅用于 rcu_sheaf）
    void *objects[];                    // 柔性数组：对象指针的数组
};
```

**slab_sheaf 内部布局**：
```
┌─────────────────────────────────────┐
│ struct slab_sheaf                   │
│  .cache     = kmem_cache-XXX       │
│  .size      = 156                   │  ← 当前空闲对象数
│  .capacity  = 170                   │  ← 最大容量
│  .node      = 0                     │
│                                     │
│  objects[0] → 0xffff...001          │
│  objects[1] → 0xffff...002          │
│  objects[2] → 0xffff...003          │
│  ...                                │
│  objects[169] → NULL (未使用)       │
└─────────────────────────────────────┘
```

**sheaf vs slab 的关系**：

在 v7.1 SLUB 中，一个 slab（从 buddy 分配的连续物理页）包含多个对象。这些对象的空闲指针组成 sheaf：
- **full slab**：所有对象已分配，sheaf 为 NULL
- **partial slab**：部分对象空闲，有 sheaf 维护空闲对象
- **empty slab**：所有对象空闲，sheaf.size == capacity，可能归还 buddy

```
slab 页（来自 buddy）
┌─────────────────────────────────────────────┐
│  obj0 (allocated)│ obj1 (free) │ obj2 (free) │
│                  │  ↑           │  ↑          │
│                  │  │           │  │          │
│  obj3 (allocated)│ obj4 (free) │ obj5 (alloc)│
│                  │  │           │             │
│  ...                                         │
└──────────────────────────┬──────────────────┘
                           │
                  slab_sheaf.objects[]
                  [0]→obj1, [1]→obj2, [2]→obj4
                  size=3, capacity=21
```

### 2.4 struct node_barn — 节点级 sheaf 仓库

`mm/slub.c:396-402`

```c
#define MAX_FULL_SHEAVES    10
#define MAX_EMPTY_SHEAVES   10

struct node_barn {
    spinlock_t lock;
    struct list_head sheaves_full;    // 满 sheaf 链表（所有对象在用）
    struct list_head sheaves_empty;   // 空 sheaf 链表（所有对象空闲）
    unsigned int nr_full;
    unsigned int nr_empty;
};
```

barn 用于批量管理 sheaf 的分配和释放，避免每次都从 buddy 分配/释放。

---

## 3. 锁顺序

`mm/slub.c:57-65`

```c
/*
 * Lock order:
 *   0. cpu_hotplug_lock
 *   1. slab_mutex (Global Mutex)
 *   2a. kmem_cache->cpu_sheaves->lock (Local trylock)
 *   2b. barn->lock (Spinlock)
 *   2c. node->list_lock (Spinlock)
 *   3. slab_lock(slab) (Only on some arches)
 */
```

```
cpu_hotplug_lock     ← 最外层
  └→ slab_mutex      ← 全局 cache 操作
       ├→ cpu_sheaves->lock  ← per-CPU local trylock
       ├→ barn->lock         ← per-node sheaf 仓库
       └→ node->list_lock    ← partial slab 链表
            └→ slab_lock     ← 单 slab 级别的锁（部分架构）
```

---

## 4. 快速路径：slab_alloc_node

`mm/slub.c:4870-4895`

快速路径是分配的热路径，设计为尽量 lockless：

```c
static __fastpath_inline void *slab_alloc_node(struct kmem_cache *s,
        struct list_lru *lru, gfp_t gfpflags, int node,
        unsigned long addr, size_t orig_size)
{
    void *object;
    bool init = false;

    // 1. 前置钩子：KASAN/KFENCE/failslab 检查
    s = slab_pre_alloc_hook(s, gfpflags);
    if (unlikely(!s))
        return NULL;

    // 2. KFENCE 采样
    object = kfence_alloc(s, orig_size, gfpflags);
    if (unlikely(object))
        goto out;

    // 3. 从 per-CPU sheaf 分配（核心快速路径）
    object = alloc_from_pcs(s, gfpflags, node);
    if (likely(object))
        goto out;

    // 4. 快速路径失败 → 进入慢速路径
    object = __slab_alloc_node(s, gfpflags, node, addr, orig_size);

out:
    // 5. 后置钩子：KASAN 染色、memset 清零、kmemleak 记录
    slab_post_alloc_hook(s, lru, gfpflags, 1, &object, init, orig_size);
    return object;
}
```

**alloc_from_pcs 内部逻辑**（简化）：
```c
alloc_from_pcs(s, gfpflags, node) {
    s = raw_cpu_ptr(s->cpu_sheaves);

    // 尝试从 main sheaf 取对象（非阻塞 trylock）
    if (local_trylock(&s->lock)) {
        sheaf = s->main;
        if (sheaf->size > 0) {
            sheaf->size--;
            object = sheaf->objects[sheaf->size];
            // 返回对象指针
            local_unlock(&s->lock);
            return object;
        }
        // main sheaf 为空 → 尝试从 spare sheaf 交换
        if (s->spare) {
            swap(s->main, s->spare);
            sheaf = s->main;
            // 从交换后的 main 取对象
            sheaf->size--;
            object = sheaf->objects[sheaf->size];
            local_unlock(&s->lock);
            return object;
        }
        local_unlock(&s->lock);
    }
    return NULL;  // 需要进入慢速路径
}
```

**快速路径流程图**：
```
slab_alloc_node()
 │
 ├─ slab_pre_alloc_hook()  —— KASAN/failslab 检查
 │
 ├─ kfence_alloc()         —— KFENCE 边界检测分配
 │
 ├─ alloc_from_pcs()       —— per-CPU sheaf 无锁分配 ★ 热路径
 │   ├─ trylock(cpu_sheaves->lock)
 │   ├─ main sheaf 非空 → pop object → 返回
 │   └─ main sheaf 空 → swap main↔spare → pop → 返回
 │
 ├─ (fast fail) → __slab_alloc_node()  —— 慢速路径
 │
 └─ slab_post_alloc_hook() —— KASAN/memset/kmemleak
```

---

## 5. 慢速路径：__slab_alloc_node

`mm/slub.c:4485-4514`

```c
static __always_inline void *__slab_alloc_node(struct kmem_cache *s,
        gfp_t gfpflags, int node, unsigned long addr, size_t orig_size)
{
    void *object;

#ifdef CONFIG_NUMA
    // NUMA 策略检查：如果 node==NUMA_NO_NODE 且 strict_numa
    if (static_branch_unlikely(&strict_numa) &&
            node == NUMA_NO_NODE) {
        struct mempolicy *mpol = current->mempolicy;
        if (mpol) {
            if (mpol->mode != MPOL_BIND ||
                !node_isset(numa_mem_id(), mpol->nodes))
                node = mempolicy_slab_node();
        }
    }
#endif

    object = ___slab_alloc(s, gfpflags, node, addr, orig_size);
    return object;
}
```

**___slab_alloc 慢速路径详解**：

```
___slab_alloc()
 │
 ├─ 1. 重试 alloc_from_pcs()
 │      (在慢速路径期间，其他 CPU 可能释放了对象)
 │
 ├─ 2. 尝试从 spare sheaf 补充 main
 │
 ├─ 3. 尝试从 barn 获取：
 │      ├─ barn->sheaves_full → 取一个满 sheaf → 放 spare
 │      └─ 如果 barn 为空：
 │           ├─ 尝试从 node partial list 取 slab
 │           │   → 构建 sheaf → 放入 spare
 │           │
 │           └─ partial list 也为空：
 │                ├─ 调用 buddy alloc_pages 分配新 slab 页
 │                │   new_slab() → alloc_pages(order) → 初始化对象
 │                │
 │                ├─ 构建 sheaf（填充所有空闲对象指针）
 │                │
 │                └─ 放入 per-CPU main sheaf
 │
 └─ 4. 返回对象
```

**新 slab 分配路径**：
```
___slab_alloc()
  └→ new_slab()
       │
       ├─ alloc_pages(oo.order, gfp_flags)  → 从 buddy 获取连续物理页
       │
       ├─ page_address(page)  → 获取 slab 的虚拟地址
       │
       ├─ 初始化空闲对象链表（构建 sheaf）
       │    for (i = 0; i < oo.objects; i++) {
       │        void *obj = slab_start + i * object_size;
       │        sheaf->objects[i] = obj;
       │    }
       │
       └─ 返回 sheaf
```

---

## 6. kmalloc — 通用大小分配

### 6.1 __kmalloc_noprof 入口

`mm/slub.c:5306-5309`
```c
void *__kmalloc_noprof(size_t size, gfp_t flags)
{
    return __do_kmalloc_node(size, NULL, flags, NUMA_NO_NODE, _RET_IP_);
}
```

### 6.2 __do_kmalloc_node — 大小路由

`mm/slub.c:5276-5299`
```c
static __always_inline
void *__do_kmalloc_node(size_t size, kmem_buckets *b, gfp_t flags,
                        int node, unsigned long caller)
{
    struct kmem_cache *s;
    void *ret;

    // 1. 超大分配（> KMALLOC_MAX_CACHE_SIZE）→ 直接走 buddy
    if (unlikely(size > KMALLOC_MAX_CACHE_SIZE)) {
        ret = __kmalloc_large_node_noprof(size, flags, node);
        return ret;
    }

    // 2. size == 0 返回特殊指针（零长分配）
    if (unlikely(!size))
        return ZERO_SIZE_PTR;

    // 3. 查找适合的 kmem_cache
    s = kmalloc_slab(size, b, flags, caller);

    // 4. 调用 slab_alloc_node 分配
    ret = slab_alloc_node(s, NULL, flags, node, caller, size);
    ret = kasan_kmalloc(s, ret, size, flags);
    return ret;
}
```

### 6.3 kmalloc 大小路由表

kmalloc 预定义了多个 size bucket，每个对应一个 kmem_cache：

```
请求 size      使用 cache name       实际 object_size
───────────────────────────────────────────────────
0..8          kmalloc-8              8
9..16         kmalloc-16             16
17..32        kmalloc-32             32
33..64        kmalloc-64             64
65..96        kmalloc-96             96
97..128       kmalloc-128            128
129..192      kmalloc-192            192
193..256      kmalloc-256            256
257..512      kmalloc-512            512
513..1024     kmalloc-1k             1024
1025..2048    kmalloc-2k             2048
2049..4096    kmalloc-4k             4096
4097..8192    kmalloc-8k             8192
> 8192        直接 buddy (order>0)
```

**kmalloc_slab() 查找逻辑**（简化）：
```c
kmalloc_slab(size, b, flags) {
    // 利用编译期常量或运行期 index 查找预定义数组
    kmalloc_caches[type][index]  // 或 kmalloc_buckets 机制
    // 返回对应的 kmem_cache 指针
}
```

---

## 7. kmem_cache 创建流程

### 7.1 __kmem_cache_create_args

`mm/slab_common.c:318-350`

```c
struct kmem_cache *__kmem_cache_create_args(const char *name,
                                            unsigned int object_size,
                                            struct kmem_cache_args *args,
                                            slab_flags_t flags)
{
    struct kmem_cache *s = NULL;
    const char *cache_name;
    int err;

    // 1. 调试标志处理 (SLUB_DEBUG)
#ifdef CONFIG_SLUB_DEBUG
    if (flags & SLAB_DEBUG_FLAGS)
        static_branch_enable(&slub_debug_enabled);
    if (flags & SLAB_STORE_USER)
        stack_depot_init();
#endif

    // 2. 如果有自定义 sheaf_capacity，禁用合并
    if (args->sheaf_capacity)
        flags |= SLAB_NO_MERGE;

    // 3. 分配并初始化 kmem_cache 结构体
    s = __kmem_cache_create(cache_name, object_size, args, flags);
    // ...
}
```

### 7.2 do_kmem_cache_create

`mm/slub.c:8551-8570`

```c
int do_kmem_cache_create(struct kmem_cache *s, const char *name,
                         unsigned int size, struct kmem_cache_args *args,
                         slab_flags_t flags)
{
    s->name = name;
    s->size = s->object_size = size;

    s->flags = kmem_cache_flags(flags, s->name);
    s->align = args->align;

    // 计算最优 slab order 和对象布局
    calculate_sizes(s);  // 确定 oo, min, sheaf_capacity

    // 计算空闲指针偏移（在对象内或对象外）
    s->offset = calculate_offset(s);

    // 分配 per-CPU sheaves
    s->cpu_sheaves = alloc_percpu(struct slub_percpu_sheaves);
    if (!s->cpu_sheaves)
        return -ENOMEM;

    // 初始化每个 CPU 的 sheaf
    init_kmem_cache_cpus(s);

    // 分配 per-node 数据
    alloc_kmem_cache_node(s);  // 初始化 node_barn

    return 0;
}
```

---

## 8. 释放路径：kfree

释放是分配的逆过程：

```c
void kfree(const void *object)
{
    struct folio *folio;
    struct kmem_cache *s;

    if (unlikely(ZERO_OR_NULL_PTR(object)))
        return;

    // 1. 获取对象所在 folio
    folio = virt_to_folio(object);

    // 2. 获取 folio 对应的 kmem_cache
    s = slab_folio_cache(folio);

    // 3. 调用 SLUB 释放
    slab_free(s, folio, object, _RET_IP_);
}
```

**slab_free 快速路径**：
```
slab_free(s, folio, object)
 │
 ├─ 检查是否是 slab 类型页
 │   ├─ 非 slab → free_large_kmalloc() → free_pages()
 │   └─ 是 slab
 │
 ├─ __slab_free()
 │   ├─ 获取 per-CPU sheaves
 │   ├─ if main sheaf not full → push object to main
 │   │   sheaf->objects[sheaf->size++] = object
 │   └─ if main sheaf full:
 │       ├─ if spare is NULL → spare = main, main = new empty sheaf
 │       └─ else → flush spare → barn
 │
 └─ 如果 slab 完全空闲：
     └─ free_slab() → free_pages() → 归还 buddy
```

---

## 9. sheaf 与 barn 的交互

```
Release flow (kfree):
  object
    │
    ▼
  per-CPU main sheaf (push)
    │ main sheaf 满
    ▼
  per-CPU spare sheaf → 交换 → 旧 main → barn sheaves_full
    │ spare 也满
    ▼
  barn sheaves_full (MAX_FULL_SHEAVES = 10)
    │ barn 满
    ▼
  归还 partial slab → node partial list

Allocation flow:
  pop from main sheaf
    │ main sheaf 空
    ▼
  swap(main, spare)
    │ spare 也空
    ▼
  从 barn sheaves_full 取一个 → 放到 main
    │ barn 空
    ▼
  从 node partial list 取 slab → build sheaf → main
    │ partial list 空
    ▼
  从 buddy 分配新 slab → build sheaf → main
```

---

## 10. SLUB 层次架构总览

```
kmalloc(size, GFP_xxx)         kmem_cache_alloc(cache, GFP_xxx)
        │                              │
        ▼                              ▼
   kmalloc_slab(size)            指定的 kmem_cache
        │                              │
        └──────────┬───────────────────┘
                   ▼
           slab_alloc_node(s, gfp)
                   │
          ┌────────┴────────┐
          ▼                 ▼
    快速路径             慢速路径
    alloc_from_pcs()    __slab_alloc_node()
    from per-CPU main      │
    sheaf (lockless)       ├─ barn sheaves_full
          │                ├─ node partial list
          │                └─ new_slab()
          │                      │
          │                      ▼
          │              alloc_pages(order)  ← 调用 Buddy
          │                      │
          │                      ▼
          │              ┌───────────────┐
          │              │  struct page  │  ← Buddy 分配器
          │              │  (物理页)     │
          │              └───────────────┘
          │
          ▼
    返回对象指针
```

---

## 11. 调试与/proc接口

### 11.1 /proc/slabinfo

```bash
cat /proc/slabinfo
slabinfo - version: 2.1
# name            <active_objs> <num_objs> <objsize> ...
kmalloc-8             1234    1280       8
kmalloc-64            5678    5760      64
dentry                3456    3480     192
inode_cache           2345    2400     592
```

### 11.2 sysfs 接口

```bash
cat /sys/kernel/slab/dentry/object_size
cat /sys/kernel/slab/dentry/objects
cat /sys/kernel/slab/dentry/slab_size
cat /sys/kernel/slab/dentry/cpu_slabs   # 查看 per-CPU 状态
```

### 11.3 SLUB debug 选项

内核启动参数：
```
slub_debug        — 启用所有调试
slub_debug=FZPU   — 启用特定选项 (Free/Zalloc/Poison/User)
slub_debug=,dentry  — 仅对 dentry cache 启用
```

调试检测项：
- **Red zone**：对象前后插入标记字节，检测越界写
- **Poisoning**：释放后填充特殊模式（0x6b），检测 use-after-free
- **Store user**：记录最后一次分配/释放的 call trace

---

## 12. NUMA 感知与远程节点

SLUB 支持 NUMA 感知分配：

```
alloc on node 0:
  ① per-CPU sheaf on CPU[node0]
  ② barn on node 0
  ③ partial slab on node 0
  ④ 如果 node 0 内存不足
     → barn on node N (remote)
     → 考虑 remote_node_defrag_ratio

remote_node_defrag_ratio:
  控制是否允许从远程节点分配 slab 页
  100 = 始终允许，0 = 从不允许
```

---

## 下一篇文章

[第三篇：页回收与 OOM Killer — kswapd、直接回收与 out_of_memory](./03-reclaim-oom.md)
