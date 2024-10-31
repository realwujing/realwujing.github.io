---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# xorg blocked when using virtio_gpu

## 初步排查

```bash
17:37:38 id:1706521058487

cat/proc/1458/stack

__switch_to+0xdc/0x148  // 进程切换的一部分，表示内核正在执行进程上下文切换的相关代码
__flush_work+0x110/0x250  // 执行工作队列刷新操作的函数
flush_work+0x10/0x18  // flush_work函数的一部分
drain_all_pages+0x178/0x248  // 执行页面排空操作的函数
__alloc_pages_nodemask+0x4c0/0xce0  // 分配内存页面的函数
ttm_pool_populate+0x314/0x460  // TTM（Translation Table Maps）内存池的填充操作
ttm_tt_populate.part.4+0x74/0x78  // TTM translation table的填充
ttm_tt_bind+0x64/0x98  // 绑定TTM translation table的操作
ttm_bo_handle_move_mem+0x36c/0x440  // TTM buffer对象在内存移动的处理
ttm_bo_validate+0x114/0x138  // 验证TTM buffer对象的函数
ttm_bo_init_reserved+0x324/0x3b8  // 初始化TTM buffer对象为预留状态
ttm_bo_init+0x3c/0xd8  // 初始化TTM buffer对象
virtio_gpu_object_create+0x114/0x170  // 创建VirtIO GPU对象的函数
virtio_gpu_alloc_object+0x14/0x30  // 分配VirtIO GPU对象的函数
virtio_gpu_resource_create_ioctl+0xa0/0x300  // 执行VirtIO GPU资源创建的ioctl操作
drm_ioctl_kernel+0x90/0xe8  // 内核层面的DRM（Direct Rendering Manager）设备IO控制
drm_ioctl+0x1c0/0x398  // DRM设备的ioctl操作
do_vfs_ioctl+0xa4/0x858  // 执行文件系统ioctl操作的内核函数
```

![switch_to](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/switch_to.png)

```c
// vim mm/page_alloc.c +2697
void drain_all_pages(struct zone *zone) // 从所有CPU中将所有per-cpu页面溢出到伙伴分配器中
{
	int cpu;

	// 在BSS中分配，以便在CONFIG_CPUMASK_OFFSTACK=y的情况下
	// 不要在直接回收路径中要求分配
	static cpumask_t cpus_with_pcps;

	// 确保在完全初始化mm_percpu_wq之前没有人触发此路径
	if (WARN_ON_ONCE(!mm_percpu_wq)) // 如果mm_percpu_wq未初始化，则产生警告并返回
		return;

	// 除非正在进行特定于区域的排空，否则不要排空
	// 此类调用方主要是CMA和内存热插拔，需要在调用返回时排空
	if (unlikely(!mutex_trylock(&pcpu_drain_mutex))) { // 如果无法获得锁，可能有排空正在进行
		if (!zone)
			return;
		mutex_lock(&pcpu_drain_mutex);
	}

	// 我们不关心与CPU热插拔事件竞争
	// 因为离线通知会导致通知的
	// CPU排空该CPU的pcps，而on_each_cpu_mask
	// 在其处理中禁用抢占
	for_each_online_cpu(cpu) { // 对每个在线CPU执行以下操作
		struct per_cpu_pageset *pcp;
		struct zone *z;
		bool has_pcps = false;

		if (zone) { // 如果指定了区域
			pcp = per_cpu_ptr(zone->pageset, cpu);
			if (pcp->pcp.count)
				has_pcps = true;
		} else { // 如果未指定区域
			for_each_populated_zone(z) { // 遍历每个已填充的内存区域
				pcp = per_cpu_ptr(z->pageset, cpu);
				if (pcp->pcp.count) {
					has_pcps = true;
					break;
				}
			}
		}

		if (has_pcps)
			cpumask_set_cpu(cpu, &cpus_with_pcps); // 如果有pcps，设置CPU位
		else
			cpumask_clear_cpu(cpu, &cpus_with_pcps); // 如果没有pcps，清除CPU位
	}

	for_each_cpu(cpu, &cpus_with_pcps) { // 对有pcps的每个CPU执行以下操作
		struct work_struct *work = per_cpu_ptr(&pcpu_drain, cpu);
		INIT_WORK(work, drain_local_pages_wq);
		queue_work_on(cpu, mm_percpu_wq, work);
	}

	for_each_cpu(cpu, &cpus_with_pcps) // 对有pcps的每个CPU执行以下操作
		flush_work(per_cpu_ptr(&pcpu_drain, cpu)); // 刷新工作队列

	mutex_unlock(&pcpu_drain_mutex); // 释放锁
}
```

