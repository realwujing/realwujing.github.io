# 第五篇：GPU 硬件架构：SM、Warp、Tensor Core、TMA

系列目录：[PyTorch → NVIDIA GPU 全链路深度解析](./README.md)

---

## 1. GPU 层次总览：以 H100 为例

### 1.1 从 Python 到晶体管

```
PyTorch tensor operation
  → CUDA kernel launch <<<grid, block>>>
    → GigaThread Engine 分发 grid 到 TPC → SM
      → Warp Scheduler 逐周期挑选 warp
        → 指令派发到 CUDA Core / Tensor Core / LD-ST Unit
          → 寄存器文件 ↔ 共享内存 ↔ L1 ↔ L2 ↔ HBM3
```

### 1.2 H100 SXM 规格

```
┌─────────────────────────────────────────────────────────────────┐
│                    NVIDIA H100 SXM GPU                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Streaming Multiprocessors (SM):      132                       │
│  CUDA Cores per SM:                   128 (FP32)                │
│  Total CUDA Cores:                    16896                     │
│  Tensor Cores (4th Gen):              528 (4 per SM)            │
│  Texture Units per SM:                4                         │
│                                                                 │
│  GPU Boost Clock:                     1830 MHz                  │
│  Peak FP32 (CUDA Cores):              60 TFLOPS                 │
│  Peak FP16 (Tensor Cores):            990 TFLOPS                │
│  Peak FP8 (Tensor Cores):             1979 TFLOPS               │
│  Peak INT8 (Tensor Cores):            1979 TOPS                 │
│  Peak BF16 (Tensor Cores):            990 TFLOPS                │
│  Peak TF32 (Tensor Cores):            495 TFLOPS                │
│  Peak FP64 (Tensor Cores):            60 TFLOPS                 │
│                                                                 │
│  Total VRAM:                          80 GB HBM3                │
│  Memory Bandwidth:                    3.35 TB/s                 │
│  Memory Bus Width:                    5120-bit                  │
│  L2 Cache:                            50 MB                     │
│                                                                 │
│  Register File per SM:                256 KB (65536 × 32-bit)   │
│  Shared Memory per SM:                Up to 227 KB              │
│  L1 Cache / Shared Memory (config):  256 KB total per SM        │
│                                                                 │
│  NVLink 4.0:                          18 links × 50 GB/s        │
│  NVSwitch:                            900 GB/s per GPU (bi-dir) │
│  PCIe 5.0:                            16 lanes × 4 GB/s = 64 GB/s│
│                                                                 │
│  TDP:                                 700 W                     │
│  Process Node:                        TSMC 4N (customized 5nm)  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 GPU vs CPU 核心哲学差异

```
CPU (x86 Zen 4):                         GPU (H100 SM):
┌──────────────────────┐                ┌──────────────────────┐
│ 16 个大核             │                │ 132 个 SM            │
│ 每核: OoO, 分支预测   │                │ 每 SM: 128 CUDA Core │
│  大 L1(32KB)+L2(1MB)  │                │  小 L1, 大 Shared Mem│
│  ~4-6 GHz             │                │  ~1.8 GHz            │
│  Latency optimized    │                │  Throughput optimized│
│  隐藏延迟: 乱序执行    │                │  隐藏延迟: 零开销切Warp│
│  擅长: 低延迟串行任务  │                │  擅长: 高吞吐并行任务 │
└──────────────────────┘                └──────────────────────┘

关键区别:
  CPU: 为单线程低延迟优化 → 分支预测器、乱序执行、大缓存
  GPU: 为大规模并行吞吐优化 → 成千上万线程、轻量切换、高带宽显存

一个 SM ≈ 一个超宽 SIMD 单元:
  - 128 CUDA Cores → 每周期可执行 128 个 FP32 FMA
  - 4 Warp Schedulers → 每周期从 4×32=128 threads 中选指令
  - 硬件管理的零开销线程切换 (类似 SMT，但有数百个硬件线程)
```

---

## 2. 线程模型：Thread → Warp → Block → Grid

### 2.1 层次定义

```cuda
// CUDA 编程模型
// Grid: 最顶层，kernel launch 时的 N 个 thread block
//   Block: 一组线程 (最多 1024)，映射到一个 SM
//     Warp: 32 个线程，SM 上的执行单位 (lockstep SIMT)
//       Thread: 单个执行通道 (lane)

__global__ void matmul_kernel(
    float* A, float* B, float* C, int M, int N, int K)
{
    // 每个 thread 通过内置变量知道自己的位置
    int row = blockIdx.y * blockDim.y + threadIdx.y;  // 全局行
    int col = blockIdx.x * blockDim.x + threadIdx.x;  // 全局列
    
    // 但这只是逻辑映射，硬件上的执行单位永远是 warp
    // threadIdx.x     → 当前 thread 在 block 内的 x 坐标 (0~blockDim.x-1)
    // threadIdx.y     → 当前 thread 在 block 内的 y 坐标
    // blockIdx.x      → 当前 block 在 grid 内的 x 坐标
    // blockDim.x      → block 在 x 方向的线程数
    // gridDim.x       → grid 在 x 方向的 block 数
    // warpSize        → 始终为 32
    // laneId = threadIdx.x % 32  → 当前 thread 在 warp 内的编号
    
    // 典型计算
    float acc = 0.0f;
    for (int k = 0; k < K; k++) {
        acc += A[row * K + k] * B[k * N + col];
    }
    C[row * N + col] = acc;
}

// kernel launch: <<<grid_dim, block_dim, shared_mem_bytes, stream>>>
int block_dim = 256;  // 每个 block 256 个线程
int grid_dim = (M * N + block_dim - 1) / block_dim;
matmul_kernel<<<grid_dim, block_dim>>>(d_A, d_B, d_C, M, N, K);
```

### 2.2 Warp：SIMT 的执行单位

Warp 是 GPU 的最小调度单位。32 个线程以 lockstep 模式执行同一条指令：

```
CUDA Warp 结构 (warp 内 32 个线程):
┌─────────────────────────────────────────────────────────────┐
│  Thread 0  Thread 1  Thread 2  ...  Thread 30  Thread 31   │
│     │          │          │              │          │       │
│     └──────────┴──────────┴──────────────┴──────────┘       │
│                       │                                     │
│                   1 条指令, 32 路数据                        │
│                   SIMT (Single Instruction Multiple Thread) │
└─────────────────────────────────────────────────────────────┘

Warp 内通信: shuffle 指令 (寄存器级，无 shared memory 开销)
  __shfl_sync:     广播一个 lane 的值到所有 lane
  __shfl_up_sync:  从低 lane 向高 lane 移动数据
  __shfl_down_sync:从高 lane 向低 lane 移动数据
  __shfl_xor_sync: 按位异或 laneId 交换数据 (蝶形归约)
