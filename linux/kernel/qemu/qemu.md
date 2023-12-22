# qemu

## 虚拟机

- [服务器虚拟化组件有哪些？](http://c.biancheng.net/view/3842.html)
- [ubuntu18.04上搭建KVM虚拟机环境超完整过程](https://mp.weixin.qq.com/s/FVyzPVwwQ85AC4jlVZvF4g)
- [<font color=Red>How to Install and Configure KVM on Debian 11 Bullseye Linux</font>](https://linux.how2shout.com/how-to-install-and-configure-kvm-on-debian-11-bullseye-linux/)

  ```bash
  sudo apt install virt-manager -y
  sudo virsh net-list --all
  sudo virsh net-start default
  sudo virsh net-autostart default
  ```

- [如何从主机粘贴到KVM客户机？](https://stc214.github.io/posts/2021%E5%B9%B42%E6%9C%886%E6%97%A512/)
- [Copy n Paste in (KVM) Kernel-based Virtual Machine](https://www.linuxsecrets.com/3883-copy-n-paste-in-kvm)
- [解决kvm安装Ubuntu虚机远程桌面无法复制粘贴问题](https://blog.csdn.net/guoyinzhao/article/details/109642824)
- [解决deepin虚拟机系统时间不正确的问题](https://blog.csdn.net/fcdm_/article/details/122150246)

    ```bash
    sudo apt install systemd-timesyncd
    ```

- [如何用命令行模式启动VMWare虚拟机](https://blog.csdn.net/u014389734/article/details/107481852)
- [在 Linux 主机上安装 Workstation Pro](https://docs.vmware.com/cn/VMware-Workstation-Pro/17/com.vmware.ws.using.doc/GUID-1F5B1F14-A586-4A56-83FA-2E7D8333D5CA.html)
- [在Ubuntu 20.04上安装VMWare Workstation](https://waydo.xyz/soft/linux/ubuntu-vmware-workstation/)
- [用 archinstall 自动化脚本安装 Arch Linux](https://linux.cn/article-14444-1.html)
- [Arch Linux图文安装教程（2022.08.01）](https://blog.csdn.net/love906897406/article/details/126109464)

- [KVM虚拟化解决方案系列之KVM管理工具-libvirt介绍篇](https://blog.csdn.net/jianghu0755/article/details/129776841)

### Mac OSX

- [<font color=Red>使用Qemu在Mac上安装虚拟机</font>](https://blog.csdn.net/weixin_39759247/article/details/126569448)
- [使用QEMU在macOS上创建Ubuntu 20.04桌面虚拟机](https://www.arthurkoziel.com/qemu-ubuntu-20-04/)
- [Running virt-manager and libvirt on macOS](https://www.arthurkoziel.com/running-virt-manager-and-libvirt-on-macos/)
- [Mac安装Linux的KVM管理工具virt-manager](https://blog.csdn.net/weixin_30883777/article/details/95729678)
- [homebrew-virt-manager](https://github.com/jeffreywildman/homebrew-virt-manager)
- [macOS系统和mac装Windows系统开启虚拟化](https://blog.csdn.net/nbin_newby/article/details/120307866)
- [macOS | nvram boot-args的作用及设置方式](https://blog.csdn.net/MissMango0820/article/details/127398047)
- [GitHub - kholia/OSX-KVM: Run macOS on QEMU/KVM. With OpenCore + Big Sur + Monterey + Ventura support now! Only commercial (paid) support is available now to avoid spammy issues. No Mac system is required.](https://github.com/kholia/OSX-KVM)
- [Linux kernel debug on macOS 搭建可视化内核debug环境](https://zhuanlan.zhihu.com/p/399857241)

## qemu

- [CentOS创建KVM虚拟机-在Edit→Preferences里面开启XML文件编辑功能](https://tinychen.com/20200405-centos-create-kvm-vm/)
- [<font color=Red>libvirt的virsh命令和qemu参数转换</font>](https://blog.csdn.net/YuZhiHui_No1/article/details/53909925)
- <https://www.qemu.org/docs/master/system/invocation.html>
- [QEMU (简体中文)](https://wiki.archlinuxcn.org/wiki/QEMU)
- [QEMU-KVM基本原理](https://www.toutiao.com/article/7194721406406787623)
- [<font color=Red>Qemu&KVM 第一篇（2） qemu kvm 相关知识</font>](https://blog.csdn.net/weixin_34253539/article/details/93084893)
- [<font color=Red>虚拟化技术 — 硬件辅助的虚拟化技术</font>](https://mp.weixin.qq.com/s/NsdNFhoP0QwjfhsQIeRWeQ)
- [<font color=Red>虚拟化技术 — QEMU-KVM 基于内核的虚拟机</font>](https://mp.weixin.qq.com/s/sn-TTwldA81uFuVn5NBMhg)
- [libvirt and QEMU 基础篇](https://blog.csdn.net/lingshengxiyou/article/details/128665491)

- [<font color=Red>https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu</font>](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu)
- [<font color=Red>Linux内核调试</font>](https://blog.csdn.net/onlylove_/category_11607029.html)

- [如何退出 QEMU 退出快捷键：Ctrl + a，然后按 x 键。](https://zhuanlan.zhihu.com/p/518032838)

### qemu monitor

- [QEMU Monitor](https://www.qemu.org/docs/master/system/monitor.html)
- [QEMU monitor控制台使用详解](https://blog.csdn.net/qq_43523618/article/details/106278245)

### 网络

- [<font color=Red>QEMU用户模式网络</font>](https://blog.csdn.net/m0_43406494/article/details/124827927)
- [<font color=Red>QEMU 网络配置</font>](https://tomwei7.com/2021/10/09/qemu-network-config/)
- [<font color=Red>Linux 内核调试 七：qemu网络配置</font>](https://blog.csdn.net/OnlyLove_/article/details/124536607)
- [<font color=Red>详解QEMU网络配置的方法</font>](https://www.jb51.net/article/97216.htm)
- [详解：VirtIO Networking 虚拟网络设备实现架构](https://mp.weixin.qq.com/s/aFOcuyypU1KdcdOecs8DdQ)
- [虚拟化技术 — Libvirt 异构虚拟化管理组件](https://mp.weixin.qq.com/s/oEDuaFs7DQM3zMCIx_CiFA)

## x86_64下qemu虚拟x86_64

- [<font color=Red>使用 VSCode + qemu 搭建 Linux 内核调试环境</font>](https://howardlau.me/programming/debugging-linux-kernel-with-vscode-qemu.html)
- [Linux内核源码远程调试（3.16.84）](https://www.cnblogs.com/harmful-chan/p/12994693.html)
- [[kernel]linux内核基础:版本、源码、编译与调试](https://blog.csdn.net/Breeze_CAT/article/details/123787636)
- [[首发][Ubuntu]VSCode搭建Linux Kernel单步调试IDE环境](https://blog.csdn.net/rlk8888/article/details/122396219)
- [<font color=Red>使用VSCode + qemu搭建Linux内核调试环境</font>](https://blog.csdn.net/eydwyz/article/details/114019532)
- [<font color=Red>利用QEMU+GDB搭建Linux内核调试环境</font>](https://bbs.huaweicloud.com/blogs/348654)
- [Linux内核调试方法](https://mp.weixin.qq.com/s/KzFFkJHLzbNFixiELMjNWg)
- [<font color=Red>基于 debootstrap 和 busybox 构建 mini ubuntu</font>](https://www.cnblogs.com/fengyc/p/6114648.html)
- [https://manpages.ubuntu.com/manpages/xenial/en/man1/debirf.1.html](https://manpages.ubuntu.com/manpages/xenial/en/man1/debirf.1.html)

- [QEMU 实验（三）：根文件系统构建 (基于 Busybox)](https://www.jianshu.com/p/b4bae215e278)
- [怎么用QEMU搭建Linux kernel开发调试环境](https://www.yisu.com/zixun/503146.html)
- [嵌入式开发模拟器：qemu使用，仿真多种方式启动内核kernel (超详细，tftp/nfs等方式)](https://blog.csdn.net/birencs/article/details/126666827)
- [使用qemu模拟调试内核和debian根文件系统](https://www.bbsmax.com/A/WpdK7VrodV/)
- [QEMU 文件系统制作：自己制作根目录和应用程序 + BUSYBOX](https://www.freesion.com/article/2442234327/)
- [QemuDebootstrap](https://wiki.ubuntu.com/ARM/RootfsFromScratch/QemuDebootstrap)
- [<font color=Red>内核调试环境：buildroot/debootstrap制作文件系统、编译内核、QEMU模拟</font>](https://blog.csdn.net/weixin_49393427/article/details/126435589)
- [<font color=Red>使用 debootstrap 制作 ARM64 rootfs.cpio</font>](https://blog.51cto.com/u_13731941/5399257)
- [<font color=Red>debootstrap 制作根文件系统</font>](https://www.cnblogs.com/huaibovip/p/debootstrap-fs.html)

- [使用Buildroot + QEMU构建和运行Linux](https://blog.csdn.net/xunknown/article/details/124521135)
- [制作基于beaglebonebalck的rootfs使用buildroot](https://blog.csdn.net/xingkong0/article/details/100586485)
- [ubuntu下qemu使用：图文详解](https://blog.csdn.net/qq_34160841/article/details/104891169)
- [QEMU调试Linux内核环境搭建](https://www.toutiao.com/article/7086755948068487687)
- [一文分析Linux虚拟化KVM-Qemu（概念篇）](https://www.toutiao.com/article/7182563085466108471)

- [<font color=Red>qemu: usb存储设备仿真</font>](https://blog.csdn.net/kingtj/article/details/82952783)
- [如何在 Linux 中导出和导入 KVM 虚拟机](https://www.51cto.com/article/708013.html)

- [<font color=Red>ARMv8架构下修改Linux内核并打开kvm硬件虚拟化支持（平台Firefly-rk3568）</font>](https://blog.csdn.net/qq_40598297/article/details/121084904)

- [<font color=Red>制作一个grub虚拟启动盘，在qemu下调试</font>](https://blog.csdn.net/qq_41957544/article/details/105769697)

- [Running a full system stack under QEMUarm64](https://cdn.kernel.org/pub/linux/kernel/people/will/docs/qemu/qemu-arm64-howto.html)
- [qemu模拟vexpress开发板](https://www.cnblogs.com/bigsissy/p/11134802.html)
- [<font color=Red>第四期 QEMU调试Linux内核实验 《虚拟机就是开发板》</font>](https://blog.csdn.net/aggresss/article/details/54946438)

## arm64下qemu虚拟arm64

- [ARM平台检测是否支持虚拟化的几种常见方法](https://blog.csdn.net/sinat_34833447/article/details/109765004)
- [X86_64 环境下使用 QEMU 虚拟机安装 ARM 版 EulerOS 小记](https://www.txisfine.cn/archives/a0d5fa12)
- [利用qemu-system-aarch64调试Linux内核（arm64）](https://blog.csdn.net/Oliverlyn/article/details/105178832)
- [QEMU搭建arm64 Linux调试环境](https://zhuanlan.zhihu.com/p/345232459)

## x86_64下qemu虚拟arm64

- [VSCode+GDB+Qemu调试ARM64 linux内核](https://zhuanlan.zhihu.com/p/510289859)
- [编译arm64内核](https://blog.csdn.net/fell_sky/article/details/119818112)
- [交叉编译arm64内核](https://blog.csdn.net/shanruo/article/details/80474338)
- [交叉编译环境下对linux内核编译](https://blog.csdn.net/ludaoyi88/article/details/115633849)
- [交叉编译linux内核并使用qemu运行](https://blog.csdn.net/jinking01/article/details/129580621)

## buildroot

- [使用Buildroot + QEMU构建和运行Linux](https://blog.csdn.net/xunknown/article/details/124521135)
- [最新Buildroot2021.08.1搭建qemu环境](https://zhuanlan.zhihu.com/p/426026299)
- [<font color=Red>从0开始使用QEMU模拟ARM开发环境之buildroot构建linux根文件系统</font>](https://blog.csdn.net/leacock1991/article/details/113730672)
- [Buildroot 切换到国内源](https://blog.csdn.net/lsg19920625/article/details/130837469)
- [<font color=Red>The Buildroot user manual</font>](https://buildroot.org/downloads/manual/manual.html)
- [error: 'ARPHRD_MCTP' undeclared (first use in this function)#20694](https://github.com/systemd/systemd/issues/20694)
- [basic/linux: Sync if_arp.h with Linux 5.14 #20695](https://github.com/systemd/systemd/pull/20695/commits/67cd626399b0d02882ee00716c8bd31ba764c862)
- buildroot/board/qemu/aarch64-virt/readme.txt

    qemu_aarch64_virt_defconfig：

    ```bash
      qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -smp 1 -kernel output/images/Image -append "rootwait root=/dev/vda console=ttyAMA0" -netdev user,id=eth0 -device virtio-net-device,netdev=eth0 -drive file=output/images/rootfs.ext4,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
    ```

## loongarch

- [龙芯虚拟化使用手册](https://blog.csdn.net/faxiang1230/article/details/120907896)

## book

- [QEMU KVM学习笔记](https://github.com/yifengyou/learn-kvm)
- [kvm虚拟化技术：实战与原理解析.pdf](http://www.downcc.com/soft/317578.html)
- [QEMU/KVM源码解析与应用-李强编著-微信读书](https://weread.qq.com/web/bookDetail/ec132be07263ffc1ec1dc10)
