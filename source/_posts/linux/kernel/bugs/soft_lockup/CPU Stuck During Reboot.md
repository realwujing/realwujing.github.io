---
date: 2024/07/24 16:07:32
updated: 2024/07/29 21:04:33
---

# CPU Stuck During Reboot

- 机型：Atlas 800I弹性裸金属

- os：CentOS-7-x86_64-Everything-1810.iso

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

## 搭建虚拟机环境

搭建一套虚拟机环境，跟物理机使用同一个iso，方便编译调试。

### iso下载

- [centos aarch64（arm64） iso 下载网址](https://blog.csdn.net/vah101/article/details/116691272)
- [https://vault.centos.org/altarch/7.6.1810/isos/aarch64/](https://vault.centos.org/altarch/7.6.1810/isos/aarch64/)
- [https://mirrors.aliyun.com/centos-vault/centos/7.6.1810/isos/x86_64/CentOS-7-x86_64-Everything-1810.iso](https://vault.centos.org/altarch/7.6.1810/isos/aarch64/)
- [安装CentOS Vault系统踩过的坑](https://www.cnblogs.com/yeziwinone/p/17916626.html)
- <https://mirrors.tuna.tsinghua.edu.cn/centos-vault/7.6.1810/os/>

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

### 安装编译工具

```bash
yum groupinstall –y “Development Tools”
yum install -y ncurses-devel make gcc bc bison flex elfutils-libelf-devel openssl-devel rpm-build redhat-rpm-config -y
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

#### 源码打rpm包

##### 直接make binrpm-pkg编译

```bash
time make binrpm-pkg -j64 2> make_error.log
```

产物如下：

```bash
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-headers-4.14.0-115.el7.0.1.aarch64.rpm
```

相较于`rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec`方式产物少了`kernel-devel`、`kernel-debuginfo`等开发调试包，不利于调试，建议使用下方命令。

##### rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec

也可直接使用rpmbuild编译出二进制的 RPM 包和对应的源代码 RPM 包：

```bash
rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec
```

使用`rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec`可以不执行上方的`rpmbuild -bp --nodeps ~/rpmbuild/SPECS/kernel-alt.spec`。

`rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec`编译产物如下：

```bash
Wrote: /root/rpmbuild/SRPMS/kernel-alt-4.14.0-115.el7.0.1.src.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-headers-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-common-aarch64-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-debuginfo-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-debuginfo-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-devel-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-debuginfo-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-devel-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-devel-4.14.0-115.el7.0.1.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-debuginfo-4.14.0-115.el7.0.1.aarch64.rpm
```

##### 开启内核编译选项panic on soft_lockup

```bash
cp -r ~/rpmbuild/SOURCES ~/rpmbuild/SOURCES.bak
```

```bash
cd ~/rpmbuild/SOURCES

sed -i 's/^# CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC is not set/CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC=y/g' *.config
sed -i 's/^CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC_VALUE=0/CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC_VALUE=1/g' *.config
```

这里直接用sed替换，貌似也不太好，但是确实能将开启内核编译选项panic on soft_lockup。

此流程建议通过patch的方式进行。

##### 自定义rpm包名

```bash
[root@centos SPECS]# diff kernel-alt.spec kernel-alt.spec.bak
22c22
< %define specrelease 115%{?dist}.0.1.ctyun.panic.soft_lockup
---
> %define specrelease 115%{?dist}.0.1
```

![diff kernel-alt.spec kernel-alt.spec.bak](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240725211511.png)

```bash
rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec
```

`rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec`编译产物如下：

```bash
Wrote: /root/rpmbuild/SRPMS/kernel-alt-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.src.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-headers-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-common-aarch64-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
```

##### 更改内核源码后编译

查看内核源码位置：

```bash
[root@centos SOURCES]# readlink -f linux-4.14.0-115.el7a.tar.xz
/root/rpmbuild/SOURCES/linux-4.14.0-115.el7a.tar.xz
```

复制linux-4.14.0-115.el7a.tar.xz到家目录：

```bash
cp /root/rpmbuild/SOURCES/linux-4.14.0-115.el7a.tar.xz ~
```

解压：

```bash
cd
tar -xf linux-4.14.0-115.el7a.tar.xz
```

修改文件，以init/main.c为例:

```bash
cd linux-4.14.0-115.el7a

vim init/main.c
```

重新压缩：

```bash
tar -cvf linux-4.14.0-115.el7a.tar.xz linux-4.14.0-115.el7a
```

将新的linux-4.14.0-115.el7a.tar.xz复制到/root/rpmbuild/SOURCES/下，记得先备份：

```bash
cp /root/rpmbuild/SOURCES/linux-4.14.0-115.el7a.tar.xz /root/rpmbuild/SOURCES/linux-4.14.0-115.el7a.bak.tar.xz
mv ~/linux-4.14.0-115.el7a.tar.xz /root/rpmbuild/SOURCES/linux-4.14.0-115.el7a.tar.xz
```

后续还是走`rpmbuild -ba ~/rpmbuild/SPECS/kernel-alt.spec`编译流程。

此流程建议通过patch的方式进行。

## initramfs无法加载根文件系统

将相关内核rpm包复制到物理机上：

```bash
rsync -avzP kernel*-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64*.rpm kmod-megaraid_sas-*.rpm root@11.96.0.42:~
```

kernel-headers 是 Linux 内核的头文件，它包含了 Linux 内核的 C 函数和数据结构定义。而 kernel-devel 则是 Linux 内核开发所需的一些工具和文件，包括内核源码、内核配置文件、内核模块等。简单来说，kernel-headers 是为了编译第三方软件使用的，而 kernel-devel 则是为了内核开发人员使用的。

- [kernel-header与kernel-devel的区别？](https://blog.csdn.net/weixin_42601134/article/details/129552893)

升级kernel-headers包，会卸载老的kernel-headers包：

```bash
rpm -Uvh kernel-headers-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
```

安装内核包、内核调试包等：

```bash
rpm -ivh kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm kernel-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm kernel-debuginfo-common-aarch64-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm kernel-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
```

在物理机安装`/root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm`包后发现initramfs无法挂载根文件系统。

在物理机上安装新内核后出现了新的故障，在initramfs无法加载根文件系统。

报错信息如下：

```bash
[  238.741337] dracut-initqueue[2520]: Warning: dracut-initqueue timeout - starting timeout scripts
[  239.311370] dracut-initqueue[2520]: Warning: dracut-initqueue timeout - starting timeout scripts
[  239.311673] dracut-initqueue[2520]: Warning: Could not boot.
```

![Warning:/devdisk/by-uuid/c38d268?-f12-469a-913f-549d23c6ba12 does not exist](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/915735adf44b509b57fb8c1d3ed60d3.png)

![20240724232711](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724232711.png)

![20240724233312](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724233312.png)

![20240724233909](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724233909.png)

### 使用镜像自带内核进入系统查看磁盘信息

```bash
[root@chuji-11-96-0-41 ~]# lsblk
NAME               MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
sda                  8:0    0 446.6G  0 disk
├─sda4               8:4    0 443.5G  0 part
│ ├─system-lv_swap 253:1    0    16G  0 lvm
│ └─system-lv_root 253:0    0    60G  0 lvm  /
├─sda2               8:2    0     1G  0 part /boot/efi
├─sda3               8:3    0     2G  0 part /boot
└─sda1               8:1    0   128M  0 part
```

```bash
[root@chuji-11-96-0-41 ~]# blkid
/dev/sda2: UUID="2450-2B50" TYPE="vfat" PARTLABEL="primary" PARTUUID="c7b2ae54-ca21-4ffc-b189-02c6aa03ca55"
/dev/sda3: UUID="7fd6bb97-b549-46e5-8fb2-415cc890ad50" TYPE="ext2" PARTLABEL="primary" PARTUUID="845bc094-af23-4e72-bfc6-a3f09cc55cde"
/dev/sda4: UUID="FlTv0Q-aeqn-ISav-dhNW-68Zj-4yGb-T04MF8" TYPE="LVM2_member" PARTLABEL="primary" PARTUUID="f664bac5-8672-430c-b2f0-247d58a51701"
/dev/mapper/system-lv_root: UUID="75336c23-e87e-4c30-949c-e272fc1dbcd7" TYPE="xfs"
/dev/mapper/system-lv_swap: UUID="cf0fe2e5-426e-4917-bf9c-4c352aa0521a" TYPE="swap"
/dev/sda1: PARTLABEL="primary" PARTUUID="c49b8dff-bf03-4702-a7d4-df04bad1732b"
```

![20240724235654](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240724235654.png)

![20240725001618](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240725001618.png)

```bash
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/sda) | grep -i drivers -C3
  looking at parent device '/devices/pci0000:00/0000:00:12.0/0000:05:00.0/host9/target9:3:111/9:3:111:0':
    KERNELS=="9:3:111:0"
    SUBSYSTEMS=="scsi"
    DRIVERS=="sd"
    ATTRS{evt_soft_threshold_reached}=="0"
    ATTRS{evt_mode_parameter_change_reported}=="0"
    ATTRS{inquiry}==""
--
  looking at parent device '/devices/pci0000:00/0000:00:12.0/0000:05:00.0/host9/target9:3:111':
    KERNELS=="target9:3:111"
    SUBSYSTEMS=="scsi"
    DRIVERS==""

  looking at parent device '/devices/pci0000:00/0000:00:12.0/0000:05:00.0/host9':
    KERNELS=="host9"
    SUBSYSTEMS=="scsi"
    DRIVERS==""

  looking at parent device '/devices/pci0000:00/0000:00:12.0/0000:05:00.0':
    KERNELS=="0000:05:00.0"
    SUBSYSTEMS=="pci"
    DRIVERS=="megaraid_sas"
    ATTRS{subsystem_device}=="0x4010"
    ATTRS{max_link_speed}=="16 GT/s"
    ATTRS{vendor}=="0x1000"
--
  looking at parent device '/devices/pci0000:00/0000:00:12.0':
    KERNELS=="0000:00:12.0"
    SUBSYSTEMS=="pci"
    DRIVERS=="pcieport"
    ATTRS{subsystem_device}=="0x371e"
    ATTRS{max_link_speed}=="16 GT/s"
    ATTRS{vendor}=="0x19e5"
--
  looking at parent device '/devices/pci0000:00':
    KERNELS=="pci0000:00"
    SUBSYSTEMS==""
    DRIVERS==""
```

```bash
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/sda1) | grep -i drivers
    DRIVERS==""
    DRIVERS=="sd"
    DRIVERS==""
    DRIVERS==""
    DRIVERS=="megaraid_sas"
    DRIVERS=="pcieport"
    DRIVERS==""
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/sda2) | grep -i drivers
    DRIVERS==""
    DRIVERS=="sd"
    DRIVERS==""
    DRIVERS==""
    DRIVERS=="megaraid_sas"
    DRIVERS=="pcieport"
    DRIVERS==""
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/sda3) | grep -i drivers
    DRIVERS==""
    DRIVERS=="sd"
    DRIVERS==""
    DRIVERS==""
    DRIVERS=="megaraid_sas"
    DRIVERS=="pcieport"
    DRIVERS==""
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/sda4) | grep -i drivers
    DRIVERS==""
    DRIVERS=="sd"
    DRIVERS==""
    DRIVERS==""
    DRIVERS=="megaraid_sas"
    DRIVERS=="pcieport"
    DRIVERS==""
