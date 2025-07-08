#!/bin/bash
# 文件名：find_unused_qnodes.sh

# 先清理旧结果
rm -f used_addr.txt unused_qnodes.txt

# 1. 提取 p_qndoes.log 中所有出现过的地址
grep -oE 'ffff[0-9a-f]{12}' p_qndoes.log > used_addr.txt

# 2. 遍历 qnodes.log 里的地址和CPU编号，输出未被指向的
grep -oP '\[\d+\]: ffff[0-9a-f]+' qnodes.log | while read line; do
    cpu=$(echo "$line" | grep -oP '\[\d+\]' | tr -d '[]')
    addr=$(echo "$line" | grep -oP 'ffff[0-9a-f]+')
    if ! grep -q "$addr" used_addr.txt; then
        echo "CPU $cpu: $addr" >> unused_qnodes.txt
    fi
done

# 3. 打印结果
cat unused_qnodes.txt