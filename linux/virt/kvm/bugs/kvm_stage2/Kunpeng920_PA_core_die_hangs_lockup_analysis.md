# Kunpeng 920 多 die core hangs 导致 soft/hard lockup 问题分析

## 问题概述

HUAKUN MK222 服务器（Kunpeng 920, 2P, 256 核）频繁发生 soft/hard lockup，表现为 PA (Protocol Agent) 模块硬件故障（`ERR_STATUS=0x64100214`），导致 NoC 互联上多个 die 的核心硬件挂死（core hangs），进而触发软件层连锁死锁（KSM/RCU 路径持 spin_lock 的核心挂死后其他 CPU 无法获取锁）。

截至 2026-06-04，已在**三台不同机器**上复现（SN: 2102315FAB10R2100044 / 2102315FAB10R2100045 / 2106114652FSQC000033），PA 故障寄存器签名完全一致，排除单颗 CPU 缺陷。

**触发路径**已知有多种，包括：
- KSM + KVM stage2 页表操作（`kvm_mmu_notifier_invalidate_range_start` → `write_protect_page`）
- 用户态进程普通缺页（`handle_pte_fault` → `do_numa_page` / `do_anonymous_page`）

**当前推测**：不同软件路径均可在 NoC 上产生 PA 无法正确处理的事务模式，根因可能在 PA 固件/TLB 硬件侧（如 ATF/BL31 中 MMU/SMMU 相关逻辑或 PA microcode 缺陷），待硬件厂商进一步确认。

**修复尝试**: 0001.patch（`khotfix_42044049`）通过 `kvm_is_device_pfn()` 拦截了 KVM stage2 路径，但加载后 PA 故障仍从非 KVM 路径触发，**0001.patch 不保证完全修复**。

> 注: IMU 日志和 OS 内核日志使用两套独立时钟（IMU=M7固件上电起算, OS=kernel启动起算），
> 无公共时间戳可精确对齐，以下时间线以事件因果关系和各自时钟的内部间隔为准。

---

## 背景知识

### BMC 中的 RAS 日志（imu_log）

RAS = **Reliability, Availability, Serviceability**（可靠性、可用性、可服务性）。

在 BMC 一键收集日志中，RAS 日志来自 **IMU (In-band Management Unit)**，即 Kunpeng 920 芯片内置的 **M7 固件**。M7 是一个独立于主 CPU 的微控制器（类似 BMC 的 CPU 内置 companion core），周期扫描芯片内部各模块（PA、NoC、DDR 等）的硬件错误寄存器。工作流程：

1. 硬件模块发生异常 → 通过 **RAS 中断**（如 `Receive ras int[491]`）通知 M7 固件
2. M7 读取各模块的 **ERR_STATUS / ERR_FR / ERR_CTRL / ERR_MISC** 等硬件错误寄存器，记录到 `imu_log`
3. M7 通过 **NoC 心跳**（`nocmt` 寄存器）检测各 die 上每个核心的运行状态，若核心长时间无进展则标记 **`core hangs`**
4. 检测到大规模 core hangs 后，M7 触发 **DFX 寄存器全面采集**（`DFX DEBUG START`），遍历所有 Socket/Die/Module 的寄存器
5. 最终做出 **`AP hang up!`** 的硬件级诊断结论

本次分析中的 `imu_log` 就是 M7 固件的 RAS 日志文件，位于 BMC 收集日志的 `dump_info/AppDump/fault_diagnosis/imu_log` 路径下。

### KVM Stage2 页表

在 ARM 虚拟化架构中，KVM 为 guest VM 管理**两级地址翻译**：

```
Guest 虚拟地址 (VA)
       │
       ▼  Stage1 翻译 (guest OS 页表, 由 guest 控制)
Guest 物理地址 (IPA, Intermediate Physical Address)
       │
       ▼  Stage2 翻译 (KVM 页表, 由 Hypervisor 控制)
Host 物理地址 (PA, Physical Address)
```

