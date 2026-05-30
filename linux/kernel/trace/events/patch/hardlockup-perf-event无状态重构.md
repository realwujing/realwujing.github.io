# watchdog/hardlockup: simplify perf event probe and remove per-cpu dependency

## 问题背景

hardlockup 检测器通过 per-CPU 的 perf event 来监测 CPU 是否正常响应 NMI。原有实现中，`hardlockup_detector_event_create()` 直接在内部操作 per-cpu 变量 `watchdog_ev`，有以下问题：

1. **隐式 per-cpu 依赖**：函数假定调用者在目标 CPU 上运行（`WARN_ON(!is_percpu_thread())`），但探针路径 (probe) 并不一定满足此约束
2. **任务迁移风险**：如果创建 perf event 期间发生任务迁移，`watchdog_ev` 可能指向错误的 CPU 或留下悬空指针
3. **probe 路径不安全**：`watchdog_hardlockup_probe()` 创建完 event 后用 `perf_event_release_kernel()` 释放，但通过 `this_cpu_read(watchdog_ev)` 获取指针——如果期间发生 CPU 迁移，将读取其他 CPU 的变量

## 优化方案

将 `hardlockup_detector_event_create()` 重构为**无状态函数**：

```c
// 优化前：返回 int，直接修改 per-cpu watchdog_ev
static int hardlockup_detector_event_create(void);

// 优化后：返回 perf_event*，由调用者管理生命周期
static struct perf_event *hardlockup_detector_event_create(unsigned int cpu);
```

**`watchdog_hardlockup_enable()` 改进：**
- 接收 `evt` 返回值，检查 `IS_ERR(evt)` 后再执行后续操作
- 不需要 `is_percpu_thread()` 断言

**`watchdog_hardlockup_probe()` 改进：**
- 创建临时 perf event，立即释放，不经过 `watchdog_ev` 中转
- 消除 CPU 迁移导致的竞态

## 代码变更

| 文件 | 变更 |
|---|---|
| `kernel/watchdog_perf.c` | 重构 `hardlockup_detector_event_create()` 返回指针；简化 `watchdog_hardlockup_enable()` 和 `watchdog_hardlockup_probe()` 的调用逻辑 |

## 上游状态

- **Lore**: https://lkml.kernel.org/r/20260129022629.2201331-1-realwujing@gmail.com
- **合入**: torvalds/master `0dddf20b4fd4`
- **合入者**: Andrew Morton <akpm@linux-foundation.org>
- **Reviewed-by**: Douglas Anderson <dianders@chromium.org>