# 第三篇：页回收与 OOM Killer — kswapd、直接回收与 out_of_memory

> 源码：mm/vmscan.c, mm/oom_kill.c, mm/page_alloc.c | 头文件：include/linux/mmzone.h

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. 概述

当 Buddy 分配器无法满足分配请求时，内核不会立即放弃。它首先尝试**回收(Reclaim)**——将可回收的页腾出内存。回收由两条路径驱动：

1. **kswapd（后台回收）**：每个 NUMA node 有一个内核线程，在空闲内存低于 LOW 水位时被唤醒，回收直到高于 HIGH 水位
2. **直接回收（direct reclaim）**：分配者在慢速路径直接回收页，发生在 kswapd 来不及补充时

当所有回收努力都无法释放足够内存时，最终手段是 **OOM Killer**——选择并杀死内存消耗最大的进程。

**核心文件**：
- `mm/vmscan.c` — 页回收核心逻辑（~8070 行）
- `mm/oom_kill.c` — OOM Killer 实现（~1260 行）
- `include/linux/mmzone.h` — LRU 链表枚举、zone watermark

---

## 2. LRU 链表架构

### 2.1 LRU 链表枚举

`include/linux/mmzone.h:382-393`
```c
#define LRU_BASE 0
#define LRU_ACTIVE 1
#define LRU_FILE 2

enum lru_list {
    LRU_INACTIVE_ANON = LRU_BASE,              // 0: 不活跃匿名页
    LRU_ACTIVE_ANON   = LRU_BASE + LRU_ACTIVE, // 1: 活跃匿名页
    LRU_INACTIVE_FILE = LRU_BASE + LRU_FILE,    // 2: 不活跃文件页
    LRU_ACTIVE_FILE   = LRU_BASE + LRU_FILE + LRU_ACTIVE, // 3: 活跃文件页
    LRU_UNEVICTABLE,                            // 4: 不可回收页
    NR_LRU_LISTS                                // 5
};
```

**LRU 分类逻辑**：

```
                          page 首次分配
                              │
                              ▼
                     ┌────────────────┐
                     │  LRU_INACTIVE  │
                     │  (不活跃链表)   │
                     └───────┬────────┘
                             │ 被访问 (folio_referenced)
                             ▼
                     ┌────────────────┐
                     │  LRU_ACTIVE    │
                     │  (活跃链表)     │
                     └───────┬────────┘
                             │ 长期未访问
                             ▼
                     ┌────────────────┐
                     │  LRU_INACTIVE  │  (回退到不活跃)
                     └───────┬────────┘
                             │ 仍被访问 → 可能再晋升
                             │ 未被访问 → 回收
                             ▼
                        回收 / swap out

LRU_UNEVICTABLE: 被 mlock() 锁定的页、被 VFIO pinned 的页
```

### 2.2 per-lruvec 层级结构

```
pglist_data (NUMA node)
└── node_mem_cgroup → lruvec
     ├── lists[LRU_INACTIVE_ANON]  → folio → folio → folio
     ├── lists[LRU_ACTIVE_ANON]    → folio → folio
     ├── lists[LRU_INACTIVE_FILE]  → folio → folio → folio → folio
     ├── lists[LRU_ACTIVE_FILE]    → folio
     └── lists[LRU_UNEVICTABLE]    → folio → folio

每个 folio 通过 folio->lru 链入对应 LRU 链表
folio 的状态通过 flags 中的 PG_lru、PG_active、PG_unevictable 等位表示
```

---

## 3. struct scan_control — 回收控制参数

`mm/vmscan.c:74-145`

```c
struct scan_control {
    unsigned long nr_to_reclaim;    // 目标回收页数
    nodemask_t  *nodemask;          // 允许扫描的 NUMA 节点集

    struct mem_cgroup *target_mem_cgroup;  // 目标 memcg（memcg 回收）

    // 文件页 vs 匿名页扫描压力平衡
    unsigned long anon_cost;
    unsigned long file_cost;

    int *proactive_swappiness;      // 用户主动回收的 swappiness

    // 回收行为控制
    unsigned int may_writepage:1;   // 允许回写脏页
    unsigned int may_unmap:1;       // 允许解除页表映射
    unsigned int may_swap:1;        // 允许 swap out 匿名页
    unsigned int may_deactivate:2;  // 允许降级活跃页
    unsigned int force_deactivate:1;
    unsigned int skipped_deactivate:1;
    unsigned int no_cache_trim_mode:1;

    // 回收优先级（0=最高，DEF_PRIORITY=12=最低）
    int priority;

    // 统计
    struct reclaim_state reclaim_state;

    // 扫描/回收计数
    struct {
        unsigned int nr_dirty;      // 遇到脏页数
        unsigned int nr_unqueued_dirty;
        unsigned int nr_congested;
        unsigned int nr_writeback;  // 正在回写页数
        unsigned int nr_immediate;
        unsigned int nr_pageout;    // 发起写回数
        unsigned int nr_ref_keep;   // 保留引用的页数
        unsigned int nr_unmap_fail;
    } nr;
};
```

