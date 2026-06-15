# 第三篇：加速库三剑客：cuBLAS、cuDNN、NCCL

系列目录：[PyTorch → NVIDIA GPU 全链路深度解析](./README.md)

---

## 1. cuBLAS — 矩阵乘法与 Tensor Core

### 1.1 GEMM：深度学习的工作引擎

```
GEMM (General Matrix Multiply):
  C = α · A · B + β · C

其中:
  A: M × K 矩阵
  B: K × N 矩阵
  C: M × N 矩阵
  α, β: 标量

在神经网络中的映射:
  - Linear (全连接层): Y = X · W^T + b
    → M = batch_size, K = in_features, N = out_features

  - Attention: Q = X · W_Q, K = X · W_K, V = X · W_V
    → 三个独立的 GEMM

  - Attention scores: S = Q · K^T / √d
    → M = seq_len, K = d_head, N = seq_len

  - Attention output: O = softmax(S) · V
    → M = seq_len, K = seq_len, N = d_head
```

### 1.2 cuBLAS API 演进

```cpp
// 第一代: cuBLAS Legacy API (CUDA 1.0+)
// 只支持单精度和双精度
cublasHandle_t handle;
cublasCreate(&handle);

// SGEMM: Single-precision GEneral Matrix Multiply
float alpha = 1.0f, beta = 0.0f;
int m = 1024, n = 1024, k = 1024;
int lda = k, ldb = n, ldc = n;

cublasSgemm(handle,
            CUBLAS_OP_N,      // A 不转置
            CUBLAS_OP_N,      // B 不转置
            m, n, k,
            &alpha,
            d_A, lda,         // A: m×k
            d_B, ldb,         // B: k×n
            &beta,
            d_C, ldc);        // C: m×n

// cuBLAS 使用列优先 (Fortran order)
// PyTorch 使用行优先 (C order)
// 因此: 调用时 A^T 和 B^T, 实际上 cuBLAS 计算的是 B^T · A^T = (A·B)^T
// 或者: cublasSgemm(CUBLAS_OP_T, CUBLAS_OP_N, ... A ... B ...)
//       实际计算: A^T · B → 结果需要 view/transpose 为 PyTorch 预期形状

// 第二代: cublasGemmEx (CUDA 8.0+)
// 支持混合精度, Tensor Core 自动启用
cublasGemmEx(handle,
             CUBLAS_OP_N, CUBLAS_OP_N,
             m, n, k,
             &alpha,
             d_A, CUDA_R_16F, lda,   // fp16 A
             d_B, CUDA_R_16F, ldb,   // fp16 B
             &beta,
             d_C, CUDA_R_16F, ldc,   // fp16 C
             CUBLAS_COMPUTE_32F,     // 内部累加用 fp32
             CUBLAS_GEMM_DEFAULT);

// 第三代: cublasLtMatmul (CUDA 10.1+, modern API)
// 自动调优, epilogue fusion, 灵活的数据布局
cublasLtMatmulDesc_t matmulDesc;
cublasLtMatrixLayout_t Adesc, Bdesc, Cdesc;
cublasLtMatmulPreference_t preference;

cublasLtMatmulDescCreate(&matmulDesc,
                          CUBLAS_COMPUTE_32F, CUDA_R_32F);
cublasLtMatrixLayoutCreate(&Adesc, CUDA_R_16F, m, k, lda);
cublasLtMatrixLayoutCreate(&Bdesc, CUDA_R_16F, k, n, ldb);
cublasLtMatrixLayoutCreate(&Cdesc, CUDA_R_16F, m, n, ldc);

// Epilogue fusion: 在 GEMM 后融合 bias + relu
cublasLtEpilogue_t epilogue = CUBLASLT_EPILOGUE_RELU_BIAS;
cublasLtMatmulDescSetAttribute(matmulDesc,
                                CUBLASLT_MATMUL_DESC_EPILOGUE,
                                &epilogue, sizeof(epilogue));

// 自动调优: cuBLAS 内部搜索最优 tile size 和 kernel
cublasLtMatmulPreferenceCreate(&preference);
cublasLtMatmulPreferenceSetAttribute(preference,
    CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES,
    &workspaceSize, sizeof(workspaceSize));

cublasLtMatmulAlgoGetHeuristic(handle, matmulDesc,
                                Adesc, Bdesc, Cdesc, Cdesc,
                                preference, 1, &heuristic, &ret);
// heuristic.algo 包含最优算法

cublasLtMatmul(handle, matmulDesc,
               &alpha, d_A, Adesc,
                       d_B, Bdesc,
               &beta,  d_C, Cdesc,
                       d_C, Cdesc,
               &heuristic.algo,
               d_workspace, workspaceSize,
               stream);

// PyTorch 内部使用的就是 cublasLtMatmul:
// aten/src/ATen/native/cuda/Blas.cpp
```

### 1.3 Tensor Core 内部原理

Tensor Core 是 Volta (V100) 架构引入的专用硬件单元，执行 `D = A · B + C` 矩阵乘累加：

```
SM 内的 Tensor Core 分布:

Volta (V100):
  每个 SM: 8 个 Tensor Core
  每个 Tensor Core/clock: 4×4×4 矩阵乘累加 (fp16×fp16→fp32)
  每个 SM 峰值: 125 TFLOPS (fp16)

Ampere (A100):
  每个 SM: 4 个 Tensor Core
  每个 Tensor Core/clock: 8×4×8 矩阵乘累加
  支持: fp16, bf16, tf32, int8, int4, int1 (稀疏)
  每个 SM 峰值: 312 TFLOPS (fp16, 非稀疏)

Hopper (H100):
  每个 SM: 4 个 Tensor Core
  支持: fp8 (E4M3, E5M2), fp16, bf16, tf32, fp64
  新增: Transformer Engine (自动缩放 fp8)
  每个 SM 峰值: 1000 TFLOPS (fp8)
```

**Tensor Core 分块计算流程**：

```
矩阵分块 (Tiling) 策略:

A (M×K):                    B (K×N):
┌──────┬──────┬───┐         ┌──────┬──────┬───┐
│ Tile │ Tile │   │         │ Tile │ Tile │   │
│ A_00 │ A_01 │...│         │ B_00 │ B_01 │...│
├──────┼──────┼───┤         ├──────┼──────┼───┤
│ Tile │      │   │         │ Tile │      │   │
│ A_10 │      │   │         │ B_10 │      │   │
├──────┼──────┼───┤         ├──────┼──────┼───┤
│  ... │      │   │         │  ... │      │   │
└──────┴──────┴───┘         └──────┴──────┴───┘

每个 Tile 大小: 128×128 (由 cuBLAS 自动选择)

C (M×N):
┌──────┬──────┬───┐
│ C_00 │ C_01 │...│    C_ij = α·(A_i0·B_0j + A_i1·B_1j + ...) + β·C_ij
├──────┼──────┼───┤
│ C_10 │      │   │
├──────┼──────┼───┤
│  ... │      │   │
└──────┴──────┴───┘

Tensor Core 执行的矩阵积木:

WMMA (Warp Matrix Multiply-Accumulate) 指令:
──────────────────────────────────────────
每个 warp (32 threads) 协作完成一个分块乘法

Thread 0-7:   加载 A_tile[0:16, 0:16] 到寄存器
Thread 8-15:  加载 B_tile[0:16, 0:16] 到寄存器
Thread 16-23: 加载 C_tile[0:16, 0:16] 到寄存器
Thread 24-31: 存储结果

WMMA 指令:
  wmma::mma_sync(accum, a_frag, b_frag, accum);
  // 1 条指令 = 16×16×16 矩阵乘法 = 4096 次乘加操作
  // 等效 4096 次 FMA (fused multiply-add)

Fragment 布局 (Ampere, fp16):
  A fragment: 8 个 32-bit 寄存器 (每线程)
  B fragment: 8 个 32-bit 寄存器
  C/D fragment: 4 个 32-bit 寄存器

  线程 0: A[0:4, 0:2], B[0:2, 0:4], C[0:4, 0:2]
  线程 1: A[0:4, 2:4], B[0:2, 4:8], C[0:4, 2:4]
  ...

  Thread 0 holds:        Thread 1 holds:
  A: a00 a01     B: b00 b01 b02 b03
     a10 a11        b10 b11 b12 b13
     a20 a21     C: c00 c01
     a30 a31        c10 c11
                    c20 c21
                    c30 c31
```

