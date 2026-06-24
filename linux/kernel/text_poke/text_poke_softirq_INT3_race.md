# text_poke_bp_batch INT3 竞态触发未处理崩溃分析

> 内核源码 commit：`d4c6c1d302656`

## 基本信息

| 项目 | 内容 |
|------|------|
| Panic 类型 | `int3: 0000` — 未处理的 INT3 断点陷阱 |
| 硬件 | Red Hat KVM, QEMU 虚拟化 |
| 崩溃场景 | 见下方三次崩溃记录 |

## 三次崩溃记录

| 日志 | 内核版本 | 崩溃 CPU | 崩溃函数 | 竞态类型 | 已应用修复 |
|------|----------|----------|----------|----------|------------|
| vmcore-dmesg.txt | 6.6.0-0010.rc2.ctl4.x86_64 | CPU 1（text_poke CPU） | `update_rt_rq_load_avg+0x1` | 同 CPU 软中断 | 无 |
| dmesg.txt | 6.6.0-0010.rc2.text_poke.ctl4.x86_64 | CPU 0（非 text_poke CPU） | `exclusived_task_running+0x1` | 跨 CPU 硬中断返回路径 | `local_bh_disable` |
| dmesg1.txt | 6.6.0-0010.rc2.text_poke2.ctl4.x86_64 | CPU 0 + CPU 1 同时 | `up_read+0x1` | 跨 CPU 缺页路径 | `local_irq_save` |

## 复现命令

```bash
./syz-execprog -arch=amd64 -executor=./syz-executor -repeat=1 -procs=1 -sandbox=none -output ./log_6
```

## 关键日志（vmcore-dmesg.txt，原始崩溃）

```
[ 2102.293808] int3: 0000 [#1] SMP NOPTI
[ 2102.294141] RIP: 0010:update_rt_rq_load_avg+0x1/0x340
[ 2102.294141] Call Trace:
[ 2102.294141]  <IRQ>
[ 2102.294141]  update_blocked_averages+0xb7/0xe90
[ 2102.294141]  run_rebalance_domains+0x45/0x60
[ 2102.294141]  handle_softirqs+0xe2/0x2a0
[ 2102.294141]  irq_exit_rcu+0x9c/0xc0
[ 2102.294141]  sysvec_apic_timer_interrupt+0x6e/0x90
[ 2102.294141]  </IRQ>
[ 2102.294141]  <TASK>
[ 2102.294141]  asm_sysvec_apic_timer_interrupt+0x21/0x40
[ 2102.294141]  __nonakr_text_poke+0x2c1/0x4e0
[ 2102.294141]  text_poke_bp_batch+0x21a/0x340
[ 2102.294141]  text_poke_queue+0x68/0xa0
[ 2102.294141]  ftrace_replace_code+0x13f/0x190
[ 2102.294141]  ftrace_shutdown.part.0+0x105/0x1e0
[ 2102.294141]  unregister_ftrace_function+0x45/0x150
[ 2102.294141]  perf_ftrace_event_register+0x90/0xc0
[ 2102.294141]  perf_trace_destroy+0x2c/0x70
[ 2102.294141]  _free_event+0x107/0x410
[ 2102.294141]  perf_event_release_kernel+0x298/0x2f0
[ 2102.294141]  do_exit+0x362/0xad0
[ 2102.294141]  </TASK>
```

## 根因分析

### 触发链：ftrace 关闭触发批量 text_poke

syz-executor 注册了绑定 ftrace 的 perf 事件，进程退出时触发以下释放链：

```
do_exit
  → perf_event_release_kernel
    → _free_event
      → perf_trace_destroy
        → unregister_ftrace_function
          → ftrace_shutdown
            → ftrace_replace_code      ← 遍历所有 ftrace 注册点批量取消 patch
              → text_poke_queue        ← 积攒 patch 条目到 tp_vec[]
                → text_poke_bp_batch   ← 每 256 条触发一次，执行 INT3 三步协议
```

### text_poke_bp_batch 三步 INT3 协议

```
Step 1: 把所有 patch 地址的第 0 字节写成 INT3 (0xCC)
        text_poke_sync() → on_each_cpu(do_sync_core) → IPI 所有 CPU 刷 I-cache

Step 2: 写新指令的第 1..N-1 字节（addr[0] 仍是 INT3）
        text_poke_sync()

Step 3: 把 addr[0] 从 INT3 替换为新指令的真实第一字节
        text_poke_sync()

atomic_dec_and_test(&bp_desc.refs)   ← INT3 窗口正式关闭
```