关键控制位：
- `may_swap`：是否允许换出匿名页到 swap。若为 0，匿名页不能被回收
- `may_writepage`：是否允许回写脏文件页。若为 0，脏文件页跳过
- `may_unmap`：是否允许解除页表映射（`try_to_unmap`）。remap 文件页可能需要

---

## 4. kswapd — 后台回收守护线程

### 4.1 kswapd 主循环

`mm/vmscan.c:7438-7510`

```c
static int kswapd(void *p)
{
    unsigned int alloc_order, reclaim_order;
    pg_data_t *pgdat = (pg_data_t *)p;
    struct task_struct *tsk = current;

    // kswapd 设为 PF_MEMALLOC：允许它使用保留内存
    tsk->flags |= PF_MEMALLOC | PF_KSWAPD;
    set_freezable();

    for ( ; ; ) {
        bool was_frozen;

        // 读取需要回收的 order 和 zone
        alloc_order = reclaim_order = READ_ONCE(pgdat->kswapd_order);
        highest_zoneidx = kswapd_highest_zoneidx(pgdat, highest_zoneidx);

kswapd_try_sleep:
        // 1. 休眠：直到内存水位低于 LOW 或被唤醒
        kswapd_try_to_sleep(pgdat, alloc_order, reclaim_order,
                            highest_zoneidx);

        // 2. 重新读取 wakeup 设置的 order
        alloc_order = READ_ONCE(pgdat->kswapd_order);
        highest_zoneidx = kswapd_highest_zoneidx(pgdat, highest_zoneidx);

        // 3. 检查是否需要停止（freeze/suspend）
        if (kthread_freezable_should_stop(&was_frozen))
            break;
        if (was_frozen)
            continue;

        // 4. 执行回收
        reclaim_order = balance_pgdat(pgdat, alloc_order, highest_zoneidx);

        // 5. 如果回收的 order 小于请求的 order
        //    → 说明高 order 回收失败，降级为 order-0
        //    → 重新休眠（等 kcompactd 做 compaction）
        if (reclaim_order < alloc_order)
            goto kswapd_try_sleep;
    }

    tsk->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
    return 0;
}
```

### 4.2 kswapd 唤醒与休眠条件

```
休眠条件：所有 zone 的空闲内存 > HIGH watermark
唤醒条件：
  ① 分配者调用 wakeup_kswapd()（page_alloc.c 慢速路径）
     → kswapd_order = max(kswapd_order, alloc_order)
     → 当 zone 空闲内存 < LOW watermark
  ② 外部事件：memory hotplug、compaction 失败等

kswapd 周期：
  ┌──────────┐    free > HIGH    ┌──────────┐
  │  SLEEP   │ ◄──────────────── │  ACTIVE  │
  │ (休眠)   │                  │  (回收)  │
  └──────────┘                  └──────────┘
       │                              │
       │ free < LOW                   │
       └──────────────────────────────┘
           被 wakeup_kswapd() 唤醒
```

### 4.3 balance_pgdat — 节点级回收循环

kswapd 调用 `balance_pgdat()`，对每个 zone 循环回收：

```c
balance_pgdat(pgdat, alloc_order, highest_zoneidx) {
    // 从最高 zone 向下扫描
    for (i = 0; i <= end_zone; i++) {
        zone = pgdat->node_zones + i;

        // 水位检查：free > high 则跳过
        if (zone_watermark_ok_safe(zone, order, high_wmark_pages(zone), ...))
            continue;

        // 回收
        sc.nr_to_reclaim = max(SWAP_CLUSTER_MAX, high_wmark_pages(zone));
        shrink_node(pgdat, &sc);
    }

    // 检查是否达到目标
    if (sc.nr_reclaimed >= nr_to_reclaim && 水位恢复)
        break;  // 回收完成
}
```

