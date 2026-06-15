# 第六篇：完整管线图：从 PyTorch 到 GPU 硬件 — 端到端时序与调用链

系列目录：[PyTorch → NVIDIA GPU 全链路深度解析](./README.md)

---

## 1. 端到端流程总览

### 1.1 一幅图的六层架构

```
Python (torch.nn)
  │
  ▼
C++ (ATen native functions, Autograd)
  │
  ▼
加速库 (cuBLAS, cuDNN, cuFFT, NCCL, CUTLASS, 自定义 CUDA kernel)
  │
  ▼
CUDA Runtime (cudaLaunchKernel, cudaMemcpy, cudaMalloc)
  │
  ▼
CUDA Driver (cuLaunchKernel, cuMemAlloc, cuCtxCreate)
  │
  ▼
nvidia.ko (kernel module: ioctl → pushbuffer → MMIO)
  │
  ▼
GPU Hardware (GigaThread Engine → TPC → SM → Warp → Core/Tensor Core)
```

### 1.2 一次 Forward Pass 的调用链

```
PyTorch Python (torch.nn.Linear)
  ↓ __call__ → forward()
torch.nn.functional.linear(input, weight, bias)
  ↓
torch._C._nn.linear(input, weight, bias)     # Python→C++ dispatch
  ↓
at::native::linear(at::Tensor input, at::Tensor weight, at::optional<Tensor> bias)
  ↓ at::native::matmul (或 addmm)
at::native::addmm(out, input, weight.t(), bias, 1, 1)
  ↓ ATen dispatcher: DispatchKey::CUDA → addmm_cuda
at::native::cuda::addmm_cuda(...)
  ↓
at::cuda::blas::gemm<at::Half>(                           # cuBLAS wrapper
    transa='n', transb='t',
    m, n, k,
    alpha=1.0,
    A, lda,
    B, ldb,
    beta=1.0,
    C, ldc
)
  ↓
cublasGemmEx(                                            # cuBLAS API
    handle,
    CUBLAS_OP_N, CUBLAS_OP_T,
    m, n, k,
    &alpha, A, CUDA_R_16F, lda,
    B, CUDA_R_16F, ldb,
    &beta, C, CUDA_R_16F, ldc,
    CUBLAS_COMPUTE_32F, CUBLAS_GEMM_DEFAULT
)
  ↓ cuBLAS internal: auto-tuning → select kernel
cublasLtMatmul(handle, matmul_desc, &alpha, A, B, &beta, C, D, ...)
  ↓ select best tile size (256×128×32 for example)
cudaLaunchKernel(                                        # CUDA Runtime
    gemm_kernel<256,128,32>,
    grid_dim=(ceil(n/256), ceil(m/128)),
    block_dim=(256),
    shared_mem_bytes,
    stream
)
  ↓
cuLaunchKernel(                                          # CUDA Driver
    gemm_kernel,
    grid_dim, block_dim,
    kernel_params,
    shared_mem_bytes,
    stream
)
  ↓ ioctl(/dev/nvidiactl, NV_ESC_RM_ALLOC_MEMORY + submit commands)
nvidia.ko: fill GPU pushbuffer with PM4 method packets
  ↓ MMIO write to GPU pushbuffer doorbell
GPU GigaThread Engine: fetch pushbuffer → parse PM4 commands
  ↓ dispatch grid to TPCs → SMs
SM Warp Scheduler: pick warps, dispatch instructions
  ↓ LDG (load global) + HMMA (Tensor Core MMA)
Register File ← Tensor Core result → STG (store global) → L2 → HBM3
```

---

## 2. Step 1: PyTorch nn.Transformer 的前向传播

### 2.1 一次完整的 Transformer Block Forward

```python
import torch
import torch.nn as nn

class TransformerBlock(nn.Module):
    """
    一个标准的 Transformer Block，包含:
    - Multi-Head Self-Attention
    - Layer Norm × 2
    - Feed-Forward Network (2-layer MLP)
    """
    def __init__(self, d_model=4096, n_heads=32, d_ff=11008):
        super().__init__()
        self.d_model = d_model
        self.n_heads = n_heads
        self.head_dim = d_model // n_heads
        
        # QKV projection (合并为一个矩阵乘法以减少 kernel launch)
        self.qkv_proj = nn.Linear(d_model, 3 * d_model, bias=False)
        
        # Output projection
        self.o_proj = nn.Linear(d_model, d_model, bias=False)
        
        # Feed-Forward: gate + up + down (SwiGLU 变体)
        self.gate_proj = nn.Linear(d_model, d_ff, bias=False)
        self.up_proj = nn.Linear(d_model, d_ff, bias=False)
        self.down_proj = nn.Linear(d_ff, d_model, bias=False)
        
        # Layer Norm
        self.input_layernorm = nn.LayerNorm(d_model)
        self.post_attention_layernorm = nn.LayerNorm(d_model)
    
    def forward(self, x: torch.Tensor, attention_mask=None):
        """
        x: [batch_size, seq_len, d_model]
        Returns: [batch_size, seq_len, d_model]
        """
        residual = x
        
        # ===== Step 1: Input LayerNorm =====
        x = self.input_layernorm(x)
        
        # ===== Step 2: QKV Projection (1×GEMM) =====
        # [b, s, 4096] @ [4096, 12288] → [b, s, 12288]
        qkv = self.qkv_proj(x)
        
        # ===== Step 3: Split Q, K, V =====
        q, k, v = qkv.split(self.d_model, dim=-1)
        # q, k, v: [b, s, 4096] each
        
        # ===== Step 4: Reshape for multi-head =====
        # [b, s, 4096] → [b, n_heads, s, head_dim]
        q = q.view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)
        k = k.view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)
        v = v.view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)
        
        # ===== Step 5: Scaled Dot-Product Attention =====
        # 这里调用 PyTorch 的 sdpa (会 dispatch 到 cuDNN 或 Flash Attention)
        attn_output = torch.nn.functional.scaled_dot_product_attention(
            q, k, v,
            attn_mask=attention_mask,
            dropout_p=0.0,
            is_causal=True,
        )
        # [b, n_heads, s, head_dim]
        
        # ===== Step 6: Reshape back =====
        attn_output = attn_output.transpose(1, 2).contiguous()
        attn_output = attn_output.view(batch_size, seq_len, self.d_model)
        
        # ===== Step 7: Output Projection (1×GEMM) =====
        attn_output = self.o_proj(attn_output)
        
        # ===== Step 8: Residual + Post-Attention LayerNorm =====
        x = residual + attn_output
        residual = x
        x = self.post_attention_layernorm(x)
        
        # ===== Step 9: Feed-Forward (SwiGLU) =====
        # gate: [b, s, 4096] @ [4096, 11008] → [b, s, 11008]
        gate = self.gate_proj(x)
        
        # up: [b, s, 4096] @ [4096, 11008] → [b, s, 11008]
        up = self.up_proj(x)
        
        # SiLU(gate) * up (element-wise)
        ff_hidden = torch.nn.functional.silu(gate) * up
        
        # down: [b, s, 11008] @ [11008, 4096] → [b, s, 4096]
        ff_output = self.down_proj(ff_hidden)
        
        # ===== Step 10: Second Residual =====
        x = residual + ff_output
        
        return x

# 实际运行一次
model = TransformerBlock().cuda().half()  # FP16 on GPU
x = torch.randn(1, 2048, 4096, device='cuda', dtype=torch.float16)
output = model(x)  # 触发完整调用链
```

### 2.2 scaled_dot_product_attention 的内部 dispatch

