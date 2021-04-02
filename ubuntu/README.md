## 1、git初始化
    git config --global user.email "178955347@qq.com"
    git config --global user.name "RealWuJing"

## 2、创建镜像    
    docker build -t ubuntu:wujing .
## 3、启动镜像    
    docker run -itd --name ubuntu-wujing ubuntu:wujing /bin/bash
    docker run -itd --name ubuntu-wujing ubuntu:wujing