---

## 5. shrink_node — 节点回收核心

`mm/vmscan.c:6190-6260`

```c
static void shrink_node(pg_data_t *pgdat, struct scan_control *sc)
{
    unsigned long nr_reclaimed, nr_scanned, nr_node_reclaimed;
    struct lruvec *target_lruvec;

    // 1. 如果启用了 MGLRU（多代 LRU），走 MGLRU 路径
    if (lru_gen_enabled() || lru_gen_switching()) {
        memset(&sc->nr, 0, sizeof(sc->nr));
        lru_gen_shrink_node(pgdat, sc);
        return;
    }

    target_lruvec = mem_cgroup_lruvec(sc->target_mem_cgroup, pgdat);

again:
    // 2. 遍历所有 memcg（如果有目标 memcg 则只处理它）
    shrink_node_memcgs(pgdat, sc);

    // 3. 如果回收了页，标记 reclaimable
    if (nr_node_reclaimed)
        reclaimable = true;

    // 4. kswapd 特殊处理：回写检查
    if (current_is_kswapd()) {
        // 如果回收的页全是回写中的页 → 标记 PGDAT_WRITEBACK
        if (sc->nr.writeback && sc->nr.writeback == sc->nr.taken)
            set_bit(PGDAT_WRITEBACK, &pgdat->flags);

        // 如果遇到 immediate reclaim 标记的页 → 节流等待回写完成
        if (sc->nr.immediate)
            reclaim_throttle(pgdat, VMSCAN_THROTTLE_WRITEBACK);
    }

    // 5. 如果需要继续（还没达到目标）
    if (should_continue_reclaim(pgdat, nr_node_reclaimed, sc))
        goto again;
}
```

---

## 6. shrink_inactive_list — 不活跃链表回收

`mm/vmscan.c:1951-2020`

这是回收的"主力"函数，负责从不活跃 LRU 链表取出页进行回收：

```c
static unsigned long shrink_inactive_list(unsigned long nr_to_scan,
        struct lruvec *lruvec, struct scan_control *sc,
        enum lru_list lru)
{
    LIST_HEAD(folio_list);
    unsigned long nr_scanned;
    unsigned int nr_reclaimed = 0;
    unsigned long nr_taken;
    struct reclaim_stat stat;
    bool file = is_file_lru(lru);
    struct pglist_data *pgdat = lruvec_pgdat(lruvec);
    bool stalled = false;

    // 1. 节流：如果太多页正在隔离，等待
    while (unlikely(too_many_isolated(pgdat, file, sc))) {
        if (stalled)
            return 0;
        stalled = true;
        reclaim_throttle(pgdat, VMSCAN_THROTTLE_ISOLATED);
        if (fatal_signal_pending(current))
            return SWAP_CLUSTER_MAX;
    }

    // 2. 从 LRU 链表隔离页（一次最多 32 页 = SWAP_CLUSTER_MAX）
    lru_add_drain();
    lruvec_lock_irq(lruvec);
    nr_taken = isolate_lru_folios(nr_to_scan, lruvec, &folio_list,
                                   &nr_scanned, sc, lru);
    lruvec_unlock_irq(lruvec);

    if (nr_taken == 0)
        return 0;

    // 3. 对隔离出的 folio 列表执行实际回收
    nr_reclaimed = shrink_folio_list(&folio_list, pgdat, sc, &stat, false,
                                     lruvec_memcg(lruvec));

    // 4. 未回收的 folio 放回 LRU
    move_folios_to_lru(&folio_list);

    return nr_reclaimed;
}
```

**shrink_folio_list 回收决策**：
```
对每个 folio:
  ├─ 脏文件页 (PG_dirty) ────────────────────────────────────────
  │   ├─ may_writepage → 发起回写 → 页标记为回写中 → 放回 LRU
  │   └─ !may_writepage → 跳过，放回 LRU
  │
  ├─ 干净文件页 ─────────────────────────────────────────────────
  │   └─ 直接释放 (folio_unlock + free)
  │
  ├─ 匿名页 (anon) ──────────────────────────────────────────────
  │   ├─ may_swap → 添加到 swap cache → 解除页表映射 → 写入 swap
  │   └─ !may_swap → 无法回收，放回 LRU
  │
  ├─ 正在回写 (PG_writeback) ────────────────────────────────────
  │   └─ 等待或跳过
  │
  └─ 被引用 (PG_referenced) ─────────────────────────────────────
      └─ 放回活跃链表
```

