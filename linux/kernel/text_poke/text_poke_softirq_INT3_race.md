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

`__nonakr_text_poke`（即上游 `__text_poke`）内部为保护 `poking_mm` 切换，
做了 `local_irq_save(flags)` / `local_irq_restore(flags)`（line 1903 / 1956）：

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

---

## 原理详解

### Q: 啥是 opcode？

opcode 是 **operation code** 的缩写，中文叫**操作码**。它是机器指令的第一个字节（或多个字节），告诉 CPU 这条指令要做什么。

以 ftrace NOP 为例：

```
0f 1f 44 00 00    ← 5 字节的 NOP 指令
↑↑
opcode (0x0f)      ← CPU 看到 0x0f 就知道这是条多字节 NOP
```

常见的 opcode：

| opcode | 指令 | 含义 |
|--------|------|------|
| `0xcc` | INT3 | 断点陷阱 |
| `0xe8` | CALL | 函数调用 |
| `0xe9` | JMP32 | 32位偏移跳转 |
| `0xeb` | JMP8 | 8位偏移跳转 |
| `0xc3` | RET | 返回 |
| `0x0f` | 多字节前缀 | 配合后续字节构成 NOP 等 |

`text_poke_bp_batch` 做的事情就是：把函数入口第一个字节（opcode）临时换成 `0xcc`，改完后面的字节后，再把 `0xcc` 换回真实的 opcode（比如 `0x0f`）。**Step 3 就是"把 INT3 的 opcode 还原成真实 opcode"这一步。**

### Q: INT3 可以截获任意函数？

是的，`text_poke_bp_batch` 的工作机制就是把**任意内核函数入口的第一个字节改成 0xCC (INT3)**。CPU 执行到这条指令时触发 INT3 异常 → `exc_int3()` → `poke_int3_handler()` 拦截，查 `bp_desc` 找到对应的 `text_poke_loc`，根据 `tp->opcode` 知道原本是什么指令并模拟执行。

能截获是因为 x86 的 INT3 (0xCC) 是 1 字节指令，可以**原子写入**任意位置。把任何一个函数的第一个字节换成 0xCC，该函数就变成了"陷阱"——CPU 执行到这里必定触发 INT3。

这就是为什么问题 3（远程 CPU 陈旧 INT3）特别危险：这些 INT3 不是"正在打补丁需要模拟"的阶段，而是"补丁已完成，INT3 本该已消失，但远程 CPU 的 i-cache 里还有残留"的阶段。此时 `bp_desc` 没了、补丁信息没了，`poke_int3_handler` 不知道这曾经是什么指令，就崩了。

### Q: ftrace 的 e8 xx xx xx xx 对应的函数就是 update_rt_rq_load_avg 吗？

不对。`e8 xx xx xx xx` **不是** `update_rt_rq_load_avg` 函数本身的代码，它是 **ftrace 在函数入口处打的桩（hook）**。

`update_rt_rq_load_avg` 真正的函数体在 5 字节之后才开始。ftrace 的工作原理是：

```
update_rt_rq_load_avg:
    e8 xx xx xx xx          ← ftrace CALL，跳到 ftrace trampoline
    ... 真正的函数代码 ...
```

没有 ftrace 时（NOP 状态）：

```
update_rt_rq_load_avg:
    0f 1f 44 00 00          ← 5 字节 NOP，啥也不干
    ... 真正的函数代码 ...
```

所以这三轮 crash 中被改的函数和 ftrace 的关系是：

| crash | 崩溃函数 | 当前状态 |
|-------|---------|---------|
| 1 | `update_rt_rq_load_avg+0x1` | CALL→NOP（ftrace 关闭） |
| 2 | `exclusived_task_running+0x1` | CALL→NOP（ftrace 关闭） |
| 3 | `up_read+0x1` | CALL→NOP（ftrace 关闭） |

每个被 ftrace 跟踪的内核函数入口都嵌着一条 `e8 xx xx xx xx`（CALL ftrace_trampoline），`text_poke_bp_batch` 把这些字节改回 `0f 1f 44 00 00`（NOP），过程中短暂留下了 `cc 1f 44 00 00`（INT3），如果此时有 CPU 执行到这个函数就崩了。

**什么是 CALL ftrace_trampoline？**

ftrace 是 Linux 内核的函数追踪框架。它用一个"插桩"机制：编译时在内核函数入口预留 5 字节 NOP 空间，开启追踪时把 NOP 替换成一条 CALL 指令，CALL 的目标就是 `ftrace_trampoline`：

