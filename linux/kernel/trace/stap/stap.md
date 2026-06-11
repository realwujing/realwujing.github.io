# SystemTap内核跟踪工具

- [【linux内核调试】SystemTap使用技巧](https://blog.csdn.net/panhewu9919/article/details/103113711)
- [SystemTap（stap）架构和原理介绍，以及脚本编写举例](https://blog.csdn.net/SaberJYang/article/details/141563417)
- [如何使用 SystemTap 对程序进行追踪](https://www.cnblogs.com/shuqin/p/13196585.html)
- [SystemTap使用技巧【一】](https://blog.csdn.net/wangzuxi/article/details/42849053)
- [SystemTap使用技巧【四】](https://blog.csdn.net/wangzuxi/article/details/44901285)
- [使用systemtap实时获取系统中半连接的数量](https://blog.csdn.net/dog250/article/details/105022347)
- [systemtap使用指南](https://zhuanlan.zhihu.com/p/587005171)
- [<font color=Red>SystemTap使用技巧</font>](https://blog.csdn.net/chiqiankuan0816/article/details/101003832)
- [4 SystemTap — 过滤和分析系统数据](https://documentation.suse.com/zh-cn/sles/15-SP6/html/SLES-all/cha-tuning-systemtap.html)
- [SystemTap 新手指南中文翻译](https://geekdaxue.co/books/SystemTap-Beginners-Guide_zh)

## examples

### 定位内核函数位置

```bash
stap -l 'kernel.function("_do_fork")'

kernel.function("_do_fork@./kernel/fork.c:2198")
```

```bash
[root@localhost ~]# stap -l 'kernel.function("*open*")' | grep do_sys_open
kernel.function("__do_sys_open@fs/open.c:1192")
kernel.function("__do_sys_open_by_handle_at@fs/fhandle.c:256")
kernel.function("__do_sys_open_tree@fs/namespace.c:2423")
kernel.function("__do_sys_openat2@fs/open.c:1207")
kernel.function("__do_sys_openat@fs/open.c:1199")
kernel.function("do_sys_open@fs/open.c:1185")
kernel.function("do_sys_openat2@fs/open.c:1156")
```

```bash
[root@fedora wujing]# stap -l 'kernel.function("stack_trace_save_regs")'
kernel.function("stack_trace_save_regs@kernel/stacktrace.c:163")
```

### 获取内核函数位置+参数

```bash
stap -L 'kernel.function("_do_fork")'

kernel.function("_do_fork@./kernel/fork.c:2198") $clone_flags:long unsigned int $stack_start:long unsigned int $stack_size:long unsigned int $parent_tidptr:int* $child_tidptr:int* $tls:long unsigned int $vfork:struct completion $nr:long int
```

### 监控公平调度器的任务选择函数

```bash
stap -ve '
probe kernel.statement("pick_next_task_fair@kernel/sched/fair.c:*") {
    printf("%s\n", pp());
}
' -DSTP_NO_BUILDID_CHECK
```

```bash
stap -ve '
probe kernel.statement("pick_next_task_fair@kernel/sched/fair.c:*") {  # 监控内核公平调度器中pick_next_task_fair函数的所有执行点
    printf("%s\n", pp());  # 打印当前探测点的详细信息（包括源码位置等）
}
' -DSTP_NO_BUILDID_CHECK  # 禁用内核构建ID检查（允许在不匹配的调试环境下运行）
```

功能说明：
1. 跟踪内核公平调度器（CFS）中的`pick_next_task_fair`函数
2. 每次函数执行时都会打印详细的代码位置信息
3. 常用于分析Linux进程调度行为

### 监控virtio网络驱动的接收缓冲区

```bash
stap -d kernel -ve '
probe module("virtio_net").function("receive_buf") {
        printf("vq=%p name=%s index=%d, num_free=%d\n", 
                $rq->vq, 
                kernel_string($rq->vq->name),
                $rq->vq->index,
                $rq->vq->num_free
                );
}
' -DSTP_NO_BUILDID_CHECK
```

```bash
stap -d kernel -ve '  # -d选项加载内核调试符号信息
probe module("virtio_net").function("receive_buf") {  # 监控virtio_net模块的receive_buf函数
        printf("vq=%p name=%s index=%d, num_free=%d\n",  # 打印虚拟队列信息：
                $rq->vq,           # 虚拟队列指针地址
                kernel_string($rq->vq->name),  # 队列名称（将内核指针转为字符串）
                $rq->vq->index,    # 队列索引号
                $rq->vq->num_free  # 队列剩余可用空间
                );
}
' -DSTP_NO_BUILDID_CHECK  # 禁用构建ID验证
```

功能说明：
1. 跟踪virtio虚拟网络设备的接收缓冲区处理函数
2. 每次接收网络数据包时打印：
   - 虚拟队列的内存地址
   - 队列名称
   - 队列编号
   - 可用缓冲区空间
3. 用于诊断virtio网络设备的性能问题和数据接收状态

共同特点：
- `-ve`参数表示显示详细错误信息
- `-DSTP_NO_BUILDID_CHECK`绕过内核版本严格匹配要求
- 都需要root权限运行
- 主要用于内核开发和驱动调试场景

### 监控cgroup的访问权限更新

```bash
stap -ve '
probe kernel.function("devcgroup_update_access") {
        printf("pid=%d comm=%s type=%d op=%s\n", pid(), execname(), $filetype, kernel_string($buffer));
        print_backtrace();
}
' -DSTP_NO_BUILDID_CHECK
```

### 深度追踪 systemd 进程中 src/core/cgroup.c 文件内所有函数的调用流程​​，并通过智能缩进直观展示函数调用层次关系

```bash
stap -ve '
probe process("/usr/lib/systemd/systemd").function("*@src/core/cgroup.c:*").call {
    printf("%s -> %s\n", thread_indent(4), ppfunc());  # 函数调用时打印缩进和函数名
}

probe process("/usr/lib/systemd/systemd").function("*@src/core/cgroup.c:*").return {
    printf("%s <- %s\n", thread_indent(-4), ppfunc()); # 函数返回时减少缩进并打印函数名
}
'
```

示例输出如下：
```bash
  0 systemd(1):    -> unit_prune_cgroup
  7 systemd(1):        -> unit_get_cpu_usage
 14 systemd(1):            -> unit_get_cpu_usage_raw
 22 systemd(1):                -> unit_has_host_root_cgroup
 30 systemd(1):                <- unit_has_host_root_cgroup
 41 systemd(1):            <- unit_get_cpu_usage_raw
 42 systemd(1):        <- unit_get_cpu_usage
120 systemd(1):        -> unit_release_cgroup
158 systemd(1):        <- unit_release_cgroup
160 systemd(1):    <- unit_prune_cgroup
```

```text
  0 systemd(1):    -> unit_prune_cgroup                # 开始执行 cgroup 修剪操作（入口）
  7 systemd(1):        -> unit_get_cpu_usage           # 获取单元CPU使用量（嵌套调用）
 14 systemd(1):            -> unit_get_cpu_usage_raw   # 获取原始CPU数据（更深层嵌套）
 22 systemd(1):                -> unit_has_host_root_cgroup  # 检查是否使用宿主机根cgroup
 30 systemd(1):                <- unit_has_host_root_cgroup  # 完成宿主机cgroup检查（返回）
 41 systemd(1):            <- unit_get_cpu_usage_raw   # 完成原始CPU数据获取（返回）
 42 systemd(1):        <- unit_get_cpu_usage           # 完成CPU使用量计算（返回）
120 systemd(1):        -> unit_release_cgroup          # 开始释放cgroup资源（新嵌套调用）
158 systemd(1):        <- unit_release_cgroup          # 完成cgroup资源释放（返回）
160 systemd(1):    <- unit_prune_cgroup                # 完成整个cgroup修剪操作（最终返回）
```

关键说明：

1. 箭头方向：
   • `->` 表示函数调用开始（进入）

   • `<-` 表示函数调用结束（返回）

2. 缩进层级：
   • 每级缩进（4空格）代表一层函数调用嵌套

   • 如 `unit_has_host_root_cgroup` 是第三层嵌套调用

3. 时间戳：
   • 左侧数字是微秒级相对时间戳（从0开始）

   • 例如 `unit_release_cgroup` 耗时 158-120=38μs

4. PID(1)：
   • systemd 主进程的进程ID始终为1

5. 执行流程：
   完整展现了 systemd 清理 cgroup 资源时的：
   • CPU使用量检测流程（第7-42μs）

   • 资源释放流程（第120-158μs）

---

## SystemTap 内核接口深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)

SystemTap 是一个**纯用户态工具**，内核中**没有** SystemTap 专属的子系统代码。它的核心思想是：把跟踪脚本翻译为 C 代码 → 编译为 `.ko` 内核模块 → 通过已有的 kprobe/tracepoint/uprobe 基础设施运行。

### 架构

```
用户态                              内核态
─────                              ─────

stap 脚本 (.stp)                   ┌─────────────────────┐
    │                              │  SystemTap 模块      │
    ▼                              │  (自动生成的 .ko)    │
stap 编译器                        │                     │
  translate → C 代码                │  register_kprobe     │
  compile  → .ko                    │  register_kretprobe  │
    │                              │  tracepoint_probe_   │
    ▼                              │      register        │
staprun (模块加载器)                 │  relay_open          │
  insmod → .ko                     │                     │
  read relay → 控制台输出           └─────────────────────┘
```

**关键**：`stap` 编译器生成的 C 代码直接在已有的 `register_kprobe`、`tracepoint_probe_register`、`uprobe_register` 上注册回调，**不修改内核任何一行代码**。

### 三种探测方式与内核接口

#### kernel.function → kprobe

SystemTap 脚本：

```systemtap
probe kernel.function("do_sys_open") {
    printf("opening %s\n", kernel_string($filename))
}
```

编译器生成模块代码：

```c
static int entry_handler(struct kprobe *p, struct pt_regs *regs) {
    // 提取参数 — $filename → struct pt_regs 偏移
    // printf → _stp_print (relay 通道)
    return 0;
}

static struct kprobe probe = {
    .symbol_name = "do_sys_open",
    .pre_handler = entry_handler,
};

static int __init my_init(void) {
    register_kprobe(&probe);              // include/linux/kprobes.h:405
}
```

底层：`register_kprobe` → `arch/x86/kernel/kprobes/core.c:978` `kprobe_int3_handler` → `aggr_pre_handler` → 调用 `entry_handler`。

#### kernel.trace → tracepoint

SystemTap 脚本：

```systemtap
probe kernel.trace("sched:sched_switch") {
    printf("%s -> %s\n", kernel_string($prev->comm), kernel_string($next->comm))
}
```

编译器生成模块代码：

```c
static void tp_handler(void *__data, bool preempt,
                       struct task_struct *prev, struct task_struct *next) {
    // printf via relay channel
}

static int __init my_init(void) {
    tracepoint_probe_register(tp, tp_handler, NULL);  // include/linux/tracepoint.h
}
```

#### process.function → uprobe

SystemTap 脚本：

```systemtap
probe process("/bin/bash").function("readline") {
    printf("readline called\n")
}
```

编译器生成模块代码：

```c
static int handler(struct uprobe_consumer *self, struct pt_regs *regs, __u64 *data) {
    _stp_printf("readline called\n");
    return 0;
}

static struct uprobe_consumer uc = { .handler = handler };

static int __init my_init(void) {
    uprobe_register(inode, offset, 0, &uc);    // include/linux/uprobes.h:209
}
```

### 输出 — relay 接口

SystemTap 模块不直接 `printk`，而是通过 `relay` 接口向用户态传递数据：

```c
// 编译器自动生成的 relay 通道
struct rchan *relay_channel = relay_open("stap", NULL, SUBBUF_SIZE,
                                         N_SUBBUFS, &relay_callbacks, NULL);
// printf → relay_write(relay_channel, buf, len)

// staprun 用户态:
read(relay_fd, buf, sizeof(buf)) → 得到 printf 输出
```

这保证了**不污染 dmesg/kmsg**，输出完全独立的通道。

### 安全模型

SystemTap 模块运行在**内核态**，但通过以下机制保证安全：

| 机制 | 实现 | 作用 |
|------|------|------|
| **编译时类型检查** | DWARF/CTF/BTF 类型信息 | 拒绝不正确的类型转换 |
| **地址检查** | `kallsyms_lookup`, `module_ksym` | 验证探测地址是有效的内核符号 |
| **数组边界** | `stp_bound_check()` | 防止数组越界 |
| **kill 免疫** | `stp_ctl_write(STP_EXIT)` | 模块被强制卸载时安全退出 |
| **guru 模式** | `-g` flag | 允许危险操作（需要 root 且明确授权） |

### 与 perf、bpftrace 的对比

| | SystemTap | perf + kprobe | bpftrace |
|---|---|---|---|
| **脚本语言** | stp 脚本 → C → .ko | perf probe 命令（字符串） | bpftrace 脚本（类 awk） |
| **编译目标** | 内核模块 (.ko) | perf 内部 bytecode | BPF bytecode |
| **安全模型** | 编译时类型检查 + 模块签名 | 无（直接注入 int3） | **Verifier 完整安全性** |
| **jitted 代码** | 否（总是原生 C in .ko） | 否 | 是（BPF JIT → x86 原生） |
| **内核入口** | register_kprobe | register_kprobe | bpf_prog_run |
| **开销** | 中（kprobe + 模块上下文切换） | 低（直接 perf ring buffer） | **最低**（BPF JIT + map 无锁） |
| **适用场景** | 复杂的多函数跟踪、模板 | 简单的单函数探测 | **高性能生产环境** |

### 与已有内核源码系列的关系

| 已有系列 | 本节覆盖 |
|---------|---------|
| kprobe/uprobe（perf 第 5 篇） | SystemTap 使用 register_kprobe/uprobe 作为底层 |
| tracepoint（ftrace 第 3 篇） | SystemTap 使用 tracepoint_probe_register 作为底层 |
| BPF（bpf 系列） | bpftrace 作为 SystemTap 的替代，BPF verifier 对比 |
