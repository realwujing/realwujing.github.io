# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## 🐧 Linux 内核

对标 Linux 内核源码树结构，按子系统组织。

```
linux/
├── kernel/                              # 内核核心子系统
│   ├── sched/                           # 进程调度 (CFS、isolcpus)
│   │   └── bugs/                        # 调度 bug 分析
│   ├── locking/                         # 锁机制 (spin_lock、hard lockup)
│   │   └── reboot/
│   ├── rcu/                             # RCU
│   │   ├── bugs/                        # RCU stall 分析
│   │   └── demo/
│   ├── irq/                             # 中断子系统
│   │   ├── bugs/                        # 8250 UAF、SDEI hard lockup
│   │   └── tick/
│   ├── trace/                           # 追踪与性能
│   │   ├── bpf/                         # BPF/BCC/bpftrace
│   │   ├── events/                      # perf 事件、CVE、UnixBench
│   │   ├── stap/                        # SystemTap
│   │   └── bugs/                        # ftrace 损坏
│   ├── sources/                         # Linux 0.11 源码
│   ├── kernel.md                        # 内核学习资源索引
│   └── Linux Kernel Quick Guide.md
│
├── mm/                                  # 内存管理
│   ├── bugs/
│   │   ├── 238303/                      # kmemleak 系列
│   │   ├── ksmd/                        # KSM 隔离核延迟
│   │   ├── insert_vmap_area_augment/
│   │   ├── deferred_split_scan/         # 虚拟机 Oops
│   │   ├── kernel-dynamic-memory/
│   │   └── si_mem_available/
│   ├── min_free_kbytes_gfp_atomic.md
│   └── 内存管理.md
│
├── fs/                                  # 文件系统
│   ├── bugs/
│   │   ├── ext4/                        # ext4 inode double free
│   │   ├── kernfs/                      # kernfs slab 膨胀
│   │   ├── dcache/                      # dcache 非法地址访问
│   │   ├── fuse/                        # alluxio-fuse EIO
│   │   └── ...
│   ├── minifs/                          # 手写 mini 文件系统 v1~v3
│   └── 文件系统.md
│
├── net/                                 # 网络栈
│   ├── bugs/
│   │   ├── ovs-veth-peer/               # netdev_pick_tx soft lockup
│   │   ├── vhost-user-vring/            # OVS-DPDK 丢包
│   │   ├── netstamp_clear/              # text_poke soft lockup
│   │   ├── bandwidth/                   # VM 带宽分析
│   │   └── ...
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
│   │   ├── nvidia-svm/                  # HMM → DRM GPUSVM → 显存管理
│   │   ├── stanford-cs336/              # Stanford 大模型课程笔记
│   │   ├── grtrace/docs/                # GPU Ring Buffer 追踪
│   │   ├── bugs/                        # GPU freeze bug 分析
│   │   ├── virtio/                      # virtio_gpu TTM 分析
│   │   └── openclaw/
│   ├── sound/
│   │   ├── bugs/                        # HDMI 声卡 bug
│   │   ├── pulseaudio/                  # PulseAudio 源码
│   │   └── phytium/                     # 飞腾声卡补丁
│   ├── console/bugs/console_unlock/
│   ├── modules/
│   │   ├── bugs/insmod/                 # insmod panic
│   │   └── README.md
│   ├── block/bugs/ceph/
│   ├── proc/                            # seq_open vs single_open
│   ├── power/bugs/
│   ├── mcu/
│   ├── udl/
│   └── 设备驱动.md
│
├── virt/                                # 虚拟化
│   ├── kvm/
│   │   ├── kickstart/                   # kickstart 无人值守安装
│   │   ├── qemu/                        # QEMU 编译调试
│   │   ├── virsh/
│   │   └── books/
│   └── container/
│       ├── docker/                      # Dockerfile 集合
│       ├── k8s/                         # K8s 集群部署
│       └── uts_namespace/
│
├── gdb/                                 # GDB 调试
│   ├── gdb/
│   │   ├── README.md
│   │   ├── .gdbinit
│   │   └── Debugging.with.gdb 中文.pdf
│   └── debug.md
│
├── kdump/                               # kdump 崩溃分析
│   └── kdump/sysrq_trigger/             # kgdb + sysrq 触发
│
├── assembly/                            # 汇编 & 二进制分析
│   ├── binary-analysis/
│   ├── 16位汇编语言.md
│   └── assembly.md
│
├── books/                               # 内核书籍 PDF
│   ├── UEFI编程实践/
│   └── Linux内核设计与实现(第三版).pdf ...
│
├── boot/                                # 系统启动
│   ├── grub/                            # GRUB 配置
│   └── monitoring/                      # 监控日志
│
├── tools/                               # 开发工具
│   ├── shell/                           # 编译脚本
│   ├── vim/                             # Vim 配置
│   ├── tmux/
│   ├── ssh/
│   ├── distro/                          # 发行版打包 (deb/rpm)
│   └── testing/ltp/
│
├── README.md
└── linux教程.md
```

---

## 💻 编程语言

| 目录 | 内容 |
|---|---|
| [cpp/](cpp/) | C/C++ — 语法、多线程、Qt、cmake、面试 |
| [python/](python/) | Python |
| [go/](go/) | Go |
| [rust/](rust/) | Rust |
| [java/](java/) | Java |
| [javascript/](javascript/) | JavaScript / Vue |

## 🏗️ 架构与算法

| 目录 | 内容 |
|---|---|
| [architect/](architect/) | 系统架构师 — OS、网络、数据库、计组 |
| [algorithm/](algorithm/) | 算法、Transformer、LeetCode |

## 🔧 工具

| 目录 | 内容 |
|---|---|
| [git/](git/) | Git |
| [jenkins/](jenkins/) | CI/CD |
| [nginx/](nginx/) | Nginx |
| [markdown/](markdown/) | Markdown、LaTeX、UML |

## 💾 存储 & 其他

| 目录 | 内容 |
|---|---|
| [sql/](sql/) | SQL / MySQL |
| [redis/](redis/) | Redis |
| [3d/](3d/) | 3D 建模 |
| [patent/](patent/) | 专利 |
| [svn/](svn/) | SVN |