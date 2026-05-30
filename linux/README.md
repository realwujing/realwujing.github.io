# Linux 内核开发与系统技术文档

对标 Linux 内核源码树 (v5.10)，按子系统组织。

```
linux/
├── kernel/                              # 内核核心子系统
│   ├── sched/                           # 进程调度 (CFS、isolcpus、systemd)
│   │   ├── bugs/
│   │   │   ├── cfs/                     # run_rebalance_domains
│   │   │   └── k8s-pod-vm/             # k8s pod 导致宿主机延迟
│   │   ├── dynamic_isolcpus_plan.md     # 动态 CPU 隔离方案
│   │   ├── systemd.md
│   │   └── 进程管理.md
│   ├── locking/                         # 锁机制
│   │   ├── reboot/
│   │   ├── hard lockup与soft locup的区别.md
│   │   └── spin_lock变体对比.md
│   ├── rcu/                             # RCU
│   │   ├── bugs/                        # RCU stall hard lockup 分析
│   │   └── demo/v1~v2/                 # RCU 示例程序
│   ├── irq/                             # 中断子系统
│   │   ├── bugs/
│   │   │   ├── 8250-shared-irq-use-after-free.md
│   │   │   └── sdei/                    # 鲲鹏920 硬件故障硬锁
│   │   └── tick/
│   │       └── tickless/                # NO_HZ_FULL tickless + RCU Stall
│   ├── trace/                           # 追踪与性能总入口
│   │   ├── bpf/                         # BPF verifier、BCC/bpftrace 教程、BPF 之巅
│   │   ├── events/                      # perf 事件分析、CVE-2024-50099、UnixBench
│   │   ├── stap/                        # SystemTap 使用教程
│   │   ├── bugs/                        # trace-cmd 导致 ftrace 损坏
│   │   ├── linux_tracing_architecture.md
│   │   └── 性能调优.md
│   ├── sources/                         # Linux 0.11 源码分析
│   │   ├── Linux-0.11-yuan-xy/          # 原码完整工程
│   │   ├── Linux-0.11-zhaojiong/        # 内核完全注释 V5.0
│   │   └── linux-0.11-debug/
│   ├── kernel.md                        # 内核学习资源索引 (1000+ 外部链接)
│   └── Linux Kernel Quick Guide.md
│
├── mm/                                  # 内存管理
│   ├── bugs/
│   │   ├── 237413/
│   │   ├── 238303/                      # kmemleak 系列 (get_cpu_name, virtio_gpu)
│   │   ├── 247295/
│   │   ├── deferred_split_scan/         # 虚拟机 stress 压测 Oops
│   │   ├── hardware/                    # 内存硬件故障导致 VM 迁移重启
│   │   ├── insert_vmap_area_augment/    # vmap_area 增强分析
│   │   ├── kernel-dynamic-memory/       # 内核动态内存 nocache 异常高
│   │   ├── ksmd/                        # KSM 隔离核导致 OVS 高延迟
│   │   │   └── ksmd-taskset/            # ksmd taskset RPM 打包
│   │   └── si_mem_available/           # 可用内存追踪分析
│   ├── min_free_kbytes_gfp_atomic.md
│   └── 内存管理.md
│
├── fs/                                  # 文件系统
│   ├── bugs/
│   │   ├── ext4/
│   │   │   └── 233267/                  # ext4_inode_info double free、dentry UAF
│   │   ├── fuse/                        # alluxio-fuse ls 返回 EIO
│   │   ├── kernfs/                      # kernfs_node_cache slab 异常高
│   │   └── dcache/                      # d_name.hash 非法地址 0x60e
│   ├── minifs/
│   │   ├── v1/                          # 手写 mini 文件系统 v1
│   │   ├── v2/
│   │   └── v3/
│   └── 文件系统.md
│
├── net/                                 # 网络栈
│   ├── bugs/
│   │   ├── bandwidth/                   # 海光4号 VM 带宽问题
│   │   ├── cinder/
│   │   ├── hns3/                        # HNS3 网卡重启 CPU 卡死
│   │   ├── netstamp_clear/              # text_poke_bp_batch soft lockup
│   │   ├── ovs-veth-peer/               # veth real_num_tx_queues==0 软锁
│   │   ├── rte_kni/
│   │   ├── server2-99-101/              # DPU 打流卡顿 perf 分析
│   │   ├── ssh/                         # DPU 节点 ssh 软锁 (net_rx 路径)
│   │   └── vhost-user-vring/            # OVS-DPDK vhost-user 丢包
│   ├── port-forward/                    # IP 端口转发脚本
│   ├── proxy/
│   │   └── mihomo/                      # mihomo 代理配置
│   ├── Linux网络收包与epoll协作机制.md
│   ├── network.md
│   ├── vxlan入门.md
│   └── win11 wsl2 vm嵌套网络拓扑.md
│
├── security/                            # 内核安全
│   └── 系统安全.md
│
├── drivers/                             # 设备驱动
│   ├── gpu/
│   │   ├── ascend/                      # 昇腾 AI C 算子、训练/推理调优
│   │   ├── nvidia-svm/                  # HMM → GPUSVM → TTM → Buddy (9 篇)
│   │   ├── stanford-cs336/              # Stanford 大模型课程笔记 (17 讲)
│   │   ├── grtrace/docs/                # GPU Ring Buffer 追踪架构设计
│   │   ├── openclaw/                    # OpenCLaw 架构分析
│   │   ├── bugs/                        # GPU freeze / pt620k / 236691
│   │   ├── virtio/                      # virtio_gpu TTM hang
│   │   ├── ai_infra.md
│   │   ├── gpu.md
│   │   ├── gpu_buddy_analysis.md
│   │   └── kernel_ai_infra.md          # 内核 AI 基础设施全景
│   ├── sound/
│   │   ├── bugs/
│   │   │   ├── 216983/                  # HDMI 声卡问题
│   │   │   └── 226916/patch/            # Realtek ALC257 重命名补丁
│   │   ├── pulseaudio/                  # PulseAudio 源码解析 (6 篇)
│   │   ├── phytium/                     # 飞腾 DP 声卡、X100 套片 + 主线补丁
│   │   ├── sound.md
│   │   ├── sound-algorithm.md
│   │   ├── hda_codec.md
│   │   └── hdmi-audio.md
│   ├── console/
│   │   └── bugs/console_unlock/
│   │       ├── devkmsg_write/           # devkmsg_write 问题
│   │       └── do_coredump/             # do_coredump 问题
│   ├── disk/
│   │   ├── bcache/                      # bcache 日志分析
│   │   ├── ceph/                        # Ceph 集群 nvme 故障率升高
│   │   └── nvme_ioctl/
│   ├── block/
│   │   └── bugs/ceph/                   # Ceph RBD fio 性能分析
│   ├── modules/
│   │   ├── bugs/insmod/                 # Ubuntu22.04 insmod panic
│   │   └── README.md
│   ├── acpi/                            # ACPI power_button
│   ├── proc/                            # seq_open vs single_open 对比
│   ├── power/bugs/                      # S4/suspend-to-disk 问题
│   ├── mcu/                             # MCU 假复位信号 ForcePowerCycle
│   ├── udl/                             # udl mutex_lock panic 补丁
│   ├── infiniband/                      # InfiniBand vmcore 分析
│   └── 设备驱动.md                      # 驱动学习资源大全
│
├── virt/                                # 虚拟化
│   ├── kvm/                             # KVM 内核虚拟化
│   │   ├── kickstart/                   # 无人值守安装 kickstart (ctyun, kylin, debian)
│   │   ├── qemu/                        # QEMU 编译调试 (2.8.1, debian9)
│   │   ├── virsh/                       # virsh 启动 + gdb 调试方法
│   │   ├── books/                       # QEMU-KVM 源码分析, KVM 实战
│   │   ├── helloworld/                  # helloworld initramfs 示例
│   │   ├── images/                      # KVM 调试截图
│   │   └── 9 篇调试文档 (amd64/arm64 qemu) ...
│   └── container/                       # 容器技术
│       ├── docker/                      # Dockerfile 集合 (ctyunos, ubuntu, debian)
│       ├── k8s/                         # K8s 集群部署、linglong 容器
│       ├── uts_namespace/               # UTS namespace 示例程序
│       └── cgroup v1 vs v2 核心区别
│
├── gdb/                                 # GDB 调试
│   ├── gdb/
│   │   ├── README.md
│   │   ├── .gdbinit
│   │   └── Debugging.with.gdb 中文.pdf
│   └── debug.md
│
├── kdump/                               # kdump 崩溃分析
│   └── kdump/
│       ├── sysrq_trigger/               # kgdb + sysrq 触发 dump
│       │   ├── sysrq_trigger.md
│       │   ├── uos-1060 kgdb 修复.md
│       │   └── 基于kgdb调试uos-1060-6026.md
│       └── 修复kunpeng 920 dpu主机无法生成vmcore.md
│
├── assembly/                            # 汇编 & 二进制分析
│   ├── binary-analysis/                 # 二进制分析实战 (PDF + 源码)
│   ├── 16位汇编语言.md
│   ├── README.md
│   ├── assembly.md
│   └── 汇编语言（第4版）@www.cmsblogs.cn.pdf
│
├── books/                               # 内核经典书籍 (20+ PDF)
│   ├── UEFI编程实践/                    # UEFI 编程实践分卷
│   ├── grub/mips-5.10/                  # MIPS GRUB 配置
│   ├── Linux内核设计与实现(第三版).pdf
│   ├── 深入分析Linux内核源代码.pdf
│   ├── 庖丁解牛十二刀5.9.pdf
│   ├── Linux设备驱动开发详解.pdf
│   ├── INTEL开发手册卷3(中文版).pdf
│   ├── SELinux详解.pdf
│   ├── UNIX 环境高级编程 第3版.pdf
│   ├── Debug Hacks 深入调试的技术和工具.pdf
│   ├── 深入浅出DPDK.pdf
│   ├── PCI_Express_体系结构导读.pdf
│   ├── 软件调试（第2版）卷1：硬件基础.pdf
│   └── （已压缩）通用图形处理器设计GPGPU.pdf
│
├── boot/                                # 系统启动
│   ├── grub/
│   │   ├── grub中iommu=pt的作用.md
│   │   └── grub中vmlinuz initramfs分析.md
│   └── monitoring/
│       ├── log/
│       │   ├── README.md
│       │   ├── boot.svg
│       │   └── systemd.svg
│       └── linux运维监控工具.md.bak
│
├── tools/                               # 开发工具与发行版
│   ├── shell/                           # 内核编译/打包脚本 (rpmbuild, zsh)
│   ├── vim/                             # Vim 配置 + Vim实用技巧(第2版).pdf
│   ├── tmux/
│   ├── ssh/                             # SSH config
│   ├── distro/                          # 发行版打包
│   │   ├── deepin/                      # Deepin KMS 配置
│   │   └── pkg/                         # deb/rpm 包管理 (pbuilder, rpm-ostree)
│   │       ├── deb/                     # debmake 教程 + debhello 示例
│   │       ├── rpm-ostree/              # rpm-ostree compose tree
│   │       ├── flatpak.md
│   │       └── 包管理.md
│   └── testing/ltp/
│       └── 88.5 ltp 失败测试用例分析.md
│
└── linux教程.md                          # Linux 学习教程资源大全 (2200+ 行链接)
```