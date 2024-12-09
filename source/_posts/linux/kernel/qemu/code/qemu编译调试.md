---
date: 2023/09/19 15:07:51
updated: 2023/09/19 15:07:51
---

# qemu编译调试

## 源码下载

本次源码解读基于git标签v7.2.4:

```bash
git clone git@github.com:realwujing/qemu.git
git checkout -b v7.2.4-comment v7.2.4
```

## 编译调试

```bash
mkdir build
cd build
../configure
make
```

## 开始调试

```bash
gdb ./qemu-system-x86_64
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
