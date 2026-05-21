# NVIDIA AI Infra 内核源码深度解析系列

本系列从 Linux 内核源码出发，逐层剖析 NVIDIA GPU 在 AI 训练/推理中的内核侧基础设施。从 CPU↔GPU 内存镜像到 GPU↔GPU RDMA 零拷贝数据传输，覆盖 CUDA Unified Memory、GPUDirect RDMA 的完整内核路径。

## 阅读指南

这是一个**从底层到上层、从通用到专用**的递进系列。建议按顺序阅读：

| 序号 | 文章 | 核心文件 | 难度 | 学完能理解 |
|------|------|---------|------|-----------|
| 1 | [CPU↔GPU 内存镜像基础：HMM 深度解析](01-hmm.md) | `mm/hmm.c` | ⭐⭐ | `cudaMallocManaged` 的内核侧原理 |
| 2 | [GPU 共享虚拟内存抽象层：DRM GPUSVM](02-drm-gpusvm.md) | `drivers/gpu/drm/drm_gpusvm.c` | ⭐⭐⭐ | GPU SVM 框架的 notifier/range/pages 体系 |
| 3 | [NVIDIA HMM 调用者：nouveau_svm.c](03-nouveau-svm.md) | `drivers/gpu/drm/nouveau/nouveau_svm.c` | ⭐⭐⭐ | NVIDIA 如何用 HMM 实现页表镜像 |
| 4 | [NVIDIA 设备显存管理：nouveau_dmem.c](04-nouveau-dmem.md) | `drivers/gpu/drm/nouveau/nouveau_dmem.c` | ⭐⭐⭐⭐ | VRAM 作为 DEVICE_PRIVATE 的内存迁移 |
| 5 | [GPU 多内存类型管理：TTM 框架](05-ttm.md) | `drivers/gpu/drm/ttm/ttm_bo.c` | ⭐⭐⭐ | VRAM/GTT/SYSTEM 内存的 placement 和迁移 |
| 6 | [GPU 显存 Buddy 分配器](06-gpu-buddy.md) | `drivers/gpu/buddy.c` | ⭐⭐ | 增广红黑树管理的显存块分配器 |
| 7 | [GPU 命令提交调度：DRM Scheduler](07-gpu-scheduler.md) | `drivers/gpu/drm/scheduler/` | ⭐⭐⭐ | GPU 命令队列调度、流控和超时处理 |
| 8 | [GPU→RDMA 零拷贝桥梁：umem_dmabuf.c](08-umem-dmabuf.md) | `drivers/infiniband/core/umem_dmabuf.c` | ⭐⭐⭐ | GPU dmabuf 如何注册为 RDMA MR |
| 9 | [RDMA 硬件侧 MR 注册：mlx5/mr.c](09-mlx5-mr.md) | `drivers/infiniband/hw/mlx5/mr.c` | ⭐⭐⭐⭐ | MKey 创建、IOMMU 编程、GPUDirect RDMA 硬件路径 |

## 三条主线

整个系列围绕三条主线展开，交叉阅读效果更好：

### 主线 A：统一内存（文 1→2→3→4→5）

```
用户: cudaMallocManaged(p, size)
  → mm/hmm.c: hmm_range_fault()     ← CPU 页表镜像
  → drm_gpusvm.c: drm_gpusvm_ctx    ← GPU SVM 层
  → nouveau_svm.c: 页故障处理       ← NVIDIA 调用者
  → nouveau_dmem.c: VRAM 分配/迁移  ← 设备内存
  → ttm/ttm_bo.c: BO placement      ← 内存类型管理
```

### 主线 B：显存管理（文 5→6）

```
GPU 驱动:
  → ttm/ttm_bo.c: ttm_bo_validate() ← BO 验证/迁移
  → drm/buddy.c: gpu_buddy_alloc()  ← 显存块分配
```

### 主线 C：GPU↔RDMA（文 8→9）

```
GPU 驱动:
  → dmabuf export                  ← 导出 GPU 显存
  → ib_umem_dmabuf_get()           ← RDMA 接收
  → mlx5/mr.c: reg_user_mr_dmabuf  ← 硬件 MR 注册
  → PCIe DMA: GPU显存 ←→ RDMA网卡  ← 零拷贝
```

## 前置知识

- 熟悉 Linux 内核基本概念（`struct page`、虚拟内存、页表）
- 了解 GPU 编程基础（CUDA/HIP、统一内存）
- 了解 RDMA 基本概念（QP、MR、CQ、WQE）
- 内核源码在 `/home/wujing/code/linux`（v7.1.0-rc4，x86，GCC 15.2.0）

## 相关文档

- `Documentation/mm/hmm.rst` — HMM 官方文档
- `Documentation/gpu/drm-mm.rst` — DRM 内存管理文档
- `Documentation/driver-api/dma-buf.rst` — DMA-BUF 文档
- `gpu_buddy_analysis.md` — GPU Buddy vs 内核 Buddy vs SLUB 深度对比
- `kernel_ai_infra.md` — Linux 内核 AI Infra 全景