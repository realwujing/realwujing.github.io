# qemu编译调试-v2.8.1

## 内核环境

镜像基于Ubuntu 16.04.7 LTS:

```bash
cat /etc/os-release

NAME="Ubuntu"
VERSION="16.04.7 LTS (Xenial Xerus)"
ID=ubuntu
ID_LIKE=debian
PRETTY_NAME="Ubuntu 16.04.7 LTS"
VERSION_ID="16.04"
HOME_URL="http://www.ubuntu.com/"
SUPPORT_URL="http://help.ubuntu.com/"
BUG_REPORT_URL="http://bugs.launchpad.net/ubuntu/"
VERSION_CODENAME=xenial
UBUNTU_CODENAME=xenial
```

将内核版本替换成linux主线v4.4:

```bash
git clone git@github.com:realwujing/linux.git
git checkout -b v4.4 v4.4
sudo apt build-dep linux
cp /boot/config-4.4.0-161-generic .config
make menuconfig
# load .config
make bindeb-pkg -j10 2> make_error.log
sudo apt install ../*.deb
```

## 源码下载

本次源码解读基于git标签v2.8.1。

方式一

qemu源码位于github上，但是v2.8.1对应的git submodules仓库位于gitlab上，无法下载对应gitmodules源码，故手动替换git submodules仓库为github镜像仓库源，详情见提交：[gitmodules: update git submodule url to github](https://github.com/realwujing/qemu/commit/56fe7ca29adddd876b590e301e62d5b6e4b3a33e)

```bash
git clone git@github.com:realwujing/qemu.git
git checkout -b v2.8.1-comment v2.8.1-comment
git submodule init
git submodule update --init --recursive
```

方式二

```bash
wget https://download.qemu.org/qemu-2.8.1.tar.xz
tar -xvJf qemu-2.8.1.tar.xz
```

## 编译调试

```bash
mkdir build
cd build
sudo apt install libglib2.0-dev libpixman-1-dev libfdt-dev
export CFLAGS="-g -O0"
export LDFLAGS="-g -O0"
../configure
make
```

## 开始调试

```bash
find . -name qemu-system-x86_64
```

```bash
gdb --args ./x86_64-softmmu/qemu-system-x86_64 \
-m 2048 \
-kernel ~/code/linux/arch/x86/boot/bzImage \
-hda ~/code/qemu-img/bionic.img \
-append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" \
-nographic
```

```c
(gdb) b main
Breakpoint 1 at 0x1fee80: file /home/wujing/code/qemu/vl.c, line 2998.
(gdb) r
Starting program: /home/wujing/code/qemu/build/x86_64-softmmu/qemu-system-x86_64 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New Thread 0x7ffff59fa700 (LWP 1509)]

Thread 1 "qemu-system-x86" hit Breakpoint 1, main (argc=1, argv=0x7fffffffdfe8, envp=0x7fffffffdff8) at /home/wujing/code/qemu/vl.c:2998
2998    {
```
