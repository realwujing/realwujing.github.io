#!/bin/bash

# set -aex

# 将当前脚本的PID写入PID文件
echo $$ > /run/ksmd_taskset.pid

# 定义一个函数来查找CPU的兄弟核心（不包括自身）
function find_cpu_sibling() {
	local cpu=$1
	local siblings=$(cat "/sys/devices/system/cpu/cpu${cpu}/topology/thread_siblings_list" 2>/dev/null)
	if [ -n "$siblings" ]; then
		# 使用 awk 提取兄弟核心（去除当前 CPU）
		local filtered_sibs=$(echo "$siblings" | awk -v cpu="$cpu" -F, '{if ($1 == cpu) print $2; else print $0}')

		# 检查 filtered_sibs 是否为空
		if [ -z "$filtered_sibs" ]; then
			echo "No siblings found or no siblings other than current CPU."
		else
			echo "$filtered_sibs"
		fi
	else
		echo "No siblings information available."
	fi
}

# 定义一个函数来处理isolcpus参数，并构建smt_isolcpus列表
function handle_isolcpus() {
	local isolcpus=$1
	local cpus=()     # 用于存储唯一的CPU编号
	local smt_cpus=() # 用于存储包括兄弟核心在内的独特CPU编号

	# 处理逗号分隔的列表和范围，去重
	IFS=',' read -ra parts <<<"$isolcpus"
	for part in "${parts[@]}"; do
		if [[ "$part" =~ ^([0-9]+)-([0-9]+)$ ]]; then
			local start=${BASH_REMATCH[1]}
			local end=${BASH_REMATCH[2]}
			for ((i = $start; i <= $end; i++)); do
				if ! [[ " ${cpus[*]} " =~ " ${i} " ]]; then
					cpus+=("$i")
				fi
			done
		else
			if ! [[ " ${cpus[*]} " =~ " ${part} " ]]; then
				cpus+=("$part")
			fi
		fi
	done

	# 查找每个CPU的兄弟核心，将找到的超线程对添加到smt_cpus中
	for cpu in "${cpus[@]}"; do
		# 确保当前CPU也被添加到smt_cpus中
		if ! [[ " ${smt_cpus[*]} " =~ " ${cpu} " ]]; then
			smt_cpus+=("$cpu")
		fi

		# 查找当前CPU的兄弟核心
		sib=$(find_cpu_sibling "$cpu")
		if [[ -n "$sib" && "$sib" =~ ^[0-9]+$ ]]; then
			if ! [[ " ${smt_cpus[*]} " =~ " ${sib} " ]]; then
				smt_cpus+=("$sib")
			fi
		fi
	done

	# 对 smt_cpus 数组进行排序
	sorted_smt_cpus=($(printf '%s\n' "${smt_cpus[@]}" | sort -n))

	# 将排序后的 smt_cpus 数组转换为空格分隔的字符串
	IFS=' ' eval "smt_isolcpus=\"\${sorted_smt_cpus[*]}\""
	echo "$smt_isolcpus"
}

# 定义函数来计算不在 smt_isolcpus 列表中的 CPU，并以逗号分隔
function calculate_non_smt_isolcpus() {
	local logical_cpus=$1 # 总逻辑 CPU 数量
	local smt_isolcpus=$2 # smt_isolcpus 列表

	# 生成从 0 到 logical_cpus-1 的整数序列
	local cpu_range=$(seq 0 $((logical_cpus - 1)))

	# 使用数组来存储结果
	local non_smt_isolcpus=()

	# 遍历 cpu_range 中的每个 CPU
	for cpu in $cpu_range; do
		# 检查当前 CPU 是否在 smt_isolcpus 列表中
		if ! [[ " $smt_isolcpus " =~ " $cpu " ]]; then
			# 如果不在 smt_isolcpus 中，添加到 non_smt_isolcpus 数组
			non_smt_isolcpus+=("$cpu")
		fi
	done

	# 将 non_smt_isolcpus 数组转换为逗号分隔的字符串
	local non_smt_isolcpus_str=$(
		IFS=,
		echo "${non_smt_isolcpus[*]}"
	)

	# 输出结果
	echo "$non_smt_isolcpus_str"
}

