# MK222 Kunpeng 920 Hard LOCKUP 根因分析

## 问题概述

HUAKUN MK222 服务器（Kunpeng 920, 2P, 256 核）频繁发生 hard lockup panic，根因为 **Socket 1 PA (Protocol Agent) 模块硅片级硬件故障**，导致 NoC 互联断裂、70+ CPU 核心硬件挂死。

> 注: IMU 日志和 OS 内核日志使用两套独立时钟（IMU=M7固件上电起算, OS=kernel启动起算），
> 无公共时间戳可精确对齐，以下时间线以事件因果关系和各自时钟的内部间隔为准。

---

## 合并时间线 (事件按因果关系排列)

```
IMU 时钟 [HH.MM.SS.ms]        ← 独立时钟 →        OS 时钟 [uptime_sec]

imu_log:127                                       systemcom.dat:3788
[37.11.28.841]                                    [0.000000]
Receive ras int[491]                              Linux 启动 (4.19.326)
PA 模块硬件错误                                       │
      │                                               │ ~37h 正常运行
      │ +0.3s                                         │
      ▼                                               │
imu_log:139                                          │
ERR_STATUS=0x64100214                                │
PA TXREQ 超时 queue_idx=2                            │
      │                                               │
      │ +0s                                           │
      ▼                                               │
imu_log:157,159                                      │
skt[1] die[3] core hangs!                            │
(nocmt0=0x18000,0x20000)                            │
      │                                               │
      │ +16.4s                                        │
      ▼                                               │
imu_log:231,233                                      │
skt[1] die[1] core hangs!                            │
      │                                               │
      │ +4.5s                                         │
      ▼                                               ▼
imu_log:242,244                              systemcom.dat:28113
skt[0] die[3] core hangs!                   [133797.922]
(跨 Socket 传播)                             CPU#62 soft lockup (23s)
      │                                      ksmd 卡在 spin_lock
      │                                      (native_queued_spin_lock_slowpath)
      │                                               │
      │ +5.5s                                         │ +24.0s
      ▼                                               ▼
imu_log:230                                  systemcom.dat:28149
Crash task collects nothing!                [133821.921]
      │                                      CPU#62 soft lockup 再次 (22s)
      │                                               │
      │ +1.0s                                         │ +10.5s
      ▼                                               ▼
imu_log:235                                  systemcom.dat:28185
[37.11.56.500] DFX DEBUG START              [133832.354]
固件开始全面寄存器采集                         RCU self-detected stall
      │                                         CPU62, 14820 ticks
      │                                               │
      │ +9.0s                                         │ +0.2s
      ▼                                               ▼
imu_log:283                                  systemcom.dat:28225
[37.12.07.201]                               [133832.565]
══ AP hang up! ══                            Sending NMI CPU62 → CPU142
固件宣布所有 AP 硬件死亡                        (70+ CPU 全部无响应)
      │                                               │
      │ +49.9s (DFX 寄存器采集)                        │ +9.4s
      ▼                                               ▼
imu_log:616                                  systemcom.dat:28407,28414
[37.16.15.396] DFX DEBUG END                 [133841.959]
                                             ══ CPU200 Hard LOCKUP ══
                                             rcu_sched 卡在 spin_lock
                                             PANIC: sdei_watchdog_callback
                                                   │
                                                   │ +0.6s
                                                   ▼
                                          systemcom.dat:28435-28469
                                          [133842.571]
                                          SMP: stopping secondary CPUs
                                          IPI failed to CPU:
                                            0,1,4-11,22-25       ← S0 Die3
                                            128-145              ← S1 Die1/3
                                            192-195              ← S1 Die3
                                                   │
                                                   ▼
                                          systemcom.dat:28470+
                                          Reboot → BIOS 重新初始化
```

**IMU 内部间隔**: RAS 中断 → AP hang up = **38.4 秒**

**OS 内部间隔**: Soft lockup 首次报告 → Hard LOCKUP PANIC = **44.0 秒**

**事件先后顺序** (确定): PA 硬件错误 → die3 core hangs (S1) → die1 core hangs (S1) → die3 core hangs (S0, 跨Socket) → soft lockup → RCU stall → **AP hang up** → NMI 回扫失败 → hard lockup → panic

**AP hang up → Hard LOCKUP 间隔** = 无法精确计算，因两个时钟无同步对齐点，但因果关系明确：AP hang up 后 udelay/NMI 回扫累积导致 hard lockup。

---

## 关键行号索引

### IMU/BMC 固件 (imu_log)

