# 第二篇：CUDA 运行时与驱动：Memory Allocator、Graph、MPS

系列目录：[PyTorch → NVIDIA GPU 全链路深度解析](./README.md)

---

## 1. CUDA Runtime vs Driver API

### 1.1 两套 API 的设计哲学

CUDA 生态有两层 API：

```
┌─────────────────────────────────────────────────────────┐
│  用户代码 (PyTorch, TensorFlow, 自定义 CUDA kernel)     │
├─────────────────────────────────────────────────────────┤
│  CUDA Runtime API (libcudart.so)                        │
│  - cudaMalloc, cudaMemcpy, cudaLaunchKernel             │
│  - 简化语法: <<<grid, block>>> 启动 kernel              │
│  - 自动模块加载 (fat binary → cubin extraction)         │
│  - 隐式初始化 (lazy context creation)                   │
│  - 设备枚举: cudaGetDeviceCount, cudaSetDevice          │
├─────────────────────────────────────────────────────────┤
│  CUDA Driver API (libcuda.so)                           │
│  - cuMemAlloc, cuMemcpyHtoD, cuLaunchKernel             │
│  - 显式 context 管理: cuCtxCreate, cuCtxSetCurrent      │
│  - 模块加载: cuModuleLoad, cuModuleGetFunction          │
│  - JIT 编译: nvPTXCompiler (PTX → SASS)                 │
│  - 性能监控: cuProfilerStart, cuProfilerStop            │
├─────────────────────────────────────────────────────────┤
│  Kernel Mode Driver (nvidia.ko)                         │
│  - MMIO 映射设置                                        │
│  - 物理内存分配 (VRAM 页表映射)                         │
│  - GPU 控制寄存器访问                                   │
│  - Interrupt 处理                                       │
│  - DMA buffer 管理                                      │
├─────────────────────────────────────────────────────────┤
│  GPU Hardware                                            │
│  - SM (流式多处理器)                                    │
│  - HBM (高带宽内存)                                     │
│  - GPC (图形处理集群)                                   │
│  - NVLink / PCIe                                        │
└─────────────────────────────────────────────────────────┘
```

### 1.2 Runtime API 如何转发到 Driver API

Runtime API 只是 Driver API 的一层薄封装：

```cpp
// libcudart.so 内部实现 (简化)
// runtime_api.cpp → driver_api.cpp

cudaError_t cudaMalloc(void** devPtr, size_t size) {
    // Step 1: 确保 context 已创建
    __cudaInitDevice();

    // Step 2: 调用 Driver API
    CUresult err = cuMemAlloc((CUdeviceptr*)devPtr, size);

    // Step 3: 错误码转换
    return (err == CUDA_SUCCESS) ? cudaSuccess : __cudaTranslateError(err);
}

cudaError_t cudaMemcpy(void* dst, const void* src,
                        size_t count, cudaMemcpyKind kind) {
    // 根据 kind 选择对应的 Driver API
    if (kind == cudaMemcpyHostToDevice) {
        return cuMemcpyHtoD((CUdeviceptr)dst, src, count);
    } else if (kind == cudaMemcpyDeviceToHost) {
        return cuMemcpyDtoH(dst, (CUdeviceptr)src, count);
    } else if (kind == cudaMemcpyDeviceToDevice) {
        return cuMemcpyDtoD((CUdeviceptr)dst, (CUdeviceptr)src, count);
    } else {
        return cuMemcpyDefault(dst, src, count);
    }
}

cudaError_t cudaLaunchKernel(const void* func, dim3 gridDim,
                              dim3 blockDim, void** args,
                              size_t sharedMem, cudaStream_t stream) {
    return cuLaunchKernel((CUfunction)func,
                          gridDim.x, gridDim.y, gridDim.z,
                          blockDim.x, blockDim.y, blockDim.z,
                          sharedMem, stream, args, NULL);
}
```

### 1.3 Context 和 Module 管理

Driver API 要求显式的 context 管理，Runtime 则自动处理：

```cpp
// Driver API 方式: 一切显式
CUdevice device;
CUcontext context;
CUmodule module;
CUfunction kernel;

// Step 1: 初始化设备
cuInit(0);
cuDeviceGet(&device, 0);

// Step 2: 创建 context (与 CPU 线程绑定)
cuCtxCreate(&context, 0, device);

// Step 3: 加载 PTX/CUBIN 模块
cuModuleLoad(&module, "kernel.ptx");

// Step 4: 获取 kernel 函数指针
cuModuleGetFunction(&kernel, module, "my_kernel");

// Step 5: 启动 kernel
void* args[] = { &d_a, &d_b, &d_c, &n };
cuLaunchKernel(kernel, gridX, gridY, gridZ,
               blockX, blockY, blockZ,
               shmem, stream, args, NULL);

// Step 6: 清理
cuCtxDestroy(context);

// Runtime API 方式: 隐式处理
cudaMalloc(&d_a, size);         // 自动创建 context
my_kernel<<<grid, block>>>(d_a, d_b, d_c, n);  // 自动加载模块
cudaFree(d_a);                  // 隐式 context 管理
```

**Context 的线程绑定**：

