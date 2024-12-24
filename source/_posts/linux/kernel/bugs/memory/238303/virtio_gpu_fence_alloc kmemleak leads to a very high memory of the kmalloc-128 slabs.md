---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# virtio_gpu_fence_alloc kmemleak leads to a very high memory of the kmalloc-128 slabs

## 环境

### 宿主机

- cpu: 飞腾D2000
- os: [uos20-1060-arm64](https://cdimage.uniontech.com/iso-v20/desktop-professional/1060/uos-desktop-20-professional-1060-arm64.iso)
- virt-manager: 1:2.0.0.6-1+eagle

  ```bash
  apt policy virt-manager
  virt-manager:
    已安装：1:2.0.0.6-1+eagle
    候选： 1:2.0.0.6-1+eagle
    版本列表：
  *** 1:2.0.0.6-1+eagle 500
          500 https://professional-packages.chinauos.com/desktop-professional eagle/main arm64 Packages
          500 https://professional-security.chinauos.com eagle/1060/main arm64 Packages
          100 /usr/lib/dpkg-db/status
  ```

![host](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/截图_选择区域_20240126141611.png)

### 虚拟机

- cpu: kvm
- os: [uos20-1060-arm64](https://cdimage.uniontech.com/iso-v20/desktop-professional/1060/uos-desktop-20-professional-1060-arm64.iso)

![guest-spice](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/截图_选择区域_20240126141808.png)
![guest-virtio](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/截图_选择区域_20240126141820.png)

#### 虚拟机初始化报错

![Failed to create /var/lib/libvirt/.cache for shader cache (Permission denied)](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/截图_选择区域_20240126115617.png)

```text
无法完成安装：'internal error: process exited while connecting to monitor: Failed to create /var/lib/libvirt/.cache for shader cache (Permission denied)---disabling.'

Traceback (most recent call last):

  File "/usr/share/virt-manager/virtManager/asyncjob.py", line 75, in cb_wrapper

    callback(asyncjob, *args, **kwargs)

  File "/usr/share/virt-manager/virtManager/create.py", line 2121, in _do_async_install

    guest.installer_instance.start_install(guest, meter=meter)

  File "/usr/share/virt-manager/virtinst/installer.py", line 419, in start_install

    doboot, transient)

  File "/usr/share/virt-manager/virtinst/installer.py", line 362, in _create_guest

    domain = self.conn.createXML(install_xml or final_xml, 0)

  File "/usr/lib/python3/dist-packages/libvirt.py", line 3732, in createXML

    if ret is None:raise libvirtError('virDomainCreateXML() failed', conn=self)

libvirt.libvirtError: internal error: process exited while connecting to monitor: Failed to create /var/lib/libvirt/.cache for shader cache (Permission denied)---disabling.

```

#### 修复方案

在宿主机上执行:

```bash
sudo -s
echo "seccomp_sandbox = 0"  >> /etc/libvirt/qemu.conf
sudo systemctl daemon-reload
sudo systemctl restart libvirtd
```

- [Bug 1659484 - Failed to create /var/lib/libvirt/.cache for shader cache (Permission denied)](https://bugzilla.redhat.com/show_bug.cgi?id=1659484#c9)
- [Debian Bug report logs - #916441 Failed to create /var/lib/libvirt/.cache for shader cache (Permission denied)](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=916441)

## virtio_gpu virgl

虚拟机上安装：

```bash
sudo apt install mesa-utils
```

虚拟机上执行测试命令：

```bash
vblank_mode=0 glxgears
```

vblank_mode=0 告诉图形卡忽略显示器的刷新率，仅尝试达到其关闭功能的maximux fps。

查看虚拟机系统的 OpenGL 配置和性能：

```bash
glxinfo -B | head -n20
```

```bash
name of display: :0
display: :0  screen: 0
direct rendering: Yes
Extended renderer info (GLX_MESA_query_renderer):
    Vendor: Red Hat (0x1af4)
    Device: virgl (0x1010)
    Version: 19.2.6
    Accelerated: yes
    Video memory: 0MB
    Unified memory: no
    Preferred profile: core (0x1)
    Max core profile version: 4.3
    Max compat profile version: 3.1
    Max GLES1 profile version: 1.1
    Max GLES[23] profile version: 3.1
OpenGL vendor string: Red Hat
OpenGL renderer string: virgl
OpenGL core profile version string: 4.3 (Core Profile) Mesa 19.2.6
OpenGL core profile shading language version string: 4.30
OpenGL core profile context flags: (none)
```

可以看到虚拟机中已经启用了virtio 3D加速。

- [Linux下2D、3D的测试软件glxgears](https://blog.csdn.net/tongxin1101124/article/details/123235465)

## 初步分析

### Slab

```bash
cat /proc/meminfo | grep Slab: -A2
Slab:             434440 kB
SReclaimable:     197732 kB
SUnreclaim:       236708 kB
```

### slabtop

```bash
cat /proc/slabinfo
cat: /proc/slabinfo: 没有那个文件或目录
```

故需要替换虚拟机内核为打开slub_debug、kmemleak的版本：

```bash
sudo apt install ./linux-image-4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak_4.19.90-5819_arm64.deb
```

安装包含slabtop的工具：

```bash
sudo apt install procps
```

```bash
slabtop -s c -o | head -n20
 Active / Total Objects (% used)    : 729456 / 769288 (94.8%)
 Active / Total Slabs (% used)      : 34804 / 34804 (100.0%)
 Active / Total Caches (% used)     : 125 / 192 (65.1%)
 Active / Total Size (% used)       : 443220.71K / 474643.98K (93.4%)
 Minimum / Average / Maximum Object : 0.34K / 0.62K / 12.00K

  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME                   
381050 375070  98%    0.62K  15242       25    243872K kmemleak_object        
150484 150259  99%    0.45K   8852       17     70816K kernfs_node_cache      
 51350  50337  98%    0.62K   2054       25     32864K kmalloc-128            
 40455  37247  92%    0.52K   1305       31     20880K dentry                 
 14145  13303  94%    1.38K    615       23     19680K ext4_inode_cache       
 13855  13235  95%    0.91K    815       17     13040K inode_cache            
 17732  14700  82%    0.52K    572       31      9152K vm_area_struct         
  7701   7099  92%    0.89K    453       17      7248K radix_tree_node        
  5728   5054  88%    0.98K    358       16      5728K proc_inode_cache       
  1085    731  67%    4.50K    155        7      4960K kmalloc-4096           
  6877   3917  56%    0.69K    299       23      4784K filp                   
  9180   8695  94%    0.47K    540       17      4320K buffer_head            
 10540   8478  80%    0.39K    527       20      4216K anon_vma_chain
```

### kmemleak

打开kmemlean扫描功能：

```bash
echo scan > /sys/kernel/debug/kmemleak
```

查看kmemleak结果：

```bash
cat /sys/kernel/debug/kmemleak
```

virtio_gpu 存在内存泄露：

```bash
unreferenced object 0xffff8000b0d4c800 (size 128):
  comm "Xorg", pid 723, jiffies 4294896491 (age 361.400s)
  hex dump (first 32 bytes):
    01 00 00 00 00 00 00 00 a8 c0 ec 08 00 00 ff ff  ................
    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
  backtrace:
    [<00000000dce2d1d0>] kmem_cache_alloc_trace+0x140/0x270
    [<000000009a6c7a12>] virtio_gpu_fence_alloc+0x28/0x68
    [<00000000fa64f478>] virtio_gpu_execbuffer_ioctl+0x2a0/0x400
    [<0000000093a38e00>] drm_ioctl_kernel+0x90/0xe8
    [<00000000653c7660>] drm_ioctl+0x1c0/0x398
    [<00000000cc088f2c>] do_vfs_ioctl+0xa4/0x858
    [<00000000baad8f0f>] ksys_ioctl+0x78/0xa8
    [<0000000075e1d8b3>] __arm64_sys_ioctl+0x1c/0x28
    [<000000006fee0928>] el0_svc_common+0x90/0x178
    [<0000000049219d6c>] el0_svc_handler+0x9c/0xa8
    [<0000000056df9638>] el0_svc+0x8/0xc
    [<00000000a96bfbe3>] 0xffffffffffffffff
```

### slub_debug

多台机器反馈kmalloc-128 slab对象占用内存最多，slub_debug可以打开来查看：

```bash
echo 1 > /sys/kernel/slab/kmalloc-128/trace  && sleep 30 && echo 0 > /sys/kernel/slab/kmalloc-128/trace
```

#### kmalloc-128 alloc

```bash
2024-01-26 15:04:11 uos-PC kernel: [  634.493181] TRACE kmalloc-128 alloc 0x000000006069c5b0 inuse=25 fp=0x          (null)
2024-01-26 15:04:11 uos-PC kernel: [  634.493185] CPU: 3 PID: 723 Comm: Xorg Tainted: G           O      4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819
2024-01-26 15:04:11 uos-PC kernel: [  634.493187] Hardware name: QEMU KVM Virtual Machine, BIOS 0.0.0 02/06/2015
2024-01-26 15:04:11 uos-PC kernel: [  634.493189] Call trace:
2024-01-26 15:04:11 uos-PC kernel: [  634.493192]  dump_backtrace+0x0/0x190
2024-01-26 15:04:11 uos-PC kernel: [  634.493194]  show_stack+0x14/0x20
2024-01-26 15:04:11 uos-PC kernel: [  634.493197]  dump_stack+0xa8/0xcc
2024-01-26 15:04:11 uos-PC kernel: [  634.493200]  alloc_debug_processing+0x58/0x188
2024-01-26 15:04:11 uos-PC kernel: [  634.493203]  ___slab_alloc.constprop.34+0x31c/0x388
2024-01-26 15:04:11 uos-PC kernel: [  634.493206]  __kmalloc+0x284/0x2e0
2024-01-26 15:04:11 uos-PC kernel: [  634.493209]  alloc_indirect.isra.2+0x1c/0x50
2024-01-26 15:04:11 uos-PC kernel: [  634.493212]  virtqueue_add_sgs+0xa4/0x488
2024-01-26 15:04:11 uos-PC kernel: [  634.493217]  virtio_gpu_queue_ctrl_buffer_locked+0xc8/0x1f0
2024-01-26 15:04:11 uos-PC kernel: [  634.493221]  virtio_gpu_queue_fenced_ctrl_buffer+0xec/0x120
2024-01-26 15:04:11 uos-PC kernel: [  634.493225]  virtio_gpu_cmd_submit+0x98/0xb0
2024-01-26 15:04:11 uos-PC kernel: [  634.493228]  virtio_gpu_execbuffer_ioctl+0x2e4/0x400
2024-01-26 15:04:11 uos-PC kernel: [  634.493232]  drm_ioctl_kernel+0x90/0xe8
2024-01-26 15:04:11 uos-PC kernel: [  634.493236]  drm_ioctl+0x1c0/0x398
2024-01-26 15:04:11 uos-PC kernel: [  634.493240]  do_vfs_ioctl+0xa4/0x858
2024-01-26 15:04:11 uos-PC kernel: [  634.493243]  ksys_ioctl+0x78/0xa8
2024-01-26 15:04:11 uos-PC kernel: [  634.493247] TRACE kmalloc-128 alloc 0x00000000c1c7d55e inuse=25 fp=0x          (null)
2024-01-26 15:04:11 uos-PC kernel: [  634.493251]  __arm64_sys_ioctl+0x1c/0x28
2024-01-26 15:04:11 uos-PC kernel: [  634.493256]  el0_svc_common+0x90/0x178
2024-01-26 15:04:11 uos-PC kernel: [  634.493259]  el0_svc_handler+0x9c/0xa8
2024-01-26 15:04:11 uos-PC kernel: [  634.493262]  el0_svc+0x8/0xc
```

#### kmalloc-128 free

```bash
2024-01-26 15:04:11 uos-PC kernel: [  634.494274] TRACE kmalloc-128(522:accounts-daemon.service) free 0x0000000085d7b43a inuse=20 fp=0x0000000024f7d5b5
2024-01-26 15:04:11 uos-PC kernel: [  634.494279] Object 0000000085d7b43a: 80 80 2c d4 00 80 ff ff 80 80 2c d4 00 80 ff ff  ..,.......,.....
2024-01-26 15:04:11 uos-PC kernel: [  634.494284] Workqueue: events virtio_gpu_dequeue_ctrl_func
2024-01-26 15:04:11 uos-PC kernel: [  634.494290] Object 000000004798057d: 70 91 3d f0 00 80 ff ff 02 00 00 08 6b 6b 6b 6b  p.=.........kkkk
2024-01-26 15:04:11 uos-PC kernel: [  634.494293] Call trace:
2024-01-26 15:04:11 uos-PC kernel: [  634.494296] Object 00000000dcf382e6: 02 00 00 00 00 00 00 00 06 00 00 00 73 79 73 6c  ............sysl
2024-01-26 15:04:11 uos-PC kernel: [  634.494300]  dump_backtrace+0x0/0x190
2024-01-26 15:04:11 uos-PC kernel: [  634.494303] Object 00000000edcab07b: 6f 67 00 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b  og.kkkkkkkkkkkkk
2024-01-26 15:04:11 uos-PC kernel: [  634.494308]  show_stack+0x14/0x20
2024-01-26 15:04:11 uos-PC kernel: [  634.494310] Object 0000000053f635bd: 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b  kkkkkkkkkkkkkkkk
2024-01-26 15:04:11 uos-PC kernel: [  634.494312] Object 000000000e1b3528: 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b  kkkkkkkkkkkkkkkk
2024-01-26 15:04:11 uos-PC kernel: [  634.494317]  dump_stack+0xa8/0xcc
2024-01-26 15:04:11 uos-PC kernel: [  634.494319] Object 0000000052bf950e: 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b  kkkkkkkkkkkkkkkk
2024-01-26 15:04:11 uos-PC kernel: [  634.494324]  free_debug_processing+0x19c/0x3a0
2024-01-26 15:04:11 uos-PC kernel: [  634.494326] Object 00000000b74c6682: 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b 6b a5  kkkkkkkkkkkkkkk.
2024-01-26 15:04:11 uos-PC kernel: [  634.494331]  __slab_free+0x230/0x3f8
2024-01-26 15:04:11 uos-PC kernel: [  634.494335]  kfree+0x1d8/0x200
2024-01-26 15:04:11 uos-PC kernel: [  634.494340]  detach_buf+0x100/0x140
2024-01-26 15:04:11 uos-PC kernel: [  634.494345]  virtqueue_get_buf_ctx+0x80/0x170
2024-01-26 15:04:11 uos-PC kernel: [  634.494352]  virtqueue_get_buf+0x10/0x18
2024-01-26 15:04:11 uos-PC kernel: [  634.494358]  reclaim_vbufs+0x40/0x80
2024-01-26 15:04:11 uos-PC kernel: [  634.494364]  virtio_gpu_dequeue_ctrl_func+0x6c/0x248
2024-01-26 15:04:11 uos-PC kernel: [  634.494370]  process_one_work+0x1e8/0x438
2024-01-26 15:04:11 uos-PC kernel: [  634.494375]  worker_thread+0x48/0x4c8
2024-01-26 15:04:11 uos-PC kernel: [  634.494379]  kthread+0x128/0x130
2024-01-26 15:04:11 uos-PC kernel: [  634.494384]  ret_from_fork+0x10/0x18
```

### 编译Xorg deb调试包

slub_debug中`kmalloc-128 alloc`、`kmalloc-128 free`的栈回溯中出现了有关virtio_gpu的调用，基本上确认virtio_gpu存在内存泄露。

查看`kmalloc-128 alloc`中提到的`Comm: Xorg`所属deb包，进一步安装对应调试包：

```bash
dpkg -S Xorg
xserver-xorg-core: /usr/bin/Xorg
xserver-xorg-core: /usr/lib/xorg/Xorg
xserver-xorg-core: /usr/share/man/man1/Xorg.1.gz
```

```bash
apt policy xserver-xorg-core
xserver-xorg-core:
  已安装：2:1.20.4.65-1+dde
  候选： 2:1.20.4.65-1+dde
  版本列表：
 *** 2:1.20.4.65-1+dde 100
        100 /usr/lib/dpkg-db/status
```

当前仓库中没有包含`xserver-xorg-core`的调试包：

```bash
apt policy xserver-xorg-core 
Display all 1824 possibilities? (y or n)
```

查看/etc/apt/sources.list：

```bash
cat /etc/apt/sources.list
## Generated by deepin-installer
deb https://professional-packages.chinauos.com/desktop-professional eagle main contrib non-free
#deb-src https://professional-packages.chinauos.com/desktop-professional eagle main contrib non-free
```

查看/etc/product-info：

```bash
cat /etc/product-info 
SystemName=UOS Desktop
EditionName=Professional
MajorVersion=20
MinorVersion=1060
ProductName=professional
BuildTime=20230706115222
Arch=aarch64
OEM=
```

```bash
apt source xorg-server
正在读取软件包列表... 完成
提示：xorg-server 的打包工作被维护于以下位置的 Git 版本控制系统中：
https://salsa.debian.org/xorg-team/xserver/xorg-server.git
请使用：
git clone https://salsa.debian.org/xorg-team/xserver/xorg-server.git
获得该软件包的最近更新(可能尚未正式发布)。
需要下载 5,247 kB 的源代码包。
获取:1 https://professional-packages.chinauos.com/desktop-professional eagle/main xorg-server 2:1.20.4.97-1+dde (diff) [129 kB]
获取:2 https://professional-packages.chinauos.com/desktop-professional eagle/main xorg-server 2:1.20.4.97-1+dde (dsc) [3,439 B]
获取:3 https://professional-packages.chinauos.com/desktop-professional eagle/main xorg-server 2:1.20.4.97-1+dde (tar) [5,114 kB]
已下载 5,247 kB，耗时 1秒 (7,585 kB/s) 
dpkg-source: info: extracting xorg-server in xorg-server-1.20.4.97
dpkg-source: info: unpacking xorg-server_1.20.4.97.orig.tar.xz
dpkg-source: info: unpacking xorg-server_1.20.4.97-1+dde.debian.tar.xz
```

`xorg-server-1.20.4.97`跟`xserver-xorg-core 2:1.20.4.65-1+dde`对不上，说明仓库中xorg-server源码与xserver-xorg-core源码不一致。

联系窗管研发，得到对应源码并编译出deb包（包含deb调试包）：

<https://gerrit.uniontech.com/c/base/xorg-server/+/202758>

```bash
mkdir xorg-server-1.20.4.65
cd xorg-server-1.20.4.65
git clone "http://gerrit.uniontech.com/base/xorg-server"
git checkout -b uos20-1060 43b8a07
mv xorg-server xorg-server-1.20.4.65
dh_make --createorig -sy    # 生成debian目录
dpkg-source -b .    # 生成构建源代码包
dpkg-buildpackage -uc -us -j4   # 编译制作deb包
cd ..
sudo apt install ./*.deb   # 安装deb包，包含deb调试包
```

### trace-bpfcc

```bash
sudo apt install bpftrace bpfcc-tools libgl1-mesa-dri-dbgsym
```

```bash
trace-bpfcc -tKU virtio_gpu_fence_alloc | tee virtio_gpu_fence_alloc.log
```

输出如下：

```bash
37.52421 10061   10061   Xorg            virtio_gpu_fence_alloc 
        virtio_gpu_fence_alloc+0x0 [kernel]
        drm_ioctl_kernel+0x90 [kernel]
        drm_ioctl+0x1c0 [kernel]
        do_vfs_ioctl+0xa4 [kernel]
        ksys_ioctl+0x78 [kernel]
        __arm64_sys_ioctl+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        __GI___ioctl+0xc [libc-2.28.so]
        virgl_bo_transfer_put+0x78 [virtio_gpu_dri.so]
        transfer_put+0x3c [virtio_gpu_dri.so]
        perform_action.isra.3+0x6c [virtio_gpu_dri.so]
        virgl_transfer_queue_clear+0xa8 [virtio_gpu_dri.so]
        virgl_flush_eq.isra.4+0x64 [virtio_gpu_dri.so]
        st_glFlush+0x24 [virtio_gpu_dri.so]
        present_execute_copy+0x8c [Xorg]
        present_execute+0x130 [Xorg]
        present_scmd_pixmap+0x300 [Xorg]
        proc_present_pixmap+0x1a4 [Xorg]
        Dispatch+0x340 [Xorg]
        dix_main+0x388 [Xorg]
        __libc_start_main+0xe4 [libc-2.28.so]
        [unknown] [Xorg]
```

```bash
ps aux | grep -i xorg
root     10061  1.0  1.4 201428 58952 tty1     Ssl+ 16:56   0:03 /usr/lib/xorg/Xorg -background none :0 -seat seat0 -auth /var/run/lightdm/root/:0 -nolisten tcp vt1 -novtswitch
uos      12130  0.0  0.0  13724   648 pts/6    S+   17:02   0:00 grep -i xorg
```

```bash
cd xorg-server-1.20.4.65/xorg-server-1.20.4.65
sudo gdb -p 10061
GNU gdb (Uos 8.2.1.1-1+security) 8.2.1
Copyright (C) 2018 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "aarch64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word".
Attaching to process 10061
[New LWP 10091]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".
0x0000ffff92739d70 in __GI_epoll_pwait (epfd=<optimized out>, events=events@entry=0xfffff150d758, maxevents=maxevents@entry=256, timeout=<optimized out>, set=set@entry=0x0)
    at ../sysdeps/unix/sysv/linux/epoll_pwait.c:42
42      ../sysdeps/unix/sysv/linux/epoll_pwait.c: 没有那个文件或目录.
(gdb) b WaitForSomething
Breakpoint 1 at 0x592ff0: file ../../../../os/WaitFor.c, line 167.
(gdb) c
Continuing.

Thread 1 "Xorg" hit Breakpoint 1, WaitForSomething (are_ready=0) at ../../../../os/WaitFor.c:167
warning: Source file is more recent than executable.
167     {
(gdb) bt
#0  WaitForSomething (are_ready=0) at ../../../../os/WaitFor.c:167
#1  0x00000000004453bc in Dispatch () at ../../../../include/list.h:220
#2  0x0000000000449700 in dix_main (argc=12, argv=0xfffff150ea48, envp=0xfffff150eab0) at ../../../../dix/main.c:276
#3  0x0000ffff9268ada4 in __libc_start_main (main=0x432748 <main>, argc=12, argv=0xfffff150ea48, init=<optimized out>, fini=<optimized out>, rtld_fini=<optimized out>, stack_end=<optimized out>)
    at ../csu/libc-start.c:308
#4  0x0000000000432798 in _start ()
Backtrace stopped: previous frame identical to this frame (corrupt stack?)
(gdb) l
162      *     pClientsReady is an array to store ready client->index values into.
163      *****************/
164
165     Bool
166     WaitForSomething(Bool are_ready)
167     {
168         int i;
169         int timeout;
170         int pollerr;
171         static Bool were_ready;
```

```bash
// cd xorg-server-1.20.4.65/xorg-server-1.20.4.65
// vim os/WaitFor.c +201
165 Bool
166 WaitForSomething(Bool are_ready)
167 {
168     int i;
169     int timeout;
170     int pollerr;
171     static Bool were_ready;
172     Bool timer_is_running;
173 
174     timer_is_running = were_ready;
175 
176     if (were_ready && !are_ready) {
177         timer_is_running = FALSE;
178         SmartScheduleStopTimer();
179     }
180 
181     were_ready = FALSE;
182 
183 #ifdef BUSFAULT
184     busfault_check();
185 #endif
186 
187     /* We need a while loop here to handle
188        crashed connections and the screen saver timeout */
189     while (1) {
190         /* deal with any blocked jobs */
191         if (workQueue) {
192             ProcessWorkQueue();
193         }
194 
195         timeout = check_timers();
196         are_ready = clients_are_ready();
197 
198         if (are_ready)
199             timeout = 0;
200 
201         BlockHandler(&timeout); // 这里进一步往下调用，最终调用内核代码virtio_gpu_fence_alloc
202         if (NewOutputPending)
203             FlushAllOutput();
204         /* keep this check close to select() call to minimize race */
205         if (dispatchException)
206             i = -1;
```

## virtio_gpu_fence_alloc源码溯源

```bash
git show 00f20720be71
commit 00f20720be7137c1355f0cd1372779be34043ce5
Author: Robert Foss <robert.foss@collabora.com>
Date:   Mon Nov 12 17:51:54 2018 +0100

    drm/virtio: add virtio_gpu_alloc_fence()
    
    Refactor fence creation, add fences to relevant GPU
    operations and add cursor helper functions.
    
    This removes the potential for allocation failures from the
    cmd_submit and atomic_commit paths.
    Now a fence will be allocated first and only after that
    will we proceed with the rest of the execution.
    
    Signed-off-by: Gustavo Padovan <gustavo.padovan@collabora.com>
    Signed-off-by: Robert Foss <robert.foss@collabora.com>
    Link: http://patchwork.freedesktop.org/patch/msgid/20181112165157.32765-2-robert.foss@collabora.com
    Suggested-by: Rob Herring <robh@kernel.org>
    Signed-off-by: Gerd Hoffmann <kraxel@redhat.com>
```

`virtio_gpu_fence_alloc`首次出现在内核4.20版本中，应该是往下迁移的。

## 修复方案

![dma_fence_put](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/dma_fence_put.png)

- [[v4] dma-buf: Rename struct fence to dma_fence](https://patchwork.freedesktop.org/patch/118020/)
