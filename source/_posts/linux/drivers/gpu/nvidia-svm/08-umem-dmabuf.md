---
title: '第8篇：GPU→RDMA 零拷贝桥梁：umem_dmabuf.c'
date: '2026/05/21 12:01:44'
updated: '2026/05/21 17:08:39'
---

# 第8篇：GPU→RDMA 零拷贝桥梁：umem_dmabuf.c

> 源码：`drivers/infiniband/core/umem_dmabuf.c` | 头文件：`include/rdma/ib_umem.h`
> 系列目录：[NVIDIA AI Infra 内核源码深度解析](https://github.com/realwujing/realwujing.github.io/blob/main/linux/drivers/gpu/nvidia-svm/README.md)

## 1. 问题：GPU 内存如何被 RDMA 网卡直接访问

机器学习训练中，一张 8×H100 GPU 节点在做 all-reduce 时，每个 GPU 需要把本地梯度发给其他 7 张 GPU。传统路径：

```
GPU VRAM → CPU 内存 (cudaMemcpy) → RDMA 网卡 DMA → 网络
                                     ↑
                                   3 次拷贝
```

GPUDirect RDMA 优化后的路径 — CPU 被完全旁路：

```
                         GPU 0 显存 (VRAM)
                        ┌──────────────────┐
  PyTorch/TensorFlow    │  ┌────────────┐  │
  all-reduce 梯度:      │  │  gradient  │  │  ① GPU 驱动 (amdgpu/nouveau)
  GPU0 → GPU1           │  │  tensor    │──┼──── dmabuf export
                        │  └────────────┘  │      (VRAM bus address)
                        └────────┬─────────┘
                                 │
                         PCIe BAR 映射
                         (GPU VRAM 地址 = PCIe 总线地址)
                                 │
                    ┌────────────┼────────────┐
                    │   ib_umem_dmabuf_get()   │  ② RDMA core 接收 dmabuf
                    │   dma_buf_map_attach()   │     获取 VRAM SG table
                    │   → PAS array            │     填充 mlx5 MTT
                    └────────────┬────────────┘
                                 │
           ┌─────────────────────┼─────────────────────┐
           │         RDMA 网卡 (Mellanox HCA)          │  ③ HCA IOMMU/TLB
           │                                           │     存 GPU VRAM
           │  ┌───────────────────────────────────┐   │     总线地址
           │  │  MKey → MTT → GPU VRAM bus addr   │   │
           │  └───────────────────────────────────┘   │
           │                    │                      │
           │           PCIe bus-master DMA             │
           │     HCA 主动读 GPU VRAM 梯度数据          │
           └────────────────────┬─────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    │   InfiniBand/RoCEv2    │  ④ 网络发送
                    │   100-400 Gbps wire    │
                    └───────────┬───────────┘
                                │
                    ┌───────────┴───────────┐
                    │   RDMA 网卡 (GPU 1)    │  ⑤ 远端 HCA
                    │   直接 DMA write 到    │     写入目标 GPU 显存
                    │   GPU 1 VRAM           │
                    └───────────────────────┘

      关键: CPU 不触碰数据，PCIe switch 直接做 GPU↔HCA P2P 转发
```

这要求 RDMA 网卡的 IOMMU 直接映射到 GPU VRAM 的 PCIe 总线地址。`umem_dmabuf.c` 就是实现这个映射的**桥梁代码**。

## 2. 整体数据流

```
GPU 驱动                umem_dmabuf.c               mlx5 驱动
   │                        │                          │
   ├─ export dmabuf fd      │                          │
   │  (GPU VRAM 地址空间)    │                          │
   │                        │                          │
用户态 ioctl ──────────→   │                          │
   │  (传入 dmabuf fd)      │                          │
   │                   ┌────┴────┐                     │
   │                   │ ib_umem │                     │
   │                   │ _dmabuf │                     │
   │                   │ _get    │                     │
   │                   │ (174)   │                     │
   │                   └────┬────┘                     │
   │                        │                          │
   │                   ┌────┴─────┐                    │
   │                   │ dma_buf  │                    │
   │                   │ _get(fd) │ ← import dmabuf    │
   │                   └────┬─────┘                    │
   │                        │                          │
   │                   ┌────┴──────────┐               │
   │                   │ dma_buf       │               │
   │                   │ _dynamic      │               │
   │                   │ _attach       │ ← 绑定设备     │
   │                   └────┬──────────┘               │
   │                        │                          │
用户态 MR 注册 ─────────────────────────────────→     │
   │                   ┌────┴──────────┐   reg_usr    │
   │                   │ dma_buf_pin   │ ← 钉住 VRAM  │
   │                   └────┬──────────┘               │
   │                   ┌────┴──────────┐               │
   │                   │ map_pages     │ ← 获取 SG    │
   │                   │  (15)         │   table       │
   │                   └────┬──────────┘               │
   │                        │                          │
   │                        │  SG table (GPU BUS addr) │
   │                        ├────────────────────────→ │
   │                        │                    ┌─────┴──────┐
   │                        │                    │  populate  │
   │                        │                    │  PAS array │
   │                        │                    │  →CREATE   │
   │                        │                    │  _MKEY fw  │
   │                        │                    └────────────┘
   │                        │                          │
   │                        │   RDMA HCA ←── DMA ───→ GPU VRAM
   │                        │          PCIe P2P
```

### 关键转换：dmabuf fd → RDMA MR

```
dmabuf fd (用户态文件描述符)
     │
     ▼
dma_buf (内核 dmabuf 对象，由 GPU 驱动导出)
     │
     ▼
dma_buf_attachment (IB 设备与 dmabuf 的绑定)
     │
     ▼
dma_buf_map_attachment → sg_table (scatter-gather list)
     │ 每个 sg 条目是一个 GPU VRAM 物理页的 DVA (Device Virtual Address)
     │ 在 x86 上即 PCIe 总线地址
     ▼
RDMA MR (mem region) → 网卡 IOMMU 可 DMA 到 GPU VRAM
```

## 3. 核心数据结构

### 3.1 ib_umem_dmabuf

`include/rdma/ib_umem.h`：

```c
struct ib_umem_dmabuf {
    struct ib_umem           umem;         // 嵌入的通用 umem
    struct dma_buf_attachment *attach;      // dmabuf attach 句柄
    struct sg_table          *sgt;          // 完整的 SG table
    struct scatterlist       *first_sg;     // 裁剪后的首个 SG
    unsigned int             first_sg_offset; // 首 SG 偏移
    struct scatterlist       *last_sg;      // 裁剪后的末 SG
    unsigned int             last_sg_trim;  // 末 SG 尾部裁剪量
    bool                     pinned : 1;    // 是否已 dma_buf_pin
    bool                     revoked : 1;   // 是否已被撤销
    void                     (*pinned_revoke)(void *priv); // 撤销回调
    void                     *private;      // 回调私有数据
};
```

**字段解析**：
- `umem`：内嵌 `ib_umem`，存储 ibdev、length、address（offset）、writable、is_dmabuf=1
- `attach`：通过 `dma_buf_dynamic_attach()` 建立 IB 设备 → dmabuf 的绑定
- `sgt`：`dma_buf_map_attachment()` 返回的完整 SG table（所有 GPU VRAM 页）
- `first_sg / first_sg_offset / last_sg / last_sg_trim`：因为用户可能只需要 VRAM 的一部分（`[offset, offset+length)`），对完整 SG table 做**头尾截断**
- `pinned`：`dma_buf_pin()` 防止 GPU 驱动迁移/回收 VRAM
- `revoked`：异步撤销标记（GPU 驱动回收 VRAM 时置位）

## 4. 核心函数解析

### 4.1 ib_umem_dmabuf_get — 导入 dmabuf fd

`drivers/infiniband/core/umem_dmabuf.c:174-181`

```c
struct ib_umem_dmabuf *ib_umem_dmabuf_get(struct ib_device *device,
                                          unsigned long offset, size_t size,
                                          int fd, int access,
                                          const struct dma_buf_attach_ops *ops)
{
    return ib_umem_dmabuf_get_with_dma_device(device, device->dma_device,
                                              offset, size, fd, access, ops);
}
EXPORT_SYMBOL(ib_umem_dmabuf_get);
```

实际工作委托给 `ib_umem_dmabuf_get_with_dma_device`（行 116-172）：

```c
static struct ib_umem_dmabuf *
ib_umem_dmabuf_get_with_dma_device(
    struct ib_device *device,
    struct device *dma_device,
    unsigned long offset, size_t size,
    int fd, int access,
    const struct dma_buf_attach_ops *ops)
{
    struct dma_buf *dmabuf;
    struct ib_umem_dmabuf *umem_dmabuf;
    struct ib_umem *umem;
    // ...

    dmabuf = dma_buf_get(fd);                    // 1. 通过 fd 获取 dmabuf
    if (IS_ERR(dmabuf))
        return ERR_CAST(dmabuf);

    if (dmabuf->size < end)
        goto out_release_dmabuf;                 // 2. 大小校验

    umem_dmabuf = kzalloc_obj(*umem_dmabuf);     // 3. 分配 umem_dmabuf
    // ...

    umem = &umem_dmabuf->umem;
    umem->ibdev = device;
    umem->length = size;
    umem->address = offset;                       // 4. 填充 ib_umem
    umem->writable = ib_access_writable(access);
    umem->is_dmabuf = 1;

    umem_dmabuf->attach = dma_buf_dynamic_attach( // 5. 绑定 dmabuf
        dmabuf, dma_device, ops, umem_dmabuf);
    // ...
    return umem_dmabuf;
}
```

### 4.2 ib_umem_dmabuf_map_pages — 获取 SG table

`drivers/infiniband/core/umem_dmabuf.c:15-83`

```c
int ib_umem_dmabuf_map_pages(struct ib_umem_dmabuf *umem_dmabuf)
{
    struct sg_table *sgt;
    struct scatterlist *sg;
    unsigned long start, end, cur = 0;
    unsigned int nmap = 0;
    long ret;
    int i;

    dma_resv_assert_held(umem_dmabuf->attach->dmabuf->resv);

    if (umem_dmabuf->revoked)
        return -EINVAL;                        // 已被撤销，拒绝

    if (umem_dmabuf->sgt)
        goto wait_fence;                       // 已 map，跳至 fence 等待

    sgt = dma_buf_map_attachment(              // 获取 GPU VRAM DMA 地址
        umem_dmabuf->attach, DMA_BIDIRECTIONAL);
    if (IS_ERR(sgt))
        return PTR_ERR(sgt);

    /* modify the sg list in-place to match umem address and length */
    start = ALIGN_DOWN(umem_dmabuf->umem.address, PAGE_SIZE);
    end = ALIGN(umem_dmabuf->umem.address + umem_dmabuf->umem.length,
                PAGE_SIZE);
```

**SG table 裁剪逻辑**（行 37-63）：

```
完整 SG table（GPU 导出的全部 VRAM 地址）:
 ┌─────────┬─────────┬─────────┬─────────┬─────────┐
 │  sg[0]  │  sg[1]  │  sg[2]  │  sg[3]  │  sg[4]  │
 └─────────┴─────────┴─────────┴─────────┴─────────┘
                     ↑                   ↑
                  offset              offset+length
                  (start)              (end)

 裁剪后:
                  ┌─────────┬─────────┐
                  │  sg[1]  │  sg[2]  │   ← nmap=2
                  └─────────┴─────────┘
              first_sg_offset ↑      ↑ last_sg_trim
                 (截去前半)          (截去后半)
```

**fence 等待**（行 69-82）：即使 SG table 已就绪，页内容可能还未迁移到位。调用 `dma_resv_wait_timeout` 等待 GPU 驱动完成迁移。

### 4.3 ib_umem_dmabuf_unmap_pages — 恢复 SG table

`drivers/infiniband/core/umem_dmabuf.c:86-113`

```c
void ib_umem_dmabuf_unmap_pages(struct ib_umem_dmabuf *umem_dmabuf)
{
    dma_resv_assert_held(umem_dmabuf->attach->dmabuf->resv);

    if (!umem_dmabuf->sgt)
        return;

    /* restore the original sg list */
    if (umem_dmabuf->first_sg) {
        sg_dma_address(umem_dmabuf->first_sg) -=
            umem_dmabuf->first_sg_offset;
        sg_dma_len(umem_dmabuf->first_sg) +=
            umem_dmabuf->first_sg_offset;
        umem_dmabuf->first_sg = NULL;
        umem_dmabuf->first_sg_offset = 0;
    }
    if (umem_dmabuf->last_sg) {
        sg_dma_len(umem_dmabuf->last_sg) +=
            umem_dmabuf->last_sg_trim;
        umem_dmabuf->last_sg = NULL;
        umem_dmabuf->last_sg_trim = 0;
    }

    dma_buf_unmap_attachment(umem_dmabuf->attach, umem_dmabuf->sgt,
                             DMA_BIDIRECTIONAL);
    umem_dmabuf->sgt = NULL;
}
```

**关键细节**：因为 map 时做了**就地修改**（in-place modification），unmap 时必须**恢复**原本的 SG 值。否则 `dma_buf_unmap_attachment` 传入的 SG table 与返回时不匹配。

### 4.4 ib_umem_dmabuf_get_pinned — 钉住 + 映射

`drivers/infiniband/core/umem_dmabuf.c:317-325`

```c
struct ib_umem_dmabuf *ib_umem_dmabuf_get_pinned(
    struct ib_device *device,
    unsigned long offset, size_t size, int fd, int access)
{
    return ib_umem_dmabuf_get_pinned_with_dma_device(
        device, device->dma_device, offset, size, fd, access);
}
```

实际路径（`ib_umem_dmabuf_get_pinned_with_dma_device` → `ib_umem_dmabuf_get_pinned_and_lock`，行 213-245）：

```c
static struct ib_umem_dmabuf *
ib_umem_dmabuf_get_pinned_and_lock(
    struct ib_device *device,
    struct device *dma_device,
    unsigned long offset, size_t size, int fd, int access,
    const struct dma_buf_attach_ops *ops)
{
    struct ib_umem_dmabuf *umem_dmabuf;
    int err;

    umem_dmabuf = ib_umem_dmabuf_get_with_dma_device( // 1. import dmabuf
        device, dma_device, offset, size, fd, access, ops);
    if (IS_ERR(umem_dmabuf))
        return umem_dmabuf;

    dma_resv_lock(umem_dmabuf->attach->dmabuf->resv, NULL);
    err = dma_buf_pin(umem_dmabuf->attach);           // 2. 钉住 VRAM
    if (err)
        goto err_release;
    umem_dmabuf->pinned = 1;

    err = ib_umem_dmabuf_map_pages(umem_dmabuf);      // 3. 获取 SG table
    if (err)
        goto err_release;
    return umem_dmabuf;
}
```

**三步走**：
1. `dma_buf_get` → import dmabuf fd
2. `dma_buf_pin` → 通知 GPU 驱动：这块 VRAM **不能被迁移/换出**
3. `dma_buf_map_attachment` → 获取 PCIe 总线地址（SG table）

### 4.5 两种模式：Pinned vs Revocable

| 特性 | Pinned 模式 | Revocable 模式 |
|------|------------|----------------|
| attach_ops | `ib_umem_dmabuf_attach_pinned_ops` (行 184) | `ib_umem_dmabuf_attach_pinned_revocable_ops` (行 208) |
| `dma_buf_pin` | ✅ | ✅ |
| invalidate 回调 | ❌ 无 | `ib_umem_dmabuf_revoke_locked` (行 188) |
| GPU 可撤销访问 | ❌ VRAM 被钉住 | ✅ GPU 可异步回收 VRAM |
| 适用场景 | GPUDirect RDMA（稳定 DMA） | ODP（按需分页） |

**Revocable 模式**（行 293-305）：

```c
struct ib_umem_dmabuf *
ib_umem_dmabuf_get_pinned_revocable_and_lock(
    struct ib_device *device,
    unsigned long offset, size_t size, int fd, int access)
{
    const struct dma_buf_attach_ops *ops =
        &ib_umem_dmabuf_attach_pinned_revocable_ops;
    return ib_umem_dmabuf_get_pinned_and_lock(
        device, device->dma_device, offset, size, fd, access, ops);
}
```

**撤销回调**（行 188-206）：

```c
static void ib_umem_dmabuf_revoke_locked(struct dma_buf_attachment *attach)
{
    struct ib_umem_dmabuf *umem_dmabuf = attach->importer_priv;

    if (umem_dmabuf->revoked)
        return;

    if (umem_dmabuf->pinned_revoke)
        umem_dmabuf->pinned_revoke(umem_dmabuf->private);

    ib_umem_dmabuf_unmap_pages(umem_dmabuf);
    if (umem_dmabuf->pinned) {
        dma_buf_unpin(umem_dmabuf->attach);
        umem_dmabuf->pinned = 0;
    }
    umem_dmabuf->revoked = 1;
}
```

## 5. dmabuf 代码集成点

`drivers/infiniband/core/umem_dmabuf.c:6-13`

```c
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>

#include "uverbs.h"

MODULE_IMPORT_NS("DMA_BUF");
```

需要 `CONFIG_DMABUF` 和 `MODULE_IMPORT_NS("DMA_BUF")`——dmabuf 子系统使用内核符号命名空间隔离。

## 6. 与 NVIDIA AI Infra 的关系

GPUDirect RDMA 完整数据流：

```
┌──────────────────────────────────────────────────────────────┐
│                       GPUDirect RDMA 路径                     │
│                                                              │
│  训练框架 (PyTorch/Horovod):                                  │
│    1. ncclAllReduce(sendbuff, recvbuff)                      │
│       ↓                                                      │
│  NVIDIA GPU 驱动:                                             │
│    2. 导出 VRAM buffer 为 dmabuf fd                          │
│       dma_buf_export(dmabuf)                                 │
│       ↓                                                      │
│  libibverbs (用户态 RDMA):                                    │
│    3. ibv_reg_dmabuf_mr(pd, dmabuf_fd)                      │
│       ↓ ioctl                                                │
│  umem_dmabuf.c (内核):                                       │
│    4. ib_umem_dmabuf_get_pinned() ← import dmabuf fd        │
│    5. dma_buf_pin()               ← 钉住 GPU VRAM            │
│    6. dma_buf_map_attachment()    ← 获取 GPU BUS 地址 (SG)   │
│       ↓                                                      │
│  mlx5/mr.c:                                                  │
│    7. mlx5_ib_reg_user_mr_dmabuf() ← 创建 HCA MKey           │
│    8. mlx5_ib_populate_pas()       ← 填充 PAS 数组           │
│    9. CREATE_MKEY (firmware cmd)    ← 编程 IOMMU              │
│       ↓                                                      │
│  ConnectX HCA:                                               │
│   10. DMA Read/Write ──── PCIe P2P ────→ GPU VRAM            │
│                                                              │
│  结果: 0 次 CPU 拷贝, HCA 直接读写 GPU 显存                   │
└──────────────────────────────────────────────────────────────┘
```

---

## 下一篇文章

[第9篇：RDMA 硬件侧 MR 注册：mlx5/mr.c](09-mlx5-mr.md)

简介：Mellanox ConnectX HCA 的 MKey 创建流程，包括 UMR 快速路径、CREATE_MKEY 慢路径，以及 GPUDirect RDMA 的硬件 IOMMU 编程。