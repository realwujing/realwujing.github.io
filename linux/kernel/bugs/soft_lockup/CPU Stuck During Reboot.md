# CPU Stuck During Reboot

机型：Atlas 800I弹性裸金属

## bug 现场

![CPU Stuck During Reboot](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_17212862205127.png)

在grub中增加如下参数后：

```bash
initcall_debug no_console_suspend ignore_loglevel
```

从下图看到reboot流程被软中断打断。

![CPU Stuck During Reboot](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_17207537656184.png)

```bash
[  452.388493] CPU: 0 PID: 0 Comm: swapper/0 Tainted: G           OE  ------------   4.14.0-115.el7a.0.1.aarch64 #1
[  452.407685] Hardware name: HUAKUN HuaKun AT3500 G3/IT21SK4E, BIOS 12.01.08 03/22/2024
[  452.424436] task: ffff000008dc5600 task.stack: ffff000008d80000
[  452.434835] PC is at __do_softirq+0xc0/0x2f0
[  452.443498] LR is at irq_exit+0x120/0x150
[  452.451806] pc : [<ffff000008081a40>] lr : [<ffff0000080db6ac>] pstate: 20400009
[  452.467753] sp : ffff00000800fec0
[  452.475238] x29: ffff00000800fec0 x28: ffff000008dc5600
[  452.484666] x27: ffff000008d52120 x26: ffff000008db0000
[  452.493973] x25: ffff000008f804f0 x24: ffff000008f216d0
[  452.503166] x23: ffff000008d8fd80 x22: 0000000000000202
[  452.512263] x21: 0000000000000001 x20: 0000000000000000
[  452.521245] x19: ffff000008d50000 x18: ffffe057cd4ca720
[  452.530141] x17: ffff000008954b30 x16: 000000000000000e
[  452.538939] x15: 0000000000000007 x14: 0000000000000000
[  452.547623] x13: 0000000000000001 x12: 0000000000000000
[  452.556164] x11: ffff0000088d2590 x10: ffff0000088d2564
[  452.564609] x9 : 0000000000000078 x8 : 0000000000000020
[  452.572969] x7 : ffff000008d50018 x6 : ffff000009601d00
[  452.581226] x5 : 0000000000000000 x4 : 0000000100003171
[  452.589394] x3 : 0000000000000100 x2 : ffff000009601000
[  452.597445] x1 : ffff000009601d00 x0 : 0000000000000000
[  452.605395] Call trace:
[  452.610362] Exception stack(0xffff00000800fd80 to 0xffff00000800fec0)
[  452.619381] fd80: 0000000000000000 ffff000009601d00 ffff000009601000 0000000000000100
[  452.632208] fda0: 0000000100003171 0000000000000000 ffff000009601d00 ffff000008d50018
[  452.645118] fdc0: 0000000000000020 0000000000000078 ffff0000088d2564 ffff0000088d2590
[  452.658079] fde0: 0000000000000000 0000000000000001 0000000000000000 0000000000000007
[  452.671138] fe00: 000000000000000e ffff000008954b30 ffffe057cd4ca720 ffff000008d50000
[  452.684443] fe20: 0000000000000000 0000000000000001 0000000000000202 ffff000008d8fd80
[  452.697864] fe40: ffff000008f216d0 ffff000008f804f0 ffff000008db0000 ffff000008d52120
[  452.711667] fe60: ffff000008dc5600 ffff00000800fec0 ffff0000080db6ac ffff00000800fec0
[  452.725926] fe80: ffff000008081a40 0000000020400009 ffff00000800fee0 ffff0000086c7a50
[  452.740590] fea0: 0000ffffffffffff 0000000000000000 ffff00000800fec0 ffff000008081a40
[  452.755647] [<ffff000008081a40>] __do_softirq+0xc0/0x2f0
[  452.764739] [<ffff0000080db6ac>] irq_exit+0x120/0x150
[  452.773572] [<ffff000008140b08>] __handle_domain_irq+0x78/0xc4
[  452.783223] [<ffff000008081868>] gic_handle_irq+0xa0/0x1b8
[  452.792536] Exception stack(0xffff000008d8fd80 to 0xffff000008d8fec0)
[  452.802899] fd80: 0000e057c4760000 ffff000008d50018 ffff000008dc0c44 0000000000000001
[  452.818739] fda0: 0000000000000000 000000105a9b5d23 0000000000014ca6 0000000100003173
[  452.834839] fdc0: ffff000008dc6360 ffff000008d8fe30 0000000000000d00 ffff0000088d2590
[  452.851048] fde0: 0000000000000000 0000000000000000 0000000000000000 0000000000000000
[  452.867264] fe00: 000000000000014f ffffffffffffffff 0000000000000008 ffff000008f224b8
[  452.883486] fe20: ffff000008d50000 ffff000008f22000 0000000000000000 ffff000008dbc50c
[  452.899701] fe40: 0000000000000000 0000000000000000 00000057fff8ab20 00000057fff63c8d
[  452.915919] fe60: 0000000000c00018 ffff000008d8fec0 ffff0000080858c8 ffff000008d8fec0
[  452.932125] fe80: ffff0000080858cc 0000000060c00009 ffff000008d8fea0 ffff000008152050
[  452.948335] fea0: ffffffffffffffff ffff0000081520b8 ffff000008d8fec0 ffff0000080858cc
[  452.964549] [<ffff0000080830b0>] el1_irq+0xb0/0x140
[  452.973580] [<ffff0000080858cc>] arch_cpu_idle+0x44/0x144
[  452.983051] [<ffff000008871b68>] default_idle_call+0x20/0x30
[  452.992715] [<ffff00000812482c>] do_idle+0x158/0x1cc
[  453.001590] [<ffff000008124a3c>] cpu_startup_entry+0x28/0x30
[  453.011152] [<ffff00000886af54>] rest_init+0xbc/0xc8
[  453.019999] [<ffff000008c00dac>] start_kernel+0x410/0x43c
[  456.343559] [ascend] [drv_pcie] [devdrv_sync_non_trans_msg_send 1239] <kworker/u385:0:15232:15232> Device is abnormal. (dev_id=5; msg_type=3; common_type=5)
[  456.365571] [ascend] [devdrv] [devdrv_refresh_aicore_info_work 322] <kworker/u385:0:15232> devdrv_manager_send_msg unsuccessful. (ret=-19; dev_id=5)
```