```

```bash
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/mapper/system-lv_root)

Udevadm info starts with the device specified by the devpath and then
walks up the chain of parent devices. It prints for every device
found, all possible attributes in the udev rules key format.
A rule to match, can be composed by the attributes of the device
and the attributes from one single parent device.

  looking at device '/devices/virtual/block/dm-0':
    KERNEL=="dm-0"
    SUBSYSTEM=="block"
    DRIVER==""
    ATTR{range}=="1"
    ATTR{capability}=="10"
    ATTR{inflight}=="       0        0"
    ATTR{ext_range}=="1"
    ATTR{ro}=="0"
    ATTR{stat}=="    5777        0   477585     1200     1535        0    47944      100        0      760     1300"
    ATTR{removable}=="0"
    ATTR{size}=="125829120"
    ATTR{alignment_offset}=="0"
    ATTR{discard_alignment}=="0"
```

```bash
[root@chuji-11-96-0-41 ~]# udevadm info -a -p $(udevadm info -q path -n /dev/mapper/system-lv_swap)

Udevadm info starts with the device specified by the devpath and then
walks up the chain of parent devices. It prints for every device
found, all possible attributes in the udev rules key format.
A rule to match, can be composed by the attributes of the device
and the attributes from one single parent device.

  looking at device '/devices/virtual/block/dm-1':
    KERNEL=="dm-1"
    SUBSYSTEM=="block"
    DRIVER==""
    ATTR{range}=="1"
    ATTR{capability}=="10"
    ATTR{inflight}=="       0        0"
    ATTR{ext_range}=="1"
    ATTR{ro}=="0"
    ATTR{stat}=="     100        0    15616       30        0        0        0        0        0       30       30"
    ATTR{removable}=="0"
    ATTR{size}=="33554432"
    ATTR{alignment_offset}=="0"
    ATTR{discard_alignment}=="0"