```

### 2.3 Block → SM 映射

```cuda
// 一个 SM 可以同时驻留多个 thread block
// 具体数量取决于 block 的资源需求

// 限制因素 (H100):
//   Max threads per SM:       2048
//   Max thread blocks per SM: 32
//   Max registers per SM:     65536 (每个 register 32-bit)
//   Max shared memory per SM: 227 KB (configurable up to)

// 资源计算示例:
//   Block dim = 256 threads
//   Registers per thread = 128
//   Shared memory per block = 32 KB
//
//   Max blocks by threads:  floor(2048 / 256) = 8
//   Max blocks by registers: floor(65536 / (256 × 128)) = floor(65536/32768) = 2
//   Max blocks by shared mem: floor(227 / 32) = 7
//
//   实际可驻留: min(8, 2, 7) = 2 blocks per SM
//   → Occupancy = (2×256) / 2048 = 25%  ← 很低！受寄存器压力限制

// 改进: 减少每线程寄存器使用
//   Registers per thread = 64 → max by reg = floor(65536/(256×64)) = 4
//   实际可驻留: min(8, 4, 7) = 4 blocks per SM
//   → Occupancy = (4×256) / 2048 = 50%
```

### 2.4 Occupancy 计算

```python
# Occupancy: 活跃 warp 数 / 最大可驻留 warp 数
# 高 occupancy 有助于隐藏延迟 (mem latency, pipeline stall)

def compute_occupancy(block_dim, regs_per_thread, shared_mem_per_block):
    """
    H100 参数:
      max_threads_per_sm = 2048
      max_warps_per_sm = 64  (2048 / 32)
      max_regs_per_sm = 65536
      max_shared_mem_per_sm = 227 * 1024  # 227 KB in bytes
      max_blocks_per_sm = 32
      register_allocation_granularity = 256  # 以 256 个 register 为单位分配
    """
    # 向上取整到 256 的倍数
    regs_per_thread_aligned = ((regs_per_thread + 255) // 256) * 256
    
    # 每个 block 使用的寄存器
    regs_per_block = block_dim * regs_per_thread_aligned
    
    # 各限制下的最大 block 数
    blocks_by_threads = max_threads_per_sm // block_dim
    blocks_by_regs = max_regs_per_sm // regs_per_block
    blocks_by_smem = max_shared_mem_per_sm // shared_mem_per_block \
                     if shared_mem_per_block > 0 \
                     else max_blocks_per_sm
    blocks_by_limit = max_blocks_per_sm
    
    active_blocks = min(blocks_by_threads, blocks_by_regs, blocks_by_smem, blocks_by_limit)
    active_warps = active_blocks * (block_dim // 32)
    
    occupancy = active_warps / max_warps_per_sm
    
    return {
        'active_blocks': active_blocks,
        'active_warps': active_warps,
        'occupancy': occupancy,
        'limiting_factor': 'registers' if blocks_by_regs == active_blocks else
                          ('shared_mem' if blocks_by_smem == active_blocks else
                          ('threads' if blocks_by_threads == active_blocks else
                          'hardware_limit')),
    }

# 示例:
#   block_dim=256, regs=128, smem=0     → occupancy=25%  (寄存器限制)
#   block_dim=256, regs=64,  smem=0     → occupancy=50%  (寄存器限制)
#   block_dim=512, regs=64,  smem=0     → occupancy=50%  (线程限制)
#   block_dim=128, regs=96,  smem=40KB  → occupancy=25%  (共享内存限制)
```

CUDA API 获取 occupancy：

```cuda
#include <cuda_runtime.h>

int block_size = 256;
cudaFuncAttributes attr;
cudaFuncGetAttributes(&attr, (void*)my_kernel);

int min_grid_size, suggested_block_size;
cudaOccupancyMaxPotentialBlockSize(
    &min_grid_size, &suggested_block_size,
    my_kernel, 0, 0  // dynamic shared memory = 0
);

// 或使用 launch bounds 提示编译器优化寄存器分配
__global__ void __launch_bounds__(256, 4)  // max threads=256, min blocks=4
my_kernel(...) {
    // 编译器会尝试将寄存器数限制在使 4 个 block 可同时驻留的水平
}
```

---

## 3. Warp Divergence：SIMT 的代价

### 3.1 分支如何拖慢 Warp

```cuda
// 场景: warp 内线程不同的执行路径

__global__ void divergent_example(float* data, int n) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx >= n) return;
    
    // Warp diverges here
    if (threadIdx.x % 2 == 0) {
        // 偶数线程路径 — 奇数线程被 mask out
        data[idx] = expensive_computation_a(data[idx]);
    } else {
        // 奇数线程路径 — 偶数线程被 mask out
        data[idx] = expensive_computation_b(data[idx]);
    }
    // 两条路径串行执行，每个路径只有 50% 的线程活跃
}

// 时间线:
// ┌──────────────────────┬───────────────────────────────────┐
// │ Cycle  1..100        │ Even threads active (16/32)       │
// │                      │ Odd threads masked (inactive)     │
// │                      │ PATH A: expensive_computation_a    │
// ├──────────────────────┼───────────────────────────────────┤
// │ Cycle 101..200       │ Odd threads active (16/32)        │
// │                      │ Even threads masked (inactive)    │
// │                      │ PATH B: expensive_computation_b    │
// └──────────────────────┴───────────────────────────────────┘
// 总耗时 = 200 cycles (两条路径各 100 cycles)
// 理想无分支: 100 cycles → 50% 效率

// 改进: 使用 data-dependent 方式或 warp 级重排
__global__ void less_divergent(float* data, int n) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx >= n) return;
    
    // 将同路径的 thread 聚集 (需要先排序数据)
    // 或利用条件谓词执行 (predicated execution)
    float val = data[idx];
    bool condition = (threadIdx.x % 2 == 0);
    
    // 编译器可能对短分支使用 predicated 执行
    // if (condition) { val = fma(val, 2.0f, 1.0f); }
    //  → 转换为: val = condition ? fma(val, 2, 1) : val
    //  通过 select 指令避免分支
}
```

### 3.2 SIMT Mask 与 Active Mask

```cuda
// CUDA 提供 warp-level 投票和 mask 操作
// 自 CUDA 9.0 开始，warp 级操作显式要求 mask 参数

// 活跃线程掩码 (ballot)
__device__ unsigned active_mask = __activemask();
// 返回 32-bit 掩码，bit i=1 表示 lane i 当前活跃

