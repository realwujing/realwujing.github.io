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
            -   [BPF verifier ID 映射重置优化](linux/kernel/trace/bpf/patch/bpf-verifier-ID映射重置优化.md) ✨ 上游已合入
        -   [events/](linux/kernel/trace/events/) — perf 事件、CVE、UnixBench
            -   [hardlockup perf event 无状态重构](linux/kernel/trace/events/patch/hardlockup-perf-event无状态重构.md) ✨ 上游已合入
        -   [stap/](linux/kernel/trace/stap/) — SystemTap
        -   [bugs/](linux/kernel/trace/bugs/) — ftrace 损坏
    -   [sources/](linux/kernel/sources/) — Linux 0.11 源码
    -   [Linux内核学习资源](linux/kernel/kernel.md) — 1000+ 外部链接索引
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
    -   [close_range() 稀疏 FD 优化](linux/fs/patch/close_range稀疏FD优化.md) ✨ Christian Brauner 合入
-   [net/](linux/net/) — 网络栈
    -   [bugs/](linux/net/bugs/) — ovs-veth-peer、vhost-user-vring、hns3
        -   [ovs-veth-peer/](linux/net/bugs/ovs-veth-peer/) — netdev_pick_tx soft lockup
        -   [vhost-user-vring/](linux/net/bugs/vhost-user-vring/) — OVS-DPDK 丢包
        -   [bandwidth/](linux/net/bugs/bandwidth/) — 海光4号 VM 带宽
        -   [ssh/](linux/net/bugs/ssh/) — DPU ssh 软锁
    -   [netns 批量 unhash 优化](linux/net/patch/netns批量unhash优化.md) ✨ Jakub Kicinski 合入
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
-   [assembly/](linux/assembly/) — 汇编 & 二进制分析
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

📝 [linux教程](linux/linux教程.md) — Linux 学习教程资源大全 (2200+ 行链接)

---

## 💻 编程语言

-   [C/C++](cpp/)
    -   [C++基础](cpp/C++基础.md)
    -   [C++多线程](cpp/C++多线程.md)
    -   [C++命名规范](cpp/C++命名规范.md)
    -   [C++进阶指南](cpp/C++进阶指南.md)
    -   [C++日志](cpp/C++日志.md)
    -   [C 实现 C++ 类](cpp/C%20实现%20C++%20类/) — 多态、封装、继承
    -   [C++对象的内存布局](cpp/C++对象的内存布局/)
    -   [Qt教程](cpp/Qt教程.md)
    -   [qt-learning/](cpp/qt-learning/)
        -   [dbus/](cpp/qt-learning/dbus/) — proxy、struct
        -   [qprocess_wget/](cpp/qt-learning/qprocess_wget/)
    -   [thread/](cpp/thread/) — 多线程
        -   [pthread/ThreadPool/](cpp/thread/pthread/ThreadPool/)
        -   [pthread/sync/](cpp/thread/pthread/sync/)
        -   [thread/](cpp/thread/thread/)
    -   [CMake](cpp/cmake.md)
    -   [MakeFile](cpp/makefile.md)
    -   [cmake-objdump/](cpp/cmake-objdump/)
    -   [GCC编译器使用指南](cpp/gcc.md)
    -   [Automake与Autoconf构建工具](cpp/automake.md)
    -   [protobuf/](cpp/protobuf/)
    -   [TARS微服务框架](cpp/tars.md)
    -   [vscode-cmake](cpp/vscode-cmake.md)
    -   [valgrind/](cpp/valgrind/)
    -   [C语言extern关键字详解](cpp/extern/extern.md)
    -   [GTK+与GLib图形界面开发](cpp/gtk.md)
    -   [静态库与动态库](cpp/静态库与动态库.md)
-   [Python](python/)
    -   [机器学习 周志华(PDF)](python/books/机器学习%20周志华.pdf)
-   [Go](go/)
-   [Rust](rust/)
    -   [cargo教程](rust/cargo教程.md)
-   [Java](java/)
    -   [Java编程学习指南](java/java-learning.md)
    -   [design/](java/design/)
    -   [reactor/](java/reactor/)
-   [JavaScript / Vue](javascript/)
    -   [my-project/](javascript/my-project/)
    -   [runoob-vue3-test/](javascript/runoob-vue3-test/)
    -   [todolist/](javascript/todolist/)
    -   [vue2_runoob/](javascript/vue2_runoob/)
    -   [vue3_runoob/](javascript/vue3_runoob/)

## 🏗️ 架构 & 算法

-   [系统架构师](architect/)
    -   [操作系统](architect/操作系统.md)
    -   [计算机网络](architect/计算机网络.md)
    -   [数据库系统](architect/数据库系统.md)
    -   [计算机组成与体系结构](architect/计算机组成与体系结构.md)
    -   [数学与经济管理](architect/数学与经济管理.md)
    -   [考试介绍及备考攻略](architect/考试介绍及备考攻略.md)
    -   [system-architect](architect/system-architect.md)
    -   [系统架构设计师考试笔记(PDF)](architect/系统架构设计师考试笔记.pdf)
    -   [xisai/](architect/xisai/) — 希赛备考资料
        -   [202409/](architect/xisai/202409/) — 思维导图、学习手册、论文范文、默写本、速记知识点
        -   [知识点集锦](architect/xisai/2024年系统架构设计师知识点集锦.pdf)
        -   [经典100题](architect/xisai/2024年系统架构设计师经典100题.pdf)
        -   [专业英语高频词汇表](architect/xisai/【新版】架构师专业英语高频词汇表.pdf)
-   [算法](algorithm/)
    -   [算法与数据结构学习指南](algorithm/algorithm.md)
    -   [Transformer 算法详解与 PyTorch 实现](algorithm/transformer.md)

## 🔧 工具

-   [Git](git/)
    -   [git教程](git/git教程.md)
    -   [git submodules](git/submodule.md)
    -   [b4 内核补丁管理利器](git/b4-usage.md)
    -   [Gerrit代码审查系统](git/gerrit.md)
    -   [github-pages](git/github-pages.md)
-   [Jenkins CI/CD](jenkins/jenkins.md)
-   [Nginx Web服务器配置指南](nginx/nginx.md)
-   [文档工具](markdown/)
    -   [Markdown文档编写与工具集](markdown/markdown.md)
    -   [LaTeX排版系统](markdown/latex.md)
    -   [Doxygen文档生成工具](markdown/doxygen.md)
    -   [UML统一建模语言与PlantUML](markdown/uml.md)
    -   [Nano文本编辑器使用指南](markdown/nano.md)
    -   [fluid 主题自定义标签页小图标](markdown/hexo/fluid/fluid主题自定义标签页小图标.md)

## 💾 其他

-   [MySQL](sql/mysql教程.md)
-   [Redis缓存数据库](redis/redis.md)
-   [3D设计与建模工具](3d/3d.md)
-   [专利](patent/)
    -   [Flatpak应用打包与分发](patent/flatpak.md)
    -   [玲珑应用包管理系统](patent/linglong.md)
    -   [玲珑仓库概要设计说明书](patent/玲珑仓库概要设计说明书.md)
-   [SVN版本控制系统使用指南](svn/svn.md)