#!/bin/bash

# 获取当前目录下的所有子目录
for dir in */; do
  # 确保是目录
  if [ -d "$dir" ]; then
    # 进入子目录
    pushd "$dir" > /dev/null

    # 在每个子目录中执行原始命令
    mods=$(find . -maxdepth 1 -type d -exec sh -c '
      dirs=""
      for dir; do
        if [ -z "$(find "$dir" -type f -name "*.o" 2>/dev/null)" ]; then
          if ! git log --pretty=format:"%h %an %s %b" "$dir" | grep -i 'chinatelecom\.cn' | grep -q .; then
            dirs="$dirs${dir#./} "
          fi
        fi
      done
      echo "$dirs"
    ' sh {} +)

    # 原命令不变，继续对每个符合条件的目录执行操作
    for mod in $mods; do
      [ -d "$mod" ] && echo "Removing directory: $mod" && rm -rf "$mod" && find . -maxdepth 1 \( -name "Kconfig" -o -name "Makefile" \) -type f -exec grep -l -E "${mod}(\/|\.o|$)" {} \; | while read file; do
        echo "Removing references to $mod in $file"
        sed -i -E "/${mod}(\/|\.o|$)/d" "$file"
      done
      git add .
      git commit -s -m "${PWD#$(realpath "/home/wujing/code/CTKernel")/}: remove $mod"
    done

    # 退出子目录
    popd > /dev/null
  fi
done

# 输出最近的 git 提交记录
git log --oneline | head -n 5

# 列出所有子目录
ls -d */

# 统计删除的代码行数
git log --author=yuanql9 --since="5 days ago" --pretty=tformat: --numstat | awk '{added+=$1; removed+=$2} END {print "Added lines:", added, "Removed lines:", removed, "Total changes:", added - removed}'

# 删除每个提交中的 drivers_no_o.txt 文件，并进行必要的历史重写操作
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch */drivers_no_o.txt" --prune-empty --tag-name-filter cat -- HEAD~380..HEAD

# 统计代码行数
yum install cloc
cloc .
