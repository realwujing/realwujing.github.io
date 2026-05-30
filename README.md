# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## 🐧 Linux 内核

对标 Linux 内核源码树，按子系统组织。

-   ⚙️ [kernel/](linux/kernel/) — 内核核心 (sched、locking、rcu、irq、trace、sources)
    -   [sched/](linux/kernel/sched/) — 进程调度 (CFS、isolcpus)
    -   [locking/](linux/kernel/locking/) — 锁机制 (spin_lock、hard lockup)
    -   [rcu/](linux/kernel/rcu/) — RCU (stall、demo)
    -   [irq/](linux/kernel/irq/) — 中断 (8250 UAF、SDEI、tickless)
    -   [trace/](linux/kernel/trace/) — 追踪 & 性能
        -   [bpf/](linux/kernel/trace/bpf/) — BPF/BCC/bpftrace、[上游 patch](linux/kernel/trace/bpf/patch/bpf-verifier-ID映射重置优化.md) ✨
        -   [events/](linux/kernel/trace/events/) — perf 事件、CVE、UnixBench、[上游 patch](linux/kernel/trace/events/patch/hardlockup-perf-event无状态重构.md) ✨
        -   [stap/](linux/kernel/trace/stap/) — SystemTap
        -   [bugs/](linux/kernel/trace/bugs/) — ftrace 损坏修复
    -   [sources/](linux/kernel/sources/) — Linux 0.11 源码
-   🧠 [mm/](linux/mm/) — 内存管理
    -   [bugs/](linux/mm/bugs/) — 238303(kmemleak)、ksmd、slab、vmap、deferred_split、si_mem_available
-   📁 [fs/](linux/fs/) — 文件系统
    -   [bugs/](linux/fs/bugs/) — ext4、kernfs、dcache、fuse
    -   [minifs/](linux/fs/minifs/) — 手写 mini 文件系统 v1~v3
    -   [patch/](linux/fs/patch/) — [close_range() 稀疏 FD 优化](linux/fs/patch/close_range稀疏FD优化.md) ✨ 上游已合入
-   🌐 [net/](linux/net/) — 网络栈
    -   [bugs/](linux/net/bugs/) — ovs-veth-peer、vhost-user-vring、hns3、bandwidth
    -   [patch/](linux/net/patch/) — [netns 批量 unhash 优化](linux/net/patch/netns批量unhash优化.md) ✨ 上游已合入
    -   [port-forward/](linux/net/port-forward/) — 端口转发脚本
    -   [proxy/mihomo/](linux/net/proxy/mihomo/) — Mihomo 代理
-   🔒 [security/](linux/security/) — 内核安全
-   🔌 [drivers/](linux/drivers/) — 设备驱动
    -   [gpu/](linux/drivers/gpu/) — GPU
        -   [nvidia-svm/](linux/drivers/gpu/nvidia-svm/) — HMM → GPUSVM → 显存管理 (9 篇)
        -   [stanford-cs336/](linux/drivers/gpu/stanford-cs336/) — Stanford 大模型课程笔记 (17 讲)
        -   [ascend/](linux/drivers/gpu/ascend/) — 昇腾 AI C 算子、训练/推理调优
        -   [grtrace/docs/](linux/drivers/gpu/grtrace/docs/) — GPU Ring Buffer 追踪架构
        -   [bugs/](linux/drivers/gpu/bugs/) — freeze/pt620k/236691
        -   [virtio/](linux/drivers/gpu/virtio/) — virtio_gpu TTM hang
    -   [sound/](linux/drivers/sound/) — 声卡 (pulseaudio 6 篇、phytium、HDMI bug)
    -   [console/bugs/](linux/drivers/console/bugs/) — devkmsg_write、do_coredump
    -   [disk/](linux/drivers/disk/) — bcache、ceph、nvme_ioctl
    -   [block/bugs/ceph/](linux/drivers/block/bugs/ceph/) — Ceph RBD 性能分析
    -   [modules/bugs/](linux/drivers/modules/bugs/) — insmod panic
    -   [power/bugs/](linux/drivers/power/bugs/) — S4 suspend
    -   [proc/](linux/drivers/proc/) — seq_open vs single_open
    -   [mcu/](linux/drivers/mcu/) — MCU 假复位信号
    -   [udl/](linux/drivers/udl/) — udl mutex_lock panic
-   🖥️ [virt/](linux/virt/) — 虚拟化
    -   [kvm/](linux/virt/kvm/) — KVM (kickstart、qemu、10+ 调试文档)
    -   [container/](linux/virt/container/) — Docker、K8s、uts_namespace、cgroup
-   🐛 [gdb/](linux/gdb/) — GDB 调试 (中文手册、gdbinit)
-   💥 [kdump/](linux/kdump/) — kdump 崩溃分析 (sysrq_trigger)
-   🔬 [assembly/](linux/assembly/) — 汇编 & 二进制分析
-   📖 [books/](linux/books/) — 内核书籍 (20+ 经典 PDF)
-   🚀 [boot/](linux/boot/) — 系统启动 (GRUB、monitoring)
-   🛠️ [tools/](linux/tools/) — 开发工具 (shell、vim、distro、ltp)

---

## 💻 编程语言

-   [cpp/](cpp/) — C/C++ — 语法、多线程、Qt、cmake
-   [python/](python/) — Python
-   [go/](go/) — Go
-   [rust/](rust/) — Rust
-   [java/](java/) — Java
-   [javascript/](javascript/) — JavaScript / Vue

## 🏗️ 架构 & 算法

-   [architect/](architect/) — 系统架构师 — OS、网络、数据库、计组
-   [algorithm/](algorithm/) — 算法、Transformer、LeetCode

## 🔧 工具

-   [git/](git/) — Git — github-pages、b4、gerrit
-   [jenkins/](jenkins/) — CI/CD
-   [nginx/](nginx/) — Nginx
-   [markdown/](markdown/) — Markdown、LaTeX、UML

## 💾 其他

-   [sql/](sql/) — MySQL
-   [redis/](redis/) — Redis
-   [3d/](3d/) — 3D 建模
-   [patent/](patent/) — 专利文档
-   [svn/](svn/) — SVN