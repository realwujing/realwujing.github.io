# 第一篇：PyTorch → CUDA：算子分发、流与事件

系列目录：[PyTorch → NVIDIA GPU 全链路深度解析](./README.md)

---

## 1. 从 `model.forward()` 到 ATen

### 1.1 Linear Layer 的 Python 前端

当你在训练循环中写下：

```python
output = model(x)  # x: torch.Tensor, shape=(batch, in_features)
```

Python 层的调用链如下：

```
model.forward(x)
  └─ self.linear(x)                          # nn.Linear.forward()
       └─ F.linear(input, self.weight, self.bias)
            └─ input @ weight.T + bias       # 运算符重载
                 └─ torch.addmm(bias, input, weight.T)
                      └─ aten::addmm          # ATen dispatch
```

让我们看看 `F.linear` 的实际实现（简化自 PyTorch 源码 `torch/nn/functional.py`）：

```python
def linear(input: Tensor, weight: Tensor, bias: Optional[Tensor] = None) -> Tensor:
    if input.dim() == 2 and bias is not None:
        # 融合 addmm → 单个 CUDA kernel
        return torch.addmm(bias, input, weight.t())
    output = input.matmul(weight.t())
    if bias is not None:
        output += bias
    return output
```

对于 3D 输入（如 Transformer 的 `batch × seq_len × hidden`），则会进入 `torch.baddbmm` 或显式展开+`matmul`：

```python
def linear(input, weight, bias=None):
    if input.dim() == 3:
        return torch.baddbmm(bias, input, weight.transpose(-2, -1))
    return torch.addmm(bias, input, weight.t())
```

### 1.2 ATen Native Function Dispatch

`torch.addmm` 是一个 **ATen 原生函数**（native function），它的声明在 `aten/src/ATen/native/native_functions.yaml` 中：

```yaml
- func: addmm(Tensor self, Tensor mat1, Tensor mat2, *, Scalar beta=1, Scalar alpha=1) -> Tensor
  dispatch:
    CPU: addmm_cpu
    CUDA: addmm_cuda
    SparseCPU: addmm_sparse_dense_cpu
    SparseCUDA: addmm_sparse_dense_cuda
```

当 `self.device == cuda` 时，dispatch key 为 `DispatchKey::CUDA`，路由到 `addmm_cuda` 函数。

**Dispatch Key 机制**是 PyTorch 的核心抽象，它是一个位掩码（bitmask），每一位代表一个"后端/特性"：

```cpp
// c10/core/DispatchKey.h
enum class DispatchKey : uint8_t {
    CPU       = 0,
    CUDA      = 1,
    Autograd  = 2,
    // ...  layout keys, autograd keys, etc.
};
```

dispatch 时 PyTorch 构建 **DispatchKeySet**，按优先级从高到低遍历：

```cpp
// aten/src/ATen/core/dispatch/Dispatcher.h
class Dispatcher {
    // operatorHandle_ 包含所有注册的 kernel
    OperatorHandle findSchema(const OperatorName& name);

    // lookup 遍历 DispatchKeySet，找到第一个匹配的 kernel
    const KernelFunction& lookup(const OperatorHandle& op, DispatchKeySet ks);
};
```

一个具体的调用路径：

```
torch.addmm(bias, input, weight.t())
  │
  ├─ Python arg parser: unpack to Tensor self, mat1, mat2
  ├─ Dispatcher::call("aten::addmm", self, mat1, mat2)
  │     │
  │     └─ DispatchKeySet: {CUDA, AutogradCPU, AutogradCUDA, ...}
  │         │ 1. DispatchKey::AutogradCUDA → wraps in autograd::addmm (记录 graph node)
  │         │ 2. DispatchKey::CUDA → calls addmm_cuda at:
  │         │      aten/src/ATen/native/cuda/Blas.cpp
  │         │
  │         └─ addmm_cuda(self, mat1, mat2, beta, alpha)
  │               │
  │               └─ if beta == 1.0 and alpha == 1.0:
  │                      cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N,
  │                                  n, m, k, &one,
  │                                  mat2_data, n,   // B^T → n × k
  │                                  mat1_data, k,   // A → m × k
  │                                  &one,
  │                                  self_data, n);  // C → m × n
  │                  else:
  │                      cublasGemmEx(...)
  │
  └─ return output Tensor (on CUDA device)
```

