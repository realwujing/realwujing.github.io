#!/bin/bash

set -aex

# 确保在 hexo 分支
current_branch=$(git branch --show-current)
if [ "$current_branch" != "hexo" ]; then
  echo "Error: Must run on hexo branch"
  exit 1
fi

# 切换到 main 分支并更新
git checkout main
git fetch origin
git pull origin main
pwd
ls -lah

# 测试 git log
git log --reverse --pretty=format:"%cd" --date=format:"%Y/%m/%d %H:%M:%S" README.md

# 复制 front_matter.sh 到 main 分支（从 hexo 分支）
git show hexo:front_matter.sh > front_matter.sh
chmod +x front_matter.sh

# 为所有 markdown 文件添加 front matter
find . -name "*.md" -exec bash front_matter.sh {} \;

# 准备 hexo 站点
rm -f front_matter.sh
rm -rf ../hexo-site
mkdir ../hexo-site

# 复制处理后的文件到临时目录
cp -r ./* ../hexo-site

# 切换回 hexo 分支
git checkout -- .
git checkout hexo

# 同步文件到 hexo 分支
rsync -avP --delete \
  --exclude='.git' \
  --exclude='.github' \
  --exclude='node_modules' \
  --exclude='public' \
  --exclude='source' \
  --exclude='scaffolds' \
  --exclude='themes' \
  --include='*/' \
  --include='*.md' \
  --include='*.png' \
  --include='*.jpg' \
  --include='*.jpeg' \
  --include='*.gif' \
  --include='*.svg' \
  --include='*.webp' \
  --exclude='*' \
  ../hexo-site/ ./source/_posts/

rsync -avP ../hexo-site/README.md source/about/index.md

# 修复所有markdown文件中的相对链接为GitHub绝对链接
python3 ./fix_relative_links.py

# 提交更改
pwd
git status
git add -A .
find source -name '*.md'
git commit -m "feat: bot auto commit"

# 安装依赖
npm install
npm uninstall hexo-renderer-marked --save
npm install hexo-renderer-pandoc --save
npm install hexo-filter-plantuml --save

# 构建站点
find source -name '*.md'
git branch
pwd
npm run clean
pwd
npm run build
ls -la
tree public

# 本地服务器（可选）
npm run server