# 🐧 Linux 内核开发与系统技术文档

对标 Linux 内核源码树，按子系统组织。

## ⚙️ kernel/ — 内核核心

### 📋 sched/ — 进程调度

| [k8s-pod-vm/](kernel/sched/bugs/k8s-pod-vm/) | k8s pod 导致宿主机延迟 |
|---|---|
| [cfs/](kernel/sched/bugs/cfs/) | run_rebalance_domains |
| [dynamic_isolcpus_plan.md](kernel/sched/dynamic_isolcpus_plan.md) | 动态 CPU 隔离方案 |
| [systemd.md](kernel/sched/systemd.md) | systemd 分析 |
| [进程管理.md](kernel/sched/进程管理.md) | 进程管理学习资料 |

### 🔐 locking/ — 锁机制

| [hard lockup与soft locup的区别.md](kernel/locking/hard%20lockup与soft%20locup的区别.md) | Hard Lockup vs Soft Lockup |
|---|---|
| [spin_lock变体对比.md](kernel/locking/spin_lock变体对比.md) | Spin Lock 变体对比 |

### ♻️ rcu/ — RCU

| [bugs/](kernel/rcu/bugs/) | RCU stall、dpu iperf soft lockup |
|---|---|
| [demo/](kernel/rcu/demo/) | RCU 示例程序 v1~v2 |

### ⚡ irq/ — 中断子系统

| [bugs/](kernel/irq/bugs/) | 8250 UAF、SDEI hard lockup、ssh 软锁 |
|---|---|
| [tick/tickless/](kernel/irq/tick/tickless/) | NO_HZ_FULL tickless + RCU Stall |

### 🔍 trace/ — 追踪与性能

| [bpf/](kernel/trace/bpf/) | BPF verifier、BCC/bpftrace 教程、BPF 之巅 |
|---|---|
| [bpf/patch/](kernel/trace/bpf/patch/) | [BPF verifier ID 映射重置优化](kernel/trace/bpf/patch/bpf-verifier-ID映射重置优化.md) ✨ 上游已合入 |
| [events/](kernel/trace/events/) | perf 事件分析、CVE-2024-50099、UnixBench |
| [events/patch/](kernel/trace/events/patch/) | [hardlockup perf event 无状态重构](kernel/trace/events/patch/hardlockup-perf-event无状态重构.md) ✨ 上游已合入 |
| [stap/](kernel/trace/stap/) | SystemTap 使用教程 |
| [bugs/](kernel/trace/bugs/) | trace-cmd 导致 ftrace 损坏 |
| [linux_tracing_architecture.md](kernel/trace/linux_tracing_architecture.md) | Linux 追踪技术全景图 |
| [性能调优.md](kernel/trace/性能调优.md) | 性能调优资源索引 |

### 📦 sources/ — 内核源码

| [Linux-0.11-yuan-xy/](kernel/sources/Linux-0.11-yuan-xy/) | 原码完整工程 |
|---|---|
| [Linux-0.11-zhaojiong/](kernel/sources/Linux-0.11-zhaojiong/) | 内核完全注释 V5.0 |
| [linux-0.11-debug/](kernel/sources/linux-0.11-debug/) | Linux 0.11 调试文档 |

### 📄 其他

| [kernel.md](kernel/kernel.md) | 内核学习资源索引 (1000+ 外部链接) |
|---|---|
| [Linux Kernel Quick Guide.md](kernel/Linux%20Kernel%20Quick%20Guide.md) | 内核速查手册 |

---

## 🧠 mm/ — 内存管理

| [bugs/238303/](mm/bugs/238303/) | kmemleak — get_cpu_name、virtio_gpu、kernfs |
|---|---|
| [bugs/ksmd/](mm/bugs/ksmd/) | KSM 隔离核导致 OVS 高延迟；ksmd-taskset RPM |
| [bugs/deferred_split_scan/](mm/bugs/deferred_split_scan/) | 虚拟机 stress 压测 Oops |
| [bugs/insert_vmap_area_augment/](mm/bugs/insert_vmap_area_augment/) | vmap_area 增强分析 |
| [bugs/kernel-dynamic-memory/](mm/bugs/kernel-dynamic-memory/) | 内核动态内存 nocache 异常高 |
| [bugs/si_mem_available/](mm/bugs/si_mem_available/) | 可用内存追踪分析 |
| [bugs/hardware/](mm/bugs/hardware/) | 内存硬件故障导致 VM 迁移重启 |
| [min_free_kbytes_gfp_atomic.md](mm/min_free_kbytes_gfp_atomic.md) | min_free_kbytes 与 GFP_ATOMIC |
| [内存管理.md](mm/内存管理.md) | 内存管理学习资料 |

---

## 📁 fs/ — 文件系统

### 🐛 BUG 分析

