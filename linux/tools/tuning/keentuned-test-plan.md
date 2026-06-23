# KeenTune 测试计划

## 测试环境要求

| 组件 | 端口 | 服务 |
|------|------|------|
| keentuned | 9871 | `systemctl start keentuned` |
| keentune-brain | 9872 | `setsid python3 -c "from brain.brain import main; main()"` |
| keentune-target | 9873 | `systemctl start keentune-target` |
| keentune-bench | 9874 | `systemctl start keentune-bench` |

**前置条件：**
- `systemctl stop tuned`（keentuned 与 tuned 冲突）
- `/etc/keentune/keentuned/conf/keentuned.conf` 中配置 sysctl.json + wrk_http_long.json
- `/var/keentune/` 工作目录自动创建
- 多机测试需配置 SSH 免密登录

---

## 一、冒烟测试（Smoke Test）<10 分钟>

目标：快速验证核心链路可用，每次构建后必跑。

| # | 测试项 | 命令 | 断言 |
|---|--------|------|------|
| S1 | 服务可用 | `curl -s localhost:9871/status` | HTTP 200 |
| S2 | brain 可用 | `curl -s localhost:9872/avaliable` | HTTP 200 |
| S3 | target 可用 | `curl -s localhost:9873/status` | HTTP 200 |
| S4 | bench 可用 | `curl -s localhost:9874/status` | HTTP 200 |
| S5 | keentune help | `keentune help` | 退出码 0，含 `Usage` + `Available Commands` |
| S6 | keentune version | `keentune -v` | 退出码 0，含 `keentune version` |
| S7 | keentune init | `keentune init` | 退出码 0，含 `Init success`；`/etc/keentune/keentuned/conf/init.yaml` 存在 |
| S8 | REST /cmd | `curl -s -X POST localhost:9871/cmd -d '{"cmd":"keentune --version"}'` | HTTP 200，`"suc":true` |
| S9 | REST /status | `curl -s localhost:9871/status` | HTTP 200 |

---

## 二、BVT / 功能验证（Build Verification Test）<1 小时

目标：覆盖所有核心功能的正向路径。

### 2.1 参数动态调优（param tune）

| # | 测试项 | 命令 | 断言 |
|---|--------|------|------|
| B1 | param tune 基础 | `keentune param tune -i 10 --job param1` | 退出码 0；日志含 Step1-6 + `[BEST] Tuning improvement` |
| B2 | param list | `keentune param list` | 显示 `sysctl.json` + `wrk_http_long.json` |
| B3 | param jobs | `keentune param jobs` | 显示 `param1` 且状态 `finish` |
| B4 | param dump | `echo y \| keentune param dump -j param1` | 退出码 0；`dump successfully`；`/var/keentune/keentuned/profile/param1_group1.conf` 存在 |
| B5 | param rollback | `keentune param rollback` | 退出码 0；`rollback finished`；`sysctl -n` 值恢复 |
| B6 | param delete | `echo y \| keentune param delete --job param1` | 退出码 0；`delete successfully`；`/var/keentune/keentuned/tuning_workspace/param1/` 不存在 |

### 2.2 静态 Profile

| # | 测试项 | 命令 | 断言 |
|---|--------|------|------|
| B7 | profile list | `keentune profile list` | 显示内置 profile + param1_group1.conf |
| B8 | profile info | `keentune profile info --name cpu_high_load.conf` | 退出码 0；显示 `[sysctl]` |
| B9 | profile set | `keentune profile set param1_group1.conf` | 退出码 0；`Succeeded`；`profile list` 显示 `active` |
| B10 | profile rollback | `keentune profile rollback` | 退出码 0；`rollback finished`；恢复到备份值 |
| B11 | profile generate | `echo y \| keentune profile generate -n param1_group1.conf -o param1_group1` | 退出码 0；`generate successfully`；JSON 文件存在 |
| B12 | profile delete | `echo y \| keentune profile delete --name param1_group1.conf` | 退出码 0；`delete successfully` |
| B13 | profile delete (内置) | `echo y \| keentune profile delete --name cpu_high_load.conf` | 退出码 1；`not supported to delete` |

### 2.3 敏感度分析（sensitize）

| # | 测试项 | 命令 | 断言 |
|---|--------|------|------|
| B14 | sensitize train | `echo y \| keentune sensitize train --data param1 --job param1` | 退出码 0；日志含 Step1-3 + `identification results successfully`；`knobs.json` 存在 |
| B15 | sensitize jobs | `keentune sensitize jobs` | 显示 `param1` |
| B16 | sensitize delete | `echo y \| keentune sensitize delete --job param1` | 退出码 0；`delete successfully`；目录不存在 |