```python
# torch.nn.functional.scaled_dot_product_attention 的 dispatch 逻辑:

# sdpa 根据输入特性选择最优的 backend:
# 1. Flash Attention (v2): 通过 cuDNN 后端
#    条件: q/k/v 为 CUDA tensor, FP16/BF16, head_dim ≤ 256
#    特征: tiled, IO-aware, O(n) memory
#
# 2. Memory Efficient Attention (xformers): 通过 PyTorch composite
#    条件: Flash Attention 不可用时的 fallback
#    特征: online softmax, O(n) memory
#
# 3. Math (naive): 通过 PyTorch composite (matmul + softmax + matmul)
#    条件: 前两者都不可用
#    特征: O(n²) memory, 慢但精度高

def sdpa_dispatch_logic(q, k, v, is_causal, dropout_p, scale):
    """简化版: PyTorch sdpa 的 backend 选择逻辑"""
    
    from torch.backends.cuda import (
        sdp_kernel, SDPBackend, 
        flash_attention_enabled, 
        mem_efficient_attention_enabled,
        math_attention_enabled,
    )
    
    # Priority order: Flash → MemEfficient → Math
    backend_priority = [
        (SDPBackend.FLASH_ATTENTION, flash_attention_enabled()),
        (SDPBackend.EFFICIENT_ATTENTION, mem_efficient_attention_enabled()),
        (SDPBackend.MATH, math_attention_enabled()),
    ]
    
    with sdp_kernel(
        enable_flash=True,
        enable_mem_efficient=True,
        enable_math=True
    ):
        # Flash Attention 的额外条件检查:
        #   - CUDA device only
        #   - dtype in (fp16, bf16) — FP32 不支持 Flash Attention
        #   - no custom scale (使用默认 1/sqrt(d))
        #   - no attention bias (cuDNN flash attention 不支持)
        if (q.device.type == 'cuda' and
            q.dtype in (torch.float16, torch.bfloat16) and
            scale is None):
            # cuDNN Flash Attention (v2)
            return SDPBackend.FLASH_ATTENTION
        
        elif (q.device.type == 'cuda' and
              q.dtype in (torch.float16, torch.bfloat16, torch.float32)):
            # Memory Efficient Attention
            return SDPBackend.EFFICIENT_ATTENTION
        
        else:
            return SDPBackend.MATH
```

cuDNN Flash Attention 的底层调用路径：

```c++
// PyTorch aten/src/ATen/native/transformers/cuda/sdp_utils.cpp

// Flash Attention 通过 cuDNN Fused Attention API 实现
// cuDNN 9.0+ 提供 cudnnGraph API (cudnn_frontend)
#include <cudnn_frontend.h>
#include <cudnn_backend.h>

// 简化版的 cuDNN Flash Attention 调用
auto sdp_forward_cudnn(
    const Tensor& q, const Tensor& k, const Tensor& v,
    std::optional<Tensor> attn_mask, double dropout, bool is_causal,
    std::optional<double> scale)
{
    // Step 1: 创建 cuDNN graph
    auto handle = getCudnnHandle();
    
    // cuDNN Flash Attention v2 graph
    // 内部使用 tiled softmax + recompute 来避免存储 O(n²) attention matrix
    // 默认在 H100 上使用 FP8 Tensor Core (如果 dtype 允许)
    
    auto q_o = cudnn_frontend::TensorBuilder()
        .setDim(4, {b, h, s_q, d})
        .setStride(4, {h*s_q*d, s_q*d, d, 1})
        .setDataType(CUDNN_DATA_HALF)
        .build();
    // ... similarly for K, V, O ...
    
    // FAD (Fused Attention) operation
    auto fad_op = cudnn_frontend::OperationBuilder(CUDNN_BACKEND_OPERATION_FUSED_ATTENTION)
        .setQ(q_o)
        .setK(k_o)
        .setV(v_o)
        .setAttnScale(1.0 / sqrt(head_dim))
        .setBias(nullptr)
        .setIsCausal(is_causal)
        .setDropout(dropout)
        .setOutput(o)
        .build();
    
    auto graph = cudnn_frontend::GraphBuilder()
        .setHandle(handle)
        .addOperation(fad_op)
        .build();
    
    auto plan = cudnn_frontend::ExecutionPlanBuilder()
        .setHandle(handle)
        .setGraph(graph)
        .build();
    
    // cuDNN 内部:
    // 1. 根据 problem size 选择 tile 大小
    // 2. 分配临时内存 (scratch space)
    // 3. 生成 CUDA kernel (或选择预编译的 heuristics)
    // 4. cudaLaunchKernel → GPU 执行
    plan.execute(stream, {q, k, v}, {o, workspace});
}
```

---

## 3. Step 2: ATen Dispatcher — 从 Python 到 CUDA

### 3.1 Dispatch Key 机制

```python
# PyTorch 的 dispatching 通过 DispatchKey 实现
# 每个 tensor 有一个 dispatch key set, 决定调用哪个 backend

import torch

x = torch.randn(4, 4)
print(x.device)  # cpu
# key_set: {DispatchKey::CPU, DispatchKey::AutogradCPU, ...}

x = x.cuda()
print(x.device)  # cuda:0
# key_set: {DispatchKey::CUDA, DispatchKey::AutogradCUDA, ...}

# addmm 在 CPU 和 CUDA 上不同的实现:
#   at::native::addmm  (operator registration)
#     ├── DispatchKey::CPU     → at::native::addmm_cpu
#     ├── DispatchKey::CUDA    → at::native::addmm_cuda
#     ├── DispatchKey::AutogradCPU  → autograd wrapper
#     └── DispatchKey::AutogradCUDA → autograd wrapper
```

C++ 端的 dispatcher 实现：

```cpp
// aten/src/ATen/core/dispatch/Dispatcher.h

// Operator registration: 为每个 dispatch key 注册 kernel
TORCH_LIBRARY_IMPL(aten, CUDA, m) {
    // 当 dispatch key 包含 CUDA 时，调用此 kernel
    m.impl("addmm", TORCH_FN(addmm_cuda));
}

// 调用路径:
//   torch.addmm(A, B, C)
//     → dispatcher.call<Tensor>(schema, A, B, C)
//       → 检查 A, B, C 的最高优先级 dispatch key
//         → 遇到 AutogradCUDA? 调 autograd wrapper
//           → 记录 backward 信息 → remove Autograd key
//             → 再次 dispatch → DispatchKey::CUDA
//               → addmm_cuda(A, B, C)

// Dispatcher 的核心循环:
template<class Return, class... Args>
Return OperatorHandle::call(Args&&... args) const {
    auto dispatch_key_set = computeDispatchKeySet(args...);
    // computeDispatchKeySet: 取所有 tensor args 的 key_set 交集
    // 加上全局 include 的 key
    
    const KernelFunction& kernel = operatorDef_->op.lookup(dispatch_key_set);
    // lookup: 按 priority 遍历 dispatch key table
    // DispatchKey priority: Autograd > Autocast > Python > CUDA/CPU > ...
    
    return kernel.template call<Return, Args...>(
        operatorDef_->op, dispatch_key_set, std::forward<Args>(args)...
    );
}
```

### 3.2 从线性层到 cuBLAS

```cpp
// ATen addmm_cuda → cuBLAS 的完整路径

// aten/src/ATen/native/cuda/Blas.cpp
Tensor& addmm_out_cuda(
    const Tensor& self,      // C matrix (bias)
    const Tensor& mat1,      // A matrix
    const Tensor& mat2,      // B matrix
    const Scalar& beta,
    const Scalar& alpha,
    Tensor& result)          // C = alpha*A*B + beta*C
{
    // Step 1: 检查维度、设备、dtype
    TORCH_CHECK(mat1.dim() == 2 && mat2.dim() == 2);
    
    // Step 2: 转换为列优先 (Fortran order) — 大部分 BLAS 期望
    // cuBLAS 期望 column-major，PyTorch 使用 row-major
    // Transpose trick: A_rowmajor * B_rowmajor = (B^T_col * A^T_col)^T
    //   cublasGemmEx(transB='T', transA='T', ...) 等价于 row-major
    
    // Step 3: 调用 cuBLAS
    at::cuda::blas::gemm<scalar_t>(
        /*transa=*/'n',        // A 不需要转置 (因为 B^T·A^T trick)
        /*transb=*/'t',        // B 需要转置
        /*m=*/mat2.size(1),    // 输出行数
        /*n=*/mat1.size(0),    // 输出列数
        /*k=*/mat1.size(1),    // 内积维度
        /*alpha=*/alpha.to<scalar_t>(),
        /*A=*/mat2.const_data_ptr<scalar_t>(),  // 注意 A/B 交换!
        /*lda=*/mat2.size(1),
        /*B=*/mat1.const_data_ptr<scalar_t>(),
        /*ldb=*/mat1.size(1),
        /*beta=*/beta.to<scalar_t>(),
        /*C=*/result.mutable_data_ptr<scalar_t>(),
        /*ldc=*/result.size(1)
    );
    
    return result;
}

// at::cuda::blas::gemm 最终调用:
template<typename T>
void gemm(char transa, char transb, int64_t m, int64_t n, int64_t k,
          T alpha, const T* A, int64_t lda, const T* B, int64_t ldb,
          T beta, T* C, int64_t ldc)
{
    cublasHandle_t handle = at::cuda::getCurrentCUDABlasHandle();
    cudaDataType_t compute_type = at::cuda::getCublasComputeType<T>();
    cudaDataType_t data_type = at::cuda::getCublasDataType<T>();
    
    // cuBLAS GEMM API
    TORCH_CUDABLAS_CHECK(cublasGemmEx(
        handle,
        cublasOperationConvert(transa),
        cublasOperationConvert(transb),
        (int)m, (int)n, (int)k,
        &alpha, A, data_type, (int)lda,
        B, data_type, (int)ldb,
        &beta, C, data_type, (int)ldc,
        compute_type,
        CUBLAS_GEMM_DEFAULT  // 或 CUBLAS_GEMM_ALGO0, ..., CUBLAS_GEMM_ALGO23
    ));
}
```

