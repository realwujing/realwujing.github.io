# 第3篇：NVIDIA HMM 调用者 — nouveau_svm.c

> 源码：`drivers/gpu/drm/nouveau/nouveau_svm.c` | 头文件：`drivers/gpu/drm/nouveau/nouveau_svm.h`  
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. 本篇位置

前两篇我们深入了 HMM（`mm/hmm.c`）和 DRM GPUSVM（`drm_gpusvm.c`）框架层。本篇聚焦 NVIDIA 的 HMM 调用者——**nouveau_svm.c**，看 NVIDIA 开源驱动如何将 GPU 页故障转化为 HMM 调用，并最终编程 GPU 页表。

```
GPU 硬件故障
    ↓
nouveau_svm_fault_buffer (ring buffer)
    ↓
nouveau_svm_fault() work handler     ← 本篇核心
    ├─ 解析故障 (addr, access, inst)
    ├─ 查找 SVMM (GPU 实例 → 进程)
    ├─ hmm_range_fault() / make_device_exclusive()
    └─ nvif_vmm_pfnmap() → 编程 GPU 页表
```

**注意**：nouveau 是 NVIDIA GPU 的开源驱动，与闭源的 nvidia.ko 不同。但 SVM 架构设计思想是相通的。

## 2. 核心数据结构

### 2.1 `struct nouveau_svm` — GPU 设备级 SVM

```c
// nouveau_svm.c:41-71
struct nouveau_svm {
    struct nouveau_drm *drm;
    struct mutex mutex;
    struct list_head inst;              // 活跃的 GPU 实例链表

    struct nouveau_svm_fault_buffer {
        int id;
        struct nvif_object object;       // GPU 寄存器映射对象
        u32 entries;                     // 故障 buffer 条目数
        u32 getaddr;                     // GPU GET 指针寄存器地址
        u32 putaddr;                     // GPU PUT 指针寄存器地址
        u32 get;                         // CPU 侧 GET 缓存
        u32 put;                         // CPU 侧 PUT 缓存
        struct nvif_event notify;        // GPU 中断 → schedule_work
        struct work_struct work;         // nouveau_svm_fault work

        struct nouveau_svm_fault {
            u64 inst;                    // GPU 实例 ID
            u64 addr;                    // 故障虚拟地址
            u64 time;                    // 故障时间戳
            u32 engine;                  // 故障引擎
            u8  gpc;                     // GPC (Graphics Processing Cluster)
            u8  hub;                     // Hub (MMU 类型)
            u8  access;                  // 访问类型
            u8  client;                  // 客户端 ID
            u8  fault;                   // 故障码
            struct nouveau_svmm *svmm;   // 关联的 SVM 实例
        } **fault;
        int fault_nr;
    } buffer[];                          // 柔性数组 (多故障 buffer)
};
```

`struct nouveau_svm` 是设备级别的 SVM 结构。核心是 `buffer[]` 柔性数组——GPU 的多条故障 buffer 环。GPU 通过寄存器 `getaddr`/`putaddr` 暴露 buffer 的读写指针，CPU 通过 `nvif_rd32` 读取。

### 2.2 `struct nouveau_svmm` — 进程级 SVM

```c
// nouveau_svm.h:9-18
struct nouveau_svmm {
    struct mmu_notifier notifier;       // CPU 侧 MMU notifier
    struct nouveau_vmm *vmm;            // GPU 侧 VMM (虚拟内存管理器)
    struct {
        unsigned long start;            // 非托管区域起始
        unsigned long limit;            // 非托管区域结束
    } unmanaged;
    struct mutex mutex;
};
```

每个开启 SVM 的 GPU 进程都有一个 `nouveau_svmm`。它连接了 CPU 侧（`mmu_notifier`）和 GPU 侧（`nouveau_vmm`）。

### 2.3 故障访问类型

```c
// nouveau_svm.c:73-76
#define FAULT_ACCESS_READ     0
#define FAULT_ACCESS_WRITE    1
#define FAULT_ACCESS_ATOMIC   2
#define FAULT_ACCESS_PREFETCH 3
```