### 1.4 cublasLtMatmul 的自动调优

```cpp
// cuBLASLt 内部的搜索过程
// 调优空间:
struct TuningSpace {
    // 1. Tile 大小
    int tile_m, tile_n, tile_k;  // 常见: 64, 128, 256

    // 2. Warp 级分块
    int wmma_m, wmma_n, wmma_k; // 16×16×16 (fp16), 16×8×16 (int8)

    // 3. 数据加载策略
    enum class LoadStrategy {
        SHARED_MEM,    // 先加载到共享内存
        GLOBAL_LOAD,   // 直接从全局内存
        LDGSTS,        // async copy (Ampere+, cp.async)
    };

    // 4. 软件流水线级数
    int pipeline_stages;  // 1-5

    // 5. 线程块配置
    int threads_per_block; // 128, 256, 512
    int warps_per_block;

    // 6. 计算精度
    cublasComputeType_t compute_type;
};

// 启发式搜索:
cublasLtMatmulAlgoGetHeuristic() {
    for (auto& strategy : strategy_cache) {
        if (meets_requirements(strategy, m, n, k, dtype)) {
            // 检查对齐要求:
            // - 16-byte alignment for fp16 Tensor Cores
            // - 4-byte alignment for fp32
            // - 8/16-byte for vectorized loads
            ...
            return strategy;
        }
    }
    // 如果没有缓存的策略，执行搜索
    for (auto& algo : enumerate_algos()) {
        algo.measure_performance(m, n, k, dtype);
    }
    return best_algo;
}
```

### 1.5 cuBLAS 完整使用示例：实现一个 Linear Layer

```cpp
#include <cublasLt.h>
#include <cuda_fp16.h>

class CublasLinear {
    cublasLtHandle_t ltHandle;
    cublasLtMatmulDesc_t matmulDesc;
    cublasLtMatrixLayout_t weightLayout;
    cublasLtMatmulAlgo_t algo;
    void* d_workspace;
    size_t workspaceSize;

    half* d_weight;  // out_features × in_features (column-major)
    half* d_bias;    // out_features
    int in_features, out_features;

public:
    CublasLinear(int in_f, int out_f)
        : in_features(in_f), out_features(out_f)
    {
        cublasLtCreate(&ltHandle);

        // 输入: batch × in_features (row-major → column-major)
        // 权重: in_features × out_features (column-major)
        // 输出: batch × out_features

        // 设置输入矩阵布局 (batch × in_features, column-major)
        // 设置权重布局 (in_features × out_features, column-major)
        cublasLtMatrixLayoutCreate(&weightLayout,
                                    CUDA_R_16F,
                                    in_features,  // rows
                                    out_features,  // cols
                                    in_features);  // ld

        // 设置 matmul 描述符
        cublasLtMatmulDescCreate(&matmulDesc,
                                  CUBLAS_COMPUTE_32F,
                                  CUDA_R_32F);  // output = fp32

        // Epilogue: C = relu(A·B + bias)
        cublasLtEpilogue_t epilogue = CUBLASLT_EPILOGUE_RELU_BIAS;
        cublasLtMatmulDescSetAttribute(matmulDesc,
            CUBLASLT_MATMUL_DESC_EPILOGUE, &epilogue, sizeof(epilogue));

        // 分配权重和偏置
        cudaMalloc(&d_weight, in_features * out_features * sizeof(half));
        cudaMalloc(&d_bias, out_features * sizeof(half));

        // 预调优
        tune(1024);  // tune for batch_size=1024
    }

    void tune(int batch_size) {
        cublasLtMatmulPreference_t preference;
        cublasLtMatmulPreferenceCreate(&preference);

        // 允许最多 4MB workspace
        workspaceSize = 4 * 1024 * 1024;
        cublasLtMatmulPreferenceSetAttribute(preference,
            CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES,
            &workspaceSize, sizeof(workspaceSize));

        int returnedResults = 0;
        cublasLtMatmulHeuristicResult_t heuristic;
        cublasLtMatmulAlgoGetHeuristic(ltHandle, matmulDesc,
            weightLayout, weightLayout, weightLayout, weightLayout,
            preference, 1, &heuristic, &returnedResults);

        if (returnedResults > 0) {
            algo = heuristic.algo;
        }
        cublasLtMatmulPreferenceDestroy(preference);

        if (workspaceSize > 0) {
            cudaMalloc(&d_workspace, workspaceSize);
        }
    }

    void forward(const half* d_input,  // batch × in_features
                 float* d_output,      // batch × out_features
                 int batch_size,
                 cudaStream_t stream) {
        // 构造输入布局
        cublasLtMatrixLayout_t inputLayout;
        cublasLtMatrixLayoutCreate(&inputLayout, CUDA_R_16F,
                                    batch_size,    // rows
                                    in_features,   // cols
                                    batch_size);   // ld

        // 构造输出布局
        cublasLtMatrixLayout_t outputLayout;
        cublasLtMatrixLayoutCreate(&outputLayout, CUDA_R_32F,
                                    batch_size,
                                    out_features,
                                    batch_size);

        // 执行 matmul: output = relu(input · weight + bias)
        half alpha = __float2half(1.0f);
        half beta  = __float2half(0.0f);

        cublasLtMatmul(ltHandle,
                       matmulDesc,
                       &alpha,
                       d_input,       inputLayout,
                       d_weight,      weightLayout,
                       &beta,
                       d_output,      outputLayout,
                       d_output,      outputLayout,  // in-place
                       &algo,
                       d_workspace, workspaceSize,
                       stream);

        cublasLtMatrixLayoutDestroy(inputLayout);
        cublasLtMatrixLayoutDestroy(outputLayout);
    }

    void set_weights(const half* w, const half* b) {
        cudaMemcpy(d_weight, w,
                   in_features * out_features * sizeof(half),
                   cudaMemcpyHostToDevice);
        cudaMemcpy(d_bias, b,
                   out_features * sizeof(half),
                   cudaMemcpyHostToDevice);
    }
};
```

---

## 2. cuDNN — 卷积与深度神经网络

### 2.1 卷积的不同算法

cuDNN 提供多种卷积实现，每种适用于不同的 shape/stride/dilation：

