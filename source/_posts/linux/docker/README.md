---
date: 2024/07/23 11:12:26
updated: 2024/09/09 10:24:01
---

# DockerProject

以下教程基于Ubuntu 16.04 LTS.

## 安装docker

    # 推荐使用使用国内 daocloud 一键安装命令
    curl -sSL https://get.daocloud.io/docker | sh

    # 使用 Docker 作为非 root 用户，则应考虑使用类似以下方式将用户添加到 docker 组
    sudo usermod -aG docker your-user

    # Docker 镜像加速
    sudo cat daemon.json > /etc/docker/daemon.json

    # 重新启动服务
    sudo systemctl daemon-reload
    sudo systemctl restart docker

## DockerFile

    cd ubuntu

    ## 创建镜像
    docker build -t ubuntu:ai2 .
    
    ## 启动镜像
    docker run -p 22223:22 -p 33307:3306 -dit --name ubuntu-ai2 ubuntu:ai2

## docker仓库源

- [Docker 安装脚本](https://linuxmirrors.cn/other/)

    ```bash
    bash <(curl -sSL https://linuxmirrors.cn/docker.sh)
    ```
