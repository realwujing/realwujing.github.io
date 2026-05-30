# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

## 🐧 Linux 内核

对标 Linux 内核源码树结构，按子系统组织。

### 核心子系统

| 目录 | 内容 |
|---|---|
| [kernel/](linux/kernel/) | 内核核心 — [sched](linux/kernel/sched/) 调度、[locking](linux/kernel/locking/) 锁机制、[rcu](linux/kernel/rcu/) RCU、[irq](linux/kernel/irq/) 中断 |
| [mm/](linux/mm/) | 内存管理 — page alloc、slab、ksm、kmemleak 分析 |
| [fs/](linux/fs/) | 文件系统 — ext4、procfs、kernfs、minifs、fuse |
| [net/](linux/net/) | 网络栈 — 收包、vxlan、vhost-user、ovs-dpdk |
| [security/](linux/security/) | 内核安全 |
| [drivers/](linux/drivers/) | 设备驱动 — GPU、声卡、串口、console、block |
| [virt/](linux/virt/) | 虚拟化 — KVM、QEMU、Docker、K8s、容器 |

### 性能追踪

| 目录 | 内容 |
|---|---|
| [kernel/trace/](linux/kernel/trace/) | 追踪 & 性能 — [bpf](linux/kernel/trace/bpf/) BPF、[events](linux/kernel/trace/events/) perf_event、[stap](linux/kernel/trace/stap/) SystemTap、性能调优 |
| [kernel/trace/events/](linux/kernel/trace/events/) | perf 事件 — CVE 分析、UnixBench 基准测试、perf 脚本 |

### 调试 & 汇编

| 目录 | 内容 |
|---|---|
| [gdb/](linux/gdb/) | GDB 内核/应用调试 |
| [kdump/](linux/kdump/) | kdump 内核 crash dump 分析 |
| [assembly/](linux/assembly/) | 汇编语言学习、二进制分析 |
| [books/](linux/books/) | 内核书籍 PDF |

### 其他

| 目录 | 内容 |
|---|---|
| [tools/](linux/tools/) | 开发工具配置 — shell, vim, tmux, ssh |
| [boot/](linux/boot/) | 系统启动 — GRUB 配置 |
| [tools/distro/](linux/tools/distro/) | 发行版 — deb/rpm 打包 |
| [linux教程.md](linux/linux教程.md) | Linux 学习资源索引 |

---

## 💻 编程语言

| 目录 | 内容 |
|---|---|
| [cpp/](cpp/) | C/C++ — 语法、多线程、Qt、cmake、面试 |
| [python/](python/) | Python |
| [go/](go/) | Go 语言 |
| [rust/](rust/) | Rust |
| [java/](java/) | Java |
| [javascript/](javascript/) | JavaScript / Vue |

## 🏗️ 架构与算法

| 目录 | 内容 |
|---|---|
| [architect/](architect/) | 系统架构师 — OS、网络、数据库、计组 |
| [algorithm/](algorithm/) | 算法 & 数据结构、Transformer、LeetCode |

## 🔧 工具

| 目录 | 内容 |
|---|---|
| [git/](git/) | Git 版本控制 |
| [jenkins/](jenkins/) | CI/CD |
| [nginx/](nginx/) | Nginx |
| [markdown/](markdown/) | Markdown、LaTeX、UML |

## 💾 存储

| 目录 | 内容 |
|---|---|
| [sql/](sql/) | SQL / MySQL |
| [redis/](redis/) | Redis |

## 📐 其他

| 目录 | 内容 |
|---|---|
| [3d/](3d/) | 3D 建模 |
| [patent/](patent/) | 专利文档 |
| [svn/](svn/) | SVN |

## 📖 外部资源

- [Linux 内核源码](https://github.com/torvalds/linux)
- [计算机经典电子书](https://github.com/GrindGold/pdf)
- [Linux 内核文档](https://www.kernel.org/doc/html/v5.10/)
