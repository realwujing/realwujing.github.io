# linux 内核调试

qemu-system-x86_64 -s -S -kernel ~/code/linux/arch/x86/boot/bzImage -initrd ~/code/busybox-1.35.0/initramfs.cpio.gz -append "nokaslr console=ttyS0" -nographic

- [利用QEMU+GDB搭建Linux内核调试环境](https://bbs.huaweicloud.com/blogs/348654)
- [使用 VSCode + qemu 搭建 Linux 内核调试环境](https://blog.csdn.net/eydwyz/article/details/114019532)
