# 第6篇：Memory Cgroup — 分层计费、保护与 OOM

> 源码：`mm/memcontrol.c` `mm/page_counter.c` | 头文件：`include/linux/page_counter.h`

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. memory cgroup 的注册

```c
// mm/memcontrol.c:80
struct cgroup_subsys memory_cgrp_subsys __read_mostly;
// 通过 cgroup_init_early → cgroup_init 注册到 cgroup 框架

// mm/memcontrol.c:83
struct mem_cgroup *root_mem_cgroup __read_mostly;
// 根组——所有内存分配的默认归宿
```

每个 cgroup 都有一组控制文件：

```
/sys/fs/cgroup/<name>/
  memory.max          — 硬限制（字节）
  memory.min          — 保护/预留（不会被回收）
  memory.low          — 尽力保护（尽量不被回收）
  memory.high         — 软限制（超过后限速，但不杀死）
  memory.current      — 当前使用量
  memory.stat         — 详细统计
  memory.oom.group    — OOM 时杀整个 cgroup
```

---

## 2. struct page_counter — 分层计数器

```c
// include/linux/page_counter.h (定义)
struct page_counter {
    atomic_long_t usage;                      // 当前使用量
    unsigned long min;                        // 保护额度（不可回收）
    unsigned long low;                        // 尽力保护
    unsigned long high;                       // 软限制
    unsigned long max;                        // ★ 硬限制（超过则失败或 OOM）

    /* 分层统计 */
    unsigned long min_usage;                  // 自己 + 子组受保护的内存
    unsigned long low_usage;
    unsigned long children_min_usage;         // 所有子组的 min_usage 之和
    unsigned long children_low_usage;

    struct page_counter *parent;              // 父组
    unsigned long watermark;                  // 上次回收时的水位
};
```

**核心操作**（`mm/page_counter.c`）：

```c
// page_counter.c:76
void page_counter_charge(struct page_counter *counter, unsigned long nr_pages)
{
    // ① 递增 usage
    // ② 如果超过 max → 计费失败
    // ③ 如果超过 high → 标记需要限速
    // ④ 如果超过 min/low → 标记需要保护
}

// page_counter.c:54
void page_counter_cancel(struct page_counter *counter, unsigned long nr_pages)
{
    // 递减 usage，释放之前失败的计费尝试
}
```

**分层计费**：每次 `page_counter_charge` 不仅递增自己，还递增所有祖先（parent chain）。这确保了 cgroup 树中任何一级的资源限制都能生效。

---

## 3. 内存计费流程

```c
// mm/memcontrol.c:5032
int __mem_cgroup_charge(struct folio *folio, struct mm_struct *mm, gfp_t gfp)
{
    struct mem_cgroup *memcg;

    // ① 确定归属: 从 mm_struct 获取当前任务的 memcg
    memcg = get_mem_cgroup_from_mm(mm);                     // memcontrol.c:1112

    // ② 计费检查: 是否超过 memory.max?
    ret = try_charge_memcg(memcg, gfp_mask, nr_pages);      // memcontrol.c:2567

    // ③ 通过 → 设置 folio->memcg_data = memcg
    //    失败 → 回滚 (gfp 里的 __GFP_NOFAIL 阻止此路径)
}
```

```c
// memcontrol.c:2567
static int try_charge_memcg(struct mem_cgroup *memcg, gfp_t gfp_mask,
                            unsigned int nr_pages)
{
    // ① 尝试 page_counter_try_charge
    //    失败: 超过 memory.max
    //    ② 触发 memcg 级别的回收 (try_to_free_mem_cgroup_pages)
    //    ③ 回收后重试
    //    ④ 再次失败: 根据 memory.oom.group 决定
    //       - 允许 OOM → 触发 per-memcg OOM killer
    //       - 不允许 → 返回 -ENOMEM
}
```

---

## 4. Per-memcg LRU — 隔离回收

每个 memcg 有自己独立的 `lruvec`（5 个 LRU 链表），这样回收不会影响其他 cgroup：

```c
// include/linux/mmzone.h:758
struct lruvec {
    struct list_head lists[NR_LRU_LISTS];   // 5 LRU lists
    struct lruvec_stats_percpu *percpu_stats;
    // ...
};

// mm/memcontrol.c:1213
struct mem_cgroup *mem_cgroup_iter(struct mem_cgroup *root,
                                   struct mem_cgroup *prev,
                                   struct mem_cgroup_reclaim_cookie *reclaim)
{
    // 分层遍历 memcg 树——从最深的 cgroup 开始回收
    // 先回收子 cgroup，再回收父 cgroup
}
```

---

## 5. lruvec 与 shrink_node 的集成