### 2.4 REST API

| # | 测试项 | 命令 | 断言 |
|---|--------|------|------|
| B17 | REST /write | `curl -X POST localhost:9871/write -d '{"name":"test","data":"hello"}'` | HTTP 200；文件写入成功 |
| B18 | REST /read | `curl -X POST localhost:9871/read -d '{"name":"test"}'` | HTTP 200；返回之前写入的 data |
| B19 | REST /benchmark_result | `curl -X POST localhost:9871/benchmark_result -d '<valid_data>'` | HTTP 200 (非 localhost 限制) |
| B20 | REST /sensitize_result | `curl -X POST localhost:9871/sensitize_result -d '<valid_data>'` | HTTP 200 |

---

## 三、异常/可靠性测试 <2 小时

目标：验证错误处理和边界条件。

### 3.1 param tune 异常输入

| # | 测试项 | 命令 | 期望 |
|---|--------|------|------|
| R1 | 缺少 --job | `keentune param tune -i 10` | 退出码 1；`Incomplete or Unmatched command` |
| R2 | --job 无值 | `keentune param tune -i 10 --job` | 退出码 1；`flag needs an argument` |
| R3 | --job 空串 | `keentune param tune -i 10 --job ''` | 退出码 1 |
| R4 | --job 空格 | `keentune param tune -i 10 --job ' '` | 退出码 1 |
| R5 | 重复 job | `keentune param tune -i 10 --job param1`（两次） | 退出码 1；`already exists` |
| R6 | -i 无值 | `keentune param tune --job test -i` | 退出码 1；`invalid argument` |
| R7 | -i 负数 | `keentune param tune -i -1 --job test` | 退出码 1；`iteration >= 10` |
| R8 | -i 0 | `keentune param tune -i 0 --job test` | 退出码 1；`iteration >= 10` |
| R9 | -i 小于10 | `keentune param tune -i 5 --job test` | 退出码 1；`iteration >= 10` |
| R10 | -i 浮点 | `keentune param tune -i 5.2 --job test` | 退出码 1 |
| R11 | -i 字符串 | `keentune param tune -i hello --job test` | 退出码 1 |
| R12 | job 含 `/` | `--job /root/test` | 退出码 1；`find unexpected characters '/'` |
| R13 | job 含 `,` | `--job test,a` | 退出码 1；`find unexpected characters ','` |
| R14 | job 含 `+` | `--job test+1` | 退出码 1 |
| R15 | job 合法字符 | `--job _test1` | 退出码 0（下划线/数字可接受） |

### 3.2 并发/中断测试

| # | 测试项 | 命令 | 期望 |
|---|--------|------|------|
| R16 | tune 中途 stop | 启动 `keentune param tune -i 100 --job stop_test`，执行 `keentune param stop` | `Abort parameter optimization job`；jobs 状态为 stopped |
| R17 | tune 期间 delete | 启动 tuning → 中途 `echo y \| keentune param delete --job stop_test` | 退出码 0；目录清理 |
| R18 | tune 期间 rollback | 启动 tuning → 中途 `keentune param rollback` | 退出码 1（有 running job）或回滚成功 |
| R19 | 重复 init | `keentune init` 执行两次 | 第二次提示已初始化或覆盖确认 |

### 3.3 profile 异常输入

| # | 测试项 | 命令 | 期望 |
|---|--------|------|------|
| R20 | set 不存在的 profile | `keentune profile set not_exist.conf` | 退出码 1 |
| R21 | 无 --name | `keentune profile info` | 报错或使用默认 |
| R22 | delete 不存在的 profile | `echo y \| keentune profile delete --name not_exist.conf` | 退出码 1 |

---

## 四、回归测试（Regression）<3 小时

目标：关键场景端到端验证，确保核心功能无回退。

### 4.1 全链路：调优→dump→set→rollback

| # | 测试项 | 流程 |
|---|--------|------|
| RG1 | 完整调优闭环 | `init` → `param tune -i 10 --job rg1` → `param dump` → `profile set rg1_group1.conf` → `profile rollback` → `param delete` |
| RG2 | 敏感度闭环 | `init` → `param tune -i 30` → `sensitize train` → `sensitize jobs` → `sensitize delete` → `param delete` |

### 4.2 Tuned Profile 兼容性

