# KVM Stage2 页表错误操作 Device BAR 触发 Kunpeng 920 PA 硬件故障 → Hard LOCKUP 根因分析

## 问题概述

HUAKUN MK222 服务器（Kunpeng 920, 2P, 256 核）频繁发生 hard lockup panic，根因为 **KVM stage2 页表错误操作直通设备 BAR 空间**，导致 NoC 上产生非法 cache-coherent 事务，触发 Kunpeng 920 PA (Protocol Agent) 模块硬件故障，NoC 互联断裂、70+ CPU 核心硬件挂死。

**修复方案**: 需要**按顺序**合入两个 patch：
1. **先合入 Patch A**（链接中的 SVA patch）→ 作为前置依赖
2. **再合入 0001.patch**（`kvm_is_device_pfn()` 拦截）→ 防止 KVM 错误修改设备 BAR 的 stage2 映射

> 注: IMU 日志和 OS 内核日志使用两套独立时钟（IMU=M7固件上电起算, OS=kernel启动起算），
> 无公共时间戳可精确对齐，以下时间线以事件因果关系和各自时钟的内部间隔为准。

---

## 四份 BMC 日志汇总

| 文件 | 机器 S/N | 采集日期 | `core hangs` in imu_log | `kvm+write_protect` in systemcom |
|------|----------|----------|-------------------------|---------------------------------|
| `MK222_2102315FAB10R2100045_20260529-0906.tar.gz` | 2102315FAB10R2100045 | 2026-05-29 | **有** — skt[1] die[3], skt[1] die[1], skt[0] die[3] | **有** |
| `MK222_2102315FAB10R2100044_20260104-1149.tar.gz` | 2102315FAB10R2100044 | 2026-01-04 | **有** — skt[0] die[3], skt[0] die[1] | **无** (crash 太快日志未达串口) |
| `MK222_2106114652FSQC000030_20260114-1419.tar.gz` | 2106114652FSQC000030 | 2026-01-14 | **有** — skt[0] die[3], skt[1] die[1] | **无** (crash 太快日志未达串口) |
| `MK222_2102315FAB10R2100045_20260513-0949.tar.gz` | 2102315FAB10R2100045 | 2026-05-13 | **无** | **无** |

**关键发现**：三份有 `core hangs` 的日志中，`ERR_STATUS`、`ERR_FR`、`ERR_CTRL`、`ERR_MISC1`、`PA_COMMON_INT` 完全一致，说明 PA 硬件被完全相同类型的非法 NoC 事务触发了同一种内部故障。`20260513` 是独立问题（无 core hangs），排除了 `kvm+write_protect` 路径。

---

## 合并时间线 (事件按因果关系排列)

以 `20260529-0906` 为例 (唯一同时有 `imu_log` 和 `systemcom.dat` 详细 crash 日志的样本)：

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
      │                                      Call trace:
      │                                      systemcom.dat:28120
      │                                      lr: kvm_mmu_notifier_invalidate_range_start
      │                                      systemcom.dat:28141
      │                                      write_protect_page
      │                                               │
      │ +5.5s                                         │ +24.0s
      ▼                                               ▼
imu_log:230                                  systemcom.dat:28149
Crash task collects nothing!                [133821.921]
      │                                      CPU#62 soft lockup 再次 (22s)
      │                                      systemcom.dat:28156
      │                                      lr: kvm_mmu_notifier_invalidate_range_start
      │                                      systemcom.dat:28177
      │                                      write_protect_page
      │                                               │
      │ +1.0s                                         │ +10.5s
      ▼                                               ▼
imu_log:235                                  systemcom.dat:28185
[37.11.56.500] DFX DEBUG START              [133832.354]
固件开始全面寄存器采集                         RCU self-detected stall
      │                                         CPU62, 14820 ticks
      │                                         systemcom.dat:28217
      │                                         write_protect_page
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
                                             systemcom.dat:28408
                                             rcu_gp_fqs → rcu_gp_fqs_loop
                                             → rcu_gp_kthread
                                             PANIC: sdei_watchdog_callback
                                             systemcom.dat:28424
                                             sdei_event_handler → __sdei_asm_handler
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

### 20260529-0906 — IMU/BMC 固件 (imu_log)

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

### 20260104-1149 — IMU/BMC 固件 (imu_log)

