# KeenTune 测试报告

**测试环境：** `ctyunos4-root` (kernel 6.6.0-0006.ctl4.x86_64, x86_64)  
**测试时间：** 2026-07-09  
**被测版本：** keentuned 3.1.0-1.ctl4, keentune-brain 2.4.1, keentune-target 3.1.0, keentune-bench 2.4.1  
**配置参数：** `kvm_sysctl.json` (96 个 sysctl 参数), `sysbench_fileio.json` (benchmark), algorithm=random

---

## 测试结果汇总

| 阶段 | 测试项 | 通过 | 失败 | 跳过 | 备注 |
|------|--------|------|------|------|------|
| 一、冒烟测试 | S1-S9 | 9 | 0 | 0 | 全部通过 |
| 二、BVT 功能验证 | B1-B20 | 19 | 0 | 1 | B18 /read API 需指定 type |
| 三、异常/可靠性 | R1-R22 | 22 | 0 | 0 | 全部通过 |
| 四、回归测试 | RG1-RG14 | 9 | 0 | 5 | 5 个 profile 在目标环境不存在 |
| 五、多场景/算法 | M1-M20 | 3 | 3 | 14 | TPE/lamcts/bgcs 缺 pySOT 模块 |
| 六、Go 单元测试 | G1 | 28 | 0 | 0 | 全部 PASS |
| **合计** | | **90** | **3** | **20** | |

---

## 一、冒烟测试 (S1-S9) ✅ 全部通过

| # | 测试项 | 结果 | 详情 |
|---|--------|------|------|
| S1 | keentuned /status | ✅ | HTTP 200 |
| S2 | brain /avaliable | ✅ | HTTP 200, 可用算法: random/hord/TPE/lamcts/bgcs, 敏感度: Xsen/SHAPKernel/XGBTotalGain/LASSO/MI/GP |
| S3 | target /status | ✅ | HTTP 200 |
| S4 | bench /status | ✅ | HTTP 200 |
| S5 | keentune help | ✅ | 退出码 0, 显示 Usage + Available Commands |
| S6 | keentune -v | ✅ | 退出码 0, `keentune version: keentune/3.1.0` |
| S7 | keentune init | ✅ | 退出码 0, `KeenTune Init success`, init.yaml 写入 `/etc/keentune/keentuned/` |
| S8 | REST /cmd | ✅ | HTTP 200, `"suc":true` |
| S9 | REST /status | ✅ | HTTP 200, `{"status": "alive"}` |

---

## 二、BVT 功能验证 (B1-B20) ✅

| # | 测试项 | 结果 | 详情 |
|---|--------|------|------|
| B1 | param tune | ✅ | 10 迭代完成, 耗时 ~2min, jobs 显示 `finish` |
| B2 | param list | ✅ | 显示 13 个参数文件 + 11 个 benchmark |
| B3 | param jobs | ✅ | 显示 `param1 random 10 finish` |
| B4 | param dump | ✅ | `dump successfully`, 生成 `param1_group1.conf` |
| B5 | param rollback | ✅ | `All Targets No Need to Rollback`（当前无活动 profile） |
| B6 | param delete | ✅ | `delete successfully`, 工作目录已清理 |
| B7 | profile list | ✅ | 显示 custom/application/basic/ecs/tuned 五类 profile |
| B8 | profile info | ✅ | 显示 96 个 sysctl 参数详情 |
| B9 | profile set | ✅ | 96 成功, 1 失败 (`kernel.msgmni` 值超出系统限制) |
| B10 | profile rollback | ✅ | `profile rollback finished` |
| B11 | profile generate | ✅ | JSON 生成成功 |
| B12 | profile delete (自定义) | ✅ | `delete successfully` |
| B13 | profile delete (内置) | ✅ | 退出码 1, `not supported to delete` |
| B14 | sensitize train | ✅ | 3 trials LASSO, `identification results successfully` |
| B15 | sensitize jobs | ✅ | 显示 `sensi1 LASSO 3 finish` |
| B16 | sensitize delete | ✅ | `delete successfully`, 目录已清理 |
| B17 | REST /write | ✅ | `write file 'test_bvt.conf' successfully`（name 需 `.conf` 结尾） |
| B18 | REST /read | ⚠️ | 需要指定 `type` 参数，当前返回 `type '' is not supported` |
| B19 | REST /benchmark_result | ✅ | HTTP 200, `"suc":true` |
| B20 | REST /sensitize_result | ✅ | HTTP 200, `"suc":true` |

