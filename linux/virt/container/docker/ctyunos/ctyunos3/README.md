# CTYunOS 3 Docker Environment Usage

This directory contains the Docker environment setup for CTYunOS 3 kernel building.

## Files

- `dockerfile`: Custom Dockerfile based on CTYunOS 3 official image
- `Makefile`: Automation scripts for common tasks
- `README.md`: This usage guide

## Quick Start

### 1. Download and Load Base Image

```bash
# Download and load the official CTYunOS 3 image
make download-image
make load-image
```

### 2. Run Container

```bash
# Start the container
make run-container
```

Or for development setup (combines load-image and run-container):

```bash
make dev-setup
```

### 3. Build Kernel

#### Inside the container (x86_64)

```bash
make build-kernel
```

#### Cross-compile for ARM64

```bash
# First setup cross-compile toolchain
make setup-cross-compile

# Then build ARM64 kernel
make build-kernel-arm64
```

## Available Commands

Run `make help` to see all available commands:

```bash
$ make help
CTYunOS 3 Docker Environment Makefile

Available targets:
  help                 Show this help message
  download-image       Download CTYunOS 3 docker image
  load-image          Load CTYunOS 3 docker image from tar.xz
  build-image         Build custom CTYunOS 3 image from Dockerfile
  run-container       Run CTYunOS 3 container
  restart-container   Restart existing container
  stop-container      Stop running container
  remove-container    Remove container
  download-cross-compile Download ARM64 cross-compile toolchain
  setup-cross-compile Extract cross-compile toolchain
  build-kernel        Build kernel inside container
  build-kernel-arm64  Build ARM64 kernel inside container
  shell               Open shell in running container
  logs                Show container logs
  clean-images        Remove CTYunOS docker images
  clean               Clean up containers and images
  status              Show container and image status
  dev-setup           Quick setup for development
  dev-clean           Clean development environment
```

## Directory Structure

The container mounts the following directories:

- Host: `/home/wujing/code` → Container: `/root/code`
- Host: `/home/wujing/code/rpmbuild` → Container: `/root/rpmbuild`
- Host: `/home/wujing/Downloads` → Container: `/root/Downloads`

You can modify these paths in the Makefile if needed.

## Building Kernels

### CTKernel-LTS-5.10 (x86_64)

```bash
# Inside container
cd /root/code/linux
git checkout ctkernel-lts-5.10/develop-0088
yum-builddep build/kernel.spec -y

# Clean and setup RPM build environment
for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do
    rm -rf /root/rpmbuild/"$dir"/*
done
mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Build kernel RPM
rm -rf /root/rpmbuild/SOURCES/kernel.tar.gz
tar --xform="s/^./kernel/" --exclude=".git" -chzf /root/rpmbuild/SOURCES/kernel.tar.gz .
cp build/* /root/rpmbuild/SOURCES
time rpmbuild -ba build/kernel.spec
```

### ARM64 Cross-Compile

```bash
# Setup cross-compile environment
export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
export ARCH=arm64

# Build
cd /root/code/linux
make openeuler_defconfig
make binrpm-pkg -j$(nproc)
```

## Troubleshooting

### Container Issues

```bash
# Check container status
make status

# View container logs
make logs

# Restart container
make restart-container

# Clean everything and start fresh
make clean
make dev-setup
```

### Repository Issues

If you encounter repository access issues, check:

1. Network connectivity to `repo.ctyun.cn` and `121.237.176.8:50001`
2. Repository configuration in `/etc/yum.repos.d/ctyunos.repo`
3. Try updating repository cache: `yum makecache`

### Cross-Compile Issues

Make sure the cross-compile toolchain is properly extracted:

```bash
ls -la /root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/
```

## Customization

### Modify Mount Points

Edit the `Makefile` and change these variables:

```makefile
HOST_CODE_DIR := /your/code/path
HOST_RPMBUILD_DIR := /your/rpmbuild/path
HOST_DOWNLOADS_DIR := /your/downloads/path
```

### Custom Dockerfile

The included `dockerfile` can be customized for additional tools or configurations. Build the custom image with:

```bash
make build-image
```