```
update_rt_rq_load_avg:
    e8 xx xx xx xx          ← CALL ftrace_trampoline
    ... 真正的函数代码 ...
```

`ftrace_trampoline` 是一个特殊的汇编入口，它做的事就是：
1. 保存调用者的寄存器现场
2. 遍历所有注册的 ftrace 回调函数（比如 function tracer、function graph tracer、perf ftrace 等），逐个调用
3. 恢复寄存器现场
4. 跳回 `update_rt_rq_load_avg` 的开头，执行真正的函数体

所以"CALL ftrace_trampoline"本质上是一个**函数调用拦截器**——每次调用 `update_rt_rq_load_avg` 时，先跳到 trampoline 让所有追踪工具"看一眼"，然后再执行真正的函数逻辑。

当 ftrace 关闭时，`text_poke_bp_batch` 把这条 CALL 改回 NOP（`0f 1f 44 00 00`），函数就恢复原样——调用直接进入函数体，不再经过 trampoline。

### Q: 还原指令为啥要借助 INT3？

因为 x86 上**无法原子地写入多字节指令**。

举例：你想把 `e8 11 22 33 44`（5字节 CALL）换成 `0f 1f 44 00 00`（5字节 NOP）。如果直接逐字节写入：

```
写字节0: 0f 11 22 33 44 → CPU 读到 0f 11...？这是啥？崩
写字节1: 0f 1f 22 33 44 → CPU 读到 0f 1f 22...？还是垃圾，崩
写字节2: 0f 1f 44 33 44 → ...
写字节3: 0f 1f 44 00 44 → ...
写字节4: 0f 1f 44 00 00 → 终于对了
```

写入过程中，任何 CPU 来执行这段代码都会看到半成品，把几个字节拼出完全不存在的指令，直接崩。

**INT3 (0xCC) 是 x86 里唯一的单字节指令**，可以原子写入。所以协议是：

```
Step 1: cc 11 22 33 44  ← 原子写入 0xCC，CPU 执行到这里 → INT3 → 被 handler 劫持
Step 2: cc 1f 44 00 00  ← 安全修改后面 4 字节，没人会执行到这里
Step 3: 0f 1f 44 00 00  ← 原子写回 0x0f，恢复成完整 NOP
```

INT3 本质上是**一个用异常机制实现的"字节级锁"**——0xCC 占住第一个字节，其他 CPU 都走异常路径（模拟执行），直到最后一个字节还原完成。

### Q: 查 bp_desc 找到对应的 text_poke_loc，根据 tp->opcode 知道原本是什么指令并模拟执行——这里具体一点，到底在干啥？大白话一下

**前置补丁信息怎么存的**

ftrace 要把函数 `update_rt_rq_load_avg` 入口的 `e8 xx xx xx xx`（5字节 CALL）改回 `0f 1f 44 00 00`（5字节 NOP）。它在 `text_poke_loc_init()` 里填了一张表：

```c
struct text_poke_loc {
    s32 rel_addr;    // 函数入口地址相对于 _stext
    s32 disp;        // CALL 的跳转偏移量
    u8  len;         // 要修改 5 个字节
    u8  opcode;      // 原本是什么指令（CALL = 0xe8）
    u8  text[5];     // 目标指令字节（NOP：0f 1f 44 00 00）
    u8  old;         // 旧的第一个字节（保存用）
};
```

这张表的条目存在 `tp_vec[]` 数组里，被 `bp_desc.vec` 指向。

**Step 1：放陷阱**

```
text_poke_bp_batch 逐个把每个函数的第一个字节改成 0xCC
    内存变成: cc xx xx xx xx  （只有第一个字节换了）
```

CPU 执行时 INT3 异常 → 进 `exc_int3()`。

**异常处理：假装没有 INT3**

```c
poke_int3_handler:
1. regs->ip 存的是 INT3 的下一条地址（0xCC 后面的地址）
2. ip = regs->ip - 1   → 回到 0xCC 的真正地址
3. 去 bp_desc.vec 里二分查找，看这个地址是不是我们在改的函数之一
4. 找到对应条目后，tp->opcode = 0xe8 → 说明"原本是条 CALL 指令"
5. ip += tp->len → 跳到 CALL 指令后面（这才是该继续执行的位置）
6. int3_emulate_call(regs, ip + tp->disp) → 模拟 CALL 的效果：
     - 把返回地址（ip）压栈
     - 把 regs->ip 设为 ip + disp → CPU 假装从 CALL 的目标地址继续跑
```

