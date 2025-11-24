#!/bin/bash

file="$1"
date=$(git log --reverse --pretty=format:"%cd" --date=format:"%Y/%m/%d %H:%M:%S" "$file" | head -1)
updated=$(git log --reverse --pretty=format:"%cd" --date=format:"%Y/%m/%d %H:%M:%S" "$file" | tail -1)

# 从文件内容中提取第一个# 标题作为title
title=$(grep -m 1 "^# " "$file" | sed "s/^# //")

# 如果没有找到标题,使用文件名(去掉.md扩展名)
if [ -z "$title" ]; then
  title=$(basename "$file" .md)
fi

# 处理title中的特殊字符
# 在YAML中使用单引号包裹，单引号内的单引号需要用两个单引号转义
escaped_title=$(echo "$title" | sed "s/'/''/g")

# 创建临时文件保存原内容
tmpfile=$(mktemp)
cat "$file" > "$tmpfile"

# 在文件开头插入 front matter
{
  echo "---"
  echo "title: '$escaped_title'"
  echo "date: '$date'"
  echo "updated: '$updated'"
  echo "---"
  echo ""
  cat "$tmpfile"
} > "$file"

rm -f "$tmpfile"
