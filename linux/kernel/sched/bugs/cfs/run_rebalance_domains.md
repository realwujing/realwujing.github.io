# run_rebalance_domains

- [CFS任务的负载均衡（load balance）](http://www.wowotech.net/process_management/load_balance_detail.html)
- [BUG: soft lockup in run_rebalance_domains](https://syzkaller.appspot.com/bug?extid=e43121895cb122e7ded5)

```bash
#!/usr/bin/env bpftrace

BEGIN {
    printf("Sampling all CPU stacks every ~10ms (99Hz)...\n");
}

// 周期性采样所有 CPU 的调用栈（每秒 99 次）
profile:hz:99 {
    $cpu = cpu;
    $pid = pid;
    $comm = comm;

    // 存储当前 CPU 的进程信息和调用栈
    @pid[$cpu] = $pid;
    @comm[$cpu] = $comm;
    @stack[$cpu] = kstack;
}

// 每 1 秒打印采样结果
interval:s:1 {
    time("%H:%M:%S\n");
    printf("CPU stacks:\n");

    // 遍历所有 CPU 的采样数据
    print(@pid);
    print(@comm);
    print(@stack);

    // 清除映射，避免累积
    clear(@pid);
    clear(@comm);
    clear(@stack);
}

// 程序退出时清理
END {
    clear(@pid);
    clear(@comm);
    clear(@stack);
}
```