| [bugs/ext4/233267/](fs/bugs/ext4/233267/) | ext4_inode_info double free、dentry UAF |
|---|---|
| [bugs/kernfs/](fs/bugs/kernfs/) | kernfs_node_cache slab 膨胀 |
| [bugs/dcache/](fs/bugs/dcache/) | d_name.hash 非法地址 0x60e |
| [bugs/fuse/](fs/bugs/fuse/) | alluxio-fuse ls 返回 EIO |

### ✍️ minifs/ — 手写文件系统

| [v1/](fs/minifs/v1/) | 手写 mini 文件系统 v1 |
|---|---|
| [v2/](fs/minifs/v2/) | 手写 mini 文件系统 v2 |
| [v3/](fs/minifs/v3/) | 手写 mini 文件系统 v3 |

### ✨ 上游补丁

| [patch/](fs/patch/) | [close_range() 稀疏 FD 优化](fs/patch/close_range稀疏FD优化.md) — Christian Brauner 合入 |

### 📝 其他

| [文件系统.md](fs/文件系统.md) | 文件系统学习资料 |

---

## 🌐 net/ — 网络栈

### 🐛 BUG 分析

| [bandwidth/](net/bugs/bandwidth/) | 海光4号 VM 带宽问题 |
|---|---|
| [hns3/](net/bugs/hns3/) | HNS3 网卡重启 CPU 卡死 |
| [ovs-veth-peer/](net/bugs/ovs-veth-peer/) | veth real_num_tx_queues==0 软锁 |
| [vhost-user-vring/](net/bugs/vhost-user-vring/) | OVS-DPDK vhost-user 丢包分析 |
| [netstamp_clear/](net/bugs/netstamp_clear/) | text_poke_bp_batch soft lockup |
| [server2-99-101/](net/bugs/server2-99-101/) | DPU 打流卡顿 perf 分析 |
| [ssh/](net/bugs/ssh/) | DPU 节点 ssh 软锁 (net_rx 路径) |
| [rte_kni/](net/bugs/rte_kni/) | KNI 问题 |
| [cinder/](net/bugs/cinder/) | Cinder 存储问题 |
| [dpu_iperf/](net/bugs/dpu_iperf/) | DPU iperf 目录 |

### ✨ 上游补丁

| [patch/](net/patch/) | [netns 批量 unhash 优化](net/patch/netns批量unhash优化.md) — Jakub Kicinski 合入 |

### 📝 其他

| [port-forward/](net/port-forward/) | IP 端口转发脚本 |
|---|---|
| [proxy/mihomo/](net/proxy/mihomo/) | Mihomo 代理配置 |
| [network.md](net/network.md) | 网络学习资料 |
| [Linux网络收包与epoll协作机制.md](net/Linux网络收包与epoll协作机制.md) | 收包路径 & epoll |
| [vxlan入门.md](net/vxlan入门.md) | VXLAN 入门 |
| [win11 wsl2 vm嵌套网络拓扑.md](net/win11%20wsl2%20vm嵌套网络拓扑.md) | WSL2 嵌套网络 |

---

## 🔒 security/ — 内核安全

| [系统安全.md](security/系统安全.md) | 内核安全学习资料 |

---

## 🔌 drivers/ — 设备驱动

### 🎮 GPU

| [nvidia-svm/](drivers/gpu/nvidia-svm/) | HMM → GPUSVM → TTM → Buddy 显存管理 (9 篇) |
|---|---|
| [stanford-cs336/](drivers/gpu/stanford-cs336/) | Stanford 大模型课程笔记 (17 讲) |
| [ascend/](drivers/gpu/ascend/) | 昇腾 AI C 算子编程、训练/推理调优 |
| [grtrace/docs/](drivers/gpu/grtrace/docs/) | GPU Ring Buffer 追踪架构设计 |
| [bugs/](drivers/gpu/bugs/) | GPU freeze/pt620k/236691 |
| [virtio/](drivers/gpu/virtio/) | virtio_gpu TTM hang |
| [ai_infra.md](drivers/gpu/ai_infra.md) | AI Infra 概述 |
| [gpu.md](drivers/gpu/gpu.md) | GPU 学习资料 |
| [gpu_buddy_analysis.md](drivers/gpu/gpu_buddy_analysis.md) | GPU Buddy 分配器分析 |
| [kernel_ai_infra.md](drivers/gpu/kernel_ai_infra.md) | 内核 AI 基础设施全景 |

### 🔊 声卡

| [pulseaudio/](drivers/sound/pulseaudio/) | PulseAudio 源码解析 (6 篇) |
|---|---|
| [phytium/](drivers/sound/phytium/) | 飞腾 DP 声卡、X100 套片 + 主线补丁 |
| [bugs/](drivers/sound/bugs/) | HDMI ALC885、ALC257 bug |
| [sound.md](drivers/sound/sound.md) | 声卡学习资料 |
| [sound-algorithm.md](drivers/sound/sound-algorithm.md) | 声卡算法分析 |
| [hda_codec.md](drivers/sound/hda_codec.md) | HDA Codec |
| [hdmi-audio.md](drivers/sound/hdmi-audio.md) | HDMI 音频 |

