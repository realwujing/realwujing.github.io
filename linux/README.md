# 🐧 Linux 内核开发与系统技术文档

对标 Linux 内核源码树，按子系统组织。

## ⚙️ kernel/ — 内核核心

-   📋 sched/ — 进程调度
    -   k8s-pod-vm/ — k8s pod 导致宿主机延迟
    -   cfs/ — run_rebalance_domains
    -   dynamic_isolcpus_plan.md — 动态 CPU 隔离方案
    -   systemd.md — systemd 分析
    -   进程管理.md — 进程管理学习资料
-   🔐 locking/ — 锁机制
    -   hard lockup与soft locup的区别.md — Hard Lockup vs Soft Lockup
    -   spin_lock变体对比.md — Spin Lock 变体对比
-   ♻️ rcu/ — RCU
    -   bugs/ — RCU stall、dpu iperf soft lockup
        -   rcu_process_callbacks/ — rcu hard lockup
    -   demo/v1~v2/ — RCU 示例程序
-   ⚡ irq/ — 中断子系统
    -   bugs/ — 8250 UAF、SDEI hard lockup
        -   sdei/ — 鲲鹏920 PA fault AP hangup
    -   tick/tickless/ — NO_HZ_FULL tickless + RCU Stall
-   🔍 trace/ — 追踪与性能
    -   bpf/ — BPF verifier、BCC/bpftrace 教程、BPF 之巅
        -   patch/ — BPF verifier ID 映射重置优化 ✨ 上游已合入
    -   events/ — perf 事件分析、CVE-2024-50099、UnixBench
        -   patch/ — hardlockup perf event 无状态重构 ✨ 上游已合入
        -   perf/243205/ — lightdm waits for plymouth-quit-wait
        -   perf/perf-event/ — perf_event 空指针卡死
        -   perf/unixbench/ — UnixBench 基准测试分析
    -   stap/ — SystemTap 使用教程
    -   bugs/ — trace-cmd 导致 ftrace 损坏
    -   linux_tracing_architecture.md — Linux 追踪技术全景图
    -   性能调优.md — 性能调优资源索引
-   📦 sources/ — 内核源码
    -   Linux-0.11-yuan-xy/ — 原码完整工程
    -   Linux-0.11-zhaojiong/ — 内核完全注释 V5.0
    -   linux-0.11-debug/ — Linux 0.11 调试文档
-   📄 kernel.md — 内核学习资源索引 (1000+ 外部链接)
-   📄 Linux Kernel Quick Guide.md — 内核速查手册

---

## 🧠 mm/ — 内存管理

-   bugs/ — 内存 BUG 分析
    -   237413/
    -   238303/ — kmemleak 系列 (get_cpu_name、virtio_gpu)
    -   247295/
    -   deferred_split_scan/ — 虚拟机 stress 压测 Oops
    -   hardware/ — 内存硬件故障导致 VM 迁移重启
    -   insert_vmap_area_augment/ — vmap_area 增强分析
    -   kernel-dynamic-memory/ — 内核动态内存 nocache 异常高
    -   ksmd/ — KSM 隔离核导致 OVS 高延迟
        -   ksmd-taskset/ — ksmd taskset RPM 打包
    -   si_mem_available/ — 可用内存追踪分析
-   min_free_kbytes_gfp_atomic.md — min_free_kbytes 与 GFP_ATOMIC
-   内存管理.md — 内存管理学习资料

---

## 📁 fs/ — 文件系统

-   🐛 bugs/
    -   dcache/ — d_name.hash 非法地址 0x60e
    -   ext4/ — ext4_inode_info double free、dentry UAF
        -   233267/ — dmesg 分析
    -   fuse/ — alluxio-fuse ls 返回 EIO
    -   kernfs/ — kernfs_node_cache slab 膨胀
-   ✍️ minifs/
    -   v1/ — 手写 mini 文件系统 v1
    -   v2/ — 手写 mini 文件系统 v2
    -   v3/ — 手写 mini 文件系统 v3
-   ✨ patch/ — close_range() 稀疏 FD 优化 (Christian Brauner 合入)
-   📝 文件系统.md — 文件系统学习资料

---

## 🌐 net/ — 网络栈

