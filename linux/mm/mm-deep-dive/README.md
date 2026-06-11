# Linux 内核内存管理 源码深度解析

📌 **源码**：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)

本系列对 Linux 内核内存管理子系统进行源码级深度解析，以真实行号定位关键数据结构与函数，配合 ASCII 流程图和数据结构关系图，逐层剖析从 page allocator 到 page fault 的完整链路。

---

## 系列文章

| 序号 | 标题 | 核心主题 |
|------|------|---------|
| 01 | [Buddy 页分配器 — 伙伴系统、水位线与迁移类型](./01-buddy.md) | free_area, migratetype, watermark, alloc_pages 快慢路径, fallback |
| 02 | [SLUB 分配器 — sheaf 缓存、slab freelist 与 kmalloc](./02-slub.md) | kmem_cache, slub_percpu_sheaves, slab_sheaf, kmalloc 快速路径, barn |
| 03 | [页回收与 OOM Killer — kswapd、直接回收与 out_of_memory](./03-reclaim-oom.md) | LRU 链表, shrink_node, kswapd, direct reclaim, OOM badness 计算 |
| 04 | [缺页异常与 mmap — page fault 处理、VMA 与 file-backed](./04-page-fault.md) | do_page_fault, handle_mm_fault, VMA lookup, filemap, swap |

---

## 源码版本

```
commit eb3f4b7426cf
Merge tag 'modules-7.1-rc5' and others into torvalds/master
Linux v7.1-rc5-26
```

---

## 阅读路线图

```
用户空间 malloc/brk/mmap
       │
       ▼
 ┌─────────────┐      ┌──────────────┐
 │  缺页异常     │◄─────│  VMA lookup   │
 │ do_page_fault│      │ find_vma     │
 └──────┬───────┘      └──────────────┘
        │
        ▼
 ┌─────────────┐      ┌──────────────┐
 │  SLUB/slab   │◄─────│  kmalloc /   │
 │  分配器       │      │  kmem_cache  │
 └──────┬───────┘      └──────────────┘
        │
        ▼
 ┌─────────────┐      ┌──────────────┐
 │  Buddy 页    │◄─────│  alloc_pages │
 │  分配器       │      │  伙伴系统     │
 └──────┬───────┘      └──────────────┘
        │
        ├── 分配成功 → 返回 page
        │
        ▼ (分配失败)
 ┌─────────────┐      ┌──────────────┐
 │  页回收       │◄─────│  kswapd      │
 │  shrink_node │      │  direct reclaim│
 └──────┬───────┘      └──────────────┘
        │
        ▼ (回收无果)
 ┌─────────────┐
 │  OOM Killer  │
 │ out_of_memory│
 └─────────────┘
```

---

## 设计理念

本系列文档遵循以下原则：

1. **源码先行**：所有数据结构、函数均标注精确行号，可以直接跳转阅读源码
2. **ASCII 图解**：用 ASCII 流程图展示调用路径、数据结构关系，避免图片加载问题
3. **逐层剖析**：从数据结构 → 快速路径 → 慢速路径 → 异常处理，层层递进
4. **内核版本绑定**：所有行号对应一个具体 commit，避免版本漂移

---

## license

This documentation is licensed under CC BY-SA 4.0.  
All kernel source code excerpts belong to their respective copyright holders.
