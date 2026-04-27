# Transformer 算法详解与 PyTorch 实现

> 基于论文 [Attention Is All You Need](https://arxiv.org/abs/1706.03762) (Vaswani et al., 2017)

---

## 1. 概述

Transformer 是一种基于**自注意力机制（Self-Attention）**的深度学习模型，完全抛弃了传统的 RNN/CNN 结构，仅依靠注意力机制来捕捉序列中的全局依赖关系。

**核心优势：**
- **并行计算**：不像 RNN 需要逐步处理，Transformer 可以并行处理整个序列
- **长距离依赖**：自注意力机制直接计算序列中任意两个位置的关系，不受距离限制
- **可扩展性**：易于堆叠多层，模型容量大

**架构总览：**

```
输入序列 → [Embedding + Positional Encoding] → N × Encoder Block → Encoder输出
                                                               ↓
目标序列 → [Embedding + Positional Encoding] → N × Decoder Block → 输出概率
```

---

## 2. 自注意力机制 (Self-Attention)

### 2.1 核心思想

对于序列中的每个位置，通过**查询（Query）**去与所有位置的**键（Key）**做匹配，得到注意力权重，再用权重对**值（Value）**加权求和，得到该位置的输出。

**直观类比：** 在数据库中查询信息 —— Query 是搜索关键词，Key 是每条记录的索引，Value 是记录内容。Query 与 Key 越匹配，对应的 Value 权重越大。

### 2.1.1 Q、K、V三者关系的整体理解

自注意力的核心公式：$\text{Attention}(Q, K, V) = \text{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right) V$

Q、K、V来自同一个输入 $x$，分别通过 $W_Q$、$W_K$、$W_V$ 投影得到。它们分工协作，各司其职：

| 角色 | 功能 | 优化目标 | 直觉类比 |
|------|------|---------|---------|
| **Q** | 决定"我想要什么信息" | 找到最相关的K | 招聘者发布需求 |
| **K** | 决定"我有什么信息标签" | 被正确的Q匹配到 | 求职者的简历标签 |
| **V** | 决定"被选中后提供什么内容" | 提供最有用的信息 | 求职者的实际能力 |

**整体数据流：**

```
输入 x ──→ W_Q → Q  "我需要什么?"  ──→ 与K匹配 ──→ 得到注意力权重α ──→ 对V加权求和 ──→ 输出
输入 x ──→ W_K → K  "我是谁?"      ──→ 被Q匹配   ↑                        ↑
输入 x ──→ W_V → V  "我有什么?"    ──→──────────────────────────────────────被α选中──→ 贡献内容
```

- **Q和K的交互**决定"路由"：哪个位置的信息流向哪个位置（注意力权重 $\alpha$）
- **V**决定"内容"：被选中后实际提供的语义信息
- 整个过程可以理解为：**Q×K 建立信息通道，V在通道中传输内容**

**为什么三者不会坍缩为相同的向量？**

三者的梯度路径完全不同：

```
Loss → output → V          (直接路径：V直接影响输出内容)
Loss → output → α → Q      (间接路径：Q只影响路由，再通过路由影响输出)
Loss → output → α → K      (间接路径：K只影响路由，再通过路由影响输出)
```

- V的梯度：$\frac{\partial L}{\partial V_j} = \sum_i \alpha_{ij} \cdot \frac{\partial L}{\partial \text{output}_i}$ → V优化"被选中后提供什么"
- K的梯度：$\frac{\partial L}{\partial K_j} = \sum_i \frac{\partial L}{\partial \alpha_{ij}} \cdot \frac{\partial \alpha_{ij}}{\partial K_j}$ → K优化"应该被谁关注"
- Q的梯度：$\frac{\partial L}{\partial Q_i} = \sum_j \frac{\partial L}{\partial \alpha_{ij}} \cdot \frac{\partial \alpha_{ij}}{\partial Q_i}$ → Q优化"应该关注谁"

三条梯度路径指向三个不同的优化目标，合并参数会同时满足三个矛盾需求，梯度会自动避免坍缩。

#### Q和K的分化：从随机到协作

**初始时**：W_Q和W_K都是Xavier随机初始化的，从第一天起Q和K就不同，但这种差异是"随机噪声"，没有语义意义。

**训练中逐步分化**的关键是——**Q和K承担不同的角色，梯度方向不同，导致参数朝不同方向演化**：

- **Q是"需求方"**：决定"我想要什么信息"——优化目标是让Q能找到最相关的K
- **K是"供给方"**：决定"我有什么信息标签"——优化目标是让K能被正确的Q匹配到

角色不同 → 梯度不同 → 参数分化。

**逐步分化的具体过程（以翻译"我爱你"→"I love you"为例）：**

**Step 0（随机初始化）：** Q和K是随机投影，注意力权重接近均匀分布（每个位置获得≈1/3的注意力），模型没有学到任何有用的模式。

**Step 100（初步分化）：** 模型发现"翻译'I'时应关注源序列的'我'"。为了实现这一点：
- **W_Q调整**：让"I"的Q向量更接近"我"的K向量 → Q学习"我是代词，我需要找名词"
- **W_K调整**：让"我"的K向量更容易被"I"的Q匹配到 → K学习"我是中文代词，我的标签应该容易被英文代词找到"

两者的梯度方向**天然相反**：
- W_Q的梯度推动Q去"寻找"（Q更泛化，能匹配多个相关K）
- W_K的梯度推动K去"被找到"（K更独特，只被相关Q匹配）

**Step 1000（角色定型）：** 经过大量训练样本后：
- Q形成了**查询模式**：代词的Q总是寻找名词、动词的Q总是寻找宾语...
- K形成了**索引模式**：名词的K总是被代词Q匹配、宾语的K总是被动词Q匹配...

对注意力权重 $\alpha_{ij} = \text{softmax}(Q_i K_j^T / \sqrt{d_k})$，注意 $\frac{\partial \alpha_{ij}}{\partial W_Q}$ 和 $\frac{\partial \alpha_{ij}}{\partial W_K}$ 的**结构不同**：

- 对Q的梯度：$\frac{\partial \alpha_{ij}}{\partial Q_i} = \alpha_{ij}(e_j - \sum_k \alpha_{ik} e_k)$ → 推动Q远离"平均K"，朝"目标K"移动
- 对K的梯度：$\frac{\partial \alpha_{ij}}{\partial K_j} = \alpha_{ij}(Q_i - \sum_k \alpha_{ik} Q_i \cdot \text{缩放因子})$ → 推动K远离"平均匹配"，朝"专属匹配"移动

**直觉类比：** Q像"招聘者"，学习发布什么样的需求描述能找到最合适的人；K像"简历标签"，学习标注什么样的技能关键词能被最合适的招聘者搜到。两者的优化方向不同，经过多轮互动（训练迭代），双方各自演化出不同的"语言体系"。

#### K和V的分工：路由与内容

**核心区别：K控制"能否被选中"，V控制"被选中后提供什么"。**

- **K**：决定注意力权重 $\alpha_{ij}$——"哪些Q会关注我"
- **V**：决定被选中后的输出内容——"关注我后得到什么信息"

V的梯度**直接**影响output——V是最终输出的原材料；K的梯度**间接**影响output——K只改变注意力权重（路由），再通过权重影响output。V优化的是"产品质量"，K优化的是"产品曝光"。

**逐步分化过程（继续以翻译为例）：**

**Step 0：** W_K和W_V随机初始化，K起不到索引作用（注意力接近均匀），V提供的内容也是随机噪声。

**Step 100：** 模型发现错误——翻译"我"时输出不对。两条优化路径同时启动：

路径A（K的优化）："我"的K没有被"I"的Q足够关注 → **增大K("我")与Q("I")的点积** → K学习"我是代词类，代词Q应该找我"

路径B（V的优化）：即使K被正确关注了，V("我")提供的信息不对 → **修正V("我")的向量** → V学习"我被选中后应该提供'第一人称'的语义信息"

两者必须**协同收敛**：如果K不被关注 → V再好也没用（信息无法传递）；如果K被关注了但V提供垃圾 → 输出仍然错误。

**Step 1000：** K和V形成稳定的分工：

```
token "我":
  K("我") ≈ [代词标志, 主语标志, ...]   ← 紧凑的"标签向量"，编码"我是什么类型"
  V("我") ≈ [第一人称, 人称代词, ...]    ← 丰富的"内容向量"，编码"我有什么含义"

token "爱":
  K("爱") ≈ [动词标志, 情感标志, ...]   ← 标签：我是动词类，宾语Q应该找我
  V("爱") ≈ [喜爱, 情感动作, ...]       ← 内容：被选中后提供"喜爱"的语义
```

K追求**紧凑区分性**（名词K vs 动词K差异大，方便路由）；V追求**丰富语义性**（同一个词的V包含多方面含义）。这两个需求矛盾：路由要维度少且差异大，内容要维度多且细粒度。分开后各得其所。

**直觉类比：** K像"餐厅招牌"（"正宗川菜"），吸引特定顾客走进来；V像"菜品"（水煮鱼、麻婆豆腐），顾客进来后实际吃到的东西。招牌优化目标：吸引爱吃川菜的顾客（路由准确性）；菜品优化目标：让顾客吃完满意（内容质量）。

#### Q和V的关系：需求的"口味偏好"

Q和V没有直接交互（Q不与V做点积），但通过注意力权重间接关联：

$$\text{output}_i = \sum_j \alpha(Q_i, K_j) \cdot V_j$$

Q决定从哪些位置取V（路由），但不直接约束取到什么V。然而，训练过程中Q和V会形成隐式配合：

- Q学会只关注"能提供有用V的位置"——如果某个位置的V总是提供噪声，Q会学会降低对它的注意力
- V学会提供"被Q选中后有价值的内容"——如果一个位置的V从未被任何Q选中，它的梯度几乎为零（参数更新停滞）

类比：招聘者(Q)会学会只面试有实力的候选人(V)，而不会浪费时间面试能力差的候选人。

#### Q、K、V三者的动态博弈

把三者放在一起看，整个训练过程是一个**三方协同博弈**：

```
训练初期：
  Q: 随机搜索，不知道找谁       → 注意力均匀分布
  K: 随机标签，不知道被谁找     → 无法有效路由
  V: 随机内容，不知道该提供什么 → 输出质量差

训练中期：
  Q→K: Q学会向"有用的K"靠近     → 注意力开始集中
  K→Q: K学会向"需要我的Q"靠近   → 路由逐渐准确
  K→V: K锁定目标客户            → V知道该服务谁
  V→Q: V提供有价值的内容        → Q学会只关注有用位置

训练后期：
  Q、K、V协同收敛：
  Q精确知道找谁(K) → K精确知道被谁找(Q) → V精确知道该提供什么
  形成稳定的信息流通路：Q×K建路，V在路上传货
```

**一句话总结：Q是"我要什么"，K是"我是谁"，V是"我有什么"。Q和K握手建通道，V在通道里传内容。三者梯度路径不同，自然分化，无需外力干预。**

### 2.2 数学公式

**缩放点积注意力（Scaled Dot-Product Attention）：**

$$
\text{Attention}(Q, K, V) = \text{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right) V
$$

- $Q \in \mathbb{R}^{n \times d_k}$，$K \in \mathbb{R}^{m \times d_k}$，$V \in \mathbb{R}^{m \times d_v}$
- $d_k$ 为每个注意力头中 Key/Query 的维度，除以 $\sqrt{d_k}$ 是为了防止点积值过大导致 softmax 梯度消失
- $n$ 为查询序列长度，$m$ 为键/值序列长度

**关键维度参数的含义：**

| 参数 | 含义 | 原论文值 | 选择逻辑 |
|------|------|---------|---------|
| $d_{\text{model}}$ | 所有层的向量维度 | 512 | 模型容量：越大表达力越强，但参数量和计算量也越大 |
| $n_{\text{heads}}$ | 注意力头数 | 8 | 多视角：越多视角越丰富，但每个头表达力越弱 |
| $d_k$ | 每个头的Key/Query维度 | 64 | 由 $d_{\text{model}}/n_{\text{heads}}$ 决定，不是独立选择 |

**$d_{\text{model}}$（模型维度）：** Transformer中所有层的输入/输出的向量维度。它是整个模型的"通用货币"——Embedding输出512维，Encoder每层输入输出512维，Decoder每层输入输出512维。所有子层的输入输出都是同一个维度，只有FFN中间层临时扩展到2048维。为什么所有层都用同一个维度？因为**残差连接要求维度一致**：`x + SubLayer(x)` 要求 `x` 和 `SubLayer(x)` shape完全相同才能相加。如果不同层维度不同，残差连接就无法实现。

**$n_{\text{heads}}$（注意力头数）：** 多头注意力中并行计算的注意力子空间数量。每个头独立在自己的 $d_k$ 维子空间里计算注意力，8个头相当于模型同时用8个"视角"观察序列。

**$d_k$ 是怎么决定的？**

$d_k$ 不是独立选择的，而是由 $d_{\text{model}}$ 和 $n_{\text{heads}}$ 的硬约束决定：

$$n_{\text{heads}} \times d_k = d_{\text{model}} \implies d_k = \frac{d_{\text{model}}}{n_{\text{heads}}}$$

必须保证 $d_{\text{model}}$ 能被 $n_{\text{heads}}$ 整除，否则 $d_k$ 不是整数，无法均分。512维向量要均分成8个头，每个头拿64维。如果不能整除（如512分成7个头），就无法均分，有的头拿73维有的拿72维，实现困难且不对称。

整个多头注意力的流程决定了这个约束：

```
输入 x: [d_model] 维
  → W_Q投影: [d_model] → [d_model]  (一次性投影所有头)
  → split成n_heads个头: [d_model] → n_heads × [d_k]   ← 每个头拿d_k维
  → 每个头独立计算注意力: [d_k]维的Q与[d_k]维的K → 输出[d_v]维
  → concat所有头: n_heads × [d_v] → [n_heads × d_v]
  → W_O投影: [n_heads × d_v] → [d_model]   ← 必须回到d_model!
```

最后一行要求：$n_{\text{heads}} \times d_v = d_{\text{model}}$。原论文中 $d_v = d_k$，所以 $n_{\text{heads}} \times d_k = d_{\text{model}}$。

$d_{\text{model}}$、$n_{\text{heads}}$、$d_k$ 三者互相约束：

- $d_{\text{model}}$ 太小 → 模型表达力不足
- $n_{\text{heads}}$ 太多 → $d_k$ 太小 → 每个头表达力不足
- $n_{\text{heads}}$ 太少 → $d_k$ 太大 → 退化为单头，失去多视角优势

| 方案 | 头数 | $d_k$ | 效果 |
|------|------|-------|------|
| 单头注意力 | 1 | 512 | 一个大空间，所有模式混在一起 |
| 8头注意力 | 8 | 64 | 8个小空间，不同头学不同模式 |
| 16头注意力 | 16 | 32 | 太小，每个头表达力不足 |

原论文实验了 $n_{\text{heads}}=8, d_k=64$ 和 $n_{\text{heads}}=1, d_k=512$，发现8头效果更好，确定了这个组合。

**计算步骤拆解：**
1. $QK^T$：计算 Query 与所有 Key 的相似度（点积），得到 $n \times m$ 的分数矩阵
2. $\div \sqrt{d_k}$：缩放，防止分数过大
3. $\text{softmax}$：归一化为概率分布（每行和为1）
4. $\times V$：用注意力权重对 Value 加权求和

**数值示例（d_k=2, 序列长度=3）：**

假设输入序列有3个token，经过投影后得到：
```
Q = [[1, 0], [0, 1], [1, 1]]   # 3个查询向量
K = [[1, 0], [0, 1], [1, 1]]   # 3个键向量
V = [[10, 0],  [0, 10], [5, 5]] # 3个值向量
```

步骤1：$QK^T$ 计算点积：
```
scores = Q @ K^T = [[1*1+0*0, 1*0+0*1, 1*1+0*1],    = [[1, 0, 1],
                    [0*1+1*0, 0*0+1*1, 0*1+1*1],      [0, 1, 1],
                    [1*1+1*0, 1*0+1*1, 1*1+1*1]]       [1, 1, 2]]
```

步骤2：除以 $\sqrt{d_k} = \sqrt{2} \approx 1.414$：
```
scaled = [[0.707, 0,     0.707],
          [0,     0.707, 0.707],
          [0.707, 0.707, 1.414]]
```

步骤3：softmax归一化（每行）：
```
weights ≈ [[0.34, 0.17, 0.34],   # 第1个token: 主要关注自己和第3个
           [0.17, 0.34, 0.34],   # 第2个token: 主要关注自己和第3个
           [0.21, 0.21, 0.42]]   # 第3个token: 最关注自己(点积最大)
```

步骤4：对V加权求和：
```
output[0] = 0.34*[10,0] + 0.17*[0,10] + 0.34*[5,5] ≈ [5.1, 2.5]
output[1] = 0.17*[10,0] + 0.34*[0,10] + 0.34*[5,5] ≈ [2.5, 5.1]
output[2] = 0.21*[10,0] + 0.21*[0,10] + 0.42*[5,5] ≈ [4.2, 4.2]
```

**为什么除以 $\sqrt{d_k}$？——梯度消失的数学推导：**

假设 $q$ 和 $k$ 的每个元素独立、均值为0、方差为1，则点积 $q \cdot k = \sum_{i=1}^{d_k} q_i k_i$ 的均值为0、方差为 $d_k$（因为独立变量乘积的方差之和）。

当 $d_k$ 很大时（如64），点积的方差约为64，值集中在 $\pm 8$ 附近。softmax 函数 $\sigma(x_i) = \frac{e^{x_i}}{\sum_j e^{x_j}}$ 在输入值很大时进入饱和区：

$$\frac{\partial \sigma(x_i)}{\partial x_i} = \sigma(x_i)(1 - \sigma(x_i))$$

当某个 $x_i \gg x_j$ 时，$\sigma(x_i) \approx 1$，梯度 $\approx 1 \times (1-1) = 0$ —— **梯度消失**！

除以 $\sqrt{d_k}$ 后，点积方差从 $d_k$ 降为1，值集中在 $\pm 1$ 附近，softmax梯度保持正常范围（最大值约0.25），训练稳定。

> **直觉：** 假设4个考生考同一份试卷，原始分数如下：
>
> | 考生 | 原始分数（未缩放） | softmax后的"录取概率" | 缩放后分数（÷√d_k） | softmax后的"录取概率" |
> |------|-------------------|----------------------|--------------------|----------------------|
> | A | 800 | 99.6% | 100 | 33.3% |
> | B | 400 | 0.2% | 50 | 16.7% |
> | C | 300 | 0.1% | 37.5 | 11.7% |
> | D | 200 | 0.1% | 25 | 38.3% |
>
> 未缩放时：A碾压其他人（99.6%），B/C/D之间的差异完全被淹没（0.2% vs 0.1%几乎无区别）。softmax看到的是"A遥遥领先，其他人都差不多"。
>
> 缩放后：概率分布更均匀（33%→16%→11%→38%），每个考生之间的差异都能被感知。softmax能区分"B比C稍好"、"D也不错"等细微差异。
>
> 对训练的影响：未缩放时，模型无法通过调整注意力来强调B而非C（因为两者概率几乎相同，调整权重的梯度≈0）；缩放后，模型能精确控制注意力分配（梯度≈0.17~0.24），训练有效。

### 2.3 PyTorch 实现

```python
import torch
import torch.nn as nn
import torch.nn.functional as F
import math

class ScaledDotProductAttention(nn.Module):
    """缩放点积注意力机制

    公式: Attention(Q, K, V) = softmax(Q @ K^T / sqrt(d_k)) @ V

    核心作用: 让每个查询位置(Query)自主决定"应该关注哪些键位置(Key)",
    然后从这些键位置对应的值(Value)中提取信息, 加权求和作为输出.

    三种角色类比:
    - Query:  "我想要什么信息?" (主动方, 发起查询)
    - Key:    "我有什么信息?"   (被动方, 提供索引标签)
    - Value:  "我的具体内容"    (被选中后, 提供实际数据)

    匹配过程: Q和K的点积衡量"需求与标签的匹配程度",
    匹配度越高, 该位置的V在输出中权重越大.

    Args:
        dropout: 注意力权重的dropout概率, 用于正则化防止过拟合
                 训练时随机置零部分注意力权重, 强制模型不过度依赖少数位置
                 推理时dropout自动关闭, 所有权重完整保留
    """

    def __init__(self, dropout=0.1):
        super().__init__()
        # dropout层, 在注意力权重上随机置零, 增强泛化能力
        # 注意: dropout作用于softmax之后的概率分布上, 而不是原始分数上
        # 这意味着被dropout的位置权重变为0, 剩余位置的权重不会自动重新归一化
        # (这与很多实现不同, 但原论文的做法就是这样, 效果相当)
        self.dropout = nn.Dropout(dropout)

    def forward(self, Q, K, V, mask=None):
        """前向传播

        数据流:
        Q,K,V → 点积 → 缩放 → mask → softmax → dropout → 加权求和 → 输出

        Args:
            Q: 查询矩阵, shape=[batch, n_heads, seq_len_q, d_k]
               每一行代表一个查询位置想寻找的信息模式
            K: 键矩阵,   shape=[batch, n_heads, seq_len_k, d_k]
               每一行代表一个键位置提供的信息标签
               seq_len_k可以与seq_len_q不同(如Cross-Attention时)
            V: 值矩阵,   shape=[batch, n_heads, seq_len_k, d_v]
               每一行代表一个键位置携带的实际内容
               d_v可以与d_k不同(但原论文中d_v=d_k)
            mask: 掩码矩阵, shape=[batch, 1, seq_len_q, seq_len_k] 或可广播的形状
                  0表示屏蔽该位置(设为-1e9, softmax后≈0)
                  1表示保留该位置(正常参与注意力计算)
                  两种用途:
                  1. Decoder的Masked Self-Attention: 防止看到未来位置(下三角)
                  2. Encoder/Decoder的padding mask: 屏蔽<PAD>位置

        Returns:
            output: 注意力输出, shape=[batch, n_heads, seq_len_q, d_v]
                    每个查询位置融合了所有(未被mask屏蔽的)值向量信息的加权平均
            attn_weights: 注意力权重, shape=[batch, n_heads, seq_len_q, seq_len_k]
                          可用于可视化模型"在看哪里", 也可用于后续分析
        """
        # 获取Key的维度d_k, 用于缩放因子
        # Q.size(-1) 取最后一个维度的大小, 即d_k
        # 例如 Q shape=[2, 8, 10, 64] → d_k=64
        d_k = Q.size(-1)

        # ===== 第一步: 计算Q和K的点积, 得到原始注意力分数 =====
        # Q: [batch, n_heads, seq_len_q, d_k]
        # K.transpose(-2, -1): 交换最后两个维度 → [batch, n_heads, d_k, seq_len_k]
        #   即把K从"每个位置一个d_k维向量"变成"每个d_k维特征一个seq_len_k维向量"
        #   这是矩阵乘法所需的标准转置操作
        # matmul结果: [batch, n_heads, seq_len_q, seq_len_k]
        # scores[i,j,m,n] 的含义:
        #   第i个batch、第j个注意力头中,
        #   第m个查询位置的Q向量与第n个键位置的K向量之间的相似度分数
        #   值越大 = 越匹配 = 该键位置对查询越重要
        #
        # 数学含义: Q[m,:] · K[n,:] = Σ_{l=0}^{d_k-1} Q[m,l] * K[n,l]
        #   这是两个d_k维向量的内积, 衡量它们的方向一致性
        #   内积越大 → 两个向量方向越接近 → 匹配度越高
        #
        # 为什么用点积而不是其他相似度度量(如余弦相似度、欧氏距离)?
        #   点积的计算效率最高(一次matmul即可得到所有pair的分数)
        #   且在d_k维度归一化后(通过缩放), 效果与余弦相似度相近
        #   论文中也对比了加性注意力(additive attention), 发现点积效果相当但更快
        scores = torch.matmul(Q, K.transpose(-2, -1))

        # ===== 第二步: 缩放 —— 除以sqrt(d_k) =====
        # 这是"Scaled Dot-Product Attention"中"Scaled"的含义
        #
        # 为什么要缩放? 数学推导:
        #   假设Q和K的元素 q_i, k_i 独立分布, 均值E[q_i]=E[k_i]=0, 方差Var[q_i]=Var[k_i]=1
        #   则点积 q·k = Σ q_i*k_i 的:
        #     均值 E[q·k] = Σ E[q_i]E[k_i] = 0
        #     方差 Var[q·k] = Σ Var[q_i*k_i] = Σ E[q_i²]E[k_i²] - (E[q_i]E[k_i])²
        #                    = Σ (1²)(1²) = d_k    (因为Var[q_i]=E[q_i²]-E[q_i]²=1)
        #   所以点积的标准差 = sqrt(d_k)
        #
        #   当d_k=64时, 点积值大部分落在[-8, 8]范围(约2个标准差)
        #   softmax在这些大值上几乎饱和: softmax([8, -4, -4]) ≈ [0.996, 0.002, 0.002]
        #   对最大值位置的梯度 ≈ 0.996 * (1-0.996) ≈ 0.004 → 接近0, 梯度消失!
        #
        #   除以sqrt(d_k)=8后, 值变为[-1, -0.5, -0.5]
        #   softmax([-1, -0.5, -0.5]) ≈ [0.21, 0.39, 0.39]
        #   对各位置的梯度 ≈ 0.21*0.79, 0.39*0.61 → 0.17, 0.24 → 正常范围!
        #
        #   缩放后, 点积方差从d_k变为1, 值集中在[-1,1]附近, softmax保持敏感
        scores = scores / math.sqrt(d_k)

        # ===== 第三步: 应用掩码(如果提供) =====
        # mask中值为0的位置, 将scores中对应位置替换为-1e9(接近负无穷)
        # softmax(-1e9) ≈ e^{-1e9} / (e^{-1e9} + Σ e^{正常值}) ≈ 0
        # 效果: 被mask的位置在注意力权重中占比几乎为0, 不影响输出
        #
        # 为什么用-1e9而不是-inf?
        #   某些softmax实现对-inf可能产生NaN(0/0的情况)
        #   -1e9足够小, softmax后≈0, 但避免了数值问题
        #
        # mask的两种典型场景:
        #   1. 下三角mask(Decoder Self-Attention):
        #      确保位置t只能看到位置≤t, 不能"偷看"未来
        #      例如 seq_len=4时:
        #      [[1,0,0,0],[1,1,0,0],[1,1,1,0],[1,1,1,1]]
        #   2. padding mask(Encoder/Decoder中):
        #      屏蔽<PAD>token, 不让模型关注这些无意义的位置
        #      例如 src=[5,3,2,PAD,PAD] → mask=[[1,1,1,0,0]]
        if mask is not None:
            scores = scores.masked_fill(mask == 0, -1e9)

        # ===== 第四步: softmax归一化, 将分数转为概率分布 =====
        # 沿最后一个维度(seq_len_k)做softmax, 每行和为1
        # softmax公式: softmax(x_i) = exp(x_i) / Σ_j exp(x_j)
        #
        # attn_weights[i,j,m,:] 表示第m个查询位置对所有键位置的注意力概率分布
        # 概率之和 = Σ_n attn_weights[i,j,m,n] = 1 (softmax保证)
        #
        # 高权重位置 → 查询位置"强烈关注"该键位置 → 该位置的Value对输出贡献大
        # 低权重位置 → 查询位置"不太关注"该键位置 → 该位置的Value对输出贡献小
        #
        # dim=-1 表示沿最后一个维度(seq_len_k)归一化
        # 为什么不是其他维度? 因为每个查询位置独立地分配其对各键位置的注意力
        attn_weights = F.softmax(scores, dim=-1)

        # 对注意力权重应用dropout, 随机置零部分权重, 防止过拟合
        # 训练时: 以概率p随机将权重设为0, 剩余权重乘以1/(1-p)保持期望不变
        # 推理时: dropout自动关闭, 所有权重完整保留
        # 作用: 防止模型过度依赖少数特定位置, 增强泛化能力
        attn_weights = self.dropout(attn_weights)

        # ===== 第五步: 用注意力权重对V加权求和 =====
        # attn_weights: [batch, n_heads, seq_len_q, seq_len_k] — 注意力概率
        # V: [batch, n_heads, seq_len_k, d_v] — 值向量
        # output: [batch, n_heads, seq_len_q, d_v] — 加权平均结果
        #
        # 计算: output[i,j,m,:] = Σ_n attn_weights[i,j,m,n] * V[i,j,n,:]
        #   即: 第m个查询位置的输出 = 所有值向量的加权和
        #   权重 = 第m个查询位置对各键位置的注意力概率
        #
        # 直觉理解:
        #   如果注意力权重[0.7, 0.2, 0.1], 则输出主要由V[0]决定(70%),
        #   V[1]和V[2]只做小幅度修正(20%和10%)
        #   这就像"综合多位专家的意见, 但更信任与问题最相关的专家"
        #
        # 与RNN的信息传递对比:
        #   RNN: 信息必须沿序列逐步传递, 远距离信息会衰减
        #   Attention: 任意两个位置直接连接, 远距离信息一步到达, 不衰减
        output = torch.matmul(attn_weights, V)

        return output, attn_weights
```

---

## 3. 多头注意力 (Multi-Head Attention)

### 3.1 核心思想

将 Q、K、V 分别投影到多个不同的子空间（头），每个头独立计算注意力，最后将所有头的输出拼接并投影回原始维度。

**好处：** 不同头可以关注序列中不同类型的关系模式（如语法关系、语义关系、位置关系等），让模型同时从多个角度理解序列。

**为什么不直接用一个大注意力头？**

一个大头（$d_k = d_{\text{model}} = 512$）虽然能捕捉所有信息，但容易将不同模式的信息混杂在一起。8个小头（$d_k = 64$）各自学习不同的注意力模式，最后通过 $W^O$ 组合，相当于给了模型8个"视角"去观察序列，每个视角独立地决定"看哪里、看什么"。

**各头的典型关注模式（实际训练中观察到的现象）：**
- 头1-2：关注相邻位置（局部依赖，类似n-gram）
- 头3-4：关注句法关系（主语-谓语、修饰语-被修饰词）
- 头5-6：关注语义关系（同义词、反义词、上下位词）
- 头7-8：关注特定结构（标点符号、句首句尾）

### 3.2 数学公式

$$
\text{MultiHead}(Q, K, V) = \text{Concat}(\text{head}_1, ..., \text{head}_h) W^O
$$

$$
\text{head}_i = \text{Attention}(QW_i^Q, KW_i^K, VW_i^V)
$$

其中 $h$ 为头数，$d_k = d_v = d_{\text{model}} / h$。

### 3.3 PyTorch 实现

```python
class MultiHeadAttention(nn.Module):
    """多头注意力机制

    将Q/K/V投影到h个不同的子空间, 每个子空间独立计算注意力,
    最后拼接所有头的输出并做线性投影.

    整体流程:
    1. 线性投影: 将输入的d_model维向量分别投影为Q/K/V (各d_model维)
    2. 分头: 把d_model拆成 n_heads × d_k, 每个头处理d_k维的子空间
    3. 并行计算: 每个头独立做缩放点积注意力, 得到 d_v维输出
    4. 拼接: 把所有头的输出拼接成 n_heads × d_v = d_model维
    5. 输出投影: 通过W_O将拼接结果映射回d_model维

    参数量分析:
    - W_Q, W_K, W_V: 各 d_model × d_model = 3 × 512² = 786,432
    - W_O: d_model × d_model = 512² = 262,144
    - 总计: 1,048,576 (与单头注意力 d_model×d_model×4 相同!)
    → 多头注意力的参数量并不比单头多, 只是把参数分配到了不同的子空间

    Args:
        d_model: 模型的特征维度 (如512)
        n_heads: 注意力头数 (如8), 要求 d_model 能被 n_heads 整除
        dropout: dropout概率
    """

    def __init__(self, d_model, n_heads, dropout=0.1):
        super().__init__()
        # 确保d_model能被头数整除, 否则每个头的维度不是整数
        # 例如 d_model=512, n_heads=8 → d_k=64 ✓
        # 例如 d_model=512, n_heads=7 → d_k≈73.14 ✗ (会报错)
        assert d_model % n_heads == 0

        # 每个头的Key/Value维度
        # d_model被平均分配给每个头: d_k = d_model / n_heads
        # 例如 512/8=64, 每个头处理64维的子空间
        # 这保证了: 拼接后 n_heads × d_k = 8 × 64 = 512 = d_model
        self.d_k = d_model // n_heads
        # 头数, 决定了模型能同时关注多少种不同的关系模式
        self.n_heads = n_heads

        # Q/K/V的线性投影层, 将d_model维映射到d_model维
        # 实际上是对所有头一起投影, 后面再split成多头
        # 这等价于对每个头分别做 d_model → d_k 的投影:
        #   W_Q_all 可以看作 n_heads 个 W_Q_i 的拼接:
        #   W_Q_all = [W_Q_1; W_Q_2; ...; W_Q_h], 每个 W_Q_i 是 d_model × d_k
        #   x @ W_Q_all 得到 d_model维向量, 再split成 h个 d_k维向量
        #   这比分别做h次投影更高效(一次matmul代替h次)
        self.W_Q = nn.Linear(d_model, d_model)  # Query投影: 输入→查询空间
        self.W_K = nn.Linear(d_model, d_model)  # Key投影: 输入→键空间
        self.W_V = nn.Linear(d_model, d_model)  # Value投影: 输入→值空间

        # 输出投影层, 将拼接后的多头输出(n_heads × d_k = d_model)映射回d_model
        # 这一步让模型学习如何组合不同头的信息:
        #   W_O的作用不是简单的维度变换, 而是"融合决策"
        #   每个头的输出包含一种关系模式的信息, W_O决定各模式的重要性
        self.W_O = nn.Linear(d_model, d_model)

        # 缩放点积注意力计算模块
        # 这里复用同一个ScaledDotProductAttention实例
        # 因为所有头共享相同的缩放因子和dropout设置
        # matmul会自动对batch和n_heads维度做并行计算
        self.attention = ScaledDotProductAttention(dropout)
        # 输出的dropout, 在W_O投影之后应用
        self.dropout = nn.Dropout(dropout)

    def forward(self, Q, K, V, mask=None):
        """前向传播

        Args:
            Q: 查询输入, shape=[batch, seq_len_q, d_model]
               Self-Attention时: Q=K=V=上一层的输出x
               Cross-Attention时: Q=Decoder的输出, K=V=Encoder的输出
            K: 键输入,   shape=[batch, seq_len_k, d_model]
               Self-Attention时: 与Q相同
               Cross-Attention时: Encoder的输出
            V: 值输入,   shape=[batch, seq_len_k, d_model]
               Self-Attention时: 与K相同
               Cross-Attention时: 与K相同
            mask: 掩码, shape=[batch, 1, seq_len_q, seq_len_k]
                  通过unsqueeze扩展到多头维度后应用于所有头

        Returns:
            output: 多头注意力输出, shape=[batch, seq_len_q, d_model]
                    融合了所有头的注意力信息
            attn_weights: 注意力权重, shape=[batch, n_heads, seq_len_q, seq_len_k]
                          注意: 这里返回的是经过分头reshape后的中间状态
                          包含所有头的权重, 可用于可视化每个头的注意力模式
        """
        batch_size = Q.size(0)

        # ====== 第一步: 线性投影 + 分头 ======
        # 这一步把d_model维的输入变换为 n_heads 个独立的子空间表示
        #
        # 以Q为例, 详细拆解reshape过程:
        # 原始Q: [batch, seq_len, d_model]
        #   ↓ self.W_Q(Q): 线性投影
        # 投影后: [batch, seq_len, d_model]
        #   ↓ .view(batch, seq_len, n_heads, d_k): 重塑形状
        #   含义: 把d_model维度拆成 n_heads × d_k
        #         即把一个大向量拆成h个小向量, 每个小向量对应一个注意力头
        # reshaped: [batch, seq_len, n_heads, d_k]
        #   ↓ .transpose(1, 2): 交换seq_len和n_heads维度
        #   含义: 把"n_heads"维度放到前面, 方便并行计算
        #         PyTorch的matmul会自动对batch和n_heads维度做batched矩阵乘法
        # 最终: [batch, n_heads, seq_len, d_k]
        #
        # 为什么transpose而不是直接reshape为[batch, n_heads, seq_len, d_k]?
        #   因为view要求内存连续, 而d_model → n_heads×d_k的拆分是沿最后一个维度
        #   view(..., n_heads, d_k) 后再transpose(1,2) 才能得到正确的排列
        #   直接view为[batch, n_heads, seq_len, d_k]会导致数据错乱
        #
        # 形状变化总结:
        #   [batch, seq_len, d_model] → [batch, seq_len, n_heads, d_k] → [batch, n_heads, seq_len, d_k]
        #   例如: [2, 10, 512] → [2, 10, 8, 64] → [2, 8, 10, 64]
        Q = self.W_Q(Q).view(batch_size, -1, self.n_heads, self.d_k).transpose(1, 2)
        K = self.W_K(K).view(batch_size, -1, self.n_heads, self.d_k).transpose(1, 2)
        V = self.W_V(V).view(batch_size, -1, self.n_heads, self.d_k).transpose(1, 2)

        # mask扩展到多头维度: [batch, 1, seq_len_q, seq_len_k] → [batch, n_heads, seq_len_q, seq_len_k]
        # unsqueeze(1): 在dim=1(即batch后)插入一个维度, 值为1
        # 通过广播机制: [batch, 1, seq_len_q, seq_len_k] → [batch, n_heads, seq_len_q, seq_len_k]
        # 效果: 所有头使用相同的mask(不同头关注不同的位置模式, 但都不能看被mask的位置)
        # 为什么所有头共享同一个mask?
        #   mask定义的是"哪些位置不可见"(如padding位置、未来位置),
        #   这是数据的客观约束, 不应该因注意力头不同而改变
        if mask is not None:
            mask = mask.unsqueeze(1)

        # ====== 第二步: 计算每个头的注意力 ======
        # 所有头的注意力计算在一次matmul调用中并行完成
        # PyTorch的matmul自动处理batch和n_heads维度:
        #   对每个(b, h)组合, 独立做 seq_len_q × d_k 的Q 与 d_k × seq_len_k 的K^T 的矩阵乘法
        # 这就是多头注意力的并行优势: 不需要for循环遍历每个头
        #
        # 注意: 这里使用的d_k(=64)比单头的d_model(=512)小8倍
        #   所以虽然头数×8, 但每个头的计算量是单头的1/8
        #   总计算量不变, 只是分散到了更多的小计算上
        attn_output, attn_weights = self.attention(Q, K, V, mask)
        # attn_output: [batch, n_heads, seq_len_q, d_k]
        #   例如 [2, 8, 10, 64] — 8个头各自输出64维的结果

        # ====== 第三步: 拼接多头输出 ======
        # 将h个头的输出从"分离"状态合并为"整体"状态
        #
        # 详细拆解:
        # 1. transpose(1,2): [batch, n_heads, seq_len, d_k] → [batch, seq_len, n_heads, d_k]
        #    把n_heads维度从第2位移回第3位, 让序列长度维度靠前
        # 2. contiguous(): 确保内存连续存储
        #    transpose只改变了视图(stride), 不改变底层内存布局
        #    view操作要求内存连续, 所以需要先contiguous()重新排列内存
        # 3. view(batch, seq_len, n_heads*d_k): [batch, seq_len, n_heads, d_k] → [batch, seq_len, d_model]
        #    把n_heads和d_k合并为一个维度: 8 × 64 = 512 = d_model
        #    效果: 每个位置有d_model维的向量, 包含了所有头的信息
        #
        # 为什么先transpose再view, 而不是直接view?
        #   因为当前内存布局是[batch, n_heads, seq_len, d_k],
        #   直接view为[batch, seq_len, d_model]会打乱数据顺序
        #   先transpose把维度排列为[batch, seq_len, n_heads, d_k],
        #   再contiguous()确保内存按这个顺序存储,
        #   最后view合并n_heads和d_k才正确
        attn_output = attn_output.transpose(1, 2).contiguous().view(
            batch_size, -1, self.n_heads * self.d_k
        )

        # ====== 第四步: 最终线性投影 ======
        # W_O: [d_model, d_model] → [n_heads*d_k, d_model] 的线性变换
        # 作用: 学习如何最优地组合h个头的信息
        #   不是简单的拼接求和, 而是通过可学习的权重矩阵做加权组合
        #   每个输出维度可以关注不同头的不同维度, 实现"交叉融合"
        #
        # 与Concat的关系:
        #   Concat只是把h个64维向量首尾相接变成512维向量
        #   W_O进一步对这个512维向量做线性变换, 让模型学习最优的融合方式
        output = self.W_O(attn_output)

        return output, attn_weights
```

---

## 4. 位置编码 (Positional Encoding)

### 4.1 为什么需要位置编码？

自注意力机制本身是**位置无关**的（置换等变），同一组 Q/K/V 无论如何排列，注意力输出只是换了个排列顺序，每个token得到的表示向量不变。因此需要注入位置信息。

**详细证明：为什么"我爱你"和"你爱我"的自注意力输出一样？**

"我爱你"的embedding矩阵（无位置编码）：

```
X₁ = [e_我, e_爱, e_你]   ← 位置0=我, 位置1=爱, 位置2=你
```

"你爱我"的embedding矩阵：

```
X₂ = [e_你, e_爱, e_我]   ← 位置0=你, 位置1=爱, 位置2=我
```

自注意力的计算：`output_i = Σ_j softmax(Q_i · K_j / √d_k) · V_j`

对"我爱你"中"我"（位置0）的输出：

```
output_我(位置0) = Σ_j attn(Q(e_我), K_j, V_j)  其中 j遍历 {e_我, e_爱, e_你}
                  = α₀·V(e_我) + α₁·V(e_爱) + α₂·V(e_你)
```

对"你爱我"中"我"（位置2）的输出：

```
output_我(位置2) = Σ_j attn(Q(e_我), K_j, V_j)  其中 j遍历 {e_你, e_爱, e_我}
                  = α₂'·V(e_你) + α₁'·V(e_爱) + α₀'·V(e_我)
```

**关键：** softmax是对所有K做归一化，求和遍历的是**同一个集合** `{e_我, e_爱, e_你}`（只是顺序不同）。集合的元素相同 → Q·K的点积值只是换了位置 → softmax后的权重也只是换了位置 → 加权求和的结果**完全相同**。

即：

```
output_我(在"我爱你"的位置0) = output_我(在"你爱我"的位置2) ← 同一个向量!
output_爱(在"我爱你"的位置1) = output_爱(在"你爱我"的位置1) ← 同一个向量!
output_你(在"我爱你"的位置2) = output_你(在"你爱我"的位置0) ← 同一个向量!
```

三个token的输出向量**完全一样**，只是排列顺序不同：

```
"我爱你": [output_我, output_爱, output_你]
"你爱我": [output_你, output_爱, output_我]  ← 只是重排了一下
```

**作为"含义"来看**：这两组输出的多集（multiset）完全相同 `{output_我, output_爱, output_你}`，模型无法区分哪个是"我爱你"哪个是"你爱我"——它把两个句子当成了一回事。

这就是"置换等变性"(permutation equivariance)：`Permute(Input) → Permute(Output)`，每个token的表示只取决于"我是谁"和"序列中有哪些token"，不取决于"我在哪个位置"。所以模型失去了位置感知能力，把序列当成了**无序的词袋（bag of words）**。

加位置编码后：`e_我 → e_我 + PE(0)`，`e_你 → e_你 + PE(2)`，同一token在不同位置获得不同的向量，打破等变性，模型就能区分顺序了。

### 4.2 正弦余弦位置编码

$$
PE(pos, 2i) = \sin\left(\frac{pos}{10000^{2i/d_{\text{model}}}}\right)
$$

$$
PE(pos, 2i+1) = \cos\left(\frac{pos}{10000^{2i/d_{\text{model}}}}\right)
$$

**特点：**
- 对于任意固定偏移 $k$，$PE(pos+k)$ 可以表示为 $PE(pos)$ 的线性函数，使得模型可以学习相对位置关系
- 偶数维度用 sin，奇数维度用 cos
- 低维度频率高（捕捉局部位置），高维度频率低（捕捉全局位置），类似二进制编码

**为什么 sin/cos 编码能表达线性相对位置关系？**

利用三角恒等式，可以证明位置 $pos+k$ 的编码是位置 $pos$ 编码的线性变换：

$$\sin(pos+k) = \sin(pos)\cos(k) + \cos(pos)\sin(k)$$

$$\cos(pos+k) = \cos(pos)\cos(k) - \sin(pos)\sin(k)$$

因此，$PE(pos+k)$ 可以通过一个仅依赖于 $k$ 的线性变换 $M_k$ 从 $PE(pos)$ 得到：

$$PE(pos+k) = M_k \cdot PE(pos)$$

其中 $M_k$ 是一个由 $\cos(k\cdot\omega_i)$ 和 $\sin(k\cdot\omega_i)$ 构成的旋转矩阵（块对角矩阵，每块2×2）。这意味着模型只需学习一组线性变换权重，就能推断任意相对位置的关系 —— 这比直接学习每个绝对位置的表示更加高效和泛化。

**频率分布与"二进制编码"类比：**

```
维度0 (ω=1/10000^0 = 1):     sin(pos)周期=2π≈6.28    — 频率最高, 区分近邻位置
维度2 (ω=1/10000^{2/512}≈0.012): 周期≈523              — 中等频率
维度510 (ω=1/10000^{510/512}≈0.0001): 周期≈62832       — 频率最低, 区分远距离位置
```

类比二进制计数器：
```
位置 0: 000  → PE的低频维度变化慢(几乎不变), 高频维度变化快(每个位置不同)
位置 1: 001  → 最右位(bit0=高频)每次都翻转, 最左位(bit2=低频)很少翻转
位置 2: 010  → sin/cos编码的频率递减模式与此类似
位置 3: 011
位置 4: 100
```

每个维度相当于一个"时钟指针"，低维度(短周期)的指针转得快，区分细微位置差异；高维度(长周期)的指针转得慢，区分宏观位置差异。多个"时钟"组合起来，就能唯一地编码任意位置。

### 4.3 PyTorch 实现

```python
class PositionalEncoding(nn.Module):
    """正弦-余弦位置编码

    为每个位置生成固定的位置向量, 加到输入embedding上,
    让模型感知序列中每个token的位置信息.

    核心思想: 用不同频率的sin/cos函数为每个位置生成独特的编码向量
    - 低维度用高频sin/cos(周期短): 区分相邻位置的细微差异
    - 高维度用低频sin/cos(周期长): 区分远距离位置的宏观差异
    - 组合起来形成每个位置唯一的"指纹"

    为什么用固定编码而不是可学习的位置向量?
    1. 固定编码无需额外参数, 不增加训练负担
    2. sin/cos的线性关系特性(见上文推导)让模型更容易学习相对位置
    3. 固定编码可以处理任意长度的序列(只要不超过max_len)
       可学习编码只能处理训练时见过的长度范围
    4. 实际效果: 研究表明两者性能相近, 但固定编码更简洁

    Args:
        d_model: 模型特征维度 (如512)
        max_len: 支持的最大序列长度 (如5000)
                 预计算max_len个位置的编码, 超过此长度会报错
        dropout: dropout概率
    """

    def __init__(self, d_model, max_len=5000, dropout=0.1):
        super().__init__()
        # dropout层, 在(x+pe)的结果上随机置零部分位置编码信息
        # 作用: 防止模型过度依赖特定位置的编码信号, 增强鲁棒性
        # 效果: 偶尔"忘记"某个位置的位置信息, 强制模型从上下文推断位置
        self.dropout = nn.Dropout(dropout)

        # ====== 预计算位置编码矩阵 ======
        # 为什么预计算? 因为位置编码是固定的(不参与训练),
        # 预计算后存储在buffer中, 每次forward只需取用, 不重复计算
        # pe: [max_len, d_model], 每行是一个位置的编码向量
        #   pe[0] = 位置0的编码: [sin(0*ω₀), cos(0*ω₀), sin(0*ω₁), cos(0*ω₁), ...]
        #   pe[1] = 位置1的编码: [sin(1*ω₀), cos(1*ω₀), sin(1*ω₁), cos(1*ω₁), ...]
        #   pe[k] = 位置k的编码: [sin(k*ω₀), cos(k*ω₀), sin(k*ω₁), cos(k*ω₁), ...]
        pe = torch.zeros(max_len, d_model)

        # position: [max_len, 1], 每个位置的序号 0,1,2,...,max_len-1
        # unsqueeze(1)将1维向量变为2维列向量, 便于后续与div_term做外积
        # 例如 max_len=5000: position = [[0],[1],[2],...,[4999]]
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)

        # div_term: [d_model//2], 频率项(角频率ω_i)
        # 计算公式: ω_i = 1 / 10000^(2i/d_model)
        #
        # 对应公式中的 1/10000^{2i/d_model} 部分:
        #   PE(pos, 2i) = sin(pos / 10000^{2i/d_model})
        #             = sin(pos × ω_i)    其中 ω_i = 1/10000^{2i/d_model}
        #             = sin(pos × exp(-2i·ln(10000)/d_model))
        #
        # 数值稳定性考虑:
        #   直接计算 10000^(2i/d_model) 可能溢出(当i很大时底数很大)
        #   利用恒等式 1/a = exp(-ln(a)):
        #     1/10000^{2i/d_model} = exp(-2i/d_model × ln(10000))
        #   这样先计算对数(ln(10000)≈9.21), 再乘以系数, 最后取exp
        #   整个过程数值范围可控, 不存在溢出风险
        #
        # div_term的取值范围:
        #   i=0:   ω_0 = exp(0) = 1.0           (最高频率, 区分相邻位置)
        #   i=255: ω_255 = exp(-255×2×ln(10000)/512) ≈ 0.0001 (最低频率, 区分远距离)
        #   频率从1到0.0001, 跨越4个数量级, 覆盖了从局部到全局的位置信息
        div_term = torch.exp(
            torch.arange(0, d_model, 2).float() * (-math.log(10000.0) / d_model)
        )

        # 偶数维度(0,2,4,...)用sin编码
        # position * div_term: [max_len, d_model//2] — 外积运算
        #   每个元素 = pos × ω_i, 即公式中的 pos/10000^{2i/d_model}
        # pe[:, 0::2] 取所有行、偶数列(第0,2,4,...列)
        # 结果: pe[pos, 2i] = sin(pos × ω_i) = sin(pos / 10000^{2i/d_model})
        pe[:, 0::2] = torch.sin(position * div_term)

        # 奇数维度(1,3,5,...)用cos编码
        # pe[:, 1::2] 取所有行、奇数列(第1,3,5,...列)
        # 结果: pe[pos, 2i+1] = cos(pos × ω_i) = cos(pos / 10000^{2i/d_model})
        # 注意: 奇偶维度使用相同的ω_i(频率), 只是分别用sin和cos
        # 这种sin/cos配对设计使得: PE(pos+k)可以通过旋转矩阵线性变换到PE(pos)
        pe[:, 1::2] = torch.cos(position * div_term)

        # 增加batch维度: [max_len, d_model] → [1, max_len, d_model]
        # 方便后续与输入 [batch, seq_len, d_model] 做广播加法:
        #   [1, max_len, d_model] + [batch, seq_len, d_model]
        #   → 广播到 [batch, seq_len, d_model]
        #   第1维(batch)通过广播复制到所有batch, 第2维(seq_len)通过切片取前seq_len个位置
        pe = pe.unsqueeze(0)

        # register_buffer: 将pe注册为模型的buffer(非参数)
        # 与nn.Parameter的区别:
        #   Parameter: 参与梯度更新, 会被optimizer调整
        #   Buffer: 不参与梯度更新, 是固定的常量数据
        # 两者的共同点:
        #   都会随模型一起保存(model.state_dict())和加载
        #   都会随model.to(device)一起移动到GPU
        # 为什么不直接用self.pe = pe?
        #   普通属性不会被state_dict记录, 模型保存/加载时pe会丢失
        #   register_buffer确保pe作为模型的一部分被持久化管理
        self.register_buffer('pe', pe)

    def forward(self, x):
        """前向传播: 将位置编码加到输入上

        Args:
            x: 输入embedding, shape=[batch, seq_len, d_model]
               经过nn.Embedding后的连续向量表示

        Returns:
            加上位置编码后的输出, shape=[batch, seq_len, d_model]
            x + pe 后每个token向量同时包含了语义信息(embedding)和位置信息(PE)

        数学含义:
            output[b, t, :] = embedding[b, t, :] + PE[t, :]
            即: 同一位置的不同batch共享相同的位置编码
            (位置编码是绝对位置, 不依赖batch内容)
        """
        # self.pe[:, :x.size(1), :] 取前seq_len个位置的编码
        # self.pe shape = [1, max_len, d_model]
        # x.size(1) = seq_len (当前输入的实际序列长度)
        # 切片: [1, seq_len, d_model] — 只取当前需要的部分, 不是全部max_len
        #
        # x + pe 利用广播机制:
        #   x: [batch, seq_len, d_model]
        #   pe: [1, seq_len, d_model]
        #   → pe的第1维(batch=1)广播到batch维度
        #   → 结果: [batch, seq_len, d_model]
        #   即每个batch都加上相同的位置编码
        x = x + self.pe[:, :x.size(1), :]

        # 应用dropout, 随机置零部分位置编码信息, 增强鲁棒性
        # dropout作用于x+pe的整体结果, 不只作用于pe
        # 这意味着偶尔会"忘记"某个token的embedding+位置信息
        # 强制模型不依赖单个位置, 而从上下文推断信息
        return self.dropout(x)
```

---

## 5. Encoder Block

### 5.1 结构

每个 Encoder Block 由两个子层组成：

```
输入 → Multi-Head Self-Attention → Add & Norm → Feed-Forward Network → Add & Norm → 输出
```

- **Add & Norm**：残差连接 + Layer Normalization，即 $LayerNorm(x + SubLayer(x))$
  - 残差连接：缓解深层网络梯度消失，让信息可以跨层传递
  - LayerNorm：对每个样本的特征维度做归一化，稳定训练
- **Feed-Forward Network**：两层线性变换 + ReLU，即 $FFN(x) = \max(0, xW_1 + b_1)W_2 + b_2$
  - 逐位置独立应用（不同位置共享参数，但各自独立计算）
  - 中间维度 $d_{ff}$ 通常为 $4 \times d_{\text{model}}$，提供非线性变换能力

**为什么要残差连接(Add)?**

深层网络的核心问题：每经过一个子层，信息可能被"扭曲"或"稀释"。残差连接让原始输入 $x$ 直接跳过子层，与子层输出 $SubLayer(x)$ 相加：

$$output = x + SubLayer(x)$$

好处：
1. **梯度直通**：反向传播时，梯度可以沿残差路径直接回传（$∂output/∂x = 1 + ∂SubLayer(x)/∂x$），至少有1的梯度保底，不会梯度消失
2. **信息保真**：即使子层学到了"无用变换"(SubLayer(x)≈0)，输出≈x，信息不会丢失 —— 子层只需学习"增量修正"
3. **训练稳定**：让深层网络(6层+)的训练变得可行，否则梯度在6次非线性变换后几乎消失

**为什么用LayerNorm而不是BatchNorm?**

| 特性 | BatchNorm | LayerNorm |
|------|-----------|-----------|
| 归一化维度 | 沿batch维度 | 沿feature维度 |
| 对batch大小敏感 | 是(小batch不稳定) | 否(每个样本独立) |
| 适用序列长度 | 固定长度 | 变长序列 |
| 隐藏状态依赖 | 依赖同batch其他样本 | 仅依赖当前样本 |

Transformer选择LayerNorm的原因：
1. 序列长度可变 → BatchNorm沿batch归一化时, 不同位置的统计量混合, 不合理
2. 推理时batch_size可能=1 → BatchNorm需要用训练时的running_mean/var, 不精确
3. 自注意力输出混合了所有位置的信息 → 每个位置的特征分布不同, 应独立归一化

### 5.2 PyTorch 实现

```python
class LayerNorm(nn.Module):
    """Layer Normalization

    对输入的最后一个维度做归一化:
    output = gamma * (x - mean) / (std + eps) + beta

    与BatchNorm不同, LayerNorm对每个样本独立归一化,
    不依赖batch中其他样本, 适合变长序列.

    归一化的作用:
    1. 稳定训练: 防止深层网络中特征值的分布随训练逐渐偏移(内部协变量偏移)
    2. 加速收敛: 让梯度保持在合理范围, 不因为特征值过大或过小而训练不稳定
    3. 不依赖batch: 每个样本独立归一化, 推理时(batch_size=1)行为与训练一致

    gamma和beta(可学习参数)的作用:
    - 归一化后所有特征均值0方差1, 丢失了原始分布信息
    - gamma/beta通过"缩放+偏移"恢复表达能力: 模型可以学出任意均值和方差
    - 初始化gamma=1, beta=0 → 初始时归一化后分布不变, 随训练逐步调整

    Args:
        d_model: 归一化的特征维度
        eps: 防止除零的小常数, 通常1e-6
             当std≈0时(如所有特征值相同), (x-mean)/(std+eps)≈0而不是inf
    """

    def __init__(self, d_model, eps=1e-6):
        super().__init__()
        # gamma: 缩放参数, shape=[d_model], 初始化为1
        # 可学习, 通过反向传播调整, 控制归一化后每个特征的"重要性"
        # gamma越大 → 该特征被放大 → 模型认为该特征更重要
        self.gamma = nn.Parameter(torch.ones(d_model))
        # beta: 偏移参数, shape=[d_model], 初始化为0
        # 可学习, 控制归一化后每个特征的"基准值"
        # beta非零 → 归一化后的特征不再以0为中心, 允许任意偏移
        self.beta = nn.Parameter(torch.zeros(d_model))
        # eps: 数值稳定项, 防止std=0时除零
        # 1e-6足够小, 不影响正常情况下的归一化精度
        self.eps = eps

    def forward(self, x):
        """前向传播

        Args:
            x: 输入, shape=[batch, seq_len, d_model]
               可能是Self-Attention或FFN的输出(经过残差连接后)

        Returns:
            归一化后的输出, shape不变
            每个位置的特征维度d_model被独立归一化

        计算步骤:
        1. mean: 每个位置(每个样本的每个token)的d_model维均值
        2. std: 每个位置(每个样本的每个token)的d_model维标准差
        3. 归一化: (x-mean)/(std+eps) → 均值0, 标准差≈1
        4. 仿射变换: gamma * 归一化结果 + beta → 可学习的均值和方差
        """
        # 沿最后一个维度(d_model)计算均值, keepdim=True保持维度便于广播
        # mean shape: [batch, seq_len, 1] — 每个token位置有一个均值
        # keepdim=True: 不压缩维度, 使mean可以与x做逐元素减法(广播)
        # 如果keepdim=False, mean=[batch, seq_len], 广播时需要unsqueeze, 不方便
        mean = x.mean(-1, keepdim=True)
        # 沿最后一个维度(d_model)计算标准差
        # std shape: [batch, seq_len, 1] — 每个token位置有一个标准差
        # 注意: PyTorch的std默认计算样本标准差(除以N-1), 不是总体标准差(除以N)
        # 这与论文中的做法略有不同, 但实际影响很小(当d_model=512时, 1/511≈1/512)
        std = x.std(-1, keepdim=True)
        # 归一化 + 仿射变换:
        # (x - mean) / (std + eps): 中心化+标准化 → 均值0, 标准差1
        # gamma * ... : 缩放 → 调整每个特征的"重要性/波动范围"
        # ... + beta : 偏移 → 调整每个特征的"基准值/中心位置"
        # 最终: 每个特征维度可以学出任意均值(beta)和任意标准差(gamma)
        return self.gamma * (x - mean) / (std + self.eps) + self.beta


class PositionwiseFeedForward(nn.Module):
    """位置前馈网络 (Position-wise Feed-Forward Network)

    对序列中每个位置独立应用相同的两层全连接网络:
    FFN(x) = W2 * ReLU(W1 * x + b1) + b2

    "位置独立"(Position-wise)的含义:
    - 不同token位置使用相同的W1,W2参数(共享权重)
    - 但每个位置独立计算, 不交换信息(不像Self-Attention那样跨位置)
    - 可以理解为: 用同一个1×1卷积核扫描序列的每个位置

    为什么需要FFN?
    - Self-Attention: 收集全局信息(跨位置交互), 但本质是加权平均 → 线性操作
    - FFN: 提供非线性变换能力, 对每个位置的信息做"深加工"
    - 两者互补: Attention负责"看哪里", FFN负责"看懂什么"

    为什么中间维度d_ff=4*d_model?
    - 高维中间层(2048 vs 512)提供更大的"记忆容量"
    - 类似KV存储: 先把信息展开到高维空间(存储), 再压缩回低维(检索)
    - 实验表明: d_ff太小(如2*d_model)性能下降, d_ff太大(如8*d_model)收益递减
    - 4倍是经验最佳值, 也是原论文的设置

    Args:
        d_model: 输入/输出维度 (如512)
        d_ff: 中间隐藏层维度 (如2048), 通常为4倍d_model
        dropout: dropout概率, 在ReLU激活后应用
    """

    def __init__(self, d_model, d_ff, dropout=0.1):
        super().__init__()
        # 第一层: d_model → d_ff, 扩展维度提取更丰富的特征
        # 相当于"编码器": 把512维的信息展开到2048维的"工作空间"
        # 在高维空间中, 不同特征更容易被分离和变换
        self.fc1 = nn.Linear(d_model, d_ff)
        # 第二层: d_ff → d_model, 压缩回原始维度
        # 相当于"解码器": 把2048维的变换结果压缩回512维, 保留最重要的信息
        self.fc2 = nn.Linear(d_ff, d_model)
        # dropout层, 在ReLU激活后应用, 防止过拟合
        # 位置: fc1 → ReLU → dropout → fc2
        # 为什么在ReLU后而不是fc1后? 
        #   ReLU会将约一半的神经元置零(负值→0), dropout在此基础上再随机置零
        #   双重稀疏化进一步减少过拟合风险
        self.dropout = nn.Dropout(dropout)

    def forward(self, x):
        """前向传播

        Args:
            x: 输入, shape=[batch, seq_len, d_model]
               每个位置是d_model维向量, 位置间独立处理

        Returns:
            输出, shape=[batch, seq_len, d_model]
               每个位置经过非线性变换后的d_model维向量

        内部形状变化:
            [batch, seq_len, d_model] → fc1 → [batch, seq_len, d_ff]
                                        → ReLU → [batch, seq_len, d_ff] (约一半变0)
                                        → dropout → [batch, seq_len, d_ff] (再随机置零)
                                        → fc2 → [batch, seq_len, d_model]
        """
        # fc1: [batch, seq_len, d_model] → [batch, seq_len, d_ff]
        # 线性变换到高维空间, 每个位置独立(不同位置用相同权重但不同输入)
        # ReLU: 非线性激活, max(0, x)
        #   作用1: 引入非线性(无ReLU则两层线性=一层线性, FFN退化为单层)
        #   作用2: 稀疏激活(约50%神经元输出为0), 提供类似"开关"的选择机制
        #   作用3: 计算高效(只需判断正负, 比sigmoid/tanh快)
        # dropout: 随机置零部分神经元, 防止过拟合
        # fc2: [batch, seq_len, d_ff] → [batch, seq_len, d_model]
        # 线性变换回原始维度, 压缩信息
        return self.fc2(self.dropout(F.relu(self.fc1(x))))


class EncoderLayer(nn.Module):
    """Encoder的一层 (一个Encoder Block)

    结构: Self-Attention → Add&Norm → FFN → Add&Norm

    数据流详解:
    x → MultiHeadAttention(Q=K=V=x, mask) → attn_output
    x + dropout(attn_output) → norm1 → x'        [残差连接+归一化]
    x' → FFN(x') → ff_output
    x' + dropout(ff_output) → norm2 → output      [残差连接+归一化]

    注意: 这里用的是Post-LN(先子层再归一化):
    output = LN(x + SubLayer(x))
    另一种变体是Pre-LN(先归一化再子层):
    output = x + SubLayer(LN(x))
    Pre-LN训练更稳定(梯度更平滑), 但Post-LN是原论文的做法

    Args:
        d_model: 模型特征维度
        n_heads: 注意力头数
        d_ff: FFN中间层维度
        dropout: dropout概率
    """

    def __init__(self, d_model, n_heads, d_ff, dropout=0.1):
        super().__init__()
        # 多头自注意力层: Q=K=V都来自上一层的输出x
        # "自"(Self)的含义: Query和Key/Value来自同一个序列
        # 每个位置可以关注序列中所有位置(包括自身), 获取全局上下文信息
        # 例如: 处理"我爱北京"时, "爱"可以同时关注"我"、"爱"、"北京"
        self.self_attn = MultiHeadAttention(d_model, n_heads, dropout)
        # 位置前馈网络: 对每个位置独立做非线性变换
        # 补充Self-Attention缺少的非线性变换能力
        self.ffn = PositionwiseFeedForward(d_model, d_ff, dropout)
        # 两个LayerNorm, 分别在Self-Attention和FFN之后
        # norm1: 归一化注意力子层的输出(经过残差连接后)
        # norm2: 归一化FFN子层的输出(经过残差连接后)
        # Pre-LN vs Post-LN: 这里用Post-LN(先子层再归一化), 原论文做法
        #   Post-LN: LN(x + SubLayer(x)) — 子层输出先与x相加, 再归一化
        #   Pre-LN:  x + SubLayer(LN(x))  — 先归一化x, 再送入子层
        #   区别: Pre-LN的子层输入始终归一化, 训练更稳定;
        #         Post-LN训练初期可能不稳定, 但最终性能可能更好(有争议)
        self.norm1 = LayerNorm(d_model)  # Self-Attention后的归一化
        self.norm2 = LayerNorm(d_model)  # FFN后的归一化
        # 子层输出的dropout, 在残差连接之前应用
        # dropout的位置: SubLayer(x) → dropout → + x → LayerNorm
        # 作用: 防止子层输出过强, 残差路径占主导时"稀释"子层的贡献
        # 原论文中dropout率=0.1, 即随机置零10%的子层输出
        self.dropout1 = nn.Dropout(dropout)
        self.dropout2 = nn.Dropout(dropout)

    def forward(self, x, mask=None):
        """前向传播

        Args:
            x: 输入, shape=[batch, seq_len, d_model]
               来自上一层EncoderLayer的输出, 或最底层来自Embedding+PE
            mask: 自注意力掩码, 用于屏蔽padding位置
                  shape=[batch, 1, 1, seq_len] 或可广播的形状
                  padding位置为0, 有效位置为1

        Returns:
            输出, shape=[batch, seq_len, d_model]
            每个位置融合了全局上下文信息(经过Attention)和非线性变换(经过FFN)
        """
        # ====== 子层1: Multi-Head Self-Attention + Add & Norm ======
        # 自注意力: Q=K=V=x, 每个位置关注序列中所有位置(包括自身)
        # 为什么Self-Attention是Encoder的核心?
        #   Encoder的任务是"理解源序列", 需要每个位置都能获取全局信息
        #   Self-Attention让每个位置直接"看"整个序列, 不像RNN逐步传递信息
        #
        # mask用于屏蔽padding位置, 避免attend到无意义的padding token
        # 例如: 源序列"我 爱 你 PAD PAD" → mask=[1,1,1,0,0]
        #   PAD位置的注意力分数设为-∞, 权重≈0, 不影响有效位置的计算
        attn_output, _ = self.self_attn(x, x, x, mask)
        # 残差连接 + dropout + LayerNorm
        # 残差: x + dropout(SubLayer(x)), 让梯度可以跳过子层直接回传
        # 数学: ∂(x+SubLayer(x))/∂x = 1 + ∂SubLayer(x)/∂x ≥ 1
        #   即使SubLayer的梯度很小, 至少有1的梯度保底, 不会梯度消失
        x = self.norm1(x + self.dropout1(attn_output))

        # ====== 子层2: Feed-Forward Network + Add & Norm ======
        # FFN: 对每个位置独立做非线性变换, 增强模型表达能力
        # 为什么需要FFN?
        #   Attention本质是加权求和(线性操作), 无法学习复杂的非线性映射
        #   FFN提供非线性变换(ReLU+两层MLP), 让模型能处理更复杂的模式
        #   类比: Attention是"收集信息", FFN是"消化理解"
        ff_output = self.ffn(x)
        # 残差连接 + dropout + LayerNorm (同子层1的结构)
        x = self.norm2(x + self.dropout2(ff_output))

        return x
```

---

## 6. Decoder Block

### 6.1 结构

每个 Decoder Block 由三个子层组成：

```
输入 → Masked Multi-Head Self-Attention → Add & Norm
     → Multi-Head Cross-Attention (用Encoder输出) → Add & Norm
     → Feed-Forward Network → Add & Norm → 输出
```

**关键区别：**
- **Masked Self-Attention**：用掩码防止看到未来位置（训练时确保自回归性质，即预测第t个位置只能看到1~t-1的位置）
- **Cross-Attention**：Query 来自 Decoder，Key 和 Value 来自 Encoder 输出 —— 这是 Decoder 获取源序列信息的方式

**三子层的详细作用：**

1. **Masked Self-Attention**：让Decoder的每个位置了解自己已经生成了什么内容
   - 类比：写作时回顾前面已经写的句子，确保续写内容与前面连贯
   - 为什么必须Masked？如果不屏蔽未来位置，模型就能"偷看"后面的答案，训练时不需要费力预测 —— 这就破坏了自回归训练的意义

2. **Cross-Attention**：让Decoder的每个位置获取源序列中最相关的信息
   - 类比：翻译时回头看原文，找到当前正在翻译的词对应的源语言词
   - Q来自Decoder（"我需要什么信息？"），K/V来自Encoder（"源序列有什么信息？"）
   - 这是Seq2Seq的核心：将源语言信息传递给目标语言的生成过程

3. **FFN**：对融合了源信息和目标历史信息的每个位置做非线性变换
   - 类比：消化吸收收集到的所有信息，做出最终的表达决策

### 6.2 掩码机制

```python
def generate_mask(seq_len):
    """生成下三角掩码矩阵, 用于Decoder的Masked Self-Attention

    掩码为下三角矩阵, 确保位置i只能attend到位置0~i(包括自身),
    不能attend到未来位置i+1~seq_len-1.

    为什么需要这个掩码?
    - Decoder是自回归模型: 预测第t个token时, 只能使用第0~t-1个token的信息
    - 如果不加掩码, 训练时第t个位置可以直接"看到"第t+1位置的ground truth
    - 这会让模型偷懒: 不需要预测, 直接复制未来位置的信息即可
    - 掩码强制模型真正学习预测能力: 只基于历史信息推断未来

    掩码在训练和推理中都起作用:
    - 训练: 输入是完整的目标序列(teacher forcing), 但掩码让每个位置只能看历史
    - 推理: 逐步生成, 自然只能看已生成的部分(掩码是冗余的安全保障)

    Args:
        seq_len: 序列长度

    Returns:
        mask: 掩码矩阵, shape=[1, 1, seq_len, seq_len]
              1表示可attend, 0表示屏蔽
    """
    # torch.tril: 取下三角部分(含对角线)
    # 例如 seq_len=4:
    # [[1, 0, 0, 0],    位置0只能看位置0 — 第1步生成, 只有起始符
    #  [1, 1, 0, 0],    位置1可以看位置0,1 — 第2步生成, 可以看起始符+第1个词
    #  [1, 1, 1, 0],    位置2可以看位置0,1,2 — 第3步生成
    #  [1, 1, 1, 1]]    位置3可以看位置0,1,2,3 — 第4步生成
    #
    # 每行代表一个查询位置, 每列代表一个键位置
    # 1=可以attend(历史位置), 0=屏蔽(未来位置)
    # 对角线=1: 位置可以attend自身(这是必要的, 因为自身包含已生成的信息)
    mask = torch.tril(torch.ones(seq_len, seq_len))

    # 增加batch和n_heads维度, 方便后续广播
    # [seq_len, seq_len] → [1, 1, seq_len, seq_len]
    # shape解释:
    #   dim0=1: batch维度(通过广播应用到所有batch)
    #   dim1=1: n_heads维度(通过广播应用到所有注意力头)
    #   dim2=seq_len: 查询序列长度(对应Q的行)
    #   dim3=seq_len: 键序列长度(对应K的列)
    mask = mask.unsqueeze(0).unsqueeze(0)

    return mask
```

**掩码示例（seq_len=4）：**

```
[[1, 0, 0, 0],
 [1, 1, 0, 0],
 [1, 1, 1, 0],
 [1, 1, 1, 1]]
```

位置 1 只能看到位置 1，位置 2 可以看到位置 1 和 2，以此类推。

### 6.3 PyTorch 实现

```python
class DecoderLayer(nn.Module):
    """Decoder的一层 (一个Decoder Block)

    结构:
    1. Masked Self-Attention → Add&Norm  (Decoder自身的历史信息)
    2. Cross-Attention → Add&Norm        (从Encoder获取源序列信息)
    3. FFN → Add&Norm                    (非线性变换)

    与EncoderLayer的关键区别:
    - 多了一个Cross-Attention子层(子层2), 这是Encoder-Decoder架构的核心
    - 子层1是Masked Self-Attention而非普通Self-Attention
    - 每个DecoderLayer都接收Encoder的最终输出(enc_output)作为参数

    Args:
        d_model: 模型特征维度
        n_heads: 注意力头数
        d_ff: FFN中间层维度
        dropout: dropout概率
    """

    def __init__(self, d_model, n_heads, d_ff, dropout=0.1):
        super().__init__()
        # Masked Self-Attention: Decoder自身的注意力, 带掩码防止看到未来
        # 与Encoder的Self-Attention完全相同的结构, 只是输入多了tgt_mask
        # 为什么需要Self-Attention? 让每个位置了解已生成的历史内容
        #   例如: 翻译"I love you"时, 生成"你"时需要看已经生成的"I love"
        self.masked_self_attn = MultiHeadAttention(d_model, n_heads, dropout)
        # Cross-Attention: Decoder关注Encoder的输出
        # Q来自Decoder(已解码的历史信息), K/V来自Encoder(源序列的编码信息)
        # 这是Encoder-Decoder架构的信息桥梁: 源→目的的信息传递
        # 为什么不把Encoder输出直接拼接给Decoder?
        #   因为不同目标位置需要关注不同的源位置(动态选择)
        #   Cross-Attention让每个目标位置自主决定"看源序列的哪个位置"
        self.cross_attn = MultiHeadAttention(d_model, n_heads, dropout)
        # 位置前馈网络, 与Encoder中完全相同
        self.ffn = PositionwiseFeedForward(d_model, d_ff, dropout)
        # 三个LayerNorm, 分别在三个子层之后
        self.norm1 = LayerNorm(d_model)  # Masked Self-Attention后
        self.norm2 = LayerNorm(d_model)  # Cross-Attention后
        self.norm3 = LayerNorm(d_model)  # FFN后
        # 三个子层的dropout
        self.dropout1 = nn.Dropout(dropout)
        self.dropout2 = nn.Dropout(dropout)
        self.dropout3 = nn.Dropout(dropout)

    def forward(self, x, enc_output, src_mask=None, tgt_mask=None):
        """前向传播

        Args:
            x: Decoder当前层的输入, shape=[batch, tgt_len, d_model]
               来自上一层DecoderLayer的输出, 或最底层来自Embedding+PE
            enc_output: Encoder的最终输出, shape=[batch, src_len, d_model]
               所有DecoderLayer共享同一个enc_output(Encoder只跑一次)
               注意: enc_output不随Decoder层数变化, 每层看到的源信息相同
            src_mask: 源序列掩码, shape=[batch, 1, 1, src_len] 或可广播
               用于Cross-Attention屏蔽Encoder中的padding位置
               防止Decoder关注源序列中无意义的PAD token
            tgt_mask: 目标序列掩码, shape=[batch, 1, tgt_len, tgt_len]
               下三角矩阵, 用于Masked Self-Attention屏蔽未来位置

        Returns:
            输出, shape=[batch, tgt_len, d_model]
            每个位置融合了: 历史目标信息(Self-Attn) + 源序列信息(Cross-Attn) + 非线性变换(FFN)
        """
        # ====== 子层1: Masked Self-Attention + Add & Norm ======
        # Q=K=V=x, Decoder的每个位置只能attend到自身及之前的位置
        # tgt_mask是下三角矩阵, 确保自回归性质(预测第t步只能用1~t-1的信息)
        # 为什么Decoder的Self-Attention需要mask而Encoder不需要?
        #   Encoder看到的是完整的源序列(所有位置都已知), 无需屏蔽
        #   Decoder看到的是目标序列(部分位置是"未来"的ground truth), 必须屏蔽
        attn_output, _ = self.masked_self_attn(x, x, x, tgt_mask)
        # 残差连接 + dropout + LayerNorm
        x = self.norm1(x + self.dropout1(attn_output))

        # ====== 子层2: Cross-Attention + Add & Norm ======
        # Q来自Decoder(当前已解码的信息): "我需要什么源信息?"
        # K和V来自Encoder的输出(源序列的编码信息): "源序列有什么信息?"
        # 作用: 让Decoder的每个位置去"查询"Encoder中最相关的源位置
        # 例如: 翻译"我 爱 你"时, 生成"I"时应该关注"我"对应的Encoder位置
        #
        # 为什么enc_output只传给K和V, 不传给Q?
        #   Q代表"查询意图"——由Decoder自主决定想查什么
        #   K是"索引标签"——由Encoder提供(源序列中每个位置的标签)
        #   V是"实际内容"——由Encoder提供(源序列中每个位置的含义)
        #   如果Q也来自Encoder, 就失去了"Decoder主动查询"的能力
        cross_output, _ = self.cross_attn(x, enc_output, enc_output, src_mask)
        # 残差连接 + dropout + LayerNorm
        x = self.norm2(x + self.dropout2(cross_output))

        # ====== 子层3: FFN + Add & Norm ======
        # 与Encoder中的FFN完全相同, 逐位置独立做非线性变换
        # 此时x已经融合了历史目标信息+源序列信息, FFN进一步做"深加工"
        ff_output = self.ffn(x)
        # 残差连接 + dropout + LayerNorm
        x = self.norm3(x + self.dropout3(ff_output))

        return x
```

---

## 7. 完整 Transformer 模型

### 7.1 PyTorch 完整实现

```python
class Encoder(nn.Module):
    """Transformer Encoder: N个EncoderLayer堆叠 + 最终LayerNorm

    整体流程: Embedding+PE → EncoderLayer_1 → EncoderLayer_2 → ... → EncoderLayer_N → Final LN

    每个EncoderLayer的输出shape不变([batch, src_len, d_model]),
    但内容逐层丰富: 底层捕捉局部关系, 高层捕捉更抽象的语义关系.

    为什么需要堆叠N层?
    - 1层: 只能捕捉一次全局交互, 关系建模能力有限
    - 6层: 每层在前一层的基础上进一步细化, 低层→高层形成层次化表示
    - 类比: 从像素→边缘→纹理→部件→对象, 逐层抽象

    为什么最后有一个额外的LayerNorm?
    - Post-LN架构中, 每个子层后都有LN, 但残差连接可能让输出偏离归一化范围
    - 最终LN确保Encoder输出的数值稳定性, 方便Decoder接收
    - 这不是原论文的做法(原论文没有最终LN), 但是常见的稳定训练变体

    Args:
        n_layers: Encoder层数 (原论文为6)
        d_model: 模型特征维度
        n_heads: 注意力头数
        d_ff: FFN中间层维度
        dropout: dropout概率
    """

    def __init__(self, n_layers, d_model, n_heads, d_ff, dropout=0.1):
        super().__init__()
        # 堆叠N个EncoderLayer, 使用ModuleList确保所有子模块被正确注册
        # 为什么用ModuleList而不是Python list?
        #   Python list中的模块不会被nn.Module自动注册 → parameters()不包含它们 → 无法训练
        #   ModuleList会将每个子模块注册到模型的_modules字典中 → 正确参与参数管理
        # 为什么不共享参数(同一层实例复用N次)?
        #   每层独立参数, 可以学习不同的表示: 底层学语法, 高层学语义
        #   共享参数会严重限制模型的表达能力
        self.layers = nn.ModuleList([
            EncoderLayer(d_model, n_heads, d_ff, dropout)
            for _ in range(n_layers)
        ])
        # 最终的LayerNorm, 在所有EncoderLayer之后
        # 注意: 原论文中每个子层后都有LN, 这里额外加一个最终LN是常见变体
        # 原论文没有最终LN, 但很多后续实现加上了, 因为:
        #   Post-LN的残差连接会让输出分布逐层偏移, 最终LN可以"重置"分布
        self.norm = LayerNorm(d_model)

    def forward(self, x, mask=None):
        """前向传播

        Args:
            x: Embedding+位置编码后的输入, shape=[batch, src_len, d_model]
            mask: 源序列掩码, 屏蔽padding位置
                  传递给每个EncoderLayer的Self-Attention

        Returns:
            Encoder最终输出, shape=[batch, src_len, d_model]
            每个位置的向量包含整个源序列的全局上下文信息(经过N层逐步融合)
        """
        # 依次通过N个EncoderLayer, 每层接收上一层的输出
        # x的shape始终不变([batch, src_len, d_model]), 但内容逐层丰富
        # 信息流动: 底层捕捉词-词的局部关系 → 高层捕捉句法/语义的全局关系
        # 类似CNN: 从边缘→纹理→部件→对象, 逐层提取更高级的特征
        for layer in self.layers:
            x = layer(x, mask)
        # 最终LayerNorm: 稳定输出分布, 确保Decoder接收到的数值在合理范围
        return self.norm(x)


class Decoder(nn.Module):
    """Transformer Decoder: N个DecoderLayer堆叠 + 最终LayerNorm

    整体流程: Embedding+PE → DecoderLayer_1(enc_out) → ... → DecoderLayer_N(enc_out) → Final LN

    关键特点:
    - 每个DecoderLayer都接收同一个enc_output(Encoder只跑一次)
    - 但不同层的Cross-Attention可以关注不同的源位置模式
    - 低层可能关注词级对应, 高层可能关注短语/句子级对应

    Args:
        n_layers: Decoder层数 (原论文为6)
        d_model: 模型特征维度
        n_heads: 注意力头数
        d_ff: FFN中间层维度
        dropout: dropout概率
    """

    def __init__(self, n_layers, d_model, n_heads, d_ff, dropout=0.1):
        super().__init__()
        # 堆叠N个DecoderLayer, 每层独立参数
        # 与Encoder相同, 使用ModuleList确保参数正确注册
        self.layers = nn.ModuleList([
            DecoderLayer(d_model, n_heads, d_ff, dropout)
            for _ in range(n_layers)
        ])
        # 最终LayerNorm, 与Encoder中的作用相同: 稳定输出分布
        self.norm = LayerNorm(d_model)

    def forward(self, x, enc_output, src_mask=None, tgt_mask=None):
        """前向传播

        Args:
            x: 目标序列的Embedding+位置编码, shape=[batch, tgt_len, d_model]
            enc_output: Encoder的输出, shape=[batch, src_len, d_model]
               注意: 所有DecoderLayer共享同一个enc_output
               Encoder只执行一次, 其输出被每层DecoderLayer的Cross-Attention复用
               这意味着不同Decoder层看到相同的源信息, 但各自的Cross-Attention
               可以学到不同的关注模式(不同层关注源序列的不同位置)
            src_mask: 源序列掩码 (用于Cross-Attention屏蔽Encoder中的padding)
            tgt_mask: 目标序列掩码 (用于Masked Self-Attention屏蔽未来位置)

        Returns:
            Decoder最终输出, shape=[batch, tgt_len, d_model]
            每个位置的向量融合了: 历史目标信息 + 源序列信息 + N层逐步精炼
        """
        # 依次通过N个DecoderLayer, 每层都接收Encoder的输出
        # enc_output在所有层间共享, 不随层数变化
        # 但每层的Cross-Attention会学到不同的源序列关注模式:
        #   低层: 关注词级对应(如"我"→"I")
        #   高层: 关注句子结构对应(如主语-谓语对齐)
        for layer in self.layers:
            x = layer(x, enc_output, src_mask, tgt_mask)
        # 最终LayerNorm
        return self.norm(x)


class Transformer(nn.Module):
    """完整的Transformer模型 (Encoder-Decoder架构)

    数据流:
    1. 源序列 → Embedding → ×√d_model → +位置编码 → Encoder → enc_output
    2. 目标序列 → Embedding → ×√d_model → +位置编码 → Decoder(enc_output) → dec_output
    3. dec_output → 线性投影 → vocab_size维logits → softmax → 概率分布

    关键设计决策:
    1. Embedding缩放(×√d_model): 使embedding量级与PE匹配(详见forward中注释)
    2. 权重共享: 论文中提议Embedding和proj共享权重, 此实现未采用(简化)
    3. Xavier初始化: 深层网络需要合理的初始化, 防止梯度消失/爆炸

    参数量估算(d_model=512, n_heads=8, n_layers=6, d_ff=2048):
    - Embedding: 2 × vocab_size × 512
    - 每个EncoderLayer: 4×512²(注意力) + 2×512×2048(FFN) + 4×512(LN) ≈ 2.5M
    - 每个DecoderLayer: 4×512²×2(两种注意力) + 2×512×2048(FFN) + 6×512(LN) ≈ 3.1M
    - Proj: 512 × vocab_size
    - 总计(不含Embedding/Proj): 6×2.5M + 6×3.1M ≈ 33.6M

    Args:
        src_vocab_size: 源语言词表大小
        tgt_vocab_size: 目标语言词表大小
        d_model: 模型特征维度 (默认512, 原论文值)
        n_heads: 注意力头数 (默认8, 原论文值)
        n_layers: Encoder/Decoder层数 (默认6, 原论文值)
        d_ff: FFN中间层维度 (默认2048, 原论文值)
        dropout: dropout概率 (默认0.1)
        max_len: 最大序列长度 (默认5000)
    """

    def __init__(self, src_vocab_size, tgt_vocab_size, d_model=512,
                 n_heads=8, n_layers=6, d_ff=2048, dropout=0.1, max_len=5000):
        super().__init__()
        # 保存d_model, 用于Embedding缩放(×√d_model)
        self.d_model = d_model

        # ====== Embedding层 ======
        # 将离散的token ID(整数)映射为d_model维的连续向量
        # nn.Embedding内部维护一个 [vocab_size, d_model] 的查找表
        # embedding(token_id) = 查找表[token_id], 即取出对应行的向量
        # 初始化: 默认用标准正态分布初始化, 均值0, 方差1/d_model(约0.002)
        # src_embed/tgt_embed不共享权重: 源语言和目标语言的语义空间不同
        # 例如: 英语的"the"和中文的"的"需要不同的向量表示
        self.src_embed = nn.Embedding(src_vocab_size, d_model)
        self.tgt_embed = nn.Embedding(tgt_vocab_size, d_model)

        # ====== 位置编码 ======
        # 源序列和目标序列分别使用独立的位置编码实例
        # (参数相同, 但各自维护pe buffer, 即各自的max_len个位置编码矩阵)
        # 为什么不共享同一个PE实例?
        #   共享也可以(因为PE是固定的, 不区分源/目标语言)
        #   但独立实例让代码更清晰, 且可以针对不同序列长度分别设置max_len
        self.src_pe = PositionalEncoding(d_model, max_len, dropout)
        self.tgt_pe = PositionalEncoding(d_model, max_len, dropout)

        # ====== Encoder & Decoder ======
        # Encoder: 将源序列编码为全局上下文表示
        # Decoder: 基于目标序列历史+源序列上下文, 预测下一个token
        self.encoder = Encoder(n_layers, d_model, n_heads, d_ff, dropout)
        self.decoder = Decoder(n_layers, d_model, n_heads, d_ff, dropout)

        # ====== 输出投影层 ======
        # 将Decoder的d_model维输出映射到目标词表大小
        # logits[i,j,k] 表示第i个batch、第j个位置、第k个词的原始分数
        # 后续通过softmax转为概率: P(词k) = softmax(logits[词k])
        # 取argmax得到预测token: pred_token = argmax(logits)
        #
        # 论文中的技巧: 可以将proj的权重与tgt_embed共享!
        #   因为两者都是 d_model → vocab_size 的映射
        #   共享权重减少参数量, 且logically consistent:
        #   Embedding将token映射到语义空间, proj将语义空间映射回token概率
        #   两者的方向相反, 但空间相同 → 共享权重是合理的
        #   此实现为简化未做权重共享
        self.proj = nn.Linear(d_model, tgt_vocab_size)

        # Xavier初始化权重, 使各层方差一致, 有助于深层网络训练
        # 为什么不在各层__init__中单独初始化?
        #   统一初始化方便管理, 且确保所有参数(包括Embedding)都被合理初始化
        #   Embedding的默认初始化是正态分布, Xavier会覆盖它
        self._init_weights()

    def _init_weights(self):
        """Xavier均匀初始化

        对所有维度>1的参数(即权重矩阵, 不含偏置和LayerNorm参数)
        使用Xavier均匀分布初始化, 使前向传播和反向传播的方差保持一致.

        Xavier初始化的数学原理:
        - 目标: 让每层的输出方差 ≈ 输入方差, 避免逐层放大或缩小
        - 线性层 y = Wx, 输入方差Var[x], 输出方差Var[y] = n_in × Var[W] × Var[x]
        - 要求 Var[y] = Var[x], 则 Var[W] = 1/n_in
        - 同时考虑反向传播: Var[∂L/∂x] = n_out × Var[W] × Var[∂L/∂y]
        - 折中: Var[W] = 2/(n_in + n_out)
        - Xavier均匀分布: W ~ Uniform(-a, a), a = √(6/(n_in+n_out))

        为什么只初始化dim>1的参数?
        - dim=1的是偏置向量(b)和LayerNorm参数(gamma,beta)
        - 偏置: 初始化为0是标准做法, Xavier初始化偏置没有意义(只有1维)
        - gamma/beta: 已经在LayerNorm.__init__中初始化为1和0, 不应覆盖
        """
        for p in self.parameters():
            if p.dim() > 1:
                nn.init.xavier_uniform_(p)

    def forward(self, src, tgt, src_mask=None, tgt_mask=None):
        """前向传播

        完整数据流:
        src → src_embed × √d_model → src_pe → Encoder → enc_output
        tgt → tgt_embed × √d_model → tgt_pe → Decoder(enc_output) → dec_output → proj → logits

        Args:
            src: 源序列token ID, shape=[batch, src_len]
                 例如翻译任务中: 源语言句子"我 爱 北京"的token ID序列
            tgt: 目标序列token ID (shifted right), shape=[batch, tgt_len]
                 训练时传入完整目标序列(去掉最后一个token)
                 例如: [BOS, I, love, Beijing] (不含EOS)
                 "shifted right": 相比原始序列右移一位, 让Decoder从BOS开始预测
            src_mask: 源序列掩码, shape=[batch, 1, 1, src_len]
                      屏蔽源序列中的padding位置, 防止Encoder/Decoder关注PAD
            tgt_mask: 目标序列掩码, shape=[batch, 1, tgt_len, tgt_len]
                      下三角矩阵, 防止Decoder看到未来位置

        Returns:
            output: logits, shape=[batch, tgt_len, tgt_vocab_size]
                    每个位置对词表中每个词的原始分数
                    取softmax得到概率分布, 取argmax得到预测token ID
        """
        # ====== Step 1: Embedding + 缩放 ======
        # 论文中将Embedding乘以√d_model, 使Embedding的量级与位置编码匹配
        # 数学推导:
        #   nn.Embedding默认初始化为标准正态分布: E[emb]=0, Var[emb]≈1/d_model
        #   位置编码PE的范围: [-1, 1] (sin/cos的值域)
        #   位置编码的方差: Var[PE]≈1/2 (sin/cos均匀分布时的方差)
        #   不缩放时: embedding量级 ≈ √(1/512) ≈ 0.044, PE量级 ≈ 0.7
        #   → PE比embedding大~16倍 → embedding被PE淹没
        #   缩放后: ×√512 → embedding量级 ≈ 0.044×22.6 ≈ 1.0, PE量级 ≈ 0.7
        #   → 两者量级一致, 位置编码和语义信息同等重要
        #
        # 另一个好处: Embedding缩放后与Xavier初始化的权重矩阵量级匹配
        #   Xavier初始化的线性层权重量级 ≈ √(2/(d_model+d_model)) ≈ 0.06
        #   缩放后embedding量级 ≈ 1, 乘以权重后 ≈ 0.06×1 ≈ 0.06
        #   不缩放: 乘以权重后 ≈ 0.06×0.04 ≈ 0.0024 (太小, 梯度消失风险)
        src_embedded = self.src_embed(src) * math.sqrt(self.d_model)
        tgt_embedded = self.tgt_embed(tgt) * math.sqrt(self.d_model)

        # ====== Step 2: 加位置编码 ======
        # 将位置信息注入Embedding, 让模型感知token的顺序
        # x + PE: 语义信息(embedding) + 位置信息(PE) = 既知道"是什么"又知道"在哪里"
        src_embedded = self.src_pe(src_embedded)
        tgt_embedded = self.tgt_pe(tgt_embedded)

        # ====== Step 3: Encoder ======
        # 将源序列编码为连续表示, 每个位置包含整个源序列的上下文信息
        # enc_output: [batch, src_len, d_model]
        #   enc_output[b, t, :] 表示: 在整个源序列的上下文中, 位置t的含义
        #   不再是孤立的token embedding, 而是融合了全局信息的"上下文向量"
        # 例如: "我爱北京天安门"中的"爱" → enc_output不仅包含"爱"本身的语义,
        #   还融合了"我"(施事者)和"北京"(受事者)的信息
        enc_output = self.encoder(src_embedded, src_mask)

        # ====== Step 4: Decoder ======
        # 基于目标序列的历史和Encoder的编码, 预测下一个token
        # Cross-Attention让Decoder关注源序列中最相关的位置
        # dec_output: [batch, tgt_len, d_model]
        #   dec_output[b, t, :] 表示: 位置t融合了已解码的目标历史 + 源序列上下文
        dec_output = self.decoder(tgt_embedded, enc_output, src_mask, tgt_mask)

        # ====== Step 5: 输出投影 ======
        # 将Decoder输出映射到词表大小的logits
        # logits: [batch, tgt_len, tgt_vocab_size]
        #   logits[b, t, k] = proj(dec_output[b, t, :])[k]
        #   即: 第b个batch、第t个位置、第k个词的原始分数
        # 后续操作(不在forward中, 在训练/推理代码中):
        #   softmax(logits) → 概率分布P(token_k | 已解码序列, 源序列)
        #   argmax(logits) → 预测的token ID
        output = self.proj(dec_output)

        return output
```

### 7.2 论文中的默认参数

| 参数 | 值 | 说明 |
|------|-----|------|
| $d_{\text{model}}$ | 512 | 模型特征维度 |
| $n_{\text{heads}}$ | 8 | 注意力头数 |
| $n_{\text{layers}}$ | 6 (Encoder) / 6 (Decoder) | 堆叠层数 |
| $d_{\text{ff}}$ | 2048 | FFN中间层维度 (4×d_model) |
| $d_k = d_v$ | 64 (= 512 / 8) | 每个头的Key/Value维度 |
| Dropout | 0.1 | 正则化概率 |

---

## 8. 训练示例

### 8.1 简单的 Seq2Seq 训练流程

**Teacher Forcing 策略详解：**

训练时使用 Teacher Forcing：每步的Decoder输入不是模型自己上一步的预测结果，而是真实的ground truth token。这是序列模型训练的标准做法。

为什么用 Teacher Forcing 而不用模型自己的预测？
1. **训练稳定**：如果模型某一步预测错误，后续步骤的输入就会偏离 → 错误累积 → 梯度爆炸
2. **收敛更快**：每步的输入都是"正确答案"，模型只需学习单步预测，不需要处理错误输入
3. **暴露偏差(Exposure Bias)**：训练时只见过正确输入，推理时却要面对自己的错误预测 → 训练和推理分布不一致。解决方案：Scheduled Sampling（逐步从模型预测中采样替代ground truth）

**Shifted Right 的含义：**

```
目标序列:     [BOS,  I,   love, Beijing, EOS]    ← 完整序列(含BOS和EOS)
tgt_input:    [BOS,  I,   love, Beijing]         ← 去掉最后一个(EOS), 作为Decoder输入
tgt_output:   [I,    love, Beijing, EOS]          ← 去掉第一个(BOS), 作为训练标签

对应关系:
  Decoder看到 BOS     → 应该预测 I
  Decoder看到 [BOS,I] → 应该预测 love
  Decoder看到 [BOS,I,love] → 应该预测 Beijing
  Decoder看到 [BOS,I,love,Beijing] → 应该预测 EOS
```

"Shifted Right"就是将目标序列右移一位（去掉EOS），让Decoder从BOS开始逐步预测。

```python
def train_step(model, src, tgt, criterion, optimizer, device):
    """单步训练函数

    训练一个batch的完整流程:
    1. 数据准备: shifted right + 生成mask
    2. 前向传播: src→Encoder, tgt_input→Decoder→logits
    3. 计算损失: logits vs tgt_output (CrossEntropy)
    4. 反向传播: loss.backward()
    5. 参数更新: optimizer.step()

    Args:
        model: Transformer模型
        src: 源序列batch, shape=[batch, src_len], token ID
             例如: [2, 15, 47, 8, 0, 0] ← "我 爱 北京 PAD PAD"
        tgt: 目标序列batch, shape=[batch, tgt_len], token ID
             包含<BOS>开头和<EOS>结尾, 如: [BOS, w1, w2, ..., wn, EOS]
             注意: tgt包含完整序列(BOS到EOS), train_step内部会做shifted right处理
        criterion: 损失函数 (CrossEntropyLoss)
        optimizer: 优化器
        device: 计算设备 (cpu/cuda)

    Returns:
        loss.item(): 当前步的损失值(浮点数)
    """
    model.train()  # 设置为训练模式, 启用dropout和BN的随机行为
    # model.train() vs model.eval():
    #   train: dropout随机置零, BN用当前batch统计量
    #   eval:  dropout关闭(保留所有), BN用running统计量
    # Transformer中主要影响: dropout层(训练时随机, 推理时完整)

    # 将数据移到GPU(如果可用)
    src = src.to(device)
    tgt = tgt.to(device)

    # ====== 准备Decoder的输入和目标 ======
    # tgt_input: 去掉最后一个token(EOS), 作为Decoder的输入
    #   例如 tgt = [BOS, I, love, Beijing, EOS], 则 tgt_input = [BOS, I, love, Beijing]
    #   这就是"shifted right"——训练时Decoder看到BOS预测I, 看到[BOS,I]预测love...
    #   为什么去掉EOS? 因为EOS是最后一个预测目标, 不需要作为Decoder的输入
    tgt_input = tgt[:, :-1]

    # tgt_output: 去掉第一个token(BOS), 作为训练目标(标签)
    #   例如 tgt = [BOS, I, love, Beijing, EOS], 则 tgt_output = [I, love, Beijing, EOS]
    #   模型需要预测的正是这些token(不包括BOS, BOS只是起始信号)
    tgt_output = tgt[:, 1:]

    # 生成Decoder的掩码: 下三角矩阵, 防止看到未来位置
    seq_len = tgt_input.size(1)
    tgt_mask = generate_mask(seq_len).to(device)

    # ====== 前向传播 ======
    # 模型输出logits: [batch, tgt_len, tgt_vocab_size]
    # src_mask=None: 此简化示例未处理源序列padding, 实际应用中应传入src_mask
    output = model(src, tgt_input, src_mask=None, tgt_mask=tgt_mask)

    # ====== 计算损失 ======
    # 将output和tgt_output展平, 适配CrossEntropyLoss的输入格式
    # CrossEntropyLoss要求:
    #   input:  [N, C] — N个样本, C个类别的logits
    #   target: [N]    — N个样本的目标类别ID
    #
    # 展平过程:
    #   output: [batch, tgt_len, tgt_vocab_size] → [batch*tgt_len, tgt_vocab_size]
    #     即把所有batch、所有位置的预测合并为一个大batch
    #   tgt_output: [batch, tgt_len] → [batch*tgt_len]
    #     即把所有batch、所有位置的目标token ID合并
    #
    # 每个位置独立计算损失:
    #   loss_i = -log(softmax(logits_i)[tgt_output_i])
    #   总损失 = Σ loss_i / (batch*tgt_len)
    #   (CrossEntropyLoss内部自动做了softmax, 不需要手动调用)
    loss = criterion(output.view(-1, output.size(-1)), tgt_output.view(-1))

    # ====== 反向传播 + 参数更新 ======
    optimizer.zero_grad()  # 清空梯度, 防止累积
    # 为什么必须zero_grad?
    #   PyTorch默认梯度累加(loss.backward()不会清空旧梯度)
    #   不清空会导致: 新梯度 + 上一步梯度 → 更新方向偏移
    #   除非你想实现梯度累加(模拟大batch_size), 此时不应zero_grad
    loss.backward()        # 反向传播, 计算梯度
    # loss.backward()的计算图:
    #   从loss出发, 反向遍历所有计算节点, 用链式法则计算每个参数的梯度
    #   梯度存储在各参数的.grad属性中
    optimizer.step()       # 根据梯度更新参数
    # optimizer.step()的更新规则(Adam):
    #   m = β1*m + (1-β1)*grad     (一阶矩, 梯度的加权平均)
    #   v = β2*v + (1-β2)*grad²   (二阶矩, 梯度平方的加权平均)
    #   θ = θ - lr * m/(√v + eps)  (参数更新, 自适应学习率)

    return loss.item()


# ====== 训练循环 ======
# 初始化模型, 移到GPU
model = Transformer(src_vocab_size=1000, tgt_vocab_size=1000).to(device)

# 交叉熵损失函数, ignore_index=0 表示忽略padding位置(token ID=0)的损失
# padding位置不参与梯度计算, 避免无意义的梯度干扰训练
#
# 为什么ignore_index=0?
#   假设PAD的token ID=0, 不同样本长度不同时需要padding对齐:
#   [BOS, I, love, Beijing, EOS, PAD, PAD] ← 长度7
#   [BOS, Hi, EOS, PAD, PAD, PAD, PAD]     ← 长度7(实际3)
#   PAD位置的预测不影响翻译质量, 其损失不应计入梯度
#   ignore_index=0让CrossEntropyLoss跳过这些位置
#
# Label Smoothing(原论文用了0.1的smoothing):
#   原论文使用Label Smoothing: 不让模型100%确信目标token
#   传统: 目标分布=[0,0,...,1,...,0] (one-hot, 目标token概率=1)
#   Label Smoothing: 目标分布=[ε/K,ε/K,...,1-ε+ε/K,...,ε/K]
#     ε=0.1, K=vocab_size → 非目标token有ε/K的概率, 目标token有1-ε+ε/K
#   好处: 防止模型过度自信 → 提高泛化能力, BLEU分数提升1-2个点
#   实现: nn.CrossEntropyLoss(label_smoothing=0.1) (PyTorch 1.10+)
criterion = nn.CrossEntropyLoss(ignore_index=0)

# Adam优化器, 参数与原论文一致:
# lr=1e-4: 学习率 (论文中用了warmup+decay, 这里简化为固定值)
# betas=(0.9, 0.98): Adam的一阶/二阶矩估计的衰减率
#   注意beta2=0.98而非默认的0.999, 这是论文的特殊设置
#   为什么0.98? 因为学习率调度中warmup阶段的梯度方差较大,
#   0.98比0.999更快适应, 让二阶矩估计更灵敏
# eps=1e-9: 数值稳定项, 比默认1e-8更小, 因为Transformer梯度可能很小
optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, betas=(0.9, 0.98), eps=1e-9)

# 训练多个epoch
for epoch in range(num_epochs):
    for batch_src, batch_tgt in train_dataloader:
        loss = train_step(model, batch_src, batch_tgt, criterion, optimizer, device)
    print(f"Epoch {epoch}: Loss = {loss:.4f}")
```

### 8.2 学习率调度：Warmup + Decay

原论文使用了一种特殊的学习率调度策略：先线性增长(warmup)，再按步数平方根衰减。

$$lr = d_{\text{model}}^{-0.5} \cdot \min(step^{-0.5}, step \cdot warmup\_steps^{-1.5})$$

**为什么需要Warmup？**

1. **训练初期不稳定**：模型参数随机初始化，梯度方向混乱，大学习率会导致训练发散
2. **Warmup阶段**：学习率从0线性增长到峰值，给模型"适应期"，逐步建立稳定的梯度方向
3. **Decay阶段**：学习率逐步衰减，精细调整参数，防止后期震荡

```python
class WarmupDecayScheduler:
    """学习率调度器: Warmup(线性增长) + Decay(平方根衰减)

    公式: lr = d_model^{-0.5} * min(step^{-0.5}, step * warmup_steps^{-1.5})

    效果:
    - step < warmup_steps: lr = d_model^{-0.5} * step * warmup_steps^{-1.5}  (线性增长)
    - step >= warmup_steps: lr = d_model^{-0.5} * step^{-0.5}               (平方根衰减)

    例如 d_model=512, warmup_steps=4000:
    - step=0:    lr=0                   (开始时学习率为0)
    - step=2000: lr≈5e-4                (warmup中途, 线性增长)
    - step=4000: lr≈1e-3                (峰值, warmup结束时)
    - step=8000: lr≈7e-4                (衰减阶段)
    - step=80000: lr≈2.5e-4             (继续衰减)

    Args:
        d_model: 模型维度, 用于学习率缩放
        warmup_steps: warmup步数, 学习率线性增长的阶段长度
    """

    def __init__(self, d_model, warmup_steps=4000):
        self.d_model = d_model
        self.warmup_steps = warmup_steps

    def get_lr(self, step):
        """根据当前步数计算学习率

        Args:
            step: 当前训练步数(全局, 不是epoch内)

        Returns:
            当前学习率
        """
        # d_model^{-0.5}: 缩放因子, 大模型需要更小的学习率
        #   512^{-0.5} ≈ 0.022
        #   这是因为大模型的参数更多, 梯度的量级更大, 需要更小的步长
        factor = self.d_model ** (-0.5)

        # min(step^{-0.5}, step * warmup_steps^{-1.5}):
        #   step^{-0.5}: 衰减阶段的学习率公式(随step增大而减小)
        #   step * warmup_steps^{-1.5}: warmup阶段的学习率公式(随step增大而线性增长)
        #   min()取两者较小值: warmup阶段用线性增长, decay阶段用平方根衰减

        step_factor = min(step ** (-0.5), step * (self.warmup_steps ** (-1.5)))

        return factor * step_factor


# 使用示例:
scheduler = WarmupDecayScheduler(d_model=512, warmup_steps=4000)
# 在optimizer.step()之后调用:
for step in range(total_steps):
    lr = scheduler.get_lr(step)
    for param_group in optimizer.param_groups:
        param_group['lr'] = lr
```

### 8.3 推理（贪心解码）

```python
def greedy_decode(model, src, max_len, start_symbol, eos_symbol, device):
    """贪心解码: 逐步生成目标序列, 每步选概率最大的token

    与训练不同, 推理时没有目标序列作为输入,
    需要模型自己逐步生成: 每生成一个token, 就追加到已生成序列中,
    作为下一步的输入.

    贪心解码的优缺点:
    - 优点: 简单、快速、确定性强(同一输入永远得到同一输出)
    - 缺点: 每步只选局部最优(概率最大的), 不保证全局最优
    - 例如: 第1步选了概率0.3的A(而非0.25的B), 但B可能引出更好的后续序列
    - 改进: Beam Search(见8.5节)同时维护多个候选序列, 更可能找到全局最优

    Args:
        model: 训练好的Transformer模型
        src: 源序列, shape=[1, src_len] (单条样本)
             注意: 贪心解码通常逐条处理, batch_size=1
             如果要批量推理, 需要更复杂的实现(不同样本长度不同)
        max_len: 最大生成长度, 防止无限循环
                 如果模型始终不生成EOS, 最多生成max_len个token后强制停止
        start_symbol: 起始符<BOS>的token ID
        eos_symbol: 结束符<EOS>的token ID, 遇到则停止生成
        device: 计算设备

    Returns:
        ys: 生成的目标序列, shape=[1, gen_len], 包含BOS和EOS
    """
    model.eval()  # 设置为评估模式, 关闭dropout
    # model.eval()的重要效果:
    #   nn.Dropout: 训练时随机置零p%, 推理时保留所有神经元(权重×1/(1-p)补偿)
    #   如果忘记eval(), 推理结果会不稳定(每次不同), 且性能下降

    src = src.to(device)

    with torch.no_grad():  # 不计算梯度, 节省内存和计算
        # torch.no_grad()的效果:
        #   不构建计算图 → 不存储中间激活 → 大幅减少GPU内存占用
        #   不计算梯度 → 禁用反向传播 → 加快推理速度
        #   对于推理, 梯度完全不需要, 必须使用no_grad()

        # ====== Step 1: 编码源序列 ======
        # 只需编码一次, 后续每步都复用Encoder的输出
        # 这是Encoder-Decoder架构的效率优势: Encoder只跑一次
        # 与自回归Decoder的重复计算形成对比:
        #   Encoder: 1次计算, O(src_len²×d)
        #   Decoder: max_len次计算, 每次O(tgt_len²×d) → 总O(max_len³×d)
        src_embedded = model.src_embed(src) * math.sqrt(model.d_model)
        src_embedded = model.src_pe(src_embedded)
        enc_output = model.encoder(src_embedded)

        # ====== Step 2: 逐步解码 ======
        # 初始化: 只有一个起始符<BOS>
        ys = torch.ones(1, 1).fill_(start_symbol).long().to(device)

        for i in range(max_len - 1):
            # 生成当前步的掩码: 随着ys变长, 掩码也变大
            # 每步mask尺寸增长: [1,1,1,1] → [1,1,2,2] → [1,1,3,3] → ...
            tgt_mask = generate_mask(ys.size(1)).to(device)

            # Decoder前向传播
            tgt_embedded = model.tgt_embed(ys) * math.sqrt(model.d_model)
            tgt_embedded = model.tgt_pe(tgt_embedded)
            # 注意: 每步都对整个已生成序列重新跑一遍Decoder
            # 这是自回归生成的特点, 无法并行
            # 计算量随生成长度增长: 第i步需要处理i个token的注意力
            # KV缓存优化: 可以缓存每步的K/V, 只计算新token的Q
            #   这将每步复杂度从O(i²)降为O(i), 总复杂度从O(n³)降为O(n²)
            out = model.decoder(tgt_embedded, enc_output, tgt_mask=tgt_mask)

            # 取最后一个位置的输出, 投影到词表维度
            # out[:, -1]: [1, d_model], 最后一个位置包含所有历史信息
            # 为什么取最后一个位置?
            #   因为掩码确保每个位置只看历史, 最后一个位置的信息最完整
            #   但实际上任意位置t的输出只依赖位置0~t-1,
            #   所以取最后一个位置等于"基于完整历史预测下一个token"
            prob = model.proj(out[:, -1])  # [1, tgt_vocab_size]

            # 贪心选择: 取概率最大的token
            # torch.max返回(values, indices), indices就是概率最大的token ID
            _, next_word = torch.max(prob, dim=1)
            next_word = next_word.item()  # 转为Python整数

            # 将新token追加到已生成序列
            ys = torch.cat([
                ys,
                torch.ones(1, 1).fill_(next_word).long().to(device)
            ], dim=1)

            # 如果生成了结束符<EOS>, 提前终止
            if next_word == eos_symbol:
                break

    return ys
```

### 8.3.1 自回归生成：为什么下一个token基于已生成的所有token？

**自回归（Autoregressive）**是Decoder推理的核心特性：每一步只生成一个token，且这个token的概率依赖前面所有已生成的token。

$$P(\text{整个序列}) = P(t_1) \times P(t_2|t_1) \times P(t_3|t_1,t_2) \times \cdots \times P(t_n|t_1,\ldots,t_{n-1})$$

即：序列的联合概率 = 各步条件概率的乘积，每步的条件概率只依赖历史，不依赖未来。

**为什么必须自回归？**

Decoder的Masked Self-Attention用下三角掩码确保：位置 $t$ 只能看到位置 $0 \sim t-1$ 的信息，看不到 $t+1$ 及之后的"未来"token。这是训练时就建立的约束：

```
训练时的目标序列: [BOS, I, love, Beijing, EOS]
                   ↓    ↓     ↓      ↓      ↓
Decoder每步看到:  BOS  BOS,I BOS,I,love  ...    只看历史,不看未来
预测目标:         I    love  Beijing  EOS      基于历史预测下一步
```

这个训练约束延续到推理：模型只学会了"基于历史预测下一个token"的能力，推理时自然也必须逐步生成——每步把新token追加到历史中，作为下一步的输入。

**非自回归的替代方案（简述）：**

非自回归模型(NAT)试图一步直接输出所有token，但面临两个难题：
1. **输出长度未知**：推理前不知道要生成多少个token（需要额外的长度预测器）
2. **多模态问题**：不同位置的token可能互相矛盾（第2步选了"love"，第3步却选了"hate"，因为两步独立决策）
Transformer原论文选择了自回归方案，保证生成质量。

### 8.3.2 KV Cache：避免重复计算

**朴素推理的浪费：**

看贪心解码代码，每一步都对**整个已生成序列**重新跑一遍Decoder：

```python
# 第1步: Decoder处理 [BOS]           → 预测 I
# 第2步: Decoder处理 [BOS, I]        → 预测 love
# 第3步: Decoder处理 [BOS, I, love]  → 预测 Beijing
# ...
```

第3步时，"BOS"和"I"的注意力计算在第1步、第2步已经做过了一遍，第3步又重新计算——这是巨大的浪费。

**KV Cache的原理：**

注意力计算 $\text{output}_i = \sum_j \alpha(Q_i, K_j) \cdot V_j$ 中，新token的Q只需要与**所有历史位置的K和V**做交互。历史位置的K/V不随新token的加入而改变（因为Masked Self-Attention下，位置j只看位置0~j-1，新token在j之后，不影响j的计算结果）。

因此，可以**缓存每一步计算的K和V**，后续步骤只需：
1. 计算新token的Q、K、V
2. 把新token的K、V追加到缓存中
3. 新token的Q与缓存中所有K做注意力计算，得到新token的输出

**不用KV Cache vs 用KV Cache的计算量对比：**

```
生成n个token:

不用KV Cache:
  第1步: 1个token的注意力 → O(1² × d)
  第2步: 2个token的注意力 → O(2² × d)
  第3步: 3个token的注意力 → O(3² × d)
  ...
  第n步: n个token的注意力 → O(n² × d)
  总计: O(n³ × d)  ← 每步重算所有历史, 计算量爆炸式增长

用KV Cache:
  第1步: 计算1个token的Q/K/V → O(d²), 1个Q与1个K → O(1 × d)
  第2步: 计算1个新token的Q/K/V → O(d²), 1个Q与2个K → O(2 × d)
  第3步: 计算1个新token的Q/K/V → O(d²), 1个Q与3个K → O(3 × d)
  ...
  第n步: 计算1个新token的Q/K/V → O(d²), 1个Q与n个K → O(n × d)
  总计: O(n² × d)  ← 只计算新token与历史的交互, 线性增长!
```

**KV Cache对注意力矩阵的影响：**

```
不用KV Cache (每步重新计算完整注意力矩阵):
  第3步的注意力矩阵:
  [[α(BOS→BOS), α(BOS→I),     α(BOS→love)]     ← BOS行的第2、3列重新计算了
   [α(I→BOS),   α(I→I),       α(I→love)]       ← I行的第3列重新计算了
   [α(love→BOS), α(love→I),   α(love→love)]]   ← 新行, 必须计算

  但前两行的前两列在第1、2步已经算过了, 重复计算!

用KV Cache (只计算新行):
  第3步只计算: [α(love→BOS), α(love→I), α(love→love)]
  因为Masked Self-Attention保证: 位置0(BOS)和位置1(I)的输出不受位置2(love)的影响
  所以不需要重算前两行
```

**为什么只缓存K和V，不缓存Q？**

因为Q只在"当前步骤"被使用，历史Q永远不会再被需要。每一步只需要计算**当前新token的输出**。要算位置t的输出，只需要：

- $Q_t$：当前新token的查询向量 —— "我想找谁"
- 所有历史 $K_{0 \sim t-1}$ 和 $V_{0 \sim t-1}$：历史token提供的"标签"和"内容"

历史位置的Q（$Q_0, Q_1, ..., Q_{t-1}$）**完全不需要**，因为：

```
第1步: 算output_0 — 需要Q_0与K_0,V_0交互 → 输出output_0，Q_0任务完成，不再需要
第2步: 算output_1 — 需要Q_1与K_0,K_1,V_0,V_1交互 → Q_0完全没用(output_0已经算过了)
第3步: 算output_2 — 需要Q_2与K_0,K_1,K_2,V_0,V_1,V_2交互 → Q_0,Q_1都没用
```

每一步只算**一个位置**的输出，只需要**一个Q**。而K和V是"被查阅的资源"——未来的每一步都需要它们，所以必须缓存。

**类比：**
- Q像"顾客"：每个顾客只来一次，点完菜就走，不需要记住这个顾客
- K像"菜单"：每个餐厅的菜单要长期保留，因为未来所有新顾客都要看
- V像"菜品"：菜品要长期供应，因为未来所有新顾客都要吃

所以只缓存"菜单"(K)和"菜品"(V)，不缓存"顾客"(Q)。

**KV Cache的内存代价：**

KV Cache虽然节省了计算，但需要存储所有历史token的K和V向量：

```
每个token的KV缓存大小 = 2 × n_layers × n_heads × d_k × sizeof(float16)
                      = 2 × 6 × 8 × 64 × 2 bytes (float16)
                      = 12,288 bytes ≈ 12KB per token

生成1000个token: 12KB × 1000 = 12MB (单条样本)
批量推理batch_size=32: 12MB × 32 = 384MB

大模型(Llama-2 70B, d_model=8192, n_layers=80, n_heads=64):
  每token KV缓存 ≈ 2 × 80 × 64 × 128 × 2 = 2.5MB
  生成2048个token: 2.5MB × 2048 ≈ 5GB (单条样本!)
```

这就是为什么大模型推理时，**内存瓶颈往往不是模型权重，而是KV Cache**。优化KV Cache内存是当前大模型推理加速的核心研究方向（如MQA/GQA多头共享、PagedAttention等）。

**PyTorch实现KV Cache的简化示例：**

```python
def greedy_decode_with_kv_cache(model, src, max_len, start_symbol, eos_symbol, device):
    """带KV Cache的贪心解码

    与朴素实现的区别:
    - 朴素: 每步把整个ys序列送入Decoder, 重新计算所有token的Q/K/V和注意力
    - KV Cache: 只把新token送入Decoder, K/V从缓存中读取
    """
    model.eval()
    src = src.to(device)

    with torch.no_grad():
        # Encoder: 与朴素实现完全相同, 只跑一次
        src_embedded = model.src_embed(src) * math.sqrt(model.d_model)
        src_embedded = model.src_pe(src_embedded)
        enc_output = model.encoder(src_embedded)

        # 初始化KV Cache: 每个DecoderLayer维护两组缓存
        # self_attn_cache: 存放Masked Self-Attention的K/V
        # cross_attn_cache: 存放Cross-Attention的K/V(Encoder的输出, 只需计算一次)
        # cache结构: [batch, n_heads, cached_len, d_k]
        self_attn_k_cache = []  # 每个DecoderLayer一个
        self_attn_v_cache = []  # 每个DecoderLayer一个
        # Cross-Attention的K/V来自Encoder, 只需算一次后缓存
        # (所有DecoderLayer共享同一个enc_output, 但各自的W_K/W_V投影不同)
        cross_attn_k_cache = []  # 每个DecoderLayer一个, 初始化后不再变化
        cross_attn_v_cache = []  # 每个DecoderLayer一个, 初始化后不再变化

        # 预计算Cross-Attention的K/V (Encoder输出 → K/V投影, 只需一次)
        # 这一步在朴素实现中每步都重复做, 现在只做一次
        for layer in model.decoder.layers:
            cross_k = layer.cross_attn.W_K(enc_output)  # [batch, src_len, d_model]
            cross_v = layer.cross_attn.W_V(enc_output)  # [batch, src_len, d_model]
            # 分头: [batch, src_len, d_model] → [batch, n_heads, src_len, d_k]
            cross_k = cross_k.view(1, enc_output.size(1), model.decoder.layers[0].cross_attn.n_heads, model.decoder.layers[0].cross_attn.d_k).transpose(1, 2)
            cross_v = cross_v.view(1, enc_output.size(1), model.decoder.layers[0].cross_attn.n_heads, model.decoder.layers[0].cross_attn.d_k).transpose(1, 2)
            cross_attn_k_cache.append(cross_k)
            cross_attn_v_cache.append(cross_v)

        # 初始化: 只有BOS
        ys = torch.ones(1, 1).fill_(start_symbol).long().to(device)

        for step in range(max_len - 1):
            # 只处理最新的1个token (而非整个ys序列!)
            # 这是KV Cache的核心优化: 每步只输入1个新token
            new_token = ys[:, -1:]  # [1, 1] — 只取最后一个(新)token

            # Embedding + 位置编码 (只对1个新token)
            new_embedded = model.tgt_embed(new_token) * math.sqrt(model.d_model)
            new_embedded = model.tgt_pe(new_embedded)  # PE自动取最后一个位置

            # 逐层处理Decoder
            x = new_embedded  # [1, 1, d_model] — 只有1个token

            for layer_idx, layer in enumerate(model.decoder.layers):
                # ====== Masked Self-Attention (带KV Cache) ======
                # 计算新token的Q/K/V
                new_q = layer.masked_self_attn.W_Q(x).view(1, 1, layer.masked_self_attn.n_heads, layer.masked_self_attn.d_k).transpose(1, 2)
                new_k = layer.masked_self_attn.W_K(x).view(1, 1, layer.masked_self_attn.n_heads, layer.masked_self_attn.d_k).transpose(1, 2)
                new_v = layer.masked_self_attn.W_V(x).view(1, 1, layer.masked_self_attn.n_heads, layer.masked_self_attn.d_k).transpose(1, 2)

                # 把新token的K/V追加到缓存
                if step == 0:
                    self_attn_k_cache.append(new_k)
                    self_attn_v_cache.append(new_v)
                else:
                    self_attn_k_cache[layer_idx] = torch.cat([self_attn_k_cache[layer_idx], new_k], dim=2)
                    self_attn_v_cache[layer_idx] = torch.cat([self_attn_v_cache[layer_idx], new_v], dim=2)

                # 新token的Q与缓存中所有K做注意力
                # Q: [1, n_heads, 1, d_k] — 只有1个查询(新token)
                # K_cached: [1, n_heads, step+1, d_k] — 所有历史+新token的键
                # scores: [1, n_heads, 1, step+1] — 新token对所有位置的注意力分数
                # 注意: 这里不需要mask! 因为新token在最后位置, 自然只能看所有历史
                # (下三角mask对新token=全1行, 不需要屏蔽任何位置)
                scores = torch.matmul(new_q, self_attn_k_cache[layer_idx].transpose(-2, -1))
                scores = scores / math.sqrt(layer.masked_self_attn.d_k)
                attn_weights = F.softmax(scores, dim=-1)
                attn_weights = layer.dropout1(attn_weights)

                # 用注意力权重对缓存中所有V加权求和
                attn_output = torch.matmul(attn_weights, self_attn_v_cache[layer_idx])
                # attn_output: [1, n_heads, 1, d_k] → reshape → [1, 1, d_model]
                attn_output = attn_output.transpose(1, 2).contiguous().view(1, 1, -1)
                attn_output = layer.masked_self_attn.W_O(attn_output)

                x = layer.norm1(x + attn_output)

                # ====== Cross-Attention (K/V从缓存读取, 不需重算) ======
                # Q来自新token, K/V来自缓存的Encoder投影(预计算好的)
                cross_q = layer.cross_attn.W_Q(x).view(1, 1, layer.cross_attn.n_heads, layer.cross_attn.d_k).transpose(1, 2)
                scores = torch.matmul(cross_q, cross_attn_k_cache[layer_idx].transpose(-2, -1))
                scores = scores / math.sqrt(layer.cross_attn.d_k)
                attn_weights = F.softmax(scores, dim=-1)
                cross_output = torch.matmul(attn_weights, cross_attn_v_cache[layer_idx])
                cross_output = cross_output.transpose(1, 2).contiguous().view(1, 1, -1)
                cross_output = layer.cross_attn.W_O(cross_output)

                x = layer.norm2(x + cross_output)

                # ====== FFN (与朴素实现相同) ======
                ff_output = layer.ffn(x)
                x = layer.norm3(x + ff_output)

            # 投影到词表, 预测下一个token
            prob = model.proj(x)  # [1, 1, tgt_vocab_size] → 取[0,0,:]即可
            _, next_word = torch.max(prob[0, 0, :], dim=0)
            next_word = next_word.item()

            ys = torch.cat([ys, torch.ones(1, 1).fill_(next_word).long().to(device)], dim=1)

            if next_word == eos_symbol:
                break

    return ys
```

### 8.5 Beam Search 解码

Beam Search 是贪心解码的改进版：同时维护 `k` 个最优候选序列，每步从所有候选的扩展中选概率最大的 `k` 个继续。

```python
def beam_search_decode(model, src, max_len, start_symbol, eos_symbol, beam_size, device):
    """Beam Search解码: 维护beam_size个最优候选序列

    与贪心解码的区别:
    - 贪心: 每步只保留1个最优token → 只有一条候选路径
    - Beam: 每步保留beam_size个最优候选 → 同时探索多条路径
    - 效果: Beam Search通常比贪心解码产出更高质量的翻译

    工作原理(beam_size=3为例):
    第1步: 从BOS出发, 找概率最大的3个token → 3条候选序列
    第2步: 每条候选扩展1个token → 3×vocab_size个扩展 → 选概率最大的3个
    第3步: 同上, 逐步扩展
    最终: 选3条候选中概率最大的那条作为输出

    概率计算:
    - 每条候选序列的概率 = 所有token概率的对数求和
    - log P(BOS, w1, w2) = log P(w1|BOS) + log P(w2|BOS,w1)
    - 用对数避免概率乘积的数值下溢(多个小概率相乘→接近0→浮点精度丢失)

    Args:
        model: 训练好的Transformer模型
        src: 源序列, shape=[1, src_len]
        max_len: 最大生成长度
        start_symbol: <BOS>的token ID
        eos_symbol: <EOS>的token ID
        beam_size: beam宽度, 同时维护的候选序列数(常用3-5)
        device: 计算设备

    Returns:
        best_sequence: 概率最大的生成序列, shape=[1, gen_len]
    """
    model.eval()
    src = src.to(device)

    with torch.no_grad():
        # 编码源序列(与贪心解码相同, 只编码一次)
        src_embedded = model.src_embed(src) * math.sqrt(model.d_model)
        src_embedded = model.src_pe(src_embedded)
        enc_output = model.encoder(src_embedded)

        # 初始化beam: 每条候选 = (累计log概率, token序列)
        # 初始只有一条: [BOS], log_prob=0 (概率=1 → log=0)
        beams = [(0, torch.ones(1, 1).fill_(start_symbol).long().to(device))]
        # 已完成的候选(遇到了EOS)
        completed = []

        for step in range(max_len - 1):
            all_candidates = []

            for log_prob, seq in beams:
                # 对每条候选序列, 扩展一步
                tgt_mask = generate_mask(seq.size(1)).to(device)
                tgt_embedded = model.tgt_embed(seq) * math.sqrt(model.d_model)
                tgt_embedded = model.tgt_pe(tgt_embedded)
                out = model.decoder(tgt_embedded, enc_output, tgt_mask=tgt_mask)
                logits = model.proj(out[:, -1])  # [1, tgt_vocab_size]

                # 取log_softmax而非softmax:
                #   直接在log空间计算, 避免 log(small_prob) 的数值问题
                #   log_softmax(x) = x - log(Σe^x), 数值稳定
                log_probs_next = F.log_softmax(logits, dim=-1)  # [1, tgt_vocab_size]

                # 取概率最大的beam_size个token
                topk_log_probs, topk_indices = log_probs_next.topk(beam_size, dim=-1)

                for k in range(beam_size):
                    # 新候选的累计log概率 = 原概率 + 新token的概率
                    new_log_prob = log_prob + topk_log_probs[0, k].item()
                    new_token = topk_indices[0, k].item()
                    # 拼接新token到序列末尾
                    new_seq = torch.cat([
                        seq,
                        torch.ones(1, 1).fill_(new_token).long().to(device)
                    ], dim=1)

                    if new_token == eos_symbol:
                        # 遇到EOS, 这条候选已完成
                        # 长度惩罚: 防止短序列因概率累积少而被低估
                        # 常用惩罚: 除以长度^α (α=0.6~0.7)
                        length_penalty = ((5 + new_seq.size(1)) / 5) ** 0.6
                        completed.append((new_log_prob / length_penalty, new_seq))
                    else:
                        all_candidates.append((new_log_prob, new_seq))

            if not all_candidates:
                # 所有候选都已完成(遇到了EOS)
                break

            # 从所有候选中选概率最大的beam_size个
            all_candidates.sort(key=lambda x: x[0], reverse=True)
            beams = all_candidates[:beam_size]

        # 如果没有已完成的候选, 选当前beam中概率最大的
        if not completed:
            completed = beams

        # 选概率最大的已完成序列
        completed.sort(key=lambda x: x[0], reverse=True)
        best_score, best_sequence = completed[0]

        return best_sequence
```

---

## 9. 关键知识点总结

### 9.1 为什么 Transformer 有效？

| 问题 | 解决方案 | 原理 |
|------|---------|------|
| RNN 无法并行 | 自注意力机制并行计算所有位置 | 所有位置的Q/K/V同时计算, matmul一次完成 |
| RNN 长距离依赖衰减 | 自注意力直接建模任意距离的关系 | 任意两位置之间只需1步attention, 不像RNN需n步 |
| 无位置信息 | 正弦余弦位置编码注入位置 | PE(pos+k)可由PE(pos)线性变换得到 |
| 深层网络梯度问题 | 残差连接 + LayerNorm | 梯度至少有1保底(残差), LN稳定分布 |
| 过拟合 | Dropout + 多头注意力分散计算 | 随机置零防止过度依赖, 多头分散注意力 |

### 9.2 Attention 计算过程图解

```
输入: X [batch, seq_len, d_model]
      │
      ├──→ W_Q → Q [batch, seq_len, d_model] → split → [batch, n_heads, seq_len, d_k]
      ├──→ W_K → K [batch, seq_len, d_model] → split → [batch, n_heads, seq_len, d_k]
      └──→ W_V → V [batch, seq_len, d_model] → split → [batch, n_heads, seq_len, d_k]
      │
      ↓ 计算注意力 (每个头独立)
      Q @ K^T = scores [batch, n_heads, seq_len, seq_len]  ← 注意力分数矩阵
      scores / √d_k = scaled_scores                         ← 缩放防止梯度消失
      + mask → masked_scores                                ← 屏蔽不可见位置
      softmax → attn_weights [batch, n_heads, seq_len, seq_len] ← 概率分布(每行和=1)
      attn_weights @ V = head_output [batch, n_heads, seq_len, d_k] ← 加权求和
      │
      ↓ 合并多头
      concat → [batch, seq_len, n_heads × d_k] = [batch, seq_len, d_model]
      W_O → output [batch, seq_len, d_model]                ← 最终线性投影
```

### 9.3 Transformer 家族

| 模型 | 年份 | 架构 | 特点 | 典型应用 |
|------|------|------|------|---------|
| Transformer | 2017 | Encoder-Decoder | 原始Seq2Seq | 机器翻译 |
| BERT | 2018 | Encoder-only | 双向理解, MLM预训练 | 文本分类、NER |
| GPT-1/2/3 | 2018-2020 | Decoder-only | 单向生成, LM预训练 | 文本生成 |
| T5 | 2019 | Encoder-Decoder | 文本到文本统一框架 | 多任务NLP |
| ViT | 2020 | Encoder-only | 图像切patch当token | 图像分类 |
| GPT-4 | 2023 | Decoder-only | 多模态大模型 | 通用AI助手 |

**Encoder-only vs Decoder-only vs Encoder-Decoder：**

| 架构 | 代表模型 | 适用任务 | 核心特点 |
|------|---------|---------|---------|
| Encoder-only | BERT | 理解类(分类、抽取) | 双向注意力, 看到完整输入 |
| Decoder-only | GPT | 生成类(对话、续写) | 单向注意力, 自回归生成 |
| Encoder-Decoder | T5 | 翻译、摘要 | 先理解(Encoder)再生成(Decoder) |

### 9.4 计算复杂度分析

| 操作 | 复杂度 | 说明 | 何时成为瓶颈 |
|------|--------|------|-------------|
| Self-Attention | $O(n^2 \cdot d)$ | n为序列长度, d为维度 | n很大时(长文本) |
| FFN | $O(n \cdot d^2)$ | 逐位置独立, 与序列长度线性关系 | d很大时(大模型) |
| RNN单步 | $O(n \cdot d^2)$ | 序列长时优于Attention, 但无法并行 | 总是(无法并行) |
| Conv(局部k) | $O(n \cdot k \cdot d^2)$ | k为卷积核宽度, 有限范围交互 | 需要多层堆叠捕捉远距离 |

**自注意力的 $O(n^2)$ 瓶颈：**

当序列长度n很大时(如n=8192), 注意力分数矩阵的size = n² = 67M, 这是Transformer处理长文本的主要瓶颈。解决方案：

| 方法 | 复杂度 | 思路 |
|------|--------|------|
| Sparse Attention | $O(n \cdot \sqrt{n})$ | 只计算部分位置的注意力(局部+全局) |
| Linformer | $O(n)$ | 用低秩投影压缩K/V的序列维度 |
| Flash Attention | $O(n^2)$ 但更快 | 利用GPU内存层次优化, 减少HBM读写 |
| KV Cache | 推理优化 | 缓存已计算的K/V, 每步只算新token |

### 9.5 Transformer 的局限与改进方向

| 局限 | 描述 | 改进方向 |
|------|------|---------|
| 长序列瓶颈 | $O(n^2)$注意力计算 | Sparse/Linear Attention |
| 位置编码局限 | 固定PE不适应动态长度 | RoPE(旋转位置编码)、ALiBi |
| 单向信息流 | Decoder只能看历史 | BERT的双向理解 |
| 无局部性 | 注意力无空间/位置约束 | 加局部窗口约束 |
| 计算量大 | 大模型需要大量GPU | 量化、蒸馏、剪枝 |

---

## 参考资料

- [Attention Is All You Need (原始论文)](https://arxiv.org/abs/1706.03762)
- [The Illustrated Transformer (Jay Alammar)](https://jalammar.github.io/illustrated-transformer/)
- [Transformer模型详解（知乎）](https://zhuanlan.zhihu.com/p/485128284)
- [PyTorch官方Transformer实现](https://pytorch.org/docs/stable/generated/torch.nn.Transformer.html)
- [近年 AI 应用技术串讲与优质文档分享｜Agent、Skill、OpenClaw、Harness……](https://oigi8odzc5w.feishu.cn/wiki/WBMfwiNkfi6uNFkRtXdcavDzn0e)
