# 第5篇：THP、Compaction 与 KSM — 大页、碎片整理与同页合并

> 源码：`mm/huge_memory.c` `mm/compaction.c` `mm/ksm.c` | 头文件：`include/linux/huge_mm.h`

系列目录：[内存管理 内核源码深度解析](./README.md)

---

## 1. THP — Transparent Huge Pages

THP 自动将连续的 512 个 4KB 页合并为一个 2MB 的大页，减少 TLB 压力。

```c
// mm/huge_memory.c:59
unsigned long transparent_hugepage_flags __read_mostly;

// 控制: /sys/kernel/mm/transparent_hugepage/enabled
// "always" — 总是分配 2MB 页
// "madvise" — 仅在 MADV_HUGEPAGE 区域分配
// "never" — 禁用（等同于 CONFIG_TRANSPARENT_HUGEPAGE=n）
```

### 1.1 THP 缺页分配

```c
// mm/huge_memory.c:1538
vm_fault_t do_huge_pmd_anonymous_page(struct vm_fault *vmf)
{
    // ① 检查是否允许: transparent_hugepage_flags & enabled
    // ② 从 buddy 分配 2MB 连续页 (order=HPAGE_PMD_ORDER)
    folio = alloc_huge_folio(vmf, HPAGE_PMD_ORDER);

    // ③ 安装 PMD 级别的 THP entry
    entry = mk_huge_pmd(&folio->page, vma->vm_page_prot);
    if (vmf->flags & FAULT_FLAG_WRITE)
        entry = pmd_mkwrite(entry, vma);
    set_pmd_at(vma->vm_mm, vmf->address, vmf->pmd, entry);

    return 0;
}
```

**关键**：THP 缺页走的是 PMD 级别，而不是 PTE 级别。`__handle_mm_fault` 在 `pmd_alloc` 后优先检查 `pmd_trans_huge`，如果满足条件直接调用 `do_huge_pmd_anonymous_page`。

### 1.2 Deferred Split — 延迟分裂

THP 分配后可能部分被释放（如 `madvise(MADV_DONTNEED)` 了其中一部分），剩下的 4KB 页浪费空间。`deferred_split_shrinker` 检测这些"半空"的 THP 并分裂：

```c
// mm/huge_memory.c:70
static struct shrinker deferred_split_shrinker = {
    .count_objects = deferred_split_count,
    .scan_objects = deferred_split_scan,
};

// mm/huge_memory.c:75
static bool split_underused_thp(struct folio *folio)
{
    // 检查 THP 中有多少"活的"4KB 子页
    // 如果利用率太低 → split 回 4KB 页
}
```

---

## 2. Compaction — 内存碎片整理

长期运行后 buddy 系统被碎片化，很多 2MB+ 的 order 无法分配。Compaction 迁移页去创建连续的空闲块。

```c
// mm/compaction.c:2528
static enum compact_result compact_zone(struct zone *zone,
                                        struct compact_control *cc)
{
    // ① 两个扫描器:
    //    migrate scanner (从 zone 低端开始): 找到可迁移的页
    //    free scanner (从 zone 高端开始): 找到空闲页
    //
    // ② 迁移: 将 migrate scanner 找到的页迁移到 free scanner 的空闲位置
    // ③ 合并: 迁移后留下的空闲区被合并为连续大块

    while (cc->nr_migratepages) {
        isolate_migratepages_range(cc, ...);    // 隔离可迁移页
        compact_finished(zone, cc, result);     // 检查是否达到目标 order
        migrate_pages(&cc->migratepages, ...);  // 迁移
    }
}
```

### 2.1 kcompactd — 后台碎片整理线程

```c
// mm/compaction.c:3182
static int kcompactd(void *p)
{
    struct compact_control cc;
    // 每个 NUMA node 一个 kcompactd
    // 当 kswapd 唤醒时也唤醒 kcompactd
    // 目标 order: COMPACTION_HPAGE_ORDER (HPAGE_PMD_ORDER)
}
```

### 2.2 何时触发 compaction

```
__alloc_pages_slowpath:
  → watermark check 失败
  → try_to_compact_pages → compact_zone  (直接整理)
  → compaction 失败后 → reclaim → 再尝试
```