---

## 4. Step 3: cuBLAS — 自动调优与 Kernel 选择

### 4.1 cuBLAS GEMM Tiling

```cpp
// cuBLAS 内部的 tile 自动调优

// cuBLAS 为每个 m,n,k 组合预训练了最佳 tile size
// 调优因素:
//   1. M, N, K 的大小
//   2. 矩阵形状 (tall-skinny, square, flat-wide)
//   3. GPU 架构 (SM 数, shared memory 大小, register file 大小)
//   4. Data type (FP16, TF32, FP32, FP64, INT8, FP8)
//   5. Epilogue 操作 (bias, ReLU, GELU, 仅计算)

// GEMM tiling 参数:
//   TILE_M: 输出矩阵每 block 处理的行数
//   TILE_N: 输出矩阵每 block 处理的列数
//   TILE_K: 每次加载的内积维度
//   WARP_M: 每个 warp 处理的行
//   WARP_N: 每个 warp 处理的列

// 典型 tile 选择 (H100, FP16):
//   Small M,N:   TILE_M=64,  TILE_N=64,  TILE_K=32
//   Medium M,N:  TILE_M=128, TILE_N=128, TILE_K=32
//   Large M,N:   TILE_M=256, TILE_N=128, TILE_K=64

// cuBLAS 通过 autotuning 选择 (cuBLASLt API):
cublasLtMatmulDesc_t matmul_desc;
cublasLtMatrixLayout_t A_desc, B_desc, C_desc, D_desc;
cublasLtMatmulPreference_t preference;
int returned_results = 0;

// 设置 matrix layouts
cublasLtMatrixLayoutCreate(&A_desc, CUDA_R_16F, m, k, lda);
cublasLtMatrixLayoutSetAttribute(
    A_desc, CUBLASLT_MATRIX_LAYOUT_ORDER, &order_col, sizeof(order_col)
);

// 设置 heuristics
cublasLtMatmulPreferenceCreate(&preference);
uint64_t workspace_size = 4 * 1024 * 1024;  // 4 MB workspace
cublasLtMatmulPreferenceSetAttribute(
    preference, CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES,
    &workspace_size, sizeof(workspace_size)
);

// 获取最佳算法
int max_algorithms = 10;
cublasLtMatmulHeuristicResult_t heuristics[10];
cublasLtMatmulAlgoGetHeuristic(
    handle, matmul_desc, A_desc, B_desc, C_desc, D_desc,
    preference, max_algorithms, heuristics, &returned_results
);

// heuristics[0] 是最优的 → 使用它
cublasLtMatmul(
    handle, matmul_desc,
    &alpha, A, A_desc,
    B, B_desc,
    &beta, C, C_desc,
    D, D_desc,
    &heuristics[0].algo,
    workspace, workspace_size,
    stream
);
```

### 4.2 GEMM Kernel 伪代码（cuBLAS 内部）

```cuda
// cuBLAS 内部 GEMM kernel (简化, 与 CUTLASS 类似)

#include <cute/tensor.hpp>
using namespace cute;

template<
    int TILE_M, int TILE_N, int TILE_K,
    int WARP_M, int WARP_N, int WARP_K,
    typename TA, typename TB, typename TC
>
__global__ void cublas_gemm_kernel(
    TA const* A, TB const* B, TC* C,
    int M, int N, int K,
    float alpha, float beta
) {
    // ========== TMA descriptors setup (H100) ==========
    // 使用 TMA 异步加载 A 和 B 的 tile
    
    extern __shared__ __align__(128) char smem[];
    TA* A_tile = reinterpret_cast<TA*>(smem);
    TB* B_tile = A_tile + TILE_M * TILE_K;
    
    // ========== Main loop over K dimension ==========
    // Mma = warp-level matrix multiply accumulate
    TC accum[WARP_M][WARP_N] = {0};
    
    #pragma unroll 1
    for (int k_block = 0; k_block < K; k_block += TILE_K) {
        // TMA async copy: A_global[k_block] → A_tile (shared memory)
        // TMA async copy: B_global[k_block] → B_tile (shared memory)
        // (使用 cp.async.bulk 指令)
        
        // Wait for previous copy to complete (pipelined)
        barrier_wait();
        
        // ===== Loop over inner K dim =====
        #pragma unroll
        for (int k_inner = 0; k_inner < TILE_K; k_inner += 16) {
            // Load fragments from shared memory
            TA frag_a[4];  // m16k16 → 8 FP16 registers per thread
            TB frag_b[2];  // k16n8  → 4 FP16 registers per thread
            
            load_fragment(A_tile, frag_a, k_inner);
            load_fragment(B_tile, frag_b, k_inner);
            
            // Tensor Core MMA
            // mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32
            mma_sync(accum, frag_a, frag_b, accum);
        }
        
        // Advance TMA barrier
        barrier_arrive();
    }
    
    // ========== Epilogue: apply bias + activation + store ==========
    // 如果在矩阵乘法后需要执行 element-wise 操作
    // cuBLAS 支持 fused epilogue: bias add, ReLU, GELU, Sigmoid, 等
    
    TC epilogue_result[WARP_M][WARP_N];
    
    // 应用 scale + bias
    for (int i = 0; i < WARP_M; i++) {
        for (int j = 0; j < WARP_N; j++) {
            epilogue_result[i][j] = alpha * accum[i][j];
            if (beta != 0) {
                epilogue_result[i][j] += beta * C_prior[i][j];
            }
        }
    }
    
    // Store to global memory (coalesced)
    for (int i = 0; i < WARP_M; i++) {
        for (int j = 0; j < WARP_N; j++) {
            int row = blockIdx.y * TILE_M + i;
            int col = blockIdx.x * TILE_N + j;
            if (row < M && col < N) {
                C[row * N + col] = epilogue_result[i][j];
            }
        }
    }
}

// cuBLAS 的 tile 选择根据 m,n,k 自动完成:
//   if (m <= 64 && n <= 64)  → TILE_M=64,  TILE_N=64,  WARP_M=32, WARP_N=32
//   if (m <= 256 && n <= 256)→ TILE_M=128, TILE_N=128, WARP_M=64, WARP_N=64
//   if (m > 256 || n > 256)  → TILE_M=256, TILE_N=128, WARP_M=64, WARP_N=64
```

### 4.3 vLLM vs PyTorch Eager 的分叉点

```
PyTorch Eager (标准路径):
  torch.nn.Linear.forward()
    → F.linear()
      → at::native::addmm
        → cuBLAS cublasGemmEx
          → CUDA kernel launch
            → GPU 执行

vLLM (自定义路径):
  vLLM 替换 attention 模块:
    → PagedAttentionWrapper.forward()
      → 构建 block_table + input tensors
        → 调用自定义 CUDA kernel (paged_attention_v1/v2)
          → 直接操作分块 KV cache
            → 内置 online softmax
              → GPU 执行

vLLM 对于其他层 (norm, FFN, projection):
  → 仍然走 cuBLAS 路径 (不受影响)
  → 可以使用 torch.compile 加速 (inducer 生成 Triton kernel)
```