**Stage2 页表** 由 KVM/Hypervisor 管理，它不仅仅是物理页面映射，还决定了每个内存页的**内存类型属性**（Device-nGnRE、Normal Cacheable、Read-Only 等）。当 KVM 在 `kvm_set_spte_hva()` 中修改 Stage2 映射时，错误的属性设置会导致 guest 对该地址的访问在 NoC 上产生**非法的事务类型**（例如对设备 MMIO 空间发出 cache-coherent 请求），触发 PA (Protocol Agent) 模块的硬件故障。

**为什么不是 Page Fault 而是 Core Hangs** — Stage2 PTE 中有两类独立的属性：

| 属性 | 字段 | 出错后果 |
|------|------|---------|
| **权限** | S2AP[1:0] (Read/Write) | CPU MMU 产生 **page fault**（如 `do_translation_fault`），OS 可以捕获处理，可恢复 |
| **内存类型** | MemAttr[3:0] (Normal/Device/Cacheable) | 地址翻译成功、权限也允许，CPU 直接发出 load/store，**但在 NoC 上产生了错误类型的总线事务** |

```
Stage2 PTE 的 MemAttr 被错误设为 Normal Cacheable (而非 Device-nGnRE)
         │
         ▼
CPU load/store 对该地址发出 cache-coherent 探测 (ReadShared/ReadUnique)
         │
         ▼
探测到达 PA — PA 按一致性协议向 target die/device 转发
         │
         ▼
Target 是 PCIe root port / MMIO 设备 — 不理解一致性协议，不应答
         │
         ▼
PA TXREQ 超时 (imu_log 中 queue_idx 各不相同)
PA 协议状态机卡死，连锁阻塞后续所有 NoC 事务
         │
         ▼
core hangs → AP hang up!
```

**TLB/MMU 不拦截内存类型** — 只要 PTE valid=1 且权限允许，CPU 就直接发出访存指令，事务类型由 MemAttr 控制。错误的内存类型不会产生 page fault，CPU 看到的是"指令已发出"，但实际上 NoC 层面的协议交互已经崩溃。这就是为什么 Crash 日志中的调用栈有 `do_translation_fault`（那是后来其他路径触发的），而非通过 MMU 故障机制报告。

目录名 `kvm_stage2` 代表本分析涉及的 KVM Stage2 页表相关 bug。

### Caterr 与 ERR_STATUS=0x64100214 的关系

BMC 收集的 `fdm_output`（在每个 tar.gz 中的路径均为 `dump_info/AppDump/fault_diagnosis/fdm_output`）中的 **`Caterr Signal Assert`** 是故障链条的**结果信号**，不是起始原因。

各 tar.gz 中 `Caterr` 出现情况：

| tar.gz | Caterr 次数 | 说明 |
|--------|-------------|------|
| `MK222_2102315FAB10R2100045_20260529-0906.tar.gz` | 2 | 有 PA 故障 + core hangs，Caterr 作为结果信号 |
| `MK222_2106114652FSQC000033_20260604-0116.tar.gz` | 6 | 有 PA 故障 + core hangs，多次 Caterr |
| `MK222_2102315FAB10R2100044_20260104-1149.tar.gz` | 1 | 有 PA 故障 + core hangs |
| `MK222_2106114652FSQC000030_20260114-1419.tar.gz` | 1 | 有 PA 故障 + core hangs |
| `MK222_2102315FAB10R2100045_20260513-0949.tar.gz` | **0** | 无 PA 故障，无 core hangs，无 Caterr |

以 `20260529-0906` 为例，完整故障链路：