```
┌────────────────────────────────────────────────────────────────┐
│  Convolution Algorithms in cuDNN                               │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  1. IMPLICIT_GEMM                                              │
│     - 将输入转成矩阵 (im2col)，用 GEMM 计算卷积                │
│     - 适用: 任意卷积参数                                       │
│     - 内存: 高 (需要 im2col 矩阵副本)                          │
│     - 速度: 中等                                               │
│                                                                │
│  2. IMPLICIT_PRECOMP_GEMM                                      │
│     - 预计算 offset 矩阵，避免显式 im2col                      │
│     - 适用: 固定 shape 的重复调用                               │
│     - 内存: 较低 (不存储大矩阵，只存 offset)                    │
│     - 速度: 快                                                 │
│                                                                │
│  3. GEMM                                                       │
│     - 显式的 im2col + GEMM                                     │
│     - 适用: 当 batch size 很大时 (分摊 im2col 开销)            │
│     - 内存: 最高                                               │
│                                                                │
│  4. DIRECT                                                     │
│     - 直接卷积，不转换为 GEMM                                  │
│     - 使用预计算的索引表                                       │
│     - 适用: 较小的 filter (1×1, 3×3)                           │
│     - 内存: 最低                                               │
│                                                                │
│  5. FFT (Fast Fourier Transform)                                │
│     - 卷积定理: conv(x, w) = IFFT(FFT(x) · FFT(w))             │
│     - 适用: 大 filter (7×7, 11×11) 和大输出                    │
│     - 计算: O(N log N) vs O(N^2)                               │
│     - 内存: 中间 (存储复数)                                    │
│     - 精度: 浮点误差累积                                       │
│                                                                │
│  6. WINOGRAD                                                   │
│     - 代数变换: 用更多的加法换更少的乘法                        │
│     - 适用: 3×3 filter, stride=1                               │
│     - F(2×2, 3×3): 2.25× speedup (16 mul 替代 36 mul)          │
│     - F(4×4, 3×3): 4.0× speedup (36 mul 替代 144 mul)          │
│     - 内存: 较高 (transform 后的 tile 更大)                    │
│     - 精度: 数值稳定性需注意                                   │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 Implicit GEMM 详解

传统 GEMM-based 卷积的做法：

```
im2col 变换:

输入 (C_in × H_in × W_in):
┌───────────┐
│ a b c d  │
│ e f g h  │  ← 2×4 输入
│          │    filter: 2×2, C_in=1, stride=1
└───────────┘

展开为 im2col 矩阵 (patches × K):
                                  matrix cols = C_in * K_h * K_w
┌──────────────────────────────┐
│ a b e f │  patch at (0,0)     │
│ b c f g │  patch at (0,1)     │
│ c d g h │  patch at (0,2)     │
└──────────────────────────────┘
  3 patches × 4 elements

filter 展开:
  [w1 w2 w3 w4]  ← 1×4 行向量

卷积 = im2col_matrix × filter^T = [patch1·w, patch2·w, patch3·w]

Implicit GEMM 优化:
──────────────────
不显式构建 im2col 矩阵
而是预计算每个 patch 的内存偏移 (offset table)

Kernel 伪代码:
  for each output position (x, y):
    float sum = 0;
    for each input channel c:
      for each filter element (ky, kx):
        // 用 offset 表直接索引原始输入
        int input_idx = offset_table[(x,y)][c][ky][kx];
        sum += input[input_idx] * filter[c][ky][kx];
    output[x][y] = sum;

优势: 省去 im2col 的写和读内存 → 内存带宽节省 ~50%
```

### 2.3 Winograd 卷积：代数优化

Winograd 最小滤波算法用更多的加法换更少的乘法：

```
标准卷积 (3×3 filter, 2×2 output):
  需要 3×3×2×2 = 36 次乘法

Winograd F(2×2, 3×3):
  输入变换: U = G · g · G^T  (滤波器变换,  离线完成)
  输出变换: V = B^T · d · B  (数据变换,    在线计算)
  逐元素乘: M = U ⊙ V        (Hadamard 积,  4 个 tile × 4 = 16 次乘法)
  结果:     Y = A^T · M · A  (逆变换,      输出)

  A, B, G 为预定义的变换矩阵 (由多项式求值点决定)
  总共: 16 次乘法 + 更多加法 = 比 36 次乘法快

变换矩阵 (F(2,3)):
       ┌           ┐       ┌               ┐
       │ 1  0  0  │       │ 1  -1  -1  0  │
  G =  │ 1  1  1  │  B^T =│ 0   1  -1  0  │
       │ 0  1 -1  │       │ 0   0   1  0  │
       │ 0  0  1  │       │ 0   0   0  1  │
       └           ┘       └               ┘

       ┌         ┐
       │ 1  0  0 │
  A^T =│ 1  1  1 │
       │ 0  1 -1 │
       └         ┘

Pseudo code for Winograd convolution:

def winograd_conv2d(input, filter):
    // Step 1: 变换 filter (可以预计算)
    U = G @ filter @ G.T    // 每个 output channel 独立

    // Step 2: 变换 input tile
    for each tile (4×4 input for F(2,3)):
        V = B.T @ input_tile @ B

        // Step 3: Hadamard 积 (逐元素乘)
        M = U ⊙ V   // 16 次乘法 / tile

        // Step 4: 逆变换
        output_tile = A.T @ M @ A

    // Step 5: 拼合所有 tile
    return assemble(output_tiles)
```

### 2.4 cuDNN 9.0 Graph API

cuDNN 9.0 引入了图 API，将多个算子融合为单个 kernel：

```cpp
// cuDNN 9.0 融合图示例:
// Conv2D + Bias + ReLU + BatchNorm → 单个 kernel

// 传统方式 (4 个 kernel):
//   conv ← kernel 1 (从 HBM 读, 写 HBM)
//   bias ← kernel 2 (读 HBM, 写 HBM)
//   relu ← kernel 3 (读 HBM, 写 HBM)
//   bn   ← kernel 4 (读 HBM, 写 HBM)
//   bandwidth: 4× 读 + 4× 写

// cuDNN Graph (1 个 kernel):
//   fused_conv_bias_relu_bn ← 单个 kernel
//   bandwidth: 1× 读 + 1× 写
//   speedup: 2-4× (memory-bound 情况下)

cudnnHandle_t handle;
cudnnCreate(&handle);

// 创建图
cudnn_frontend::graph::Graph graph;

// 定义张量
auto X = graph.tensor_like("X", CUDNN_TENSOR_NCHW, ...);     // input
auto W = graph.tensor_like("W", CUDNN_TENSOR_NCHW, ...);     // filter
auto B = graph.tensor_like("B", CUDNN_TENSOR_NCHW, ...);     // bias
auto Y = graph.tensor_like("Y", CUDNN_TENSOR_NCHW, ...);     // output

auto scale = graph.tensor_like("scale", ...);                 // BN scale
auto bias_bn = graph.tensor_like("bias_bn", ...);             // BN bias
auto mean = graph.tensor_like("mean", ...);                   // running mean
auto var = graph.tensor_like("var", ...);                     // running var

// 定义操作链
auto conv_output = graph.Conv_fprop(X, W, B, Y,
    {padding}, {stride}, {dilation});

auto relu_output = graph.Pointwise_relu(conv_output);

auto bn_output = graph.Batchnorm_fprop_inference(
    relu_output, scale, bias_bn, mean, var, epsilon);

