#!/bin/sh

#最大等待30s,此时设备一定已加载出来
LOOP_CNT=30

#echo $1 >> /root/hello.txt
sleep 10
/sbin/bcache register /dev/$1

for ((i=1; i<=LOOP_CNT; i++)); do 
 sleep 1;
 #echo "check /sys/class/block/$1/bcache" >> /root/hello.txt
 if [ -d /sys/class/block/$1/bcache ]; then
  if [ -d /sys/class/block/$1/bcache/set ];then

   #开始做cache的配置
   if [ -e /sys/class/block/$1/bcache/set/congested_read_threshold_us ]; then
    echo 0 > /sys/class/block/$1/bcache/set/congested_read_threshold_us
   fi

   if [ -e /sys/class/block/$1/bcache/set/congested_write_threshold_us ]; then
    echo 0 > /sys/class/block/$1/bcache/set/congested_write_threshold_us
   fi

   break;
   
  else
   #后端设备使用其他事件配置
   break;
  fi
 fi
done