```cpp
// 每个 CPU 线程只能有一个 current context
CUcontext ctx0, ctx1;
cuCtxCreate(&ctx0, 0, device0);
cuCtxCreate(&ctx1, 0, device1);

cuCtxSetCurrent(ctx0);   // 当前线程绑定到 device0
cudaMalloc(&ptr0, size); // 在 device0 上分配

cuCtxPushCurrent(ctx1);  // 压栈: 当前绑定到 device1
cudaMalloc(&ptr1, size); // 在 device1 上分配

cuCtxPopCurrent(&ctx1);  // 弹栈: 恢复绑定到 device0
```

### 1.4 Primary Context vs Custom Context

```cpp
// CUDA 4.0+ 引入了 primary context，由 Runtime 自动管理
// 一个设备只有一个 primary context，多个线程共享

// 获取 primary context (Driver API)
CUcontext primaryCtx;
cuDevicePrimaryCtxRetain(&primaryCtx, device);  // 引用计数+1
// ... 使用 ...
cuDevicePrimaryCtxRelease(device);               // 引用计数-1

// Runtime 方式: 完全隐式
cudaSetDevice(0);  // 自动 retain primary context
// ... 使用 ...
cudaDeviceReset(); // 释放 primary context
```

**Primary Context 的关键特性**：

- 所有 `cudaSetDevice()` 到同一设备的线程共享同一 context
- 同一设备上的内存分配、kernel 启动、stream 都在同一 context 内
- 不需要在多线程间传递 context（比 OpenCL 方便太多）

---

## 2. CUDA Memory Allocator

### 2.1 内存分配的三层结构

```
┌──────────────────────────────────────────────────────────────────┐
│  CUDA Memory Allocator Stack                                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Layer 1: User API                                               │
│  ─────────────────────                                           │
│  cudaMalloc(&ptr, size)            // 普通分配                   │
│  cudaMallocAsync(&ptr, size, stream) // 流序分配 (CUDA 11.2+)   │
│  cudaMallocManaged(&ptr, size)     // 统一内存 (UM)              │
│  cudaMallocHost(&ptr, size)        // 固定主机内存 (pinned)      │
│                                                                  │
│  Layer 2: CUDA Runtime Allocator                                 │
│  ────────────────────────────────                                │
│  - 维护一个设备端内存池 (device memory pool)                     │
│  - cudaMalloc: 小分配从池中切分，大分配向 Driver 请求            │
│  - cudaMallocAsync: 使用 stream-ordered allocation，无 CPU 同步  │
│  - 碎片整理: 合并相邻的空闲块                                    │
│                                                                  │
│  Layer 3: CUDA Driver / nvidia.ko                                │
│  ─────────────────────────────────                               │
│  - cuMemAlloc: 通过 ioctl 向 kernel 驱动请求 VRAM                │
│  - nvidia.ko: 管理 GPU 页表，分配物理 VRAM 页面                 │
│  - CUDA Virtual Memory Management: cuMemCreate, cuMemMap         │
│    (支持稀疏分配，按需提交)                                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 PyTorch 的 Caching Allocator

PyTorch 不直接使用 CUDA 的默认分配器，而是维护自己的缓存分配器：

```cpp
// c10/cuda/CUDACachingAllocator.cpp

class CUDACachingAllocator {
    // 核心数据结构: Block Pool
    // 大块设备内存被切分为 Block，按大小组织为 free list

    // 分配流程
    Block* malloc(size_t size) {
        // 1. 向上取整到 block size 单位
        size = round_size(size);  // 例如 512 bytes → alloc_size

        // 2. 在 free list 中查找
        auto pool = get_free_list(size);
        if (auto block = find_first_fit(pool, size)) {
            // 2a. 命中缓存: 直接返回
            split_block(block, size);
            return block;
        }

        // 3. 缓存未命中: 向 CUDA 申请大块内存
        if (!try_free_cached_blocks(size)) {
            // 如果有闲置的缓存块，先尝试释放
            // (cudaFree 归还给 CUDA Runtime)
        }

        // 4. 分配新的大块
        void* dev_ptr;
        cudaMalloc(&dev_ptr, alloc_size);  // 例如 20MB
        auto block = create_block(dev_ptr, alloc_size);
        add_to_pool(block);
        return split_block(block, size);
    }

    // 释放流程: 不立即归还 CUDA，而是缓存起来
    void free(Block* block) {
        // 1. 标记为 free
        block->allocated = false;

        // 2. 与前后相邻的 free block 合并
        block = try_merge(block);

        // 3. 插入 free list
        insert_free_block(block);

        // 4. 如果缓存过多，归还一些给 CUDA
        if (stats.allocated_bytes - stats.reserved_bytes > threshold) {
            release_cached_blocks();
        }
    }
};
```

**PyTorch Caching Allocator 的关键统计数据**：

```python
import torch

# 查看 PyTorch 的内存统计
print(torch.cuda.memory_stats())
# {
#   'allocated_bytes.all.current':   2.5 GB,   # 当前使用的
#   'reserved_bytes.all.current':    3.0 GB,   # 向 CUDA 申请的
#   'active_bytes.all.current':      2.5 GB,   # 在缓存中的活跃块
#   'inactive_split_bytes.all.current': 0.1 GB, # 可复用的碎片
# }

# allocated: 实际被 tensor 使用的
# reserved:  PyTorch 向 CUDA Runtime 申请的 ('cached')
# active:    reserved 中正在活跃使用的
# inactive:  reserved 中空闲但未释放的