### 1.3 TensorIterator — 广播与类型提升

对于逐元素操作（element-wise ops），PyTorch 使用 **TensorIterator** 作为统一的计算框架。它处理：

1. **广播（broadcasting）**：形状不同的张量如何对齐
2. **类型提升（type promotion）**：`int32 + float32 → float32`
3. **内存步长（stride）**：连续 vs 不连续内存
4. **输出分配**：是否复用 pre-allocated output

TensorIterator 的内核构建过程：

```cpp
// aten/src/ATen/TensorIterator.cpp
void TensorIterator::build() {
    // Step 1: 计算广播形状
    // a: [3, 1, 5], b: [1, 4, 5] → broadcast shape: [3, 4, 5]

    // Step 2: 决定是否原地操作
    // out.add_(a) → output = a

    // Step 3: 分析是否连续
    // is_contiguous() → fast path or slow path

    // Step 4: 选择 launch 策略
    if (is_contiguous_) {
        // 使用 vectorized kernel
        launch_vectorized_kernel(...);
    } else {
        // 使用 legacy kernel (warp per row)
        launch_legacy_kernel(...);
    }
}
```

一个具体的例子：`torch.add(a, b)` 其中 `a.shape = (3, 1)`，`b.shape = (1, 4)`：

```
a:              b:              result:
[ a1 ]          [ b1 b2 b3 b4 ] [ a1+b1  a1+b2  a1+b3  a1+b4 ]
[ a2 ]     +                    [ a2+b1  a2+b2  a2+b3  a2+b4 ]
[ a3 ]                          [ a3+b1  a3+b2  a3+b3  a3+b4 ]

TensorIterator::build():
  shape_  = {3, 4}
  strides = compute_broadcast_strides()
  op     = ADD
```

---

## 2. CUDA 流与事件

### 2.1 流（Stream）基础

CUDA 流是一个**内核执行序列**。同一个流中的操作按 FIFO 顺序执行，不同流之间可以并发执行。本质上，流映射到 GPU 硬件上的**引擎队列**（engine queue）。

```cpp
// 默认流（legacy default stream）
cudaStream_t default_stream = 0;  // NULL stream

// 非阻塞流（per-thread default stream）
cudaStream_t stream;
cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);

// 显式流
cudaStreamCreate(&stream);  // 阻塞流（与 NULL stream 同步）
```

**默认流 vs 每线程默认流**：

```
Legacy default stream (NULL):
  线程 A 的 kernel ─────┐
  线程 B 的 kernel ─────┤→ 全部序列化到 NULL stream
  线程 C 的 kernel ─────┘

Per-thread default stream (cudaStreamPerThread):
  线程 A 的 stream_A ──── kernel_A1, kernel_A2 (相互顺序)
  线程 B 的 stream_B ──── kernel_B1, kernel_B2 (与 A 并发)
```

启用每线程默认流需要编译标志：

```cpp
// 编译时: --default-stream per-thread
// 或运行时: cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);

// 或使用 CUDA API:
// 编译时添加: nvcc --default-stream per-thread
```

### 2.2 流同步

**Host-side 同步**：

```cpp
// 阻塞 CPU 直到 stream 上的所有操作完成
cudaError_t cudaStreamSynchronize(cudaStream_t stream);

// 阻塞 CPU 直到整个设备空闲
cudaError_t cudaDeviceSynchronize();
```

`cudaStreamSynchronize` 的实现路径：

```
用户代码: cudaStreamSynchronize(stream)
  │
  ├─ CUDA Runtime: __cudaStreamSynchronize(stream)
  │     │
  │     ├─ 将 stream 对象转换为设备指针
  │     ├─ 调用 Driver API: cuStreamSynchronize(stream)
  │     │     │
  │     │     ├─ Driver: 向 kernel 发送 SYNC 命令
  │     │     │     │
  │     │     │     ├─ GPU 将 stream 的 pending work 都执行完
  │     │     │     ├─ GPU 写入 fence（隔离点）
  │     │     │     └─ GPU 触发 interrupt 通知 CPU
  │     │     │
  │     │     └─ CPU spin-wait or sleep on semaphore
  │     │           │
  │     │           └─ nvidia.ko 的 GPU interrupt handler 唤醒等待线程
  │     │
  │     └─ return cudaSuccess
  │
  └─ 继续执行后续 CPU 代码
```

