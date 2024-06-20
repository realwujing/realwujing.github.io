# qemu-kvm

- [<font color=Red>虚拟化指南</font>](https://documentation.suse.com/zh-cn/sles/15-SP4/html/SLES-all/cha-kvm.html)
- [<font color=Red>Linux虚拟化</font>](https://www.cnblogs.com/LoyenWang/category/1828942.html)

- [服务器虚拟化组件有哪些？](http://c.biancheng.net/view/3842.html)
- [ubuntu18.04上搭建KVM虚拟机环境超完整过程](https://mp.weixin.qq.com/s/FVyzPVwwQ85AC4jlVZvF4g)

- <https://www.qemu.org/docs/master/system/invocation.html>
- [QEMU (简体中文)](https://wiki.archlinuxcn.org/wiki/QEMU)
- [QEMU-KVM基本原理](https://www.toutiao.com/article/7194721406406787623)
- [<font color=Red>Qemu&KVM 第一篇（2） qemu kvm 相关知识</font>](https://blog.csdn.net/weixin_34253539/article/details/93084893)
- [<font color=Red>虚拟化技术 — 硬件辅助的虚拟化技术</font>](https://mp.weixin.qq.com/s/NsdNFhoP0QwjfhsQIeRWeQ)
- [<font color=Red>虚拟化技术 — QEMU-KVM 基于内核的虚拟机</font>](https://mp.weixin.qq.com/s/sn-TTwldA81uFuVn5NBMhg)

- [<font color=Red>https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu</font>](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu)
- [<font color=Red>Linux内核调试</font>](https://blog.csdn.net/onlylove_/category_11607029.html)

## virt-manager

- [<font color=Red>虚拟化入门指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_getting_started_guide/index)
- [<font color=Red>虚拟化部署和管理指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/index)
- [<font color=Red>How to Install and Configure KVM on Debian 11 Bullseye Linux</font>](https://linux.how2shout.com/how-to-install-and-configure-kvm-on-debian-11-bullseye-linux/)

  ```bash
  sudo apt install virt-manager -y
  sudo virsh net-list --all
  sudo virsh net-start default
  sudo virsh net-autostart default
  ```

  uos20-1060 arm64建议通过下方命令安装：

  ```bash
  sudo apt-get install libvirt0 libvirt-daemon qemu virt-manager bridge-utils libvirt-clients python-libvirt qemu-efi uml-utilities virtinst qemu-system libvirt-daemon-system
  sudo virsh net-list --all
  sudo virsh net-start default
  sudo virsh net-autostart default
  ```

### libvirt

- [KVM虚拟化解决方案系列之KVM管理工具-libvirt介绍篇](https://blog.csdn.net/jianghu0755/article/details/129776841)
- [CentOS创建KVM虚拟机-在Edit→Preferences里面开启XML文件编辑功能](https://tinychen.com/20200405-centos-create-kvm-vm/)
- [<font color=Red>libvirt的virsh命令和qemu参数转换</font>](https://blog.csdn.net/YuZhiHui_No1/article/details/53909925)
- [libvirt and QEMU 基础篇](https://blog.csdn.net/lingshengxiyou/article/details/128665491)
- [KVM虚拟化解决方案系列之KVM管理工具-libvirt介绍篇](https://blog.csdn.net/jianghu0755/article/details/129776841)
- [CentOS创建KVM虚拟机-在Edit→Preferences里面开启XML文件编辑功能](https://tinychen.com/20200405-centos-create-kvm-vm/)
- [<font color=Red>libvirt的virsh命令和qemu参数转换</font>](https://blog.csdn.net/YuZhiHui_No1/article/details/53909925)
- [libvirt and QEMU 基础篇](https://blog.csdn.net/lingshengxiyou/article/details/128665491)

### virsh

- [virsh命令来创建虚拟机](https://www.cnblogs.com/klb561/p/8724487.html)
- [<font color=Red>虚拟化部署和管理指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/index)
- [15.5 更改引导选项](https://documentation.suse.com/zh-cn/sles/15-SP2/html/SLES-all/cha-libvirt-config-virsh.html#sec-libvirt-config-boot-menu-virsh)
- [libvirt-使用iso镜像创建主机&修改启动盘&启动](https://blog.csdn.net/qq_25730711/article/details/72835565)
- [Libvirt: how to pass qemu command line args?](https://unix.stackexchange.com/questions/235414/libvirt-how-to-pass-qemu-command-line-args)

#### virt-install

- [CentOS 7 virt-install 命令行方式（非图形界面）安装KVM虚拟机](https://blog.csdn.net/mshxuyi/article/details/99852820)

创建一个名为 yql-ctyunos 的虚拟机，配置了适当的内存、CPU、磁盘、安装位置以及启动参数，以便正确连接到串口控制台和控制台输出:

```bash
virt-install \
  --name yql-ctyunos \
  --ram 32768 \
  --vcpus 64 \
  --disk path=/inf/yql/yql-ctyunos.qcow2,size=200 \
  --location /inf/yql/ctyunos-22.09-240618-x86_64-dvd.iso \
  --os-type generic \
  --network default \
  --graphics none \
  --console pty,target_type=serial \
  -x 'console=ttyS0,115200n8 console=tty0' \
  --extra-args 'console=ttyS0,115200n8'
```

参数说明:

- `--name yql-ctyunos`：设置虚拟机的名称为 `yql-ctyunos`。
- `--ram 32768`：分配 32GB 内存给虚拟机（单位为 MB）。
- `--vcpus 64`：分配 64 个虚拟 CPU 给虚拟机。
- `--disk path=/inf/yql/yql-ctyunos.qcow2,size=200`：指定虚拟机的磁盘文件路径和大小为 200GB。
- `--location /inf/yql/ctyunos-22.09-240618-x86_64-dvd.iso`：指定用于安装的 ISO 文件的位置。
- `--os-type generic`：指定操作系统类型为通用类型。
- `--network default`：指定虚拟机的网络接口为默认网络。
- `--graphics none`：禁用图形界面。
- `--console pty,target_type=serial`：设置虚拟机的控制台类型为串口控制台。
- `-x 'console=ttyS0,115200n8 console=tty0'`：启动参数，设置串口控制台和控制台输出。
- `--extra-args 'console=ttyS0,115200n8'`：额外的启动参数，设置串口控制台输出。

#### 克隆一个虚拟机

```bash
virt-clone --original yql --name wujing --file wujing.qcow2
```

#### 启动虚拟机

```bash
virsh start wujing
```

#### 将虚拟机vcpu扩充到16核心

```bash
virsh shutdown wujing
virsh edit wujing   # 修改vcpu字段
virsh start wujing
```

重启虚拟机后验证是否16核心：

```bash
nproc
```

#### 删除虚拟机

- [KVM 报错以及处理](https://www.cnblogs.com/loudyten/articles/10233268.html)

使用 `virsh` 命令中取消定义一个带有 NVRAM 的虚拟机:

```bash
virsh undefine wujing --nvram
```

参数说明:

- `undefine` 表示取消定义（即删除）一个虚拟机。
- `--nvram` 选项用于指示虚拟机具有 NVRAM（非易失性随机存储器），需要一并删除。
- `wujing` 是要删除的虚拟机的名称或定义文件的路径。

### 共享剪切板

- [qemu/kvm linux 虚拟机配置（共享剪切版，文件拖拽进虚拟机）](https://blog.csdn.net/qq_33831360/article/details/123700719)
- [How can I copy&paste from the host to a KVM guest?](https://askubuntu.com/questions/858649/how-can-i-copypaste-from-the-host-to-a-kvm-guest)

  在虚拟机上安装spice-vdagent即可：

  ```bash
  sudo apt install spice-vdagent
  ```

### 共享目录

在宿主机查看虚拟机共享目录配置:

![共享目录](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240202130011.png)

在虚拟机上执行下方命令：

```bash
sudo mount -t 9p -o trans=virtio /media/wujing/data /media/wujing/data
```

如果在虚拟机上想启动就默认挂载，可以写入 /etc/fstab：

```bash
sudo cat << EOF >> /etc/fstab
/media/wujing/data /media/wujing/data 9p trans=virtio 0 0
EOF
```

在虚拟机上重新挂载/etc/fstab：

```bash
sudo systemctl daemon-reload
sudo mount -a
```

- [virt-manager设置主机和虚拟机之间文件共享](https://blog.csdn.net/sinat_38816924/article/details/120284285)

### 时间

- [解决deepin虚拟机系统时间不正确的问题](https://blog.csdn.net/fcdm_/article/details/122150246)

    ```bash
    sudo apt install systemd-timesyncd
    ```

### 网络

- [<font color=Red>QEMU用户模式网络</font>](https://blog.csdn.net/m0_43406494/article/details/124827927)
- [<font color=Red>QEMU 网络配置</font>](https://tomwei7.com/2021/10/09/qemu-network-config/)
- [<font color=Red>Linux 内核调试 七：qemu网络配置</font>](https://blog.csdn.net/OnlyLove_/article/details/124536607)
- [<font color=Red>详解QEMU网络配置的方法</font>](https://www.jb51.net/article/97216.htm)
- [详解：VirtIO Networking 虚拟网络设备实现架构](https://mp.weixin.qq.com/s/aFOcuyypU1KdcdOecs8DdQ)
- [虚拟化技术 — Libvirt 异构虚拟化管理组件](https://mp.weixin.qq.com/s/oEDuaFs7DQM3zMCIx_CiFA)
- [<font color=Red>Ubuntu 20.04 物理机 QEMU-KVM + Virt-Manager 创建桥接模式的虚拟机</font>](https://www.cnblogs.com/whjblog/p/17213359.html)
- [[Debian10]使用KVM虚拟机并配置桥接网络](https://www.cnblogs.com/DouglasLuo/p/12731591.html)

## qemu monitor

- [36 使用 QEMU 监视器管理虚拟机](https://documentation.suse.com/zh-cn/sles/15-SP4/html/SLES-all/cha-qemu-monitor.html)
- [QEMU Monitor](https://www.qemu.org/docs/master/system/monitor.html)
- [QEMU monitor控制台使用详解](https://blog.csdn.net/qq_43523618/article/details/106278245)
- [如何退出 QEMU 退出快捷键：Ctrl + a，然后按 x 键。](https://zhuanlan.zhihu.com/p/518032838)

## VIFO透传

- [KVM设备透传与重定向](https://blog.csdn.net/zhongbeida_xue/article/details/103602105)
- [19.3.2. 将 USB 设备附加到虚拟机](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/sect-the_virtual_hardware_details_window-attaching_usb_devices_to_a_guest_virtual_machine)
- [19.3.3. USB 重定向](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/sect-the_virtual_hardware_details_window-usb_redirection)
- [USB虚拟化](https://blog.csdn.net/jerryxiao1979/article/details/128453912)
- [unraid 直通usb](https://juejin.cn/s/unraid%20%E7%9B%B4%E9%80%9Ausb)
- [libvirt-usb设备透传给虚拟机](http://www.manongjc.com/detail/50-cxzuhdjzmdqrklm.html)

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

## 显示虚拟化

- [QEMU显示虚拟化的几种选项](https://blog.csdn.net/tugouxp/article/details/134487575)

## book

- [QEMU KVM学习笔记](https://github.com/yifengyou/learn-kvm)
- [kvm虚拟化技术：实战与原理解析.pdf](http://www.downcc.com/soft/317578.html)
- [QEMU/KVM源码解析与应用-李强编著-微信读书](https://weread.qq.com/web/bookDetail/ec132be07263ffc1ec1dc10)

## openstack

- [什么是OpenStack？](https://info.support.huawei.com/info-finder/encyclopedia/zh/OpenStack.html)
- [OpenStack是什么？](https://c.biancheng.net/view/3892.html)
- [OpenStack Installation Guide for Ubuntu](https://docs.openstack.org/mitaka/zh_CN/install-guide-ubuntu/)
- [超详细ubuntu20安装搭建openstack](https://blog.csdn.net/qq_53348314/article/details/123021856)
- [Ubuntu 20使用devstack快速安装openstack最新版](https://www.cnblogs.com/dyd168/p/14476271.html)

## More

- [Wine 开发者指导/架构概览](https://blog.csdn.net/Flora_xuan1993/article/details/89205922)
- [如何用命令行模式启动VMWare虚拟机](https://blog.csdn.net/u014389734/article/details/107481852)
- [在 Linux 主机上安装 Workstation Pro](https://docs.vmware.com/cn/VMware-Workstation-Pro/17/com.vmware.ws.using.doc/GUID-1F5B1F14-A586-4A56-83FA-2E7D8333D5CA.html)
- [在Ubuntu 20.04上安装VMWare Workstation](https://waydo.xyz/soft/linux/ubuntu-vmware-workstation/)
- [用 archinstall 自动化脚本安装 Arch Linux](https://linux.cn/article-14444-1.html)
- [Arch Linux图文安装教程（2022.08.01）](https://blog.csdn.net/love906897406/article/details/126109464)

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
- [UTM - Virtual machines for iOS and macOS](https://github.com/utmapp/UTM)
- [Parallels Desktop 19.2.1 54832 破解教程（pd19永久授权）](https://www.luoxx.top/archives/pd-18-active?cid=162)
- [如何轻松扩展Parallel Desktop下的Ubuntu虚拟机磁盘空间](https://blog.coderluny.com/archives/53)

### vmware

- [安装 Open VM Tools](https://docs.vmware.com/cn/VMware-Tools/12.4.0/com.vmware.vsphere.vmwaretools.doc/GUID-C48E1F14-240D-4DD1-8D4C-25B6EBE4BB0F.html)

Ubuntu、Debian 及相关操作系统

如果虚拟机具有 GUI（X11 等），请安装或升级 open-vm-tools-desktop：

```bash
sudo apt-get install open-vm-tools-desktop
```

否则，请使用以下命令安装 open-vm-tools：

```bash
sudo apt-get install open-vm-tools
```

RHEL、Fedora 和 CentOS

如果虚拟机具有 GUI（X11 等），请安装或升级 open-vm-tools-desktop：

```bash
sudo yum install open-vm-tools-desktop
```

否则，请安装 open-vm-tools：

```bash
sudo yum install open-vm-tools
```
