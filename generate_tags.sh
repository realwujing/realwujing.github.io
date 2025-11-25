#!/bin/bash

# 扫描所有markdown文件的目录结构，生成唯一的标签列表
# 只取前三级目录，过滤掉以.开头的隐藏目录
find . -name "*.md" -type f | while read file; do
  dir=$(dirname "$file")
  # 只取前三级目录，分割路径并输出每个部分
  echo "$dir" | cut -d'/' -f1-4 | tr '/' '\n'
done | sort -u | grep -v "^\.$" | grep -v "^$" | grep -v "^\." | tr '\n' ', ' | sed 's/, $//'