// 验证和执行
graph.validate();
graph.build_operation_graph(handle);

std::unordered_map<..., void*> var_pack = {
    {X, d_X},
    {W, d_W},
    {B, d_B},
    ...
};

graph.execute(handle, var_pack);

// cuDNN 内部将整个图编译为一个或多个 CUDA kernel
```

### 2.5 cuDNN 算法选择的完整示例

```cpp
#include <cudnn.h>

void configure_convolution(cudnnHandle_t& handle,
                           int batch, int in_c, int in_h, int in_w,
                           int out_c, int out_h, int out_w,
                           int k_h, int k_w,
                           int pad_h, int pad_w,
                           int stride_h, int stride_w) {

    // Step 1: 创建张量描述符
    cudnnTensorDescriptor_t x_desc, y_desc;
    cudnnFilterDescriptor_t w_desc;
    cudnnConvolutionDescriptor_t conv_desc;

    cudnnCreateTensorDescriptor(&x_desc);
    cudnnCreateTensorDescriptor(&y_desc);
    cudnnCreateFilterDescriptor(&w_desc);
    cudnnCreateConvolutionDescriptor(&conv_desc);

    cudnnSetTensor4dDescriptor(x_desc, CUDNN_TENSOR_NCHW,
                                CUDNN_DATA_HALF,  // 输入 fp16
                                batch, in_c, in_h, in_w);
    cudnnSetTensor4dDescriptor(y_desc, CUDNN_TENSOR_NCHW,
                                CUDNN_DATA_HALF,
                                batch, out_c, out_h, out_w);
    cudnnSetFilter4dDescriptor(w_desc, CUDNN_DATA_HALF,
                                CUDNN_TENSOR_NCHW,
                                out_c, in_c, k_h, k_w);
    cudnnSetConvolution2dDescriptor(conv_desc,
                                     pad_h, pad_w,
                                     stride_h, stride_w,
                                     1, 1,  // dilation
                                     CUDNN_CONVOLUTION, CUDNN_DATA_FLOAT);

    // Step 2: 获取所有可用算法
    const int max_algo_count = 8;
    int returned_algo_count;
    cudnnConvolutionFwdAlgoPerf_t algo_perf[max_algo_count];

    cudnnFindConvolutionForwardAlgorithm(
        handle,
        x_desc, w_desc, conv_desc, y_desc,
        max_algo_count,
        &returned_algo_count,
        algo_perf);

    // Step 3: 选择最快的算法
    cudnnConvolutionFwdAlgo_t best_algo = algo_perf[0].algo;
    float best_time = algo_perf[0].time;

    for (int i = 1; i < returned_algo_count; i++) {
        if (algo_perf[i].status == CUDNN_STATUS_SUCCESS &&
            algo_perf[i].time < best_time) {
            best_algo = algo_perf[i].algo;
            best_time  = algo_perf[i].time;
        }
        printf("  Algorithm %d: %.3f ms, memory: %zu\n",
               algo_perf[i].algo,
               algo_perf[i].time,
               algo_perf[i].memory);
    }
    printf("  Best: %d (%.3f ms)\n", best_algo, best_time);

    // Step 4: 分配 workspace
    size_t workspace_size;
    cudnnGetConvolutionForwardWorkspaceSize(
        handle, x_desc, w_desc, conv_desc, y_desc,
        best_algo, &workspace_size);

    void* d_workspace;
    cudaMalloc(&d_workspace, workspace_size);

    // Step 5: 执行卷积
    float alpha = 1.0f, beta = 0.0f;
    cudnnConvolutionForward(handle,
                            &alpha,
                            x_desc, d_x,
                            w_desc, d_w,
                            conv_desc, best_algo,
                            d_workspace, workspace_size,
                            &beta,
                            y_desc, d_y);

    // 清理
    cudaFree(d_workspace);
    cudnnDestroyTensorDescriptor(x_desc);
    cudnnDestroyTensorDescriptor(y_desc);
    cudnnDestroyFilterDescriptor(w_desc);
    cudnnDestroyConvolutionDescriptor(conv_desc);
}
```

### 2.6 cuDNN 调优参数汇总

```
卷积性能影响因素:
══════════════════

1. 数据格式 (layout)
   NCHW  vs  NHWC
   NHWC 在 Tensor Core 上通常更快 (连续通道访问)

2. 算子融合
   单独 conv → 融合 conv+bias+relu+bn
   减少 75% 的显存读写

3. 精度混合
   fp16 compute + fp32 accumulate (Tensor Core)
   fp8 training (H100 Transformer Engine)

4. Tile 大小
   小的 tile → 更好的 cache 局域性, 但更多 tile 开销
   大的 tile → 更少的 tile 开销, 但更多的寄存器压力

5. 内存对齐
   128-byte 对齐 (cachelines)
   避免 bank conflicts

6. Batch 聚合
   多个小 batch 合并为一个大 batch → 提升 GPU 利用率

7. 确定性 vs 性能
   CUDNN_DETERMINISTIC: 保证相同输入产生相同输出
   CUDNN_NON_DETERMINISTIC: 允许使用更快但不保证确定性的算法
```

---

## 3. NCCL — 多 GPU 通信

### 3.1 All-Reduce 的三种经典算法

NCCL 实现了多种集合通信算法，根据 GPU 数量和互联拓扑自动选择：

```
场景: 4 个 GPU, 每个有张量 G[0], G[1], G[2], G[3]
目标: all-reduce (sum), 结果 = G[0]+G[1]+G[2]+G[3]

═══════════════════════════════════════════════════════════════

算法 1: Ring All-Reduce
───────────────────────
拓扑: GPU0 → GPU1 → GPU2 → GPU3 → GPU0 (环形)

Phase 1: Reduce-Scatter (N-1 = 3 步)
─────────────────────────────────────
Step 1: 每个 GPU 发送 1/N chunk 给下一个 GPU
  GPU0 → GPU1: chunk_0_of_G0
  GPU1 → GPU2: chunk_1_of_G1
  GPU2 → GPU3: chunk_2_of_G2
  GPU3 → GPU0: chunk_3_of_G3

Step 2: GPU 接收后累加，发送不同的 chunk
  GPU1: recv chunk_0_of_G0, add to G1[0], send chunk_0_of_G1 → GPU2
  GPU2: recv chunk_1_of_G1, add to G2[1], send chunk_1_of_G2 → GPU3
  ...

Step 3: 同 Step 2

Phase 2: All-Gather (N-1 = 3 步)
────────────────────────────────
  将完整的结果分布到所有 GPU

总通信量: 2(N-1)/N × data ≈ 2 × data (N 大时)
步数: 2(N-1)
带宽最优 (每个 GPU 同时收发，PCIe/NVLink 双向)

ASCII 图:
  GPU0 ──chunk0──► GPU1 ──chunk0+1──► GPU2 ──chunk0+1+2──► GPU3
    ▲                                                         │
    └───────────────────chunk0+1+2+3──────────────────────────┘

═══════════════════════════════════════════════════════════════

算法 2: Tree All-Reduce
───────────────────────
拓扑: 二分/多叉树

Phase 1: Reduce (logN 步)
──────────────────────────
  GPU0, GPU1 → GPU0 (求和)
  GPU2, GPU3 → GPU2 (求和)
  GPU0, GPU2 → GPU0 (求和，GPU0 拥有完整结果)

