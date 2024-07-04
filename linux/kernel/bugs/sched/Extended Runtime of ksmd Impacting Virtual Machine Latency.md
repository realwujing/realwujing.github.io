# Extended Runtime of ksmd Impacting Virtual Machine Latency

ksmd频繁被调度导致虚拟机性能下降。

## perf sched

- [perf sched查看调度延迟与唤醒延迟](https://blog.csdn.net/yiyeguzhou100/article/details/102809576)

在物理机上查看cpu 43 的时间片使用情况:

```bash
perf sched record -C 43
```

```bash
perf sched timehist
```

通过下图可以看到内核线程ksmd被周期性调度，每次运行时长12ms左右。

![perf sched timehist](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240704154742.png)

## ksmd

内核中的Kernel Samepage Merging (KSM)机制被设计用于合并相同的内存页，以节省物理内存。

合并相同的内存页是Kernel Samepage Merging (KSM) 的核心功能，其目的是通过检测并合并相同的内存页来减少物理内存的使用。这一机制特别适用于虚拟化环境，在这些环境中，多个虚拟机可能会运行相同的操作系统和应用程序，从而拥有大量相同的内存页。

### ksmd内核参数

ksmd是内核KSM机制的守护进程，它的相关信息可以在/sys/kernel/mm/ksm目录中找到。

在`/sys/kernel/mm/ksm`目录中，您会找到以下文件，每个文件都有特定的功能和用途：

1. **full_scans**：显示自KSM启动以来完成的完整扫描次数。

2. **max_page_sharing**：控制一个共享页面的最大引用次数。

3. **merge_across_nodes**：控制是否允许在不同NUMA节点之间合并页面。值为1表示允许，0表示不允许。

4. **pages_shared**：显示当前被共享的内存页数。

5. **pages_sharing**：显示当前有多少页在共享同一页。

6. **pages_to_scan**：控制每个扫描周期中KSM扫描的页数。

7. **pages_unshared**：显示当前未被共享的页数。

8. **pages_volatile**：显示当前不稳定的页数，这些页可能会被KSM合并。

9. **run**：控制KSM的运行状态。写入`0`停止KSM，写入`1`启动KSM。

10. **sleep_millisecs**：控制KSM扫描周期的时间间隔，以毫秒为单位。

11. **stable_node_chains**：显示稳定节点链的数量。

12. **stable_node_chains_prune_millisecs**：控制修剪稳定节点链的时间间隔，以毫秒为单位。

13. **stable_node_dups**：显示稳定节点的副本数量。

14. **use_zero_pages**：控制是否使用零页来合并空闲页。值为1表示使用，0表示不使用。

### 动态调整ksmd参数

`ksmd` 线程在以下情况下会被调度运行：

#### 启动KSM后

KSM通过将`/sys/kernel/mm/ksm/run`文件的值设置为`1`来启动。一旦启动，`ksmd`线程会按照配置开始周期性地扫描和合并内存页。

#### 配置的扫描周期

`ksmd`线程的调度频率由`/sys/kernel/mm/ksm/sleep_millisecs`文件中配置的扫描周期决定。该文件的值是一个以毫秒为单位的时间间隔，表示`ksmd`线程在每次扫描之间的睡眠时间。例如：

```bash
# 设置扫描周期为100毫秒
echo 100 | sudo tee /sys/kernel/mm/ksm/sleep_millisecs
```

在这种配置下，`ksmd`线程会每隔100毫秒被调度运行一次，以扫描和合并内存页。

#### 每次扫描的页数

`ksmd`线程在每次被调度运行时，会根据`/sys/kernel/mm/ksm/pages_to_scan`文件中配置的值来决定每次扫描的页数。例如：

```bash
# 设置每次扫描的页数为256页
echo 256 | sudo tee /sys/kernel/mm/ksm/pages_to_scan
```

这意味着每次`ksmd`线程被调度运行时，它将扫描256个内存页。

#### 内核调度器的调度

- [Linux 内核调度器源码分析 - 初始化](https://www.cnblogs.com/tencent-cloud-native/p/14767478.html)

`ksmd`线程作为内核线程，由内核调度器进行调度。内核调度器会根据系统的整体负载和`ksmd`线程的优先级来决定何时运行`ksmd`线程。尽管`ksmd`线程的优先级通常较低，但在配置的扫描周期到达时，调度器会尝试调度它运行。

```c
start_kernel  // 0号线程
    -> sched_init // 调度器初始化
        -> init_sched_fair_class // cfs_class初始化
    -> reset_init
        -> kernel_init  // 1号线程, sched_normal
                -> kernel_init_freeable
                        -> wait_for_completion(&kthreadd_done) // Wait until kthreadd is all set-up.
                        -> smp_init // 多核初始化
                        -> sched_init_smp // 多核调度器初始化
                                -> sched_init_domains
                                        -> cpumask_and // 排除isolated cpus
                                -> init_sched_rt_class
                                -> init_sched_vms_class
                                -> init_sched_dl_class
                        -> do_basic_setup
                                -> do_initcalls
                                        -> subsys_initcall(ksm_init)    // ksmd, sched_normal
        -> kthreadd  // 2号线程，sched_normal
        -> complete(&kthreadd_done)
```

```bash
taskset -pc 2
pid 2's current affinity list: 0-95
```

```bash
taskset -pc 510
pid 510's current affinity list: 0-95
```

##### PF_NO_SETAFFINITY

- [PF_NO_SETAFFINITY校验值](https://cloud.tencent.com/developer/ask/sof/108318424/answer/119151179)

在内核源码中查看PF_NO_SETAFFINITY定义：

```c
#linux/sched.h

#define PF_NO_SETAFFINITY 0x04000000        /* Userland is not allowed to meddle with cpus_allowed */
```

在物理机上查看ksmd进程号：

```bash
ps aux | grep -i ksmd
root         510 11.1  0.0      0     0 ?        RN   Jan19 26874:37 [ksmd]
root     1979672  0.0  0.0 213952  1600 pts/16   S+   16:13   0:00 grep -i ksmd

进一步查看ksmd进程的状态：

```bash
[root@gzinf-kunpeng-55e235e17e57 yql]# cat /proc/510/stat
510 (ksmd) S 2 0 0 0 -1 2097216 0 0 0 0 3 161248078 0 0 25 5 1 0 13 0 0 18446744073709551615 0 0 0 0 0 0 0 2147483647 0 1 0 0 17 69 0 0 0 0 0 0 0 0 0 0 0 0 0
```

查看ksmd进程的标志位（flags）：

```bash
[root@gzinf-kunpeng-55e235e17e57 yql]# flags=$(cut -f 9 -d ' ' /proc/510/stat)
```

查看ksmd进程的标志位（flags）是否包含PF_NO_SETAFFINITY（0x04000000）：

```bash
[root@gzinf-kunpeng-55e235e17e57 yql]# echo $(($flags & 0x40000000))
0
```

![20240704161455](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240704161455.png)

可以看到ksmd进程的标志位（flags）不包含PF_NO_SETAFFINITY（0x04000000）。故可以通过taskset或者cgroups设置ksmd的cpu亲和性，将起绑定到非客户虚拟机所在的cpu上。

## 总结

优先建议通过调整ksmd参数来提高虚拟机的性能。

进一步可以通过设置ksmd的cpu亲和性，将其绑定到非客户虚拟机所在的cpu上。

将ksmd的亲和性设置为10号cpu:

```bash
taskset -pc 10 510
```

将ksmd的亲和性设置为0-63号cpu:

```bash
taskset -pc 0-63 510
```

通过上述两步即可将ksmd从隔离的cpu上迁走。
