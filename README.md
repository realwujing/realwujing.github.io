# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## 🐧 Linux 内核

对标 Linux 内核源码树，按子系统组织。

### ⚙️ kernel/ — 内核核心

| | 调度、锁、RCU、中断、追踪、源码 |
|---|---|
| [sched/](linux/kernel/sched/) | 进程调度 — CFS、isolcpus、systemd |
| [sched/bugs/](linux/kernel/sched/bugs/) | 调度 BUG — cfs、k8s-pod-vm |
| [locking/](linux/kernel/locking/) | 锁机制 — spin_lock 变体、hard lockup 分析 |
| [rcu/](linux/kernel/rcu/) | RCU — stall 分析、demo 示例 |
| [irq/](linux/kernel/irq/) | 中断子系统 |
| [irq/bugs/](linux/kernel/irq/bugs/) | 中断 BUG — 8250 UAF、SDEI hard lockup |
| [irq/tick/tickless/](linux/kernel/irq/tick/tickless/) | NO_HZ_FULL tickless + RCU Stall |
| [trace/](linux/kernel/trace/) | 追踪 & 性能 |
| [trace/bpf/](linux/kernel/trace/bpf/) | BPF/BCC/bpftrace、verifier 分析 |
| [trace/bpf/patch/](linux/kernel/trace/bpf/patch/) | [BPF verifier ID 映射优化](linux/kernel/trace/bpf/patch/bpf-verifier-ID映射重置优化.md) ✨ |
| [trace/events/](linux/kernel/trace/events/) | perf 事件、CVE-2024-50099、UnixBench |
| [trace/events/patch/](linux/kernel/trace/events/patch/) | [hardlockup perf event 重构](linux/kernel/trace/events/patch/hardlockup-perf-event无状态重构.md) ✨ |
| [trace/stap/](linux/kernel/trace/stap/) | SystemTap 教程 |
| [trace/bugs/](linux/kernel/trace/bugs/) | ftrace 损坏修复 |
| [sources/](linux/kernel/sources/) | Linux 0.11 源码分析 |
| [kernel.md](linux/kernel/kernel.md) | 内核学习资源索引 |
| [Linux Kernel Quick Guide.md](linux/kernel/Linux%20Kernel%20Quick%20Guide.md) | 内核速查手册 |

### 🧠 mm/ — 内存管理

| [bugs/](linux/mm/bugs/) | 内存 BUG — 238303(kmemleak)、ksmd、slab、vmap |
|---|---|
| [内存管理.md](linux/mm/内存管理.md) | 内存管理学习资料 |

### 📁 fs/ — 文件系统

| [bugs/](linux/fs/bugs/) | 文件系统 BUG — ext4、kernfs、dcache、fuse |
|---|---|
| [minifs/](linux/fs/minifs/) | 手写 mini 文件系统 v1~v3 |
| [patch/](linux/fs/patch/) | [close_range() 稀疏 FD 优化](linux/fs/patch/close_range稀疏FD优化.md) ✨ 上游已合入 |
| [文件系统.md](linux/fs/文件系统.md) | 文件系统学习资料 |

### 🌐 net/ — 网络栈

| [bugs/](linux/net/bugs/) | 网络 BUG — ovs-veth-peer、vhost-user-vring、hns3 |
|---|---|
| [patch/](linux/net/patch/) | [netns 批量 unhash 优化](linux/net/patch/netns批量unhash优化.md) ✨ 上游已合入 |
| [port-forward/](linux/net/port-forward/) | 端口转发脚本 |
| [proxy/mihomo/](linux/net/proxy/mihomo/) | Mihomo 代理配置 |
| [network.md](linux/net/network.md) | 网络学习资料 |
| [Linux网络收包与epoll协作机制.md](linux/net/Linux网络收包与epoll协作机制.md) | 收包 & epoll |
| [vxlan入门.md](linux/net/vxlan入门.md) | VXLAN 入门 |

### 🔒 security/ — 内核安全

| [系统安全.md](linux/security/系统安全.md) | 内核安全学习资料 |

### 🔌 drivers/ — 设备驱动