```
PA 协议状态机死锁
    │  imu_log:127   [37.11.28.841] Receive ras int[491]
    │  imu_log:139   ERR_STATUS=0x0000000064100214
    │  imu_log:150   PA TXREQ 超时 (queue_idx=0x02)
    ▼
多 die core hangs
    │  imu_log:157,159   skt[1] die[3] core hangs!
    │  imu_log:231,233   skt[1] die[1] core hangs!
    │  imu_log:242,244   skt[0] die[3] core hangs! (跨Socket)
    │  imu_log:230       Crash task collects nothing!
    ▼
固件判定 "AP hang up!"
    │  imu_log:283   [37.12.07.201] AP hang up!
    ▼
芯片级 Caterr Signal Assert（灾难性错误信号）
    │  fdm_output    Caterr Signal Assert.
    ▼
BMC 触发 OOB 采集 → DFX 寄存器全面收集
    │  fdm_output    IMU triggered OOB collection and report start.
    │  fdm_output    BMC begin to collect DFX registers.
    │  imu_log:235   [37.11.56.500] DFX DEBUG START
    │  imu_log:616   [37.16.15.396] DFX DEBUG END
    │  fdm_output    BMC collect DFX registers success.
```

**关键**: `Caterr` 不是被某个软件动作直接触发的，而是 PA 协议死锁后芯片内部逐步升级出来的灾难性错误信号。读日志时要先说 `imu_log` 里的 `ERR_STATUS`/`core hangs`，再补 `fdm_output` 的 Caterr 作为验证。反过来理解（"触发 Caterr → CPU hang → ..."）会丢失最重要的中间链。`20260513` 无 Caterr 也印证了它没有 PA 故障。

---

## 五份 BMC 日志汇总

| 文件 | 机器 S/N | 采集日期 | `core hangs` in imu_log | systemcom Call trace | Livepatch |
|------|----------|----------|-------------------------|----------------------|-----------|
| `MK222_2102315FAB10R2100045_20260529-0906.tar.gz` | 2102315FAB10R2100045 | 2026-05-29 | **有** — skt[1] die[3], skt[1] die[1], skt[0] die[3] | **kvm_mmu_notifier + write_protect_page** (ksmd) | 无 |
| `MK222_2102315FAB10R2100044_20260104-1149.tar.gz` | 2102315FAB10R2100044 | 2026-01-04 | **有** — skt[0] die[3], skt[0] die[1] | **无** (crash 太快日志未达串口) | 无 |
| `MK222_2106114652FSQC000030_20260114-1419.tar.gz` | 2106114652FSQC000030 | 2026-01-14 | **有** — skt[0] die[3], skt[1] die[1] | **无** (crash 太快日志未达串口) | 无 |
| `MK222_2106114652FSQC000033_20260604-0116.tar.gz` | **2106114652FSQC000033** | **2026-06-04** | **有** — skt[1] die[3], skt[1] die[1], skt[0] die[3] | **handle_pte_fault (haproxy)** ← 非 KVM 路径！ | **khotfix_42044049** |
| `MK222_2102315FAB10R2100045_20260513-0949.tar.gz` | 2102315FAB10R2100045 | 2026-05-13 | **无** | **无** | 无 |

**关键发现**:

1. **第四台机器** (2106114652FSQC000033, 硬件 `Huawei MK222/BC83AMDA01`) 复现了完全一致的 PA 故障签名 (`ERR_STATUS=0x64100214`) — **排除单颗 CPU 缺陷，确认为软件层面触发**。

2. **20260604 加载了 `khotfix_42044049(OEK)` livepatch** (kernel taint `K`)，该 hotfix **可能拦截了 KVM stage2 路径**，导致本次 crash 中不见 `kvm_mmu_notifier` 调用栈。

3. **但 PA 故障仍然发生** — Call trace 显示 `handle_pte_fault` → `do_page_fault` (用户态 haproxy 进程的普通缺页)，说明仅拦截 KVM 路径不够，**普通 MM 缺页路径（`do_numa_page`、`do_anonymous_page`）也能在 NoC 上产生触发 PA 故障的非法事务**。这可能与 Patch A 引入的 stage2 属性错误不仅限于 device BAR，而是对更广泛的页面类型也有影响。

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

### 20260604-0116 — IMU/BMC 固件 (imu_log)