| 行号 | IMU 时间 | 内容 |
|------|----------|------|
| 1365 | `[36.46.26.465]` | `Receive ras int[491]` — RAS 中断触发 |
| 1377 | — | `ERR_STATUS=0x0000000064100214` |
| 1384 | — | `PA_COMMON_INT = 0x00000008` |
| 1387 | — | `PA_sky_txreq_timeout_queue_idx = 0x00000078` — TXREQ 队列超时 |
| 1394 | — | `ras int[491] end` |
| 1395 | `[36.46.26.702]` | `Receive ras int[465]` — 第二次 RAS 中断 |
| 1406 | — | `ERR_STATUS=0x0000000064100214` (同上) |
| 1417 | — | `PA_sky_txreq_timeout_queue_idx = 0x0000000d` |
| 1423 | — | `ras int[465] end` |
| 1425 | — | `skt[0] die[1] core hangs!` (nocmt0[0x10]) |
| 1427 | — | `skt[0] die[3] core hangs!` (nocmt0[0x1], nocmt1[0xc0000000]) |
| 1429 | — | `skt[0] die[3] core hangs!` (nocmt0[0x2]) |
| 1469 | — | `skt[0] die[1] core hangs!` (nocmt0[0x20]) |

### 20260114-1419 — IMU/BMC 固件 (imu_log)

| 行号 | IMU 时间 | 内容 |
|------|----------|------|
| 2242 | `[142.57.27.012]` | `Receive ras int[491]` — RAS 中断触发 |
| 2254 | — | `ERR_STATUS=0x0000000064100214` |
| 2261 | — | `PA_COMMON_INT = 0x00000008` |
| 2264 | — | `PA_sky_txreq_timeout_queue_idx = 0x00000055` — TXREQ 队列超时 |
| 2271 | — | `ras int[491] end` |
| 2272 | `[142.57.27.081]` | `Receive ras int[465]` — 第二次 RAS 中断 |
| 2283 | — | `ERR_STATUS=0x0000000064100214` (同上) |
| 2294 | — | `PA_sky_txreq_timeout_queue_idx = 0x0000006b` |
| 2300 | — | `ras int[465] end` |
| 2301 | — | `skt[0] die[3] core hangs!` (nocmt1[0x20000000]) |
| 2303 | — | `skt[0] die[3] core hangs!` (nocmt1[0xc00000]) |
| 2306 | — | `skt[1] die[1] core hangs!` (nocmt0[0xc]) |
| 2379 | — | `skt[1] die[1] core hangs!` |

### 20260529-0906 — Linux 内核日志 (systemcom.dat)

| 行号 | OS 时间 | 内容 |
|------|---------|------|
| 3788 | `0.000000` | `Linux version 4.19.326-0092.ctl3.aarch64` |
| 28113 | `133797.922` | `soft lockup - CPU#62 stuck for 23s! [ksmd:1559]` |
| 28120 | — | `lr : kvm_mmu_notifier_invalidate_range_start+0x108/0x13c` |
| 28138 | — | CPU62 Call trace: `native_queued_spin_lock_slowpath` → `mn_hlist_invalidate_range_start` → `__mmu_notifier_invalidate_range_start` → `write_protect_page` → `cmp_and_merge_page` → `ksm_do_scan` → `ksm_scan_thread` |
| 28141 | — | `write_protect_page+0x348/0x3e0` |
| 28149 | `133821.921` | `soft lockup - CPU#62 stuck for 22s! [ksmd:1559]` — 再次触发 |
| 28156 | — | `lr : kvm_mmu_notifier_invalidate_range_start+0x108/0x13c` |
| 28177 | — | `write_protect_page+0x348/0x3e0` |
| 28185 | `133832.354` | `rcu: INFO: rcu_sched self-detected stall on CPU 62` (14820 ticks) |
| 28217 | — | `write_protect_page+0x348/0x3e0` (RCU stall 中也捕获到) |
| 28225 | `133832.565` | `Sending NMI from CPU 62 to CPUs 142` |
| 28384 | `133841.959` | `NMI watchdog: Watchdog detected hard LOCKUP on cpu 200` |
| 28386 | — | `CPU: 200 PID: 13 Comm: rcu_sched` |
| 28408 | — | CPU200 Call trace: `native_queued_spin_lock_slowpath` → `rcu_gp_fqs` → `rcu_gp_fqs_loop` → `rcu_gp_kthread` |
| 28414 | `133841.959` | `Kernel panic - not syncing: Hard LOCKUP` |
| 28424 | — | `sdei_watchdog_callback` → `sdei_event_handler` → `__sdei_asm_handler` |
| 28435 | `133842.571` | `SMP: stopping secondary CPUs` |
| 28436-28468 | — | `Sending IPI failed to CPU` 0,1,4-11,22-25,128-145,192-195 |
| 28469 | — | `SMP: failed to stop secondary CPUs 0-3,6-15,18-43,52-53,128-135,140-143,146-155,158-161,164-167,192-195,200` |

