#! /bin/bash

set -aex

git fetch origin main
git fetch origin auto-pages
git checkout -b auto-pages origin/auto-pages
git merge --strategy-option=theirs origin/main --allow-unrelated-histories --no-edit

# find . \( -path ./node_modules -o -path ./source  -o -path ./scaffolds \) -prune -o -type f -name "*.md" ! -name "README.md" | grep md | grep -v linglong | grep -v 玲珑仓库 | xargs -I {} rsync -avzu {} source/_posts

# find . \( -path ./node_modules -o -path ./source -o -path ./scaffolds -o -path ./filtered_directory -prune \) -o -type f -name "*.md" -exec sh -c 'mkdir -p source/_posts/"$(dirname "{}")" && cp "{}" source/_posts/"{}"' \;

# rm -rf source/_posts/*

# rsync -avP --exclude='.git'--exclude='.github' --exclude='node_modules' --exclude='source' --exclude='scaffolds' --include='*/' --include='*.md' --exclude='*' ./ ./source/_posts/

# rm -rf source/_posts/.git source/_posts/.github source/_posts/.gitignore source/_posts/.DS_Store

# find source/_posts -type f ! -name "*.md"

rsync -avP  --delete --exclude='.git'--exclude='.github' --exclude='node_modules' --exclude='source' --exclude='scaffolds' --include='*/' --include='*.md' --exclude='*' ./ ./source/_posts/
rsync -avP README.md source/about/index.md
rm -rf source/_posts/.git source/_posts/.github source/_posts/.gitignore source/_posts/.DS_Store
find source/_posts -type f ! -name "*.md"
git add -A .
git branch -u origin/auto-pages
git commit -m "feat: new pages" --allow-empty
# git push origin HEAD:auto-pages -f

