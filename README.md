# 📚 技术笔记与资源库

> Stay hungry. Stay foolish. - Steve Jobs

个人技术学习笔记与资源整理，涵盖操作系统、编程语言、架构设计等多个技术领域。

## 🗂️ 目录结构

### 🐧 Linux

Linux系统技术文档，包含内核开发、性能调优、虚拟化等内容

- **[kernel](./linux/kernel/)** - Linux内核开发与源码分析
- **[performance](./linux/performance/)** - 性能分析工具 (perf, BPF, stap)
- **[debug](./linux/debug/)** - 调试工具与技术 (GDB, kdump, assembly)
- **[virt](./linux/virt/)** - 虚拟化技术 (KVM, Docker, K8s)
- **[drivers](./linux/drivers/)** - 设备驱动开发 (GPU, 声卡, 字符设备等)
- **[boot](./linux/boot/)** - 系统启动与GRUB
- **[monitoring](./linux/monitoring/)** - 系统监控与日志分析
- **[testing](./linux/testing/)** - Linux测试框架 (LTP)
- **[tools](./linux/tools/)** - 日常工具配置 (shell, vim, tmux, ssh)
- **[distro](./linux/distro/)** - 发行版相关 (deepin, 包管理)

### 💻 编程语言

- **[cpp](./cpp/)** - C/C++学习笔记与面试指南
- **[python](./python/)** - Python开发技术
- **[go](./go/)** - Go语言学习
- **[rust](./rust/)** - Rust编程语言
- **[java](./java/)** - Java开发
- **[javascript](./javascript/)** - JavaScript/前端技术

### 🏗️ 架构与设计

- **[architect](./architect/)** - 系统架构师学习笔记
  - 操作系统、计算机组成、网络、数据库
  - 系统设计与架构模式

### 🔧 开发工具与实践

- **[git](./git/)** - Git版本控制
- **[jenkins](./jenkins/)** - CI/CD持续集成
- **[nginx](./nginx/)** - Nginx配置与优化
- **[markdown](./markdown/)** - Markdown文档编写

### 💾 数据存储

- **[sql](./sql/)** - SQL数据库技术
- **[redis](./redis/)** - Redis缓存技术

### 📐 其他技术

- **[3d](./3d/)** - 3D建模与图形学
- **[algorithm](./algorithm/)** - 算法与数据结构
- **[patent](./patent/)** - 专利相关
- **[svn](./svn/)** - SVN版本控制

## 🔥 热门推荐

### Linux内核与系统

- [Linux内核开发](./linux/kernel/) - 内核源码分析、驱动开发、内存管理
- [性能分析利器](./linux/performance/bpf/) - BPF/eBPF追踪与性能调优
- [虚拟化技术](./linux/virt/) - KVM、QEMU、Docker、K8s实战

### 调试与分析

- [GDB调试技巧](./linux/debug/gdb/) - 内核与应用程序调试
- [汇编语言分析](./linux/debug/assembly/) - 底层代码分析
- [性能监控](./linux/monitoring/) - 系统监控与日志分析

### GPU与AI

- [昇腾AI开发](./linux/drivers/gpu/ascend/) - 昇腾训练与推理性能调优
- [机器学习经典](./python/books/) - 周志华《机器学习》西瓜书

### 开发实践

- [C/C++开发](./cpp/) - 语法、面试、最佳实践
- [包管理与打包](./linux/distro/pkg/) - deb/rpm包制作

## 📚 书籍与资源

### Linux内核与系统

#### 基础与核心

- [Linux内核设计与实现(第三版)](./linux/kernel/books/Linux内核设计与实现(第三版中文高清带目录).pdf)
- [Linux内核完全注释(修正版V5.0)](./linux/kernel/sources/Linux-0.11-zhaojiong/Linux内核完全注释(修正版V5.0).pdf)
- [深入分析Linux内核源代码](./linux/kernel/books/深入分析Linux内核源代码.pdf)
- [内核深度解析](./linux/kernel/books/内核深度解析_余华兵著_北京：人民邮电出版社%20,%202019.05_P622.pdf)
- [庖丁解牛十二刀5.9](./linux/kernel/books/庖丁解牛十二刀5.9.pdf)

