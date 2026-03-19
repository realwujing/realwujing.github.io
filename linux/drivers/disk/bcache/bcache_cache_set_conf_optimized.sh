#!/bin/sh

# 定义全局锁文件，用于串行化 bcache 操作
LOCKFILE="/run/lock/bcache_setup.lock"

# 1. 预检：如果已经注册过，直接跳到配置阶段
if [ ! -d "/sys/class/block/$1/bcache" ]; then
  # 使用 flock 确保同一时间只有一个脚本在执行 register 动作
  (
    flock -x -w 600 200 || exit 1
    # 注册设备 (bcache register 会阻塞直到内核操作完成)
    /sbin/bcache register /dev/"$1"
  ) 200>"$LOCKFILE"
fi

# 2. 异步等待设备就绪并进行配置 (在锁外执行，避免排队超时)
# 最大等待 30s 确认设备就绪
LOOP_CNT=30
for ((i=1; i<=LOOP_CNT; i++)); do 
  if [ -d /sys/class/block/"$1"/bcache/set ]; then
    
    # 只有在值不为 0 时才写入，减少无效的持锁动作
    CONF_DIR="/sys/class/block/$1/bcache/set"
    
    # 配置 congested_read_threshold_us
    READ_ATTR="$CONF_DIR/congested_read_threshold_us"
    if [ -e "$READ_ATTR" ] && [ "$(cat "$READ_ATTR")" != "0" ]; then
      echo 0 > "$READ_ATTR"
    fi

    # 配置 congested_write_threshold_us
    WRITE_ATTR="$CONF_DIR/congested_write_threshold_us"
    if [ -e "$WRITE_ATTR" ] && [ "$(cat "$WRITE_ATTR")" != "0" ]; then
      echo 0 > "$WRITE_ATTR"
    fi

    break
  fi
  sleep 1
done

