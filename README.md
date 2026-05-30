# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## 🐧 Linux 内核

对标 Linux 内核源码树，按子系统组织。

-   [kernel/](linux/kernel/) — 内核核心
    -   [sched/](linux/kernel/sched/) — 进程调度
        -   [bugs/](linux/kernel/sched/bugs/) — cfs、k8s-pod-vm
    -   [locking/](linux/kernel/locking/) — 锁机制 (spin_lock、hard lockup)
    -   [rcu/](linux/kernel/rcu/) — RCU
        -   [bugs/](linux/kernel/rcu/bugs/) — rcu hard lockup、dpu iperf
    -   [irq/](linux/kernel/irq/) — 中断子系统
        -   [bugs/](linux/kernel/irq/bugs/) — 8250 UAF、SDEI
        -   [tick/tickless/](linux/kernel/irq/tick/tickless/) — NO_HZ_FULL + RCU Stall
    -   [trace/](linux/kernel/trace/) — 追踪 & 性能
        -   [bpf/](linux/kernel/trace/bpf/) — BPF/BCC/bpftrace
            -   [patch/](linux/kernel/trace/bpf/patch/bpf-verifier-ID映射重置优化.md) ✨ verifier ID 优化
        -   [events/](linux/kernel/trace/events/) — perf 事件、CVE、UnixBench
            -   [patch/](linux/kernel/trace/events/patch/hardlockup-perf-event无状态重构.md) ✨ hardlockup 重构
        -   [stap/](linux/kernel/trace/stap/) — SystemTap
        -   [bugs/](linux/kernel/trace/bugs/) — ftrace 损坏
    -   [sources/](linux/kernel/sources/) — Linux 0.11 源码
-   [mm/](linux/mm/) — 内存管理
    -   [bugs/](linux/mm/bugs/) — 238303(kmemleak)、ksmd、slab
        -   [238303/](linux/mm/bugs/238303/) — kmemleak 系列
        -   [ksmd/](linux/mm/bugs/ksmd/) — KSM 隔离核延迟
        -   [deferred_split_scan/](linux/mm/bugs/deferred_split_scan/) — 虚拟机 Oops
        -   [insert_vmap_area_augment/](linux/mm/bugs/insert_vmap_area_augment/) — vmap 增强分析
-   [fs/](linux/fs/) — 文件系统
    -   [bugs/](linux/fs/bugs/) — ext4、kernfs、dcache、fuse
        -   [ext4/233267/](linux/fs/bugs/ext4/) — double free、dentry UAF
        -   [kernfs/](linux/fs/bugs/kernfs/) — slab 膨胀
        -   [dcache/](linux/fs/bugs/dcache/) — 非法地址 0x60e
    -   [minifs/](linux/fs/minifs/) — 手写 mini 文件系统 v1~v3
    -   [patch/](linux/fs/patch/close_range稀疏FD优化.md) ✨ close_range() 稀疏 FD 优化
-   [net/](linux/net/) — 网络栈
    -   [bugs/](linux/net/bugs/) — ovs-veth-peer、vhost-user-vring、hns3
        -   [ovs-veth-peer/](linux/net/bugs/ovs-veth-peer/) — netdev_pick_tx soft lockup
        -   [vhost-user-vring/](linux/net/bugs/vhost-user-vring/) — OVS-DPDK 丢包
        -   [bandwidth/](linux/net/bugs/bandwidth/) — 海光4号 VM 带宽
        -   [ssh/](linux/net/bugs/ssh/) — DPU ssh 软锁
    -   [patch/](linux/net/patch/netns批量unhash优化.md) ✨ netns 批量 unhash
    -   [port-forward/](linux/net/port-forward/) — 端口转发脚本
    -   [proxy/mihomo/](linux/net/proxy/mihomo/) — Mihomo 代理
