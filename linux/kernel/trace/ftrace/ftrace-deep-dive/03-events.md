# 第3篇：Trace Events — TRACE_EVENT 宏展开、过滤与 perf 桥接

> 源码：`kernel/trace/trace_events.c` `include/trace/trace_events.h` `kernel/trace/trace_event_perf.c`

系列目录：[ftrace 内核源码深度解析](./README.md)

---

## 1. TRACE_EVENT — 一行代码生成的完整框架

当你写：

```c
TRACE_EVENT(sched_switch,
    TP_PROTO(bool preempt, struct task_struct *prev, struct task_struct *next),
    TP_ARGS(preempt, prev, next),
    TP_STRUCT__entry(
        __array(char, prev_comm, TASK_COMM_LEN)
        __field(pid_t, prev_pid)
        __field(int,  prev_prio)
        __array(char, next_comm, TASK_COMM_LEN)
        __field(pid_t, next_pid)
        __field(int,  next_prio)
    ),
    TP_fast_assign(
        memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
        __entry->prev_pid = prev->pid;
        __entry->prev_prio = prev->prio;
        ...
    ),
    TP_printk("prev_comm=%s prev_pid=%d prev_prio=%d ...")
);
```

这一行生成**完整的 tracepoint 管道**：

| 生成内容 | 位置 | 用途 |
|---------|------|------|
| 事件类型定义 | `DECLARE_EVENT_CLASS`, `DEFINE_EVENT` | 同一类事件的共享定义 |
| ftrace ring buffer 代码 | `TP_fast_assign` 写入 `__entry` 结构体 | ftrace trace 输出 |
| perf 采样代码 | `perf_trace_##call` 函数 | perf record 输出 |
| trace event filter 代码 | `event_filter` 解析和评估 | 复杂过滤表达式 |
| trace event format | `/sys/.../events/.../format` | 用户态工具读取事件结构 |
| 注册/注销回调 | `trace_event_raw_init`, `trace_event_reg` | 注册事件和 trigger |

---

## 2. 事件的三个表示法

| 表示 | 结构 | 用途 |
|------|------|------|
| **trace_event_call** | 每个事件一个实例 | 顶层钩子：将 tracepoint 连接到 ftrace/perf/histogram |
| **trace_event_class** | 共享类模板 | `sched_switch`, `sched_wakeup` 共享同一个类 |
| **trace_event_file** | 实例化文件 | `/sys/kernel/tracing/events/sched/sched_switch/enable` |

```c
struct trace_event_call {
    struct list_head        list;           // 在全局 call 链表中
    struct trace_event_class *class;        // ★ 指向共享模板
    struct trace_event      event;          // 事件信息（fmt, type）
    struct trace_event_file *se;            // 对应 sysfs enable 文件
    struct event_filter     *filter;        // ★ 复杂过滤表达式
    int                     flags;          // TRACE_EVENT_FL_* flags
    struct tracepoint       *tp;           // ★ 底层 tracepoint 实例
};
```

---

## 3. 从 tracepoint 到 ftrace 输出

```
内核中:
  trace_sched_switch(preempt, prev, next)
    │
    ├── ▶ Stub 为空 (没有 ftrace 文件启用此事件时)
    │
    └── ▶ 有 /sys/.../enable 写入:
          → __DO_TRACE(tp, proto, args)
            → trace_event_buffer_reserve → 从 per-CPU ring buffer 分配空间
            → TP_fast_assign → 填充事件数据到 ring buffer
            → trace_event_buffer_commit → 提交 ring buffer
            → 用户在 cat trace 或 trace-cmd report 时看到格式化输出
```

### ftrace enable/disable 流程

```
echo 1 > events/sched/sched_switch/enable
  → event_enable_write
    → ftrace_event_enable_disable
      → tracepoint_probe_register → 注册 tracepoint 回调
      → __ftrace_event_enable_disable → 设置 event->flags |= EVENT_FILE_FL_ENABLED
```

禁用时反向：注销 tracepoint 回调，清除启用位。不需要修改 mcount 桩（和 function tracer 不同）。

---

## 4. 过滤表达式

filter 写入 enable 文件时可以附带过滤条件：

```
echo "next_pid == 1 && prev_prio > 100" > events/sched/sched_switch/filter
```