**Device-side 同步**：使用事件（event）

### 2.3 CUDA Event — 跨流同步

事件是一个时间戳+同步原语。它记录一个流上的位置，允许其他流等待该位置。

```cpp
// 事件生命周期
cudaEvent_t event;
cudaEventCreate(&event);                 // 创建事件
cudaEventRecord(event, stream_A);        // 在 stream_A 上记录一个"时间点"
cudaStreamWaitEvent(stream_B, event, 0); // stream_B 等待 event 完成
cudaEventSynchronize(event);             // CPU 等待 event
cudaEventElapsedTime(&ms, start, stop);  // 测量两个事件之间的时间
cudaEventDestroy(event);                 // 销毁事件
```

**多流并发示例**：流水线化 kernel 执行和数据传输

```cpp
// 经典的双缓冲流水线
cudaStream_t compute_stream, copy_stream;
cudaStreamCreate(&compute_stream);
cudaStreamCreate(&copy_stream);

cudaEvent_t copy_done, compute_done;
cudaEventCreate(&copy_done);
cudaEventCreate(&compute_done);

for (int i = 0; i < n_iterations; i++) {
    if (i > 0) {
        // 等待上一轮 compute 完成，才能 overwrite input
        cudaStreamWaitEvent(copy_stream, compute_done, 0);
    }

    // 异步拷贝 H→D
    cudaMemcpyAsync(d_input, h_input + i * size, size,
                    cudaMemcpyHostToDevice, copy_stream);

    // 记录"拷贝完成"事件
    cudaEventRecord(copy_done, copy_stream);

    // compute stream 等待 copy 完成才开始 kernel
    cudaStreamWaitEvent(compute_stream, copy_done, 0);

    // 启动 kernel
    my_kernel<<<grid, block, 0, compute_stream>>>(d_input, d_output, size);

    // 记录"计算完成"事件，供下一轮 copy 使用
    cudaEventRecord(compute_done, compute_stream);

    // 异步拷贝 D→H
    cudaMemcpyAsync(h_output + i * size, d_output, size,
                    cudaMemcpyDeviceToHost, compute_stream);
}

// 流水线时间线:
// copy_stream:  [copy H→D_0]········[copy H→D_1]········[copy H→D_2]
// compute_..:              [kernel_0]················[kernel_1]
// 时间节省: 隐藏了数据传输延迟
```

### 2.4 PyTorch 内部的流使用

PyTorch 内部维护了以下几个关键流：

```cpp
// c10/cuda/CUDACachingAllocator.cpp
// c10/cuda/CUDAStream.cpp

class CUDAStreamPool {
    // 当前线程的"计算流" (per-thread default stream)
    CUDAStream default_stream;

    // 流池: 用于并发执行多个独立操作
    // 例如: 多个独立的 layer 可以并发执行
    std::vector<CUDAStream> pool;

    // 数据传输流 (high priority)
    CUDAStream copy_stream;

    // NCCL 通信流 (用于多 GPU 通信)
    CUDAStream nccl_stream;
};

CUDAStream getStreamFromPool() {
    // 循环返回池中的流，用于并发 kernel 执行
    static thread_local int idx = 0;
    return pool[(idx++) % pool.size()];
}
```

PyTorch 的 copy 流用于 CUDA Graph capture 等场景，将 host→device copy 和 compute kernel 分离到不同流：

```cpp
// torch/csrc/cuda/Module.cpp 中的 copy 操作
at::Tensor copy_to_cuda(const at::Tensor & self) {
    // 获取专用的 copy stream
    auto copy_stream = at::cuda::getCurrentCUDAStream();

    // 在一个独立的 copy stream 上执行 cudaMemcpy
    // 主计算流不受影响
    ...
}
```

---

## 3. Kernel Launch 底层探秘

### 3.1 从 ATen 宏到 Tesla 架构

当 ATen 调用一个 CUDA kernel 时，使用 `AT_CUDA_CHECK` 包装：