-   [security/](linux/security/) — 内核安全
-   [drivers/](linux/drivers/) — 设备驱动
    -   [gpu/](linux/drivers/gpu/) — GPU
        -   [nvidia-svm/](linux/drivers/gpu/nvidia-svm/) — HMM → 显存管理 (9 篇)
        -   [stanford-cs336/](linux/drivers/gpu/stanford-cs336/) — 大模型课程笔记 (17 讲)
        -   [ascend/](linux/drivers/gpu/ascend/) — 昇腾 AI C 算子
        -   [grtrace/docs/](linux/drivers/gpu/grtrace/docs/) — GPU Ring Buffer 追踪
        -   [bugs/](linux/drivers/gpu/bugs/) — freeze/pt620k/236691
        -   [virtio/](linux/drivers/gpu/virtio/) — virtio_gpu TTM hang
    -   [sound/](linux/drivers/sound/) — 声卡
        -   [pulseaudio/](linux/drivers/sound/pulseaudio/) — PulseAudio 源码 (6 篇)
        -   [phytium/](linux/drivers/sound/phytium/) — 飞腾 DP 声卡
        -   [bugs/](linux/drivers/sound/bugs/) — HDMI ALC885/ALC257
    -   [console/bugs/](linux/drivers/console/bugs/) — devkmsg_write、do_coredump
    -   [disk/](linux/drivers/disk/) — bcache、ceph、nvme_ioctl
        -   [bcache/](linux/drivers/disk/bcache/) — bcache 日志分析
        -   [ceph/](linux/drivers/disk/ceph/) — Ceph 集群故障
    -   [block/bugs/ceph/](linux/drivers/block/bugs/ceph/) — Ceph RBD 性能分析
    -   [modules/bugs/insmod/](linux/drivers/modules/bugs/insmod/) — insmod panic
    -   [power/bugs/236037/](linux/drivers/power/bugs/) — S4 suspend
    -   [proc/](linux/drivers/proc/) — seq_open vs single_open
    -   [mcu/](linux/drivers/mcu/) — MCU 假复位信号
    -   [udl/](linux/drivers/udl/) — udl mutex_lock panic
-   [virt/](linux/virt/) — 虚拟化
    -   [kvm/](linux/virt/kvm/) — KVM
        -   [kickstart/](linux/virt/kvm/kickstart/) — 无人值守安装 (ctyun、kylin、debian)
        -   [qemu/](linux/virt/kvm/qemu/) — QEMU 编译调试
        -   [virsh/](linux/virt/kvm/virsh/) — virsh + gdb 调试
        -   [books/](linux/virt/kvm/books/) — KVM 书籍
    -   [container/](linux/virt/container/) — 容器
        -   [docker/](linux/virt/container/docker/) — Dockerfile (ctyunos、ubuntu、debian)
        -   [k8s/](linux/virt/container/k8s/) — K8s 集群部署、linglong
        -   [uts_namespace/](linux/virt/container/uts_namespace/) — UTS namespace
-   [gdb/](linux/gdb/) — GDB 调试
    -   [gdb/](linux/gdb/gdb/) — 中文手册、gdbinit
-   [kdump/](linux/kdump/) — kdump 崩溃分析
    -   [kdump/sysrq_trigger/](linux/kdump/kdump/sysrq_trigger/) — kgdb + sysrq
-   [assembly/](linux/assembly/) — 汇编
    -   [binary-analysis/](linux/assembly/binary-analysis/) — 二进制分析实战
-   [books/](linux/books/) — 内核书籍 (20+ PDF)
    -   [UEFI编程实践/](linux/books/UEFI编程实践/)
-   [boot/](linux/boot/) — 系统启动
    -   [grub/](linux/boot/grub/) — GRUB iommu=pt
    -   [monitoring/log/](linux/boot/monitoring/) — 系统日志
-   [tools/](linux/tools/) — 开发工具
    -   [shell/](linux/tools/shell/) — 编译脚本
    -   [vim/](linux/tools/vim/) — Vim 配置
    -   [distro/](linux/tools/distro/) — 发行版
        -   [pkg/deb/](linux/tools/distro/pkg/deb/) — debmake 教程
        -   [pkg/rpm-ostree/](linux/tools/distro/pkg/rpm-ostree/) — rpm-ostree
    -   [testing/ltp/](linux/tools/testing/ltp/) — LTP 测试分析

[📝 linux教程.md](linux/linux教程.md) — Linux 学习资源大全 (2200+ 行链接)

---

## 💻 编程语言

