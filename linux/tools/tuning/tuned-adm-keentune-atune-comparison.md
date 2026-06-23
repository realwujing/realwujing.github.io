# tuned-adm vs KeenTune vs A-Tune 对比分析

三款 Linux 系统性能调优工具的全方位对比。

---

## 1. 基本信息

| 维度 | **tuned-adm** | **KeenTune** | **A-Tune** |
|------|---------------|--------------|------------|
| 来源 | Red Hat (2008~) | 阿里云/龙蜥社区 | 华为/openEuler |
| 仓库 | [redhat-performance/tuned](https://github.com/redhat-performance/tuned) | [anolis/keentuned](https://gitee.com/anolis/keentuned) + 5 个子仓库 | [openeuler/A-Tune](https://gitee.com/openeuler/A-Tune) |
| License | GPLv2 | Mulan PSL v2 | Mulan PSL v2 |
| 二进制 | `tuned` (daemon) + `tuned-adm` (CLI) | `keentuned` (daemon) + `keentune` (CLI) | `atuned` (daemon) + `atune-adm` (CLI) |
| 语言 | Python 3 | Go 1.21 + Python (brain/target/bench) | Go 1.14 + Python (engine/rest) |
| 部署范围 | Fedora/RHEL/CentOS **默认集成** | AnolisOS + 龙蜥生态 | openEuler 社区 |
| 文档 | man pages + 英文 README | README + `misc/docs/` (中英文) | README + `docs/` (中英文, 24.03 LTS) |

---

## 2. 架构对比

```
tuned-adm (单仓库，单守护进程):
┌──────────────┐     DBus      ┌──────────────┐
│  tuned-adm   │◄─────────────►│    tuned     │
│   (CLI)      │               │  (daemon)    │
└──────────────┘               └──────────────┘

KeenTune (6 仓库，4 服务):
                                ┌─ keentune-brain :9872 (AI 搜索)
┌──────────────┐     DBus       │
│  keentune    │◄──────────────►│─ keentuned :9871 (调度核心)
│   (CLI)      │               │
└──────────────┘               ├─ keentune-target :9873 (下发参数)
                               └─ keentune-bench  :9874 (压测)

A-Tune (4-5 仓库，4 服务):
                                ┌─ atune-engine :3838 (ML 引擎)
┌──────────────┐    gRPC        │
│  atune-adm   │◄──────────────►│─ atuned (gRPC + Unix socket)
│   (CLI)      │               │
└──────────────┘               ├─ atune-rest :8383 (监控采集)
                               └─ A-Tune-Collector (独立采集器)
```

### IPC 机制差异

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **CLI ↔ Daemon** | DBus | DBus system bus | **gRPC** (Unix socket / TCP) |
| **Daemon ↔ 子服务** | N/A (无子服务) | REST (HTTP, port 9871) | REST (HTTP, port 8383/3838) |
| **子服务间通信** | N/A | DBus session bus (bench/sensi信号) | gRPC 双向流 |
| **TLS 安全** | ❌ | ❌ | ✅ gRPC + REST 双向 TLS |
| **多节点集群** | ❌ | 支持多 target/bench 机器 | ✅ gRPC TCP 集群模式 |

---

## 3. 调优能力对比

### 3.1 静态 Profile

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **Profile 数量** | 39 个预定义 | 可动态创建 + JSON 管理 | 15 大类型 (数百个 .conf) |
| **配置文件格式** | INI 风格 `tuned.conf` | JSON 参数文件 | INI 风格 `.conf` |
| **配置粒度** | `[cpu]`, `[vm]`, `[sysctl]`, `[disk]`, `[net]` 等 section | domain + name + range + step + dtype | `[sysctl]`, `[sysfs]`, `[bios]`, `[bootloader]`, `[schedule_policy]`, `[script]` 等 |
| **Profile 继承** | ✅ `include=throughput-performance` | ❌ | ✅ `include=default-default` |
| **历史回滚** | ❌ | ✅ `profile rollback` | ✅ `profile rollback` |
| **Profile 校验** | ✅ `tuned-adm verify` | ❌ | ❌ |

示例 — tuned-adm 的 network-throughput profile:
```ini
[main]
include=throughput-performance

[sysctl]
net.ipv4.tcp_rmem="4096 131072 16777216"
net.ipv4.tcp_wmem="4096 16384 16777216"
```

示例 — KeenTune 的参数 JSON:
```json
{
    "domain": "sysctl",
    "name": "net.unix.max_dgram_qlen@group-1",
    "range": [128, 1048576],
    "dtype": "int",
    "step": 128,
    "base": "400383"
}
```

示例 — A-Tune 的 profile conf:
```ini
[sysctl]
vm.swappiness=10
kernel.sched_min_granularity_ns=10000000
net.core.rmem_default=262144

[schedule_policy]
group=0:0:f:p:0
```

### 3.2 动态调优

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **调优方式** | 内置 monitor 自适应调整 | AI 迭代搜索 + benchmark 反馈 | AI 优化 + benchmark 反馈 + 规则引擎 |
| **AI 引擎** | ❌ 无 | random, hord, lamcts, bgcs | **13 种**: Bayes(GP), GA, PPO, TPE, grid, random, forest, extraTrees, gbrt, lhs, abtest, traverse, Bayesian_Transfer |
| **负载感知** | 磁盘/网络/CPU **动态监控** | ❌ 无监控 | ✅ RandomForest/SVM 在线识别 11 种负载 |
| **瓶颈检测** | ❌ | ❌ | ✅ 5 种 (CPU/Memory/Network/NetIO/DiskIO) |
| **规则引擎** | ❌ | ❌ | ✅ **grool + YQL** 动态规则调优 |
| **自适应** | ✅ 根据负载变化实时调整 CPU governor 等 | ❌ | ✅ 负载分类后自动匹配 profile + 规则 |

### 3.3 敏感度分析

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **敏感度分析** | ❌ | ✅ LASSO, SHAPKernel, XGBTotalGain, MI, GP | ✅ wefs / vrfs 特征选择 |
| **用途** | N/A | 先识别重要参数，缩小搜索空间 | 同，减少调优参数数量 |

### 3.4 基准测试

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **内置 benchmark** | ❌ | ✅ bench 组件 (wrk/sysbench/fio/nginx 等) | ✅ Evaluation YAML 定义 |
| **多轮重复** | N/A | ✅ 可配置 baseline/tuning/recheck 轮次 | ✅ 配置迭代次数 |
| **结果格式** | N/A | `key=value,key=value` | 支持 grep 正则提取 |

---

## 4. 高级特性

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **CPU 调度绑定** | ❌ | ❌ | ✅ 内置 scheduler (绑核/IRQ/NUMA) |
| **异常检测** | ❌ | ❌ | ✅ Python 异常检测模块 |
| **自动 Profile 推荐** | ✅ 基于 `system-release-cpe` + `virt-what` | ❌ | ✅ ML 模型自动分类 + 推荐 |
| **Web UI** | ❌ | ✅ keentune-ui (Node.js) | ✅ A-Tune-UI (Node.js) |
| **Bash 补全** | ✅ | ✅ | ❌ |
| **自动配置检测** | ❌ | ❌ | ✅ `make autoconfig` 自动检测磁盘/网卡 |
| **Docker** | ✅ | ❌ | ✅ engine 独立 Dockerfile |

---

## 5. 工程化对比

### 5.1 构建

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| 构建系统 | Makefile + RPM spec | `go mod vendor` + Makefile | `go mod vendor` + Makefile |
| 构建命令 | `make` | `make daemon && make cli` | `make` |
| 二进制输出 | Python (不编译) | `pkg/keentuned`, `pkg/keentune` | `pkg/atuned`, `pkg/atune-adm` |
| 编译依赖 | Python 3 | Go 1.21 + `-mod=vendor -extldflags "-static"` | Go 1.14 + `-mod=vendor` |
| 子组件安装 | N/A | 需单独安装 brain/target/bench (各从独立 repo) | 需单独安装 A-Tune-Collector (`pip3 install`) |

### 5.2 测试

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **单元测试** | ✅ Python unittest (profiles/plugins/monitors/utils) | ✅ Go gomonkey + goconvey (仅 `daemon/service_test.go`) | ❌ 无 Go 单元测试 |
| **集成测试** | ✅ beakerlib 7 个场景 | ✅ Python unittest 60+ 用例 (需 4 服务全启动) | ✅ 27 个 Shell 测试脚本 |
| **测试运行** | `make test` | `make check` (需要 4 服务运行中) | `make check` |
| **CI/CD** | ✅ Packit / Fedora CI | ❌ | ❌ |
| **E2E 覆盖** | profile 切换 / API / error 处理 | CLI 功能 + 可靠性 + REST API + UI + 长时间稳定性 | CLI 功能 + 配置 + 启停 |

### 5.3 数据库

| | tuned-adm | KeenTune | A-Tune |
|--|-----------|----------|--------|
| **数据库** | 无 | SQLite3 | SQLite3 (xorm ORM) |
| **存储内容** | N/A | job 历史 CSV (/var/keentune/) | profile 元数据 / 历史日志 / 规则 |

---

## 6. 架构拓扑

```
tuned-adm:
  ┌─ 单机部署 ─────────────────────────┐
  │  tuned (daemon)                     │
  │  ├─ CPU governor                   │
  │  ├─ disk readahead / scheduler     │
  │  ├─ vm dirty ratio                 │
  │  ├─ sysctl (tcp/udp/net buffer)    │
  │  └─ monitor (disk/load/net)        │
  └─────────────────────────────────────┘

KeenTune:
  ┌─ 单机或多机 ──────────────────────────────────────────────────┐
  │  keentuned :9871 (调度核心)                                    │
  │  ├─ DBus ──► keentune (CLI)                                  │
  │  ├─ REST ──► keentune-brain :9872 (AI 搜索算法)              │
  │  ├─ REST ──► keentune-target :9873 (参数下发到目标机器)       │
  │  └─ REST ──► keentune-bench :9874 (基准测试执行)              │
  │       target/bench 可部署在远端不同机器                        │
  └────────────────────────────────────────────────────────────────┘

A-Tune:
  ┌─ 单机或集群 ─────────────────────────────────────────────────────────────────┐
  │  atuned (gRPC server, Unix socket + TCP)                                     │
  │  ├─ gRPC ──► atune-adm (CLI)                                                │
  │  ├─ REST ──► atune-rest :8383 (监控/采集/配置)                               │
  │  ├─ REST ──► atune-engine :3838 (ML 优化引擎, 支持独立 Docker 部署)           │
  │  │            ├─ 负载分类 (RandomForest/SVM)                                 │
  │  │            ├─ 瓶颈检测 (5 种)                                             │
  │  │            ├─ 贝叶斯优化 + GA + PPO + Transfer Learning                   │
  │  │            └─ 规则引擎 (grool + YQL)                                      │
  │  ├─ 扩展 ──► A-Tune-Collector (独立 collector 采集系统指标)                   │
  │  ├─ 扩展 ──► A-Tune-UI (Node.js Web 界面)                                   │
  │  └─ 集群 ──► 多 atuned 实例 gRPC TCP 互联 (集群模式)                         │
  └───────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. 核心思路对比

| 工具 | 核心哲学 | 一句话 |
|------|---------|--------|
| **tuned-adm** | **专家知识驱动** — "我知道什么场景该调什么参数" | 一键切换预设方案，开箱即用 |
| **KeenTune** | **迭代搜索驱动** — "让算法在参数空间里试出最优值" | 对特定 workload 精准寻优，需要配置 benchmark 脚本 |
| **A-Tune** | **智能识别驱动** — "先识别你是谁，再决定怎么优化你" | 全自动化，适合复杂异构环境，特性最全面但复杂度最高 |

三者是逐步演进的关系：

```
tuned-adm ──► KeenTune ──► A-Tune
(静态规则)    (AI 搜索)     (识别+搜索+规则)
```

- **tuned-adm** 解决"知道怎么调，快速切换"的问题
- **KeenTune** 解决"不知道最优值，让算法找"的问题
- **A-Tune** 解决"不知道当前是什么负载，先识别再优化"的问题，同时保留了前两者的能力

---

## 8. 选型建议

| 场景 | 推荐 |
|------|------|
| **Fedora/RHEL/CentOS 环境，通用场景** | tuned-adm（零依赖，系统自带） |
| **需要对特定 benchmark 做极限性能调优** | KeenTune（AI 搜索 + benchmark 闭环） |
| **openEuler 环境，需要全自动化** | A-Tune（负载识别 + 自动匹配 + AI 优化） |
| **混合/异构集群调优** | A-Tune（gRPC 集群模式 + TLS 安全） |
| **需要敏感度分析缩小参数空间** | KeenTune 或 A-Tune（两者都有特征选择） |
| **需要调度优化（绑核/IRQ/NUMA）** | A-Tune（内置 scheduler） |
| **需要异常检测** | A-Tune（内置 anomaly detection） |
| **需要 Docker 化部署** | A-Tune（engine 支持独立 Dockerfile） |
| **快速验证，不依赖外部服务** | tuned-adm |