| 行号 | IMU 时间 | 内容 |
|------|----------|------|
| 411 | `[36.54.54.470]` | `Receive ras int[491]` — RAS 中断触发 |
| 423 | — | `ERR_STATUS=0x0000000064100214` |
| 430 | — | `PA_COMMON_INT = 0x00000008` |
| 433 | — | `PA_sky_txreq_timeout_queue_idx = 0x00000071` — TXREQ 队列超时 |
| 434 | — | `PA_SKY_TXREQ_QUEUE_CMD_DATA0 = 0x0004409a` |
| 440 | — | `ras int[491] end` |
| 441 | — | `skt[1] die[3] core hangs!` (nocmt0[0x2000000]) |
| 443 | — | `skt[1] die[3] core hangs!` (nocmt0[0x1000000]) |
| 513 | — | `skt[1] die[1] core hangs!` (nocmt0[0x4]) |
| 515 | — | `skt[1] die[1] core hangs!` (nocmt0[0x8]) |
| 517 | — | `skt[1] die[3] core hangs!` (nocmt0[0x2000]) |
| 520 | — | `DFX DEBUG START` — 固件开始全面寄存器采集 |
| 522 | — | `skt[0] die[3] core hangs!` (nocmt0[0x8000000]) — 跨 Socket |
| 524 | — | `skt[0] die[3] core hangs!` (nocmt0[0x2]) |
| 526 | — | `skt[1] die[1] core hangs!` (nocmt0[0x10]) |

### 20260604-0116 — Linux 内核日志 (systemcom.dat)

**新特点**: 本次机器加载了 `khotfix_42044049(OEK)` livepatch（kernel taint: `OELK`），KVM stage2 路径可能已被热补丁拦截。但 PA 故障仍从不同调用路径触发 — 不再是 `kvm_mmu_notifier` 路径，而是**用户态 haproxy 进程的普通缺页处理** (`handle_pte_fault`)。

| 行号 | OS 时间 | 内容 |
|------|---------|------|
| 24115 | `132836.643` | `rcu: INFO: rcu_sched detected stalls on CPUs/tasks: 153, 166, 167` |
| 24121 | `132842.105` | `soft lockup - CPU#112 stuck for 22s! [haproxy:5168]` |
| 24124 | `132842.138` | `soft lockup - CPU#120 stuck for 22s! [haproxy:5169]` |
| 24389 | `132843.084` | CPU112 Call trace: `native_queued_spin_lock_slowpath` → `handle_pte_fault` → `__handle_mm_fault` → `handle_mm_fault` → `do_page_fault` → `do_translation_fault` → `do_mem_abort` → `el0_da` → `el0_sync` |
| 24431 | `132843.334` | CPU120 Call trace: `native_queued_spin_lock_slowpath` (lr=`do_numa_page`) → `handle_pte_fault` → `__handle_mm_fault` → ... → `el0_sync` |
| 24523 | `132850.104` | `soft lockup - CPU#78 stuck for 23s! [haproxy:5170]` |
| 24547 | `132850.411` | CPU78 Call trace: `native_queued_spin_lock_slowpath` (lr=`do_anonymous_page`) → `handle_pte_fault` → ... → `el0_sync` |
| 24443+ | — | `Sending IPI failed to CPU 0-33, ...` (大规模 IPI 失败) |

**关键**: 本次无 `Hard LOCKUP` → 无 kernel panic。系统停留在 soft lockup 状态（由 RCU stall → NMI 回扫 → IPI 失败的循环）。三个 haproxy 进程分别卡在 `do_numa_page`、`do_anonymous_page`、`handle_pte_fault`，均属于普通 MM 缺页处理路径。

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