```

```bash
[root@chuji-11-96-0-41 ~]# lsinitrd /boot/initramfs-4.14.0.img | grep megaraid_sas
-rw-r--r--   1 root     root      2237671 Jul 24 22:55 usr/lib/modules/4.14.0/kernel/drivers/scsi/megaraid/megaraid_sas.ko
```

```bash
[root@chuji-11-96-0-41 ~]# lsinitrd /boot/initramfs-4.14.0-115.el7a.0.1.aarch64.img | grep megaraid_sas
-rw-r--r--   1 root     root        67020 Jul 19 17:22 usr/lib/modules/4.14.0-115.el7a.0.1.aarch64/extra/megaraid_sas.ko.xz
```

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

dracut -f --add-drivers crypto initramfs-4.14.0.img 4.14.0

dracut -f --add-drivers /usr/lib/modules/4.14.0/kernel/drivers/scsi/scsi_transport_sas initramfs-`uname -r`.img `uname -r`
```

将`/usr/lib/modules/4.14.0/kernel/drivers/scsi`路径下的所有内容打包到initramfs下的`/usr/lib/modules/4.14.0/kernel/drivers/scsi`:

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/scsi /usr/lib/modules/4.14.0/kernel/drivers/scsi initramfs-`uname -r`.img `uname -r`

dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/scsi /usr/lib/modules/4.14.0/kernel/drivers/scsi initramfs-4.14.0.img 4.14.0
```

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/md /usr/lib/modules/4.14.0/kernel/drivers/md initramfs-`uname -r`.img `uname -r`

dracut -f -i /usr/lib/modules/4.14.0/kernel/drivers/md /usr/lib/modules/4.14.0/kernel/drivers/md initramfs-4.14.0.img 4.14.0
```

```bash
dracut -f -i /usr/lib/modules/4.14.0/kernel/lib/raid6 /usr/lib/modules/4.14.0/kernel/lib/raid6 initramfs-`uname -r`.img `uname -r`

dracut -f -i /usr/lib/modules/4.14.0/kernel/lib/raid6 /usr/lib/modules/4.14.0/kernel/lib/raid6 initramfs-4.14.0.img 4.14.0
```

将上述模块都添加到initramfs中后，还是卡在无法挂载根文件系统。

### 编译安装新版本megaraid_sas驱动

从<https://www.broadcom.com/products/storage/raid-controllers/megaraid-9560-8i>下载`kmod-megaraid_sas-07.729.00.00-1.src.rpm`包。

```bash
wget https://docs.broadcom.com/docs-and-downloads/07.729.00.00-1_MR7.29_Linux_driver.zip
```

解压即可看到`kmod-megaraid_sas-07.729.00.00-1.src.rpm`源码包。

`e:/Downloads/07.729.00.00-1_MR7.29_Linux_driver/mrlinuxdrv_rel/megaraid_sas_components/megaraid_sas_components/kmod_srpm/kmod-megaraid_sas-07.729.00.00-1.src.rpm`

