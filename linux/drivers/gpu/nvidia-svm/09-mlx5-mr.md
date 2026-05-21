# 第9篇：RDMA 硬件侧 MR 注册：mlx5/mr.c

> 源码：`drivers/infiniband/hw/mlx5/mr.c` | 头文件：`drivers/infiniband/hw/mlx5/mlx5_ib.h`
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](./README.md)

## 1. MKey 是什么

MKey（Memory Key）是 InfiniBand/RoCE 协议中最核心的内存管理概念。每个远程 DMA 操作（RDMA Read/Write/Atomic）都需要携带一个有效的 MKey，HCA（Host Channel Adapter，如 ConnectX-7）用 MKey 来：

1. **查 IOMMU 页表**：将虚拟地址翻译为物理总线地址
2. **权限检查**：remote read/write/atomic 是否允许
3. **地址空间隔离**：进程 A 的 MKey 无法访问进程 B 的内存

MKey 的创建需要**固件命令**（CREATE_MKEY），这在 RDMA 中是开销最大的操作之一。mlx5/mr.c（2197 行）是整个 ConnectX 系列内存注册的枢纽。

## 2. 架构总览

MR 注册的三条路径：

```
                    mlx5_ib_reg_user_mr (854)
                            │
                    ┌───────┼──────────┐
                    │       │          │
            ODP 流程  UMR 快速路径   reg_create 慢路径
            (874)    (755)           (532)
```

```
寄存器路径决策:
  ┌─────────────────────────────────────────────────────┐
  │ access_flags & IB_ACCESS_ON_DEMAND?                  │
  │   YES → create_user_odp_mr()  [ODP 按需分页]        │
  │   NO  → 继续                                        │
  │                                                      │
  │ 用户内存: ib_umem_get() 获取 SG table                │
  │   → create_real_mr()                                 │
  │                                                      │
  │ dmabuf 内存: reg_user_mr_dmabuf()                    │
  │   → alloc_cacheable_mr() [UMR 快速路径]              │
  └─────────────────────────────────────────────────────┘
```

## 3. 核心函数

### 3.1 mlx5_ib_reg_user_mr — 公共入口

`drivers/infiniband/hw/mlx5/mr.c:854-880`

```c
struct ib_mr *mlx5_ib_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
                                  u64 iova, int access_flags,
                                  struct ib_dmah *dmah,
                                  struct ib_udata *udata)
{
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    struct ib_umem *umem;
    int err;

    if (!IS_ENABLED(CONFIG_INFINIBAND_USER_MEM) ||
        ((access_flags & IB_ACCESS_ON_DEMAND) && dmah))
        return ERR_PTR(-EOPNOTSUPP);

    mlx5_ib_dbg(dev,
        "start 0x%llx, iova 0x%llx, length 0x%llx, access_flags 0x%x\n",
        start, iova, length, access_flags);

    err = mlx5r_umr_resource_init(dev);  // 初始化 UMR 资源
    if (err)
        return ERR_PTR(err);

    if (access_flags & IB_ACCESS_ON_DEMAND)
        return create_user_odp_mr(pd, start, length, iova, access_flags, udata);

    umem = ib_umem_get(&dev->ib_dev, start, length, access_flags);
    if (IS_ERR(umem))
        return ERR_CAST(umem);

    return create_real_mr(pd, umem, iova, access_flags, dmah);
}
```

**三路分发**：
- ODP（On-Demand Paging）：用户启用了 `IB_ACCESS_ON_DEMAND`
- 非 ODP 用户内存：`ib_umem_get` → `create_real_mr`
- dmabuf 内存：走 `mlx5_ib_reg_user_mr_dmabuf`

### 3.2 create_real_mr — UMR vs 慢路径决策

`drivers/infiniband/hw/mlx5/mr.c:735-769`

