# stap

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
