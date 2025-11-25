# helloworld initramfs

## helloworld.c源码

做一个最简单的Hello World initramfs，来直观地理解initramfs。

Hello World的C程序如下，与普通的Hello World相比，加了一行while(1)。

```c
#include <stdio.h>

void main()
{
    printf("Hello World!\n");
    fflush(stdout);
    /* 让程序打印完后继续维持在用户态 */
    while(1);
}
```

## 编译helloworld.c程序

```bash
cd helloworld
gcc -static helloworld.c -o init
```

- -static: On systems that support dynamic linking, this prevents linking with the shared libraries. //不让gcc动态链接shared libraries

- -o init 当 GRUB 载入 kernel 和 initramfs 后，kernel 会把 initramfs 在内存中展开，然后执行其根目录下的 init ，也就是上面的脚本。以上的脚本会执行 mount 工作，准备好目录结构，然后执行 /sbin/init 转换入 ubuntu 的初始化过程（system-v init ，upstart ， systemd，用 udev 自动创建设备文件等）。

## 打包initramfs文件

```bash
mkdir tmp
cp init tmp
cd tmp
find . | cpio -o -H newc | gzip > ../helloworld-initramfs.cpio.gz
```

## 在qemu中启动编译好的内核，把helloworld-initramfs.cpio.gz指定为initrd

```bash
qemu-system-x86_64 -kernel ~/code/linux/arch/x86/boot/bzImage -initrd ~/code/linux-learning/debug/helloworld/helloworld-initramfs.cpio.gz -append "console=ttyS0" -nographic
```

系统能成功启动到输出"Hello World!"，并且在用户态停住。结合前文“在qemu环境中用gdb调试Linux内核”，可以看到qemu虚机中运行的Linux系统已经成功挂载了initramfs, 在console日志中也能看到“Unpacking initramfs...”。

<!-- ![helloworld](images/helloworld.png) -->
![helloworld](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/helloworld.png)

- [Initramfs 原理和实践](https://www.cnblogs.com/wipan/p/9269505.html)
- [initramfs的使用方法](https://blog.csdn.net/u010444107/article/details/79427542)
- [initramfs的制作和使用](https://blog.csdn.net/greatyoulv/article/details/117175103)
- [initramfs的使用方法](https://blog.csdn.net/u010444107/article/details/79427542)
- [基于 debootstrap 和 busybox 构建 mini ubuntu](https://www.cnblogs.com/fengyc/p/6114648.html)