#### 驱动与设备

- [Linux设备驱动开发详解(基于Linux4.0)](./linux/kernel/books/Linux设备驱动开发详解：基于最新的Linux4.0内核.pdf)
- [PCI Express体系结构导读](./linux/kernel/books/PCI_Express_体系结构导读.pdf)
- [ACPI高级配置与电源接口](./linux/kernel/books/ACPI高级配置与电源接口.pdf)
- [UEFI原理与编程](./linux/kernel/books/UEFI原理与编程_戴正华(著)%20机械工业出版社_完整版.pdf)
- [INTEL开发手册卷3(中文版)](./linux/kernel/books/INTEL开发手册卷3(中文版).pdf)
- [Linux音频驱动](./linux/drivers/sound/linux音频驱动.pptx)
- [音频调试工具介绍](./linux/drivers/sound/音频调试工具介绍.pptx)

#### 网络

- [Linux开源网络全栈详解：从DPDK到OpenFlow](./linux/kernel/books/Linux开源网络全栈详解：从DPDK到OpenFlow+-+英特尔亚太研发有限公司(2019).pdf)
- [深入浅出DPDK](./linux/kernel/books/深入浅出DPDK.pdf)

#### 安全

- [Linux内核安全模块深入剖析](./linux/kernel/books/Linux内核安全模块深入剖析.pdf)
- [SELinux详解](./linux/kernel/books/SELinux详解.pdf)

### 调试与性能分析

