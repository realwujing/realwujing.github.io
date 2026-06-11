# 第3篇：x86 PMU 驱动 — 硬件计数器与 NMI 采样

> 源码：`arch/x86/events/intel/core.c` `arch/x86/events/perf_event.h` | 头文件：`include/linux/perf_event.h`

---

## 1. PMU 驱动层次

```
                 perf_event_open 系统调用
                        │
            ┌───────────▼───────────┐
            │   kernel/events/      │  通用 perf_event 核心
            │   core.c              │  PMU 抽象，ring buffer，采样
            └───────────┬───────────┘
                        │ pmu->event_init / add / del / start / stop
            ┌───────────▼───────────┐
            │   arch/x86/events/    │  x86 PMU 驱动
            │   intel/core.c        │  Intel 特定：固定计数器、PEBS、LBR
            │   intel/uncore.c      │  Uncore：L3 cache，IMC，QPI
            │   amd/core.c          │  AMD 特定：IBS
            └───────────┬───────────┘
                        │ MSR read/write
            ┌───────────▼───────────┐
            │   x86 硬件 PMU        │
            │   IA32_PERFEVTSELx    │  事件选择器 MSR
            │   IA32_PMCx           │  性能计数器 MSR
            │   IA32_FIXED_CTRx     │  固定功能计数器
            └───────────────────────┘
```

---

## 2. x86 硬件 PMU 的两种计数器

### 2.1 一般用途计数器 (GP)

4-8 个 `IA32_PMCx` 寄存器（48 位），通过 `IA32_PERFEVTSELx` 配置：

| MSR | 地址 | 用途 |
|-----|------|------|
| IA32_PERFEVTSEL0 | 0x186 | 选择 PMC0 要计数的硬件事件 |
| IA32_PMC0 | 0xC1 | PMC0 的计数器值 |
| ... | | |
| IA32_PERFEVTSEL7 | 0x18D | 选择 PMC7 要计数的硬件事件 |
| IA32_PMC7 | 0xC8 | PMC7 的计数器值 |

`IA32_PERFEVTSELx` 的格式：
```
Bit:  31..24  23..16  15..8  7..0
    ┌────────┬────────┬──────┬───────────┐
    │        │        │      │  event    │
    │ CMASK  │ INV    │ umask│  select   │
    └────────┴────────┴──────┴───────────┘
```

### 2.2 固定功能计数器 (Fixed)

3 个 `IA32_FIXED_CTRx` MSR，不能重新配置，只计数固定的硬件事件：

| MSR | 固定计数的事件 |
|-----|-------------|
| IA32_FIXED_CTR0 | 指令执行数 (INST_RETIRED.ANY) |
| IA32_FIXED_CTR1 | CPU 周期数 (CPU_CLK_UNHALTED.THREAD) |
| IA32_FIXED_CTR2 | 参考周期数 (CPU_CLK_UNHALTED.REF_TSC) |

这些计数器通过 `IA32_FIXED_CTR_CTRL` MSR 控制启用/禁用以及是否在 ring 0/1/2/3 级别计数。

---

## 3. Intel PMU 驱动的核心方法

```c
// arch/x86/events/intel/core.c（结构体在 arch/x86/events/perf_event.h）

// ★ 全局 PMU 实例
struct pmu intel_pmu = {
    .event_init = intel_pmu_event_init,
    .add        = intel_pmu_add,
    .del        = intel_pmu_del,
    .start      = intel_pmu_start,
    .stop       = intel_pmu_stop,
    .read       = intel_pmu_read,
};

// ★ Intel 特定扩展
struct cpu_hw_events {
    struct perf_event *events[X86_PERF_MAX_COUNTERS];  // 每个计数器对应的事件
    unsigned long active_mask;                          // 已激活的事件位掩码
    unsigned long running;                              // 正在运行的事件位掩码
    unsigned long interrupted;                          // 已发生中断的事件位掩码

    int n_events;                                       // 活跃事件数
    int n_added;                                        // 本轮 add 的数量
    u64 pebs_enabled;                                   // PEBS 启用位掩码
    u64 pebs_data_cfg;                                  // PEBS 数据配置
};
```

### 3.1 event_init — 验证 attr 并分配资源

```c
// arch/x86/events/intel/core.c
static int intel_pmu_event_init(struct perf_event *event)
{
    // ① 验证 attr.type == PERF_TYPE_HARDWARE
    // ② 查找 x86 事件表：attr.config → 找到对应的 x86 事件描述符
    //    (event_code, umask, 哪些 CPU models 支持)
    // ③ 分配计数器（如果 attr.pinned，分配固定计数器）
    // ④ 验证采样条件（如 attr.sample_period 非零但 not supported）
    // ⑤ 返回 0 或错误
}
```

### 3.2 add — 分配硬件计数器

