# qemu-kvm

- [<font color=Red>虚拟化指南</font>](https://documentation.suse.com/zh-cn/sles/15-SP4/html/SLES-all/cha-kvm.html)
- [<font color=Red>Linux虚拟化</font>](https://www.cnblogs.com/LoyenWang/category/1828942.html)

- [服务器虚拟化组件有哪些？](http://c.biancheng.net/view/3842.html)
- [ubuntu18.04上搭建KVM虚拟机环境超完整过程](https://mp.weixin.qq.com/s/FVyzPVwwQ85AC4jlVZvF4g)

- <https://www.qemu.org/docs/master/system/invocation.html>
- [QEMU (简体中文)](https://wiki.archlinuxcn.org/wiki/QEMU)
- [QEMU-KVM基本原理](https://www.toutiao.com/article/7194721406406787623)
- [<font color=Red>Qemu&KVM 第一篇（2） qemu kvm 相关知识</font>](https://blog.csdn.net/weixin_34253539/article/details/93084893)
- [<font color=Red>虚拟化技术 — 硬件辅助的虚拟化技术</font>](https://mp.weixin.qq.com/s/NsdNFhoP0QwjfhsQIeRWeQ)
- [<font color=Red>虚拟化技术 — QEMU-KVM 基于内核的虚拟机</font>](https://mp.weixin.qq.com/s/sn-TTwldA81uFuVn5NBMhg)

- [<font color=Red>https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu</font>](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/qemu)
- [<font color=Red>Linux内核调试</font>](https://blog.csdn.net/onlylove_/category_11607029.html)

- [Quickly create and run optimised Windows, macOS and Linux virtual machines](https://github.com/quickemu-project/quickemu)

## book

- [QEMU KVM学习笔记](https://github.com/yifengyou/learn-kvm)
- [kvm虚拟化技术：实战与原理解析.pdf](http://www.downcc.com/soft/317578.html)
- [QEMU/KVM源码解析与应用-李强编著-微信读书](https://weread.qq.com/web/bookDetail/ec132be07263ffc1ec1dc10)

## virt-manager

- [<font color=Red>虚拟化入门指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_getting_started_guide/index)
- [<font color=Red>虚拟化部署和管理指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/index)
- [<font color=Red>How to Install and Configure KVM on Debian 11 Bullseye Linux</font>](https://linux.how2shout.com/how-to-install-and-configure-kvm-on-debian-11-bullseye-linux/)

  ```bash
  sudo apt install virt-manager -y
  sudo virsh net-list --all
  sudo virsh net-start default
  sudo virsh net-autostart default
  ```

  uos20-1060 arm64建议通过下方命令安装：

  ```bash
  sudo apt-get install libvirt0 libvirt-daemon qemu virt-manager bridge-utils libvirt-clients python-libvirt qemu-efi uml-utilities virtinst qemu-system libvirt-daemon-system
  sudo virsh net-list --all
  sudo virsh net-start default
  sudo virsh net-autostart default
  ```

### libvirt

- [KVM虚拟化解决方案系列之KVM管理工具-libvirt介绍篇](https://blog.csdn.net/jianghu0755/article/details/129776841)
- [CentOS创建KVM虚拟机-在Edit→Preferences里面开启XML文件编辑功能](https://tinychen.com/20200405-centos-create-kvm-vm/)
- [<font color=Red>libvirt的virsh命令和qemu参数转换</font>](https://blog.csdn.net/YuZhiHui_No1/article/details/53909925)
- [libvirt and QEMU 基础篇](https://blog.csdn.net/lingshengxiyou/article/details/128665491)
- [KVM虚拟化解决方案系列之KVM管理工具-libvirt介绍篇](https://blog.csdn.net/jianghu0755/article/details/129776841)
- [CentOS创建KVM虚拟机-在Edit→Preferences里面开启XML文件编辑功能](https://tinychen.com/20200405-centos-create-kvm-vm/)
- [<font color=Red>libvirt的virsh命令和qemu参数转换</font>](https://blog.csdn.net/YuZhiHui_No1/article/details/53909925)
- [libvirt and QEMU 基础篇](https://blog.csdn.net/lingshengxiyou/article/details/128665491)

### virsh

- [virsh命令来创建虚拟机](https://www.cnblogs.com/klb561/p/8724487.html)
- [<font color=Red>虚拟化部署和管理指南</font>](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/index)
- [15.5 更改引导选项](https://documentation.suse.com/zh-cn/sles/15-SP2/html/SLES-all/cha-libvirt-config-virsh.html#sec-libvirt-config-boot-menu-virsh)
- [libvirt-使用iso镜像创建主机&修改启动盘&启动](https://blog.csdn.net/qq_25730711/article/details/72835565)
- [Libvirt: how to pass qemu command line args?](https://unix.stackexchange.com/questions/235414/libvirt-how-to-pass-qemu-command-line-args)

#### qemu-img

如果你需要创建一个新的虚拟磁盘镜像，你可以使用 `qemu-img` 工具来完成。

```bash
qemu-img create -f qcow2 yql-openeuler.img 200G
```

#### virt-format

如果你已经有一个存在的 `qcow2` 格式的虚拟磁盘镜像，但希望对其进行格式化（即清空数据，重置为初始状态）。

1. **安装 `libguestfs-tools` 包**：

   首先确保你已经安装了 `libguestfs-tools` 包，这个包中包含了 `virt-format` 工具。

   ```bash
   sudo apt-get install libguestfs-tools   # 对于基于 Debian/Ubuntu 的系统
   ```

   或者

   ```bash
   sudo yum install libguestfs-tools       # 对于基于 CentOS/RHEL 的系统
   ```

2. **使用 `virt-format` 命令格式化 `qcow2` 虚拟磁盘镜像**：

   假设你要格式化的虚拟磁盘镜像是 `/var/lib/libvirt/images/mydisk.qcow2`，可以通过以下命令来执行格式化：

   ```bash
   virt-format --format=qcow2 -a /var/lib/libvirt/images/mydisk.qcow2
   ```

   - `--format=qcow2`：指定要格式化为 `qcow2` 格式。
   - `-a /var/lib/libvirt/images/mydisk.qcow2`：指定要操作的虚拟磁盘镜像文件路径。

3. **确认格式化结果**：

   格式化完成后，你可以使用 `virt-df` 命令来查看虚拟磁盘镜像的分区和使用情况，确认数据已经被清空并且恢复到初始状态。

   ```sh
   virt-df -a /var/lib/libvirt/images/mydisk.qcow2
   ```

#### virt-install

##### CentOS

- [CentOS 7 virt-install 命令行方式（非图形界面）安装KVM虚拟机](https://blog.csdn.net/mshxuyi/article/details/99852820)
- [anaconda在tmux文本模式时的快捷键](https://blog.csdn.net/weixin_53389944/article/details/130849245)

创建一个名为 yql-openeuler 的虚拟机，配置了适当的内存、CPU、磁盘、安装位置以及启动参数，以便正确连接到串口控制台和控制台输出:

```bash
virt-install \
  --name yql-openeuler \
  --ram 32768 \
  --vcpus 64 \
  --disk path=/inf/yql/yql-openeuler.qcow2,size=200 \
  --location /inf/yql/openeuler-22.09-240618-x86_64-dvd.iso \
  --os-type generic \
  --network default \
  --graphics none \
  --console pty,target_type=serial \
  -x 'console=ttyS0,115200n8 console=tty0' \
  --extra-args 'console=ttyS0,115200n8'
```

参数说明:

- `--name yql-openeuler`：设置虚拟机的名称为 `yql-openeuler`。
- `--ram 32768`：分配 32GB 内存给虚拟机（单位为 MB）。
- `--vcpus 64`：分配 64 个虚拟 CPU 给虚拟机。
- `--disk path=/inf/yql/yql-openeuler.qcow2,size=200`：指定虚拟机的磁盘文件路径和大小为 200GB。
- `--location /inf/yql/openeuler-22.09-240618-x86_64-dvd.iso`：指定用于安装的 ISO 文件的位置。
- `--os-type generic`：指定操作系统类型为通用类型。
- `--network default`：指定虚拟机的网络接口为默认网络。
- `--graphics none`：禁用图形界面。
- `--console pty,target_type=serial`：设置虚拟机的控制台类型为串口控制台。
- `-x 'console=ttyS0,115200n8 console=tty0'`：启动参数，设置串口控制台和控制台输出。
- `--extra-args 'console=ttyS0,115200n8'`：额外的启动参数，设置串口控制台输出。

##### fedora

```bash
osinfo-query os | grep fedora
 fedora-unknown       | Fedora                                             | unknown  | http://fedoraproject.org/fedora/unknown
 fedora1              | Fedora Core 1                                      | 1        | http://fedoraproject.org/fedora/1
 fedora10             | Fedora 10                                          | 10       | http://fedoraproject.org/fedora/10
 fedora11             | Fedora 11                                          | 11       | http://fedoraproject.org/fedora/11
 fedora12             | Fedora 12                                          | 12       | http://fedoraproject.org/fedora/12
 fedora13             | Fedora 13                                          | 13       | http://fedoraproject.org/fedora/13
 fedora14             | Fedora 14                                          | 14       | http://fedoraproject.org/fedora/14
 fedora15             | Fedora 15                                          | 15       | http://fedoraproject.org/fedora/15
 fedora16             | Fedora 16                                          | 16       | http://fedoraproject.org/fedora/16
 fedora17             | Fedora 17                                          | 17       | http://fedoraproject.org/fedora/17
 fedora18             | Fedora 18                                          | 18       | http://fedoraproject.org/fedora/18
 fedora19             | Fedora 19                                          | 19       | http://fedoraproject.org/fedora/19
 fedora2              | Fedora Core 2                                      | 2        | http://fedoraproject.org/fedora/2
 fedora20             | Fedora 20                                          | 20       | http://fedoraproject.org/fedora/20
 fedora21             | Fedora 21                                          | 21       | http://fedoraproject.org/fedora/21
 fedora22             | Fedora 22                                          | 22       | http://fedoraproject.org/fedora/22
 fedora23             | Fedora 23                                          | 23       | http://fedoraproject.org/fedora/23
 fedora24             | Fedora 24                                          | 24       | http://fedoraproject.org/fedora/24
 fedora25             | Fedora 25                                          | 25       | http://fedoraproject.org/fedora/25
 fedora26             | Fedora 26                                          | 26       | http://fedoraproject.org/fedora/26
 fedora27             | Fedora 27                                          | 27       | http://fedoraproject.org/fedora/27
 fedora28             | Fedora 28                                          | 28       | http://fedoraproject.org/fedora/28
 fedora3              | Fedora Core 3                                      | 3        | http://fedoraproject.org/fedora/3
 fedora4              | Fedora Core 4                                      | 4        | http://fedoraproject.org/fedora/4
 fedora5              | Fedora Core 5                                      | 5        | http://fedoraproject.org/1mfedo/5
 fedora6              | Fedora Core 6                                      | 6        | http://fedoraproject.org/fedora/6
 fedora7              | Fedora 7                                           | 7        | http://fedoraproject.org/fedora/7
 fedora8              | Fedora 8                                           | 8        | http://fedoraproject.org/fedora/8
 fedora9              | Fedora 9                                           | 9        | http://fedoraproject.org/fedora/9
 silverblue28         | Fedora Silverblue 28                               | 28       | http://fedoraproject.org/silverblue/28
```

```bash
virt-install --virt-type kvm \
--name fedora \
--memory 32768 \
--vcpus=64 \
--location /data/yql/Fedora-Server-dvd-aarch64-40-1.14.iso \
--disk path=/data/yql/fedora.qcow2,size=300 \
--network network=default \
--os-type=linux \
--os-variant=fedora28 \
--graphics none \
--extra-args="console=ttyS0"
```

##### debian

```bash
virt-install --virt-type kvm \
--name debian \
--memory 32768 \
--vcpus=64 \
--location /data/yql/debian-12.5.0-arm64-DVD-1.iso \
--disk path=/data/yql/debian.qcow2,size=300 \
--network network=default \
--os-type=linux \
--os-variant=debian9 \
--graphics none \
--extra-args="console=ttyS0"
```

##### /dev/vda

qemu一般将`--disk path=/data/yql/fedora.qcow2,size=300`挂在到虚拟机的`/dev/vda`或`/dev/sda`。

```bash
lsblk
NAME             MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
sda                8:0    0   300G  0 disk
|-sda1             8:1    0   600M  0 part /boot/efi
|-sda2             8:2    0     1G  0 part /boot
|-sda3             8:3    0 298.4G  0 part
  |-ctyunos-root 252:0    0    70G  0 lvm  /
  |-ctyunos-home 252:1    0 228.4G  0 lvm  /home
```

```bash
lsblk
NAME             MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
vda                8:0    0   300G  0 disk
|-vda1             8:1    0   600M  0 part /boot/efi
|-vda2             8:2    0     1G  0 part /boot
|-vda3             8:3    0 298.4G  0 part
  |-ctyunos-root 252:0    0    70G  0 lvm  /
  |-ctyunos-home 252:1    0 228.4G  0 lvm  /home
```

这里假设`/dev/vda`，现在我们来创建上面lsblk展示的分区结构。

###### 1. **分区创建**

使用 `parted` 创建分区：

```bash
# 创建新的分区表
parted /dev/vda mklabel gpt

# 创建 vda1 分区，用于 /boot/efi
parted /dev/vda mkpart primary fat32 1MiB 601MiB

# 设置第一个分区为 EFI 系统分区
parted /dev/vda set 1 esp on

# 创建 vda2 分区，用于 /boot
parted /dev/vda mkpart primary xfs 601MiB 1601MiB

# 创建 vda3 分区，用于 LVM，使用剩余的空间
parted /dev/vda mkpart primary 1601MiB 100%
```

###### 2. **配置文件系统**

接下来，为 `vda1` 和 `vda2` 创建文件系统：

```bash
# 格式化 /boot/efi 为 vfat
mkfs.vfat /dev/vda1

# 格式化 /boot 为 XFS
mkfs.xfs /dev/vda2
```

###### 3. **设置 LVM**

创建物理卷、卷组和逻辑卷：

```bash
# 创建物理卷
pvcreate /dev/vda3

# 创建卷组 ctyunos
vgcreate ctyunos /dev/vda3

# 创建逻辑卷 ctyunos-root (70G) 和 ctyunos-home (剩余空间)
lvcreate -L 70G -n root ctyunos
lvcreate -l 100%FREE -n home ctyunos
```

在设置 LVM 时，创建的逻辑卷可以放置在 `/dev/mapper/` 下。这实际上是 LVM 的默认行为，LVM 逻辑卷会在 `/dev/mapper/` 目录下自动创建符号链接，指向 `/dev/<卷组名>/<逻辑卷名>`。

- 卷组名称为 `ctyunos`。
- 逻辑卷名称为 `root` 和 `home`。

会看到以下两个设备路径：

- `/dev/ctyunos/root` 和 `/dev/mapper/ctyunos-root`
- `/dev/ctyunos/home` 和 `/dev/mapper/ctyunos-home`

这两个路径实际上指向同一个逻辑卷设备。

###### 4. **格式化 LVM 逻辑卷**

为逻辑卷创建 XFS 文件系统：

```bash
# 格式化 root 逻辑卷
mkfs.xfs /dev/mapper/ctyunos-root

# 格式化 home 逻辑卷
mkfs.xfs /dev/mapper/ctyunos-home
```

###### 5. **挂载分区**

挂载分区和逻辑卷：

```bash
# 挂载 root
mount /dev/mapper/ctyunos-root /mnt

# 创建 /mnt/home 并挂载 home
mkdir /mnt/home
mount /dev/mapper/ctyunos-home /mnt/home

# 挂载 /boot
mkdir /mnt/boot
mount /dev/vda2 /mnt/boot

# 挂载 /boot/efi
mkdir /mnt/boot/efi
mount /dev/vda1 /mnt/boot/efi
```

###### 6. **更新 `/etc/fstab`**

在安装完成或手动配置时，将这些挂载点添加到 `/etc/fstab` 以便系统启动时自动挂载：

```bash
/dev/mapper/ctyunos-root   /           xfs    defaults        0 0
/dev/mapper/ctyunos-home   /home       xfs    defaults        0 0
/dev/vda2                  /boot       xfs    defaults        0 0
/dev/vda1                  /boot/efi   vfat   umask=0077      0 1
```

这些命令确保了 EFI 分区使用 `vfat` 文件系统，并且所有剩余空间都分配给 `vda3`，用于 LVM 管理。

这一章节只是为了展示qumu如何将`--disk path=/data/yql/fedora.qcow2,size=300`挂在到虚拟机的`/dev/vda`或`/dev/sda`，并在磁盘`/dev/sda`上创建了用`uefi`引导的完整文件系统，`/`、`/home`是 `lvm`逻辑卷。

##### kickstart

ks.cfs位于/root目录下。

```bash
/root/anaconda-ks.cfg
/root/initial-setup-ks.cfg
```

- [CentOS安装LVM方式](https://blog.csdn.net/weixin_34308389/article/details/90627069)
- [Linux系统安装：磁盘自定义分区](https://www.toutiao.com/article/7347161180114403849)

- [virt-install之kickstart方式安装](https://www.cnblogs.com/nihaoit/articles/15570640.html)
- [virt-install](https://www.jianshu.com/p/f586f4ce0722)
- [kickstart自动安装脚本](https://blog.csdn.net/xixlxl/article/details/79127131)
- [Linux高级篇--使用system-config-kickstart工具制作kickstart应答文件](https://blog.csdn.net/u013168176/article/details/82817300)
- [linux使用system-config-kickstart 自动安装系统](https://blog.csdn.net/shang_feng_wei/article/details/89218552)
- [使用Kickstart自动化部署Linux操作系统](https://xwanli0923.github.io/2023/02/13/Install-Linux-Using-Kickstart.html)
- [第 33 章 Kickstart Configurator](https://docs.redhat.com/zh_hans/documentation/red_hat_enterprise_linux/6/html/installation_guide/ch-redhat-config-kickstart#ch-redhat-config-kickstart)
- [Kickstart自动安装脚本](https://blog.csdn.net/qq_41582883/article/details/109401305)
- [kickstart+virt-install安装虚拟机](https://www.cnblogs.com/powerrailgun/p/12158037.html)
- [fedora Kickstart 语法参考](https://docs.fedoraproject.org/zh_CN/fedora/f26/install-guide/appendixes/Kickstart_Syntax_Reference/)
- [<font color=Red>centos7 27.2. 如何执行 Kickstart 安装？</font>](https://docs.redhat.com/zh_hans/documentation/red_hat_enterprise_linux/7/html/installation_guide/sect-kickstart-howto)
- [<font color=Red>27.3.3. 预安装脚本</font>](https://docs.redhat.com/zh_hans/documentation/red_hat_enterprise_linux/7/html/installation_guide/sect-kickstart-preinstall)
- [openeurler 使用kickstart自动化安装](https://docs.openeuler.org/zh/docs/20.03_LTS/docs/Installation/%E4%BD%BF%E7%94%A8kickstart%E8%87%AA%E5%8A%A8%E5%8C%96%E5%AE%89%E8%A3%85.html)
- [<font color=Red>kickstart安装</font>](https://www.cnblogs.com/zhenhui/p/6233285.html)
- [银河麒麟服务器操作系统V10SP1基于Kickstart无人值守安装](https://blog.csdn.net/weixin_54752007/article/details/126037635)

```bash
virt-install --virt-type kvm \
--initrd-inject=/mnt/sdd1/yql/ks1.cfg \
--name wujing-zy-xfs \
--memory 32768 \
--vcpus=64 \
--location /mnt/sdd1/yql/ctyunos-2-24.07-240716-rc2-aarch64-dvd.iso \
--disk path=/mnt/sdd1/yql/wujing-zy-xfs.qcow2,size=128 \
--network network=default \
--os-type=linux \
--os-variant=centos7.0 \
--graphics none \
--extra-args="console=ttyS0 inst.ks=file:/ks1.cfg"
```

#### 克隆一个虚拟机

```bash
virt-clone --original yql --name wujing --file wujing.qcow2
```

#### 启动虚拟机

```bash
virsh start wujing
```

#### 将虚拟机vcpu扩充到16核心

```bash
virsh shutdown wujing
virsh edit wujing   # 修改vcpu字段
virsh start wujing
```

重启虚拟机后验证是否16核心：

```bash
nproc
```

#### 删除虚拟机

- [KVM 报错以及处理](https://www.cnblogs.com/loudyten/articles/10233268.html)

使用 `virsh` 命令中取消定义一个带有 NVRAM 的虚拟机:

```bash
virsh undefine wujing --nvram
```

参数说明:

- `undefine` 表示取消定义（即删除）一个虚拟机。
- `--nvram` 选项用于指示虚拟机具有 NVRAM（非易失性随机存储器），需要一并删除。
- `wujing` 是要删除的虚拟机的名称或定义文件的路径。

### vcpupin

- [KVM虚拟机cpu资源限制和vcpu亲缘性绑定](https://www.cnblogs.com/anay/p/11121708.html)

### 共享剪切板

- [qemu/kvm linux 虚拟机配置（共享剪切版，文件拖拽进虚拟机）](https://blog.csdn.net/qq_33831360/article/details/123700719)
- [How can I copy&paste from the host to a KVM guest?](https://askubuntu.com/questions/858649/how-can-i-copypaste-from-the-host-to-a-kvm-guest)

  在虚拟机上安装spice-vdagent即可：

  ```bash
  sudo apt install spice-vdagent
  ```

#### win11共享剪切板

- [kvm安装windows11](https://www.cnblogs.com/studywithallofyou/p/17788892.html)
  - [virtio-win-guest-tools.exe](https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/archive-virtio/virtio-win-0.1.240-1/)

### 共享目录

#### 9p

在宿主机查看虚拟机共享目录配置:

![共享目录](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240202130011.png)

在虚拟机上执行下方命令：

```bash
sudo mount -t 9p -o trans=virtio /media/wujing/data /media/wujing/data
```

如果在虚拟机上想启动就默认挂载，可以写入 /etc/fstab：

```bash
sudo cat << EOF >> /etc/fstab
/media/wujing/data /media/wujing/data 9p trans=virtio 0 0
EOF
```

在虚拟机上重新挂载/etc/fstab：

```bash
sudo systemctl daemon-reload
sudo mount -a
```

- [virt-manager设置主机和虚拟机之间文件共享](https://blog.csdn.net/sinat_38816924/article/details/120284285)

#### virtio-fs

![共享目录](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20250106194859.png)

在虚拟机上执行下方命令：

```bash
sudo mount -t virtiofs /home/wujing/code /home/wujing/code
sudo mount -t virtiofs /home/wujing/Downloads /home/wujing/Downloads
```

如果在虚拟机上想启动就默认挂载，可以写入 /etc/fstab：

```bash
sudo cat << EOF >> /etc/fstab
/home/wujing/code    /home/wujing/code    virtiofs    defaults    0    0
/home/wujing/Downloads /home/wujing/Downloads virtiofs defaults    0    0
EOF
```

在虚拟机上重新挂载/etc/fstab：

```bash
sudo systemctl daemon-reload
sudo mount -a
```

- [virtiofs: virtio-fs 主机<->客机共享文件系统](https://www.kernel.org/doc/html/latest/translations/zh_CN/filesystems/virtiofs.html)

### 时间

- [解决deepin虚拟机系统时间不正确的问题](https://blog.csdn.net/fcdm_/article/details/122150246)

    ```bash
    sudo apt install systemd-timesyncd
    ```

## 网络虚拟化

- [<font color=Red>QEMU用户模式网络</font>](https://blog.csdn.net/m0_43406494/article/details/124827927)
- [<font color=Red>QEMU 网络配置</font>](https://tomwei7.com/2021/10/09/qemu-network-config/)
- [<font color=Red>Linux 内核调试 七：qemu网络配置</font>](https://blog.csdn.net/OnlyLove_/article/details/124536607)
- [<font color=Red>详解QEMU网络配置的方法</font>](https://www.jb51.net/article/97216.htm)
- [详解：VirtIO Networking 虚拟网络设备实现架构](https://mp.weixin.qq.com/s/aFOcuyypU1KdcdOecs8DdQ)
- [虚拟化技术 — Libvirt 异构虚拟化管理组件](https://mp.weixin.qq.com/s/oEDuaFs7DQM3zMCIx_CiFA)
- [<font color=Red>Ubuntu 20.04 物理机 QEMU-KVM + Virt-Manager 创建桥接模式的虚拟机</font>](https://www.cnblogs.com/whjblog/p/17213359.html)
- [[Debian10]使用KVM虚拟机并配置桥接网络](https://www.cnblogs.com/DouglasLuo/p/12731591.html)

### 动态添加网络接口

在虚拟化环境中，`virsh` 是一个常用的命令行工具，用于管理虚拟机。通过 `virsh attach-device` 命令，可以在虚拟机运行时动态添加设备，如网络接口。以下是一个简单的示例，展示如何通过 `nic.xml` 配置文件为虚拟机添加一个网络接口。

1. **查看网络接口配置文件**：
   使用 `cat` 命令查看 `nic.xml` 文件内容，该文件定义了一个类型为 `network` 的虚拟网络接口，源网络为 `default`，模型类型为 `virtio`。

   ```bash
   cat nic.xml
   <interface type='network'>
       <source network='default'/>
       <model type='virtio'/>
   </interface>
   ```

2. **动态添加网络接口**：
   使用 `virsh attach-device` 命令将 `nic.xml` 中定义的网络接口动态添加到指定的虚拟机中。`vm-uuid` 是目标虚拟机的唯一标识符，`--live` 选项表示在虚拟机运行时进行添加。

   ```bash
   virsh attach-device vm-uuid nic.xml --live
   ```

通过这种方式，可以在不重启虚拟机的情况下，动态调整虚拟机的网络配置，提高系统的灵活性和可用性。

### 网络虚拟化类型对比

---

| **属性**                       | **全虚拟化 (纯软件模拟)**          | **半虚拟化 (vhost-net)**           | **半虚拟化 (vhost-user)**          | **半虚拟化 (vDPA)**                | **VFIO**                          |
|-------------------------------|------------------------------------|------------------------------------|------------------------------------|------------------------------------|------------------------------------|
| **实现方式**                  | 纯软件模拟                        | vhost-net                         | vhost-user                        | vDPA                              | VFIO                              |
| **前端驱动名称**              | `virtio-net`                      | `virtio-net`                      | `virtio-net`                      | `virtio-net`                      | 硬件设备原生驱动（如 `ixgbe`、`mlx5_core`） |
| **后端驱动名称**              | QEMU 用户空间模拟                  | `vhost-net`（内核驱动）            | vhost-user（用户空间，通常由 OVS + DPDK 管理） | `vdpa-net`（网络设备）<br>`vdpa-blk`（块设备） | `vfio-pci`（PCI 设备）<br>`vfio-platform`（平台设备） |
| **前端驱动位置**              | 虚拟机内部                        | 虚拟机内部                        | 虚拟机内部                        | 虚拟机内部                        | 虚拟机内部                        |
| **后端驱动位置**              | 物理机用户空间                    | 物理机内核空间                    | 物理机用户空间                    | 物理机硬件设备                    | 物理机硬件设备                    |
| **数据包是否经过 QEMU 转发**   | 是                               | 否                                | 否                                | 否                                | 否                                |
| **控制信息**                  | virtio 配置、队列状态、事件通知    | virtio 配置、队列状态、事件通知    | virtio 配置、队列状态、事件通知    | virtio 配置、队列状态、事件通知    | 硬件设备配置、队列状态、事件通知   |
| **控制信息是否经过 QEMU 转发** | 是                               | 是                                | 否                                | 否                                | 否                                |
| **数据包传递机制**            | 中断（**中断模拟**）              | **eventfd**（接收方向：后端触发通知）<br>**ioeventfd**（发送方向：前端触发通知） | **eventfd**（接收方向：后端触发通知）<br>**ioeventfd**（发送方向：前端触发通知） | 硬件加速（绕过软件栈，**中断透传**） | 硬件中断（直接访问硬件，**中断透传**） |
| **中断透传是否用到**          | 否                               | 否                                | 否                                | 是（硬件中断直通）                | 是（硬件中断直通）                |
| **中断虚拟化方式**            | **中断模拟**：QEMU 模拟中断注入   | **中断注入**：**eventfd + IRQFD**（接收）<br>**ioeventfd**（发送） | **中断注入**：**eventfd + IRQFD**（接收）<br>**ioeventfd**（发送） | **中断透传**：硬件直接触发中断    | **中断透传**：硬件直接触发中断    |
| **中断透传的实现方式**        | 中断由 QEMU 模拟，虚拟机通过虚拟中断机制接收中断信号 | **eventfd + IRQFD** 替代中断，虚拟机通过事件驱动接收通知 | **eventfd + IRQFD** 替代中断，虚拟机通过事件驱动接收通知 | 硬件设备的中断直接传递给虚拟机，绕过宿主机的软件栈 | 硬件设备的中断直接传递给虚拟机，绕过宿主机的软件栈 |
| **是否使用 eventfd**          | 否                               | 是（接收方向：后端写入 **eventfd** 通知前端） | 是（接收方向：后端写入 **eventfd** 通知前端） | 否                                | 否                                |
| **是否使用 ioeventfd**        | 否                               | 是（发送方向：前端 kick 触发 **ioeventfd** 通知后端） | 是（发送方向：前端 kick 触发 **ioeventfd** 通知后端） | 否                                | 否                                |
| **是否使用 IRQFD**            | 否                               | 是（接收方向：KVM 将 **eventfd** 转化为虚拟中断） | 是（接收方向：QEMU 通过 **IRQFD** 通知 KVM 注入中断） | 否                                | 否                                |
| **是否使用 vring**            | 是                               | 是                                | 是                                | 是                                | 否                                |
| **数据收发包机制**            | **中断模拟**                     | **eventfd + IRQFD**（接收）<br>**ioeventfd**（发送） | **eventfd + IRQFD**（接收）<br>**ioeventfd**（发送） | **中断透传**                     | **中断透传**                     |
| **数据收包链路**              | 物理网络 → 宿主机内核网络栈 → QEMU → vring → 虚拟机前端驱动 → Guest OS（**中断模拟**） | 物理网络 → 宿主机内核网络栈 → vhost-net → **eventfd** → KVM (**IRQFD**) → vring → 虚拟机前端驱动 → Guest OS（**中断注入**） | 物理网络 → OVS + DPDK → vhost-user → **eventfd** → QEMU → KVM (**IRQFD**) → vring → 虚拟机前端驱动 → Guest OS（**中断注入**） | 物理网络 → 硬件设备 → vring → 虚拟机前端驱动 → Guest OS（**中断透传**） | 物理网络 → 硬件设备 → 虚拟机原生驱动 → Guest OS（**中断透传**） |
| **数据发包链路**              | Guest OS → 虚拟机前端驱动 → vring → QEMU → 宿主机内核网络栈 → 物理网络（**中断模拟**） | Guest OS → 虚拟机前端驱动 → vring → KVM (**ioeventfd**) → vhost-net → 宿主机内核网络栈 → 物理网络（**中断注入**） | Guest OS → 虚拟机前端驱动 → vring → KVM (**ioeventfd**) → vhost-user → OVS + DPDK → 物理网络（**中断注入**） | Guest OS → 虚拟机前端驱动 → vring → 硬件设备 → 物理网络（**中断透传**） | Guest OS → 虚拟机原生驱动 → 硬件设备 → 物理网络（**中断透传**） |
| **是否使用 SR-IOV**           | 否                               | 否                                | 否                                | 是                                | 是                                |
| **是否使用 IOMMU**            | 否                               | 是                                | 是                                | 是                                | 是                                |
| **是否支持 APIC 虚拟化**      | 是（若硬件支持，如 APICv/AVIC，可减少 VM Exit） | 是（若硬件支持，可优化 **IRQFD** 注入效率） | 是（若硬件支持，可优化 **IRQFD** 注入效率） | 是（若硬件支持，可优化中断透传）  | 是（若硬件支持，可优化中断处理）  |

---

#### 1. **全虚拟化 (纯软件模拟)**
- **前端驱动**：`virtio-net`（虚拟机内部）
- **后端驱动**：QEMU 用户空间模拟（物理机用户空间）
- **数据包是否经过 QEMU 转发**：是
- **数据收发包机制**：中断

##### **接收数据包（物理网络 → Guest OS）**
- **链路**：物理网络 → 宿主机内核网络栈 → QEMU → vring → 虚拟机前端驱动 → Guest OS
- **流程**：
  1. 数据包从物理网络到达宿主机，通过内核网络栈（如 TAP 设备）传递到 QEMU。
  2. QEMU 接收数据包并将其写入 **vring** 的可用环。
  3. QEMU 模拟一个虚拟中断（通过 KVM 的虚拟中断机制，例如虚拟 APIC）。
  4. KVM 将虚拟中断注入虚拟机，触发 `virtio-net` 前端驱动的中断处理程序。
  5. 前端驱动从 **vring** 中读取数据包，传递给 Guest OS 的网络协议栈。
- **特点**：
  - 数据包和通知都经过 QEMU，性能较低，因涉及用户态和内核态切换。
  - 不使用 **eventfd**、**ioeventfd** 或 **IRQFD**，完全依赖 QEMU 模拟中断。

##### **发送数据包（Guest OS → 物理网络）**
- **链路**：Guest OS → 虚拟机前端驱动 → vring → QEMU → 宿主机内核网络栈 → 物理网络
- **流程**：
  1. Guest OS 的网络协议栈生成数据包，传递给 `virtio-net` 前端驱动。
  2. 前端驱动将数据包写入 **vring** 的可用环。
  3. 前端通过写入 virtio 通知寄存器（kick 操作）通知 QEMU。
  4. KVM 捕获通知（VM Exit），将控制权交给 QEMU。
  5. QEMU 从 **vring** 中读取数据包，通过宿主机内核网络栈（如 TAP 设备）发送到物理网络。
- **特点**：
  - 数据包和通知都经过 QEMU，QEMU 模拟中断处理，效率较低。

---

#### 2. **半虚拟化 (vhost-net)**
- **前端驱动**：`virtio-net`（虚拟机内部）
- **后端驱动**：`vhost-net`（物理机内核空间）
- **数据包是否经过 QEMU 转发**：否
- **数据收发包机制**：**eventfd + IRQFD**（接收）、**ioeventfd**（发送）

##### **接收数据包（物理网络 → Guest OS）**
- **链路**：物理网络 → 宿主机内核网络栈 → vhost-net → **eventfd** → KVM (**IRQFD**) → vring → 虚拟机前端驱动 → Guest OS
- **流程**：
  1. 数据包从物理网络到达宿主机，通过内核网络栈（如 TAP 设备）传递到 `vhost-net` 内核模块。
  2. `vhost-net` 将数据包写入 **vring** 的可用环。
  3. `vhost-net` 通过 `write()` 将非零值写入 **eventfd**，触发通知。
  4. KVM 通过预先注册的 **IRQFD** 检测到 **eventfd** 变化，将其转化为虚拟中断（例如 MSI）。
  5. KVM 注入虚拟中断到虚拟机，触发 `virtio-net` 前端驱动。
  6. 前端驱动从 **vring** 中读取数据包，传递给 Guest OS。
- **特点**：
  - 数据包绕过 QEMU，由 `vhost-net` 在内核态直接处理。
  - **eventfd + IRQFD** 替代传统中断，提高通知效率。

##### **发送数据包（Guest OS → 物理网络）**
- **链路**：Guest OS → 虚拟机前端驱动 → vring → KVM (**ioeventfd**) → vhost-net → 宿主机内核网络栈 → 物理网络
- **流程**：
  1. Guest OS 的网络协议栈生成数据包，传递给 `virtio-net` 前端驱动。
  2. 前端驱动将数据包写入 **vring** 的可用环。
  3. 前端通过写入 virtio 通知寄存器（kick 操作）触发通知。
  4. KVM 捕获通知（VM Exit），通过预先注册的 **ioeventfd** 将事件转化为 **eventfd** 的计数器增加。
  5. `vhost-net` 在内核态监控 **eventfd**（如通过 `wait_queue`），检测到变化后从 **vring** 读取数据包。
  6. `vhost-net` 将数据包通过宿主机内核网络栈发送到物理网络。
- **特点**：
  - 数据包绕过 QEMU，`vhost-net` 直接处理。
  - **ioeventfd** 将前端通知高效传递给后端。

---

#### 3. **半虚拟化 (vhost-user)**
- **前端驱动**：`virtio-net`（虚拟机内部）
- **后端驱动**：vhost-user（物理机用户空间，通常由 OVS + DPDK 管理）
- **数据包是否经过 QEMU 转发**：否
- **数据收发包机制**：**eventfd + IRQFD**（接收）、**ioeventfd**（发送）

##### **接收数据包（物理网络 → Guest OS）**
- **链路**：物理网络 → OVS + DPDK → vhost-user → **eventfd** → QEMU → KVM (**IRQFD**) → vring → 虚拟机前端驱动 → Guest OS
- **流程**：
  1. 数据包从物理网络到达 OVS，通过 DPDK 零拷贝技术转发到 vhost-user 后端。
  2. vhost-user 将数据包写入 **vring** 的可用环。
  3. vhost-user 通过 `write()` 将非零值写入 **eventfd**（由 QEMU 初始化并传递）。
  4. QEMU 监控 **eventfd**（通过 `poll()` 或 `epoll()`），检测到变化后通过 **IRQFD** 调用 KVM。
  5. KVM 将 **IRQFD** 事件转化为虚拟中断，注入虚拟机。
  6. `virtio-net` 前端驱动接收中断，从 **vring** 中读取数据包，传递给 Guest OS。
- **特点**：
  - 数据包绕过 QEMU，由 vhost-user 在用户态处理。
  - **eventfd + IRQFD** 需要 QEMU 协调，将通知传递给 KVM。

##### **发送数据包（Guest OS → 物理网络）**
- **链路**：Guest OS → 虚拟机前端驱动 → vring → KVM (**ioeventfd**) → vhost-user → OVS + DPDK → 物理网络
- **流程**：
  1. Guest OS 的网络协议栈生成数据包，传递给 `virtio-net` 前端驱动。
  2. 前端驱动将数据包写入 **vring** 的可用环。
  3. 前端通过写入 virtio 通知寄存器（kick 操作）触发通知。
  4. KVM 捕获通知，通过 **ioeventfd** 将事件转化为 **eventfd** 的计数器增加。
  5. vhost-user 后端（OVS + DPDK）监控 **eventfd**（通过 `poll()` 或 `epoll()`），检测到变化后从 **vring** 读取数据包。
  6. vhost-user 将数据包传递给 OVS，通过 DPDK 发送到物理网络。
- **特点**：
  - 数据包绕过 QEMU，由 vhost-user 直接处理。
  - **ioeventfd** 将前端通知高效传递给用户态后端。

---

#### 4. **半虚拟化 (vDPA)**
- **前端驱动**：`virtio-net`（虚拟机内部）
- **后端驱动**：`vdpa-net`（物理机硬件设备）
- **数据包是否经过 QEMU 转发**：否
- **数据收发包机制**：硬件中断

##### **接收数据包（物理网络 → Guest OS）**
- **链路**：物理网络 → 硬件设备 → vring → 虚拟机前端驱动 → Guest OS
- **流程**：
  1. 数据包从物理网络到达硬件设备（支持 vDPA 的网卡）。
  2. 硬件设备直接将数据包写入 **vring**（通过硬件加速和 IOMMU 映射）。
  3. 硬件设备触发硬件中断，直接传递给虚拟机（通过中断透传，如 SR-IOV）。
  4. `virtio-net` 前端驱动接收中断，从 **vring** 中读取数据包，传递给 Guest OS。
- **特点**：
  - 数据包完全绕过软件栈，由硬件直接处理。
  - 不使用 **eventfd**、**ioeventfd** 或 **IRQFD**，依赖硬件中断。

##### **发送数据包（Guest OS → 物理网络）**
- **链路**：Guest OS → 虚拟机前端驱动 → vring → 硬件设备 → 物理网络
- **流程**：
  1. Guest OS 的网络协议栈生成数据包，传递给 `virtio-net` 前端驱动。
  2. 前端驱动将数据包写入 **vring**。
  3. 前端通过硬件通知机制（如 doorbell）通知硬件设备。
  4. 硬件设备直接从 **vring** 读取数据包，通过物理网络发送。
- **特点**：
  - 数据包由硬件处理，绕过 QEMU 和 KVM。
  - 使用硬件中断透传，无需软件通知机制。

---

#### 5. **VFIO**
- **前端驱动**：硬件设备原生驱动（如 `ixgbe`、`mlx5_core`）（虚拟机内部）
- **后端驱动**：`vfio-pci`（物理机硬件设备）
- **数据包是否经过 QEMU 转发**：否
- **数据收发包机制**：硬件中断

##### **接收数据包（物理网络 → Guest OS）**
- **链路**：物理网络 → 硬件设备 → 虚拟机原生驱动 → Guest OS
- **流程**：
  1. 数据包从物理网络到达硬件设备（如网卡的 VF）。
  2. 硬件设备通过 DMA 将数据包写入虚拟机的内存（通过 IOMMU 映射）。
  3. 硬件设备触发硬件中断，直接传递给虚拟机（通过 VFIO 和 SR-IOV）。
  4. 虚拟机中的原生驱动（如 `ixgbe`）接收中断，处理数据包并传递给 Guest OS。
- **特点**：
  - 数据包直接由硬件处理，不经过 QEMU 或 **vring**。
  - 不使用 **eventfd**、**ioeventfd** 或 **IRQFD**。

##### **发送数据包（Guest OS → 物理网络）**
- **链路**：Guest OS → 虚拟机原生驱动 → 硬件设备 → 物理网络
- **流程**：
  1. Guest OS 的网络协议栈生成数据包，传递给原生驱动。
  2. 原生驱动通过 DMA 将数据包写入硬件设备的缓冲区。
  3. 原生驱动触发硬件通知（如 doorbell），通知硬件发送数据。
  4. 硬件设备将数据包发送到物理网络。
- **特点**：
  - 数据包直接由硬件处理，无 **vring** 或软件通知机制。

---

#### 总结对比
- **全虚拟化**：收发包依赖 QEMU 模拟中断，性能最低。
- **vhost-net**：收包用 **eventfd + IRQFD**，发包用 **ioeventfd**，内核态优化。
- **vhost-user**：收包用 **eventfd + IRQFD**（需 QEMU 协调），发包用 **ioeventfd**，用户态高性能。
- **vDPA**：收发包用硬件中断，依赖硬件加速。
- **VFIO**：收发包用硬件中断，直接透传，无 virtio 机制。

### eventfd、模拟中断对比

**eventfd** 是一种高效的进程间通信机制，用于在 Linux 内核和用户空间之间传递事件通知。在虚拟化环境中，**eventfd** 被广泛用于替代传统的中断机制，以减少中断开销并提升性能。以下是 **eventfd** 如何替代中断的详细说明：

---

#### 1. **传统中断机制的局限性**
在传统的 virtio 实现中：
- **中断机制**：
  - 当虚拟机前端有数据包需要处理时，会触发一个中断通知后端驱动。
  - 后端驱动通过中断处理程序响应通知，并从 virtio 环（vring）中读取数据包。
- **问题**：
  - **高开销**：中断机制涉及上下文切换和内核栈处理，开销较大。
  - **低效**：频繁的中断会导致 CPU 利用率下降，影响性能。

---

#### 2. **eventfd 的工作原理**
**eventfd** 是一种基于文件描述符的事件通知机制，允许内核和用户空间之间高效地传递事件。它的核心思想是通过内存映射和事件通知替代传统的中断机制。

##### **eventfd 的关键特性**
- **文件描述符**：
  - eventfd 创建一个文件描述符，用于在内核和用户空间之间传递事件。
- **计数器**：
  - eventfd 内部维护一个计数器，用于记录事件的数量。
- **通知机制**：
  - 当事件发生时，计数器递增，并通过文件描述符通知接收方。
- **高效性**：
  - eventfd 通过内存映射和事件驱动机制实现高效的事件通知，避免了传统中断的开销。

---

#### 3. **eventfd 在虚拟化中的应用**
在虚拟化环境中，**eventfd** 被用于替代传统的中断机制，具体流程如下：

##### **数据包接收流程**
1. **虚拟机前端**：
   - 虚拟机前端将数据包放入 vring 中。
   - 虚拟机前端通过写入 eventfd 文件描述符通知后端驱动有新数据包。
2. **后端驱动**：
   - 后端驱动通过读取 eventfd 文件描述符接收通知。
   - 后端驱动从 vring 中读取数据包并进行处理。

##### **数据包发送流程**
1. **后端驱动**：
   - 后端驱动将数据包放入 vring 中。
   - 后端驱动通过写入 eventfd 文件描述符通知虚拟机前端有新数据包。
2. **虚拟机前端**：
   - 虚拟机前端通过读取 eventfd 文件描述符接收通知。
   - 虚拟机前端从 vring 中读取数据包并传递给 Guest OS。

---

#### 4. **eventfd 替代中断的优势**
- **减少中断开销**：
  - eventfd 通过内存映射和事件驱动机制替代传统的中断机制，减少了上下文切换和内核栈处理的开销。
- **高效事件通知**：
  - eventfd 的计数器机制可以批量处理事件，减少了事件通知的频率。
- **低延迟**：
  - eventfd 的事件通知机制延迟极低，适合高性能场景。

---

#### 5. **eventfd 与 ioeventfd 的关系**
- **eventfd**：
  - 用于通用的进程间事件通知。
- **ioeventfd**：
  - 是 eventfd 的一个变种，专门用于虚拟化环境中的高效事件通知。
  - ioeventfd 通过内存映射触发事件，当虚拟机访问特定的 I/O 地址时，会触发 ioeventfd 事件。

##### **ioeventfd 的工作流程**
1. **注册 ioeventfd**：
   - 宿主机为虚拟机注册一个 ioeventfd，并将其映射到特定的 I/O 地址。
2. **触发事件**：
   - 虚拟机访问该 I/O 地址时，会触发 ioeventfd 事件。
3. **事件通知**：
   - ioeventfd 事件通过 eventfd 文件描述符通知宿主机。
4. **处理事件**：
   - 宿主机从 vring 中读取数据包并进行处理。

---

#### 6. **总结**
- **eventfd** 通过内存映射和事件驱动机制替代传统的中断机制，减少了中断开销并提升了性能。
- 在虚拟化环境中，**eventfd** 和 **ioeventfd** 被广泛用于高效的事件通知，适合高性能场景（如 NFV、云计算）。
- **eventfd** 的优势包括减少中断开销、高效事件通知和低延迟。

### ioeventfd、irqfd到底是优化的模拟中断这一部分还是别的？

您的问题非常精准，直指 **ioeventfd** 和 **irqfd** 在虚拟化中断处理中的优化作用。简单回答是：**ioeventfd** 和 **irqfd** 确实是对传统**模拟中断**（Interrupt Emulation）的优化，但它们优化的具体部分不同，且不仅仅局限于模拟中断本身，而是改善了整个中断相关的通信流程。下面我将详细分析这两者如何优化模拟中断，以及它们的作用范围。

---

#### **背景：传统模拟中断的低效之处**
在传统全虚拟化中（如 QEMU 纯软件模拟），模拟中断的流程如下：
1. **Guest → Host 通知**：
   - 虚拟机（Guest）发起 I/O 请求（如写 PIO/MMIO 触发通知寄存器）。
   - 触发 VM Exit，KVM 捕获并交给 QEMU。
   - QEMU 处理 I/O 请求（分发到设备模拟逻辑）。
2. **Host → Guest 通知**：
   - QEMU 完成 I/O 请求后，模拟中断。
   - QEMU 调用 KVM 接口（如 `KVM_INTERRUPT`），注入虚拟中断。
   - KVM 通过虚拟 APIC 将中断注入虚拟机，触发 Guest 的中断处理程序。
3. **低效点**：
   - **Guest → Host**：每次 I/O 请求导致 VM Exit 和 QEMU 的分发，开销大。
   - **Host → Guest**：QEMU 模拟中断需用户态到内核态切换，注入过程复杂。

**ioeventfd** 和 **irqfd** 分别针对这两个方向的低效问题进行了优化，但它们的优化目标和作用范围有所不同。

---

#### **1. ioeventfd：优化 Guest → Host 通知**
##### **作用**
- **ioeventfd** 优化的是 **Guest 向 Host 发送通知** 的过程，即虚拟机触发 I/O 操作时通知宿主机的部分。
- 它并不直接优化“模拟中断”的注入，而是减少了传统模拟中断流程中 **VM Exit 后 QEMU 分发的开销**。

##### **原理**
- **初始化**：
  - QEMU 通过 `ioctl(KVM_IOEVENTFD)` 将虚拟机的特定 I/O 地址（如 virtio 通知寄存器）与一个 **eventfd** 文件描述符绑定。
- **工作流程**：
  1. Guest 写入 I/O 地址（例如 virtio 的 kick 操作）。
  2. KVM 捕获 VM Exit，但不将控制权交给 QEMU，而是直接触发绑定的 **eventfd**（增加计数器）。
  3. 后端（如 vhost-net、vhost-user）监控 **eventfd**（通过 `poll()` 或 `epoll()`），直接处理 I/O 请求。
- **优化点**：
  - 绕过了 QEMU 的分发逻辑，KVM 在内核态直接完成通知。
  - 减少了从 KVM 到 QEMU 的用户态切换。

##### **与模拟中断的关系**
- **不是直接优化模拟中断**：
  - **ioeventfd** 不涉及 Host → Guest 的中断注入，而是优化了 Guest → Host 的通知路径。
  - 在传统模拟中断中，这部分是 VM Exit 后 QEMU 的分发逻辑（例如解析 I/O 请求并调用设备模拟函数）。
- **间接提升效率**：
  - 通过减少 VM Exit 的后续处理开销，间接为整个 I/O 流程（包括后续的中断注入）腾出性能空间。

##### **适用场景**
- 半虚拟化（如 vhost-net、vhost-user）的发送方向（Guest → Host）。
- 示例：Guest OS 发送数据包时，`virtio-net` 前端通过 **ioeventfd** 通知后端。

---

#### **2. irqfd：优化 Host → Guest 中断注入**
##### **作用**
- **irqfd** 直接优化的是 **Host 向 Guest 注入中断** 的过程，即传统模拟中断中 QEMU 模拟并注入中断的部分。
- 它是模拟中断的具体优化，减少了用户态处理和注入的开销。

##### **原理**
- **初始化**：
  - QEMU 或后端通过 `ioctl(KVM_IRQFD)` 将一个 **eventfd** 与虚拟中断（如 MSI 或虚拟 IRQ）绑定。
- **工作流程**：
  1. 后端（如 vhost-net、vhost-user）完成 I/O 请求，写入 **eventfd**（增加计数器）。
  2. KVM 检测到 **eventfd** 变化，通过 **irqfd** 直接将绑定的虚拟中断注入虚拟机。
  3. Guest 的中断处理程序被触发，处理事件。
- **优化点**：
  - 绕过了 QEMU 的用户态模拟和手动调用 KVM 注入中断的步骤。
  - KVM 在内核态直接完成中断注入，减少用户态到内核态的切换。

##### **与模拟中断的关系**
- **直接优化模拟中断**：
  - 在传统模拟中断中，QEMU 需要模拟中断信号并通过 KVM 接口注入，涉及多次切换。
  - **irqfd** 将这一过程简化为：后端触发 **eventfd** → KVM 自动注入中断，效率更高。
- **本质仍是模拟中断**：
  - **irqfd** 注入的仍然是虚拟中断（模拟硬件中断的行为），但实现方式从 QEMU 的软件模拟变为 KVM 的内核态事件驱动。

##### **适用场景**
- 半虚拟化（如 vhost-net、vhost-user）的接收方向（Host → Guest）。
- 示例：vhost-user 接收数据包后，通过 **irqfd** 通知 `virtio-net` 前端。

---

#### **3. 对比总结：优化的是哪部分？**
| **机制**       | **优化目标**                          | **优化的模拟中断部分**                  | **作用范围**                          |
|----------------|---------------------------------------|-----------------------------------------|---------------------------------------|
| **ioeventfd** | Guest → Host 通知（I/O 请求触发）     | VM Exit 后 QEMU 分发的低效流程          | 通知后端处理 I/O，不涉及中断注入      |
| **irqfd**     | Host → Guest 中断注入                | QEMU 模拟并注入中断的低效流程           | 直接注入虚拟中断，优化通知 Guest      |

- **ioeventfd**：
  - 优化的不是“模拟中断”的注入，而是模拟中断流程中的**前置通知阶段**（Guest → Host）。
  - 它解决的是传统流程中 VM Exit 后 QEMU 分发的低效问题。
- **irqfd**：
  - 直接优化的是“模拟中断”的**注入阶段**（Host → Guest）。
  - 它将 QEMU 的模拟中断逻辑替换为 KVM 的高效注入机制。

---

#### **4. 在表格中的体现**
结合之前的表格：
- **全虚拟化 (纯软件模拟)**：
  - 无 **ioeventfd** 或 **irqfd**，完全依赖传统模拟中断。
  - 收包：QEMU 模拟中断注入。
  - 发包：VM Exit 后 QEMU 分发。
- **vhost-net/vhost-user**：
  - **ioeventfd**：发送方向，优化 Guest → Host 通知（前端 kick 触发后端）。
  - **irqfd**：接收方向，优化 Host → Guest 中断注入（后端通知前端）。
- **vDPA/VFIO**：
  - 无 **ioeventfd** 或 **irqfd**，依赖中断透传，不涉及模拟中断。

##### **示例流程**
- **vhost-net 收包**：
  - vhost-net → **eventfd** → KVM (**irqfd**) → 注入中断（优化后的模拟中断）。
- **vhost-net 发包**：
  - 前端 kick → KVM (**ioeventfd**) → vhost-net（优化通知，不涉及中断注入）。

---

#### **5. 更广义的优化作用**
- **ioeventfd 和 irqfd 的整体目标**：
  - 不仅仅优化模拟中断，而是提升整个虚拟化 I/O 流程的效率。
  - **ioeventfd** 优化了 Guest 发起请求的路径。
  - **irqfd** 优化了 Host 通知 Guest 的路径。
- **与模拟中断的关系**：
  - 传统模拟中断是全流程（Guest → Host 和 Host → Guest）的低效实现。
  - **ioeventfd** 和 **irqfd** 分担了这一流程的两端，分别优化通知和注入。

---

#### **6. 结论**
- **ioeventfd**：优化的不是模拟中断的注入，而是 **Guest → Host 通知**（传统模拟中断的前置部分），减少 QEMU 分发开销。
- **irqfd**：直接优化 **Host → Guest 的模拟中断注入**，将 QEMU 的软件模拟替换为 KVM 的事件驱动注入。

两者共同协作，针对传统模拟中断的两个低效环节（通知和注入）分别优化，使半虚拟化（如 vhost-net、vhost-user）的性能显著提升。

### SR-IOV、IOMMU对比

**SR-IOV（Single Root I/O Virtualization）** 和 **IOMMU（Input-Output Memory Management Unit）** 是两种不同的技术，尽管它们都用于虚拟化环境中提升 I/O 性能，但它们的功能和应用场景有所不同。以下是它们的详细对比：

---

#### 1. **SR-IOV（Single Root I/O Virtualization）**
##### **定义**
- SR-IOV 是一种硬件虚拟化技术，允许物理设备（如网卡、GPU）虚拟出多个虚拟功能（VF），每个 VF 可以直接分配给虚拟机使用。

##### **工作原理**
- **物理功能（PF）**：
  - 物理设备的完整功能，由宿主机管理。
- **虚拟功能（VF）**：
  - 物理设备虚拟出的多个独立功能，每个 VF 可以直接分配给虚拟机。
- **资源分配**：
  - 每个 VF 拥有独立的资源（如队列、内存），可以直接与虚拟机通信。

##### **优点**
- **高性能**：
  - 虚拟机直接访问硬件设备，绕过宿主机的软件栈，性能极高。
- **资源隔离**：
  - 每个 VF 独立分配给虚拟机，资源隔离性好。
- **灵活性**：
  - 支持动态分配和回收 VF，资源利用率高。

##### **应用场景**
- 高性能虚拟化场景，如 NFV（网络功能虚拟化）、云计算。

---

#### 2. **IOMMU（Input-Output Memory Management Unit）**
##### **定义**
- IOMMU 是一种硬件单元，用于管理设备对内存的访问，提供地址翻译和设备隔离功能。

##### **工作原理**
- **地址翻译**：
  - IOMMU 将设备的物理地址转换为宿主机的物理地址，允许设备直接访问虚拟机的内存。
- **设备隔离**：
  - IOMMU 确保设备只能访问分配给它的内存区域，防止设备越界访问。
- **DMA 重映射**：
  - IOMMU 重映射设备的 DMA 请求，确保设备只能访问授权的内存区域。

##### **优点**
- **安全性**：
  - 防止设备越界访问内存，提升系统安全性。
- **性能**：
  - 允许设备直接访问虚拟机的内存，减少内存复制开销。
- **兼容性**：
  - 支持多种设备和虚拟化方案。

##### **应用场景**
- 设备直通（如 VFIO）、虚拟化环境中的内存管理。

---

#### 3. **SR-IOV 和 IOMMU 的区别**
| **特性**         | **SR-IOV**                              | **IOMMU**                              |
|------------------|-----------------------------------------|----------------------------------------|
| **功能**         | 硬件虚拟化，将物理设备虚拟为多个 VF。    | 内存管理，提供地址翻译和设备隔离。       |
| **主要目标**     | 提升虚拟机的 I/O 性能。                  | 提升内存访问的安全性和效率。             |
| **硬件支持**     | 需要支持 SR-IOV 的硬件设备（如网卡）。    | 需要支持 IOMMU 的 CPU 和芯片组。         |
| **资源分配**     | 每个 VF 独立分配给虚拟机。               | 设备直接访问虚拟机的内存。               |
| **性能优化**     | 虚拟机直接访问硬件设备，绕过软件栈。      | 减少内存复制开销，提升 DMA 效率。        |
| **安全性**       | 资源隔离性好，但主要关注性能。            | 提供设备隔离，防止越界访问内存。         |
| **应用场景**     | 高性能虚拟化（如 NFV、云计算）。          | 设备直通（如 VFIO）、内存管理。          |

---

#### 4. **SR-IOV 和 IOMMU 的关系**
- **SR-IOV** 和 **IOMMU** 可以结合使用：
  - SR-IOV 提供硬件虚拟化功能，将物理设备虚拟为多个 VF。
  - IOMMU 提供内存管理功能，确保设备只能访问授权的内存区域。
- **典型应用**：
  - 在 VFIO 方案中，SR-IOV 用于将物理设备虚拟为多个 VF，IOMMU 用于管理设备对内存的访问。

---

#### 5. **总结**
- **SR-IOV** 是一种硬件虚拟化技术，用于将物理设备虚拟为多个 VF，提升虚拟机的 I/O 性能。
- **IOMMU** 是一种内存管理技术，用于提供地址翻译和设备隔离，提升内存访问的安全性和效率。
- **SR-IOV** 和 **IOMMU** 可以结合使用，适用于高性能虚拟化场景（如 NFV、云计算）。

## 中断虚拟化

在虚拟化环境中，中断处理是一个关键部分，不同的技术（如**中断模拟**、**中断注入**、**中断透传**和 **APIC 虚拟化**）在实现方式、性能和适用场景上各有差异。以下是对这四种中断虚拟化技术的详细对比，结合原理、实现方式、优缺点和典型应用场景进行讲解。

---

### **1. 中断模拟 (Interrupt Emulation)**
#### **定义**
- 通过虚拟化软件（如 QEMU）完全模拟物理中断控制器的行为，向虚拟机提供与真实硬件一致的中断体验。

#### **实现方式**
- **流程**：
  1. QEMU 模拟一个中断控制器（如 8259 PIC 或 IOAPIC）。
  2. 当需要通知虚拟机时，QEMU 通过 KVM 的接口（如 `KVM_INTERRUPT`）模拟中断信号。
  3. KVM 将模拟的中断注入虚拟机，触发 Guest OS 的中断处理程序。
- **典型场景**：
  - Guest 发起 I/O 请求（如写 PIO/MMIO），QEMU 完成操作后模拟中断通知 Guest。
  - 外部事件（如定时器）通过 QEMU 模拟触发。

#### **特点**
- **位置**：用户态（QEMU）主导，KVM 辅助注入。
- **性能**：低，涉及用户态到内核态切换和 VM Exit/Entry。
- **依赖性**：无需 Guest OS 修改，完全模拟硬件行为。

#### **优点**
- 通用性强，适用于全虚拟化，兼容任何 Guest OS。
- 不需要硬件支持或半虚拟化驱动。

#### **缺点**
- 开销大：每次中断需经过 QEMU 处理和 KVM 注入。
- 延迟高：多层软件栈（QEMU → KVM → Guest）。

#### **适用场景**
- 全虚拟化（如 QEMU 纯软件模拟的传统设备：串口、网卡）。
- 低性能需求场景。

##### 中断模拟主要用在全虚拟化？半虚拟化下没有用到？

您的疑问非常有针对性：**中断模拟（Interrupt Emulation）主要用在全虚拟化中吗？半虚拟化下是否完全不用到？** 为了回答这个问题，我将从中断模拟的定义、适用场景以及全虚拟化和半虚拟化的具体需求出发，详细分析中断模拟在两种虚拟化类型中的使用情况，并澄清其在半虚拟化中的角色。

---

###### **1. 中断模拟的定义与核心特点**
- **定义**：
  - 中断模拟是通过虚拟化软件（如 QEMU）完全模拟物理中断控制器的行为（如 8259 PIC、IOAPIC、MSI），以向虚拟机提供与真实硬件一致的中断体验。
- **核心特点**：
  - **兼容性**：无需 Guest OS 修改，模拟硬件中断的完整流程。
  - **软件主导**：QEMU 负责模拟中断控制器状态，KVM 负责注入。
  - **低效性**：涉及 VM Exit、用户态到内核态切换，性能较低。

---

###### **2. 中断模拟在全虚拟化中的主要应用**
- **全虚拟化（Full Virtualization）**：
  - **定义**：Guest OS 无需修改，直接运行在虚拟化层（如 QEMU + KVM）上，虚拟机认为自己在物理硬件上运行。
  - **需求**：模拟真实的硬件行为，包括中断控制器，以支持未经修改的操作系统。
- **中断模拟的使用**：
  - **场景**：
    - 模拟传统设备（如串口、键盘、网卡）或 virtio 设备时，Guest OS 使用标准硬件驱动。
    - 示例：Windows Guest 使用标准以太网驱动，QEMU 模拟网卡和中断。
  - **流程**：
    - **收包**：物理网络 → QEMU → vring → QEMU 模拟中断 → KVM 注入。
    - **发包**：Guest → vring → VM Exit → QEMU 处理 → QEMU 模拟中断。
  - **中断控制器**：
    - QEMU 模拟 8259 PIC、IOAPIC 或 MSI，KVM 注入虚拟中断。
- **原因**：
  - 全虚拟化要求虚拟机完全感知不到虚拟化层，中断模拟是实现硬件透明性的关键。
  - Guest OS 无法利用半虚拟化优化（如 **eventfd**），只能依赖模拟。

###### **结论**
- **中断模拟是全虚拟化的主要方式**，因为它提供了硬件级别的兼容性，适用于任何未经修改的 Guest OS。

---

###### **3. 中断模拟在半虚拟化中的角色**
- **半虚拟化（Paravirtualization）**：
  - **定义**：Guest OS 经过修改，感知虚拟化环境，使用优化的前端驱动（如 virtio-net）与后端（如 vhost-net、vhost-user）通信。
  - **需求**：追求高性能，减少虚拟化开销，通常通过事件驱动机制（如 **eventfd**、**ioeventfd**、**irqfd**）替代传统中断模拟。
- **中断模拟是否完全不用？**：
  - **通常不用**：
    - 半虚拟化优先使用优化机制（如 **irqfd** 和 **ioeventfd**），以减少 VM Exit 和 QEMU 的介入。
    - 示例：
      - **vhost-net**：接收用 **eventfd + IRQFD**，发送用 **ioeventfd**。
      - **vhost-user**：类似，但 QEMU 仅协调 **irqfd**。
  - **仍可能用到中断模拟的情况**：
    1. **初始化阶段**：
       - 在 virtio 设备初始化时，Guest OS 可能通过标准中断（如 MSI）与 QEMU 交互，此时 QEMU 仍需模拟中断控制器。
       - 示例：virtio-net 配置 MSI 时，QEMU 模拟 MSI 配置过程。
    2. **非优化设备**：
       - 如果虚拟机中混用了非半虚拟化设备（如传统串口），QEMU 仍需模拟中断（如 8259 PIC）。
    3. **后端未完全接管**：
       - 在 vhost 未启用或部分功能未卸载到后端时，QEMU 可能回退到中断模拟。
       - 示例：vhost-net 未启用时，QEMU 模拟 virtio-net 的中断。
    4. **IPI 场景**：
       - 多核虚拟机的核间中断（IPI）仍需 QEMU 模拟 vAPIC 的中断注入，尤其在无 APIC 虚拟化支持时。

###### **半虚拟化的优化机制**
- **irqfd**：
  - 代替 QEMU 模拟中断注入，KVM 直接注入虚拟中断。
  - 优化 Host → Guest 通知。
- **ioeventfd**：
  - 代替 VM Exit 后 QEMU 的分发，KVM 直接触发 **eventfd**。
  - 优化 Guest → Host 通知。
- **结果**：
  - 半虚拟化通过这些机制大幅减少了对中断模拟的依赖，QEMU 的角色从模拟转为协调。

###### **具体流程对比**
- **全虚拟化（中断模拟）**：
  - 收包：QEMU 模拟网卡 → vring → QEMU 模拟中断 → KVM 注入。
  - 发包：Guest → vring → VM Exit → QEMU 处理。
- **半虚拟化（vhost-net）**：
  - 收包：vhost-net → **eventfd** → KVM (**irqfd**) → vring → Guest。
  - 发包：Guest → vring → KVM (**ioeventfd**) → vhost-net。

###### **结论**
- **半虚拟化下中断模拟不是主要方式**，但并非完全不用。在特定场景（如初始化、非优化设备或 IPI），中断模拟仍可能作为补充手段。

---

###### **4. 在表格中的体现**
结合之前的表格：
- **全虚拟化 (纯软件模拟)**：
  - **中断虚拟化方式**：**中断模拟**。
  - **数据收发包机制**：QEMU 模拟中断（8259 PIC、IOAPIC、MSI）。
  - **依赖性**：完全依赖 QEMU 模拟。
- **半虚拟化 (vhost-net/vhost-user)**：
  - **中断虚拟化方式**：**中断注入**（**irqfd** 和 **ioeventfd**）。
  - **数据收发包机制**：**eventfd + IRQFD**（接收）、**ioeventfd**（发送）。
  - **中断模拟的角色**：仅用于初始化或非优化设备（如传统串口）。
- **半虚拟化 (vDPA)**：
  - **中断虚拟化方式**：**中断透传**。
  - **中断模拟**：无。
- **VFIO**：
  - **中断虚拟化方式**：**中断透传**。
  - **中断模拟**：无。

---

###### **5. 为什么半虚拟化减少中断模拟？**
- **性能驱动**：
  - 全虚拟化追求兼容性，接受低效的模拟中断。
  - 半虚拟化追求性能，通过 **eventfd** 等机制卸载中断处理到后端。
- **前端-后端协作**：
  - 半虚拟化的 virtio 前端和 vhost 后端通过共享内存（vring）和事件通知（**eventfd**）通信，减少了对 QEMU 模拟的需求。
- **中断注入的优化**：
  - **irqfd** 将 QEMU 的中断模拟替换为 KVM 的直接注入，保留了虚拟中断的效果但提升了效率。

---

###### **6. 总结**
- **中断模拟主要用于全虚拟化**：
  - 全虚拟化依赖 QEMU 模拟中断控制器（如 PIC、IOAPIC、MSI），以支持未经修改的 Guest OS。
  - 示例：QEMU 模拟网卡中断注入。
- **半虚拟化下并非完全不用**：
  - 主要使用 **irqfd** 和 **ioeventfd** 优化中断处理，减少对中断模拟的依赖。
  - 但在初始化、非优化设备或 IPI 场景中，QEMU 仍可能模拟中断。
- **区别**：
  - 全虚拟化：中断模拟是核心机制。
  - 半虚拟化：中断模拟是辅助手段，优化机制（如 **irqfd**）为主。
---

#### **中断模拟对比**
##### **概述**
在虚拟化环境中，中断注入是指将模拟的中断信号从宿主机（Host）传递到虚拟机（Guest），以通知 Guest 处理特定事件（如 I/O 完成）。中断注入是中断模拟的核心部分，通常由 QEMU 和 KVM 协作完成，目的是让 Guest OS 感知到与物理中断一致的行为。

##### **实现流程**
1. **事件触发**：
   - 宿主机上的虚拟设备（如 QEMU 模拟的网卡）完成某项操作（如接收数据包）。
   - QEMU 决定需要通知 Guest。
2. **中断模拟**：
   - QEMU 更新虚拟中断控制器的状态。例如：
     - 对于 8259 PIC，设置中断请求寄存器（IRR）。
     - 对于 IOAPIC，更新中断路由表。
   - QEMU 调用 KVM 的接口（如 `KVM_INTERRUPT` 或 `KVM_SET_IRQ`）请求注入中断。
3. **KVM 处理**：
   - KVM 接收 QEMU 的请求，模拟虚拟中断控制器（如 vPIC 或 vAPIC）。
   - KVM 通过虚拟中断向量将中断注入虚拟机，触发 Guest 的中断处理程序。
4. **Guest 响应**：
   - Guest OS 检测到中断，调用相应的中断服务例程（ISR）处理事件。

##### **关键技术**
- **虚拟中断控制器**：
  - KVM 维护虚拟机的 vPIC（8259 PIC）或 vAPIC（Local APIC），模拟物理中断控制器的行为。
- **注入方式**：
  - **传统注入**：通过 `KVM_INTERRUPT` 注入固定 IRQ。
  - **MSI 注入**：通过内存写操作模拟消息信号中断。
- **同步性**：
  - KVM 确保中断注入与 Guest 的运行状态同步，避免丢失或重复。

##### **性能瓶颈**
- 每次注入需要 QEMU（用户态）与 KVM（内核态）的协作，涉及上下文切换。
- Guest 处理中断可能触发 VM Exit（如访问中断控制器寄存器），增加开销。

##### **应用场景**
- 全虚拟化中，QEMU 模拟传统设备（如串口、网卡）时，通过中断注入通知 Guest。

---

##### **PIC 中断模拟**
###### **概述**
PIC（Programmable Interrupt Controller，可编程中断控制器）是早期 x86 系统中使用的中断控制器（如 8259），支持 8 个中断请求（IRQ），通过级联可扩展到 15 个。在虚拟化中，QEMU 模拟虚拟 PIC（vPIC）以支持传统设备的中断处理。

###### **实现原理**
- **状态模拟**：
  - QEMU 维护 vPIC 的寄存器状态：
    - **IRR（Interrupt Request Register）**：记录待处理的中断请求。
    - **ISR（In-Service Register）**：记录正在处理的中断。
    - **IMR（Interrupt Mask Register）**：屏蔽特定 IRQ。
  - Guest 访问这些寄存器时，QEMU 拦截并模拟读写。
- **中断触发**：
  - QEMU 模拟设备（如串口）触发 IRQ（如 IRQ 4）。
  - QEMU 更新 vPIC 的 IRR，标记中断请求。
- **注入流程**：
  - QEMU 调用 KVM 接口（如 `KVM_INTERRUPT`），将 IRQ 注入虚拟机。
  - KVM 通过 vPIC 将中断传递给 Guest 的某个 vCPU。

###### **具体流程**
1. **设备触发**：
   - QEMU 模拟的串口收到数据，决定触发 IRQ 4。
2. **vPIC 更新**：
   - QEMU 设置 vPIC 的 IRR 位 4。
3. **注入中断**：
   - QEMU 请求 KVM 注入 IRQ 4。
   - KVM 通过 vPIC 将中断注入 Guest。
4. **Guest 处理**：
   - Guest OS 检测到 IRQ 4，调用串口驱动的中断处理程序。
5. **中断结束**：
   - Guest 写入 vPIC 的 EOI（End of Interrupt），QEMU 模拟清除 ISR。

###### **特点**
- **简单性**：
  - 支持单核或简单设备，逻辑清晰。
- **局限性**：
  - 只支持 15 个 IRQ，不适合现代多核系统或高并发设备。

###### **应用场景**
- 模拟传统设备（如串口、并口、键盘），常见于老旧 Guest OS。

---

##### **I/O APIC 中断模拟**
###### **概述**
I/O APIC（I/O Advanced Programmable Interrupt Controller）是现代 x86 系统中使用的中断控制器，支持更多 IRQ（通常 24 个或更多），并与 Local APIC 配合，提供灵活的中断路由。在虚拟化中，QEMU 模拟虚拟 I/O APIC（vIOAPIC）以支持复杂设备的中断需求。

###### **实现原理**
- **状态模拟**：
  - QEMU 维护 vIOAPIC 的寄存器状态：
    - **Redirection Table**：每个 IRQ 的路由信息（如目标 CPU、中断向量）。
    - **Interrupt Pin Assertion**：记录设备触发的中断。
  - Guest 访问 vIOAPIC 寄存器时，QEMU 拦截并模拟。
- **中断触发**：
  - QEMU 模拟设备（如网卡）触发某个 IRQ。
  - QEMU 更新 vIOAPIC 的 Redirection Table，指定目标 vCPU 和向量。
- **注入流程**：
  - QEMU 调用 KVM 接口，注入指定向量到目标 vCPU。
  - KVM 通过 vAPIC（虚拟 Local APIC）完成注入。

###### **具体流程**
1. **设备触发**：
   - QEMU 模拟的网卡收到数据包，触发 IRQ 10。
2. **vIOAPIC 更新**：
   - QEMU 设置 vIOAPIC 的 Redirection Table，将 IRQ 10 路由到 vCPU0，向量 32。
3. **注入中断**：
   - QEMU 请求 KVM 注入向量 32。
   - KVM 通过 vCPU0 的 vAPIC 注入中断。
4. **Guest 处理**：
   - Guest OS 检测到向量 32，调用网卡驱动处理数据包。
5. **中断结束**：
   - Guest 写入 vAPIC 的 EOI，QEMU 模拟清除 vIOAPIC 状态。

###### **特点**
- **灵活性**：
  - 支持多核路由，适应复杂设备。
- **复杂性**：
  - 需与 vAPIC 协同，模拟逻辑更复杂。

###### **应用场景**
- 模拟现代设备（如 virtio-net、磁盘控制器），支持多核虚拟机。

---

##### **MSI 中断模拟**
###### **概述**
MSI（Message Signaled Interrupts）和 MSI-X 是现代设备使用的中断机制，通过内存写操作触发中断，不依赖传统中断线。在虚拟化中，QEMU 模拟设备的 MSI/MSI-X 功能，提供高性能中断支持。

###### **实现原理**
- **状态模拟**：
  - QEMU 模拟设备的 MSI/MSI-X 配置空间：
    - **Message Address**：目标中断控制器的内存地址。
    - **Message Data**：中断向量和触发信息。
  - Guest 配置 MSI 时，QEMU 拦截并记录。
- **中断触发**：
  - QEMU 模拟设备触发 MSI，构造内存写操作。
  - QEMU 将 Message Data（向量）传递给 KVM。
- **注入流程**：
  - KVM 解析 MSI 请求，通过 vAPIC 注入指定向量到目标 vCPU。

###### **具体流程**
1. **设备配置**：
   - Guest 的 virtio-net 驱动配置 MSI，写入 Message Address 和 Data。
   - QEMU 拦截，记录映射关系（例如向量 40 映射到 vCPU0）。
2. **设备触发**：
   - QEMU 模拟 virtio-net 收到数据包，触发 MSI。
   - QEMU 构造 MSI 写操作（地址指向 vAPIC，数据为向量 40）。
3. **注入中断**：
   - KVM 接收 MSI 请求，通过 vCPU0 的 vAPIC 注入向量 40。
4. **Guest 处理**：
   - Guest OS 检测到向量 40，调用 virtio-net 中断处理程序。
5. **中断结束**：
   - Guest 写入 vAPIC 的 EOI，QEMU 模拟清除状态。

###### **特点**
- **高效性**：
  - 无需共享中断线，支持大量独立向量。
- **现代性**：
  - 广泛用于 virtio 和高性能设备。

###### **应用场景**
- 模拟 virtio 设备（如 virtio-net、virtio-blk），支持高吞吐量场景。

---

##### **总结与对比**
| **类型**       | **控制器**   | **特点**                        | **适用场景**                |
|----------------|--------------|---------------------------------|-----------------------------|
| **PIC**        | 8259 vPIC    | 简单，支持 15 个 IRQ           | 传统设备（串口、键盘）      |
| **I/O APIC**   | vIOAPIC      | 灵活，支持多核路由            | 现代设备（网卡、磁盘）      |
| **MSI**        | vAPIC + MSI  | 高效，支持大量独立向量         | 高性能设备（virtio）        |

- **虚拟化环境下的中断注入**：
  - 通用流程：QEMU 模拟 → KVM 注入。
  - 性能瓶颈：上下文切换和 VM Exit。
- **中断模拟的细分**：
  - PIC：老旧但简单。
  - I/O APIC：现代且灵活。
  - MSI：高效且高性能。
---

### **2. 中断注入 (Interrupt Injection)**
#### **定义**
- 虚拟化层（通常是 KVM）将中断信号注入虚拟机，通常结合半虚拟化优化机制（如 **irqfd**），提升效率。

#### **实现方式**
- **传统中断注入**：
  - QEMU 完成 I/O 操作后，通过 KVM 接口（如 `KVM_INTERRUPT`）请求注入虚拟中断。
  - KVM 模拟 APIC 或 PIC，将中断注入虚拟机。
- **irqfd 优化**：
  1. 初始化时，QEMU 或后端通过 `ioctl(KVM_IRQFD)` 将 **eventfd** 与虚拟中断绑定。
  2. 后端（如 vhost-net）触发 **eventfd**，KVM 直接注入中断，无需 QEMU 介入。
- **流程**：
  - 后端触发事件 → KVM 注入虚拟中断 → Guest 处理。

#### **特点**
- **位置**：内核态（KVM）为主，QEMU 仅初始化或协调。
- **性能**：中等至高（传统方式较低，**irqfd** 优化较高）。
- **依赖性**：半虚拟化（如 virtio）中常见，需 KVM 支持。

#### **优点**
- **irqfd** 优化下效率高，减少用户态开销。
- 灵活性强，可用于多种虚拟设备（如 virtio-net）。

#### **缺点**
- 传统方式仍需 QEMU 处理，开销较大。
- 需要 KVM 支持（如 **irqfd**），配置较复杂。

#### **适用场景**
- 半虚拟化（如 vhost-net、vhost-user 的接收方向）。
- 需要高效通知 Guest 的场景。

---

### **3. 中断透传 (Interrupt Passthrough)**
#### **定义**
- 将物理硬件的中断直接传递给虚拟机，绕过虚拟化层的模拟或注入，依赖硬件支持（如 SR-IOV 或 VFIO）。

#### **实现方式**
- **流程**：
  1. 硬件设备（如网卡的 VF）通过 IOMMU 分配给虚拟机。
  2. 物理中断触发后，IOMMU 将中断直接映射到虚拟机的中断控制器（如 vAPIC）。
  3. Guest OS 的原生驱动直接处理中断，无需 KVM 或 QEMU 介入。
- **技术支持**：
  - **SR-IOV**：网卡生成多个虚拟功能（VF），分配给虚拟机。
  - **VFIO**：通过 VFIO 框架将设备直通，配置中断映射。

#### **特点**
- **位置**：硬件直接与虚拟机交互。
- **性能**：最高，几乎无虚拟化开销。
- **依赖性**：需要硬件支持（如 SR-IOV 网卡）和 IOMMU。

#### **优点**
- 接近物理机性能，中断处理零延迟。
- 绕过虚拟化层，简化流程。

#### **缺点**
- 硬件依赖性强，需支持 SR-IOV 或直通。
- 灵活性低，一个设备只能分配给一个虚拟机。

#### **适用场景**
- 高性能场景（如 vDPA、VFIO 的网络或存储设备）。
- 数据中心需要低延迟的虚拟化。

---

### **4. APIC 虚拟化 (APIC Virtualization)**
#### **定义**
- 通过硬件辅助（如 Intel VT-x 的 APICv 或 AMD 的 AVIC）虚拟化高级可编程中断控制器（APIC），减少 VM Exit，提高中断处理效率。

#### **实现方式**
- **流程**：
  1. CPU 硬件支持（如 Intel APICv）直接管理虚拟机的 APIC。
  2. 虚拟机访问 APIC（如发送 EOI 或接收中断）时，硬件自动处理，无需 VM Exit。
  3. KVM 配置硬件支持，注入中断或处理 IPI（核间中断）时直接由硬件完成。
- **技术支持**：
  - **Intel APICv**：包括 Posted Interrupts（直接投递中断）和 Virtual Interrupt Delivery。
  - **AMD AVIC**：类似功能，优化中断虚拟化。

#### **特点**
- **位置**：硬件层（CPU）主导，KVM 配置。
- **性能**：极高，接近物理机，VM Exit 大幅减少。
- **依赖性**：需要 CPU 支持（如 Intel VT-x 或 AMD-V）。

#### **优点**
- 极低延迟，中断处理无需退出虚拟机。
- 支持复杂场景（如多核虚拟机的 IPI）。

#### **缺点**
- 硬件依赖性强，老旧 CPU 不支持。
- 配置复杂，需 KVM 和 Guest OS 支持。

#### **适用场景**
- 高性能虚拟化（如现代数据中心 KVM 虚拟机）。
- 多核虚拟机需要高效 IPI 的场景。

---

### **5. 对比表格**
| **特性**            | **中断模拟**          | **中断注入**          | **中断透传**          | **APIC 虚拟化**       |
|---------------------|-----------------------|-----------------------|-----------------------|-----------------------|
| **实现层**          | 用户态（QEMU）        | 内核态（KVM）         | 硬件层（IOMMU）       | 硬件层（CPU）         |
| **性能**            | 低                   | 中等至高（**irqfd** 高） | 最高                 | 极高                 |
| **开销**            | 高（多层切换）        | 中等（**irqfd** 低）  | 几乎无               | 极低（无 VM Exit）   |
| **依赖性**          | 无硬件依赖            | KVM + 半虚拟化        | SR-IOV/IOMMU          | CPU 支持（APICv/AVIC）|
| **通知方式**        | QEMU 模拟注入         | KVM 注入（**irqfd** 优化） | 硬件直通            | 硬件直接处理          |
| **适用虚拟化类型**  | 全虚拟化             | 半虚拟化              | vDPA/VFIO             | 所有类型（硬件支持）  |
| **典型应用**        | 传统设备（如串口）    | virtio-net            | 高性能网卡            | 多核虚拟机            |
| **灵活性**          | 高                   | 中等                  | 低                   | 中等                  |

---

### **6. 在收发包中的应用**
结合之前表格的收发包流程：
- **全虚拟化 (纯软件模拟)**：
  - **中断模拟**：QEMU 模拟中断注入（接收）和捕获通知（发送），性能最低。
- **vhost-net/vhost-user**：
  - **中断注入**：接收用 **irqfd** 注入中断，发送用 **ioeventfd** 通知后端，中等至高性能。
- **vDPA**：
  - **中断透传**：硬件直接触发中断，接收和发送均高效。
- **VFIO**：
  - **中断透传**：物理中断直通虚拟机，性能最佳。
- **APIC 虚拟化**：
  - 可与上述任一方式结合（如 vhost-net + APICv），进一步减少 VM Exit，提升效率。

---

### **7. 总结**
- **中断模拟**：全虚拟化的基石，通用但低效，适合传统设备。
- **中断注入**：半虚拟化的优化（如 **irqfd**），效率较高，广泛用于 virtio。
- **中断透传**：硬件直通的极致性能，依赖 SR-IOV 和 VFIO，适合高性能需求。
- **APIC 虚拟化**：硬件辅助的未来方向，减少 VM Exit，适用于现代虚拟化。

## 显示虚拟化

- [QEMU显示虚拟化的几种选项](https://blog.csdn.net/tugouxp/article/details/134487575)
- [GPU虚拟化技术探索](https://mp.weixin.qq.com/s/LEVdZYLz62H_zwTthxuw7A)

- [FT2000+ openEuler 20.03 LTS SP3 yum install qemu手动创建虚拟机 图形界面安装openEuler 20.03系统](https://blog.csdn.net/hknaruto/article/details/130154674)
- [解决qemu-system-aarch64 Guest has not initialized the display (yet)](https://blog.csdn.net/hknaruto/article/details/127515804)

## KVM性能优化

- [KVM性能优化最佳实践](https://blog.csdn.net/allway2/article/details/102760738)
- [KVM 虚拟化基本原理](https://www.toutiao.com/article/6862880595601523211)

```bash
ps -eLf | grep -i vm-uuid | awk '{print $2}' | xargs -I {} sh -c 'echo "==== PID: {} ===="; cat /proc/{}/stack'
```

### ksm

- [内存管理实战案例分析2：KSM和NUMA引发的虚拟机宕机](https://juejin.cn/post/7068991559689535525)
- [针对低端机KSM的优化](https://blog.csdn.net/pillarbuaa/article/details/79205426)
- [kvm内存优化--KSM](https://www.cnblogs.com/lcword/p/14361931.html)
- [浅析内核同页合并(Kernel Same-page Merging, KSM)](https://www.toutiao.com/article/7227665373129212456)

### VIFO透传

- [KVM设备透传与重定向](https://blog.csdn.net/zhongbeida_xue/article/details/103602105)
- [19.3.2. 将 USB 设备附加到虚拟机](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/sect-the_virtual_hardware_details_window-attaching_usb_devices_to_a_guest_virtual_machine)
- [19.3.3. USB 重定向](https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/sect-the_virtual_hardware_details_window-usb_redirection)
- [USB虚拟化](https://blog.csdn.net/jerryxiao1979/article/details/128453912)
- [unraid 直通usb](https://juejin.cn/s/unraid%20%E7%9B%B4%E9%80%9Ausb)
- [libvirt-usb设备透传给虚拟机](http://www.manongjc.com/detail/50-cxzuhdjzmdqrklm.html)

## qemu

### qemu monitor

- [36 使用 QEMU 监视器管理虚拟机](https://documentation.suse.com/zh-cn/sles/15-SP4/html/SLES-all/cha-qemu-monitor.html)
- [QEMU Monitor](https://www.qemu.org/docs/master/system/monitor.html)
- [QEMU monitor控制台使用详解](https://blog.csdn.net/qq_43523618/article/details/106278245)
- [如何退出 QEMU 退出快捷键：Ctrl + a，然后按 x 键。](https://zhuanlan.zhihu.com/p/518032838)

导出压缩的内存快照:

```bash
virsh qemu-monitor-command vm-uuid --hmp "dump-guest-memory -z vmcore"
```

导出的vmcore默认存放在根目录下。

### x86_64下qemu虚拟x86_64

- [<font color=Red>使用 VSCode + qemu 搭建 Linux 内核调试环境</font>](https://howardlau.me/programming/debugging-linux-kernel-with-vscode-qemu.html)
- [Linux内核源码远程调试（3.16.84）](https://www.cnblogs.com/harmful-chan/p/12994693.html)
- [[kernel]linux内核基础:版本、源码、编译与调试](https://blog.csdn.net/Breeze_CAT/article/details/123787636)
- [[首发][Ubuntu]VSCode搭建Linux Kernel单步调试IDE环境](https://blog.csdn.net/rlk8888/article/details/122396219)
- [<font color=Red>使用VSCode + qemu搭建Linux内核调试环境</font>](https://blog.csdn.net/eydwyz/article/details/114019532)
- [<font color=Red>利用QEMU+GDB搭建Linux内核调试环境</font>](https://bbs.huaweicloud.com/blogs/348654)
- [Linux内核调试方法](https://mp.weixin.qq.com/s/KzFFkJHLzbNFixiELMjNWg)
- [<font color=Red>基于 debootstrap 和 busybox 构建 mini ubuntu</font>](https://www.cnblogs.com/fengyc/p/6114648.html)
- [https://manpages.ubuntu.com/manpages/xenial/en/man1/debirf.1.html](https://manpages.ubuntu.com/manpages/xenial/en/man1/debirf.1.html)

- [QEMU 实验（三）：根文件系统构建 (基于 Busybox)](https://www.jianshu.com/p/b4bae215e278)
- [怎么用QEMU搭建Linux kernel开发调试环境](https://www.yisu.com/zixun/503146.html)
- [嵌入式开发模拟器：qemu使用，仿真多种方式启动内核kernel (超详细，tftp/nfs等方式)](https://blog.csdn.net/birencs/article/details/126666827)
- [使用qemu模拟调试内核和debian根文件系统](https://www.bbsmax.com/A/WpdK7VrodV/)
- [QEMU 文件系统制作：自己制作根目录和应用程序 + BUSYBOX](https://www.freesion.com/article/2442234327/)
- [QemuDebootstrap](https://wiki.ubuntu.com/ARM/RootfsFromScratch/QemuDebootstrap)
- [<font color=Red>内核调试环境：buildroot/debootstrap制作文件系统、编译内核、QEMU模拟</font>](https://blog.csdn.net/weixin_49393427/article/details/126435589)
- [<font color=Red>使用 debootstrap 制作 ARM64 rootfs.cpio</font>](https://blog.51cto.com/u_13731941/5399257)
- [<font color=Red>debootstrap 制作根文件系统</font>](https://www.cnblogs.com/huaibovip/p/debootstrap-fs.html)

- [使用Buildroot + QEMU构建和运行Linux](https://blog.csdn.net/xunknown/article/details/124521135)
- [制作基于beaglebonebalck的rootfs使用buildroot](https://blog.csdn.net/xingkong0/article/details/100586485)
- [ubuntu下qemu使用：图文详解](https://blog.csdn.net/qq_34160841/article/details/104891169)
- [QEMU调试Linux内核环境搭建](https://www.toutiao.com/article/7086755948068487687)
- [一文分析Linux虚拟化KVM-Qemu（概念篇）](https://www.toutiao.com/article/7182563085466108471)

- [<font color=Red>qemu: usb存储设备仿真</font>](https://blog.csdn.net/kingtj/article/details/82952783)
- [如何在 Linux 中导出和导入 KVM 虚拟机](https://www.51cto.com/article/708013.html)

- [<font color=Red>ARMv8架构下修改Linux内核并打开kvm硬件虚拟化支持（平台Firefly-rk3568）</font>](https://blog.csdn.net/qq_40598297/article/details/121084904)

- [<font color=Red>制作一个grub虚拟启动盘，在qemu下调试</font>](https://blog.csdn.net/qq_41957544/article/details/105769697)

- [Running a full system stack under QEMUarm64](https://cdn.kernel.org/pub/linux/kernel/people/will/docs/qemu/qemu-arm64-howto.html)
- [qemu模拟vexpress开发板](https://www.cnblogs.com/bigsissy/p/11134802.html)
- [<font color=Red>第四期 QEMU调试Linux内核实验 《虚拟机就是开发板》</font>](https://blog.csdn.net/aggresss/article/details/54946438)

### arm64下qemu虚拟arm64

- [ARM平台检测是否支持虚拟化的几种常见方法](https://blog.csdn.net/sinat_34833447/article/details/109765004)
- [X86_64 环境下使用 QEMU 虚拟机安装 ARM 版 EulerOS 小记](https://www.txisfine.cn/archives/a0d5fa12)
- [利用qemu-system-aarch64调试Linux内核（arm64）](https://blog.csdn.net/Oliverlyn/article/details/105178832)
- [QEMU搭建arm64 Linux调试环境](https://zhuanlan.zhihu.com/p/345232459)

### x86_64下qemu虚拟arm64

- [VSCode+GDB+Qemu调试ARM64 linux内核](https://zhuanlan.zhihu.com/p/510289859)
- [编译arm64内核](https://blog.csdn.net/fell_sky/article/details/119818112)
- [交叉编译arm64内核](https://blog.csdn.net/shanruo/article/details/80474338)
- [交叉编译环境下对linux内核编译](https://blog.csdn.net/ludaoyi88/article/details/115633849)
- [交叉编译linux内核并使用qemu运行](https://blog.csdn.net/jinking01/article/details/129580621)

### kunpeng 920

```bash
/usr/libexec/qemu-kvm \
-smp 8 \
-enable-kvm \
-cpu host \
-m 16G \
-machine virt-rhel7.6.0,gic-version=3 \
-drive file=/usr/share/edk2/aarch64/QEMU_EFI-pflash.raw,if=pflash,format=raw \
-drive file=/var/lib/libvirt/qemu/nvram/wujing_VARS.fd,if=pflash,format=raw \
-hda /data_vm/wujing/virsh/wujing/wujing.qcow2 \
-nographic
```

该命令配置和启动一个基于 ARM 架构（aarch64）的虚拟机，并将所有输出重定向到当前终端。以下是该命令的详细解析：

1. **`/usr/libexec/qemu-kvm`**：指定使用 `qemu-kvm` 可执行文件来启动虚拟机，通常位于 `/usr/libexec/` 目录下。

2. **`-smp 8`**：为虚拟机分配 8 个虚拟 CPU。

3. **`-enable-kvm`**：启用 KVM 硬件加速，利用主机的 CPU 资源来提升虚拟机的性能。

4. **`-cpu host`**：配置虚拟机的 CPU 设置，使其与主机的 CPU 一致，以便最大限度地利用主机 CPU 的特性。

5. **`-m 16G`**：分配 16GB 内存给虚拟机。

6. **`-machine virt-rhel7.6.0,gic-version=3`**：指定虚拟机的机器类型为 `virt-rhel7.6.0`，并设置 ARM 架构的中断控制器 GIC 的版本为 3。

7. **`-drive file=/usr/share/edk2/aarch64/QEMU_EFI-pflash.raw,if=pflash,format=raw`**：指定用于 UEFI 引导的固件文件（这是一个 pflash 设备），通常用于 ARM 架构系统的引导。

8. **`-drive file=/var/lib/libvirt/qemu/nvram/wujing_VARS.fd,if=pflash,format=raw`**：指定虚拟机的 NVRAM 文件（另一个 pflash 设备），用于存储 UEFI 固件变量和配置。

9. **`-hda /data_vm/wujing/virsh/wujing/wujing.qcow2`**：指定虚拟机的硬盘镜像文件为 `wujing.qcow2`，这是虚拟机的主存储设备，包含操作系统和数据。

10. **`-nographic`**：禁用虚拟机的图形输出，将所有输出（包括引导信息和操作系统日志）重定向到当前终端，使其适合在非图形界面或远程 SSH 环境中使用。

### Mac OSX

- [<font color=Red>使用Qemu在Mac上安装虚拟机</font>](https://blog.csdn.net/weixin_39759247/article/details/126569448)
- [使用QEMU在macOS上创建Ubuntu 20.04桌面虚拟机](https://www.arthurkoziel.com/qemu-ubuntu-20-04/)
- [Running virt-manager and libvirt on macOS](https://www.arthurkoziel.com/running-virt-manager-and-libvirt-on-macos/)
- [Mac安装Linux的KVM管理工具virt-manager](https://blog.csdn.net/weixin_30883777/article/details/95729678)
- [homebrew-virt-manager](https://github.com/jeffreywildman/homebrew-virt-manager)
- [macOS系统和mac装Windows系统开启虚拟化](https://blog.csdn.net/nbin_newby/article/details/120307866)
- [macOS | nvram boot-args的作用及设置方式](https://blog.csdn.net/MissMango0820/article/details/127398047)
- [GitHub - kholia/OSX-KVM: Run macOS on QEMU/KVM. With OpenCore + Big Sur + Monterey + Ventura support now! Only commercial (paid) support is available now to avoid spammy issues. No Mac system is required.](https://github.com/kholia/OSX-KVM)
- [Linux kernel debug on macOS 搭建可视化内核debug环境](https://zhuanlan.zhihu.com/p/399857241)
- [UTM - Virtual machines for iOS and macOS](https://github.com/utmapp/UTM)
- [Parallels Desktop 19.2.1 54832 破解教程（pd19永久授权）](https://www.luoxx.top/archives/pd-18-active?cid=162)
- [如何轻松扩展Parallel Desktop下的Ubuntu虚拟机磁盘空间](https://blog.coderluny.com/archives/53)

## buildroot

- [使用Buildroot + QEMU构建和运行Linux](https://blog.csdn.net/xunknown/article/details/124521135)
- [最新Buildroot2021.08.1搭建qemu环境](https://zhuanlan.zhihu.com/p/426026299)
- [<font color=Red>从0开始使用QEMU模拟ARM开发环境之buildroot构建linux根文件系统</font>](https://blog.csdn.net/leacock1991/article/details/113730672)
- [Buildroot 切换到国内源](https://blog.csdn.net/lsg19920625/article/details/130837469)
- [<font color=Red>The Buildroot user manual</font>](https://buildroot.org/downloads/manual/manual.html)
- [error: 'ARPHRD_MCTP' undeclared (first use in this function)#20694](https://github.com/systemd/systemd/issues/20694)
- [basic/linux: Sync if_arp.h with Linux 5.14 #20695](https://github.com/systemd/systemd/pull/20695/commits/67cd626399b0d02882ee00716c8bd31ba764c862)
- buildroot/board/qemu/aarch64-virt/readme.txt

    qemu_aarch64_virt_defconfig：

    ```bash
      qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -smp 1 -kernel output/images/Image -append "rootwait root=/dev/vda console=ttyAMA0" -netdev user,id=eth0 -device virtio-net-device,netdev=eth0 -drive file=output/images/rootfs.ext4,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
    ```

## loongarch

- [龙芯虚拟化使用手册](https://blog.csdn.net/faxiang1230/article/details/120907896)

## openstack

- [什么是OpenStack？](https://info.support.huawei.com/info-finder/encyclopedia/zh/OpenStack.html)
- [OpenStack是什么？](https://c.biancheng.net/view/3892.html)
- [OpenStack Installation Guide for Ubuntu](https://docs.openstack.org/mitaka/zh_CN/install-guide-ubuntu/)
- [超详细ubuntu20安装搭建openstack](https://blog.csdn.net/qq_53348314/article/details/123021856)
- [Ubuntu 20使用devstack快速安装openstack最新版](https://www.cnblogs.com/dyd168/p/14476271.html)

## vmware

- [在Ubuntu 20.04上安装VMWare Workstation](https://waydo.xyz/soft/linux/ubuntu-vmware-workstation/)
- [如何用命令行模式启动VMWare虚拟机](https://blog.csdn.net/u014389734/article/details/107481852)
- [在 Linux 主机上安装 Workstation Pro](https://docs.vmware.com/cn/VMware-Workstation-Pro/17/com.vmware.ws.using.doc/GUID-1F5B1F14-A586-4A56-83FA-2E7D8333D5CA.html)

### VMware 共享文件夹

- [安装 Open VM Tools](https://docs.vmware.com/cn/VMware-Tools/12.4.0/com.vmware.vsphere.vmwaretools.doc/GUID-C48E1F14-240D-4DD1-8D4C-25B6EBE4BB0F.html)
- [open-vm-tools使用共享文件夹](https://blog.csdn.net/weixin_44565095/article/details/95937794)
- [open-vm-tools工具的安装与使用](https://www.jianshu.com/p/b6ad701a2045)
- [vmware使用open-vm-tools配置ubuntu共享文件夹](https://blog.csdn.net/chinoukin/article/details/84984947)
- [VMware中Linux虚拟机挂载主机共享文件夹的方法](https://www.cnblogs.com/foolbit/p/9954332.html)

1. **列出共享文件夹**：

   ```bash
   vmware-hgfsclient
   code
   ```

   - 这个命令列出所有在 VMware 虚拟机中配置的共享文件夹。在你的情况下，它显示了名为 `code` 的共享文件夹。

2. **创建挂载点目录**：

   ```bash
   mkdir -p /mnt/hgfs
   ```

   - 这个命令创建了一个挂载点目录 `/mnt/hgfs`，如果目录已经存在则不会有任何效果。`-p` 选项确保目录的父目录也被创建（如果不存在）。

3. **挂载共享文件夹**：

   ```bash
   sudo vmhgfs-fuse .host:/ /mnt/hgfs -o subtype=vmhgfs-fuse,allow_other
   ```

   - 这个命令使用 `vmhgfs-fuse` 挂载 VMware 虚拟机中的共享文件夹到 `/mnt/hgfs` 目录。
   - `.host:/` 表示挂载主机的共享文件夹。
   - `-o subtype=vmhgfs-fuse,allow_other` 是挂载选项，`subtype` 指定了文件系统的类型，`allow_other` 允许其他用户访问挂载点。

4. **取消挂载共享文件夹**：

   ```bash
   umount /mnt/hgfs
   ```

   - 这个命令取消了之前挂载到 `/mnt/hgfs` 目录的文件系统。

5. **将挂载设置添加到 `/etc/fstab`**：

   ```bash
   .host:/ /mnt/hgfs fuse.vmhgfs-fuse defaults,allow_other 0 0
   ```

   - 这个命令将挂载配置添加到 `/etc/fstab` 文件中，以便在系统启动时自动挂载共享文件夹。
   - `fuse.vmhgfs-fuse` 是文件系统类型，`defaults,allow_other` 是挂载选项，`0 0` 表示不进行文件系统检查，也不进行备份。

总结：这些命令用于配置 VMware 的共享文件夹，使其在虚拟机中可用，并在系统启动时自动挂载。

## wine

- [Wine 开发者指导/架构概览](https://blog.csdn.net/Flora_xuan1993/article/details/89205922)

## Arch Linux

- [用 archinstall 自动化脚本安装 Arch Linux](https://linux.cn/article-14444-1.html)
- [Arch Linux图文安装教程（2022.08.01）](https://blog.csdn.net/love906897406/article/details/126109464)
