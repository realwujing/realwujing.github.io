---
date: 2023/10/12 21:45:58
updated: 2023/10/13 18:41:39
---

# amd64下基于qemu tap(NAT网络)、debootstrap调试amd64内核、根文件系统

## 1. 编译调试版linux内核

### 安装编译依赖

```bash
sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev
```

### 下载源码

调试标准版内核，下载github代码即可，本文选择tag v5.10-rc7分支：

```bash
git clone https://github.com/torvalds/linux.git
git checkout -b v5.10-rc7 v5.10-rc7
```

调试uos-v20-1054-2内核，下载gerrit代码即可：

```bash
git clone "ssh://ut004487@gerrit.uniontech.com:29418/kernel/x86-kernel" && scp -p -P 29418 ut004487@gerrit.uniontech.com:hooks/commit-msg "x86-kernel/.git/hooks/"
git checkout -b 1054-2 499e91c36f62c1790063cabdacff94fd8220f145
```

### 内核编译选项配置

进入源码目录，配置编译选项：

```bash
cd linux
make menuconfig
```

比较重要的配置项有：

```text
首先内核中支持tap/tuns设备：
Device Drivers  ---> 
     Networking support  --->
        [M] Universal TUN/TAP device driver support
```

```text
Kernel hacking  --->
    [*] Kernel debugging
    Compile-time checks and compiler options  --->
        [*] Compile the kernel with debug info
        [*]   Provide GDB scripts for kernel debugging
```

一定要打开。
还有下面的选项会导致打断点失败，一定要关闭：

```text
Processor type and features ---->
    [] Randomize the address of the kernel image (KASLR)
```

保存并退出 menuconfig，开始编译之旅：

```bash
make -j`expr $(nproc) / 2`
```

