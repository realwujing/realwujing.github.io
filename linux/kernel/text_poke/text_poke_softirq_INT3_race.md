# text_poke_bp_batch 软中断竞态触发 INT3 未处理崩溃分析

## 基本信息

| 项目 | 内容 |
|------|------|
| Panic 类型 | `int3: 0000` — 未处理的 INT3 断点陷阱 |
| 内核版本 | 6.6.0-0010.rc2.ctl4.x86_64 |
| 崩溃地址 | `update_rt_rq_load_avg+0x1` |
| CPU | CPU 1, PID 2766 (syz-executor) |
| 硬件 | Red Hat KVM, QEMU 虚拟化 |
| 触发上下文 | 定时器硬中断 → 退出时触发软中断 → SCHED_SOFTIRQ → 调度器 PELT 更新 |

## 复现命令

```bash
./syz-execprog -arch=amd64 -executor=./syz-executor -repeat=1 -procs=1 -sandbox=none -output ./log_6
```

## 关键日志

```
[ 2102.293808] int3: 0000 [#1] SMP NOPTI
[ 2102.294141] RIP: 0010:update_rt_rq_load_avg+0x1/0x340
[ 2102.294141] RSP: 0018:ff580133800e0ef0
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
[ 2102.294141] RIP: 0010:__nonakr_text_poke+0x2c1/0x4e0
[ 2102.294141]  text_poke_bp_batch+0x21a/0x340
[ 2102.294141]  text_poke_queue+0x68/0xa0
[ 2102.294141]  ftrace_replace_code+0x13f/0x190
[ 2102.294141]  ftrace_modify_all_code+0x124/0x140
[ 2102.294141]  ftrace_shutdown.part.0+0x105/0x1e0
[ 2102.294141]  unregister_ftrace_function+0x45/0x150
[ 2102.294141]  perf_ftrace_event_register+0x90/0xc0
[ 2102.294141]  perf_trace_destroy+0x2c/0x70
[ 2102.294141]  _free_event+0x107/0x410
[ 2102.294141]  perf_event_release_kernel+0x298/0x2f0
[ 2102.294141]  perf_release+0x11/0x20
[ 2102.294141]  __fput+0xeb/0x2a0
[ 2102.294141]  task_work_run+0x5a/0x90
[ 2102.294141]  do_exit+0x362/0xad0
[ 2102.294141]  do_group_exit+0x2c/0x80
[ 2102.294141]  get_signal+0x923/0x930
[ 2102.294141]  arch_do_signal_or_restart+0x2e/0x230
[ 2102.294141]  syscall_exit_to_user_mode+0x205/0x220
[ 2102.294141]  do_syscall_64+0x79/0x190
[ 2102.294141]  entry_SYSCALL_64_after_hwframe+0x76/0x7e
[ 2102.294141]  </TASK>
```

### Code 指令

```
update_rt_rq_load_avg 入口:
     0f 1f 44 00 00    → nopl (%rax,%rax)  (ftrace NOP)
                        → Step 1 写入 INT3 后变为: cc 1f 44 00 00

__nonakr_text_poke+0x2c1:
     c6 07 00          → movb $0x0, (%rdi)  (正在向目标地址写入 0x00)
```

### 关键寄存器

| 寄存器 | 值 | 含义 |
|--------|-----|------|
| RIP (IRQ上下文) | ffffffff83ca4341 | `update_rt_rq_load_avg+0x1`，命中 INT3 后 CPU 保存的下一条指令地址 |
| RIP (进程上下文) | ffffffff8258f131 | `__nonakr_text_poke+0x2c1`，movb $0x0, (%rdi) |
| RDI (进程上下文) | fff0e235800418a8 | 正在被 text_poke 修改的目标地址 |
| RBP (进程上下文) | ffffffff83ca4340 | `update_rt_rq_load_avg` 函数入口地址 |

## 调用链分析

### 进程上下文 (`<TASK>`) — 被打断的补丁代码

```
do_exit                                       ← syz-executor 进程退出
  perf_event_release_kernel
    _free_event
      perf_trace_destroy
        perf_ftrace_event_register            ← 注销 ftrace 事件
          unregister_ftrace_function
            ftrace_shutdown
              ftrace_modify_all_code
                ftrace_replace_code           → arch/x86/kernel/ftrace.c:241
                  text_poke_queue              → alternative.c:2625
                    text_poke_bp_batch         → alternative.c:2383
                      __nonakr_text_poke       → alternative.c:1855
```

进程退出释放 perf ftrace 事件，触发 ftrace 关闭。`ftrace_replace_code` 遍历所有已注册的 `dyn_ftrace` 记录，对于 `FTRACE_UPDATE_MAKE_NOP` 状态（将函数入口 `call ftrace_trampoline` 替换为 `nop`），调用 `text_poke_queue` 将修改请求加入批量队列，最终由 `text_poke_bp_batch` 通过 INT3 协议执行。

### 中断上下文 (`<IRQ>`) — 崩溃路径

