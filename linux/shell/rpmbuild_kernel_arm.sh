#!/bin/bash

set -aex

# 默认源码目录
SOURCE_DIR=~/code/linux

# 初始化分支名、是否提供分支名
BRANCH=""
BRANCH_PROVIDED=false

# 打印帮助信息
print_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --source=<path>         Specify the source code directory (default: $SOURCE_DIR)"
    echo "  --branch=<branch_name>  Specify the git branch to build (default: $DEFAULT_BRANCH)"
    echo "  -h, --help              Show this help message and exit"
    exit 0
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        --source=*)
            SOURCE_DIR="${1#*=}"
            shift
            ;;
        --branch=*)
            BRANCH="${1#*=}"
            BRANCH_PROVIDED=true
            shift
            ;;
        -h|--help)
            print_help
            ;;
        *)
            echo "Unknown option: $1"
            print_help
            exit 1
            ;;
    esac
done

# 进入源码目录
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory $SOURCE_DIR does not exist."
    exit 1
fi
cd "$SOURCE_DIR" || { echo "Error: Failed to cd to $SOURCE_DIR"; exit 1; }

# 获取当前分支作为默认值
DEFAULT_BRANCH=$(git symbolic-ref --short HEAD)

# 如果用户没有提供分支名，则使用默认分支
if [ "$BRANCH_PROVIDED" = false ]; then
    BRANCH="$DEFAULT_BRANCH"
fi

# 检查工作目录和暂存区是否有更改
if [ -n "$(git status --porcelain)" ]; then
    echo "错误：当前 Git 工作目录有未提交的更改！"
    git status  # 显示具体的更改状态，便于调试
    exit 1      # 退出脚本并返回错误码 1
fi

git restore .
# 只有在提供了分支名时才执行 git checkout
if $BRANCH_PROVIDED; then
    git checkout "$BRANCH"
fi
make distclean
git clean -fdx
git restore .
git reset --hard HEAD

# 69478eae21ab 在分支 ctkernel-lts-5.10/yuanql9/develop-cross-compile中
# 检查提交 69478eae21ab 的 subject 是否已存在于当前分支
CHERRY_PICK_COMMIT="69478eae21ab"
COMMIT_SUBJECT=$(git log -1 --format=%s "$CHERRY_PICK_COMMIT")

if ! git log --format=%s | grep -qF "$COMMIT_SUBJECT"; then
    # 获取当前 HEAD 的提交哈希（cherry-pick 前的状态）
    PRE_CHERRY_PICK_HEAD=$(git rev-parse HEAD)

    # 执行 cherry-pick
    if git cherry-pick "$CHERRY_PICK_COMMIT"; then
        echo "Cherry-pick $CHERRY_PICK_COMMIT succeeded."

        # 软回退到 cherry-pick 前的状态
        git reset --soft "$PRE_CHERRY_PICK_HEAD"
        echo "Reset to pre-cherry-pick state (soft reset)."
        # 取消所有暂存的更改
        git restore --staged .
    else
        echo "Cherry-pick $CHERRY_PICK_COMMIT failed. Resetting to original state."
        git cherry-pick --abort
    fi
else
    echo "Commit $CHERRY_PICK_COMMIT already applied, skipping cherry-pick."
fi

# 不要检查kabi变化
sed -i -E '0,/%define with_kabichk 1/! {/%define with_kabichk 1/s/1/0/}' build/kernel.spec

# 在版本号后面追加.$(git rev-parse HEAD | head -c 12)，$(git rev-parse HEAD | head -c 12)指向最后一次提交的commit hash的前12位
sed -i "/%global pkg_release/s/$/\.$(git rev-parse HEAD | head -c 12)/" build/kernel.spec

# 交叉编译
export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
export ARCH=arm64

rm -rf /root/rpmbuild/SOURCES/kernel.tar.gz
tar --xform="s/^./kernel/" --exclude=".git" -chzf /root/rpmbuild/SOURCES/kernel.tar.gz .
cp build/* /root/rpmbuild/SOURCES

time rpmbuild -ba --target=aarch64 --define "_host_cpu aarch64" --without=bpftool --without=perf --without=kvm_stat build/kernel.spec
