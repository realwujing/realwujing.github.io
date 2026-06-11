# 第1篇：ftrace 核心 — mcount 桩、ftrace_ops 与动态替换

> 源码：`kernel/trace/ftrace.c` | 头文件：`include/linux/ftrace.h`

系列目录：[ftrace 内核源码深度解析](./README.md)

---

## 1. ftrace 的基本原理

ftrace 的核心机制是**在编译时给每个函数入口插入 mcount 桩**：

```c
// 编译后的形态
void foo(void) {
    __fentry__();  // ← GCC -pg / -mfentry 插入的桩
    // 函数体
}
```

内核将所有函数的 mcount 桩替换为 NOP（无跟踪时）或调用跟踪器的函数（跟踪时）。**不跟踪时完全零开销**，跟踪时动态替换为实际函数。

---

## 2. struct ftrace_ops — 跟踪器注册的实体

```c
// include/linux/ftrace.h:447
struct ftrace_ops {
    ftrace_func_t       func;           // ★ 跟踪回调函数
    ftrace_func_t       saved_func;     // 上一个跟踪器的回调（链式切换）
    struct ftrace_ops   *next;          // 链式 ops 链表（全局链表）
    unsigned long       flags;          // FTRACE_OPS_FL_* 标志
    int __percpu        *disabled;      // per-CPU 禁用的计数器
    struct ftrace_ops   *local_ops;     // 每 CPU 本地 ops
    void                *private;       // 跟踪器私有数据
    struct ftrace_hash  *func_hash;     // ★ set_ftrace_filter 过滤器
    struct ftrace_hash  *notrace_hash;  // set_ftrace_notrace 反向过滤器

#ifdef CONFIG_DYNAMIC_FTRACE
    ftrace_func_t       trampoline;     // 跳板函数（动态 ftrace）
    int                 trampoline_size;
    unsigned long       trampoline_status;
#endif
};
```

**三个核心字段**：
- `func`：实际被 mcount 桩调用的回调函数——每个被跟踪的函数调用都会经过这个回调
- `func_hash`：`set_ftrace_filter` 设置的函数白名单（只跟踪这些函数）
- `notrace_hash`：`set_ftrace_notrace` 设置的反向黑名单（永远不跟踪这些函数）

---

## 3. register_ftrace_function — 注册跟踪器

```c
// kernel/trace/ftrace.c
int register_ftrace_function(struct ftrace_ops *ops)
{
    // ① 验证: ops->func 不能为 NULL
    // ② 加入全局 ftrace_ops_list 链表
    //    ftrace_ops_list → ops1 → ops2 → ... → ftrace_list_end
    // ③ 调用 ftrace_startup
    //    → 对所有 mcount 桩: NOP → call ftrace_caller
    //    → ftrace_caller 遍历 ftrace_ops_list，调用每个 ops->func
    // ④ 返回 0
}

void unregister_ftrace_function(struct ftrace_ops *ops)
{
    // ① 从 ftrace_ops_list 移除
    // ② 调用 ftrace_shutdown
    //    → 如果链表为空: ftrace_caller → NOP（恢复零开销）
}
```

**关键**：每次注册/注销都导致**所有**函数的 mcount 桩被重写。但因为有缓存（record 列表），只需更新那些真正被过滤到的函数。

---

## 4. 桩的类型 — NOP、ftrace_caller、REG

ftrace 维护每个函数的"记录"（`struct dyn_ftrace`）：

```c
// include/linux/ftrace.h
struct dyn_ftrace {
    unsigned long ip;               // 函数入口的 IP 地址
    unsigned long flags;            // FTRACE_FL_* 标志
    unsigned long counter;          // 此函数被多少个 ops 跟踪
    struct dyn_arch_ftrace arch;    // 架构特定的 NOP/REG 信息
};
```

**三种桩状态**：

