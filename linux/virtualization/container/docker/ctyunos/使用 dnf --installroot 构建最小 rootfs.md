# 使用 dnf --installroot 构建最小 rootfs

1. 创建 rootfs 目录

```bash
mkdir -p /home/wujing/Downloads/ctyunos-rootfs
```

2. 使用 dnf 安装最小系统（以 ctyunos 22.06 为例）

```bash
dnf -c /etc/yum.repos.d/ctyunos.repo \
    --installroot=/home/wujing/Downloads/ctyunos-rootfs \
    --releasever=22.06 \
    --setopt=install_weak_deps=false \
    --setopt=tsflags=nodocs \
    install bash coreutils ctyunos-release yum
```

3. 清理缓存

```bash
rm -rf /home/wujing/Downloads/ctyunos-rootfs/var/cache/dnf
```

4. 打包 rootfs

```bash
cd /home/wujing/Downloads/ctyunos-rootfs
```

```bash
tar --numeric-owner --exclude='./dev/*' --exclude='./proc/*' --exclude='./sys/*' -cvf ../ctyunos-22.06.tar .
```

# 5. 导入 Docker

```bash
cat /home/wujing/Downloads/ctyunos-22.06.tar | docker import - ctyunos-22.06
```

# 6. 测试

```bash
docker run -it --rm ctyunos-22.06 /bin/bash
```