Phase 2: Broadcast (logN 步)
────────────────────────────
  GPU0 → GPU2
  GPU0 → GPU1, GPU2 → GPU3

总通信量: 2 × data
步数: 2 log₂(N)
Latency 最优 (对数步数 vs 线性步数)

ASCII 图:
        GPU0 (root)
       /          \
    GPU0          GPU2
   /    \        /    \
 GPU0   GPU1  GPU2   GPU3
  ↓ reduce 阶段上卷
        GPU0 (= sum of all)
       /          \
    GPU0          GPU2
   /    \        /    \
 GPU0   GPU1  GPU2   GPU3
  ↓ broadcast 阶段下推

═══════════════════════════════════════════════════════════════

算法 3: CollNet (NCCL 2.4+)
───────────────────────────
利用 NVSwitch 的全互联拓扑
所有 GPU 同时通信，一步完成 reduce

条件: 需要 NVSwitch (DGX-2, DGX A100, HGX H100)
      所有 GPU 通过 NVSwitch 全互联

总通信量: 2 × data
步数: 1 (reduce) + 1 (broadcast)
带宽: 单个 GPU 全 NVLink 带宽 (900 GB/s per GPU)

ASCII 图 (NVSwitch):
  GPU0 ──┐
  GPU1 ──┼──► [NVSwitch] ── reduce ──► 所有 GPU
  GPU2 ──┤         │
  GPU3 ──┘    full crossbar, 所有 GPU 同时发送/接收
```

### 3.2 NCCL API 使用

```cpp
#include <nccl.h>

// ===== 初始化 =====
ncclComm_t comm;
ncclUniqueId id;

// Rank 0 生成唯一 ID
if (rank == 0) {
    ncclGetUniqueId(&id);
}
// 广播给所有进程 (通过 MPI, TCP 等)
MPI_Bcast(&id, sizeof(ncclUniqueId), MPI_BYTE, 0, MPI_COMM_WORLD);

// 所有进程创建 communicator
ncclCommInitRank(&comm, nRanks, id, rank);

// ===== All-Reduce (求和) =====
float* sendbuf;
float* recvbuf;
cudaMalloc(&sendbuf, count * sizeof(float));
cudaMalloc(&recvbuf, count * sizeof(float));

// 执行 all-reduce
ncclAllReduce(sendbuf, recvbuf, count, ncclFloat,
              ncclSum, comm, stream);

// 等价于:
// recvbuf[i] = sendbuf_rank0[i] + sendbuf_rank1[i] + ... + sendbuf_rankN[i]

// ===== All-Gather =====
// 从所有 GPU 收集数据 (每个 GPU 贡献 local_size)
size_t local_size = count / nRanks;
ncclAllGather(sendbuf, recvbuf, local_size, ncclFloat,
              comm, stream);
// recvbuf 现在包含所有 GPU 的数据

// ===== Broadcast =====
ncclBroadcast(sendbuf, recvbuf, count, ncclFloat,
              0,  // root rank
              comm, stream);

// ===== Reduce =====
// 结果只在 root rank 上
ncclReduce(sendbuf, recvbuf, count, ncclFloat,
           ncclSum, 0, comm, stream);

// ===== Reduce-Scatter =====
// 每个 rank 得到结果的一个不同片段
ncclReduceScatter(sendbuf, recvbuf, count / nRanks,
                  ncclFloat, ncclSum,
                  comm, stream);

// ===== Point-to-Point (Send/Recv) =====
ncclSend(sendbuf, count, ncclFloat, peer_rank, comm, stream);
ncclRecv(recvbuf, count, ncclFloat, peer_rank, comm, stream);

// ===== 销毁 =====
ncclCommDestroy(comm);
```

### 3.3 NCCL 拓扑检测与通信路由

NCCL 在初始化时自动检测 GPU 拓扑，选择最优通信路径：

```cpp
// NCCL 内部的拓扑发现 (简化)
// src/nccl/topo.cc

struct NcclTopo {
    struct Link {
        float bandwidth;   // GB/s
        float latency;     // µs
        int type;          // NVLink, PCIe, network (IB/RoCE)
        bool supportP2P;   // 是否支持 P2P 直接访问
    };

    // GPU 之间的邻接矩阵
    Link paths[MAX_GPUS][MAX_GPUS];

    void detect() {
        // Step 1: 枚举所有 GPU
        int nDevices;
        cudaGetDeviceCount(&nDevices);

        // Step 2: 检测 NVLink 连接
        for (int i = 0; i < nDevices; i++) {
            for (int j = i+1; j < nDevices; j++) {
                // 检查 NVLink P2P
                int access;
                cudaDeviceCanAccessPeer(&access, i, j);

                if (access) {
                    // 通过 NVLink topology 获取带宽
                    paths[i][j].bandwidth = getNvlinkBandwidth(i, j);
                    paths[i][j].type = LINK_NVLINK;
                } else {
                    // 检查 PCIe P2P
                    access = checkPcieP2P(i, j);
                    if (access) {
                        paths[i][j].bandwidth = getPcieBandwidth();
                        paths[i][j].type = LINK_PCIE;
                    } else {
                        // 通过 CPU 中转
                        paths[i][j].bandwidth = getHostBandwidth();
                        paths[i][j].type = LINK_HOST;
                    }
                }
            }
        }

        // Step 3: 检测 RDMA/Network
        paths = detectNetworkPaths();

        // Step 4: 构建通信图
        // 使用 Dijkstra/max-flow 算法寻找最优路径
        // 优先级: NVLink > PCIe P2P > PCIe through CPU > Network
        buildCommunicationGraph();
    }

    // 选择 AllReduce 算法
    ncclResult_t selectAllReduceAlgo(int nRanks,
                                     const Link paths[][...])
    {
        if (hasNvSwitch(paths, nRanks)) {
            return NCCL_ALGO_COLLNET;  // NVSwitch 全互联 → CollNet
        } else if (nRanks <= 8 && hasNvlinkRing(paths, nRanks)) {
            return NCCL_ALGO_RING;     // 小集群 NVLink → Ring
        } else if (nRanks > 8) {
            return NCCL_ALGO_TREE;     // 大集群 → Tree
        } else {
            return NCCL_ALGO_RING;     // 默认
        }
    }
};
```

### 3.4 NVLink 与多 GPU 互联拓扑

```
═══════════════════════════════════════════════════
DGX H100 互联拓扑 (8 × H100 + 4 × NVSwitch)
═══════════════════════════════════════════════════

         ┌───────────┐  ┌───────────┐
         │ NVSwitch 0│  │ NVSwitch 1│
         │   (ports) │  │   (ports) │
         └─┬──┬──┬──┬┘  └─┬──┬──┬──┬┘
           │  │  │  │     │  │  │  │
  ┌────────┘  │  │  └─────┼──┼──┼──┼───────────────┐
  │  ┌────────┘  └────────┼──┼──┼──┼───────────────┼───┐
  │  │  ┌─────────────────┘  │  │  └───────────────┼───┼───┐
  │  │  │  ┌─────────────────┘  └──────────────────┼───┼───┼───┐
  │  │  │  │  ┌────────────────────────────────────┘   │   │   │
  ▼  ▼  ▼  ▼  ▼                                      ▼   ▼   ▼