在 Step 1 至 Step 3 完成之前，所有被 patch 的函数入口都是 INT3。设计上由 `poke_int3_handler` 在其他 CPU 命中 INT3 时做指令模拟。

### INT3 异常分发链路

```
exc_int3()                                      traps.c:792
  ├── poke_int3_handler()                       alternative.c:2276
  │     ├── try_get_desc()                      ← raw_atomic_inc_not_zero(&bp_desc.refs)
  │     │     refs==0 → return NULL → return 0  ← 失败路径 A
  │     ├── bsearch(ip, bp_desc.vec, ...)       ← 二分查找当前 patch 条目
  │     │     找不到   → goto out_put → return 0 ← 失败路径 B
  │     └── 按 tp->opcode 模拟执行
  │           JMP32+0 → int3_emulate_jmp()      ← NOP5 的模拟方式
  │           CALL    → int3_emulate_call()
  │           RET     → int3_emulate_ret()
  ├── do_int3()                                 ← poke_int3_handler 返回 0 时
  │     ├── kprobe_int3_handler()
  │     ├── klp_int3_handler()
  │     └── notify_die(DIE_INT3)
  └── die("int3")                               ← 全部未处理 → 内核崩溃
```

---

## 崩溃场景一：同 CPU 软中断竞态（vmcore-dmesg.txt）

### 竞态根因

CTK 内核的 `__nonakr_text_poke`（alternative.c:1855）内部为保护 `poking_mm` 切换，
自行做了 `local_irq_save(flags)` / `local_irq_restore(flags)`（line 1903 / 1956）：

```c
local_irq_save(flags);        // IRQ 关
use_temporary_mm(poking_mm);
func(dst, src, len);          // 写字节
unuse_temporary_mm(prev);
flush_tlb_mm_range(...);
local_irq_restore(flags);     // ⚠ IRQ 重新开启 — 竞态窗口在此打开
pte_unmap_unlock(ptep, ptl);  // 崩溃时 CPU 卡在这条指令 (+0x2c1)
```

`local_irq_restore` 重开中断后，**被推迟的定时器中断立刻触发**，由于进程上下文未禁
软中断，`irq_exit_rcu` 直接调用 `handle_softirqs`：

```
sysvec_apic_timer_interrupt（CPU 1，text_poke CPU）
  → irq_exit_rcu()
    → handle_softirqs()          ← 无 local_bh_disable，软中断直接运行
      → SCHED_SOFTIRQ
        → run_rebalance_domains
          → update_rt_rq_load_avg  ← 入口是 INT3（Step 1 已写入）
```

CPU 1 此时既在跑 `text_poke_bp_batch`（TASK 上下文），又被软中断打断进入
`update_rt_rq_load_avg`。`poke_int3_handler` 在自身 CPU 上因批次边界或
`bp_desc.refs` 状态不匹配而返回 0，触发 `die("int3")`。

### 已应用修复：`local_bh_disable`

在 `smp_wmb()` 之后、Step 1 之前加 `local_bh_disable()`，在 Step 3 的
`text_poke_sync()` 之后加 `local_bh_enable()`，阻止软中断在 INT3 窗口内运行。

**效果**：修复了同 CPU 软中断竞态（vmcore-dmesg.txt 不再复现）。  
**不足**：仅保护 text_poke CPU，跨 CPU 崩溃依然存在。

---

## 崩溃场景二：跨 CPU 竞态（dmesg.txt / dmesg1.txt）

### dmesg.txt（`local_bh_disable` 修复后仍崩溃）

```
CPU 0（非 text_poke CPU，运行 sed）：
  sysvec_apic_timer_interrupt
    → irqentry_exit_to_user_mode  ← 从定时器中断返回用户态
      → TIF_NEED_RESCHED → schedule()
        → __schedule()
          → exclusived_task_running  ← 入口是 INT3
            INT3 → poke_int3_handler → 返回 0 → die("int3")
```

### dmesg1.txt（`local_irq_save` 修复后仍崩溃，两个 CPU 同时崩溃）