```cpp
// aten/src/ATen/cuda/Exceptions.h
#define AT_CUDA_CHECK(expr)                                        \
    do {                                                           \
        cudaError_t __err = expr;                                  \
        if (__err != cudaSuccess) {                                \
            AT_ERROR("CUDA error: ", cudaGetErrorString(__err),    \
                     " at ", __FILE__, ":", __LINE__);             \
        }                                                          \
    } while (0)
```

实际的 kernel 启动：

```cpp
// 一个 element-wise add kernel 的定义
__global__ void add_kernel(const float* a, const float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}

// 启动
dim3 grid((n + block_size - 1) / block_size, 1, 1);
dim3 block(block_size, 1, 1);
add_kernel<<<grid, block>>>(d_a, d_b, d_c, n);
```

### 3.2 Grid/Block 维度详解

Tesla 架构的层级执行模型：

```
Grid (1D/2D/3D)
 ├── Block (0,0,0)
 │    ├── Thread (0)
 │    ├── Thread (1)
 │    ├── ...
 │    └── Thread (blockDim.x - 1)
 ├── Block (1,0,0)
 │    ├── Thread (0)
 │    └── ...
 └── Block (nx-1,0,0)
      └── ...

每个 Block 映射到一个 SM (Streaming Multiprocessor)
每个 SM 内部以 Warp (32 threads) 为单位调度
```

计算 grid/block 维度的典型模式：

```cpp
// 1D 数据 (如向量加法)
int block_size = 256;
int grid_size  = CEIL_DIV(n, block_size);  // (n + 255) / 256
dim3 grid(grid_size);
dim3 block(block_size);

// 2D 数据 (如矩阵乘法，每个 block 处理一个 tile)
dim3 block(TILE_SIZE, TILE_SIZE);   // 16x16 = 256 threads
dim3 grid(CEIL_DIV(M, TILE_SIZE),
          CEIL_DIV(N, TILE_SIZE));

// 共享内存
__global__ void matmul(const float* A, const float* B, float* C,
                       int M, int N, int K) {
    __shared__ float As[TILE_SIZE][TILE_SIZE];
    __shared__ float Bs[TILE_SIZE][TILE_SIZE];

    int row = blockIdx.y * TILE_SIZE + threadIdx.y;
    int col = blockIdx.x * TILE_SIZE + threadIdx.x;

    float sum = 0.0f;
    for (int t = 0; t < (K + TILE_SIZE - 1) / TILE_SIZE; t++) {
        // 协同加载 tile 到共享内存
        As[threadIdx.y][threadIdx.x] = A[row * K + t * TILE_SIZE + threadIdx.x];
        Bs[threadIdx.y][threadIdx.x] = B[(t * TILE_SIZE + threadIdx.y) * N + col];

        // block 内所有线程同步，确保 tile 加载完成
        __syncthreads();

        // 计算 tile 乘积
        for (int k = 0; k < TILE_SIZE; k++) {
            sum += As[threadIdx.y][k] * Bs[k][threadIdx.x];
        }
        __syncthreads();  // 下一轮 tile 加载前同步
    }
    C[row * N + col] = sum;
}
```

### 3.3 动态共享内存

启动时指定共享内存大小：

```cpp
// kernel 声明使用 extern __shared__，大小由启动参数决定
__global__ void reduce_kernel(const float* input, float* output, int n) {
    extern __shared__ float sdata[];  // 运行时确定大小

    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + tid;

    sdata[tid] = (idx < n) ? input[idx] : 0.0f;
    __syncthreads();

    // 在共享内存中进行归约
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        output[blockIdx.x] = sdata[0];
    }
}

// 启动时指定共享内存大小
int smem_size = block_size * sizeof(float);
reduce_kernel<<<grid, block, smem_size>>>(d_input, d_output, n);
```

### 3.4 Kernel Launch → GPU 命令缓冲区的底层路径