```c
static struct ib_mr *create_real_mr(struct ib_pd *pd, struct ib_umem *umem,
                                    u64 iova, int access_flags,
                                    struct ib_dmah *dmah)
{
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    struct mlx5_ib_mr *mr = NULL;
    bool xlt_with_umr;
    u16 st_index = MLX5_MKC_PCIE_TPH_NO_STEERING_TAG_INDEX;
    u8 ph = MLX5_IB_NO_PH;
    int err;

    xlt_with_umr = mlx5r_umr_can_load_pas(dev, umem->length);
    if (xlt_with_umr) {
        mr = alloc_cacheable_mr(pd, umem, iova, access_flags,
                                MLX5_MKC_ACCESS_MODE_MTT,
                                st_index, ph);
    } else {
        unsigned long page_size = mlx5_umem_mkc_find_best_pgsz(
            dev, umem, iova, MLX5_MKC_ACCESS_MODE_MTT);

        mutex_lock(&dev->slow_path_mutex);
        mr = reg_create(pd, umem, iova, access_flags, page_size,
                        true, MLX5_MKC_ACCESS_MODE_MTT,
                        st_index, ph);
        mutex_unlock(&dev->slow_path_mutex);
    }
```

**关键决策**（行 754）：`mlx5r_umr_can_load_pas` 判断 PAS（Physical Address Segment）是否能在单个 UMR WQE 中装下。

```
┌─────────────────────────────────────────────────────────┐
│  UMR (User Mode Registration) 快速路径:                  │
│  条件: PAS entries 数量 ≤ UMR WQE 容量                  │
│  流程: alloc_cacheable_mr → 从 FRMR pool 获取预分配 MKey │
│        → mlx5r_umr_update_mr_pas 修改 MKey 的 PAS       │
│  优点: 无需固件命令，只写 WQE，延迟 ~1μs                │
│                                                         │
│  reg_create 慢路径:                                     │
│  条件: PAS 数量 > UMR WQE 容量，或超大内存              │
│  流程: 构建 CREATE_MKEY 固件命令 → kvzalloc 命令缓冲区   │
│        → 填充 PAS/MKC 字段 → 发送固件命令               │
│  缺点: 固件 RPC 延迟 ~50-100μs                          │
└─────────────────────────────────────────────────────────┘
```

### 3.3 reg_create — CREATE_MKEY 固件命令

`drivers/infiniband/hw/mlx5/mr.c:532-601`

```c
static struct mlx5_ib_mr *reg_create(struct ib_pd *pd, struct ib_umem *umem,
                                     u64 iova, int access_flags,
                                     unsigned long page_size, bool populate,
                                     int access_mode, u16 st_index, u8 ph)
{
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    struct mlx5_ib_mr *mr;
    __be64 *pas;
    void *mkc;
    int inlen;
    u32 *in;
    int err;
    bool pg_cap = !!(MLX5_CAP_GEN(dev->mdev, pg)) &&
        (access_mode == MLX5_MKC_ACCESS_MODE_MTT) &&
        (ph == MLX5_IB_NO_PH);
    bool ksm_mode = (access_mode == MLX5_MKC_ACCESS_MODE_KSM);

    if (!page_size)
        return ERR_PTR(-EINVAL);
    mr = kzalloc_obj(*mr);
    if (!mr)
        return ERR_PTR(-ENOMEM);

    mr->ibmr.pd = pd;
    mr->access_flags = access_flags;
    mr->page_shift = order_base_2(page_size);

    inlen = MLX5_ST_SZ_BYTES(create_mkey_in);
    if (populate)
        inlen += sizeof(*pas) *
                 roundup(ib_umem_num_dma_blocks(umem, page_size), 2);
    in = kvzalloc(inlen, GFP_KERNEL);
    // ...
    pas = (__be64 *)MLX5_ADDR_OF(create_mkey_in, in, klm_pas_mtt);
    if (populate) {
        if (WARN_ON(access_flags & IB_ACCESS_ON_DEMAND || ksm_mode)) {
            err = -EINVAL;
            goto err_2;
        }
        mlx5_ib_populate_pas(umem, 1UL << mr->page_shift, pas,
                             pg_cap ? MLX5_IB_MTT_PRESENT : 0);
    }
```