---

## 5. Step 4: CUDA Runtime → Driver → nvidia.ko

### 5.1 CUDA Runtime API

```cpp
// CUDA Runtime API → Driver API 的映射

// Runtime API (高层, 简洁)
cudaError_t cudaLaunchKernel(
    const void* func,           // kernel function pointer
    dim3 gridDim,               // grid 维度
    dim3 blockDim,              // block 维度
    void** args,                // kernel 参数
    size_t sharedMem,           // dynamic shared memory
    cudaStream_t stream         // stream
);

// 内部实现 (libcudart.so):
cudaError_t cudaLaunchKernel(...) {
    // Step 1: 获取 Driver API 函数指针
    static auto cuLaunchKernel_ptr = 
        get_driver_entry_point("cuLaunchKernel");
    
    // Step 2: 计算 kernel 参数 buffer
    // 将 args (指向参数指针的指针数组) 转换为连续的参数 buffer
    // 因为 GPU ABI 需要参数按寄存器顺序排列
    
    // Step 3: 调用 Driver API
    return to_cuda_error(cuLaunchKernel_ptr(
        func,
        gridDim.x, gridDim.y, gridDim.z,
        blockDim.x, blockDim.y, blockDim.z,
        sharedMem,
        stream,
        args,        // kernel 参数
        nullptr      // extra options
    ));
}
```

### 5.2 CUDA Driver API

```cpp
// CUDA Driver API (低层, 完全控制)

// libcuda.so 中的实现
CUresult cuLaunchKernel(
    CUfunction f,          // kernel handle (从 CUmodule 获取)
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,  // dynamic shared memory
    CUstream hStream,
    void** kernelParams,          // kernel 参数数组
    void** extra                  // 额外配置
) {
    // Step 1: Validate kernel parameters
    // Step 2: Build the command packet (PM4 method)
    // Step 3: Submit to GPU pushbuffer via ioctl
    
    // 设置 GPU 状态:
    // - COMPUTE_PIPELINE (计算管线, 而非图形管线)
    // - SHADER_TYPE = COMPUTE
    // - COMPUTE_START_X/Y/Z = 0 (compute grid 起始)
    // - COMPUTE_NUM_THREAD_X/Y/Z = blockDim (每 block 的线程数)
    // - COMPUTE_USER_DATA_0..15 = kernel params (寄存器传递)
    // - COMPUTE_START = launch the grid
    
    // 填充 PM4 (PM4 = GPU command packet format)
    // PM4 包格式: [header | payload]
    //   header:  [type(2) | reserved | count(14) | opcode(8)]
    //   payload: method data (连续的 dword)
    
    struct PM4Packet {
        uint32_t header;
        uint32_t payload[];
    };
    
    // Method IDs (简化):
    // SET_SH_REG: 设置 shader register
    // SET_SH_REG_OFFSET: 设置带偏移的 shader register
    // INDIRECT_BUFFER: 间接 buffer (command buffer)
    // WRITE_DATA: 写数据到 GPU 地址
    // NOP: 无操作 (用于对齐)
    
    // 实际提交:
    // ioctl(fd, NV_ESC_RM_ALLOC_MEMORY, ...) — 分配 GPU pushbuffer 空间
    // 写入 PM4 packets
    // ioctl(fd, NV_ESC_RM_CONTROL, ...) — 通知 GPU 有新命令
}
```

### 5.3 nvidia.ko — 内核模块的 ioctl 路径

```c
// nvidia.ko 内核模块处理 cuLaunchKernel 的 ioctl 调用

// drivers/gpu/drm/nouveau/nvkm/...  (Nouveau 开源驱动类似)
// 隐藏闭源驱动的近似逻辑:

// nvidia.ko 的 ioctl 分发表
static long nvidia_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
    case NV_ESC_RM_ALLOC_MEMORY:
        // 分配 GPU 可访问的内核内存 (pushbuffer, 命令缓冲区)
        return nv_rm_alloc(file, (void __user *)arg);
    
    case NV_ESC_RM_CONTROL:
        // 提交命令到 GPU (触发 launch)
        return nv_rm_control(file, (void __user *)arg);
    
    case NV_ESC_RM_ALLOC:
        // 分配 GPU 资源 (context, memory, channel, 等)
        return nv_rm_alloc_resource(file, (void __user *)arg);
    
    case NV_ESC_RM_FREE:
        // 释放 GPU 资源
        return nv_rm_free(file, (void __user *)arg);
    
    case NV_ESC_REGISTER_FD:
        // 注册一个 fd (用于 CUDA IPC, GPUDirect 等)
        return nv_register_fd(file, (void __user *)arg);
    
    default:
        return -ENOTTY;
    }
}

// nv_rm_control: 提交 GPU 命令
static int nv_rm_control(struct file *filp, void __user *arg) {
    struct nv_rm_control_params params;
    
    if (copy_from_user(&params, arg, sizeof(params)))
        return -EFAULT;
    
    // Step 1: 获取 GPU channel (类似 CPU 的进程上下文)
    struct nv_channel *channel = get_channel(filp, params.client_handle);
    
    // Step 2: 构建 GPU 命令 (PM4 packets)
    // 包括: SET_SH_REG for kernel config, INDIRECT_BUFFER for launch
    uint32_t *pushbuffer = channel->pushbuffer_cpu_addr;
    uint32_t pushbuffer_offset = channel->pushbuffer_offset;
    
    // PD (Pushbuffer DWord) — 写入 GPU 命令
    // 通过 PCIe MMIO BAR 写入 GPU 寄存器
    
    // 方法: 使用 WC (Write-Combining) 内存映射
    // GPU MMIO 区域通过 PCIe BAR 映射到 CPU 虚拟地址空间
    void __iomem *gpu_mmio = channel->gpu_mmio_base;
    
    // 写入 PM4 packets 到 pushbuffer (CPU 端内存)
    pushbuffer[pushbuffer_offset++] = PM4_TYPE3_HEADER(PM4_SET_SH_REG, 3);
    pushbuffer[pushbuffer_offset++] = SH_REG_COMPUTE_START_X;
    pushbuffer[pushbuffer_offset++] = 0;                    // START_X = 0
    pushbuffer[pushbuffer_offset++] = blockDimX;            // NUM_THREAD_X
    
    pushbuffer[pushbuffer_offset++] = PM4_TYPE3_HEADER(PM4_SET_SH_REG, 3);
    pushbuffer[pushbuffer_offset++] = SH_REG_COMPUTE_START_Y;
    pushbuffer[pushbuffer_offset++] = 0;                    // START_Y = 0
    pushbuffer[pushbuffer_offset++] = blockDimY;            // NUM_THREAD_Y
    
    pushbuffer[pushbuffer_offset++] = PM4_TYPE3_HEADER(PM4_SET_SH_REG, 3);
    pushbuffer[pushbuffer_offset++] = SH_REG_COMPUTE_START_Z;
    pushbuffer[pushbuffer_offset++] = 0;                    // START_Z = 0
    pushbuffer[pushbuffer_offset++] = blockDimZ;            // NUM_THREAD_Z
    
    // 传递 kernel 参数 (通过 USER_DATA registers)
    for (int i = 0; i < num_params; i++) {
        pushbuffer[pushbuffer_offset++] = PM4_TYPE3_HEADER(PM4_SET_SH_REG, 1);
        pushbuffer[pushbuffer_offset++] = SH_REG_COMPUTE_USER_DATA_0 + i;
        pushbuffer[pushbuffer_offset++] = params.kernel_params[i];
    }
    
    // INDIRECT_BUFFER: 告诉 GPU 从哪里开始执行
    pushbuffer[pushbuffer_offset++] = PM4_TYPE3_HEADER(
        PM4_INDIRECT_BUFFER, 3
    );
    pushbuffer[pushbuffer_offset++] = lower_32(kernel_code_gpu_addr);
    pushbuffer[pushbuffer_offset++] = upper_32(kernel_code_gpu_addr);
    pushbuffer[pushbuffer_offset++] = kernel_code_size;
    
    // Step 3: 刷新 pushbuffer → 通过 MMIO 写入 GPU doorbell
    channel->pushbuffer_offset = pushbuffer_offset;
    
    // 写 GPU PUT register (doorbell)
    // PUT 寄存器告诉 GPU 有新增的命令
    writel(pushbuffer_offset, gpu_mmio + NV_PFIFO_GPFIFO_PUT);
    
    // GPU 会异步提取 pushbuffer 并执行
    return 0;
}
```