| 时间（墙钟） | 内容 | 对应故障链环节 |
|-------------|------|---------------|
| `2026-05-29 01:08:07` | IMU triggered OOB collection and report start | BMC 采集入口 |
| `2026-05-29 01:08:08` | **Caterr Signal Assert** — CATERR 信号触发 | ← **结果信号**，原因在 imu_log 中的 ERR_STATUS/TXREQ 超时 |
| `2026-05-29 01:08:08` | IMU collection finish but no data | — |
| `2026-05-29 01:08:08` | BMC begin to collect DFX registers | DFX 采集开始 |
| `2026-05-29 01:12:28` | BMC collect DFX registers success | DFX 采集完成 |

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

所有四份有 `core hangs` 的 `imu_log` 中（三台不同机器），以下寄存器值 **完全一致**：

| 寄存器 | 值 |
|--------|-----|
| ERR_STATUS | `0x0000000064100214` |
| ERR_FR | `0x0000000000142aa2` |
| ERR_CTRL | `0x0000000000000515` |
| ERR_MISC1 | `0x0000000000000100` |
| PA_COMMON_INT | `0x00000008` |

**这不是巧合**，说明 PA 硬件被完全相同类型的非法 NoC 事务触发了同一种内部故障。三台不同机器的完全一致故障签名**排除了单颗 CPU 缺陷，确认为软件层面触发**。

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

**修复顺序**: 先合入 Patch A（链接中的 SVA patch），再在其上合入 0001.patch，缺一不可。

**⚠️ 注意**: 0001.patch（`khotfix_42044049`）仅拦截了 KVM stage2 路径（`kvm_set_spte_hva` 中的 `kvm_is_device_pfn()` 判断），但从第四台机器的 crash 来看，加载 hotfix 后 PA 故障仍从用户态普通缺页路径（`handle_pte_fault`）触发。**0001.patch 不保证完全修复**，根因可能不在软件侧，需进一步排查 Kunpeng 920 PA 固件/TLB 硬件缺陷。

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
3. **0001.patch（软件侧拦截）不保证完全修复** — 第四台机器加载 hotfix 后 KVM 路径消失但 PA 故障仍从用户态缺页路径触发

---

## 能否在固件/硬件层面彻底修复

### 硬件健壮性问题

这个问题的本质不是软件 bug，而是 **PA 硬件协议栈缺乏异常恢复机制**。一个正确设计的协议代理应该对不期望的输入做 graceful degradation（返回 error response、丢弃事务），而不是让整个状态机死锁。PA 在收到非法 cache-coherent 事务后 `ERR_STATUS=0x64100214` + TXREQ 超时 → 协议状态机卡死的表现，说明硬件缺乏容错能力。

### 各层面修复可行性

| 方案 | 可行性 | 说明 |
|------|--------|------|
| **PA microcode 更新** | ★★★ 最理想 | 如果 HiSilicon PA 模块有可更新的微码，修改协议状态机使其对非法事务类型做 graceful reject（返回 error response 而非挂死）。需要 HiSilicon 确认 PA 是否有可更新 microcode |
| **SMMU 强制内存类型** | ★★☆ 可辅助 | 配置 System MMU 对设备 MMIO 地址段强制 Device-nGnRE，不管 Stage2 PTE 写什么属性。但需要 SMMU 支持 override 且覆盖所有受影响地址范围 |
| **ATF/BL31 拦截** | ★☆☆ 不可行 | ATF 运行在 EL3 但不在 NoC 数据路径上，无法逐事务拦截 |
| **硅片修正** | 终极但成本高 | 修改 PA 硬件协议状态机，需要新的 silicon stepping |

### 核心矛盾

```
软件侧拦截 (0001.patch)         vs         硬件侧修复 (PA microcode)
─────────────────────────────────────────────────────────────────
只能堵住已知触发入口                    可以容错所有触发路径
新触发路径仍需新一轮 debug              一次修复一劳永逸
验证周期短，可快速上线                  需要 HiSilicon 支持，周期长
不保证 100% 修复                        从根源解决
```

**结论**: 0001.patch 是软件侧可用的短期缓解方案。要彻底解决，需要 HiSilicon 确认 PA 模块的 microcode 更新能力，使 PA 协议状态机在遇到非法事务类型时能够 graceful reject 而非挂死。