**说白了**：INT3 占了一个字节，真正想执行的是 CALL 或 NOP 是 5 字节。`poke_int3_handler` 做的事就是——**别当真执行 INT3，就当刚刚执行了一条正常的 CALL/JMP/NOP，把 CPU 寄存器（IP、栈）改到正确状态，然后让 CPU 从这个状态继续跑**。

**为什么崩溃**：如果 `bp_desc.refs == 0`（补丁已完成），找不到 `text_poke_loc` 条目了，就不知道这以前是什么指令，模拟不了，CPU 就真当 INT3 处理 → `die("int3")`。

### Q: 这个 bug 的根因其实是函数未替换完，还是 INT3 误以为替换完了，把 INT3 当正常指令执行了？但是为啥会 die 呢？

根因是第二种情况——**INT3 实际已经被替换完了（内存里写回了真实 opcode），但远程 CPU 从自己的 i-cache 读出来还是 0xCC**。

> **注**：这个结论是基于 crash 日志特征的逻辑推论，并非通过 dump 物理内存直接测量。以下是推导过程。

**证据链**：

1. **dmesg1.txt 中 crash 的进程是 `ldd`**，它不是跑 `text_poke_bp_batch` 的那个进程。text_poke CPU 是另一个 CPU（当前 batch 还在干活），`ldd` 跑在远程 CPU 上。

2. **调用链是 `exc_page_fault → up_read`**，跟软中断、irq→schedule 都没关系，就是一个普通缺页异常路径。

3. **既然崩了（`die("int3")`），说明 `poke_int3_handler` 返回了 0**。返回 0 只有两种可能：
   - `try_get_desc()` → NULL（`bp_desc.refs == 0`）
   - 地址不在当前 `bp_desc.vec` 中

4. **两个不同 CPU 在 7ms 内同时崩在同一个函数 `up_read+0x1`**。如果是地址不在 vec 中（第二种情况），那说明当前活跃 batch 的 vec 不包括 `up_read`——但当前 batch 根本没在改 `up_read`，那 `up_read` 入口的 INT3 只能是**上一个已完成 batch 留下的陈旧 INT3**。

5. **上一个 batch 已经完成**，意味着 Step 3 已经执行了——`0xcc` 已经被 `text_poke()` 写回成真实 opcode（比如 `0x0f`）。内存里已经是正确的指令了。

6. **但 CPU 还是读到了 `0xcc`**。为什么？因为 `text_poke_sync()` 调的是 `sync_core()`，它只做指令流水线序列化（`CPUID` 或类似指令），强行让 CPU 重新从 L1 i-cache 取指——但**它不刷 i-cache**。如果 i-cache 里还有上一批的 `0xcc`，`sync_core()` 拿到的还是 `0xcc`。

**为什么内存对了，CPU 却读错？**

```
Step 3 执行完后：
  物理内存: 0f 1f 44 00 00   ← 正确的 NOP

CPU 0 的 L1 i-cache: 0f 1f 44 00 00   ← 本地 CPU，已刷新 ✓
CPU 1 的 L1 i-cache: cc 1f 44 00 00   ← 远程 CPU，还残留在状态 ✗
```

`text_poke_sync()` 调用的是 `sync_core()`——它只序列化指令流水线（让 CPU 重新从缓存取指），**不刷 i-cache**。i-cache 硬件一致性最终会让 0xCC 失效，但有个延迟窗口。VM 中这个窗口被放大到毫秒级。

**为什么会 die？**

```
CPU 1 的 ldd 进程缺页异常 → exc_page_fault → up_read()

CPU 1 从 i-cache 取指: cc → INT3 异常！

exc_int3()
  └─ poke_int3_handler()
       └─ try_get_desc() = NULL  ← bp_desc.refs 已经是 0，batch 早完成了
       └─ return 0              ← "我处理不了"

do_int3()
  └─ kprobe_int3_handler() → 不匹配
  └─ notify_die(DIE_INT3)  → 没人认领
  └─ die("int3")            ← 内核死给你看
```

**所以本质是：INT3 已经还原了，但远程 CPU 以为自己看到了 INT3，去问 handler "这是咋回事"，handler 说 "我早下班了这活不是我发的"，于是内核 panic。**

