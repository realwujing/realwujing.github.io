#!/bin/bash

# 安装必要的工具
function apt_install() {
  apt install linux-perf dstat sysstat iotop -y
}

# 实时报告系统的磁盘 I/O 统计信息
function iostat_record() {
  iostat_output_dir="/var/log/iostat"
  # Check if the output directory exists, create it if not
  if [ ! -d "$iostat_output_dir" ]; then
    echo "Creating directory: $iostat_output_dir"
    mkdir -p "$iostat_output_dir"
    chown $USER:$USER "$iostat_output_dir"
  fi

  # Run iostat command in the background and redirect output to iostat.log
  timestamp=$(date +%Y%m%d%H%M%S)
  iostat_full_command="iostat -txz 1 >> "$iostat_output_dir/iostat_$timestamp.log""
  echo "Running command: $iostat_full_command"
  (iostat -txz 1 >> "$iostat_output_dir/iostat_$timestamp.log" &)
}

# 找到 IO 占用高的进程
function pidstat_record() {
  pidstat_output_dir="/var/log/pidstat"
  # Check if the output directory exists, create it if not
  if [ ! -d "$pidstat_output_dir" ]; then
    echo "Creating directory: $pidstat_output_dir"
    mkdir -p "$pidstat_output_dir"
    chown $USER:$USER "$pidstat_output_dir"
  fi

  # Run pidstat command in the background and redirect output to pidstat.log
  timestamp=$(date +%Y%m%d%H%M%S)
  pidstat_full_command="pidstat -d 1 >> "$pidstat_output_dir/pidstat_$timestamp.log""
  echo "Running command: $pidstat_full_command"
  (pidstat -d 1 >> "$pidstat_output_dir/pidstat_$timestamp.log" &)
}

# 实时报告系统的进程资源占用
function top_record() {
  top_output_dir="/var/log/top"

  # Check if the output directory exists, create it if not
  if [ ! -d "$top_output_dir" ]; then
    echo "Creating directory: $top_output_dir"
    mkdir -p "$top_output_dir"
    chown $USER:$USER "$top_output_dir"
  fi

  # Run top command in the background and redirect output to top.log
  timestamp=$(date +%Y%m%d%H%M%S)
  top_full_command="top -d 5 -b >> "$top_output_dir/top_$timestamp.log""
  echo "Running command: $top_full_command"
  (top -d 5 -b >> "$top_output_dir/top_$timestamp.log" &)
}

# 实时报告系统的性能热点
function perf_record() {
  perf_output_dir="/var/log/perf"
  perf_path="${2:-/usr/bin/perf}"  # 设置默认值为 /usr/bin/perf
  duration="${3:-10}"

  # Check if the output directory exists, create it if not
  if [ ! -d "$perf_output_dir" ]; then
    echo "Creating directory: $perf_output_dir"
    mkdir -p "$perf_output_dir"
    chown $USER:$USER "$perf_output_dir"
  fi

  while true; do
    timestamp=$(date +%Y%m%d%H%M%S)
    perf_record_full_command="$perf_path record -F 999 -g -o $perf_output_dir/perf_$timestamp.data -- sleep $duration"
    echo "Running command: $perf_record_full_command"
    $perf_record_full_command
    # 在这里可以添加其他操作或休眠时间
    sleep 1
  done
}

# 启用所有记录函数
function start_record() {
  apt_install
  iostat_record
  pidstat_record
  top_record
  perf_record "$@"  # 传递所有参数给 perf_record 函数
}

# 停止所有记录函数
function stop_record() {
  # killall $0 iostat pidstat top
  ps aux | grep -Ew "$0|iostat|pidstat|top" | grep -v grep | awk {'print $2'} | xargs -n1 -I {} kill -9 {}
  
  # killall perf
  pidof $perf_path | xargs -n1 -I {} kill -2 {}
  pidof $perf_path | xargs -n1 -I {} kill -2 {}
}

# 检查参数，根据参数调用相应的函数
if [ "$1" == "start" ]; then
  start_record "$@"  # 传递所有参数给 start_record 函数
  echo "Recording started."
elif [ "$1" == "stop" ]; then
  stop_record
  echo "Recording stopped."
else
  echo "Usage: sudo $0 [start|stop]"
fi