四种故障类型由 `info` 寄存器的 `[19:16]` 位编码（`nouveau_svm.c:493`）。

### 2.4 `struct svm_notifier` — 局部 interval notifier

```c
// nouveau_svm.c:501-504
struct svm_notifier {
    struct mmu_interval_notifier notifier;
    struct nouveau_svmm *svmm;
};
```

这是 nouveau 自己的 `mmu_interval_notifier` 包装——用于单页故障处理中的 per-fault interval notifier，与 GPUSVM 的 `drm_gpusvm_notifier` 是独立的概念。

## 3. 初始化链路

### 3.1 设备级初始化 — `nouveau_svm_init`

```c
// nouveau_svm.c:1048-1087
void nouveau_svm_init(struct nouveau_drm *drm)
```

流程：
1. **平台检查**（line 1062）：仅 Pascal 及以下启用 SVM（Volta+ 暂时禁用，因 channel recovery 未修复）
2. **分配 `struct nouveau_svm`**（line 1065）：`kzalloc_flex(*drm->svm, buffer, 1)` — 一个故障 buffer
3. **查找故障 buffer 类**（line 1073）：`nvif_mclass()` 查找 GPU 支持的故障 buffer 类（Volta → Maxwell 降级尝试）
4. **构造故障 buffer**（line 1080）：`nouveau_svm_fault_buffer_ctor()`

### 3.2 故障 Buffer 构造 — `nouveau_svm_fault_buffer_ctor`

```c
// nouveau_svm.c:984-1018
static int
nouveau_svm_fault_buffer_ctor(struct nouveau_svm *svm, s32 oclass, int id)
```

流程：
1. **创建 GPU 对象**（line 995）：`nvif_object_ctor(device, "svmFaultBuffer", 0, oclass, ...)` — 在 GPU 上分配故障 buffer
2. **映射到 CPU**（line 1002）：`nvif_object_map()` — 将 GPU 寄存器映射到 CPU 可访地址
3. **读取 buffer 参数**（line 1003-1005）：entries, getaddr, putaddr
4. **注册 work**（line 1006）：`INIT_WORK(&buffer->work, nouveau_svm_fault)` — 故障处理器
5. **注册 GPU 中断**（line 1008）：`nvif_event_ctor(..., nouveau_svm_event, ...)` — GPU 写故障后触发中断 → `schedule_work()`
6. **分配 fault 数组**（line 1013）：`kvzalloc_objs(*buffer->fault, buffer->entries)`
7. **初始化**（line 1017）：同步 get/put 指针，启用中断

### 3.3 进程级初始化 — `nouveau_svmm_init`

```c
// nouveau_svm.c:316-377
int nouveau_svmm_init(struct drm_device *dev, void *data,
                      struct drm_file *file_priv)
```

这是用户空间 ioctl 路径（`NOUVEAU_SVM_INIT`）。流程：
1. **分配 `struct nouveau_svmm`**（line 329）
2. **设置非托管区域**（line 332-333）：某些地址范围不属于 SVM（由用户空间指定）
3. **创建可重放故障的 GPU VMM**（line 349-356）：
   - `nvif_vmm_ctor(..., "svmVmm", ..., MANAGED, ..., .fault_replay = true)` 
   - `MANAGED` 模式告诉 GPU：缺页不要崩溃，记录到故障 buffer
   - `fault_replay = true`：故障处理后 GPU 可以重放（重试）访问
4. **注册 CPU MMU Notifier**（line 358-361）：`__mmu_notifier_register(&svmm->notifier, current->mm)` — 监听 CPU 页表变化

## 4. GPU 故障处理 — 核心路径

### 4.1 故障中断触发

```
GPU MMU 缺页
    ↓
GPU 写入故障信息到 fault buffer (ring buffer)
    ├─ inst (GPU 实例 ID)
    ├─ addr (故障地址)
    ├─ access (读/写/原子/预取)
    ├─ hub/gpc/client/engine (故障源)
    └─ fault (故障类型码)
    ↓
GPU 更新 PUT 指针
    ↓
GPU 产生中断 → nvif_event 回调
    ↓
nouveau_svm_event() → schedule_work(&buffer->work)
    ↓
nouveau_svm_fault() work handler ← 核心处理
```

