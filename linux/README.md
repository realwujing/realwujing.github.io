# Linux 内核开发与系统技术文档

对标 Linux 内核源码树，按子系统组织。

## kernel/ — 内核核心

| [sched/](kernel/sched/) | 进程调度 — CFS、isolcpus、systemd、[bugs](kernel/sched/bugs/) |
|---|---|
| [locking/](kernel/locking/) | 锁机制 — spin_lock 变体、hard lockup 分析 |
| [rcu/](kernel/rcu/) | RCU — stall 分析、demo 示例 |
| [irq/](kernel/irq/) | 中断 — 8250 UAF、SDEI hard lockup、[tickless](kernel/irq/tick/tickless/) |
| [trace/](kernel/trace/) | 追踪 & 性能 — [bpf](kernel/trace/bpf/)、[events](kernel/trace/events/)、[stap](kernel/trace/stap/)、[上游 patch](kernel/trace/bpf/patch/) |
| [sources/](kernel/sources/) | Linux 0.11 源码分析 |
| [kernel.md](kernel/kernel.md) | 内核学习资源索引 |
| [Linux Kernel Quick Guide.md](kernel/Linux%20Kernel%20Quick%20Guide.md) | 内核速查手册 |

## mm/ — 内存管理

| 文件 | 内容 |
|---|---|
| [内存管理.md](mm/内存管理.md) | 内存管理学习资料 |
| [min_free_kbytes_gfp_atomic.md](mm/min_free_kbytes_gfp_atomic.md) | min_free_kbytes 与 GFP_ATOMIC |
| [bugs/](mm/bugs/) | 内存 BUG 分析 — 238303(kmemleak)、ksmd、insert_vmap_area_augment |

## fs/ — 文件系统

| 文件 | 内容 |
|---|---|
| [文件系统.md](fs/文件系统.md) | 文件系统学习资料 |
| [minifs/](fs/minifs/) | 手写 mini 文件系统 v1~v3 |
| [bugs/](fs/bugs/) | 文件系统 BUG — ext4 double free、kernfs slab、dcache |
| [patch/](fs/patch/) | [close_range() 稀疏 FD 优化](fs/patch/close_range稀疏FD优化.md) ✨ 上游已合入 |

## net/ — 网络栈

| 文件 | 内容 |
|---|---|
| [network.md](net/network.md) | 网络学习资料 |
| [Linux网络收包与epoll协作机制.md](net/Linux网络收包与epoll协作机制.md) | 收包 & epoll |
| [vxlan入门.md](net/vxlan入门.md) | VXLAN 入门 |
| [bugs/](net/bugs/) | 网络 BUG — ovs-veth-peer、vhost-user-vring、hns3 |
| [patch/](net/patch/) | [netns 批量 unhash 优化](net/patch/netns批量unhash优化.md) ✨ 上游已合入 |
| [port-forward/](net/port-forward/) | 端口转发脚本 |
| [proxy/](net/proxy/) | 代理配置 (mihomo) |

## security/ — 内核安全

| [系统安全.md](security/系统安全.md) | 内核安全学习资料 |

## drivers/ — 设备驱动

| 目录 | 内容 |
|---|---|
| [gpu/](drivers/gpu/) | GPU 驱动 — [nvidia-svm](drivers/gpu/nvidia-svm/) (9 篇)、[stanford-cs336](drivers/gpu/stanford-cs336/) (17 讲)、[ascend](drivers/gpu/ascend/)、[grtrace](drivers/gpu/grtrace/docs/) |
| [sound/](drivers/sound/) | 声卡驱动 — [pulseaudio](drivers/sound/pulseaudio/) (6 篇)、[phytium](drivers/sound/phytium/) 飞腾声卡 |
| [console/](drivers/console/) | console — [console_unlock](drivers/console/bugs/console_unlock/) BUG 分析 |
| [disk/](drivers/disk/) | 磁盘 — bcache、ceph、nvme_ioctl |
| [block/](drivers/block/) | 块设备 — [ceph RBD 性能分析](drivers/block/bugs/ceph/) |
| [modules/](drivers/modules/) | 内核模块 — [insmod panic](drivers/modules/bugs/insmod/) |
| [proc/](drivers/proc/) | proc — seq_open vs single_open |
| [power/](drivers/power/) | 电源管理 — [S4 suspend](drivers/power/bugs/) |
| [udl/](drivers/udl/) | UDL — mutex_lock panic 补丁 |
| [mcu/](drivers/mcu/) | MCU 假复位信号分析 |
| [设备驱动.md](drivers/设备驱动.md) | 驱动学习资源大全 |

## virt/ — 虚拟化

| 目录 | 内容 |
|---|---|
| [kvm/](virt/kvm/) | KVM — kickstart、qemu 编译调试 (10+ 篇)、virsh、books |
| [container/](virt/container/) | 容器 — docker（ctyunos/ubuntu/debian）、k8s 集群部署、uts_namespace |

## gdb/ — GDB 调试

| [gdb/](gdb/gdb/) | GDB 中文手册、gdbinit 配置 |
|---|---|
| [debug.md](gdb/debug.md) | 调试总览 |

## kdump/ — 崩溃分析

| [kdump/](kdump/kdump/) | kdump 分析 — [sysrq_trigger](kdump/kdump/sysrq_trigger/)、鲲鹏 920 vmcore 修复 |

## assembly/ — 汇编

| [binary-analysis/](assembly/binary-analysis/) | 二进制分析实战 |
|---|---|
| [assembly.md](assembly/assembly.md) | 汇编语言教程 |
| [16位汇编语言.md](assembly/16位汇编语言.md) | 16 位汇编 |

## books/ — 内核书籍

[UEFI 编程实践](books/UEFI编程实践/)、Linux 内核设计与实现、深入分析 Linux 内核源代码、庖丁解牛
UNIX 环境高级编程、深入浅出 DPDK、PCI Express 体系结构、SELinux 详解 等 20+ PDF。

## boot/ — 系统启动

| [grub/](boot/grub/) | GRUB — iommu=pt、vmlinuz initramfs 分析 |
|---|---|
| [monitoring/](boot/monitoring/) | 监控日志 — systemd、boot |

## tools/ — 开发工具

| [shell/](tools/shell/) | 内核编译/打包脚本 |
|---|---|
| [vim/](tools/vim/) | Vim 配置 & 实用技巧 |
| [tmux/](tools/tmux/) | Tmux |
| [ssh/](tools/ssh/) | SSH config |
| [distro/](tools/distro/) | 发行版 — deepin KMS、[deb](tools/distro/pkg/deb/) & [rpm-ostree](tools/distro/pkg/rpm-ostree/) 打包 |
| [testing/ltp/](tools/testing/ltp/) | LTP 测试用例分析 |

## linux教程.md

[Linux 学习教程资源大全](linux教程.md) — 2200+ 行外部链接索引。
