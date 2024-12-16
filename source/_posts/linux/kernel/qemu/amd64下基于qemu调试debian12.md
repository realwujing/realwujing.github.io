---
date: 2023/06/28 10:36:12
updated: 2023/06/28 10:36:12
---

# amd64下基于qemu调试debian12

## 安装环境

```bash
sudo apt install virt-manager
```

## 制作启动盘

### 下载镜像

```bash
wget https://cdimage.debian.org/debian-cd/current/amd64/iso-dvd/debian-12.0.0-amd64-DVD-1.iso
```

### 安装镜像到虚拟盘

直接利用virt-manager图形界面安装镜像更快。

添加仓库源，仓库源位于:[https://mirrors.tuna.tsinghua.edu.cn/help/debian/](https://mirrors.tuna.tsinghua.edu.cn/help/debian/)

```text
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm main contrib non-free non-free-firmware
# deb-src https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm main contrib non-free non-free-firmware

deb https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm-updates main contrib non-free non-free-firmware
# deb-src https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm-updates main contrib non-free non-free-firmware

deb https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm-backports main contrib non-free non-free-firmware
# deb-src https://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm-backports main contrib non-free non-free-firmware

# deb https://mirrors.tuna.tsinghua.edu.cn/debian-security bookworm-security main contrib non-free non-free-firmware
# # deb-src https://mirrors.tuna.tsinghua.edu.cn/debian-security bookworm-security main contrib non-free non-free-firmware

deb https://security.debian.org/debian-security bookworm-security main contrib non-free non-free-firmware
# deb-src https://security.debian.org/debian-security bookworm-security main contrib non-free non-free-firmware
```

在虚拟机中下载内核调试包，三个deb包版本号要保持一致：

```bash
apt policy linux-image-`uname -r`
```

```text
linux-image-6.1.0-9-amd64:
  已安装：6.1.27-1
  候选： 6.1.27-1
  版本列表：
 *** 6.1.27-1 500
        500 https://mirrors.tuna.tsinghua.edu.cn/debian bookworm/main amd64 Packages
        100 /var/lib/dpkg/status
```

```bash
sudo apt install linux-headers-$(uname -r)=6.1.27-1 linux-image-$(uname -r)-dbg=6.1.27-1
```

在虚拟机中执行下方命令：

```bash
sudo su
scp /boot/initrd.img-6.1.0-9-amd64 wujing@10.20.42.43:~/code/qemu/amd64/debian/12
scp /boot/vmlinuz-6.1.0-9-amd64 wujing@10.20.42.43:~/code/qemu/amd64/debian/12
scp /usr/lib/debug/boot/vmlinux-6.1.0-9-amd64 wujing@10.20.42.43:~/code/qemu/amd64/debian/12
scp /boot/config-6.1.0-9-amd64 wujing@10.20.42.43:~/code/qemu/amd64/debian/12
```

上述操作执行完后关闭此虚拟机。

## 调试虚拟机

在宿主机上执行下方命令:

```bash
sudo qemu-system-x86_64 \
    -accel kvm \
    -cpu host \
    -m 4G \
    -smp 8 \
    -kernel /home/wujing/code/qemu/amd64/debian/12/vmlinuz-6.1.0-9-amd64 \
    -initrd /home/wujing/code/qemu/amd64/debian/12/initrd.img-6.1.0-9-amd64 \
    -drive file=/media/wujing/data/Downloads/kvm/debian12.qcow2,if=virtio,format=qcow2 \
    -append "root=/dev/vda1 ro video=efifb:nobgrt quiet nokaslr" \
    -device virtio-scsi-pci,id=scsi0 \
    -device virtio-net-pci,netdev=net0 -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-gpu-pci,id=video0 \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -S -s
```

`-drive file=/media/wujing/data/Downloads/kvm/debian12.qcow2,if=virtio,format=qcow2`中的`if=virtio`很重要，不然内核启动报错：

```text
Gave up waiting for root device. Common problems:
-Boot args (cat /proc/cmdline)
  -Check rootdelay= (did the system wait long enough?)
-Missing modules (cat /proc/modules; ls /dev)
ALERT! /dev/vda1 does not exist. Dropping to a shell!

BusyBox v1.27.2 (debian 1:1.27.2-2debian3.2) built-in shell (ash)
Enter 'help' for a list of built-in commands.

(initramfs)_
```

`if=virtio`参数必须附带的原因是本次调试宿主机也为debian12，`qemu-system-x86`版本为`1:7.2+dfsg-7`，默认采用virtio块设备，virtio块设备是 kvm 来宾的半虚拟化设备，与普通的模拟硬盘驱动器不同，传输速度更快。

`-append`传递给内核的参数可以通过在启动时，访问 GRUB 启动菜单，按 `e`键编辑引导参数可以看到参数，也可虚拟机启动后通过 `cat /proc/cmdline`查看。

启动时，我们做一下端口转发，我们这里把host的127.0.0.1:2222端口转发到guest的22端口，我们可以使用ssh来连接guest系统。

在宿主机上执行下方命令：

```bash
ssh wujing@127.0.0.1 -p 2222
```

在宿主机上下载debian 12 linux-image-6.1.0-9-amd64对应的内核源码：

```bash
apt source linux-source=6.1.27-1
```

创建软链接vmlinux到源码目录：

```bash
ln -s /home/wujing/code/qemu/amd64/debian/12/vmlinux-6.1.0-9-amd64 /home/wujing/code/qemu/amd64/debian/12/linux-6.1.27/vmlinux
```

在 `/home/wujing/code/qemu/amd64/debian/12/linux-6.1.27`目录下启动gdb：

```bash
gdb vmlinux
target remote :1234
hb start_kernel
c
```

debian12内核第一个断点必须设置为硬件断点，否则gdb输出如下Warning且无法捕捉断点：

```text
Warning:
Cannot insert breakpoint 1.
Cannot access memory at address 0xffffffff824bbb8d

Command aborted.
```

敲 `c`命令输出如下：

```text
Thread 1 hit Breakpoint 1, start_kernel () at init/main.c:531
531     {  
```

可以看到断点正常命中，继续敲 `c`命令可以看到虚拟机正常启动到登录界面。
