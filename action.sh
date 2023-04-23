#! /bin/bash

set -aex
git config --global user.email "178955347@qq.com"
git config --global user.name "bot"
git fetch origin
git checkout main
git pull origin main
pwd
find . -name "*.md" -exec sh -c 'sed -i "1 i ---\ndate: $(git log --reverse --pretty=format:"%cd" --date=format:"%Y/%m/%d %H:%M:%S" "{}" | head -1)\nupdated: $(git log --reverse --pretty=format:"%cd" --date=format:"%Y/%m/%d %H:%M:%S" "{}" | tail -1)\n---\n" "{}"' \;
rm -rf ../hexo-site
mkdir ../hexo-site
cp -r ./* ../hexo-site
git checkout -- .
git checkout -b hexo origin/hexo
rsync -avP  --delete --exclude='.git'--exclude='.github' --exclude='node_modules' --exclude='public' --exclude='source' --exclude='scaffolds' --exclude='themes' --include='*/' --include='*.md' --exclude='*' ../hexo-site/ ./source/_posts/
rsync -avP ../hexo-site/README.md source/about/index.md
pwd
git status
git add -A .
find source -name '*.md'
git commit -m "feat: bot auto commit"
git push origin HEAD:bot -u -f
pwd
npm install
find source -name '*.md'
git branch
find source -name '*.md'
pwd
npm run clean
pwd
npm run build
ls -la
tree public
npm run server