#!/bin/bash

# 定义补丁目录变量
PATCH_DIR="../patchs"

# 生成补丁文件到指定目录
git log --oneline --reverse --author="^realwujing" | awk '{print $1}' | xargs -I {} git format-patch -1 {} -o "$PATCH_DIR"

# 切换到补丁目录
cd "$PATCH_DIR" || { echo "Directory $PATCH_DIR does not exist"; exit 1; }

# 计数器初始化
i=1

# 遍历所有 .patch 文件，按照当前目录中的排序方式处理
for patch in $(ls -lt | tac | awk '{print $NF}' | grep '\.patch$'); do
    # 使用 sed 删除原有的序号部分
    base_name=$(echo "$patch" | sed 's/^[0-9]\{4\}-//')

    # 使用 printf 格式化为四位数字，不足前面补零
    new_name=$(printf "%04d-%s" $i "$base_name")

    # 显示原始文件名和新的文件名
    echo "Renaming $patch to $new_name"

    # 重命名文件
    mv "$patch" "$new_name"

    # 增加计数器
    i=$((i + 1))
done