### 🔧 其他驱动

| [console/bugs/](drivers/console/bugs/) | devkmsg_write、do_coredump |
|---|---|
| [disk/bcache/](drivers/disk/bcache/) | bcache 日志分析 |
| [disk/ceph/](drivers/disk/ceph/) | Ceph 集群 nvme 故障率升高 |
| [block/bugs/ceph/](drivers/block/bugs/ceph/) | Ceph RBD fio 性能分析 |
| [modules/bugs/insmod/](drivers/modules/bugs/insmod/) | Ubuntu22.04 insmod panic |
| [proc/](drivers/proc/) | seq_open vs single_open |
| [power/bugs/](drivers/power/bugs/) | S4 suspend-to-disk |
| [mcu/](drivers/mcu/) | MCU 假复位信号 |
| [udl/](drivers/udl/) | udl mutex_lock panic |
| [acpi/](drivers/acpi/) | ACPI power_button |
| [设备驱动.md](drivers/设备驱动.md) | 驱动学习资源大全 |

---

## 🖥️ virt/ — 虚拟化

### 💿 KVM

| [kickstart/](virt/kvm/kickstart/) | 无人值守安装 — ctyun、kylin、debian |
|---|---|
| [qemu/](virt/kvm/qemu/) | QEMU 编译调试 (2.8.1、debian9) |
| [virsh/](virt/kvm/virsh/) | virsh 启动 + gdb 调试方法 |
| [books/](virt/kvm/books/) | QEMU-KVM 源码分析、KVM 实战 |
| [images/](virt/kvm/images/) | KVM 调试截图 |
| 10+ 篇文档 | amd64/arm64 on qemu、helloworld-initramfs、do_initcalls ... |

### 📦 容器

| [docker/](virt/container/docker/) | Dockerfile 集合 (ctyunos、ubuntu、debian) |
|---|---|
| [k8s/](virt/container/k8s/) | K8s 集群部署、linglong 容器 |
| [uts_namespace/](virt/container/uts_namespace/) | UTS namespace 示例程序 |
| [cgroup v1 vs v2](virt/container/cgroup%20v1%20和%20cgroup%20v2%20的核心区别.md) | cgroup 核心区别 |

---

## 🐛 gdb/ — GDB 调试

| [gdb/](gdb/gdb/) | GDB 中文手册、gdbinit 配置 |
|---|---|
| [debug.md](gdb/debug.md) | 调试总览 |

## 💥 kdump/ — 崩溃分析

| [kdump/](kdump/kdump/) | kdump 分析 |
|---|---|
| [sysrq_trigger/](kdump/kdump/sysrq_trigger/) | kgdb + sysrq 触发 dump；鲲鹏 920 vmcore 修复 |

## 🔬 assembly/ — 汇编 & 二进制分析

| [binary-analysis/](assembly/binary-analysis/) | 二进制分析实战 (PDF + 源码) |
|---|---|
| [assembly.md](assembly/assembly.md) | 汇编语言教程 |
| [16位汇编语言.md](assembly/16位汇编语言.md) | 16 位汇编 |

## 📖 books/ — 内核书籍

[UEFI 编程实践](books/UEFI编程实践/) · Linux 内核设计与实现 · 深入分析 Linux 内核源代码 · 庖丁解牛
UNIX 环境高级编程 · 深入浅出 DPDK · PCI Express 体系结构 · SELinux 详解 · Debug Hacks 等 20+ 经典 PDF。

## 🚀 boot/ — 系统启动

| [grub/](boot/grub/) | GRUB — iommu=pt、vmlinuz initramfs 分析 |
|---|---|
| [monitoring/log/](boot/monitoring/log/) | 系统日志 — systemd、boot |

## 🛠️ tools/ — 开发工具

| [shell/](tools/shell/) | 内核编译/打包/模块脚本 (rpmbuild、zsh) |
|---|---|
| [vim/](tools/vim/) | Vim 配置 + Vim实用技巧(第2版) |
| [tmux/](tools/tmux/) | Tmux |
| [ssh/](tools/ssh/) | SSH config |
| [distro/deepin/](tools/distro/deepin/) | Deepin KMS 配置 |
| [distro/pkg/deb/](tools/distro/pkg/deb/) | debmake 教程 + debhello 示例 + flatpak |
| [distro/pkg/rpm-ostree/](tools/distro/pkg/rpm-ostree/) | rpm-ostree compose tree |
| [testing/ltp/](tools/testing/ltp/) | 88.5 LTP 失败测试用例分析 |

## 📝 linux教程

[Linux 学习教程资源大全](linux教程.md) — 2200+ 行外部链接索引。