#!/bin/bash
# 文件名：find_duplicate_next_cpu_fixed.sh

# 1. 找出被多个节点指向的 next 地址（去掉0x前缀）
grep -oP 'next = 0x[0-9a-f]+' p_qndoes.log | grep -v 'next = 0x0' | awk '{print $3}' | sed 's/^0x//' | sort | uniq -c | awk '$1 > 1 {print $2}' > dup_next.txt

# 2. 建立地址到CPU编号的映射表
awk '/\[[0-9]+\]: ffff/ {
    match($0, /\[([0-9]+)\]: (ffff[0-9a-f]+)/, arr);
    if (arr[1] && arr[2]) print arr[2] " " arr[1];
}' qnodes.log > addr2cpu.txt

# 3. 查找每个重复next对应的CPU编号
while read addr; do
    cpu=$(grep -i "^$addr " addr2cpu.txt | awk '{print $2}')
    if [ -n "$cpu" ]; then
        echo "$addr 对应的CPU编号: $cpu" >> dup_next_cpu.txt
    else
        echo "$addr 未找到对应CPU"
    fi
done < dup_next.txt