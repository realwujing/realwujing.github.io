---
title: 大模型从0到1｜第一讲：概述和Tokenization
date: 2025/11/22 23:22:50
updated: 2025/11/22 23:22:50
---
 
# 大模型从0到1｜第一讲：概述和Tokenization

> 课程链接：[Stanford CS336 Spring 2025 - Lecture 1](https://stanford-cs336.github.io/spring2025-lectures/?trace=var/traces/lecture_01.json)

---

## 第一部分：课程概述与动机

### 1. 为什么需要这门课？

**当前AI教育的空白**：
- 大多数课程教你如何**使用**大模型（调用API、prompt engineering）
- 一些课程教你如何**微调**现有模型（fine-tuning）
- 但很少有课程教你如何**从零构建**一个大模型

**CS336的独特定位**：
```
其他课程: 使用者视角 → 如何调用模型
CS336:    构建者视角 → 如何创造模型
```

**课程目标**：
1. 理解大模型的每一个组件
2. 能够从零实现一个完整的语言模型
3. 掌握训练、评估、部署的完整流程
4. 理解设计决策背后的原理

### 2. 课程结构：从0到1的完整路径

**第一阶段：基础组件（Weeks 1-3）**
```
Week 1: Tokenization          ← 本讲
Week 2: Model Architecture    
Week 3: Training Basics
```

**第二阶段：预训练（Weeks 4-6）**
```
Week 4: Data Processing
Week 5: Optimization
Week 6: Distributed Training
```

**第三阶段：对齐与应用（Weeks 7-9）**
```
Week 7: Instruction Tuning
Week 8: RLHF
Week 9: Evaluation & Deployment
```

**第四阶段：前沿话题（Weeks 10-12）**
```
Week 10: Scaling Laws
Week 11: Efficient Training
Week 12: Future Directions
```

### 3. 学习成果

完成本课程后，你将能够：

**理论层面**：
- 解释每个组件的设计原理
- 理解不同架构选择的权衡
- 掌握训练大模型的关键技术

**实践层面**：
- 从零实现一个Transformer模型
- 训练一个小规模但完整的语言模型
- 评估和优化模型性能

**工程层面**：
- 处理大规模数据
- 实现分布式训练
- 优化推理性能


---

## 第二部分：什么是Tokenization？

### 1. 核心概念

**定义**：Tokenization是将原始文本转换为模型可处理的数字序列的过程。

**完整流程**：
```
原始文本 (Raw Text)
    ↓
分词 (Tokenization)
    ↓
Token序列 (Token Sequence)
    ↓
Token ID序列 (Token ID Sequence)
    ↓
嵌入向量 (Embedding Vectors)
    ↓
模型输入 (Model Input)
```

**示例**：
```python
# 原始文本
text = "Hello, world!"

# 分词
tokens = ["Hello", ",", " world", "!"]

# Token IDs
token_ids = [15496, 11, 995, 0]

# 嵌入向量（假设d_model=768）
embeddings = [
    [0.123, -0.456, ..., 0.789],  # "Hello"的768维向量
    [0.234, -0.567, ..., 0.890],  # ","的768维向量
    [0.345, -0.678, ..., 0.901],  # " world"的768维向量
    [0.456, -0.789, ..., 0.012],  # "!"的768维向量
]
```

### 2. 为什么需要Tokenization？

**问题1：神经网络只能处理数字**
```
文本: "cat"
❌ 不能直接输入神经网络
✓ 需要转换为数字: [3415]
✓ 再转换为向量: [0.1, -0.2, 0.3, ...]
```

**问题2：直接使用字符效率低**
```
文本: "The cat sat on the mat"
字符级: ['T','h','e',' ','c','a','t',' ','s','a','t',' ','o','n',' ','t','h','e',' ','m','a','t']
长度: 22个字符

子词级: ['The', ' cat', ' sat', ' on', ' the', ' mat']
长度: 6个token
```

**问题3：直接使用单词词汇表爆炸**
```
英语单词数: ~170,000
加上变形: ~500,000+
加上专有名词: 无限

子词方案: 30,000-50,000个token即可覆盖
```

### 3. Token粒度的选择

| 粒度 | 词汇表大小 | 序列长度 | 优点 | 缺点 | 使用场景 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **字符级** | 很小（~100） | 很长 | 无OOV，简单 | 序列太长，难学习 | 拼写检查 |
| **子词级** | 中等（30k-50k） | 中等 | 平衡好 | 需要训练 | **现代LLM** |
| **单词级** | 很大（>100k） | 短 | 语义清晰 | OOV严重 | 传统NLP |
| **字节级** | 256 | 最长 | 通用性强 | 序列极长 | 多语言 |

**实际对比**：
```python
text = "The tokenization process is important."

# 字符级（包括空格）
char_tokens = ['T','h','e',' ','t','o','k','e','n','i','z','a','t','i','o','n',' ','p','r','o','c','e','s','s',' ','i','s',' ','i','m','p','o','r','t','a','n','t','.']
print(f"字符级长度: {len(char_tokens)}")  # 38

# 子词级（GPT-2 tokenizer）
subword_tokens = ['The', ' token', 'ization', ' process', ' is', ' important', '.']
print(f"子词级长度: {len(subword_tokens)}")  # 7

# 单词级
word_tokens = ['The', 'tokenization', 'process', 'is', 'important', '.']
print(f"单词级长度: {len(word_tokens)}")  # 6
```

**为什么子词级是最佳选择？**

1. **处理未知词**：
```
单词级: "ChatGPT" → [UNK]（未知）
子词级: "ChatGPT" → ["Chat", "G", "PT"]（可以表示）
```

2. **学习词根和词缀**：
```
"play", "player", "playing", "played"
子词级可以学到 "play" 是共同的词根
```

3. **多语言支持**：
```
英语: "hello" → ["hello"]
中文: "你好" → ["你", "好"]
都可以用同一个tokenizer处理
```


---

## 第三部分：主流Tokenization算法

### 1. BPE (Byte-Pair Encoding)

#### 1.1 算法原理

**历史**：BPE最初是1994年提出的数据压缩算法，2016年被Sennrich等人引入NLP。

**核心思想**：从字符开始，迭代地合并最频繁出现的相邻字符对。

**算法流程**：
```
1. 初始化：将文本分割为字符
2. 统计：计算所有相邻字符对的频率
3. 合并：选择频率最高的字符对，合并为新token
4. 更新：更新词汇表和文本
5. 重复：重复步骤2-4，直到达到目标词汇表大小
```

#### 1.2 详细示例

**训练语料**：
```
"low low low low low"
"lower lower"
"newest newest newest newest newest newest"
```

**训练过程**：

**步骤0：初始化**
```
词汇表: ['l', 'o', 'w', 'e', 'r', 'n', 's', 't', '</w>']
文本表示:
"l o w </w>" (出现5次)
"l o w e r </w>" (出现2次)
"n e w e s t </w>" (出现6次)
```

**步骤1：统计字符对频率**
```
'l o': 7次 (5次在low, 2次在lower)
'o w': 7次
'w </w>': 5次
'w e': 8次 (2次在lower, 6次在newest)
'e w': 6次
'e s': 6次
's t': 6次
't </w>': 6次
...
```

**步骤2：合并最频繁的对 'w e' (8次)**
```
词汇表: ['l', 'o', 'w', 'e', 'r', 'n', 's', 't', '</w>', 'we']
文本表示:
"l o w </w>" (5次)
"l o we r </w>" (2次)
"n e we s t </w>" (6次)
```

**步骤3：继续合并 'e we' (6次)**
```
词汇表: [..., 'we', 'ewe']
文本表示:
"l o w </w>" (5次)
"l o we r </w>" (2次)
"n ewe s t </w>" (6次)
```

**步骤4：合并 'l o' (7次)**
```
词汇表: [..., 'lo']
文本表示:
"lo w </w>" (5次)
"lo we r </w>" (2次)
"n ewe s t </w>" (6次)
```

**继续迭代...**

**最终词汇表可能包含**：
```
基础字符: ['l', 'o', 'w', 'e', 'r', 'n', 's', 't', '</w>']
合并的token: ['lo', 'low', 'we', 'ewe', 'est', 'west', 'newest', ...]
```

#### 1.3 Python实现

```python
import re
from collections import defaultdict, Counter

class BPETokenizer:
    def __init__(self, vocab_size=1000):
        self.vocab_size = vocab_size
        self.vocab = {}
        self.merges = []
    
    def get_stats(self, vocab):
        """统计相邻token对的频率"""
        pairs = defaultdict(int)
        for word, freq in vocab.items():
            symbols = word.split()
            for i in range(len(symbols) - 1):
                pairs[symbols[i], symbols[i + 1]] += freq
        return pairs
    
    def merge_vocab(self, pair, vocab):
        """合并指定的token对"""
        new_vocab = {}
        bigram = ' '.join(pair)
        replacement = ''.join(pair)
        
        for word in vocab:
            new_word = word.replace(bigram, replacement)
            new_vocab[new_word] = vocab[word]
        
        return new_vocab
    
    def train(self, texts):
        """训练BPE tokenizer"""
        # 1. 初始化词汇表（字符级）
        vocab = defaultdict(int)
        for text in texts:
            words = text.split()
            for word in words:
                # 添加词尾标记
                vocab[' '.join(list(word)) + ' </w>'] += 1
        
        # 2. 迭代合并
        num_merges = self.vocab_size - len(set(''.join(texts)))
        
        for i in range(num_merges):
            pairs = self.get_stats(vocab)
            if not pairs:
                break
            
            # 选择频率最高的pair
            best_pair = max(pairs, key=pairs.get)
            vocab = self.merge_vocab(best_pair, vocab)
            
            # 记录合并操作
            self.merges.append(best_pair)
            
            if (i + 1) % 100 == 0:
                print(f"Merge {i+1}: {best_pair} (freq: {pairs[best_pair]})")
        
        # 3. 构建最终词汇表
        self.vocab = self._build_vocab(vocab)
    
    def _build_vocab(self, vocab):
        """从训练结果构建词汇表"""
        tokens = set()
        for word in vocab.keys():
            tokens.update(word.split())
        return {token: idx for idx, token in enumerate(sorted(tokens))}
    
    def tokenize(self, text):
        """使用训练好的BPE进行分词"""
        words = text.split()
        tokens = []
        
        for word in words:
            # 初始化为字符级
            word_tokens = list(word) + ['</w>']
            
            # 应用学到的合并规则
            for pair in self.merges:
                i = 0
                while i < len(word_tokens) - 1:
                    if (word_tokens[i], word_tokens[i + 1]) == pair:
                        word_tokens = (word_tokens[:i] + 
                                     [''.join(pair)] + 
                                     word_tokens[i + 2:])
                    else:
                        i += 1
            
            tokens.extend(word_tokens)
        
        return tokens
    
    def encode(self, text):
        """将文本编码为token IDs"""
        tokens = self.tokenize(text)
        return [self.vocab.get(token, self.vocab.get('<unk>', 0)) 
                for token in tokens]
    
    def decode(self, token_ids):
        """将token IDs解码为文本"""
        inv_vocab = {v: k for k, v in self.vocab.items()}
        tokens = [inv_vocab.get(tid, '<unk>') for tid in token_ids]
        text = ''.join(tokens).replace('</w>', ' ').strip()
        return text

# 使用示例
texts = [
    "low low low low low",
    "lower lower",
    "newest newest newest newest newest newest",
    "widest widest widest"
]

tokenizer = BPETokenizer(vocab_size=50)
tokenizer.train(texts)

# 测试
test_text = "lowest newer"
tokens = tokenizer.tokenize(test_text)
print(f"Tokens: {tokens}")

ids = tokenizer.encode(test_text)
print(f"Token IDs: {ids}")

decoded = tokenizer.decode(ids)
print(f"Decoded: {decoded}")
```

#### 1.4 Byte-Level BPE

**GPT-2引入的改进**：在字节级别而不是字符级别操作。

**优势**：
```
字符级BPE: 需要处理所有Unicode字符（>100k）
字节级BPE: 只需要处理256个字节

示例：
"你好" (Unicode字符)
→ UTF-8编码: [0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD]
→ 6个字节
→ 可以用256个基础token表示任何文本
```

**实现**：
```python
def bytes_to_unicode():
    """
    创建字节到Unicode字符的映射
    避免使用控制字符和空白字符
    """
    bs = (list(range(ord("!"), ord("~") + 1)) + 
          list(range(ord("¡"), ord("¬") + 1)) + 
          list(range(ord("®"), ord("ÿ") + 1)))
    
    cs = bs[:]
    n = 0
    for b in range(2**8):
        if b not in bs:
            bs.append(b)
            cs.append(2**8 + n)
            n += 1
    
    cs = [chr(n) for n in cs]
    return dict(zip(bs, cs))

# GPT-2使用的映射
byte_encoder = bytes_to_unicode()
byte_decoder = {v: k for k, v in byte_encoder.items()}

def encode_text(text):
    """将文本编码为字节序列"""
    return [byte_encoder[b] for b in text.encode('utf-8')]

def decode_bytes(byte_tokens):
    """将字节序列解码为文本"""
    byte_array = bytes([byte_decoder[c] for c in byte_tokens])
    return byte_array.decode('utf-8', errors='replace')
```


### 2. WordPiece

#### 2.1 算法原理

**历史**：由Google开发，用于BERT等模型。

**与BPE的区别**：
```
BPE:       选择频率最高的字符对
WordPiece: 选择能最大化语言模型似然度的字符对
```

**合并准则**：
```
score(x, y) = log P(xy) - log P(x) - log P(y)
            = log [P(xy) / (P(x) × P(y))]

选择score最大的pair进行合并
```

**直觉理解**：
- 如果x和y经常一起出现，P(xy)会很大
- 如果它们独立出现也很频繁，P(x)和P(y)也大
- score衡量的是"一起出现"相对于"独立出现"的增益

#### 2.2 实现示例

```python
import math
from collections import defaultdict

class WordPieceTokenizer:
    def __init__(self, vocab_size=1000):
        self.vocab_size = vocab_size
        self.vocab = {}
    
    def get_pair_scores(self, vocab):
        """计算每个pair的score"""
        # 统计单个token和pair的频率
        token_freq = defaultdict(int)
        pair_freq = defaultdict(int)
        total = 0
        
        for word, freq in vocab.items():
            symbols = word.split()
            for symbol in symbols:
                token_freq[symbol] += freq
                total += freq
            
            for i in range(len(symbols) - 1):
                pair = (symbols[i], symbols[i + 1])
                pair_freq[pair] += freq
        
        # 计算score
        scores = {}
        for pair, freq in pair_freq.items():
            x, y = pair
            # score = log(P(xy) / (P(x) * P(y)))
            p_xy = freq / total
            p_x = token_freq[x] / total
            p_y = token_freq[y] / total
            
            if p_x > 0 and p_y > 0:
                scores[pair] = math.log(p_xy / (p_x * p_y))
        
        return scores
    
    def train(self, texts):
        """训练WordPiece tokenizer"""
        # 初始化
        vocab = defaultdict(int)
        for text in texts:
            words = text.split()
            for word in words:
                vocab[' '.join(list(word)) + ' </w>'] += 1
        
        # 迭代合并
        num_merges = self.vocab_size - len(set(''.join(texts)))
        
        for i in range(num_merges):
            scores = self.get_pair_scores(vocab)
            if not scores:
                break
            
            # 选择score最高的pair
            best_pair = max(scores, key=scores.get)
            vocab = self.merge_vocab(best_pair, vocab)
            
            if (i + 1) % 100 == 0:
                print(f"Merge {i+1}: {best_pair} (score: {scores[best_pair]:.4f})")
        
        self.vocab = self._build_vocab(vocab)
    
    def merge_vocab(self, pair, vocab):
        """合并指定的pair"""
        new_vocab = {}
        bigram = ' '.join(pair)
        replacement = ''.join(pair)
        
        for word in vocab:
            new_word = word.replace(bigram, replacement)
            new_vocab[new_word] = vocab[word]
        
        return new_vocab
    
    def _build_vocab(self, vocab):
        """构建词汇表"""
        tokens = set()
        for word in vocab.keys():
            tokens.update(word.split())
        return {token: idx for idx, token in enumerate(sorted(tokens))}
```

#### 2.3 WordPiece的特殊标记

**BERT使用的标记**：
```python
# 子词前缀
"playing" → ["play", "##ing"]
"unhappiness" → ["un", "##happiness"]

# 特殊token
[CLS]: 句子开始
[SEP]: 句子分隔
[MASK]: 掩码（用于MLM预训练）
[PAD]: 填充
[UNK]: 未知词
```

**示例**：
```python
from transformers import BertTokenizer

tokenizer = BertTokenizer.from_pretrained('bert-base-uncased')

text = "playing football"
tokens = tokenizer.tokenize(text)
print(tokens)  # ['play', '##ing', 'football']

# 完整编码（包含特殊token）
encoded = tokenizer.encode(text, add_special_tokens=True)
print(encoded)  # [101, 2652, 2075, 2374, 102]
# 101: [CLS], 2652: play, 2075: ##ing, 2374: football, 102: [SEP]
```

### 3. Unigram Language Model

#### 3.1 算法原理

**核心思想**：与BPE/WordPiece相反，从大词汇表开始，逐步删除。

**算法流程**：
```
1. 初始化：创建一个大的候选词汇表（所有子串）
2. 训练：使用EM算法估计每个token的概率
3. 剪枝：删除对似然度贡献最小的token
4. 重复：重复步骤2-3，直到达到目标大小
```

**数学基础**：

给定词汇表V和文本X，目标是最大化：
```
L(X) = Σ log P(x) for x in X

其中 P(x) = Σ P(segmentation) × P(tokens in segmentation)
```

对于每个词x，可能有多种分词方式：
```
"unhappiness"可能的分词：
- ["unhappiness"]
- ["un", "happiness"]
- ["un", "happy", "ness"]
- ["u", "n", "h", "a", "p", "p", "i", "n", "e", "s", "s"]
```

#### 3.2 实现示例

```python
import math
from collections import defaultdict

class UnigramTokenizer:
    def __init__(self, vocab_size=1000):
        self.vocab_size = vocab_size
        self.vocab = {}
    
    def initialize_vocab(self, texts):
        """初始化候选词汇表"""
        vocab = set()
        
        for text in texts:
            words = text.split()
            for word in words:
                # 添加所有可能的子串
                for i in range(len(word)):
                    for j in range(i + 1, len(word) + 1):
                        vocab.add(word[i:j])
        
        # 初始化概率（均匀分布）
        prob = 1.0 / len(vocab)
        return {token: prob for token in vocab}
    
    def get_best_segmentation(self, word, vocab):
        """使用动态规划找到最佳分词"""
        n = len(word)
        # dp[i] = (最大log概率, 分词方案)
        dp = [(-float('inf'), [])] * (n + 1)
        dp[0] = (0.0, [])
        
        for i in range(1, n + 1):
            for j in range(i):
                token = word[j:i]
                if token in vocab:
                    score = dp[j][0] + math.log(vocab[token])
                    if score > dp[i][0]:
                        dp[i] = (score, dp[j][1] + [token])
        
        return dp[n][1]
    
    def em_step(self, texts, vocab):
        """EM算法的一步"""
        # E-step: 计算期望计数
        token_count = defaultdict(float)
        total_count = 0
        
        for text in texts:
            words = text.split()
            for word in words:
                segmentation = self.get_best_segmentation(word, vocab)
                for token in segmentation:
                    token_count[token] += 1
                    total_count += 1
        
        # M-step: 更新概率
        new_vocab = {}
        for token in vocab:
            new_vocab[token] = token_count[token] / total_count if total_count > 0 else 0
        
        return new_vocab
    
    def compute_loss(self, texts, vocab):
        """计算当前词汇表的损失"""
        total_loss = 0
        for text in texts:
            words = text.split()
            for word in words:
                segmentation = self.get_best_segmentation(word, vocab)
                word_loss = sum(math.log(vocab[token]) for token in segmentation)
                total_loss += word_loss
        return total_loss
    
    def train(self, texts, num_iterations=10):
        """训练Unigram tokenizer"""
        # 1. 初始化大词汇表
        vocab = self.initialize_vocab(texts)
        print(f"Initial vocab size: {len(vocab)}")
        
        # 2. EM迭代
        for iteration in range(num_iterations):
            vocab = self.em_step(texts, vocab)
            loss = self.compute_loss(texts, vocab)
            print(f"Iteration {iteration + 1}, Loss: {loss:.2f}")
        
        # 3. 剪枝到目标大小
        while len(vocab) > self.vocab_size:
            # 计算删除每个token的损失增加
            loss_increase = {}
            current_loss = self.compute_loss(texts, vocab)
            
            for token in list(vocab.keys()):
                if len(token) == 1:  # 保留单字符
                    continue
                
                # 临时删除token
                temp_vocab = vocab.copy()
                del temp_vocab[token]
                
                # 重新归一化
                total_prob = sum(temp_vocab.values())
                temp_vocab = {k: v / total_prob for k, v in temp_vocab.items()}
                
                # 计算损失增加
                new_loss = self.compute_loss(texts, temp_vocab)
                loss_increase[token] = new_loss - current_loss
            
            # 删除损失增加最小的token
            if loss_increase:
                token_to_remove = min(loss_increase, key=loss_increase.get)
                del vocab[token_to_remove]
                
                # 重新归一化
                total_prob = sum(vocab.values())
                vocab = {k: v / total_prob for k, v in vocab.items()}
                
                if len(vocab) % 100 == 0:
                    print(f"Vocab size: {len(vocab)}, Removed: {token_to_remove}")
        
        self.vocab = vocab
    
    def tokenize(self, text):
        """分词"""
        words = text.split()
        tokens = []
        for word in words:
            segmentation = self.get_best_segmentation(word, self.vocab)
            tokens.extend(segmentation)
        return tokens
```

#### 3.3 Unigram的优势

**1. 多种分词方式**：
```python
# Unigram可以为同一个词提供多种分词
# 在训练时可以采样不同的分词方式，增加鲁棒性

word = "unhappiness"
possible_segmentations = [
    (["unhappiness"], 0.3),
    (["un", "happiness"], 0.5),
    (["un", "happy", "ness"], 0.2)
]

# 可以根据概率采样
import random
def sample_segmentation(word, vocab):
    # 获取所有可能的分词及其概率
    # 根据概率采样
    pass
```

**2. 对噪声更鲁棒**：
```python
# 即使有拼写错误，也能找到合理的分词
"hapiness" → ["hap", "i", "ness"]  # 缺少一个p
"happinesss" → ["happiness", "s"]  # 多了一个s
```

**3. 更灵活的词汇表**：
```python
# 可以轻松调整词汇表大小
# 不需要重新训练，只需要调整剪枝阈值
```


### 4. 三种算法对比总结

| 特性 | BPE | WordPiece | Unigram |
| :--- | :--- | :--- | :--- |
| **训练方向** | 自底向上（合并） | 自底向上（合并） | 自顶向下（删除） |
| **合并/删除准则** | 频率最高 | 似然度增益最大 | 似然度损失最小 |
| **分词确定性** | 确定性 | 确定性 | 可以概率采样 |
| **训练复杂度** | O(n²) | O(n²) | O(n³) |
| **实现难度** | 简单 | 中等 | 复杂 |
| **多语言支持** | 好 | 好 | 最好 |
| **对噪声鲁棒性** | 中等 | 中等 | 好 |
| **代表模型** | GPT系列, Llama | BERT, DistilBERT | T5, mBART, XLNet |

**性能对比实验**：
```python
# 在相同语料上训练三种tokenizer
corpus = load_corpus("wikitext-103")

# BPE
bpe = BPETokenizer(vocab_size=32000)
bpe.train(corpus)
bpe_compression = evaluate_compression(bpe, test_corpus)
bpe_coverage = evaluate_coverage(bpe, test_corpus)

# WordPiece
wp = WordPieceTokenizer(vocab_size=32000)
wp.train(corpus)
wp_compression = evaluate_compression(wp, test_corpus)
wp_coverage = evaluate_coverage(wp, test_corpus)

# Unigram
uni = UnigramTokenizer(vocab_size=32000)
uni.train(corpus)
uni_compression = evaluate_compression(uni, test_corpus)
uni_coverage = evaluate_coverage(uni, test_corpus)

# 结果（示例）
"""
算法        压缩率    覆盖率    训练时间
BPE         4.2      99.5%     10min
WordPiece   4.3      99.6%     15min
Unigram     4.4      99.7%     25min
"""
```

---

## 第四部分：SentencePiece - 生产级实现

### 1. 为什么需要SentencePiece？

**传统tokenizer的问题**：
```python
# 传统流程
text = "Hello world"
# 1. 预分词（依赖语言）
words = text.split()  # ["Hello", "world"]
# 2. 子词分词
tokens = subword_tokenize(words)  # ["Hello", "world"]

# 问题：
# - 依赖空格分词（中文、日文怎么办？）
# - 预处理步骤不可逆（丢失了原始格式）
# - 难以处理多语言
```

**SentencePiece的解决方案**：
```python
# SentencePiece流程
text = "Hello world"
# 直接处理原始文本，将空格视为特殊字符
tokens = sp.encode(text)  # ["▁Hello", "▁world"]

# 优势：
# - 语言无关（不需要预分词）
# - 完全可逆（可以完美还原原文）
# - 统一处理所有语言
```

### 2. SentencePiece的核心特性

#### 2.1 语言无关性

**空格处理**：
```python
import sentencepiece as spm

# 训练
spm.SentencePieceTrainer.train(
    input='corpus.txt',
    model_prefix='sp_model',
    vocab_size=32000,
    character_coverage=0.9995,  # 字符覆盖率
    model_type='bpe'  # 或 'unigram'
)

# 使用
sp = spm.SentencePieceProcessor()
sp.load('sp_model.model')

# 英文（有空格）
text_en = "Hello world"
tokens_en = sp.encode_as_pieces(text_en)
print(tokens_en)  # ['▁Hello', '▁world']

# 中文（无空格）
text_zh = "你好世界"
tokens_zh = sp.encode_as_pieces(text_zh)
print(tokens_zh)  # ['▁你好', '世界']

# 混合
text_mix = "Hello 世界"
tokens_mix = sp.encode_as_pieces(text_mix)
print(tokens_mix)  # ['▁Hello', '▁世界']
```

**▁符号的含义**：
```
▁ (U+2581) 表示原始文本中的空格
这样可以区分：
"hello world" → ["▁hello", "▁world"]
"helloworld"  → ["▁helloworld"]
```

#### 2.2 完全可逆性

```python
# 编码
text = "Hello, world! How are you?"
ids = sp.encode_as_ids(text)
pieces = sp.encode_as_pieces(text)

print(f"Original: {text}")
print(f"IDs: {ids}")
print(f"Pieces: {pieces}")

# 解码
decoded_from_ids = sp.decode_ids(ids)
decoded_from_pieces = sp.decode_pieces(pieces)

print(f"Decoded from IDs: {decoded_from_ids}")
print(f"Decoded from pieces: {decoded_from_pieces}")

# 验证可逆性
assert text == decoded_from_ids
assert text == decoded_from_pieces
```

#### 2.3 子词正则化（Subword Regularization）

**核心思想**：在训练时，对同一个词使用不同的分词方式，增加模型鲁棒性。

```python
# 启用子词正则化
sp = spm.SentencePieceProcessor()
sp.load('sp_model.model')

text = "unhappiness"

# 确定性分词
tokens_deterministic = sp.encode_as_pieces(text)
print(f"Deterministic: {tokens_deterministic}")

# 随机采样不同的分词（用于训练）
for i in range(5):
    tokens_sampled = sp.sample_encode_as_pieces(text, nbest_size=-1, alpha=0.1)
    print(f"Sample {i+1}: {tokens_sampled}")

# 输出示例：
"""
Deterministic: ['▁un', 'happiness']
Sample 1: ['▁un', 'happiness']
Sample 2: ['▁un', 'happy', 'ness']
Sample 3: ['▁unhappiness']
Sample 4: ['▁un', 'hap', 'pi', 'ness']
Sample 5: ['▁un', 'happiness']
"""
```

**参数说明**：
```python
# nbest_size: 考虑的top-k分词方案
# nbest_size=-1: 考虑所有可能的分词
# nbest_size=10: 只考虑概率最高的10种

# alpha: 平滑参数
# alpha=0.0: 总是选择最优分词（确定性）
# alpha=1.0: 完全随机
# alpha=0.1: 轻微随机化（推荐）
```

### 3. SentencePiece实战

#### 3.1 训练自定义tokenizer

```python
import sentencepiece as spm
import os

# 准备训练数据
def prepare_corpus(input_files, output_file):
    """合并多个文件为训练语料"""
    with open(output_file, 'w', encoding='utf-8') as outf:
        for input_file in input_files:
            with open(input_file, 'r', encoding='utf-8') as inf:
                outf.write(inf.read())

# 训练参数
train_params = {
    'input': 'corpus.txt',
    'model_prefix': 'my_tokenizer',
    'vocab_size': 32000,
    'character_coverage': 0.9995,
    'model_type': 'bpe',  # 或 'unigram', 'char', 'word'
    
    # 特殊token
    'pad_id': 0,
    'unk_id': 1,
    'bos_id': 2,
    'eos_id': 3,
    'pad_piece': '[PAD]',
    'unk_piece': '[UNK]',
    'bos_piece': '[BOS]',
    'eos_piece': '[EOS]',
    
    # 额外的特殊token
    'user_defined_symbols': ['[MASK]', '[CLS]', '[SEP]'],
    
    # 训练参数
    'num_threads': 16,
    'max_sentence_length': 16384,
    'shuffle_input_sentence': True,
    'train_extremely_large_corpus': False,
    
    # 字符覆盖率（重要！）
    # 0.9995: 适合大多数语言
    # 1.0: 包含所有字符（可能包含噪声）
    'character_coverage': 0.9995,
    
    # 分词参数
    'split_by_whitespace': True,
    'split_by_number': True,
    'split_by_unicode_script': True,
    
    # 控制词汇表
    'max_sentencepiece_length': 16,
    'byte_fallback': True,  # 使用字节回退处理未知字符
}

# 训练
spm.SentencePieceTrainer.train(**train_params)

print("Training completed!")
print(f"Model saved: my_tokenizer.model")
print(f"Vocab saved: my_tokenizer.vocab")
```

#### 3.2 加载和使用

```python
# 加载模型
sp = spm.SentencePieceProcessor()
sp.load('my_tokenizer.model')

# 基本信息
print(f"Vocab size: {sp.vocab_size()}")
print(f"BOS ID: {sp.bos_id()}")
print(f"EOS ID: {sp.eos_id()}")
print(f"PAD ID: {sp.pad_id()}")
print(f"UNK ID: {sp.unk_id()}")

# 编码
text = "Hello, world! This is a test."

# 方法1: 编码为pieces
pieces = sp.encode_as_pieces(text)
print(f"Pieces: {pieces}")

# 方法2: 编码为IDs
ids = sp.encode_as_ids(text)
print(f"IDs: {ids}")

# 方法3: 同时获取pieces和IDs
pieces_and_ids = [(sp.id_to_piece(id), id) for id in ids]
print(f"Pieces and IDs: {pieces_and_ids}")

# 解码
decoded = sp.decode_ids(ids)
print(f"Decoded: {decoded}")

# 验证
assert text == decoded
```

#### 3.3 批处理

```python
# 批量编码
texts = [
    "Hello, world!",
    "How are you?",
    "I'm fine, thank you."
]

# 编码为IDs
ids_batch = [sp.encode_as_ids(text) for text in texts]

# 填充到相同长度
max_len = max(len(ids) for ids in ids_batch)
padded_ids = [
    ids + [sp.pad_id()] * (max_len - len(ids))
    for ids in ids_batch
]

print(f"Padded batch shape: {len(padded_ids)} x {max_len}")

# 解码
decoded_texts = [sp.decode_ids(ids) for ids in ids_batch]
for original, decoded in zip(texts, decoded_texts):
    assert original == decoded
```

#### 3.4 与Hugging Face集成

```python
from transformers import PreTrainedTokenizerFast
from tokenizers import SentencePieceBPETokenizer

# 方法1: 直接使用SentencePiece
import sentencepiece as spm

class SPTokenizer:
    def __init__(self, model_path):
        self.sp = spm.SentencePieceProcessor()
        self.sp.load(model_path)
    
    def __call__(self, text, **kwargs):
        return {
            'input_ids': self.sp.encode_as_ids(text),
            'attention_mask': [1] * len(self.sp.encode_as_ids(text))
        }
    
    def decode(self, ids, **kwargs):
        return self.sp.decode_ids(ids)

# 方法2: 转换为Hugging Face格式
# 需要先导出为JSON格式
tokenizer = PreTrainedTokenizerFast(
    tokenizer_file="my_tokenizer.json",
    bos_token="[BOS]",
    eos_token="[EOS]",
    unk_token="[UNK]",
    pad_token="[PAD]",
)
```


---

## 第五部分：Tokenization的关键设计决策

### 1. 词汇表大小（Vocabulary Size）

#### 1.1 权衡分析

**小词汇表（如8k-16k）**：
```
优点：
- 嵌入矩阵小（vocab_size × d_model）
- 训练更快
- 显存占用少

缺点：
- 序列更长（每个词被分成更多token）
- 注意力计算成本高（O(n²)）
- 信息密度低
```

**大词汇表（如100k-256k）**：
```
优点：
- 序列更短
- 信息密度高
- 更好的多语言支持

缺点：
- 嵌入矩阵巨大
- 训练慢
- 容易过拟合
```

**实验对比**：
```python
# 在相同文本上测试不同词汇表大小
text = "The quick brown fox jumps over the lazy dog"

# 词汇表大小: 1000
tokenizer_1k = train_tokenizer(corpus, vocab_size=1000)
tokens_1k = tokenizer_1k.encode(text)
print(f"Vocab 1k: {len(tokens_1k)} tokens")  # 例如: 15 tokens

# 词汇表大小: 10000
tokenizer_10k = train_tokenizer(corpus, vocab_size=10000)
tokens_10k = tokenizer_10k.encode(text)
print(f"Vocab 10k: {len(tokens_10k)} tokens")  # 例如: 10 tokens

# 词汇表大小: 50000
tokenizer_50k = train_tokenizer(corpus, vocab_size=50000)
tokens_50k = tokenizer_50k.encode(text)
print(f"Vocab 50k: {len(tokens_50k)} tokens")  # 例如: 9 tokens

# 计算成本对比
def compute_cost(vocab_size, seq_len, d_model=768, num_layers=12):
    # 嵌入层参数
    embedding_params = vocab_size * d_model
    
    # 注意力计算（简化）
    attention_flops = num_layers * seq_len * seq_len * d_model
    
    return {
        'embedding_params': embedding_params,
        'attention_flops': attention_flops,
        'total_cost': embedding_params + attention_flops
    }

for vocab_size, seq_len in [(1000, 15), (10000, 10), (50000, 9)]:
    cost = compute_cost(vocab_size, seq_len)
    print(f"Vocab {vocab_size}, Seq {seq_len}: {cost}")
```

#### 1.2 主流模型的选择

| 模型 | 词汇表大小 | 原因 |
| :--- | :--- | :--- |
| **GPT-2** | 50,257 | 字节级BPE，覆盖所有Unicode |
| **GPT-3** | 50,257 | 与GPT-2相同 |
| **BERT** | 30,522 | WordPiece，英文为主 |
| **RoBERTa** | 50,265 | 字节级BPE |
| **T5** | 32,000 | SentencePiece Unigram |
| **Llama** | 32,000 | SentencePiece BPE |
| **Llama 2** | 32,000 | 与Llama相同 |
| **PaLM** | 256,000 | 大词汇表，多语言 |
| **GPT-4** | ~100,000 | 推测，支持更多语言 |

**趋势观察**：
```
早期模型（2018-2019）: 30k-50k
现代模型（2020-2022）: 32k-50k（标准化）
多语言模型（2022+）: 100k-256k
```

#### 1.3 如何选择词汇表大小？

**决策树**：
```python
def choose_vocab_size(
    languages,           # 支持的语言
    domain,             # 领域（通用/专业）
    model_size,         # 模型参数量
    compute_budget      # 计算预算
):
    if len(languages) == 1 and languages[0] == 'english':
        # 单语言英文
        if model_size < 1e9:  # <1B参数
            return 16000
        elif model_size < 10e9:  # 1B-10B
            return 32000
        else:  # >10B
            return 50000
    
    elif len(languages) <= 5:
        # 少数语言
        return 32000
    
    else:
        # 多语言
        if compute_budget == 'low':
            return 64000
        elif compute_budget == 'medium':
            return 128000
        else:
            return 256000
    
    # 领域调整
    if domain == 'code':
        # 代码需要更大的词汇表
        return int(base_size * 1.5)
    elif domain == 'medical':
        # 医学术语多
        return int(base_size * 1.3)
    
    return base_size
```

### 2. 特殊Token的设计

#### 2.1 必需的特殊Token

```python
class SpecialTokens:
    # 核心特殊token
    PAD = '[PAD]'    # ID: 0, 填充token
    UNK = '[UNK]'    # ID: 1, 未知token
    BOS = '[BOS]'    # ID: 2, 序列开始
    EOS = '[EOS]'    # ID: 3, 序列结束
    
    # 可选特殊token
    CLS = '[CLS]'    # 分类token（BERT）
    SEP = '[SEP]'    # 分隔token（BERT）
    MASK = '[MASK]'  # 掩码token（MLM）
```

**使用场景**：

**1. PAD - 填充**：
```python
# 批处理时对齐序列长度
texts = ["Hello", "Hello world", "Hi"]
tokenized = [tokenizer.encode(t) for t in texts]

# 不同长度
print(tokenized)
# [[15496], [15496, 995], [17250]]

# 填充到相同长度
max_len = max(len(t) for t in tokenized)
padded = [
    t + [tokenizer.pad_id] * (max_len - len(t))
    for t in tokenized
]

print(padded)
# [[15496, 0, 0], [15496, 995, 0], [17250, 0, 0]]
```

**2. BOS/EOS - 序列边界**：
```python
# 生成任务
prompt = "Once upon a time"
input_ids = [tokenizer.bos_id] + tokenizer.encode(prompt)

# 生成
generated_ids = model.generate(input_ids)

# 检测结束
if generated_ids[-1] == tokenizer.eos_id:
    print("Generation completed")
```

**3. MASK - 掩码语言模型**：
```python
# BERT预训练
text = "The cat sat on the mat"
tokens = tokenizer.encode(text)

# 随机掩码15%的token
import random
masked_tokens = tokens.copy()
for i in range(len(masked_tokens)):
    if random.random() < 0.15:
        masked_tokens[i] = tokenizer.mask_id

# 训练目标：预测被掩码的token
```

#### 2.2 自定义特殊Token

```python
# 为特定任务添加特殊token
custom_tokens = [
    '[INST]',      # 指令开始
    '[/INST]',     # 指令结束
    '[SYS]',       # 系统消息
    '[/SYS]',      # 系统消息结束
    '[USER]',      # 用户消息
    '[ASSISTANT]', # 助手消息
]

# 添加到tokenizer
tokenizer.add_special_tokens({'additional_special_tokens': custom_tokens})

# 使用
prompt = "[INST] What is the capital of France? [/INST]"
tokens = tokenizer.encode(prompt)
```

**Llama 2的Chat格式**：
```python
# Llama 2使用特殊格式
template = """<s>[INST] <<SYS>>
{system_prompt}
<</SYS>>

{user_message} [/INST]"""

# 示例
conversation = template.format(
    system_prompt="You are a helpful assistant.",
    user_message="What is the capital of France?"
)

# Tokenize
tokens = tokenizer.encode(conversation)
```

### 3. 预处理策略

#### 3.1 Unicode规范化

```python
import unicodedata

def normalize_text(text, form='NFKC'):
    """
    Unicode规范化
    
    Forms:
    - NFC: Canonical Decomposition, followed by Canonical Composition
    - NFD: Canonical Decomposition
    - NFKC: Compatibility Decomposition, followed by Canonical Composition
    - NFKD: Compatibility Decomposition
    """
    return unicodedata.normalize(form, text)

# 示例
text1 = "café"  # 使用组合字符 é
text2 = "café"  # 使用 e + 重音符

print(f"Text1 length: {len(text1)}")  # 4
print(f"Text2 length: {len(text2)}")  # 5

# 规范化后相同
norm1 = normalize_text(text1, 'NFC')
norm2 = normalize_text(text2, 'NFC')
print(f"Normalized equal: {norm1 == norm2}")  # True
```

#### 3.2 大小写处理

```python
# 策略1: 保留原始大小写（GPT系列）
text = "Hello World"
tokens = tokenizer.encode(text)  # ["Hello", " World"]

# 策略2: 转换为小写（BERT uncased）
text = "Hello World"
text_lower = text.lower()
tokens = tokenizer.encode(text_lower)  # ["hello", " world"]

# 策略3: 混合策略
# - 保留专有名词的大小写
# - 其他词转小写
def smart_lowercase(text):
    words = text.split()
    result = []
    for word in words:
        if is_proper_noun(word):
            result.append(word)
        else:
            result.append(word.lower())
    return ' '.join(result)
```

#### 3.3 空白字符处理

```python
import re

def normalize_whitespace(text):
    """规范化空白字符"""
    # 1. 替换各种空白字符为标准空格
    text = re.sub(r'[\t\n\r\f\v]', ' ', text)
    
    # 2. 合并多个连续空格
    text = re.sub(r' +', ' ', text)
    
    # 3. 去除首尾空格
    text = text.strip()
    
    return text

# 示例
text = "Hello    world\n\nHow  are\tyou?"
normalized = normalize_whitespace(text)
print(f"Normalized: '{normalized}'")
# "Hello world How are you?"
```

#### 3.4 数字处理

```python
# 策略1: 保留原始数字（GPT）
text = "The price is $123.45"
tokens = tokenizer.encode(text)
# ["The", " price", " is", " $", "123", ".", "45"]

# 策略2: 数字分离（某些tokenizer）
# "123" → ["1", "2", "3"]

# 策略3: 数字规范化
def normalize_numbers(text):
    # 将所有数字替换为特殊token
    text = re.sub(r'\d+', '[NUM]', text)
    return text

text = "I have 3 apples and 5 oranges"
normalized = normalize_numbers(text)
# "I have [NUM] apples and [NUM] oranges"

# 策略4: 数字分组
def group_digits(text):
    # 按位数分组
    def replace_number(match):
        num = match.group()
        if len(num) <= 2:
            return num
        else:
            # 长数字分组
            return ' '.join(num[i:i+2] for i in range(0, len(num), 2))
    
    return re.sub(r'\d+', replace_number, text)

text = "The year is 2024"
grouped = group_digits(text)
# "The year is 20 24"
```


---

## 第六部分：多语言Tokenization

### 1. 多语言的挑战

#### 1.1 不同语言的特点

```python
# 英语：空格分隔，字母表小
text_en = "Hello world"
# 特点：26个字母，明确的词边界

# 中文：无空格，字符多
text_zh = "你好世界"
# 特点：数千个常用字，无明确词边界

# 日语：混合文字系统
text_ja = "こんにちは世界"
# 特点：平假名、片假名、汉字混用

# 阿拉伯语：从右到左
text_ar = "مرحبا بالعالم"
# 特点：RTL书写，连写

# 韩语：音节文字
text_ko = "안녕하세요 세계"
# 特点：音节组合，有空格
```

#### 1.2 词汇表分配不均

**问题**：
```python
# 在多语言语料上训练tokenizer
corpus = {
    'english': 1000000,  # 100万句
    'chinese': 100000,   # 10万句
    'swahili': 10000,    # 1万句
}

# 训练后的词汇表分布
vocab_distribution = {
    'english': 25000,  # 78%的词汇表
    'chinese': 6000,   # 19%
    'swahili': 1000,   # 3%
}

# 结果：
# - 英语：高效编码（平均4个字符/token）
# - 中文：中等效率（平均2个字符/token）
# - 斯瓦希里语：低效（接近字符级）
```

**解决方案1：语言采样**
```python
def sample_corpus(corpus, target_distribution):
    """
    按目标分布采样语料
    """
    sampled = {}
    for lang, target_ratio in target_distribution.items():
        # 过采样低资源语言
        if lang in ['swahili', 'zulu', ...]:
            sample_ratio = target_ratio * 2
        else:
            sample_ratio = target_ratio
        
        sampled[lang] = corpus[lang][:int(len(corpus[lang]) * sample_ratio)]
    
    return sampled

# 使用
target_dist = {
    'english': 0.4,   # 40%
    'chinese': 0.3,   # 30%
    'swahili': 0.3,   # 30%（过采样）
}

balanced_corpus = sample_corpus(corpus, target_dist)
```

**解决方案2：字符覆盖率**
```python
# SentencePiece的character_coverage参数
spm.SentencePieceTrainer.train(
    input='multilingual_corpus.txt',
    model_prefix='multilingual_tokenizer',
    vocab_size=250000,  # 更大的词汇表
    character_coverage=0.9995,  # 覆盖99.95%的字符
    # 这会确保低频字符也被包含
)
```

### 2. 多语言Tokenizer的最佳实践

#### 2.1 使用SentencePiece

```python
import sentencepiece as spm

# 训练多语言tokenizer
spm.SentencePieceTrainer.train(
    input='multilingual_corpus.txt',
    model_prefix='multilingual_sp',
    vocab_size=250000,
    character_coverage=0.9995,
    model_type='unigram',  # Unigram对多语言更友好
    
    # 多语言设置
    split_by_unicode_script=True,  # 按Unicode脚本分割
    split_by_whitespace=True,
    split_by_number=True,
    
    # 字节回退
    byte_fallback=True,  # 处理未知字符
    
    # 采样
    input_sentence_size=10000000,  # 使用1000万句训练
    shuffle_input_sentence=True,
)
```

#### 2.2 评估多语言性能

```python
def evaluate_multilingual_tokenizer(tokenizer, test_corpora):
    """
    评估tokenizer在不同语言上的性能
    """
    results = {}
    
    for lang, corpus in test_corpora.items():
        # 1. 压缩率
        total_chars = sum(len(text) for text in corpus)
        total_tokens = sum(len(tokenizer.encode(text)) for text in corpus)
        compression_rate = total_chars / total_tokens
        
        # 2. 未知token比例
        unk_count = 0
        for text in corpus:
            tokens = tokenizer.encode(text)
            unk_count += tokens.count(tokenizer.unk_id)
        unk_ratio = unk_count / total_tokens
        
        # 3. 平均token长度
        avg_token_len = total_chars / total_tokens
        
        results[lang] = {
            'compression_rate': compression_rate,
            'unk_ratio': unk_ratio,
            'avg_token_length': avg_token_len,
        }
    
    return results

# 使用
test_corpora = {
    'english': load_corpus('en'),
    'chinese': load_corpus('zh'),
    'arabic': load_corpus('ar'),
    'swahili': load_corpus('sw'),
}

results = evaluate_multilingual_tokenizer(tokenizer, test_corpora)

# 打印结果
for lang, metrics in results.items():
    print(f"\n{lang}:")
    print(f"  Compression rate: {metrics['compression_rate']:.2f}")
    print(f"  UNK ratio: {metrics['unk_ratio']:.2%}")
    print(f"  Avg token length: {metrics['avg_token_length']:.2f}")

# 期望结果（示例）：
"""
english:
  Compression rate: 4.2
  UNK ratio: 0.01%
  Avg token length: 4.2

chinese:
  Compression rate: 2.1
  UNK ratio: 0.05%
  Avg token length: 2.1

arabic:
  Compression rate: 3.5
  UNK ratio: 0.02%
  Avg token length: 3.5

swahili:
  Compression rate: 3.8
  UNK ratio: 0.10%
  Avg token length: 3.8
"""
```

### 3. 代码和特殊领域

#### 3.1 代码Tokenization

**挑战**：
```python
# 代码有特殊的结构
code = """
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
"""

# 需要保留：
# - 缩进（空格/tab）
# - 标识符（变量名、函数名）
# - 关键字（def, if, return）
# - 运算符（+, -, <=）
# - 括号和标点
```

**解决方案**：
```python
# 1. 训练代码专用tokenizer
spm.SentencePieceTrainer.train(
    input='code_corpus.txt',  # 包含多种编程语言
    model_prefix='code_tokenizer',
    vocab_size=50000,
    
    # 代码特定设置
    split_by_whitespace=False,  # 保留缩进
    treat_whitespace_as_suffix=False,
    
    # 保留特殊字符
    user_defined_symbols=[
        '==', '!=', '<=', '>=',  # 运算符
        '->', '=>',              # 箭头
        '...', '...',            # 省略号
    ],
    
    # 不分割数字
    split_by_number=False,
)

# 2. 使用专门的代码tokenizer（如CodeBERT）
from transformers import RobertaTokenizer

tokenizer = RobertaTokenizer.from_pretrained('microsoft/codebert-base')
tokens = tokenizer.tokenize(code)
print(tokens)
```

#### 3.2 数学公式

```python
# LaTeX公式
formula = r"\frac{-b \pm \sqrt{b^2 - 4ac}}{2a}"

# 挑战：
# - 特殊符号：\, {, }, ^, _
# - 命令：\frac, \sqrt, \pm
# - 需要保持结构

# 解决方案：添加数学符号到词汇表
math_symbols = [
    r'\frac', r'\sqrt', r'\sum', r'\int',
    r'\alpha', r'\beta', r'\gamma',
    r'\pm', r'\times', r'\div',
]

tokenizer.add_tokens(math_symbols)
```

---

## 第七部分：Tokenization的陷阱和最佳实践

### 1. 常见陷阱

#### 1.1 训练-推理不一致

**问题**：
```python
# 训练时
train_tokenizer = load_tokenizer('v1.0')
train_data = tokenizer.encode(texts)

# 几个月后，推理时
inference_tokenizer = load_tokenizer('v2.0')  # ❌ 版本不同！
test_data = tokenizer.encode(texts)

# 结果：token IDs不匹配，模型性能下降
```

**解决方案**：
```python
# 1. 版本控制
class TokenizerConfig:
    version = "1.0.0"
    vocab_size = 32000
    model_type = "bpe"
    # ... 其他配置

# 2. 保存tokenizer和模型一起
model_dir = "my_model_v1"
tokenizer.save(f"{model_dir}/tokenizer")
model.save(f"{model_dir}/model")

# 3. 在模型配置中记录tokenizer信息
config = {
    'model_name': 'my_gpt',
    'tokenizer_version': '1.0.0',
    'tokenizer_hash': compute_hash(tokenizer),
}
```

#### 1.2 特殊字符处理不当

**问题**：
```python
# 表情符号
text = "I love Python! 😍🐍"

# 不当处理
tokens_bad = tokenizer.encode(text)
# 可能结果：["I", " love", " Python", "!", " ", "�", "�", "�", "�"]

# 正确处理（字节级BPE）
tokens_good = byte_level_tokenizer.encode(text)
# ["I", " love", " Python", "!", " 😍", "🐍"]
```

**解决方案**：
```python
# 使用字节级BPE或设置byte_fallback
spm.SentencePieceTrainer.train(
    input='corpus.txt',
    model_prefix='tokenizer',
    vocab_size=32000,
    byte_fallback=True,  # 关键！
    character_coverage=0.9995,
)
```

#### 1.3 空格处理不一致

**问题**：
```python
# 不同的空格处理
text1 = "Hello world"
text2 = "Hello  world"  # 两个空格

# 某些tokenizer
tokens1 = tokenizer.encode(text1)  # ["Hello", " world"]
tokens2 = tokenizer.encode(text2)  # ["Hello", "  world"]  # 不同！

# 解码后
decoded1 = tokenizer.decode(tokens1)  # "Hello world"
decoded2 = tokenizer.decode(tokens2)  # "Hello  world"
```

**解决方案**：
```python
# 预处理：规范化空格
def preprocess(text):
    # 合并多个空格
    text = re.sub(r' +', ' ', text)
    return text

text = preprocess(text)
tokens = tokenizer.encode(text)
```

### 2. 最佳实践

#### 2.1 Tokenizer训练流程

```python
class TokenizerTrainingPipeline:
    def __init__(self, config):
        self.config = config
    
    def prepare_corpus(self, raw_files):
        """1. 准备训练语料"""
        print("Step 1: Preparing corpus...")
        
        # 合并文件
        corpus_file = 'training_corpus.txt'
        with open(corpus_file, 'w', encoding='utf-8') as outf:
            for file in raw_files:
                with open(file, 'r', encoding='utf-8') as inf:
                    for line in inf:
                        # 清洗
                        line = self.clean_text(line)
                        if line.strip():
                            outf.write(line + '\n')
        
        return corpus_file
    
    def clean_text(self, text):
        """2. 文本清洗"""
        # Unicode规范化
        text = unicodedata.normalize('NFKC', text)
        
        # 去除控制字符
        text = ''.join(ch for ch in text if unicodedata.category(ch)[0] != 'C' or ch in '\n\t')
        
        # 规范化空白
        text = re.sub(r'[ \t]+', ' ', text)
        
        return text
    
    def train(self, corpus_file):
        """3. 训练tokenizer"""
        print("Step 2: Training tokenizer...")
        
        spm.SentencePieceTrainer.train(
            input=corpus_file,
            model_prefix=self.config['model_prefix'],
            vocab_size=self.config['vocab_size'],
            character_coverage=self.config['character_coverage'],
            model_type=self.config['model_type'],
            
            # 特殊token
            pad_id=0,
            unk_id=1,
            bos_id=2,
            eos_id=3,
            
            # 训练参数
            num_threads=16,
            train_extremely_large_corpus=True,
            shuffle_input_sentence=True,
        )
    
    def evaluate(self, test_corpus):
        """4. 评估tokenizer"""
        print("Step 3: Evaluating tokenizer...")
        
        sp = spm.SentencePieceProcessor()
        sp.load(f"{self.config['model_prefix']}.model")
        
        metrics = {
            'compression_rate': [],
            'unk_ratio': [],
        }
        
        for text in test_corpus:
            tokens = sp.encode_as_ids(text)
            metrics['compression_rate'].append(len(text) / len(tokens))
            metrics['unk_ratio'].append(tokens.count(sp.unk_id()) / len(tokens))
        
        return {
            'avg_compression_rate': np.mean(metrics['compression_rate']),
            'avg_unk_ratio': np.mean(metrics['unk_ratio']),
        }
    
    def save_metadata(self):
        """5. 保存元数据"""
        print("Step 4: Saving metadata...")
        
        metadata = {
            'version': '1.0.0',
            'config': self.config,
            'training_date': datetime.now().isoformat(),
            'corpus_stats': self.get_corpus_stats(),
        }
        
        with open(f"{self.config['model_prefix']}_metadata.json", 'w') as f:
            json.dump(metadata, f, indent=2)

# 使用
config = {
    'model_prefix': 'my_tokenizer',
    'vocab_size': 32000,
    'character_coverage': 0.9995,
    'model_type': 'bpe',
}

pipeline = TokenizerTrainingPipeline(config)
corpus_file = pipeline.prepare_corpus(raw_files)
pipeline.train(corpus_file)
metrics = pipeline.evaluate(test_corpus)
pipeline.save_metadata()

print(f"Training completed!")
print(f"Metrics: {metrics}")
```

#### 2.2 Tokenizer测试

```python
import unittest

class TokenizerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tokenizer = load_tokenizer('my_tokenizer')
    
    def test_reversibility(self):
        """测试可逆性"""
        texts = [
            "Hello, world!",
            "你好，世界！",
            "مرحبا بالعالم",
            "Hello 世界 مرحبا",
        ]
        
        for text in texts:
            encoded = self.tokenizer.encode(text)
            decoded = self.tokenizer.decode(encoded)
            self.assertEqual(text, decoded, f"Failed for: {text}")
    
    def test_special_tokens(self):
        """测试特殊token"""
        # BOS/EOS
        text = "Hello"
        encoded = self.tokenizer.encode(text, add_special_tokens=True)
        self.assertEqual(encoded[0], self.tokenizer.bos_id)
        self.assertEqual(encoded[-1], self.tokenizer.eos_id)
        
        # PAD
        self.assertIsNotNone(self.tokenizer.pad_id)
    
    def test_empty_string(self):
        """测试空字符串"""
        encoded = self.tokenizer.encode("")
        self.assertEqual(len(encoded), 0)
    
    def test_long_text(self):
        """测试长文本"""
        long_text = "Hello " * 10000
        encoded = self.tokenizer.encode(long_text)
        decoded = self.tokenizer.decode(encoded)
        self.assertEqual(long_text, decoded)
    
    def test_special_characters(self):
        """测试特殊字符"""
        special_texts = [
            "Hello\nWorld",  # 换行
            "Hello\tWorld",  # Tab
            "Hello😀World",  # Emoji
            "Hello\u200bWorld",  # 零宽空格
        ]
        
        for text in special_texts:
            encoded = self.tokenizer.encode(text)
            decoded = self.tokenizer.decode(encoded)
            self.assertEqual(text, decoded)
    
    def test_consistency(self):
        """测试一致性"""
        text = "Hello, world!"
        
        # 多次编码应该得到相同结果
        encoded1 = self.tokenizer.encode(text)
        encoded2 = self.tokenizer.encode(text)
        self.assertEqual(encoded1, encoded2)

# 运行测试
if __name__ == '__main__':
    unittest.main()
```


---

## 第八部分：实战案例分析

### 1. GPT-2/GPT-3 Tokenizer深度解析

#### 1.1 设计特点

```python
from transformers import GPT2Tokenizer

tokenizer = GPT2Tokenizer.from_pretrained('gpt2')

# 关键特性
print(f"Vocab size: {tokenizer.vocab_size}")  # 50257
print(f"Model max length: {tokenizer.model_max_length}")  # 1024

# 特殊token
print(f"BOS token: {tokenizer.bos_token}")  # '<|endoftext|>'
print(f"EOS token: {tokenizer.eos_token}")  # '<|endoftext|>'
print(f"PAD token: {tokenizer.pad_token}")  # None（GPT-2没有PAD）
```

**字节级BPE的实现**：
```python
# GPT-2使用字节级BPE
text = "Hello, 世界! 😀"

# 1. 转换为字节
bytes_text = text.encode('utf-8')
print(f"Bytes: {bytes_text}")
# b'Hello, \xe4\xb8\x96\xe7\x95\x8c! \xf0\x9f\x98\x80'

# 2. 字节到Unicode映射
byte_encoder = tokenizer.byte_encoder
unicode_text = ''.join([byte_encoder[b] for b in bytes_text])
print(f"Unicode representation: {unicode_text}")

# 3. BPE分词
tokens = tokenizer.tokenize(text)
print(f"Tokens: {tokens}")
# ['Hello', ',', ' äļ', 'ĭ', 'ķ', 'çĥ', '¼', '!', ' ðŁĺ', 'Ģ']

# 4. Token IDs
ids = tokenizer.encode(text)
print(f"IDs: {ids}")
```

**为什么词汇表是50257？**
```python
# 256个字节 + 50000个合并 + 1个特殊token
base_vocab = 256  # 所有可能的字节
num_merges = 50000  # BPE合并次数
special_tokens = 1  # <|endoftext|>

total = base_vocab + num_merges + special_tokens
print(f"Total vocab size: {total}")  # 50257
```

#### 1.2 实际使用

```python
# 示例1：基本编码解码
text = "The quick brown fox jumps over the lazy dog."
tokens = tokenizer.tokenize(text)
ids = tokenizer.encode(text)
decoded = tokenizer.decode(ids)

print(f"Original: {text}")
print(f"Tokens: {tokens}")
print(f"IDs: {ids}")
print(f"Decoded: {decoded}")

# 示例2：批处理
texts = [
    "Hello, world!",
    "How are you?",
    "I'm fine, thank you."
]

# 编码（自动填充）
encoded = tokenizer(
    texts,
    padding=True,
    truncation=True,
    max_length=512,
    return_tensors='pt'
)

print(f"Input IDs shape: {encoded['input_ids'].shape}")
print(f"Attention mask shape: {encoded['attention_mask'].shape}")

# 示例3：生成任务
prompt = "Once upon a time"
input_ids = tokenizer.encode(prompt, return_tensors='pt')

# 生成（假设有模型）
# output_ids = model.generate(input_ids, max_length=50)
# generated_text = tokenizer.decode(output_ids[0])
```

### 2. BERT Tokenizer深度解析

#### 2.1 WordPiece特点

```python
from transformers import BertTokenizer

tokenizer = BertTokenizer.from_pretrained('bert-base-uncased')

# 关键特性
print(f"Vocab size: {tokenizer.vocab_size}")  # 30522
print(f"Do lower case: {tokenizer.do_lower_case}")  # True

# 特殊token
print(f"CLS token: {tokenizer.cls_token}")  # '[CLS]'
print(f"SEP token: {tokenizer.sep_token}")  # '[SEP]'
print(f"PAD token: {tokenizer.pad_token}")  # '[PAD]'
print(f"MASK token: {tokenizer.mask_token}")  # '[MASK]'
```

**WordPiece的##前缀**：
```python
# 子词使用##前缀
text = "playing football"
tokens = tokenizer.tokenize(text)
print(f"Tokens: {tokens}")
# ['play', '##ing', 'football']

# 完整的编码（包含特殊token）
encoded = tokenizer.encode(text, add_special_tokens=True)
print(f"Encoded: {encoded}")
# [101, 2652, 2075, 2374, 102]
# 101: [CLS], 2652: play, 2075: ##ing, 2374: football, 102: [SEP]

# 解码
decoded = tokenizer.decode(encoded)
print(f"Decoded: {decoded}")
# "[CLS] playing football [SEP]"
```

#### 2.2 BERT的特殊用法

```python
# 1. 单句分类
text = "This movie is great!"
encoded = tokenizer(
    text,
    add_special_tokens=True,
    max_length=512,
    padding='max_length',
    truncation=True,
    return_tensors='pt'
)
# 格式: [CLS] This movie is great ! [SEP] [PAD] [PAD] ...

# 2. 句子对任务（如NLI）
text_a = "The cat sat on the mat."
text_b = "A cat was sitting on a mat."

encoded = tokenizer(
    text_a,
    text_b,
    add_special_tokens=True,
    max_length=512,
    padding='max_length',
    truncation=True,
    return_tensors='pt'
)
# 格式: [CLS] text_a [SEP] text_b [SEP] [PAD] ...

# token_type_ids区分两个句子
print(f"Token type IDs: {encoded['token_type_ids']}")
# [0, 0, 0, ..., 0, 1, 1, ..., 1, 0, 0, ...]
#  ↑ text_a      ↑ text_b      ↑ padding

# 3. 掩码语言模型
text = "The cat sat on the [MASK]."
encoded = tokenizer.encode(text)
mask_token_id = tokenizer.mask_token_id

print(f"Encoded: {encoded}")
print(f"MASK token ID: {mask_token_id}")
# 模型预测[MASK]位置的token
```

### 3. Llama Tokenizer深度解析

#### 3.1 SentencePiece BPE

```python
from transformers import LlamaTokenizer

tokenizer = LlamaTokenizer.from_pretrained('meta-llama/Llama-2-7b-hf')

# 关键特性
print(f"Vocab size: {tokenizer.vocab_size}")  # 32000
print(f"BOS token: {tokenizer.bos_token}")  # '<s>'
print(f"EOS token: {tokenizer.eos_token}")  # '</s>'
print(f"UNK token: {tokenizer.unk_token}")  # '<unk>'
```

**SentencePiece的▁符号**：
```python
text = "Hello world"
tokens = tokenizer.tokenize(text)
print(f"Tokens: {tokens}")
# ['▁Hello', '▁world']

# 注意：▁表示原始空格
text_no_space = "Helloworld"
tokens_no_space = tokenizer.tokenize(text_no_space)
print(f"Tokens (no space): {tokens_no_space}")
# ['▁Hello', 'world']  # 第二个token没有▁
```

#### 3.2 Llama 2的Chat格式

```python
# Llama 2 Chat使用特殊格式
B_INST, E_INST = "[INST]", "[/INST]"
B_SYS, E_SYS = "<<SYS>>\n", "\n<</SYS>>\n\n"

def format_llama2_prompt(system_prompt, user_message):
    """格式化Llama 2 Chat提示"""
    return f"<s>{B_INST} {B_SYS}{system_prompt}{E_SYS}{user_message} {E_INST}"

# 使用
system_prompt = "You are a helpful, respectful and honest assistant."
user_message = "What is the capital of France?"

prompt = format_llama2_prompt(system_prompt, user_message)
print(prompt)

# Tokenize
tokens = tokenizer.encode(prompt)
print(f"Token count: {len(tokens)}")
```

---

## 第九部分：性能优化

### 1. 编码速度优化

```python
import time
import numpy as np

def benchmark_tokenizer(tokenizer, texts, num_runs=100):
    """测试tokenizer性能"""
    
    # 1. 单个文本编码
    start = time.time()
    for _ in range(num_runs):
        for text in texts:
            _ = tokenizer.encode(text)
    single_time = time.time() - start
    
    # 2. 批量编码
    start = time.time()
    for _ in range(num_runs):
        _ = tokenizer.batch_encode_plus(texts)
    batch_time = time.time() - start
    
    # 3. 并行编码（如果支持）
    if hasattr(tokenizer, 'enable_parallelism'):
        tokenizer.enable_parallelism(True)
        start = time.time()
        for _ in range(num_runs):
            _ = tokenizer.batch_encode_plus(texts)
        parallel_time = time.time() - start
    else:
        parallel_time = None
    
    return {
        'single': single_time,
        'batch': batch_time,
        'parallel': parallel_time,
        'speedup_batch': single_time / batch_time,
        'speedup_parallel': single_time / parallel_time if parallel_time else None,
    }

# 测试
texts = ["Hello world"] * 1000
results = benchmark_tokenizer(tokenizer, texts)

print("Performance Results:")
print(f"Single encoding: {results['single']:.3f}s")
print(f"Batch encoding: {results['batch']:.3f}s")
print(f"Speedup (batch): {results['speedup_batch']:.2f}x")
if results['parallel']:
    print(f"Parallel encoding: {results['parallel']:.3f}s")
    print(f"Speedup (parallel): {results['speedup_parallel']:.2f}x")
```

### 2. 内存优化

```python
# 1. 使用生成器处理大文件
def tokenize_large_file(file_path, tokenizer, batch_size=1000):
    """流式处理大文件"""
    def read_batches():
        batch = []
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                batch.append(line.strip())
                if len(batch) >= batch_size:
                    yield batch
                    batch = []
            if batch:
                yield batch
    
    for batch in read_batches():
        encoded = tokenizer.batch_encode_plus(
            batch,
            padding=True,
            truncation=True,
            max_length=512,
            return_tensors='pt'
        )
        # 处理encoded batch
        yield encoded

# 2. 缓存常用编码
from functools import lru_cache

@lru_cache(maxsize=10000)
def cached_encode(text, tokenizer_name):
    """缓存编码结果"""
    tokenizer = load_tokenizer(tokenizer_name)
    return tuple(tokenizer.encode(text))

# 3. 使用更紧凑的数据类型
import numpy as np

# 默认：int64
ids_int64 = np.array(tokenizer.encode(text), dtype=np.int64)
print(f"Size (int64): {ids_int64.nbytes} bytes")

# 优化：int32（词汇表<2^31）
ids_int32 = np.array(tokenizer.encode(text), dtype=np.int32)
print(f"Size (int32): {ids_int32.nbytes} bytes")

# 进一步优化：int16（词汇表<2^15）
if tokenizer.vocab_size < 32768:
    ids_int16 = np.array(tokenizer.encode(text), dtype=np.int16)
    print(f"Size (int16): {ids_int16.nbytes} bytes")
```

### 3. 分布式Tokenization

```python
from multiprocessing import Pool
import os

def tokenize_chunk(args):
    """处理一个数据块"""
    texts, tokenizer_path = args
    
    # 在子进程中加载tokenizer
    tokenizer = load_tokenizer(tokenizer_path)
    
    results = []
    for text in texts:
        encoded = tokenizer.encode(text)
        results.append(encoded)
    
    return results

def parallel_tokenize(texts, tokenizer_path, num_workers=None):
    """并行tokenization"""
    if num_workers is None:
        num_workers = os.cpu_count()
    
    # 分割数据
    chunk_size = len(texts) // num_workers
    chunks = [
        texts[i:i + chunk_size]
        for i in range(0, len(texts), chunk_size)
    ]
    
    # 并行处理
    with Pool(num_workers) as pool:
        args = [(chunk, tokenizer_path) for chunk in chunks]
        results = pool.map(tokenize_chunk, args)
    
    # 合并结果
    all_encoded = []
    for chunk_results in results:
        all_encoded.extend(chunk_results)
    
    return all_encoded

# 使用
texts = load_large_corpus()
encoded = parallel_tokenize(texts, 'my_tokenizer.model', num_workers=8)
```

---

## 总结

### 核心要点回顾

1. **Tokenization是LLM的第一步**：
   - 将文本转换为数字序列
   - 平衡词汇表大小和序列长度
   - 影响模型的效率和能力

2. **三种主流算法**：
   - **BPE**：简单高效，自底向上合并
   - **WordPiece**：基于似然度，BERT使用
   - **Unigram**：自顶向下删除，灵活鲁棒

3. **SentencePiece是生产标准**：
   - 语言无关
   - 完全可逆
   - 支持子词正则化

4. **关键设计决策**：
   - 词汇表大小：30k-50k是平衡点
   - 特殊token：PAD, UNK, BOS, EOS等
   - 预处理：Unicode规范化、空格处理

5. **多语言挑战**：
   - 词汇表分配不均
   - 需要语言采样和字符覆盖率调整
   - Unigram对多语言更友好

6. **最佳实践**：
   - 版本控制和元数据
   - 全面的测试（可逆性、特殊字符）
   - 性能优化（批处理、并行、缓存）

### 实践建议

**选择tokenizer**：
```
英文为主 → BPE (GPT风格)
多任务 → WordPiece (BERT风格)
多语言 → Unigram (T5风格)
生产环境 → SentencePiece
```

**词汇表大小**：
```
小模型(<1B) → 16k-32k
中等模型(1B-10B) → 32k-50k
大模型(>10B) → 50k-100k
多语言 → 100k-256k
```

**训练流程**：
```
1. 准备和清洗语料
2. 选择算法和参数
3. 训练tokenizer
4. 评估性能（压缩率、覆盖率）
5. 测试边界情况
6. 保存元数据和版本信息
```

### 下一步学习

**第二讲预告**：模型架构
- Transformer详解
- 注意力机制
- 位置编码
- 前馈网络

**推荐资源**：
- [Neural Machine Translation of Rare Words with Subword Units](https://arxiv.org/abs/1508.07909) - BPE原始论文
- [SentencePiece: A simple and language independent approach](https://arxiv.org/abs/1808.06226) - SentencePiece论文
- [Subword Regularization](https://arxiv.org/abs/1804.10959) - Unigram论文
- [Hugging Face Tokenizers文档](https://huggingface.co/docs/tokenizers/)
- [SentencePiece GitHub](https://github.com/google/sentencepiece)

---

## 附录：快速参考

### A. 常用代码片段

**训练BPE tokenizer**：
```python
from tokenizers import Tokenizer
from tokenizers.models import BPE
from tokenizers.trainers import BpeTrainer

tokenizer = Tokenizer(BPE(unk_token="[UNK]"))
trainer = BpeTrainer(vocab_size=30000, special_tokens=["[UNK]", "[PAD]", "[BOS]", "[EOS]"])
tokenizer.train(files=["corpus.txt"], trainer=trainer)
tokenizer.save("tokenizer.json")
```

**训练SentencePiece**：
```python
import sentencepiece as spm

spm.SentencePieceTrainer.train(
    input='corpus.txt',
    model_prefix='sp_model',
    vocab_size=32000,
    model_type='bpe',
    character_coverage=0.9995
)
```

**使用Hugging Face tokenizer**：
```python
from transformers import AutoTokenizer

tokenizer = AutoTokenizer.from_pretrained('gpt2')
encoded = tokenizer("Hello, world!", return_tensors='pt')
decoded = tokenizer.decode(encoded['input_ids'][0])
```

### B. 调试检查清单

- [ ] 可逆性：`text == tokenizer.decode(tokenizer.encode(text))`
- [ ] 特殊字符：测试emoji、Unicode、控制字符
- [ ] 空字符串：`tokenizer.encode("")`应该返回空或只有特殊token
- [ ] 长文本：测试超长文本的处理
- [ ] 批处理：验证批处理和单个处理结果一致
- [ ] 多语言：测试不同语言的编码效率
- [ ] 版本一致性：确保训练和推理使用相同版本

### C. 性能基准

| 操作 | GPT-2 | BERT | Llama | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| 编码速度 | ~10k tokens/s | ~8k tokens/s | ~12k tokens/s | 单线程 |
| 解码速度 | ~15k tokens/s | ~12k tokens/s | ~18k tokens/s | 单线程 |
| 内存占用 | ~200MB | ~150MB | ~100MB | 模型文件 |
| 加载时间 | ~0.5s | ~0.3s | ~0.2s | 首次加载 |

希望这份详细的Tokenization笔记能帮助你深入理解大模型的第一步！