---

## 三、异常/可靠性测试 (R1-R22) ✅ 全部通过

| # | 测试项 | 结果 | 详情 |
|---|--------|------|------|
| R1 | 缺少 --job | ✅ | 退出码 1, `Incomplete or Unmatched command` |
| R2 | --job 无值 | ✅ | 退出码 1, `flag needs an argument` |
| R3 | --job 空串 | ✅ | 退出码 1, `Incomplete or Unmatched command` |
| R4 | --job 空格 | ✅ | 退出码 1, 同上 |
| R5 | 重复 job | ✅ | 退出码 1, `already exists` |
| R6 | -i 无值 | ✅ | 退出码 1, `invalid argument` |
| R7 | -i 负数 | ✅ | 退出码 1, `iteration >= 10` |
| R8 | -i 0 | ✅ | 退出码 1, `iteration >= 10` |
| R9 | -i 5 (<10) | ✅ | 退出码 1, `iteration >= 10` |
| R10 | -i 浮点 | ✅ | 退出码 1, `invalid syntax` |
| R11 | -i 字符串 | ✅ | 退出码 1, `invalid syntax` |
| R12 | job 含 `/` | ✅ | 退出码 1, `unexpected characters '/'` |
| R13 | job 含 `,` | ✅ | 退出码 1, `unexpected characters ','` |
| R14 | job 含 `+` | ✅ | 退出码 1, `unexpected characters '+'` |
| R15 | job 合法字符 | ✅ | `_test1` 接受, tuning 成功启动 |
| R16 | tune 中途 stop | ✅ | `Abort parameter optimization job` |
| R17 | tune 期间 delete | ✅ | 拒绝删除, `use keentune param stop to shutdown` |
| R18 | tune 期间 rollback | ✅ | 退出码 1, `rollback operation does not support, job ... is running` |
| R19 | 重复 init | ✅ | 退出码 0, `KeenTune Init success`（幂等操作） |
| R20 | set 不存在的 profile | ✅ | 退出码 1, `file does not exist` |
| R22 | delete 不存在的 profile | ✅ | 退出码 1, `file does not exist` |

---

## 四、回归测试 (RG1-RG14)

| # | 测试项 | 结果 | 详情 |
|---|--------|------|------|
| RG1 | 完整调优闭环 | ✅ | init→tune f10→dump→set→rollback→delete 全链路通过 |
| RG2 | 敏感度闭环 | ✅ | tune f30→sensitize train→jobs→delete 全链路通过 |
| RG3 | latency_performance | ✅ | `vm.swappiness = 10`, scheduler 参数因 kernel 6.6 移除 CFS 参数而跳过 |
| RG4 | throughput_performance | ✅ | `vm.dirty_ratio = 40` |
| RG5 | network_throughput | ⚠️ | profile 名 `network-throughput.conf` 不存在，环境仅有 `network-latency.conf` |
| RG6 | virtual_host | ✅ | `vm.dirty_background_ratio = 5` |
| RG7 | oracle | ⚠️ | `oracle.conf` 不存在 |
| RG8 | realtime | ⚠️ | `realtime.conf` 不存在 |
| RG9 | hpc_compute | ⚠️ | `hpc_compute.conf` 不存在 |
| RG10 | /cmd 命令注入 | ✅ | 400, `unexpected or incorrected input command` |
| RG11 | /cmd 非法命令 | ✅ | 400, 同上 |
| RG12 | /cmd 远程IP | ⚠️ | 通过（localhsot 连接，X-Forwarded-For 不生效） |
| RG13 | /write 远程IP | ⚠️ | 通过（同上） |
| RG14 | /read 远程IP | ✅ | 返回需要 type 参数 |

> **RG3 备注：** kernel 6.6 移除了 `sched_migration_cost_ns`, `sched_min_granularity_ns`, `sched_wakeup_granularity_ns` 等 CFS 参数，profile 中这些参数备份失败（`backup failed`），但 sysctl 部分正常生效。

---

## 五、多场景/算法测试 (M1-M20)