| # | 测试项 | 验证点 |
|---|--------|--------|
| RG3 | latency_performance | `profile set latency_performance.conf` → `sysctl -n vm.swappiness` = 10 |
| RG4 | throughput_performance | `profile set throughput_performance.conf` → `sysctl -n vm.dirty_ratio` = 40 |
| RG5 | network_throughput | `profile set network_throughput.conf` → `sysctl -n net.ipv4.tcp_rmem` = "4096 87380 16777216" |
| RG6 | virtual_host | `profile set virtual_host.conf` → `sysctl -n vm.dirty_background_ratio` = 5 |
| RG7 | oracle | `profile set oracle.conf` → `sysctl -n kernel.shmmni` = 4096 |
| RG8 | realtime | `profile set realtime.conf` → irqbalance banned_cpus / systemd CPUAffinity |
| RG9 | hpc_compute | `profile set hpc_compute.conf` → THP = always |

### 4.3 REST API 安全性

| # | 测试项 | 验证点 |
|---|--------|------|
| RG10 | /cmd 命令注入 | POST `{"cmd":"keentune param tune -i 10 --job x1 && rm -rf /"}` | 400，`suc: false` |
| RG11 | /cmd 非法命令 | POST `{"cmd":"rm -rf /"}` | 400，不匹配 cmdPattern |
| RG12 | /cmd 远程 IP | 从非 localhost 访问 | 401/403，被 auth 中间件拒绝 |
| RG13 | /write 远程 IP | 从非 localhost 访问 | 401/403 |
| RG14 | /read 远程 IP | 从非 localhost 访问 | 401/403 |

---

## 五、多场景 / 多算法测试 <6 小时

目标：覆盖不同参数域和算法组合。

### 5.1 多参数域

| # | 测试项 | 参数文件 | Benchmark |
|---|--------|----------|-----------|
| M1 | sysctl 域 | `sysctl.json` | `wrk_http_long.json` |
| M2 | nginx 域 | `nginx_conf.json` | `wrk_http_long.json` |
| M3 | MySQL 域 | `mysql.json` | `sysbench_mysql_read_write.json` |
| M4 | fio 域 | `fio.json` | fio benchmark |
| M5 | iperf 域 | `iperf.json` | iperf benchmark |
| M6 | sysbench 域 | `sysbench.json` | sysbench |

### 5.2 多算法迭代

| # | 算法类型 | 算法 | 命令 |
|---|---------|------|------|
| M7 | 调优 | random | 修改 conf `AUTO_TUNING_ALGORITHM=random` → tune |
| M8 | 调优 | hord | 修改 conf → tune |
| M9 | 调优 | lamcts | 修改 conf → tune |
| M10 | 调优 | bgcs | 修改 conf → tune |
| M11 | 调优 | tpe | 修改 conf → tune |
| M12 | 敏感度 | LASSO | 修改 conf → sensitize train |
| M13 | 敏感度 | MI（互信息） | 修改 conf → sensitize train |
| M14 | 敏感度 | GP（高斯过程） | 修改 conf → sensitize train |
| M15 | 敏感度 | XGBTotalGain | 修改 conf → sensitize train |
| M16 | 敏感度 | Xsen | 修改 conf → sensitize train |
| M17 | 敏感度 | SHAPKernel | 修改 conf → sensitize train |

### 5.3 多 Target 拓扑（需多机环境）

| # | 拓扑 | 描述 |
|---|------|------|
| M18 | 2 group | group1=localhost(sysctl), group2=target(nginx) |
| M19 | 3 group | group1=localhost, group2=target, group3=bench |
| M20 | 4 group | 四台机器各一 group，验证跨机参数下发和一致性 |

---

## 六、性能 / 压力测试

### 6.1 算法性能对比

| # | 测试项 | 描述 |
|---|--------|------|
| P1 | 算法收敛效率 | 对比 random/hord/tpe/lamcts/bgcs 在相同参数空间达到相同提升所需的迭代次数 |
| P2 | 特征选择精度 | 对比 LASSO/MI/GP/XGBTotalGain/SHAP 识别关键参数的准确率 |
| P3 | 大参数空间 | 100+ kbobs 参数空间下的搜索效率（算法是否能有效降维） |

### 6.2 稳定性

| # | 测试项 | 描述 |
|---|--------|------|
| P4 | 长时间运行 | 连续 72 小时循环执行 `init → tune → dump → set → rollback → delete`，检查内存泄漏和服务存活 |
| P5 | 高频启停 | `systemctl restart keentuned` 连续 100 次，检查启动成功率和 DBus 注册 |
| P6 | 大批量 job | 创建 100 个连续 tuning job，检查 CSV 文件读写和目录管理 |