```c
// nouveau_svm.c:883-890
static int
nouveau_svm_event(struct nvif_event *event, void *argv, u32 argc)
{
    struct nouveau_svm_fault_buffer *buffer =
        container_of(event, typeof(*buffer), notify);
    schedule_work(&buffer->work);
    return NVIF_EVENT_KEEP;
}
```

### 4.2 故障读取 — `nouveau_svm_fault_cache`

```c
// nouveau_svm.c:452-499
static void
nouveau_svm_fault_cache(struct nouveau_svm *svm,
    struct nouveau_svm_fault_buffer *buffer, u32 offset)
```

从 GPU 故障 buffer 的每个 32 字节条目（偏移 0x00-0x1c）中读取故障详情：

```
偏移    内容
0x00    inst 低32位     ← nvif_rd32(memory, offset + 0x00)
0x04    inst 高32位
0x08    addr 低32位
0x0c    addr 高32位
0x10    time 低32位
0x14    time 高32位
0x18    engine
0x1c    info:
        [31]    valid (0x80000000)
        [27:24] gpc
        [20]    hub
        [19:16] access
        [14:8]  client
        [4:0]   fault type
```

读取后清除 valid 位（line 475），让 GPU 可以复用该条目。

### 4.3 故障排序 — `nouveau_svm_fault_cmp`

```c
// nouveau_svm.c:438-450
static int
nouveau_svm_fault_cmp(const void *a, const void *b)
{
    const struct nouveau_svm_fault *fa = *(struct nouveau_svm_fault **)a;
    const struct nouveau_svm_fault *fb = *(struct nouveau_svm_fault **)b;
    int ret;
    if ((ret = (s64)fa->inst - fb->inst)) return ret;  // 先按实例排序
    if ((ret = (s64)fa->addr - fb->addr)) return ret;  // 再按地址
    return nouveau_svm_fault_priority(fa->access) -
           nouveau_svm_fault_priority(fb->access);     // 最后按访问优先级
}
```

排序优先级（`nouveau_svm_fault_priority`，line 420-436）：

```
PREFETCH(0) < READ(1) < WRITE(2) < ATOMIC(3)
```

这样 WRITE 和 ATOMIC 故障排最前面，同一地址只需处理一次——WRITE 处理已经满足了 READ 需求。

### 4.4 故障处理主循环 — `nouveau_svm_fault`

```c
// nouveau_svm.c:716-881
static void
nouveau_svm_fault(struct work_struct *work)
```

这是整个 nouveau SVM 的**核心**：

```
Phase 1: 读取
    while (GET != PUT) {
        nouveau_svm_fault_cache()  ← 从 GPU buffer 读一个 32B 条目
        GET++
    }
    nvif_wr32(device, getaddr, GET)  ← 更新 GPU 侧 GET 指针

Phase 2: 排序
    sort(buffer->fault, buffer->fault_nr, ..., nouveau_svm_fault_cmp, NULL)

Phase 3: 实例查找
    mutex_lock(&svm->mutex)
    for each fault:
        ivmm = nouveau_ivmm_find(svm, fault->inst)  ← inst → svmm
        fault->svmm = ivmm ? ivmm->svmm : NULL       ← 非 SVM 通道 → NULL

Phase 4: 逐故障处理
    for each fault:
        ├─ svmm == NULL → nouveau_svm_fault_cancel()  ← 非 SVM 通道，取消故障
        ├─ ATOMIC → nouveau_atomic_range_fault()      ← 原子操作专用路径
        └─ 其他   → nouveau_range_fault()             ← 标准 HMM 路径

Phase 5: 故障重放
    if (replay > 0)
        nouveau_svm_fault_replay(svm)  ← 通知 GPU 重试之前故障的访问
```

### 4.5 故障重放与取消