---

## 7. shrink_active_list — 活跃链表降级

`mm/vmscan.c:2068-2130`

活跃链表的作用是保护频繁访问的页不被回收。`shrink_active_list` 将活跃页降级到不活跃链表：

```c
static void shrink_active_list(unsigned long nr_to_scan,
                               struct lruvec *lruvec,
                               struct scan_control *sc,
                               enum lru_list lru)
{
    unsigned long nr_taken;
    LIST_HEAD(l_hold);    // 从活跃链表隔离出的页
    LIST_HEAD(l_active);  // 保留在活跃链表的页
    LIST_HEAD(l_inactive); // 降级到不活跃链表的页
    unsigned nr_deactivate, nr_activate;
    bool file = is_file_lru(lru);

    lru_add_drain();
    lruvec_lock_irq(lruvec);

    // 1. 从活跃 LRU 隔离 folio
    nr_taken = isolate_lru_folios(nr_to_scan, lruvec, &l_hold,
                                   &nr_scanned, sc, lru);
    lruvec_unlock_irq(lruvec);

    // 2. 逐个检查每个 folio 的引用状态
    while (!list_empty(&l_hold)) {
        folio = lru_to_folio(&l_hold);
        list_del(&folio->lru);

        // 不可回收页 → 放回
        if (unlikely(!folio_evictable(folio))) {
            folio_putback_lru(folio);
            continue;
        }

        // 3. 检查是否被引用 (folio_referenced)
        if (folio_referenced(folio, 0, sc->target_mem_cgroup,
                             &vm_flags) != 0) {
            // 仍被引用 → 保留在活跃链表
            nr_rotate++;
            list_add(&folio->lru, &l_active);
        } else {
            // 未被引用 → 降级到不活跃链表
            nr_deactivate++;
            list_add(&folio->lru, &l_inactive);
        }
    }

    // 4. 将页放回对应 LRU
    move_active_folios_to_lru(lruvec, &l_active, &l_inactive, lru);
}
```

**活跃/不活跃迁移图示**：
```
                     folio_referenced() == 0 (最近未访问)
LRU_ACTIVE  ──────────────────────────────────────►  LRU_INACTIVE
  (活跃)                                               (不活跃)
    ▲                                                       │
    │                          folio_referenced() != 0       │
    │                          (被访问)                      │
    │                                                       │
    └───────────────────────────────────────────────────────┘
                    新分配页 → INACTIVE
                    INACTIVE 被再次访问 → ACTIVE
```

---

## 8. 直接回收 (Direct Reclaim)

### 8.1 __alloc_pages_direct_reclaim

`mm/page_alloc.c:4409-4430`

当 kswapd 回收速度跟不上分配速度，且慢速路径已经尝试过低水位分配后仍然失败：

```c
static inline struct page *
__alloc_pages_direct_reclaim(gfp_t gfp_mask, unsigned int order,
        unsigned int alloc_flags, const struct alloc_context *ac,
        unsigned long *did_some_progress)
{
    struct page *page = NULL;

    // 1. 执行直接回收
    *did_some_progress = __perform_reclaim(gfp_mask, order, ac);

    if (unlikely(!(*did_some_progress)))
        return NULL;

    // 2. 回收后重试分配
retry:
    page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

    // 3. 如果没有 GFP_NORETRY，允许循环
    if (!page && !(gfp_mask & __GFP_NORETRY))
        goto retry;

    return page;
}
```

### 8.2 throttle_direct_reclaim — 限制并发直接回收

`mm/vmscan.c:6640-6660`

过多的直接回收者会相互竞争，内核会限制并发量：

```c
static bool throttle_direct_reclaim(gfp_t gfp_mask, struct zonelist *zonelist,
                                    nodemask_t *nodemask)
{
    // 检查每个 zone 的空闲内存是否极度不足
    // 如果 PFMEMALLOC 保留内存也被大量消耗 → 需要节流
    // 让 kswapd 有 CPU 时间做后台回收
}
```

---

## 9. 回收优先级 (Scan Priority)

回收使用 12 级优先级（`DEF_PRIORITY = 12`，0 最高）：

```
priority 12 (最低优先级):
  扫描少量页，只回收最不活跃的页
    │
    ▼
priority 8:
  扫描更多页，开始扫描活跃链表
    │
    ▼
priority 0 (最高优先级):
  扫描所有 LRU 页，不放过任何可回收的页
  尝试 flush 脏页，尝试 swap
```