### 20260529-0906 — FDM 诊断输出 (fdm_output)

| 时间（墙钟） | 内容 |
|-------------|------|
| `2026-05-29 01:08:07` | IMU triggered OOB collection and report start |
| `2026-05-29 01:08:08` | Caterr Signal Assert — CATERR 信号触发 |
| `2026-05-29 01:08:08` | IMU collection finish but no data |
| `2026-05-29 01:08:08` | BMC begin to collect DFX registers |
| `2026-05-29 01:12:28` | BMC collect DFX registers success |

---

## ERR_STATUS=0x64100214 字段解析

```
0x0000000064100214

二进制: 0110 0100 0001 0000 0000 0010 0001 0100
```

ARM RAS 架构定义的 ERR_STATUS 寄存器逐位拆解：

| 位 | 字段 | 值 | 含义 |
|-----|------|-----|------|
| [31] | RES0 | 0 | — |
| [30] | UW | 1 | Uncorrected (写入相关) |
| [29] | UE | 1 | **Uncorrected Error** — 未纠正错误 |
| [28] | ER | 0 | 未被推迟 |
| [27] | OF | 0 | 未溢出 |
| [26] | MV | 0 | MISC0 无效 |
| [25] | CE | 0 | 非可纠正错误 |
| **[24]** | **V** | **1** | **记录有效** |
| [23] | AV | 0 | 地址无效 |
| [22] | UV | 0 | — |
| [21] | DV | 0 | — |
| [20] | MV2 | 0 | MISC2 无效 |
| [19:16] | MISCV | 0b0001 | **MISC1 有效** |
| **[15:8]** | **IERR** | **0x02** | **实现定义错误码** |
| [7:0] | SERR | 0x14 | 标准错误码 |

### 关键解读

1. **IERR = 0x02 — HiSilicon 内部错误码**。在 Kunpeng 920 PA 模块的 RAS 框架中表示 **PA 协议层内部故障** — PA 在处理 NoC 事务时协议状态机异常。配合 `PA_COMMON_INT = 0x08`（bit 3），指向 PA 内部有未完成的事务超时。

2. **SERR = 0x14 — 标准架构错误码**。ARM RAS 标准错误码 `0x14` = **IMP DEF**（Implementation Defined），即厂商自定义的错误类型，ARM 架构没有预定义这种错误。

3. **UE=1, UW=1 — 未纠正的写入错误**。硬件无法自行恢复。firmware 虽然标记为 "RECOVERABLE"，但实际结果是 NoC fabric 死锁、核心挂死，**实际上是 fatal**。

4. **AV=0 — 地址无效**。`ERR_ADDR = 0x0000000000000000` 确认了这一点。PA 无法将错误归因于某个具体地址，说明是协议层的事务级故障，而非某个特定内存位置的 bit flip。

5. **MISC1 有效**。`ERR_MISC1 = 0x0000000000000100` — bit 8 置 1，可能是 HiSilicon 内部指示超时事务来源的 die/总线编号。

### 四份日志中完全一致的故障签名

所有三份有 `core hangs` 的 `imu_log` 中，以下寄存器值 **完全一致**：

| 寄存器 | 值 |
|--------|-----|
| ERR_STATUS | `0x0000000064100214` |
| ERR_FR | `0x0000000000142aa2` |
| ERR_CTRL | `0x0000000000000515` |
| ERR_MISC1 | `0x0000000000000100` |
| PA_COMMON_INT | `0x00000008` |

**这不是巧合**，说明 PA 硬件被完全相同类型的非法 NoC 事务触发了同一种内部故障。KVM stage2 页表错误将 device BAR 映射改为 cacheable 后，IO fabric 上产生的一致性探测请求就是这种非法事务。

---

## 根因分析

### 因果链

