#! /bin/bash

set -aex

git fetch origin
git checkout -b auto-pages origin/auto-pages
git merge --strategy-option=theirs origin/main --allow-unrelated-histories --no-edit

rm -rf source/_posts/*
rsync -avP  --delete --exclude='.git'--exclude='.github' --exclude='node_modules' --exclude='public' --exclude='source' --exclude='scaffolds' --exclude='themes' --include='*/' --include='*.md' --exclude='*' ./ ./source/_posts/
rsync -avP README.md source/about/index.md
rm -rf source/_posts/.git source/_posts/.github source/_posts/.gitignore source/_posts/.DS_Store
find source/_posts -type f ! -name "*.md"
git status
git add -A .
git branch -u origin/auto-pages
git diff-index --quiet HEAD || git commit -m "feat: bot automatic update"
# git push origin HEAD:auto-pages -f