**扫描量与 priority 的关系**：
```
每个 zone 每次扫描的页数：
  scan_size = zone_total_pages >> priority

priority=12 → 扫描 zone_total_pages / 4096 页
priority=8  → 扫描 zone_total_pages / 256 页
priority=0  → 扫描 zone_total_pages 页（全部扫）
```

**回收循环**：
```
do {
    shrink_node() → 遍历 LRU，递减 priority
} while (sc->nr_reclaimed < sc->nr_to_reclaim && --sc->priority >= 0);
```

---

## 10. 文件页 vs 匿名页的平衡

### swappiness — 匿名页回收倾向

`/proc/sys/vm/swappiness`（默认 60）：

```
swappiness 决定匿名页相对于文件页的回收倾向：

  swappiness ≈ 0   → 优先回收文件页，几乎不 swap
  swappiness = 60  → 文件页和匿名页差不多平等对待
  swappiness = 100 → 文件页和匿名页完全平等对待
  swappiness = 200 → 优先回收匿名页

anon_prio = sc->swappiness    (swappiness value, max 200)
file_prio = 200 - anon_prio + 1

scan_balance:
  anon_scan_ratio : file_scan_ratio ≈ anon_prio : file_prio
```

---

## 11. OOM Killer

### 11.1 out_of_memory — OOM 入口

`mm/oom_kill.c:1103-1170`

当直接回收也无法获取内存时，慢速路径调用 `out_of_memory()`：

```c
bool out_of_memory(struct oom_control *oc)
{
    unsigned long freed = 0;

    // 1. OOM killer 是否被禁用？
    if (oom_killer_disabled)
        return false;

    // 2. 非 memcg OOM：先通知 oom_notify_list（给注册者一次释放机会）
    if (!is_memcg_oom(oc)) {
        blocking_notifier_call_chain(&oom_notify_list, 0, &freed);
        if (freed > 0 && !is_sysrq_oom(oc))
            return true;  // 有些内存被释放了
    }

    // 3. 如果 current 进程正被 SIGKILL 或正在退出
    //    → 标记为 OOM victim，让它尽快结束释放内存
    if (task_will_free_mem(current)) {
        mark_oom_victim(current);
        queue_oom_reaper(current);
        return true;
    }

    // 4. GFP_NOFS 且非 memcg OOM → 无法做更多，返回
    if (!(oc->gfp_mask & __GFP_FS) && !is_memcg_oom(oc))
        return true;

    // 5. 检查分配约束（cpuset/memory policy/memcg）
    oc->constraint = constrained_alloc(oc);
    check_panic_on_oom(oc);

    // 6. 如果设置了 oom_kill_allocating_task → 直接杀分配者
    if (!is_memcg_oom(oc) && sysctl_oom_kill_allocating_task && ...) {
        get_task_struct(current);
        oc->chosen = current;
        oom_kill_process(oc, "Out of memory (oom_kill_allocating_task)");
        return true;
    }

    // 7. 选择最坏的进程
    select_bad_process(oc);
    if (!oc->chosen) {
        // 没有可杀进程 → 系统即将 panic
        dump_header(oc);
        pr_warn("Out of memory and no killable processes...\n");
        panic("System is deadlocked on memory\n");
    }

    // 8. 杀死选中的进程
    if (oc->chosen && oc->chosen != (void *)-1UL)
        oom_kill_process(oc, !is_memcg_oom(oc) ?
                         "Out of memory" : "Memory cgroup out of memory");

    return !!oc->chosen;
}
```

### 11.2 oom_badness — 计算进程"坏度"

`mm/oom_kill.c:199-237`

```c
long oom_badness(struct task_struct *p, unsigned long totalpages)
{
    long points;
    long adj;

    // 1. 跳过不可杀的任务（init、内核线程）
    if (oom_unkillable_task(p))
        return LONG_MIN;

    p = find_lock_task_mm(p);
    if (!p)
        return LONG_MIN;

    // 2. 跳过 oom_score_adj = -1000 或已被标记 OOM_SKIP 的进程
    adj = (long)p->signal->oom_score_adj;
    if (adj == OOM_SCORE_ADJ_MIN ||   // -1000（不可被 OOM kill）
        mm_flags_test(MMF_OOM_SKIP, p->mm) ||
        in_vfork(p)) {
        return LONG_MIN;
    }

    // 3. 核心评分公式：
    //    points = RSS + Swap Entries + Page Tables
    points = get_mm_rss_sum(p->mm) +
             get_mm_counter_sum(p->mm, MM_SWAPENTS) +
             mm_pgtables_bytes(p->mm) / PAGE_SIZE;

    // 4. 应用 oom_score_adj：
    //    adj = oom_score_adj * totalpages / 1000
    //    points = points + adj
    adj *= totalpages / 1000;
    points += adj;

    return points;
}
```