当前目录下也有一份`kmod-megaraid_sas-07.729.00.00-1.src.rpm`离线包可供下载。

安装megaraid_sas源码包：

```bash
rpm -ivh kmod-megaraid_sas-07.729.00.00-1.src.rpm
```

编译megaraid_sas源码包：

```bash
rpmbuild -ba ~/rpmbuild/SPECS/megaraid_sas.spec

Wrote: /root/rpmbuild/SRPMS/kernel-alt-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.src.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-headers-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-common-aarch64-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/perf-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/python-perf-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-libs-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-tools-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-devel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
Wrote: /root/rpmbuild/RPMS/aarch64/kernel-debug-debuginfo-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
```

先将物理机切换到系统自带的内核，以便正常进系统。

进入系统后安装上一步编译出的megaraid_sas rpm包`/root/rpmbuild/RPMS/aarch64/kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm`:

```bash
rpm -ivh kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.rpm
```

检查initramfs中时候包含megaraid_sas驱动：

```bash
[root@chuji-11-96-0-41 ~]# lsinitrd /boot/initramfs-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64.img | grep megaraid_sas
drwxr-xr-x   2 root     root            0 Jul 25 14:59 usr/lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/extra/megaraid_sas
-rw-r--r--   1 root     root       285208 Jul 25 14:59 usr/lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/extra/megaraid_sas/megaraid_sas.ko
```

上述结果表明megaraid_sas驱动已压缩到调试内核对应的initramfs中。

重启物理机，选择调试内核，发现可以挂载根文件系统，可以进入系统。

## 禁止mlx网卡驱动加载

在GRUB_CMDLINE_LINUX新增参数禁止mlx网卡驱动加载：

```bash
vim /etc/default/grub

initcall_blacklist=mlx5_core_init module_blacklist=mlx5_core
```

增加参数后的`/etc/default/grub`:

```bash
cat /etc/default/grub
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="$(sed 's, release .*$,,g' /etc/system-release)"
GRUB_DEFAULT=saved
GRUB_DISABLE_SUBMENU=true
GRUB_TERMINAL="serial console"
GRUB_SERIAL_COMMAND="serial --speed=115200"
GRUB_CMDLINE_LINUX="console=tty0 pci=realloc hardened_usercopy=off noirqdebug pciehp.pciehp_force=1 crashkernel=512M initcall_blacklist=mlx5_ib_init,mx5_core_init module_blacklist=mlx5_ib,mlx5_core biosdevname=0 net.ifnames=0 console=ttyS0,115200n8"
GRUB_DISABLE_RECOVERY="true"
```

```bash
cat /proc/cmdline
BOOT_IMAGE=/vmlinuz-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 root=/dev/mapper/system-lv_root ro console=tty0 pci=realloc hardened_usercopy=off noirqdebug pciehp.pciehp_force=1 crashkernel=512M initcall_blacklist=mlx5_ib_init,mx5_core_init module_blacklist=mlx5_ib,mlx5_core biosdevname=0 net.ifnames=0 console=ttyS0,115200n8
```

进入系统后，使用lsmod命令查看mlx5_core驱动是否加载：

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# lsmod | grep mlx5_core
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]#
```

从上面的输出看到，mlx5_core驱动模块未加载。

禁止mlx网卡驱动加载后，reboot还是soft_lockup。

## kdump 服务无法正常启动

将`crashkernel`参数配置成`crashkernel=768M`或者`crashkernel=1024M.high`或者`crashkernel=2048M.high`，kdump服务都无法正常启动。

将`crashkernel`参数配置成`crashkernel=auto`或者`crashkernel=512M`后，kdump可以正常启动，但是会kudmp在生成转储文件的时候会发生oom，导致没有生成转储文件。

- [kdump安装及调试策略（详细）](https://blog.csdn.net/lindorx/article/details/135489412)

### kdump生成转储文件时oom

修改kdumpctl脚本，在第二内核启动时禁用部分驱动。具体修改方法为：

vim /usr/bin/kdumpctl，注释掉prepare_cmdline()函数的cmdline变量赋值语句，添加一行cmdline变量的赋值语句，赋值的内容为cat /proc/cmdline命令的显示结果，并且加上modprobe.blacklist=hisi_sas_v3_hw,hisi_sas_main，配置完成后使用systemctl restart kdump.service重启kdump服务。

```bash
vim /usr/bin/kdumpctl

cmdline="BOOT_IMAGE=/vmlinuz-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 root=/dev/mapper/system-lv_root ro console=tty0 pci=realloc hardened_usercopy=off noirqdebug pciehp.pciehp_force=1 crashkernel=512M initcall_blacklist=mlx5_ib_init,mx5_core_init module_blacklist=mlx5_ib,mlx5_core modprobe.blacklist=hisi_sas_v3_hw,hisi_sas_main biosdevname=0 net.ifnames=0 console=ttyS0,115200n8"
```

![modprobe.blacklist=hisi_sas_v3_hw,hisi_sas_main](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240725221906.png)

```bash
systemctl restart kdump.service
```

- [TaiShan 200 2280 CentOS7.6无法生成kdump日志](https://support.huawei.com/enterprise/zh/knowledge/EKB1100058786)

建议将物理机重启，选择调试内进入系统后，执行reboot命令：

```bash
reboot
```

这回可以生成kudmp了，具体参见下图。

![kdump](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240725203101.png)

```bash
[root@chuji-11-96-0-41 ~]# cd /var/crash/127.0.0.1-2024-07-25-20\:29\:18/
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# ls
vmcore  vmcore-dmesg.txt
```

### 根据vmcore确定根因

高精度时钟堆栈是内核watch dog线程发现网卡enp0导致cpu soft lockup后打印的。

enp0驱动指向了hns3。

驱动提供方为华为海思。

```bash
# vim vmcore-dmesg.txt +3055