- [使用 VSCode + qemu 搭建 Linux 内核调试环境](https://blog.csdn.net/eydwyz/article/details/114019532)

## 2. debootstrap制作根文件系统

### 安装依赖

```bash
sudo apt-get install qemu qemu-user-static binfmt-support debootstrap debian-archive-keyring
```

### 制作文件系统

使用也十分简单，命令格式为：

```bash
sudo debootstrap --arch [平台] [发行版本代号] [构建目录] [镜像地址]
```

以在Deepin 20.7 amd64上构建ubuntu18(bionic) amd64为例，预装ifupdown是因为下方配置网络的时候需要用到：

```bash
sudo su
cd /usr/share/debootstrap/scripts/
cp sid bionic
exit
```

```bash
sudo debootstrap --no-check-gpg \
--arch=amd64 \
--include=ifupdown,net-tools,build-essential,gdb,cmake,openssh-server,vim,bash-completion \
bionic \
linux-rootfs \
http://mirrors.aliyun.com/ubuntu/
```

以在Deepin 20.7 amd64上构建uos-v20-1054(eagle/1054) amd64为例，预装ifupdown是因为下方配置网络的时候需要用到：

```bash
sudo su
cd /usr/share/debootstrap/scripts/
rm eagle -rf
mkdir eagle
cp sid eagle/1054
exit
```

```bash
sudo debootstrap --no-check-gpg \
--arch=amd64 \
--include=ifupdown,net-tools,build-essential,gdb,cmake,openssh-server,vim,bash-completion \
eagle/1054 \
linux-rootfs \
https://pools.uniontech.com/desktop-professional/
```

- [ubuntu 下安装C/C++ 开发编译环境](https://blog.csdn.net/houxian1103/article/details/121886365)

arm64下交叉编译构建amd64需要执行下方命令：

```bash
sudo cp -a /usr/bin/qemu-x86_64-static linux-rootfs/usr/bin/qemu-x86_64-static
```

### 进入文件系统

```bash
wget https://raw.githubusercontent.com/ywhs/linux-software/master/ch-mount.sh
chmod 777 ch-mount.sh
# 执行脚本后，没有报错会进入文件系统，交叉编译时显示 I have no name ，这是因为还没有初始化。
./ch-mount.sh -m linux-rootfs/

debootstrap/debootstrap --second-stage # 交叉编译时执行，初始化文件系统，会把一个系统的基础包初始化
exit
./ch-mount.sh -u linux-rootfs/
# ./ch-mount.sh -m linux-rootfs/
# 再次进入时，执行如下命令即可
# sudo chroot linux-rootfs
```

### 定制文件系统

#### 配置网络

要确保进入文件系统后有网络，可以将 ​​/etc/resolv.conf​​​ 文件拷贝到 ​​linux-rootfs/etc/resolv.conf​​。

```bash
sudo cp ​​/etc/resolv.conf​​​ ​​linux-rootfs/etc/resolv.conf
```

#### ubuntu18(bionic)更换国内镜像源

```bash
# 若是遇到没法拉取 https 源的状况，请先使用 http 源并安装
sudo chroot linux-rootfs
apt install apt-transport-https
cp /etc/apt/sources.list /etc/apt/sources.list.bak
# 把文件内容所有替换为对应阿里源，参见：https://developer.aliyun.com/mirror/?spm=a2c6h.12873639.J_5404914170.29.2feb6235F6x30d
vim /etc/apt/sources.list
echo "deb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ bionic main restricted universe multiverse" >> /etc/apt/sources.list
```

#### 配置 root 用户密码

```bash
passwd
```

#### 建立一个普通用户

```bash
# 这两个环境变量能够自行修改
USER=wujing
HOST=wujing
useradd -G sudo -m -s /bin/bash $USER
passwd $USER
```

### 退出文件系统

```bash
exit
./ch-mount.sh -u linux-rootfs
```

### 制作文件系统镜像(initrd)

当文件系统是`uos-v20-1054(eagle/1054)`时，将下方`bionic`替换为`eagle-1054`。

```bash
vim bionic.sh
```

将下方内容追加到bionic.sh:

```text
#! /bin/bash

dd if=/dev/zero of=bionic.img bs=1M seek=2047 count=1
sudo mkfs.ext4 -F bionic.img
sudo mkdir -p /mnt/bionic
sudo mount -o loop bionic.img /mnt/bionic
sudo cp -a linux-rootfs/. /mnt/bionic/.
sudo umount /mnt/bionic
sudo chmod 666 bionic.img
```

```bash
chmod +x bionic.sh
sudo ./bionic.sh
```

- [内核调试环境：buildroot/debootstrap制作文件系统、编译内核、QEMU模拟](https://blog.csdn.net/weixin_49393427/article/details/126435589)
- [debootstrap 制作根文件系统](https://www.cnblogs.com/huaibovip/p/debootstrap-fs.html)
- [使用 debootstrap 制作 ARM64 rootfs.cpio](https://blog.51cto.com/u_13731941/5399257)

## 3. 创建 tap0 虚拟网卡

宿主机上执行：

```bash
ifconfig
sudo ip tuntap add dev tap0 mode tap
sudo ip link set dev tap0 up
sudo ip address add dev tap0 192.168.2.128/24
ifconfig
```

宿主机需要为虚拟机开启IP数据包转发，即在192.168.2.*网段转发数据：

```bash
sudo su
echo 1 > /proc/sys/net/ipv4/ip_forward   #可能sudo也会权限不够，在su以后执行即可
iptables -t nat -A POSTROUTING -j MASQUERADE
```

- [Linux 内核调试 七：qemu网络配置](https://blog.csdn.net/OnlyLove_/article/details/124536607)
- [在qemu-kvm配置桥接网络](https://www.shuzhiduo.com/A/xl56Dmq0zr/)

## 4. 启动内核并调试

### 依赖安装

```bash
sudo apt install qemu qemu-system qemu-kvm
```

### qemu启动内核并挂载文件系统调试

下方两个命令都行，第一个报warning，第二个file=后面要使用绝对路径。

当文件系统是`uos-v20-1054(eagle/1054)`时，将下方`bionic`替换为`eagle-1054`。

```bash
qemu-system-x86_64 \
-m 2048 \
-kernel ~/code/linux/arch/x86/boot/bzImage \
-hda ~/code/tmp/bionic.img \
-append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" \
-nographic \
-net nic -net tap,ifname=tap0,script=no,downscript=no \
-s -S
```

```bash
qemu-system-x86_64 \
-m 2048 \
-kernel ~/code/linux/arch/x86/boot/bzImage \
-drive format=raw,file=/home/wujing/code/tmp/bionic.img \
-append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" \
-nographic \
-net nic -net tap,ifname=tap0,script=no,downscript=no \
-s -S
```

- [Linux aarch64 编译 & qemu 搭建实验平台 initrd initramfs](https://blog.csdn.net/FJDJFKDJFKDJFKD/article/details/100021609)

### 加载内核调试工具

然后切换到内核源码目录，启动 gdb，不过在启动之前，请向 ~/code/linux/.gdbinit 添加如下内容：

```bash
cd linux
echo "add-auto-load-safe-path ./scripts/gdb/vmlinux-gdb.py" >> .gdbinit
```

加载内核调试工具，然后执行：

```bash
gdb vmlinux
target remote :1234
c
```

来连接到虚拟机上的 gdb 服务。

到这里，你就可以像调试普通程序一样调试 Linux 内核了。Linux 的内核入口函数是位于 init/main.c 中的 start_kernel ，在这里完成各种内核数据结构的初始化。

## 5. 虚拟机中配置网络

输入用户密码登录。

```bash
ip addr
```

假设网卡名为`enp0s3`。

执行如下命令：

```bash
sudo ip addr add 192.168.2.129/24 dev enp0s3
ip addr
sudo ip link set enp0s3 up
ping 192.168.2.128 # 192.168.2.128为宿主机虚拟网卡tap0地址，可以ping通
```

此刻在宿主机上`ping 192.168.2.129`发现也能ping通了，但是此时，虚拟机还不能上外网，因为虚拟机缺少网关。

现在把虚拟机的tap0的地址，192.168.2.128，设置为虚拟机的网关：

```bash
sudo route add default gw 192.168.2.128
```

这样，也可以ping通外网了，比如`ping 10.20.52.86`，`10.20.52.86`是宿主机真实网卡`ip`。

但是`ping www.baidu.com`却不行，因为缺少DNS服务器！

查看宿主机`/etc/resolv.conf`内容：

```bash
cat /etc/resolv.conf
```

输出如下：

```text
# Generated by NetworkManager
nameserver 10.20.0.10
```

现在就把`10.20.0.10`指定为虚拟机的DNS服务器：

```bash
sudo su
echo "nameserver 10.20.0.10" >> /etc/resolv.conf
```

写入文件之后，DNS立即生效了。现在，虚拟机既能上外网，又能与宿主机通信了。

```text
root@wujing-PC:# ping baidu.com
PING baidu.com (110.242.68.66) 56(84) bytes of data.
64 bytes from 110.242.68.66 (110.242.68.66): icmp_seq=1 ttl=52 time=27.2 ms
```

现在可以使用apt安装依赖：

```bash
apt update
```

## 6. 虚拟机挂载NFS网络文件系统

在宿主机上安装 NFS 服务：

```bash
sudo apt install nfs-kernel-server
id
sudo su
echo "/home/wujing/code 192.168.2.0/24(rw,sync,all_squash,anonuid=1000,anongid=1000,no_subtree_check)" >> /etc/exports # 创建共享目录
exportfs -arv # 更新exports配置
showmount -e # 查看NFS共享情况
```

在虚拟机上安装 NFS 客户端：

```bash
sudo apt install nfs-common
showmount -e 192.168.2.128 # 查看NFS服务器共享目录
sudo mount -t nfs 192.168.2.128:/home/wujing/code /home/wujing/code # 临时挂载 NFS 文件系统
```

临时挂载 NFS 文件系统输出如下：

```text
[ 7990.116497] NFS4: Couldn't follow remote path
[ 7990.118433] NFS4: Couldn't follow remote path
```

上述输出仅为warning，请忽略。

- [挂载NFS网络文件系统教程](https://www.cnblogs.com/lizhuming/p/13946107.html)
- [NFS原理详解](https://blog.51cto.com/atong/1343950)

## More

- [详解QEMU网络配置的方法](https://www.jb51.net/article/97216.htm)
- [QEMU的网络配置方法解析](https://js.aizhan.com/server/jishu/6594.html)
- [在qemu-kvm配置桥接网络](https://www.shuzhiduo.com/A/xl56Dmq0zr/)
- [QEMU--用户模式网络](https://blog.csdn.net/m0_43406494/article/details/124827927)
- [QEMU 网络配置](https://tomwei7.com/2021/10/09/qemu-network-config/)
- [Linux 内核调试 七：qemu网络配置](https://blog.csdn.net/OnlyLove_/article/details/124536607)