**评分详解**：

```
points = 常驻内存页(RSS)
       + swap entries (换出但未释放的页)
       + 页表页 (Page Table pages)
       + oom_score_adj * totalpages / 1000

其中 oom_score_adj:
  -1000 → 完全豁免 OOM kill（如 sshd）
  0     → 默认
  500   → 增加 50% 的"坏度"，更容易被杀
  1000  → 总是被杀
```

可通过 `/proc/<pid>/oom_score` 查看当前分数，`/proc/<pid>/oom_score_adj` 调整偏移。

### 11.3 select_bad_process — 选出最坏进程

`mm/oom_kill.c:362-380`

```c
static void select_bad_process(struct oom_control *oc)
{
    oc->chosen_points = LONG_MIN;

    if (is_memcg_oom(oc))
        // memcg OOM：只扫描该 cgroup 内的进程
        mem_cgroup_scan_tasks(oc->memcg, oom_evaluate_task, oc);
    else {
        // 全局 OOM：遍历所有进程
        struct task_struct *p;
        rcu_read_lock();
        for_each_process(p)
            if (oom_evaluate_task(p, oc))
                break;  // 找到足够"坏"的进程
        rcu_read_unlock();
    }
}
```

**oom_evaluate_task 选择策略**：
```
对每个进程：
  ① 计算 oom_badness(p)
  ② 如果分数 > oc->chosen_points → 更新 chosen
  ③ 额外启发式：
     - 优先杀子进程多的（释放更多内存）
     - 优先杀非 root 进程
     - 如果启用了 oom_reaper，优先杀已有已退出线程的进程（内存已被 reaper 标记）
     - 如果 oc->chosen_points > oc->totalpages → 宣布找到（足够"坏"了）
```

### 11.4 oom_kill_process — 执行杀死

`mm/oom_kill.c:1008-1045`

```c
static void oom_kill_process(struct oom_control *oc, const char *message)
{
    struct task_struct *victim = oc->chosen;
    struct mem_cgroup *oom_group;

    // 1. 如果 victim 正在退出 → 标记 OOM victim 即可，让它自然死亡
    task_lock(victim);
    if (task_will_free_mem(victim)) {
        mark_oom_victim(victim);
        queue_oom_reaper(victim);  // 异步回收器会加速释放
        task_unlock(victim);
        return;
    }
    task_unlock(victim);

    // 2. 如果是 memcg oom，尝试发送 SIGKILL 到整个 cgroup
    if (is_memcg_oom(oc)) {
        oom_group = mem_cgroup_get_oom_group(victim, oc->memcg);
        if (oom_group) {
            // 杀整个 cgroup
            ...
        }
    }

    // 3. 打印 OOM 报告
    dump_header(oc);
    pr_err("%s: Killed process %d (%s) ...\n",
           message, task_pid_nr(victim), victim->comm);

    // 4. 发送 SIGKILL
    do_send_sig_info(SIGKILL, SEND_SIG_PRIV, victim, PIDTYPE_TGID);
}
```

### 11.5 oom_reaper — 异步内存回收

OOM reaper 是一个独立的内核线程，它不会等待 victim 进程自然退出，而是主动进行异步回收：

```
victim 被标记 OOM victim
  │
  ├─ mark_oom_victim()  → MMF_OOM_VICTIM 标志
  │
  ├─ queue_oom_reaper() → 唤醒 oom_reaper 线程
  │   │
  │   └─ oom_reaper:
  │       ├─ 获取 victim 的 mm
  │       ├─ mmap_write_lock(mm)
  │       ├─ 遍历所有 VMA
  │       │   ├─ 匿名页 → 解除页表映射 (unmap)
  │       │   └─ 文件页 → 释放
  │       ├─ mmap_write_unlock(mm)
  │       └─ 标记 MMF_OOM_SKIP (下次 OOM 跳过此进程)
  │
  └─ victim 收到 SIGKILL → 退出 → free 剩余内存
```

