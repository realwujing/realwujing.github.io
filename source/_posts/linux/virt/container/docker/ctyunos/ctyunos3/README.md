---
title: 'CTYunOS 3 Docker 内核编译环境'
date: '2025/11/25 17:15:02'
updated: '2025/11/25 21:09:36'
---

# CTYunOS 3 Docker 内核编译环境

基于 CTYunOS 3 (23.01) 的 Docker 容器化内核编译环境，支持 AMD64 和 ARM64 架构的内核编译和 RPM 打包。

## 特性

- **自动化构建流程**：通过 Makefile 一键完成镜像构建、容器管理和内核编译
- **用户权限映射**：容器内用户自动映射为 host 用户，避免文件权限问题
- **多架构支持**：支持 AMD64 和 ARM64 (交叉编译) 内核构建
- **智能分支检测**：自动识别 git worktree 和分支目录
- **增量构建**：镜像分层设计，支持快速重建

## 镜像架构

```
ctyunos:23.01           ← 基础 OS 镜像 (从 tar.xz 加载)
    ↓
ctyunos:23.01-build     ← 安装 sudo、shadow-utils，创建用户
    ↓
ctyunos:23.01-dev       ← 开发镜像 (运行时通过 --user 映射用户)
```

## 快速开始

### 1. 查看帮助

```bash
make help
```

### 2. 编译内核 (AMD64)

```bash
# 使用默认分支编译 5.10 内核
make build-5.10-amd64

# 使用默认分支编译 6.6 内核
make build-6.6-amd64

# 使用自定义分支
make build-5.10-amd64 KERNEL_5_10_BRANCH=my-feature-branch
```

### 3. 构建 RPM 包 (AMD64)

```bash
# 构建 5.10 内核 RPM
make rpmbuild-5.10-amd64

# 构建 6.6 内核 RPM
make rpmbuild-6.6-amd64
```

### 4. 进入容器 Shell

```bash
make shell
```

## 常用命令

### 内核编译

| 命令 | 说明 |
|------|------|
| `make build-5.10-amd64` | 编译 ctkernel-lts-5.10 (AMD64) |
| `make build-6.6-amd64` | 编译 ctkernel-lts-6.6 (AMD64) |
| `make build-5.10-arm64` | 交叉编译 ctkernel-lts-5.10 (ARM64) |
| `make build-6.6-arm64` | 交叉编译 ctkernel-lts-6.6 (ARM64) |

### RPM 打包

| 命令 | 说明 |
|------|------|
| `make rpmbuild-5.10-amd64` | 构建 5.10 内核 RPM (AMD64) |
| `make rpmbuild-6.6-amd64` | 构建 6.6 内核 RPM (AMD64) |
| `make rpmbuild-5.10-arm64` | 构建 5.10 内核 RPM (ARM64) |
| `make rpmbuild-6.6-arm64` | 构建 6.6 内核 RPM (ARM64) |

### 容器管理

| 命令 | 说明 |
|------|------|
| `make shell` | 进入容器 Shell |
| `make setup-container` | 启动容器 |
| `make restart-container` | 重启容器 |
| `make stop-container` | 停止容器 |
| `make clean-container` | 删除容器 (保留镜像) |
| `make status` | 查看容器和镜像状态 |

### 镜像管理

| 命令 | 说明 |
|------|------|
| `make load-image` | 加载基础镜像 |
| `make build-base-image` | 构建带构建工具的镜像 |
| `make build-image` | 构建开发镜像 |
| `make clean-images` | 删除所有镜像 |
| `make clean` | 清理所有容器和镜像 |

### ARM64 交叉编译

| 命令 | 说明 |
|------|------|
| `make setup-cross-compile` | 下载并解压 ARM64 交叉编译工具链 |

## 目录映射

容器自动挂载以下目录：

| Host 路径 | 容器路径 | 说明 |
|-----------|----------|------|
| `~/code` | `/home/$USER/code` | 代码目录 |
| `~/code/rpmbuild` | `/home/$USER/rpmbuild` | RPM 构建目录 |
| `~/Downloads` | `/home/$USER/Downloads` | 下载目录 (包含交叉编译工具链) |

## 分支配置

默认分支可以通过命令行参数覆盖：

