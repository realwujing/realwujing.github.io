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

### 使用 `virsh` 命令为虚拟机动态添加网络接口

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

## 显示虚拟化

- [QEMU显示虚拟化的几种选项](https://blog.csdn.net/tugouxp/article/details/134487575)
- [GPU虚拟化技术探索](https://mp.weixin.qq.com/s/LEVdZYLz62H_zwTthxuw7A)

- [FT2000+ openEuler 20.03 LTS SP3 yum install qemu手动创建虚拟机 图形界面安装openEuler 20.03系统](https://blog.csdn.net/hknaruto/article/details/130154674)
- [解决qemu-system-aarch64 Guest has not initialized the display (yet)](https://blog.csdn.net/hknaruto/article/details/127515804)

## KVM性能优化

- [KVM性能优化最佳实践](https://blog.csdn.net/allway2/article/details/102760738)
- [KVM 虚拟化基本原理](https://www.toutiao.com/article/6862880595601523211)

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
