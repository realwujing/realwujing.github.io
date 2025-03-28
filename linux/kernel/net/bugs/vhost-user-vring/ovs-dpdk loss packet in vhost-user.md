# ovs-dpdk loss packet in vhost-user

## ovs-dpdk观察vring

在host上观察到vring一直都是满的：
![dpdk_vring](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/dpdk_vring.png)

## kvm统计中断注入

kvm:kvm_inj_virq：表示 KVM 向 guest 注入虚拟中断。
kvm:kvm_set_irq：表示设置某个中断请求（IRQ）。
kvm:kvm_exit 和 kvm:kvm_entry：用于追踪 VM Exit 和 Entry，可能与中断处理相关。
irq:irq_handler_entry 和 irq:irq_handler_exit：追踪主机端的 IRQ 处理，可能与 virtio-net 的中断相关。

```bash
#!/usr/bin/bpftrace

tracepoint:kvm:kvm_inj_virq {
    @interrupts[args->irq] = count();
}

END {
    printf("KVM 注入的中断统计:\n");
    print(@interrupts);
}

kvm_inj_virq：这个跟踪点会在 KVM 注入虚拟中断时触发。
args->irq：表示注入的中断号（IRQ 向量）。
@interrupts：一个哈希表，用于按中断号统计注入次数。
```

## vm中软中断、硬中断

### /proc/interrupts

![vm_proc_interrupts](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/vm_proc_interrupts.png)

### /proc/softirqs

```bash
/usr/share/bcc/tools/trace -tKU pick_next_task_idle
```

![trace_pick_next_task_idle](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/trace_pick_next_task_idle.png)

追踪pick_next_task_fair函数内部:
```bash
sudo stap -ve '
probe kernel.statement("pick_next_task_fair@kernel/sched/fair.c:*") {
    printf("%s\n", pp());
}
' -DSTP_NO_BUILDID_CHECK
```

追踪 net_rx_action 实际耗时:

```bash
bpftrace -e '
kprobe:net_rx_action { 
    @start[tid] = nsecs; 
}
kretprobe:net_rx_action /@start[tid]/ { 
    @latency_us = hist((nsecs - @start[tid]) / 1000);  // 转换为微秒再生成直方图
    delete(@start[tid]); 
}
interval:s:1 {
    print(@latency_us);
    clear(@latency_us);
}'
```

```bash
bpftrace -e '
kretprobe:virtnet_poll {
    printf("time=%ld.%ld retval=%d cpu=%d pid=%d comm=%s\n", 
           nsecs / 1000, nsecs % 1000, 
           retval, cpu, pid, comm);
    printf("stack:\n%s\n", kstack);
}
'
```

```bash
bpftrace -e '
kretprobe:virtnet_poll {
        printf("time=%ld.%ld retval=%d cpu=%d pid=%d comm=%s\n", 
                nsecs / 1000, nsecs % 1000,
                retval, cpu, pid, comm);
}
'
```

```bash
bpftrace -e '
kprobe:virtnet_poll {
    printf("time=%ld.%ld cpu=%d pid=%d comm=%s\n", 
           nsecs / 1000, nsecs % 1000, 
           cpu, pid, comm);
    printf("stack:\n%s\n", kstack);
}

kretprobe:virtnet_poll {
        printf("time=%ld.%ld retval=%d cpu=%d pid=%d comm=%s\n", 
                nsecs / 1000, nsecs % 1000,
                retval, cpu, pid, comm);
}
'
```

![kprobe_virtnet_poll](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/kprobe_virtnet_poll.png)

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

![virtio_net_receive_buf](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/virtio_net_receive_buf.png)

```bash
cat /proc/net/softnet_stat
```
- 第 1 列：CPU 编号。

- 第 2 列：已处理的包数（若持续增长，说明轮询未完成）。

- 第 3 列：time_squeeze 计数（因超时退出的次数）。

```bash
[root@mengjunz-node20-hc3 ~]# cat /proc/net/softnet_stat
00b2b85e 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 00a8e14f 00000000
002923c4 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 0026a75e 00000000
00044858 00000000 0000017a 00000000 00000000 00000000 00000000 00000000 00000000 0001af31 00000000
00c051de 00000000 00000003 00000000 00000000 00000000 00000000 00000000 00000000 00b170d9 00000000
000f1032 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 000d5f66 00000000
00002bc7 00000000 000017d4 00000000 00000000 00000000 00000000 00000000 00000000 000004f5 00000000
008eb9b3 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00852e5b 00000000
008fa824 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0086279b 00000000
[root@mengjunz-node20-hc3 ~]# cat /proc/net/softnet_stat
00b30992 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 00a92dfe 00000000
002930d0 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 0026b3c7 00000000
00044858 00000000 0000017a 00000000 00000000 00000000 00000000 00000000 00000000 0001af31 00000000
00c09445 00000000 00000003 00000000 00000000 00000000 00000000 00000000 00000000 00b1aa8c 00000000
000f1038 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 000d5f6c 00000000
00002bc9 00000000 000017e0 00000000 00000000 00000000 00000000 00000000 00000000 000004f7 00000000
008f12db 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0085824d 00000000
00900f39 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00868893 00000000
[root@mengjunz-node20-hc3 ~]# cat /proc/net/softnet_stat
00b3471f 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 00a96804 00000000
002933f9 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 0026b6b4 00000000
00044861 00000000 0000017a 00000000 00000000 00000000 00000000 00000000 00000000 0001af35 00000000
00c0c10c 00000000 00000003 00000000 00000000 00000000 00000000 00000000 00000000 00b1d156 00000000
000f1038 00000000 00000001 00000000 00000000 00000000 00000000 00000000 00000000 000d5f6c 00000000
00002bca 00000000 000017e8 00000000 00000000 00000000 00000000 00000000 00000000 000004f8 00000000
008f59be 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0085c4d7 00000000
00904a0b 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0086bfe5 00000000
```

主要问题：仅 CPU 核5 的 time_squeeze 缓慢增长，但无丢包或处理延迟。

操作建议：暂不调优，持续监控；若增长加速或出现性能问题，再增大 netdev_budget 或优化多队列。
