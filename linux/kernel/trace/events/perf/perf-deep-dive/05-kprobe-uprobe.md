# 第5篇：Kprobe、Uprobe 与 Tracepoint — 三大探测机制与 perf 集成

> 源码：`kernel/kprobes.c` `kernel/events/uprobes.c` `kernel/trace/trace_event_perf.c` | 头文件：`include/linux/kprobes.h`

系列目录：[perf 内核源码深度解析](./README.md)

---

## 1. 三大探测机制概览

| 机制 | 探测点 | 触发方式 | 开销 | 精度 |
|------|--------|---------|------|------|
| **tracepoint** | 内核静态埋点（TRACE_EVENT 宏） | 函数调用 | 极低（条件判断 + 一个 NOP） | 只在有埋点的地方可用 |
| **kprobe** | 内核任意指令地址 + 函数入口 | int3 断点 | 中等（int3 陷入 + 单步） | 任意地址，包括 inline 函数 |
| **uprobe** | 用户态进程任意指令地址 | int3 断点（同进程页表） | 较高（int3 + 页表操作） | 任意地址，用户空间包括 JIT 代码 |

---

## 2. Kprobe — 内核断点探测

### 2.1 struct kprobe

```c
// include/linux/kprobes.h:59
struct kprobe {
    struct hlist_node   hlist;          // 哈希表节点（按地址索引）
    unsigned long       nmissed;        // 衰减计数器（用于优化/去优化决策）
    kprobe_opcode_t     *addr;          // ★ 探测点的实际地址
    const char          *symbol_name;   // ★ 符号名称（如 "do_sys_open"）
    unsigned int        offset;         // ★ 符号内的偏移（0=函数入口）
    kprobe_pre_handler_t  pre_handler;  // ★ 断点命中的回调
    kprobe_post_handler_t post_handler; // 单步执行后的回调
    kprobe_opcode_t     opcode;         // 保存的原指令字节（用于恢复）
    struct arch_specific_insn ainsn;    // ★ 架构特定的复制的原指令（x86: 5 字节）
    u32                 flags;          // KPROBE_FLAG_GONE/DISABLED/OPTIMIZED/FTRACE
};
```

**标志位**（kprobes.h:97-105）：
- `KPROBE_FLAG_GONE` — 探测点已失效（模块卸载等原因）
- `KPROBE_FLAG_DISABLED` — 用户已禁用
- `KPROBE_FLAG_OPTIMIZED` — 已优化为 `jmp` 而非 `int3`
- `KPROBE_FLAG_FTRACE` — 通过 ftrace mcount 桩注入（零 int3 开销）

### 2.2 哈希表

```c
// kernel/kprobes.c:51-64
#define KPROBE_HASH_BITS    6
#define KPROBE_TABLE_SIZE   (1 << KPROBE_HASH_BITS)  // 64 桶
static struct hlist_head kprobe_table[KPROBE_TABLE_SIZE];  // 按地址哈希
static DEFINE_MUTEX(kprobe_mutex);                         // 序列化注册/注销
```

### 2.3 register_kprobe — 注册

```c
// kernel/kprobes.c:1708
int register_kprobe(struct kprobe *p)
{
    // ① 规范地址：从 symbol_name + offset 解析为实际地址
    addr = _kprobe_addr(p->addr, p->symbol_name, p->offset);

    // ② 安全检查：地址是否在黑名单中？是否在 NMI 区域？
    check_kprobe_address_safe(p, ...);

    // ③ 检查是否有已存在的探测点 → 合并为 aggregated probe
    // ④ hlist_add_head_rcu → 把 kprobe 加上哈希表
    // ⑤ arm_kprobe(p) → 在 addr 处写入 int3 (x86: 0xCC)
    // ⑥ try_to_optimize_kprobe(p) → 如果可能，改为 jmp 优化（无 int3 开销）
}
```

### 2.4 kprobe_int3_handler — 命中断点

```c
// arch/x86/kernel/kprobes/core.c:978
int kprobe_int3_handler(struct pt_regs *regs)
{
    // ① 用户态 → 忽略（uprobe 的 int3 有独立的处理）
    if (user_mode(regs)) return 0;

    // ② IP 中含有 int3 指令 → 回退到 int3 之前的地址
    addr = regs->ip - sizeof(kprobe_opcode_t);  // x86: -1

    // ③ 查找探测点
    p = get_kprobe(addr);
    if (p) {
        // ④ 设置状态 = KPROBE_HIT_ACTIVE
        kcb->kprobe_status = KPROBE_HIT_ACTIVE;

        // ⑤ ★ 调用 pre_handler
        if (p->pre_handler(p, regs))
            return 1;  // pre_handler 处理完毕，不需要单步

        // ⑥ 设置单步执行（恢复原指令，执行 1 条指令，调用 post_handler，重新插入 int3）
        setup_singlestep(p, regs, kcb, 0);
        return 1;
    }
}
```

### 2.5 kretprobe — 函数返回探测