## panic_on_soft_lockup

- kernel.softlockup_panic

    作用： 软死锁是指内核中的一个任务（线程）由于长时间未能释放CPU而导致系统失去响应时是否触发 panic。

    潜在风险： 如果启用，系统在检测到软锁定时会触发 panic。

    ```bash
    sysctl -w kernel.softlockup_panic=1
    ```

    内核编译选项：CONFIG_SOFTLOCKUP_DETECTOR=y BOOTPARAM_SOFTLOCKUP_PANIC=y

从下图可以看到当前内核软锁的时候不会触发panic。

![panic_on_soft_lockup](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_17206693863448.png)

## 搭建环境

### iso下载

- [centos aarch64（arm64） iso 下载网址](https://blog.csdn.net/vah101/article/details/116691272)
- [https://vault.centos.org/altarch/7.6.1810/isos/aarch64/](https://vault.centos.org/altarch/7.6.1810/isos/aarch64/)
- [https://mirrors.aliyun.com/centos-vault/centos/7.6.1810/isos/x86_64/CentOS-7-x86_64-Everything-1810.iso](https://vault.centos.org/altarch/7.6.1810/isos/aarch64/)
- [安装CentOS Vault系统踩过的坑](https://www.cnblogs.com/yeziwinone/p/17916626.html)

```bash
wget https://vault.centos.org/altarch/7.6.1810/isos/aarch64/CentOS-7-aarch64-Everything-1810.iso
```

### 创建安装磁盘

```bash
qemu-img create -f qcow2 centos.qcow2 128G
```

### 安装镜像

```bash
virt-install --virt-type kvm \
--name centos \
--memory 32768 \
--vcpus=64 \
--location /root/inf-test/CentOS-7-aarch64-Everything-1810.iso \
--disk path=/root/inf-test/centos.qcow2,size=128 \
--network network=default \
--os-type=linux \
--os-variant=rhel7 \
--graphics none \
--extra-args="console=ttyS0"
```

### 内核源码src.rpm下载

- [https://vault.centos.org/7.6.1810/os/Source/SPackages/](https://vault.centos.org/7.6.1810/os/Source/SPackages/)
- [https://vault.centos.org/7.6.1810/os/Source/SPackages/kernel-alt-4.14.0-115.el7a.0.1.src.rpm](https://vault.centos.org/7.6.1810/os/Source/SPackages/kernel-alt-4.14.0-115.el7a.0.1.src.rpm)
![https://vault.centos.org/7.6.1810/os/Source/SPackages/kernel-alt-4.14.0-115.el7a.0.1.src.rpm](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724145414.png)

- [https://vault.centos.org/altarch/7.6.1810/kernel/Source/SPackages/](https://vault.centos.org/altarch/7.6.1810/kernel/Source/SPackages/)

![https://vault.centos.org/altarch/7.6.1810/kernel/Source/SPackages/](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724145716.png)

从上面的链接可以看到，`kernel-alt-4.14.0-115.el7a.0.1.src.rpm`没有归档到内核源码src.rpm专用链接<https://vault.centos.org/altarch/7.6.1810/kernel/Source/SPackages/>里面，而是归档到了公共src.rpm链接<https://vault.centos.org/7.6.1810/os/Source/SPackages/](https://vault.centos.org/7.6.1810/os/Source/SPackages/>里面，故`kernel-alt-4.14.0-115.el7a.0.1.src.rpm`到底是不是`https://vault.centos.org/altarch/7.6.1810/isos/aarch64/CentOS-7-aarch64-Everything-1810.iso`对应的内核源码？

#### 下载`kernel-alt-4.14.0-115.el7a.0.1.src.rpm`

- [CentOS上编译安装内核](https://goodcommand.readthedocs.io/zh-cn/latest/os/CentOS_build_kernel.html)

```bash
wget https://vault.centos.org/7.6.1810/os/Source/SPackages/kernel-alt-4.14.0-115.el7a.0.1.src.rpm
```

#### 源码包安装解压

```bash
rpm -ivh kernel-alt-4.14.0-115.el7a.0.1.src.rpm
```

#### 源码打patch

打patch主要是因为CentOS的源码其实和Redhat一致，Redhat的源码应用patch后就变成CentOS。

打patch之前需要安装一些工具,主要是rpmbuild和git：

```bash
yum install -y rpm-build git
```

首先打源码包里面的patch,可以在当前用户的任意路径执行:

```bash
rpmbuild -bp --nodeps ~/rpmbuild/SPECS/kernel-alt.spec
```

执行上述命令后，进入内核源码路径：

```bash
cd ~/rpmbuild/BUILD/kernel-alt-4.14.0-115.el7a/linux-4.14.0-115.el7.0.1.aarch64
```

#### 编译内核rpm包

这一步，编译内核，并且打包成rpm安装包，这样就可以把安装包拷贝到目标机器上执行安装了。

安装编译工具:

```bash
yum groupinstall –y “Development Tools”
yum install -y ncurses-devel make gcc bc bison flex elfutils-libelf-devel openssl-devel rpm-build redhat-rpm-config -y
```

配合config. 主要是对内核编译选项进行设置。先把当前用来启动系统的config拷贝过来是最保险的。注意是.config

```bash
cp /boot/config-4.14.0-115.el7a.0.1.aarch64 ./.config
```

修改.config中的CONFIG_SYSTEM_TRUSTED_KEYS=“certs/centos.pem”为空。

```bash
CONFIG_SYSTEM_TRUSTED_KEYS=""
```

##### 使用build-kernel-natively.sh脚本编译

执行编译, 下载编译脚本：

```bash
wget https://raw.githubusercontent.com/xin3liang/home-bin/master/build-kernel-natively.sh
```

`build-kernel-natively.sh`内容如下：

```bash
#!/bin/bash
set -x

## params
version_prefix="rhel8.1-sas-5.1-update"

if [ $# -ge 1 ]; then
	version_prefix=$1
fi

COFNIG_FILE="~/config-`uname -r`"
export LOCALVERSION="-${version_prefix}-`date +%F`"
cpunum=$(getconf _NPROCESSORS_ONLN)
#cpunum=1

# targets: bindeb-pkg binrpm-pkg INSTALL_MOD_STRIP=1 Image modules INSTALL_MOD_PATH=${OUT} modules_install"
BUILD_TARGETS="rpm-pkg INSTALL_MOD_STRIP=1"

## kernel .config compile
# cp /boot/conf-xx kernel_source_dir/.config
# CONFIG: oldconfig defconfig estuary_defconfig
CONFIG="oldconfig"

make ${CONFIG}
make ${BUILD_TARGETS} -j$cpunum

set +x
```

在源码目录执行:

```bash
./build-kernel-natively.sh
```

##### 直接make binrpm-pkg编译

```bash
time make binrpm-pkg -j64 2> make_error.log
```

## initramfs无法加载根文件系统

在物理机上安装新内核后出现了新的故障，在initramfs无法加载根文件系统。

报错信息如下：

```bash
[  238.741337] dracut-initqueue[2520]: Warning: dracut-initqueue timeout - starting timeout scripts
[  239.311370] dracut-initqueue[2520]: Warning: dracut-initqueue timeout - starting timeout scripts
[  239.311673] dracut-initqueue[2520]: Warning: Could not boot.
```

![Warning:/devdisk/by-uuid/c38d268?-f12-469a-913f-549d23c6ba12 does not exist](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/915735adf44b509b57fb8c1d3ed60d3.png)

### 使用 dracut 重新生成initramfs

- [dracut 使用笔记](https://blog.csdn.net/superbfly/article/details/122114397)
- [How to build an initramfs using Dracut on Linux](https://linuxconfig.org/how-to-build-an-initramfs-using-dracut-on-linux)
- [dracut(8) — Linux manual page](https://www.man7.org/linux/man-pages/man8/dracut.8.html)

根据下图可以看到，新编内核生成的initramfs与镜像中自带的initramfs差异点主要在于：

```bash
usr/lib/modules/4.14.0/kernel/crypto
usr/lib/modules/4.14.0/kernel/drivers/md
usr/lib/modules/4.14.0/kernel/drivers/net
usr/lib/modules/4.14.0/kernel/drivers/scsi
usr/lib/modules/4.14.0/kernel/lib/raid6
```

![initramfs.log <-> initramfs1.log](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724155620.png)

使用 dracut 根据当前内核生成 initramfs：

```bash
dracut -f
```

使用 dracut 生成指定版本内核对应的 initramfs：

```bash
dracut -f initramfs-`uname -r`.img `uname -r`
dracut -f initramfs-4.14.0.img 4.14.0
```

将某个驱动模块添加到initramfs中(可以使用绝对路径，但是要去掉结尾.ko)：

```bash
dracut -f --add-drivers crypto initramfs-`uname -r`.img `uname -r`
dracut -f --add-drivers /usr/lib/modules/4.14.0/kernel/drivers/scsi/scsi_transport_sas initramfs-`uname -r`.img `uname -r`
```

将`/usr/lib/modules/4.14.0/kernel/drivers/scsi`路径下的所有内容打包到initramfs下的`/usr/lib/modules/4.14.0/kernel/drivers/scsi`:

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/scsi /usr/lib/modules/4.14.0/kernel/drivers/scsi initramfs-`uname -r`.img `uname -r`
```

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/md /usr/lib/modules/4.14.0/kernel/drivers/md initramfs-`uname -r`.img `uname -r`
```

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/lib/raid6 /usr/lib/modules/4.14.0/kernel/lib/raid6 initramfs-`uname -r`.img `uname -r`
```

后续排查故障过程更新了mlx网卡驱动`MLNX_OFED_LINUX-5.8-4.1.5.0-rhel7.6alternate-aarch64.tgz`，安装时报错如下：

![Failed to query 45:00.0 device, error: No such file or directory. MFE NO FLASH DETECTED](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724153818.png)

由于不知道具体的网络驱动是啥，将net目录整体加入到initramfs中：

```bash
dracut -f -v -i /lib/modules/4.14.0-115.el7a.0.1.aarch64/extra/mlnx-ofa_kernel/drivers/net initramfs-`uname -r`.img `uname -r`
```

reboot后又卡cpu stuck，报错截图如下：

![a2219e1405d94ad7ff8a8541b128db7](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/a2219e1405d94ad7ff8a8541b128db7.png)

reboot是热重启，将机器掉电后热重启，能进入系统：

```bash
![lsmod | grep mlx5 core](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/06c9a78d1a02d386ddf4f1552a09006.png)
```

目前基本确定是mlx网卡驱动导致的reboot流程被打断。
