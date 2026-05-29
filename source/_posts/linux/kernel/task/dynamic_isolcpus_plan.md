---
title: '动态隔离核心 (Dynamic Housekeeping & Enhanced Isolation - DHEI) 开发计划'
date: '2026/02/06 04:05:16'
updated: '2026/02/06 04:05:16'
---

# 动态隔离核心 (Dynamic Housekeeping & Enhanced Isolation - DHEI) 开发计划

## 1. 项目核心目标
打破 Linux 内核中 CPU 隔离（Housekeeping）必须通过引导参数（`isolcpus` / `nohz_full`）配置且重启生效的限制。实现一个可以在运行时通过 `sysfs` 接口动态调整隔离掩码，并自动同步到 IRQ, RCU, 调度域, NOHZ_FULL, 工作队列, 监控狗等子系统的完整框架。

## 2. 系统架构设计
DHEI 采用**通知链（Notifier Chain）驱动模型**：
- **核心组件**：`kernel/sched/isolation.c` 负责管理全局隔离掩码，并通过 `housekeeping_mutex` 保证并发更新安全。
- **触发机制**：用户写入 `/sys/kernel/housekeeping/xx`。
- **分发机制**：`HK_UPDATE_MASK` 通知的广播。
- **同步机制**：各子系统订阅通知并执行热迁移逻辑（如重映射 IRQ 亲和性、踢出 NOHZ_FULL CPU、重新绑定内核线程等）。

## 3. 详细开发路线 (Commit-by-Commit)

### Phase 1: 基础框架解耦 (Infrastructure Decoupling)

- **Commit 1**: `caa6eeef47aa` - 移除了 `housekeeping.c` 中的 `__init` 限制。
    - 将引导时分配改用 `alloc_cpumask_var()`，根据 `system_state` 动态选择 `GFP_NOWAIT` 或 `GFP_KERNEL`。
    - 引入 `housekeeping_mutex` 保护全局掩码数据结构。

- **Commit 2**: `300a8fdb1c4e` - 引入 `blocking_notifier_chain`。
    - 定义了 `struct housekeeping_update` 结构体，用于在掩码变化时传递类型和新的亲和性列表。
    - 提供了核心通知辅助函数 `housekeeping_update_notify()`。

### Phase 2: 子系统动态同步实现 (Subsystem Synchronization)

- **Commit 3**: `ce83d6c0f482` - **genirq** 动态迁移。
    - 实现了对管理中断（Managed IRQs）的响应逻辑，通过迭代所有中断并调用 `irq_set_affinity_locked()` 将中断从新隔离的 CPU 迁移到新 housekeeping CPU。

- **Commit 4**: `86489c76a857` - **RCU** 掩码同步。
    - 实现了 `rcu_housekeeping_reconfigure()`。
    - 动态更新 `rcu_state.gp_kthread`（RCU 宽限期内核线程）的亲和性。

- **Commit 5**: `88b36ffd8d3c` - **调度器核心** 域调整。
    - 在 `HK_TYPE_DOMAIN` 更新时触发 `rebuild_sched_domains()`。
    - 实现了在运行时将 CPU 动态隔离出（或拉回）普通的调度负载均衡领域。

- **Commit 6**: `2c25c75b053d` - **Watchdog** 亲和性切换。
    - 动态更新 `watchdog_cpumask`。
    - 调用 `proc_watchdog_update()` 使硬锁检测线程同步移动。

- **Commit 7**: `6c708991a41a` - **Workqueue (unbound)** 支持。
    - 绑定 `HK_TYPE_WQ` 通知。
    - 动态更新 `wq_unbound_cpumask` 和 `wq_isolated_cpumask`，确保无绑定工作队列不占用隔离核心。

- **Commit 8**: `23ba0d066e70` - **kcompactd** 线程同步。
    - 解决了内存压缩线程（kcompactd）在隔离 CPU 上运行导致高抖动的问题，实现了动态亲和性重亲。

### Phase 3: 用户接口与高级特性 (Interface & Advanced Features)

- **Commit 9**: `ae1753a2997f` - **sysfs 接口实现**。
    - 在 `/sys/kernel/housekeeping/` 目录下创建子节点：`timer`, `rcu`, `misc`, `tick`, `domain`, `workqueue`, `managed_irq`, `kthread`。
    - 实现了各类型隔离任务的长久分离控制。

- **Commit 10**: `027043166da7` - **动态 NOHZ_FULL 切换**。
    - 技术最为复杂的一环：移除了 `tick-sched` 中关于 full dynticks 的初始化限制。
    - 调用 `tick_nohz_full_kick_cpu()` 在掩码改变后强制所有 CPU 重新评估 tick 依赖，实现非重启开启 tick 隔离。

- **Commit 11 & 12**: `8babc98515e3` - **SMT 感知与安全性**。
    - 引入 `smt_aware_mode`：当开启时，隔离一个 CPU 会自动隔离其所有的超线程兄弟核心。
    - 实现了 **"Last Housekeeping CPU"** 防护：禁止用户隔离所有的在线 CPU，确保系统不会死锁。

- **Commit 13**: `9d50c77451c9` - **引导参数接管与桥接**。
    - 将 DHEI 接口与传统的 `isolcpus=` 引导链条对接，确保启动配置能被运行时接口完美继承和修改。

## 4. 使用说明
用户可以通过以下方式在运行时改变隔离策略：
```bash
# 查看当前计时器 housekeeping CPU
cat /sys/kernel/housekeeping/timer

# 动态隔离 CPU 4-7，让它们不再处理任何内核杂务（NOHZ_FULL 等）
echo 0-3 > /sys/kernel/housekeeping/tick
echo 0-3 > /sys/kernel/housekeeping/timer
echo 0-3 > /sys/kernel/housekeeping/rcu
```

## 5. 总结
DHEI 使得 Linux 内核能够更好地适应现代云原生和高频交易等对延迟极端敏感的场景，赋予了管理员在不中断业务的情况下根据负载动态调整隔离核心的能力。