### 6.3 并发

| # | 测试项 | 描述 |
|---|--------|------|
| P7 | DBus 并发 | 多个 `keentune` CLI 进程同时发起请求，检查互斥锁 |
| P8 | REST 并发 | `ab -n 1000 -c 50 localhost:9871/status`，检查响应时间和内存 |

---

## 七、Go 单元测试

```bash
cd daemon && go test -mod=vendor -v ./...
```

覆盖：lock、service 创建、配置加载、param/profile/sensitize 操作、rollback、watch、abort 等 DBus 方法。

| # | 测试文件 | 测试数 | 覆盖内容 |
|---|---------|--------|---------|
| G1 | `daemon/service_test.go` | ~50+ | gomonkey mock：lock 机制、service 创建/重载、param 删除、profile 操作、sensitize、tuning workflow、train、rollback、config watch、abort、dump |

---

## 八、安装/部署测试

| # | 测试项 | 描述 |
|---|--------|------|
| I1 | RPM 安装 | `rpm -ivh keentuned-*.rpm` → 检查二进制 `/usr/bin/keentuned`, `/usr/bin/keentune`，配置 `/etc/keentune/keentuned/`，systemd unit |
| I2 | RPM 卸载 | `rpm -e keentuned` → 检查文件清理、服务移除 |
| I3 | 源码编译 | `go mod vendor && make daemon && make cli` → 检查 `pkg/keentuned`, `pkg/keentune` |
| I4 | make install | `make install` → 验证二进制、配置、man page、bash completion 安装位置 |
| I5 | systemd 启动 | `systemctl start keentuned` → `systemctl is-active keentuned` = active |
| I6 | 端口冲突 | 端口 9871 被占用时 keentuned 启动失败，有明确错误提示 |
| I7 | tuned 冲突 | tuned.service 运行中 → keentuned 启动警告 |

---

## 九、测试执行矩阵

| 阶段 | 测试集 | 耗时 | 触发条件 |
|------|--------|------|---------|
| **每次 commit** | 冒烟测试 (S1-S9) | <10 min | CI / pre-commit |
| **每次 MR/PR** | 冒烟 + BVT (S1-S9, B1-B20) | <1 h | CI pipeline |
| **每日构建** | 冒烟 + BVT + 异常测试 (R1-R22) | <2 h | Nightly CI |
| **每周构建** | 全功能回归 (R1-R22, RG1-RG14) | <3 h | Weekly CI |
| **发版前** | 全部（含多场景、性能、稳定性、安装） | <24 h | Release gate |
| **按需** | 长时间稳定性 (P4) | 72 h | 手动触发 |
| **按需** | 多机拓扑 (M18-M20) | 数小时 | 手动触发 |
| **按需** | Go 单元测试 (G1) | <5 min | 代码变更触发 |

---

## 十、已知限制和待补测试

### 10.1 不跑的测试（test 目录中存在但默认不执行）

| 文件 | 原因 |
|------|------|
| `test_keenopt.py` (30+ tests) | 需要 `keenopt` Python 库，且 `keenopt_suite` 构建后未加入返回 suite |
| `test_keenopt_performance.py` (8 tests) | 同上，性能对比非功能验证 |
| `test_wrk_benchmark.py` (24 tests) | 执行时间过长，需要 wrk HTTP 服务 |
| `test_long_stability.py` | 需手动设置 `time_limit` 属性，执行数小时到数天 |
| `test_rollback_all.py` | 被注释掉 |
| `test/installation/` | 需要干净环境，不在 `make check` 中 |

### 10.2 需要补充的测试

| 领域 | 缺什么 |
|------|--------|
| Go 单元测试 | 只在 daemon 有测试；`common/`, `modules/`, `api/` 包无测试 |
| mock 测试 | 无 brain/target/bench 的 mock，集成测试依赖完整的 4 服务 |
| 协议测试 | 无 DBus 接口 schema 验证测试 |
| 配置热加载 | conf watcher 的 fsnotify 测试仅在 Go unit test 中 mock，无集成验证 |
| brain 端到端 | 无 GetAvailable/Acquire/Feedback/Best 的独立 mock 测试 |
| benchmark 格式校验 | benchmark 脚本输出 `key=value,key=value` 格式的验证测试不存在 |
| 日志轮转 | file-rotatelogs 的轮转测试不存在 |
| signal 处理 | SIGTERM/SIGINT/SIGQUIT 优雅退出测试不存在 |
| 数据一致性 | tuning/sensitize CSV 文件在异常退出时是否损坏 |