```
Python: output = torch.add(a, b)
  │
  ├─ ATen Dispatch: aten::add → add_stub
  │     │
  │     └─ tensor_iterator 选择 kernel
  │           │
  │           └─ gpu_kernel_impl 模板函数
  │
  ├─ 生成 grid/block 维度和 shared memory 参数
  │
  ├─ CUDA Runtime: cudaLaunchKernel(
  │       kernel_ptr,        // GPU 函数指针
  │       dim3 grid,         // grid 维度
  │       dim3 block,        // block 维度
  │       void** args,       // kernel 参数
  │       size_t shmem,      // 动态共享内存
  │       cudaStream_t stream // 流
  │   )
  │     │
  │     └─ cuLaunchKernel(...) // Driver API
  │           │
  │           ├─ 验证参数:
  │           │  - grid/block 维度不超硬件上限
  │           │  - shared memory ≤ device 容量
  │           │  - 参数大小 ≤ 4KB (kernel 参数缓冲区)
  │           │
  │           ├─ 构造 CUDA 命令包 (command packet):
  │           │  ┌──────────────────────────────────┐
  │           │  │ Command Type: LAUNCH_KERNEL       │
  │           │  │ Grid: (gx, gy, gz)               │
  │           │  │ Block: (bx, by, bz)              │
  │           │  │ Shmem: smem_size                 │
  │           │  │ Kernel PC: function_address       │
  │           │  │ Args: copied to constant buffer   │
  │           │  │ Performance: priority, cache hints │
  │           │  └──────────────────────────────────┘
  │           │
  │           ├─ 写入设备命令缓冲区 (push buffer):
  │           │  - 通过 MMIO 写入 GPU 的 circular buffer
  │           │  - HOST → GPU doorbell write (写寄存器触发)
  │           │
  │           └─ 函数返回 (控制权返回 CPU，GPU 异步执行)
  │
  └─ Python 层得到输出 Tensor 引用（不等待 GPU 完成）
```

关键点：**`cuLaunchKernel` 是异步的**，它只是把一个命令写入 GPU 的命令缓冲区（亦称 push buffer / channel），然后立即返回。CPU 通过写 doorbell 寄存器通知 GPU 有新命令可执行。GSP（GPU System Processor）或主机接口单元从 push buffer 读取命令，分发给各个引擎（GPC、L2、Copy Engine）。

---

## 4. 具体实例：`torch.add(a, b)` 在 GPU 上的完整路径

### 4.1 源码逐帧分析

```python
# Python 端
a = torch.randn(1024, device='cuda')
b = torch.randn(1024, device='cuda')
c = torch.add(a, b)  # 或 c = a + b
```

**Step 1: ATen Dispatch**

```cpp
// build/aten/src/ATen/Operators_0.cpp (自动生成)
// aten::add.Tensor(Tensor self, Tensor other, *, Scalar alpha=1) -> Tensor
Tensor add(const Tensor & self, const Tensor & other,
           const Scalar & alpha) {
    // Dispatcher 找到 CUDA kernel
    auto dispatch_key_set = computeDispatchKeySet(self, other);
    // dispatch_key_set = {DispatchKey::CUDA, ...}
    return dispatcher.call("aten::add.Tensor", self, other, alpha,
                           dispatch_key_set);
}
```

**Step 2: gpu_kernel 模板**

```cpp
// aten/src/ATen/native/cuda/UnaryOpsKernel.cu
void add_kernel_cuda(TensorIteratorBase& iter, const Scalar& alpha) {
    // TensorIterator 已经分析了广播和类型提升
    gpu_kernel(iter,
        [alpha] GPU_LAMBDA (scalar_t a, scalar_t b) -> scalar_t {
            return a + alpha.to<scalar_t>() * b;
        });
}
```

**Step 3: gpu_kernel_impl 展开**

```cpp
// aten/src/ATen/cuda/CUDALoops.cuh
template <typename func_t>
void gpu_kernel_impl(TensorIteratorBase& iter, const func_t& f) {
    if (iter.is_contiguous()) {
        // Fast path: 连续内存 → 向量化 kernel
        launch_vectorized_kernel(iter, f);
    } else {
        // Slow path: 非连续内存 → 逐元素 kernel
        launch_legacy_kernel(iter, f);
    }
}
```

**Step 4: 向量化 kernel 启动**

```cpp
// 向量化: 每个线程处理 vec_size 个元素
constexpr int vec_size = 4;           // float4 → 128-bit loads
constexpr int block_size = 256;       // threads per block

int total_elements = iter.numel();    // 1024
int vec_grid = (total_elements + block_size * vec_size - 1)
               / (block_size * vec_size);
// vec_grid = 1 (since 1024 <= 256*4 = 1024)

kernel<<<vec_grid, block_size, 0, stream>>>(
    input_data, output_data, total_elements);
```