# 典型情景:
# allocated < reserved: PyTorch 缓存了多余空间供后续使用
# allocated == reserved: 没有缓存，可能伴有频繁分配

print(torch.cuda.memory_summary())
# |===========================================================================|
# |                  PyTorch CUDA memory summary, device ID 0                 |
# |---------------------------------------------------------------------------|
# |            CUDA OOMs: 0            |        cudaMalloc retries: 0         |
# |===========================================================================|
# |        Metric         | Cur Usage  | Peak Usage | Tot Alloc  | Tot Freed  |
# |---------------------------------------------------------------------------|
# | Allocated memory      |   2560 MB  |   5120 MB  |  61440 MB  |  58880 MB  |
# |       from large pool |   2304 MB  |   4608 MB  |  55296 MB  |  52992 MB  |
# |       from small pool |    256 MB  |    512 MB  |   6144 MB  |   5888 MB  |
# |---------------------------------------------------------------------------|
# | Active memory         |   2560 MB  |   5120 MB  |  61440 MB  |  58880 MB  |
# | Requested memory      |   2560 MB  |   5120 MB  |  61440 MB  |  58880 MB  |
# | GPU reserved memory   |   3072 MB  |   6144 MB  |  61440 MB  |  58368 MB  |
# |      largest free block: 1024 MB  |                     |              |
# |---------------------------------------------------------------------------|
```

### 2.3 CUDA Stream-Ordered Allocation (cudaMallocAsync)

CUDA 11.2 引入了流序分配，彻底改变了分配语义：

```cpp
// 传统分配: 隐式全局同步
for (int i = 0; i < N; i++) {
    cudaMalloc(&ptr[i], size);  // ← 内部可能触发 cudaDeviceSynchronize!
    kernel<<<grid, block, 0, stream>>>(ptr[i], ...);
}

// 流序分配: 按 stream 顺序分配，无需 CPU 同步
cudaMemPool_t pool;
cudaDeviceGetMemPool(&pool, device);

for (int i = 0; i < N; i++) {
    // 分配在 stream 上排队，不阻塞 CPU
    cudaMallocAsync(&ptr[i], size, stream);
    kernel<<<grid, block, 0, stream>>>(ptr[i], ...);
    // 释放也在 stream 上排队
    cudaFreeAsync(ptr[i], stream);
}

// 性能对比:
// 传统: 100x 分配 = 100 × 50µs = 5ms
// 流序: 100x 分配 = 20µs  (所有分配合并为批处理)
```

**cudaMallocAsync 的内部实现**：

```cpp
// 流序分配使用内存池 (Memory Pool)
// 每个 pool 维护一个待办列表，按 stream 顺序处理

cudaMallocAsync(void** ptr, size_t size, cudaStream_t stream) {
    CUmemPoolPtrExportData handle;
    cuMemAllocFromPoolAsync((CUdeviceptr*)ptr, size, pool, stream);
    // 内部:
    // 1. 在 pool 的 free list 中查找
    // 2. 如果没有可用块，向 cuMemAllocFromPoolAsync 请求新内存
    // 3. 将分配操作插入 stream 的 pending queue
    // 4. stream 上的后续 kernel 自动等待该分配完成
    // 5. 没有 cudaDeviceSynchronize，没有 CPU 阻塞!
}
```

### 2.4 内存池属性调优

```cpp
// 获取和设置内存池属性
cudaMemPool_t pool;
cudaDeviceGetMemPool(&pool, device);

// 设置 release threshold: 当缓存的空闲内存超过此比例时
// 自动归还给操作系统
uint64_t threshold = UINT64_MAX;  // 0 = always release, UINT64_MAX = never
cudaMemPoolSetAttribute(pool, cudaMemPoolAttrReleaseThreshold,
                        &threshold);

// 查询当前池的使用情况
cudaMemPoolProps props;
cudaDeviceGetMemPoolProps(&props, pool);
// props.allocated  = 当前从池中分配的总量
// props.reserved   = 池中保留的总量（含空闲）
// props.free       = 池中空闲的总量
// props.used       = 实际使用的总量
```

### 2.5 碎片化与内存分配器设计

```
小块分配的碎片化问题:

初始状态:       [================================================]  (1GB free)
              ptr=0x0                                      0x40000000

分配 A (256MB): [AAAAAAAAAAAAAAAAAAAAAAAAA===================]
              ptr=0x0          0x10000000             0x40000000

分配 B (128MB): [AAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBB=======]
              ptr=0x0          0x10000000  0x18000000 0x40000000

释放 A:         [................................B===============]
                       256MB hole!                  1GB - 128MB free
                                                     但无法分配 400MB

解决碎片化的策略:
────────────────────────────────────────

1. Buddy Allocator:
   - 所有块大小都是 2 的幂
   - 分配: 找到 ≥ 请求大小的最小区块，递归二分
   - 释放: 与相邻 buddy 合并
   - 优势: 快速合并，减少外部碎片
   - 劣势: 内部碎片 (分配 33KB → 分配 64KB)

2. Slab Allocator:
   - 为常用大小预分配 slab
   - 每种大小维护一个自由链表
   - 例如: 256B, 512B, 1KB, 2KB, 4KB slabs
   - 适合: 大量同尺寸小对象 (tensor metadata, autograd node)