**命令布局**：

```
┌─────────────────────────────────────────────────────────┐
│              CREATE_MKEY firmware command                │
│                                                         │
│  ┌─────────────────────────────────────────────────────┐│
│  │  MKC (Memory Key Context)                           ││
│  │  - access_mode: MTT/KSM/SW_ICM/INTERLEAVED         ││
│  │  - iova: 起始虚拟地址                               ││
│  │  - len: 内存区域长度                                ││
│  │  - log_page_size: 页大小 (log2)                     ││
│  │  - bsf_octword_size: 0 (不启用 bank separated format)││
│  │  - translations_octword_size: PAS 数组大小          ││
│  │  - access_flags: local/remote read/write/atomic     ││
│  │  - pd: protection domain                           ││
│  │  - free: 0 (MTT 模式不掉就释放)                     ││
│  │  - umr_en: 1 (允许后续 UMR 重编程)                  ││
│  ├─────────────────────────────────────────────────────┤│
│  │  PAS (Physical Address Segments)                    ││
│  │  ┌─────────────────────┬───────────────────────────┐││
│  │  │ PAS[0] = BUS_ADDR_0 │ PAS[1] = BUS_ADDR_1 │ ... │││
│  │  └─────────────────────┴───────────────────────────┘││
│  │  每个 PAS 是一个 64-bit PCIe 总线地址                 ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```

**MKC 字段设置**（行 583-604）：

```c
    mkc = MLX5_ADDR_OF(create_mkey_in, in, memory_key_mkey_entry);
    set_mkc_access_pd_addr_fields(mkc, access_flags, iova, ...);
    MLX5_SET(mkc, mkc, free, !populate);
    MLX5_SET(mkc, mkc, access_mode_1_0, access_mode);
    MLX5_SET(mkc, mkc, umr_en, 1);          // 允许后续 UMR 更新
    MLX5_SET64(mkc, mkc, len, umem->length);
    MLX5_SET(mkc, mkc, bsf_octword_size, 0);
    MLX5_SET(mkc, mkc, log_page_size, mr->page_shift);
```

### 3.4 alloc_cacheable_mr — FRMR 池分配（UMR 快速路径）

`drivers/infiniband/hw/mlx5/mr.c:440-469`

```c
static struct mlx5_ib_mr *alloc_cacheable_mr(struct ib_pd *pd,
                                             struct ib_umem *umem, u64 iova,
                                             int access_flags, int access_mode,
                                             u16 st_index, u8 ph)
{
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    struct mlx5_ib_mr *mr;
    unsigned long page_size;

    if (umem->is_dmabuf)
        page_size = mlx5_umem_dmabuf_default_pgsz(umem, iova);
    else
        page_size = mlx5_umem_mkc_find_best_pgsz(dev, umem, iova,
                                                 access_mode);
    if (WARN_ON(!page_size))
        return ERR_PTR(-EINVAL);

    mr = _mlx5_frmr_pool_alloc(dev, umem, access_flags, access_mode,
                               page_size, st_index, ph);
    if (IS_ERR(mr))
        return mr;

    mr->mmkey.type = MLX5_MKEY_MR;
    mr->ibmr.pd = pd;
    mr->umem = umem;
    mr->page_shift = order_base_2(page_size);
    set_mr_fields(dev, mr, umem->length, access_flags, iova);

    return mr;
}
```

FRMR（Fast Register Memory Region）pool 预分配了一组 MKey，可以从池中获取而**不需要固件命令**。获取后通过 `mlx5r_umr_update_mr_pas` 用 UMR WQE 设置 PAS。

### 3.5 mlx5_ib_populate_pas — 构建 PAS 数组

`drivers/infiniband/hw/mlx5/mem.c:41-51`