3055 [  139.896889] NETDEV WATCHDOG: enp0 (hns3): transmit queue 0 timed out
3056 [  139.896911] ------------[ cut here ]------------
3057 [  139.896925] WARNING: CPU: 8 PID: 0 at net/sched/sch_generic.c:320 dev_watchdog+0x2b0/0x2b8
3058 [  139.896927] Modules linked in: xt_conntrack ipt_MASQUERADE nf_nat_masquerade_ipv4 nf_conntrack_netlink nfnetlink xt_addrtype iptable_filter iptable_nat nf_conntrack_ipv4 nf_defrag_ipv4 nf_nat_ipv4 nf_nat nf_conntrack br_netfilter bridge 8021q ga     rp mrp stp llc bonding tun uio_pci_generic uio vfio_pci vfio_virqfd vfio_iommu_type1 vfio cuse fuse overlay ib_ipoib(OE) ib_cm(OE) ib_uverbs(OE) ib_umad(OE) ib_core(OE) sunrpc vfat fat ext4 mbcache jbd2 crc32_ce ghash_ce sha2_ce sha256_arm64 sha1_c     e sbsa_gwdt ses enclosure sg gpio_dwapb gpio_generic ipmi_ssif ipmi_devintf ipmi_msghandler dm_multipath knem(OE) ip_tables xfs libcrc32c virtio_net mlxfw(OE) ptp pps_core hibmc_drm auxiliary(OE) devlink drm_kms_helper virtio_pci mlx_compat(OE) sys     copyarea sysfillrect virtio_ring sysimgblt virtio fb_sys_fops ttm
3059 [  139.897097]  drm hns3 hclge hnae3 hisi_sas_v3_hw hisi_sas_main megaraid_sas(OE) libsas scsi_transport_sas i2c_designware_platform i2c_designware_core dm_mirror dm_region_hash dm_log dm_mod
3060 [  139.897134] CPU: 8 PID: 0 Comm: swapper/8 Kdump: loaded Tainted: G        W  OE  ------------   4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 #1
3061 [  139.897136] Hardware name: HUAKUN HuaKun AT3500 G3/IT21SK4E, BIOS 12.01.08 03/22/2024
3062 [  139.897138] task: ffff80302b853200 task.stack: ffff00000bb60000
3063 [  139.897143] PC is at dev_watchdog+0x2b0/0x2b8
3064 [  139.897146] LR is at dev_watchdog+0x2b0/0x2b8
3065 [  139.897149] pc : [<ffff000008754640>] lr : [<ffff000008754640>] pstate: 60400009
3066 [  139.897151] sp : ffff0000099efd40
3067 [  139.897153] x29: ffff0000099efd40 x28: ffff80302b853200
3068 [  139.897159] x27: ffff000008d52120 x26: ffff000008db0000
3069 [  139.897164] x25: 00000000ffffffff x24: 0000000000000008
3070 [  139.897169] x23: ffff80283209d49c x22: ffff80283190d280
3071 [  139.897174] x21: ffff80283190d200 x20: ffff80283209d000
3072 [  139.897179] x19: ffff000008db1000 x18: 0000ffffdefffdf0
3073 [  139.897184] x17: 0000ffff9d140028 x16: ffff0000080da4d4
3074 [  139.897188] x15: 0000000000000000 x14: 0000000000000000
3075 [  139.897193] x13: 0000000000000000 x12: 0000000000000bee
3076 [  139.897197] x11: 0000000000000004 x10: 0000000000000bef
3077 [  139.897202] x9 : 00000000001bb120 x8 : ffffe057ffa44f28
3078 [  139.897207] x7 : 0000000000000000 x6 : 0000000000000000
3079 [  139.897212] x5 : ffffe057cd6369f0 x4 : 0000000000000000
3080 [  139.897216] x3 : 0000000000000001 x2 : 419176834f6d5000
3081 [  139.897221] x1 : 419176834f6d5000 x0 : 0000000000000038
3082 [  139.897226] Call trace:
3083 [  139.897230] Exception stack(0xffff0000099efc00 to 0xffff0000099efd40)
3084 [  139.897235] fc00: 0000000000000038 419176834f6d5000 419176834f6d5000 0000000000000001
3085 [  139.897238] fc20: 0000000000000000 ffffe057cd6369f0 0000000000000000 0000000000000000
3086 [  139.897241] fc40: ffffe057ffa44f28 00000000001bb120 0000000000000bef 0000000000000004
3087 [  139.897244] fc60: 0000000000000bee 0000000000000000 0000000000000000 0000000000000000
3088 [  139.897247] fc80: ffff0000080da4d4 0000ffff9d140028 0000ffffdefffdf0 ffff000008db1000
3089 [  139.897251] fca0: ffff80283209d000 ffff80283190d200 ffff80283190d280 ffff80283209d49c
3090 [  139.897254] fcc0: 0000000000000008 00000000ffffffff ffff000008db0000 ffff000008d52120
3091 [  139.897257] fce0: ffff80302b853200 ffff0000099efd40 ffff000008754640 ffff0000099efd40
3092 [  139.897260] fd00: ffff000008754640 0000000060400009 ffff0000015c02e8 0000000000000000
3093 [  139.897263] fd20: 0000ffffffffffff ffff80283190d280 ffff0000099efd40 ffff000008754640
3094 [  139.897268] [<ffff000008754640>] dev_watchdog+0x2b0/0x2b8
3095 [  139.897274] [<ffff00000815caf8>] call_timer_fn+0x54/0x16c
3096 [  139.897278] [<ffff00000815cd04>] expire_timers+0xf4/0x170
3097 [  139.897281] [<ffff00000815ce34>] run_timer_softirq+0xb4/0x1dc
3098 [  139.897286] [<ffff000008081a9c>] __do_softirq+0x11c/0x2f0
3099 [  139.897290] [<ffff0000080db6ac>] irq_exit+0x120/0x150
3100 [  139.897297] [<ffff000008140b08>] __handle_domain_irq+0x78/0xc4
3101 [  139.897300] [<ffff000008081868>] gic_handle_irq+0xa0/0x1b8
```

![NETDEV WATCHDOG: enp0 (hns3): transmit queue 0 timed out](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/ec67d505f622676ed7bca734415116c.jpg)

```c
// vim net/sched/sch_generic.c +320