```c
// nouveau_svm.c:379-388
static void
nouveau_svm_fault_replay(struct nouveau_svm *svm)
{
    WARN_ON(nvif_object_mthd(&svm->drm->client.vmm.vmm.object,
        GP100_VMM_VN_FAULT_REPLAY,
        &(struct gp100_vmm_fault_replay_vn) {}, ...));
}
```

重放：告诉 GPU "之前故障的地址现在有页表了，重试你的访问"。

```c
// nouveau_svm.c:395-408
static void
nouveau_svm_fault_cancel(struct nouveau_svm *svm,
    u64 inst, u8 hub, u8 gpc, u8 client)
{
    WARN_ON(nvif_object_mthd(&svm->drm->client.vmm.vmm.object,
        GP100_VMM_VN_FAULT_CANCEL, ...));
}
```

取消：无法处理的故障（非 SVM 通道、mm 已销毁等）→ 告诉 GPU "放弃，触发引擎重置，杀掉通道"（GPU SIGSEGV）。

### 4.6 标准 HMM 路径 — `nouveau_range_fault`

```c
// nouveau_svm.c:652-714
static int nouveau_range_fault(struct nouveau_svmm *svmm,
    struct nouveau_drm *drm,
    struct nouveau_pfnmap_args *args, u32 size,
    unsigned long hmm_flags,
    struct svm_notifier *notifier)
```

这是 nouveau 调用 HMM 的关键函数：

```
1. mmu_interval_notifier_insert() — 注册 per-fault interval notifier
   ↓ (line 671-673)
2. 循环 (timeout 保护):
   ├─ mmu_interval_read_begin()          ← 获取当前 seqno
   ├─ mmap_read_lock(mm)                 ← 持有 mmap 读锁
   ├─ hmm_range_fault(&range)            ← 核心 HMM 调用！
   ├─ mmap_read_unlock(mm)
   └─ mutex_lock(&svmm->mutex)
       ├─ mmu_interval_read_retry()?      ← seqno 变了？重试
       └─ 不变 → break
   ↓ (line 680-703)
3. nouveau_hmm_convert_pfn() — 将 HMM PFN 转为 nouveau 内部格式
   ↓ (line 705)
4. nvif_object_ioctl(..., NVIF_VMM_V0_PFNMAP, ...) — 编程 GPU 页表
   ↓ (line 707)
5. mmu_interval_notifier_remove() — 清理
```

**`nouveau_hmm_convert_pfn`** 是 PFN 格式转换的关键（line 537-583）：

```c
// nouveau_svm.c:537-583
static void nouveau_hmm_convert_pfn(struct nouveau_drm *drm,
    struct hmm_range *range, struct nouveau_pfnmap_args *args)
{
    if (!(range->hmm_pfns[0] & HMM_PFN_VALID)) {
        args->p.phys[0] = 0;       // 无效 → 零
        return;
    }

    page = hmm_pfn_to_page(range->hmm_pfns[0]);

    // 大页支持：如果 CPU 也是大页映射，GPU 也用大页
    if (hmm_pfn_to_map_order(range->hmm_pfns[0])) {
        args->p.page = hmm_pfn_to_map_order(range->hmm_pfns[0]) + PAGE_SHIFT;
        args->p.size = 1UL << args->p.page;
        // 重新对齐地址到页起始
    }

    if (is_device_private_page(page))
        args->p.phys[0] = nouveau_dmem_page_addr(page) |
            NVIF_VMM_PFNMAP_V0_V | NVIF_VMM_PFNMAP_V0_VRAM;
    else
        args->p.phys[0] = page_to_phys(page) |
            NVIF_VMM_PFNMAP_V0_V | NVIF_VMM_PFNMAP_V0_HOST;

    if (range->hmm_pfns[0] & HMM_PFN_WRITE)
        args->p.phys[0] |= NVIF_VMM_PFNMAP_V0_W;
}
```