```c
void mlx5_ib_populate_pas(struct ib_umem *umem, size_t page_size, __be64 *pas,
                          u64 access_flags)
{
    struct ib_block_iter biter;

    rdma_umem_for_each_dma_block (umem, &biter, page_size) {
        *pas = cpu_to_be64(rdma_block_iter_dma_address(&biter) |
                           access_flags);
        pas++;
    }
}
```

**核心循环**：遍历 `ib_umem` 的 DMA 块（由 `dma_buf_map_attachment` 返回的 SG table），将每个 DMA 块的物理地址以**大端序**（`cpu_to_be64`）写入 PAS 数组。

```
PAS 数组填充示意:
  umem SG table:              生成的 PAS 数组:
  ┌──────────────┐            ┌────────────────────────┐
  │ sg[0]: 0x1A000│           │ PAS[0]: 0x00000001A000 │
  │ len: 4096    │    ──→     │ (大端, 可能带 access)  │
  ├──────────────┤            ├────────────────────────┤
  │ sg[1]: 0x2B000│           │ PAS[1]: 0x00000002B000 │
  │ len: 4096    │            ├────────────────────────┤
  ├──────────────┤            │ PAS[2]: 0x00000003C000 │
  │ sg[2]: 0x3C000│            └────────────────────────┘
  │ len: 4096    │
  └──────────────┘
```

## 4. GPUDirect RDMA 路径：dmabuf MR 注册

### 4.1 mlx5_ib_reg_user_mr_dmabuf — 公共入口

`drivers/infiniband/hw/mlx5/mr.c:1030-1066`

```c
struct ib_mr *mlx5_ib_reg_user_mr_dmabuf(struct ib_pd *pd, u64 offset,
                                         u64 length, u64 virt_addr,
                                         int fd, int access_flags,
                                         struct ib_dmah *dmah,
                                         struct uverbs_attr_bundle *attrs)
{
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    int mlx5_access_flags = 0;
    int err;

    if (!IS_ENABLED(CONFIG_INFINIBAND_USER_MEM) ||
        !IS_ENABLED(CONFIG_INFINIBAND_ON_DEMAND_PAGING))
        return ERR_PTR(-EOPNOTSUPP);

    // ...

    /* dmabuf requires xlt update via umr to work. */
    if (!mlx5r_umr_can_load_pas(dev, length))
        return ERR_PTR(-EINVAL);

    if (mlx5_access_flags & MLX5_IB_UAPI_REG_DMABUF_ACCESS_DATA_DIRECT)
        return reg_user_mr_dmabuf_by_data_direct(pd, offset, length,
                                                 virt_addr, fd, access_flags);

    return reg_user_mr_dmabuf(pd, NULL, offset, length, virt_addr, fd,
                              access_flags, MLX5_MKC_ACCESS_MODE_MTT, dmah);
}
```

**约束**（行 1057）：dmabuf MR 注册**必须能用 UMR**——这是硬性要求。如果内存区域太大，无法在 UMR WQE 中容纳全部 PAS，直接返回 `-EINVAL`。

### 4.2 reg_user_mr_dmabuf — 核心转换

`drivers/infiniband/hw/mlx5/mr.c:902-974`

