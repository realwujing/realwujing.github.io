---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

<!--
 * @Author: wujing
 * @Date: 2021-04-02 20:14:30
 * @LastEditTime: 2021-04-08 01:40:29
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /DockerProject/ubuntu/README.md
 * 可以输入预定的版权声明、个性签名、空行等
-->
## 1、git初始化
    git config --global user.email "178955347@qq.com"
    git config --global user.name "RealWuJing"

## 2、创建镜像    
    docker build -t ubuntu:ai .
    docker build -t ubuntu:ai2 .
## 3、启动镜像
    # bash    
    docker run -itd --name ubuntu-ai ubuntu:ai /bin/bash
    # dash
    docker run -itd --name ubuntu-wujing ubuntu:wujing
    # 端口映射
    docker run -p 22222:22 -p 33306:3306 -dit --name ubuntu-ai ubuntu:ai
    docker run -p 22222:22 -p 33306:3306 -dit --name ubuntu-ai ubuntu:ai /bin/bash
    docker run -p 22223:22 -p 33307:3306 -dit --name ubuntu-ai2 ubuntu:ai2 /bin/bash
    

## 4、mysql
[mysql 启动失败：su: warning: cannot change directory to /nonexistent: No such file or directory](https://www.cnblogs.com/cnwcl/p/13805643.html)

[Ubuntu20.04安装Mysql（亲测有效，一定要按步骤来）](https://blog.csdn.net/YM_1111/article/details/107555383)

    # 修复 mysql 启动失败
    sudo usermod -d /var/lib/mysql/ mysql
    # 启动 mysql
    sudo service mysql start
    # mysql 初始化配置
    sudo mysql_secure_installation

    
## 5、ssh
    # 启动 ssh
    sudo service ssh start
    # 配置ssh客户端，去掉PasswordAuthentication yes前面的#号，保存退出
    sudo vim /etc/ssh/ssh_config

## 6、anaconda
    # 安装包
    https://mirrors.bfsu.edu.cn/anaconda/archive/Anaconda3-2020.02-Linux-x86_64.sh
    # .condarc
    https://mirrors.bfsu.edu.cn/help/anaconda/

    # https://mirrors.bfsu.edu.cn/help/pypi/
    pip config set global.index-url https://mirrors.bfsu.edu.cn/pypi/web/simple
    
## AI环境
    # 新建ai python环境
    conda create -y -n ai python=3.6
    # 激活环境
    conda activate ai
    # 安装tensorflow pytorch等
    conda install -y tensorflow pytorch pandas matplotlib