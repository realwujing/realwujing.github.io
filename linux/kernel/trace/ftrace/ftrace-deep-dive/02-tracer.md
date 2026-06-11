# 第2篇：Tracer 引擎 — trace_array、function/function_graph 与 trace_pipe

> 源码：`kernel/trace/trace.c` `kernel/trace/trace_functions.c` `kernel/trace/trace_functions_graph.c` | 头文件：`kernel/trace/trace.h`

系列目录：[ftrace 内核源码深度解析](./README.md)

---

## 1. struct trace_array — 跟踪实例

每个 ftrace 实例都是独立的 `struct trace_array`（全局实例为 `global_trace`）：

```c
// kernel/trace/trace.h:333
struct trace_array {
    struct list_head        list;           // 在全局 trace 实例链表中
    char                    *name;          // 实例名（如 "global"、"instances/my_trace"）
    struct trace_buffer      trace_buffer;  // ★ 每 CPU 的 ring buffer
    struct trace_buffer      max_buffer;    // snapshot 缓冲区
    int                     buffer_disabled; // 用户态禁用跟踪的标志

    struct tracer           *current_trace; // ★ 当前活跃的跟踪器
    struct ftrace_ops       *ops;           // function tracer 使用的 ftrace_ops
    struct dentry           *dir;           // /sys/kernel/tracing/instances/<name>/

    /* 过滤 */
    struct ftrace_hash      *function_hash; // set_ftrace_filter
    struct ftrace_hash      *function_notrace_hash; // set_ftrace_notrace

    /* 输出选项 */
    int                     trace_flags;    // TRACE_ITER_* flags
    unsigned long           trace_flags_index;

    /* 管道读取 */
    wait_queue_head_t       wait;           // poll/epoll 的等待队列
    struct list_head        iter_list;      // 文件描述符的迭代器
    struct list_head        sparse_list;    // 内存节省模式

    /* 事件子系统 */
    struct list_head        systems;        // event subsystem 链表
    struct list_head        events;         // event entry 链表

    /* 统计 */
    unsigned int            trace_cpu;      // per-CPU 跟踪开关
    u64                     time_start;     // 实例创建的时间戳
};
```

**`global_trace`** 是单例的根实例——挂载于 `/sys/kernel/tracing/trace`。其他实例（`/sys/kernel/tracing/instances/xxx/trace`）是独立的 `trace_array` 维护各自的 ring buffer。

---

## 2. 内核的环形缓冲 — trace_buffer

每个 trace_array 都有一组 `trace_buffer`，按 CPU 分开：

```
struct trace_buffer:
  每个 CPU 由一个 ring buffer 页链组成
  写入: trace_buffer_lock_reserve → 预留空间 → 填充数据 → trace_buffer_unlock_commit
  读取: trace_iterator → 从每 CPU 的 ring buffer 中按顺序读取
```

**关键**：与 perf 的 ring buffer 不同，ftrace 的 `trace_buffer` 不通过 mmap 直接映射给用户态，而是通过 `trace_pipe` 的 `read()` 和 `splice()` 接口——每次 read 从 ring buffer 清理出旧数据。

---

## 3. `function` tracer — 基础函数跟踪

```c
// kernel/trace/trace_functions.c

// ★ 注册为 ftrace_ops 的回调
static struct ftrace_ops function_trace_ops = {
    .func = function_trace_call,
};

// ★ mcount 桩调用的实际函数
static void function_trace_call(unsigned long ip, unsigned long parent_ip,
                                struct ftrace_ops *op, struct ftrace_regs *fregs)
{
    struct trace_array *tr = op->private;      // 获取 trace_array
    struct trace_array_cpu *cpu_data;

    // ① 从 trace_buffer 分配一个条目
    // ② 填充: ip (函数地址), parent_ip (调用者地址)
    // ③ 提交到 ring buffer
    // ④ 检查是否需要调度

    // 用户态通过 cat trace 查看: "do_sys_open() { sys_openat() { ... } }"
}
```

**使用方式**：
```
echo function > /sys/kernel/tracing/current_tracer
echo "do_sys_*" > /sys/kernel/tracing/set_ftrace_filter
cat /sys/kernel/tracing/trace
```