---

## 6. Step 5: GPU 硬件执行

### 6.1 GigaThread Engine 分发

```
Host (CPU)                     nvidia.ko                    GPU Hardware
    │                              │                              │
    │ cuLaunchKernel(...)          │                              │
    ├──────────────────────────────►                              │
    │   ioctl(NV_ESC_RM_CONTROL)    │                              │
    │                              ├──── MMIO write(PUT register)─►
    │                              │   (doorbell: new commands    │
    │                              │    available in pushbuffer)  │
    │                              │                              │
    │                              │          ┌───────────────────┤
    │                              │          │ GigaThread Engine │
    │                              │          │ (GTE) picks up    │
    │                              │          │ pushbuffer cmds   │
    │                              │          │                   │
    │                              │          │ Parse PM4 packets: │
    │                              │          │ - SET_SH_REG:      │
    │                              │          │   configure SM    │
    │                              │          │   state           │
    │                              │          │ - INDIRECT_BUFFER:│
    │                              │          │   load kernel code│
    │                              │          │   into I-cache     │
    │                              │          │                   │
    │                              │          │ Dispatch:          │
    │                              │          │ grid_dim.x ×      │
    │                              │          │ grid_dim.y ×      │
    │                              │          │ grid_dim.z blocks  │
    │                              │          │ → distribute to   │
    │                              │          │   available TPCs  │
    │                              │          │   → SMs           │
    │                              │          └───────────────────┤
```

### 6.2 SM 端执行时序

```
SM 内部一个 Block 的执行序列:

┌──────────────────────────────────────────────────────────────────────┐
│ Cycles 0-4:    Warp Scheduler 选取第一个 warp (warp 0)              │
│                Scoreboard: all warps initially ready                 │
│                Dispatch: LDG (load global) → warp 0                 │
│                                                                     │
│ Cycles 5-8:    Warp Scheduler 选取 warp 1 (warp 0 waiting for LDG)  │
│                Dispatch: LDG → warp 1                               │
│                                                                     │
│ Cycles 9-12:   Warp Scheduler 选取 warp 2                           │
│                Dispatch: LDG → warp 2                               │
│                                                                     │
│ Cycles 13-16:  Warp Scheduler 选取 warp 3                           │
│                Dispatch: LDG → warp 3                               │
│                                                                     │
│ Cycle 17:      warp 0 LDG complete → Scoreboard marks warp 0 ready  │
│                ... (GPU LDG latency ~200-800 cycles for HBM)        │
│                Meanwhile, other warps issued in round-robin          │
│                                                                     │
│ Cycle 500:     warp 0 picked again, now loads HMMA (Tensor Core)    │
│                (operands from registers after LDG, NOT from HBM)    │
│                HMMA latency: ~8-16 cycles (pipelined)              │
│                                                                     │
│ Cycle 508:     warp 0 issues next HMMA                              │
│                (Tensor Core pipelined: new op every ~4 cycles)      │
│                                                                     │
│ Cycle 1500:    warp 0 starts epilogue: apply bias + activation      │
│                FFMA (fused multiply-add on CUDA cores)              │
│                                                                     │
│ Cycle 1600:    warp 0 issues STG (store global)                     │
│                STG is fire-and-forget (non-blocking)                │
│                Data written to L2 → eventually evicted to HBM3      │
│                                                                     │
│ Cycle 2000:    All warps complete → thread block exits              │
│                SM resources freed → next block can start            │
└──────────────────────────────────────────────────────────────────────┘

Warp 切换特点:
  - 零开销 (硬件线程, 无上下文保存/恢复)
  - Round-robin 调度 (4 warp schedulers, 每个管理 16 warps)
  - Scoreboard 跟踪每个 warp 的指令依赖
```

---

## 7. 时序分析：一个 Transformer Block

### 7.1 单个 Block 的时序拆解

以 Llama-2-7B, B=1, seq_len=2048, H100, FP16 为基准：

```
┌─────────────────────────────────────────────────────────────┐
│  Operation                    │  Time (µs)  │  占比        │
├─────────────────────────────────────────────────────────────┤
│  Input LayerNorm              │     5       │   1.5%       │
│  QKV Projection (GEMM)        │   150       │  44.1%       │
│  ┌─ Q: [1,2048,4096]×[4096,4096]                          │
│  ├─ K: same shape                                          │
│  └─ V: same shape                                          │
│  注: QKV 合并为一个 GEMM, 形状 [1,2048,4096]×[4096,12288]  │
│                                                             │
│  RoPE (Rotary Embedding)      │    10       │   2.9%       │
│  Attention (sdpa / FA)        │    20       │   5.9%       │
│  ┌─ Q·K^T: [1,32,2048,128]×[1,32,2048,128]                │
│  ├─ Softmax (online, tiled)                                │
│  └─ softmax(QK^T)·V                                        │
│  注: 使用 Flash Attention v2, IO-aware tiling              │
│                                                             │
│  O Projection (GEMM)          │    50       │  14.7%       │
│  [1,2048,4096]×[4096,4096]                                 │
│                                                             │
│  Post-Attention LayerNorm     │     5       │   1.5%       │
│                                                             │
│  Gate Projection (GEMM)       │    40       │  11.8%       │
│  [1,2048,4096]×[4096,11008]                                │
│                                                             │
│  Up Projection (GEMM)         │    40       │  11.8%       │
│  [1,2048,4096]×[4096,11008]                                │
│  (可以和 gate GEMM 融合)                                    │
│                                                             │
│  SiLU Activation              │     2       │   0.6%       │
│  Element-wise Multiply        │     2       │   0.6%       │
│                                                             │
│  Down Projection (GEMM)       │    50       │  14.7%       │
│  [1,2048,11008]×[11008,4096]                               │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  Total Forward (1 block)      │  ~340       │ 100.0%       │
└─────────────────────────────────────────────────────────────┘

按运算类型汇总:
  GEMM (矩阵乘法):     ~290 µs (85%) — 纯 Tensor Core bound
  Attention:            ~20 µs (6%)  — memory bound (KV cache 读)
  Element-wise/RoPE:    ~14 µs (4%)  — CUDA Core bound (memory-bandwidth limited)
  Norm:                 ~10 µs (3%)  — CUDA Core bound (reduction)
  Overhead/其他:         ~6 µs (2%)  — kernel launch, sync
```

### 7.2 Llama-2-7B 完整 Forward (32 layers)

```
Llama-2-7B: 32 个 Transformer block + embedding + lm_head

┌─────────────────────────────────────────────────────────────┐
│  Operation                    │  Time (µs)  │  占比        │
├─────────────────────────────────────────────────────────────┤
│  Token Embedding              │     2       │   0.02%      │
│  32 × Transformer Block       │ 10880       │  99.3%       │
│  (340 µs × 32)                │             │              │
│  Final LayerNorm              │     5       │   0.05%      │
│  LM Head (GEMM)               │    60       │   0.5%       │
│  [1,1,4096]×[4096,32000]                                   │
├─────────────────────────────────────────────────────────────┤
│  Total Forward (32 layers)    │ ~10950      │ 100.0%       │
│   ≈ 11 ms for seq_len=2048    │             │              │
│   Tokens per second (prefill) │ ~187 tokens  │             │
└─────────────────────────────────────────────────────────────┘

Decode 阶段 (每次处理 1 个 token):
  每个 block: ~290 µs (GEMM) + ~20 µs (attn on full KV) ≈ 310 µs
  32 blocks: ~10 ms per token
  (KV cache 增长后 attention 时间会增加, 接近 memory-bound)
  Memory bandwidth bound, NOT compute bound
```