```c
// memcontrol.c:1399
struct lruvec *folio_lruvec_lock(struct folio *folio)
{
    struct mem_cgroup *memcg = folio_memcg(folio);
    // 从 folio 找到对应 memcg
    // 从 memcg 找到对应 lruvec
    // 返回 lruvec（带锁）
}

// memcontrol.c:1427
struct lruvec *folio_lruvec_lock_irq(struct folio *folio)
{
    // 同上面但关中断——用于 LRU 操作在中断上下文中
}
```

**实际回收路径**：`shrink_node` → `shrink_node_memcgs` → 遍历 memcg 树 → 对每个 memcg 的 lruvec 调用 `shrink_lruvec`。这是全局回收和 per-memcg 回收的统一入口。

---

## 6. Per-memcg OOM

```c
// mm/memcontrol.c:1885
static bool mem_cgroup_out_of_memory(struct mem_cgroup *memcg,
                                     gfp_t gfp_mask, int order)
{
    struct oom_control oc = { .memcg = memcg, .gfp_mask = gfp_mask };

    // ★ 仅在 memory.oom.group 条件下才走 per-memcg OOM
    if (!memcg->oom_group)
        return false;

    // 范围: 只杀这个 cgroup 内的进程
    oc.order = order;
    oc.memcg = memcg;
    out_of_memory(&oc);
    return true;
}

// mm/memcontrol.c:1947
struct mem_cgroup *mem_cgroup_get_oom_group(struct task_struct *victim,
                                            struct mem_cgroup *oom_domain)
{
    // 找到 victim 上标记了 oom_group 的最近 memcg
    // 只有这个组内的进程会被杀
}
```

**全局 OOM vs memcg OOM 的区别**：

| | 全局 OOM | memcg OOM |
|---|---|---|
| 触发条件 | 系统所有 zone 水位失败 | memory.max 被超过 |
| 受害者范围 | 全系统的进程 | 只有这个 cgroup 内的进程 |
| oom_kill_process 调用 | 通用 | mem_cgroup_out_of_memory 子调用 |
| oom_score_adj 的比较范围 | 全局 | 本 cgroup 内 |

---

## 7. 保护机制 — memory.min 和 memory.low

```c
// Protection 在回收时被检测:
// shrink_node → shrink_lruvec:
//   if (memcg->memory.min > 0) {
//       // 该 memcg 有 min 保护 → 跳过，先回收其他无保护的 memcg
//       protected = true;
//   }
//   if (memcg->memory.low > 0 && !protected) {
//       // low 保护 → 尽量不回收，但压力大时可能被回收
//       proportional_reclaim_weight = calc_low_weight();
//   }
```

**保护的分层逻辑**：父组的 `children_min_usage` 会向下传递保护——如果父组有 1GB min，子组可以共享这 1GB 的受保护内存。

---

## 8. vmpressure — 内存压力通知

```c
// mm/vmpressure.c (14KB)
// 当 cgroup 的回收率达到一定阈值时，通过 eventfd 通知用户态
// 事件级别: low (回收 60%)、medium (70%)、critical (80%)

void vmpressure(gfp_t gfp, struct mem_cgroup *memcg, bool tree,
                unsigned long scanned, unsigned long reclaimed)
{
    // 计算压力: reclaimed / scanned
    // 达到阈值 → 通过 vmpressure->events 链表通知用户态
    // 用户态可以据此触发 workload shutdown、memory hotplug 等
}
```

---

## 9. 系列结语

6 篇文章从 buddy 页分配器的伙伴系统+水位线+迁移类型起步，到 SLUB 的 sheaf 缓存+锁less 快速路径+slab 慢路径，再到 vmscan 的 LRU 回收+kswapd+直接回收+OOM killer 的全套内存压力处理，然后深入到缺页处理（匿名页、文件页、COW）+mmap/mlock/madvise 的 VMA 生命周期，接着展开 THP 的大页机制+compaction 的碎片整理+KSM 的同页合并三者的协同关系，最后覆盖 memory cgroup 的分层计费+保护+per-memcg OOM+vmpressure 压力通知。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-buddy | Buddy 页分配器 | `page_alloc.c:1883,3389`, `mmzone.h:192` |
| 02-slub | SLUB 分配器 | `slub.c:404,420,4870`, `slab_common.c:318` |
| 03-reclaim-oom | 页回收与 OOM | `vmscan.c:7438,1951`, `oom_kill.c:1103,199` |
| 04-page-fault | 缺页与 VMA 生命周期 | `memory.c:4244,5337,6699`, `mmap.c:336` |
| 05-thp-compact-ksm | THP+Compaction+KSM | `huge_memory.c:1538`, `compaction.c:2528`, `ksm.c:2783` |
| 06-memcg | Memory Cgroup | `memcontrol.c:80,2567,5032`, `page_counter.c:76` |