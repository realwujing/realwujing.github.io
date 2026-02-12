#!/bin/bash

# 批量压缩当前目录下的所有 qcow2 镜像
# 使用方法: ./compress_qcow2.sh

# 设置压缩算法 (zstd 或 zlib)
COMPRESSION_ALGORITHM="zstd"

# 遍历当前目录下的所有 .qcow2 文件
for file in *.qcow2; do
    if [ ! -f "$file" ]; then
        continue  # 跳过不存在的文件
    fi
    
    echo "正在处理文件: $file"
    
    # 1. 压缩镜像到临时文件
    temp_file="${file%.qcow2}-compressed.qcow2"
    qemu-img convert -O qcow2 -c -o compression_type=$COMPRESSION_ALGORITHM "$file" "$temp_file"
    
    # 检查是否压缩成功
    if [ $? -ne 0 ]; then
        echo "错误: $file 压缩失败!"
        rm -f "$temp_file"
        continue
    fi
    
    # 2. 检查新镜像是否正常
    qemu-img check "$temp_file"
    if [ $? -ne 0 ]; then
        echo "错误: $temp_file 校验失败!"
        rm -f "$temp_file"
        continue
    fi
    
    # 3. 计算压缩前后的文件大小
    original_size=$(du -h "$file" | cut -f1)
    compressed_size=$(du -h "$temp_file" | cut -f1)
    
    # 4. 替换原文件
    mv "$temp_file" "$file"
    
    echo "完成: $file (原始大小: $original_size → 压缩后: $compressed_size)"
    echo "--------------------------------------------------"
done

echo "所有 qcow2 文件压缩完成!"