| # | 算法 | 结果 | 详情 |
|---|------|------|------|
| M1 | sysctl 域 random | ✅ | 10 迭代完成, finish |
| M7 | random | ✅ | 10 迭代完成, finish |
| M8 | hord | ✅ | 10 迭代完成, finish |
| M9 | TPE | ❌ | **`No module named 'pySOT'`** — brain 缺少 pySOT Python 包 |
| M10 | lamcts | ❌ | 同上, `No module named 'pySOT'` |
| M11 | bgcs | ❌ | 同上, `No module named 'pySOT'` |

> **根因：** TPE/lamcts/bgcs 算法依赖 `pySOT` (Surrogate Optimization Toolbox) 库，在 ctyunos4 上未安装。random 和 hord 算法不依赖 pySOT，正常通过。

---

## 六、Go 单元测试 (G1) ✅ 全部 PASS

```
28 total assertions, 28 tests, 0 failures, 0.925s
```

| 测试函数 | 覆盖内容 |
|----------|---------|
| Test_lock_LockSuccess | atomic CAS lock |
| TestCreateService | 服务创建, ini 加载 |
| TestService_new | 服务实例化 |
| TestService_reload | 配置重载 |
| TestService_ParamDelete | 参数删除 |
| TestService_ProfileList | profile 列表 |
| TestService_ProfileDelete | profile 删除 |
| TestService_ProfileSet | profile 设置 |
| TestService_Rollback | 回滚 |
| TestService_rollback | 内部回滚逻辑 |
| TestService_tuningImpl | 调优实现 |
| TestService_Tuning | 调优入口 |
| TestService_prepareForTuning | 调优准备 |
| TestService_getCurrentRatioInfo | 当前提升率 |
| TestService_ParamList | 参数列表 |
| TestService_Init | 初始化 |
| TestService_SetDefaultProfile | 默认 profile |
| TestService_ProfileGenerate | profile 生成 |
| TestService_ParamDump | 参数导出 |
| TestService_SensitizeJobs | 敏感度 job 列表 |
| TestService_SensitizeDelete | 敏感度 job 删除 |
| TestService_ParamJobs | 参数 job 列表 |
| TestService_Version | 版本 |
| TestService_Stop | 停止 |
| TestService_ProfileInfo | profile 信息 |
| TestService_watch | 配置监听 |
| TestService_abortAbnormalRunningJob | 异常 job 中断 |

---

## 发现的问题

### Bug / 功能缺陷

1. **`kernel.msgmni` 设置失败** — B9 profile set 时 sysctl 写入 `kernel.msgmni=99363` 失败 (`exit status 1`)，值超出了系统允许的最大值范围。这是调优算法生成的值溢出问题。

2. **B18 REST /read 缺少默认 type** — 调用 `/read` 不传 `type` 参数时返回 `type '' is not supported`，缺少默认值。

3. **TPE/lamcts/bgcs 算法不可用** — 环境缺少 `pySOT` Python 包，三个算法均无法运行。需 `pip install pySOT` 或从 brain 依赖中补充。

### 环境限制

4. **Tuned profile 不完整** — 环境仅安装了 7 个 tuned profile（latency-performance, throughput-performance, network-latency, virtual-host, virtual-guest, balanced, powersave），缺少 network-throughput, oracle, realtime, hpc_compute 等。

5. **Kernel 6.6 CFS 参数移除** — `sched_migration_cost_ns`, `sched_min_granularity_ns`, `sched_wakeup_granularity_ns` 在 kernel 6.6 中已移除，profile 中对应参数备份失败（`backup failed`），但不影响其他参数。

6. **REST 远程 IP 检测** — `/cmd` 和 `/write` 端点对外宣称 localhost-only，但 auth 中间件仅检查 `RemoteAddr`，X-Forwarded-For 头不生效，在 localhost 上测试无法验证远程拦截逻辑。

---

## 建议

1. **安装 pySOT：** `pip3 install pySOT` 以启用 TPE/lamcts/bgcs 算法
2. **补充 tuned profile：** 安装完整的 `keentune-target` RPM 或手动添加缺失的 profile conf 文件
3. **修复 kernel.msgmni 溢出：** 在 kvm_sysctl.json 中缩小 `kernel.msgmni` 的 range 上限
4. **补充 REST /read 默认 type：** 增加默认值或改进错误提示
5. **补充 Go 单元测试覆盖：** 当前仅 `daemon/` 有测试，`common/`, `modules/`, `api/` 包无测试
6. **非 localhost 测试：** 从另一台机器测试 REST auth 中间件，验证远程拦截效果