---

## 3. KSM — Kernel Same-page Merging

KSM 扫描匿名页内容，发现完全相同的页后合并为单个写保护的共享页。**虚拟机场景神器**——多台相同 OS 的 VM 共享大量相同的代码页和数据页。

```c
// mm/ksm.c:61-80 (DOC block)
// Two data structures:
//  stable tree: merged pages, sorted by content, write-protected
//  unstable tree: candidate pages not yet merged
```

### 3.1 扫描线程

```c
// mm/ksm.c:2783
static void ksm_do_scan(unsigned int scan_npages)
{
    // ① 遍历注册到 KSM 的 VMA（标记了 MADV_MERGEABLE）
    // ② 读页内容 → 计算 checksum
    // ③ 搜索 stable tree 查找匹配
    //    找到: cmp_and_merge_page → 如果内容相等 → try_to_merge_with_ksm_page
    //          → 释放多出来的物理页 → 设置写保护
    //    未找到: add to unstable tree（候选树）
    // ④ 扫描速率: /sys/kernel/mm/ksm/pages_to_scan (默认 100 页/次)
}
```

### 3.2 合并流程

```c
// mm/ksm.c:2251
static void cmp_and_merge_page(struct page *page, struct ksm_rmap_item *rmap_item)
{
    // ① 在 stable tree 中搜索匹配页
    tree_rmap_item = unstable_tree_search_insert(rmap_item, page, &tree_page);

    if (tree_rmap_item) {
        // ② 找到匹配 → 尝试合并
        ret = try_to_merge_with_ksm_page(rmap_item, page, tree_page);
        if (!ret) {
            // ★ 合并成功: 释放 tree_page，rmap_item 指向共享 KSM page
            // 原访问者触发 COW → do_wp_page 时复制
        }
    } else {
        // ③ 未找到匹配 → 挂入 unstable tree
        unstable_tree_insert(rmap_item, page);
    }
}
```

### 3.3 Write-after-Merge — COW 集成

KSM 合并后的页被标记为只读（写保护）。任何 guest 尝试写入这个共享页时，触发 `do_wp_page`——COW 分配新页，guest 获得自己的私有副本。KSM 从不阻止写入，只是通过 COW 延迟复制。

---

## 4. 三者关系图

```
                   THP                           Compaction                    KSM
                   ────                          ──────────                    ────
目标:              减少 TLB miss                 创建 2MB+ 连续块             合并相同内容页
手段:              直接分配 2MB 页               迁移碎片页                    内容 hash 匹配
入口:              do_huge_pmd_anonymous_page     compact_zone + kcompactd     ksm_do_scan
触发条件:          hugepage=always/madvise       大 order alloc 失败           扫描 MADV_MERGEABLE
利益:              ↓ 15% TLB miss                ↑ alloc 成功率                ↑ 内存利用率 (多VM)
代价:              alloc 2MB 可能失败             迁移页消耗 CPU                扫描消耗 CPU
协同:              THP 依赖 compaction           为 THP 提供连续块             与 THP 互斥
                   获取 2MB 块                   compaction                               (KSM 页不能是 THP)
```

---

## 5. 完整的物理页流转状态机

```
Buddy Allocator (free pages, per-order per-migratetype)
        │
        ├── alloc_pages(order)
        │     ├── → copy → do_anonymous_page → user page
        │     │       → COW → do_wp_page → 新页
        │     ├── → do_fault → page cache (file-backed)
        │     └── → do_huge_pmd_anonymous_page → THP (order=9, 2MB)
        │               → deferred_split → split → back to buddy
        │
        ├── free_page → back to buddy
        │
        ├── Compaction (kcompactd / direct):
        │       → migrate_pages → move pages around
        │       → create large contiguous blocks → THP alloc
        │
        ├── KSM:
        │       → scan → checksum match → merge → write-protect
        │       → COW on write → do_wp_page → new private page
        │
        └── Reclaim (kswapd / direct):
                → shrink_inactive_list → clean:free, dirty:writeback
                → swap out → swap slot → buddy
                → OOM: kill process → free all its pages
```

---

## 下一篇文章

→ [第6篇：Memory Cgroup — 分层计费、保护与 OOM](./06-memcg.md)