// 投票操作
int predicate = __all_sync(0xFFFFFFFF, (data > 0));
// 如果 warp 内所有活跃线程的 data > 0 → 返回非零

predicate = __any_sync(0xFFFFFFFF, (data > 0));
// 如果任意活跃线程的 data > 0 → 返回非零

unsigned vote = __ballot_sync(0xFFFFFFFF, (data > 0));
// 返回 mask，bit i=1 表示 lane i 的 data > 0

// 归约操作 (shuffle — 无需 shared memory!)
float val = data[threadIdx.x];
// 每个 lane 贡献一个值，用位异或模式做蝶形归约
#pragma unroll
for (int offset = 16; offset > 0; offset /= 2) {
    val += __shfl_xor_sync(0xFFFFFFFF, val, offset);
}
// 现在所有 lane 的 val 都等于总和

// 分歧感知的归约:
// 只对满足条件的 lane 做归约
unsigned active = __ballot_sync(0xFFFFFFFF, condition);
float result = warp_reduce_sum(val, active);  // 自定义函数
// warp_reduce_sum 用 active mask 做 shuffle，跳过 inactive lane
```

### 3.3 减少 Warp Divergence 的策略

```cuda
// Strategy 1: 使分支边界对齐 warp 边界
__global__ void aligned_divergence(float* data, int n) {
    int warp_id = threadIdx.x / 32;
    int lane_id = threadIdx.x % 32;
    
    // 整个 warp 走同一分支 → 0% divergence
    if (warp_id % 2 == 0) {
        // 这个 warp 的 32 个线程全部走 path A
        data[threadIdx.x] = path_a(data[threadIdx.x]);
    } else {
        data[threadIdx.x] = path_b(data[threadIdx.x]);
    }
}

// Strategy 2: 对短分支使用 predicated 执行 (无 jump)
// 短条件赋值: 编译器自动用 predication
// 长条件路径: 无法避免，考虑重建

// Strategy 3: 数据预处理 — 将同类计算分组
// 把需要走 path_a 的数据放到连续地址，path_b 的数据放到后面
// 然后用 warp-level early exit

// Strategy 4: 对 index-out-of-bounds 的线程尽早退出
// 这是最常见的 divergence 来源
__global__ void safe_kernel(float* data, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // 先检查 OOB
    if (idx >= n) return;
    // 剩余的线程不会发散 (因为 OOB 线程已退出)
    
    // 所有活跃线程都执行相同的后续代码
    float val = data[idx];
    val = val * val + val;
    data[idx] = val;
}

// 注意: 如果 OOB 条件在 warp 内部分真部分假 → 仍有 divergence
// Warp idx=0: lanes 0..30 有效, lane 31 OOB
//   → lane 31 returns, lanes 0..30 continue
//   → 之后如果 lanes 0..30 又有分支 → 仍然 31/32 效率
```

---

## 4. 显存层次：寄存器到 HBM

### 4.1 完整内存层次

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        GPU 内存层次 (H100)                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ Register File                                                 │   │
│  │ 容量:    256 KB per SM (65536 × 32-bit)                       │   │
│  │ 延迟:    ~0 cycles (操作数直接来自寄存器)                      │   │
│  │ 带宽:    ~8 TB/s per SM (上限)                                 │   │
│  │ 作用域:  每个 Thread 私有                                      │   │
│  │ 硬件:    Banked SRAM, 4 register banks                         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ Shared Memory + L1 Data Cache (统一 256 KB SRAM)               │   │
│  │ 可配置:  L1=256KB Shmem=0  ~  L1=0 Shmem=227KB                │   │
│  │          H100 最大 Shmem = 227 KB (L1=29KB)                    │   │
│  │ 延迟:    ~20-30 cycles                                          │   │
│  │ 带宽:    ~3.3 TB/s per SM (128 bytes/cycle/SM × 1.8 GHz)       │   │
│  │ 作用域:  同一个 Thread Block 内共享                             │   │
│  │ 硬件:    32 banks, 4 bytes per bank                             │   │
│  │ 管理:    程序员显式管理 (__shared__ + __syncthreads())         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ L2 Cache (Unified, on-die)                                     │   │
│  │ 容量:    50 MB (H100), 40 MB (A100), 6 MB (V100)              │   │
│  │ 延迟:    ~200 cycles                                            │   │
│  │ 带宽:    ~4 TB/s (H100), ~3 TB/s (A100)                        │   │
│  │ 作用域:  所有 SM 共享                                           │   │
│  │ 硬件:    多 bank SRAM, crossbar 连接到所有 SM                   │   │
│  │ 管理:    硬件自动缓存 (不可编程)                                │   │
│  │          CUDA 11.3+ 支持 L2 cache hint / reservation            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ HBM3 VRAM (Off-die)                                            │   │
│  │ 容量:    80 GB (H100), 40/80 GB (A100)                         │   │
│  │ 延迟:    ~500-800 cycles (首次访问, 未命中 L1/L2)              │   │
│  │ 带宽:    3.35 TB/s (H100), 2.0 TB/s (A100)                     │   │
│  │ 总线:    5120-bit (5 HBM3 stacks, 每个 1024-bit)               │   │
│  │ 管理:    cudaMalloc / cudaFree                                  │   │
│  │          Device: 从 GPU 访问                                     │   │
│  │          Pinned: 从 CPU 和 GPU 都可访问 (cudaMallocHost)         │   │
│  │          Managed: 统一虚拟地址 (cudaMallocManaged)               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 全局内存合并访问 (Coalescing)

```cuda
// 全局内存的访问粒度: 32-byte 事务 (L2 cache line)
// H100: 实际从 HBM 获取的最小单元是 64-byte sectored cache line

// ❌ 非合并访问: 相邻线程访问跨步内存 → 多次 memory transaction
__global__ void strided_access(float* A, float* B, int width) {
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int row = blockIdx.y * blockDim.y + ty;
    int col = blockIdx.x * blockDim.x + tx;
    
    // stride = width (跨行访问)
    // Thread 0 读 A[0], Thread 1 读 A[width], Thread 2 读 A[2*width]
    // 每条 32-byte cache line 只命中 1 个 float (4 bytes) → 32次事务
    B[col * width + row] = A[row * width + col];  // 矩阵转置
}

// ✅ 合并访问: 相邻线程访问相邻地址 → 最小化 transaction 数
__global__ void coalesced_access(float* A, float* B, int width) {
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int row = blockIdx.y * blockDim.y + ty;
    int col = blockIdx.x * blockDim.x + tx;
    
    // Thread 0 读 A[0], Thread 1 读 A[1], Thread 2 读 A[2]
    // 32 个线程一次性读 128 bytes → 4 条 cache line → 4次事务
    int idx = row * width + col;
    B[idx] = A[idx] * 2.0f;
}

