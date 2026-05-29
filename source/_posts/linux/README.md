---
title: 'Linux内核开发与系统技术文档'
date: '2025/11/18 17:50:58'
updated: '2025/11/22 18:21:52'
---

# Linux内核开发与系统技术文档

## 目录结构说明

### 📂 [boot](https://github.com/realwujing/realwujing.github.io/tree/main/linux/boot)

系统启动相关技术

- [grub](https://github.com/realwujing/realwujing.github.io/tree/main/linux/boot/grub): GRUB引导加载器配置和调试

### 🔍 [debug](https://github.com/realwujing/realwujing.github.io/tree/main/linux/debug)

纯调试工具和技术

- [assembly](https://github.com/realwujing/realwujing.github.io/tree/main/linux/debug/assembly): 汇编语言调试
- [binary-analysis](https://github.com/realwujing/realwujing.github.io/tree/main/linux/debug/binary-analysis): 二进制分析
- [gdb](https://github.com/realwujing/realwujing.github.io/tree/main/linux/debug/gdb): GDB调试器使用
- [kdump](https://github.com/realwujing/realwujing.github.io/tree/main/linux/debug/kdump): 内核转储分析

### 📊 [performance](https://github.com/realwujing/realwujing.github.io/tree/main/linux/performance)

性能分析和优化工具

- [perf](https://github.com/realwujing/realwujing.github.io/tree/main/linux/performance/perf): Linux perf 性能分析
- [bpf](https://github.com/realwujing/realwujing.github.io/tree/main/linux/performance/bpf): BPF/eBPF 追踪和性能分析
- [stap](https://github.com/realwujing/realwujing.github.io/tree/main/linux/performance/stap): SystemTap 动态追踪

### 🧪 [testing](https://github.com/realwujing/realwujing.github.io/tree/main/linux/testing)

测试框架和工具

- [ltp](https://github.com/realwujing/realwujing.github.io/tree/main/linux/testing/ltp): Linux Test Project

### 📈 [monitoring](https://github.com/realwujing/realwujing.github.io/tree/main/linux/monitoring)

系统监控和日志分析

- [log](https://github.com/realwujing/realwujing.github.io/tree/main/linux/monitoring/log): 日志管理和分析
- [linux运维监控工具](https://github.com/realwujing/realwujing.github.io/tree/main/linux/monitoring/linux运维监控工具): 运维监控工具集

### 🖥️ [virtualization](https://github.com/realwujing/realwujing.github.io/tree/main/linux/virtualization)

虚拟化和容器技术

- [kvm](https://github.com/realwujing/realwujing.github.io/tree/main/linux/virtualization/kvm): KVM虚拟机管理
- [container](https://github.com/realwujing/realwujing.github.io/tree/main/linux/virtualization/container): Docker、K8s等容器技术

### 🔧 [tools](https://github.com/realwujing/realwujing.github.io/tree/main/linux/tools)

日常使用的系统工具

- [shell](https://github.com/realwujing/realwujing.github.io/tree/main/linux/tools/shell): Shell脚本和ZSH配置
- [vim](https://github.com/realwujing/realwujing.github.io/tree/main/linux/tools/vim): Vim编辑器配置
- [tmux](https://github.com/realwujing/realwujing.github.io/tree/main/linux/tools/tmux): 终端复用器
- [ssh](https://github.com/realwujing/realwujing.github.io/tree/main/linux/tools/ssh): SSH配置和使用

### 🐧 [distro](https://github.com/realwujing/realwujing.github.io/tree/main/linux/distro)

Linux发行版相关

- [deepin](https://github.com/realwujing/realwujing.github.io/tree/main/linux/distro/deepin): Deepin系统相关
- [pkg](https://github.com/realwujing/realwujing.github.io/tree/main/linux/distro/pkg): 包管理(deb/rpm)

### 🎮 [gpu](https://github.com/realwujing/realwujing.github.io/tree/main/linux/gpu)

GPU相关技术

- [昇腾AI开发](https://github.com/realwujing/realwujing.github.io/tree/main/linux/drivers/gpu/ascend) - 昇腾训练与推理性能调优


### ⚙️ [kernel](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel)

Linux内核开发

- [drivers](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/drivers): 驱动开发
- [mm](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/mm): 内存管理
- [net](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/net): 网络子系统
- [fs](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/fs): 文件系统
- [irq](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/irq): 中断处理
- [task](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/task): 进程调度
- [mutex](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/mutex): 同步机制
- [security](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/security): 安全模块
- [sources](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/sources): 内核源码分析
- [books](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/books): 内核相关书籍笔记