```c
static struct ib_mr *
reg_user_mr_dmabuf(struct ib_pd *pd, struct device *dma_device,
                   u64 offset, u64 length, u64 virt_addr,
                   int fd, int access_flags, int access_mode,
                   struct ib_dmah *dmah)
{
    bool pinned_mode = (access_mode == MLX5_MKC_ACCESS_MODE_KSM);
    struct mlx5_ib_dev *dev = to_mdev(pd->device);
    struct mlx5_ib_mr *mr = NULL;
    struct ib_umem_dmabuf *umem_dmabuf;
    // ...
    int err;

    err = mlx5r_umr_resource_init(dev);
    if (err)
        return ERR_PTR(err);

    if (!pinned_mode)
        umem_dmabuf = ib_umem_dmabuf_get(&dev->ib_dev,
                                         offset, length, fd, access_flags,
                                         &mlx5_ib_dmabuf_attach_ops);
    else if (dma_device)
        umem_dmabuf = ib_umem_dmabuf_get_pinned_with_dma_device(
            &dev->ib_dev, dma_device, offset, length, fd, access_flags);
    else
        umem_dmabuf = ib_umem_dmabuf_get_pinned(
            &dev->ib_dev, offset, length, fd, access_flags);
    // ...

    mr = alloc_cacheable_mr(pd, &umem_dmabuf->umem, virt_addr,
                            access_flags, access_mode, st_index, ph);
    // ...

    umem_dmabuf->private = mr;
    if (!pinned_mode) {
        err = mlx5r_store_odp_mkey(dev, &mr->mmkey);
        // ...
    } else {
        mr->data_direct = true;          // GPUDirect RDMA!
    }

    err = mlx5_ib_init_dmabuf_mr(mr);  // 初始化 dmabuf MR
    // ...
    return &mr->ibmr;
}
```

**流程图**：

```
reg_user_mr_dmabuf()
  │
  ├─ 【步骤 1】mlx5r_umr_resource_init(dev)
  │   初始化 UMR 资源（WQE buffer、固件上下文）
  │
  ├─ 【步骤 2】ib_umem_dmabuf_get_pinned(fd)
  │   → dma_buf_get(fd)
  │   → dma_buf_pin(attach)
  │   → dma_buf_map_attachment() → GPU VRAM bus addresses (SG table)
  │
  ├─ 【步骤 3】alloc_cacheable_mr()
  │   从 FRMR pool 获取预分配的 MKey
  │
  ├─ 【步骤 4】mlx5r_umr_update_mr_pas()
  │   用 UMR WQE 更新 MKey 的 PAS
  │   PAS 条目 = GPU VRAM 的 PCIe 总线地址
  │
  ├─ 【步骤 5】标记 data_direct = true
  │   告诉内核: 这个 MKey 的 PAS 指向 GPU 显存
  │
  └─ 【步骤 6】返回 ib_mr
      用户态拿到 lkey/rkey, 可以做 RDMA
```

## 5. dmabuf 撤销回调

`drivers/infiniband/hw/mlx5/mr.c:883-900`

```c
static void mlx5_ib_dmabuf_invalidate_cb(struct dma_buf_attachment *attach)
{
    struct ib_umem_dmabuf *umem_dmabuf = attach->importer_priv;
    struct mlx5_ib_mr *mr = umem_dmabuf->private;

    dma_resv_assert_held(umem_dmabuf->attach->dmabuf->resv);

    if (!umem_dmabuf->sgt || !mr)
        return;

    mlx5r_umr_update_mr_pas(mr, MLX5_IB_UPD_XLT_ZAP);
    ib_umem_dmabuf_unmap_pages(umem_dmabuf);
}

static struct dma_buf_attach_ops mlx5_ib_dmabuf_attach_ops = {
    .allow_peer2peer = 1,                              // 支持 PCIe P2P
    .invalidate_mappings = mlx5_ib_dmabuf_invalidate_cb, // 撤销回调
};
```

当 GPU 驱动异步撤销 dmabuf 时（例如 VRAM 被迁移回系统内存），`invalidate_cb` 通过 `MLX5_IB_UPD_XLT_ZAP` 使 MKey 的 PAS 失效。后续任何通过该 MKey 的 RDMA 操作都会收到 **Access Error**。

## 6. ODP（On-Demand Paging）分支

`drivers/infiniband/hw/mlx5/mr.c:874`

```c
    if (access_flags & IB_ACCESS_ON_DEMAND)
        return create_user_odp_mr(pd, start, length, iova, access_flags, udata);
```