// 合并访问的量化:
// Warp 32 threads × 4 bytes/float = 128 bytes
// 128 bytes / 32 bytes/cache_line = 4 transactions (ideal)
// 如果 stride=2: 32 threads × 2 = 64 floats worth of range → 64×4/32=8 transactions
// 如果 stride=32: 每条 cache line 只能服务 1 个 thread → 32 transactions (worst)
```

向量化加载进一步优化：

```cuda
// 使用向量化类型一次加载 128 bits (4 floats)
__global__ void vectorized_load(float* A, float* B, int N) {
    int idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < N) {
        // 一次 16-byte load → 单条 LDG 指令
        float4 a_vec = reinterpret_cast<float4*>(&A[idx])[0];
        
        // FMA on each float
        float4 result;
        result.x = a_vec.x * a_vec.x + a_vec.x;
        result.y = a_vec.y * a_vec.y + a_vec.y;
        result.z = a_vec.z * a_vec.z + a_vec.z;
        result.w = a_vec.w * a_vec.w + a_vec.w;
        
        reinterpret_cast<float4*>(&B[idx])[0] = result;
    }
}
```

### 4.3 Shared Memory Bank Conflicts

```cuda
// Shared memory 有 32 个 bank，每个 bank 4 bytes 宽
// 同一周期内，多个线程访问同一 bank 的不同地址 → bank conflict → 串行化

// ❌ 32-way bank conflict: 所有线程访问同一 bank
__global__ void bank_conflict(float* data) {
    __shared__ float shmem[1024];
    
    int tx = threadIdx.x;
    shmem[tx] = data[tx];
    __syncthreads();
    
    // Bank index = (address / 4) % 32
    // 如果 stride = 32, 所有线程都访问 bank 0
    // → 32-way conflict, 32 次重放
    float val = shmem[tx * 32];  // thread 0→bank0, thread1→bank0? 不对...
}

// 正确计算 bank index:
// 每个 float = 4 bytes = 1 bank width
// shmem[0]  → bank 0
// shmem[1]  → bank 1
// ...
// shmem[31] → bank 31
// shmem[32] → bank 0  ← 回到 bank 0!
// shmem[33] → bank 1
//
// 所以 stride=32 导致:
//   thread 0 → shmem[0]   → bank 0
//   thread 1 → shmem[32]  → bank 0  ← conflict!
//   thread 2 → shmem[64]  → bank 0  ← conflict!
//   ...
//   32 个线程访问 32 个不同地址, 但全在 bank 0 → 32-way conflict

// ✅ 解决: 加 padding 打破 stride=32 的对齐
__global__ void no_bank_conflict(float* data) {
    // 每一行加 1 个 float padding
    __shared__ float shmem[32][32 + 1];  // +1 padding
    
    int tx = threadIdx.x;
    
    // 列访问时:
    // shmem[row][col]  → bank = (row * 33 + col) % 32
    // 当 row=0: bank = col % 32 → 无冲突
    // 当 row=1: bank = (33+col) % 32 = (1+col) % 32 → 错位
    // → 消除了同一列对齐到同一 bank 的问题
}

// 通用规则:
// bank = (byte_address / 4) % 32
// conflict 当且仅当: 两个线程访问同一 bank 且不同地址 (同一地址是广播, 无冲突)
```

### 4.4 内存延迟隐藏

```cuda
// GPU 隐藏内存延迟的核心机制: warp 级上下文切换
// 
// 当 warp 发送 LDG (load global) 指令后:
//   1. 指令发射 → LD/ST Unit
//   2. LD/ST Unit 检查 L1 → miss → L2 (或 HBM)
//   3. 在等待期间, warp scheduler 不等待, 立即切换到另一个就绪 warp
//   4. 另一个 warp 的指令可能已就绪 (因为 GPU 有数百个硬件线程)
//   5. 当 load 数据到达 → scoreboard 标记 warp 再次就绪 → 重新被调度
//
// 这就是为什么高 occupancy 对 latency-bound kernel 至关重要:
//   occupancy = 25%: 只有 16 warps, load latency=600 cycles
//                    平均 600/16=37.5 cycles 切换一次
//                    管线可能空闲 (没有就绪 warp)
//   occupancy = 75%: 有 48 warps, load latency=600 cycles
//                    平均 600/48=12.5 cycles 切换一次
//                    管线几乎无空闲

// 量化: 需要多少 warps 隐藏延迟?
// Warps_needed = ceil(latency / throughput_per_warp)
// 例: H100 FP32 吞吐 = 1 FLOP/cycle/core
//     Load 延迟 = 600 cycles
//     Warp 每 32 cycles 消费一条 FMA 指令 (128 FP32 lanes / 4 warps)
//     600 / 32 ≈ 19 warps 即可完全隐藏延迟
```

---

## 5. Tensor Core：矩阵乘法加速器

### 5.1 Tensor Core 演进

```
┌──────────┬───────────────────┬───────────────────────────────────────┐
│ 架构     │ Tensor Core 代    │ 特性                                   │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Volta    │ 1st Gen           │ FP16 input, FP32 accumulate            │
│ (V100)   │ 8 per SM          │ D = A·B + C, m16n16k16 FP16           │
│          │                   │ Throughput: 125 TFLOPS FP16           │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Turing   │ 2nd Gen           │ + INT8, INT4, INT1                    │
│ (T4/RTX) │                   │ + TF32 (TensorFloat-32)               │
│          │                   │ Throughput: 130 TFLOPS TF32           │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Ampere   │ 3rd Gen           │ + BF16, TF32                         │
│ (A100)   │ 4 per SM          │ + Sparsity (2:4) → 2× throughput     │
│          │                   │ Throughput: 312 TFLOPS FP16          │
│          │                   │             624 TFLOPS FP16 sparse   │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Hopper   │ 4th Gen           │ + FP8 (e4m3, e5m2)                   │
│ (H100)   │ 4 per SM          │ + DPX instructions (INT8)            │
│          │                   │ Throughput: 1979 TFLOPS FP8         │
│          │                   │             3958 TFLOPS FP8 sparse  │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Blackwell│ 5th Gen           │ + FP4, FP6                          │
│ (B200)   │                   │ + micro-tensor scaling               │
│          │                   │ Throughput: 4500 TFLOPS FP8          │
├──────────┼───────────────────┼───────────────────────────────────────┤
│ Rubin    │ 6th Gen           │ + FP4 with 4× throughput boost       │
│ (2026)   │                   │ (announced)                           │
└──────────┴───────────────────┴───────────────────────────────────────┘
```

### 5.2 MMA 指令与 PTX

Tensor Core 通过 PTX 指令 `mma.sync` 编程：

```cuda
// H100 Tensor Core mma.sync 指令 (简化 PTX)
// 使用 inline PTX 汇编

