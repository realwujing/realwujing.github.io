# text_poke_bp_batch INT3 未处理崩溃分析（QEMU TCG 陈旧取指）

> 内核源码 commit：`d4c6c1d302656`
> 修复分支：`ctkernel-lts-6.6/bugfix-text_poke-int3-fallback`

## 结论先行

- **现象**：`int3: 0000` 未处理断点陷阱，崩溃地址总是某内核函数入口 `func+0x1`（ftrace 插桩点）。
- **直接原因**：CPU 取指拿到一个**内存里其实已经不存在的 INT3（陈旧取指）**，`poke_int3_handler` 在当前活跃 batch 里找不到该地址 → 返回 0 → `die("int3")`。
- **根本原因**：**QEMU TCG（纯软件模拟）** 对跨 CPU 修改代码（cross-modifying code）的 translation block 失效时序，与真实硬件的 `sync_core`/IPI 序列化语义不一致。**不是内核 bug，不是硬件 bug，也不是 KVM bug。**
- **决定性证据**：把 VM 的 `<domain type='qemu'>`（TCG 纯模拟）换成 KVM（硬件虚拟化）后，**问题不再复现**。
- **澄清误导线索**：guest 里显示的 “AMD” 只是 QEMU 模拟出的 CPU 型号；宿主其实是 **Intel**，且走的是纯模拟而非 KVM。早期“KVM-on-AMD 硬件缺陷”的判断是错的。
- **修复**：
  - 治本（不改内核）：测试/运行改用 **KVM**；若必须 TCG，则 `-accel tcg,thread=single`（关 MTTCG）/ 单 vCPU / 升级 QEMU。
  - 内核兜底（TCG 专用）：`poke_int3_emulate_stale()` —— 命中陈旧 INT3 时**从内存解码真实指令并向前模拟**，不崩。

---

## 基本信息

| 项目 | 内容 |
|------|------|
| Panic 类型 | `int3: 0000` — 未处理的 INT3 断点陷阱 |
| 运行环境 | QEMU **TCG 纯模拟**（VM XML `type='qemu'`），宿主 Intel，guest 模拟为 AMD |
| 复现 | TCG 下可复现（需压测 / fuzzing，分钟级）；**KVM、物理机不复现** |

## 崩溃记录

> 说明：前三次都是**同一个无修复基线内核**的三次独立崩溃（不是"每次换了不同修复"）。

| 日志 | 内核 | 崩溃 CPU | 崩溃函数 | 上下文 | 备注 |
|------|------|----------|----------|--------|------|
| vmcore-dmesg.txt | baseline | CPU 1 | `update_rt_rq_load_avg+0x1` | 软中断（与 text_poke 同 CPU） | vmcore 不完整 |
| dmesg.txt | baseline | CPU 0 | `exclusived_task_running+0x1` | irq 返回 → schedule | 串口日志 |
| dmesg1.txt | baseline | CPU 0 + CPU 1 | `up_read+0x1` | 缺页 → up_read | 两 CPU 同时崩 |
| dmesg3.txt | 调试内核 | — | （无 int3 崩溃） | ftrace-in-idle 的 RCU 告警，与本 bug 无关 | 干扰项 |
| vmcore-dmesg1.txt | 调试内核 + 压测 | CPU 0 + CPU 1 | `__mod_node_page_state+0x1` | 缺页 → filemap_map_pages | **抓到 INT3-DEBUG** |
| dmesg4.txt | rewind 修复 + 压测 | CPU 0 + CPU 1 | `__do_sys_getpid+0x0` | — | **soft lockup 死循环**（修复尝试失败） |

## 复现命令

```bash
# baseline
./syz-execprog -arch=amd64 -executor=./syz-executor -repeat=1 -procs=1 -sandbox=none -output ./log_6
# 压测脚本（更稳定复现）：见 stress_text_poke.sh（疯狂开关 ftrace + 缺页/调度风暴）
```

---

## 触发链与 INT3 三步协议

### 触发链：ftrace 开/关触发批量 text_poke

```
do_exit / perf_event_open
  → (un)register_ftrace_function
    → ftrace_replace_code      ← 遍历所有 ftrace 插桩点
      → text_poke_queue        ← 积攒 patch 条目到 tp_vec[]（按地址有序）
        → text_poke_bp_batch   ← 每 256 条（TP_VEC_MAX）一批，跑 INT3 三步协议
```

### text_poke_bp_batch 三步协议

x86 无法原子写多字节指令，所以借助单字节的 INT3(0xCC) 当“字节级锁”：