┌────┐┌────┐┌────┐┌────┐         ┌───────────┐     ┌────┐┌────┐┌────┐
│GPU0││GPU1││GPU2││GPU3│         │ NVSwitch 2│     │GPU4││GPU5││GPU6││GPU7│
└────┘└────┘└────┘└────┘         │   (ports) │     └────┘└────┘└────┘└────┘
                                   └─┬──┬──┬──┬┘
                                     │  │  │  │
                                     │  │  │  └────────────────┐
                                     │  │  └──────────────────┐ │
                                     │  └────────────────────┐│ │
                                     └──────────────────────┐││ │
                                                            ▼▼▼ ▼
                                           ┌───────────┐
                                           │ NVSwitch 3│
                                           │   (ports) │
                                           └───────────┘

每个 H100: 18 个 NVLink 4.0 通道
  - 带宽: 50 GB/s per channel, 900 GB/s bidirectional per GPU
  - Switch: 450 GB/s per GPU per NVSwitch × 4 = 1.8 TB/s aggregate
  - Topology: 完全无阻塞 (full-bisection bandwidth)

8-GPU All-Reduce 性能:
  - 128MB tensor: 47 GB/s (8-GPU total)
  - 算法: CollNet (利用 NVSwitch)
  - 延迟: < 3ms

═══════════════════════════════════════════════════
DGX A100 互联拓扑 (8 × A100 + 6 × NVSwitch)
═══════════════════════════════════════════════════

所有 8 个 GPU 通过 6 个 NVSwitch 全互联
每对 GPU 之间: 600 GB/s bidirectional (NVLink 3.0)
```

### 3.5 GPUDirect RDMA

```
GPUDirect RDMA 让 GPU 显存直接通过网络发送，无需 CPU 中转:

传统路径:
  GPU → CPU memory (PCIe DMA) → CPU → NIC → Network → NIC → CPU → GPU
  延迟: ~50µs, 带宽受 PCIe 限制

GPUDirect RDMA:
  GPU → NIC → Network → NIC → GPU
  延迟: ~5µs, 满带宽 (200/400 Gbps)

设置步骤:
───────────────────────────────────────

# 1. 确认硬件支持
lspci | grep Mellanox
nvidia-smi topo -m  # 查看 GPU-NIC 是否在同一 PCIe switch

# 2. 加载内核模块
modprobe nvidia_peermem  # 允许 GPU 内存直接暴露给 RDMA

# 3. 配置 NCCL 使用 GPUDirect RDMA
export NCCL_IB_DISABLE=0         # 启用 Infiniband
export NCCL_NET_GDR_LEVEL=5      # 使用 GPUDirect RDMA
export NCCL_IB_GID_INDEX=3       # RoCEv2 GID index
export NCCL_IB_HCA=mlx5_0,mlx5_1 # 指定 HCA (网络适配器)
export NCCL_SOCKET_IFNAME=eth0

# 4. 验证 GPUDirect RDMA 是否生效
# 查看 NCCL debug log
export NCCL_DEBUG=INFO
mpirun -np 8 python train.py 2>&1 | grep GDR
# 期望输出:
#   NET/IB : Using GPUDirect RDMA
#   或
#   NET/IB : Using GDR
```

### 3.6 NCCL All-Reduce Ring 算法的详细步进

```
NCCL Ring All-Reduce over 4 GPUs, tensor size = 4 elements:

初始状态:
  GPU0: [ A B C D ]           GPU2: [ I J K L ]
  GPU1: [ E F G H ]           GPU3: [ M N O P ]

Phase 1: Reduce-Scatter (3 steps)
──────────────────────────────────

每个 GPU 将 tensor 分为 4 个 chunk:
                                  chunk 0  chunk 1  chunk 2  chunk 3
  GPU0: send chunk (rank+3)%4     [   A  ]  [   B  ]  [   C  ]  [   D  ]
  GPU1:                           [   E  ]  [   F  ]  [   G  ]  [   H  ]
  GPU2:                           [   I  ]  [   J  ]  [   K  ]  [   L  ]
  GPU3:                           [   M  ]  [   N  ]  [   O  ]  [   P  ]

Step 1: GPU i sends chunk (i+3)%4 to GPU (i+1)%4
  GPU0 → GPU1: chunk 3 = D
  GPU1 → GPU2: chunk 0 = E
  GPU2 → GPU3: chunk 1 = J
  GPU3 → GPU0: chunk 2 = O

  结果:
  GPU0: [ A    B    C    D+O ]
  GPU1: [ E+D  F    G    H   ]
  GPU2: [ I    J+E  K    L   ]
  GPU3: [ M    N    O+J  P   ]

Step 2: GPU i sends chunk (i+2)%4 to GPU (i+1)%4
  GPU0 → GPU1: chunk 2 = C
  GPU1 → GPU2: chunk 3 = D+O (received in step 1)
  GPU2 → GPU3: chunk 0 = I
  GPU3 → GPU0: chunk 1 = N

  结果:
  GPU0: [ A    B+N  C    D+O ]
  GPU1: [ E+D  F    G+C  H     ]
  GPU2: [ I    J+E  K    L+D+O ]
  GPU3: [ M+I  N    O+J  P     ]

Step 3: GPU i sends chunk (i+1)%4 to GPU (i+1)%4
  GPU0 → GPU1: chunk 1 = B+N
  GPU1 → GPU2: chunk 2 = G+C
  GPU2 → GPU3: chunk 3 = L+D+O
  GPU3 → GPU0: chunk 0 = M+I

  结果 (reduce-scatter 完成):
  GPU0: [ A+M+I      B+N     C       D+O       ]  ← chunk 0 有完整 reduce 结果
  GPU1: [ E+D        F       G+C     H+B+N     ]
  GPU2: [ I          J+E     K       L+D+O+G+C ]
  GPU3: [ M+I+A      N       O+J     P+L+D+O   ]

Phase 2: All-Gather (3 steps)
─────────────────────────────

Step 4-6: 每个 GPU 将 chunk 传递给下一个 GPU
  最终:
  GPU0: [ SUM_A  SUM_B  SUM_C  SUM_D  ]
  GPU1: [ SUM_E  SUM_F  SUM_G  SUM_H  ]
  GPU2: [ SUM_I  SUM_J  SUM_K  SUM_L  ]
  GPU3: [ SUM_M  SUM_N  SUM_O  SUM_P  ]

  其中 SUM_X = 所有 4 个 GPU 的 X 之和
```

### 3.7 Data Parallel 训练中的 NCCL 调用时机

```python
# PyTorch DDP (DistributedDataParallel) 的训练循环
import torch.distributed as dist

model = torch.nn.parallel.DistributedDataParallel(model)

for batch_idx, (data, target) in enumerate(train_loader):
    # Forward: 每个 GPU 独立执行 (无通信)
    output = model(data)
    loss = criterion(output, target)

    # Backward: 自动触发梯度同步
    loss.backward()
    # │
    # ├─ 计算 local grad
    # ├─ 插入 NCCL all-reduce hook
    # │    └─ ncclAllReduce(local_grad, global_grad, ..., ncclSum)
    # └─ 所有 GPU 的梯度被平均 (或求和 + 后续平均)

    # Optimizer step: 每个 GPU 用同步后的梯度更新本地模型拷贝
    optimizer.step()
    optimizer.zero_grad()

# 梯度同步的 hook 实现 (简化):
# torch/nn/parallel/distributed.py