```bash
# 默认分支
KERNEL_5_10_BRANCH = ctkernel-lts-5.10/develop
KERNEL_6_6_BRANCH = ctkernel-lts-6.6/develop

# 使用自定义分支
make build-5.10-amd64 KERNEL_5_10_BRANCH=ctkernel-lts-5.10/my-feature
make rpmbuild-6.6-amd64 KERNEL_6_6_BRANCH=ctkernel-lts-6.6/bugfix-123
```

## 源码目录检测

构建命令会按以下顺序查找源码目录：

1. **Git worktree**：检查 `~/code/linux` 的 worktree 列表
2. **分支目录**：检查 `~/code/<branch-name>` (斜杠替换为连字符)
3. **主仓库**：使用 `~/code/linux` 并切换到指定分支

示例：
```bash
# 分支: ctkernel-lts-5.10/develop
# 查找顺序:
# 1. git worktree 中的 ctkernel-lts-5.10/develop
# 2. ~/code/ctkernel-lts-5.10-develop
# 3. ~/code/linux (切换到该分支)
```

## 用户权限

容器运行时自动映射 host 用户的 UID/GID，确保：

- 容器内用户名与 host 一致
- 创建的文件在 host 上有正确的权限
- 支持 sudo 无密码执行 root 命令

验证用户映射：
```bash
make shell
# 在容器内
whoami  # 显示你的用户名
id      # 显示 UID/GID
sudo whoami  # 显示 root (无需密码)
```

## 内核配置

不同版本使用不同的 defconfig：

| 内核版本 | AMD64 defconfig | ARM64 defconfig |
|----------|-----------------|-----------------|
| 5.10 | `openeuler_defconfig` | `openeuler_defconfig` |
| 6.6 | `ctyunos_defconfig` | `ctyunos_defconfig` |

## 构建输出

### 编译输出

编译后的内核文件位于源码目录：
- `arch/x86/boot/bzImage` (AMD64)
- `arch/arm64/boot/Image` (ARM64)

### RPM 包输出

RPM 包位于：
- AMD64: `~/rpmbuild/RPMS/x86_64/`
- ARM64: `~/rpmbuild/RPMS/aarch64/`

## 故障排查

### 容器无法启动

```bash
# 查看状态
make status

# 清理并重建
make clean
make setup-container
```

### 用户权限问题

```bash
# 检查容器内用户
docker exec ctyunos-23.01-dev id

# 应该显示你的 UID/GID，而不是 root (0:0)
```

### 镜像构建失败

```bash
# 清理所有镜像重新构建
make clean-images
make build-image
```

### 找不到源码目录

确保源码位于以下位置之一：
- `~/code/linux` (主仓库)
- `~/code/<branch-name>` (分支目录)
- Git worktree 中

### ARM64 交叉编译失败

```bash
# 确保工具链已安装
make setup-cross-compile

# 验证工具链路径
ls -la ~/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/
```

## 高级用法

### 手动进入容器执行命令

```bash
docker exec -it ctyunos-23.01-dev bash
```

### 查看容器日志

```bash
docker logs ctyunos-23.01-dev
```

### 自定义目录映射

编辑 Makefile 中的以下变量：

```makefile
HOST_CODE_DIR := $(HOST_HOME)/code
HOST_RPMBUILD_DIR := $(HOST_CODE_DIR)/rpmbuild
HOST_DOWNLOADS_DIR := $(HOST_HOME)/Downloads
```

### 保留容器运行

默认情况下，构建完成后容器会自动停止。如需保持运行，修改 Makefile 中的构建命令，删除最后的 `docker stop` 行。

## 依赖项

### Host 系统要求

- Docker
- Make
- wget (用于下载镜像和工具链)
- xz (用于解压镜像)

### 容器内自动安装

- sudo
- shadow-utils
- 内核构建依赖 (通过 `yum-builddep` 自动安装)

## 文件说明

- `Makefile`: 自动化构建脚本
- `Dockerfile`: 最小化 Dockerfile (用户映射在运行时处理)
- `README.md`: 本文档

## 参考资源

- CTYunOS 官方文档: https://repo.ctyun.cn/hostos/ctyunos-23.01/
- ARM 交叉编译工具链: https://developer.arm.com/downloads/-/gnu-a

## 许可证

本项目遵循 CTYunOS 的许可证条款。
