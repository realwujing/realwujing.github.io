# KeenTune AI OS 全栈性能调优体系架构与优化方案

## 一、五仓库系统架构

```
                    ┌──────────────┐
                    │ keentune-ui  │  React + Ant Design Pro
                    │ 可视化平台    │  静态资源 → httpd 托管
                    └──────┬───────┘
                           │ HTTP REST
                    ┌──────▼───────┐
                    │   keentuned   │  Go 1.21, DBus + HTTP (:9871)
                    │  管控调度组件  │  编排 brain/target/bench
                    └──┬──┬──┬─────┘
                       │  │  │
          ┌────────────┘  │  └────────────┐
          │ HTTP REST     │ HTTP REST     │ HTTP REST
  ┌───────▼──────┐ ┌─────▼──────┐ ┌──────▼───────┐
  │keentune-brain│ │keentune-   │ │keentune-bench│
  │ AI决策引擎   │ │  target    │ │ 压测控制器   │
  │ Python :9872 │ │Python :9873│ │ Python :9874 │
  └──────────────┘ └────────────┘ └──────────────┘
```

| 组件 | 语言 | 定位 | 核心职责 |
|------|------|------|---------|
| **keentuned** | Go 1.21 | 调度中枢 | DBus 暴露方法给 CLI、HTTP REST 编排三个子组件、fsnotify 热加载配置 |
| **keentune-brain** | Python 3 | AI 引擎 | 调优算法(TPE/HORD/random/lamcts/bgcs)、敏感度分析(LASSO/Xsen/SHAPKernel/XGBTotalGain) |
| **keentune-target** | Python 3 | 参数执行 | 执行 sysctl/grubby/nginx/mysql 等参数的 set/get/backup/rollback |
| **keentune-bench** | Python 3 | 基准测试 | 远程执行 benchmark 脚本，解析 `key=value,key=value` 格式的度量结果 |
| **keentune-ui** | React/Node.js | 可视化 | 基于 Ant Design Pro，`npm run build` 生成静态 HTML，由 httpd 托管 |

### 调优全流程

```
CLI 命令 → keentuned (调度)
              ├── brain.init()          → 初始化优化器
              ├── loop:
              │   ├── brain.acquire()   → 获取候选参数
              │   ├── target.set()      → 设置参数到业务机
              │   ├── bench.run()       → 跑 benchmark 打分
              │   └── brain.feedback()  → 反馈分数
              ├── brain.best()          → 获取最优配置
              └── brain.end()           → 结束优化器
```

---

## 二、五仓库之上的优化工作

我们在五个开源仓库基础上，完成了以下优化，使 KeenTune 从"可运转的系统"升级为"AI Agent 一句话全栈性能调优平台"。

### 2.1 RPM 打包体系（一键部署）

**优化前**：四个仓库各自手动编译，brain 需特殊启动方式，部署流程分散且容易出错。

**优化后**：

| 仓库 | spec 文件 | 产物 |
|------|----------|------|
| keentuned | `keentuned.spec` | keentuned-3.1.0 + keentune meta 包 (x86_64) |
| keentune_target | `keentune-target.spec` | keentune-target-3.1.0 (noarch) |
| keentune_brain | `keentune-brain.spec` | keentune-brain-2.4.1 (noarch) |
| keentune_bench | `keentune-bench.spec` | keentune-bench-2.4.1 (noarch) |
| keentune_ui | `keentune-ui.spec` (已有) | 静态 HTML + httpd 配置 (noarch) |

**keentune meta 包**一键安装全部组件：
```bash
rpm -ivh keentune-3.1.0.rpm  # 自动拉入 keentuned + target + brain + bench
```

### 2.2 全栈 systemd 服务化

**优化前**：brain 用 `setsid` hack 脱离 shell，target/bench 手动后台启动，进程不稳定。

**优化后**：四个组件全部注册为 systemd service，`systemctl start keentune-{target,brain,bench}` 统一管理，支持开机自启。

**踩坑记录**：
- brain 因 tornado ioloop 特性，需要 systemd service 而非手动后台运行
- 二进制路径不一致（`/usr/bin` vs `/usr/local/bin`），统一修复
- numpy 版本冲突（yum 1.x vs pip 2.x → 强制 yum numpy 1.24.3）

### 2.3 benchmark 矩阵扩展

**优化前**：仅有 wrk HTTP 和 sysbench MySQL 两种 benchmark，场景覆盖面极窄。

**优化后**：新增 4 种 benchmark，覆盖计算/IO/存储/网络四个维度：