ODP 模式下，MKey 创建时**不填充 PAS**——HCA 在 DMA 时自己处理缺页。页故障由 `mlx5_ib_page_fault_resume` 处理，自动从系统内存或 GPU 显存获取页。

## 7. 访问模式对比

| 模式 | 宏 | 含义 | 典型场景 |
|------|-----|------|---------|
| MTT | `MLX5_MKC_ACCESS_MODE_MTT` | 标准页表翻译 | 常规用户内存 MR |
| KSM | `MLX5_MKC_ACCESS_MODE_KSM` | Key Segment Mode | NIC 内部内存（如 Striding RQ） |
| SW ICM | `MLX5_MKC_ACCESS_MODE_SW_ICM` | 软件管理 ICM | Header modify pattern |
| INTERLEAVED | 交错模式 | 多通道交错访问 | 某些 HPC 场景 |

## 8. 端到端 GPUDirect RDMA 时序

```
                           时间轴 →
用户态    ┌─ibv_reg_dmabuf_mr────┐                ┌─ibv_post_send(RDMA_WRITE)─┐
          │                       │                │                          │
内核      │ dma_buf_get(fd)       │                │                          │
          │ dma_buf_pin           │                │                          │
          │ dma_buf_map_attachment│                │                          │
          │ alloc_cacheable_mr    │                │                          │
          │ UMR update PAS ──────→│  MKey created  │                          │
          │     (GPU VRAM bus)    │  PAS = GPU     │                          │
          │                       │  addrs         │                          │
HCA 固件  │                       │  CREATE_MKEY   │                          │
          │                       │  (如果走慢路径) │                          │
          │                       │                │                          │
HCA 硬件  │                       │                │ DMA read GPU VRAM → 网络 │
          │                       │                │  (PCIe P2P, 零 CPU 参与)   │
          └───────────────────────┴────────────────┴──────────────────────────┘
           ~100μs (UMR fast)       MKey ready!       0-copy DMA!
           ~200μs (reg_create)
```

## 9. 系列结语

至此，我们已经完成了从 **HMM 内存镜像** 到 **GPUDirect RDMA 硬件 DMA** 的完整内核路径：

```
第1篇 HMM          │ CPU↔GPU 页表镜像基础
第2篇 DRM GPUSVM   │ GPU 共享虚拟内存框架
第3篇 nouveau_svm  │ NVIDIA 的 HMM 调用者
第4篇 nouveau_dmem │ GPU VRAM 作为 DEVICE_PRIVATE 内存
第5篇 TTM          │ 多内存类型管理的 BO 框架
第6篇 GPU Buddy    │ 增广红黑树显存块分配器
第7篇 DRM Scheduler│ 信用制 GPU 命令调度
第8篇 umem_dmabuf  │ GPU dmabuf → RDMA MR 桥梁
第9篇 mlx5/mr      │ HCA 硬件侧 MKey 创建与 IOMMU 编程
```

这 9 篇文章覆盖了 NVIDIA AI Infra 的三个维度：

- **统一内存**（文章 1→4）：CUDA Unified Memory 的内核实现
- **显存管理**（文章 5→6）：GPU VRAM 的分配、迁移与回收
- **零拷贝通信**（文章 8→9）：GPUDirect RDMA 的全链路

AI 训练中的 `ncclAllReduce(data, ...)` 一行调用，背后是这数千行内核代码在协作：`hmm_range_fault` 确保 GPU 能访问 CPU 页 → `gpu_buddy_alloc_blocks` 在 VRAM 中分配 buffer → `drm_sched_entity_push_job` 提交计算命令 → `ib_umem_dmabuf_get_pinned` 将 VRAM 映射为 RDMA 可 DMA 的地址 → `mlx5_ib_reg_user_mr_dmabuf` 编程 HCA IOMMU → HCA 通过 PCIe P2P 直接读写 GPU VRAM。

**0 次 CPU 拷贝**，不是魔法——是精心设计的 Linux 内核子系统协同工作的结果。