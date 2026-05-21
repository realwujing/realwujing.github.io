# 第5篇：GPU 多内存类型管理 — TTM 框架

> 源码：`drivers/gpu/drm/ttm/ttm_bo.c` | 头文件：`include/drm/ttm/ttm_bo.h`  
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. 本篇位置

前四篇文章我们逐层解析了 HMM → GPUSVM → nouveau_svm → nouveau_dmem 的链条。`nouveau_dmem.c` 通过 `nouveau_bo_new_pin()` 在 VRAM 中分配内存——这个调用背后就是 **TTM (Translation Table Manager)** 框架。

TTM 是 Linux DRM 子系统的 GPU 内存管理器，解决一个根本问题：**GPU 有多块不同类型的物理内存（VRAM、GTT、System RAM），BO (Buffer Object) 应该在哪儿？何时迁移？**

```
┌─────────────────────────────────────────────────────┐
│                   TTM 框架 (ttm_bo.c)                │
│                                                      │
│  ttm_bo_init_reserved()    ← 创建 BO                │
│  ttm_bo_validate()         ← 核心：placement 决策    │
│  ttm_bo_pin()/unpin()     ← 固定/解除                │
│  ttm_bo_wait_ctx()         ← 等待 GPU 空闲           │
│  ttm_bo_move_to_lru_tail() ← LRU 管理               │
│  ttm_bo_evict()            ← 驱逐                    │
│  ttm_bo_swapout()          ← 换出到 swap             │
│                                                      │
│  内存域: TTM_PL_VRAM | TTM_PL_TT | TTM_PL_SYSTEM    │
└─────────────────────────────────────────────────────┘
```

## 2. GPU 的三种内存域

TTM 管理三种核心内存类型（Place，即 `ttm_place.mem_type`）：

```
┌──────────────────────────────────────────────────────┐
│  GPU 芯片                                             │
│  ┌──────────────┐       ┌──────────────────┐         │
│  │  计算单元     │       │  DRAM Controller  │         │
│  │  (SM/CU)     │       └────────┬─────────┘         │
│  └──────┬───────┘                │                   │
│         │               ┌────────▼─────────┐         │
│  ┌──────▼───────┐       │     VRAM          │         │
│  │  TLB / MMU   │       │  TTM_PL_VRAM      │         │
│  │  (GART/GTT)  │       │  8-80 GB HBM/GDDR │         │
│  └──────┬───────┘       └──────────────────┘         │
│         │                                            │
│  ┌──────▼───────┐       ┌──────────────────┐         │
│  │   PCIe BAR   │       │   系统内存        │         │
│  │  TTM_PL_TT   │       │   TTM_PL_SYSTEM   │         │
│  │  (GTT 窗口)  │  ──── │   256-1024 GB    │         │
│  └──────────────┘  PCIe └──────────────────┘         │
└──────────────────────────────────────────────────────┘

TTM_PL_VRAM:   板载显存 (HBM/GDDR) — 极快，受限容量
TTM_PL_TT:     通过 PCIe BAR 映射的系统内存 — GPU 可见
TTM_PL_SYSTEM: 纯系统内存 — CPU 可见，需 dma_map 才能 GPU 访问
```

**GTT (Graphics Translation Table)** 是 `TTM_PL_TT` 的命名来源——GPU 通过 PCIe BAR 窗口"偷看"系统内存，窗口大小受限于 BAR 大小（通常 256MB-512MB），通过 GTT 页表做地址翻译。

## 3. 核心数据结构

### 3.1 `struct ttm_buffer_object` — 缓冲区对象

```c
// include/drm/ttm/ttm_bo.h:101-138
struct ttm_buffer_object {
    struct drm_gem_object base;        // DRM GEM 基类

    struct ttm_device *bdev;           // TTM 设备
    enum ttm_bo_type type;             // device / kernel / sg
    uint32_t page_alignment;
    void (*destroy)(struct ttm_buffer_object *);

    struct kref kref;                  // 引用计数
    struct ttm_resource *resource;     // 当前 placement
    struct ttm_tt *ttm;                // TTM 结构 (系统内存页)
    bool deleted;                      // 僵尸标记
    struct ttm_lru_bulk_move *bulk_move;
    unsigned priority;                 // LRU 优先级 (低优先先驱逐)
    unsigned pin_count;                // Pin 计数 (>0 不可驱逐)

    struct work_struct delayed_delete;  // 延迟删除 work
    struct sg_table *sg;               // dmabuf sg 表
};
```