```
bp_desc.vec = tp; bp_desc.nr_entries = n; atomic_set_release(&refs, 1);
Step 1: 把每个地址第 0 字节写成 0xCC        → text_poke_sync()  ← IPI 所有 CPU sync_core
Step 2: 写新指令第 1..N-1 字节（首字节仍 0xCC） → text_poke_sync()
Step 3: 把首字节从 0xCC 换回真实 opcode      → text_poke_sync()
atomic_dec_and_test(&refs)                  ← refs→0，INT3 窗口关闭
```

窗口期内任何 CPU 命中 INT3，都由 `poke_int3_handler` 查 `bp_desc.vec` 找到条目、按 `tp->opcode` **向前模拟**（CALL/JMP/Jcc/RET/NOP），不真正执行 INT3。

### INT3 分发与两条失败路径

```
exc_int3()
  ├── poke_int3_handler()
  │     ├── try_get_desc()  refs==0 → return 0          ← 失败路径 A
  │     ├── bsearch(ip, bp_desc.vec) 找不到 → return 0  ← 失败路径 B
  │     └── 按 tp->opcode 向前模拟 → return 1（正常）
  ├── do_int3()  ← 返回 0 时：kprobe / kgdb / notify_die
  └── die("int3") ← 全部未认领 → 崩
```

---

## 根因定位全过程

### 1. 直接原因：陈旧 INT3 取指（path B）

最干净的 vmcore-dmesg.txt（同 CPU）给出铁证：

- IRQ 上下文崩在 `update_rt_rq_load_avg+0x1`（CPU 在 `+0x0` 命中 INT3 后压 RIP+1），
  但同一行 **Code dump 显示该处内存已是完整 NOP `0f <1f> 44 00 00`，没有 `0xcc`**。
- TASK 上下文正在 `text_poke_bp_batch` 里 patch **另一个相距 ~28MB 的地址**
  （RBX `ffffffff8258f131` vs `update_rt` `ffffffff83ca4340`），单批 256 条目跨度只有
  几百 KB，**不可能横跨 28MB** → `update_rt` 不在当前批次。

→ CPU 取到的 `0xcc` 是**之前已完成 batch 残留的陈旧取指**；当前活跃 batch 的 `vec`
不含该地址，`poke_int3_handler` 二分查找失败（path B）→ `die`。

### 2. 实测确认（vmcore-dmesg1.txt，调试内核打印 bp_desc）

压测下两 CPU 同时命中，INT3-DEBUG 输出：

```
INT3-DEBUG: cpu=1 ip=__mod_node_page_state+0x0 (ffffffffb55c1c00) refs=1 nr_entries=256 vec=ffffffffb808ea40 in_vec=0 path=B:addr-not-in-batch
INT3-DEBUG: cpu=0 ip=__mod_node_page_state+0x0 (ffffffffb55c1c00) refs=1 nr_entries=256 vec=ffffffffb808ea40 in_vec=0 path=B:addr-not-in-batch
INT3-DEBUG: vec[0]=ffffffffb55cb9b0 vec[255]=ffffffffb55e8410
```

- `refs=1`（有活跃 batch）、`in_vec=0`（地址不在当前 batch）→ **path B 实锤**。
- 崩溃地址 `b55c1c00` 比当前 batch 最低地址 `vec[0]=b55cb9b0` **还低 ~39KB** → 属于上一个已完成的、地址更低的 batch。
- 该处 Code dump 显示真实 opcode（一条 `e8` CALL，ftrace 已开启、Step 3 已写回），**内存里无 `0xcc`** → 再次坐实陈旧取指。

### 3. 关键机制：TCG 下 sync_core 形同虚设，数据/取指视图不一致

- `poke_int3_handler` 正常工作依赖一条硬件契约：每个 CPU 在“代码被改”到“下次取指”之间执行一条序列化指令（`text_poke_sync` 用 IPI 让每个 CPU 跑 `sync_core`/IRET-to-self），强制重新取指。
- dmesg4.txt 揭示：出故障的 vCPU **数据读**已看到新字节（`0xe8`），但**取指**仍是 `0xcc`，且 `sync_core()` 执行后**仍取到 `0xcc`**。
- 这不是真实硬件的 i-cache 行为（x86 i-cache 与内存写一致，`sync_core` 处理的是预取流水线），而是 **QEMU TCG 把 guest 代码翻译成 translation block(TB) 缓存**，对跨 CPU 改代码的 **TB 失效时序**没有忠实建模成 `sync_core`/IPI 这个重新取指屏障 → 一个 vCPU 仍在执行缓存里旧的、带 INT3 的 TB。

### 4. 决定性实验：换 KVM 即消失

