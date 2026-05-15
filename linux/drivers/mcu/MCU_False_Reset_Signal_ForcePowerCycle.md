# BMC误判Host异常重启根因分析：MCU固件2.03.30多线程资源竞争再发

## 1 背景

2026年5月13日，DPU2.5鲲鹏环境节点10.8.94.254（SoC 10.8.94.231）出现异常重启，BMC一键收集日志后需定位根因。

此前同类问题（ZJSNC-2449，2026年4月）已定位为MCU固件多线程资源竞争导致误上报CPLD寄存器信息，BMC误判Host reset后误触发ForcePowerCycle。本次需确认是否同类根因。

## 2 推理链

先说结论，再说证据：

```
Host没重启 → 为什么BMC认为Host重启了？
  → BMC通过MCU读取CPLD寄存器获取reset信号
  → MCU有已知bug（多线程资源竞争）导致上报信息异常
  → MCU误上报了"Host reset"信号给BMC
  → BMC误判Host异常重启
  → host_checker 3分钟超时
  → BMC强制PowerCycle（把正常运行的Host强行断电重启了）
```

**一句话总结**：Host本来在正常运行，MCU固件bug导致误报了一个假的"Host reset"信号，BMC信了，3分钟后把Host强行断电重启——这才是业务中断的真正原因。

## 3 关键概念

在理解根因之前，先梳理Host reset信号从硬件到BMC的完整路径：

```
Host硬件reset信号线 → CPLD硬件检测 → CPLD寄存器(SysResetDetected位) → MCU通信通道(SMBus) → BMC读取
```

| 组件 | 说明 |
|------|------|
| **CPLD** | Complex Programmable Logic Device，可编程硬件芯片。通过物理信号线检测Host reset，将结果写入SysResetDetected寄存器位。纯硬件逻辑，不运行软件 |
| **MCU** | 微控制器，运行固件(2.03.30)。作为BMC与CPLD之间的通信桥梁——BMC通过MCU(SMBus通道)读取CPLD寄存器值 |
| **BMC** | 基带管理控制器。通过MCU读取CPLD寄存器获取Host reset状态，如果检测到SysResetDetected=1且非BMC主动发起，则判定Host异常reset |

**核心**：CPLD只做信号检测和寄存器存储，MCU负责通信传递。如果MCU把CPLD寄存器值传错了，BMC就会收到假的reset信号。

## 4 证据链

### 证据1（新）：Host OS内核日志在00:51:42前后完全连续，无任何重启记录

BMC在00:51:42检测到`SysResetDetected=1`，但Host OS的messages日志在该时间前后完全连续运行，kernel uptime计数器从31664秒连续到31806秒，没有任何中断：

```
messages-20260513:168053  00:51:09 kernel uptime连续 kubelet正常
messages-20260513:168074  00:51:43 kubelet正常（BMC说Host reset后1秒！）
messages-20260513:168075  00:51:47 kubelet正常
messages-20260513:168076  00:51:49 kubelet正常
messages-20260513:168081  00:52:01 systemd正常
messages-20260513:168094  00:52:09 kubelet正常
messages-20260513:168106  00:52:29 kernel: [31802秒] vfio-pci正常（uptime连续）
```

kernel uptime从31664秒连续到31806秒，Host从未重启。00:51:42 BMC声称检测到Host reset时，Host OS还在正常写日志。

**这是判断是否MCU误报的关键证据**：Host OS日志在00:51:42前后是否有重启记录——无记录=MCU误报（同ZJSNC-2449），有记录=Host真异常需进一步排查。本次明确：**无记录，MCU误报。**

> 日志路径：`messages-20260513/messages-20260513`

### 证据2：BMC通过MCU读取CPLD寄存器获得假reset信号

```
app.log:10675  00:51:42 Refresh system reset detected 1
app.log:10676  00:51:42 reset not initiated by BMC
app.log:10680  00:51:42 [monitor-power] monitor system reset, power state is ON, SysResetDetected value is 1
```

Host实际没重启 → SysResetDetected=1是假信号 → MCU误上报CPLD寄存器

> 日志路径：`LogDump/app.log`

### 证据3：对比正常CTCC生命周期操作 vs 异常00:51:42

**正常CTCC host_checker（09:14:20，BMC主动控制）**：

```
09:14:20  Notify cpld to send short button signal through hwproxy Accessor   ← BMC主动通知CPLD发reset信号
09:14:20  Set sys_reset_flag to true                                         ← BMC标记是自己发起的
09:14:23  Refresh system reset detected 1                                    ← 检测到reset（预期中的）
09:14:23  Set sys_reset_flag to false                                        ← 清除标记
09:14:53  clear system reset detected                                        ← 30秒后正常清除
```

**异常00:51:42（BMC未发起）**：

```
00:51:42  Refresh system reset detected 1                                    ← 检测到reset，但没有"BMC通知CPLD"的前置步骤！
00:51:42  reset not initiated by BMC                                        ← BMC确认不是自己发起的
00:51:42  Refresh system reset detected 0                                    ← 0.66秒后reset信号消失
00:51:44  Set sys_reset_flag to false                                        ← 异常reset没有sys_reset_flag=true的前置步骤
00:52:14  clear system reset detected                                        ← 32秒后清除
```

**关键差异**：正常操作时BMC先通知CPLD发送信号再检测到reset；异常时没有BMC通知步骤，reset信号直接从CPLD寄存器读出 → **MCU误上报CPLD寄存器信息**。

> 日志路径：`LogDump/app.log`

