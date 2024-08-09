---
date: 2024/01/30 20:06:42
updated: 2024/04/26 16:13:54
---


# GUI

- [深夜磨刀，Linux Graphics Stack 概述 | Linux 驱动](https://zhuanlan.zhihu.com/p/414403029)
- [深度探索Linux操作系统 —— Linux图形原理探讨](https://blog.csdn.net/Liuqz2009/article/details/134988734)
- [GPU入门](https://qiankunli.github.io/2021/08/18/gpu.html)

## 桌面环境

- [<font color=Red>Desktop environment (简体中文)</font>](https://wiki.archlinux.org/title/Desktop_environment_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87))
- [【转】四大Linux图形界面赏析：KDE、Gnome、Xfce、LXDE](https://blog.csdn.net/chantal20080409/article/details/82986283)
- [<font color=Red>GTK、KDE、Gnome、XWindows图形界面</font>](https://blog.csdn.net/iteye_4195/article/details/82522264)
- [Linux桌面环境（桌面系统）大比拼[附带优缺点]](http://c.biancheng.net/view/2912.html)

### GNOME 桌面

```bash
sudo apt install task-gnome-desktop
```

- [https://wiki.debian.org/zh_CN/Gnome](https://wiki.debian.org/zh_CN/Gnome)

### 显示管理器

- [<font color=Red>Linux图形界面：X,X11,XFREE,WM,KDE,GNOME的关系</font>](http://www.javashuo.com/article/p-yftqzthz-cp.html)

- [<font color=Red>Linux 知识分享：显示管理器和桌面环境容易混淆？分分钟带你脱离认识误区</font>](https://zhuanlan.zhihu.com/p/272740410)
- [<font color=Red>「小白课程」openKylin登录&锁屏简要介绍</font>](https://www.toutiao.com/article/7174576146997625352)

### 显示服务器

- [Linux黑话解释：什么是显示服务器，用来做什么？](https://www.toutiao.com/article/6869367787744133636/)
- [X11和Wayland的区别，一点感悟](https://blog.csdn.net/sunxiaopengsun/article/details/119895985)

#### x11

- [<font color=Red>linux图形窗口这家子(xwindows x11 xserver xclient窗口管理器 xdm xwm kde QT GTK+)</font>](https://blog.csdn.net/u014305876/article/details/89475789)
- [Linux图像系统框架-理解X11与Qt的层次结构](https://www.cnblogs.com/newjiang/p/8414625.html)
- [[GUI] QT事件与X11的关系](https://www.cnblogs.com/yongpenghan/p/4555634.html)
- [Qt主线程卡死，竟然与X11的_XReply()有关](https://www.cnblogs.com/winafa/p/14206600.html)
- [Xsession :warning:unable to write to /tmp; X session may exit with an error问题解决](https://blog.csdn.net/moyu123456789/article/details/90483108)
- [Ubuntu系统开机后类似死机（鼠标键盘失效 但系统时间仍在正常更新）解决办法](https://www.cnblogs.com/yutian-blogs/p/13549657.html)

##### multiseat

- [Xorg multiseat](https://wiki.archlinux.org/title/Xorg_multiseat)
- [Multi_Seat_Debian_HOWTO](https://wiki.debian.org/Multi_Seat_Debian_HOWTO)
- [Multiseat on Debian](https://kcore.org/2010/04/04/multiseat-on-debian/)
- [MultiseatOneCard  多座一卡](https://help.ubuntu.com/community/MultiseatOneCard)

##### 窗口管理器

- [<font color=Red>Window manager (简体中文)</font>](https://wiki.archlinux.org/title/Window_manager_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87))
- [linux（ubuntu）系统什么叫：桌面管理器，窗口管理器？](https://my.oschina.net/aspirs/blog/607710)
- [Linux中桌面环境和窗口管理器的区别](https://geek-docs.com/linux/linux-ask-answer/difference-between-desktop-environment-vs-window-manager-in-linux.html)

#### wayland

- [linux wayland qt,详解Qt Lighthouse和Wayland](https://blog.csdn.net/weixin_36156325/article/details/116895549)
- [通过docker使用wayland和x11的gui程序](https://blog.csdn.net/yogoloth/article/details/105683815)
- [Wayland开发入门系列4：xserver](https://blog.csdn.net/qq_26056015/article/details/122406051)
- [<font color=Red>Wayland开发入门</font>](https://blog.csdn.net/qq_26056015/category_11559440.html)
- [Wayland是一个简单的“显示服务器”（Display Server）](https://www.baike.com/wikiid/3479851875664899506)
- [Linux图形栈一览：基于DRM和Wayland](https://blog.csdn.net/M120674/article/details/123534336)

## gpu

- [Linux图形显示系统之DRM](https://blog.csdn.net/yangguoyu8023/article/details/129241987)
- [GPGPU&&渲染GPU的工作原理和认知总结](https://blog.csdn.net/tugouxp/article/details/126594480)
- [Linux显卡驱动，DRM显示框架简单介绍](https://www.toutiao.com/article/6973922609868063264)
- [Linux显卡驱动，DRM Atomic接口简说](https://www.toutiao.com/article/6982072379140784670)
- [Linux显卡驱动，TTM内存管理介绍](https://www.toutiao.com/article/6989969291902763558)

- [drm gpu scheduler](https://blog.csdn.net/xuelin273/article/details/131297186)
- [GPU 调度 - Linux 实现](https://mp.weixin.qq.com/s/_oe409y93Qm5l3j3o_-P8Q)

- [【干货】一文看懂GPU流处理器](https://mp.weixin.qq.com/s/IHardB0dyky8fhgQkU55pw)
- [【干货】三张图看懂主流GPU性能](https://mp.weixin.qq.com/s/bNGBoeKnvcEB7prpDeAXeQ)
- [【干货】一文看懂CPU与GPU异构计算](https://mp.weixin.qq.com/s/xGwaaaaMED23dNbO9wNICg)
- [Linux 显示子系统之 Framebuffer 与 DRM](https://mp.weixin.qq.com/s/apXuHnOTpKq0Za8VsFhM0A)

### admgpu

在 AMD GPU 驱动中，gfx（Graphics Core Next） Ring Buffer 和 uvd（Unified Video Decoder） Ring Buffer 分别用于图形和视频解码任务，是 GPU 调度器中的两个不同的环形缓冲区。以下是它们之间的主要区别：

gfx Ring Buffer（图形环形缓冲区）:

- 用途： 用于处理图形渲染和计算任务。
- 任务类型： 包括图形渲染管道中的图形指令和通用计算任务。
- 关联的任务： 与图形 API（如 OpenGL、Vulkan）相关联，用于执行图形任务。

uvd Ring Buffer（视频解码环形缓冲区）:

- 用途： 用于处理硬件视频解码任务。
- 任务类型： 专注于解码视频流，支持 Unified Video Decoder（UVD）功能。
- 关联的任务： 与视频解码 API 相关，例如在视频播放中执行解码任务。

总的来说，这两个环形缓冲区服务于不同类型的 GPU 任务。gfx Ring Buffer 处理与图形渲染和通用计算相关的任务，而 uvd Ring Buffer 专门处理硬件视频解码任务。它们在 GPU 调度器中的调度和执行上有所区别，以满足图形和视频处理的不同需求。

- [<font color=Red>AMD GPU任务调度（1）—— 用户态分析</font>](https://blog.csdn.net/huang987246510/article/details/106658889)
- [<font color=Red>AMD GPU任务调度（2）—— 内核态分析</font>](https://blog.csdn.net/huang987246510/article/details/106737570)
- [AMD GPU任务调度（3） —— fence机制](https://blog.csdn.net/huang987246510/article/details/106865386)

- [<font color=Red>AMD GPU 内核驱动分析(一)总览</font>](https://blog.csdn.net/tugouxp/article/details/132819114)
- [AMD GPU 内核驱动分析(二)-video-ring的调度](https://blog.csdn.net/tugouxp/article/details/132953439)
- [AMD GPU 内核驱动分析(三)-gpu scheduler ring fence同步工作模型](https://blog.csdn.net/tugouxp/article/details/133519133)

#### virtio

- [VFIO硬件依赖——IOMMU机制](https://blog.csdn.net/huang987246510/article/details/106179145)
- [VirtIO-GPU —— 2D加速原理分析](https://blog.csdn.net/huang987246510/article/details/106254294)
- [VirtIO-GPU环境搭建与应用](https://blog.csdn.net/huang987246510/article/details/106245900)
- [Hello OpenGL](https://blog.csdn.net/huang987246510/article/details/106322012)
- [VirtIO GPU基本原理](https://blog.csdn.net/huang987246510/article/details/107729881)
- [DRAW_INDEX与图形流水线](https://blog.csdn.net/huang987246510/article/details/107283374)
- [Android graphical stack and qemu virtualization](https://eshard.com/posts/Android-graphical-stack-virtualization)
