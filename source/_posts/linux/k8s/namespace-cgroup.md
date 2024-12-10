---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

# namespace cgroup

- [Linux沙箱技术](https://hyperj.net/2016/2016-11-23-linux-sandbox/)
- [沙盒化容器：是容器还是虚拟机](https://atbug.com/sandboxed-container/)
- [Linux的Namespace与Cgroups介绍](https://www.cnblogs.com/wjoyxt/p/9935098.html)
- [沙箱权限](https://docs.flatpak.org/zh_CN/latest/sandbox-permissions.html)
- [Docker 基础技术之 Linux namespace 详解](https://www.cnblogs.com/bakari/p/8560437.html)
- [Linux的命名空间详解Linux进程的管理与调度（二）](https://blog.csdn.net/gatieme/article/details/51383322)
- [浅谈Cgroups](https://www.toutiao.com/i6694554806003958284/)
- [LINUX CGROUP总结](https://www.cnblogs.com/menkeyi/p/10941843.html)
- [Linux mount （第一部分）](https://segmentfault.com/a/1190000006878392)
- [Linux mount （第二部分 - Shared subtrees）](https://segmentfault.com/a/1190000006899213)
- [命名空间介绍之八：挂载命名空间和共享子树](https://cloud.tencent.com/developer/article/1531989)
- [<font color=Red>Linux Namespace和Cgroup</font>](https://segmentfault.com/a/1190000009732550)
- [Linux Namespace : User](https://www.cnblogs.com/sparkdev/p/9462838.html)
- [链接器命名空间](https://source.android.com/devices/architecture/vndk/linker-namespace?hl=zh_cn)
- [走进docker系列：开篇](https://segmentfault.com/a/1190000009309276)
- [3分钟快速了解Docker的底层原理](https://www.toutiao.com/article/7106794699364745768)
- [命名空间介绍之五：用户命名空间](https://cloud.tencent.com/developer/article/1531673)
- [LXC容器](https://www.cnblogs.com/chendeqiang/p/14318770.html)
- [关于docker:彻底搞懂容器技术的基石-namespace-下](https://lequ7.com/guan-yu-docker-che-di-gao-dong-rong-qi-ji-shu-de-ji-shi-namespace-xia.html)
- [徹底搞懂容器技術的基石：namespace（下）](https://copyfuture.com/blogs-details/202112141432392239)
- [一篇搞懂容器技术的基石： cgroup](https://zhuanlan.zhihu.com/p/434731896)
- [搞懂容器技术的基石： namespace （上）](https://zhuanlan.zhihu.com/p/443605569)
- [使用eBPF LSM热修复Linux内核漏洞](https://mp.weixin.qq.com/s/CDEr3aM1MyxR_g5u_X0rGQ)
- [Docker 学习笔记12 容器技术原理 User Namespace](https://blog.csdn.net/xundh/article/details/106780266)

- [探秘 Docker 容器化技术黑科技 Cgroups](https://mp.weixin.qq.com/s/zVmwzZ5WC4cbp19CrAtFdA)

- [使用 Linux 网络虚拟化技术探究容器网络原理](https://mp.weixin.qq.com/s/9p8-qeIMvAPBwZBh4YEhTg)

- [LINUX系统安全_SANDBOX](https://codeantenna.com/a/JHdL7Auz5n)
- [内核是如何给容器中的进程分配CPU资源的？](https://mp.weixin.qq.com/s/Fw4gE2d0hnRJX5iQfkStQA)
- [从 500 行 C 代码全面解析 Linux 容器底层工作机制](https://mp.weixin.qq.com/s/BnYtkQO03MR8KnxRX7lWLg)

## cgroup

- [Linux Control Groups V1 和 V2 原理和区别](https://mikechengwei.github.io/2020/06/03/cgroup%E5%8E%9F%E7%90%86/)

CPU 控制组：可以使用 cgroup 创建任务组和把进程加入任务组。cgroup 已经从版本 1（cgroup v1）演进到版本 2（cgroup v2）：

- 版本 1 可以创建多个控制组层级树
- 版本 2 只有一个控制组层级树
