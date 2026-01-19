# Linux 内核 AI 基础设计 (AI Infra) 参与指南

在 `linux` 目录下，如果你希望深度参与 **AI 基础设计（AI Infra）**，以下是按核心价值排序的技术地图。高优先级的“必争之地”排在最前面：

---

## 第一梯队：算力核心与异构管理 (Core Compute & Heterogeneous Management)
*这是 AI Infra 的心脏，决定了内核如何与 AI 芯片对话。*

### 1. AI 加速器子系统 (`drivers/accel/`)
这是 Linux 内核中关于 AI Infra 最直接的模块。它是专门为计算加速器（Compute Accelerators）设立的子系统，目标是统一 AI 芯片的驱动架构。
*   **关键目录**：
    *   [drivers/accel/](https://github.com/torvalds/linux/tree/master/drivers/accel)：子系统核心框架。
    *   [drivers/accel/habanalabs/](https://github.com/torvalds/linux/tree/master/drivers/accel/habanalabs)：Intel Habana Labs AI 芯片驱动。
    *   [drivers/accel/amdxdna/](https://github.com/torvalds/linux/tree/master/drivers/accel/amdxdna)：AMD XDNA AI 引擎驱动。
    *   [drivers/accel/ivpu/](https://github.com/torvalds/linux/tree/master/drivers/accel/ivpu)：Intel NPU (Vision Processing Unit) 驱动。
    *   [drivers/accel/qaic/](https://github.com/torvalds/linux/tree/master/drivers/accel/qaic)：Qualcomm Cloud AI 加速卡驱动。

### 2. GPU 与 DRM 框架 (`drivers/gpu/drm/`)
GPU 是当前 AI 计算的基石。在 Linux 中，GPU 驱动不仅负责显示，更重要的是通过 `DRM` (Direct Rendering Manager) 提供计算接口。
*   **参与点**：
    *   [drivers/gpu/drm/amd/amdgpu/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/amd/amdgpu)：AMD 的高性能计算架构。
    *   [drivers/gpu/drm/xe/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/xe)：Intel 为其 XPU 架构设计的新一代驱动（性能更优，专为计算优化）。
    *   [drivers/gpu/drm/nouveau/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/nouveau)：Nvidia 的开源驱动。

### 3. AMD KFD (Kernel Fusion Driver)
这是 AMD ROCm 计算栈在内核的核心组件，是除了显卡驱动外最值得 AI 工程师研究的模块。
*   [drivers/gpu/drm/amd/amdkfd/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/amd/amdkfd)：专门负责异构计算的进程管理、地址空间映射（SVM）和多 GPU 拓扑识别。

### 4. 自定义 AI 调度器 (sched_ext)
这是当前 Linux 内核调度器领域最前沿的热点，专为大模型训练等负载设计。
*   [kernel/sched/ext.c](https://github.com/torvalds/linux/blob/master/kernel/sched/ext.c)：**sched_ext**。它允许开发者直接在 eBPF 中实现复杂的 AI 任务调度策略，无需重新部署内核，即可在运行时优化 GPU 任务的 CPU 协同调度。

### 5. 集用化的加速器调度 (`drivers/gpu/drm/scheduler/`)
多个 AI 任务如何竞争同一块 GPU 的执行权？
*   [drivers/gpu/drm/scheduler/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/scheduler)：**DRM GPU Scheduler**。这是一个通用的任务调度框架（被 AMD、Intel、ARM 等广泛使用），负责处理任务优先级、依赖关系以及超时恢复（TDR），是 AI 算力调度逻辑的核心。

---

## 第二梯队：内存架构与“内存墙”破局 (Memory Wall & Interconnect)
*AI 模型的上限取决于数据搬运速度。*

### 6. 异构计算互联 (PCIe & CXL)
AI 集群的性能上限往往取决于卡间互联（Interconnect）。
*   **CXL (`drivers/cxl/`)**：**Compute Express Link**，是当前 AI 领域最热门的硬件协议之一，解决了 CPU 与 GPU/NPU 之间的内存池化（Memory Pooling）和扩展。
*   **PCIe P2P (`drivers/pci/`)**：研究 **Peer-to-Peer DMA**，这是实现 GPU 之间不绕过 CPU 直接传输数据的底层技术，也是 NVLink/Infinity Fabric 兼容内核的基础。

### 7. 异构内存管理 (`mm/`)
AI 模型通常很大，CPU 和 GPU/NPU 之间的高效内存共享是 AI Infra 的难点。
*   **关键代码**：
    *   [mm/hmm.c](https://github.com/torvalds/linux/blob/master/mm/hmm.c)：**HMM (Heterogeneous Memory Management)**，支持 CPU 和加速器共享统一虚拟地址空间。
    *   [mm/migrate_device.c](https://github.com/torvalds/linux/blob/master/mm/migrate_device.c)：加速器与系统内存之间的数据迁移逻辑。

### 8. 多层级内存调度 (Memory Tiering)
在现代 AI 服务器中，内存不再是单一层级（HBM + DDR + CXL）。
*   [mm/memory-tiers.c](https://github.com/torvalds/linux/blob/master/mm/memory-tiers.c)：**Memory Tiering** 框架负责决定页面在不同速度内存间的移动（Promotion/Demotion），确保热数据留在 HBM，冷数据下放到 CXL。

### 9. 虚拟化与直通 (IOMMUFD)
在云原生环境下，如何让 AI 算力安全、高效地暴露给容器或虚拟机。
*   [drivers/iommu/iommufd/](https://github.com/torvalds/linux/tree/master/drivers/iommu/iommufd)：**IOMMUFD**，这是为了支持现代加速器直通而重新设计的框架，极大地提升了 AI 加速器的管理灵活性。
*   [drivers/vfio/](https://github.com/torvalds/linux/tree/master/drivers/vfio)：**GPU/NPU Passthrough** 的核心实现。

### 10. 独立显存的精密管家 (`drivers/gpu/drm/ttm/`)
对于拥有独立显存（Discrete VRAM）的千亿级参数加速卡，内存管理异常复杂。
*   [drivers/gpu/drm/ttm/](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/ttm)：**Translation Table Manager (TTM)**。它负责在系统内存 and 设备显存之间透明地分配、迁移 and 置换数据块，是高性能 AI 芯片处理超显存（Out-of-Core）任务的底层依赖。

---

## 第三梯队：分布式集群与极速通信 (Scaling & High-Speed Networking)
*万卡集群的效率取决于网络协议栈的开销。*

### 11. 高性能互联与 RDMA (`drivers/infiniband/`)
分布式 AI 训练需要极低延迟的网络栈。RDMA (Remote Direct Memory Access) 是大模型集群（如万卡集群）的核心。
*   [drivers/infiniband/](https://github.com/torvalds/linux/tree/master/drivers/infiniband)：包括 Mellanox (Nvidia) 等厂商的驱动，支持 RoCE 和 InfiniBand。

### 12. 混合动力通信层 (SMC)
在大模型集群中实现比原生 TCP 更快的协议栈。
*   [net/smc/](https://github.com/torvalds/linux/tree/master/net/smc)：**Shared Memory Communications**。它为应用程序提供标准 Socket 接口，但在底层利用 RDMA 或共享内存进行通信，极大地提升了模型参数同步的速度。

### 13. 极速网络数据路径 (XDP)
AI 模型分发和参数梯度交换需要榨干每一比特带宽。
*   [net/core/xdp.c](https://github.com/torvalds/linux/blob/master/net/core/xdp.c)：**Express Data Path**，利用 eBPF 在网卡驱动层直接处理报文。参与这里可以设计出极低延迟的 AI 集群通信组件。

### 14. 集群级拥塞算法调优 (`net/ipv4/tcp_congestion.c`)
万卡集群内，如何避免网络拥塞导致的训练停滞？
*   [net/ipv4/tcp_congestion.c](https://github.com/torvalds/linux/blob/master/net/ipv4/tcp_congestion.c)：通过在这里实现或优化拥塞控制算法（如 BBRv3 或专用 AI 算法），可以极大降低分布式训练中的长尾延迟（Tail Latency）。

---

## 第四梯队：极致性能观测与诊断 (Observability & Profiling)
*看不见的瓶颈是最大的浪费。*

### 15. 可观测性核心 (eBPF)
eBPF 正在成为 AI Infra 监控和调度的利器：
*   **eBPF (`kernel/bpf/`)**：用于 AI 集群的性能画像（Profiling）、网络流量整形以及定制化的 GPU 任务监控。

### 16. 统一硬件性能计数器 (Hardware PMU)
获取 AI 芯片内部的最底层指标（如算力利用率、Cache 命中率）。
*   [drivers/perf/](https://github.com/torvalds/linux/tree/master/drivers/perf)：除了通用 perf，这里包含了各厂商特定的 Performance Monitoring Unit 驱动。

### 17. 系统压力“急救包” (PSI)
在 AI 训练压榨系统极致性能时，如何防止系统由于内存或 IO 过载而瞬间“休克”。
*   [kernel/sched/psi.c](https://github.com/torvalds/linux/blob/master/kernel/sched/psi.c)：**Pressure Stall Information**。通过监控 CPU、内存、IO 的竞争压力，AI 训练框架可以实现更加智能的降速或 OOM 提前规避。

### 18. 数据访问画像与自动化调优 (DAMON)
大模型训练中哪些数据是“热点”？哪些可以挪到慢速内存？
*   [mm/damon/](https://github.com/torvalds/linux/tree/master/mm/damon)：**Data Access MONitoring**，内核原语级的数据访问监控框架。

---

## 第五梯队：高速数据加载 (Storage & Feeding)
*确保 GPU “粮食”供应永不断货。*

### 19. 数据加载加速 (`fs/io_uring.c`)
利用 io_uring 减少训练过程中海量小文件的 IO 系统调用开销。
*   [fs/io_uring.c](https://github.com/torvalds/linux/blob/master/fs/io_uring.c)：高性能异步 IO 框架。

### 20. 直接访问技术 (DAX)
在 AI 模型部署时，如何绕过内核 Page Cache，实现真正的“零拷贝”加载。
*   [fs/dax.c](https://github.com/torvalds/linux/blob/master/fs/dax.c)：直接访问持久化存储，大幅降低推理延迟。

### 21. AI 专属存储网关 (NVMe-oF Target)
将 NVMe 存储通过高性能网络直接透传给计算节点。
*   [drivers/nvme/target/](https://github.com/torvalds/linux/tree/master/drivers/nvme/target)：**NVMe over Fabrics (NVMe-oF)**。

---

## 第六梯队：虚拟化、安全与可靠性 (Cloud, Security & RAS)
*生产环境的最后一道防线。*

### 22. AI 机密计算 (Confidential AI)
保护昂贵的模型资产和敏感训练数据。
*   [drivers/virt/coco/](https://github.com/torvalds/linux/tree/master/drivers/virt/coco)：涉及 Intel TDX / AMD SEV。

### 23. GPU 硬件切分与虚拟化 (PCIe SR-IOV)
如何在虚拟化环境下，将一张物理 GPU 拆分成多个强隔离实例。
*   [drivers/pci/iov.c](https://github.com/torvalds/linux/blob/master/drivers/pci/iov.c)：**Single Root I/O Virtualization**。

### 24. 集群稳定性与纠错 (RAS & EDAC)
在大规模 GPU 集群中处理显存翻转（Bit Flip）。
*   [drivers/edac/](https://github.com/torvalds/linux/tree/master/drivers/edac)：内存错误检测与纠正。
*   [drivers/ras/](https://github.com/torvalds/linux/tree/master/drivers/ras)：内核可靠性管理框架。

---

## 其他重要基石模块 (Other Foundation Modules)
*   **内存优化**：[mm/hugetlb.c](https://github.com/torvalds/linux/blob/master/mm/hugetlb.c) (大页支持), [mm/khugepaged.c](https://github.com/torvalds/linux/blob/master/mm/khugepaged.c) (透明大页), [mm/compaction.c](https://github.com/torvalds/linux/blob/master/mm/compaction.c) (内存规整), [mm/zswap.c](https://github.com/torvalds/linux/blob/master/mm/zswap.c) (内存压缩)。
*   **计算原语**：[arch/x86/kernel/fpu/xstate.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/fpu/xstate.c) (AMX 支持), [drivers/fpga/](https://github.com/torvalds/linux/tree/master/drivers/fpga) (可重构 AI 算力), [drivers/dma/](https://github.com/torvalds/linux/tree/master/drivers/dma) (DMA 编排)。
*   **边缘/嵌入式**：[drivers/remoteproc/](https://github.com/torvalds/linux/tree/master/drivers/remoteproc) (NPU 管理), [drivers/rpmsg/](https://github.com/torvalds/linux/tree/master/drivers/rpmsg) (低延迟通信), [drivers/iommu/arm/arm-smmu-v3/](https://github.com/torvalds/linux/tree/master/drivers/iommu/arm/arm-smmu-v3) (缓存一致性)。
*   **调度与拓扑**：[kernel/sched/topology.c](https://github.com/torvalds/linux/blob/master/kernel/sched/topology.c) (NUMA 亲和性), [kernel/sched/fair.c](https://github.com/torvalds/linux/blob/master/kernel/sched/fair.c) (EAS 调度)。
*   **同步与共享**：[drivers/dma-buf/](https://github.com/torvalds/linux/tree/master/drivers/dma-buf) (零拷贝同步), [mm/memfd.c](https://github.com/torvalds/linux/blob/master/mm/memfd.c) (张量共享)。
*   **基础设施管理**：[drivers/fwctl/](https://github.com/torvalds/linux/tree/master/drivers/fwctl) (固件控制), [drivers/thermal/](https://github.com/torvalds/linux/tree/master/drivers/thermal) (能效温控), [kernel/irq/affinity.c](https://github.com/torvalds/linux/blob/master/kernel/irq/affinity.c) (中断平衡)。

---
**建议入门路径**：
1.  **算力核心**：从 `drivers/accel/` 了解新一代 AI 架构的设计思想。
2.  **调度突破**：关注 `sched_ext`，这是目前内核界将 eBPF 与 AI 结合最成功的案例。
3.  **内存破局**：研究 `CXL` 与 `Memory Tiering`，这是大模型时代解决存储成本与性能矛盾的终极方案。
