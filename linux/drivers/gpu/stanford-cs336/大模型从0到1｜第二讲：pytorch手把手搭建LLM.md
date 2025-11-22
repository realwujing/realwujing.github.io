# 大模型从0到1｜第二讲：PyTorch手把手搭建LLM

> 课程链接：[Stanford CS336 Spring 2025 - Lecture 2](https://stanford-cs336.github.io/spring2025-lectures/?trace=var/traces/lecture_02.json)

---

## 第一部分：语言模型基础

### 1. 什么是语言模型？

**定义**：语言模型是一个概率分布，用于预测文本序列的可能性。

**数学表示**：
```
P(w₁, w₂, ..., wₙ) = P(w₁) × P(w₂|w₁) × P(w₃|w₁,w₂) × ... × P(wₙ|w₁,...,wₙ₋₁)
```

**核心任务**：给定前面的词（上下文），预测下一个词的概率分布。

**应用场景**：
- 文本生成（如ChatGPT）
- 机器翻译
- 文本补全
- 问答系统
- 代码生成

### 2. 自回归语言模型（Autoregressive Language Model）

**核心思想**：从左到右，逐个生成Token，每次生成都依赖于之前生成的所有Token。

**生成过程**：
```
输入: "The cat"
步骤1: P(sat | The cat) → 生成 "sat"
步骤2: P(on | The cat sat) → 生成 "on"
步骤3: P(the | The cat sat on) → 生成 "the"
步骤4: P(mat | The cat sat on the) → 生成 "mat"
```


**优点**：
- 生成质量高，符合语言的自然流畅性
- 训练简单，只需要大量文本数据
- 可以生成任意长度的文本

**代表模型**：GPT系列、Llama、PaLM

---

## 第二部分：Transformer架构详解

### 1. Transformer的整体结构

Transformer是现代大语言模型的核心架构，由Google在2017年的论文"Attention is All You Need"中提出。

**核心组件**：
1. **Token Embedding**：将Token ID转换为向量
2. **Position Embedding**：为每个位置添加位置信息
3. **Transformer Blocks**：多层堆叠的注意力和前馈网络
4. **Layer Normalization**：归一化层
5. **Output Layer**：预测下一个Token的概率分布

**架构图**：
```
输入Token IDs
    ↓
Token Embedding + Position Embedding
    ↓
Transformer Block 1
    ↓
Transformer Block 2
    ↓
    ...
    ↓
Transformer Block N
    ↓
Layer Norm
    ↓
输出层（预测下一个Token）
```

### 2. Token Embedding（词嵌入）

**作用**：将离散的Token ID转换为连续的向量表示。

**实现**：
```python
import torch
import torch.nn as nn

class TokenEmbedding(nn.Module):
    def __init__(self, vocab_size, d_model):
        super().__init__()
        self.embedding = nn.Embedding(vocab_size, d_model)
        self.d_model = d_model
    
    def forward(self, x):
        # x: [batch_size, seq_len]
        # 输出: [batch_size, seq_len, d_model]
        return self.embedding(x) * (self.d_model ** 0.5)
```


**参数说明**：
- `vocab_size`：词汇表大小（如32000）
- `d_model`：嵌入维度（如512、768、1024等）
- 乘以 `sqrt(d_model)` 是为了缩放，使得嵌入和位置编码的量级相当

**示例**：
```python
vocab_size = 32000
d_model = 512
embedding = TokenEmbedding(vocab_size, d_model)

# 输入: [batch_size=2, seq_len=10]
input_ids = torch.randint(0, vocab_size, (2, 10))
output = embedding(input_ids)
print(output.shape)  # torch.Size([2, 10, 512])
```

### 3. Position Embedding（位置编码）

**为什么需要位置编码？**
- Transformer的自注意力机制本身是位置无关的
- 需要显式地告诉模型每个Token的位置信息
- "I love you" 和 "you love I" 应该有不同的表示

**两种主流方法**：

#### 3.1 绝对位置编码（Absolute Position Encoding）

**正弦位置编码（Sinusoidal）**：
```python
import math

class SinusoidalPositionEncoding(nn.Module):
    def __init__(self, d_model, max_len=5000):
        super().__init__()
        # 创建位置编码矩阵
        pe = torch.zeros(max_len, d_model)
        position = torch.arange(0, max_len).unsqueeze(1).float()
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * 
                            -(math.log(10000.0) / d_model))
        
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        
        self.register_buffer('pe', pe.unsqueeze(0))
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        return x + self.pe[:, :x.size(1)]
```


**可学习位置编码（Learned Position Encoding）**：
```python
class LearnedPositionEncoding(nn.Module):
    def __init__(self, d_model, max_len=5000):
        super().__init__()
        self.position_embedding = nn.Embedding(max_len, d_model)
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        batch_size, seq_len, _ = x.size()
        positions = torch.arange(seq_len, device=x.device).unsqueeze(0)
        return x + self.position_embedding(positions)
```

**对比**：
| 方法 | 优点 | 缺点 | 使用模型 |
| :--- | :--- | :--- | :--- |
| 正弦位置编码 | 可以外推到更长序列 | 固定，不可学习 | 原始Transformer |
| 可学习位置编码 | 可以学习最优表示 | 无法外推到训练时未见过的长度 | GPT-2, BERT |

#### 3.2 相对位置编码（Relative Position Encoding）

现代模型（如GPT-3、Llama）更倾向于使用相对位置编码，在注意力计算中直接编码相对位置关系。

### 4. Self-Attention（自注意力机制）

**核心思想**：让每个Token关注序列中的所有其他Token，学习它们之间的关系。

**数学公式**：
```
Attention(Q, K, V) = softmax(QK^T / √d_k) V
```

**三个关键矩阵**：
- **Q (Query)**：查询矩阵，"我想要什么信息"
- **K (Key)**：键矩阵，"我有什么信息"
- **V (Value)**：值矩阵，"我的信息内容是什么"


**PyTorch实现**：
```python
class SelfAttention(nn.Module):
    def __init__(self, d_model, d_k):
        super().__init__()
        self.d_k = d_k
        self.W_q = nn.Linear(d_model, d_k)
        self.W_k = nn.Linear(d_model, d_k)
        self.W_v = nn.Linear(d_model, d_k)
    
    def forward(self, x, mask=None):
        # x: [batch_size, seq_len, d_model]
        Q = self.W_q(x)  # [batch_size, seq_len, d_k]
        K = self.W_k(x)  # [batch_size, seq_len, d_k]
        V = self.W_v(x)  # [batch_size, seq_len, d_k]
        
        # 计算注意力分数
        scores = torch.matmul(Q, K.transpose(-2, -1)) / (self.d_k ** 0.5)
        # scores: [batch_size, seq_len, seq_len]
        
        # 应用mask（用于因果注意力）
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        # Softmax归一化
        attention_weights = torch.softmax(scores, dim=-1)
        
        # 加权求和
        output = torch.matmul(attention_weights, V)
        # output: [batch_size, seq_len, d_k]
        
        return output, attention_weights
```

**注意力可视化示例**：
```
输入: "The cat sat on the mat"

Token "sat" 的注意力权重:
The:  0.05
cat:  0.35  ← 高权重，"sat"关注主语"cat"
sat:  0.20
on:   0.10
the:  0.05
mat:  0.25  ← 高权重，"sat"关注宾语"mat"
```


### 5. Causal Attention（因果注意力/掩码注意力）

**为什么需要因果注意力？**
- 在自回归语言模型中，生成第i个Token时，只能看到前i-1个Token
- 不能"偷看"未来的Token，否则训练和推理不一致

**实现方式**：使用下三角掩码矩阵

```python
def create_causal_mask(seq_len):
    """创建因果掩码矩阵"""
    mask = torch.tril(torch.ones(seq_len, seq_len))
    return mask  # 下三角矩阵，1表示可见，0表示不可见

# 示例
mask = create_causal_mask(5)
print(mask)
# tensor([[1., 0., 0., 0., 0.],
#         [1., 1., 0., 0., 0.],
#         [1., 1., 1., 0., 0.],
#         [1., 1., 1., 1., 0.],
#         [1., 1., 1., 1., 1.]])
```

**可视化**：
```
位置:  0    1    2    3    4
      [T0] [T1] [T2] [T3] [T4]

T0 可以看到: T0
T1 可以看到: T0, T1
T2 可以看到: T0, T1, T2
T3 可以看到: T0, T1, T2, T3
T4 可以看到: T0, T1, T2, T3, T4
```

### 6. Multi-Head Attention（多头注意力）

**核心思想**：使用多个注意力头，让模型从不同的表示子空间学习信息。

**为什么需要多头？**
- 单个注意力头可能只关注某一种模式（如语法关系）
- 多个头可以同时关注不同的模式（语法、语义、位置等）
- 类似于CNN中的多个卷积核


**PyTorch实现**：
```python
class MultiHeadAttention(nn.Module):
    def __init__(self, d_model, num_heads):
        super().__init__()
        assert d_model % num_heads == 0, "d_model必须能被num_heads整除"
        
        self.d_model = d_model
        self.num_heads = num_heads
        self.d_k = d_model // num_heads
        
        # 为所有头创建Q、K、V的投影矩阵
        self.W_q = nn.Linear(d_model, d_model)
        self.W_k = nn.Linear(d_model, d_model)
        self.W_v = nn.Linear(d_model, d_model)
        
        # 输出投影
        self.W_o = nn.Linear(d_model, d_model)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, d_model = x.size()
        
        # 线性投影并分割成多个头
        # [batch_size, seq_len, d_model] -> [batch_size, num_heads, seq_len, d_k]
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        K = self.W_k(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        V = self.W_v(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        
        # 计算注意力
        scores = torch.matmul(Q, K.transpose(-2, -1)) / (self.d_k ** 0.5)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        attention_output = torch.matmul(attention_weights, V)
        
        # 合并多个头
        # [batch_size, num_heads, seq_len, d_k] -> [batch_size, seq_len, d_model]
        attention_output = attention_output.transpose(1, 2).contiguous().view(
            batch_size, seq_len, d_model
        )
        
        # 输出投影
        output = self.W_o(attention_output)
        
        return output
```


**参数配置示例**：
| 模型 | d_model | num_heads | d_k (每个头) |
| :--- | :--- | :--- | :--- |
| GPT-2 Small | 768 | 12 | 64 |
| GPT-2 Medium | 1024 | 16 | 64 |
| GPT-2 Large | 1280 | 20 | 64 |
| GPT-3 | 12288 | 96 | 128 |

### 7. Feed-Forward Network（前馈网络）

**作用**：在注意力层之后，对每个位置独立地应用非线性变换。

**结构**：两层全连接网络 + 激活函数（通常是GELU或ReLU）

**PyTorch实现**：
```python
class FeedForward(nn.Module):
    def __init__(self, d_model, d_ff, dropout=0.1):
        super().__init__()
        self.linear1 = nn.Linear(d_model, d_ff)
        self.linear2 = nn.Linear(d_ff, d_model)
        self.dropout = nn.Dropout(dropout)
        self.activation = nn.GELU()  # 或 nn.ReLU()
    
    def forward(self, x):
        # x: [batch_size, seq_len, d_model]
        x = self.linear1(x)           # [batch_size, seq_len, d_ff]
        x = self.activation(x)
        x = self.dropout(x)
        x = self.linear2(x)           # [batch_size, seq_len, d_model]
        x = self.dropout(x)
        return x
```

**参数说明**：
- `d_ff`（前馈维度）通常是 `d_model` 的4倍
- 例如：d_model=768, d_ff=3072

**为什么需要前馈网络？**
- 注意力机制是线性的（加权求和）
- 前馈网络引入非线性，增强模型的表达能力
- 可以看作是对每个位置进行独立的特征变换


### 8. Layer Normalization（层归一化）

**作用**：稳定训练，加速收敛。

**PyTorch实现**：
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

**两种LayerNorm位置**：

**Pre-LN（现代模型常用）**：
```
x → LayerNorm → Attention → Add → LayerNorm → FFN → Add
```

**Post-LN（原始Transformer）**：
```
x → Attention → Add → LayerNorm → FFN → Add → LayerNorm
```

**Pre-LN的优势**：
- 训练更稳定
- 不需要学习率预热（warmup）
- 更容易训练深层网络

### 9. Residual Connection（残差连接）

**作用**：缓解梯度消失问题，使得深层网络更容易训练。

**实现**：
```python
# 在Transformer Block中
x = x + attention(layer_norm(x))  # 残差连接
x = x + ffn(layer_norm(x))        # 残差连接
```

**为什么有效？**
- 提供梯度的"高速公路"，直接传播到前面的层
- 允许模型学习"增量"而不是完整的变换
- 使得训练100层以上的模型成为可能


---

## 第三部分：完整的Transformer Block实现

### 1. 单个Transformer Block

```python
class TransformerBlock(nn.Module):
    def __init__(self, d_model, num_heads, d_ff, dropout=0.1):
        super().__init__()
        
        # 多头注意力
        self.attention = MultiHeadAttention(d_model, num_heads)
        
        # 前馈网络
        self.ffn = FeedForward(d_model, d_ff, dropout)
        
        # Layer Normalization
        self.ln1 = nn.LayerNorm(d_model)
        self.ln2 = nn.LayerNorm(d_model)
        
        # Dropout
        self.dropout = nn.Dropout(dropout)
    
    def forward(self, x, mask=None):
        # Pre-LN架构
        # 注意力子层
        attn_output = self.attention(self.ln1(x), mask)
        x = x + self.dropout(attn_output)  # 残差连接
        
        # 前馈子层
        ffn_output = self.ffn(self.ln2(x))
        x = x + self.dropout(ffn_output)   # 残差连接
        
        return x
```

### 2. 完整的GPT模型

```python
class GPT(nn.Module):
    def __init__(
        self,
        vocab_size,      # 词汇表大小
        d_model,         # 模型维度
        num_layers,      # Transformer层数
        num_heads,       # 注意力头数
        d_ff,            # 前馈网络维度
        max_seq_len,     # 最大序列长度
        dropout=0.1
    ):
        super().__init__()
        
        # Token嵌入
        self.token_embedding = nn.Embedding(vocab_size, d_model)
        
        # 位置嵌入
        self.position_embedding = nn.Embedding(max_seq_len, d_model)
        
        # Transformer blocks
        self.blocks = nn.ModuleList([
            TransformerBlock(d_model, num_heads, d_ff, dropout)
            for _ in range(num_layers)
        ])
        
        # 最终的Layer Norm
        self.ln_f = nn.LayerNorm(d_model)
        
        # 输出层（语言模型头）
        self.lm_head = nn.Linear(d_model, vocab_size, bias=False)
        
        # 权重共享：输出层和嵌入层共享权重
        self.lm_head.weight = self.token_embedding.weight
        
        self.dropout = nn.Dropout(dropout)
        
        # 初始化权重
        self.apply(self._init_weights)
    
    def _init_weights(self, module):
        if isinstance(module, nn.Linear):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
            if module.bias is not None:
                torch.nn.init.zeros_(module.bias)
        elif isinstance(module, nn.Embedding):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
    
    def forward(self, input_ids):
        batch_size, seq_len = input_ids.size()
        
        # Token嵌入
        token_emb = self.token_embedding(input_ids)  # [B, T, d_model]
        
        # 位置嵌入
        positions = torch.arange(0, seq_len, device=input_ids.device).unsqueeze(0)
        pos_emb = self.position_embedding(positions)  # [1, T, d_model]
        
        # 组合嵌入
        x = self.dropout(token_emb + pos_emb)
        
        # 创建因果掩码
        mask = torch.tril(torch.ones(seq_len, seq_len, device=input_ids.device))
        mask = mask.unsqueeze(0).unsqueeze(0)  # [1, 1, T, T]
        
        # 通过所有Transformer blocks
        for block in self.blocks:
            x = block(x, mask)
        
        # 最终的Layer Norm
        x = self.ln_f(x)
        
        # 输出logits
        logits = self.lm_head(x)  # [B, T, vocab_size]
        
        return logits
```


### 3. 模型配置示例

```python
# GPT-2 Small配置
config_small = {
    'vocab_size': 50257,
    'd_model': 768,
    'num_layers': 12,
    'num_heads': 12,
    'd_ff': 3072,
    'max_seq_len': 1024,
    'dropout': 0.1
}

# GPT-2 Medium配置
config_medium = {
    'vocab_size': 50257,
    'd_model': 1024,
    'num_layers': 24,
    'num_heads': 16,
    'd_ff': 4096,
    'max_seq_len': 1024,
    'dropout': 0.1
}

# 创建模型
model = GPT(**config_small)
print(f"模型参数量: {sum(p.numel() for p in model.parameters()) / 1e6:.2f}M")
```

### 4. 模型使用示例

```python
# 创建模型
model = GPT(
    vocab_size=32000,
    d_model=512,
    num_layers=6,
    num_heads=8,
    d_ff=2048,
    max_seq_len=512
)

# 准备输入
input_ids = torch.randint(0, 32000, (2, 10))  # [batch_size=2, seq_len=10]

# 前向传播
logits = model(input_ids)  # [2, 10, 32000]

# 预测下一个token
next_token_logits = logits[:, -1, :]  # [2, 32000]
next_token_probs = torch.softmax(next_token_logits, dim=-1)
next_token = torch.argmax(next_token_probs, dim=-1)  # [2]

print(f"输入形状: {input_ids.shape}")
print(f"输出logits形状: {logits.shape}")
print(f"预测的下一个token: {next_token}")
```

---

## 第四部分：训练语言模型

### 1. 损失函数：交叉熵损失

**目标**：最大化正确Token的概率，等价于最小化交叉熵损失。


**数学公式**：
```
Loss = -∑ log P(target_token | context)
```

**PyTorch实现**：
```python
def compute_loss(logits, targets):
    """
    logits: [batch_size, seq_len, vocab_size]
    targets: [batch_size, seq_len]
    """
    # 重塑为2D
    logits = logits.view(-1, logits.size(-1))  # [B*T, vocab_size]
    targets = targets.view(-1)                  # [B*T]
    
    # 计算交叉熵损失
    loss = F.cross_entropy(logits, targets)
    
    return loss

# 使用示例
logits = model(input_ids)
targets = input_ids[:, 1:]  # 目标是下一个token
logits = logits[:, :-1, :]  # 对齐维度

loss = compute_loss(logits, targets)
```

### 2. 训练循环

```python
import torch.optim as optim

# 初始化模型和优化器
model = GPT(**config_small)
optimizer = optim.AdamW(model.parameters(), lr=3e-4, betas=(0.9, 0.95))

# 训练循环
model.train()
for epoch in range(num_epochs):
    for batch in dataloader:
        # 获取输入和目标
        input_ids = batch['input_ids']  # [B, T]
        
        # 前向传播
        logits = model(input_ids)
        
        # 计算损失（预测下一个token）
        # 输入: [0, 1, 2, 3, 4]
        # 目标: [1, 2, 3, 4, 5]
        shift_logits = logits[:, :-1, :].contiguous()
        shift_targets = input_ids[:, 1:].contiguous()
        
        loss = compute_loss(shift_logits, shift_targets)
        
        # 反向传播
        optimizer.zero_grad()
        loss.backward()
        
        # 梯度裁剪（防止梯度爆炸）
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        
        # 更新参数
        optimizer.step()
        
        if step % 100 == 0:
            print(f"Epoch {epoch}, Step {step}, Loss: {loss.item():.4f}")
```


### 3. 学习率调度

**常用策略**：Warmup + Cosine Decay

```python
from torch.optim.lr_scheduler import LambdaLR
import math

def get_lr_scheduler(optimizer, warmup_steps, max_steps):
    def lr_lambda(current_step):
        if current_step < warmup_steps:
            # Warmup阶段：线性增长
            return float(current_step) / float(max(1, warmup_steps))
        else:
            # Cosine decay阶段
            progress = float(current_step - warmup_steps) / float(max(1, max_steps - warmup_steps))
            return 0.5 * (1.0 + math.cos(math.pi * progress))
    
    return LambdaLR(optimizer, lr_lambda)

# 使用
scheduler = get_lr_scheduler(optimizer, warmup_steps=1000, max_steps=100000)

# 在训练循环中
for step in range(max_steps):
    # ... 训练代码 ...
    optimizer.step()
    scheduler.step()
```

**学习率曲线**：
```
LR
 ^
 |     /\
 |    /  \___
 |   /       \___
 |  /            \___
 | /                 \___
 |/________________________> Steps
   Warmup    Cosine Decay
```

### 4. 数据准备

```python
class TextDataset(torch.utils.data.Dataset):
    def __init__(self, text_file, tokenizer, max_length=512):
        self.tokenizer = tokenizer
        self.max_length = max_length
        
        # 读取并tokenize文本
        with open(text_file, 'r', encoding='utf-8') as f:
            text = f.read()
        
        self.tokens = tokenizer.encode(text)
    
    def __len__(self):
        return len(self.tokens) // self.max_length
    
    def __getitem__(self, idx):
        start = idx * self.max_length
        end = start + self.max_length
        
        input_ids = torch.tensor(self.tokens[start:end], dtype=torch.long)
        
        return {'input_ids': input_ids}

# 创建DataLoader
dataset = TextDataset('corpus.txt', tokenizer, max_length=512)
dataloader = torch.utils.data.DataLoader(
    dataset,
    batch_size=8,
    shuffle=True,
    num_workers=4
)
```


---

## 第五部分：文本生成

### 1. 贪婪解码（Greedy Decoding）

**策略**：每次选择概率最高的Token。

```python
@torch.no_grad()
def generate_greedy(model, input_ids, max_new_tokens=50):
    model.eval()
    
    for _ in range(max_new_tokens):
        # 获取logits
        logits = model(input_ids)
        
        # 只关注最后一个位置的预测
        next_token_logits = logits[:, -1, :]
        
        # 选择概率最高的token
        next_token = torch.argmax(next_token_logits, dim=-1, keepdim=True)
        
        # 添加到序列
        input_ids = torch.cat([input_ids, next_token], dim=1)
        
        # 如果生成了结束符，停止
        if next_token.item() == eos_token_id:
            break
    
    return input_ids

# 使用示例
prompt = "Once upon a time"
input_ids = tokenizer.encode(prompt, return_tensors='pt')
output_ids = generate_greedy(model, input_ids, max_new_tokens=50)
generated_text = tokenizer.decode(output_ids[0])
print(generated_text)
```

**优点**：简单、快速、确定性
**缺点**：生成的文本可能重复、缺乏多样性

### 2. 采样（Sampling）

**策略**：根据概率分布随机采样。

```python
@torch.no_grad()
def generate_sample(model, input_ids, max_new_tokens=50, temperature=1.0):
    model.eval()
    
    for _ in range(max_new_tokens):
        logits = model(input_ids)
        next_token_logits = logits[:, -1, :] / temperature
        
        # 计算概率分布
        probs = torch.softmax(next_token_logits, dim=-1)
        
        # 从分布中采样
        next_token = torch.multinomial(probs, num_samples=1)
        
        input_ids = torch.cat([input_ids, next_token], dim=1)
        
        if next_token.item() == eos_token_id:
            break
    
    return input_ids
```

**Temperature参数**：
- `temperature < 1.0`：分布更尖锐，更确定性
- `temperature = 1.0`：原始分布
- `temperature > 1.0`：分布更平滑，更随机


### 3. Top-k 采样

**策略**：只从概率最高的k个Token中采样。

```python
@torch.no_grad()
def generate_top_k(model, input_ids, max_new_tokens=50, top_k=50, temperature=1.0):
    model.eval()
    
    for _ in range(max_new_tokens):
        logits = model(input_ids)
        next_token_logits = logits[:, -1, :] / temperature
        
        # 只保留top-k个最高的logits
        top_k_logits, top_k_indices = torch.topk(next_token_logits, top_k)
        
        # 将其他位置设为-inf
        next_token_logits = torch.full_like(next_token_logits, float('-inf'))
        next_token_logits.scatter_(1, top_k_indices, top_k_logits)
        
        # 采样
        probs = torch.softmax(next_token_logits, dim=-1)
        next_token = torch.multinomial(probs, num_samples=1)
        
        input_ids = torch.cat([input_ids, next_token], dim=1)
        
        if next_token.item() == eos_token_id:
            break
    
    return input_ids
```

### 4. Top-p (Nucleus) 采样

**策略**：从累积概率达到p的最小Token集合中采样。

```python
@torch.no_grad()
def generate_top_p(model, input_ids, max_new_tokens=50, top_p=0.9, temperature=1.0):
    model.eval()
    
    for _ in range(max_new_tokens):
        logits = model(input_ids)
        next_token_logits = logits[:, -1, :] / temperature
        
        # 排序
        sorted_logits, sorted_indices = torch.sort(next_token_logits, descending=True)
        cumulative_probs = torch.cumsum(torch.softmax(sorted_logits, dim=-1), dim=-1)
        
        # 移除累积概率超过top_p的token
        sorted_indices_to_remove = cumulative_probs > top_p
        sorted_indices_to_remove[..., 1:] = sorted_indices_to_remove[..., :-1].clone()
        sorted_indices_to_remove[..., 0] = 0
        
        indices_to_remove = sorted_indices_to_remove.scatter(1, sorted_indices, sorted_indices_to_remove)
        next_token_logits[indices_to_remove] = float('-inf')
        
        # 采样
        probs = torch.softmax(next_token_logits, dim=-1)
        next_token = torch.multinomial(probs, num_samples=1)
        
        input_ids = torch.cat([input_ids, next_token], dim=1)
        
        if next_token.item() == eos_token_id:
            break
    
    return input_ids
```


### 5. Beam Search（束搜索）

**策略**：维护k个最可能的序列，每次扩展所有候选。

```python
@torch.no_grad()
def generate_beam_search(model, input_ids, max_new_tokens=50, num_beams=5):
    model.eval()
    batch_size = input_ids.size(0)
    
    # 初始化beam
    # 每个beam: (sequence, score)
    beams = [(input_ids[0], 0.0)]
    
    for _ in range(max_new_tokens):
        all_candidates = []
        
        for seq, score in beams:
            if seq[-1].item() == eos_token_id:
                all_candidates.append((seq, score))
                continue
            
            # 获取下一个token的概率
            logits = model(seq.unsqueeze(0))
            next_token_logits = logits[0, -1, :]
            log_probs = torch.log_softmax(next_token_logits, dim=-1)
            
            # 获取top-k个候选
            top_log_probs, top_indices = torch.topk(log_probs, num_beams)
            
            for log_prob, token_id in zip(top_log_probs, top_indices):
                new_seq = torch.cat([seq, token_id.unsqueeze(0)])
                new_score = score + log_prob.item()
                all_candidates.append((new_seq, new_score))
        
        # 选择得分最高的num_beams个序列
        beams = sorted(all_candidates, key=lambda x: x[1], reverse=True)[:num_beams]
        
        # 如果所有beam都结束了，停止
        if all(seq[-1].item() == eos_token_id for seq, _ in beams):
            break
    
    # 返回得分最高的序列
    best_seq, best_score = beams[0]
    return best_seq.unsqueeze(0)
```

### 6. 生成策略对比

| 策略 | 优点 | 缺点 | 适用场景 |
| :--- | :--- | :--- | :--- |
| **Greedy** | 快速、确定性 | 容易重复、缺乏多样性 | 需要确定性输出 |
| **Sampling** | 多样性好 | 可能不连贯 | 创意写作 |
| **Top-k** | 平衡质量和多样性 | k值难以调整 | 通用文本生成 |
| **Top-p** | 动态调整候选集 | 计算稍慢 | 高质量生成（推荐） |
| **Beam Search** | 全局最优 | 慢、缺乏多样性 | 翻译、摘要 |


---

## 第六部分：优化技巧和最佳实践

### 1. 权重初始化

**重要性**：好的初始化可以加速收敛，避免梯度消失/爆炸。

```python
def _init_weights(self, module):
    if isinstance(module, nn.Linear):
        # Xavier/Glorot初始化
        torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
        if module.bias is not None:
            torch.nn.init.zeros_(module.bias)
    elif isinstance(module, nn.Embedding):
        torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
    elif isinstance(module, nn.LayerNorm):
        torch.nn.init.ones_(module.weight)
        torch.nn.init.zeros_(module.bias)
```

### 2. 梯度裁剪

**作用**：防止梯度爆炸。

```python
# 在optimizer.step()之前
torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
```

### 3. 混合精度训练

**作用**：加速训练，减少显存占用。

```python
from torch.cuda.amp import autocast, GradScaler

scaler = GradScaler()

for batch in dataloader:
    optimizer.zero_grad()
    
    # 使用混合精度
    with autocast():
        logits = model(input_ids)
        loss = compute_loss(logits, targets)
    
    # 缩放损失并反向传播
    scaler.scale(loss).backward()
    
    # 梯度裁剪
    scaler.unscale_(optimizer)
    torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
    
    # 更新参数
    scaler.step(optimizer)
    scaler.update()
```

### 4. 梯度累积

**作用**：在显存有限时模拟大batch size。

```python
accumulation_steps = 4  # 累积4个batch

for i, batch in enumerate(dataloader):
    logits = model(input_ids)
    loss = compute_loss(logits, targets)
    
    # 归一化损失
    loss = loss / accumulation_steps
    loss.backward()
    
    # 每accumulation_steps步更新一次
    if (i + 1) % accumulation_steps == 0:
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        optimizer.step()
        optimizer.zero_grad()
```


### 5. 权重共享（Weight Tying）

**策略**：输入嵌入层和输出层共享权重。

```python
# 在模型初始化中
self.token_embedding = nn.Embedding(vocab_size, d_model)
self.lm_head = nn.Linear(d_model, vocab_size, bias=False)

# 共享权重
self.lm_head.weight = self.token_embedding.weight
```

**优点**：
- 减少参数量（约减少vocab_size × d_model个参数）
- 提高训练效率
- 通常不损失性能

### 6. Dropout策略

**位置**：
- 嵌入层之后
- 注意力输出之后
- 前馈网络之后

**推荐值**：
- 小模型：0.1
- 大模型：0.0-0.1（大模型通常不需要太多dropout）

### 7. 模型保存和加载

```python
# 保存模型
def save_checkpoint(model, optimizer, epoch, loss, path):
    checkpoint = {
        'epoch': epoch,
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'loss': loss,
    }
    torch.save(checkpoint, path)

# 加载模型
def load_checkpoint(model, optimizer, path):
    checkpoint = torch.load(path)
    model.load_state_dict(checkpoint['model_state_dict'])
    optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
    epoch = checkpoint['epoch']
    loss = checkpoint['loss']
    return epoch, loss

# 使用
save_checkpoint(model, optimizer, epoch, loss, 'checkpoint.pt')
epoch, loss = load_checkpoint(model, optimizer, 'checkpoint.pt')
```

### 8. 评估指标：困惑度（Perplexity）

**定义**：衡量模型预测的不确定性，越低越好。

```python
def compute_perplexity(model, dataloader):
    model.eval()
    total_loss = 0
    total_tokens = 0
    
    with torch.no_grad():
        for batch in dataloader:
            input_ids = batch['input_ids']
            logits = model(input_ids)
            
            shift_logits = logits[:, :-1, :].contiguous()
            shift_targets = input_ids[:, 1:].contiguous()
            
            loss = compute_loss(shift_logits, shift_targets)
            
            total_loss += loss.item() * shift_targets.numel()
            total_tokens += shift_targets.numel()
    
    avg_loss = total_loss / total_tokens
    perplexity = math.exp(avg_loss)
    
    return perplexity

# 使用
ppl = compute_perplexity(model, val_dataloader)
print(f"Perplexity: {ppl:.2f}")
```


---

## 第七部分：完整训练示例

### 完整的训练脚本

```python
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
import math

# 1. 配置
config = {
    'vocab_size': 32000,
    'd_model': 512,
    'num_layers': 6,
    'num_heads': 8,
    'd_ff': 2048,
    'max_seq_len': 512,
    'dropout': 0.1,
    'batch_size': 32,
    'num_epochs': 10,
    'learning_rate': 3e-4,
    'warmup_steps': 1000,
    'max_steps': 100000,
}

# 2. 创建模型
model = GPT(
    vocab_size=config['vocab_size'],
    d_model=config['d_model'],
    num_layers=config['num_layers'],
    num_heads=config['num_heads'],
    d_ff=config['d_ff'],
    max_seq_len=config['max_seq_len'],
    dropout=config['dropout']
)

# 移到GPU
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
model = model.to(device)

print(f"模型参数量: {sum(p.numel() for p in model.parameters()) / 1e6:.2f}M")

# 3. 准备数据
train_dataset = TextDataset('train.txt', tokenizer, max_length=config['max_seq_len'])
val_dataset = TextDataset('val.txt', tokenizer, max_length=config['max_seq_len'])

train_loader = DataLoader(
    train_dataset,
    batch_size=config['batch_size'],
    shuffle=True,
    num_workers=4
)

val_loader = DataLoader(
    val_dataset,
    batch_size=config['batch_size'],
    shuffle=False,
    num_workers=4
)

# 4. 优化器和调度器
optimizer = optim.AdamW(
    model.parameters(),
    lr=config['learning_rate'],
    betas=(0.9, 0.95),
    weight_decay=0.1
)

scheduler = get_lr_scheduler(
    optimizer,
    warmup_steps=config['warmup_steps'],
    max_steps=config['max_steps']
)

# 5. 训练循环
global_step = 0
best_val_loss = float('inf')

for epoch in range(config['num_epochs']):
    # 训练阶段
    model.train()
    train_loss = 0
    
    for batch_idx, batch in enumerate(train_loader):
        input_ids = batch['input_ids'].to(device)
        
        # 前向传播
        logits = model(input_ids)
        
        # 计算损失
        shift_logits = logits[:, :-1, :].contiguous()
        shift_targets = input_ids[:, 1:].contiguous()
        loss = compute_loss(shift_logits, shift_targets)
        
        # 反向传播
        optimizer.zero_grad()
        loss.backward()
        
        # 梯度裁剪
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        
        # 更新参数
        optimizer.step()
        scheduler.step()
        
        train_loss += loss.item()
        global_step += 1
        
        # 日志
        if global_step % 100 == 0:
            avg_loss = train_loss / (batch_idx + 1)
            ppl = math.exp(avg_loss)
            lr = scheduler.get_last_lr()[0]
            print(f"Epoch {epoch}, Step {global_step}, "
                  f"Loss: {avg_loss:.4f}, PPL: {ppl:.2f}, LR: {lr:.6f}")
    
    # 验证阶段
    model.eval()
    val_loss = 0
    
    with torch.no_grad():
        for batch in val_loader:
            input_ids = batch['input_ids'].to(device)
            logits = model(input_ids)
            
            shift_logits = logits[:, :-1, :].contiguous()
            shift_targets = input_ids[:, 1:].contiguous()
            loss = compute_loss(shift_logits, shift_targets)
            
            val_loss += loss.item()
    
    avg_val_loss = val_loss / len(val_loader)
    val_ppl = math.exp(avg_val_loss)
    
    print(f"\nEpoch {epoch} - Validation Loss: {avg_val_loss:.4f}, PPL: {val_ppl:.2f}\n")
    
    # 保存最佳模型
    if avg_val_loss < best_val_loss:
        best_val_loss = avg_val_loss
        save_checkpoint(model, optimizer, epoch, avg_val_loss, 'best_model.pt')
        print(f"保存最佳模型，验证损失: {avg_val_loss:.4f}")

print("训练完成！")
```


---

## 第八部分：常见问题和调试技巧

### 1. 显存不足（OOM）

**解决方案**：
```python
# 方法1: 减小batch size
config['batch_size'] = 16  # 从32减到16

# 方法2: 减小序列长度
config['max_seq_len'] = 256  # 从512减到256

# 方法3: 使用梯度累积
accumulation_steps = 4

# 方法4: 使用梯度检查点（Gradient Checkpointing）
from torch.utils.checkpoint import checkpoint

def forward_with_checkpointing(self, x):
    for block in self.blocks:
        x = checkpoint(block, x)
    return x

# 方法5: 使用混合精度训练
from torch.cuda.amp import autocast, GradScaler
```

### 2. 训练不收敛

**可能原因和解决方案**：

**学习率过大**：
```python
# 降低学习率
optimizer = optim.AdamW(model.parameters(), lr=1e-4)  # 从3e-4降到1e-4
```

**梯度爆炸**：
```python
# 检查梯度范数
total_norm = 0
for p in model.parameters():
    if p.grad is not None:
        param_norm = p.grad.data.norm(2)
        total_norm += param_norm.item() ** 2
total_norm = total_norm ** 0.5
print(f"Gradient norm: {total_norm}")

# 使用更严格的梯度裁剪
torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=0.5)
```

**权重初始化不当**：
```python
# 使用更小的初始化标准差
torch.nn.init.normal_(module.weight, mean=0.0, std=0.01)
```

### 3. 生成文本质量差

**问题1：重复生成**
```python
# 解决方案：使用repetition penalty
def apply_repetition_penalty(logits, input_ids, penalty=1.2):
    for token_id in set(input_ids.tolist()):
        logits[token_id] /= penalty
    return logits
```

**问题2：生成不连贯**
```python
# 解决方案：调整temperature和top_p
generated = generate_top_p(
    model, 
    input_ids, 
    temperature=0.8,  # 降低随机性
    top_p=0.9         # 只从高概率token中采样
)
```

**问题3：生成太短**
```python
# 解决方案：添加长度惩罚
def length_penalty(length, alpha=0.6):
    return ((5 + length) / 6) ** alpha
```


### 4. 调试技巧

**检查模型输出形状**：
```python
def check_shapes(model, batch_size=2, seq_len=10):
    input_ids = torch.randint(0, model.vocab_size, (batch_size, seq_len))
    
    print(f"Input shape: {input_ids.shape}")
    
    # Token embedding
    token_emb = model.token_embedding(input_ids)
    print(f"Token embedding shape: {token_emb.shape}")
    
    # Position embedding
    positions = torch.arange(0, seq_len).unsqueeze(0)
    pos_emb = model.position_embedding(positions)
    print(f"Position embedding shape: {pos_emb.shape}")
    
    # Full forward pass
    logits = model(input_ids)
    print(f"Output logits shape: {logits.shape}")
    
    assert logits.shape == (batch_size, seq_len, model.vocab_size)
    print("✓ All shapes correct!")

check_shapes(model)
```

**可视化注意力权重**：
```python
import matplotlib.pyplot as plt
import seaborn as sns

def visualize_attention(attention_weights, tokens):
    """
    attention_weights: [seq_len, seq_len]
    tokens: list of token strings
    """
    plt.figure(figsize=(10, 8))
    sns.heatmap(
        attention_weights.cpu().numpy(),
        xticklabels=tokens,
        yticklabels=tokens,
        cmap='viridis',
        cbar=True
    )
    plt.xlabel('Key')
    plt.ylabel('Query')
    plt.title('Attention Weights')
    plt.show()
```

**监控训练指标**：
```python
from torch.utils.tensorboard import SummaryWriter

writer = SummaryWriter('runs/gpt_training')

# 在训练循环中
writer.add_scalar('Loss/train', loss.item(), global_step)
writer.add_scalar('Loss/val', val_loss, epoch)
writer.add_scalar('Perplexity/train', train_ppl, global_step)
writer.add_scalar('Perplexity/val', val_ppl, epoch)
writer.add_scalar('Learning_rate', lr, global_step)

# 可视化权重分布
for name, param in model.named_parameters():
    writer.add_histogram(name, param, global_step)

writer.close()
```

---

## 第九部分：扩展和改进

### 1. Flash Attention

**作用**：加速注意力计算，减少显存占用。

```python
# 安装: pip install flash-attn
from flash_attn import flash_attn_func

class FlashMultiHeadAttention(nn.Module):
    def __init__(self, d_model, num_heads):
        super().__init__()
        self.d_model = d_model
        self.num_heads = num_heads
        self.d_k = d_model // num_heads
        
        self.W_qkv = nn.Linear(d_model, 3 * d_model)
        self.W_o = nn.Linear(d_model, d_model)
    
    def forward(self, x):
        batch_size, seq_len, _ = x.size()
        
        # 计算Q, K, V
        qkv = self.W_qkv(x)
        qkv = qkv.reshape(batch_size, seq_len, 3, self.num_heads, self.d_k)
        qkv = qkv.permute(2, 0, 3, 1, 4)  # [3, B, H, T, d_k]
        q, k, v = qkv[0], qkv[1], qkv[2]
        
        # 使用Flash Attention
        output = flash_attn_func(q, k, v, causal=True)
        
        # 重塑并投影
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, self.d_model)
        output = self.W_o(output)
        
        return output
```


### 2. Rotary Position Embedding (RoPE)

**优势**：相对位置编码，外推性能好。

```python
class RotaryPositionEmbedding(nn.Module):
    def __init__(self, d_model, max_seq_len=2048):
        super().__init__()
        inv_freq = 1.0 / (10000 ** (torch.arange(0, d_model, 2).float() / d_model))
        self.register_buffer('inv_freq', inv_freq)
        
        # 预计算位置编码
        t = torch.arange(max_seq_len).type_as(self.inv_freq)
        freqs = torch.einsum('i,j->ij', t, self.inv_freq)
        emb = torch.cat((freqs, freqs), dim=-1)
        self.register_buffer('cos_cached', emb.cos())
        self.register_buffer('sin_cached', emb.sin())
    
    def rotate_half(self, x):
        x1, x2 = x[..., :x.shape[-1]//2], x[..., x.shape[-1]//2:]
        return torch.cat((-x2, x1), dim=-1)
    
    def forward(self, q, k):
        seq_len = q.shape[1]
        cos = self.cos_cached[:seq_len, ...]
        sin = self.sin_cached[:seq_len, ...]
        
        q_embed = (q * cos) + (self.rotate_half(q) * sin)
        k_embed = (k * cos) + (self.rotate_half(k) * sin)
        
        return q_embed, k_embed
```

### 3. Grouped Query Attention (GQA)

**优势**：减少KV cache，加速推理。

```python
class GroupedQueryAttention(nn.Module):
    def __init__(self, d_model, num_heads, num_kv_heads):
        super().__init__()
        assert num_heads % num_kv_heads == 0
        
        self.d_model = d_model
        self.num_heads = num_heads
        self.num_kv_heads = num_kv_heads
        self.num_queries_per_kv = num_heads // num_kv_heads
        self.d_k = d_model // num_heads
        
        self.W_q = nn.Linear(d_model, d_model)
        self.W_k = nn.Linear(d_model, num_kv_heads * self.d_k)
        self.W_v = nn.Linear(d_model, num_kv_heads * self.d_k)
        self.W_o = nn.Linear(d_model, d_model)
    
    def forward(self, x, mask=None):
        batch_size, seq_len, _ = x.size()
        
        # Q: [B, num_heads, T, d_k]
        Q = self.W_q(x).view(batch_size, seq_len, self.num_heads, self.d_k).transpose(1, 2)
        
        # K, V: [B, num_kv_heads, T, d_k]
        K = self.W_k(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        V = self.W_v(x).view(batch_size, seq_len, self.num_kv_heads, self.d_k).transpose(1, 2)
        
        # 复制K和V以匹配Q的头数
        K = K.repeat_interleave(self.num_queries_per_kv, dim=1)
        V = V.repeat_interleave(self.num_queries_per_kv, dim=1)
        
        # 标准注意力计算
        scores = torch.matmul(Q, K.transpose(-2, -1)) / (self.d_k ** 0.5)
        
        if mask is not None:
            scores = scores.masked_fill(mask == 0, float('-inf'))
        
        attention_weights = torch.softmax(scores, dim=-1)
        output = torch.matmul(attention_weights, V)
        
        output = output.transpose(1, 2).contiguous().view(batch_size, seq_len, self.d_model)
        output = self.W_o(output)
        
        return output
```

### 4. SwiGLU激活函数

**优势**：比GELU性能更好（用于Llama等模型）。

```python
class SwiGLU(nn.Module):
    def forward(self, x):
        x, gate = x.chunk(2, dim=-1)
        return F.silu(gate) * x

class FeedForwardSwiGLU(nn.Module):
    def __init__(self, d_model, d_ff, dropout=0.1):
        super().__init__()
        # 注意：需要2倍的d_ff因为会split
        self.linear1 = nn.Linear(d_model, 2 * d_ff)
        self.linear2 = nn.Linear(d_ff, d_model)
        self.dropout = nn.Dropout(dropout)
        self.swiglu = SwiGLU()
    
    def forward(self, x):
        x = self.linear1(x)
        x = self.swiglu(x)
        x = self.dropout(x)
        x = self.linear2(x)
        x = self.dropout(x)
        return x
```


---

## 第十部分：与现代LLM的对比

### 主流模型架构对比

| 特性 | GPT-2 | GPT-3 | Llama | Llama 2 | GPT-4 (推测) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **参数量** | 1.5B | 175B | 7B-65B | 7B-70B | 1.76T |
| **层数** | 48 | 96 | 32 | 32-80 | ? |
| **d_model** | 1600 | 12288 | 4096 | 4096-8192 | ? |
| **注意力头数** | 25 | 96 | 32 | 32-64 | ? |
| **位置编码** | Learned | Learned | RoPE | RoPE | ? |
| **激活函数** | GELU | GELU | SwiGLU | SwiGLU | ? |
| **归一化** | LayerNorm | LayerNorm | RMSNorm | RMSNorm | ? |
| **词汇表** | 50257 | 50257 | 32000 | 32000 | ? |
| **上下文长度** | 1024 | 2048 | 2048 | 4096 | 32k-128k |

### 关键改进点

**1. RMSNorm vs LayerNorm**
```python
class RMSNorm(nn.Module):
    def __init__(self, d_model, eps=1e-6):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(d_model))
        self.eps = eps
    
    def forward(self, x):
        # 只使用RMS，不减去均值
        rms = torch.sqrt(torch.mean(x ** 2, dim=-1, keepdim=True) + self.eps)
        return self.weight * x / rms
```

**优势**：计算更快，性能相当或更好

**2. 并行化Attention和FFN（PaLM架构）**
```python
class ParallelTransformerBlock(nn.Module):
    def forward(self, x, mask=None):
        # 并行计算attention和ffn
        attn_out = self.attention(self.ln1(x), mask)
        ffn_out = self.ffn(self.ln2(x))
        
        # 合并输出
        x = x + attn_out + ffn_out
        return x
```

**优势**：训练速度提升15-20%

---

## 总结

### 核心要点回顾

1. **Transformer架构**是现代LLM的基础：
   - Token Embedding + Position Embedding
   - Multi-Head Self-Attention（多头自注意力）
   - Feed-Forward Network（前馈网络）
   - Layer Normalization + Residual Connection

2. **因果注意力**是自回归语言模型的关键：
   - 使用下三角掩码
   - 确保训练和推理一致

3. **训练技巧**：
   - 合适的学习率调度（Warmup + Cosine Decay）
   - 梯度裁剪防止梯度爆炸
   - 混合精度训练加速
   - 权重共享减少参数

4. **生成策略**：
   - Greedy：快速但缺乏多样性
   - Top-p：推荐的平衡策略
   - Beam Search：适合翻译等任务

5. **现代改进**：
   - RoPE位置编码
   - GQA减少KV cache
   - SwiGLU激活函数
   - Flash Attention加速


### 从零到一的完整流程

```
1. 数据准备
   ↓
2. Tokenization（第一讲）
   ↓
3. 构建模型架构（本讲）
   - Token Embedding
   - Position Embedding
   - Transformer Blocks
   - Output Layer
   ↓
4. 训练
   - 定义损失函数
   - 选择优化器
   - 设置学习率调度
   - 训练循环
   ↓
5. 评估
   - 计算困惑度
   - 生成样本检查
   ↓
6. 推理和生成
   - 选择生成策略
   - 调整超参数
   ↓
7. 优化和部署
   - 模型量化
   - 推理加速
```

### 实践建议

**从小模型开始**：
- 先用小数据集（如WikiText-2）
- 小模型配置（6层，512维度）
- 验证代码正确性

**逐步扩大规模**：
- 确认小模型能正常训练
- 逐步增加模型大小
- 使用更大的数据集

**关注关键指标**：
- 训练损失是否下降
- 验证困惑度是否合理
- 生成文本是否连贯

**调试优先级**：
1. 确保代码能运行（形状正确）
2. 确保损失下降（学习率、初始化）
3. 确保不过拟合（dropout、数据量）
4. 优化生成质量（采样策略）

### 下一步学习

**第三讲预告**：
- 大规模数据处理
- 数据清洗和过滤
- 数据并行和模型并行
- 分布式训练

**推荐资源**：
- [Attention is All You Need](https://arxiv.org/abs/1706.03762) - Transformer原始论文
- [Language Models are Unsupervised Multitask Learners](https://d4mucfpksywv.cloudfront.net/better-language-models/language_models_are_unsupervised_multitask_learners.pdf) - GPT-2论文
- [Llama 2: Open Foundation and Fine-Tuned Chat Models](https://arxiv.org/abs/2307.09288) - Llama 2论文
- [The Illustrated Transformer](http://jalammar.github.io/illustrated-transformer/) - 可视化教程
- [nanoGPT](https://github.com/karpathy/nanoGPT) - Andrej Karpathy的最小GPT实现

---

## 附录：完整代码仓库结构

```
gpt-from-scratch/
├── model/
│   ├── __init__.py
│   ├── embedding.py          # Token和Position Embedding
│   ├── attention.py          # Multi-Head Attention
│   ├── feedforward.py        # Feed-Forward Network
│   ├── transformer_block.py  # Transformer Block
│   └── gpt.py               # 完整GPT模型
├── data/
│   ├── __init__.py
│   ├── dataset.py           # 数据集类
│   └── tokenizer.py         # Tokenizer封装
├── training/
│   ├── __init__.py
│   ├── trainer.py           # 训练器
│   └── scheduler.py         # 学习率调度器
├── generation/
│   ├── __init__.py
│   └── sampling.py          # 各种采样策略
├── utils/
│   ├── __init__.py
│   ├── checkpoint.py        # 模型保存/加载
│   └── metrics.py           # 评估指标
├── configs/
│   ├── gpt_small.yaml       # 小模型配置
│   ├── gpt_medium.yaml      # 中等模型配置
│   └── gpt_large.yaml       # 大模型配置
├── train.py                 # 训练脚本
├── generate.py              # 生成脚本
├── evaluate.py              # 评估脚本
└── requirements.txt         # 依赖包
```

希望这份详细的笔记能帮助你深入理解如何从零开始构建一个语言模型！下一讲我们将深入探讨大规模训练的技术细节。
