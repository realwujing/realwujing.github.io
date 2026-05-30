# 📚 技术笔记与资源库

## 🐧 Linux 内核

对标 Linux 内核源码树，按子系统组织。

| 目录 | 内容 |
|---|---|
| [kernel/](linux/kernel/) | 内核核心 — sched、locking、rcu、irq、trace(bpf,events,stap) |
| [mm/](linux/mm/) | 内存管理 — page alloc、slab、ksm、kmemleak |
| [fs/](linux/fs/) | 文件系统 — ext4、kernfs、dcache、fuse、minifs、[上游 patch](linux/fs/patch/) |
| [net/](linux/net/) | 网络栈 — 收包 epoll、vxlan、vhost-user、[上游 patch](linux/net/patch/) |
| [security/](linux/security/) | 内核安全 |
| [drivers/](linux/drivers/) | 设备驱动 — GPU(nvidia-svm、stanford-cs336)、声卡、block |
| [virt/](linux/virt/) | 虚拟化 — KVM、QEMU、Docker、K8s |
| [gdb/](linux/gdb/) | GDB 内核/应用调试 |
| [kdump/](linux/kdump/) | kdump 崩溃分析 |
| [assembly/](linux/assembly/) | 汇编 & 二进制分析 |
| [books/](linux/books/) | 内核经典书籍 (20+ PDF) |
| [boot/](linux/boot/) | 系统启动 — GRUB |
| [tools/](linux/tools/) | 开发工具 — shell、vim、tmux、ssh、distro |
| [linux教程.md](linux/linux教程.md) | Linux 学习资源索引 |

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
| [markdown/](markdown/) | Markdown、LaTeX、Doxygen、UML |

## 💾 其他

| [sql/](sql/) | MySQL |
|---|---|
| [redis/](redis/) | Redis |
| [3d/](3d/) | 3D 建模 |
| [patent/](patent/) | 专利文档 |
| [svn/](svn/) | SVN |