3. PyTorch 的混合策略:
   - large blocks (>1MB):  直接 cuMemAlloc / 两种大小的 pool
   - small blocks (<1MB):  slab-like caching
   - 流式分配:             cudaMallocAsync for inference
```

---

## 3. CUDA Graph

### 3.1 概念：将一系列操作捕获为图

CUDA Graph 是 CUDA 10 引入的革命性特性，它将 GPU 操作记录为一个 DAG（有向无环图），之后可以一次性重放整个图。

```
没有 CUDA Graph:
  CPU:  launch(A) ── wait ── launch(B) ── wait ── launch(C) ── wait ── launch(D)
  GPU:  [A         ][ B     ][  C      ][   D     ]
        total latency = 4 × kernel_time + 4 × launch_overhead

使用 CUDA Graph:
  CPU:  [capture A,B,C,D] ──────────── [launch graph] ──────────── [launch graph]
  GPU:  [A][B/C][D        ][A][B/C][D        ][A][B/C][D        ]
        total latency = kernel_time + 1 × launch_overhead (per iteration)

  每个 kernel launch 的开销: 10-100 µs
  Graph launch 的开销: 1-2 µs
  节省: ~90% 的 CPU 端 launch 开销
```

### 3.2 CUDA Graph API 完整生命周期

```cpp
// ===== Phase 1: 捕获 =====
cudaStream_t captureStream;
cudaStreamCreate(&captureStream);

cudaStreamBeginCapture(captureStream, cudaStreamCaptureModeGlobal);

// 在捕获期间执行的所有 CUDA 操作都被记录
for (int i = 0; i < n_layers; i++) {
    // 这些 kernel 和 memcpy 不会被立即执行
    // 而是记录为图中的节点
    linear_kernel<<<grid, block, 0, captureStream>>>(d_input, d_weight, d_output);
    relu_kernel<<<grid, block, 0, captureStream>>>(d_output, n);
}

cudaGraph_t graph;
cudaStreamEndCapture(captureStream, &graph);

// ===== Phase 2: 实例化 =====
// 实例化会分配所有静态资源，优化图结构
cudaGraphExec_t instance;
cudaGraphInstantiate(&instance, graph, NULL, NULL, 0);

// ===== Phase 3: 执行 (可以重复多次) =====
for (int iter = 0; iter < n_iterations; iter++) {
    // 单次 launch 替换了之前所有的 kernel launch
    cudaGraphLaunch(instance, captureStream);
}

// ===== Phase 4: 更新 (可选, CUDA 12+) =====
// 无需重新捕获，直接更新图中的参数 (如新的输入地址)
cudaGraphExecUpdate(instance, graph, &updateResult);

// ===== Phase 5: 清理 =====
cudaGraphExecDestroy(instance);
cudaGraphDestroy(graph);
cudaStreamDestroy(captureStream);
```

### 3.3 PyTorch 中的 CUDA Graph 使用

```python
import torch

# PyTorch 的 CUDA Graph wrapper
g = torch.cuda.CUDAGraph()

# Step 1: 第一次迭代: 积累图的形状和内存模式
s = torch.cuda.Stream()
with torch.cuda.stream(s):
    # warmup: 确保分配器已缓存足够内存
    output = model(input)
    loss.backward()
torch.cuda.synchronize()

# Step 2: 捕获图
with torch.cuda.graph(g, stream=s):
    output = model(input)   # forward 操作被记录
    loss = criterion(output, target)
    loss.backward()         # backward 操作被记录

# Step 3: 重放图
for epoch in range(n_epochs):
    for batch in dataloader:
        # 更新输入
        input.copy_(batch_data)   # ← 必须是 in-place copy

        # 重放整个 forward+backward
        g.replay()

        # optimizer step (不能在图中，因为有 Python 代码)
        optimizer.step()
        optimizer.zero_grad()
```

**PyTorch CUDA Graph 的实现细节**：

```cpp
// torch/csrc/cuda/CUDAGraph.cpp

void CUDAGraph::capture_begin() {
    // 确保所有池中的内存都可用
    // 因为图中只能引用固定的内存地址
    cudaStreamBeginCapture(stream_, cudaStreamCaptureModeThreadLocal);

    // 记录当前所有 malloc 的指针
    // 图重放时, 这些指针必须指向有效内存
    record_mem_pool_state();
}

void CUDAGraph::capture_end() {
    cudaGraph_t graph;
    cudaStreamEndCapture(stream_, &graph);

    // 实例化图: 分配内部内存, 验证节点
    cudaGraphInstantiate(&graph_exec_, graph, &error_log, NULL, 0);

    cudaGraphDestroy(graph);
}

void CUDAGraph::replay() {
    // 更新输入/输出指针 (如果图支持更新)
    if (has_graph_exec_update_) {
        cudaGraphExecUpdate(graph_exec_, updated_graph_, ...);
    }

    // 单次调用, 重放所有 kernel
    cudaGraphLaunch(graph_exec_, stream_);
}
```

### 3.4 CUDA Graph 的限制与应对

```python
# 限制 1: 不支持动态形状
# 错误: 每次迭代不同 batch size
with torch.cuda.graph(g):
    output = model(input)  # input: batch=32
g.replay()  # 重放时 input 必须是 batch=32

# 解决: padding 到固定形状 + 使用 mask
max_seq_len = 512
padded_input = pad_sequence(inputs, max_seq_len)
mask = create_mask(inputs, max_seq_len)
with torch.cuda.graph(g):
    output = model(padded_input, mask=mask)

