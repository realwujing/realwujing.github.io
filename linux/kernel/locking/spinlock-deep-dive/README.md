# spin_lock 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, eb3f4b7426cf (v7.1-rc5-26)

---

## 系列目录

| 篇目 | 标题 | 核心内容 |
|------|------|----------|
| [第1篇](./01-types.md) | 三层类型体系 | `arch_spinlock_t` → `raw_spinlock_t` → `spinlock_t` 的层次结构，PREEMPT_RT 差异，lockdep 注解 |
| [第2篇](./02-slowpath.md) | 慢路径深度解析 | qspinlock 获取锁的完整流程：fast path、pending 自旋、MCS 队列、锁传递 |
| 第3篇 | PREEMPT_RT + rwlock | PREEMPT_RT 下 spinlock→rt_mutex 的映射，rwlock 实现解析 |
| 第4篇 | seqlock + API 参考 | 顺序锁的实现，spin_lock 完整 API 速查表 |

---

## 整体架构图

```
 用户层 API                内核层                  硬件/算法层
┌─────────────┐      ┌──────────────┐      ┌──────────────────────┐
│ spin_lock() │      │              │      │                      │
│ spin_unlock()│─────▶│ spinlock_t  │─────▶│  qspinlock / ticket  │
│ spin_lock_irq()│    │              │      │  (arch_spinlock_t)   │
│ spin_lock_bh() │    │ spinlock_types.h│   │ qspinlock_types.h    │
│ ...            │    │   line 17-30 │      │   line 67-93          │
└─────────────┘      └──────┬───────┘      └──────────┬───────────┘
                            │                          │
                  ┌─────────▼─────────┐               │
                  │  raw_spinlock_t   │               │
                  │ spinlock_types_   │               │
                  │   raw.h:14-24     │───────────────┘
                  └─────────┬─────────┘
                            │
                  ┌─────────▼─────────┐
                  │   lockdep 注解     │
                  │  magic, owner_cpu │
                  │  (DEBUG config)   │
                  └───────────────────┘

 PREEMPT_RT 路径:  spinlock_t → rt_mutex_base → 睡眠锁（非自旋）
 非 RT 路径:       spinlock_t → raw_spinlock_t → arch_spinlock_t → 原子操作
```

**核心层级**：

```
spinlock_t                        ← 用户可见的锁类型，对 PREEMPT_RT 透明
  └── raw_spinlock_t              ← 真正的自旋锁，关抢占、中断保护
       └── arch_spinlock_t        ← 架构相关的原子操作实现（qspinlock / ticket lock）
```

---

## 文件索引

| 文件 | 用途 |
|------|------|
| `include/linux/spinlock_types.h` (78 行) | spinlock_t 类型定义（RT/非 RT 分支） |
| `include/linux/spinlock_types_raw.h` (74 行) | raw_spinlock_t 类型，lockdep 注解宏 |
| `include/asm-generic/qspinlock_types.h` (95 行) | arch_spinlock_t 类型，位域布局和掩码 |
| `include/asm-generic/qspinlock.h` (152 行) | qspinlock fast path、unlock、helper 函数 |
| `kernel/locking/qspinlock.c` (410 行) | qspinlock slow path 完整实现 |
| `kernel/locking/qspinlock.h` (201 行) | slow path 共享宏/内联函数 |
| `kernel/locking/mcs_spinlock.h` (113 行) | MCS 锁通用实现 |
| `include/asm-generic/mcs_spinlock.h` (19 行) | MCS 节点结构体定义 |
| `arch/x86/include/asm/qspinlock.h` (39 行) | x86 特定的 pending bit 操作 |
| `include/linux/spinlock.h` (678 行) | spin_lock/spin_unlock 用户 API 宏 |

---

## 前置知识

阅读本系列前，建议掌握：

1. **内核锁基本概念**：临界区、死锁、锁竞争、可重入性
2. **内存模型基础**：acquire/release 语义、store/load barrier、原子操作
3. **内核模块基础**：编译内核模块、使用 `spin_lock`/`spin_unlock`
4. **CPU 架构常识**：缓存一致性（MESI）、cacheline、LOCK 前缀指令
5. **Linux 抢占模型**：CONFIG_PREEMPT、CONFIG_PREEMPT_RT、中断上下文

---

## 预备的预处理器宏

理解系列代码需要知道的关键宏：

```c
// include/linux/compiler.h 系列
__always_inline        // 强制内联，即使未优化
likely() / unlikely()  // 分支预测提示
READ_ONCE()            // 防止编译器优化/分裂读取
WRITE_ONCE()           // 防止编译器优化/分裂写入

// include/linux/compiler_types.h
__section()            // 将函数放入指定段，如 .spinlock.text

// arch/x86/include/asm/barrier.h
smp_store_release()    // 带 release 语义的 store
smp_cond_load_acquire() // 带 acquire 语义的条件 load+spin
smp_wmb()              // 写内存屏障

// include/linux/atomic.h
atomic_cmpxchg()       // 原子 compare-and-swap
atomic_read()          // 原子读取
xchg()                 // 原子交换
```

---

## 快速开始

```bash
# 克隆内核源码
git clone git@github.com:torvalds/linux.git
cd linux
git checkout eb3f4b7426cf

# 查看 spinlock 类型定义
cat include/linux/spinlock_types.h
cat include/linux/spinlock_types_raw.h
cat include/asm-generic/qspinlock_types.h

# 查看慢路径实现
cat kernel/locking/qspinlock.c

# 在 x86 目标上编译并跟踪
make x86_64_defconfig
make -j$(nproc)
objdump -d kernel/locking/qspinlock.o | less
```
