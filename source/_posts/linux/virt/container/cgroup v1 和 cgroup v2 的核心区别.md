---
title: 'cgroup v1 和 cgroup v2 的核心区别'
date: '2025/11/25 17:15:02'
updated: '2025/11/25 17:15:02'
---

# cgroup v1 和 cgroup v2 的核心区别

以下是 cgroup v1 和 cgroup v2 的核心区别总结，基于本次聊天记录的讨论内容，从架构设计、功能实现到实际应用进行全面对比：

---

**1. 架构设计**

| 特性               | cgroup v1                          | cgroup v2                          |
|------------------------|----------------------------------------|----------------------------------------|
| 层级结构           | 多棵树：每个控制器（如 CPU、内存）独立挂载，形成多套层级 | 单一树：所有控制器统一挂载到一棵层级树中 |
| 进程归属           | 一个进程可属于不同控制器的多个 cgroup，规则可能冲突 | 一个进程只能属于一个 cgroup，所有控制器原子化生效 |
| 控制器管理         | 控制器分散，需单独配置（如 `cpu.shares` 和 `memory.limit` 分开） | 控制器统一，通过 `cgroup.subtree_control` 动态启用 |

---

**2. 功能实现**

| 功能               | cgroup v1                          | cgroup v2                          |
|------------------------|----------------------------------------|----------------------------------------|
| CPU 控制           | `cpu.shares`（相对权重，默认 1024）    | `cpu.weight`（1-10000，更精细）        |
| 内存控制           | 硬限制（`memory.limit_in_bytes`），无压力反馈 | 支持柔性限制（`memory.high` + `memory.max`）和 OOM 优先级 |
| IO 控制            | `blkio.weight`（简单权重）             | `io.weight` + `io.max`（精确带宽/IPOS 限制） |
| 设备控制           | 通过 `devices` 控制器黑白名单           | 移除 `devices`，改用 eBPF 精细化控制    |
| 网络控制           | `net_cls` 和 `net_prio` 分开           | 移除，依赖 eBPF 或 cgroup-aware 网络栈  |

---

**3. 进程管理**

| 操作               | cgroup v1                          | cgroup v2                          |
|------------------------|----------------------------------------|----------------------------------------|
| 添加进程           | 需写入多个控制器的 `tasks` 文件（如 `echo PID > /sys/fs/cgroup/cpu/g1/tasks`） | 统一写入 `cgroup.procs`（如 `echo PID > /sys/fs/cgroup/g1/cgroup.procs`） |
| 子进程继承         | 子进程默认在根 cgroup，需手动迁移       | 子进程自动继承父进程 cgroup             |
| 多进程支持         | 支持，但需分别操作每个控制器             | 支持，统一管理，无冲突风险             |

---

**4. 性能与资源隔离**

| 场景               | cgroup v1                          | cgroup v2                          |
|------------------------|----------------------------------------|----------------------------------------|
| NUMA 亲和性        | 需手动协调 `cpuset` 和 `memory` 控制器 | 统一通过 `cpuset.cpus` + `cpuset.mems` 设置 |
| 资源冲突风险       | 高（如 CPU 宽松但内存严格可能导致 OOM） | 低（所有控制器协同限制）               |
| 延迟敏感型应用     | 无直接支持                             | 提供 `cpu.latency` 和 `io.latency` 接口 |

---

**5. 实际应用示例**
**(1) 绑定 Redis 到 NUMA 节点**
• cgroup v1：  

  ```bash
  # 需分别设置 cpu 和 memory 控制器
  echo 0-7 > /sys/fs/cgroup/cpuset/redis/cpuset.cpus
  echo 0 > /sys/fs/cgroup/cpuset/redis/cpuset.mems
  echo PID > /sys/fs/cgroup/cpuset/redis/tasks
  echo PID > /sys/fs/cgroup/memory/redis/tasks
  ```

• cgroup v2：  

  ```bash
  # 统一设置
  echo "0-7" > /sys/fs/cgroup/redis/cpuset.cpus
  echo "0" > /sys/fs/cgroup/redis/cpuset.mems
  echo PID > /sys/fs/cgroup/redis/cgroup.procs
  ```

**(2) 查看进程归属**
• cgroup v1：  

  ```bash
  cat /proc/PID/cgroup
  # 输出示例：
  # 1:cpu:/redis
  # 2:memory:/redis
  ```

• cgroup v2：  

  ```bash
  cat /proc/PID/cgroup
  # 输出示例：
  # 0::/redis
  ```

---

**6. 迁移与兼容性**

| 场景               | cgroup v1                          | cgroup v2                          |
|------------------------|----------------------------------------|----------------------------------------|
| 内核支持           | 所有内核版本                           | 需内核 ≥4.5（推荐 ≥5.0）               |
| 容器运行时         | Docker 旧版本默认                      | Docker/Kubernetes 新版本默认           |
| 启用方式           | 默认启用                               | 需内核参数 `systemd.unified_cgroup_hierarchy=1` |

---

**总结**
• cgroup v1：  

  优点：兼容旧系统和工具；缺点：管理复杂，易冲突。  
  适用场景：传统环境或需兼容旧应用的场景。  

• cgroup v2：  

  优点：统一管理、功能精细、安全性高；缺点：需较新内核。  
  适用场景：现代容器化环境（如 Kubernetes）、性能敏感型应用。  

迁移建议：  

1. 新部署环境优先使用 cgroup v2。  
2. 旧系统可通过内核参数 `cgroup_no_v1=all` 强制禁用 v1。  
3. 检查工具链（如 Docker、Kubernetes）的 cgroup 版本支持。