### 7.3 NCCL All-Reduce 时序 (多 GPU 训练)

```python
# 多 GPU 训练时, 每个 backward 后需要 all-reduce gradient
# 通过 NCCL + NVSwitch 实现

import torch.distributed as dist

# Gradient all-reduce
def all_reduce_gradients(model, world_size):
    for param in model.parameters():
        if param.grad is not None:
            # NCCL all-reduce: ring or tree algorithm
            dist.all_reduce(param.grad, op=dist.ReduceOp.SUM)
            param.grad /= world_size  # 平均

# 时序 (H100 × 8, NVSwitch):
#   Gradient size per layer: ~4.7 MB (Llama-2-7B, FP32 grad)
#   NVSwitch bandwidth: 900 GB/s bidir per GPU → 450 GB/s one-way
#
#   Time per all-reduce for one layer:
#     Size = 4.7 MB, Algo = Ring (best for small messages)
#     Ring: 2 × (N-1)/N × (data_size / bandwidth)
#          = 2 × 7/8 × (4.7 MB / 450 GB/s)
#          = 1.75 × 10.4 µs
#          ≈ 18 µs per layer's gradient
#
#   For 32 layers: 32 × 18 µs ≈ 576 µs per step
#
#   But NCCL can overlap communication with backward computation:
#     As backward finishes layer L, immediately start all-reduce for layer L
#     While still computing backward for layers L-1, L-2, ...
#   → Effective overhead < 100 µs per step

# 总训练步时间 (B=1, seq=2048):
#   Forward:  ~11 ms
#   Backward: ~22 ms (≈ 2× forward)
#   NCCL all-reduce: ~0.6 ms (overlapped with backward)
#   Optimizer step (AdamW): ~2 ms
#   Total: ~35 ms per step
#
#   Throughput: ~59 tokens/s (B=1), ~1888 tokens/s (B=32)
```

---

## 8. NCCL All-Reduce 的 GPU 硬件视角

### 8.1 Ring Algorithm over NVSwitch

```
8 GPU All-Reduce via NVSwitch (Fully Connected Topology):

GPU 0 ──NVSwitch── GPU 1      GPU 每个 input 分割成 7 份 (N-1)
GPU 2 ──NVSwitch── GPU 3      Ring algorithm: 2 轮通信
GPU 4 ──NVSwitch── GPU 5
GPU 6 ──NVSwitch── GPU 7

NVSwitch 特性:
  - 全互联: 每个 GPU 通过 900 GB/s 同时与任意其他 GPU 通信
  - 无阻塞: 等效于 all-to-all crossbar
  - 延迟: ~1 µs (NVSwitch 内部交换)

Ring Algorithm (适用于 NVSwitch):
┌──────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  第 1 轮: Scatter-Reduce (N-1 steps)                                │
│    Step 1: GPU i sends chunk i to GPU i+1                           │
│    Step 2: GPU i sends received chunk i-1 to GPU i+1                │
│    ...                                                               │
│    Step N-1: 每个 GPU 持有 chunk i 的 reduce-som 部分              │
│                                                                     │
│  第 2 轮: All-Gather (N-1 steps)                                    │
│    Step 1: GPU i sends its reduced chunk to GPU i+1                │
│    Step 2: GPU i forwards received chunk to GPU i+1                │
│    ...                                                               │
│    Step N-1: 所有 GPU 都拥有完整的 reduced result                   │
│                                                                     │
└──────────────────────────────────────────────────────────────────────┘

总时间 (ring over NVSwitch):
  T = 2 × (N-1) × (α + data_per_gpu / B)
  其中:
    α = 启动延迟 (~1 µs)
    data_per_gpu = total_data / N
    B = NVSwitch 带宽 = 900 GB/s bidir / 2 = 450 GB/s effective
  
  对于 4.7 MB, N=8:
    T = 2 × 7 × (1 µs + 4.7MB/450GB/s)
      = 14 × (1 µs + 10.4 µs)
      = 14 × 11.4 µs
      = 160 µs

实际测量:
  - 小消息 (≤ 256 KB): 使用 NVSwitch direct (无 ring)
  - 中等消息 (256 KB - 4 MB): 使用 ring
  - 大消息 (> 4 MB): 使用 tree algorithm (更优)
```

### 8.2 NCCL 的内部调用链

```
PyTorch (dist.all_reduce)
  ↓
torch._C._distributed_c10d.ProcessGroupNCCL::allreduce
  ↓ C++ bindings
c10d::ProcessGroupNCCL::allreduce_impl
  ↓ 入队到 NCCL collective queue
ncclAllReduce(sendbuff, recvbuff, count, datatype, op, comm, stream)
  ↓ libnccl.so
ncclAllReduce_impl
  ↓ 协商最佳算法 (thread 0 of each rank)
ncclTopoGetAlgoTime  → 选择 ring/tree/collNetDirect
  ↓ 提交通信 kernel
ncclKernel_AllReduce_Ring_LL(...)  // 或 _Tree, _CollNet
  ↓ 每个 chunk:
cuMemcpyPeerAsync 或 直接 NVLink copy
  ↓
GPU SM executes: reduce + copy → NVLink → NVSwitch → peer GPU
```

---

## 9. 全链路调用图

### 9.1 完整 ASCII 调用栈

