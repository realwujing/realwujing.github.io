#!/bin/bash
# 针对 text_poke_bp_batch INT3 竞态的确定性压测复现脚本。
# 目标：在带 INT3-DEBUG 调试代码的内核上稳定砸开那个窄窗口。
#
# 竞态三要素，本脚本同时拉满：
#   1) text_poke churn —— 疯狂开关 ftrace（尤其「关闭」CALL->NOP 是历史崩溃路径）
#   2) 跨 CPU 执行被 patch 的热函数 —— up_read/schedule/缺页 等
#   3) VM 时序放大 —— 建议宿主把 vCPU 超分（overcommit）以放大 VM-exit/IPI 延迟
#
# 用法：在 VM 里 root 执行： ./stress_text_poke.sh
# 崩溃时串口会先打印 INT3-DEBUG: ... path=<A|B|C> 再 die("int3")。

set -u
TRACE=/sys/kernel/debug/tracing
[ -d "$TRACE" ] || TRACE=/sys/kernel/tracing
NCPU=$(nproc)

echo "[*] ncpu=$NCPU tracefs=$TRACE"
cleanup() { kill $(jobs -p) 2>/dev/null; echo nop > "$TRACE/current_tracer" 2>/dev/null; }
trap cleanup EXIT INT TERM

# (1) 最猛的 text_poke 来源：反复全量开/关 function tracer
#     每次切换都触发 ftrace_replace_code -> 成百上千个 text_poke_bp_batch
( while :; do
    echo function       > "$TRACE/current_tracer"
    echo nop            > "$TRACE/current_tracer"
    echo function_graph > "$TRACE/current_tracer"
    echo nop            > "$TRACE/current_tracer"
  done ) &

# (2) 再叠一层：开关单个 kprobe / ftrace filter，制造小批次（更易踩批次边界 path B）
( while :; do
    echo 'schedule'              > "$TRACE/set_ftrace_filter" 2>/dev/null
    echo function                > "$TRACE/current_tracer"
    echo 'up_read'               > "$TRACE/set_ftrace_filter" 2>/dev/null
    echo 'exclusived_task_running' >> "$TRACE/set_ftrace_filter" 2>/dev/null
    echo nop                     > "$TRACE/current_tracer"
    echo                         > "$TRACE/set_ftrace_filter" 2>/dev/null
  done ) &

# (3) 每个 CPU 上猛跑会命中被 patch 热函数的负载：
#     - 大量缺页 + mmap_lock（up_read 路径，dmesg1.txt 崩这里）
#     - 大量调度切换（schedule/exclusived_task_running，dmesg.txt 崩这里）
for c in $(seq 0 $((NCPU-1))); do
  # 缺页 / mmap 风暴
  taskset -c "$c" bash -c '
    while :; do
      for i in $(seq 1 200); do
        cat /proc/self/maps >/dev/null 2>&1
        : $(</proc/self/stat)
      done
    done' &
  # 调度风暴
  taskset -c "$c" bash -c 'while :; do :; done' &
done

# (4) perf ftrace 反复注册/注销（命中原始 do_exit->perf_trace_destroy 路径）
if command -v perf >/dev/null 2>&1; then
  ( while :; do
      timeout 1 perf record -e cycles -a -g -- sleep 0.5 >/dev/null 2>&1
      timeout 1 perf ftrace ls >/dev/null 2>&1
    done ) &
fi

echo "[*] 压测运行中。崩溃会出现在串口：INT3-DEBUG: ... path=<A|B|C>，随后 int3: 0000 / die。"
echo "[*] Ctrl-C 停止。建议至少连续跑 30~60 分钟；宿主 vCPU 超分能显著提高命中率。"
wait