# 限制 2: 不允许 CPU-GPU 同步
# 错误代码:
with torch.cuda.graph(g):
    x = layer1(input)
    y = layer2(input)
    torch.cuda.synchronize()  # ← 不允许! 捕获时禁止同步
    z = x + y

# 解决: 使用 CUDA event 替代 host sync
with torch.cuda.graph(g):
    x = layer1(input)
    event.record()
    event.wait()
    y = layer2(input)
    z = x + y

# 限制 3: 不支持 cudaMalloc / cudaFree inside capture
# 图要求所有内存地址在捕获时就确定
# 解决: 使用 PyTorch caching allocator + warmup
for _ in range(3):
    model(input)  # 让 allocator 缓存足够内存
# 然后捕获 (此时没有 cudaMalloc 调用)

# 限制 4: 不支持 host callbacks
# 错误:
with torch.cuda.graph(g):
    kernel<<<...>>>(...)
    cudaLaunchHostFunc(stream, my_callback, data)  # 不允许

# 限制 5: 不能嵌套 CUDA graph capture
# cudaStreamBeginCapture 不能在被捕获的 stream 上调用
```

### 3.5 CUDA Graph 适合的场景

```
非常适合:
  ✅ Transformer 推理 (固定 seq_len)
  ✅ ResNet/CNN 推理 (固定输入尺寸)
  ✅ 自动微分训练 (固定 batch size)
  ✅ Real-time 推理服务 (延迟敏感)

不太适合:
  ❌ 动态 batch size 的训练
  ❌ 条件分支很多的算子 (if/else based on data)
  ❌ 稀疏矩阵操作 (non-deterministic memory pattern)
  ❌ Debug/profiling 阶段 (图中难以插入打印和断点)
```

---

## 4. MPS（Multi-Process Service）

### 4.1 问题：多进程共享 GPU 的挑战

```
没有 MPS:
─────────
  Process A: [kernel_A1][kernel_A2]-----------[kernel_A3]
  Process B: --------[kernel_B1]-----------[kernel_B2]
  Process C: ----------------[kernel_C1][kernel_C2]

  GPU 执行:  [A1][B1?]← B1 必须等 A1 完成 (context switch)
             因为每个进程有自己的 context
             所有 context 的 kernel 被提交到同一硬件队列
             但 context switch 有开销 (flush L1/L2, 重新加载页表)

使用 MPS:
────────
  MPS Server (daemon):
  ┌─────────────────────────────────────────────┐
  │ 接收所有客户端进程的 kernel launch           │
  │ 按公平/优先级策略调度                        │
  │ 消除 context switch 开销                    │
  │ 共享 GPU 资源 (SM, 内存带宽)                │
  └─────────────────────────────────────────────┘
     ↑           ↑           ↑
  Process A   Process B   Process C

  GPU 执行:  [A1][B1][A2][C1][B2][A3][C2]  (交错, 无 context switch)
```

### 4.2 MPS 的配置与启动

```bash
# Step 1: 查看 MPS 是否可用
nvidia-smi   # 检查 driver version ≥ 支持 MPS

# Step 2: 设置 MPS daemon
# 方法 A: 手动启动
export CUDA_VISIBLE_DEVICES=0
nvidia-cuda-mps-control -d   # 启动 MPS daemon

# 方法 B: 作为 systemd 服务
sudo systemctl start nvidia-mps

# Step 3: 验证 MPS 状态
nvidia-smi -q | grep -A 10 "Processes"
# 如果是 MPS, 进程显示为 "[MPS]"

echo quit | nvidia-cuda-mps-control  # 手动停止

# Step 4: 限制 MPS 中每个进程的资源使用
export CUDA_MPS_ACTIVE_THREAD_PERCENTAGE=50  # 最大使用 50% SM
export CUDA_MPS_PINNED_DEVICE_MEM_LIMIT=4GB  # 最多使用 4GB 显存
```

### 4.3 MPS 的内部架构

```
┌─────────────────────────────────────────────────────────────────┐
│  MPS Client (用户进程)                                          │
├─────────────────────────────────────────────────────────────────┤
│  libcuda.so (MPS client library)                                │
│    │                                                            │
│    ├─ CUDA API 调用 (cuLaunchKernel, cuMemAlloc...)             │
│    ├─ 编码为 MPS protocol message                               │
│    ├─ 通过 Unix domain socket 发送给 MPS server                  │
│    └─ 等待 server 确认 (或异步返回)                             │
└────────────────────┬────────────────────────────────────────────┘
                     │ Unix Domain Socket
