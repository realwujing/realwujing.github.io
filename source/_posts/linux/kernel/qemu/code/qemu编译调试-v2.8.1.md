---
date: 2023/09/22 00:37:45
updated: 2023/10/08 17:24:31
---

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
--enable-kvm \
-m 2048 \
-kernel ~/code/linux/arch/x86/boot/bzImage \
-hda ~/code/qemu-img/bionic.img \
-append "root=/dev/sda rootfstype=ext4 rw console=ttyS0 nokaslr" \
-nographic
```

```c
(gdb) b cpus.c:1421
Breakpoint 1 at 0x237720: file /home/wujing/code/qemu/cpus.c, line 1425.
(gdb) c
The program is not being run.
(gdb) r
Starting program: /home/wujing/code/qemu/build/x86_64-softmmu/qemu-system-x86_64 --enable-kvm -m 2048 -kernel /home/wujing/code/linux/arch/x86/boot/bzImage -hda /home/wujing/code/qemu-img/bionic.img -append root=/dev/sda\ rootfstype=ext4\ rw\ console=ttyS0\ nokaslr -nographic
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
[New Thread 0x7ffff59fa700 (LWP 19433)]
[New Thread 0x7ffff51f9700 (LWP 19434)]
WARNING: Image format was not specified for '/home/wujing/code/qemu-img/bionic.img' and probing guessed raw.
         Automatically detecting the format is dangerous for raw images, write operations on block 0 will be restricted.
         Specify the 'raw' format explicitly to remove the restrictions.

Thread 1 "qemu-system-x86" hit Breakpoint 1, qemu_kvm_start_vcpu (cpu=0x5555565ab4b0) at /home/wujing/code/qemu/cpus.c:1425
1425        cpu->thread = g_malloc0(sizeof(QemuThread));
(gdb) bt
#0  qemu_kvm_start_vcpu (cpu=0x5555565ab4b0) at /home/wujing/code/qemu/cpus.c:1425
#1  qemu_init_vcpu (cpu=cpu@entry=0x5555565ab4b0) at /home/wujing/code/qemu/cpus.c:1470
#2  0x0000555555834a4a in x86_cpu_realizefn (dev=0x5555565ab4b0, errp=0x7fffffffdcf0) at /home/wujing/code/qemu/target-i386/cpu.c:3361
#3  0x00005555558dbb65 in device_set_realized (obj=<optimized out>, value=<optimized out>, errp=0x7fffffffdde0) at /home/wujing/code/qemu/hw/core/qdev.c:918
#4  0x0000555555a01f2e in property_set_bool (obj=0x5555565ab4b0, v=<optimized out>, name=<optimized out>, opaque=0x55555659e2a0, errp=0x7fffffffdde0)
    at /home/wujing/code/qemu/qom/object.c:1854
#5  0x0000555555a05da1 in object_property_set_qobject (obj=obj@entry=0x5555565ab4b0, value=value@entry=0x5555565c68b0, name=name@entry=0x555555b187ab "realized", 
    errp=errp@entry=0x7fffffffdde0) at /home/wujing/code/qemu/qom/qom-qobject.c:27
#6  0x0000555555a03c40 in object_property_set_bool (obj=0x5555565ab4b0, value=<optimized out>, name=0x555555b187ab "realized", errp=0x7fffffffdde0)
    at /home/wujing/code/qemu/qom/object.c:1157
#7  0x00005555557f0d6e in pc_new_cpu (typename=typename@entry=0x555556506200 "qemu64-x86_64-cpu", apic_id=0, errp=0x5555564ee820 <error_fatal>) at /home/wujing/code/qemu/hw/i386/pc.c:1099
#8  0x00005555557f407c in pc_cpus_init (pcms=pcms@entry=0x555556581970) at /home/wujing/code/qemu/hw/i386/pc.c:1188
#9  0x00005555557f6de3 in pc_init1 (machine=0x555556581970, pci_type=0x555555b54084 "i440FX", host_type=0x555555adcc01 "i440FX-pcihost") at /home/wujing/code/qemu/hw/i386/pc_piix.c:149
#10 0x000055555588358d in main (argc=11, argv=0x7fffffffe2b8, envp=0x7fffffffe318) at /home/wujing/code/qemu/vl.c:4548
(gdb) i b
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x000055555578b720 in qemu_kvm_start_vcpu at /home/wujing/code/qemu/cpus.c:1425
        breakpoint already hit 1 time
```