```c
// include/linux/kprobes.h:146
struct kretprobe {
    struct kprobe kp;
    kretprobe_handler_t handler;        // ★ 函数返回时调用的回调
    kretprobe_handler_t entry_handler;  // ★ 函数进入时调用的回调
    int maxactive;                      // 最大并发探测数
    int nmissed;                        // 衰减计数器
    struct rethook *rh;                 // rethook 引擎
};

// kernel/kprobes.c:~2255
int register_kretprobe(struct kretprobe *rp)
{
    // 在函数入口处设置 kprobe（pre_handler = pre_handler_kretprobe）
    rp->kp.pre_handler = pre_handler_kretprobe;
    register_kprobe(&rp->kp);

    // 在函数返回处设置 rethook（替换返回地址）
}
```

---

## 3. Uprobe — 用户态断点探测

### 3.1 struct uprobe

```c
// kernel/events/uprobes.c:62
struct uprobe {
    struct rb_node          rb_node;         // 在 inode 的红黑树中的节点
    struct rw_semaphore     register_rwsem;  // 消费者修改锁
    struct list_head        consumers;       // ★ uprobe_consumer 链表
    struct inode            *inode;          // 包含探测点的文件 inode
    loff_t                  offset;          // ★ 探测点的文件偏移
    struct arch_uprobe      arch;            // 架构特定的原指令副本
};
```

### 3.2 uprobe_register

```c
// kernel/events/uprobes.c:1390
struct uprobe *uprobe_register(struct inode *inode,
                               loff_t offset, loff_t ref_ctr_offset,
                               struct uprobe_consumer *uc)
{
    // ① 在 inode 的红黑树中查找/创建 uprobe
    // ② 添加 consumer 到 uprobe->consumers 链表
    // ③ 在 inode 的页面缓存中写入断点指令 (int3 on x86)
}
```

### 3.3 handle_swbp — 命中断点

```c
// kernel/events/uprobes.c:2712
static void handle_swbp(struct pt_regs *regs)
{
    // ① 获取断点 VA（有符号扩展）
    // ② 查找对应 uprobe（通过 mm→inode→offset）
    // ③ 恢复原指令，调用 handler_chain
    // ④ 设置单步执行或 SSOL
}
```

---

## 4. Perf 集成 — perf_kprobe_init / perf_uprobe_init

### 4.1 kprobe 作为 perf_event

```c
// kernel/trace/trace_event_perf.c:247
int perf_kprobe_init(struct perf_event *p_event, bool is_retprobe)
{
    // ① 从用户态复制 kprobe 符号名: p_event->attr.kprobe_func
    // ② 创建 trace_kprobe: create_local_trace_kprobe()
    // ③ 初始化 perf trace_event: perf_trace_event_init(tp_event, p_event)
}
```

**使用方式**：
```
perf probe -a 'myprobe do_sys_open filename=%si'
perf record -e probe:myprobe -a -- <cmd>
```

### 4.2 uprobe 作为 perf_event

```c
// kernel/trace/trace_event_perf.c:298
int perf_uprobe_init(struct perf_event *p_event,
                     unsigned long ref_ctr_offset, bool is_retprobe)
{
    // ① 从用户态复制文件路径: p_event->attr.uprobe_path
    // ② 创建 trace_uprobe
    // ③ 初始化 perf trace_event
}
```

**使用方式**：
```
perf probe -x /bin/bash -a 'myprobe readline'
perf record -e probe_myprobe -- <cmd>
```

---

## 5. 完整探测路径

```
用户态 perf probe 命令
  → 写入 /sys/kernel/tracing/kprobe_events 或 uprobe_events
    → trace_kprobe_create / trace_uprobe_create (dyn_event 框架)
      → register_kprobe / uprobe_register
        → 在探测地址写入 int3 (0xCC)

执行流:
  进程/中断到达探测地址
    → int3 陷入内核
      → kprobe_int3_handler (内核地址) 或 handle_swbp (用户态地址)
        → pre_handler 回调:
          → 如果是 perf 事件: perf_trace_perf_submit → perf_output_sample
          → 如果是 ftrace: trace_event_buffer_reserve → commit
          → 如果是 BPF: bpf_prog 回调
        → 单步执行原指令
        → post_handler 回调
```

---

## 6. 总结

| 组件 | 关键文件 | 核心作用 |
|------|---------|---------|
| kprobe 核心 | `kprobes.c:1708` register_kprobe | int3 替换 + 哈希表管理 |
| kprobe 命中 | `core.c:978` kprobe_int3_handler | int3 陷入分发 + 单步 |
| kretprobe | `kprobes.c:~2255` register_kretprobe | 返回地址 rethook |
| uprobe 核心 | `uprobes.c:1390` uprobe_register | 文件 inode 红黑树 + 断点注入 |
| uprobe 命中 | `uprobes.c:2712` handle_swbp | 查找 uprobe + handler_chain |
| perf kprobe | `trace_event_perf.c:247` perf_kprobe_init | kprobe → perf_event 桥接 |
| perf uprobe | `trace_event_perf.c:298` perf_uprobe_init | uprobe → perf_event 桥接 |
