# ftrace 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`kernel/trace/ftrace.c` `kernel/trace/trace.c` | 头文件：`include/linux/ftrace.h`

---

ftrace 是 Linux 内核的函数级跟踪框架。本系列解剖 `register_ftrace_function`、`trace_array`、`function_graph` 和 `trace_event` 的内核实现。

## 文章目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [01-core.md](./01-core.md) | ftrace 核心 — struct ftrace_ops、mcount 桩与过滤器 |
| 2 | [02-tracer.md](./02-tracer.md) | Tracer 引擎 — trace_array、function/function_graph、trace_pipe |
| 3 | [03-events.md](./03-events.md) | Trace Events — TRACE_EVENT 宏展开、perf 桥接 |

## 前置知识

- 了解 `/sys/kernel/tracing/` 的基本使用（`echo function > current_tracer`）
- 了解 `trace-cmd` 或 `perf ftrace` 用法
- 基础内核知识（list、mutex、seq_file）

## 架构总览

```
         /sys/kernel/tracing/
              │
    ┌─────────▼─────────┐
    │   trace.c          │   trace_array, trace_pipe, ring_buffer
    │   trace_output.c   │   用户态格式化输出
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │   ftrace.c         │   mcount 桩替换、动态修改函数入口
    │                    │   register_ftrace_function 注册回调
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │   trace_events.c   │   TRACE_EVENT 宏→tracepoint→ftrace
    │   (各子系统的     │
    │    tracepoint 定义) │
    └────────────────────┘
```
