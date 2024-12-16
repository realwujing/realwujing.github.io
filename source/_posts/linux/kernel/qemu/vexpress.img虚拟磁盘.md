---
date: 2023/12/05 15:26:14
updated: 2023/12/05 15:26:14
---

# vexpress.img虚拟磁盘

## 制作虚拟磁盘

```bash
dd if=/dev/zero of=vexpress.img bs=512 count=$((2*1024*48))
```

```bash
qemu-img create -f raw vexpress.img 48M
```

这两种方法任选一种执行就可以，目的就是生成一个vexpress.img的虚拟镜像文件，为了更好的兼容性选择 raw 格式的镜像。

## 虚拟磁盘中创建分区并修改

1. 磁盘分区

    ```bash
    fdisk vexpress.img
    ```

    使用`n`命令创建分区，各种下一步就行。

2. 挂载vexpress.img到/dev/loop0设备上

    ```bash
    sudo losetup /dev/loop0 vexpress.img
    ```

3. 使用partx命令让系统刷新系统的分区信息

    ```bash
    sudo partx -u /dev/loop0
    ```

4. 制作ext4格式的文件系统

    ```bash
    sudo mkfs.ext4 /dev/loop0p1
    ```

5. 把vexpress.img的根文件系统分区挂载到img

    ```bash
    mkdir img
    sudo mount -o loop /dev/loop0p1 img
    ```

    执行到这里虚拟磁盘就已经制作好了，下面的两个步骤是卸载磁盘时的操作。

6. 卸载loop0设备下的分区

    ```bash
    sudo partx -d /dev/loop0
    ```

    如果执行不成功可以试试下方命令：

    ```bash
    sudo umount -f img
    ```

7. 卸载loop0设备

    ```bash
    sudo losetup -d /dev/loop0
    ```

执行完上述流程后，已成功制作出vexpress.img，可以使用书上命令测试：

```bash
sudo mount -o loop,offset=$((2048*512)) vexpress.img img
```

## More

- [Linux设备驱动开发详解：基于最新的Linux4.0内核.pdf](https://manongbook.com/linux/684.html)
- [第四期 QEMU调试Linux内核实验 《虚拟机就是开发板》](https://blog.csdn.net/aggresss/article/details/54946438)