---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# s4 cannot go to sleep

236037

## 正常机器

```bash
cd ~/Downloads/sjgd20231215001/sjgd20231215001/没问题机器all_log_files_20231213_10：35/all_log_files/kernel
```

```bash
 19 2023-12-05 14:32:51 T-KT013M30 kernel: [    0.000000] Linux version 4.19.0-amd64-desktop (uos@x86-compile-PC) (gcc version 8.3.0 (Uos 8.3.0.5-1+dde)) #5317 SMP Mon Feb 6 16:04:31 CST 2023                     
  20 2023-12-05 14:32:51 T-KT013M30 kernel: [    0.000000] Command line: BOOT_IMAGE=/vmlinuz-4.19.0-amd64-desktop root=UUID=2bca42f1-05dc-42b8-b3ee-567e1ced9ba7 ro splash quiet DEEPIN_GFXMODE= ima_appraise=off lib     ahci.ignore_sss=1
```

```bash
sed -n '1119,1224'p kern.log > pm.log
```

```bash
1155 2023-12-06 14:26:42 T-KT013M30 kernel: [28643.179825] ACPI: Preparing to enter system sleep state S4
1156 2023-12-06 14:26:42 T-KT013M30 kernel: [28643.179912] PM: Saving platform NVS memory
```

NVS（Non-Volatile Storage）内存不是指硬盘。NVS内存是一种特殊类型的非易失性存储器，它用于存储一些系统配置和状态信息，以便在系统休眠或关机期间保持数据的持久性。

NVS内存通常是集成在计算机的主板上或与主板紧密相关的芯片中。它用于存储一些与硬件配置、电源管理、系统状态等相关的数据。这些数据在系统重新启动后可以被恢复，以确保系统的连续性和一致性。

与硬盘不同，NVS内存的容量通常较小，它主要用于存储一些关键的系统信息，以便在系统休眠或关机期间使用。硬盘是一种用于存储大量数据的存储设备，它通常用于长期保存文件、应用程序和操作系统等数据。

因此，NVS内存和硬盘是不同的存储介质，用于不同的目的。

S4是系统休眠状态，也称为挂起到磁盘（Suspend to Disk）或休眠到磁盘（Hibernate）。在S4状态下，系统的状态将被保存到硬盘或其他非易失性存储介质中，以便在下次启动时能够恢复到先前的状态。

在S4睡眠状态中，系统会将当前的内存状态写入到硬盘上的特殊文件（通常是称为休眠文件或交换文件）中。这个休眠文件会记录当前的内存内容，包括打开的应用程序、文件和系统状态等。当系统重新启动时，它会读取休眠文件，并将内存状态恢复到先前保存的状态，从而实现快速恢复到休眠前的状态。

因此，S4睡眠状态涉及将系统的内存状态写入到硬盘或其他非易失性存储介质中，而不是将数据存储到NVS内存或硬盘中。对于休眠到磁盘的操作，数据会存储在硬盘上的特殊文件中，以便在下次启动时进行恢复。

## 异常机器

```bash
cd ~/Downloads/sjgd20231215001/sjgd20231215001/问题机all_log_files_20231213_0958&1010/all_log_files/kernel

```bash
 220 2023-12-04 08:26:02 T-KT0123SY kernel: [    0.000000] Linux version 4.19.0-amd64-desktop (uos@x86-compile-PC) (gcc version 8.3.0 (Uos 8.3.0.5-1+dde)) #5317 SMP Mon Feb 6 16:04:31 CST 2023                    
  221 2023-12-04 08:26:02 T-KT0123SY kernel: [    0.000000] Command line: BOOT_IMAGE=/vmlinuz-4.19.0-amd64-desktop root=UUID=419ef5d7-3a1c-4c9e-a35d-3bee7483cfc8 ro splash quiet DEEPIN_GFXMODE= ima_appraise=off li      bahci.ignore_sss=1  
```

```bash
sed -n '16130,16141'p kern.log > pm_error.log
```

异常机器在睡下去执行到`PM: Syncing filesystems ... `就结束了。

## 继续排查方向

grub中增加下方参数：

- loglevel=8
- initcall_debug
- no_console_suspend
- console=ttyS0,115200

`console=ttyS0,115200`接串口用，ttyS0请用cutecom确认。

update-grub后重启机器

执行`sudo systemctl hibernate`

再次进入桌面将`/var/log/`目录发来。
