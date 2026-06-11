# RCU 内核源码深度解析 — 系列目录

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)

> 基于 Linux 内核源码逐行分析 RCU (Read-Copy Update) 机制的实现原理。所有行号来自 `/home/wujing/code/linux` 实际源码。

## 系列文章

### [第一篇：RCU 核心 — Grace Period 状态机与合并树](./01-gp-state-machine.md)

深入剖析 RCU 的心脏——`rcu_gp_kthread()` 驱动的 9 状态状态机，以及 `rcu_node` 合并树如何在整个系统中高效收集静默状态（Quiescent State）。从 `rcu_state` 全局状态出发，走过完整的 GP 生命周期。

**源码文件**：`kernel/rcu/tree.h`, `kernel/rcu/tree.c`, `kernel/rcu/tiny.c`

### [第二篇：RCU API — rcu_read_lock、synchronize_rcu、call_rcu 与指针宏](./02-api.md)

详解 RCU 核心 API 的实现细节：`rcu_read_lock/unlock` 的锁依赖注解与抢占控制、`synchronize_rcu()` 的基于 completion 的等待机制、`call_rcu()` 的分段回调列表排队、`rcu_assign_pointer/rcu_dereference` 的内存屏障语义，以及 `rcu_barrier()` 的全系统回调冲刷。

**源码文件**：`include/linux/rcupdate.h`, `kernel/rcu/tree.c`

### [第三篇：SRCU、Tasks RCU 与 Tiny RCU 变体详解](./03-variants.md)

对比分析 RCU 家族三大变体：SRCU (Sleepable RCU) 允许读侧睡眠的独立 srcu_struct 设计、Tasks RCU 基于任务追踪的无锁宽限期、Tiny RCU 在 UP 系统上的极简实现。每种变体都走读关键函数的源码行级实现。

**源码文件**：`kernel/rcu/srcutree.c`, `kernel/rcu/tasks.h`, `kernel/rcu/tiny.c`

### [第四篇：RCU 诊断与调优 — CPU Stall、 Torture 测试与调试接口](./04-diagnostics.md)

覆盖 RCU 的运行时诊断基础设施：CPU Stall 检测机制与 `rcu_check_callbacks()`、rcutorture 模块的压力测试框架、`tree_stall.h` 中的警告消息格式、`rcu_dump_rcu_node_tree()` 的树形结构诊断输出，以及各个 `/sys/kernel/debug/rcu/` 下的调优参数。

**源码文件**：`kernel/rcu/tree_stall.h`, `kernel/rcu/tree_plugin.h`, `kernel/rcu/rcutorture.c`