### Q: 三次 crash 是不是同一个问题？怎么判断的？

核心判断依据是 **异常类型和崩溃模式高度一致**：

1. **相同的异常类型**：三次都是 `int3: 0000` — 未处理的 INT3 异常，不是空指针、page fault 等其他异常
2. **相同的崩溃位置特征**：都是 `func+0x1` — INT3 恰好打在 ftrace NOP 占位处（函数入口第一个字节），典型的 text_poke_bp_batch 正在修改该函数
3. **补丁策略的渐进验证**：第一次 `local_bh_disable()` 修复后，原崩溃路径（SCHED_SOFTIRQ → `update_rt_rq_load_avg`）消失，但冒出新路径（irq 返回 → `schedule()` → `exclusived_task_running`）。新路径恰好落在 `local_bh_disable()` 没覆盖到的范围（硬中断返回后的进程调度），说明是**同一竞态窗口的不同入口**，而非新 bug

```
第一次 crash:  timer IRQ → softirq(SCHED_SOFTIRQ) → 命中 INT3 ❌
                ↓ local_bh_disable() 修复 ✓

第二次 crash:  timer IRQ → irq_exit → schedule() → 命中 INT3 ❌
                ↓ 绕过了 local_bh_disable 保护
                ↓ 用 local_irq_disable 覆盖 ✓

第三次 crash:  远程 CPU 缺页异常 → up_read → 命中陈旧 INT3 ❌
                ↓ local_irq_disable 只保护本地 CPU，管不到远程
                ↓ 需要 poke_int3_handler fallback + clflush_cache_range
```

### Q: 第三次 crash 跟前两次的本质区别

前两次 crasher 是 `poke_int3_handler` **正常工作**（bp_desc 活跃，能找到 text_poke_loc）——handler 找到了条目，在模拟执行 CALL→NOP 转换时出了差错。第三次 crash 是 `poke_int3_handler` **已经下班了**（bp_desc.refs==0，batch 早就完成了），handler 直接返回 0，走 `do_int3()` → `die("int3")`。

| 次数 | 崩溃函数 | 上下文 | 发生在哪个 CPU | bp_desc.refs | 防住? |
|------|----------|--------|---------------|-------------|-------|
| 1 | `update_rt_rq_load_avg` | 软中断 | 本地 CPU | 活跃 | `local_bh_disable` ✓ |
| 2 | `exclusived_task_running` | irq→schedule | 本地 CPU | 活跃 | `local_irq_disable` ✓ |
| 3 | `up_read` | 缺页异常 | **远程 CPU** | **0（已下班）** | `local_irq_disable` ✗ |

### Q: local_irq_disable 能覆盖 local_bh_disable 吗？

能。`local_irq_disable()` 是 `local_bh_disable()` 的严格超集：

| 阻断能力 | `local_bh_disable` | `local_irq_disable` |
|---------|:---:|:---:|
| 软中断 (softirq) | ✓ | ✓ |
| 硬中断 (hardirq) | ✗ | ✓ |
| irq_exit → schedule() | ✗ | ✓ |
| irq_exit → 软中断 | ✗ | ✓ |
| 抢占调度 | ✗ | ✓ |

`local_irq_disable` 关闭本地 CPU 的中断响应，中断不来 → 不会有 irq_exit → 不会有软中断、不会有 schedule()、不会有任何上下文切换。所以它**完全覆盖**了 `local_bh_disable` 的保护范围。

### Q: 这个问题在 VM 中很容易复现但物理机上不复现？

主要原因是 **VM 退出 (VM exit) 导致的定时器中断堆积**。

`text_poke_sync()` 内部调用 `on_each_cpu(do_sync_core, NULL, 1)` 向其他 CPU 发 IPI 并等待同步完成。在物理机上这很快（几百个 CPU 周期），但在 KVM 虚拟机中：

1. **IPI 触发 VM exit**：向其他 vCPU 发 IPI 时，vCPU 需要 VM exit 到 Host 的 KVM 处理 IPI，期间当前 vCPU 也可能被 Host 调度出去
2. **VM exit 期间定时器中断堆积**：vCPU 在 VM exit 期间不响应中断，Host 上积累的定时器中断在 vCPU 重新进入 Guest 时**集中投递**
3. **中断集中投递 + 软中断挂起**：定时器中断退出时 `irq_exit_rcu()` 发现挂起的 `SCHED_SOFTIRQ`，立即调度 `run_rebalance_domains`，此时 `update_rt_rq_load_avg` 入口的 INT3 尚未被替换