```
┌──────────────────────────────────────────────────────────────────────────────┐
│            从 PyTorch 到 GPU 晶体管 — 完整调用链                                 │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ Layer 1: Python User Code                                           │    │
│  │                                                                     │    │
│  │   model.forward(input_ids)                                          │    │
│  │     ↓                                                               │    │
│  │   nn.Linear(input).__call__(input)                                  │    │
│  │     ↓                                                               │    │
│  │   F.linear(input, weight, bias)                                     │    │
│  │     ↓                                                               │    │
│  │   torch._C._nn.linear(input, weight, bias)                          │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│  ┌──────────────────────────▼──────────────────────────────────────────┐    │
│  │ Layer 2: ATen C++ Dispatcher                                        │    │
│  │                                                                     │    │
│  │   at::native::linear(input, weight, bias)                           │    │
│  │     ↓                                                               │    │
│  │   at::native::matmul / at::native::addmm                            │    │
│  │     ↓ DispatchKey::CUDA                                             │    │
│  │   at::native::addmm_out_cuda(input, weight.t(), bias, ...)          │    │
│  │     ↓                                                               │    │
│  │   at::cuda::blas::gemm<at::Half>(...)                               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│  ┌──────────────────────────▼──────────────────────────────────────────┐    │
│  │ Layer 3: Acceleration Libraries (cuBLAS)                            │    │
│  │                                                                     │    │
│  │   cublasGemmEx(handle, transa, transb, m, n, k, ...)                │    │
│  │     ↓ auto-tuning: select best tile size + epilogue                 │    │
│  │   cublasLtMatmul(desc, alpha, A, B, beta, C, D, algo, ws, stream)  │    │
│  │     ↓ 选择或编译 kernel 变体                                        │    │
│  │   gemm_kernel_256x128x32<<<grid, block, smem, stream>>>(A, B, C)   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│  ┌──────────────────────────▼──────────────────────────────────────────┐    │
│  │ Layer 4: CUDA Runtime + Driver                                      │    │
│  │                                                                     │    │
│  │   cudaLaunchKernel(kernel, grid, block, args, smem, stream)         │    │
│  │     ↓ libcudart.so → libcuda.so                                     │    │
│  │   cuLaunchKernel(func, gx,gy,gz, bx,by,bz, smem, stream, args)     │    │
│  │     ↓ 构建 PM4 command packets                                      │    │
│  │     ↓ ioctl(/dev/nvidiactl, NV_ESC_RM_CONTROL, ...)                 │    │
│  │   syscall ioctl → kernel space                                       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│  ┌──────────────────────────▼──────────────────────────────────────────┐    │
│  │ Layer 5: nvidia.ko Kernel Module                                    │    │
│  │                                                                     │    │
│  │   nvidia_unlocked_ioctl(filp, NV_ESC_RM_CONTROL, arg)               │    │
│  │     ↓ 获取 GPU channel                                              │    │
│  │   nv_rm_control:                                                    │    │
│  │     - 填充 pushbuffer (PM4 packets):                                │    │
│  │       · SET_SH_REG for compute config                               │    │
│  │       · INDIRECT_BUFFER pointing to kernel code                     │    │
│  │     - writel(MMIO_BASE + NV_PFIFO_GPFIFO_PUT, pushbuffer_offset)    │    │
│  │     ↓ PCIe MMIO write to GPU BAR                                    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                              │                                               │
│  ┌──────────────────────────▼──────────────────────────────────────────┐    │
│  │ Layer 6: GPU Hardware                                               │    │
│  │                                                                     │    │
│  │   GigaThread Engine:                                                │    │
│  │     - 读取 pushbuffer (DMA from sysmem to GPU)                      │    │
│  │     - 解析 PM4 packets                                              │    │
│  │     - 分发 grid → TPC → SM                                         │    │
│  │                                                                     │    │
│  │   SM 内部:                                                          │    │
│  │     Warp Scheduler → 选取 warp (round-robin, 4 per cycle)           │    │
│  │       ↓                                                             │    │
│  │     Dispatch Unit → 发射指令 (LDG / HMMA / FFMA / STG)              │    │
│  │       ↓                                                             │    │
│  │     LD/ST Unit → 从 L1/L2/HBM3 加载数据到寄存器                     │    │
│  │       ↓                                                             │    │
│  │     Tensor Core → mma.sync.aligned.m16n8k16 (FP16 matrix multiply)  │    │
│  │       ↓                                                             │    │
│  │     Register File ← Tensor Core 结果                                │    │
│  │       ↓                                                             │    │
│  │     CUDA Core → epilogue: bias, activation (FFMA)                   │    │
│  │       ↓                                                             │    │
│  │     LD/ST Unit → STG (store global) → L2 cache → HBM3              │    │
│  │                                                                     │    │
│  │   HBM3 VRAM:                                                        │    │
│  │     - 80 GB, 5 个 HBM3 stack, 5120-bit bus                         │    │
│  │     - 3.35 TB/s peak bandwidth                                      │    │
│  │     - 最终数据就位                                                   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  额外路径 (仅训练/多 GPU 推理):                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ NCCL All-Reduce Path:                                               │    │
│  │   torch.distributed.all_reduce(grad)                                 │    │
│  │     → ncclAllReduce(grad, grad, size, datatype, SUM, comm, stream)  │    │
│  │       → nvidia.ko: peer-to-peer NVLink setup                       │    │
│  │         → GPU SM: reduce-scatter kernel (CUDA Core)                 │    │
│  │           → NVLink 4.0 × 18 links → NVSwitch                       │    │
│  │             → peer GPU: receive + accumulate → all-gather          │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. vLLM 与本系列其他文章的关系

### 10.1 与 nvidia-svm 系列的关系

本系列（pytorch-to-gpu）与同伴系列（nvidia-svm）有以下几个交叉点：

```
本系列覆盖的层次           nvidia-svm 系列覆盖的对应层次
─────────────────────────  ─────────────────────────────────
CUDA Runtime              nvidia-svm/06-gpu-buddy-allocator
  cudaMalloc                 → nvidia.ko mmap 路径 → GPU Buddy Allocator
  cudaFree                   → 释放 GPU 页

CUDA Driver                nvidia-svm/01-hmm-basic + 03-hmm-fault
  cuMemAlloc / cuMemMap      → HMM 统一虚拟地址空间
  GPU page fault             → HMM migrate_to_device

NCCL                       nvidia-svm/08-gpudirect + 09-umem-dmabuf
  GPUDirect RDMA             → GPU 内存导出为 dmabuf → NIC 直接访问
  NCCL all-reduce            → NVLink + NVSwitch 互联

GPU PCIe                   pcie-deep-dive 系列
  PCIe BAR 映射              → MMIO 通过 PCIe 事务到达 GPU
  GPU DMA                    → PCIe Memory Read/Write TLP
```

### 10.2 端到端示例：cudaMalloc 的完整路径

```
# 当 PyTorch 执行 tensor.cuda() 时:

Python: x = x.cuda()

  ↓ 最终调用 cuMemAlloc

cuMemAlloc(&dptr, size)
  ↓ CUDA Driver API (libcuda.so)

  ↓ ioctl(/dev/nvidiactl, NV_ESC_RM_ALLOC_MEMORY, ...)

nvidia.ko: rm_alloc_memory
  ↓ 在 GPU VA space 中分配虚拟地址
  ↓ 使用 GPU Buddy Allocator 分配物理页
  ↓ 创建 GPU page table 映射 (GMMU: GPU Memory Management Unit)
  ↓ → nvidia-svm/06 (Buddy Allocator 详解)

  ↓ GPU Buddy Allocator:
  │   维护 per-order free list (order 0 = 4KB, order 1 = 8KB, ..., order max)
  │   分配: 找到合适的 order → 分割 → 返回
  │   释放: 合并 buddy → 返回 free list

  ↓ 如果需要系统内存 (cudaMallocManaged / oversubscription):
  │   触发 HMM → 分配 CPU 页 → 按需 migrate → nvidia-svm/03 (HMM fault)

返回 GPU 虚拟地址 dptr → CUDA 程序可用
```

---

## 11. 性能优化检查清单

### 11.1 PyTorch 层面

```python
# 1. 使用 torch.compile 加速
#    inductor backend 会将 Python op 链融合为单个 CUDA kernel
import torch

@torch.compile(mode="reduce-overhead")
def transformer_block(x):
    # 前向会被编译成优化后的 kernel
    # 减少 Python overhead, kernel launch overhead, 融合操作
    return model(x)

# 2. 启用 TF32 (Ampere+) 加速 GEMM
torch.backends.cuda.matmul.allow_tf32 = True
torch.backends.cudnn.allow_tf32 = True
# TF32: 19 bits mantissa (vs FP16 10 bits) → higher precision, same speed
# 对于大矩阵 GEMM, TF32 提供 FP32 精度 + FP16 性能

# 3. 使用混合精度 (AMP)
with torch.autocast(device_type='cuda', dtype=torch.float16):
    output = model(input)  # 自动使用 FP16

# 4. 避免同步操作
# ❌ x.item() / x.cpu() / torch.cuda.synchronize() — 强制 CPU-GPU 同步
# ✅ 保持异步, 批量收集结果

# 5. 使用 CUDA graphs 减少 launch overhead
g = torch.cuda.CUDAGraph()
with torch.cuda.graph(g):
    output = model(static_input)
# 后续: g.replay()
```

### 11.2 Kernel 层面

```cuda
// 1. 确保全局内存合并访问
// 2. 使用 shared memory 缓存频繁访问的数据
// 3. 避免 bank conflicts (padding shared memory arrays)
// 4. 最小化 warp divergence (分支对齐到 warp 边界)
// 5. 使用向量化加载 (float4) 提升内存带宽
// 6. 平衡 occupancy (寄存器 vs shared memory vs 线程数)
// 7. 使用 TMA (H100) 做异步数据搬运
// 8. 使用 FP8 (H100) 提升 Tensor Core 吞吐
// 9. 使用 CUDA graph 消除 launch overhead
// 10. Profile with Nsight Compute, identify stall reasons

// 检查清单 (每个 kernel 应该回答):
// ▢ 是否确保了合并的内存访问？
// ▢ 是否有效利用 shared memory？
// ▢ occupancy 是否 > 50%？
// ▢ Warp divergence 是否 < 10%？
// ▢ 是否有 bank conflicts？
// ▢ 是否隐藏了内存延迟 (足够多的 warp)？
// ▢ 是否使用了最大带宽的指令 (FP8 > FP16 > TF32 > FP32)？
```

### 11.3 系统层面

```bash
# 1. 正确设置 CPU affinity
#    GPU 线程应该 pin 到物理上靠近 GPU 的 CPU core (NUMA-aware)
numactl --cpunodebind=0 --membind=0 python train.py

# 2. 使用 CUDA MPS (Multi-Process Service) 共享 GPU
#    允许多个进程同时使用一个 GPU (提高利用率)
#    启动: nvidia-cuda-mps-control -d

