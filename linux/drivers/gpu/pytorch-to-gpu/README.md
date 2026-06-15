# PyTorch → NVIDIA GPU 全链路深度解析

> 📌 本系列讲的是 PyTorch 调用到 NVIDIA GPU 硬件之间的完整软件硬件桥接栈
> 与已有 [nvidia-svm 内核源码系列](../nvidia-svm/README.md) 互补

---

本系列与已有的 [nvidia-svm 内核源码系列](../nvidia-svm/README.md) 互补：

| 本系列 | nvidia-svm 系列 |
|--------|----------------|
| PyTorch dispatcher、CUDA runtime、cuBLAS、vLLM | HMM 缺页处理、VRAM 管理、GPU 调度器 |
| 软件层怎么调用 GPU | GPU 在内核中怎么被管理 |
| CUDA kernel launch 参数封装 | CUDA kernel 在内核中怎么被调度执行 |

两条线在 **CUDA kernel launch** 处交汇——本系列讲上层的算子分发到 launch，nvidia-svm 系列讲 launch 之后的内核路径。

## 文章目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [PyTorch → CUDA：算子分发、流与事件](./01-pytorch-dispatch.md) | `model.forward()` → ATen dispatcher → CUDA kernel launch |
| 2 | [CUDA 运行时与驱动：Memory Allocator、Graph、MPS](./02-cuda-runtime.md) | cuMemAlloc、CUDA Graph capture/replay、多进程服务 |
| 3 | [加速库三剑客：cuBLAS、cuDNN、NCCL](./03-cublas-cudnn-nccl.md) | GEMM over Tensor Core、卷积算法选择、All-Reduce ring/tree |
| 4 | [vLLM 推理引擎：PagedAttention、Continuous Batching](./04-vllm-inference.md) | KV Cache 虚拟页管理、动态组batch、为什么快 20x |
| 5 | [GPU 硬件架构：SM、Warp、Tensor Core](./05-gpu-arch.md) | SM 调度、Warp scheduler、Tensor Core MMA、H100 TMA |
| 6 | [完整管线图：从 PyTorch 到 GPU 硬件](./06-full-pipeline.md) | 端到端时序、每层的调用链和耗时 |

## 前置知识

- PyTorch 的基本使用（`model.forward()`、`loss.backward()`）
- CUDA 基本概念（kernel、grid、block、shared memory）
- 内核源码系列的基础是可选的，但了解 HMM 和 GPU scheduler 会加深理解

## 完整管线总览

```
PyTorch (model.forward)
  → ATen dispatcher (native function dispatch)
    → cuBLAS / cuDNN (加速库选择)
      → CUDA Runtime (cudaLaunchKernel, cuMemAlloc)
        → CUDA Driver (ioctl, kernel params)
          → NVIDIA GPU 硬件
            → SM (Streaming Multiprocessor)
              → Warp Scheduler → Tensor Core (MMA 指令)
                → VRAM (HBM3)
```