```
sysvec_apic_timer_interrupt                   ← 定时器硬中断
  handle_softirqs                             ← 硬中断退出时触发软中断处理
    run_rebalance_domains (SCHED_SOFTIRQ)     → fair.c:14291
      update_blocked_averages                 → fair.c:14309
        update_rt_rq_load_avg                 → pelt.c:346
          → 命中 INT3 !
```

定时器硬中断在处理完成后退出时（`irq_exit_rcu`），触发挂起的软中断。`SCHED_SOFTIRQ` 的处理函数 `run_rebalance_domains` 调用 `update_blocked_averages`，进而执行到 `update_rt_rq_load_avg`，其函数入口处的 INT3(0xcc) 尚未被替换为真实 opcode。

## 根因分析

### text_poke_bp_batch 三步协议

`text_poke_bp_batch()`（`arch/x86/kernel/alternative.c:2383`）使用 INT3 断点协议对 live 内核代码进行原子性替换：

```
Step 1: 在所有要修改的地址写入 0xcc (INT3) → text_poke_sync()
Step 2: 修改除第一个字节外的其余字节 → text_poke_sync()
Step 3: 将第一个字节从 INT3 (0xcc) 替换为真实操作码 → text_poke_sync()
```

`text_poke_sync()` 通过 `on_each_cpu(do_sync_core, NULL, 1)` 同步所有 CPU 的指令流，但没有禁用中断。

### INT3 异常分发链路

```
exc_int3()                                        traps.c:792
  ├── poke_int3_handler()                         alternative.c:2276  ← 第一步，text_poke INT3 模拟
  │     ├── try_get_desc() → bp_desc              ← 获取当前活跃的补丁描述符
  │     ├── 二分查找 tp_vec 匹配地址
  │     └── 根据 tp->opcode 模拟执行:
  │           CALL (0xE8) → int3_emulate_call()
  │           JMP  (0xE9/0xEB) → int3_emulate_jmp()
  │           Jcc  (0x70-0x7f) → int3_emulate_jcc()
  │           RET  (0xC3) → int3_emulate_ret()
  │           其他 → BUG()
  ├── do_int3()                                    traps.c:757   ← poke_int3_handler 返回 0 时
  │     ├── kprobe_int3_handler()
  │     ├── klp_int3_handler()
  │     └── notify_die(DIE_INT3)
  └── die("int3")                                  ← 所有 handler 均未处理
```

### ftrace NOP 的 INT3 模拟设置

在 `text_poke_loc_init()`（`alternative.c:2516`）中，ftrace call→nop 场景的处理：

```c
// alternative.c:2524-2533
memcpy((void *)tp->text, opcode+i, len-i);   // tp->text = nop (0f 1f 44 00 00)
if (!emulate) emulate = opcode;              // emulate = nop
ret = insn_decode_kernel(&insn, emulate);     // 解码 nop → opcode.bytes[0] = 0x0f
tp->opcode = insn.opcode.bytes[0];            // tp->opcode = 0x0f

// alternative.c:2570-2587 — default: assume NOP
switch (len) {
case 5:                                       // MCOUNT_INSN_SIZE = 5
    BUG_ON(memcmp(emulate, x86_nops[len], len));
    tp->opcode = JMP32_INSN_OPCODE;           // 重写为 0xE9
    tp->disp = 0;                              // 模拟 JMP +0 = nop 效果
    break;
}
```

因此 `poke_int3_handler` 遇到此条目时会执行 `int3_emulate_jmp(regs, ip + 0)`，效果等同于 nop。

### 崩溃的直接原因

`poke_int3_handler` **理论上应能正确处理** ftrace nop 的 INT3 模拟（JMP32+0 = 直接跳过）。但崩溃仍然发生了，说明 `poke_int3_handler` 返回了 0（未处理）。

可能的原因：

1. **`try_get_desc()` 返回 NULL**：如果 `bp_desc.refs` 为 0，说明 `poke_int3_handler` 执行时对应批次的 `text_poke_bp_batch` 尚未设置 `bp_desc` 或已经完成清理。ftrace 批量修改时，`ftrace_replace_code` 可能分多个批次调用 `text_poke_bp_batch`（每批最多 TP_VEC_MAX 个条目）。在批次切换的临界区，存在 `bp_desc` 未正确覆盖当前 INT3 的窗口。

2. **地址不匹配**：`update_rt_rq_load_avg` 可能不在当前批次的 `desc->vec` 中（属于上一批或下一批），二分查找失败，返回 `goto out_put`。

3. **更本质的竞态**：`text_poke_bp_batch` 内部的 Step 1 循环是逐个调用 `text_poke()` 写入 INT3 的。写入一个 INT3 后，如果本地 CPU 立即被定时器中断，且在中断退出时处理软中断命中该 INT3，而此时 `bp_desc.vec` 和 `bp_desc.nr_entries` 可能尚未完全建立或条目不匹配。

### 竞态时间线

