# 第6篇：BPF 程序运行 — BPF_PROG_TYPE_KPROBE/TRACEPOINT/PERF_EVENT 与 bpf_trace.c

> 源码：`kernel/trace/bpf_trace.c` `net/core/filter.c` | 头文件：`include/uapi/linux/bpf.h`

系列目录：[eBPF 内核源码深度解析](./README.md)

---

## 1. BPF 程序的三类内核 entry point

| prog_type | 触发点 | 上下文 | 典型 helper |
|-----------|--------|--------|------------|
| `BPF_PROG_TYPE_KPROBE` | kprobe 断点 | 任意内核上下文 | `bpf_get_current_pid_tgid`, `bpf_get_current_comm` |
| `BPF_PROG_TYPE_TRACEPOINT` | tracepoint 埋点 | 触发 tracepoint 的上下文 | `bpf_get_smp_processor_id`, `bpf_get_current_cgroup_id` |
| `BPF_PROG_TYPE_PERF_EVENT` | perf_event 溢出 | NMI 上下文 | **只允许 map 操作**（不能再调度） |

### 1.1 BPF_PROG_TYPE_KPROBE 的注册

```c
// kernel/trace/bpf_trace.c
// Kprobe BPF 程序的 verifier 批准的 helper 集合:
bpf_kprobe_multi_func_proto(enum bpf_func_id func_id, const struct bpf_prog *prog)
{
    switch (func_id) {
    case BPF_FUNC_get_current_pid_tgid:         // 获取当前 PID/TID
    case BPF_FUNC_get_current_uid_gid:          // 获取 UID/GID
    case BPF_FUNC_get_current_comm:             // 获取当前进程名
    case BPF_FUNC_perf_event_output:            // 向 perf_event 发送输出
    case BPF_FUNC_get_stackid:                  // 获取栈 ID
    case BPF_FUNC_get_stack:                    // 获取用户/内核栈
    case BPF_FUNC_ktime_get_ns:                 // 获取当前时间
    case BPF_FUNC_trace_printk:                 // 向 ftrace ring buffer 写日志
    case BPF_FUNC_map_lookup_elem:              // map 查找
    case BPF_FUNC_map_update_elem:              // map 更新
    // ... 40+ helpers ...
    }
}
```

### 1.2 kprobe_multi —— 一个 BPF 程序同时探测多个函数

```c
// kernel/trace/bpf_trace.c:1281-1303
static bool is_kprobe_multi(const struct bpf_prog *prog)
{
    return prog->type == BPF_PROG_TYPE_KPROBE &&
           (prog->expected_attach_type == BPF_TRACE_KPROBE_MULTI ||
            prog->expected_attach_type == BPF_TRACE_KPROBE_SESSION);
}

// BPF_TRACE_KPROBE_MULTI:
// 一个 BPF 程序附加到多个内核函数，例如：
// bpftrace -e 'kprobe:tcp_* { @bytes = count(); }'
// 自动探测所有 tcp_ 前缀的函数
```

### 1.3 kprobe_session —— 入口+出口对

```c
// kernel/trace/bpf_trace.c:1287-1291
static bool is_kprobe_session(const struct bpf_prog *prog)
{
    return prog->type == BPF_PROG_TYPE_KPROBE &&
           prog->expected_attach_type == BPF_TRACE_KPROBE_SESSION;
}

// BPF_TRACE_KPROBE_SESSION:
// 一个 BPF 程序在函数入口执行，另一个在出口执行。
// 两个程序共享同一个 map，实现"entry 记录开始 → exit 计算耗时"。
```

---

## 2. BPF_PROG_TYPE_TRACEPOINT

```c
// kernel/trace/bpf_trace.c
bpf_tracing_func_proto(enum bpf_func_id func_id, const struct bpf_prog *prog)
{
    switch (func_id) {
    case BPF_FUNC_map_lookup_elem:
    case BPF_FUNC_map_update_elem:
    case BPF_FUNC_map_delete_elem:
    case BPF_FUNC_map_push_elem:
    case BPF_FUNC_map_pop_elem:
    case BPF_FUNC_map_peek_elem:
    case BPF_FUNC_get_current_pid_tgid:
    case BPF_FUNC_get_current_uid_gid:
    // ...
    }
}
```

tracepoint BPF 程序能读取 tracepoint 的 `ctx` 参数。例如 `tracepoint:net:netif_receive_skb` — BPF 程序可以直接访问 `skb` 指针。

---

## 3. BPF_PROG_TYPE_PERF_EVENT

```c
// Perf_event BPF 程序只有*非常受限*的 helper 集合:
// 原因是 perf event 在 NMI 上下文中运行，不能睡眠、不能调度、不能拿 mutex

bpf_perf_event_func_proto(enum bpf_func_id func_id, ...)
{
    // 允许: map_lookup_elem, map_update_elem, perf_event_output
    // 禁止: get_current_pid_tgid, kmalloc, spin_lock, 等
}
```