-   [cpp/](cpp/) — C/C++
    -   [C++基础.md](cpp/C++基础.md) · [C++多线程.md](cpp/C++多线程.md) · [C++命名规范.md](cpp/C++命名规范.md)
    -   [C++进阶指南.md](cpp/C++进阶指南.md) · [C++日志.md](cpp/C++日志.md)
    -   [C 实现 C++ 类/](cpp/C%20实现%20C++%20类/) — 多态、封装、继承
    -   [C++对象的内存布局/](cpp/C++对象的内存布局/)
    -   [Qt教程.md](cpp/Qt教程.md) · [qt-learning/](cpp/qt-learning/)
        -   [dbus/](cpp/qt-learning/dbus/) — proxy、struct
        -   [qprocess_wget/](cpp/qt-learning/qprocess_wget/)
    -   [thread/](cpp/thread/) — 多线程
        -   [pthread/ThreadPool/](cpp/thread/pthread/ThreadPool/) · [pthread/sync/](cpp/thread/pthread/sync/)
        -   [thread/](cpp/thread/thread/)
    -   [cmake.md](cpp/cmake.md) · [makefile.md](cpp/makefile.md) · [cmake-objdump/](cpp/cmake-objdump/)
    -   [gcc.md](cpp/gcc.md) · [automake.md](cpp/automake.md)
    -   [protobuf/](cpp/protobuf/) · [tars.md](cpp/tars.md)
    -   [vscode-cmake.md](cpp/vscode-cmake.md) · [valgrind/](cpp/valgrind/)
    -   [extern/extern.md](cpp/extern/extern.md) · [gtk.md](cpp/gtk.md)
    -   [静态库与动态库.md](cpp/静态库与动态库.md)
-   [python/](python/) — Python
    -   [books/机器学习 周志华.pdf](python/books/机器学习%20周志华.pdf)
-   [go/](go/) — Go
-   [rust/](rust/) — Rust
    -   [cargo教程.md](rust/cargo教程.md)
-   [java/](java/) — Java
    -   [java-learning.md](java/java-learning.md) · [design/](java/design/) · [reactor/](java/reactor/)
-   [javascript/](javascript/) — JavaScript / Vue
    -   [my-project/](javascript/my-project/) · [runoob-vue3-test/](javascript/runoob-vue3-test/)
    -   [todolist/](javascript/todolist/) · [vue2_runoob/](javascript/vue2_runoob/) · [vue3_runoob/](javascript/vue3_runoob/)

## 🏗️ 架构 & 算法

-   [architect/](architect/) — 系统架构师
    -   [操作系统.md](architect/操作系统.md) · [计算机网络.md](architect/计算机网络.md)
    -   [数据库系统.md](architect/数据库系统.md) · [计算机组成与体系结构.md](architect/计算机组成与体系结构.md)
    -   [数学与经济管理.md](architect/数学与经济管理.md)
    -   [考试介绍及备考攻略.md](architect/考试介绍及备考攻略.md)
    -   [system-architect.md](architect/system-architect.md)
    -   [系统架构设计师考试笔记.pdf](architect/系统架构设计师考试笔记.pdf)
    -   [xisai/](architect/xisai/) — 希赛备考资料
        -   [202409/](architect/xisai/202409/) — 思维导图、学习手册、论文范文、默写本、速记知识点
        -   [2024年知识点集锦.pdf](architect/xisai/2024年系统架构设计师知识点集锦.pdf)
        -   [经典100题.pdf](architect/xisai/2024年系统架构设计师经典100题.pdf)
        -   [专业英语高频词汇表.pdf](architect/xisai/【新版】架构师专业英语高频词汇表.pdf)
-   [algorithm/](algorithm/) — 算法
    -   [algorithm.md](algorithm/algorithm.md) · [transformer.md](algorithm/transformer.md)

## 🔧 工具

-   [git/](git/) — Git
    -   [git教程.md](git/git教程.md) · [submodule.md](git/submodule.md)
    -   [b4-usage.md](git/b4-usage.md) · [gerrit.md](git/gerrit.md) · [github-pages.md](git/github-pages.md)
-   [jenkins/](jenkins/) — Jenkins CI/CD
    -   [jenkins.md](jenkins/jenkins.md)
-   [nginx/](nginx/) — Nginx
    -   [nginx.md](nginx/nginx.md)
-   [markdown/](markdown/) — 文档工具
    -   [markdown.md](markdown/markdown.md) · [latex.md](markdown/latex.md)
    -   [doxygen.md](markdown/doxygen.md) · [uml.md](markdown/uml.md) · [nano.md](markdown/nano.md)
    -   [hexo/fluid/](markdown/hexo/fluid/) — fluid 主题自定义标签页小图标

## 💾 其他

-   [sql/](sql/) — MySQL
    -   [mysql教程.md](sql/mysql教程.md)
-   [redis/](redis/) — Redis
    -   [redis.md](redis/redis.md)
-   [3d/](3d/) — 3D 建模
    -   [3d.md](3d/3d.md)
-   [patent/](patent/) — 专利
    -   [flatpak.md](patent/flatpak.md) · [linglong.md](patent/linglong.md)
    -   [玲珑仓库概要设计说明书.md](patent/玲珑仓库概要设计说明书.md)
-   [svn/](svn/) — SVN
    -   [svn.md](svn/svn.md)