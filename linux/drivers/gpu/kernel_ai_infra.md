# Linux 内核 AI Infra 全景：GPU、加速器、RDMA 与异构计算

## 目录
1. [内核 AI 基础设施总览](#内核-ai-基础设施总览)
2. [专用 AI 加速器框架：drivers/accel](#专用-ai-加速器框架driversaccel)
3. [GPU 计算驱动](#gpu-计算驱动)
4. [RDMA 子系统详解](#rdma-子系统详解)
5. [GPU ↔ RDMA 集成：零拷贝数据传输](#gpu--rdma-集成零拷贝数据传输)
6. [异构内存管理：HMM/GPUSVM/Pagemap](#异构内存管理hmmsvm)
7. [其他 AI 相关基础设施](#其他-ai-相关基础设施)
8. [分布式 AI 训练的完整内核栈](#分布式-ai-训练的完整内核栈)

---

## 内核 AI 基础设施总览

Linux 内核中与 AI Infra 相关的代码分四大板块：

```
┌─────────────────────────────────────────────────────────────┐
│                      AI Infra in Linux                       │
│                                                             │
│  ┌─────────┐  ┌──────────┐  ┌─────────┐  ┌──────────────┐  │
│  │ accel   │  │ GPU DRM  │  │ RDMA/IB │  │ 跨子系统基础  │  │
│  │ 加速器   │  │ GPU 驱动  │  │ 网络互联 │  │ HMM/DMA-BUF  │  │
│  └────┬────┘  └────┬─────┘  └────┬────┘  └──────┬───────┘  │
│       │            │             │               │          │
│  ─────┴────────────┴─────────────┴───────────────┴──────    │
│       all built on DRM base + IB core + mm core             │
└─────────────────────────────────────────────────────────────┘
```

| 板块 | 路径 | 核心职责 |
|------|------|---------|
| **accel** | `drivers/accel/` | 专用 NPU/AI 加速器驱动框架 |
| **GPU DRM** | `drivers/gpu/drm/` | GPU 计算驱动（amdgpu/nouveau/xe/panthor） |
| **RDMA/IB** | `drivers/infiniband/` | 高速网络互联（InfiniBand/RoCE/iWARP） |
| **跨子系统** | `mm/hmm.c`, `drivers/dma-buf/`, `drivers/cxl/` | 异构内存、零拷贝共享、缓存一致性互联 |

---

## 专用 AI 加速器框架：drivers/accel/

内核为 AI/ML 加速器设立了**专门的子系统** `drivers/accel/`，Kconfig 明确声明：

> *"Machine-Learning and Deep-Learning acceleration devices."*

它基于 DRM 构建，但有独立的设备节点 `/dev/accel/accel*`（major 号与 DRM 分离），提供专门的 UAPI。

### 核心框架

```
drivers/accel/
├── Kconfig              ← DRM_ACCEL config entry
├── Makefile             ← 构建 6 个加速器驱动
└── drm_accel.c          ← 核心粘合层，与 DRM 衔接
```

### 已有驱动

| 驱动 | 目录 | 硬件 | 定位 |
|------|------|------|------|
| **amdxdna** | `drivers/accel/amdxdna/` | AMD NPU (Ryzen AI 300) | 客户端 AI 推理，AIE2 solver，HMM 支持 |
| **habanalabs** | `drivers/accel/habanalabs/` | Habana Gaudi/Goya | 数据中心 DL 训练/推理 |
| **ivpu** | `drivers/accel/ivpu/` | Intel NPU (Meteor Lake+) | 客户端 CV/DL 推理 |
| **qaic** | `drivers/accel/qaic/` | Qualcomm Cloud AI | 云端 DL 推理加速卡 |
| **rocket** | `drivers/accel/rocket/` | Rockchip NPU (RK3588) | 边缘 AI 推理 (RKNN) |
| **ethosu** | `drivers/accel/ethosu/` | Arm Ethos-U65/U85 | 嵌入式 microNPU |

### UAPI 头文件

```
include/uapi/drm/amdxdna_accel.h      # AMD NPU UAPI
include/uapi/drm/habanalabs_accel.h   # Habana AIP UAPI
include/uapi/drm/ivpu_accel.h         # Intel NPU UAPI
include/uapi/drm/qaic_accel.h         # Qualcomm Cloud AI UAPI
include/uapi/drm/rocket_accel.h       # Rockchip NPU UAPI
include/uapi/drm/ethosu_accel.h       # Arm Ethos-U UAPI
```

---

## GPU 计算驱动

GPU 仍是 AI 训练/推理的主力硬件，内核中主要 GPU 计算驱动如下：

### AMD GPU — 最完整的开源 AI 计算栈

```
drivers/gpu/drm/amd/
├── amdgpu/             ← GPU 驱动核心 (CDNA/MI Instinct + RDNA)
├── amdkfd/             ← HSA Kernel Fusion Driver (ROCm 后端)
└── ...
```

**amdkfd** 是 AMD AI 计算的关键：
- `HSA_AMD_SVM`：HMM-based shared virtual memory（HIP 统一内存）
- `HSA_AMD_P2P`：GPU to GPU peer-to-peer DMA
- 选择 `HMM_MIRROR`，支持 CPU-GPU 页表镜像

### Intel GPU — oneAPI 后端

```
drivers/gpu/drm/
├── i915/               ← Intel 集显 (pre-Xe)，OpenCL/Vulkan compute
├── xe/                 ← Intel Xe2 独立显卡 (Battlemage/Lunar Lake)
│   ├── DRM_XE_GPUSVM   ← GPU 共享虚拟内存 (ZONE_DEVICE)
│   └── DRM_XE_PAGEMAP  ← 设备内存池管理器
```

### NVIDIA GPU — 开源 + Rust 新驱动

```
drivers/gpu/drm/
├── nouveau/            ← 传统开源驱动
│   ├── nouveau_svm.c   ← HMM SVM (CUDA Unified Memory)
│   └── nouveau_dmem.c  ← 设备内存管理
├── nova/               ← Rust 编写的 GSP 架构新驱动 (WIP)
└── nova-core/          ← Rust GSP 核心库 (WIP)
```

### ARM Mali GPU — 移动/边缘 AI

```
drivers/gpu/drm/
├── panfrost/           ← Mali Midgard/Bifrost
├── panthor/            ← Mali Valhall CSF (v10+)
└── tyr/                ← Rust 编写的 Mali CSF 新驱动 (WIP)
```

### 其他有计算能力的 GPU

```
drivers/gpu/drm/
├── msm/                ← Qualcomm Adreno (Vulkan/OpenCL 通过 freedreno)
├── v3d/                ← Broadcom VideoCore (Raspberry Pi, OpenCL)
├── etnaviv/            ← Vivante GCxxx (NXP i.MX)
├── lima/               ← ARM Mali Utgard (Mali-400/450)
├── imagination/        ← PowerVR Rogue
└── virtio/             ← 虚拟 GPU (KVM GPU 直通/虚拟化)
```

### GPU 核心计算框架

```
drivers/gpu/drm/
├── scheduler/          ← GPU 命令提交调度 (DRM_SCHED)
│                         amdgpu/nouveau/xe/panthor/所有 accel 驱动均依赖
├── ttm/                ← GPU 多内存类型管理 (VRAM/GTT/system)
├── drm_gpuvm.c         ← GPU 虚拟地址空间管理 (rb-tree)
├── drm_gpusvm.c        ← GPU 共享虚拟内存层 (基于 HMM)
└── drm_pagemap.c       ← 设备内存池 (xe 使用)
```

---

## RDMA 子系统详解

RDMA 代码在 `drivers/infiniband/` 下，分四个层次：

```
drivers/infiniband/
├── core/          ← RDMA 核心框架
├── hw/            ← 硬件驱动 (19 个)
├── sw/            ← 软件实现
└── ulp/           ← Upper Layer Protocols
```

### 核心模块 (`core/`)

| 模块 | 产出的 .ko | 功能 |
|------|-----------|------|
| `ib_core` | `ib_core.ko` | IB 核心：verbs API、MR、CQ、QP |
| `rdma_cm` | `rdma_cm.ko` | RDMA 连接管理器 |
| `ib_cm` | `ib_cm.ko` | IB 通信管理器 |
| `iw_cm` | `iw_cm.ko` | iWARP 通信管理器 |
| `ib_uverbs` | `ib_uverbs.ko` | 用户态 Verbs API |
| `ib_umad` | `ib_umad.ko` | 用户态 MAD 接口 |
| `rdma_ucm` | `rdma_ucm.ko` | 用户态连接管理器 |

### 硬件驱动 (`hw/`)

| 驱动 | 厂商 | 协议 | AI 训练集群常见度 |
|------|------|------|------------------|
| **mlx5** | NVIDIA/Mellanox ConnectX-4+ | InfiniBand + RoCE | ★★★★★ |
| **mlx4** | Mellanox ConnectX-3 | InfiniBand + RoCE | ★★★ |
| **irdma** | Intel E810+ | RoCEv2 | ★★★ |
| **hfi1** | Intel Omni-Path | Omni-Path | ★★★ |
| **erdma** | 阿里云 Elastic RDMA | RoCEv2 (云原生) | ★★★ |
| **hns** | 华为 HiSilicon | RoCE | ★★ |
| **bnxt_re** | Broadcom NetXtreme-E | RoCEv2 | ★★ |
| **ionic** | AMD Pensando | RoCE | ★★ |
| **mana** | Microsoft Azure MANA | RDMA (Azure) | ★★ |
| **efa** | Amazon EFA | SRD (AWS) | ★★★ |
| **vmw_pvrdma** | VMware | 虚拟 RDMA | ★★ |
| **cxgb4** | Chelsio T4/T5 | iWARP | ★ |
| **qedr** | Marvell FastLinQ | RoCE/iWARP | ★ |
| **ocrdma** | Emulex OneConnect | RoCE | ★ |
| **usnic** | Cisco UCS VIC | USNIC | ★ |
| **bng_re** | Broadcom NetXtreme-C/E | RoCEv2 | ★ |
| **mthca** | Mellanox InfiniHost (legacy) | InfiniBand | ★ |

### 软件实现 (`sw/`)

| 模块 | 协议 | 说明 |
|------|------|------|
| **rxe** | RoCEv2 软件实现 | 无需 RDMA 硬件，以太网即可 |
| **siw** | iWARP 软件实现 | 无 RDMA 硬件也能跑 iWARP |
| **rdmavt** | — | RDMA 虚拟传输层库 |

### Upper Layer Protocols (`ulp/`)

| 模块 | 说明 |
|------|------|
| **ipoib** | IP over InfiniBand |
| **iser/isert** | iSCSI over RDMA (initiator/target) |
| **srp/srpt** | SCSI RDMA Protocol (initiator/target) |
| **rtrs** | RDMA Transport (块设备远程挂载) |

---

## GPU ↔ RDMA 集成：零拷贝数据传输

GPU 和 RDMA 的核心集成点是**零拷贝**——RDMA 网卡直接 DMA 读写 GPU 显存，CPU 和系统内存被完全旁路。

### 集成点 1：DMA-BUF (`umem_dmabuf.c`)

```
drivers/infiniband/core/umem_dmabuf.c
```

RDMA 通过 `ib_umem_dmabuf_get()` 接收 GPU 导出的 `dmabuf`：

```c
// umem_dmabuf.c:32-66
sgt = dma_buf_map_attachment(umem_dmabuf->attach,
                             DMA_BIDIRECTIONAL);
// 然后将 GPU 显存的 scatter-gather list 映射到 RDMA 设备的 IOMMU 空间
umem_dmabuf->umem.sgt_append.sgt.sgl = umem_dmabuf->first_sg;
umem_dmabuf->umem.sgt_append.sgt.nents = nmap;
```

之后 RDMA 网卡 DMA 直接操作 GPU 显存：

```
GPU 显存 → dmabuf export → ib_umem_dmabuf_get → dma_buf_map → RDMA NIC DMA
                                                         ↑ CPU 旁路
```

GPU 驱动端（以 `amdgpu` 为例）负责将显存 Buffer 导出为 dmabuf，RDMA 端接收后直接做 DMA 映射。整个过程不需要 CPU 拷贝数据。

### 集成点 2：HMM + ODP (`umem_odp.c`)

```
drivers/infiniband/core/umem_odp.c
```

```c
// umem_odp.c:43-44
#include <linux/hmm.h>
#include <linux/hmm-dma.h>

// umem_odp.c:365-373
range.hmm_pfns = &(umem_odp->map.pfn_list[pfn_start_idx]);
ret = hmm_range_fault(&range);
```

IB On-Demand Paging (ODP) 直接调用 `hmm_range_fault()`。当 GPU 和 CPU 共享统一内存（通过 HMM）时，RDMA 可以按需获取页映射，不需要预先 pin 所有物理内存。

### 完整的 GPUDirect RDMA 数据路径

```
 ┌────────┐                                           ┌────────┐
 │ GPU 0  │  VRAM                                    │ GPU 1  │ VRAM
 │ ┌────┐ │                                          │ ┌────┐ │
 │ │Buffer│──► dmabuf export                         │ │Buffer│
 │ └────┘ │     │                                     │ └────┘ │
 └────────┘     │                                     └────────┘
                ▼                                          ▲
         ┌─────────────┐                            ┌─────────────┐
         │ ib_umem_dmabuf│──► ib_core MR ──► QP send──► ib_core  │
         │  _get()      │                            │ verbs       │
         └──────┬──────┘                            └──────┬──────┘
                │                                          │
         ┌──────▼──────┐                            ┌──────▼──────┐
         │  RDMA NIC   │════════ RDMA wire ═════════│  RDMA NIC   │
         │   mlx5      │     zero-copy DMA          │   mlx5      │
         └─────────────┘                            └─────────────┘
              ▲                                          ▲
              └───────── CPU 完全旁路 ──────────────────┘
```

关键点：
- **GPU 驱动** (amdgpu/nouveau/xe)：导出 dmabuf
- **RDMA core** (`umem_dmabuf.c`)：接收 dmabuf，建立 DMA 映射
- **RDMA NIC 驱动** (mlx5/irdma/etc)：硬件 DMA 引擎直接读写 GPU 显存
- **HMM/ODP** (`mm/hmm.c` + `umem_odp.c`)：按需页镜像，避免预 pin

---

## 异构内存管理：HMM/GPUSVM/Pagemap

```
mm/hmm.c                        ← 核心：CPU 页表镜像到设备页表
drivers/gpu/drm/drm_gpusvm.c    ← DRM 层 GPU SVM（基于 HMM）
drivers/gpu/drm/drm_pagemap.c   ← 设备内存池（xe 使用）
drivers/gpu/drm/drm_gpuvm.c     ← GPU 虚拟地址空间管理
```

### 调用关系

```
用户态 (CUDA/HIP/oneAPI)
  │
  ├── gpu_buddy_alloc_blocks()  ← GPU 显存分配 (drivers/gpu/buddy.c)
  │      元数据从 SLUB kmem_cache 分配
  │
  ├── drm_gpusvm / drm_pagemap  ← 共享虚拟内存管理
  │      │
  │      └── hmm_range_fault()  ← CPU 页表镜像 (mm/hmm.c)
  │
  ├── drm_gpuvm                 ← GPU VA 空间管理 (rb-tree)
  │
  └── dmabuf export → ib_umem_dmabuf → RDMA DMA
```

---

## 其他 AI 相关基础设施

### CXL (Compute Express Link)

```
drivers/cxl/
├── core/           ← CXL 核心：区域管理、内存设备、端口、HDM 解码器、RAS
├── pci.c           ← CXL PCI 设备枚举
├── acpi.c          ← CXL ACPI 表解析
└── pmem.c          ← CXL 持久内存支持
```

CXL 允许 CPU 和加速器之间缓存一致性内存共享（CXL.cache / CXL.mem），是未来 GPU/加速器直接访问系统内存或扩展显存的关键技术。

### DMA-BUF 共享框架

```
drivers/dma-buf/
├── dma-buf.c       ← buffer 共享核心
├── dma-fence.c     ← 跨设备同步
├── dma-resv.c      ← 缓冲区预留管理
├── sync_file.c     ← 用户态同步文件
├── udmabuf.c       ← 用户态可映射 DMA-BUF
└── heaps/          ← 用户态可分配 DMA-BUF heaps (system, CMA)
```

### GPU 内存分配器

```
drivers/gpu/buddy.c         ← GPU Buddy 分配器（增广红黑树，clear/dirty 双树）
drivers/gpu/drm/drm_buddy.c ← DRM 层 buddy 打印封装
drivers/gpu/drm/ttm/        ← TTM 多内存类型管理器
```

详细对比见 `gpu_buddy_analysis.md`。

---

## 分布式 AI 训练的完整内核栈

```
┌──────────────────────────────────────────────────────────────────┐
│                      分布式 AI 训练 (e.g. PyTorch DDP)            │
│                                                                  │
│  All-Reduce: GPU 0 ←─RDMA─→ GPU 1 ←─RDMA─→ GPU 2 ←─...          │
└──────────────────────────────────────────────────────────────────┘
                              │
     ┌────────────────────────┼────────────────────────┐
     │ 用户态                  │                        │
     │ ┌──────────┐  ┌────────┴─────┐  ┌────────────┐ │
     │ │  CUDA/   │  │ libibverbs   │  │ 用户态驱动  │ │
     │ │  ROCm    │  │ (rdma-core)  │  │ (UMD)      │ │
     │ └────┬─────┘  └──────┬───────┘  └─────┬──────┘ │
     └──────┼───────────────┼────────────────┼────────┘
            │               │                │
     ═══════╪═══════════════╪════════════════╪══════════════
            │  内核态        │                │
            │               │                │
     ┌──────▼──────┐  ┌─────▼──────┐  ┌─────▼──────┐
     │ amdgpu/xe/  │  │ ib_core    │  │ accel      │
     │ nouveau     │  │ ib_uverbs  │  │ (amdxdna/  │
     │ (GPU 驱动)   │  │ rdma_cm    │  │ habanalabs) │
     └──┬──────┬───┘  └──┬────┬────┘  └────────────┘
        │      │          │    │
   ┌────▼──┐ ┌─▼────────┐ │ ┌──▼──────────┐
   │ gpu_  │ │drm_gpusvm│ │ │ umem_dmabuf │  ← GPU dmabuf → RDMA 零拷贝
   │ buddy │ │drm_gpuvm │ │ │ umem_odp    │  ← HMM 按需页映射
   └───────┘ └──┬───────┘ │ └─────────────┘
                │          │
          ┌─────▼──────┐ ┌─▼──────────┐
          │ mm/hmm.c   │ │ mlx5/irdma │
          │ HMM        │ │ (RDMA NIC) │
          └────────────┘ └────────────┘
                │
          ┌─────▼──────┐
          │ 内核 Buddy  │  ← 物理 RAM 页框
          │ page_alloc  │
          └────────────┘
```

### 关键数据流

1. **内存分配**：GPU Buddy 从 SLUB 借元数据管理 GPU 显存，内核 Buddy 管理物理 RAM
2. **显存注册**：GPU 驱动将显存 buffer 导出为 dmabuf
3. **RDMA 注册**：`ib_umem_dmabuf_get()` 接收 dmabuf，建立 DMA 映射
4. **数据传输**：RDMA NIC DMA 引擎直接读写 GPU 显存，CPU 完全旁路
5. **统一内存**：HMM 镜像 CPU 页表 → GPU SVM 层 → GPUDirect RDMA + ODP 按需 pin

---

## 源码速查

| 想了解的内容 | 先看这里 |
|-------------|---------|
| NPU 加速器驱动怎么写 | `drivers/accel/drm_accel.c` + 任意一个 accel 驱动 |
| GPU 显存怎么管 | `drivers/gpu/buddy.c` (分配), `drivers/gpu/drm/ttm/` (迁移) |
| GPU 统一内存怎么实现 | `mm/hmm.c` → `drivers/gpu/drm/drm_gpusvm.c` |
| RDMA 怎么收 GPU 内存 | `drivers/infiniband/core/umem_dmabuf.c` |
| RDMA 怎么用 HMM | `drivers/infiniband/core/umem_odp.c:365` `hmm_range_fault()` |
| 多 GPU 怎么 P2P | `drivers/gpu/drm/amd/amdkfd/` `HSA_AMD_P2P` |
| CXL 内存怎么共享 | `drivers/cxl/core/region.c` |