class _AllReduceSum(torch.autograd.Function):
    @staticmethod
    def forward(ctx, input):
        # 收集所有 GPU 的 input 并求和
        dist.all_reduce(input, op=dist.ReduceOp.SUM)
        return input

    @staticmethod
    def backward(ctx, grad_output):
        # backward 不需要通信 (grad 已同步)
        return grad_output

# DDP 内部:
# 对于每个参数的 .grad，注册一个 hook:
# after_backward_hook:
#   ncclAllReduce(param.grad)  # 梯度同步 → 每个 GPU 得到相同的平均梯度
```

---

## 4. cuBLAS + cuDNN 的实战融合

### 4.1 Transformer Block 的算子分解

一个 Transformer Encoder Block 包含以下操作：

```
Self-Attention:
  Q = cuBLAS GEMM: X @ W_Q          [batch, seq, d_model] @ [d_model, d_q]   → [batch, seq, d_q]
  K = cuBLAS GEMM: X @ W_K          [batch, seq, d_model] @ [d_model, d_k]   → [batch, seq, d_k]
  V = cuBLAS GEMM: X @ W_V          [batch, seq, d_model] @ [d_model, d_v]   → [batch, seq, d_v]

  S = cuBLAS GEMM: Q @ K^T          [batch, seq, d_q] @ [d_k, seq]           → [batch, seq, seq]
  S_scaled = S / sqrt(d_k)          element-wise kernel
  A = softmax(S_scaled)             softmax kernel (max, exp, sum, div)
  O = cuBLAS GEMM: A @ V            [batch, seq, seq] @ [seq, d_v]           → [batch, seq, d_v]

Feed-Forward:
  FF1 = cuBLAS GEMM: O @ W1         [batch*seq, d_model] @ [d_model, d_ff]   → [batch*seq, d_ff]
  A1 = activation(FF1)              element-wise kernel (GELU/ReLU)
  FF2 = cuBLAS GEMM: A1 @ W2        [batch*seq, d_ff] @ [d_ff, d_model]      → [batch*seq, d_model]

优化机会:
  - QKV 合并为单个 GEMM: [d_model, 3·d_qkv] → 一次 cublasGemmEx 替代三次
  - FF1 + activation 可以融合: cublasLtMatmul with RELU epilogue
  - LayerNorm 不作为单独 kernel: fused layernorm kernel (CUDA 自定义)
```

### 4.2 FlashAttention: 融合注意力计算

```cpp
// 标准 Attention (多次 HBM 读写):
//   S = QK^T        → write S (seq²) to HBM
//   S = S/√d        → read S, write S
//   A = softmax(S)  → read S, write A
//   O = AV          → read A, V, write O
//   总计: O(seq²) HBM 访问

// FlashAttention (tiled, single kernel):
//   for each block of Q (outer loop):
//     for each block of K, V (inner loop):
//       S_block = Q_block @ K_block^T  (on-chip SRAM)
//       S_block = S_block / √d
//       P_block = softmax_online(S_block, running_max, running_sum)
//       O_block += P_block @ V_block
//     write O_block to HBM
//   总计: O(seq²/M) HBM 访问 (M = SRAM size)

__global__ void flash_attention_kernel(
    const half* Q, const half* K, const half* V,
    half* O,
    int batch, int seq, int d_head
) {
    extern __shared__ half smem[];  // SRAM buffer

    half* Qi = smem;
    half* Kj = smem + Br * d_head;
    half* Vj = smem + (Br + Bc) * d_head;
    half* Sij = smem + (Br + 2*Bc) * d_head;

    int block_i = blockIdx.x;  // Q block index
    int q_start = block_i * Br;

    // Load Q block into SRAM (persistent)
    load_tile(Qi, Q + q_start * d_head, Br, d_head);

    // Initialize running stats for online softmax
    float m_i_prev = -INFINITY;  // running max
    float l_i_prev = 0.0f;       // running sum

    // Output accumulator (in SRAM)
    float Oi[Br][d_head] = {0};

    // Inner loop: iterate over K,V blocks
    for (int block_j = 0; block_j < n_blocks_kv; block_j++) {
        int kv_start = block_j * Bc;

        // Load K, V block into SRAM
        load_tile(Kj, K + kv_start * d_head, Bc, d_head);
        load_tile(Vj, V + kv_start * d_head, Bc, d_head);

        // Compute S = QK^T (on SRAM)
        // Sij[Br][Bc] = Qi[Br][d_head] @ Kj^T[d_head][Bc]
        matmul_tile(Sij, Qi, Kj, Br, Bc, d_head);

        // m = rowmax(Sij)
        // P = exp(Sij - m) / sum(exp(Sij - m))
        // 在线更新 Oi
        float m_i = m_i_prev;
        float l_i = l_i_prev;

        for (int r = 0; r < Br; r++) {
            float row_max = max(Sij[r]);
            float new_max = fmaxf(m_i, row_max);

            // Rescale accumulated sum
            float scale = expf(m_i - new_max);
            l_i *= scale;

            // Compute exp for current row
            float row_sum = 0;
            for (int c = 0; c < Bc; c++) {
                Sij[r][c] = expf(Sij[r][c] - new_max);
                row_sum += Sij[r][c];
            }
            l_i += row_sum;

            // Update Oi
            float correct = expf(m_i - new_max);
            for (int d = 0; d < d_head; d++) {
                Oi[r][d] *= correct;
                float ov = 0;
                for (int c = 0; c < Bc; c++) {
                    ov += Sij[r][c] * Vj[c][d];
                }
                Oi[r][d] += ov;
            }
            m_i = new_max;
        }
        m_i_prev = m_i;
        l_i_prev = l_i;
    }

    // Normalize and write output
    for (int r = 0; r < Br; r++) {
        for (int d = 0; d < d_head; d++) {
            O[(q_start + r) * d_head + d] = __float2half(Oi[r][d] / l_i_prev);
        }
    }
}
```

### 4.3 卷积 + 批归一化 + 激活的融合

```
传统流程 (3 个独立的 kernel):

  [Conv]               [BatchNorm]           [ReLU]
  ┌───────┐            ┌───────┐            ┌───────┐
  │ Input │──Kernel1──►│  BN   │──Kernel2──►│ ReLU  │──► Output
  │  HBM  │   (读写)   │  HBM  │   (读写)   │  HBM  │
  └───────┘            └───────┘            └───────┘
  内存访问: 3×读HBM + 3×写HBM

cuDNN Graph 融合 (1 个 kernel):

  [Conv+Bias+BN+ReLU]   ← 所有计算在一个 kernel 内完成
  ┌───────┐
  │ Input │──Fused Kernel──► Output
  │  HBM  │   (1读1写)      │  HBM  │
  └───────┘                 └───────┘
  内存访问: 1×读HBM + 1×写HBM → 3× 加速 (memory-bound 情况)

融合 Kernel 伪代码 (cuDNN 内部):
───────────────────────────────
for each output position (n, c, h, w):
  // Step 1: 卷积 (寄存器内完成)
  float sum = bias[c];
  for (int ic = 0; ic < in_channels; ic++) {
    for (int ky = 0; ky < kH; ky++) {
      for (int kx = 0; kx < kW; kx++) {
        sum += input[n, ic, h*S+ky, w*S+kx] * weight[c, ic, ky, kx];
      }
    }
  }

  // Step 2: BatchNorm (寄存器内完成)
  sum = (sum - mean[c]) * rsqrt(var[c] + eps) * scale[c] + bias_bn[c];

  // Step 3: ReLU (寄存器内完成)
  sum = max(sum, 0.0f);

  // 一次写入 HBM
  output[n, c, h, w] = sum;
