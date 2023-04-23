---
date: 2023/04/19 14:31:36
updated: 2023/04/19 14:31:36
---

# linux 内核调试

```bash
qemu-system-x86_64 -s -S -kernel ~/code/linux/arch/x86/boot/bzImage -initrd ~/code/busybox-1.35.0/initramfs.cpio.gz -append "nokaslr console=ttyS0" -nographic
```

```bash
qemu-system-x86_64 -s -S -m 2048 -kernel ~/code/linux/arch/x86/boot/bzImage -hda ~/code/tmp/bionic.img -append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" -nographic
```

```bash
qemu-system-x86_64 -s -S -m 2048 -kernel ~/code/linux/arch/x86/boot/bzImage -drive format=raw,file=/home/wujing/code/tmp/trusty.img -append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" -nographic
```

- [利用QEMU+GDB搭建Linux内核调试环境](https://bbs.huaweicloud.com/blogs/348654)
- [使用 VSCode + qemu 搭建 Linux 内核调试环境](https://blog.csdn.net/eydwyz/article/details/114019532)
- [内核调试环境：buildroot/debootstrap制作文件系统、编译内核、QEMU模拟](https://blog.csdn.net/weixin_49393427/article/details/126435589)
- [使用 debootstrap 制作 ARM64 rootfs.cpio](https://blog.51cto.com/u_13731941/5399257)
- [debootstrap 制作根文件系统 ](https://www.cnblogs.com/huaibovip/p/debootstrap-fs.html)