-   🐛 bugs/
    -   bandwidth/ — 海光4号 VM 带宽问题
    -   hns3/ — HNS3 网卡重启 CPU 卡死
    -   netstamp_clear/ — text_poke_bp_batch soft lockup
    -   ovs-veth-peer/ — veth real_num_tx_queues==0 软锁
    -   rte_kni/ — KNI 问题
    -   server2-99-101/ — DPU 打流卡顿 perf 分析
    -   ssh/ — DPU 节点 ssh 软锁 (net_rx 路径)
    -   vhost-user-vring/ — OVS-DPDK vhost-user 丢包分析
    -   cinder/ — Cinder 存储问题
    -   dpu_iperf/ — DPU iperf 目录
-   ✨ patch/ — netns 批量 unhash 优化 (Jakub Kicinski 合入)
-   📝 port-forward/ — IP 端口转发脚本
-   📝 proxy/mihomo/ — Mihomo 代理配置
-   📄 network.md — 网络学习资料
-   📄 Linux网络收包与epoll协作机制.md — 收包路径 & epoll
-   📄 vxlan入门.md — VXLAN 入门
-   📄 win11 wsl2 vm嵌套网络拓扑.md — WSL2 嵌套网络

---

## 🔒 security/ — 内核安全

-   系统安全.md — 内核安全学习资料

---

## 🔌 drivers/ — 设备驱动

-   🎮 gpu/
    -   ascend/ — 昇腾 AI C 算子编程、训练/推理调优
    -   nvidia-svm/ — HMM → GPUSVM → TTM → Buddy 显存管理
        -   01-hmm.md — CPU↔GPU 内存镜像基础
        -   02-drm-gpusvm.md — GPU 共享虚拟内存抽象层
        -   03-nouveau-svm.md — NVIDIA HMM 调用者
        -   04-nouveau-dmem.md — NVIDIA 设备显存管理
        -   05-ttm.md — GPU 多内存类型管理
        -   06-gpu-buddy.md — GPU 显存 Buddy 分配器
        -   07-gpu-scheduler.md — GPU 调度
        -   08-umem-dmabuf.md — GPU→RDMA 零拷贝桥梁
        -   09-mlx5-mr.md — mlx5 MR 管理
    -   stanford-cs336/ — Stanford 大模型课程笔记 (17 讲)
        -   arch/、gpu/、moe/ — 架构、GPU、MoE
        -   Alignment-RL/、Alignment-SFT-RLHF/ — 对齐技术
        -   parallelism/ — 并行化策略
        -   scaling-laws/、scaling-laws2/ — 扩展定律
    -   grtrace/docs/ — GPU Ring Buffer 追踪架构设计
    -   openclaw/ — OpenCLaw 架构分析
    -   bugs/ — GPU freeze/pt620k/236691
        -   227273/ — pt620k 1031 desktop freeze
        -   234167/ — UNIS D3812 D2000 desktop freeze
        -   236691/
    -   virtio/ — virtio_gpu TTM hang
    -   ai_infra.md — AI Infra 概述
    -   gpu.md — GPU 学习资料
    -   gpu_buddy_analysis.md — GPU Buddy 分配器分析
    -   kernel_ai_infra.md — 内核 AI 基础设施全景
    -   显示功能学习comments.md
    -   算子(Operator) vs 核函数(Kernel) 框架图.md
-   🔊 sound/
    -   bugs/ — HDMI ALC885、ALC257
        -   216983/ — HDMI 声卡问题
        -   226916/patch/ — Realtek ALC257 重命名补丁
    -   pulseaudio/ — PulseAudio 源码解析 (6 篇)
        -   mapping_paths_probe.md、pa_alsa_path_probe.md
        -   pa_alsa_profile_set_new.md、pa_alsa_profile_set_probe.md
        -   profile_finalize_probing.md、pulseaudio.md、pulseaudio调试.md
    -   phytium/ — 飞腾 DP 声卡、X100 套片 + 主线补丁
        -   patch/ — 基于主线 linux4.19 合入飞腾补丁
    -   ALC885_1-1.md — ALC885 声卡分析
    -   hda_codec.md — HDA Codec
    -   hdmi-audio.md — HDMI 音频
    -   sound-algorithm.md — 声卡算法分析
    -   sound.md — 声卡学习资料
-   🔧 console/
    -   bugs/console_unlock/ — console 解锁 BUG
        -   devkmsg_write/ — devkmsg_write 问题
        -   do_coredump/ — do_coredump 问题
-   💾 disk/
    -   bcache/ — bcache 日志分析
    -   ceph/ — Ceph 集群 nvme 故障率升高
    -   nvme_ioctl/
-   📦 block/
    -   bugs/ceph/ — Ceph RBD fio 性能分析
-   📦 modules/
    -   bugs/insmod/ — Ubuntu22.04 insmod panic
    -   README.md