把 VM XML 从 `type='qemu'`（TCG 纯模拟）改为 KVM（硬件虚拟化），**问题不再复现**。

- 物理机、KVM：在真实 CPU 上跑，序列化契约被忠实兑现 → batch 做完 Step3+sync 后不可能再有陈旧 INT3 → **窗口为零**。
- TCG：契约未兑现 → 窗口被打开，压测分钟级即可命中。

→ 这是 **QEMU TCG 的模拟保真度问题**，常与 **MTTCG（多线程 TCG）** 下跨 CPU TB 失效竞态相关。

---

## 为什么 TCG 纯模拟会崩、KVM 加速不会（详解）

两者执行 guest 代码的方式根本不同，这决定了 INT3 协议的“序列化契约”在一边成立、在另一边不成立。

### KVM：guest 指令直接跑在物理 CPU 上

KVM 是硬件辅助虚拟化（Intel VT-x / AMD-V）。guest 的每条指令**就是物理 CPU 在执行**，QEMU 不翻译、不缓存代码。于是 x86 硬件的两条铁律原样生效：

1. **i-cache 与内存写一致**：一个 CPU 写代码页，硬件 snoop 会让其他 CPU 的 i-cache 行失效——其他 CPU 不会从 i-cache 拿到旧字节。
2. **序列化指令 = 强制重新取指**：`sync_core()`（`IRET`-to-self 或 `SERIALIZE`）会丢弃 CPU 的预取/已译码流水线，强制从一致的内存层级重新取指。

INT3 三步协议正是踩在这两条铁律上设计的：Step 3 写回真实首字节后，`text_poke_sync()` 用 IPI 让**每个物理 CPU 都跑一次 `sync_core()`**，然后才 `refs→0`。等协议结束，所有 CPU 都已重新取指，**绝不可能再有陈旧 INT3**。窗口为零。物理机同理。

### TCG：guest 代码被翻译成 host 代码并缓存复用

TCG（Tiny Code Generator）是纯软件模拟：它把一段 guest 指令**动态翻译**成等价的 host 指令，打包成 **translation block（TB）缓存起来**；下次再执行同一个 guest PC，直接复用缓存的 TB，不再重译——这就是它的性能来源。

所以 `__do_sys_getpid` 这样的函数，**只被翻译一次**，之后反复跑的是那块缓存的 host 代码。一旦 guest 改了这个函数的字节，TCG 必须**作废（invalidate）**对应的 TB，否则会继续跑旧翻译。问题恰恰出在“作废”这件事上，有三个错位点：

**错位点 1：TCG 靠“写内存”触发作废，而不是靠“序列化指令”。**
真实硬件上，执行方 CPU 是在**命中 `sync_core`** 时被迫重新取指的。TCG 没有“硬件预取缓冲”这个概念，它的 TB 是否有效，取决于**对应 guest 内存有没有被写过**。text_poke 的写确实会写到那一页（虽然走的是 `poking_mm` 别名地址，但映射到同一 guest 物理地址，按理应触发作废）——但作废的**时机**与执行方是否正在跑旧 TB 之间，存在竞态。

**错位点 2：guest 的 `sync_core()` 对 TCG 几乎没有意义。**
`sync_core()`（`IRET`-to-self）在 TCG 眼里只是“又一条普通 guest 指令”。TCG 会在某些边界结束当前 TB，但**它不会因为执行了一条 IRET，就去把别处（被 patch 的那个函数）已经缓存的 TB 强制重译**。也就是说：内核精心设计、在真实硬件上保证正确性的那套 `sync_core` 屏障，**在 TCG 里对“那个陈旧 TB”没有任何清除作用**。这正对应实测现象——`sync_core()` 执行后，取指仍是旧的 `0xcc`。

**错位点 3：MTTCG 多线程下，跨 vCPU 的 TB 作废时序与 guest 的序列化点不对齐。**
开了 MTTCG（多线程 TCG），每个 vCPU 跑在独立 host 线程里**并行执行**。当 vCPU‑A 写代码（Step 3 还原指令、并作废 TB）时，vCPU‑B 可能正在执行、或刚链入那块**从旧字节翻译出来、仍含 INT3** 的 TB。TCG 跨线程作废靠异步任务 / `cpu_exit` 把对方踢出当前 TB，但这套时机**不等同于** guest 里的 `text_poke_sync` IPI；于是出现一个窗口：

- guest 内存：`refs→0`、首字节已是真实 opcode（`0xe8` / `0x0f`）；
- vCPU‑B 的取指：仍在跑那块带 `0xcc` 的旧 TB → 触发 INT3；
- `poke_int3_handler`：当前活跃 batch 的 `vec` 不含这个地址（它属于已完成的上一批）→ 返回 0 → `die`。

