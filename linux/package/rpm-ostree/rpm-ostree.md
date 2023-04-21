# rpm-ostree

## 官网

- <https://rpm-ostree.readthedocs.io/en/stable>

- <https://coreos.github.io/rpm-ostree/>

## 源码

- <https://github.com/coreos/rpm-ostree>

- <https://lazka.github.io/pgi-docs/OSTree-1.0/mapping.html>

- <https://lazka.github.io/pgi-docs/OSTree-1.0/index.html>

- <https://ostreedev.github.io/ostree/reference/ostree-Root-partition-mount-point.html#ostree-sysroot-query-deployments-for>

- <https://ostreedev.github.io/ostree/reference/reference.html#api-index-full>

- [内容分发网络 CDN-文件下载加速](https://support.huaweicloud.com/productdesc-cdn/cdn_01_0067.html)

- [OpenShift 4 - Fedora CoreOS (6) - 用rpm-ostree安装软件、升级回滚CoreOS_多恩斯基的博客-CSDN博客](https://blog.csdn.net/weixin_43902588/article/details/108998894)
 
- [OSTree 背景介绍 | Yiran's Blog (zdyxry.github.io)](https://zdyxry.github.io/2021/05/22/OSTree-%E8%83%8C%E6%99%AF%E4%BB%8B%E7%BB%8D/)

- [os-level版本控制工具_追寻神迹-CSDN博客_ostree](https://blog.csdn.net/halcyonbaby/article/details/43500231)

- [atomic_在Atomic Host上运行容器的10个任务_cumo3681的博客-CSDN博客](https://blog.csdn.net/cumo3681/article/details/107416567)

- [为你详细介绍Fedora Silverblue版本，包括常见问题解答](https://www.ywnz.com/linuxxw/5534.html)

- [rpm-ostree - Man Page](https://www.mankier.com/1/rpm-ostree)

- [atomic_在Atomic Host上运行容器的10个任务_cumo3681的博客-CSDN博客](https://blog.csdn.net/cumo3681/article/details/107416567)

- [Silverblue 文件系统组织结构](https://docs.fedoraproject.org/zh_Hans/fedora-silverblue/technical-information/)
 
- [【解决办法】升级Fedora33后，屏幕分辨率无法随着VMware窗口大小的改变而改变，设置里面也没有1920x1080分辨率的选项](https://blog.csdn.net/ZLK1214/article/details/113727039)

- [关于编译安装提示No package ** found时可能需配置pkg-config](https://blog.csdn.net/lsg9012/article/details/106117895)

- [Automake的使用](https://www.jianshu.com/p/17e777868d6b)

- [解剖automake和autoconf(autoreconf)](https://www.jianshu.com/p/3f69197f9055)

- [Pkgconfig(polkit-gobject-1) Download (RPM) (pkgs.org)](https://pkgs.org/download/pkgconfig(polkit-gobject-1))

- [autotools自动编译系列之三---autogen.sh实例](https://blog.csdn.net/kongshuai19900505/article/details/79104442)

- [Linux工具之autogen.sh](https://blog.csdn.net/asbhunan129/article/details/88109632)

- <https://stepfunc.io/blog/bindings/>

- <https://cxx.rs/>

- <https://plantuml.com/zh/>

- <http://www.plantuml.com/plantuml/uml/SyfFKj2rKt3CoKnELR1Io4ZDoSa70000>

- <https://archives.fedoraproject.org/pub/archive/fedora/linux/releases/27/>

|          |                            | 解决问题                                                                                                                                                     | 存在问题                                                                                        |
| :------- | :------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------ |
| 软件分发     | 包管理器(RPM)                  | 指定软件分发标准 元数据存储包版本、大小、说明... 安装、卸载、升级、校验... 将源代码打包成二进制软件包 自动查找依赖                                                                                           | RPM 大部分只能更新，不能回滚 依赖关系管理，对系统的基础包依赖导致系统更新困难 不同的发行版之间切换几乎不可能 使用RPM部署复杂应用困难 RPM 编写规则不友好         |
| 应用解决方式   | SCLs(Software Collections) | 不需要修改 RPM 本身（修改 spec 文件） 不会覆盖修改系统文件，/opt/rh/root （不影响系统更新） 可以依赖其他 SCL 允许同时存在多个版本，可按需卸载                                                                   | 依赖发行版（RHEL，CentOS，Fedora...）不同的发行版之间切换几乎不可能 使用RPM部署复杂应用困难 RPM 编写规则不友好                       |
|          | Container                  | 不会覆盖修改系统文件，container image 不依赖特定发行版 允许同时存在多个版本 container image                                                                                           | container 生命周期管理， Kubernetes 安全问题，大部分 container image 都存在各种 CVE(漏洞)                         |
| 操作系统解决方式 | OS 安装方式                    | 不同的 Linux 发行版有不同的安装器进行安装： Redhat - Anaconda Ubuntu - Preseed Photon - photon-os-installer                                                                | 所有应用都运行在 Container 中，Host OS(宿主机)仍可能安装部分软件，比如 Debug、硬件驱动等。 存在问题： 更新依赖问题 回滚困难 随着软件包增多，测试成本增加 |
|          | Active-backup(主动备份)        | 原子更新 目标版本（状态）明确                                                                                                                                          | 分区固定，需要衡量根分区大小 需要多个 rootfs 对应分区，浪费空间 需要重启生效 每次下载完整的 rootfs ，升级动作耗时长                         |
|          | rpm-ostree(OSTree)         | 基于 OSTree "git for OS" 原子升级，支持回滚 允许在多个 rootfs 之间切换 不可变文件系统 使用 RPM 构建 rootfs Package Layer ： 支持通过 rpm-ostree install 安装 RPM 原子更新 单一 repo 存储多个 rootfs 增量更新 | 需要重启生效 文件系统只读(除 /var 和 /etc)                                                                |

![rpm-ostree分支](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230419214153.png)

```text
git checkout e6a65b80dfd380bd994cf7851c6ff28e992ab2f8

ostree cat REF info.json --repo repo
```