| Benchmark | JSON 配置 | Python 脚本 | 测试维度 |
|-----------|----------|------------|---------|
| sysbench CPU | sysbench_cpu.json | sysbench_cpu.py | 纯计算 |
| sysbench FileIO | sysbench_fileio.json | sysbench_fileio.py | 磁盘随机读 |
| FIO Seq Read | fio_read.json | fio_read.py | 磁盘顺序读 |
| iperf3 Loopback | iperf3.json | iperf3.py | 网络吞吐 |

Agent 根据调优场景自动匹配合适的 benchmark。

### 2.4 内核参数自动适配

**优化前**：参数 JSON 在不同内核版本下可能包含不存在的参数（如内核 6.6 移除 `sched_migration_cost_ns`），直接使用会导致调优失败。

**优化后**：`scripts/filter_params.py` 自动检测本机内核实际存在的参数：

```bash
python3 scripts/filter_params.py sysctl.json kvm_sysctl.json
# 输出: Total: 98, Valid on this kernel: 97, Removed: 1
```

### 2.5 敏感度分析全链路打通

**优化前**：`sensitize train` 功能存在但从未端到端验证，brain 子进程和 Python 依赖问题阻塞全链路。

**优化后**：完整验证 `param tune(生成数据) → sensitize train(LASSO) → 参数重要性排名` 闭环。

验证结果（sysbench FileIO 场景）：
```
参数重要性 Top 5:
1. kernel.sched_rt_runtime_us       0.253
2. net.ipv4.tcp_reordering          0.113
3. net.ipv4.tcp_retries2            0.087
4. net.ipv4.tcp_synack_retries      0.076
5. vm.dirty_expire_centisecs        0.049
（其余 92 个参数 weight≈0，可安全剔除）
```

### 2.6 A-Tune 知识库移植

**优化前**：KeenTune 没有场景-参数映射，用户不知道不同应用场景该调哪些参数。

**优化后**：从 A-Tune 社区 52 个场景提取 430+ 参数映射表，集成到 SKILL.md 知识库：

- **Web 服务**：nginx/http/tomcat/httpd 等 8 种场景的参数映射
- **数据库**：mysql/mongodb/redis/postgresql 等 12 种场景
- **大数据**：spark/hadoop/Hive/kafka 等 7 种场景
- **消息队列**：rabbitmq/rocketmq/activemq
- **计算密集**：HPC/speccpu/specjbb/encryption
- **通用 OS 参数**：CPU调度/内存VM/块设备/文件系统/网络 9 层参数分层

### 2.7 AI Agent Skill 化

创建 `SKILL.md`（477 行），集成到 OpenCode 的 skill 体系中：

- 全栈部署命令序列（源码编译 + RPM 构建）
- 一键启动脚本 `scripts/start_all.sh`
- 全流程调优闭环 6 步（环境检查→场景诊断→参数过滤→benchmark→调优→结果导出）
- 常见错误诊断清单（9 条实战坑记录）
- Agent 场景决策表（按用户需求自动匹配工作流）

### 2.8 文档与运维脚本

| 文件 | 用途 |
|------|------|
| `AGENTS.md` | OpenCode Agent 快速上手指南（编译/测试/陷阱） |
| `scripts/start_all.sh` | 一键启动 4 个服务 + 健康检查 |
| `scripts/check_deps.sh` | 依赖检测（go/python/bench 工具） |
| `scripts/filter_params.py` | 内核参数自动过滤 |

---

## 三、Git 提交记录

| 仓库 | 提交数 | 关键内容 |
|------|--------|---------|
| keentuned | 5 commits | spec、AGENTS.md、scripts、meta 包 |
| keentune_target | 1 commit | RPM spec |
| keentune_brain | 1 commit | RPM spec |
| keentune_bench | 1 commit | RPM spec |
| keentune_ui | 0 commits | 已有 spec，无需新增 |

---

## 四、能力矩阵总结

| 能力维度 | 优化前 | 优化后 |
|---------|--------|--------|
| **部署** | 4 组件手动编译，brain 需 hack | `yum install keentune` 一键安装 |
| **启动** | 依次启动，brain 不稳定 | `systemctl start` 四服务统一管理 |
| **benchmark** | 2 种（wrk HTTP + sysbench MySQL） | 6 种（+CPU/FileIO/FIO/iperf3） |
| **内核适配** | 手动排查不存在的参数 | `filter_params.py` 自动过滤 |
| **敏感度分析** | 功能未验证 | 全链路打通（tune→train→排名） |
| **场景知识** | 无参数映射 | 52 场景 + 430 参数知识库 |
| **Agent 能力** | 无 | SKILL.md + AGENTS.md 完整文档 |
| **运维工具** | 无 | start_all.sh / check_deps.sh / filter_params.py |