---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

# docker

## 原理

- [Docker 夺命连环 15 问](https://mp.weixin.qq.com/s/GAAJOoF6hCRY0CxfaTpfAg)
- [Docker 核心技术与实现原理](https://draveness.me/docker/)
- [一篇文章带你吃透 Docker 原理](https://www.cnblogs.com/michael9/p/13039700.html)
- [docker 镜像分层原理](https://www.cnblogs.com/handwrit2000/p/12871493.html)
- [Docker五种存储驱动原理及应用场景和性能测试对比](http://dockone.io/article/1513)
- [技术总监对Docker理解的太透彻了，几句话给程序员新人整的明明白白！](https://mp.weixin.qq.com/s/EHgCOgXJfZ7OtV35qDzzsg)
- [【云原生|实战入门】1：Docker、K8s简单实战与核心概念理解](https://blog.csdn.net/weixin_51484460/article/details/125041875)
- [chroot，pivot_root和switch_root 区别](https://blog.csdn.net/u012385733/article/details/102565591)
- [inode、chroot、pivot_root](https://www.cnblogs.com/valon/p/6869368.html)
- [pivot_root实现原理](https://zhuanlan.zhihu.com/p/101096040)
- [PIVOT_ROOT命令的使用](https://www.cnblogs.com/bianhao3321/p/6873511.html)
- [chroot, exec, pivot_root](https://blog.51cto.com/jiangjqian/381778)
- [自己写Docker_挂载busybox作为容器操作系统的rootfs](https://www.bilibili.com/read/cv11533449/)

- [寻根究底，为什么Docker中的Alpine Linux镜像能这么小](https://www.toutiao.com/article/7195362738607424003)

- [【容器安全篇】原来root在容器里也不是万能的](https://www.toutiao.com/article/7208512937982689831)

## 基础

- [Docker学习笔记](https://zhuanlan.zhihu.com/p/82433360)
- [ubuntu docker开启2375端口，支持远程访问](https://www.cnblogs.com/lhns/p/13958249.html)
- [docker出现GPG error: At least one invalid signature was encountered相关问题及解决方法](https://blog.csdn.net/u014374009/article/details/114010841)
- [docker基础镜像有哪些](https://m.php.cn/docker/486829.html)

## 安装

- [Docker Community Edition 镜像使用帮助](https://mirrors.tuna.tsinghua.edu.cn/help/docker-ce/)
- [Install Docker Engine on Debian](https://docs.docker.com/engine/install/debian/)
- [Deepin v20 正式版安装 Docker](https://www.cnblogs.com/langkyeSir/p/14032801.html)

```bash
sudo apt install docker-ce
sudo groupadd docker            # 有则不用创建
sudo usermod -aG docker $USER   # USER 为加入 docker 组的用户
newgrp docker                   # 刷新 docker 组
docker run hello-world          # 测试无 root 权限能否使用 docker
```

### docker源

编辑 Docker 配置文件:

```bash
sudo -s
cat << EOF > /etc/docker/daemon.json
{
    "registry-mirrors": [
        "https://dockerproxy.com",
        "https://hub-mirror.c.163.com",
        "https://mirror.baidubce.com",
        "https://ccr.ccs.tencentyun.com"
    ]
}
EOF
```

重启 Docker:

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
```

- [阿里云Docker镜像仓库](https://cr.console.aliyun.com/cn-hangzhou/instance/dashboard)
- [Docker 配置国内源加速(2023/05/14)](https://blog.csdn.net/qq_44797987/article/details/112681224)

查看是否成功:

```bash
docker info
```

出现以下字段代表配置成功:

```text
Registry Mirrors:
  https://dockerproxy.com/
  https://hub-mirror.c.163.com/
  https://mirror.baidubce.com/
  https://ccr.ccs.tencentyun.com/
```

## DockerFile

- [DockerFile集成mysql，nginx，zookeeper，redis，tomcat为一个镜像](https://blog.csdn.net/tengchengbaba/article/details/83501697)
- [Docker build时提示source not found](http://wxnacy.com/2020/10/01/docker-source-not-found/)
- [通过Dockerfile文件为linux images添加新用户](https://blog.csdn.net/tony1130/article/details/53170228)
- [docker环境变量设置](https://blog.csdn.net/a12345676abc/article/details/84651477)
- [docker 容器服务脚本自启动](https://www.cnblogs.com/erlou96/p/13884646.html)
- [docker容器设置时区](https://jiayaoo3o.github.io/2019/06/29/docker%E5%AE%B9%E5%99%A8%E8%AE%BE%E7%BD%AE%E6%97%B6%E5%8C%BA/)

- [构建 Docker 镜像的 N 个小技巧，运维工程师看过来，学到了~](https://mp.weixin.qq.com/s/tcv0zPDzfrFX_uvPPJ7lHw)

- [你在使用 Docker 吗？那不能错过这款 Linux](https://mp.weixin.qq.com/s/vI5rs_4ukKhaPcT8EolahA)

## 上下文路径

- [Docker Dockerfile上下文路径](https://www.runoob.com/docker/docker-dockerfile.html)
- [使用 Dockerfile 定制镜像  镜像构建上下文（Context）](https://blog.csdn.net/chenji4315/article/details/100623754)

## 命令补全

- [解决ubuntu docker容器命令tab无法自动补全问题](https://blog.csdn.net/Mr_chunping/article/details/122089360)
- [docker疑难杂症：docker命令Tab无法自动补全](https://blog.csdn.net/qq_39680564/article/details/97026656)

## commit

- [Docker 使用容器来创建镜像](https://www.runoob.com/w3cnote/docker-use-container-create-image.html)
- [Docker  将容器打包成新镜像，将镜像打包成文件和加载镜像包](https://blog.csdn.net/Aeve_imp/article/details/101531225)

## 端口映射

- [Docker容器内部端口映射到外部宿主机端口-运维笔记](https://www.cnblogs.com/kevingrace/p/9453987.html)
- [6 张图详解 Docker 容器网络配置](https://mp.weixin.qq.com/s/zbbTNjcNNcJFGN9lbaqqPw)
- [5 年工作经验，Docker 的几种网络模式都说不清，你敢信？](https://www.toutiao.com/article/7174322723983098368)
- [不可错过！5 张图带你搞懂容器网络原理](https://mp.weixin.qq.com/s/ZN-84Z_NMdFF9pqWXQJk_A)

## 磁盘清理

- [如何清理 Docker 占用的磁盘空间](https://cloud.tencent.com/developer/article/1581147)
- [Docker容器动态添加端口](https://zhuanlan.zhihu.com/p/65938559)
- [使用iptables为docker容器动态添加端口映射](https://blog.csdn.net/weixin_42271016/article/details/104786418)
- [docker容器启动后添加端口映射](https://blog.csdn.net/weixin_42181917/article/details/107936753)

## Docker-compose

- [docker-compose部署mysql](https://blog.csdn.net/weixin_43997548/article/details/122693332)
- [docker-compose配置mysql，密码无效问题修复已实践](https://www.pudn.com/news/62bc68c1f3cc394cf1dd7c3b.html)
- [Docker mysql:5.7 root用户无法登录的问题](https://www.akersman.com/article/71)
- [Error 1045 (28000) access denied for user root localhost](https://www.stechies.com/error-1045-28000-access-denied-user-root-localhost/Error%201045%20(28000)%20access%20denied%20for%20user%20root%20localhost)

- [为什么不建议你在 Docker 中跑 Mysql ?](https://mp.weixin.qq.com/s/cbRW2jnq4pFiSaJ6KxI4Nw)

- [docker socket设置](http://docs.lvrui.io/2017/02/19/docker-socket%E8%AE%BE%E7%BD%AE/)
- [如何更改Docker sock文件的位置？](https://www.srcmini.com/50365.html)

- [懂了！VMware/KVM/Docker原来是这么回事儿](https://developer.aliyun.com/article/768343)
