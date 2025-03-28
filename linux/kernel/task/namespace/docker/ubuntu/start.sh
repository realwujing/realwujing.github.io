#!/bin/bash

echo '1. starting mysql...'

echo "2988" | sudo -S service mysql start

echo '2. starting ssh...'
echo "2988" | sudo -S service ssh start

# 开机启动服务成功
echo $(date "+%Y-%m-%d %H:%M:%S") start success... >&1 | tee -a /usr/local/start.log

tail -f /dev/null