-   🔌 proc/ — seq_open vs single_open 对比
-   🔋 power/bugs/ — S4 suspend-to-disk (236037)
-   🔌 mcu/ — MCU 假复位信号 ForcePowerCycle
-   🔌 udl/ — udl mutex_lock panic
-   🔌 acpi/ — ACPI power_button
-   📝 设备驱动.md — 驱动学习资源大全

---

## 🖥️ virt/ — 虚拟化

-   💿 kvm/
    -   kickstart/ — 无人值守安装
        -   ctyun/ — ctyun kickstart
        -   kylin/ — kylin kickstart
        -   debian/ — virt-install auto install debian
        -   ctenos/bios/、ctenos/uefi/ — ctenos kickstart
    -   qemu/ — QEMU 编译调试
        -   qemu-2.8.1编译调试.md
        -   qemu编译调试.md
        -   基于debian9编译调试qemu.md
    -   virsh/ — virsh 启动 + gdb 调试方法
    -   books/ — QEMU-KVM 源码分析、KVM 实战
    -   images/ — KVM 调试截图
    -   helloworld/ — helloworld initramfs 示例
    -   amd64/arm64 调试文档 (10+ 篇)
    -   do_initcalls.md、helloworld-initramfs.md
    -   linux内核调试.md、qemu-kvm.md
    -   虚拟机CPU拓扑解析.md、去掉编译内核的优化选项.md
-   📦 container/
    -   docker/ — Dockerfile 集合
        -   ctyunos/ — ctyunos2/3/4 + dnf installroot
        -   ubuntu/ — Ubuntu Dockerfile 集合
        -   debian/ — Debian Docker 部署
        -   iptables.md
    -   k8s/ — K8s 集群部署
        -   k8s集群部署/ — k8s 集群安装教程
        -   linglong/ — 玲珑容器配置
        -   configmaps/、jenkins/ — ConfigMap、Jenkins
        -   docker.md、kubernetes.md、namespace-cgroup.md
    -   uts_namespace/ — UTS namespace 示例程序
    -   cgroup v1 和 cgroup v2 的核心区别.md

---

## 🐛 gdb/ — GDB 调试

-   gdb/ — GDB 中文手册、gdbinit 配置
-   debug.md — 调试总览

## 💥 kdump/ — 崩溃分析

-   kdump/
    -   sysrq_trigger/ — kgdb + sysrq 触发 dump
        -   sysrq_trigger.md
        -   uos-1060 kgdb 修复.md
        -   基于kgdb调试uos-1060-6026.md
    -   修复kunpeng 920 dpu主机无法生成vmcore.md

## 🔬 assembly/ — 汇编 & 二进制分析

-   binary-analysis/ — 二进制分析实战 (PDF + 源码)
-   assembly.md — 汇编语言教程
-   16位汇编语言.md — 16 位汇编

## 📖 books/ — 内核书籍

-   UEFI编程实践/ — UEFI 编程实践分卷
-   Linux内核设计与实现(第三版).pdf
-   深入分析Linux内核源代码.pdf
-   庖丁解牛十二刀5.9.pdf
-   Linux设备驱动开发详解.pdf
-   INTEL开发手册卷3(中文版).pdf
-   SELinux详解.pdf
-   UNIX 环境高级编程 第3版.pdf
-   Debug Hacks 深入调试的技术和工具.pdf
-   深入浅出DPDK.pdf
-   PCI_Express_体系结构导读.pdf
-   软件调试（第2版）卷1：硬件基础.pdf
-   （已压缩）通用图形处理器设计GPGPU.pdf
-   grub/mips-5.10/grub.md

## 🚀 boot/ — 系统启动

-   grub/ — GRUB iommu=pt、vmlinuz initramfs 分析
-   monitoring/log/ — 系统日志 (systemd、boot)

## 🛠️ tools/ — 开发工具

-   shell/ — 内核编译/打包脚本 (rpmbuild、zsh)
-   vim/ — Vim 配置 + Vim实用技巧(第2版)
-   tmux/ — Tmux
-   ssh/ — SSH config
-   distro/
    -   deepin/kms/ — Deepin KMS 配置
    -   pkg/
        -   deb/ — debmake 教程 + debhello 示例 + pbuilder
        -   rpm-ostree/ — rpm-ostree compose tree
        -   flatpak.md — Flatpak 使用
        -   包管理.md
-   testing/ltp/ — 88.5 LTP 失败测试用例分析

## 📝 linux教程

Linux 学习教程资源大全 — 2200+ 行外部链接索引。