**Step 5: Kernel 内部的 SIMT 执行**

```cuda
// 生成的 kernel (伪代码)
__global__ void add_vectorized(const float* __restrict__ a,
                               const float* __restrict__ b,
                               float* __restrict__ c, int n) {
    // 每个 thread 处理 4 个元素 (vec_size = 4)
    int base_idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;

    if (base_idx + 3 < n) {
        float4 a_vec = reinterpret_cast<const float4*>(a)[base_idx / 4];
        float4 b_vec = reinterpret_cast<const float4*>(b)[base_idx / 4];
        float4 c_vec;
        c_vec.x = a_vec.x + b_vec.x;
        c_vec.y = a_vec.y + b_vec.y;
        c_vec.z = a_vec.z + b_vec.z;
        c_vec.w = a_vec.w + b_vec.w;
        reinterpret_cast<float4*>(c)[base_idx / 4] = c_vec;
    } else {
        // 处理尾部不完整的元素
        for (int i = base_idx; i < n; i++) {
            c[i] = a[i] + b[i];
        }
    }
}
```

### 4.2 GPU 硬件执行时间线

```
时间 →
CPU:        [构建命令包]→[写 push buffer]→[写 doorbell]→[继续 Python 代码]
                                                           │
GPU:                         [GSP/MCPU 读取 push buffer]   │
                               │                            │
                               ├─ 解码 LAUNCH_KERNEL 命令   │
                               ├─ 分配 SM 资源              │
                               ├─ 加载 kernel 到 I-Cache    │
                               │                            │
                               └─ SM 执行:                   │
                                   Thread 0-31  (Warp 0)    │
                                   Thread 32-63 (Warp 1)    │
                                   ...                      │
                                   所有 warp 完成 →          │
                                   写完成标志到 memory fence │
                                                         [cudaStreamSynchronize]
                                                         返回结果所在内存已就绪
```

### 4.3 内存视角

```
GPU Global Memory (HBM):
  ┌────────────────────────────────────────┐
  │ a (float[1024]): 0x7f_f800_0000        │
  │ b (float[1024]): 0x7f_f800_1000        │
  │ c (float[1024]): 0x7f_f800_2000        │ ← output Tensor 的 storage
  └────────────────────────────────────────┘

L2 Cache:
  每次 warp 的 load: 128 bytes (32 threads × 4 bytes)
  vec4 kernel: 一次 load 128 bytes (刚好 cache line 大小)
  命中率: 接近 100% (顺序访问，prefetch 友好)

SMEM: 未使用 (element-wise 不需要共享内存)
```

---

## 5. Autograd 集成

### 5.1 每步操作都记录 Autograd Node

当 `a.requires_grad = True` 时，`torch.add(a, b)` 的 dispatch 会先经过 `AutogradCUDA` key：

```cpp
// torch/csrc/autograd/generated/ADInplaceOrViewTypeEverything.cpp
Tensor add_Tensor(const Tensor& self, const Tensor& other,
                  const Scalar& alpha) {
    // 第一步: 记录 autograd graph
    auto result = [&]() {
        // 检查是否需要 autograd
        if (self.requires_grad() || other.requires_grad()) {
            // 构建 Autograd Node
            auto node = std::make_shared<AddBackward0>();
            node->set_ctx(...);  // 保存上下文供 backward 使用

            // 将 node 连接到当前 graph
            // 这会在 autograd tape 上增加一个条目
        }
        // 调用实际的 CUDA kernel
        return add_stub(CUDA, tensor_iterator, alpha);
    }();

    // 第二步: 设置 edge 信息
    // result → node → (self, other)
    // backward 时沿这个图反向传播

    return result;
}
```

### 5.2 反向传播示例

