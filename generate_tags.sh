#!/bin/bash

# 扫描所有markdown文件的目录结构，生成唯一的标签列表
find . -name "*.md" -type f | while read file; do
  dir=$(dirname "$file")
  # 分割路径并输出每个部分
  echo "$dir" | tr '/' '\n'
done | sort -u | grep -v "^\.$" | grep -v "^$" | tr '\n' ', ' | sed 's/, $//'
