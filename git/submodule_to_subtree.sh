#!/bin/bash

# Read the .gitmodules file line by line
while read -r line1 && read -r line2 && read -r line3 && read -r line4; do
  # Use awk to extract the submodule name, url, path, and branch
  submodule=$(echo "$line1" | cut -d \" -f 2 | xargs basename)
  path=$(echo "$line2" | awk -F'=' '{print $2}' | sed 's/ //g')
  url=$(echo "$line3" | awk -F'=' '{print $2}' | sed 's/ //g')
  branch=$(echo "$line4" | awk -F'=' '{print $2}' | sed 's/ //g')
  
  # Print the results
  echo "Submodule:$submodule"
  echo "Path:$path"
  echo "URL:$url"
  echo "Branch:$branch"
  echo # Add a blank line for readability
  # git remote remove $submodule
  git submodule deinit $path
  git rm $path
  git commit --am "refactor: convert git submodule $submodule to subtree"
  git remote remove $submodule
  git remote add -f $submodule $url
  git subtree add --prefix=$path $submodule main
done < .gitmodules

git rm .gitmodules

git commit --am "refactor: convert all git submodule to subtree"