BO 通过 `resource` 知道自己当前在哪种内存中，通过 `ttm` 持有系统内存的后备页（用于 swap/迁移）。

### 3.2 `struct ttm_operation_ctx` — 操作上下文

```c
// include/drm/ttm/ttm_bo.h:173-199
struct ttm_operation_ctx {
    bool interruptible;       // 是否可中断睡眠
    bool no_wait_gpu;         // GPU 忙时立即返回
    bool gfp_retry_mayfail;   // 页面分配可失败
    bool allow_res_evict;     // 允许驱逐 reserved BO
    struct dma_resv *resv;    // 共享 reservation
    uint64_t bytes_moved;     // 统计：移动了多少字节
};
```

### 3.3 BO 类型

```c
// include/drm/ttm/ttm_bo.h:67-71
enum ttm_bo_type {
    ttm_bo_type_device,   // 普通设备 BO (可 mmap)
    ttm_bo_type_kernel,   // 内核专用 BO (不可 mmap)
    ttm_bo_type_sg         // dmabuf sg 表 BO
};
```

## 4. BO 创建 — `ttm_bo_init_reserved`

```c
// ttm_bo.c:929-984
int ttm_bo_init_reserved(struct ttm_device *bdev, struct ttm_buffer_object *bo,
    enum ttm_bo_type type, struct ttm_placement *placement,
    uint32_t alignment, struct ttm_operation_ctx *ctx,
    struct sg_table *sg, struct dma_resv *resv,
    void (*destroy)(struct ttm_buffer_object *))
```

创建流程：

```c
// 1. 初始化基本字段 (line 937-949)
kref_init(&bo->kref);
bo->bdev = bdev;
bo->type = type;
bo->page_alignment = alignment;
bo->destroy = destroy;
bo->pin_count = 0;
bo->sg = sg;
bo->bulk_move = NULL;
bo->base.resv = resv ?: &bo->base._resv; // 使用共享或私有 reservation

// 2. 全局 BO 计数 (line 949)
atomic_inc(&ttm_glob.bo_count);

// 3. 分配设备地址空间 (line 955-960)
if (bo->type == ttm_bo_type_device || bo->type == ttm_bo_type_sg) {
    ret = drm_vma_offset_add(bdev->vma_manager, &bo->base.vma_node,
                             PFN_UP(bo->base.size));
}

// 4. 立即验证 placement (line 970)
ret = ttm_bo_validate(bo, placement, ctx);
```

## 5. 核心函数：`ttm_bo_validate` — Placement 引擎

```c
// ttm_bo.c:818-894
int ttm_bo_validate(struct ttm_buffer_object *bo,
    struct ttm_placement *placement,
    struct ttm_operation_ctx *ctx)
```

这是 TTM 的**心脏**——决定 BO 的物理位置。调用者提供一个 `placement` 数组（按优先级排列），TTM 尝试将 BO 放到最佳位置。

### 5.1 placement 数据结构

`struct ttm_placement` 包含两个数组：
- `placement[]`：首选位置（按优先级排列）
- `busy_placement[]`：GPU 忙时的备选位置

每个 `struct ttm_place` 包含：
- `mem_type`：目标内存域（VRAM, TT, SYSTEM）
- `flags`：放置标志（DESIRED, FALLBACK, CONTIGUOUS, etc.）

### 5.2 验证流程

```c
force_space = false;
do {
    // 步骤 1: 检查当前 placement 是否已满足需求
    //         (line 838-841)
    if (bo->resource &&
        ttm_resource_compatible(bo->resource, placement, force_space))
        return 0;  // ← 已经在合适的位置，直接返回

    // 步骤 2: 禁止移动 pin 住的 BO (line 844)
    if (bo->pin_count)
        return -EINVAL;

    // 步骤 3: 分配资源 (line 856-857)
    ret = ttm_bo_alloc_resource(bo, placement, ctx, force_space, &res);

    // 第一次尝试：force_space=false (不驱逐)
    // 第二次尝试：force_space=true  (驱逐其他 BO)
    force_space = !force_space;

    if (ret == -ENOSPC)
        continue;  // 该 placement 放不下，尝试下一个

    // 步骤 4: 执行物理移动 (line 865)
    ret = ttm_bo_handle_move_mem(bo, res, false, ctx, &hop);
    // -EMULTIHOP → 通过中间域中转
    //   例如: VRAM 不够 → 先移到 SYSTEM → 等待 VRAM 释放 → 移回

} while (ret && force_space);
```