# 定义函数来计算不在 smt_isolcpus 列表中的在线 CPU
function calculate_non_smt_isolated_online_cpus() {
	local smt_isolcpus_str=$1 # 从 calculate_non_smt_isolcpus 返回的字符串

	# 使用数组来存储结果
	local non_smt_isolated_online_cpus=()
	local non_smt_isolated_offline_cpus=()

	# 将输入字符串转换为数组
	IFS=, read -r -a cpus <<< "$smt_isolcpus_str"

	# 遍历 CPU 列表，检查每个 CPU 是否在线
	for cpu in "${cpus[@]}"; do
		if [ -f "/sys/devices/system/cpu/cpu${cpu}/online" ] && [ "$(cat /sys/devices/system/cpu/cpu${cpu}/online)" -eq 1 ]; then
			# 如果 CPU 在线，添加到 non_smt_isolated_online_cpus 数组
			non_smt_isolated_online_cpus+=("$cpu")
		else
			# 如果 CPU 不在线，添加到 non_smt_isolated_offline_cpus 数组
			non_smt_isolated_offline_cpus+=("$cpu")
		fi
	done

	# 输出离线的 CPU 列表，使用 >&2 将其输出到标准错误流，以避免干扰函数的实际返回值
	if [ ${#non_smt_isolated_offline_cpus[@]} -gt 0 ]; then
		echo "Offline non-SMT-isolated CPUs: $(IFS=,; echo "${non_smt_isolated_offline_cpus[*]}")" >&2
	fi

	# 将 non_smt_isolated_online_cpus 数组转换为逗号分隔的字符串
	local non_smt_isolated_online_cpus_str=$(
		IFS=,
		echo "${non_smt_isolated_online_cpus[*]}"
	)

	# 返回在线的 CPU 列表
	echo "$non_smt_isolated_online_cpus_str"
}

# 查找 ksmd 的 PID
ksmd_pid=$(ps -e | grep '[k]smd' | awk '$NF == "ksmd" {print $1}')

# 判断 ksmd_pid 是否为空
if [ -n "$ksmd_pid" ]; then
	echo "ksmd pid: $ksmd_pid"
	# ksmd_cpuid: ksmd当前所在cpu id
	ksmd_cpuid=$(ps -o cpuid -p $ksmd_pid | awk 'NR==2 {print $1}')
	echo "ksmd is running on cpu: $ksmd_cpuid"
	if [ -z "$ksmd_cpuid" ]; then
		# 处理没有第二行输出的情况（虽然在这个特定情况下不太可能）
		echo "ksmd_cpuid is null."
	else
		# logical_cpus: 总逻辑 CPU 数量
		logical_cpus=$(nproc --all)
		echo "logical_cpus: $logical_cpus"

		# isolcpus: 隔离的cpu核心
		isolcpus=$(grep -oP 'isolcpus=\K[^ ]+' /proc/cmdline)
		echo "args isolcpus= in /proc/cmdline: isolcpus=$isolcpus"

		smt_isolcpus=$(handle_isolcpus "$isolcpus")
		echo "smt_isolcpus: $smt_isolcpus"
		# 统计 smt_isolcpus 的计数
		smt_isolcpus_count=$(echo "$smt_isolcpus" | wc -w)
		# 打印计数
		echo "smt_isolcpus count: $smt_isolcpus_count"

		# 调用calculate_non_smt_isolcpus函数并获取结果
		non_smt_isolcpus=$(calculate_non_smt_isolcpus "$logical_cpus" "$smt_isolcpus")
		echo "non_smt_isolcpus: $non_smt_isolcpus"
		# 统计 non_smt_isolcpus 的计数
		non_smt_isolcpus_count=$(echo "$non_smt_isolcpus" | tr ',' ' ' | wc -w)
		# 打印计数
		echo "non_smt_isolcpus count: $non_smt_isolcpus_count"

		# 计算 Online non-SMT-isolated CPUs
		non_smt_isolated_online_cpus=$(calculate_non_smt_isolated_online_cpus "$non_smt_isolcpus")
		# 打印 Online non-SMT-isolated CPUs
		echo "Online non-SMT-isolated CPUs: $non_smt_isolated_online_cpus"

		# 验证计数和逻辑 CPU 数量是否一致
		if [ $(($smt_isolcpus_count + $non_smt_isolcpus_count)) -eq $logical_cpus ]; then
			# 判断 ksmd_cpuid 是否在 non_smt_isolated_online_cpus 列表中
			if ! echo "$non_smt_isolated_online_cpus" | tr ',' ' ' | grep -qw "$ksmd_cpuid"; then
				echo "ksmd is running on cpu $ksmd_cpuid, cpu $ksmd_cpuid is not in the non_smt_isolated_online_cpus list, need to migrate."

				# 选择 non_smt_isolated_online_cpus 列表中的最后一个核心进行迁移
				new_cpu=$(echo "$non_smt_isolated_online_cpus" | awk -F ',' '{print $NF}')
				if [ -n "$new_cpu" ]; then
					echo "migrating ksmd from cpu: $ksmd_cpuid to cpu: $new_cpu."
					command="taskset -pc $new_cpu $ksmd_pid"
					echo "command: $command"
					# 使用 eval 执行command字符串中的命令
					eval "$command"

					# 主动调度一次ksmd
					command="echo 1 > /sys/kernel/mm/ksm/run"
					echo "command: $command"
					# 使用 eval 执行command字符串中的命令
					eval "$command"

					# 进行循环检查，确保 ksmd 迁移完成
					while true; do
						ksmd_taskset_cpuid=$(ps -o cpuid -p $ksmd_pid | awk 'NR==2 {print $1}')
						if [ "$ksmd_taskset_cpuid" == "$new_cpu" ]; then

							# 再次计算 Online non-SMT-isolated CPUs，防止再次迁移前有cpu被offline
							non_smt_isolated_online_cpus=$(calculate_non_smt_isolated_online_cpus "$non_smt_isolcpus")
							# 打印 Online non-SMT-isolated CPUs
							echo "Online non-SMT-isolated CPUs: $non_smt_isolated_online_cpus"

							# 再次将ksmd迁移到 non_smt_isolated_online_cpus 列表中的核心
							echo "ksmd is running on cpu: $ksmd_taskset_cpuid, now migrating to cpu: $non_smt_isolated_online_cpus."
							command="taskset -pc $non_smt_isolated_online_cpus $ksmd_pid"
							echo "command: $command"
							# 使用 eval 执行command字符串中的命令
							eval "$command"

							# 主动调度一次ksmd
							command="echo 1 > /sys/kernel/mm/ksm/run"
							echo "command: $command"
							# 使用 eval 执行command字符串中的命令
							eval "$command"

							break
						else
							echo "ksmd is still running on cpu: $ksmd_cpuid, retrying migration..."
							sleep 5
						fi
					done
				else
					echo "No available CPUs in non_smt_isolated_online_cpus list for migration."
				fi
			else
				echo "ksmd is running on cpu: $ksmd_cpuid, cpu: $ksmd_cpuid is in the non_smt_isolated_online_cpus list. There's no need to migrate immediately. However, to prevent ksmd from running on isolated cores in the future, it's important to set CPU affinity."
				# 再次计算 Online non-SMT-isolated CPUs，防止再次迁移前有cpu被offline
				non_smt_isolated_online_cpus=$(calculate_non_smt_isolated_online_cpus "$non_smt_isolcpus")
				# 打印 Online non-SMT-isolated CPUs
				echo "Online non-SMT-isolated CPUs: $non_smt_isolated_online_cpus"

				# 设置ksmd亲和性到 non_smt_isolated_online_cpus 列表中的核心
				echo "ksmd is running on cpu: $ksmd_taskset_cpuid, now migrating to cpu: $non_smt_isolated_online_cpus."
				command="taskset -pc $non_smt_isolated_online_cpus $ksmd_pid"
				echo "command: $command"
				# 使用 eval 执行command字符串中的命令
				eval "$command"
			fi
		else
			echo "The total count of smt_isolcpus and non_smt_isolcpus is not equal to logical_cpus."
		fi
	fi
else
	echo "ksmd is not running."
fi

# 脚本执行完成后删除 PID 文件
rm -f /run/ksmd_taskset.pid