关键标志位：
- `NVIF_VMM_PFNMAP_V0_V`：页表项有效
- `NVIF_VMM_PFNMAP_V0_VRAM`：指向 VRAM（设备物理地址→`nouveau_dmem_page_addr`）
- `NVIF_VMM_PFNMAP_V0_HOST`：指向系统内存（物理地址→`page_to_phys`）
- `NVIF_VMM_PFNMAP_V0_W`：可写
- `NVIF_VMM_PFNMAP_V0_A`：原子操作支持

### 4.7 原子操作路径 — `nouveau_atomic_range_fault`

```c
// nouveau_svm.c:585-650
static int nouveau_atomic_range_fault(struct nouveau_svmm *svmm,
    struct nouveau_drm *drm,
    struct nouveau_pfnmap_args *args, u32 size,
    struct svm_notifier *notifier)
```

原子操作（ATOMIC）不能通过 HMM 的标准路径处理，因为需要 **exclusive 访问权**：

```
1. mmu_interval_notifier_insert()       ← 注册 interval notifier
2. make_device_exclusive(mm, start, drm->dev, &folio)  ← 核心！
   ↓ 让 CPU 侧该页 exclusive 给设备
3. mutex_lock(&svmm->mutex)
   ├─ mmu_interval_read_retry()?
   └─ 不变 → break
4. nvif_object_ioctl(..., NVIF_VMM_PFNMAP_V0_V | V0_W | V0_A | V0_HOST, ...)
   ↓ 编程 GPU 页表，标记原子操作支持
5. folio_unlock(folio); folio_put(folio)
6. mmu_interval_notifier_remove()
```

注意：原子操作只能映射到 HOST 内存（系统内存），不可能是 VRAM（`NVIF_VMM_PFNMAP_V0_HOST`）。

### 4.8 故障分组优化

同一 SVMM 的连续故障可以批处理（line 778-864）：

```c
for (fi = 0; fn = fi + 1, fi < buffer->fault_nr; fi = fn) {
    // 处理第一个故障 (fi)
    start = buffer->fault[fi]->addr;
    limit = start + PAGE_SIZE;
    // ...
    ret = nouveau_range_fault(svmm, drm, args, ..., hmm_flags, &notifier);

    // 批处理后续故障，只要：
    //   - 同一 SVMM
    //   - 地址落在大页范围内
    //   - 访问权限已被当前映射满足
    limit = args->p.addr + args->p.size;
    for (fn = fi; ++fn < buffer->fault_nr; ) {
        if (buffer->fault[fn]->svmm != svmm ||
            buffer->fault[fn]->addr >= limit ||
            (fi->access == READ && !V) ||   // 没映射 → 不能跳过
            (fi->access != READ && fi->access != PREFETCH && !W) || // 不可写
            ...)
            break;
    }
}
```

## 5. CPU 侧 MMU Notifier — 页表失效

```c
// nouveau_svm.c:251-290
static int
nouveau_svmm_invalidate_range_start(struct mmu_notifier *mn,
    const struct mmu_notifier_range *update)
{
    struct nouveau_svmm *svmm =
        container_of(mn, struct nouveau_svmm, notifier);

    // 跳过 MMU_NOTIFY_MIGRATE（迁移过程自己处理失效）
    if (update->event == MMU_NOTIFY_MIGRATE &&
        update->owner == svmm->vmm->cli->drm->dev)
        goto out;

    // 跳过非托管区域
    if (limit > svmm->unmanaged.start && start < svmm->unmanaged.limit) {
        if (start < svmm->unmanaged.start)
            nouveau_svmm_invalidate(svmm, start, svmm->unmanaged.limit);
        start = svmm->unmanaged.limit;
    }

    // 失效 GPU 页表
    nouveau_svmm_invalidate(svmm, start, limit);
}
```

当 CPU 侧页表变化（`munmap`, `mprotect`, `madvise` 等）时，通过 `mmu_notifier` 回调失效对应的 GPU 页表项。关键是跳过 **self-migration**——如果 GPU 自己正在迁移页，不需要再失效自己刚刚修改的页表。

`nouveau_svmm_invalidate`（line 239-249）通过 `NVIF_VMM_V0_PFNCLR` 方法清除 GPU 页表项。

