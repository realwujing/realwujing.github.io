# SystemTap — 动态跟踪的内核模块编译器

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)

---

SystemTap 是一个**用户态工具**，内核中**没有** SystemTap 专属的子系统代码。它的核心思想是：把跟踪脚本编译为 C 代码 → 编译为 `.ko` 内核模块 → 通过已有的 kprobe/tracepoint/uprobe 基础设施运行。

---

## 1. 架构

```
用户态                              内核态
─────                              ─────

stap 脚本 (.stp)                   ┌─────────────────────┐
    │                              │  SystemTap 模块      │
    ▼                              │  (自动生成的 .ko)    │
stap 编译器                        │                     │
  translate → C 代码                │  register_kprobe     │
  compile  → .ko                    │  register_kretprobe  │
    │                              │  tracepoint_probe_   │
    ▼                              │      register        │
staprun (模块加载器)                 │  relay_open          │
  insmod → .ko                     │                     │
  read relay → 控制台输出           └─────────────────────┘
```

**关键**：`stap` 编译器生成的 C 代码直接在已有的 `register_kprobe`、`tracepoint_probe_register`、`uprobe_register` 上注册回调，**不修改内核任何一行代码**。

---

## 2. 三种探测方式与内核接口

### 2.1 kernel.function → kprobe

```systemtap
# 探测 do_sys_open 函数入口
probe kernel.function("do_sys_open") {
    printf("opening %s\n", kernel_string($filename))
}
```

编译器生成：
```c
// 自动生成的模块代码
static int entry_handler(struct kprobe *p, struct pt_regs *regs) {
    // 提取参数 — $filename → struct pt_regs 偏移
    // printf → _stp_print (relay 通道)
    return 0;
}

static struct kprobe probe = {
    .symbol_name = "do_sys_open",
    .pre_handler = entry_handler,
};

static int __init my_init(void) {
    register_kprobe(&probe);              // kprobes.h:405
}
```

底层：`register_kprobe` → `arch/x86/kernel/kprobes/core.c:978` `kprobe_int3_handler` → `aggr_pre_handler` → 调用 `entry_handler`

### 2.2 kernel.trace → tracepoint

```systemtap
# 探测 tracepoint
probe kernel.trace("sched:sched_switch") {
    printf("%s -> %s\n", kernel_string($prev->comm), kernel_string($next->comm))
}
```

编译器生成模块代码：
```c
static void tp_handler(void *__data, bool preempt,
                       struct task_struct *prev, struct task_struct *next) {
    // $prev → prev, $next → next (从 tracepoint 原型的参数映射)
}

static int __init my_init(void) {
    tracepoint_probe_register(tp, tp_handler, NULL);  // tracepoint.h
}
```

底层：`tracepoint_probe_register` 在 `include/linux/tracepoint.h` 中，内核中注册最多 `2^16` 个回调。

### 2.3 process.function → uprobe

```systemtap
# 探测用户态函数  
probe process("/bin/bash").function("readline") {
    printf("readline called\n")
}
```

编译器生成：
```c
static int handler(struct uprobe_consumer *self, struct pt_regs *regs, __u64 *data) {
    _stp_printf("readline called\n");
    return 0;
}

static struct uprobe_consumer uc = { .handler = handler };

static int __init my_init(void) {
    uprobe_register(inode, offset, 0, &uc);    // uprobes.h:209
}
```

---

## 3. 输出 — relay 接口

SystemTap 模块不直接 `printk`，而是通过 `relay` 接口向用户态传递数据：

```c
// 编译器自动生成的 relay 通道
struct rchan *relay_channel = relay_open("stap", NULL, SUBBUF_SIZE,
                                         N_SUBBUFS, &relay_callbacks, NULL);
// printf → relay_write(relay_channel, buf, len)

// staprun 用户态:
read(relay_fd, buf, sizeof(buf)) → 得到 printf 输出
```

这保证了**不污染 dmesg/kmsg**，输出完全独立的通道。

---

## 4. 安全模型

SystemTap 模块运行在**内核态**，但通过以下机制保证安全：

| 机制 | 实现 | 作用 |
|------|------|------|
| **编译时类型检查** | DWARF/CTF/BTF 类型信息 | 拒绝不正确的类型转换 |
| **地址检查** | `kallsyms_lookup`, `module_ksym` | 验证探测地址是有效的内核符号 |
| **数组边界** | `stp_bound_check()` | 防止数组越界 |
| **kill 免疫** | `stp_ctl_write(STP_EXIT)` | 模块被强制卸载时安全退出 |
| **guru 模式** | `-g` flag | 允许危险操作（需要 root 且明确授权） |

---

## 5. 与 perf、bpftrace 的对比

| | SystemTap | perf + kprobe | bpftrace |
|---|---|---|---|
| **脚本语言** | stp 脚本 → C → .ko | perf probe 命令（字符串） | bpftrace 脚本（类 awk） |
| **编译目标** | 内核模块 (.ko) | perf 内部 bytecode | BPF bytecode |
| **安全模型** | 编译时类型检查 + 模块签名 | 无（直接注入 int3） | **Verifier 完整安全性** |
| **jitted 代码** | 否（总是原生 C in .ko） | 否 | 是（BPF JIT → x86 原生） |
| **内核入口** | register_kprobe | register_kprobe | bpf_prog_run |
| **开销** | 中（kprobe + 模块上下文切换） | 低（直接 perf ring buffer） | **最低**（BPF JIT + map 无锁） |
| **适用场景** | 复杂的多函数跟踪、模板 | 简单的单函数探测 | **高性能生产环境** |

**SystemTap 的优势**：
- 模板和宏系统非常强大（遍历所有内核函数、按模式匹配）
- 支持复杂的数据聚合、直方图
- C 代码输出使得复杂逻辑更直接
- 不需要 BPF verifier 限制

**bpftrace 的优势**：
- 安全性（BPF verifier 保证）
- 内核许可（多数探测点甚至非 GPL 程序也能用）
- 性能（JIT + 无锁 map）

---

## 6. 与已有系列的关系

| 已有系列 | 本节覆盖 |
|---------|---------|
| kprobe/uprobe（perf 第 5 篇） | SystemTap 使用 register_kprobe/uprobe 作为底层 |
| tracepoint（ftrace 第 3 篇） | SystemTap 使用 tracepoint_probe_register 作为底层 |
| BPF（bpf 系列） | bpftrace 作为 SystemTap 的替代，BPF verifier 对比 |

---

## 总结

SystemTap 是**用户态编译器 + 内核基础设施调用者**的组合。它不修改内核代码，通过 `register_kprobe`、`tracepoint_probe_register`、`uprobe_register` 在现有基础上构建跟踪能力。与 bpftrace 的核心区别在于编译目标：SystemTap 编译为完整的内核模块，bpftrace 编译为受限的 BPF 程序——前者能力更强但安全性更低，后者安全但受 verifier 限制。