```c
// arch/x86/events/intel/core.c
static int intel_pmu_add(struct perf_event *event, int flags)
{
    // ① 找一个空闲的硬件计数器
    //    - 如果是 pinned 事件 → 优先分配固定功能计数器
    //    - 否则 → 分配 GP 计数器
    // ② 写 IA32_PERFEVTSELx：编码 event_code + umask + user/kernel ring + edge 等
    // ③ 如果 flag 里有 PERF_EF_START → 立即调用 intel_pmu_start
    // ④ cpuc->active_mask |= (1 << index)
}
```

### 3.3 del — 释放硬件计数器

```c
// arch/x86/events/intel/core.c
static void intel_pmu_del(struct perf_event *event, int flags)
{
    // ① 停用计数器（写 IA32_PERFEVTSELx 禁用位）
    // ② 保存当前计数值到 event->hw.prev_count → event->count 通过 pmu->read() 累计
    // ③ cpuc->active_mask &= ~(1 << index)
}
```

### 3.4 read — 读取硬件计数器

```c
// arch/x86/events/intel/core.c
static void intel_pmu_read(struct perf_event *event)
{
    // ① 读 IA32_PMCx（当前周期已计数值）
    // ② prev_count = new_count - prev_count
    // ③ local64_add(delta, &event->count)  ← 累计到 perf_event.count
}
```

---

## 4. NMI 采样 — 从溢出到 perf 记录

### 4.1 硬件中断

通用计数器到溢出（48-bit max）时，硬件产生 PMI（Performance Monitoring Interrupt）。PMI 被路由到 APIC 的 NMI 向量或专用性能中断向量。

### 4.2 中断处理

```c
// arch/x86/events/intel/core.c
// NMI handler
static int intel_pmu_handle_irq(struct pt_regs *regs)
{
    // ① 读 IA32_PERF_GLOBAL_STATUS MSR → 确定哪些计数器溢出了
    // ② 对每个溢出的计数器：
    //    a. 读取 PMC 当前值
    //    b. 将其重置为 sample_period（或者由 perf_event 的 auto-reload 逻辑重置）
    //    c. 调用 perf_sample_data_init 准备采样数据
    //    d. 调用 perf_event_overflow(event, data, regs)
    //         → perf_output_sample(event, data, regs)  ← 写入 ring buffer
    // ③ 写 IA32_PERF_GLOBAL_ACK MSR 清除溢出位
}
```

### 4.3 perf_event_overflow — 采样决策

```c
// kernel/events/core.c
int perf_event_overflow(struct perf_event *event,
                        struct perf_sample_data *data,
                        struct pt_regs *regs)
{
    // ① 检查 event_limit（是否已达最大采样数）
    // ② 检查 event->pending（是否已在处理中，同一 event 不重入）
    // ③ 调用 __perf_event_overflow → perf_output_sample(event, data, regs)
    //    → 构造 PERF_RECORD_SAMPLE 记录
    //    → 包含: IP(x86:regs->ip)、PID、TID、时间戳、调用链
    //    → 写入 ring buffer
    // ④ 调用 perf_event_update_userpage → 推进 data_head（用户态可见）
    // ⑤ 如果用户态需要唤醒：irq_work_queue(&event->pending_irq) → 唤醒 poll/epoll
}
```

---

## 5. Intel 处理器的事件表

Intel 为每个 CPU Generation 发布一个 JSON 事件表（`tools/perf/pmu-events/`），内核只编码核心硬件机制。事件由 `perf_event_attr.config` 的 32-bit 原始值编码：

```
config = (umask << 8) | event_code
```

例如，`BRANCH_INSTRUCTIONS_RETIRED` 在 Skylake 上是：
```
Event Code = 0xC4
UMask = 0x00
config = 0x00C4
```

用户态 `perf stat -e branch-instructions` → `perf_event_open(attr.config=0x00C4)`。

---

## 6. 与 NMI watchdog 的关系

Hard lockup detector 的 Perf 路径也使用了 x86 的 NMI 计数器溢出（见 `lockup-deep-dive/03-hard-lockup-perf.md`）。但两者使用**不同**的计数器：

- **perf tool**：使用 `IA32_PERFEVTSELx` + `IA32_PMCx`（一般计数器）
- **NMI watchdog**：也使用 `IA32_PERFEVTSELx` + `IA32_PMCx` 但通过 `watchdog_perf.c` 注册为独立的 perf_event

两者通过 perf_event 的 PMU 资源管理共享硬件寄存器——perf 对用户分配计数器，NMI watchdog 是内核自己独占一个计数器。

---

## 7. 总结

| 层级 | 组件 | 核心作用 |
|------|------|---------|
| 用户态 | `perf stat -e cycles` | select hardware counter → perf_event_open attr.config=0x003C |
| perf 核心 | `perf_event_overflow` | 调用 PMU 的 NMI handler |
| x86 通用 | `intel_pmu_handle_irq` | 读 GLOBAL_STATUS MSR → 每个溢出的 counter 处理 |
| x86 寄存器 | `IA32_PMCx` MSR | 48-bit 计数器，自动递增直到溢出 |
| 中断路由 | APIC PMI → NMI 向量 | 处理器内部信号 → 按 NMI 协议进入内核 |