这与实测的 `数据读=新指令、取指=旧 0xcc、refs=1、in_vec=0、path=B` 完全吻合。

### 一句话对照

| | KVM / 物理机 | QEMU TCG |
|---|---|---|
| guest 代码怎么跑 | 直接在物理 CPU 上执行 | 翻译成 host TB、缓存复用 |
| 改代码后旧指令怎么消失 | 硬件 i-cache 一致性 + `sync_core` 强制重新取指 | 靠 TCG 作废 TB（时序与 guest 序列化点不对齐） |
| guest `sync_core()` 的效果 | 真·重新取指（架构保证） | 基本无效（不会重译别处缓存的 TB） |
| INT3 协议的窗口 | **为零**（契约被兑现） | **被打开**（契约未兑现） |
| 结果 | 不复现 | 压测分钟级复现 |

> 一句话：**内核的 INT3 协议把正确性押在“序列化指令=重新取指”这条硬件契约上；KVM/物理机忠实兑现，TCG 不兑现。** 所以这不是“物理机也有、只是罕见”的竞态——物理机上根本没有这个窗口，它是 TCG 缓存翻译造出来的。

---

## 走过的弯路（已否定的修复）

| 尝试 | 思路 | 为何失败 |
|------|------|----------|
| `local_bh_disable` | 禁本 CPU 软中断 | 只挡同 CPU 软中断；跨 CPU 照崩 |
| `local_irq_disable` / `local_irq_save` | 禁本 CPU 全部中断 | CPU-local，对其他 CPU 无效；且把 `text_poke_sync→on_each_cpu` 包进关中断区（`smp_call_function_many_cond` 有 `lockdep_assert_irqs_enabled`，“Can deadlock when called with interrupts disabled”）—— **本身危险** |
| `poke_int3_recover_stale`：`sync_core()`+退回 RIP 重新执行 | 让 CPU 重新取指 | **dmesg4 死循环**：TCG 下重新取指仍是陈旧 `0xcc` → 反复陷阱；另一 CPU 卡在 `text_poke_sync` 等 IPI → 双核 soft lockup + RCU stall |
| `clflush_cache_range` | 刷 i-cache | clflush 刷的是数据缓存，碰不到取指；对 TCG TB 无意义 |

**共同教训**：任何“CPU-local 屏蔽中断”都治不了跨 CPU；任何“重新取指/重新执行”在 TCG 下都会死循环。唯一在内核侧可行的是**向前模拟，绕过那个陈旧字节**。

---

## 修复方案

### 治本（不改内核，推荐）

1. **改用 KVM**：VM XML `type='kvm'`（已验证不复现）。fuzzing/CI 若能用 KVM，问题直接消失。
2. 若**必须 TCG**：
   - `-accel tcg,thread=single`（关 MTTCG，串行化 vCPU，消除并行 TB 竞态）；
   - 或单 vCPU（`<vcpu>1</vcpu>`，消除跨 CPU）；
   - 或升级 QEMU（MTTCG XMC 相关 bug 上游修过多次）。
3. （可选）给 QEMU 报 TCG 跨修改代码 TB 失效的 bug。

### 内核兜底（TCG 专用 workaround）

分支 `ctkernel-lts-6.6/bugfix-text_poke-int3-fallback`：在 `exc_int3()` 的 `die("int3")` 之前调用 `poke_int3_emulate_stale()`：

```c
/* exc_int3() */
if (!do_int3(regs) && !poke_int3_emulate_stale(regs))
    die("int3", regs, 0);
```

`poke_int3_emulate_stale()` 逻辑：

1. 读崩溃地址内存首字节；若仍是 `0xCC` → 是真 INT3（kprobe/kgdb/BUG）→ 放行原路径。
2. 否则真实指令已在内存（协议最后才写首字节，首字节≠0xCC 即整条完整）→ `insn_decode_kernel` 解码，
   用与 `poke_int3_handler` 相同的 `CALL/JMP/Jcc/RET/NOP` 逻辑**向前模拟**（RIP 推进到指令之后或分支目标）。
3. 永不退回 RIP → 不会重新取那个陈旧字节 → **不死循环**；不关中断 → 无死锁；同/跨 CPU 通杀。

**定位**：这是给 QEMU TCG 模拟缺陷兜底的 workaround，**不建议进生产内核语义**（真机/ KVM 上这段恢复逻辑永不触发）。如保留，commit message 应标明 TCG 专用，并建议加 `pr_warn_ratelimited` 留痕以量化触发频率。