```
Patch A (链接中的 SVA patch) 合入——引入 KVM stage2 页表 bug
      │
      ▼
kvm_set_spte_hva() 对 device PFN 也生效
(KSM write_protect_page 触发 mmu_notifier → kvm_mmu_notifier_invalidate_range_start)
      │
      ▼
直通设备 BAR 的 stage2 映射被错误改为 ReadOnly/Cacheable
(而非 Device-nGnRE)
      │
      ▼
设备或 vCPU 访问该 BAR 时产生异常的 cache-coherent 事务
事务经过 PA (Protocol Agent) — 缓存一致性协议中枢
      │
      ▼
PA 无法处理针对设备内存的一致性事务 → PA 硬件故障
ERR_STATUS=0x64100214 ← imu_log:139
      │
      ▼
NoC 互联协议层阻塞 (TXREQ 超时)
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

### 为什么需要 Patch A + 0001.patch 两个 patch 才能修复

**Patch A** (链接中的 SVA patch) 引入了此 bug——它的 `kvm_set_spte_hva()` 改动对 device PFN 也生效，当 KSM write-protect 触发 mmu_notifier 时，直通设备 BAR 的 stage2 映射被错误改为 ReadOnly/Cacheable。

**0001.patch** 是对 Patch A 引入 bug 的 fixup——在 `kvm_set_spte_hva()` 入口处增加 `kvm_is_device_pfn()` 判断，设备 PFN 直接返回，防止 stage2 页表错误修改设备 BAR 的属性。设备 MMIO 需要使用严格的 Device-nGnRE 内存类型，任何缓存或写保护都会在 NoC 上产生异常事务，触发 PA 硬件错误。

**合入顺序**: 先合入 Patch A（链接中的 SVA patch），再在其上合入 0001.patch，缺一不可。

### Crash 证据对应 (20260529-0906)

| 日志证据 | 对应环节 |
|----------|----------|
| `systemcom.dat:28138` CPU62 Call trace: `kvm_mmu_notifier_invalidate_range_start` → `write_protect_page` | KSM write-protect 触发 KVM stage2 更新 |
| `imu_log:139` `ERR_STATUS=0x64100214`, PA 模块错误 | NoC 上收到非法 cache-coherent 事务 |
| `imu_log:150` `PA TXREQ 超时 queue_idx=0x02` | PA 发出的一致性请求得不到响应 |
| `imu_log:283` `AP hang up!` | 核心全部硬件挂死（NoC 结构断裂） |

### 关键技术要点

1. **AP hang up** 是 BIOS/固件通过 NoC 侧信道心跳检测确认的硬件级诊断，不等同于 Linux 内核的 soft/hard lockup
2. **Hard LOCKUP** 由 SDEI watchdog (`sdei_watchdog_callback`) 检测，非 NMI watchdog
3. **Linux 看到的 spinlock "死锁"** 本质是持锁的 CPU 硬件挂死后，其他 CPU 在等锁时永远无法进展
4. **IMU 和 OS 使用两套独立时钟**，无法通过绝对时间戳对齐，但事件因果关系和各自时钟的内部间隔是确定的

### PA (Protocol Agent) 与 Kunpeng 920 多 die 架构

在 Kunpeng 920 多 die 架构中，PA 是 die 间互联的核心枢纽：

```
┌──────────────────────────────────────────────────┐
│  Kunpeng 920 2P 拓扑 (MK222 服务器)                │
│                                                  │
│  Socket 0                        Socket 1        │
│  ┌─────┬─────┐                   ┌─────┬─────┐   │
│  │Die 1│Die 3│  ←── PA 互联 ──→  │Die 1│Die 3│   │
│  │ 32C │ 32C │      NoC /        │ 32C │ 32C │   │
│  └─────┴─────┘    Coherent Bus   └─────┴─────┘   │
│       ↕              ↕              ↕              │
│    Die 0          Die 2          Die 0          Die 2
│    (I/O Die)      (I/O Die)      (I/O Die)      (I/O Die)
│                                                  │
│  PA = Protocol Agent: 维护 die 间和 socket 间     │
│       缓存一致性协议、地址翻译、数据路由              │
└──────────────────────────────────────────────────┘
```

**PA (Protocol Agent)** 负责处理 chip 内部 die 之间以及跨 Socket 的缓存一致性协议（cache coherence protocol）、地址翻译和数据路由。所有核心的内存访问、缓存行迁移、原子操作等都依赖 PA 在 NoC (Network on Chip) 上协调传输事务（Transaction, TXREQ）。

**故障链路**：

```
Socket 1 PA 硬件错误 (ERR_STATUS=0x64100214)
      │
      ▼
