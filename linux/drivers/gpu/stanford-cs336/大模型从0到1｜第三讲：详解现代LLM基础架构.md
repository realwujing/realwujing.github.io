# 大模型从0到1｜第三讲：详解现代LLM基础架构

> 课程链接：[Stanford CS336 Spring 2025 - Lecture 3: Architecture](https://github.com/stanford-cs336/spring2025-lectures/blob/main/nonexecutable/2025%20Lecture%203%20-%20architecture.pdf)

---

## 第一部分：Transformer架构回顾

### 1. 标准Transformer结构

**核心组件**：
```
输入 Token IDs
    ↓
Token Embedding + Position Embedding
    ↓
┌─────────────────────────────────┐
│  Transformer Block 1            │
│  ├─ Layer Norm                  │
│  ├─ Multi-Head Attention        │
│  ├─ Residual Connection         │
│  ├─ Layer Norm                  │
│  ├─ Feed-Forward Network        │
│  └─ Residual Connection         │
└─────────────────────────────────┘
    ↓
    ... (重复N层)
    ↓
Layer Norm
    ↓
Language Model Head (Linear)
    ↓
输出 Logits
```

### 2. 三种主流架构类型

| 架构类型 | 注意力模式 | 典型应用 | 代表模型 |
| :--- | :--- | :--- | :--- |
| **Encoder-only** | 双向注意力 | 理解任务（分类、NER） | BERT, RoBERTa |
| **Decoder-only** | 因果注意力 | 生成任务 | GPT系列, Llama |
| **Encoder-Decoder** | 编码器双向+解码器因果 | 序列到序列 | T5, BART |

**本课重点**：Decoder-only架构（现代LLM的主流选择）


---

## 第二部分：位置编码（Position Encoding）

位置编码是让模型理解Token顺序的关键机制。

### 1. 绝对位置编码

#### 1.1 正弦位置编码（Sinusoidal Position Encoding）

**原始Transformer使用的方法**

**公式**：
```
PE(pos, 2i) = sin(pos / 10000^(2i/d_model))
PE(pos, 2i+1) = cos(pos / 10000^(2i/d_model))
```

**特点**：
- 固定的、不可学习的
- 可以外推到训练时未见过的序列长度
- 不同位置之间有确定的数学关系

**PyTorch实现**：
```python
import torch
import torch.nn as nn
import math

class SinusoidalPositionEncoding(nn.Module):
    def __init__(self, d_model, max_len=5000):
        super().__init__()
        
        # 创建位置编码矩阵 [max_len, d_model]
        pe = torch.zeros(max_len, d_model)
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)
        
        # 计算除数项
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * 
                            (-math.log(10000.0) / d_model))
        
        # 应用sin和cos
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        
        # 注册为buffer（不参与梯度更新）
        pe = pe.unsqueeze(0)  # [1, max_len, d_model]
        self.register_buffer('pe', pe)
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        return x + self.pe[:, :x.size(1), :]
```

**优点**：
- 无需训练
- 理论上可以处理任意长度
- 位置之间的相对关系可以通过三角函数性质表达

**缺点**：
- 不如可学习的位置编码灵活
- 在实践中性能略逊


#### 1.2 可学习的绝对位置编码（Learned Absolute Position Encoding）

**GPT-2、BERT使用的方法**

**实现**：
```python
class LearnedPositionEncoding(nn.Module):
    def __init__(self, max_len, d_model):
        super().__init__()
        # 位置嵌入表，可学习
        self.position_embeddings = nn.Embedding(max_len, d_model)
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        batch_size, seq_len, _ = x.size()
        
        # 创建位置索引
        positions = torch.arange(seq_len, device=x.device).unsqueeze(0)
        # positions: [1, seq_len]
        
        # 获取位置嵌入
        position_embeddings = self.position_embeddings(positions)
        # position_embeddings: [1, seq_len, d_model]
        
        return x + position_embeddings
```

**优点**：
- 可以学习到最适合任务的位置表示
- 实践中通常性能更好

**缺点**：
- 无法外推到超过max_len的序列
- 需要额外的参数（max_len × d_model）

### 2. 相对位置编码

相对位置编码关注的是Token之间的相对距离，而不是绝对位置。

#### 2.1 ALiBi (Attention with Linear Biases)

**核心思想**：在注意力分数上添加与相对距离成比例的偏置。

**公式**：
```
attention_score(q_i, k_j) = q_i · k_j - m × |i - j|
```
其中m是每个注意力头特定的斜率。

**实现**：
```python
class ALiBiPositionBias(nn.Module):
    def __init__(self, num_heads, max_seq_len=2048):
        super().__init__()
        # 为每个头计算斜率
        slopes = torch.tensor(self._get_slopes(num_heads))
        
        # 创建相对位置矩阵
        # alibi: [num_heads, max_seq_len, max_seq_len]
        alibi = self._build_alibi_tensor(slopes, max_seq_len)
        self.register_buffer('alibi', alibi)
    
    def _get_slopes(self, num_heads):
        # 计算几何序列的斜率
        def get_slopes_power_of_2(n):
            start = 2 ** (-(2 ** -(math.log2(n) - 3)))
            ratio = start
            return [start * (ratio ** i) for i in range(n)]
        
        if math.log2(num_heads).is_integer():
            return get_slopes_power_of_2(num_heads)
        else:
            # 处理非2的幂次的情况
            closest_power_of_2 = 2 ** math.floor(math.log2(num_heads))
            return (get_slopes_power_of_2(closest_power_of_2) + 
                   self._get_slopes(2 * closest_power_of_2)[0::2][:num_heads - closest_power_of_2])
    
    def _build_alibi_tensor(self, slopes, max_seq_len):
        # 创建相对位置矩阵
        # 对于位置i和j，值为 -slope * |i - j|
        alibi = torch.zeros(len(slopes), max_seq_len, max_seq_len)
        for head_idx, slope in enumerate(slopes):
            for i in range(max_seq_len):
                for j in range(max_seq_len):
                    alibi[head_idx, i, j] = -slope * abs(i - j)
        return alibi
    
    def forward(self, attention_scores):
        # attention_scores: [batch, num_heads, seq_len, seq_len]
        seq_len = attention_scores.size(-1)
        return attention_scores + self.alibi[:, :seq_len, :seq_len]
```

**优点**：
- 无需额外参数（除了预定义的斜率）
- 外推性能优秀
- 训练和推理效率高

**缺点**：
- 相比可学习的方法，灵活性稍低

**使用模型**：BLOOM, MPT


#### 2.2 RoPE (Rotary Position Embedding)

**核心思想**：通过旋转变换将相对位置信息编码到Query和Key中。

**数学原理**：
对于位置m的向量，应用旋转矩阵R_m：
```
q_m = R_m · q
k_n = R_n · k

q_m^T · k_n = q^T · R_m^T · R_n · k = q^T · R_(n-m) · k
```
这样注意力分数只依赖于相对位置(n-m)。

**实现**：
```python
class RotaryPositionEmbedding(nn.Module):
    def __init__(self, dim, max_seq_len=2048, base=10000):
        super().__init__()
        # 计算频率
        inv_freq = 1.0 / (base ** (torch.arange(0, dim, 2).float() / dim))
        self.register_buffer('inv_freq', inv_freq)
        
        # 预计算cos和sin值
        t = torch.arange(max_seq_len, dtype=torch.float)
        freqs = torch.einsum('i,j->ij', t, inv_freq)
        emb = torch.cat((freqs, freqs), dim=-1)
        
        self.register_buffer('cos_cached', emb.cos()[None, None, :, :])
        self.register_buffer('sin_cached', emb.sin()[None, None, :, :])
    
    def rotate_half(self, x):
        """旋转输入张量的一半维度"""
        x1, x2 = x[..., :x.shape[-1]//2], x[..., x.shape[-1]//2:]
        return torch.cat((-x2, x1), dim=-1)
    
    def forward(self, q, k, seq_len=None):
        """
        q, k: [batch, num_heads, seq_len, head_dim]
        """
        if seq_len is None:
            seq_len = q.shape[2]
        
        # 获取对应长度的cos和sin
        cos = self.cos_cached[:, :, :seq_len, :]
        sin = self.sin_cached[:, :, :seq_len, :]
        
        # 应用旋转
        q_embed = (q * cos) + (self.rotate_half(q) * sin)
        k_embed = (k * cos) + (self.rotate_half(k) * sin)
        
        return q_embed, k_embed
```

**使用示例**：
```python
# 在Multi-Head Attention中使用
class MultiHeadAttentionWithRoPE(nn.Module):
    def __init__(self, d_model, num_heads):
        super().__init__()
        self.num_heads = num_heads
        self.d_k = d_model // num_heads
        
        self.W_q = nn.Linear(d_model, d_model)
        self.W_k = nn.Linear(d_model, d_model)
        self.W_v = nn.Linear(d_model, d_model)
        self.W_o = nn.Linear(d_model, d_model)
        
        # RoPE
        self.rope = RotaryPositionEmbedding(self.d_k)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, d_model = x.size()
        
        # 线性投影
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        K = self.W_k(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        V = self.W_v(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        
        # 应用RoPE
        Q, K = self.rope(Q, K, seq_len)
        
        # 标准注意力计算
        scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(self.d_k)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        output = torch.matmul(attention_weights, V)
        
        # 重塑并输出投影
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, d_model)
        output = self.W_o(output)
        
        return output
```

**优点**：
- 优秀的外推性能
- 相对位置信息自然编码
- 不增加额外参数

**缺点**：
- 实现相对复杂
- 计算开销略高于简单的位置编码

**使用模型**：Llama, Llama 2, PaLM, GPT-NeoX

### 3. 位置编码方法对比

| 方法 | 类型 | 参数量 | 外推能力 | 代表模型 |
| :--- | :--- | :--- | :--- | :--- |
| **Sinusoidal** | 绝对 | 0 | ✓ | 原始Transformer |
| **Learned Absolute** | 绝对 | max_len × d_model | ✗ | GPT-2, BERT |
| **ALiBi** | 相对 | 0 | ✓✓ | BLOOM, MPT |
| **RoPE** | 相对 | 0 | ✓✓ | Llama, PaLM |

**趋势**：现代大模型倾向于使用相对位置编码（RoPE或ALiBi），因为它们在长序列外推上表现更好。


---

## 第三部分：归一化层（Normalization）

归一化是稳定深度网络训练的关键技术。

### 1. Layer Normalization

**标准LayerNorm（原始Transformer）**

**公式**：
```
LN(x) = γ · (x - μ) / √(σ² + ε) + β
```
其中：
- μ = mean(x)：均值
- σ² = var(x)：方差
- γ, β：可学习的缩放和偏移参数
- ε：数值稳定性常数（通常为1e-5或1e-6）

**实现**：
```python
class LayerNorm(nn.Module):
    def __init__(self, d_model, eps=1e-6):
        super().__init__()
        self.gamma = nn.Parameter(torch.ones(d_model))
        self.beta = nn.Parameter(torch.zeros(d_model))
        self.eps = eps
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        mean = x.mean(dim=-1, keepdim=True)
        std = x.std(dim=-1, keepdim=True)
        return self.gamma * (x - mean) / (std + self.eps) + self.beta
```

**特点**：
- 对每个样本的每个位置独立归一化
- 在特征维度上计算统计量
- 适合序列模型（不依赖batch size）

### 2. RMSNorm (Root Mean Square Normalization)

**简化版的LayerNorm，去掉了均值中心化和偏移项**

**公式**：
```
RMSNorm(x) = γ · x / RMS(x)
RMS(x) = √(mean(x²) + ε)
```

**实现**：
```python
class RMSNorm(nn.Module):
    def __init__(self, d_model, eps=1e-6):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(d_model))
        self.eps = eps
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        # 计算RMS
        rms = torch.sqrt(torch.mean(x ** 2, dim=-1, keepdim=True) + self.eps)
        # 归一化并缩放
        return self.weight * x / rms
```

**优点**：
- 计算更快（省略了均值计算和偏移）
- 参数更少（只有γ，没有β）
- 实践中性能与LayerNorm相当或更好
- 训练更稳定

**使用模型**：Llama, Llama 2, Gopher

### 3. LayerNorm的位置：Pre-LN vs Post-LN

#### 3.1 Post-LN（原始Transformer）

```python
# Post-LN架构
def transformer_block_post_ln(x):
    # 注意力子层
    x = x + attention(x)
    x = layer_norm(x)
    
    # 前馈子层
    x = x + ffn(x)
    x = layer_norm(x)
    
    return x
```

**特点**：
- 归一化在残差连接之后
- 需要学习率预热（warmup）
- 深层网络训练不稳定

#### 3.2 Pre-LN（现代模型标准）

```python
# Pre-LN架构
def transformer_block_pre_ln(x):
    # 注意力子层
    x = x + attention(layer_norm(x))
    
    # 前馈子层
    x = x + ffn(layer_norm(x))
    
    return x
```


**完整的Pre-LN Transformer Block**：
```python
class TransformerBlockPreLN(nn.Module):
    def __init__(self, d_model, num_heads, d_ff, dropout=0.1):
        super().__init__()
        
        self.attention = MultiHeadAttention(d_model, num_heads)
        self.ffn = FeedForward(d_model, d_ff, dropout)
        
        # Pre-LN: 在子层之前归一化
        self.ln1 = nn.LayerNorm(d_model)
        self.ln2 = nn.LayerNorm(d_model)
        
        self.dropout = nn.Dropout(dropout)
    
    def forward(self, x, mask=None):
        # 注意力子层（Pre-LN）
        normalized = self.ln1(x)
        attn_output = self.attention(normalized, mask)
        x = x + self.dropout(attn_output)
        
        # 前馈子层（Pre-LN）
        normalized = self.ln2(x)
        ffn_output = self.ffn(normalized)
        x = x + self.dropout(ffn_output)
        
        return x
```

**Pre-LN vs Post-LN对比**：

| 特性 | Post-LN | Pre-LN |
| :--- | :--- | :--- |
| **训练稳定性** | 较差，深层网络难训练 | 好，易于训练深层网络 |
| **学习率预热** | 必需 | 可选 |
| **梯度流** | 可能不稳定 | 更稳定 |
| **最终性能** | 略好（如果能训练好） | 略逊但更稳定 |
| **使用模型** | 原始Transformer | GPT-2, GPT-3, Llama |

**现代选择**：几乎所有现代LLM都使用Pre-LN，因为训练稳定性更重要。

### 4. 最终的LayerNorm

在模型最后通常还有一个LayerNorm：

```python
class GPTModel(nn.Module):
    def __init__(self, config):
        super().__init__()
        # ... 其他组件 ...
        
        self.blocks = nn.ModuleList([
            TransformerBlockPreLN(...) for _ in range(num_layers)
        ])
        
        # 最终的LayerNorm
        self.ln_f = nn.LayerNorm(d_model)
        
        self.lm_head = nn.Linear(d_model, vocab_size, bias=False)
    
    def forward(self, x):
        # ... embedding ...
        
        for block in self.blocks:
            x = block(x)
        
        # 最终归一化
        x = self.ln_f(x)
        
        logits = self.lm_head(x)
        return logits
```

---

## 第四部分：激活函数

激活函数为模型引入非线性，是深度学习的核心。

### 1. ReLU (Rectified Linear Unit)

**公式**：
```
ReLU(x) = max(0, x)
```

**实现**：
```python
import torch.nn.functional as F

def relu(x):
    return F.relu(x)
```

**特点**：
- 简单、快速
- 可能导致"神经元死亡"（负值梯度为0）
- 早期Transformer使用

### 2. GELU (Gaussian Error Linear Unit)

**公式**：
```
GELU(x) = x · Φ(x)
```
其中Φ(x)是标准正态分布的累积分布函数。

**近似实现**：
```python
def gelu(x):
    return 0.5 * x * (1.0 + torch.tanh(
        math.sqrt(2.0 / math.pi) * (x + 0.044715 * torch.pow(x, 3))
    ))

# PyTorch内置
def gelu_pytorch(x):
    return F.gelu(x)
```

**特点**：
- 平滑的非线性
- 在负值区域有小的梯度（避免神经元死亡）
- 性能通常优于ReLU

**使用模型**：BERT, GPT-2, GPT-3


### 3. SwiGLU (Swish-Gated Linear Unit)

**核心思想**：使用门控机制，结合Swish激活函数。

**公式**：
```
SwiGLU(x, W, V, b, c) = Swish(xW + b) ⊗ (xV + c)
Swish(x) = x · σ(x) = x · sigmoid(x)
```

**实现**：
```python
class SwiGLU(nn.Module):
    def forward(self, x):
        # x已经通过线性层，维度是2倍的d_ff
        x, gate = x.chunk(2, dim=-1)
        return F.silu(gate) * x  # silu = swish

class FeedForwardSwiGLU(nn.Module):
    def __init__(self, d_model, d_ff, dropout=0.1):
        super().__init__()
        # 注意：输出维度是2*d_ff，因为会split
        self.w1 = nn.Linear(d_model, 2 * d_ff, bias=False)
        self.w2 = nn.Linear(d_ff, d_model, bias=False)
        self.dropout = nn.Dropout(dropout)
    
    def forward(self, x):
        # x: [batch, seq_len, d_model]
        x = self.w1(x)  # [batch, seq_len, 2*d_ff]
        
        # Split并应用SwiGLU
        x, gate = x.chunk(2, dim=-1)
        x = F.silu(gate) * x  # [batch, seq_len, d_ff]
        
        x = self.dropout(x)
        x = self.w2(x)  # [batch, seq_len, d_model]
        x = self.dropout(x)
        
        return x
```

**变体：GLU家族**

| 激活函数 | 门控函数 | 公式 |
| :--- | :--- | :--- |
| **GLU** | Sigmoid | σ(xW) ⊗ (xV) |
| **ReGLU** | ReLU | ReLU(xW) ⊗ (xV) |
| **GEGLU** | GELU | GELU(xW) ⊗ (xV) |
| **SwiGLU** | Swish | Swish(xW) ⊗ (xV) |

**优点**：
- 性能优于GELU和ReLU
- 门控机制提供更灵活的非线性
- 实践中收敛更快

**缺点**：
- 参数量增加（需要2倍的d_ff）
- 计算量略高

**使用模型**：Llama, Llama 2, PaLM

### 4. 激活函数对比

| 激活函数 | 计算复杂度 | 性能 | 参数量 | 代表模型 |
| :--- | :--- | :--- | :--- | :--- |
| **ReLU** | 低 | 基线 | 标准 | 早期模型 |
| **GELU** | 中 | 好 | 标准 | GPT-2/3, BERT |
| **SwiGLU** | 高 | 最好 | 2倍 | Llama, PaLM |

**趋势**：现代大模型倾向于使用SwiGLU，尽管参数量更大，但性能提升值得。

---

## 第五部分：注意力机制的优化

### 1. Multi-Query Attention (MQA)

**核心思想**：所有注意力头共享同一组Key和Value。

**标准Multi-Head Attention**：
- 每个头有独立的Q, K, V投影
- 参数：3 × d_model × d_model

**Multi-Query Attention**：
- 每个头有独立的Q投影
- 所有头共享一组K, V投影
- 参数：d_model × d_model (Q) + 2 × d_model × d_k (共享的K, V)

**实现**：
```python
class MultiQueryAttention(nn.Module):
    def __init__(self, d_model, num_heads):
        super().__init__()
        self.num_heads = num_heads
        self.d_k = d_model // num_heads
        
        # 每个头独立的Query投影
        self.W_q = nn.Linear(d_model, d_model)
        
        # 共享的Key和Value投影（只有一组）
        self.W_k = nn.Linear(d_model, self.d_k)
        self.W_v = nn.Linear(d_model, self.d_k)
        
        self.W_o = nn.Linear(d_model, d_model)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, d_model = x.size()
        
        # Query: 每个头独立
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        # Q: [batch, num_heads, seq_len, d_k]
        
        # Key和Value: 所有头共享
        K = self.W_k(x).unsqueeze(1)  # [batch, 1, seq_len, d_k]
        V = self.W_v(x).unsqueeze(1)  # [batch, 1, seq_len, d_k]
        
        # 广播K和V到所有头
        K = K.expand(batch_size, self.num_heads, seq_len, self.d_k)
        V = V.expand(batch_size, self.num_heads, seq_len, self.d_k)
        
        # 标准注意力计算
        scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(self.d_k)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        output = torch.matmul(attention_weights, V)
        
        # 合并头
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, d_model)
        output = self.W_o(output)
        
        return output
```

**优点**：
- 大幅减少KV cache（推理时的显存占用）
- 推理速度更快
- 参数量减少

**缺点**：
- 表达能力略有下降
- 训练时性能可能略逊

**使用模型**：PaLM, Falcon


### 2. Grouped-Query Attention (GQA)

**核心思想**：MQA和MHA的折中方案，将头分组，每组共享K和V。

**实现**：
```python
class GroupedQueryAttention(nn.Module):
    def __init__(self, d_model, num_heads, num_kv_heads):
        super().__init__()
        assert num_heads % num_kv_heads == 0, "num_heads必须能被num_kv_heads整除"
        
        self.num_heads = num_heads
        self.num_kv_heads = num_kv_heads
        self.num_queries_per_kv = num_heads // num_kv_heads
        self.d_k = d_model // num_heads
        
        # Query: 所有头
        self.W_q = nn.Linear(d_model, d_model)
        
        # Key和Value: 只有num_kv_heads组
        self.W_k = nn.Linear(d_model, num_kv_heads * self.d_k)
        self.W_v = nn.Linear(d_model, num_kv_heads * self.d_k)
        
        self.W_o = nn.Linear(d_model, d_model)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, d_model = x.size()
        
        # Query: [batch, num_heads, seq_len, d_k]
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        
        # Key和Value: [batch, num_kv_heads, seq_len, d_k]
        K = self.W_k(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        V = self.W_v(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        
        # 将K和V复制到对应的Query头
        # 每个KV组对应num_queries_per_kv个Query头
        K = K.repeat_interleave(self.num_queries_per_kv, dim=1)
        V = V.repeat_interleave(self.num_queries_per_kv, dim=1)
        # 现在K和V: [batch, num_heads, seq_len, d_k]
        
        # 标准注意力计算
        scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(self.d_k)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        output = torch.matmul(attention_weights, V)
        
        # 合并头
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, d_model)
        output = self.W_o(output)
        
        return output
```

**配置示例**：
```python
# Llama 2 70B的配置
# num_heads = 64
# num_kv_heads = 8
# 每8个Query头共享一组KV

gqa = GroupedQueryAttention(
    d_model=8192,
    num_heads=64,
    num_kv_heads=8  # 8组KV
)
```

**三种注意力机制对比**：

| 特性 | MHA | GQA | MQA |
| :--- | :--- | :--- | :--- |
| **KV头数** | num_heads | num_kv_heads | 1 |
| **KV参数量** | 2 × d_model² | 2 × d_model × num_kv_heads × d_k | 2 × d_model × d_k |
| **KV cache大小** | 最大 | 中等 | 最小 |
| **表达能力** | 最强 | 中等 | 较弱 |
| **推理速度** | 慢 | 中等 | 快 |
| **使用模型** | GPT-2/3, BERT | Llama 2 | PaLM, Falcon |

**趋势**：GQA是当前的最佳平衡点，Llama 2等最新模型都采用了这种方案。

### 3. Flash Attention

**核心思想**：通过重新组织注意力计算的顺序，减少HBM（高带宽内存）访问，加速计算。

**标准注意力的问题**：
```python
# 标准实现
S = Q @ K.T  # 需要存储整个注意力矩阵 [seq_len, seq_len]
P = softmax(S)
O = P @ V
```
对于长序列，注意力矩阵非常大，导致显存瓶颈。

**Flash Attention的优化**：
- 分块计算（tiling）
- 在SRAM中完成更多计算
- 减少HBM读写次数

**使用（需要安装flash-attn库）**：
```python
# 安装: pip install flash-attn
from flash_attn import flash_attn_func

def flash_attention(q, k, v, causal=True):
    """
    q, k, v: [batch, num_heads, seq_len, head_dim]
    """
    output = flash_attn_func(q, k, v, causal=causal)
    return output
```

**性能提升**：
- 训练速度：2-4倍加速
- 显存占用：减少到O(N)而不是O(N²)
- 支持更长的序列

**使用模型**：几乎所有现代大模型训练都使用Flash Attention


---

## 第六部分：并行化Transformer Block

### 1. 标准串行架构

```python
class StandardTransformerBlock(nn.Module):
    def forward(self, x):
        # 串行执行
        x = x + attention(ln(x))
        x = x + ffn(ln(x))
        return x
```

### 2. 并行架构（Parallel Transformer）

**核心思想**：并行计算注意力和FFN，而不是串行。

**实现**：
```python
class ParallelTransformerBlock(nn.Module):
    def __init__(self, d_model, num_heads, d_ff, dropout=0.1):
        super().__init__()
        
        self.attention = MultiHeadAttention(d_model, num_heads)
        self.ffn = FeedForward(d_model, d_ff, dropout)
        
        # 只需要一个LayerNorm
        self.ln = nn.LayerNorm(d_model)
        
        self.dropout = nn.Dropout(dropout)
    
    def forward(self, x, mask=None):
        # 归一化一次
        normalized = self.ln(x)
        
        # 并行计算attention和ffn
        attn_output = self.attention(normalized, mask)
        ffn_output = self.ffn(normalized)
        
        # 合并输出
        x = x + self.dropout(attn_output) + self.dropout(ffn_output)
        
        return x
```

**优点**：
- 训练速度提升15-20%
- 减少一个LayerNorm层
- 更好的并行性

**缺点**：
- 与标准架构不完全兼容
- 可能需要调整超参数

**使用模型**：PaLM, GPT-J

### 3. 架构对比

```
标准串行架构:
x → LN → Attn → Add → LN → FFN → Add

并行架构:
        ┌→ Attn →┐
x → LN →┤        ├→ Add
        └→ FFN  →┘
```

---

## 第七部分：现代LLM架构总览

### 1. GPT-2 架构

```python
class GPT2Config:
    vocab_size = 50257
    n_positions = 1024  # 最大序列长度
    n_embd = 768        # d_model (small), 1024 (medium), 1280 (large)
    n_layer = 12        # 层数 (small), 24 (medium), 36 (large)
    n_head = 12         # 注意力头数
    
    # 架构选择
    position_encoding = "learned"
    normalization = "LayerNorm"
    norm_position = "pre"
    activation = "GELU"
    attention_type = "multi-head"
```

**关键特点**：
- Learned position embeddings
- Pre-LN
- GELU激活
- 标准Multi-Head Attention

### 2. GPT-3 架构

```python
class GPT3Config:
    vocab_size = 50257
    n_positions = 2048
    n_embd = 12288      # 175B模型
    n_layer = 96
    n_head = 96
    
    # 与GPT-2基本相同，主要是规模扩大
    position_encoding = "learned"
    normalization = "LayerNorm"
    norm_position = "pre"
    activation = "GELU"
    attention_type = "multi-head"
```

**关键特点**：
- 架构与GPT-2相同
- 主要是规模扩大（175B参数）
- 更长的上下文（2048 tokens）


### 3. Llama / Llama 2 架构

```python
class LlamaConfig:
    vocab_size = 32000
    max_position_embeddings = 2048  # Llama 1
    # max_position_embeddings = 4096  # Llama 2
    hidden_size = 4096      # 7B模型
    num_hidden_layers = 32
    num_attention_heads = 32
    
    # Llama 2 70B使用GQA
    num_key_value_heads = 8  # GQA (仅Llama 2 70B)
    
    # 架构选择
    position_encoding = "RoPE"
    normalization = "RMSNorm"
    norm_position = "pre"
    activation = "SwiGLU"
    attention_type = "grouped-query"  # Llama 2 70B
    
    # FFN维度
    intermediate_size = 11008  # SwiGLU需要更大的中间维度
```

**关键特点**：
- RoPE位置编码（更好的外推性能）
- RMSNorm（更快的归一化）
- SwiGLU激活（更好的性能）
- GQA（Llama 2 70B，更快的推理）

**完整实现**：
```python
class LlamaTransformerBlock(nn.Module):
    def __init__(self, config):
        super().__init__()
        
        # RMSNorm
        self.input_layernorm = RMSNorm(config.hidden_size)
        self.post_attention_layernorm = RMSNorm(config.hidden_size)
        
        # Attention with RoPE
        self.self_attn = GroupedQueryAttention(
            d_model=config.hidden_size,
            num_heads=config.num_attention_heads,
            num_kv_heads=config.num_key_value_heads
        )
        
        # SwiGLU FFN
        self.mlp = FeedForwardSwiGLU(
            d_model=config.hidden_size,
            d_ff=config.intermediate_size
        )
    
    def forward(self, x, mask=None):
        # Pre-RMSNorm + Attention
        residual = x
        x = self.input_layernorm(x)
        x = self.self_attn(x, mask)
        x = residual + x
        
        # Pre-RMSNorm + FFN
        residual = x
        x = self.post_attention_layernorm(x)
        x = self.mlp(x)
        x = residual + x
        
        return x
```

### 4. PaLM 架构

```python
class PaLMConfig:
    vocab_size = 256000  # 更大的词汇表
    max_position_embeddings = 2048
    hidden_size = 18432  # 540B模型
    num_hidden_layers = 118
    num_attention_heads = 48
    
    # 架构选择
    position_encoding = "RoPE"
    normalization = "LayerNorm"  # 不是RMSNorm
    norm_position = "pre"
    activation = "SwiGLU"
    attention_type = "multi-query"  # MQA
    
    # 并行架构
    parallel_blocks = True
```

**关键特点**：
- 并行Transformer Block
- Multi-Query Attention
- SwiGLU激活
- RoPE位置编码

### 5. 主流模型架构对比

| 特性 | GPT-2 | GPT-3 | Llama | Llama 2 | PaLM |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **位置编码** | Learned | Learned | RoPE | RoPE | RoPE |
| **归一化** | LayerNorm | LayerNorm | RMSNorm | RMSNorm | LayerNorm |
| **归一化位置** | Pre | Pre | Pre | Pre | Pre |
| **激活函数** | GELU | GELU | SwiGLU | SwiGLU | SwiGLU |
| **注意力类型** | MHA | MHA | MHA | GQA (70B) | MQA |
| **并行Block** | ✗ | ✗ | ✗ | ✗ | ✓ |
| **词汇表大小** | 50K | 50K | 32K | 32K | 256K |
| **上下文长度** | 1K | 2K | 2K | 4K | 2K |

**演进趋势**：
1. **位置编码**：Learned → RoPE（更好的外推）
2. **归一化**：LayerNorm → RMSNorm（更快）
3. **激活函数**：GELU → SwiGLU（更好的性能）
4. **注意力**：MHA → GQA/MQA（更快的推理）
5. **架构**：串行 → 并行（更快的训练）

---

## 第八部分：架构设计的权衡

### 1. 模型深度 vs 宽度

**深度（层数）**：
- 优点：更强的表达能力，更好的组合性
- 缺点：训练更难，梯度问题

**宽度（d_model）**：
- 优点：更多的特征表示能力
- 缺点：参数量和计算量增加

**经验法则**：
- 小模型：更宽（如GPT-2 Small: 12层 × 768维）
- 大模型：更深（如GPT-3: 96层 × 12288维）

### 2. 注意力头数的选择

**常见配置**：
```python
# 保持每个头的维度为64或128
d_k = d_model // num_heads

# 常见选择
# d_model=768, num_heads=12, d_k=64
# d_model=1024, num_heads=16, d_k=64
# d_model=2048, num_heads=32, d_k=64
```

**原则**：
- d_k通常在64-128之间
- 太小：每个头的表达能力不足
- 太大：头数太少，多样性不足


### 3. FFN维度的选择

**标准配置**：
```python
# 传统：d_ff = 4 × d_model
d_ff = 4 * d_model

# SwiGLU：需要更大的维度（因为会split）
# 实际有效维度是 d_ff / 2
d_ff = 8 * d_model / 3  # 约2.67倍
```

**Llama的选择**：
```python
# Llama 7B
d_model = 4096
d_ff = 11008  # 约2.69倍

# 计算方式：
# 先计算 4 * d_model = 16384
# 然后调整到最接近的8的倍数：11008
```

### 4. 词汇表大小的权衡

**小词汇表（如32K）**：
- 优点：嵌入矩阵小，训练快
- 缺点：序列更长，某些语言表示效率低

**大词汇表（如256K）**：
- 优点：序列更短，多语言支持好
- 缺点：嵌入矩阵大，训练慢

**常见选择**：
- 英语为主：32K-50K
- 多语言：100K-256K

### 5. 上下文长度的选择

**短上下文（1K-2K）**：
- 训练快，显存占用少
- 适合大多数任务

**长上下文（4K-32K）**：
- 可以处理更长的文档
- 训练和推理成本高

**技术挑战**：
- 注意力复杂度：O(N²)
- 位置编码的外推性能
- KV cache的显存占用

**解决方案**：
- Flash Attention（减少显存）
- RoPE/ALiBi（更好的外推）
- GQA/MQA（减少KV cache）

---

## 第九部分：实现一个现代LLM架构

### 完整的Llama风格模型

```python
import torch
import torch.nn as nn
import torch.nn.functional as F
import math

class RMSNorm(nn.Module):
    def __init__(self, dim, eps=1e-6):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(dim))
        self.eps = eps
    
    def forward(self, x):
        rms = torch.sqrt(torch.mean(x ** 2, dim=-1, keepdim=True) + self.eps)
        return self.weight * x / rms

class RotaryPositionEmbedding(nn.Module):
    def __init__(self, dim, max_seq_len=2048, base=10000):
        super().__init__()
        inv_freq = 1.0 / (base ** (torch.arange(0, dim, 2).float() / dim))
        self.register_buffer('inv_freq', inv_freq)
        
        t = torch.arange(max_seq_len, dtype=torch.float)
        freqs = torch.einsum('i,j->ij', t, inv_freq)
        emb = torch.cat((freqs, freqs), dim=-1)
        
        self.register_buffer('cos_cached', emb.cos()[None, None, :, :])
        self.register_buffer('sin_cached', emb.sin()[None, None, :, :])
    
    def rotate_half(self, x):
        x1, x2 = x[..., :x.shape[-1]//2], x[..., x.shape[-1]//2:]
        return torch.cat((-x2, x1), dim=-1)
    
    def forward(self, q, k):
        seq_len = q.shape[2]
        cos = self.cos_cached[:, :, :seq_len, :]
        sin = self.sin_cached[:, :, :seq_len, :]
        
        q_embed = (q * cos) + (self.rotate_half(q) * sin)
        k_embed = (k * cos) + (self.rotate_half(k) * sin)
        
        return q_embed, k_embed

class GroupedQueryAttention(nn.Module):
    def __init__(self, d_model, num_heads, num_kv_heads):
        super().__init__()
        assert num_heads % num_kv_heads == 0
        
        self.num_heads = num_heads
        self.num_kv_heads = num_kv_heads
        self.num_queries_per_kv = num_heads // num_kv_heads
        self.d_k = d_model // num_heads
        
        self.W_q = nn.Linear(d_model, d_model, bias=False)
        self.W_k = nn.Linear(d_model, num_kv_heads * self.d_k, bias=False)
        self.W_v = nn.Linear(d_model, num_kv_heads * self.d_k, bias=False)
        self.W_o = nn.Linear(d_model, d_model, bias=False)
        
        self.rope = RotaryPositionEmbedding(self.d_k)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, d_model = x.size()
        
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        K = self.W_k(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        V = self.W_v(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        
        # 应用RoPE
        Q, K = self.rope(Q, K)
        
        # 扩展K和V
        K = K.repeat_interleave(self.num_queries_per_kv, dim=1)
        V = V.repeat_interleave(self.num_queries_per_kv, dim=1)
        
        # 注意力计算
        scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(self.d_k)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        output = torch.matmul(attention_weights, V)
        
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, d_model)
        output = self.W_o(output)
        
        return output

class SwiGLUFeedForward(nn.Module):
    def __init__(self, d_model, d_ff):
        super().__init__()
        self.w1 = nn.Linear(d_model, d_ff, bias=False)
        self.w2 = nn.Linear(d_ff, d_model, bias=False)
        self.w3 = nn.Linear(d_model, d_ff, bias=False)
    
    def forward(self, x):
        # SwiGLU: swish(x @ w1) * (x @ w3) @ w2
        return self.w2(F.silu(self.w1(x)) * self.w3(x))

class LlamaTransformerBlock(nn.Module):
    def __init__(self, d_model, num_heads, num_kv_heads, d_ff):
        super().__init__()
        
        self.attention_norm = RMSNorm(d_model)
        self.attention = GroupedQueryAttention(d_model, num_heads, num_kv_heads)
        
        self.ffn_norm = RMSNorm(d_model)
        self.ffn = SwiGLUFeedForward(d_model, d_ff)
    
    def forward(self, x, mask=None):
        # Attention with residual
        x = x + self.attention(self.attention_norm(x), mask)
        
        # FFN with residual
        x = x + self.ffn(self.ffn_norm(x))
        
        return x

class LlamaModel(nn.Module):
    def __init__(
        self,
        vocab_size,
        d_model,
        num_layers,
        num_heads,
        num_kv_heads,
        d_ff,
        max_seq_len
    ):
        super().__init__()
        
        # Token嵌入
        self.token_embedding = nn.Embedding(vocab_size, d_model)
        
        # Transformer blocks
        self.blocks = nn.ModuleList([
            LlamaTransformerBlock(d_model, num_heads, num_kv_heads, d_ff)
            for _ in range(num_layers)
        ])
        
        # 最终归一化
        self.norm = RMSNorm(d_model)
        
        # 输出层（权重共享）
        self.lm_head = nn.Linear(d_model, vocab_size, bias=False)
        self.lm_head.weight = self.token_embedding.weight
        
        self.max_seq_len = max_seq_len
    
    def forward(self, input_ids):
        batch_size, seq_len = input_ids.size()
        
        # Token嵌入
        x = self.token_embedding(input_ids)
        
        # 创建因果掩码
        mask = torch.tril(torch.ones(seq_len, seq_len, device=input_ids.device))
        mask = mask.unsqueeze(0).unsqueeze(0)
        
        # 通过所有Transformer blocks
        for block in self.blocks:
            x = block(x, mask)
        
        # 最终归一化
        x = self.norm(x)
        
        # 输出logits
        logits = self.lm_head(x)
        
        return logits

# 创建Llama 7B风格的模型
model = LlamaModel(
    vocab_size=32000,
    d_model=4096,
    num_layers=32,
    num_heads=32,
    num_kv_heads=32,  # Llama 1使用MHA，Llama 2 70B使用8
    d_ff=11008,
    max_seq_len=2048
)

print(f"模型参数量: {sum(p.numel() for p in model.parameters()) / 1e9:.2f}B")
```


---

## 第十部分：架构选择的实验结果

### 1. 位置编码的影响

**实验设置**：在相同数据和模型规模下比较不同位置编码

**结果**：
| 位置编码 | 训练困惑度 | 外推性能 | 训练速度 |
| :--- | :--- | :--- | :--- |
| Learned | 基线 | 差 | 快 |
| Sinusoidal | +2% | 中 | 快 |
| ALiBi | +1% | 好 | 快 |
| RoPE | 基线 | 最好 | 中 |

**结论**：RoPE是当前最佳选择，特别是需要长序列外推时。

### 2. 归一化方法的影响

**实验结果**：
| 归一化 | 训练速度 | 最终性能 | 稳定性 |
| :--- | :--- | :--- | :--- |
| Post-LN | 慢 | 好（如果收敛） | 差 |
| Pre-LN | 快 | 好 | 好 |
| RMSNorm | 最快 | 好 | 好 |

**结论**：Pre-LN + RMSNorm是现代标准。

### 3. 激活函数的影响

**在相同FLOPs下的性能**：
| 激活函数 | 相对性能 | 参数效率 |
| :--- | :--- | :--- |
| ReLU | 基线 | 基线 |
| GELU | +3% | 相同 |
| SwiGLU | +5% | 需要更多参数 |

**结论**：SwiGLU性能最好，但需要权衡参数量。

### 4. 注意力机制的影响

**推理速度对比（相同模型大小）**：
| 注意力类型 | KV Cache | 推理速度 | 性能 |
| :--- | :--- | :--- | :--- |
| MHA | 100% | 基线 | 100% |
| GQA (8组) | 25% | 1.5x | 98% |
| MQA | 12.5% | 2x | 95% |

**结论**：GQA是最佳平衡点。

---

## 第十一部分：架构设计的最佳实践

### 1. 从零开始设计LLM架构的检查清单

**基础组件**：
- ✓ Token Embedding + 权重共享
- ✓ 位置编码：RoPE（推荐）或ALiBi
- ✓ 归一化：Pre-LN + RMSNorm
- ✓ 激活函数：SwiGLU（如果计算资源充足）或GELU
- ✓ 注意力：GQA（推荐）或MHA
- ✓ 残差连接
- ✓ Dropout（小模型需要，大模型可选）

**超参数选择**：
```python
# 小模型（~1B参数）
config_small = {
    'd_model': 2048,
    'num_layers': 24,
    'num_heads': 16,
    'num_kv_heads': 16,  # 或8用于GQA
    'd_ff': 5504,        # 约2.7x d_model
    'vocab_size': 32000,
    'max_seq_len': 2048,
}

# 中等模型（~7B参数）
config_medium = {
    'd_model': 4096,
    'num_layers': 32,
    'num_heads': 32,
    'num_kv_heads': 8,   # GQA
    'd_ff': 11008,
    'vocab_size': 32000,
    'max_seq_len': 4096,
}

# 大模型（~70B参数）
config_large = {
    'd_model': 8192,
    'num_layers': 80,
    'num_heads': 64,
    'num_kv_heads': 8,   # GQA
    'd_ff': 22016,
    'vocab_size': 32000,
    'max_seq_len': 4096,
}
```

### 2. 常见错误和陷阱

**错误1：使用Post-LN**
```python
# ❌ 错误：Post-LN难以训练
x = layer_norm(x + attention(x))

# ✓ 正确：Pre-LN更稳定
x = x + attention(layer_norm(x))
```

**错误2：忘记权重共享**
```python
# ❌ 错误：浪费参数
self.token_embedding = nn.Embedding(vocab_size, d_model)
self.lm_head = nn.Linear(d_model, vocab_size)

# ✓ 正确：共享权重
self.token_embedding = nn.Embedding(vocab_size, d_model)
self.lm_head = nn.Linear(d_model, vocab_size, bias=False)
self.lm_head.weight = self.token_embedding.weight
```

**错误3：不正确的掩码**
```python
# ❌ 错误：没有因果掩码
attention_scores = Q @ K.T

# ✓ 正确：使用因果掩码
mask = torch.tril(torch.ones(seq_len, seq_len))
attention_scores = attention_scores.masked_fill(mask == 0, float('-inf'))
```

**错误4：维度不匹配**
```python
# ❌ 错误：SwiGLU的FFN维度
d_ff = 4 * d_model  # 太大了

# ✓ 正确：SwiGLU需要调整
d_ff = int(8 * d_model / 3)  # 约2.67x
```

### 3. 性能优化建议

**训练优化**：
1. 使用Flash Attention
2. 使用混合精度训练（FP16/BF16）
3. 使用梯度检查点（长序列）
4. 使用编译优化（torch.compile）

**推理优化**：
1. 使用GQA或MQA减少KV cache
2. 使用KV cache复用
3. 使用量化（INT8/INT4）
4. 使用批处理

**代码示例**：
```python
# 使用torch.compile加速
model = torch.compile(model)

# 使用混合精度
from torch.cuda.amp import autocast

with autocast():
    logits = model(input_ids)

# 使用梯度检查点
from torch.utils.checkpoint import checkpoint

def forward_with_checkpoint(self, x):
    for block in self.blocks:
        x = checkpoint(block, x, use_reentrant=False)
    return x
```

---

## 总结

### 核心要点

1. **位置编码的演进**：
   - Learned → RoPE/ALiBi
   - 相对位置编码提供更好的外推性能

2. **归一化的选择**：
   - Pre-LN是现代标准
   - RMSNorm更快且性能相当

3. **激活函数的趋势**：
   - GELU → SwiGLU
   - 门控机制提供更好的性能

4. **注意力机制的优化**：
   - MHA → GQA → MQA
   - 权衡性能和推理效率

5. **架构设计原则**：
   - 简单性：避免过度复杂
   - 可扩展性：设计要支持大规模
   - 效率：考虑训练和推理成本

### 现代LLM架构的标准配置

```python
# 推荐的现代LLM架构
class ModernLLMConfig:
    # 位置编码
    position_encoding = "RoPE"  # 或ALiBi
    
    # 归一化
    normalization = "RMSNorm"
    norm_position = "pre"
    
    # 激活函数
    activation = "SwiGLU"  # 或GELU（资源受限时）
    
    # 注意力
    attention_type = "GQA"  # num_kv_heads = num_heads // 4 或 // 8
    
    # 其他
    bias = False  # 大多数线性层不使用bias
    tie_word_embeddings = True  # 权重共享
```

### 下一步学习

**第四讲预告**：
- 训练目标和损失函数
- 优化器选择
- 学习率调度策略
- 训练稳定性技巧

**推荐阅读**：
- [Attention is All You Need](https://arxiv.org/abs/1706.03762) - Transformer原始论文
- [RoFormer: Enhanced Transformer with Rotary Position Embedding](https://arxiv.org/abs/2104.09864) - RoPE论文
- [Root Mean Square Layer Normalization](https://arxiv.org/abs/1910.07467) - RMSNorm论文
- [GLU Variants Improve Transformer](https://arxiv.org/abs/2002.05202) - SwiGLU论文
- [GQA: Training Generalized Multi-Query Transformer](https://arxiv.org/abs/2305.13245) - GQA论文
- [Llama 2: Open Foundation and Fine-Tuned Chat Models](https://arxiv.org/abs/2307.09288) - Llama 2论文

---

## 附录：快速参考

### A. 各组件的PyTorch实现速查

**RMSNorm**：
```python
class RMSNorm(nn.Module):
    def __init__(self, dim, eps=1e-6):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(dim))
        self.eps = eps
    
    def forward(self, x):
        return self.weight * x / torch.sqrt(x.pow(2).mean(-1, keepdim=True) + self.eps)
```

**RoPE**：
```python
def apply_rotary_emb(q, k, cos, sin):
    q_embed = (q * cos) + (rotate_half(q) * sin)
    k_embed = (k * cos) + (rotate_half(k) * sin)
    return q_embed, k_embed
```

**SwiGLU**：
```python
def swiglu(x):
    x, gate = x.chunk(2, dim=-1)
    return F.silu(gate) * x
```

**GQA**：
```python
# 扩展KV到所有Query头
K = K.repeat_interleave(num_queries_per_kv, dim=1)
V = V.repeat_interleave(num_queries_per_kv, dim=1)
```

### B. 模型配置速查表

| 模型 | 参数量 | 层数 | d_model | 头数 | KV头数 | FFN维度 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| GPT-2 Small | 124M | 12 | 768 | 12 | 12 | 3072 |
| GPT-2 Large | 774M | 36 | 1280 | 20 | 20 | 5120 |
| GPT-3 | 175B | 96 | 12288 | 96 | 96 | 49152 |
| Llama 7B | 7B | 32 | 4096 | 32 | 32 | 11008 |
| Llama 2 70B | 70B | 80 | 8192 | 64 | 8 | 28672 |
| PaLM 540B | 540B | 118 | 18432 | 48 | 1 | 49152 |

希望这份详细的架构笔记能帮助你深入理解现代LLM的设计选择！
