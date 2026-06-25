# CTYunOS 2 Docker 内核编译环境

基于 CTYunOS 2 (22.06) 的 Docker 容器化内核编译环境，主要用于编译 openeuler-4-19 内核源码，支持 AMD64 和 ARM64 架构。

## 特性

- **一键构建**：通过 Makefile 自动完成镜像构建、容器启动和内核编译
- **权限映射**：容器内自动映射宿主机用户 UID/GID，避免权限冲突
- **工具链集成**：自动配置 YUM 源并安装必要的开发工具（bc, rpm-build 等）
- **多架构支持**：支持 AMD64 原生编译和 ARM64 交叉编译
- **CPU/NUMA 绑定**：容器自动绑定到单个 NUMA 节点（不跨 NUMA），使用该节点全部 CPU，避免占满整机

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

## CPU / NUMA 限制

容器创建时会自动绑定到**单个 NUMA 节点**，使用该节点的**全部 CPU**，并把内存分配限制在同一节点，避免跨 NUMA 访问内存导致的性能下降，同时不会占满整机。

使用整节点 CPU（而非写死数量）可自动适配不同机器：某些机器单节点 CPU 数可能少于 32，也无需手动调整。

对应 Makefile 变量与 `docker run` 选项：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `NUMA_NODE` | 空闲内存最多的节点（自动检测） | 容器绑定的 NUMA 节点 |

```makefile
--cpuset-cpus=<该节点全部 CPU>   # 例如 96-119,288-311，全部在同一节点
--cpuset-mems=$(NUMA_NODE)       # 内存只在该节点分配 → 不跨 NUMA
```

默认行为：

- `NUMA_NODE` 自动选择当前**空闲内存最多**的 NUMA 节点（内核编译较吃内存）。
- 读取该节点的 `cpulist`，把节点上**所有逻辑 CPU** 绑定给容器。
- 容器内 `nproc` 会遵守 cgroup cpuset 返回该节点的 CPU 数，因此 `make -j$(nproc)` 的并行度会自动对齐到节点 CPU 数。

覆盖默认值：

```bash
# 指定 NUMA 节点
make build-4.19-amd64 NUMA_NODE=4
make shell NUMA_NODE=5
```

> ⚠️ CPU/NUMA 绑定只在**容器创建时**（`docker run`）生效。修改 `NUMA_NODE` 或希望重新自动选节点时，需先 `make clean-container`（**不要用 `make clean`，它会删除镜像**）再重建容器。

## 参考资源

- CTYunOS 官方资源: https://repo.ctyun.cn/hostos/ctyunos-22.06/
- openeuler-4-19 源码: /home/wujing/code/openeuler-4-19-release-0062.y