---

## 12. 回收与 OOM 的完整决策链

```
alloc_pages(order, gfp)
  │
  ├─[快速路径]────────────────────────────────
  │  ALLOC_WMARK_LOW
  │  get_page_from_freelist → 成功 → 返回
  │
  └─[慢速路径] __alloc_pages_slowpath
       │
       ├─ ① wake kswapd
       │     → 后台异步回收（不阻塞分配者）
       │
       ├─ ② ALLOC_WMARK_MIN → 再试分配
       │     get_page_from_freelist → 成功 → 返回
       │
       ├─ ③ compaction → 再试分配
       │     成功 → 返回
       │
       ├─ ④ direct reclaim
       │     __alloc_pages_direct_reclaim()
       │       → __perform_reclaim()
       │         → try_to_free_pages()
       │           → do_try_to_free_pages()
       │             ├─ shrink_node(pgdat, &sc)
       │             │   └─ shrink_node_memcgs()
       │             │       └─ shrink_lruvec()
       │             │           ├─ shrink_active_list()  (活跃→不活跃)
       │             │           └─ shrink_inactive_list() (回收/swap)
       │             └─ 返回 reclaimed 页数
       │
       │     回收了一些页 → 再试分配 → 成功 → 返回
       │     没有进展 → 视情况重试
       │
       ├─ ⑤ 循环重试（最多 16 次，含 compaction 重试）
       │     should_compact_retry() → 再试 compaction
       │     should_reclaim_retry()  → 再试 reclaim
       │     成功 → 返回
       │
       ├─ ⑥ __GFP_RETRY_MAYFAIL 且 order <= COMPACT_COSTLY →
       │     允许无限重试（但会 throttle）
       │
       └─ ⑦ 所有手段耗尽
            │
            ├─ __GFP_NOFAIL → goto retry (永远重试，可能死锁)
            │
            └─ 触发 OOM：
                 __alloc_pages_may_oom()
                   → out_of_memory(&oc)
                     ├─ select_bad_process()
                     │   └─ oom_badness() 对每个进程评分
                     └─ oom_kill_process()
                         ├─ 发送 SIGKILL
                         ├─ queue_oom_reaper() → 异步回收内存
                         └─ dump_header() → dmesg 输出 OOM 报告
```

---

## 13. OOM 触发条件总结

OOM 不是任意条件下都会触发。内核会逐步升级：

```
条件                                                        动作
─────────────────────────────────────────────────────────────────────
zone free < WMARK_LOW                                     → wake kswapd
zone free < WMARK_MIN 且 kswapd 未及时补充                 → direct reclaim
direct reclaim 达到 priority=0 且 nr_reclaimed=0         → 考虑 OOM
非 ALLOC_OOM / ALLOC_NO_WATERMARKS 分配者                 → OOM
当前有 OOM victim 正在退出                                → 等待，不触发新 OOM
oom_killer_disabled (sysctl)                              → 直接返回失败
panic_on_oom (sysctl)                                     → panic 而不是 kill
```

**抑制 OOM 的条件**：
- 已有 OOM victim 正在被 oom_reaper 回收中
- `oom_killer_disabled` 开启（通常在内存热插拔期间）
- `GFP_NOFS` 且不是 memcg OOM（文件系统递归风险）
- `current` 已被标记 OOM victim（让它自己释放内存）

---

## 14. /proc 监控接口

```bash
# 查看 LRU 链表大小
cat /proc/meminfo | grep -E "Active|Inactive|Unevictable"
Active:          4194304 kB
Inactive:        2097152 kB
Active(anon):    1048576 kB
Inactive(anon):   524288 kB
Active(file):    3145728 kB
Inactive(file):  1572864 kB
Unevictable:          0 kB

# 查看各 zone 水位线
cat /proc/zoneinfo | grep -E "min|low|high"

# 查看各进程 oom_score
cat /proc/$(pgrep -f process)/oom_score

# 调整 oom_score_adj（-1000 到 1000）
echo -500 > /proc/$(pgrep -f process)/oom_score_adj

# swappiness
cat /proc/sys/vm/swappiness  # 默认 60

# 是否有正在进行的 OOM
dmesg | grep -i "out of memory\|oom"
```

---

## 下一篇文章

[第四篇：缺页异常与 mmap — page fault 处理、VMA 与 file-backed](./04-page-fault.md)