| 行号 | IMU 时间 | 内容 |
|------|----------|------|
| 127 | `[37.11.28.841]` | `Receive ras int[491]` — RAS 中断触发 |
| 131 | — | `Found error node[2]` — 定位错误源 |
| 139 | — | `ERR_STATUS=0x0000000064100214` — PA 模块硬件错误状态 |
| 150 | — | `PA_SKY_TXREQ_QUEUE_CMD_DATA0=0x0004409a` — TXREQ 队列超时 |
| 157 | — | `skt[1] die[3] core hangs!` (nocmt0[0x18000]) |
| 159 | — | `skt[1] die[3] core hangs!` (nocmt0[0x20000]) |
| 230 | — | `Crash task collects nothing!` |
| 231 | — | `skt[1] die[1] core hangs!` (nocmt0[0x4]) |
| 233 | — | `skt[1] die[1] core hangs!` (nocmt0[0x8]) |
| 235 | `[37.11.56.500]` | `DFX DEBUG START` — 固件开始全面寄存器采集 |
| 242 | — | `skt[0] die[3] core hangs!` (nocmt0[0x40]) — 跨 Socket |
| 244 | — | `skt[0] die[3] core hangs!` (nocmt0[0x80]) |
| 252 | — | `skt[0] die[3] core hangs!` (nocmt0[0x1]) |
| 254 | — | `skt[0] die[3] core hangs!` (nocmt0[0x2]) |
| 283 | `[37.12.07.201]` | **`AP hang up!`** — 固件宣布所有应用处理器硬件死亡 |
| 616 | `[37.16.15.396]` | `DFX DEBUG END` — 寄存器采集结束 |

### Linux 内核日志 (systemcom.dat)

| 行号 | OS 时间 | 内容 |
|------|---------|------|
| 3788 | `0.000000` | `Linux version 4.19.326-0092.ctl3.aarch64` |
| 28113 | `133797.922` | `soft lockup - CPU#62 stuck for 23s! [ksmd:1559]` |
| 28138 | — | CPU62 Call trace: `native_queued_spin_lock_slowpath` → `kvm_mmu_notifier_invalidate_range_start` → `write_protect_page` → `cmp_and_merge_page` → `ksm_do_scan` → `ksm_scan_thread` |
| 28149 | `133821.921` | `soft lockup - CPU#62 stuck for 22s! [ksmd:1559]` — 再次触发 |
| 28185 | `133832.354` | `rcu: INFO: rcu_sched self-detected stall on CPU 62` (14820 ticks) |
| 28225 | `133832.565` | `Sending NMI from CPU 62 to CPUs 142` |
| 28384 | `133841.959` | `NMI watchdog: Watchdog detected hard LOCKUP on cpu 200` |
| 28386 | — | `CPU: 200 PID: 13 Comm: rcu_sched` |
| 28408 | — | CPU200 Call trace: `native_queued_spin_lock_slowpath` → `rcu_gp_fqs` → `rcu_gp_fqs_loop` → `rcu_gp_kthread` |
| 28414 | `133841.959` | `Kernel panic - not syncing: Hard LOCKUP` |
| 28424 | — | `sdei_watchdog_callback` → `sdei_event_handler` → `__sdei_asm_handler` |
| 28435 | `133842.571` | `SMP: stopping secondary CPUs` |
| 28436-28468 | — | `Sending IPI failed to CPU` 0,1,4-11,22-25,128-145,192-195 |
| 28469 | — | `SMP: failed to stop secondary CPUs 0-3,6-15,18-43,52-53,128-135,140-143,146-155,158-161,164-167,192-195,200` |

### FDM 诊断输出 (fdm_output)

| 时间（墙钟） | 内容 |
|-------------|------|
| `2026-05-29 01:08:07` | IMU triggered OOB collection and report start |
| `2026-05-29 01:08:08` | Caterr Signal Assert — CATERR 信号触发 |
| `2026-05-29 01:08:08` | IMU collection finish but no data |
| `2026-05-29 01:08:08` | BMC begin to collect DFX registers |
| `2026-05-29 01:12:28` | BMC collect DFX registers success |

---

## 根因分析

```
PA 模块硅片硬件故障
      │
      ▼
NoC 互联协议层阻塞 (TXREQ 超时, queue_idx=0x02)
      │
      ▼
70+ CPU 核心硬件挂死 (不可达, 不响应 IPI)
      │
      ├── 持有 spin_lock 的核心挂死 → 连锁死锁
      │   ├── CPU62 (ksmd): mmu_notifier 路径 spin_lock  → soft lockup (23s+22s)
      │   └── CPU200 (rcu_sched): RCU force_qs 路径 spin_lock → hard lockup
      │
      └── SDEI watchdog 检测到 CPU200 PC 未移动 → PANIC
```

### 关键技术要点

1. **AP hang up** 是 BIOS/固件通过 NoC 侧信道心跳检测确认的硬件级诊断，不等同于 Linux 内核的 soft/hard lockup
2. **PA (Protocol Agent)** 是 Kunpeng 920 多 die 间缓存一致性协议的中枢，PA 故障直接导致互联 fabric 断裂
3. **Linux 看到的 spinlock "死锁"** 本质是持锁的 CPU 硬件挂死后，其他 CPU 在等锁时永远无法进展
4. **Hard LOCKUP** 由 SDEI watchdog (`sdei_watchdog_callback`) 检测，非 NMI watchdog
5. **IMU 和 OS 使用两套独立时钟**，无法通过绝对时间戳对齐，但事件因果关系和各自时钟的内部间隔是确定的

---

## 建议

- **更换 Socket 1 的 Kunpeng 920 CPU** — PA 模块存在硅片级硬件缺陷
- 同时排查 **PCIe Card 5 (Zijin-DPU2.5)** 的 Surprise Down Error (93) 反复异常（近 2 个月内 26+ 次，独立问题）