```python
x = torch.randn(10, requires_grad=True, device='cuda')
w = torch.randn(10, requires_grad=True, device='cuda')
b = torch.randn(1,  requires_grad=True, device='cuda')

# Forward: y = w * x + b
y = w * x + b  # mul → add: 两个 autograd node

# Loss
loss = y.sum()
loss.backward()

# Backward graph:
# loss (sum)
#   └─ grad_output = 1
#       └─ AddBackward0:
#           dL/d(w*x) = 1, dL/db = 1
#           └─ MulBackward0:
#               dL/dw = x * 1 = x
#               dL/dx = w * 1 = w
```

**梯度累积的关键语义**：

```cpp
// torch/csrc/autograd/functions/accumulate_grad.cpp
void AccumulateGrad::apply(std::vector<Variable>&& grads) {
    // critical: += 不是 =
    // 这允许从多个路径来的梯度累加 (多 GPU 场景)
    at::add_out(grad_output, new_grad, variable.grad());
    // equivalent to: variable.grad() += new_grad
}

// 错误做法:    variable.grad() = new_grad;  // 覆盖之前的梯度!
// 正确做法:    variable.grad() += new_grad; // 累加

// 多 GPU 场景:
// GPU 0: w.grad += dL0/dw
// GPU 1: w.grad += dL1/dw
// w.grad 最终包含所有 GPU 上的梯度贡献
```

### 5.3 Autograd 的 CUDA 同步

```cpp
// 当 tensor 从 GPU 复制到 CPU 用于 Python 检查或打印时
// PyTorch 会自动插入同步

Tensor& variable::grad() {
    // 如果 grad 尚未计算但已调度，则阻塞
    if (auto grad_acc = this->grad_accumulator()) {
        // 等待 grad 所在的 CUDA stream 完成
    }
    return grad_;
}

// 危险操作: 不同步就读取 GPU tensor 的值
float x = my_tensor.item<float>();  // ← 隐式 cudaDeviceSynchronize()
                                     // 确保 GPU 操作完成
```

### 5.4 PyTorch Autograd Graph 完整 ASCII 图

```
┌─────────────────────────────────────────────────────────────────────┐
│  Forward: y = F.linear(x, w, b)                                     │
│                                                                     │
│  x ───┐                                                             │
│  w ───┼──→ [Linear] ──→ y                                           │
│  b ───┘  (aten::addmm)                                              │
│            │                                                        │
│            ├─ ATen dispatch                                          │
│            ├─ cublasSgemm (if fp32)                                  │
│            └─ CUDA stream → GPU                                      │
│                                                                     │
│  Backward: loss.backward()                                          │
│                                                                     │
│  loss ──→ [AddmmBackward0]                                          │
│             ├─ grad_x = grad_y @ w.T   (matmul)                     │
│             ├─ grad_w = grad_y.T @ x   (matmul)                     │
│             └─ grad_b = grad_y.sum(dim=0) (reduce)                  │
│                                                                     │
│  Each backward op is also dispatched → cublas/SGEMM → GPU           │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 6. 完整调用链 ASCII 图

```
Python
────────────────────────────────────────────────────────────────────
  output = F.linear(input, weight, bias)
    │
    ├─ input @ weight.T + bias          (Python 运算符重载)
    ├─ torch.addmm(bias, input, weight.T)
    │
C++ (libtorch.so / libtorch_cuda.so)
────────────────────────────────────────────────────────────────────
    │
    ├─ Dispatcher::call("aten::addmm", ...)
    │   ├─ DispatchKey::AutogradCUDA → addmm_autograd (记录 Node)
    │   └─ DispatchKey::CUDA → addmm_cuda
    │                              │
    │     ┌────────────────────────┘
    │     │
    │     ├─ 检查 shape, dtype, beta/alpha
    │     ├─ stride check: contiguous?
    │     ├─ 调用 cuBLAS
    │     │     │
    │     │     └─ cublasSgemm(handle,
    │     │           CUBLAS_OP_T, CUBLAS_OP_N,
    │     │           m, n, k,
    │     │           &alpha,
    │     │           A, lda,
    │     │           B, ldb,
    │     │           &beta,
    │     │           C, ldc)
    │     │           │
    │     │           └─ cuBLAS library → internal tile selection
    │     │                              → launch optimized SGEMM kernel
    │     │
    │     └─ 返回 output Tensor (指向 GPU memory)
    │