### 证据4：与ZJSNC-2449同类根因，MCU_BCU固件版本完全一致

| MCU芯片 | ZJSNC-2449（上次） | 本次 | 是否一致 |
|---------|-------------------|------|---------|
| MCU_BCU_010101 | **2.03.30** | **2.03.30** | **完全一致** |
| MCU_IEU | 1.47.00 | 1.26.00(01010109) / 1.47.00(01010D) | IEU主体一致 |
| MCU_CLU_010103 | 缺失 | 缺失 | 都缺失 |

上次MCU_BCU = 2.03.30，本次MCU_BCU = 2.03.30，**问题未修复再发生**。

上次报告明确指出："MCU由于多线程的资源竞争导致的MCU上报给BMC的信息异常，而未知原因重启的信号同样也是BMC通过MCU读取的CPLD寄存器信息"。

> 上次日志路径：`MK222_2102315JJK10R3100019_20260427-1704/dump_info/AppDump/general_hardware/mcu_info`
> 本次日志路径：`AppDump/general_hardware/mcu_info`

### 证据5：BMC误判后的连锁反应——强制断电重启正常运行的Host

```
app.log:10678  00:51:42 host_agent: reset monitor: sys reset detected to 1     ← BMC误判Host reset
app.log        00:54:42 host_checker timeout                                    ← Host实际没reset，无法通过checker
app.log:10749  00:54:43 execute force power off                                 ← ForcePowerCycle restart_cause=Oem
```

BMC误判 → host_checker超时 → ForcePowerCycle → 强制断电重启正常运行的Host

**Host被ForcePowerCycle强制重启才导致业务中断**——不是Host自己出了问题，是BMC基于MCU的假信号把正常运行的Host强行杀掉了。

> 日志路径：`LogDump/app.log`

## 5 根因确认

### 判断逻辑

采用与ZJSNC-2449相同的判断逻辑：

| 条件 | 结果 | 本次情况 |
|------|------|---------|
| 仅带外(BMC)有reset记录 | BMC可能误判 | ✅ BMC检测到SysResetDetected=1 |
| Host OS/BIOS无reset记录 | Host实际没重启 | ✅ messages日志连续，uptime连续，无任何重启记录 |
| Host实际没重启 + BMC读到reset信号 | MCU误上报CPLD寄存器 | ✅ Host没reset，但BMC读到SysResetDetected=1 |

### 信号传递路径中的出错点

```
Host硬件reset信号线（无信号，Host没reset）
    ↓
CPLD硬件检测（CPLD寄存器应为0）
    ↓
MCU通信通道 ← ❌ 出错点：MCU多线程资源竞争导致上报信息异常，把CPLD寄存器值传成了1
    ↓
BMC读取（读到SysResetDetected=1，误判Host reset）
    ↓
host_checker超时 → ForcePowerCycle → 正常运行的Host被强制断电重启
```

### 结论

**与ZJSNC-2449同类根因：MCU_BCU固件2.03.30多线程资源竞争导致上报BMC的CPLD寄存器信息异常（SysResetDetected从0误报为1），BMC误判Host异常reset，触发host_checker超时后ForcePowerCycle，将正常运行的Host强制断电重启。两次事件MCU_BCU固件版本完全一致（2.03.30），问题未修复。**

### 排除项

| 假设 | 排除依据 |
|------|---------|
| 硬件问题导致Host重启 | 硬件重启会带外上报硬件告警 + 掉电无法恢复后自动重启进入系统，与本次不一致；且Host OS日志连续证明Host没有真的重启 |
| Host OS自身异常重启 | messages日志在00:51:42前后完全连续，无reboot/shutdown/crash/panic记录 |
| BMC主动发起reset | BMC日志明确记录"reset not initiated by BMC" |

## 6 时间线

| 时间 | 事件 | 来源 |
|------|------|------|
| 05-03 | framework.log首次记录SMBus失败（比app.log早9天） | framework.log |
| 05-11 23:06 | PCIe error 201首次出现（4154次，贯穿全程） | app.log |
| 05-12 01:26 | app.log首次SMBus失败 + IOError + DPU掉电#1 | app.log |
| 05-12 09:08/13:01/15:54 | CTCC正常生命周期操作 + host_checker×3（均在3分钟内恢复） | app.log |
| 05-13 00:46:58 | SMBus失败 + IOError + DPU掉电On→Off→On(1秒恢复) | app.log |
| **05-13 00:51:42** | **MCU误报SysResetDetected=1 → BMC误判Host reset** | **app.log:10675** |
| 05-13 00:54:42 | host_checker超时 | app.log |
| 05-13 00:54:43 | ForcePowerCycle（restart_cause=Oem） | app.log:10749 |

## 7 建议

| 优先级 | 建议 | 责任方 |
|--------|------|--------|
| P0 | **升级MCU_BCU固件**，修复多线程资源竞争问题 | SDI MCU团队 |
| P0 | 联系SDI MCU团队读取MCU error log，确认MCU内部是否有异常记录 | SDI MCU团队 |
| P1 | BMC软件改进：检测到SysResetDetected=1时，先通过IPMI/Redfish验证Host OS是否真的在运行，再决定是否触发ForcePowerCycle | BMC软件团队 |
| P2 | 复现测试，保持环境不变观察是否再次出现 | 测试团队 |
| P2 | Host OS配置改进：配置GRUB kbox_mem参数、启用rasdaemon、内核启用pstore | OS团队 |
| P2 | BIOS配置改进：配置ACPI ERST表，正确配置_OSC暴露AER/DPC能力 | BIOS团队 |