内核将表达式解析为 `predicate` 结构体，在每个 tracepoint 触发时评估：

```c
struct event_filter {
    struct filter_pred  *root;      // 解析后的过滤表达式树
    char                *filter_string; // 原始过滤字符串
};

// 评估: filter_match_preds(filter, entry)
//   → 只匹配通过 filter 时 tracepoint 才实际写入 ring buffer
```

语法支持：
- `==`, `!=`, `<`, `>`, `>=`, `<=` — 比较
- `&&`, `||` — 逻辑
- 字符串匹配: `prev_comm == "bash"`
- 正则: `prev_comm ~ "kworker/*"`

---

## 5. 与 perf 的桥接 (trace_event_perf.c)

TRACE_EVENT 自动产生 perf 的输出：

```c
// 由 TRACE_EVENT 生成
static void perf_trace_sched_switch(void *__data,
                                    bool preempt,
                                    struct task_struct *prev,
                                    struct task_struct *next)
{
    struct trace_event_call *tp_event = __data;
    struct trace_event_data_offsets_sched_switch __maybe_unused __data_offsets;
    struct trace_event_raw_sched_switch *entry;

    // ① 分配 perf ring buffer 空间
    int __entry_size = sizeof(*entry) + sizeof(u32);
    char *__perf_task = NULL;
    entry = perf_trace_buf_alloc(__entry_size, &__perf_task);

    // ② 填充事件数据
    perf_fetch_caller_regs(&__regs);
    entry->prev_pid = prev->pid;
    entry->next_pid = next->pid;
    ...

    // ③ 提交给 perf
    perf_trace_run_bpf_submit(entry, __entry_size, rctx, &__regs,
                              tp_event, __count, &__perf_task);
}
```

`perf_trace_run_bpf_submit` → `perf_output_sample(event, headers, data, regs)` → 写入 perf ring buffer

---

## 6. tracepoint 的注册和生成

TRACE_EVENT 的完整生命周期：

```
编译时:
  TRACE_EVENT(sched_switch, ...)
    → include/trace/events/sched.h 中进行宏展开
    → 生成:
       - struct trace_event_raw_sched_switch (__entry 结构)
       - trace_event_raw_event_sched_switch (ftrace 路径)
       - perf_trace_sched_switch (perf 路径)
       - /sys/.../events/sched/sched_switch/format (输出格式)
       - trace_event_class_sched + trace_event_call_sched_switch (注册结构)

加载时:
  trace_event_init → __trace_early_add_events → 初始化全局 call 列表

用户启用:
  echo 1 > events/sched/sched_switch/enable
    → tracepoint_probe_register
    → ftrace_event_enable_disable

禁用:
  echo 0 > events/sched/sched_switch/enable
    → tracepoint_probe_unregister
    → ftrace_event_enable_disable (清除)
```

---

## 7. 总结

| 组件 | 关键文件 | 核心作用 |
|------|---------|---------|
| TRACE_EVENT | `include/trace/` | 一行生成 ftrace + perf 管道 |
| trace_event_call | `kernel/trace/trace_events.c` | 事件注册/注销 |
| ftrace 输出 | `trace_event_buffer_reserve` → `commit` | 写入 per-CPU ring buffer |
| perf 输出 | `perf_trace_##call` → `perf_output_sample` | 写入 perf ring buffer |
| filter 过滤 | `event_filter` + `filter_match_preds` | 条件匹配评估 |
| sysfs 接口 | `/sys/.../events/.../enable` | 用户态控制 |

## 系列结语

3 篇文章从 ftrace 的核心 `mcount` 桩替换机制和 `register_ftrace_function` 注册起步，到 `trace_array` / `trace_buffer` 的跟踪引擎和 `function`/`function_graph` 两大跟踪器，最后到 `TRACE_EVENT` 宏展开的完整管道——覆盖了 ftrace 从硬件桩到用户态输出的全栈实现。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-core | mcount 桩 + ftrace_ops 注册/注销 | `ftrace.c`, `ftrace.h:447` |
| 02-tracer | trace_array + function/function_graph + trace_pipe | `trace.c`, `trace_functions.c` |
| 03-events | TRACE_EVENT 宏 + 过滤 + perf 桥接 | `trace_events.c`, `trace_event_perf.c` |