// m16n8k16: 每个 warp 计算 16×8 个 C 元素
//           输入: A(m16×k16 fp16), B(k16×n8 fp16)
//           累加: C(m16×n8 fp32)
__global__ void tcore_gemm_fp16(
    half* A, half* B, float* C, int M, int N, int K)
{
    // 每个 warp 维护自己的 16×8 C 分片 (4 个寄存器组)
    // FRAGMENT_A: 8 个 32-bit 寄存器 = 8 × 2 × fp16 = 16 个 fp16 元素
    // FRAGMENT_B: 4 个 32-bit 寄存器 = 4 × 2 × fp16 = 8 个 fp16 元素
    // FRAGMENT_C: 4 个 32-bit 寄存器 = 4 个 fp32 元素 (accumulator)
    
    // 初始化 C fragment 为 0
    float frag_c[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    for (int k_block = 0; k_block < K; k_block += 16) {
        half frag_a[8];
        half frag_b[4];
        
        // 从 shared memory 加载 A 和 B 的 fragment
        load_matrix_fragments(frag_a, frag_b, A, B, k_block);
        
        // Tensor Core PTX 指令 (inline assembly)
        asm volatile(
            "mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
            "{%0, %1, %2, %3}, "
            "{%4, %5, %6, %7}, "
            "{%8, %9}, "
            "{%10, %11, %12, %13};\n"
            : "=f"(frag_c[0]), "=f"(frag_c[1]), "=f"(frag_c[2]), "=f"(frag_c[3])
            : "r"(reinterpret_cast<unsigned&>(frag_a[0])),
              "r"(reinterpret_cast<unsigned&>(frag_a[2])),
              "r"(reinterpret_cast<unsigned&>(frag_a[4])),
              "r"(reinterpret_cast<unsigned&>(frag_a[6])),
              "r"(reinterpret_cast<unsigned&>(frag_b[0])),
              "r"(reinterpret_cast<unsigned&>(frag_b[2])),
              "f"(frag_c[0]), "f"(frag_c[1]), "f"(frag_c[2]), "f"(frag_c[3])
        );
    }
    
    // 写回结果
    store_c_fragment(C, frag_c);
}

// 重要: H100 上 mma.sync 要求同一个 warp 内的所有线程协作
// 不同 lane 持有 fragment 的不同部分
//
// m16n8k16 FP16 fragment 分布:
//   Thread 0:  A[0:1][0:1]*2, B[0:1][0:1]*2   → C[0:1][0:1]
//   Thread 1:  A[0:1][2:3]*2, B[0:1][0:1]*2   → C[0:1][0:1]
//   ... (具体分布见 NVIDIA PTX ISA 文档)
//
// 因此所有 32 个线程必须同时到达 mma.sync，否则死锁
```

### 5.3 FP8 与 Hopper 的吞吐翻倍

```cuda
// H100 支持 FP8 Tensor Core (e4m3 和 e5m2 格式)
// 
// FP8 E4M3: 1 sign, 4 exponent, 3 mantissa = 1+4+3=8 bits
//   Range: min=2^-6 × 2^-2 = 0.00195, max=448 (紧凑格式)
//   Precision: ~3 significant decimal digits
//
// FP8 E5M2: 1 sign, 5 exponent, 2 mantissa = 8 bits
//   Range: min=2^-14 × 2^-2 ≈ 0.000015, max=57344 (宽范围格式)
//   Precision: ~2 significant decimal digits
//
// 通常用法: 
//   E4M3 用于 forward 的 weight 和 activation (需要精度)
//   E5M2 用于 backward 的 gradient (需要范围)
//
// mma.sync 在 H100 上的 FP8 版本:
//   m16n8k32.f32.e4m3.e4m3.f32: A(fp8), B(fp8), C/D(fp32)
//   k=32 (FP8) vs k=16 (FP16) → 矩阵乘积运算量翻倍

// FP8 GEMM kernel (简化)
__global__ void tcore_gemm_fp8(
    __nv_fp8_e4m3* A,   // e4m3 input
    __nv_fp8_e4m3* B,   // e4m3 input
    float* C,            // fp32 output/accumulate
    float* scale_a,      // per-row or per-block quantization scale
    float* scale_b       // per-column scale
) {
    // FP8 需要 scale factor 来恢复数值范围
    // C = (A_dequantized @ B_dequantized)
    // A_dequantized = A_fp8 * scale_a
    // B_dequantized = B_fp8 * scale_b
    
    // Fragment 声明
    float frag_c[4] = {0.0f};  // accumulator
    
    for (int k = 0; k < K; k += 32) {
        // 加载 FP8 fragments
        // H100 上 FP8 fragment:
        //   A fragment: 4 个 32-bit 寄存器 = 4 FP8 元素? 
        //   实际 layout 比 FP16 复杂，详见 PTX ISA
        unsigned frag_a[2];  // 2 × 32-bit = 8 × fp8 elements
        unsigned frag_b[1];  // 1 × 32-bit = 4 × fp8 elements
        
        asm volatile(
            "mma.sync.aligned.m16n8k32.row.col.f32.e4m3.e4m3.f32 "
            "{%0, %1, %2, %3}, "   // D = C accumulator
            "{%4, %5}, "           // A operand (2 registers × 4 fp8 = 8 fp8)
            "{%6}, "               // B operand (1 register × 4 fp8 = 4 fp8)
            "{%7, %8, %9, %10};"   // C accumulator (read-write)
            : "=f"(frag_c[0]), "=f"(frag_c[1]), "=f"(frag_c[2]), "=f"(frag_c[3])
            : "r"(frag_a[0]), "r"(frag_a[1]),
              "r"(frag_b[0]),
              "f"(frag_c[0]), "f"(frag_c[1]), "f"(frag_c[2]), "f"(frag_c[3])
        );
    }
}
```

### 5.4 Tensor Core 编程抽象：wmma 与 mma

```cuda
// NVIDIA 提供了两个级别的 Tensor Core 编程接口:

// 1. wmma (Warp Matrix Multiply Accumulate) — 较高级别
#include <cuda_fp16.h>
#include <mma.h>
using namespace nvcuda;

__global__ void wmma_gemm(half* A, half* B, float* C, int M, int N, int K) {
    // 声明 fragment (自动管理寄存器分配)
    wmma::fragment<wmma::matrix_a, 16, 16, 16, half, wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, 16, 16, 16, half, wmma::col_major> b_frag;
    wmma::fragment<wmma::accumulator, 16, 16, 16, float> c_frag;
    
    // 初始化累加器
    wmma::fill_fragment(c_frag, 0.0f);
    
    // 分块循环
    for (int k = 0; k < K; k += 16) {
        // 从全局或共享内存加载到 fragment
        wmma::load_matrix_sync(a_frag, A + k, 16);  // 简化
        wmma::load_matrix_sync(b_frag, B + k, 16);
        
        // 矩阵乘累加
        wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    
    // 存回结果
    wmma::store_matrix_sync(C, c_frag, N, wmma::mem_row_major);
}

// 2. mma.sync — 较低级别 (PTX 直接映射)
// 更灵活，更多形状选择 (m16n8k16, m16n8k32, m8n8k4 等)
// 需要手动管理 fragment 的寄存器布局
// cuBLAS/cuDNN 底层使用 mma 指令获得最佳性能
```

---

## 6. H100 TMA (Tensor Memory Accelerator)

### 6.1 TMA 解决了什么问题

传统 GPU kernel 的数据搬运模式：

```
传统模式 (LDG + compute):
┌────────┬────────┬────────┬────────┬────────┬────────┐
│ LDG    │ compute│ LDG    │ compute│ LDG    │ compute│
│ addr   │ FMA    │ addr   │ FMA    │ addr   │ FMA    │
│ calc   │        │ calc   │        │ calc   │        │
└────────┴────────┴────────┴────────┴────────┴────────┘

问题:
  1. 地址计算指令消耗 CUDA Core 周期 (有时 50%+ 的指令是地址计算)
  2. 数据经过寄存器 (占用稀有寄存器空间)
  3. 需要 __syncthreads() 来协调 shared memory 写入

TMA 模式 (异步 copy + compute overlap):
┌────────┬────────┬────────┬────────┬────────┬────────┐
│ TMA    │ compute│ TMA    │ compute│ TMA    │ compute│
│ copy   │ FMA    │ copy   │ FMA    │ copy   │ FMA    │
│ (auto) │        │ (auto) │        │ (auto) │        │
└────────┴────────┴────────┴────────┴────────┴────────┘

优势:
  1. 硬件自动计算地址 (免除地址计算指令)
  2. 数据直接 global→shared, 绕过寄存器
  3. 异步 barrier 替代 __syncthreads() (更轻量)
  4. 支持 2D/3D tile copy (矩形块, 自动处理 stride)
```

### 6.2 TMA 编程模型

```cuda
// H100 上使用 TMA 的 GEMM kernel (简化)

#include <cuda/barrier>
#include <cuda/ptx>
using barrier = cuda::barrier<cuda::thread_scope_block>;

#define TILE_M 128
#define TILE_N 128
#define TILE_K 32

__global__ void gemm_with_tma(
    half* A, half* B, float* C, int M, int N, int K)
{
    // ========== TMA Descriptor ==========
    // TMA descriptor 描述从全局内存的数据搬运
    // 在 host 端创建, 通过 constant memory 或 global memory 传递
    
    extern __shared__ __align__(128) char smem[];
    half* A_tile = reinterpret_cast<half*>(smem);
    half* B_tile = A_tile + TILE_M * TILE_K;
    float* C_tile = reinterpret_cast<float*>(B_tile + TILE_K * TILE_N);
    
    // ========== Async Pipeline Barrier ==========
    // H100 支持异步 barrier (token-based)
    // 用于跟踪异步 copy 是否完成 (不需要 __syncthreads)
    
    #pragma nv_diag_suppress static_var_with_dynamic_init
    __shared__ barrier bar;
    
    if (threadIdx.x == 0) {
        init(&bar, blockDim.x);  // 参与 barrier 的线程数
    }
    __syncthreads();  // 确保 barrier 初始化完成
    
    // ========== Main Loop ==========
    float accum[TILE_M * TILE_N / (blockDim.x * blockDim.y)] = {0.0f};
    
    for (int k_idx = 0; k_idx < K; k_idx += TILE_K) {
        // ===== TMA async copy: Global → Shared =====
        // cp.async.bulk: H100 新指令, 硬件管理的批量拷贝
        
        if (threadIdx.x == 0) {
            // 只有 1 个线程发起 TMA copy (硬件完成所有地址计算)
            // A_tile: 2D tile from global memory A
            //   source: A[row_start:row_start+TILE_M][k_idx:k_idx+TILE_K]
            //   destination: A_tile[0:TILE_M][0:TILE_K]
            
            asm volatile(
                "cp.async.bulk.shared::cluster.global.mbarrier::complete_tx::bytes"
                " [%0], [%1], %2, [%3];"
                :: "r"(static_cast<unsigned>(__cvta_generic_to_shared(A_tile))),
                   "l"(A + row_start * K + k_idx),  // global src
                   "n"(TILE_M * TILE_K * sizeof(half)),  // bytes to copy
                   "r"(static_cast<unsigned>(__cvta_generic_to_shared(&bar)))
            );
            
            // B_tile 同理
            asm volatile(
                "cp.async.bulk.shared::cluster.global.mbarrier::complete_tx::bytes"
                " [%0], [%1], %2, [%3];"
                :: "r"(static_cast<unsigned>(__cvta_generic_to_shared(B_tile))),
                   "l"(B + k_idx * N + col_start),
                   "n"(TILE_K * TILE_N * sizeof(half)),
                   "r"(static_cast<unsigned>(__cvta_generic_to_shared(&bar)))
            );
        }
        
        // ===== 等待上一轮 copy 完成 (异步 barrier) =====
        // 第 0 次迭代跳过 (没有上一轮)
        // 第 1 次迭代开始: 等待 k_idx=0 时发起的 copy 完成
        barrier::arrival_token token;
        if (k_idx > 0) {
            token = bar.arrive();       // 标记本线程已就绪
            bar.wait(std::move(token)); // 等待所有线程和所有 copy 完成
            __syncthreads();
        }
        
        // ===== Tensor Core 计算 =====
        // 使用当前 tile 的 A_tile 和 B_tile 做 mma
        // (同时下一轮 k_idx+1 的 TMA copy 在进行)
        // 这就形成了 compute/copy overlap
        
        for (int k_inner = 0; k_inner < TILE_K; k_inner += 16) {
            // mma.sync 指令 ...
            accum[i] += frag_a * frag_b;
        }
    }
    
    // Epilogue: 写回结果 + apply activation
    // ...
}

// 关键启示:
// TMA 对 GEMM 的加速:
//   1. 地址计算指令减少 50-70%
//   2. 寄存器压力降低 (数据不经过寄存器)
//   3. Compute/Copy overlap 隐藏内存延迟
//   4. 2D/3D tile 自动 stride 处理
//
// 适用场景:
//   GEMM (矩阵乘法)
//   Convolution (im2col + GEMM)
//   Flash Attention (tiled softmax)
//   PagedAttention (block-table-based gather)
```

### 6.3 TMA Prefetch

```cuda
// TMA 支持预取到 L2 cache (而非 shared memory)
// 在计算当前 tile 时, 提前把下一个 tile 加载到 L2
// 进一步隐藏 HBM 延迟

// cp.async.bulk.prefetch: 预取到 L2 cache
asm volatile(
    "cp.async.bulk.prefetch.L2.global"
    " [%0], %1;"
    :: "l"(global_next_tile_ptr),
       "n"(tile_size_bytes)
);
```

---

## 7. SM 内部架构图

### 7.1 ASCII：H100 SM 内部结构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           H100 Streaming Multiprocessor (SM)                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         Instruction Cache                           │   │
│  │                           (L0 I-Cache)                              │   │
│  └──────────────────────────────────┬──────────────────────────────────┘   │
│                                     │                                      │
│          ┌──────────────────────────┼──────────────────────────┐           │
│          │              Warp Scheduler (×4)                      │           │
│          │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐ │           │
│          │  │ WS #0   │  │ WS #1   │  │ WS #2   │  │ WS #3   │ │           │
│          │  │ 64 warps │  │ 64 warps│  │ 64 warps│  │ 64 warps│ │           │
│          │  │ per WS   │  │ per WS  │  │ per WS  │  │ per WS  │ │           │
│          │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘ │           │
│          └───────┼─────────────┼─────────────┼─────────────┼───────┘           │
│                  │             │             │             │                   │
│                  └─────────────┼─────────────┼─────────────┘                   │
│                                │             │                                 │
│                    Dispatch (per cycle)       │                                 │
│                ┌───────────────┼──────────────┼───────────────┐                │
│                │               │              │               │                │
│         ┌──────▼──────┐ ┌──────▼──────┐ ┌─────▼──────┐ ┌─────▼──────┐        │
│         │  CUDA Cores │ │  CUDA Cores │ │Tensor Cores│ │ Tensor Cores│       │
│         │  (FP32/INT32│ │  (FP32/INT32│ │  (4th Gen) │ │  (4th Gen)  │       │
│         │   64 lanes) │ │   64 lanes) │ │   1 unit    │ │   1 unit    │       │
│         │  + SFU ×16  │ │  + SFU ×16  │ │(mma.sync)   │ │(mma.sync)   │       │
│         └─────────────┘ └─────────────┘ └────────────┘ └────────────┘        │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                      Register File                                   │  │
│  │                  256 KB (65536 × 32-bit)                             │  │
│  │                   4 banks, 每 bank 64-bit 读口                        │  │
│  │                   每周期可读 4×64-bit = 256-bit = 8 × 32-bit reg     │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│  ┌──────────────────────┐  ┌───────────────────────────────────────────┐  │
│  │    L1 Data Cache     │  │         Shared Memory (SRAM)              │  │
│  │    + Texture Cache   │  │         Up to 227 KB                     │  │
│  │  (配置: 29KB ~ 256KB) │  │         32 banks × 4 bytes               │  │
│  └──────────────────────┘  │         每 bank 每周期 128-bit 读/写      │  │
│                             └───────────────────────────────────────────┘  │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │  Load/Store Unit (LSU)     │  Texture Unit (×4)   │  Special Func    │  │
│  │  Address generation        │  HW bilinear filter  │  Unit (SFU)     │  │
│  │  Global→Shared→Reg 拷贝     │  Texture cache       │  sin,cos,exp,log│  │
│  │  支持 TMA 异步拷贝          │                      │  rcp, rsqrt     │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │  L1 Instruction Cache │ Branch Unit │ Scoreboard │ Barrier Unit      │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

关键数据:
  - 每周期最多 4 条 warp 指令 (4 Warp Schedulers)
  - 每周期 128 FP32 FMA (CUDA Cores) + Tensor Core 并发执行
  - Tensor Core 吞吐与 CUDA Core 吞吐独立 (不同执行单元)
  - Register File: 4 读口 (每周期读 8 个 32-bit 寄存器)
```

### 7.2 GPU 级别架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          H100 GPU Die (Total)                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    GigaThread Engine (GTE)                           │   │
│  │  接收来自 Host 的 kernel launch → 分配 grid → 分发 block 到 TPC     │   │
│  └──────────────────────────────────┬──────────────────────────────────┘   │
│                                     │                                      │
│     ┌───────────────────────────────┼───────────────────────────────┐      │
│     │                               │                               │      │
│  ┌──▼──────┐  ┌──────────┐  ┌──────▼───┐  ┌──────────┐  ┌─────────▼─┐   │
│  │  TPC 0  │  │  TPC 1   │  │  TPC 2   │  │  ...     │  │ TPC 65   │   │
│  │ ┌─────┐ │  │ ┌──────┐ │  │ ┌──────┐ │  │          │  │ ┌──────┐ │   │
│  │ │ SM0 │ │  │ │ SM2  │ │  │ │ SM4  │ │  │          │  │ │SM130 │ │   │
│  │ │ SM1 │ │  │ │ SM3  │ │  │ │ SM5  │ │  │          │  │ │SM131 │ │   │
│  │ └─────┘ │  │ └──────┘ │  │ └──────┘ │  │          │  │ └──────┘ │   │
│  └─────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                       L2 Cache (50 MB)                             │  │
│  │              Crossbar 连接所有 TPC 和显存控制器                      │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│       │            │            │            │            │             │
│  ┌────▼────┐  ┌────▼────┐  ┌────▼────┐  ┌────▼────┐  ┌────▼────┐     │
│  │  HBM3   │  │  HBM3   │  │  HBM3   │  │  HBM3   │  │  HBM3   │     │
│  │ Stack 0 │  │ Stack 1 │  │ Stack 2 │  │ Stack 3 │  │ Stack 4 │     │
│  │ 16 GB   │  │ 16 GB   │  │ 16 GB   │  │ 16 GB   │  │ 16 GB   │     │
│  │1024-bit │  │1024-bit │  │1024-bit │  │1024-bit │  │1024-bit │     │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘     │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │               NVLink 4.0 × 18 (900 GB/s)                          │  │
│  │               PCIe Gen5 × 16 (64 GB/s)                            │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

TPC = Texture Processing Cluster
每个 TPC 包含 2 个 SM
66 TPC × 2 SM/TPC = 132 SM (实际配置: 132 SM, 但并非全部启用)
```

---

## 8. CUDA Graph：消除 Launch Overhead

### 8.1 Kernel Launch 的隐藏开销

```python
# PyTorch 每次调用 CUDA kernel 都需要:
# 1. Python → C++ FFI  (argparse, dispatch)
# 2. ATen dispatcher
# 3. CUDA Runtime API call (cudaLaunchKernel)
# 4. CUDA Driver API call (cuLaunchKernel)
# 5. User mode → Kernel mode switch (ioctl)
# 6. nvidia.ko 填充 pushbuffer
# 7. GPU 读取 pushbuffer 命令
# 总延迟: 5-15 µs per launch

import torch

# 每次都有 launch overhead
for _ in range(1000):
    x = torch.randn(256, 256, device='cuda')
    y = x @ x  # 一个 GEMM kernel launch → ~10 µs overhead

# CUDA Graph: 预先录制整个调用图, 一次 replay
g = torch.cuda.CUDAGraph()
# 第一次: 录制
with torch.cuda.graph(g):
    y = x @ x
# 后续: replay (只需 1 次 launch → 消除 N-1 次 overhead)
g.replay()
```

### 8.2 vLLM 中的 CUDA Graph

```python
# vLLM 利用 CUDA Graph 加速 decode 阶段
# decode 每次只处理 1 个 token，计算量小 → launch overhead 占比大
# 通过录制一套 decode graph 并缓存不同 batch size 的 graph，大幅降低开销

class CUDAGraphRunner:
    """缓存不同 batch_size 对应的 CUDA graph"""
    
    def __init__(self):
        self.graphs: Dict[int, torch.cuda.CUDAGraph] = {}
        self.max_batch_size = 256
    
    def capture(self, model, batch_sizes):
        """预录制不同 batch size 的 graph"""
        for bs in batch_sizes:
            # 创建静态输入 (固定 shape)
            input_ids = torch.zeros(bs, dtype=torch.long, device='cuda')
            positions = torch.zeros(bs, dtype=torch.long, device='cuda')
            
            g = torch.cuda.CUDAGraph()
            with torch.cuda.graph(g):
                # 录制整个 forward pass
                hidden_states = model(
                    input_ids=input_ids, positions=positions
                )
            
            self.graphs[bs] = g
    
    def replay(self, batch_size):
        """回放预录制的 graph"""
        return self.graphs[batch_size].replay()

# 性能:
# 无 CUDA Graph:  每 token ~50 µs (其中 ~10 µs 是 launch overhead)
# 有 CUDA Graph:  每 token ~40 µs (~0 µs launch overhead)
# 提升: ~20% decode 阶段吞吐
```

---

## 9. 性能分析与调优工具

### 9.1 Nsight Compute

```bash
# 分析单个 kernel 的执行细节
ncu --set full \
    --kernel-name my_kernel \
    --launch-count 1 \
    --launch-skip 10 \
    -o profile_output \
    python my_script.py

# 关键指标:
# Memory Throughput: 实际显存带宽利用率 (%)
# Compute Throughput: SM 利用率 (%)
# Occupancy: 活跃 warp / 理论最大 warp
# Registers per Thread: 寄存器压力
# Shared Memory Bank Conflicts: bank 冲突次数
# Warp Divergence: 分支发散程度
# Stall Reasons: 暂停原因 (Long Scoreboard=等内存, 
#                 Barrier=等同步, Not Selected=未选中)
```

### 9.2 Nsight Systems

```bash
# 全局视图: kernel 并发、内存拷贝、API 调用、多 GPU 通信
nsys profile \
    --trace=cuda,cublas,cudnn,nvtx,osrt \
    --cuda-memory-usage=true \
    --gpuctxsw=true \
    -o timeline_output \
    python my_script.py

# 在 GUI 中查看 timeline:
# - 每个 kernel 的启动时间和持续时间
# - kernel 之间是否有 gap (idle time)
# - 是否重叠了 memcpy 和 kernel 执行
# - multi-GPU NCCL 通信的 timeline
```

### 9.3 Occupancy 微调

```cuda
// 编译时指定 -Xptxas -maxrregcount=N 限制寄存器使用
// nvcc -Xptxas -maxrregcount=64 ...

// 或使用 launch_bounds
#define MAX_THREADS_PER_BLOCK 256
#define MIN_BLOCKS_PER_SM 4

__global__ void __launch_bounds__(MAX_THREADS_PER_BLOCK, MIN_BLOCKS_PER_SM)
tuned_kernel(float* data, int N) {
    // 编译器会尝试将寄存器数限制在允许 4 个 block 同时驻留的水平
    // 如果做不到 → 编译器会 spill 到 local memory (L1 缓存)
    // Spill 的代价: ~30 cycles (L1) vs 0 cycles (register)
    //   轻量 spill 可接受, 重度 spill (~50%+ 寄存器溢出) 重创性能
    
    // 手动控制: 通过减小 live variable 范围帮助编译器
    {
        float temp = complex_compute(data[threadIdx.x]);
        data[threadIdx.x] = temp;  // temp 生命周期结束
    }
    
    // 而非:
    // float temp = complex_compute(data[threadIdx.x]);
    // ... 100 more lines of code with temp still alive ...
    // data[threadIdx.x] = temp;  // temp 占用寄存器 100 行
}
```

---

## 10. 总结

H100 GPU 是一个高度并行的计算集群：

1. **132 SM** × 128 CUDA Cores = 16896 FP32 核心，以 SIMT 模式每周期执行 128 条指令
2. **Warp (32 threads)** 是调度和组织的基本单位，Divergence 是其核心性能陷阱 — 同一 warp 内分支会使部分线程闲置
3. **内存层次**：Register (0 cycle) → Shared Mem (20 cycles) → L1 (30) → L2 (200) → HBM3 (500-800)。正确利用 Shared Memory 和非合并访问优化可带来 10-100× 的性能差异
4. **Tensor Core**：warp 级矩阵乘法加速器，FP8 下 H100 达到 1979 TFLOPS — 是 Transformer 推理的核心引擎
5. **TMA**：Hopper 新特性，硬件自主进行 global→shared 批量拷贝，消除地址计算指令，实现 compute/copy overlap

下一篇文章中，我们将把这 5 篇文章的知识串联起来，构建一条从 `model.forward()` 到 GPU 晶体管的全链路调用链，并给出完整的时序分析。

---

## 下一篇文章

[第六篇：完整管线图：从 PyTorch 到 GPU 硬件 — 端到端时序与调用链](./06-full-pipeline.md)
