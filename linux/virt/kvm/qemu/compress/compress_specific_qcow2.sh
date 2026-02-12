#!/bin/bash

# 专门压缩指定的 qcow2 镜像文件
# 使用方法: ./compress_specific_qcow2.sh

# 设置压缩算法 (zstd 或 zlib)
COMPRESSION_ALGORITHM="zstd"

# 定义要压缩的镜像文件列表
IMAGES=(
    "ctyunos-22.06-230117-aarch64.qcow2"
    "ctyunos-23.01-230117-aarch64.qcow2"
    "ctyunos-23.01-230117-x86_64.qcow2"
    "debian10.qcow2"
    "debian11.qcow2"
    "debian9.qcow2"
    "ubuntu16.04.qcow2"
    "ubuntu18.04.qcow2"
)

echo "开始压缩指定的 qcow2 镜像文件..."
echo "压缩算法: $COMPRESSION_ALGORITHM"
echo "=================================================="

# 遍历指定的镜像文件
for file in "${IMAGES[@]}"; do
    # 检查文件是否存在
    if [ ! -f "$file" ]; then
        echo "警告: 文件 $file 不存在，跳过!"
        echo "--------------------------------------------------"
        continue
    fi
    
    echo "正在处理文件: $file"
    
    # 显示原始文件大小
    original_size=$(du -h "$file" | cut -f1)
    echo "原始大小: $original_size"
    
    # 1. 压缩镜像到临时文件
    temp_file="${file%.qcow2}-compressed.qcow2"
    echo "正在压缩..."
    
    qemu-img convert -O qcow2 -c -o compression_type=$COMPRESSION_ALGORITHM "$file" "$temp_file"
    
    # 检查是否压缩成功
    if [ $? -ne 0 ]; then
        echo "错误: $file 压缩失败!"
        rm -f "$temp_file"
        echo "--------------------------------------------------"
        continue
    fi
    
    # 2. 检查新镜像是否正常
    echo "正在校验新镜像..."
    qemu-img check "$temp_file"
    if [ $? -ne 0 ]; then
        echo "错误: $temp_file 校验失败!"
        rm -f "$temp_file"
        echo "--------------------------------------------------"
        continue
    fi
    
    # 3. 计算压缩后的文件大小
    compressed_size=$(du -h "$temp_file" | cut -f1)
    
    # 4. 替换原文件
    mv "$temp_file" "$file"
    
    echo "压缩完成: $file"
    echo "大小变化: $original_size → $compressed_size"
    echo "--------------------------------------------------"
done

echo "所有指定的 qcow2 文件压缩完成!"
echo "=================================================="

# 显示最终的文件大小信息
echo "最终文件大小统计:"
echo "=================================================="
for file in "${IMAGES[@]}"; do
    if [ -f "$file" ]; then
        size=$(du -h "$file" | cut -f1)
        echo "$file: $size"
    fi
done
