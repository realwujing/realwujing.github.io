# 第4篇：tracepoint、ftrace 集成与 perf 工具链

> 源码：`kernel/events/core.c` `kernel/trace/trace_event_perf.c` `kernel/trace/` | 头文件：`include/trace/perf.h`

---

## 1. tracepoint 到 perf_event 的桥接

tracepoint 是内核中的静态埋点（`TRACE_EVENT` 宏）。用户态可以通过 perf 订阅这些事件，每个事件触发时 perf_event 写入 ring buffer。

### 1.1 tracepoint 的 perf 注册路径

```
用户态: perf_event_open(attr.type = PERF_TYPE_TRACEPOINT, attr.config = TRACEPOINT_ID)

  → perf_init_event → 通过 PERF_TYPE_TRACEPOINT 找到 perf_tracepoint_pmu

  → perf_tp_event_init(event)
    → 根据 attr.config 查找对应的 tracepoint (通过 tracepoint id)
    → 安装 perf 到 tracepoint 的 perf_probe 列表

每一触发的 tracepoint（例如 sched_switch）:
  → trace_sched_switch(...) in kernel source
    → perf_trace_run_bpf_submit(entry, count, rctx, regs, head, task)
      → trace_event_buffer_reserve → allocate ring buffer space
      → perf_trace_buf_submit → output tracepoint data to perf buffer
        → perf_output_sample(event, data) → per-event ring buffer write
```

### 1.2 核心桥接代码

```c
// kernel/trace/trace_event_perf.c
int perf_trace_init(struct perf_event *event)
{
    // ① 通过 event->attr.config 找到 tracepoint id
    // ② 获取对应的 tracepoint 结构体
    // ③ 注册 perf_event 到 tracepoint 的 probe 列表
    //    → tp->probes[TP_PROBE_PERF] = event
}

// kernel/trace/trace_event_perf.c
void perf_trace_run_bpf_submit(void *raw_data, int size, int rctx,
                               struct trace_event_call *call, u64 count,
                               struct pt_regs *regs, struct hlist_head *head,
                               struct task_struct *task)
{
    // ① 预留 ring buffer 空间
    // ② 填充 perf 的 sample data（IP, PID, timestamp, callchain）
    // ③ 调用 perf_output_sample(event, data, regs)
}
```

**关键**: `perf record -e sched:sched_switch` 可以捕获每次 task switch 的调用信息，并且后续用 `perf report` 分析。

---

## 2. 与 ftrace 的集成

perf 使用 ftrace 的函数图追踪作为采样基础：

```
perf record -g (采集调用链):
  → perf_event_open + 采样
    → NMI 触发时读取 pt_regs->ip（当前 IP）
    → 调用链回溯: perf_callchain_kernel(regs)
      → unwind_stack → 读取栈帧上的返回地址
    → 调用链写入 ring buffer 的 PERF_RECORD_SAMPLE 记录
    → 用户态 perf report 构建火焰图

perf ftrace (perf 内部用 ftrace 做函数级采样):
  → echo "function_graph" > /sys/kernel/tracing/current_tracer
  → ftrace 回调 → perf 桥接 → 写入 perf ring buffer
```

### 2.1 perf probe — 动态插入 tracepoint

```
perf probe -a 'myprobe do_sys_open filename=%x0:%si'
  → 通过 debugfs/sysfs 创建一个 kprobe
  → 注册到 tracepoint 系统
  → 表现为一个新的 perf 事件: perf_event_open(attr.type = PERF_TYPE_TRACEPOINT, ...)
```

---

## 3. bpf 的叠加

内核中等价于 `perf_event_open(attr.type = BPF_PROG_TYPE_PERF_EVENT)` 的路径允许 BPF 程序在 perf event 采样时被触发：

```
硬件 PMU 采样（NMI）→ intel_pmu_handle_irq
  → perf_event_overflow
    → 如果有附加的 BPF 程序: bpf_overflow_handler
      → BPF 程序运行在 NMI 上下文
      → 可以读取 PMC 值、更新 map、发送 ring buffer 到用户态
    → perf_output_sample (标准 perf 采样)
```

这使得 BPF 能够在硬件性能事件发生时做**实时**计算，而不只是事后分析——这是 Netflix/BPF 性能工具的核心。

---

## 4. 文件系统接口

| 路径 | 用途 |
|------|------|
| `/sys/bus/event_source/devices/` | 所有已注册的 PMU 目录 |
| `/sys/bus/event_source/devices/cpu/type` | PERF_TYPE_HARDWARE = 0 |
| `/sys/bus/event_source/devices/cpu/events/` | 列出所有可采样的硬件事件 |
| `/sys/bus/event_source/devices/cpu/format/` | attr.config 的编码格式说明 |
| `/sys/kernel/tracing/events/` | 所有 tracepoint 和 kprobes |
| `/sys/kernel/tracing/events/sched/sched_switch/id` | 该 tracepoint 的 perf_event_open config 值 |
| `/sys/kernel/debug/tracing/per_cpu/cpu0/` | ftrace 的每 CPU 输出 |

---

## 5. perf record 完整数据流

```
① 创建 perf_event
  perf_event_open(attr.type=HARDWARE, attr.config=cpu_cycles, attr.sample_period=10000)
    → struct perf_event 创建
    → fd 返回给用户态

② 启用计数
  ioctl(fd, PERF_EVENT_IOC_ENABLE)
    → pmu->add(event) → 选择硬件计数器

③ mmap ring buffer
  mmap(fd, 0, (1 + 2^n) * PAGE_SIZE, ...)
    → 映射 struct perf_event_mmap_page + data pages

④ 等待采样 (poll)
  poll(&fds, 1, -1, ...)
    → 每 10000 个 CPU 周期 NMI 触发
      → perf_event_overflow → perf_output_sample → ring buffer 写入
      → data_head 推进
      → perf_event_update_userpage → userpg->data_head = rb->head
    → irq_work → wake_up(poll_wait) → poll() 返回

⑤ 读取采样
  while (userpg->data_tail != userpg->data_head) {
    read sample record (IP, PID, timestamp, callchain)
    → write to perf.data file
    userpg->data_tail = new_tail
  }
```

---

## 6. 总结 — 四条路径对比

| 路径 | 触发源 | 写到哪里 | 谁消费 |
|------|--------|---------|--------|
| **perf stat** | 无 (counter only) | event->count | 用户态 read(fd) |
| **perf record** | PMU NMI 溢出 | ring buffer | mmap 轮询 |
| **perf tracepoint** | 内核 tracepoint 调用 | 每事件 ring buffer | mmap 轮询 |
| **perf + bpf** | PMU NMI 溢出 | bpf output ring buffer | bpf 用户态读取 |

---

## 系列结语

4 篇文章从 `struct perf_event` 和 `perf_event_open` 系统调用起步，深入到 Ring Buffer 的零系统调用 mmap 轮询和 `perf_output_begin/end` 写入路径，再到 x86 Intel PMU 驱动的硬件计数器分配和 NMI 采样处理，最后覆盖 perf 与 ftrace、BPF 的交互以及完整的 `perf record` 数据流。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-perf-event | struct perf_event + syscall | `include/linux/perf_event.h:765`, `core.c:13844` |
| 02-ring-buffer | mmap + 用户态零拷贝 | `core.c:6825`, `ring_buffer.c` |
| 03-pmu | x86 PMU 驱动 + NMI 采样 | `arch/x86/events/intel/core.c` |
| 04-tooling | tracepoint/ftrace/bpf 集成 | `kernel/trace/trace_event_perf.c` |