## 6. Blockable interval notifier

```c
// nouveau_svm.c:506-531
static bool nouveau_svm_range_invalidate(struct mmu_interval_notifier *mni,
    const struct mmu_notifier_range *range,
    unsigned long cur_seq)
{
    // 跳过自己的 exclusive 操作
    if (range->event == MMU_NOTIFY_EXCLUSIVE &&
        range->owner == sn->svmm->vmm->cli->drm->dev)
        return true;

    // 持 mutex 串行化 seq 更新，防止 PTE 失效在 HW 编程中发生
    if (mmu_notifier_range_blockable(range))
        mutex_lock(&sn->svmm->mutex);
    else if (!mutex_trylock(&sn->svmm->mutex))
        return false;
    mmu_interval_set_seq(mni, cur_seq);
    mutex_unlock(&sn->svmm->mutex);
    return true;
}
```

这是 per-fault 处理的 interval notifier——与设备级 `mmu_notifier` 配合，在 HMM range 操作期间保护页表一致性。

## 7. 完整流程图

```
应用: cudaMallocManaged(p, size)
    ↓
GPU 访问 p[0] → MMU 缺页
    ↓
GPU 写入 fault buffer: { inst, addr=0x7f..., access=WRITE }
GPU 更新 PUT 指针
GPU 触发中断
    ↓
nvif_event → schedule_work(&buffer->work)
    ↓
nouveau_svm_fault():
    ├─ 读取所有故障条目
    ├─ 排序 (inst → addr → access priority)
    ├─ inst → svmm 映射
    ├─ 对于每个故障:
    │   ├─ svmm 不存在 → cancel (GPU SIGSEGV)
    │   ├─ ATOMIC → nouveau_atomic_range_fault()
    │   │   └─ make_device_exclusive() → nvif_vmm_pfnmap()
    │   └─ READ/WRITE/PREFETCH → nouveau_range_fault()
    │       ├─ mmu_interval_notifier_insert()
    │       ├─ loop:
    │       │   ├─ mmu_interval_read_begin()
    │       │   ├─ mmap_read_lock(mm)
    │       │   ├─ hmm_range_fault()            ← HMM 调用！
    │       │   ├─ mmap_read_unlock(mm)
    │       │   └─ mmu_interval_read_retry()? ── YES → 重试
    │       ├─ nouveau_hmm_convert_pfn()          ← PFN → GPU 地址
    │       │   ├─ device_private_page? → nouveau_dmem_page_addr()
    │       │   └─ normal page?          → page_to_phys()
    │       ├─ nvif_vmm_pfnmap()                  ← 编程 GPU 页表
    │       └─ mmu_interval_notifier_remove()
    └─ nouveau_svm_fault_replay() → GPU 重试访问
```

## 8. 与 DRM GPUSVM 的关系

本篇 `nouveau_svm.c` 使用的是一个**较老**的自有实现（Copyright 2018），不依赖 `drm_gpusvm.c`（后者是 2024 年的新框架）。但它们的设计思想高度一致：

| 概念 | nouveau_svm.c (2018) | drm_gpusvm.c (2024) |
|------|---------------------|---------------------|
| Notifier | `mmu_notifier` + `svm_notifier` | `drm_gpusvm_notifier` (interval tree) |
| Range | 隐式（单页 fault） | `drm_gpusvm_range` (多页 chunk) |
| HMM 调用 | `hmm_range_fault()` 1 页 | `hmm_range_fault()` N 页 |
| DMA 映射 | GPU 内部处理 | `drm_gpusvm_pages` |
| 锁 | `svmm->mutex` | `gpusvm->notifier_lock` (rw_sem) |

nouveau 的实现更紧密地耦合到 NVIDIA 硬件（`nvif_vmm_pfnmap` / `nvif_object_ioctl`），而 GPUSVM 提供了更通用的框架层。

## 下一篇文章

[第4篇：NVIDIA 设备显存管理：nouveau_dmem.c](04-nouveau-dmem.md)