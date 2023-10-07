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
../configure
make
```

## 开始调试

```bash
find . -name qemu-system-x86_64
gdb ./x86_64-softmmu/qemu-system-x86_64
```

```c
(gdb) b main
Breakpoint 1 at 0x2d9700: file ../softmmu/main.c, line 47.
(gdb) b qemu_main_loop
Breakpoint 2 at 0x4a38f0: file ../softmmu/runstate.c, line 729.
(gdb) i b
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x00000000002d9700 in main at ../softmmu/main.c:47
2       breakpoint     keep y   0x00000000004a38f0 in qemu_main_loop at ../softmmu/runstate.c:729
(gdb) r
Starting program: /home/wujing/code/qemu/build/qemu-system-x86_64 
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New Thread 0x7ffff753c6c0 (LWP 1027371)]

Thread 1 "qemu-system-x86" hit Breakpoint 1, main (argc=1, argv=0x7fffffffdc18) at ../softmmu/main.c:47
47          qemu_init(argc, argv);
(gdb) c
Continuing.
[New Thread 0x7ffff68236c0 (LWP 1027464)]
[New Thread 0x7fffa79ff6c0 (LWP 1027465)]
VNC server running on ::1:5900

Thread 1 "qemu-system-x86" hit Breakpoint 2, qemu_main_loop () at ../softmmu/runstate.c:729
729     {
```

```c    // softmmu/main.c
#include "qemu/osdep.h"                                      // QEMU 系统仿真器
#include "qemu-main.h"                                      // QEMU 主程序
#include "sysemu/sysemu.h"                                   // QEMU 系统仿真

#ifdef CONFIG_SDL
#include <SDL.h>                                             // 简单直接媒体层（Simple DirectMedia Layer，SDL）库
#endif

int qemu_default_main(void)
{
    int status;

    status = qemu_main_loop();                                // 执行 QEMU 主循环
    qemu_cleanup();                                            // 清理 QEMU 资源

    return status;
}

int (*qemu_main)(void) = qemu_default_main;                   // 指向 QEMU 主函数的指针

int main(int argc, char **argv)
{
    qemu_init(argc, argv);                                     // 初始化 QEMU
    return qemu_main();                                       // 执行 QEMU 主函数
}
```