```

---

## 5. 三剑客协作全景图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Distributed Training with PyTorch                   │
│                      8 × GPU training, data parallel                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Device 0                            ...    Device 7                    │
│  ┌─────────────────────────────────┐       ┌─────────────────────────┐  │
│  │ Forward Pass                    │       │ Forward Pass            │  │
│  │                                 │       │                         │  │
│  │  Input (HBM)                    │       │  Input (HBM)            │  │
│  │    │                            │       │    │                    │  │
│  │    ├─ Conv2D                    │       │    ├─ Conv2D            │  │
│  │    │  └─ cuDNN (IMPLICIT_GEMM)  │       │    │  └─ cuDNN         │  │
│  │    │                             │       │    │                   │  │
│  │    ├─ BatchNorm + ReLU          │       │    ├─ BatchNorm + ReLU  │  │
│  │    │  └─ cuDNN fused kernel     │       │    │  └─ cuDNN fused   │  │
│  │    │                             │       │    │                   │  │
│  │    ├─ Linear (mm)               │       │    ├─ Linear (mm)       │  │
│  │    │  └─ cuBLAS cublasLtMatmul  │       │    │  └─ cuBLAS        │  │
│  │    │                             │       │    │                   │  │
│  │    ├─ Attention (QKV + softmax) │       │    ├─ Attention        │  │
│  │    │  └─ cuBLAS + FlashAttention│       │    │  └─ cuBLAS + FA   │  │
│  │    └─ Loss (CrossEntropy)       │       │    └─ Loss             │  │
│  │                                 │       │                         │  │
│  │  Backward Pass                  │       │  Backward Pass          │  │
│  │                                 │       │                         │  │
│  │  grad_output (HBM)              │       │  grad_output (HBM)      │  │
│  │    │                            │       │    │                    │  │
│  │    ├─ dL/dW (GEMM)              │       │    ├─ dL/dW (GEMM)      │  │
│  │    │  └─ cuBLAS (transpose GEMM)│       │    │  └─ cuBLAS        │  │
│  │    │                             │       │    │                   │  │
│  │    └─ dL/dX (transposed conv)   │       │    └─ dL/dX            │  │
│  │       └─ cuDNN backward data    │       │       └─ cuDNN         │  │
│  │                                 │       │                         │  │
│  │  Gradient Sync                  │       │  Gradient Sync          │  │
│  │  ┌───────────────────────────┐  │       │  ┌───────────────────┐  │  │
│  │  │ NCCL All-Reduce           │  │       │  │ NCCL All-Reduce   │  │  │
│  │  │  - Ring over NVLink       │──┼───────┼──│  (same NCCL op)   │  │  │
│  │  │  - avg(gradients)         │  │       │  │                   │  │  │
│  │  └───────────────────────────┘  │       │  └───────────────────┘  │  │
│  │                                 │       │                         │  │
│  │  Optimizer Step (local)         │       │  Optimizer Step         │  │
│  │  w -= lr * avg_grad             │       │  w -= lr * avg_grad     │  │
│  └─────────────────────────────────┘       └─────────────────────────┘  │
│                                                                         │
│  Communication Backend:                                                 │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  NCCL                                                             │   │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐      │   │
│  │  │  GPU 0    │  │  GPU 1    │  │  GPU 2    │  │  GPU 3    │      │   │
│  │  │ NVLink 4.0│  │ NVLink 4.0│  │ NVLink 4.0│  │ NVLink 4.0│      │   │
│  │  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘      │   │
│  │        └──────────────┼──────────────┼──────────────┘             │   │
│  │                       ▼              ▼                            │   │
│  │               ┌──────────────────────────┐                        │   │
│  │               │  NVSwitch (full crossbar) │                        │   │
│  │               │  1.6 TB/s aggregate       │                        │   │
│  │               └──────────────────────────┘                        │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 6. 关键源码与参考

| 主题 | 源码/文档 |
|------|----------|
| cuBLAS 头文件 | `/usr/local/cuda/include/cublas_v2.h` |
| cuBLASLt 头文件 | `/usr/local/cuda/include/cublasLt.h` |
| cuBLAS 文档 | `CUDA Toolkit Documentation > cuBLAS` |
| cuDNN 头文件 | `/usr/local/cuda/include/cudnn.h` |
| cuDNN 9.0 Graph API | `/usr/local/cuda/include/cudnn_frontend.h` |
| cuDNN 开发者指南 | `cuDNN Developer Guide (PDF)` |
| NCCL 头文件 | `/usr/local/cuda/include/nccl.h` |
| NCCL 源码 | `github.com/NVIDIA/nccl` (开放源码) |
| NCCL 拓扑检测 | `nccl/src/graph/topo.cc` |
| PyTorch cuBLAS 封装 | `aten/src/ATen/native/cuda/Blas.cpp` |
| PyTorch cuDNN 封装 | `aten/src/ATen/native/cudnn/Conv.cpp` |
| PyTorch NCCL 封装 | `torch/csrc/distributed/c10d/ProcessGroupNCCL.cpp` |
| FlashAttention | `github.com/Dao-AILab/flash-attention` |
| WMMA 编程指南 | `CUDA C++ Programming Guide > Warp Matrix Functions` |

---

## 7. 性能数据参考

```
GEMM Performance (A100, fp16 Tensor Core):
──────────────────────────────────────────
Shape           | cuBLAS    | 理论峰值利用率
────────────────┼───────────┼──────────────
1024×1024×1024  | 270 TFLOPS| 87%
2048×2048×2048  | 295 TFLOPS| 95%
4096×4096×1024  | 280 TFLOPS| 90%
16384×8192×8192 | 300 TFLOPS| 96%

Convolution Performance (A100, fp16, cuDNN):
────────────────────────────────────────────
Shape              | Algo      | TFLOPS | 带宽利用率
────────────────────┼───────────┼────────┼──────────
3×3, s1, 64c, 224² | WINOGRAD  | 180    | 92%
3×3, s2, 128c, 28² | IMPL_GEMM  | 200    | 87%
1×1, s1, 256c, 14² | IMPL_GEMM  | 220    | 95%
7×7, s2, 64c, 112² | FFT       | 160    | 80%

NCCL All-Reduce Performance (H100, NVLink 4.0):
──────────────────────────────────────────────
Size     | 2 GPU    | 4 GPU    | 8 GPU
─────────┼──────────┼──────────┼──────────
1 MB     | 84 GB/s  | 170 GB/s | 330 GB/s
16 MB    | 150 GB/s | 300 GB/s | 600 GB/s
128 MB   | 165 GB/s | 330 GB/s | 660 GB/s
1024 MB  | 168 GB/s | 340 GB/s | 680 GB/s

Bus Bandwidth (all-reduce effective):
  bus_bw = data_size × 2 × (N-1) / N / time
  对于 8 GPU, bus_bw = data_size × 1.75 / time
```

---

## 下一篇文章

[第四篇：vLLM 推理加速篇](./04-vllm-inference.md)