---

## 4. `function_graph` tracer — 调用图（函数进入/退出）

```
用户态看到的效果:
 0)               |  do_sys_open() {
 0)   0.233 us    |    getname();
 0)   0.432 us    |    get_unused_fd_flags();
 0)   0.181 us    |    do_filp_open();
 0)   4.912 us    |  }
```

这是通过两个钩子实现的：

```c
// kernel/trace/trace_functions_graph.c

// ① 进入钩子: function_graph_enter
static int function_graph_enter(unsigned long func, ...)
{
    // 保存当前任务的开始时间戳
    // 在 ring buffer 中预留空间
    // 设置 trace->ret_stack[trace->curr_ret_stack] = start_time
}

// ② 退出钩子: function_graph_exit
static void function_graph_exit(...)
{
    // 从 ret_stack 取出 enter_time
    // 计算持续时间
    // 在 ring buffer 中提交（含 func + duration）
}

// 注册这两个钩子到全局 trace 系统
static struct ftrace_ops graph_ops = {
    .func = NULL,  // ← 不用 mcount 直接调用
    .funcgraph_ops = {
        .entryfunc = &function_graph_enter,
        .retfunc   = &function_graph_exit,
    },
};
```

**跟踪器注册**：
```
echo function_graph > /sys/kernel/tracing/current_tracer
```

---

## 5. trace_pipe — 用户态读取

### 5.1 cat trace（静态读取）

```
cat /sys/kernel/tracing/trace
  → tracing_read_pipe (file operations)
    → trace_iterator → 遍历所有已记录的 ring buffer 条目
    → 格式化输出: 通过 trace_event.call->print 回调
```

### 5.2 cat trace_pipe（流式读取）

```
cat /sys/kernel/tracing/trace_pipe
  → tracing_read_pipe (non-zero 消费模式)
    → 等待新条目 (wait_for_completion(&iter->wait))
    → 读取新条目 + 格式化
    → iter->pos 推进，旧条目被"消费"掉
```

### 5.3 splice 到 perf（零拷贝）

```
perf trace 或 trace-cmd 使用 splice(2) 从 trace_pipe 到用户态:
  → tracing_splice_read_pipe
    → 直接传递 page 引用给用户态 (splice_to_pipe)
    → 避免了 copy_to_user → 零拷贝管道
```

---

## 6. 跟踪状态的流转

```
① 注册跟踪器
  echo "function" > current_tracer
    → tracing_set_trace_write
      → __tracing_set_tracer(tr, buf)
        → tracer_init(tracer)    — the tracer's optional init callback
        → tr->current_trace = t   — assign the tracer
        → create_trace_option_files(tr, t)  — create per-tracer control files

② enabler  计数器跟踪
  tracing_start() / tracing_stop()  (写入 /sys/kernel/tracing/tracing_on)
    → ring_buffer_record_on/off(tr->trace_buffer.buffer)
       → 1: 允许 mcount 桩回调写入 ring buffer
       → 0: mcount 桩回调被忽略 (ring buffer 的锁定计数器)

③ 数据流向
  每个 mcount 调用 → tracer->func → trace_buffer_lock_reserve
    → 写入每 CPU 的 ring buffer
    → 用户态 cat trace → tracing_read 读取
```

---

## 7. 总结

| 组件 | 关键结构 | 核心作用 |
|------|---------|---------|
| trace_array | `struct trace_array` (trace.h:333) | 每个跟踪实例的完整状态 |
| trace_buffer | `struct trace_buffer` (per trace_array) | 每 CPU 的 ring buffer |
| function tracer | `function_trace_call` (trace_functions.c) | 记录每个函数的进入 |
| function_graph | `function_graph_enter/exit` (trace_functions_graph.c) | 记录函数进入/退出+持续时间 |
| trace 输出 | `tracing_read_pipe` → trace_iterator | 从 ring buffer 格式化读取 |
| splice 零拷贝 | `tracing_splice_read_pipe` | 直接传 page 给用户态 |

## 下一篇文章

→ [第3篇：Trace Events — TRACE_EVENT 宏展开与 perf 桥接](./03-events.md)