### 5.3 资源分配 — `ttm_bo_alloc_resource`

```c
// ttm_bo.c:710-772
static int ttm_bo_alloc_resource(struct ttm_buffer_object *bo,
    struct ttm_placement *placement,
    struct ttm_operation_ctx *ctx,
    bool force_space,
    struct ttm_resource **res)
```

遍历 placement 数组中的每个位置尝试分配：

```c
for (i = 0; i < placement->num_placement; ++i) {
    const struct ttm_place *place = &placement->placement[i];
    struct ttm_resource_manager *man;

    man = ttm_manager_type(bdev, place->mem_type);
    if (!man || !ttm_resource_manager_used(man))
        continue;

    // 跳过不适合当前 force_space 模式的 placement
    if (place->flags & (force_space ? TTM_PL_FLAG_DESIRED :
                        TTM_PL_FLAG_FALLBACK))
        continue;

    // 尝试直接分配
    may_evict = (force_space && place->mem_type != TTM_PL_SYSTEM);
    ret = ttm_resource_alloc(bo, place, res, ...);

    if (ret == -ENOSPC && may_evict) {
        // 需要驱逐其他 BO
        ret = ttm_bo_evict_alloc(bdev, man, place, bo, ctx,
                                 ticket, res, limit_pool);
    }
    // ...
}
```

### 5.4 物理移动 — `ttm_bo_handle_move_mem`

```c
// ttm_bo.c:121-173
static int ttm_bo_handle_move_mem(struct ttm_buffer_object *bo,
    struct ttm_resource *mem, bool evict,
    struct ttm_operation_ctx *ctx,
    struct ttm_place *hop)
```

```c
// 1. 如果需要，创建 system memory 后备 (TTM_PL_TT 用的)
if (new_use_tt) {
    ret = ttm_tt_create(bo, old_use_tt);
    if (mem->mem_type != TTM_PL_SYSTEM) {
        ret = ttm_bo_populate(bo, ctx);  // 填充系统内存页
    }
}

// 2. 预留 fence 槽 (line 154)
ret = dma_resv_reserve_fences(bo->base.resv, 1);

// 3. 调用设备驱动 move 函数 (line 158)
ret = bdev->funcs->move(bo, evict, ctx, mem, hop);
//   - bdev->funcs->move 通常指向 ttm_bo_move_memcpy 或驱动自定义
//   - -EMULTIHOP: 需要中间步骤

// 4. 统计 (line 165)
ctx->bytes_moved += bo->base.size;
```

## 6. 驱逐流程 — `ttm_bo_evict`

```c
// ttm_bo.c:358-410
static int ttm_bo_evict(struct ttm_buffer_object *bo,
                        struct ttm_operation_ctx *ctx)
```

当需要释放 VRAM 空间时：

```c
// 1. 获取驱逐目标位置 (line 371)
bo->bdev->funcs->evict_flags(bo, &placement);
// 通常目标 = TTM_PL_SYSTEM (把 BO 遣返到系统内存)

// 2. 如果无有效目标，等待 BO 空闲后 pipeline gutting (line 373-383)
if (!placement.num_placement) {
    ret = ttm_bo_wait_ctx(bo, ctx);
    return ttm_bo_pipeline_gutting(bo);  // 直接释放后备存储
}

// 3. 分配目标空间 (line 385)
ret = ttm_bo_mem_space(bo, &placement, &evict_mem, ctx);

// 4. 执行移动（支持 MULTIHOP）(line 396-401)
do {
    ret = ttm_bo_handle_move_mem(bo, evict_mem, true, ctx, &hop);
    if (ret != -EMULTIHOP) break;
    ret = ttm_bo_bounce_temp_buffer(bo, ctx, &hop);  // 中间跳
} while (!ret);
```

EMULTIHOP 机制：有些 GPU 架构不支持直接从 VRAM 到 SYSTEM 的拷贝，需要先经由 GTT 中转。

```
VRAM ──(hop)──→ GTT ──(hop)──→ SYSTEM
```

`ttm_bo_bounce_temp_buffer`（line 334-356）负责执行中间跳。