291 static void dev_watchdog(unsigned long arg)
292 {
293         struct net_device *dev = (struct net_device *)arg;
294
295         netif_tx_lock(dev);
296         if (!qdisc_tx_is_noop(dev)) {
297                 if (netif_device_present(dev) &&
298                     netif_running(dev) &&
299                     netif_carrier_ok(dev)) {
300                         int some_queue_timedout = 0;
301                         unsigned int i;
302                         unsigned long trans_start;
303
304                         for (i = 0; i < dev->num_tx_queues; i++) {
305                                 struct netdev_queue *txq;
306
307                                 txq = netdev_get_tx_queue(dev, i);
308                                 trans_start = txq->trans_start;
309                                 if (netif_xmit_stopped(txq) &&
310                                     time_after(jiffies, (trans_start +
311                                                          dev->watchdog_timeo))) {
312                                         some_queue_timedout = 1;
313                                         txq->trans_timeout++;
314                                         break;
315                                 }
316                         }
317
318                         if (some_queue_timedout) {
319                                 WARN_ONCE(1, KERN_INFO "NETDEV WATCHDOG: %s (%s): transmit queue %u timed out\n",
320                                        dev->name, netdev_drivername(dev), i);
321                                 dev->netdev_ops->ndo_tx_timeout(dev);
322                         }
323                         if (!mod_timer(&dev->watchdog_timer,
324                                        round_jiffies(jiffies +
325                                                      dev->watchdog_timeo)))
326                                 dev_hold(dev);
327                 }
328         }
329         netif_tx_unlock(dev);
330
331         dev_put(dev);
332 }
```

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# ethtool -i enp0
driver: hns3
version: 4.14.0-115.el7.0.1.ctyun.panic.
firmware-version: 0x0114001b
expansion-rom-version:
bus-info: 0000:bd:00.0
supports-statistics: yes
supports-test: yes
supports-eeprom-access: no
supports-register-dump: yes
supports-priv-flags: no
```

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# modinfo hns3
filename:       /lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/kernel/drivers/net/ethernet/hisilicon/hns3/hns3.ko.xz
version:        1.0
alias:          pci:hns-nic
license:        GPL
author:         Huawei Tech. Co., Ltd.
description:    HNS3: Hisilicon Ethernet Driver
rhelversion:    7.6
srcversion:     C47CD4CEBBA5E0E90661D99
alias:          pci:v000019E5d0000A22Fsv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A22Esv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A226sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A225sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A224sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A223sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A222sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A221sv*sd*bc*sc*i*
alias:          pci:v000019E5d0000A220sv*sd*bc*sc*i*
depends:        hnae3
intree:         Y
name:           hns3
vermagic:       4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 SMP mod_unload modversions aarch64
```

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# yum provides /lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/kernel/drivers/net/ethernet/hisilicon/hns3/hns3.ko.xz
Loaded plugins: fastestmirror, langpacks
Loading mirror speeds from cached hostfile
kernel-4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 : The Linux kernel
Repo        : installed
Matched from:
Filename    : /lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/kernel/drivers/net/ethernet/hisilicon/hns3/hns3.ko.xz
```

hns3是核内驱动，建议使用核外驱动排查一下是否还会导致reboot时cpu soft lockup。

![ethtool -i enp0 modinfo hns3](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/9c97ab1b42c7fba03a674c70b3f326b.jpg)

