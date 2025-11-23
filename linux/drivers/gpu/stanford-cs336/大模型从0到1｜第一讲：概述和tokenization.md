# 大模型从0到1｜第一讲：概述和Tokenization

> 课程链接：[Stanford CS336 Spring 2025 - Lecture 1](https://stanford-cs336.github.io/spring2025-lectures/?trace=var/traces/lecture_01.json)

---

## 课程团队

![Course Staff](https://stanford-cs336.github.io/spring2025-lectures/images/course-staff.png)

**CS336: Language Models From Scratch (Spring 2025)**

- 这是 CS336 的第二次开课
- 斯坦福版本规模扩大了 50%
- 讲座将发布到 YouTube，向全世界开放

---

## 为什么要开设这门课？

### 问题：研究者与底层技术的脱节

让我们问问 GPT-4：
> "Why teach a course on building language models from scratch? Answer in one sentence."

**核心问题：** 研究者正在与底层技术**脱节**

**时间线：**
- **8 年前：** 研究者会实现并训练自己的模型
- **6 年前：** 研究者会下载模型（如 BERT）并微调
- **今天：** 研究者只是提示专有模型（GPT-4/Claude/Gemini）

**抽象层次的提升：**
- ✅ 提高生产力
- ❌ 但这些抽象是有漏洞的（与编程语言或操作系统不同）
- ❌ 仍有需要撕开整个技术栈的基础研究

**核心理念：** 对这项技术的**完全理解**对于**基础研究**是必要的

**本课程方法：** 通过**构建**来**理解**

但有一个小问题...

---

## 大模型的工业化

![Industrialization](https://upload.wikimedia.org/wikipedia/commons/thumb/c/cc/Industrialisation.jpg/440px-Industrialisation.jpg)

**规模现状：**
- GPT-4 据称有 1.8T 参数
- GPT-4 据称训练成本 $100M
- xAI 用 200,000 个 H100 构建集群训练 Grok
- Stargate（OpenAI, NVIDIA, Oracle）4 年投资 $500B

**透明度问题：**

没有关于前沿模型如何构建的公开细节。

来自 GPT-4 技术报告：

![GPT-4 No Details](https://stanford-cs336.github.io/spring2025-lectures/images/gpt4-no-details.png)

---

## More is Different（规模带来质变）

**挑战：**
- 前沿模型对我们来说遥不可及
- 构建小型语言模型（<1B 参数）可能无法代表大型语言模型

**示例 1：** 注意力 vs MLP 的 FLOPs 占比随规模变化

![Roller FLOPs](https://stanford-cs336.github.io/spring2025-lectures/images/roller-flops.png)

**示例 2：** 行为的涌现（Emergence）

![Wei Emergence](https://stanford-cs336.github.io/spring2025-lectures/images/wei-emergence-plot.png)

---

## 本课程能学到什么可以迁移到前沿模型？

**三类知识：**

1. **机制（Mechanics）：** 事物如何工作
   - Transformer 是什么
   - 模型并行如何利用 GPU
   - ✅ **可迁移**

2. **思维方式（Mindset）：** 充分利用硬件，认真对待规模
   - 扩展定律（Scaling Laws）
   - ✅ **可迁移**

3. **直觉（Intuitions）：** 哪些数据和建模决策产生好的准确性
   - ⚠️ **部分可迁移**（不一定跨规模迁移）

---

## 直觉？🤷

**现实：** 一些设计决策无法（尚未）证明合理性，只能来自实验

**示例：** Noam Shazeer 引入 SwiGLU 的论文

![Divine Benevolence](https://stanford-cs336.github.io/spring2025-lectures/images/divine-benevolence.png)

---

## The Bitter Lesson（痛苦的教训）

**错误解读：** 规模就是一切，算法不重要

**正确解读：** 能够扩展的算法才重要

### accuracy = efficiency × resources

**效率的重要性：**
- 在更大规模下，效率更加重要（不能浪费）
- 2012-2019 年，ImageNet 上的算法效率提升了 44 倍

**框架：** 给定一定的计算和数据预算，能构建的最佳模型是什么？

换句话说，**最大化效率**！

---

## 大模型发展历程

### 前神经网络时代（2010年代之前）

- **语言模型测量英语熵** - Shannon (1950)
- **N-gram 语言模型** - 用于机器翻译、语音识别 - Brants et al. (2007)

### 神经网络组件（2010年代）

- **首个神经语言模型** - Bengio et al. (2003)
- **序列到序列建模** - 用于机器翻译 - Sutskever et al. (2014)
- **Adam 优化器** - Kingma & Ba (2014)
- **注意力机制** - 用于机器翻译 - Bahdanau et al. (2015)
- **Transformer 架构** - 用于机器翻译 - Vaswani et al. (2017)
- **混合专家（MoE）** - Shazeer et al. (2017)
- **模型并行** - GPipe (2018), ZeRO (2019), Megatron-LM (2019)

### 早期基础模型（2010年代末）

- **ELMo：** LSTM 预训练，微调帮助任务
- **BERT：** Transformer 预训练，微调帮助任务
- **Google T5 (11B)：** 将一切转换为文本到文本

### 拥抱规模，更加封闭

- **OpenAI GPT-2 (1.5B)：** 流畅文本，零样本的初步迹象，分阶段发布
- **扩展定律：** 为扩展提供希望/可预测性 - Kaplan et al. (2020)
- **OpenAI GPT-3 (175B)：** 上下文学习，封闭
- **Google PaLM (540B)：** 大规模，训练不足
- **DeepMind Chinchilla (70B)：** 计算最优扩展定律

### 开源模型

- **EleutherAI：** 开放数据集（The Pile）和模型（GPT-J）
- **Meta OPT (175B)：** GPT-3 复制，许多硬件问题
- **Hugging Face / BigScience BLOOM：** 专注于数据来源
- **Meta Llama 系列：** Llama, Llama 2, Llama 3
- **Alibaba Qwen 系列：** Qwen 2.5
- **DeepSeek 系列：** DeepSeek 67B, DeepSeek-V2, DeepSeek-V3
- **AI2 OLMo 2：** OLMo 7B, OLMo 2

### 开放程度的层次

1. **封闭模型（如 GPT-4o）：** 仅 API 访问
2. **开放权重模型（如 DeepSeek）：** 权重可用，论文有架构细节，一些训练细节，无数据细节
3. **开源模型（如 OLMo）：** 权重和数据可用，论文有大部分细节（但不一定有理由、失败实验）

### 当今的前沿模型

- **OpenAI o3**
- **Anthropic Claude Sonnet 3.7**
- **xAI Grok 3**
- **Google Gemini 2.5**
- **Meta Llama 3.3**
- **DeepSeek r1**
- **Alibaba Qwen 2.5 Max**
- **Tencent Hunyuan-T1**

---

## 什么是可执行讲座？

这是一个**可执行讲座**，一个通过执行来传递讲座内容的程序。

**可执行讲座的优势：**
- 查看和运行代码（因为一切都是代码！）
- 查看变量值和执行流程
- 看到讲座的层次结构
- 跳转到定义和概念

**示例：**
```python
total = 0  # 可以检查值
for x in [1, 2, 3]:  # 可以看到 x 的变化
    total += x  # 可以看到 total 的更新
```

---

## 课程信息

**官网：** https://stanford-cs336.github.io/spring2025/

**学分：** 5 个学分

**工作量警告：**
> 来自 2024 年春季课程评估的评论：
> "整个作业的工作量大约相当于 CS 224n 的所有 5 个作业加上最终项目。而这只是第一个作业。"

### 为什么应该选这门课

- 你有强迫性需要理解事物如何工作
- 你想锻炼研究工程能力

### 为什么不应该选这门课

- 你这个季度实际上想完成研究（和你的导师谈谈）
- 你对学习 AI 最新技术感兴趣（如多模态、RAG 等）→ 应该选研讨课
- 你想在自己的应用领域获得好结果 → 应该提示或微调现有模型

### 如何在家跟随

- 所有讲座材料和作业将在线发布
- 讲座通过 CGOE 录制并在 YouTube 上提供（有一定延迟）
- 我们计划明年再次开设这门课

### 作业

- **5 个作业：** 基础、系统、扩展定律、数据、对齐
- **无脚手架代码：** 但提供单元测试和适配器接口帮助检查正确性
- **本地实现测试正确性，然后在集群上运行进行基准测试**（准确性和速度）
- **排行榜：** 某些作业（在训练预算下最小化困惑度）
- **AI 工具：** CoPilot、Cursor 可能会影响学习，自行承担风险

### 计算集群

- 感谢 Together AI 提供计算集群 🙏
- 请阅读[使用指南](https://docs.google.com/document/d/1BSSig7zInyjDKcbNGftVxubiHlwJ-ZqahQewIzBmBOo/edit)
- **提前开始作业**，因为临近截止日期集群会很满！

---

## 课程核心：效率

**资源：** 数据 + 硬件（计算、内存、通信带宽）

**核心问题：** 给定固定资源，如何训练最佳模型？

**示例：** 给定 Common Crawl 数据和 32 个 H100 GPU 2 周时间，应该怎么做？

### 设计决策

![Design Decisions](https://stanford-cs336.github.io/spring2025-lectures/images/design-decisions.png)

---

## 课程概览

### Part 1: 基础（Basics）

**目标：** 让完整流程的基本版本运行起来

**组件：** Tokenization, 模型架构, 训练

#### Tokenization

Tokenizer 在字符串和整数序列（token）之间转换

![Tokenized Example](https://stanford-cs336.github.io/spring2025-lectures/images/tokenized-example.png)

**直觉：** 将字符串分解为流行的片段

**本课程：** Byte-Pair Encoding (BPE) tokenizer

**无 Tokenizer 方法：** ByT5, MEGABYTE, BLT, TFree
- 直接使用字节，有前景，但尚未扩展到前沿

#### 架构

**起点：** 原始 Transformer

![Transformer Architecture](https://stanford-cs336.github.io/spring2025-lectures/images/transformer-architecture.png)

**变体：**
- **激活函数：** ReLU, SwiGLU
- **位置编码：** Sinusoidal, RoPE
- **归一化：** LayerNorm, RMSNorm
- **归一化位置：** Pre-norm vs Post-norm
- **MLP：** Dense, Mixture of Experts
- **注意力：** Full, Sliding Window, Linear
- **低维注意力：** Group-Query Attention (GQA), Multi-Head Latent Attention (MLA)
- **状态空间模型：** Hyena

#### 训练

- **优化器：** AdamW, Muon, SOAP
- **学习率调度：** Cosine, WSD
- **批大小：** Critical batch size
- **正则化：** Dropout, Weight Decay
- **超参数：** 网格搜索（头数、隐藏维度）

#### Assignment 1

**GitHub：** https://github.com/stanford-cs336/assignment1-basics

**任务：**
- 实现 BPE tokenizer
- 实现 Transformer、交叉熵损失、AdamW 优化器、训练循环
- 在 TinyStories 和 OpenWebText 上训练
- 排行榜：在 H100 上 90 分钟内最小化 OpenWebText 困惑度

---

### Part 2: 系统（Systems）

**目标：** 充分利用硬件

**组件：** Kernels, 并行化, 推理

#### Kernels

**A100 GPU 架构：**

![A100 Architecture](https://miro.medium.com/v2/resize:fit:2000/format:webp/1*6xoBKi5kL2dZpivFe1-zgw.jpeg)

**类比：** 仓库 : DRAM :: 工厂 : SRAM

![Factory Bandwidth](https://horace.io/img/perf_intro/factory_bandwidth.png)

**技巧：** 通过最小化数据移动来最大化 GPU 利用率

**工具：** CUDA / **Triton** / CUTLASS / ThunderKittens

#### 并行化

**多 GPU 场景（8 个 A100）：**

![8xA100 Topology](https://www.fibermall.com/blog/wp-content/uploads/2024/09/the-hardware-topology-of-a-typical-8xA100-GPU-host.png)

**原则：** GPU 间数据移动更慢，但同样的"最小化数据移动"原则适用

**技术：**
- 集合操作（gather, reduce, all-reduce）
- 跨 GPU 分片（参数、激活、梯度、优化器状态）
- 并行策略：数据并行、张量并行、流水线并行、序列并行

#### 推理

**目标：** 给定提示生成 token（实际使用模型所需！）

**用途：** 强化学习、测试时计算、评估

**全局视角：** 推理计算（每次使用）超过训练计算（一次性成本）

**两个阶段：**

![Prefill Decode](https://stanford-cs336.github.io/spring2025-lectures/images/prefill-decode.png)

- **Prefill（类似训练）：** Token 已给定，可一次处理（计算受限）
- **Decode：** 需要逐个生成 token（内存受限）

**加速解码方法：**
- 使用更便宜的模型（剪枝、量化、蒸馏）
- 推测解码：使用便宜的"草稿"模型生成多个 token，然后用完整模型并行评分（精确解码！）
- 系统优化：KV 缓存、批处理

#### Assignment 2

**GitHub（2024）：** https://github.com/stanford-cs336/spring2024-assignment2-systems

**任务：**
- 用 Triton 实现融合 RMSNorm kernel
- 实现分布式数据并行训练
- 实现优化器状态分片
- 对实现进行基准测试和性能分析

---

### Part 3: 扩展定律（Scaling Laws）

**目标：** 在小规模做实验，预测大规模的超参数/损失

**问题：** 给定 FLOPs 预算 C，使用更大的模型 N 还是训练更多 token D？

**计算最优扩展定律：** Kaplan et al. (2020), Chinchilla

![Chinchilla Isoflop](https://stanford-cs336.github.io/spring2025-lectures/images/chinchilla-isoflop.png)

**结论：** D* = 20 N*（例如，1.4B 参数模型应在 28B token 上训练）

**注意：** 这没有考虑推理成本！

#### Assignment 3

**GitHub（2024）：** https://github.com/stanford-cs336/spring2024-assignment3-scaling

**任务：**
- 我们定义训练 API（超参数 → 损失）基于之前的运行
- 提交"训练作业"（在 FLOPs 预算下）并收集数据点
- 拟合扩展定律到数据点
- 提交扩展后超参数的预测
- 排行榜：在 FLOPs 预算下最小化损失

---

### Part 4: 数据（Data）

**问题：** 我们希望模型具有什么能力？

多语言？代码？数学？

![The Pile Chart](https://ar5iv.labs.arxiv.org/html/2101.00027/assets/pile_chart2.png)

#### 评估

- **困惑度：** 语言模型的教科书评估
- **标准化测试：** MMLU, HellaSwag, GSM8K
- **指令遵循：** AlpacaEval, IFEval, WildBench
- **扩展测试时计算：** Chain-of-thought, 集成
- **LM-as-a-judge：** 评估生成任务
- **完整系统：** RAG, agents

#### 数据策划

- 数据不会从天而降
- **来源：** 从互联网爬取的网页、书籍、arXiv 论文、GitHub 代码等
- **版权：** 诉诸合理使用来训练版权数据？
- **许可：** 可能需要许可数据（如 Google 与 Reddit）
- **格式：** HTML, PDF, 目录（不是文本！）

#### 数据处理

- **转换：** 将 HTML/PDF 转换为文本（保留内容、一些结构、重写）
- **过滤：** 保留高质量数据，删除有害内容（通过分类器）
- **去重：** 节省计算，避免记忆；使用 Bloom 过滤器或 MinHash

#### Assignment 4

**GitHub（2024）：** https://github.com/stanford-cs336/spring2024-assignment4-data

**任务：**
- 将 Common Crawl HTML 转换为文本
- 训练分类器过滤质量和有害内容
- 使用 MinHash 去重
- 排行榜：在 token 预算下最小化困惑度

---

### Part 5: 对齐（Alignment）

**基础模型：** 原始潜力，非常擅长完成下一个 token

**对齐：** 使模型真正有用

**对齐目标：**
- 让语言模型遵循指令
- 调整风格（格式、长度、语气等）
- 纳入安全性（如拒绝回答有害问题）

#### 监督微调（SFT）

**指令数据：** (prompt, response) 对

```python
ChatExample(
    turns=[
        Turn(role="system", content="You are a helpful assistant."),
        Turn(role="user", content="What is 1 + 1?"),
        Turn(role="assistant", content="The answer is 2."),
    ],
)
```

**数据：** 通常涉及人工标注

**直觉：** 基础模型已经有技能，只需要少量示例来激发它们

**方法：** 监督学习，微调模型以最大化 p(response | prompt)

#### 从反馈中学习

**偏好数据：** 使用模型生成多个响应（如 [A, B]）到给定提示

用户提供偏好（如 A < B 或 A > B）

```python
PreferenceExample(
    history=[
        Turn(role="system", content="You are a helpful assistant."),
        Turn(role="user", content="What is the best way to train a language model?"),
    ],
    response_a="You should use a large dataset and train for a long time.",
    response_b="You should use a small dataset and train for a short time.",
    chosen="a",
)
```

**验证器：**
- 形式验证器（如代码、数学）
- 学习验证器：针对 LM-as-a-judge 训练

**算法：**
- **PPO：** 来自强化学习的近端策略优化
- **DPO：** 直接策略优化，用于偏好数据，更简单
- **GRPO：** 组相对偏好优化，移除价值函数

#### Assignment 5

**GitHub（2024）：** https://github.com/stanford-cs336/spring2024-assignment5-alignment

**任务：**
- 实现监督微调
- 实现直接偏好优化（DPO）
- 实现组相对偏好优化（GRPO）

---

## 效率驱动设计决策

**当前：** 我们受计算约束，因此设计决策将反映充分利用给定硬件

- **数据处理：** 避免在坏/无关数据上浪费宝贵计算
- **Tokenization：** 使用原始字节很优雅，但在当今模型架构下计算效率低
- **模型架构：** 许多变化是为了减少内存或 FLOPs（如共享 KV 缓存、滑动窗口注意力）
- **训练：** 我们可以只用一个 epoch！
- **扩展定律：** 在较小模型上使用更少计算进行超参数调优
- **对齐：** 如果将模型更多调整到期望用例，需要更小的基础模型

**未来：** 我们将变得数据受限...

---
## Tokenization 详解

> 本单元受 Andrej Karpathy 关于 tokenization 的视频启发，推荐观看！
> [YouTube 视频](https://www.youtube.com/watch?v=zduSFxRajkE)

---

### 什么是 Tokenization？

**原始文本：** 通常表示为 Unicode 字符串
```python
string = "Hello, 🌍! 你好!"
```

**语言模型：** 在 token 序列（通常用整数索引表示）上放置概率分布
```python
indices = [15496, 11, 995, 0]
```

**需求：**
- **编码（Encode）：** 将字符串转换为 token 的过程
- **解码（Decode）：** 将 token 转换回字符串的过程

**Tokenizer：** 实现 encode 和 decode 方法的类

```python
class Tokenizer(ABC):
    def encode(self, string: str) -> list[int]:
        raise NotImplementedError

    def decode(self, indices: list[int]) -> str:
        raise NotImplementedError
```

**词汇表大小（Vocabulary Size）：** 可能的 token 数量（整数）

---

### Tokenization 示例

**交互式网站：** https://tiktokenizer.vercel.app/?encoder=gpt2

**观察：**
- 单词及其前面的空格是同一个 token 的一部分（如 " world"）
- 开头和中间的单词表示不同（如 "hello hello"）
- 数字被 tokenize 为每几位数字

**GPT-2 Tokenizer 实战：**

```python
tokenizer = tiktoken.get_encoding("gpt2")
string = "Hello, 🌍! 你好!"

# 编码
indices = tokenizer.encode(string)
# 输出: [15496, 11, 12520, 234, 171, 120, 234, 0, 220, 20998, 25001, 171, 120, 234]

# 解码
reconstructed_string = tokenizer.decode(indices)
# 输出: "Hello, 🌍! 你好!"

assert string == reconstructed_string  # 往返一致性

# 压缩率
compression_ratio = len(string.encode("utf-8")) / len(indices)
# 约 1.5-2.0
```

---

### 方法 1: 基于字符的 Tokenization

**原理：** Unicode 字符串是 Unicode 字符的序列

**转换：**
- 字符 → 码点（整数）：`ord()`
- 码点 → 字符：`chr()`

```python
assert ord("a") == 97
assert ord("🌍") == 127757
assert chr(97) == "a"
assert chr(127757) == "🌍"
```

**实现：**

```python
class CharacterTokenizer(Tokenizer):
    def encode(self, string: str) -> list[int]:
        return list(map(ord, string))

    def decode(self, indices: list[int]) -> str:
        return "".join(map(chr, indices))
```

**测试：**

```python
tokenizer = CharacterTokenizer()
string = "Hello, 🌍! 你好!"
indices = tokenizer.encode(string)
# [72, 101, 108, 108, 111, 44, 32, 127757, 33, 32, 20320, 22909, 33]

reconstructed_string = tokenizer.decode(indices)
assert string == reconstructed_string
```

**问题：**

1. **词汇表太大：** 约 150K Unicode 字符
2. **效率低：** 许多字符非常罕见（如 🌍），词汇表使用效率低
3. **压缩率差：** compression_ratio ≈ 1.0

---

### 方法 2: 基于字节的 Tokenization

**原理：** Unicode 字符串可以表示为字节序列

**UTF-8 编码：** 最常见的 Unicode 编码
- 某些字符用 1 个字节表示：`"a"` → `b"a"`
- 其他字符需要多个字节：`"🌍"` → `b"\xf0\x9f\x8c\x8d"`

**实现：**

```python
class ByteTokenizer(Tokenizer):
    def encode(self, string: str) -> list[int]:
        string_bytes = string.encode("utf-8")
        indices = list(map(int, string_bytes))
        return indices

    def decode(self, indices: list[int]) -> str:
        string_bytes = bytes(indices)
        string = string_bytes.decode("utf-8")
        return string
```

**测试：**

```python
tokenizer = ByteTokenizer()
string = "Hello, 🌍! 你好!"
indices = tokenizer.encode(string)
# [72, 101, 108, 108, 111, 44, 32, 240, 159, 140, 141, 33, 32, 228, 189, 160, 229, 165, 189, 33]

reconstructed_string = tokenizer.decode(indices)
assert string == reconstructed_string
```

**优点：**
- ✅ 词汇表小：256（一个字节可以表示 256 个值）

**问题：**
- ❌ 压缩率糟糕：compression_ratio = 1.0
- ❌ 序列太长
- ❌ 由于 Transformer 的上下文长度有限（注意力是二次的），这不太好...

---

### 方法 3: 基于单词的 Tokenization

**原理：** 将字符串分割成单词（经典 NLP 方法）

**简单正则表达式：**

```python
string = "I'll say supercalifragilisticexpialidocious!"
segments = regex.findall(r"\w+|.", string)
# ['I', 'll', 'say', 'supercalifragilisticexpialidocious', '!']
```

**GPT-2 风格正则表达式：**

```python
GPT2_TOKENIZER_REGEX = r"""'(?:[sdmt]|ll|ve|re)| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+"""

segments = regex.findall(GPT2_TOKENIZER_REGEX, string)
# ['I', "'ll", ' say', ' supercalifragilisticexpialidocious', '!']
```

**问题：**

1. **单词数量巨大：** 类似 Unicode 字符
2. **许多单词罕见：** 模型学不到太多
3. **没有固定词汇表大小**
4. **未见过的单词：** 需要特殊的 UNK token，影响困惑度计算

---

### 方法 4: Byte Pair Encoding (BPE)

**历史：**
- 1994 年由 Philip Gage 引入用于数据压缩
- 2016 年适配到 NLP 用于神经机器翻译（Sennrich et al.）
- GPT-2 使用 BPE

**基本思想：** 在原始文本上**训练** tokenizer 以自动确定词汇表

**直觉：** 常见字符序列用单个 token 表示，罕见序列用多个 token 表示

**算法草图：** 从每个字节作为 token 开始，逐步合并最常见的相邻 token 对

---

#### BPE 训练过程

**示例：**

```python
string = "the cat in the hat"
num_merges = 3
```

**步骤 1：** 从字节开始

```python
indices = [116, 104, 101, 32, 99, 97, 116, 32, 105, 110, 32, 116, 104, 101, 32, 104, 97, 116]
# 对应: t h e   c a t   i n   t h e   h a t
```

**步骤 2：** 统计相邻 token 对的出现次数

```python
counts = {
    (116, 104): 2,  # "th"
    (104, 101): 2,  # "he"
    (101, 32): 2,   # "e "
    (32, 116): 1,   # " t"
    ...
}
```

**步骤 3：** 找到最常见的对并合并

```python
# 第一次合并: (116, 104) -> 256  # "th"
indices = [256, 101, 32, 99, 97, 116, 32, 105, 110, 32, 256, 101, 32, 104, 97, 116]

# 第二次合并: (256, 101) -> 257  # "the"
indices = [257, 32, 99, 97, 116, 32, 105, 110, 32, 257, 32, 104, 97, 116]

# 第三次合并: (257, 32) -> 258  # "the "
indices = [258, 99, 97, 116, 32, 105, 110, 32, 258, 104, 97, 116]
```

**结果：**

```python
vocab = {
    0-255: 原始字节,
    256: b"th",
    257: b"the",
    258: b"the ",
}

merges = {
    (116, 104): 256,
    (256, 101): 257,
    (257, 32): 258,
}
```

---

#### BPE 实现

**数据结构：**

```python
@dataclass(frozen=True)
class BPETokenizerParams:
    vocab: dict[int, bytes]              # index -> bytes
    merges: dict[tuple[int, int], int]   # (index1, index2) -> new_index
```

**训练函数：**

```python
def train_bpe(string: str, num_merges: int) -> BPETokenizerParams:
    # 从字节开始
    indices = list(map(int, string.encode("utf-8")))
    merges = {}
    vocab = {x: bytes([x]) for x in range(256)}
    
    for i in range(num_merges):
        # 统计相邻对
        counts = defaultdict(int)
        for index1, index2 in zip(indices, indices[1:]):
            counts[(index1, index2)] += 1
        
        # 找到最常见的对
        pair = max(counts, key=counts.get)
        index1, index2 = pair
        
        # 合并
        new_index = 256 + i
        merges[pair] = new_index
        vocab[new_index] = vocab[index1] + vocab[index2]
        indices = merge(indices, pair, new_index)
    
    return BPETokenizerParams(vocab=vocab, merges=merges)
```

**合并辅助函数：**

```python
def merge(indices: list[int], pair: tuple[int, int], new_index: int) -> list[int]:
    """将 indices 中所有 pair 的实例替换为 new_index"""
    new_indices = []
    i = 0
    while i < len(indices):
        if i + 1 < len(indices) and indices[i] == pair[0] and indices[i + 1] == pair[1]:
            new_indices.append(new_index)
            i += 2
        else:
            new_indices.append(indices[i])
            i += 1
    return new_indices
```

**Tokenizer 类：**

```python
class BPETokenizer(Tokenizer):
    def __init__(self, params: BPETokenizerParams):
        self.params = params

    def encode(self, string: str) -> list[int]:
        indices = list(map(int, string.encode("utf-8")))
        # 注意：这是一个非常慢的实现
        for pair, new_index in self.params.merges.items():
            indices = merge(indices, pair, new_index)
        return indices

    def decode(self, indices: list[int]) -> str:
        bytes_list = list(map(self.params.vocab.get, indices))
        string = b"".join(bytes_list).decode("utf-8")
        return string
```

---

#### 使用 BPE Tokenizer

**训练：**

```python
string = "the cat in the hat"
params = train_bpe(string, num_merges=3)
```

**编码新文本：**

```python
tokenizer = BPETokenizer(params)
string = "the quick brown fox"
indices = tokenizer.encode(string)
reconstructed_string = tokenizer.decode(indices)
assert string == reconstructed_string
```

---

### Assignment 1 中的改进

在 Assignment 1 中，你需要超越这个基础实现：

1. **优化 encode()：** 当前循环所有合并，只循环相关的合并
2. **特殊 token：** 检测并保留特殊 token（如 `<|endoftext|>`）
3. **预 tokenization：** 使用 GPT-2 tokenizer 正则表达式
4. **性能优化：** 尽可能提高实现速度

---

## 总结

### Tokenization 要点

- **Tokenizer：** 字符串 ↔ token（索引）
- **基于字符、字节、单词的 tokenization：** 高度次优
- **BPE：** 查看语料库统计的有效启发式方法
- **Tokenization 是必要之恶：** 也许有一天我们会直接从字节做起...

### 下节课预告

**主题：** PyTorch 构建块，资源核算

---

## 参考资源

**Tokenization：**
- [Andrej Karpathy's Tokenization Video](https://www.youtube.com/watch?v=zduSFxRajkE)
- [BPE Wikipedia](https://en.wikipedia.org/wiki/Byte_pair_encoding)
- [Original BPE Paper (Gage 1994)](http://www.pennelynn.com/Documents/CUJ/HTML/94HTML/19940045.HTM)
- [BPE for NMT (Sennrich et al. 2016)](https://arxiv.org/abs/1508.07909)
- [tiktoken (OpenAI)](https://github.com/openai/tiktoken)

**课程资源：**
- [Course Website](https://stanford-cs336.github.io/spring2025/)
- [Assignment 1 GitHub](https://github.com/stanford-cs336/assignment1-basics)
- [2024 Leaderboard](https://github.com/stanford-cs336/spring2024-assignment1-basics-leaderboard)