---

## 相关源码

| 文件 | 内容 |
|------|------|
| `arch/x86/kernel/alternative.c` | `__nonakr_text_poke()` / `poke_int3_handler()` / `poke_int3_emulate_stale()` / `text_poke_bp_batch()` / `text_poke_sync()` |
| `arch/x86/kernel/traps.c` | `exc_int3()` — INT3 IDT 入口 |
| `kernel/smp.c` | `smp_call_function_many_cond()` — `text_poke_sync` 的 IPI 等待 |
| `arch/x86/kernel/ftrace.c` | `ftrace_replace_code()` — 批量 text_poke 来源 |

---

## 原理详解（教学）

### Q: 啥是 opcode？

opcode（operation code，操作码）是机器指令的第一个（或多个）字节，告诉 CPU 这条指令要做什么。

| opcode | 指令 | 含义 |
|--------|------|------|
| `0xcc` | INT3 | 断点陷阱（单字节，可原子写入） |
| `0xe8` | CALL | 函数调用 |
| `0xe9` | JMP32 | 32 位偏移跳转 |
| `0xeb` | JMP8 | 8 位偏移跳转 |
| `0xc3` | RET | 返回 |
| `0x0f` | 两字节前缀 | 如 `0f 1f 44 00 00` = NOP5 |

### Q: 为什么还原指令要借助 INT3？

x86 无法原子写多字节指令。直接逐字节改 `e8..` → `0f 1f 44 00 00`，中途任何 CPU 取到半成品都会崩。INT3 是唯一单字节指令，可原子写入，于是：

```
Step 1: cc 11 22 33 44   ← 原子写 0xCC，命中即被 handler 劫持
Step 2: cc 1f 44 00 00   ← 安全改后 4 字节
Step 3: 0f 1f 44 00 00   ← 原子写回首字节，恢复完整 NOP
```

INT3 = 用异常机制实现的“字节级锁”，其他 CPU 走异常路径（模拟执行），直到还原完成。

### Q: ftrace 的 `e8 xx xx xx xx` 是函数本身吗？

不是。它是 ftrace 在函数入口预留的 5 字节插桩位：开启追踪时是 `CALL ftrace_trampoline`（`e8..`），关闭时是 `NOP5`（`0f 1f 44 00 00`）。trampoline 负责保存现场、遍历调用所有 ftrace 回调、恢复现场、跳回函数体。`text_poke_bp_batch` 就是在 CALL↔NOP 之间切换，过程中短暂留下 INT3。

### Q: poke_int3_handler 到底在干啥？

`bp_desc.vec` 指向一张 `text_poke_loc` 表（每条记录目标地址、原 opcode、disp、len）。命中 INT3 时：

```
1. ip = regs->ip - 1            → 回到 0xCC 的地址
2. 在 bp_desc.vec 里二分查找 ip
3. 据 tp->opcode 向前模拟：CALL→压返回地址+跳目标；JMP/Jcc→改 RIP；NOP→RIP 跳过；
4. return 1                     → exc_int3 不再 die，继续跑
```

注意：它**向前模拟、绝不重新执行那条 INT3**。这正是为什么修复也必须“向前模拟”而非“退回重试”——退回重试在 TCG 下会取到同一个陈旧 `0xcc` 而死循环。

### Q: 为什么执行 INT3 时不崩，最后却 die？

区别在 `poke_int3_handler` 能否**认领**这个 INT3：

- 能认领（地址在当前活跃 `bp_desc.vec` 里）→ 模拟 → `return 1` → 不崩。
- 不能认领（陈旧 INT3：地址不在当前 batch，或 `refs==0`）→ `return 0` → `do_int3` 无人认领 → `die("int3")`。

本 bug 属于后者：TCG 让 CPU 取到一个**内存里已不存在**的陈旧 INT3，handler 自然认领不了。

### Q: 为什么物理机/KVM 不复现，只有 TCG 复现？

`poke_int3_handler` 的正确性依赖“每个 CPU 改代码后、取指前会被 `sync_core` 强制重新取指”这条硬件契约。

- 物理机 / KVM：真实 CPU 忠实兑现 → batch 完成后无陈旧 INT3，窗口为零。
- QEMU TCG：把 guest 代码翻译成 TB 缓存，跨 CPU 改代码的 TB 失效时序没建模成 `sync_core` 屏障 → 仍执行旧 TB（带 INT3）→ 窗口被打开。

所以这不是“物理机也存在、只是极罕见”的竞态——物理机上**根本没有**这个窗口，它是 TCG 模拟造出来的。