### 6.1 驱逐选择 — `ttm_bo_eviction_valuable`

```c
// ttm_bo.c:420-434
bool ttm_bo_eviction_valuable(struct ttm_buffer_object *bo,
                              const struct ttm_place *place)
{
    struct ttm_resource *res = bo->resource;

    // SYSTEM 中的 BO 总是可驱逐的
    if (res->mem_type == TTM_PL_SYSTEM)
        return true;

    // 只驱逐占用所需 placement 的 BO
    return ttm_resource_intersects(bo->bdev, res, place, bo->base.size);
}
```

驱逐原则：只驱逐那些占据"目标"内存域的 BO。比如需要 VRAM 空间，就只驱逐 VRAM 中的 BO。

### 6.2 LRU 驱逐遍历

```c
// ttm_bo.c:514-550 (回调函数)
static s64 ttm_bo_evict_cb(struct ttm_lru_walk *walk,
                           struct ttm_buffer_object *bo)
{
    // 跳过 pinned BO
    if (bo->pin_count || !bo->bdev->funcs->eviction_valuable(bo, place))
        return 0;

    // 处理 deleted BO（等待清理）
    if (bo->deleted) {
        lret = ttm_bo_wait_ctx(bo, walk->arg.ctx);
        if (!lret)
            ttm_bo_cleanup_memtype_use(bo);
    } else {
        lret = ttm_bo_evict(bo, walk->arg.ctx);
    }
}
```

TTM 使用 `ttm_lru_walk_for_evict` 遍历 LRU 链表，从最少使用的 BO 开始驱逐。支持 cgroup DMEM 策略（`dmem_cgroup_state_evict_valuable`）和 low watermark 控制。

## 7. Pin/Unpin — 固定 BO

```c
// ttm_bo.c:624-634
void ttm_bo_pin(struct ttm_buffer_object *bo)
{
    dma_resv_assert_held(bo->base.resv);
    spin_lock(&bo->bdev->lru_lock);
    if (bo->resource)
        ttm_resource_del_bulk_move(bo->resource, bo);  // 移出 bulk move
    if (!bo->pin_count++ && bo->resource)
        ttm_resource_move_to_lru_tail(bo->resource);   // 移到 LRU 尾部
    spin_unlock(&bo->bdev->lru_lock);
}
```

```c
// ttm_bo.c:643-656
void ttm_bo_unpin(struct ttm_buffer_object *bo)
{
    dma_resv_assert_held(bo->base.resv);
    spin_lock(&bo->bdev->lru_lock);
    if (!--bo->pin_count && bo->resource) {
        ttm_resource_add_bulk_move(bo->resource, bo);   // 恢复 bulk move
        ttm_resource_move_to_lru_tail(bo->resource);    // 移到 LRU 尾部
    }
    spin_unlock(&bo->bdev->lru_lock);
}
```

Pin 机制的核心：`pin_count > 0` 的 BO 不会被驱逐（`ttm_bo_evict_cb` line 524 检查 `bo->pin_count`）。Pin/Unpin 时将 BO 移到 LRU 尾部，降低被驱逐的优先级。

**nouveau_dmem.c 中正是使用 `nouveau_bo_new_pin` 来固定 VRAM 中的 DMEM chunk**。

## 8. LRU 管理与 `ttm_bo_move_to_lru_tail`

```c
// ttm_bo.c:80-86
void ttm_bo_move_to_lru_tail(struct ttm_buffer_object *bo)
{
    dma_resv_assert_held(bo->base.resv);
    if (bo->resource)
        ttm_resource_move_to_lru_tail(bo->resource);
}
```

TTM 为每种内存域维护独立的 LRU 链表。当 BO 被访问时，移到 LRU 尾部——这样 LRU 头部的 BO 就是"最久未使用"的，优先被驱逐。

TPS (Bulk Move) 优化（`ttm_bo_set_bulk_move`，line 103-118）：多个 BO 可以绑定到同一个 `ttm_lru_bulk_move`，一次性将整组 BO 移到 LRU 尾部，避免逐个操作的锁争用。

## 9. BO 生命周期与引用计数

### 9.1 释放 — `ttm_bo_put`

```c
// ttm_bo.c:323-326
void ttm_bo_put(struct ttm_buffer_object *bo)
{
    kref_put(&bo->kref, ttm_bo_release);
}
```

### 9.2 `ttm_bo_release` — 真正的释放

