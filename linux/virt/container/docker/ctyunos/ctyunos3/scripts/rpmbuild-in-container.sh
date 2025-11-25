#!/usr/bin/env bash
set -aexuo pipefail

# Usage: rpmbuild-in-container.sh <branch> <host_user>
BRANCH=${1:-}
HOST_USER=${2:-wujing}

if [ -z "$BRANCH" ]; then
    echo "Usage: $0 <branch> <host_user>"
    exit 2
fi

# compute branch dir name
BRANCH_DIR=$(printf '%s' "$BRANCH" | sed 's|/|-|g')

if [ -d "/home/${HOST_USER}/code/${BRANCH_DIR}" ]; then
    cd "/home/${HOST_USER}/code/${BRANCH_DIR}"
else
    HOST_WORK_DIR=$(git -C "/home/${HOST_USER}/code/linux" worktree list 2>/dev/null | grep "$BRANCH" | awk '{print $1}' | head -1 || true)
    if [ -n "$HOST_WORK_DIR" ]; then
        cd "$HOST_WORK_DIR"
    else
        cd "/home/${HOST_USER}/code/linux" && git checkout "$BRANCH"
    fi
fi

# locate spec under build/
SPEC=$(find build -type f -name "*.spec" -print -quit 2>/dev/null || true)
if [ -z "$SPEC" ]; then
    echo "No .spec found under build/ in $PWD"
    exit 1
fi
echo "Using spec: $SPEC"

# prepare rpmbuild dirs and run build
sudo yum-builddep "$SPEC" -y
for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do
    rm -rf "/home/${HOST_USER}/rpmbuild/${dir}/*" || true
done
mkdir -p "/home/${HOST_USER}/rpmbuild/"{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
rm -rf "/home/${HOST_USER}/rpmbuild/SOURCES/kernel.tar.gz" || true
echo "Current working directory: $PWD"
echo "======= Starting RPM Build Process ======="
time ./build/build.sh
echo "======= RPM Build Completed ======="
echo "RPM packages location: /home/${HOST_USER}/rpmbuild/RPMS/"
