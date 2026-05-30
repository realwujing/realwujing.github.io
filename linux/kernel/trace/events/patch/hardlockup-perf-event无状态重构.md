# 内核看门狗 上游补丁

| # | 提交日期 | Subject | Patch |
|---|---------|---------|-------|
| 1 | 2026-01-28 | watchdog/hardlockup: simplify perf event probe and remove per-cpu dependency | [0001-watchdog-hardlockup-simplify-perf-event-probe-and-re.patch](./0001-watchdog-hardlockup-simplify-perf-event-probe-and-re.patch) |

### 详情

简化 hardlockup 检测器的 probe 路径，移除对固定 per-cpu 执行的隐式依赖。重构 `hardlockup_detector_event_create()` 为无状态函数，将创建的 perf_event 指针返回给调用者，而非直接修改 per-cpu 变量 `watchdog_ev`，避免任务迁移时留下悬空指针。

Signed-off-by: Shouxin Sun <sunshx@chinatelecom.cn>
Signed-off-by: Junnan Zhang <zhangjn11@chinatelecom.cn>
Signed-off-by: Qiliang Yuan <realwujing@gmail.com>
Reviewed-by: Douglas Anderson <dianders@chromium.org>
Merged by: Andrew Morton <akpm@linux-foundation.org>