```bash
# vim vmcore-dmesg.txt +3190

3190 [  164.106663] Kernel panic - not syncing: softlockup: hung tasks
3191 [  164.114920] CPU: 0 PID: 0 Comm: swapper/0 Kdump: loaded Tainted: G        W  OEL ------------   4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64 #1
3192 [  164.133349] Hardware name: HUAKUN HuaKun AT3500 G3/IT21SK4E, BIOS 12.01.08 03/22/2024
3193 [  164.146366] Call trace:
3194 [  164.151375] [<ffff000008089e14>] dump_backtrace+0x0/0x23c
3195 [  164.159407] [<ffff00000808a074>] show_stack+0x24/0x2c
3196 [  164.167113] [<ffff0000088568a8>] dump_stack+0x84/0xa8
3197 [  164.174811] [<ffff0000080d4adc>] panic+0x138/0x2a0
3198 [  164.182227] [<ffff0000081b1170>] watchdog+0x0/0x38
3199 [  164.189649] [<ffff00000815eff8>] __hrtimer_run_queues+0x110/0x2f8
3200 [  164.198420] [<ffff00000815f428>] hrtimer_interrupt+0xc0/0x23c
3201 [  164.206861] [<ffff0000086c7a50>] arch_timer_handler_phys+0x3c/0x48
3202 [  164.215797] [<ffff000008146810>] handle_percpu_devid_irq+0x98/0x210
3203 [  164.224851] [<ffff0000081402fc>] generic_handle_irq+0x34/0x4c
3204 [  164.233373] [<ffff000008140afc>] __handle_domain_irq+0x6c/0xc4
3205 [  164.241993] [<ffff000008081868>] gic_handle_irq+0xa0/0x1b8
3206 [  164.250263] Exception stack(0xffff00000800fca0 to 0xffff00000800fde0)
3207 [  164.259488] fca0: 3ffffffffffffffe 419176834f6d5000 ffff000008db3000 ffff000008dc5600
3208 [  164.272819] fcc0: 0000e057c4760000 0000000000000000 ffff000009601d00 ffff000008d50018
3209 [  164.286460] fce0: 000000000000006f 000000009b64c2b0 000000000b28d3ad 000000000000004c
3210 [  164.300439] fd00: 0000000000000033 0000000000000019 0000000000000001 0000000000000007
3211 [  164.314778] fd20: 000000000000000e ffff000008954bb0 ffffe057cd4ca720 00000000000007d0
3212 [  164.329297] fd40: 0000000000000003 ffff000008db1000 0000000000000008 0000000000000003
3213 [  164.343905] fd60: ffff000008f216d0 ffff000008d6e980 ffff000008db0000 ffff000008d52120
3214 [  164.358551] fd80: ffff000008dc5600 ffff00000800fde0 ffff000008722878 ffff00000800fde0
3215 [  164.373304] fda0: ffff000008159dac 0000000000400009 ffff000009601d00 ffff000008d50018
3216 [  164.388348] fdc0: 0000ffffffffffff 000000009b64c2b0 ffff00000800fde0 ffff000008159dac
3217 [  164.403797] [<ffff0000080830b0>] el1_irq+0xb0/0x140
3218 [  164.412586] [<ffff000008159dac>] __usecs_to_jiffies+0x34/0x54
3219 [  164.422254] [<ffff000008722878>] net_rx_action+0x58/0x470
3220 [  164.431569] [<ffff000008081a9c>] __do_softirq+0x11c/0x2f0
3221 [  164.440861] [<ffff0000080db6ac>] irq_exit+0x120/0x150
3222 [  164.449785] [<ffff000008140b08>] __handle_domain_irq+0x78/0xc4
3223 [  164.459503] [<ffff000008081868>] gic_handle_irq+0xa0/0x1b8
3224 [  164.468877] Exception stack(0xffff000008d8fd80 to 0xffff000008d8fec0)
3225 [  164.479275] fd80: 0000e057c4760000 ffff000008d50018 ffff000008dc0c44 0000000000000001
3226 [  164.494945] fda0: 0000000000000000 ffffe057cd4b9010 ffff00000979ad40 ffffe057cd4b9168
3227 [  164.510674] fdc0: ffff000008dc6360 ffff000008d8fe30 0000000000000d00 ffff0000088d2590
3228 [  164.526396] fde0: 0000000000000000 0000000000000005 0000000000000000 0000000000000007
3229 [  164.542111] fe00: 000000000000000e ffff000008954bb0 ffff000059f8fd38 ffff000008f224b8
3230 [  164.557836] fe20: ffff000008d50000 ffff000008f22000 0000000000000000 ffff000008dbc50c
3231 [  164.573548] fe40: 0000000000000000 0000000000000000 00000057fff8b460 00000057ffed8be0
3232 [  164.589263] fe60: 0000000000c00018 ffff000008d8fec0 ffff0000080858c8 ffff000008d8fec0
3233 [  164.604977] fe80: ffff0000080858cc 0000000060c00009 ffff000008d8fea0 ffff000008152050
3234 [  164.620701] fea0: ffffffffffffffff ffff0000081520b8 ffff000008d8fec0 ffff0000080858cc
3235 [  164.636423] [<ffff0000080830b0>] el1_irq+0xb0/0x140
3236 [  164.645208] [<ffff0000080858cc>] arch_cpu_idle+0x44/0x144
3237 [  164.654485] [<ffff000008871b68>] default_idle_call+0x20/0x30
3238 [  164.664028] [<ffff00000812482c>] do_idle+0x158/0x1cc
3239 [  164.672859] [<ffff000008124a3c>] cpu_startup_entry+0x28/0x30
3240 [  164.682401] [<ffff00000886af54>] rest_init+0xbc/0xc8
3241 [  164.691249] [<ffff000008c00dac>] start_kernel+0x410/0x43c
3242 [  164.700793] SMP: stopping secondary CPUs
3243 [  164.725314] Starting crashdump kernel...
3244 [  164.733064] Bye!
```

![Kernel panic - not syncing: softlockup: hung tasks](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_17219115889811.png)

进一步使用crash查看vmcore转储文件内容：

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# crash /usr/lib/debug/lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/vmlinux vmcore                                                                                          [162/1929]

