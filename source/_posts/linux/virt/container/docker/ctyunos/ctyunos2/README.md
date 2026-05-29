---
title: 'CTYunOS 2 Docker 内核编译环境'
date: '2026/01/22 06:21:10'
updated: '2026/01/22 06:21:10'
---

# CTYunOS 2 Docker 内核编译环境

基于 CTYunOS 2 (22.06) 的 Docker 容器化内核编译环境，主要用于编译 openeuler-4-19 内核源码，支持 AMD64 和 ARM64 架构。

## 特性

- **一键构建**：通过 Makefile 自动完成镜像构建、容器启动和内核编译
- **权限映射**：容器内自动映射宿主机用户 UID/GID，避免权限冲突
- **工具链集成**：自动配置 YUM 源并安装必要的开发工具（bc, rpm-build 等）
- **多架构支持**：支持 AMD64 原生编译和 ARM64 交叉编译

## 快速开始

### 1. 查看帮助
```bash
make help
```

### 2. 加载并构建镜像
```bash
# 下载并加载基础 OS 镜像
make load-image

# 构建带开发工具的镜像 (ctyunos:22.06-dev)
make build-image
```

### 3. 编译内核 (AMD64)
```bash
# 使用默认分支编译 4.19 内核
make build-4.19-amd64

# 使用自定义分支
make build-4.19-amd64 KERNEL_4_19_BRANCH=your-branch
```

### 4. 构建 RPM 包 (AMD64)
```bash
make rpmbuild-4.19-amd64
```

### 5. 交叉编译 (ARM64)
```bash
# 设置交叉编译工具链 (gcc-linaro-7.3.1)
make setup-cross-compile

# 执行交叉编译
make build-4.19-arm64
```

## 目录映射

| 宿主机路径 | 容器路径 | 说明 |
|-----------|----------|------|
| `~/code` | `/home/$USER/code` | 内核源码目录 |
| `~/code/rpmbuild` | `/home/$USER/rpmbuild` | RPM 构建输出目录 |
| `~/Downloads` | `/home/$USER/Downloads` | 工具链存放目录 |

## 参考资源

- CTYunOS 官方资源: https://repo.ctyun.cn/hostos/ctyunos-22.06/
- openeuler-4-19 源码: /home/wujing/code/openeuler-4-19-release-0062.y