```c
// ttm_bo.c:249-320
static void ttm_bo_release(struct kref *kref)
```

关键路径：

```
1. 检查 pin_count (必须为 0)
2. ttm_bo_individualize_resv() — 如有共享 resv，复制 fence 到私有 resv
3. drm_vma_offset_remove() — 移除设备地址空间映射
4. ttm_mem_io_free() — 释放 I/O 映射
5. 判断 BO 是否空闲：
   ├─ 空闲 → ttm_bo_cleanup_memtype_use() — 立即清理 resource + tt
   │         dma_resv_unlock()
   │         bo->destroy(bo) — 调用驱动销毁函数
   └─ 不空闲 → 延迟删除：
              ttm_bo_flush_all_fences() — 加速 fence signal
              bo->deleted = true
              kref_init(&bo->kref) — 复活引用计数
              INIT_WORK(&bo->delayed_delete, ttm_bo_delayed_delete)
              queue_work_node(..., &bo->delayed_delete)
```

延迟删除的核心（`ttm_bo_delayed_delete`，line 235-247）：

```c
static void ttm_bo_delayed_delete(struct work_struct *work)
{
    bo = container_of(work, typeof(*bo), delayed_delete);

    // 等待所有 fence 完成
    dma_resv_wait_timeout(&bo->base._resv, DMA_RESV_USAGE_BOOKKEEP,
                          false, MAX_SCHEDULE_TIMEOUT);

    // 现在可以安全释放了
    dma_resv_lock(bo->base.resv, NULL);
    ttm_bo_cleanup_memtype_use(bo);
    dma_resv_unlock(bo->base.resv);
    ttm_bo_put(bo);
}
```

## 10. Idle 等待 — `ttm_bo_wait_ctx`

```c
// ttm_bo.c:1071-1091
int ttm_bo_wait_ctx(struct ttm_buffer_object *bo,
                    struct ttm_operation_ctx *ctx)
{
    if (ctx->no_wait_gpu) {
        // 不等待：检查 fence 是否已 signaled
        if (dma_resv_test_signaled(bo->base.resv, DMA_RESV_USAGE_BOOKKEEP))
            return 0;
        return -EBUSY;
    }

    // 等待最多 15 秒
    ret = dma_resv_wait_timeout(bo->base.resv, DMA_RESV_USAGE_BOOKKEEP,
                                ctx->interruptible, 15 * HZ);
}
```

等待 BO 的 GPU 操作完成。`DMA_RESV_USAGE_BOOKKEEP` 级别的 fence 包括所有 GPU 使用。（15 秒是硬编码的超时）

## 11. Swap-out — 内存压力下的换出

```c
// ttm_bo.c:1213-1230
s64 ttm_bo_swapout(struct ttm_device *bdev, struct ttm_operation_ctx *ctx,
                   struct ttm_resource_manager *man, gfp_t gfp_flags,
                   s64 target)
```

swapout 回调（`ttm_bo_swapout_cb`，line 1106-1195）：

```c
// 1. 跳过 pinned BO (line 1123)
if (bo->pin_count || !bdev->funcs->eviction_valuable(bo, &place))
    return -EBUSY;

// 2. 如果 BO 不在 SYSTEM 中，先移到 SYSTEM (line 1149-1166)
if (bo->resource->mem_type != TTM_PL_SYSTEM) {
    place.mem_type = TTM_PL_SYSTEM;
    ttm_resource_alloc(bo, &place, &evict_mem, NULL);
    ttm_bo_handle_move_mem(bo, evict_mem, true, ctx, &hop);
}

// 3. 等待 BO 空闲 (line 1171)
ttm_bo_wait_ctx(bo, ctx);

// 4. 取消 vmap (line 1175)
ttm_bo_unmap_virtual(bo);

// 5. 换出到 shmem (line 1179-1180)
ttm_tt_swapout(bdev, tt, swapout_walk->gfp_flags);
// 系统内存页 → 交换文件
```

Swap-out 将 BO 的后备页（`ttm->pages[]`）换出到 shmem 交换空间，释放系统内存。

## 12. 数据搬运工具 — `ttm_bo_util.c`

虽然本文聚焦 `ttm_bo.c`，但物理搬运的实际执行在 `ttm_bo_util.c` 中：