# 3. 使用 MIG (Multi-Instance GPU)
#    将 A100/H100 切分为独立的 GPU 实例
#    适用于多租户场景

# 4. 预分配 CUDA memory (避免运行时 OOM)
import torch
torch.cuda.empty_cache()
torch.cuda.reset_peak_memory_stats()

# 5. 调整 GPU 频率 (需要 root)
nvidia-smi -ac 1215,1410  # 锁定内存和核心频率 (避免动态调频抖动)

# 6. 使用 ECC 模式 (默认开启, 牺牲 ~6% 带宽换取数据完整性)
nvidia-smi -e 1  # 启用 ECC
nvidia-smi -e 0  # 禁用 ECC (提升带宽, 但风险自担)
```

---

## 12. 端到端性能数据汇总

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 Llama-2-7B 单步端到端性能 (H100 SXM, FP16)                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  推理 (B=1, seq=2048 → 256 tokens):                                         │
│                                                                             │
│    Phase                      Time        GPU 利用率    瓶颈                │
│    ────────────────────────  ─────────    ──────────    ────────────        │
│    Prefill (2048 tokens)     ~11 ms       85-95%       Compute bound        │
│    Decode (per token)        ~10 ms       15-25%       Memory bound         │
│    (batch_size=1 时 decode 极度 memory bound)                               │
│                                                                             │
│    Throughput:                                                                │
│      Prefill: ~187 tokens/s                                                  │
│      Decode:  ~100 tokens/s (batch=1)                                        │
│      vLLM batch=64 decode: ~2,800 tokens/s (memory bound → batching 缓解)   │
│                                                                             │
│  训练 (B=32, seq=2048):                                                      │
│                                                                             │
│    Phase                      Time        GPU 利用率    内存                │
│    ────────────────────────  ─────────    ──────────    ────────────        │
│    Forward                    ~75 ms      90-95%        ~55 GB              │
│    Backward                   ~150 ms     95-98%        ~65 GB (峰值)       │
│    All-Reduce (NCCL, 8 GPU)   ~0.6 ms     (overlapped)  N/A                │
│    Optimizer (AdamW)          ~10 ms      20-30%        ~5 GB               │
│    Total Step                 ~236 ms                                       │
│                                                                             │
│    Throughput:                                                               │
│      ~4.2 steps/s                                                            │
│      ~280,000 tokens/s (32 × 2048 × 4.2)                                    │
│      ~22,000 tokens/s/GPU                                                    │
│                                                                             │
│  延迟 vs 吞吐 的权衡:                                                         │
│    B=1:   低延迟 (~10 ms/token), 低吞吐 (~100 tok/s)                        │
│    B=256: 高延迟 (~200 ms/token), 高吞吐 (~14,000 tok/s) — vLLM             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 13. vLLM 与其他路径的对比总结

```
┌────────────────────┬──────────────────┬───────────────────────────────────┐
│ 调用路径           │ 执行引擎         │ 特点                              │
├────────────────────┼──────────────────┼───────────────────────────────────┤
│ PyTorch Eager      │ cuBLAS           │ 每次 op → 一次 kernel launch     │
│                    │ (cublasGemmEx)   │ Python dispatch overhead          │
│                    │                  │ 标准 KV cache (连续, 碎片化)       │
├────────────────────┼──────────────────┼───────────────────────────────────┤
│ PyTorch torch.comp.│ Triton           │ Op fusion → 减少 kernel launch    │
│ (inductor)         │ (PTX生成)        │ 自动 tiling                       │
│                    │                  │ 对 attention 有 Flash Attention   │
├────────────────────┼──────────────────┼───────────────────────────────────┤
│ vLLM               │ PagedAttention   │ Paged KV cache → 96% 利用率       │
│                    │ custom CUDA kern │ Continuous batching → 消除 idle   │
│                    │ + cuBLAS for FFN │ Block table → virtual memory      │
│                    │                  │ Online softmax → O(n) memory      │
├────────────────────┼──────────────────┼───────────────────────────────────┤
│ TensorRT-LLM       │ TRT compiled     │ 整图编译 (最强的 kernel fusion)   │
│                    │ kernels          │ FP8/INT8 量化优化                 │
│                    │                  │ 最大吞吐 (但需要编译步骤)          │
├────────────────────┼──────────────────┼───────────────────────────────────┤
│ llama.cpp          │ 自定义 CUDA/Metal│ 极低精度量化 (Q4, Q2, IQ)         │
│ (ggml/gguf)        │ kernels          │ CPU + GPU hybrid execution        │
│                    │                  │ 消费端 GPU (RTX 系列) 优化        │
└────────────────────┴──────────────────┴───────────────────────────────────┘
```

---

## 系列结语

本系列 6 篇文章构成了从 PyTorch 顶层 API 到 NVIDIA GPU 物理晶体管的完整知识图谱：

| 文章 | 主题 | 覆盖层次 |
|------|------|----------|
| 第 1 篇 | PyTorch Tensor & Autograd | 用户代码 → 自动微分引擎 |
| 第 2 篇 | ATen Dispatcher | Python → C++ dispatch, 算子注册 |
| 第 3 篇 | cuBLAS/cuDNN/NCCL | 加速库, GEMM tiling, Flash Attention |
| 第 4 篇 | vLLM & PagedAttention | KV cache 虚拟化, continuous batching |
| 第 5 篇 | GPU 架构 | SM, Warp, Tensor Core, TMA, 内存层次 |
| 第 6 篇 | 全链路调用 | Python → C++ → 加速库 → Driver → nvidia.ko → GPU |

**核心认知**：

1. **抽象层次多但非冗余** — 每一层都有明确的设计目标：
   - PyTorch Python 层提供易用性和动态图
   - ATen C++ 层提供类型安全的算子 dispatch 和 autograd 集成
   - cuBLAS/cuDNN 层提供硬件架构感知的自动调优
   - CUDA Runtime/Driver 层管理 GPU 资源和内核状态
   - nvidia.ko 层处理 CPU↔GPU 的 PCIe 通信和 pushbuffer 管理
   - GPU 硬件层以 SIMT/Tensor Core 模型执行大规模并行计算

2. **性能瓶颈在各层间漂移**：
   - Prefill 阶段：compute bound（Tensor Core GEMM 占 85% 时间）
   - Decode 阶段（小 batch）：memory bound（KV cache 读取是瓶颈）
   - Decode 阶段（大 batch, vLLM）：memory bound 但被 continuous batching 分摊
   - 多 GPU 训练：forward/backward 占 95%，NCCL 通信被完美重叠

3. **优化的核心原则**：
   - 数据局部性（shared memory, register, tiling）带来 10-100× 加速
   - 内存管理策略（PagedAttention）可以比纯计算优化更有效
   - 异步执行（CUDA streams, TMA, non-blocking）隐藏 CPU-GPU 同步延迟
   - 硬件感知编程（coalescing, bank conflicts, occupancy）将架构潜力转化为实际性能

4. **与本系列其他文章的呼应**：
   - 当 CUDA 执行 `cuMemAlloc` → 穿过 nvidia.ko 的 mmap → GPU Buddy Allocator (`nvidia-svm/06`)
   - 当 NCCL 使用 GPUDirect RDMA → umem_dmabuf 导出 GPU 内存 → mlx5 MR 注册 (`nvidia-svm/08+09`)
   - 当 GPU 缺页 → HMM 触发 migrate (`nvidia-svm/01+03`)
   - 整个 GPU 设备挂在 PCIe 总线上 (`pcie-deep-dive/`)

从 `model.forward()` 出发，历经 Python→C++→加速库→CUDA Runtime→CUDA Driver→nvidia.ko→GPU GigaThread Engine→TPC→SM→Warp Scheduler→Tensor Core→Register File→L1→L2→HBM3，这就是 PyTorch 模型在 NVIDIA GPU 上完整运行的物理图景。

希望这 6 篇文章能够帮助你建立起从应用层到底层硬件的立体认知。当你下次运行 `model.forward()` 时，你将看到的不只是 Python 代码，而是成千上万的 Warp 在 Tensor Core 上并行运转，数据在 Register→Shared Memory→L1→L2→HBM3 之间精确定时流淌的整体交响。