┌────────────────────┴────────────────────────────────────────────┐
│  MPS Server (nvidia-cuda-mps-control daemon)                    │
├─────────────────────────────────────────────────────────────────┤
│  Client Manager:                                                │
│    - 管理所有客户端连接                                          │
│    - 追踪每个客户端的资源配额                                    │
│    - 分配 GPU context 给客户端                                  │
│                                                                 │
│  Sub-Contexts:                                                  │
│    ┌──────────┐  ┌──────────┐  ┌──────────┐                    │
│    │ Client A  │  │ Client B  │  │ Client C  │                    │
│    │ context   │  │ context   │  │ context   │                    │
│    │ GPU: 40%  │  │ GPU: 30%  │  │ GPU: 30%  │                    │
│    └─────┬─────┘  └─────┬─────┘  └─────┬─────┘                    │
│          └──────────────┼──────────────┘                          │
│                         ▼                                        │
│                    GPU Work Scheduler                             │
│                    (round-robin / priority)                       │
│                         │                                        │
│                         ▼                                        │
│                  cuLaunchKernel (single context)                  │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────┴────────────────────────────────────────────┐
│  GPU Hardware                                                    │
│  - 所有 clients 的 kernels 在同一个 context 内交错执行            │
│  - 无 context switch 开销                                       │
│  - SM 利用率更高                                                │
└──────────────────────────────────────────────────────────────────┘
```

### 4.4 MIG (Multi-Instance GPU) vs MPS

```
┌──────────────────────────┬────────────────────────────┐
│  MIG (硬件分区)           │  MPS (软件时间分片)         │
├──────────────────────────┼────────────────────────────┤
│  硬件级别隔离             │  用户级隔离                 │
│  支持的 GPU: A100, A30,  │  支持所有 NVIDIA GPU        │
│    H100, H200             │                            │
│  每个 MIG instance 有:    │  所有进程共享同一 GPU       │
│    - 专用 SM              │   无专用硬件资源            │
│    - 专用 L2 cache slice  │   L2 cache 共享，可能竞争  │
│    - 专用内存带宽 slice   │   内存带宽共享              │
│    - 专用显存             │   显存共享 (有配额限制)    │
│  错误隔离: 强             │  错误隔离: 弱              │
│  故障域: 独立             │  故障域: 共享              │
│  配置: 静态 (重启需求)    │  配置: 动态                │
└──────────────────────────┴────────────────────────────┘

A100 MIG 配置示例:
  nvidia-smi mig -cgi 9,9,14    # 创建 3 个 GPU instance
  # layout: 2 instances of 20GB, 1 instance of 40GB
  # 每个 instance 有独立的 SM, L2 和 内存

  nvidia-smi mig -cci 0,0       # 在 GPU instance 上创建 compute instance
  # 每个 compute instance 对应用户可见的 CUDA 设备

  nvidia-smi mig -dci           # 删除所有 MIG 配置
```

### 4.5 MPS 与 CUDA_VISIBLE_DEVICES 的交互

```bash
# 场景: 4 GPU 系统, 运行 8 个训练任务

# 方案 1: MPS 共享
export CUDA_VISIBLE_DEVICES=0
nvidia-cuda-mps-control -d
# Task 1-2 都在 GPU 0 上运行, MPS 公平调度

# 方案 2: MIG 分区 (A100)
sudo nvidia-smi mig -cgi 19,19,19,19,19,19,19
# 将一个 A100 80GB 切分为 7 个 MIG instance (各 ~10GB)
# 每个 task 有独立的 GPU 实例

# 方案 3: 组合使用
export CUDA_VISIBLE_DEVICES=0,1
# GPU 0 用 MPS 跑 2 个推理任务
# GPU 1 用 MIG 跑 3 个训练任务 (硬件隔离)
```

---

## 5. CUDA IPC（进程间通信）

### 5.1 共享 GPU 内存

```cpp
// ===== Process A (Producer) =====
cudaIpcMemHandle_t handle;
void* d_data;
cudaMalloc(&d_data, size);

// 先执行一些操作
kernel<<<grid, block>>>(d_data, ...);
cudaDeviceSynchronize();

// 导出内存句柄: 获取 IPC 可共享的引用
cudaIpcGetMemHandle(&handle, d_data);

// 通过任意 IPC 机制发送 handle 给 Process B
// (例如: shared memory, pipe, TCP, etc.)
send_to_process_B(&handle, sizeof(handle));

// ===== Process B (Consumer) =====
cudaIpcMemHandle_t handle;
receive_from_process_A(&handle, sizeof(handle));

void* d_shared;
cudaIpcOpenMemHandle(&d_shared, handle,
                     cudaIpcMemLazyEnablePeerAccess);
// d_shared 现在指向与 Process A 相同的物理 GPU 内存!

// Process B 可以读取
kernel_read<<<grid, block>>>(d_shared, d_output, size);

// 注意: Process B 不能 free d_shared (由 Process A 负责)

cudaIpcCloseMemHandle(d_shared);  // 释放 IPC 引用
```

### 5.2 IPC 的底层实现: dma-buf

Linux 内核中, GPU 内存共享通过 `dma-buf` 框架实现:

```
Process A                        Linux Kernel              Process B
    │                                │                       │
    │ cudaIpcGetMemHandle()          │                       │
    ├───────────────────────────────►│                       │
    │                                │ export dma_buf        │
    │                                │ ← 获取 file descriptor│
    │                                │                       │
    │ 收到 opaque handle (含 fd)     │                       │
    │──── send via IPC ──────────────│── handle ────────────►│
    │                                │                       │
    │                                │                   cudaIpcOpenMemHandle()
    │                                │◄───────────────────────┤
    │                                │ import dma_buf (fd)    │
    │                                │ ← 映射到 Process B 的  │
    │                                │    GPU 虚拟地址空间    │
    │                                │                       │
    │  GPU Physical Memory           │                       │
    │  ┌─────────────────┐           │                       │
    │  │ Original VA → PA│           │                       │
    │  └─────────────────┘           │                       │
    │                                │  ┌─────────────────┐  │
    │                                │  │ New VA → same PA│  │
    │                                │  └─────────────────┘  │
