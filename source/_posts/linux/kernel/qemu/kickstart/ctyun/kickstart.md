---
date: 2024/08/29 16:20:02
updated: 2024/08/29 16:20:02
---

# kickstart

```bash
virt-install --virt-type kvm \
--initrd-inject=/mnt/sdd1/yql/anaconda-ks.cfg \
--name wujing-ctyunos-22.06 \
--memory 32768 \
--vcpus=64 \
--location /mnt/sdd1/yql/ctyunos-22.06-230117-aarch64-dvd.iso \
--disk path=/mnt/sdd1/yql/wujing-ctyunos-22.06.qcow2,size=128,device=disk,bus=scsi \
--network network=default \
--os-type=linux \
--os-variant=centos7.0 \
--graphics none \
--extra-args="console=ttyS0 inst.ks=file:/anaconda-ks.cfg"
```

## /dev/sda

将虚拟磁盘指定为 SCSI 设备，并且在虚拟机中看到 `/dev/sda`，则需要了解 SCSI 和 IDE 设备在虚拟机中的行为：

- **IDE** 通常会显示为 `/dev/sda`，这对大多数系统是标准的。
- **SCSI** 设备通常显示为 `/dev/sdX`，其中 `X` 是设备标识符，通常是从 `a` 开始。

使用 `bus=scsi`，虚拟磁盘将被配置为 SCSI 设备，通常会显示为 `/dev/sda`。但如果系统已识别为其他 SCSI 设备（如 `/dev/sdb`），则可能需要调整 Kickstart 文件或其他配置以匹配实际设备名称。

### 使用 SCSI 配置虚拟磁盘

在 `virt-install` 命令中，指定 `bus=scsi` 可以将虚拟磁盘配置为 SCSI 设备。以下是如何使用 `bus=scsi` 的示例：

```bash
virt-install --virt-type kvm \
--initrd-inject=/mnt/sdd1/yql/ks.cfg \
--name wujing-zy-xfs \
--memory 32768 \
--vcpus=64 \
--location /mnt/sdd1/yql/ctyunos-2-24.07-240716-rc2-aarch64-dvd.iso \
--disk path=/mnt/sdd1/yql/wujing-zy-xfs.qcow2,size=128,device=disk,bus=scsi \
--network network=default \
--os-type=linux \
--os-variant=centos7.0 \
--graphics none \
--extra-args="console=ttyS0 inst.ks=file:/ks.cfg"
```

### 选项解释

- **`path=/mnt/sdd1/yql/wujing-zy-xfs.qcow2`**：磁盘映像的路径。
- **`size=128`**：指定磁盘的大小为 128GiB。
- **`device=disk`**：指定设备类型为磁盘。
- **`bus=scsi`**：将磁盘总线类型设置为 SCSI，这通常使磁盘设备在虚拟机中显示为 `/dev/sdX`。

### 验证设备名称

在虚拟机启动并安装后，你可以通过以下命令验证磁盘设备名称：

```bash
lsblk
```

如果设备名称不是你期望的 `/dev/sda`，请检查虚拟机的设备列表和其他配置文件，确保设备名称匹配。如果使用 `bus=scsi`，它通常显示为 `/dev/sdX`，而不是 `/dev/sda`。

### 修改 Kickstart 文件

确保 Kickstart 文件中的磁盘设备名称与你的虚拟机配置一致。例如，如果你使用 SCSI 设备，可能需要调整 Kickstart 文件中的设备名称：

```bash
# Disk partitioning information
part /boot/efi --fstype="efi" --ondisk=sda --size=200 --fsoptions="umask=0077,shortname=winnt"
part /boot --fstype="xfs" --ondisk=sda --size=1024
part pv.156 --fstype="lvmpv" --ondisk=sda --size=129846
volgroup ctyunos_ctyunos --pesize=4096 pv.156
logvol swap  --fstype="swap" --size=8064 --name=swap --vgname=ctyunos_ctyunos
logvol /  --fstype="xfs" --grow --maxsize=51200 --size=1024 --name=root --vgname=ctyunos_ctyunos
logvol /home  --fstype="xfs" --grow --size=500 --name=home --vgname=ctyunos_ctyunos
```