crash 7.2.3-8.el7
Copyright (C) 2002-2017  Red Hat, Inc.
Copyright (C) 2004, 2005, 2006, 2010  IBM Corporation
Copyright (C) 1999-2006  Hewlett-Packard Co
Copyright (C) 2005, 2006, 2011, 2012  Fujitsu Limited
Copyright (C) 2006, 2007  VA Linux Systems Japan K.K.
Copyright (C) 2005, 2011  NEC Corporation
Copyright (C) 1999, 2002, 2007  Silicon Graphics, Inc.
Copyright (C) 1999, 2000, 2001, 2002  Mission Critical Linux, Inc.
This program is free software, covered by the GNU General Public License,
and you are welcome to change it and/or distribute copies of it under
certain conditions.  Enter "help copying" to see the conditions.
This program has absolutely no warranty.  Enter "help warranty" for details.

GNU gdb (GDB) 7.6
Copyright (C) 2013 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "aarch64-unknown-linux-gnu"...

crash: seek error: kernel virtual address: ffffe057cd4b0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd4e0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd510090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd540090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd570090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd5a0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd5d0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd600090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd630090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd660090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd690090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd6c0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd6f0090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd720090  type: "IRQ stack pointer"
crash: seek error: kernel virtual address: ffffe057cd750090  type: "IRQ stack pointer"
```

- [arm64-kernel-5.4-crash-7.3.0-run-directly-error #91](https://github.com/crash-utility/crash/issues/91)
- [crash_arm64 can not load coredump #134](https://github.com/crash-utility/crash/issues/134)
- [Getting the error message "crash: seek error: kernel virtual address: XXXXXXXXXXXXXXXX type: "cpu_possible_mask", while opening an incomplete vmcore in "crash utility".](https://access.redhat.com/solutions/171713)

上述红帽链接建议附加`--minimal`参数：

```bash
[root@chuji-11-96-0-41 127.0.0.1-2024-07-25-20:29:18]# crash --minimal  /usr/lib/debug/lib/modules/4.14.0-115.el7.0.1.ctyun.panic.soft_lockup.aarch64/vmlinux vmcore

crash 7.2.3-8.el7
Copyright (C) 2002-2017  Red Hat, Inc.
Copyright (C) 2004, 2005, 2006, 2010  IBM Corporation
Copyright (C) 1999-2006  Hewlett-Packard Co
Copyright (C) 2005, 2006, 2011, 2012  Fujitsu Limited
Copyright (C) 2006, 2007  VA Linux Systems Japan K.K.
Copyright (C) 2005, 2011  NEC Corporation
Copyright (C) 1999, 2002, 2007  Silicon Graphics, Inc.
Copyright (C) 1999, 2000, 2001, 2002  Mission Critical Linux, Inc.
This program is free software, covered by the GNU General Public License,
and you are welcome to change it and/or distribute copies of it under
certain conditions.  Enter "help copying" to see the conditions.
This program has absolutely no warranty.  Enter "help warranty" for details.

GNU gdb (GDB) 7.6
Copyright (C) 2013 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "aarch64-unknown-linux-gnu"...

NOTE: minimal mode commands: log, dis, rd, sym, eval, set, extend and exit

crash>
```

只能使用几个命令，基本上没多大用。

## 禁掉hns3驱动

```bash
lsmod | grep -i hns3
hns3                  262144  0
hnae3                 262144  2 hns3,hclge
```

在GRUB_CMDLINE_LINUX新增参数禁止hns3驱动加载：

```bash
vim /etc/default/grub

initcall_blacklist=hns3_init,hns_roce_init,hns_roce_hw_v2_init module_blacklist=hns3,hns_roce,hns_roce_hw_v2 initcall_debug no_console_suspend ignore_loglevel
```

hns_roce、hns_roce_hw_v2s是后续建议禁用的选项，禁用这两者的时候不要禁用hns3。

```bash
[root@chuji-11-96-0-41 ~]# cat /etc/default/grub
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="$(sed 's, release .*$,,g' /etc/system-release)"
GRUB_DEFAULT=saved
GRUB_DISABLE_SUBMENU=true
GRUB_TERMINAL="serial console"
GRUB_SERIAL_COMMAND="serial --speed=115200"
GRUB_CMDLINE_LINUX="console=tty0 initcall_blacklist=hns3_init module_blacklist=hns3 initcall_debug no_console_suspend ignore_loglevel pci=realloc hardened_usercopy=off noirqdebug pciehp.pciehp_force=1 crashkernel=2048M biosdevname=0 net.ifnames=0 console=ttyS0,115200n8"
GRUB_DISABLE_RECOVERY="true"
```

```bash
[root@chuji-11-96-0-41 ~]# grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg
/etc/sysconfig/kernel: line 7: alias: scsi_hostadapter0: not found
/etc/sysconfig/kernel: line 7: alias: megaraid_sas: not found
Generating grub configuration file ...
Found linux image: /boot/vmlinuz-4.14.0-115.el7a.0.1.aarch64
Found initrd image: /boot/initramfs-4.14.0-115.el7a.0.1.aarch64.img
Found linux image: /boot/vmlinuz-0-rescue-4ad9ae0bc10540edac7304e5603fd71e
Found initrd image: /boot/initramfs-0-rescue-4ad9ae0bc10540edac7304e5603fd71e.img
done
```

grub中禁掉hns3驱动后根本进不去系统，一下USB disconnect。

![initcall_blacklist=hns3_init module_blacklist=hns3 initcall_debug no_console_suspend ignore_loglevel](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240727222640.png)