```

### 5.3 实际应用场景

```python
# 场景 1: 推理服务, 多个 worker 共享模型权重
# Process 1: 加载模型
model = load_huge_model()  # 加载到 GPU 0
# 导出所有权重的 IPC handle
handles = [torch.cuda.ipc.collect(model)]

# Process 2-8: 接收 handles, 映射到自己的地址空间
model = torch.cuda.ipc.consume(handles)
# 所有进程共享同一份 GPU 权重, 节省 VRAM
# 8 workers × 40GB 模型 → 只需 40GB (而不是 320GB)

# 场景 2: 多进程训练 (不推荐, 用 NCCL 更好)
# 场景 3: 生产者-消费者流水线
# GPU 0: 数据预处理 → GPU 1: 模型推理
```

---

## 6. 完整体系架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         用户层 (User Space)                         │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────────┐│
│  │  PyTorch     │  │  TensorFlow  │  │  自定义 CUDA 程序            ││
│  │  (Python/C++)│  │  (Python)    │  │  (C/C++)                     ││
│  └──────┬───────┘  └──────┬───────┘  └──────────────┬───────────────┘│
│         │                 │                         │                │
│  ┌──────┴─────────────────┴─────────────────────────┴──────────────┐│
│  │  CUDA Runtime API (libcudart.so)                                ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐    ││
│  │  │ cudaMalloc   │  │ cudaMemcpy   │  │ cudaLaunchKernel   │    ││
│  │  └──────┬───────┘  └──────┬───────┘  └────────┬───────────┘    ││
│  │         │                 │                    │                ││
│  │  ┌──────┴─────────────────┴────────────────────┴────────────┐  ││
│  │  │  CUDA Driver API (libcuda.so)                            │  ││
│  │  │  ┌────────────┐  ┌────────────┐  ┌──────────────────┐  │  ││
│  │  │  │ cuMemAlloc │  │ cuMemcpy.. │  │ cuLaunchKernel   │  │  ││
│  │  │  └─────┬──────┘  └─────┬──────┘  └───────┬──────────┘  │  ││
│  │  │        │               │                  │             │  ││
│  │  │  ┌─────┴───────────────┴──────────────────┴─────────┐   │  ││
│  │  │  │  CUDA Memory Allocator                           │   │  ││
│  │  │  │  - Caching (PyTorch internal)                    │   │  ││
│  │  │  │  - Stream-ordered (cudaMallocAsync)              │   │  ││
│  │  │  │  - Pool management (cudaMemPool)                 │   │  ││
│  │  │  └────────────────────┬─────────────────────────────┘   │  ││
│  │  │                       │                                 │  ││
│  │  │  ┌────────────────────┴─────────────────────────────┐   │  ││
│  │  │  │  CUDA Graph API                                  │   │  ││
│  │  │  │  - cudaStreamBeginCapture / EndCapture            │   │  ││
│  │  │  │  - cudaGraphInstantiate / Launch                  │   │  ││
│  │  │  └────────────────────┬─────────────────────────────┘   │  ││
│  │  │                       │                                 │  ││
│  │  │  ┌────────────────────┴─────────────────────────────┐   │  ││
│  │  │  │  MPS (Multi-Process Service)                     │   │  ││
│  │  │  │  - Client/Server via Unix Domain Socket          │   │  ││
│  │  │  │  - Round-robin scheduler                         │   │  ││
│  │  │  └────────────────────┬─────────────────────────────┘   │  ││
│  │  └───────────────────────┼─────────────────────────────────┘  ││
│  └──────────────────────────┼────────────────────────────────────┘│
└─────────────────────────────┼──────────────────────────────────────┘
                              │ ioctl (device file: /dev/nvidia*)
┌─────────────────────────────┼──────────────────────────────────────┐
│                    内核层 (Kernel Space)                            │
│                             │                                       │
│  ┌──────────────────────────┴────────────────────────────────────┐ │
│  │  nvidia.ko (NVIDIA Kernel Module)                             │ │
│  │                                                               │ │
│  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐  │ │
│  │  │ 内存管理        │  │ GPU 控制       │  │ Context 管理    │  │ │
│  │  │ - VRAM 页表     │  │ - MMIO 寄存器  │  │ - GPU context   │  │ │
│  │  │ - dma-buf      │  │ - push buffer  │  │ - channel      │  │ │
│  │  │ - import/export│  │ - interrupt    │  │ - TSG          │  │ │
│  │  └────────────────┘  └────────────────┘  └────────────────┘  │ │
│  │                                                               │ │
│  │  ┌────────────────────────────────────────────────────────┐   │ │
│  │  │  IOMMU (可选)                                          │   │ │
│  │  │  - GPU 地址 → 物理地址映射                              │   │ │
│  │  │  - 防止 GPU DMA 越界访问                               │   │ │
│  │  └────────────────────────────────────────────────────────┘   │ │
│  └──────────────────────┬─────────────────────────────────────────┘ │
│                         │                                           │
└─────────────────────────┼───────────────────────────────────────────┘
                          │ PCIe / NVLink
┌─────────────────────────┼───────────────────────────────────────────┐
│                    GPU 硬件 (Hardware)                               │
│                         │                                           │
│  ┌──────────────────────┴──────────────────────────────────────┐   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │   │
│  │  │ GPC 0    │  │ GPC 1    │  │ ...      │  │ GPC N    │   │   │
│  │  │ SM0..SMm │  │ SM0..SMm │  │          │  │ SM0..SMm │   │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │   │
│  │       │              │              │              │        │   │
│  │  ┌────┴──────────────┴──────────────┴──────────────┴────┐  │   │
│  │  │  L2 Cache (shared across all GPCs)                   │  │   │
│  │  └──────────────────────┬───────────────────────────────┘  │   │
│  │                         │                                  │   │
│  │  ┌──────────────────────┴───────────────────────────────┐  │   │
│  │  │  HBM (High Bandwidth Memory)                         │  │   │
│  │  │  - H100: 80GB, 3.35 TB/s                             │  │   │
│  │  │  - A100: 40/80GB, 2 TB/s                             │  │   │
│  │  │  - H200: 141GB, 4.8 TB/s                             │  │   │
│  │  └──────────────────────────────────────────────────────┘  │   │
│  │                                                             │   │
│  │  ┌──────────────────────────────────────────────────────┐   │   │
│  │  │  NVSwitch / NVLink                                   │   │   │
│  │  │  - NVLink 4.0: 900 GB/s per GPU (bidirectional)      │   │   │
│  │  │  - NVSwitch: 8-GPU all-to-all at full bandwidth      │   │   │
│  │  └──────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 7. 实用调优与诊断

### 7.1 内存分配分析

```python
# PyTorch 内存快照
import torch