CUDA Driver (libcuda.so)
────────────────────────────────────────────────────────────────────
    │
    ├─ cuBLAS 内部调用 cuLaunchKernel
    │   ├─ 构造命令包 (LAUNCH_KERNEL)
    │   ├─ 写入 push buffer (channel)
    │   ├─ 写 doorbell → GPU 被唤醒
    │   └─ 返回 (异步)
    │
NVIDIA Kernel Driver (nvidia.ko)
────────────────────────────────────────────────────────────────────
    │
    ├─ MMIO write → GPU push buffer
    ├─ GPU GSP/MCPU 解码命令
    ├─ 调度到 GPC (Graphics Processing Cluster)
    │   ├─ SM (Streaming Multiprocessor) 执行 warps
    │   ├─ Tensor Core: 矩阵乘累加 (FMA)
    │   └─ 结果写入 HBM
    │
    └─ 完成 → 发 interrupt 给 CPU (可选)
```

---

## 7. 实用调试技巧

### 7.1 查看 ATen dispatch 日志

```bash
# 环境变量启用 dispatch 日志
TORCH_SHOW_DISPATCH_TRACE=1 python train.py

# 输出示例:
# [Dispatch] aten::addmm -> DispatchKey::CUDA -> addmm_cuda
# [Dispatch] aten::relu -> DispatchKey::CUDA -> relu_kernel_cuda
```

### 7.2 NSight Systems 分析 CUDA Stream

```cpp
// 在 PyTorch 代码中使用 NVTX 标记
#include <nvtx3/nvToolsExt.h>

void forward_pass() {
    nvtxRangePushA("forward:layer1");
    output = layer1(input);
    nvtxRangePop();

    // NSight Systems 时间线将显示:
    // ┌── forward:layer1 ────────┐
    // │ CUDA stream 14: [kernel] │ [kernel] [kernel]
    // │ CUDA stream 15:          [memcpy D→H]
    // └──────────────────────────┘
}
```

### 7.3 验证多个流是否并发

```python
# PyTorch 的多流并发测试
import torch

s1 = torch.cuda.Stream()
s2 = torch.cuda.Stream()

with torch.cuda.stream(s1):
    a = torch.randn(10000, 10000, device='cuda')
    b = a @ a  # 操作在 s1 上

with torch.cuda.stream(s2):
    c = torch.randn(10000, 10000, device='cuda')
    d = c @ c  # 操作在 s2 上

torch.cuda.synchronize()  # 等待所有流完成

# 使用 nvprof 验证并发:
# nvprof --print-gpu-trace python script.py
# 如果看到 s1 的 kernel 和 s2 的 kernel 时间重叠 → 并发成功
```

### 7.4 PyTorch Profiler 查看 CUDA 时间

```python
from torch.profiler import profile, ProfilerActivity

with profile(activities=[ProfilerActivity.CPU, ProfilerActivity.CUDA]) as prof:
    output = model(input)
    loss.backward()

print(prof.key_averages().table(sort_by="cuda_time_total", row_limit=10))
# 输出:
# Name                          CPU total    CUDA total    Calls
# cublasGemmEx                  5.2ms        4.8ms         64
# elementwise_kernel            1.3ms        1.2ms         128
# cudaMemcpyAsync               0.5ms        0.4ms         32
```

---

## 8. 关键源码文件索引

| 功能 | 源码位置 |
|------|---------|
| `F.linear` Python 实现 | `torch/nn/functional.py` |
| ATen native 函数注册 | `aten/src/ATen/native/native_functions.yaml` |
| CUDA addmm 实现 | `aten/src/ATen/native/cuda/Blas.cpp` |
| TensorIterator | `aten/src/ATen/TensorIterator.h` |
| gpu_kernel 模板 | `aten/src/ATen/cuda/CUDALoops.cuh` |
| Dispatch Key 定义 | `c10/core/DispatchKey.h` |
| Dispatcher | `aten/src/ATen/core/dispatch/Dispatcher.h` |
| Autograd Node | `torch/csrc/autograd/function.h` |
| CUDA Stream 包装 | `c10/cuda/CUDAStream.h` |
| CUDA allocator | `c10/cuda/CUDACachingAllocator.cpp` |

---

## 下一篇文章

[第二篇：CUDA 运行时与驱动：Memory Allocator、Graph、MPS](./02-cuda-runtime.md)