- [Debug Hacks 深入调试的技术和工具](./linux/kernel/books/[Debug%20Hacks%20深入调试的技术和工具].2011.中文版.pdf)
- [软件调试（第2版）卷1：硬件基础](./linux/kernel/books/软件调试（第2版）卷1：硬件基础（异步图书）%20(张银奎)%20.pdf)
- [Debugging with gdb 中文](./linux/debug/gdb/Debugging.with.gdb%20中文.pdf)
- [Debugging with gdb 手册](http://sourceware.org/gdb/current/onlinedocs/gdb/)
- [100个gdb小技巧](https://wizardforcel.gitbooks.io/100-gdb-tips/content/index.html)
- [BPF Performance Tools](./linux/performance/bpf/BPF.Performance.Tools.2019.12.pdf)
- [BPF之巅 洞悉Linux系统和应用性能](./linux/performance/bpf/BPF之巅%20洞悉Linux系统和应用性能.pdf.tar.gz)

### 汇编与底层分析

- [汇编语言（第4版）](./linux/debug/assembly/汇编语言（第4版）@www.cmsblogs.cn.pdf)
- [二进制分析实战](./linux/debug/binary-analysis/二进制分析实战（打造自己的二进制插桩、分析和反汇编的Linux工具）%20([荷]丹尼斯·安德里斯)%20(z-lib.org).pdf)

### 虚拟化技术

- [QEMU-KVM源码分析与应用](./linux/virt/kvm/books/QEMU-KVM源码分析与应用.pdf)
- [KVM虚拟化技术：实战与原理解析](./linux/virt/kvm/books/kvm虚拟化技术：实战与原理解析.pdf)

### GPU、AI与机器学习

- [通用图形处理器设计GPGPU编程模型与架构原理](./linux/kernel/books/（已压缩）通用图形处理器设计GPGPU编程模型与架构原理.pdf)
- [昇腾训练性能调优(Pytorch) 赋能](./linux/drivers/gpu/ascend/昇腾训练性能调优(Pytorch)%20赋能.pdf)
- [昇腾推理服务化profiling快速入门](./linux/drivers/gpu/ascend/昇腾推理服务化profiling快速入门.docx)
- [昇腾推理服务化参数自动寻优](./linux/drivers/gpu/ascend/昇腾推理服务化参数自动寻优.pptx)
- [机器学习 周志华](./python/books/机器学习%20周志华.pdf)
- [《机器学习》PDF下载(西瓜书)](https://pdfs.top/book/dpdyt)

### 编程语言与开发

#### C/C++

- [UNIX环境高级编程(第3版)](./linux/kernel/books/UNIX%20环境高级编程%20第3版.pdf)
- [Linux C编程一站式学习](https://www.bookstack.cn/read/linux-c/menu.md)
- [Makefile 基础教程](https://www.bookstack.cn/read/makefile-basic/16858cbb3e00b884.md)
- [跟我一起写Makefile](https://www.bookstack.cn/read/how-to-write-makefile/b473e56b6c52d350.md)
- [CMake菜谱(CMake Cookbook中文版)](https://www.bookstack.cn/read/CMake-Cookbook/README.md)
- [C++11的原生字符串](https://wizardforcel.gitbooks.io/cpp-11-faq/content/52.html)

#### Qt

- [Qt中文文档](https://www.qtdoc.cn/BookInfo.html)
- [Qt 学习之路 2](https://www.bookstack.cn/read/qt-study-road-2/ddf84b4ac149953f.md)
- [QtConcurrent多线程编程](https://runebook.dev/zh-CN/docs/qt/qtconcurrenttask)

#### Go

- [写写代码，喝喝茶，搞搞 Go](https://eddycjy.gitbook.io/golang/di-3-ke-gin/log)
- [Golang Gin 实践 - Swagger集成](https://www.bookstack.cn/read/gin-EDDYCJY-blog/golang-gin-2018-03-18-Gin%E5%AE%9E%E8%B7%B5-%E8%BF%9E%E8%BD%BD%E5%85%AB-%E4%B8%BA%E5%AE%83%E5%8A%A0%E4%B8%8ASwagger.md)

#### 分布式框架

- [TarsCPP 快速入门](https://www.bookstack.cn/read/Tars-1.8/rumen-hello-world-1-tarscpp-kuai-su-ru-men.md)

#### 开发工具

- [Vim实用技巧（第2版）](./linux/tools/vim/Vim实用技巧（第2版）.pdf)
- [Vim实用技巧在线版](https://github.com/wsdjeg/vim-galore-zh_cn)

### 系统架构师考试

#### 学习资料

- [系统架构设计师学习手册](./architect/xisai/2024年下半年系统架构设计师学习手册.pdf)
- [系统架构设计师考试笔记](./architect/系统架构设计师考试笔记.pdf)
- [系统架构设计师思维导图](./architect/xisai/202409/系统架构设计师思维导图.pdf)
- [系统架构设计师学习计划](./architect/xisai/202409/系统架构设计师学习计划_.pdf)
- [希赛学习资料](https://wangxiao.xisaiwang.com/rk/xxzl/n101.html)

#### 知识点汇总

- [架构师专业英语高频词汇表](./architect/xisai/【新版】架构师专业英语高频词汇表.pdf)
- [系统架构设计师知识点集锦](./architect/xisai/2024年系统架构设计师知识点集锦.pdf)
- [系统架构设计师重要知识点100条](./architect/xisai/2024年下半年系统架构设计师重要知识点100条.pdf)
- [系统架构设计师速记50个高频知识点](./architect/xisai/202409/系统架构设计师速记50个高频知识点.pdf)
- [系统架构设计师易混淆知识点](./architect/xisai/202409/系统架构设计师易混淆知识点.pdf)

#### 习题与模拟

- [系统架构设计师经典100题](./architect/xisai/2024年系统架构设计师经典100题.pdf)
- [系统架构设计师默写本](./architect/xisai/2024年系统架构设计师默写本.pdf)
- [系统架构设计师案例简答合集](./architect/xisai/202409/系统架构设计师案例简答合集.pdf)
- [系统架构设计师论文范文](./architect/xisai/202409/系统架构设计师论文范文.pdf)
- [2024年下半年第一期模考试卷（案例分析）](./architect/xisai/202409/2024年下半年系统架构设计师第一期模考试卷（案例分析）_.pdf)

## 📖 外部资源

- [计算机经典电子书与学习资源](https://github.com/GrindGold/pdf)
- [Linux内核源码](https://github.com/torvalds/linux)

## 🤝 贡献

欢迎提交Issue和Pull Request来完善这个知识库！

## 📄 许可证

本仓库内容遵循各自的开源许可证。个人笔记部分采用 MIT License。