```
CPU 1（ldd，exc_page_fault 路径）：
  exc_page_fault
    → ... → up_read   ← 入口是 INT3
      INT3 → poke_int3_handler → 返回 0 → die("int3") [#1]

CPU 0（ldd，同路径）：
  exc_page_fault
    → ... → up_read   ← 入口是 INT3
      INT3 → poke_int3_handler → 返回 0 → die("int3") [#2]
```

### 跨 CPU 竞态的本质

所有 `local_bh_disable`、`local_irq_disable`、`local_irq_save/restore` 均是
**CPU-local 操作**，对其他 CPU 的执行流无任何约束。其他 CPU 在 INT3 窗口内照常
调用被 patch 的函数，命中 INT3 后 `poke_int3_handler` 若返回 0 即崩溃。

`poke_int3_handler` 在跨 CPU 场景下返回 0，只有两条路径：

| 失败路径 | 原因 |
|----------|------|
| `try_get_desc()` 返回 NULL | `bp_desc.refs == 0`，batch 已完成但 I-cache 仍有旧 INT3 |
| 二分查找失败 | 命中地址不在当前 `bp_desc.vec` 中 |

### 已尝试的修复对比

| 修复方式 | 对同 CPU 的效果 | 对跨 CPU 的效果 |
|----------|----------------|----------------|
| `local_bh_disable/enable` | ✓ 阻止软中断 | ✗ 无效 |
| `local_irq_disable/enable` | ✓ 阻止硬+软中断 | ✗ 无效 |
| `local_irq_save/restore`（当前 HEAD） | ✓ 阻止硬+软中断，且嵌套安全 | ✗ 无效 |

**关键区别**：`local_irq_save` 比 `local_bh_disable` 更强——有了外层
`local_irq_save(flags)` 后，`__nonakr_text_poke` 内层的 `local_irq_restore(flags_inner)`
恢复的是 IF=0（因为 flags_inner 保存时 IRQ 已关），整个 Step 1→Step 3 窗口
text_poke CPU 硬中断**始终关闭**，消除了所有同 CPU 竞态。但跨 CPU 崩溃不受影响。

---

## 当前状态与待解问题

当前 HEAD（`local_irq_save/restore`）已完全修复同 CPU 竞态，但跨 CPU 的
`poke_int3_handler` 返回 0 问题**仍未解决**。

需要确认 `poke_int3_handler` 具体走的是哪条失败路径，可在 `exc_int3` 的
`die("int3")` 前加临时 debug 打印：

```c
/* arch/x86/kernel/traps.c，die("int3") 前 */
pr_warn_once("int3 unhandled: ip=%pS refs=%d nr=%d\n",
             (void *)regs->ip - 1,
             atomic_read(&bp_desc.refs),
             bp_desc.nr_entries);
```

- **refs=0**：batch 已完成，某 CPU 的 I-cache 刷新滞后（`text_poke_sync()` 窗口问题）
- **refs≠0，地址不在 vec**：batch 分配或排序问题

---

## 相关源码文件

| 文件 | 行号 | 内容 |
|------|------|------|
| `arch/x86/kernel/alternative.c` | 1855–1959 | `__nonakr_text_poke()` — 实际内存写入，含 `local_irq_save/restore` |
| `arch/x86/kernel/alternative.c` | 2079–2091 | `__text_poke()` — 路由到 NAKR/非 NAKR 实现 |
| `arch/x86/kernel/alternative.c` | 2233–2237 | `struct bp_patching_desc` — INT3 批处理描述符 |
| `arch/x86/kernel/alternative.c` | 2241–2250 | `try_get_desc()` — 原子获取批次描述符 |
| `arch/x86/kernel/alternative.c` | 2276–2356 | `poke_int3_handler()` — INT3 异常模拟处理器 |
| `arch/x86/kernel/alternative.c` | 2383–2525 | `text_poke_bp_batch()` — INT3 三步批量补丁 |
| `arch/x86/kernel/alternative.c` | 2527–2600 | `text_poke_loc_init()` — NOP5 编码为 JMP32+0 |
| `arch/x86/kernel/alternative.c` | 2606–2644 | `tp_order_fail()` / `text_poke_queue()` — 有序批量队列 |
| `arch/x86/kernel/traps.c` | 792–824 | `exc_int3()` — INT3 IDT 入口 |
| `arch/x86/kernel/ftrace.c` | 241–297 | `ftrace_replace_code()` — ftrace 批量代码替换 |
