#!/bin/bash

set -aex

# 进入内核源码所在目录
cd ~/code/linux

git restore .
git checkout ctkernel-lts-5.10/yuanql9/develop-amd-genoa
make distclean
git clean -fdx
git restore .
git reset --hard HEAD

rm -rf /root/rpmbuild/SOURCES/kernel.tar.gz
tar --xform="s/^./kernel/" --exclude=".git" -chzf /root/rpmbuild/SOURCES/kernel.tar.gz .
cp build/* /root/rpmbuild/SOURCES

# 不要检查kabi变化
sed -i -E '0,/%define with_kabichk 1/! {/%define with_kabichk 1/s/1/0/}' build/kernel.spec

# 在版本号后面追加.amd.genoa.$(git rev-parse HEAD | head -c 12)，$(git rev-parse HEAD | head -c 12)指向最后一次提交的commit hash的前12位
sed -i "/%global pkg_release/s/$/\.amd.genoa.$(git rev-parse HEAD | head -c 12)/" build/kernel.spec

rpmbuild -ba build/kernel.spec