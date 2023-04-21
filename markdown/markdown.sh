#! /bin/bash

# 将 OneNote导出到有道云笔记后，复制到markdown文件后，进一步格式化

sed -i 's/^|/-/g' $1

sed -i 's/ \{2,\}/ /g' $1

sed -i 's/ |$//g' $1

sed -i 's/^|/-/g' $1


sed -i 's/ //g' $1

sed -i 's/- $//g' $1

sed -i 's/--//g' $1

sed -i 's/- ://g' $1

sed -i '/\[/!s/^-/##/g' $1

sed -i '/^##/{G;}' $1