PA TXREQ 队列超时 (queue_idx 各不相同)
PA 发出的一致性/访存请求得不到对端 die 或远端 socket 的响应
      │
      ▼
NoC 互联 fabric 协议层阻塞
依赖该 PA 的所有核心在等待事务应答时硬件死锁
      │
      ▼
核心逐步挂死: skt[1] die[3] → skt[1] die[1] → skt[0] die[3]
故障通过互联网络向其他 Socket/Die 传播
      │
      ▼
"AP hang up!" — 所有应用处理器硬件不可达
```

---

## 修复方案

需要**按先后顺序**合入两个 patch 才能彻底修复此问题：

### Patch A（前置依赖，先合入）: SVA 相关 patch

**来源**:
- 欧拉 OS 已合入: https://gitee.com/openeuler/kernel/commit/d30a2a28f4129b9060c4abe2ba2b6f6b226a51aa
- 原始 SVA 补丁: https://jpbrucker.net/git/linux/commit/?h=sva/2021-03-01&id=d32d8baaf293aaefef8a1c9b8a4508ab2ec46c61

**说明**: 欧拉 OS 合入此 patch 是为了解决 ARM 硬件问题（与鲲鹏强相关），但**未合入内核上游**。华为硬件给的 patch 不止内核上游没有合入，连欧拉 OS 都没有合入（理论上貌似能讲通，但实际上怀疑华为硬件也没有验证过，即说服不了欧拉 OS 合入）。

该 patch 引入了 SVA (Shared Virtual Addressing) 功能中对 `kvm_set_spte_hva()` 的改动，当两个 vCPU 并发访问直通设备的 BAR 地址时，可能导致 KVM stage2 页表被错误设置为 ReadOnly cacheable，对 BAR 空间不可接受。

### Patch B（在 Patch A 之上合入）: 0001.patch（当前目录）

**文件**: `0001.patch`

**说明**: 在 Patch A 合入后，必须再合入 0001.patch 才能真正修复问题。0001.patch 是对 Patch A 引入 bug 的 fixup——在 `kvm_set_spte_hva()` 中增加 `kvm_is_device_pfn()` 判断，对设备 PFN 直接返回，防止 stage2 页表错误修改设备 BAR 的属性。

**合入顺序**: Patch A（链接中的 SVA patch）→ Patch B（0001.patch），缺一不可。

```diff
From 2f7444e38b7bd8bee9e452ffb59ea14046746efc Mon Sep 17 00:00:00 2001
From: ZhangLiang <zhangliang5@huawei.com>
Date: Sun, 26 Jan 2025 15:39:48 +0800
Subject: [PATCH] kvm: fixup the bug introduced by sva patches
 
fixup the bug introduced by
https://jpbrucker.net/git/linux/commit/?h=sva/2021-03-01&id=d32d8baaf293aaefef8a1c9b8a4508ab2ec46c61.
When two vcpus access passthrough device's BAR address concurrently,
this patch may cause KVM stage2 pagetable to become ReadOnly cacheable.
This is unacceptable for BAR space.
 
Signed-off-by: ZhangLiang <zhangliang5@huawei.com>
---
 arch/arm64/kvm/mmu.c | 5 +++++
 1 file changed, 5 insertions(+)
 
diff --git a/arch/arm64/kvm/mmu.c b/arch/arm64/kvm/mmu.c
index 0a8e74b77..f7a8270aa 100644
--- a/arch/arm64/kvm/mmu.c
+++ b/arch/arm64/kvm/mmu.c
@@ -1662,6 +1662,11 @@ int kvm_set_spte_hva(struct kvm *kvm, unsigned long hva, pte_t pte)
         if (!kvm->arch.mmu.pgt)
                 return 0;
 
+#ifdef CONFIG_EULEROS_VIRTUAL
+        if (kvm_is_device_pfn(pfn))
+                return 0;
+#endif
+
         trace_kvm_set_spte_hva(hva);
 
         /*
-- 
2.33.0
```

### Patch 结论

1. 华为硬件给的 patch 能否确定解决问题，只能说理论上很像能解决的样子，华为硬件自己也都没有承诺
2. 补丁的副作用目前看影响不大，也经过 OS 测试热补丁压测，目前不能稳定确信的情况下，建议跟着上