```
T0: text_poke_bp_batch() 开始
T1: bp_desc.vec = tp; bp_desc.nr_entries = nr_entries;  ← 设置描述符
T2: atomic_set_release(&bp_desc.refs, 1);               ← 激活
T3: smp_wmb();
T4: for (i = 0; i < nr_entries; i++)
      text_poke(text_poke_addr(&tp[i]), &int3, 1);       ← 逐个写入 INT3
         └── 写入 update_rt_rq_load_avg 入口的 INT3
T5: [定时器中断] → [软中断] → update_rt_rq_load_avg → exc_int3 → poke_int3_handler ← 应处理
T6: text_poke_sync();                                    ← 同步所有 CPU

T7: Step 2 — 修改其余字节
T8: text_poke_sync();
T9: Step 3 — 替换 INT3 为真实 opcode
T10: text_poke_sync();
T11: atomic_dec_and_test(&bp_desc.refs)                  ← 反激活
```

## 修复方案

### 推荐方案：在 text_poke_bp_batch 中禁用软中断

在 `text_poke_bp_batch` 的 INT3 窗口期（Step 1 写入 INT3 → Step 3 替换完成）禁用软中断，防止同一 CPU 的软中断路径进入正在被修改的函数：

```c
static void text_poke_bp_batch(struct text_poke_loc *tp, unsigned int nr_entries)
{
    ...
    atomic_set_release(&bp_desc.refs, 1);
    ...
    smp_wmb();

    /*
     * Disable softirqs to prevent the local CPU from executing
     * a patched function in softirq context while INT3 is in place.
     * The INT3 handler (poke_int3_handler) handles remote CPUs via
     * IPI-based text_poke_sync(), but the local CPU can be interrupted
     * at any point.
     */
    local_bh_disable();

    // Step 1: 写入 INT3
    for (i = 0; i < nr_entries; i++) {
        tp[i].old = *(u8 *)text_poke_addr(&tp[i]);
        text_poke(text_poke_addr(&tp[i]), &int3, INT3_INSN_SIZE);
    }
    text_poke_sync();

    // Step 2: 修改其余字节
    ...

    // Step 3: 替换第一个字节
    ...

    local_bh_enable();

    ...
}
```

**优点**：
- 改动最小，仅影响 text_poke_bp_batch 执行期间
- 硬中断仍可正常响应，不影响系统延迟
- 针对性强，直接解决软中断路径（SCHED_SOFTIRQ 等）的竞态

**为什么不用 `local_irq_disable`**：
- ftrace 单次 batch 最多 256 个条目（TP_VEC_MAX），关中断时间不可控
- 硬中断路径调用被 patch 函数的情况极少
- 硬中断场景下 `poke_int3_handler` 的 INT3 模拟机制正常工作（JMP32+0 = nop 可通过模拟安全跳过）

### 备选方案：在 text_poke_bp_batch 中保存/恢复第一个字节而非使用 INT3

使用 `stop_machine()` 替代 INT3 协议。但这会引入更大开销，仅在极端场景（被 patch 函数在 NMI 上下文中被调用）才需要。

## 相关源码文件

| 文件 | 行号 | 内容 |
|------|------|------|
| `arch/x86/kernel/alternative.c` | 1855–1959 | `__nonakr_text_poke()` — 实际内存写入 |
| `arch/x86/kernel/alternative.c` | 2079–2091 | `__text_poke()` — 路由到 NAKR/非NAKR 实现 |
| `arch/x86/kernel/alternative.c` | 2222–2231 | `struct text_poke_loc` — 补丁条目结构 |
| `arch/x86/kernel/alternative.c` | 2233–2237 | `struct bp_patching_desc` — INT3 批处理描述符 |
| `arch/x86/kernel/alternative.c` | 2276–2356 | `poke_int3_handler()` — INT3 异常模拟处理器 |
| `arch/x86/kernel/alternative.c` | 2383–2514 | `text_poke_bp_batch()` — INT3 三步批量补丁 |
| `arch/x86/kernel/alternative.c` | 2516–2589 | `text_poke_loc_init()` — 补丁条目初始化 |
| `arch/x86/kernel/alternative.c` | 2625–2633 | `text_poke_queue()` — 批量队列入口 |
| `arch/x86/kernel/traps.c` | 757–780 | `do_int3()` — INT3 异常分发 |
| `arch/x86/kernel/traps.c` | 792–824 | `exc_int3()` — INT3 IDT 入口 |
| `arch/x86/include/asm/text-patching.h` | 147–218 | `int3_emulate_*()` — 指令模拟函数 |
| `arch/x86/kernel/ftrace.c` | 241–297 | `ftrace_replace_code()` — ftrace 批量代码替换 |
| `kernel/sched/pelt.c` | 346–359 | `update_rt_rq_load_avg()` — 崩溃命中函数 |
| `kernel/sched/fair.c` | 14291 | `run_rebalance_domains()` — SCHED_SOFTIRQ 处理函数 |