| 状态 | mcount 处实际代码 | 何时使用 |
|------|------------------|---------|
| **NOP** | `nop` (x86: 5-byte NOP) | 函数不被任何跟踪器跟踪 |
| **CALL** | `call ftrace_caller` | 函数被至少一个跟踪器跟踪 |
| **REGS** | `call ftrace_regs_caller` | 跟踪器需要寄存器快照 (SAVE_REGS flag) |

内核只在状态转换时修改代码：

```
NOP → CALL (register_ftrace_function):
  ftrace_make_call(rec, ip) → 修改 mcount 处的代码，从 nop 改为 call ftrace_caller

CALL → NOP (unregister_ftrace_function):
  ftrace_make_nop(rec, ip) → 修改 mcount 处的代码，从 call 改为 nop
```

---

## 5. 函数过滤 — set_ftrace_filter

```c
// 通过 /sys/kernel/tracing/set_ftrace_filter 控制
// echo "do_sys_open" > set_ftrace_filter
// echo "tcp_*" > set_ftrace_filter
// echo ":mod:ext4" > set_ftrace_filter

// 内核侧: ftrace_ops->func_hash 存储过滤列表
// update_ops_hash(ops, hash) → 重新扫描 mcount 桩
// 只对匹配的函数: NOP → CALL
```

**匹配链**：
```
ftrace_match_records(hash, func_name)
  → 遍历 dyn_ftrace 记录表
  → glob_match(rec->ip → kallsyms_lookup → function_name, filter)
  → 匹配的函数标记为 ENABLED
  → ftrace_update_code → 修改 mcount 桩
```

---

## 6. 动态 ftrace（DYNAMIC_FTRACE）

动态 ftrace 允许**运行时**添加/删除跟踪点，不需要重新编译内核。实现步骤：

```
启动时 (ftrace_init):
  ① 扫描所有编译时记录的 mcount 调用点 (__mcount_loc 段)
  ② 创建 struct dyn_ftrace 记录数组
  ③ 将所有 mcount 桩改为 NOP

注册时:
  ① 更新 func_hash 过滤表
  ② ftrace_update_code → 匹配的函数桩: NOP → CALL
  ③ 通过 __stop_machine 原子操作保证所有 CPU 的一致性
```

**`__stop_machine` 是关键**：重写 mcount 桩需要**所有 CPU 都停止**（类似 FTEXT 段修改），否则可能出现不一致的代码读取。

---

## 7. mcount 桩的具体实现 (x86)

```asm
// arch/x86/kernel/ftrace_64.S
ENTRY(ftrace_caller)
    pushq %rbp                        ; 保存调用方栈帧
    movq %rsp, %rbp

    /* 计算父亲函数的 IP (mcount 被调用时的返回地址) */
    movq 8(%rbp), %rax               ; parent_ip = 返回地址
    subq $MCOUNT_INSN_SIZE, %rax     ; 修正为 mcount 指令本身

    /* 遍历 ftrace_ops_list，调用每个 ops->func */
    call ftrace_ops_list_func

    popq %rbp                         ; 恢复栈帧
    ret
```

---

## 8. 总结

| 组件 | 关键函数/结构 | 核心作用 |
|------|-------------|---------|
| mcount 桩 | `__fentry__` | 编译时自动插入到每个函数入口 |
| dyn_ftrace 记录 | `dyn_ftrace.ip` / `.flags` | 每个函数的跟踪状态 |
| 桩类型 | NOP / CALL / REGS | 零开销 / 有跟踪器 / 有寄存器要求 |
| filter | `func_hash` / `notrace_hash` | set_ftrace_filter/set_ftrace_notrace |
| 注册/注销 | `register_ftrace_function` / `unregister_ftrace_function` | 加入/移除 ftrace_ops_list |
| 代码修改 | `ftrace_make_call` / `ftrace_make_nop` | NOP ↔ CALL 桩切换 |

## 下一篇文章

→ [第2篇：Tracer 引擎 — trace_array、function/function_graph 与 trace_pipe](./02-tracer.md)