for_each_cpu(cpu, &cpus_with_pcps)：

- for_each_cpu 是一个内核宏，它用于遍历指定的 CPU 集合在这里，对 cpus_with_pcps 集合中被设置的每个 CPU 进行迭代。
- cpu 是循环体内部的循环变量，表示当前迭代的 CPU。

flush_work(per_cpu_ptr(&pcpu_drain, cpu));：

- per_cpu_ptr 是一个宏，用于获取指向 per-CPU 变量的指针在这里，获取指向 pcpu_drain 在当前 CPU 上的实例的指针。
- flush_work 函数用于刷新工作队列工作队列是 Linux 内核中一种机制，允许将工作推迟到稍后执行，通常在进程上下文之外执行flush_work 会强制执行工作队列中排队的工作项。
- 因此，这一行代码的作用是对于集合 cpus_with_pcps 中每个被设置的 CPU，都刷新了与之相关的 pcpu_drain 工作队列。

总体而言，这段代码的目的是确保每个具有pcps的CPU上的 pcpu_drain 工作队列中的工作项被立即执行，以完成页面排空的操作。

## 疑点

__switch_to 和 __flush_work 是两个不同的内核函数，它们通常不会直接存在调用关系。__switch_to 主要涉及进程上下文切换，而 __flush_work 则用于刷新工作队列中的工作项。

在正常情况下，这两个函数不会直接相互调用。但是，如果在 __flush_work 的执行过程中涉及了进程上下文切换，可能会导致 __switch_to 被调用。

一些可能的情景：

- 中断上下文： 如果 __flush_work 在中断上下文中执行，而中断处理程序切换了进程上下文，可能会涉及到 __switch_to。

- 定时器上下文： 如果 __flush_work 是由定时器触发的，而定时器又在进程上下文切换的情况下执行，那么也可能导致 __switch_to 被调用。

- 其他内核上下文： 在复杂的内核场景中，有可能存在其他的内核上下文切换情况，导致 __switch_to 被执行。

## 继续排查方向

### kdump

开启kudmp：

```bash
sudo apt install kdump-tools crash kexec-tools makedumpfile linux-image-$(uname -r)-dbg
```

### panic

开启多项panic：

```bash
sudo cat << EOF >> /etc/sysctl.conf

# sysrq

# CONFIG_MAGIC_SYSRQ=y
kernel.sysrq=1

# panic

# CONFIG_PANIC_TIMEOUT=0
kernel.panic=10

# CONFIG_PANIC_ON_OOPS=y
kernel.panic_on_oops=1

# CONFIG_DETECT_HUNG_TASK=y CONFIG_BOOTPARAM_HUNG_TASK_PANIC=y
kernel.hung_task_panic=1

# CONFIG_DEFAULT_HUNG_TASK_TIMEOUT=120
kernel.hung_task_timeout_secs=60

vm.panic_on_oom=1

# CONFIG_SOFTLOCKUP_DETECTOR=y BOOTPARAM_SOFTLOCKUP_PANIC=y
kernel.softlockup_panic=1

kernel.panic_on_warn=1
EOF
```

```bash
sysctl -p
```

### offcputime

```bash
ps aux | grep -i xorg
root       17159  1.3  0.7 660348 122888 tty1    Ssl+ 1月31  20:18 /usr/lib/xorg/Xorg -background none :0 -seat seat0 -auth /var/run/lightdm/root/:0 -nolisten tcp vt1 -novtswitch
wujing    425544  0.0  0.0  14180  2328 pts/3    S+   14:53   0:00 grep -i xorg
```

```bash
sudo apt install linux-headers-$(uname -r) bpftrace bpfcc-tools
```

查看xorg阻塞时的内核调用栈，参考bpf之巅6.3.9节 offcputime 232页：

```bash
offcputime-bpfcc -p 6065
```

筛选处于TASK_UNINTERRUPTIBLE状态的用户态进程并打印内核调用栈，参考bpf之巅14.4.2节 offcputime 677页：

```bash
offcputime-bpfcc -uK --state=2
```

### wakeuptime

查看xorg进程被哪个进程阻塞：

```bash
wakeuptime -p $(pgerp -n Xorg) 5
```

### offwaketime

查看xorg进程被哪个进程阻塞:

```bash
offwaketime -p $(pgerp -n Xorg) 5
```
