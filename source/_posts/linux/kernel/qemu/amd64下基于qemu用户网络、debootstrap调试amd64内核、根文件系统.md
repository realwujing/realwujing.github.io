---
date: 2023/10/12 21:45:58
updated: 2023/10/13 18:41:39
---

# amd64下基于qemu用户网络、debootstrap调试amd64内核、根文件系统

## 1. 编译调试版linux内核

### 下载源码

```bash
git clone https://github.com/torvalds/linux.git
```

### 安装编译依赖

```bash
sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev
```

### 内核编译选项配置

进入源码目录，配置编译选项：

```bash
cd linux
git checkout -b v5.10-rc7 v5.10-rc7
make menuconfig
```

比较重要的配置项有：

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
sudo debootstrap --arch=amd64 --include=ifupdown bionic linux-rootfs http://mirrors.aliyun.com/ubuntu/
```

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
./ch-mount.sh -m linux-rootfs/
# 再次进入时，执行如下命令即可
# sudo chroot linux-rootfs
```

### 定制文件系统

#### 配置网络

要确保进入文件系统后有网络，可以将 /etc/resolv.conf 文件拷贝到 linux-rootfs/etc/resolv.conf。

```bash
sudo cp /etc/resolv.conf linux-rootfs/etc/resolv.conf
```

#### 更换国内镜像源

```bash
# 若是遇到没法拉取 https 源的状况，请先使用 http 源并安装
apt install apt-transport-https
cp /etc/apt/sources.list /etc/apt/sources.list.bak
# 把文件内容所有替换为对应阿里源，参见：https://developer.aliyun.com/mirror/?spm=a2c6h.12873639.J_5404914170.29.2feb6235F6x30d
vim /etc/apt/source.list
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

#### 设置主机名和以太网

```bash
echo $HOST > /etc/hostname
echo "127.0.0.1 localhost.localdomain localhost" > /etc/hosts
echo "127.0.0.1 $HOST" >> /etc/hosts
echo "auto enp0s3" >> /etc/network/interfaces
echo "iface enp0s3 inet dhcp" >> /etc/network/interfaces
```

### 退出文件系统

```bash
exit
./ch-mount.sh -u linux-rootfs
```

### 制作文件系统镜像(initrd)

```bash
dd if=/dev/zero of=bionic.img bs=1M seek=2047 count=1
sudo mkfs.ext4 -F bionic.img
sudo mkdir -p /mnt/bionic
sudo mount -o loop bionic.img /mnt/bionic
sudo cp -a linux-rootfs/. /mnt/bionic/.
sudo umount /mnt/bionic
sudo chmod 666 bionic.img
```

- [内核调试环境：buildroot/debootstrap制作文件系统、编译内核、QEMU模拟](https://blog.csdn.net/weixin_49393427/article/details/126435589)
- [debootstrap 制作根文件系统](https://www.cnblogs.com/huaibovip/p/debootstrap-fs.html)
- [使用 debootstrap 制作 ARM64 rootfs.cpio](https://blog.51cto.com/u_13731941/5399257)

## 3. 启动内核并调试

### 依赖安装

```bash
sudo apt install qemu qemu-system qemu-kvm
```

### qemu启动内核并挂载文件系统调试

下方两个命令都行，第一个报warning，第二个file=后面要使用绝对路径。

```bash
qemu-system-x86_64 -s -S -m 2048 -kernel ~/code/linux/arch/x86/boot/bzImage -hda ~/code/tmp/bionic.img -append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" -nographic
```

```bash
qemu-system-x86_64 -s -S -m 2048 -kernel ~/code/linux/arch/x86/boot/bzImage -drive format=raw,file=/home/wujing/code/tmp/bionic.img -append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" -nographic
```

- [Linux aarch64 编译 &amp; qemu 搭建实验平台 initrd initramfs](https://blog.csdn.net/FJDJFKDJFKDJFKD/article/details/100021609)

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

## 4. 网络修复

输入root密码登录后，查看IP地址：

```bash
ip addr
```

假设网卡名为enp0s3。

假设上方设置主机名和以太网中网卡名为eth0。

执行如下命令：

```bash
sed -i "s/eth0/enp0s3/g" /etc/network/interfaces
ifup enp0s3
```

现在可以使用apt安装依赖：

```bash
apt update
```

qemu默认使用的用户模式网络非常适合允许访问网络资源，包括 Internet。 特别是，它允许从guest到主机的 ssh流量。 但是，默认情况下，它充当防火墙，不允许任何传入流量。 它也不支持 TCP 和 UDP 以外的协议 - 例如，ping 和其他 ICMP 程序将不起作用。

- [QEMU--用户模式网络](https://blog.csdn.net/m0_43406494/article/details/124827927)
- [QEMU 网络配置](https://tomwei7.com/2021/10/09/qemu-network-config/)
- [Linux 内核调试 七：qemu网络配置](https://blog.csdn.net/OnlyLove_/article/details/124536607)
