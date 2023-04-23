---
date: 2023/04/19 14:31:36
updated: 2023/04/19 14:31:36
---

# mac下基于qemu调试ubuntu-22.04.01

## 安装环境

```bash
brew install qemu
```

## 创建磁盘映像

```bash
qemu-img create -f qcow2 ubuntu-22.04.1-desktop-amd64.qcow2 20G
```

## 安装 Ubuntu

```bash
qemu-system-x86_64 \
    -machine type=q35,accel=hvf \
    -smp 2 \
    -hda /Users/wujing/code/qemu/ubuntu-22.04.1-desktop-amd64.qcow2 \
    -cdrom /Users/wujing/Downloads/ubuntu-22.04.1-desktop-amd64.iso \
    -m 4G \
    -vga virtio \
    -usb \
    -device usb-tablet \
    -display default,show-cursor=on
```

## 运行 Ubuntu

```bash
qemu-system-x86_64 \
    -machine type=q35,accel=hvf \
    -smp 2 \
    -hda /Users/wujing/code/qemu/ubuntu-22.04.1-desktop-amd64.qcow2 \
    -m 4G \
    -vga virtio \
    -usb \
    -device usb-tablet \
    -display default,show-cursor=on \
    -net user,hostfwd=tcp::2222-:22 -net nic
```

运行 VM 时，我们不需要挂载 Ubuntu ISO，可以通过省略以下选项将其删除：-cdrom。
增加-net选项把host的127.0.0.1:2222端口转发到guest的22端口，我们可以使用ssh来连接guest系统。

在宿主机上执行下方命令：

```bash
ssh wujing@127.0.0.1 -p 2222
```

## 调试 Ubuntu

添加 -dbgsym.ddeb 包 仓库源：

```bash
echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-proposed main restricted universe multiverse" | \
sudo tee -a /etc/apt/sources.list.d/ddebs.list
```

从 Ubuntu 服务器导入调试符号存档签名密钥。在 Ubuntu 18.04 LTS 及更高版本上：

```bash
sudo apt install ubuntu-dbgsym-keyring
```

在早期版本的 Ubuntu 使用：

```bash
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys F2EDC64DC5AEE1F6B9C621F0C8CAB6595FDFF622
```

在虚拟机中下载内核调试包，三个deb包版本号要保持一致：

```bash
sudo apt install linux-headers-$(uname -r)=5.19.0-38.39~22.04.1 linux-i
mage-$(uname -r)-dbgsym=5.19.0-38.39~22.04.1
```

在虚拟机中下载内核源码：

```bash
sudo apt install dpkg-dev
```

```bash
apt source linux-image-$(uname -r)
```

复制内核源码到宿主机：

```bash
scp -r linux-signed-hwe-5.19-5.19.0 wujing@192.168.31.9:~/code
```

复制带调试符号的内核二进制vmlinux到宿主机：

```bash
scp /lib/debug/boot/vmlinux-5.19.0-38-generic wujing@192.168.31.9:~/code/linux-signed-hwe-5.19-5.19.0
```

在宿主机上执行下方命令:

```bash
qemu-system-x86_64 \
    -machine type=q35,accel=hvf \
    -smp 2 \
    -hda /Users/wujing/code/qemu/ubuntu-22.04.1-desktop-amd64.qcow2 \
    -m 4G \
    -vga virtio \
    -usb \
    -device usb-tablet \
    -display default,show-cursor=on \
    -net user,hostfwd=tcp::2222-:22 -net nic \
    -S -s
```

报错如下：

```text
qemu-system-x86_64: -s: gdbstub: current accelerator doesn't support guest debugging
```

看来想在mac上使用hvf作为加速器调试目前还行不通。