```c
// ttm_bo_util.c (声明在 ttm_bo.h:461-473)

// CPU memcpy 方式 (慢但可靠)
int ttm_bo_move_memcpy(struct ttm_buffer_object *bo,
                       struct ttm_operation_ctx *ctx,
                       struct ttm_resource *new_mem);

// 加速清理 (GPU blit 后 pipeline cleanup)
int ttm_bo_move_accel_cleanup(struct ttm_buffer_object *bo,
                              struct dma_fence *fence, bool evict,
                              bool pipeline,
                              struct ttm_resource *new_mem);

// 同步清理 (CPU 已等待 fence)
void ttm_bo_move_sync_cleanup(struct ttm_buffer_object *bo,
                              struct ttm_resource *new_mem);
```

驱动实现 `bdev->funcs->move` 时通常会选择：
- 如果 GPU blit 引擎可用 → `ttm_bo_move_accel_cleanup`（异步，在 fence signal 后清理旧资源）
- 否则 → `ttm_bo_move_memcpy`（CPU 搬运，同步）

## 13. 完整状态图

```
                  ttm_bo_init_reserved()
                         │
                         ▼
              ┌─────────────────────┐
              │    ttm_bo_validate  │
              │  (placement 决策)   │
              └─────────┬───────────┘
                        │
           ┌────────────┼────────────┐
           ▼            ▼            ▼
    ┌──────────┐ ┌──────────┐ ┌──────────┐
    │  VRAM    │ │   GTT    │ │  SYSTEM  │
    │ PL_VRAM  │ │  PL_TT   │ │PL_SYSTEM │
    └────┬─────┘ └────┬─────┘ └────┬─────┘
         │            │            │
         │   ┌────────┼────────┐   │
         ▼   ▼        ▼        ▼   ▼
    ┌──────────────────────────────────┐
    │        ttm_bo_handle_move_mem    │
    │   ├─ ttm_tt_create (if GTT)     │
    │   ├─ ttm_bo_populate            │
    │   ├─ bdev->funcs->move()        │
    │   └─ ctx->bytes_moved += size   │
    └──────────────────────────────────┘
                        │
         ┌──────────────┼──────────────┐
         ▼                             ▼
    ┌──────────────┐            ┌──────────────┐
    │ ttm_bo_evict │            │ttm_bo_swapout│
    │ (内存压力)    │            │ (内存压力)    │
    │ → SYSTEM     │            │ → shmem swap │
    └──────────────┘            └──────────────┘
         │                             │
         ▼                             ▼
    ┌──────────────────────────────────────┐
    │          ttm_bo_put()                │
    │   ├─ 空闲 → 立即释放 resource + tt   │
    │   └─ 忙   → delayed_delete work      │
    └──────────────────────────────────────┘
```

## 14. 与 nouveau_dmem 的关系

回顾《第4篇》，`nouveau_dmem_chunk_alloc` 调用 `nouveau_bo_new_pin` → 这实际上是用 TTM 在 `TTM_PL_VRAM` 中分配一个 pinned BO：

```
nouveau_dmem_chunk_alloc()
    → nouveau_bo_new_pin(NOUVEAU_GEM_DOMAIN_VRAM, DMEM_CHUNK_SIZE, &chunk->bo)
        → ttm_bo_init_reserved(...)
            → ttm_bo_validate(placement={TTM_PL_VRAM})
                → VRAM 资源分配
        → ttm_bo_pin(chunk->bo)     ← 固定，永不驱逐
```

这就是为什么 DMEM chunk 的 VRAM 是"稳定的"——TTM 的 pin 机制保证它不会被驱逐。

## 15. 总结

TTM 框架是 GPU 多内存类型管理的基石：

1. **Placement 决策**：`ttm_bo_validate()` 根据 placement 数组决定 BO 在哪个内存域
2. **驱逐机制**：`ttm_bo_evict()` 通过 LRU walk 释放空间，支持 MULTIHOP 中转
3. **Pin/Unpin**：`pin_count` 保护关键 BO 不被驱逐
4. **Bulk Move**：优化多 BO 同时移动的 LRU 开销
5. **延迟删除**：GPU 忙时暂缓释放，通过 fence 等待后清理
6. **Swap-out**：内存压力时换出后备页到 shmem

下一篇文章将深入 GPU 显存的具体分配算法——GPU Buddy 分配器。

## 下一篇文章

[第6篇：GPU 显存 Buddy 分配器](06-gpu-buddy.md)