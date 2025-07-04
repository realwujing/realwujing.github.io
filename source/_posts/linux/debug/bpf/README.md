---
date: 2024/07/23 10:29:55
updated: 2024/07/23 10:29:55
---

# bpf-learning

## 安装教程

```bash
sudo apt install bpfcc-tools linux-headers-$(uname -r) bpftrace
```

- [BPF Compiler Collection (BCC)](bcc.md)
- [bpftrace](bpftrace.md)

## 动态追踪

可以通过探针机制，来采集内核或者应用程序的运行信息。所谓动态，表示他可以不用修改内核和应用程序的代码，就获得丰富的信息。因为在以往的排查和调试性能问题过程中，我们往往会先给应用程序设置一系列的断点(gdb),然后以手动或者脚本(GDB的Python扩展)的方式，在断点处去分析程序状态，或者增加一系列的日志，在日志中寻找线索。相比以往的进程及追踪方法(ptrace)，动态追踪往往只需要消耗很小的性能消耗（5%甚至更少）。

根据类型不同，动态追踪所使用的事件源，可以分为静态探针，动态探针，硬件事件这三类。

### 硬件事件

其中硬件事件通常由性能监控计数器PMC(Performance Monitoring Counter)产生，他包括了各种硬件的性能指标，比如CPU的缓存，指令周期，分支预测等等。

### 静态探针

静态探针，指事先在代码中定义好，并编译到应用程序和内核中的探针。这些探针只有在开启探测功能时，才会被执行到。未开启时，不会执行。常见的静态探针包括内核中的跟踪点(tracepoints)和USDT(Userland Statically Defined Tracing)探针：

- 跟踪点(tracepoints) - 在源码中插入一些带控制条件的探测点，这些探测点允许事后添加处理函数，比如在内核中，最常见的静态追踪方法就是printk(),输出日志，Linux内核定义了大量的跟踪点，可以通过内核编译选项来开启和关闭。

- USDT探针，全称是用户级静态定义跟踪，需要在源码中插入DTRACE_PROBE()代码，并编译到应用程序中。不过在很多应用程序内置了USDT探针，比如MySQL, PostgreSQL等。

### 动态探针

动态探针，是指没有实现在代码定义，但却可以在运行时动态添加的探针。比如函数的调用和返回等，动态探针支持按需在内核或者应用程序中添加探测点，具有更高的灵活性，常见的动态探针有两种，即用于内核态的kprobes以及用户态的uprobes:

- kprobes - 用来跟踪内核态的函数，包括用于函数调用的kprobe和用于函数返回的kretprobe。

- uprobes - 用来跟踪用户态的函数，包括用于函数调用的uprobe以及用于函数返回的uretprobe。

## More

- [Linux动态追踪技术概念介绍](https://www.toutiao.com/article/7077801491347833374)