```
物理机:  INT3写入 → [几十ns] → text_poke_sync完成 → Step3替换
VM:       INT3写入 → text_poke_sync → VM exit(Host调度) → [数百us~ms]
                                                ↓
                                   Guest重入 → 堆积的定时器中断
                                                ↓
                                   irq_exit → SCHED_SOFTIRQ → 命中INT3!
```

物理机上 `text_poke_sync()` 几乎瞬时完成，定时器中断很难恰好落在 INT3 窗口内，所以不复现。

### Q: 为啥用 local_irq_disable 而非 local_bh_disable？

`local_bh_disable()` 只阻止软中断执行。但 `irqentry_exit_to_user_mode` → `schedule()` 是硬中断退出路径中的进程调度，不在软中断范围内。任何用户态进程（如 sed）在 text_poke INT3 窗口期内被定时器打断，调度回来时都可能命中 INT3。

`local_irq_disable` 关闭本地 CPU 中断响应，中断不来 → 不会有 irq_exit → 不会有软中断、不会有 schedule()、不会有任何上下文切换。但只保护本地 CPU，管不到远程 CPU。

### Q: 最终修复方案

三层防护：

1. **`local_irq_save/restore`** — 阻断本地 CPU 中断、软中断、irq→schedule 路径（问题 1、2）
2. **`poke_int3_handler` fallback** — 远程 CPU 命中陈旧 INT3 时，无论 `refs==0` 还是地址不在当前 batch，都安全跳过（问题 3）
3. **`clflush_cache_range`** — Step 3 后刷新所有被修改地址的 i-cache，从根源清除远程 CPU 的陈旧 INT3（问题 3 根治）

### Q: 为啥执行 INT3 替换指令的时候不崩溃，最后 die("int3")?

关键区别在于 **`bp_desc.refs` 的值**。

**正常情况（不崩溃）：`refs != 0`**

```
text_poke_bp_batch() Step 1 之前:
  atomic_set_release(&bp_desc.refs, 1);    ← refs = 1

CPU 1 执行到 INT3:
  exc_int3()
    → poke_int3_handler()
        → try_get_desc() = &bp_desc        ← refs > 0，获取成功
        → 二分查找 → 找到 text_poke_loc
        → tp->opcode = JMP32_INSN_OPCODE   ← "这条指令要模拟成 JMP32+0"
        → int3_emulate_jmp(regs, ip + 0)   ← 模拟执行 NOP，跳过
        → return 1                         ← "处理完了，没事"
```

`poke_int3_handler` 成功处理后返回 1，`exc_int3()` 看到返回值非 0，就**不调用 `do_int3()` 和 `die()`**，而是返回被中断的代码继续跑。

**崩溃情况：`refs == 0`**

```
text_poke_bp_batch() Step 3 之后:
  atomic_dec_and_test(&bp_desc.refs)       ← refs → 0
  // batch 完成了，bp_desc 释放了

CPU 1 执行到 INT3（i-cache 残留）:
  exc_int3()
    → poke_int3_handler()
        → try_get_desc() = NULL            ← refs == 0，拿不到描述符
        → return 0                         ← "处理不了，不知道这是什么"

    → do_int3()                             ← handler 说处理不了
        → kprobe_int3_handler() → 不匹配
        → notify_die(DIE_INT3) → 没人认领
        → die("int3")                       ← 崩
```

**决定性的区别**：`try_get_desc()` 的返回值。`refs > 0` 时 handler 知道"有人正在 patch，这个 INT3 是合法的，我来模拟"。`refs == 0` 时 handler 不知道这是啥，返回 0，`do_int3()` 把检查链走完没人认领 → `die("int3")`。

**所以不是"执行 INT3 不崩"**，而是**执行 INT3 时 handler 在岗，把 INT3 瞒过去了**。handler 下班后，残留在 i-cache 里的 INT3 就变成了无人认领的 trap，内核直接 panic。

### Q: 这个问题上游有修复吗？

上游没有。这个 INT3 竞态是 CTK 内核 + KVM 虚拟化环境下的特定问题，上游主线未必会触发（物理机上 `text_poke_sync()` 足够快）。但对于虚拟化和 syzkaller fuzzing 场景，这是一个真实的缺陷。
