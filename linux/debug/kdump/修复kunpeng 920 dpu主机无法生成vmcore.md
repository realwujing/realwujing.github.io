# 修复kunpeng 920 dpu主机无法生成vmcore

## crashkernel

本批次机器无法生成vmcore文件，部分原因是crashkernel配置错误。

本批次机器需修改`crashkernel=512M`，其他机器可能需要配置为`crashkernel=1024M,high`或其他参数。

vim /etc/default/grub，确保crashkernel参数如下：
```bash
crashkernel=512M
```

重新生成grub.cfg:

```bash
grub2-mkconfig -o /boot/grub2/grub.cfg
```

```bash
grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg
```

重启：
```bash
reboot
```

## kdump中oom修复

kengpeng 920 dpu host主机生成kdump时会oom，需要在kdump启用第二内核时禁用某些驱动。

vim /etc/sysconfig/kdump，在KDUMP_COMMANDLINE_APPEND=参数后面追加参数禁用某些驱动:

```bash
KDUMP_COMMANDLINE_APPEND="initcall_blacklist=mlx5_ib_init,mlx5_core_init module_blacklist=mlx5_ib,mlx5_core modprobe.blacklist=hisi_sas_v3_hw,hisi_sas_main"
```

重启kdump服务：
```bash
systemctl restart kdump.service
```

## 魔法键c生成vmcore

使用魔法键c测试是否能生成vmcore。

使能魔法键c:

```bash
echo 1 > /proc/sys/kernel/sysrq
```

魔法键c主动生成kdump:

```bash
echo c > /proc/sysrq-trigger
```

执行完上述命令后，机器会重启，重启后会发现/var/crash目录中已经有vmcore生成：

```bash
[root@npu-910b-7dcb9 127.0.0.1-2025-06-20-15:31:46]# ls /var/crash/127.0.0.1-2025-06-20-15\:31\:46/
vmcore vmcore-dmesg.txt
```