根据你的虚拟化环境和操作系统，选择合适的总线类型和配置，以确保磁盘设备名称符合你的需求。

### /dev/vda

要在虚拟机中将 `wujing-zy-xfs.qcow2` 映像指定为 `/dev/sda`，你需要确保虚拟机中的磁盘设备名称与你在 Kickstart 文件中指定的设备名称一致。`virt-install` 默认情况下可能会使用 `vda` 或其他设备名称，因此你需要通过以下步骤进行配置：

### 1. 配置磁盘设备名称

在 `virt-install` 命令中，可以使用 `--disk` 参数来设置磁盘设备名称。你可以通过在 Kickstart 文件中指定正确的设备名称来确保虚拟机使用 `/dev/sda`。

### 2. 修改 Kickstart 文件

确保 Kickstart 文件中使用的设备名称与你在 `virt-install` 中指定的设备名称一致。如果你的 Kickstart 文件使用 `vda`，则需要将其修改为 `sda`。

### 3. 使用 `virt-install` 创建虚拟机

确保在虚拟机配置中设置正确的磁盘设备名称。你可以通过 `virt-install` 的 `--disk` 参数设置磁盘设备名称。下面是如何确保磁盘设备名称设置为 `sda` 的示例：

```bash
virt-install --virt-type kvm \
--initrd-inject=/mnt/sdd1/yql/ks1.cfg \
--name wujing-zy-xfs \
--memory 32768 \
--vcpus=64 \
--location /mnt/sdd1/yql/ctyunos-2-24.07-240716-rc2-aarch64-dvd.iso \
--disk path=/mnt/sdd1/yql/wujing-zy-xfs.qcow2,size=128,device=disk,bus=virtio \
--network network=default \
--os-type=linux \
--os-variant=centos7.0 \
--graphics none \
--extra-args="console=ttyS0 inst.ks=file:/ks1.cfg"
```

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

### 4. 验证磁盘设备名称

如果虚拟机创建成功后，你可以通过以下命令验证磁盘设备名称是否正确：

```bash
lsblk
```

确保输出中的磁盘设备名称与你在 Kickstart 文件中指定的名称一致。

### 5. 修改 Kickstart 文件

确保你的 Kickstart 文件使用 `/dev/sda` 作为设备名称：

```bash
# Disk partitioning information
part /boot/efi --fstype="efi" --ondisk=sda --size=200 --fsoptions="umask=0077,shortname=winnt"
part /boot --fstype="xfs" --ondisk=sda --size=1024
part pv.156 --fstype="lvmpv" --ondisk=sda --size=129846
volgroup ctyunos_ctyunos --pesize=4096 pv.156
logvol swap  --fstype="swap" --size=8064 --name=swap --vgname=ctyunos_ctyunos
logvol /  --fstype="xfs" --grow --maxsize=51200 --size=1024 --name=root --vgname=ctyunos_ctyunos
logvol /home  --fstype="xfs" --grow --size=500 --name=home --vgname=ctyunos_ctyunos
```

通过上述步骤，你可以确保虚拟机磁盘设备名称与 Kickstart 文件中指定的一致。如果在虚拟机中出现其他设备名称（如 `vda`），则需要在 Kickstart 文件中相应地调整设备名称。

## virtio与scsi对比

- **`virtio`**：通常效率更高，提供更好的性能，特别适合高性能 I/O 需求。需要虚拟化主机和操作系统支持 `virtio` 驱动。

- **`scsi`**：性能较 `virtio` 稍低，但提供较好的兼容性，适合需要模拟传统硬件的环境。
