#!/bin/bash
# 文件名：print_mcs_chain.sh

# 1. 地址->CPU编号
awk '/\[[0-9]+\]: ffff/ {match($0, /\[([0-9]+)\]: (ffff[0-9a-f]+)/, arr); if (arr[1] && arr[2]) print arr[2] " " arr[1];}' qnodes.log > addr2cpu.txt

# 2. CPU编号->next地址
awk '
/per_cpu\(qnodes, [0-9]+\)/ {match($0, /per_cpu\(qnodes, ([0-9]+)\)/, arr); cpu=arr[1];}
/next = 0x[0-9a-f]+/ {match($0, /next = 0x([0-9a-f]+)/, arr2); if (cpu != "" && arr2[1] != "0") print cpu " " arr2[1];}
' p_qndoes.log > cpu2next.txt

# 3. 所有被next指向的地址
awk '{print $2}' cpu2next.txt | sort | uniq > pointed_addr.txt

# 4. 所有链表头（未被任何 next 指向的节点）
awk '{print $1}' addr2cpu.txt | while read addr; do
    if ! grep -qi "$addr" pointed_addr.txt; then
        echo "$addr"
    fi
done > heads.txt

# 5. 遍历所有链表头，记录所有链表
chains=()
while read head_addr; do
    cpu=$(awk -v a="$head_addr" 'BEGIN{IGNORECASE=1} $1==a{print $2}' addr2cpu.txt)
    chain="CPU $cpu($head_addr)"
    visited=("$head_addr")
    cur_addr=$head_addr
    while true; do
    cur_cpu=$(awk -v a="$cur_addr" 'BEGIN{IGNORECASE=1} $1==a{print $2}' addr2cpu.txt)
    next_addr=$(awk -v c="$cur_cpu" '$1==c{print $2}' cpu2next.txt)
    if [ -z "$next_addr" ] || [ "$next_addr" == "0" ]; then
        last_cpu=$(awk -v a="$cur_addr" 'BEGIN{IGNORECASE=1} $1==a{print $2}' addr2cpu.txt)
        if [ -n "$last_cpu" ] && [ "$cur_addr" != "$head_addr" ]; then
            chain="$chain -> CPU $last_cpu($cur_addr)"
        fi
        break
    fi
    # next指向自己，直接break，不再append
    if [ "$next_addr" = "$cur_addr" ]; then
        break
    fi
    # 检查环路
    found_loop=0
    for v in "${visited[@]}"; do
        if [ "$v" = "$next_addr" ]; then
            next_cpu=$(awk -v a="$next_addr" 'BEGIN{IGNORECASE=1} $1==a{print $2}' addr2cpu.txt)
            if [ -z "$next_cpu" ]; then
                chain="$chain -> CPU ?($next_addr) [环路!]"
            else
                chain="$chain -> CPU $next_cpu($next_addr) [环路!]"
            fi
            found_loop=1
            break
        fi
    done
    if [ "$found_loop" -eq 1 ]; then
        break
    fi
    next_cpu=$(awk -v a="$next_addr" 'BEGIN{IGNORECASE=1} $1==a{print $2}' addr2cpu.txt)
    if [ -z "$next_cpu" ]; then
        chain="$chain -> CPU ?($next_addr)"
    else
        chain="$chain -> CPU $next_cpu($next_addr)"
    fi
    visited+=("$next_addr")
    cur_addr=$next_addr
done
    chains+=("$chain")
done < heads.txt

# 6. 只输出不被其它链表包含的链表，并用分隔线分隔
for i in "${!chains[@]}"; do
    is_sub=0
    for j in "${!chains[@]}"; do
        if [ "$i" -ne "$j" ] && [[ "${chains[$j]}" == *"${chains[$i]}"* ]]; then
            is_sub=1
            break
        fi
    done
    if [ "$is_sub" -eq 0 ]; then
        echo "${chains[$i]}" >> mcs_chain.txt
        echo "----------------------------------------" >> mcs_chain.txt
    fi
done

# 清理临时文件
rm -f addr2cpu.txt cpu2next.txt pointed_addr.txt heads.txt