**典型用途**：Netflix 的 flame graph 工具——BPF 程序在 `perf stat -e cycles` 的每个采样中运行，聚合到 map 中，用户态定期 dump map。

---

## 4. BPF_PROG_TYPE_XDP 的 helper

```c
// net/core/filter.c:3992
BPF_CALL_2(bpf_xdp_adjust_head, struct xdp_buff *, xdp, int, offset)
{
    // 移动 xdp->data 指针：允许 BPF 程序在包前添加/移除头部
    // 支持 IPIP/GRE 解封装、VLAN 标签操作等
}

// net/core/filter.c:4668
BPF_CALL_3(bpf_xdp_redirect_map, struct bpf_map *, map, u64, key, u64, flags)
{
    // 将 XDP 处理后的包重定向到其他网卡或 CPU
    // map 类型: BPF_MAP_TYPE_XSKMAP, BPF_MAP_TYPE_DEVMAP, BPF_MAP_TYPE_CPUMAP
}

// XDP 程序可用的其他 helper:
// bpf_xdp_adjust_tail — 在包尾添加/移除数据
// bpf_xdp_redirect — 重定向到单个网卡
// bpf_xdp_get_buff_len — 获取包长度
// bpf_xdp_load_bytes / bpf_xdp_store_bytes — 安全地读写包数据
```

---

## 5. BPF 程序的完整生命周期

```
用户态: bpftrace -e 'kprobe:do_sys_open { printf("%s\n", str(args->filename)); }'

  → bpftrace 解析脚本
  → 生成 BPF bytecode (LLVM/BPF CO-RE)
  → bpf(BPF_PROG_LOAD, prog_type=BPF_PROG_TYPE_KPROBE, ...)
    → bpf_prog_load
      → bpf_check (verifier)
        → 检查所有 helper 是否允许在这个 BPF_PROG_TYPE 中使用
        → 追踪所有寄存器和栈
      → bpf_prog_select_runtime (JIT)
    → 返回 fd

  → perf_event_open(PERF_TYPE_KPROBE, kprobe_func="do_sys_open")
    → register_kprobe → int3 写入

内核中 do_sys_open 调用:
  → int3 → kprobe_int3_handler
    → aggr_pre_handler (遍历所有附加的 BPF 程序)
      → bpf_prog_run(prog, ctx)
        → BPF 已 JIT 编译(直接跳转) → x86 原生代码 → 写入 map
        → 或 解释器执行(无 JIT)
    → 单步执行原指令
```

---

## 6. BPF 与 perf/ftrace/tracepoint 的关系

```
        ┌──────────────────────────────────────────────┐
        │              探测点                            │
        │  int3 (kprobe/uprobe) / tracepoint 调用        │
        └──────────────────┬───────────────────────────┘
                           │
        ┌──────────────────┼───────────────────────────┐
        │                  │                           │
        ▼                  ▼                           ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐
│   ftrace      │  │    perf       │  │        BPF            │
│               │  │               │  │                       │
│ trace_kprobe  │  │ perf_kprobe_  │  │ BPF_PROG_TYPE_KPROBE  │
│ trace_uprobe  │  │ init          │  │                       │
│               │  │ perf_uprobe_  │  │ bpf_trace_kfuncs_init │
│ 通过 trace_   │  │ init          │  │ (注册允许的 helper)    │
│ probe_register│  │               │  │                       │
│ 回调          │  │ 通过 perf_trace│  │ 通过 bpf_prog_run     │
│               │  │ _event_init   │  │ 回调                  │
└───────────────┘  └───────────────┘  └───────────────────────┘
        │                  │                    │
        └──────────────────┼────────────────────┘
                           │
                           ▼
                内核基础: register_kprobe() + kprobe_int3_handler()
```

三者共享相同的底层探测机制（kprobe/uprobe/tracepoint），但在上层提供不同的用户接口和数据处理路径。

## 系列结语

6 篇文章从 `struct perf_event` 和系统调用起步，到 ring buffer 的零拷贝 mmap 轮询，再到 x86 Intel PMU 驱动 NMI 采样，再深入到 tracepoint/ftrace 桥接，最后覆盖 kprobe/uprobe 两大动态探测机制，完整覆盖了内核 perf_event 子系统和 BPF 跟踪程序的实现。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-perf-event | struct perf_event + syscall | `include/linux/perf_event.h:765`, `core.c:13844` |
| 02-ring-buffer | mmap + 零拷贝 | `core.c:6825`, `ring_buffer.c` |
| 03-pmu | x86 PMU + NMI | `arch/x86/events/intel/core.c` |
| 04-tooling | tracepoint/ftrace 集成 | `trace_event_perf.c` |
| 05-kprobe-uprobe | kprobe/uprobe 探测 + perf bridge | `kprobes.c:1708`, `uprobes.c:1390`, `trace_event_perf.c:247` |
| 06-bpf-progs (本篇) | BPF 程序运行 | `bpf_trace.c`, `filter.c` |
