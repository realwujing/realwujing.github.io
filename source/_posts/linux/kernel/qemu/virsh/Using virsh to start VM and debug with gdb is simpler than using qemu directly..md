---
date: 2024/06/22 11:17:36
updated: 2024/07/24 16:07:32
---

# Using virsh to start VM and debug with gdb is simpler than using qemu directly

## virt-install安装镜像

创建一个名为 yql-openeuler 的虚拟机，配置了适当的内存、CPU、磁盘、安装位置以及启动参数，以便正确连接到串口控制台和控制台输出从而在非图形化界面上正确安装虚拟机:

使用qemu-img创建虚拟机所需的磁盘：

```bash
qemu-img create -f qcow2 yql-openeuler.qcow2 200G
```

### x86架构

x86架构参考命令如下:

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

如果上述命令无法选定安装源，需要指定`inst.stage2=hd:LABEL`，参考命令如下：

```bash
virt-install \
  --name wujing \
  --ram 32768 \
  --vcpus 64 \
  --disk path=/inf/yql/wujing.qcow2,size=300 \
  --location /inf/yql/openeuler-22.06-230117-x86_64-dvd.iso \
  --os-type generic \
  --network default \
  --graphics none \
  --console pty,target_type=serial \
  -x 'console=ttyS0,115200n8 console=tty0' \
  --extra-args 'inst.stage2=hd:LABEL=openeuler-22.06-x86_64 console=ttyS0,115200n8'
```

### arm架构

arm架构参考命令如下:

```bash
#!/bin/bash

vm_name=yql
iso_path=/data/yql/openeuler-22.09-240618-aarch64-dvd.iso
hd_label=openeuler-22.09-aarch64

virt-install --virt-type kvm \
--name "$vm_name" \
--memory 32768 \
--vcpus=64 \
--location "$iso_path" \
--disk path="$vm_name.qcow2",size=200,format=qcow2 \
--network network=default \
--os-type=linux \
--os-variant=rhel7 \
--graphics none \
--extra-args="inst.stage2=hd:LABEL=$hd_label console=ttyS0"
```

`--debug`参数用于开启调试模式，这会提供更详细的输出，帮助开发者或用户诊断问题。

#### cnetos7 arm

```bash
qemu-img create -f qcow2 centos.qcow2 128G
```

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

```bash
root
F7!r9Bn2@kP
```

- [内核源码编译 安装](https://www.cnblogs.com/gefish/p/12029847.html)
- [https://vault.centos.org/7.6.1810/os/Source/SPackages/](https://vault.centos.org/7.6.1810/os/Source/SPackages/)

## 拷贝虚拟机的vmlinux、initramfs

将虚拟机上的vmlinux、initramfs复制到宿主机上:

```bash
rsync -avzP -e 'ssh -p 10000' /boot/initramfs-4.19.0-amd64-desktop.img /boot/vmlinuz-4.19.0-amd64-desktop root@10.63.8.158:/inf/yql/code/rpm
```

## virsh dumpxml

将虚拟机的xml文件复制到宿主机上:

```bash
virsh dumpxml yql-openeuler > yql-openeuler.xml
```

将自定义虚拟机启动vmlinux、initramfs、启动参数nokaslr且让虚拟机支持调试后的yql-openeuler 的xml文件也复制到宿主机上:

```bash
virsh dumpxml yql-openeuler > yql-openeuler3.xml
```

```bash
diff yql-openeuler.xml yql-openeuler3.xml
1c1
< <domain type='kvm'>
---
> <domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
11a12,14
>     <kernel>/inf/yql/code/rpm/vmlinuz-4.19.0-amd64-desktop</kernel>
>     <initrd>/inf/yql/code/rpm/initramfs-4.19.0-amd64-desktop.img</initrd>
>     <cmdline>root=/dev/mapper/openeuler-root rw console=ttyS0 nokaslr</cmdline>
---
130a134,136
>   <qemu:commandline>
>     <qemu:arg value='-S'/>
>     <qemu:arg value='-s'/>
>   </qemu:commandline>
```

主要改动点说明：

### 1. 更改 Domain 类型

```diff
1c1
< <domain type='kvm'>
---
> <domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
```

**解释**:

- 增加了 `xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'`。这意味着你可以在 XML 文件中使用一些特定于 QEMU 的扩展和命名空间。

### 2. 添加内核、initrd 和命令行参数

```diff
11a12,14
>     <kernel>/inf/yql/code/rpm/vmlinuz-4.19.0-amd64-desktop</kernel>
>     <initrd>/inf/yql/code/rpm/initramfs-4.19.0-amd64-desktop.img</initrd>
>     <cmdline>root=/dev/mapper/openeuler-root rw console=ttyS0 nokaslr</cmdline>
```

**解释**:

- `<kernel>`: 指定了内核映像的路径。
- `<initrd>`: 指定了初始内存盘（initramfs）的路径。
- `<cmdline>`: 指定了内核命令行参数，包括：
  - `root=/dev/mapper/openeuler-root`：指定根文件系统所在的设备。
  - `rw`：使根文件系统以读写模式挂载。
  - `console=ttyS0`：指定控制台输出到串口 `ttyS0`。
  - `nokaslr`：禁用内核地址空间布局随机化（KASLR）。

### 3. 添加 QEMU 命令行参数

```diff
130a134,136
>   <qemu:commandline>
>     <qemu:arg value='-S'/>
>     <qemu:arg value='-s'/>
>   </qemu:commandline>
```

**解释**:

- `<qemu:commandline>` 和 `<qemu:arg>`：允许你直接向 QEMU 传递命令行参数。
- `-S`：启动时暂停 CPU 执行，等待调试器连接和指令。
- `-s`：这是一个 QEMU 调试选项，启动 GDB 服务器监听在 TCP 端口 1234，允许你在虚拟机启动时通过 GDB 进行调试。

### 综合解释

这些改动主要有以下几个目的：

1. **指定内核和 initrd**：直接从指定的内核和 initrd 文件启动虚拟机，而不是使用默认的引导加载程序。这在需要使用自定义内核或 initrd 时特别有用。
2. **内核命令行参数**：设置内核启动时的参数，确保系统能够找到正确的根文件系统，并且将输出发送到串口（对于调试非常有用）。
3. **QEMU 调试选项**：通过 QEMU 的命令行选项启动 GDB 服务器，这对内核开发和调试非常有帮助。

通过这些改动，你可以更好地控制虚拟机的启动过程，并且能够在需要时进行调试。

## virsh start 启动虚拟机

在宿主机上启动虚拟机：

```bash
virsh start yql-openeuler
```

## gdb vmlinux

在宿主机上有linux源码目录下执行`gdb vmlinux`:

```bash
gdb vmlinux
```

![virsh start yql-openeuler gdb vmlinux target remote :1234](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240621194930.png)

## virsh domifaddr 查看虚拟机网卡地址

在宿主机上查看虚拟机网卡地址：

```bash
virsh domifaddr yql-openeuler
```

## ssh 虚拟机

在宿主机上ssh到虚拟机:

```bash
ssh wujing@192.168.122.46
```

![virsh domifaddr yql-openeuler ssh wujing@192.168.122.46](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240621194807.png)

## virsh console 打开虚拟机控制台

在宿主机上打开虚拟机控制台：

```bash
virsh console yql-openeuler
```

![gdb vmlinux target remote :1234 virsh console yql-openeuler](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240622092900.png)

既可以通过ssh到虚拟机，也可以通过virsh console打开虚拟机控制台。