# 记录当前内存分配状态
snapshot = torch.cuda.memory_snapshot()

# 查看哪些 tensor 占用了最多的内存
for seg in snapshot:
    if seg['allocated']:
        print(f"segment: {seg['address']} size: {seg['total_size']/1e6:.1f}MB")

# 内存分析工具
# 使用 memory_profiler 追踪 Python 侧的分配
# pip install memory_profiler
from memory_profiler import profile

@profile
def train_step():
    output = model(input)    # 查看这里分配了多少
    loss.backward()           # 查看 backward 的额外分配
```

### 7.2 Dump CUDA API 调用

```bash
# 使用 CUPTI (CUDA Profiling Tools Interface)
# 记录所有 cudaMalloc / cudaFree 调用
nvprof --print-api-trace python train.py

# 使用 NVIDIA Nsight Systems
nsys profile --trace=cuda,osrt python train.py
# 生成 .qdrep 文件, 用 Nsight Systems GUI 查看时间线

# 使用 CUDA_LAUNCH_BLOCKING 串行化所有 kernel 启动
# 用于 debug: 让异步错误变成同步错误
CUDA_LAUNCH_BLOCKING=1 python train.py
```

### 7.3 CUDA Graph 调试

```python
# 检查 CUDA Graph 是否成功捕获
torch.cuda.synchronize()
with torch.cuda.graph(g):
    y = model(x)
torch.cuda.synchronize()

# 查看 graph 的内存使用
print(torch.cuda.memory_stats())

# Debug 模式: 逐步捕获每个 layer
layers = []
for i, layer in enumerate(model.children()):
    g = torch.cuda.CUDAGraph()
    with torch.cuda.graph(g):
        # 捕获单个 layer
        x = layer(x)
    layers.append((layer, g))
    # 重放测试
    for _ in range(3):
        g.replay()
```

### 7.4 检测 OOM 根因

```python
# PyTorch OOM 调试
import os
os.environ['PYTORCH_CUDA_ALLOC_CONF'] = 'expandable_segments:True'

# 启用后, allocator 按需扩展 segment, 减少碎片
# 其他选项:
# max_split_size_mb:512      # 最大的拆分块大小 (MB)
# garbage_collection_threshold:0.8  # 触发 GC 的阈值
# roundup_power2_divisions:16 # 2 的幂次对齐 (减少碎片)

# 使用 torch.cuda.OutOfMemoryError 捕获
try:
    output = model(input)
except torch.cuda.OutOfMemoryError as e:
    print(f"OOM! {e}")
    # 打印当前内存使用
    print(torch.cuda.memory_summary())
```

---

## 8. 关键源码与文档索引

| 主题 | 源码/位置 |
|------|----------|
| CUDA Runtime API 头文件 | `/usr/local/cuda/include/cuda_runtime_api.h` |
| CUDA Driver API 头文件 | `/usr/local/cuda/include/cuda.h` |
| PyTorch Caching Allocator | `c10/cuda/CUDACachingAllocator.cpp` |
| PyTorch CUDA Graph | `torch/csrc/cuda/CUDAGraph.cpp` |
| CUDA SDK Memory Management | `CUDA/samples/common/inc/helper_cuda.h` |
| CUDA Graph 示例 | `CUDA/samples/0_Simple/simpleCudaGraphs/` |
| MPS 文档 | `NVIDIA MPS User Guide` |
| CUDA IPC 示例 | `CUDA/samples/0_Simple/simpleIPC/` |
| nvidia.ko 源码 | `NVIDIA/open-gpu-kernel-modules` (GitHub) |

---

## 下一篇文章

[第三篇：加速库三剑客：cuBLAS、cuDNN、NCCL](./03-cublas-cudnn-nccl.md)