| [gpu/](linux/drivers/gpu/) | GPU 驱动 |
|---|---|
| [gpu/nvidia-svm/](linux/drivers/gpu/nvidia-svm/) | HMM → GPUSVM → 显存管理 (9 篇) |
| [gpu/stanford-cs336/](linux/drivers/gpu/stanford-cs336/) | Stanford 大模型课程笔记 (17 讲) |
| [gpu/ascend/](linux/drivers/gpu/ascend/) | 昇腾 AI C 算子、训练/推理调优 |
| [gpu/grtrace/docs/](linux/drivers/gpu/grtrace/docs/) | GPU Ring Buffer 追踪架构 |
| [gpu/bugs/](linux/drivers/gpu/bugs/) | GPU freeze/pt620k/236691 |
| [sound/](linux/drivers/sound/) | 声卡 — pulseaudio 源码、飞腾 DP、ALC |
| [console/bugs/](linux/drivers/console/bugs/) | console — devkmsg_write、do_coredump |
| [disk/](linux/drivers/disk/) | 磁盘 — bcache、ceph、nvme_ioctl |
| [block/bugs/ceph/](linux/drivers/block/bugs/ceph/) | Ceph RBD fio 性能分析 |
| [modules/bugs/](linux/drivers/modules/bugs/) | insmod panic |
| [proc/](linux/drivers/proc/) | seq_open vs single_open |
| [power/bugs/](linux/drivers/power/bugs/) | S4 suspend |
| [mcu/](linux/drivers/mcu/) | MCU 假复位信号 |
| [udl/](linux/drivers/udl/) | udl mutex_lock panic |
| [设备驱动.md](linux/drivers/设备驱动.md) | 驱动学习资源大全 |

### 🖥️ virt/ — 虚拟化

| [kvm/](linux/virt/kvm/) | KVM — kickstart、qemu 编译调试、virsh |
|---|---|
| [kvm/books/](linux/virt/kvm/books/) | QEMU-KVM 源码分析、KVM 实战 |
| [container/](linux/virt/container/) | 容器 — Docker、K8s、uts_namespace |
| [container/docker/](linux/virt/container/docker/) | Dockerfile 集合 (ctyunos、ubuntu、debian) |
| [container/k8s/](linux/virt/container/k8s/) | K8s 集群部署、linglong |

### 🐛 gdb/ — GDB 调试

| [gdb/](linux/gdb/gdb/) | GDB 中文手册、gdbinit 配置 |
|---|---|
| [debug.md](linux/gdb/debug.md) | 调试总览 |

### 💥 kdump/ — 崩溃分析

| [kdump/](linux/kdump/kdump/) | kdump 分析 |
|---|---|
| [kdump/sysrq_trigger/](linux/kdump/kdump/sysrq_trigger/) | kgdb + sysrq 触发 dump |

### 🔬 assembly/ — 汇编

| [binary-analysis/](linux/assembly/binary-analysis/) | 二进制分析实战 |
|---|---|
| [assembly.md](linux/assembly/assembly.md) | 汇编语言教程 |

### 📖 books/ — 内核书籍

UEFI 编程实践、Linux 内核设计与实现、深入分析 Linux 内核源代码、庖丁解牛、UNIX 环境高级编程、深入浅出 DPDK、PCI Express 体系结构、SELinux 详解等 20+ 经典 PDF。

### 🚀 boot/ — 系统启动

| [grub/](linux/boot/grub/) | GRUB — iommu=pt、vmlinuz initramfs 分析 |
|---|---|
| [monitoring/](linux/boot/monitoring/) | 监控日志 — systemd、boot |

### 🛠️ tools/ — 开发工具

| [shell/](linux/tools/shell/) | 内核编译/打包脚本 |
|---|---|
| [vim/](linux/tools/vim/) | Vim 配置 & 实用技巧 |
| [distro/](linux/tools/distro/) | 发行版 — deepin KMS、deb/rpm-ostree 打包 |
| [testing/ltp/](linux/tools/testing/ltp/) | LTP 测试用例分析 |

### 📝 linux教程

[Linux 学习教程资源大全](linux/linux教程.md) — 2200+ 行外部链接索引。

---

## 💻 编程语言

| [cpp/](cpp/) | C/C++ — 语法、多线程、Qt、cmake |
|---|---|
| [python/](python/) | Python |
| [go/](go/) | Go |
| [rust/](rust/) | Rust |
| [java/](java/) | Java |
| [javascript/](javascript/) | JavaScript / Vue |

## 🏗️ 架构 & 算法

| [architect/](architect/) | 系统架构师 — OS、网络、数据库、计组 |
|---|---|
| [algorithm/](algorithm/) | 算法、Transformer、LeetCode |

## 🔧 工具

| [git/](git/) | Git — github-pages、b4、gerrit |
|---|---|
| [jenkins/](jenkins/) | CI/CD |
| [nginx/](nginx/) | Nginx |
| [markdown/](markdown/) | Markdown、LaTeX、UML |

## 💾 其他

| [sql/](sql/) | MySQL |
|---|---|
| [redis/](redis/) | Redis |
| [3d/](3d/) | 3D 建模 |
| [patent/](patent/) | 专利文档 |
| [svn/](svn/) | SVN |