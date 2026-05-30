# 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## Linux 内核

对标 Linux 内核源码树结构，按子系统组织。

```
linux/
├── kernel/                              # 内核核心子系统
│   ├── sched/                           # 进程调度 (CFS、isolcpus、systemd)
│   │   └── bugs/                        # 调度 bug 分析 (k8s pod vm lag)
│   ├── locking/                         # 锁机制 (spin_lock、hard lockup)
│   ├── rcu/                             # RCU
│   │   ├── bugs/                        # RCU stall 分析
│   │   └── demo/
│   ├── irq/                             # 中断子系统
│   │   ├── bugs/                        # 8250 UAF、SDEI hard lockup
│   │   └── tick/
│   │       └── tickless/                # NO_HZ_FULL tickless + RCU Stall
│   ├── trace/                           # 追踪与性能
│   │   ├── bpf/                         # BPF/BCC/bpftrace 教程
│   │   ├── events/                      # perf 事件、CVE、UnixBench
│   │   ├── stap/                        # SystemTap
│   │   ├── bugs/                        # ftrace 损坏修复
│   │   ├── linux_tracing_architecture.md
│   │   └── 性能调优.md
│   ├── sources/                         # Linux 0.11 源码
│   ├── kernel.md                        # 内核学习资源索引
│   └── Linux Kernel Quick Guide.md
│
├── mm/                                  # 内存管理
│   ├── bugs/
│   │   ├── 238303/                      # kmemleak 系列 (virtio_gpu, kernfs)
│   │   ├── ksmd/                        # KSM 隔离核延迟
│   │   ├── insert_vmap_area_augment/
│   │   ├── deferred_split_scan/         # 虚拟机 Oops
│   │   ├── kernel-dynamic-memory/
│   │   ├── si_mem_available/
│   │   └── hardware/                    # 内存硬件故障
│   ├── min_free_kbytes_gfp_atomic.md
│   └── 内存管理.md
│
├── fs/                                  # 文件系统
│   ├── bugs/
│   │   ├── ext4/                        # ext4 inode double free、UAF
│   │   ├── kernfs/                      # kernfs_node_cache slab 膨胀
│   │   ├── dcache/                      # VFS dcache 非法地址访问
│   │   ├── fuse/                        # alluxio-fuse EIO
│   │   └── ...
│   ├── minifs/                          # 手写 mini 文件系统 v1~v3
│   └── 文件系统.md
│
├── net/                                 # 网络栈
│   ├── bugs/
│   │   ├── ovs-veth-peer/               # netdev_pick_tx soft lockup
│   │   ├── vhost-user-vring/            # OVS-DPDK 丢包分析
│   │   ├── netstamp_clear/              # text_poke soft lockup
│   │   ├── bandwidth/                   # VM 带宽问题
│   │   ├── hns3/                        # CPU 卡死在重启
│   │   ├── server2-99-101/              # DPU 打流卡顿
│   │   ├── dpu_iperf/
│   │   ├── rte_kni/
│   │   ├── ssh/                         # DPU 节点软锁 panic
│   │   └── cinder/
│   ├── port-forward/
│   ├── proxy/mihomo/
│   ├── Linux网络收包与epoll协作机制.md
│   ├── network.md
│   └── vxlan入门.md
│
├── security/                            # 内核安全
│   └── 系统安全.md
│
├── drivers/                             # 设备驱动
│   ├── gpu/
│   │   ├── nvidia-svm/                  # HMM → DRM GPUSVM → 显存管理 (9 篇)
│   │   ├── stanford-cs336/              # Stanford 大模型课程笔记 (17 讲)
│   │   ├── grtrace/docs/                # GPU Ring Buffer 追踪架构
│   │   ├── ascend/                      # 昇腾 AI C 算子编程、训练/推理调优
│   │   ├── bugs/                        # GPU freeze/pt620k/236691
│   │   ├── virtio/                      # virtio_gpu TTM hang
│   │   ├── openclaw/
│   │   ├── ai_infra.md、gpu.md、gpu_buddy_analysis.md
│   │   └── kernel_ai_infra.md
│   ├── sound/
│   │   ├── bugs/                        # HDMI ALC885 bug
│   │   ├── pulseaudio/                  # PulseAudio 源码深度解析 (6 篇)
│   │   ├── phytium/                     # 飞腾 DP 声卡、x100 套片、主线补丁
│   │   ├── sound.md、sound-algorithm.md
│   │   └── hda_codec.md、hdmi-audio.md
│   ├── console/
│   │   └── bugs/console_unlock/         # devkmsg_write + do_coredump
│   ├── disk/
│   │   ├── bcache/                      # bcache 日志分析
│   │   ├── ceph/                        # Ceph 集群 nvme 故障
│   │   └── nvme_ioctl/
│   ├── block/
│   │   └── bugs/ceph/                   # Ceph RBD fio 性能分析
│   ├── modules/
│   │   └── bugs/insmod/                 # Ubuntu22.04 insmod panic
│   ├── acpi/                            # power_button
│   ├── proc/                            # seq_open vs single_open
│   ├── power/bugs/
│   ├── mcu/                             # MCU 假复位信号
│   ├── udl/                             # udl mutex_lock panic
│   ├── infiniband/
│   └── 设备驱动.md
│
├── virt/                                # 虚拟化
│   ├── kvm/
│   │   ├── kickstart/                   # 无人值守安装 (ctyun, kylin, debian)
│   │   ├── qemu/                        # QEMU 编译调试
│   │   ├── virsh/
│   │   └── books/
│   └── container/
│       ├── docker/                      # Dockerfile 集合 (ctyunos, ubuntu, debian)
│       ├── k8s/                         # K8s 集群部署、linglong
│       └── uts_namespace/
│
├── gdb/                                 # GDB 调试
│   ├── gdb/                             # gdbinit、中文调试手册
│   └── debug.md
│
├── kdump/                               # kdump 崩溃分析
│   └── kdump/sysrq_trigger/             # kgdb + sysrq 触发 dump
│
├── assembly/                            # 汇编 & 二进制分析
│   ├── binary-analysis/                 # 二进制分析实战
│   ├── 16位汇编语言.md
│   └── assembly.md
│
├── books/                               # 内核书籍 PDF
│   ├── UEFI编程实践/
│   └── 20+ 内核经典书籍
│
├── boot/                                # 系统启动
│   ├── grub/                            # GRUB iommu=pt、vmlinuz initramfs
│   └── monitoring/                      # 监控日志 (systemd, boot)
│
├── tools/                               # 开发工具
│   ├── shell/                           # 内核编译/打包脚本
│   ├── vim/                             # Vim 配置与手册
│   ├── tmux/
│   ├── ssh/
│   ├── distro/                          # 发行版 (deepin KMS, deb/rpm 打包)
│   └── testing/ltp/
│
├── README.md
└── linux教程.md
```

---

## 编程语言

| [cpp/](cpp/) | C/C++ — 语法、多线程、Qt、cmake、面试 |
| [python/](python/) | Python |
| [go/](go/) | Go |
| [rust/](rust/) | Rust (cargo) |
| [java/](java/) | Java |
| [javascript/](javascript/) | JavaScript / Vue |

## 架构与算法

| [architect/](architect/) | 系统架构师 — OS、网络、数据库、计组 |
| [algorithm/](algorithm/) | 算法、Transformer、LeetCode |

## 工具

| [git/](git/) | Git (github-pages, b4, gerrit) |
| [jenkins/](jenkins/) | CI/CD |
| [nginx/](nginx/) | Nginx |
| [markdown/](markdown/) | Markdown、LaTeX、Doxygen、UML |

## 其他

| [sql/](sql/) | MySQL |
| [redis/](redis/) | Redis |
| [3d/](3d/) | 3D 建模 |
| [patent/](patent/) | 专利文档 (flatpak, 玲珑) |
| [svn/](svn/) | SVN |