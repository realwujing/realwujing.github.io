---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# very high memory usage due to kernfs_node_cache slabs

<https://pms.uniontech.com/bug-view-238303.html>

## bug环境

```bash
uname -a
Linux 0000000g-A8cUta6pu5 4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819 SMP Fri Jan 5 12:27:47 CST 2024 aarch64 GNU/Linux
```

```bash
apt policy systemd
systemd:
  已安装：241.61-deepin1
  候选： 241.61-deepin1
  版本列表：
 *** 241.61-deepin1 500
        500 http://pools.uniontech.com/ppa/dde-eagle eagle/1070/main arm64 Packages
        100 /var/lib/dpkg/status
```

查看当前用户密码，方便切root：

```bash
cat /var/log/cloud-init.log | grep ctyun
```

## 初步分析

### Slab

#### /proc/meminfo

```bash
cat /proc/meminfo | grep Slab: -A2
Slab:             913952 kB
SReclaimable:     175568 kB
SUnreclaim:       738384 kB
```

#### slabtop

```bash
slabtop -s c -o | head -n20
 Active / Total Objects (% used)    : 924058 / 1039323 (88.9%)
 Active / Total Slabs (% used)      : 43496 / 43496 (100.0%)
 Active / Total Caches (% used)     : 121 / 187 (64.7%)
 Active / Total Size (% used)       : 815879.95K / 908598.60K (89.8%)
 Minimum / Average / Maximum Object : 0.36K / 0.87K / 16.81K

  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME                   
 55251  52846  95%    4.44K   7893        7    252576K names_cache            
229119 223856  97%    0.48K   6943       33    111088K kernfs_node_cache      
141850 137390  96%    0.62K   5674       25     90784K kmalloc-128            
 13776  12659  91%    4.50K   1968        7     62976K kmalloc-4096           
104670  87963  84%    0.53K   3489       30     55824K dentry                 
 63020  52097  82%    0.69K   2740       23     43840K filp                   
 72090  59483  82%    0.53K   2403       30     38448K vm_area_struct         
 21142  19921  94%    1.41K    961       22     30752K ext4_inode_cache       
 58555  55044  94%    0.45K   1673       35     26768K buffer_head            
 38775  34343  88%    0.62K   1551       25     24816K skbuff_head_cache      
 23256  21205  91%    0.94K    684       34     21888K inode_cache            
 21248  17845  83%    1.00K    664       32     21248K kmalloc-512            
 47502  40866  86%    0.41K   1218       39     19488K anon_vma_chain       
```

#### slub_debug

打开下方slub_debug相关内核编译选项：

```bash
CONFIG_SLUB=y

CONFIG_SLUB_DEBUG=y

CONFIG_SLUB_DEBUG_ON=y

CONFIG_SLUB_STATS=y
```

在grub中增加参数：

```bash
slub_debug=UFPZ
```

追踪kernfs_node_cache分配释放：

```bash
echo 1 > /sys/kernel/slab/kernfs_node_cache/trace  && sleep 60 && echo 0 > /sys/kernel/slab/kernfs_node_cache/trace
```

```bash
/sys/kernel/slab/kernfs_node_cache/group
```

在/var/log/kern.log中查看kernfs_node_cache alloc:

```bash
1673 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.452633] TRACE kernfs_node_cache alloc 0x00000000a05f4917 inuse=35 fp=0x          (null)
1674 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.454989] CPU: 7 PID: 1 Comm: systemd Tainted: G           O      4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819
1675 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.457735] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
1676 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.459811] Call trace:
1677 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.460466]  dump_backtrace+0x0/0x190
1678 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.461368]  show_stack+0x14/0x20
1679 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.462291]  dump_stack+0xa8/0xcc
1680 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.463168]  alloc_debug_processing+0x58/0x188
1681 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.464272]  ___slab_alloc.constprop.34+0x31c/0x388
1682 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.465491]  kmem_cache_alloc+0x210/0x278
1683 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.466538]  __kernfs_new_node+0x60/0x1f8
1684 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.467553]  kernfs_new_node+0x24/0x48
1685 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.468508]  kernfs_create_dir_ns+0x30/0x88
1686 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.469584]  cgroup_mkdir+0x2f0/0x4e8
1687 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.470516]  kernfs_iop_mkdir+0x58/0x88
1688 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.471497]  vfs_mkdir+0xfc/0x1c0
1689 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.472328]  do_mkdirat+0xec/0x100
1690 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.473172]  __arm64_sys_mkdirat+0x1c/0x28
1691 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.474210]  el0_svc_common+0x90/0x178
1692 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.475125]  el0_svc_handler+0x9c/0xa8
1693 2024-01-24 12:40:55 0000000g-A8cUta6pu5 kernel: [  201.476049]  el0_svc+0x8/0xc
```

```bash
grep "kernfs_node_cache alloc" kern.log | wc -l
7239
```

```bash
./scripts/faddr2line vmlinux vfs_mkdir+0xfc/0x1c0
vfs_mkdir+0xfc/0x1c0:
vfs_mkdir 于 fs/namei.c:3820
```

```bash
./scripts/faddr2line vmlinux kernfs_iop_mkdir+0x58/0x88
kernfs_iop_mkdir+0x58/0x88:
kernfs_iop_mkdir 于 fs/kernfs/dir.c:1120
```

```bash
./scripts/faddr2line vmlinux cgroup_mkdir+0x2f0/0x4e8
cgroup_mkdir+0x2f0/0x4e8:
kernfs_create_dir 于 include/linux/kernfs.h:507
(已内连入)cgroup_mkdir 于 kernel/cgroup/cgroup.c:5032
```

在/var/log/kern.log中查看kernfs_node_cache free:

```bash
3106 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.212900] TRACE kernfs_node_cache free 0x00000000e2ea365c inuse=34 fp=0x00000000a8805aea
3107 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.215538] Object 00000000e2ea365c: 00 00 00 00 01 00 00 80 78 a2 37 a6 03 80 ff ff  ........x.7.....
3108 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.218656] Object 000000003a5b5659: 00 52 45 de 03 80 ff ff 40 92 37 a6 03 80 ff ff  .RE.....@.7.....
3109 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.221710] Object 000000007229340d: 80 b6 37 a6 03 80 ff ff 00 00 00 00 00 00 00 00  ..7.............
3110 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.224824] Object 00000000cc138d0e: 00 00 00 00 00 00 00 00 7e 16 88 71 00 00 00 00  ........~..q....
3111 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.227947] Object 000000001cecdc04: b0 e4 a2 09 00 00 ff ff 00 00 00 00 00 00 00 00  ................
3112 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.231088] Object 00000000c3ac2227: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
3113 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.234279] Object 00000000737356c8: 98 dc a2 09 00 00 ff ff 14 09 00 00 01 00 00 00  ................
3114 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.237384] Object 00000000ba78c59c: 52 20 a4 81 00 00 00 00 00 00 00 00 00 00 00 00  R ..............
3115 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.240508] CPU: 3 PID: 1 Comm: systemd Tainted: G           O      4.19.0-arm64-desktop-tyy-5819-ext4-slub-debug-kmemleak #5819
3116 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.244280] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
3117 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.246454] Call trace:
3118 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.247264]  dump_backtrace+0x0/0x190
3119 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.248512]  show_stack+0x14/0x20
3120 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.249584]  dump_stack+0xa8/0xcc
3121 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.250672]  free_debug_processing+0x19c/0x3a0
3122 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.252143]  __slab_free+0x230/0x3f8
3123 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.253321]  kmem_cache_free+0x200/0x220
3124 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.254657]  kernfs_put+0x100/0x238
3125 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.255806]  kernfs_evict_inode+0x2c/0x38
3126 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.257119]  evict+0xc0/0x1c0
3127 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.258067]  iput+0x1c8/0x288
3128 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.259371]  dentry_unlink_inode+0xb0/0xe8
3129 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.260600]  __dentry_kill+0xc4/0x1d0
3130 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.261434]  shrink_dentry_list+0x1ac/0x2c0
3131 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.262413]  shrink_dcache_parent+0x78/0x80
3132 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.263336]  vfs_rmdir+0xf0/0x190
3133 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.264066]  do_rmdir+0x1c0/0x1f8
3134 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.264791]  __arm64_sys_unlinkat+0x4c/0x60
3135 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.265710]  el0_svc_common+0x90/0x178
3136 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.266968]  el0_svc_handler+0x9c/0xa8
3137 2024-01-24 12:40:56 0000000g-A8cUta6pu5 kernel: [  203.267828]  el0_svc+0x8/0xc
```

```bash
grep "kernfs_node_cache free" kern.log | wc -l
5034
```

### trace-bpfcc

```bash
sudo apt install systemd-dbgsym
apt source systemd=241.61-deepin1
```

```bash
trace-bpfcc -tKU 'r::kernfs_new_node "%llx", retval'
```

从kernfs_new_node.log内容来看，主要有内核线程kworker和systemd在调用kernfs_new_node。

#### kworker

```bash
3.796820 47      47      kworker/3:1     kernfs_new_node  
        kernfs_new_node+0x0 [kernel]
        sysfs_add_file_mode_ns+0x9c [kernel]
        internal_create_group+0x104 [kernel]
        sysfs_create_group+0x14 [kernel]  # 上游4.19.306这里没走到
        sysfs_slab_add+0xb8 [kernel]      # 上游4.19.306这里走到了
        __kmem_cache_create+0x128 [kernel]
        create_cache+0xcc [kernel]
        memcg_create_kmem_cache+0xf8 [kernel]
        memcg_kmem_cache_create_func+0x1c [kernel]
        process_one_work+0x1e8 [kernel]
        worker_thread+0x48 [kernel]
        kthread+0x128 [kernel]
        ret_from_fork+0x10 [kernel]
        -14
```

在 Linux 内核中，错误代码 -14 对应的含义是 EFAULT，表示发生了“无效地址”（Bad address）错误。通常情况下，EFAULT 错误表示系统调用中传递的参数指针指向的地址无效，无法访问或者无法写入。

##### sysfs_slab_add

tyy-1053 sysfs_slab_add 参数追踪：

```c
trace-bpfcc -t -Ilinux/slab.h -Ilinux/slub_def.h 'sysfs_slab_add(struct kmem_cache *s) "%llx" s->memcg_params.root_cache'
TIME     PID     TID     COMM            FUNC             -
96.61296 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f10d0780
96.61350 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f10d0400
96.61372 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f10d3880
96.61390 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f10d2e00
96.61434 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f131b500
96.61457 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f0a3a700
96.61485 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f131b880
96.61504 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff684780
96.61535 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff7d4b00
96.61571 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff7d5580
96.61589 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff7d6700
96.61608 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff7d7500
96.61624 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff7d4780
96.61962 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001f10d2a80
96.61981 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff684400
96.62001 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff687180
96.62299 23371   23371   kworker/2:1     sysfs_slab_add   ffff8001ff687500
```

可以看到root_cache不是null。

##### ret_from_fork+0x10

```bash
./scripts/faddr2line vmlinux ret_from_fork+0x10
ret_from_fork+0x10/0x18:
ret_from_fork 于 arch/arm64/kernel/entry.S:1063
```

```c vim arch/arm64/kernel/entry.S +1063
1055 /*
1056  * This is how we return from a fork.
1057  */
1058 ENTRY(ret_from_fork)
1059     bl  schedule_tail                       // 调用 schedule_tail 函数，执行内核线程的调度
1060     cbz x19, 1f                            // 如果 x19 为零，则跳转到标签 1f，表示不是内核线程
1061     mov x0, x20                            // 将 x20 的值移动到 x0 寄存器中，作为返回值
1062     blr x19                                // 通过 blr 指令跳转到 x19 寄存器中存储的地址，返回到 fork 调用的位置
1063 1:  get_thread_info tsk                    // 获取当前线程的线程信息，并存储在 tsk 中
1064     b   ret_to_user                        // 跳转到 ret_to_user 标签，返回到用户空间
1065 ENDPROC(ret_from_fork)                     // 结束 ret_from_fork 过程
1066 NOKPROBE(ret_from_fork)                    // 禁用对 ret_from_fork 过程的探查（probing）
```

##### kthread+0x128

```bash
./scripts/faddr2line vmlinux kthread+0x128
kthread+0x128/0x130:
kthread 于 kernel/kthread.c:246
```

```c // vim kernel/kthread.c +246
205 static int kthread(void *_create)
206 {
207     /* Copy data: it's on kthread's stack */
208     struct kthread_create_info *create = _create;    // 复制数据：它位于 kthread 的堆栈上
209     int (*threadfn)(void *data) = create->threadfn; // 获取线程函数指针
210     void *data = create->data;                      // 获取线程函数的参数
211     struct completion *done;                        // 完成标志
212     struct kthread *self;                           // 当前线程结构指针
213     int ret;                                        // 返回值
214 
215     self = kzalloc(sizeof(*self), GFP_KERNEL);      // 分配 kthread 结构内存
216     set_kthread_struct(self);                       // 设置线程结构
217 
218     /* If user was SIGKILLed, I release the structure. */
219     done = xchg(&create->done, NULL);               // 交换完成标志
220     if (!done) {                                    // 如果完成标志为空
221         kfree(create);                              // 释放线程结构内存
222         do_exit(-EINTR);                            // 退出线程
223     }
224 
225     if (!self) {                                    // 如果线程结构为空
226         create->result = ERR_PTR(-ENOMEM);          // 设置返回结果为内存分配错误
227         complete(done);                             // 标记完成
228         do_exit(-ENOMEM);                           // 退出线程
229     }
230 
231     self->data = data;                              // 设置线程结构中的参数
232     init_completion(&self->exited);                 // 初始化完成标志
233     init_completion(&self->parked);                 // 初始化完成标志
234     current->vfork_done = &self->exited;            // 设置 vfork 完成标志
235 
236     /* OK, tell user we're spawned, wait for stop or wakeup */
237     __set_current_state(TASK_UNINTERRUPTIBLE);      // 设置当前线程状态为不可中断
238     create->result = current;                       // 设置返回结果为当前线程
239     complete(done);                                 // 标记完成
240     schedule();                                     // 进行调度
241 
242     ret = -EINTR;                                   // 设置返回值为中断错误
243     if (!test_bit(KTHREAD_SHOULD_STOP, &self->flags)) { // 如果线程未被停止
244         cgroup_kthread_ready();                     // 准备 cgroup 线程
245         __kthread_parkme(self);                     // 将当前线程挂起
246         ret = threadfn(data);                       // 执行线程函数
247     }
248     do_exit(ret);                                   // 退出线程
249 }
```

###### worker_thread+0x48

```bash
./scripts/faddr2line vmlinux worker_thread+0x48
worker_thread+0x48/0x4c8:
__read_once_size 于 include/linux/compiler.h:193
(已内连入)list_empty 于 include/linux/list.h:203
(已内连入)worker_thread 于 kernel/workqueue.c:2297
```

```c // vim kernel/workqueue.c +2297
2226 /**
2227  * worker_thread - the worker thread function
2228  * @__worker: self
2229  *
2230  * 工作线程函数。所有的工作线程都属于一个工作池 - 无论是每个 CPU 的一个还是动态的无绑定的一个。
2231  * 这些工作线程处理所有的工作项，而不管它们具体的目标工作队列是什么。唯一的例外是属于具有救援者的工作队列的工作项，
2232  * 这将在 rescuer_thread() 中解释。
2233  *
2234  * 返回值: 0
2235  */
2236 static int worker_thread(void *__worker)
2237 {
2238     struct worker *worker = __worker;           // 获取工作线程结构指针
2239     struct worker_pool *pool = worker->pool;    // 获取工作池指针
2240 
2241     /* 告诉调度程序这是一个工作队列工作者 */
2242     set_pf_worker(true);
2243 woke_up:
2244     spin_lock_irq(&pool->lock);                 // 获取工作池的自旋锁
2245 
2246     /* 我是否要死了？ */
2247     if (unlikely(worker->flags & WORKER_DIE)) {
2248         spin_unlock_irq(&pool->lock);           // 释放工作池的自旋锁
2249         WARN_ON_ONCE(!list_empty(&worker->entry));
2250         set_pf_worker(false);
2251 
2252         set_task_comm(worker->task, "kworker/dying");
2253         ida_simple_remove(&pool->worker_ida, worker->id);
2254         worker_detach_from_pool(worker);
2255         kfree(worker);
2256         return 0;
2257     }
2258 
2259     worker_leave_idle(worker);                  // 离开空闲状态
2260 recheck:
2261     /* 不再需要更多的工作线程？ */
2262     if (!need_more_worker(pool))                // 如果不再需要更多的工作线程
2263         goto sleep;                             // 跳转到休眠状态
2264 
2265     /* 我们需要管理吗？ */
2266     if (unlikely(!may_start_working(pool)) && manage_workers(worker)) // 如果不可能开始工作并且需要管理工作线程
2267         goto recheck;                           // 重新检查
2268 
2269     /*
2270      * ->scheduled 列表只能在工作线程准备处理工作或实际处理工作时填充。
2271      * 确保我在睡眠时没有人捣乱。
2272      */
2273     WARN_ON_ONCE(!list_empty(&worker->scheduled));
2274 
2275     /*
2276      * 完成 PREP 阶段。我们保证至少有一个空闲的工作线程或其他人已经承担了管理者的角色。
2277      * 这是 @worker 开始参与并发管理的地方（如果适用），并且在重新绑定后恢复并发管理。有关详细信息，请参见 rebind_workers()。
2278      */
2279     worker_clr_flags(worker, WORKER_PREP | WORKER_REBOUND);
2280 
2281     do {
2282         struct work_struct *work =
2283             list_first_entry(&pool->worklist,
2284                      struct work_struct, entry);
2285 
2286         pool->watchdog_ts = jiffies;
2287 
2288         if (likely(!(*work_data_bits(work) & WORK_STRUCT_LINKED))) {
2289             /* 优化路径，不是严格必要的 */
2290             process_one_work(worker, work);
2291             if (unlikely(!list_empty(&worker->scheduled)))
2292                 process_scheduled_works(worker);
2293         } else {
2294             move_linked_works(work, &worker->scheduled, NULL);
2295             process_scheduled_works(worker);
2296         }
2297     } while (keep_working(pool));
2298 
2299     worker_set_flags(worker, WORKER_PREP);
2300 sleep:
2301     /*
2302      * 持有 pool->lock，并且没有要处理的工作，也没有需要管理，就休眠。
2303      * 只有在持有 pool->lock 或者从本地 CPU 上唤醒工作线程时，才会唤醒工作线程，因此在释放 pool->lock 前设置当前状态就足以防止丢失任何事件。
2304      */
2305     worker_enter_idle(worker);                   // 进入空闲状态
2306     __set_current_state(TASK_IDLE);              // 设置当前线程状态为空闲
2307     spin_unlock_irq(&pool->lock);                // 释放工作池的自旋锁
2308     schedule();                                  // 进行调度
2309     goto woke_up;                                // 回到唤醒状态
2310 }
```

###### create_worker

```c // vim kernel/workqueue.c +1813
1779  * create_worker - 创建一个新的工作队列工作者                                                                                           
1780  * @pool: 新工作者将属于的工作池                                                                                              
1781  *                                                                                                                                         
1782  * 创建并启动一个新的工作者，它附加到 @pool。                                                                               
1783  *                                                                                                                                         
1784  * 上下文：                                                                                                                                
1785  * 可能睡眠。进行 GFP_KERNEL 分配。                                                                                              
1786  *                                                                                                                                         
1787  * 返回值：                                                                                                                                 
1788  * 指向新创建的工作者的指针。                                                                                                    
1789  */                                                                                                                                        
1790 static struct worker *create_worker(struct worker_pool *pool)                                                                              
1791 {                                                                                                                                          
1792     struct worker *worker = NULL;                                                                                                          
1793     int id = -1;                                                                                                                           
1794     char id_buf[16];                                                                                                                       
1795                                                                                                                                            
1796     /* 需要ID来确定 kthread 的名称 */                                                                                           
1797     id = ida_simple_get(&pool->worker_ida, 0, 0, GFP_KERNEL);                                                                              
1798     if (id < 0)                                                                                                                            
1799         goto fail;                                                                                                                         
1800                                                                                                                                            
1801     worker = alloc_worker(pool->node);                                                                                                     
1802     if (!worker)                                                                                                                           
1803         goto fail;                                                                                                                         
1804                                                                                                                                            
1805     worker->id = id;                                                                                                                       
1806                                                                                                                                            
1807     if (pool->cpu >= 0)                                                                                                                    
1808         snprintf(id_buf, sizeof(id_buf), "%d:%d%s", pool->cpu, id,                                                                         
1809              pool->attrs->nice < 0  ? "H" : "");                                                                                           
1810     else                                                                                                                                   
1811         snprintf(id_buf, sizeof(id_buf), "u%d:%d", pool->id, id);                                                                          
1812                                                                                                                                            
1813     worker->task = kthread_create_on_node(worker_thread, worker, pool->node,
1814                           "kworker/%s", id_buf); /* 创建内核线程 */
1815     if (IS_ERR(worker->task)) /* 如果创建失败 */
1816         goto fail; /* 跳转到 fail 标签 */
1817 
1818     set_user_nice(worker->task, pool->attrs->nice); /* 设置内核线程的优先级 */
1819     kthread_bind_mask(worker->task, pool->attrs->cpumask); /* 绑定内核线程到指定的 CPU 掩码 */
1820                                                                                                                                                                           
1821     /* 成功，将工作者附加到工作池 */                                                                                        
1822     worker_attach_to_pool(worker, pool);                                                                                                   
1823                                                                                                                                            
1824     /* 启动新创建的工作者 */                                                                                                   
1825     spin_lock_irq(&pool->lock);                                                                                                            
1826     worker->pool->nr_workers++;                                                                                                            
1827     worker_enter_idle(worker);                                                                                                             
1828     wake_up_process(worker->task);                                                                                                         
1829     spin_unlock_irq(&pool->lock);                                                                                                          
1830                                  
1831     return worker; /* 返回创建的工作者 */
1832 
1833 fail:
1834     if (id >= 0)
1835         ida_simple_remove(&pool->worker_ida, id); /* 移除已经获取的 ID */
1836     kfree(worker); /* 释放已经分配的工作者 */
1837     return NULL; /* 返回 NULL */
1838 }
```

###### kthread_create_on_node

```c
// vim kernel/kthread.c +370

347 /**
 348  * kthread_create_on_node - 创建一个内核线程。
 349  * @threadfn: 要运行的函数，直到 signal_pending(current)。
 350  * @data: @threadfn 的数据指针。
 351  * @node: 为线程分配任务和线程结构的节点。
 352  * @namefmt: 线程的 printf 格式名称。
 353  *                                                                                                                                         
 354  * 描述：这个辅助函数创建并命名一个内核线程。
 355  * 线程将会被停止：使用 wake_up_process() 来启动它。
 356  * 也可以参见 kthread_run()。
 357  * 新线程具有 SCHED_NORMAL 策略，并且与所有 CPU 有亲和性。
 358  *                                                                                                                                         
 359  * 如果线程将绑定到特定的 CPU，请在 @node 中给出它的节点，
 360  * 以获取 kthread 堆栈的 NUMA 亲和性，否则给出 NUMA_NO_NODE。
 361  * 在唤醒时，线程将以 @data 作为其参数运行 @threadfn()。
 362  * @threadfn() 可以直接调用 do_exit()，如果它是一个独立的线程，
 363  * 没有人会调用 kthread_stop()，或者当 'kthread_should_stop()' 为真时返回
 364  * （这意味着已经调用了 kthread_stop()）。
 365  * 返回值应该是零或负的错误号码；它将传递给 kthread_stop()。
 366  *                                                                                                                                         
 367  * 返回一个 task_struct 或 ERR_PTR(-ENOMEM) 或 ERR_PTR(-EINTR)。
 368  */                                                                                                                                        
 369 struct task_struct *kthread_create_on_node(int (*threadfn)(void *data),                                                                    
 370                        void *data, int node,                                                                                               
 371                        const char namefmt[],                                                                                               
 372                        ...)                                                                                                                
 373 {                                                                                                                                          
 374     struct task_struct *task;                                                                                                              
 375     va_list args;                                                                                                                          
 376                                                                                                                                            
 377     va_start(args, namefmt);                                                                                                               
 378     task = __kthread_create_on_node(threadfn, data, node, namefmt, args);                                                                  
 379     va_end(args);                                                                                                                          
 380                                                                                                                                            
 381     return task;                                                                                                                           
 382 }                                                                                                                                          
 383 EXPORT_SYMBOL(kthread_create_on_node);  

```

###### maybe_create_worker

```c
// vim kernel/workqueue.c +1972

1943 /**
1944  * maybe_create_worker - 如果需要，创建一个新的工作线程
1945  * @pool: 用于创建新工作线程的线程池
1946  *
1947  * 如果需要，为 @pool 创建一个新的工作线程。函数返回时，保证 @pool 至少有一个空闲的工作线程。
1948  * 如果创建一个新的工作线程花费的时间超过 MAYDAY_INTERVAL，可能会向所有在 @pool 上计划工作的救援者发送求助信号，
1949  * 以解决可能的分配死锁。
1950  *
1951  * 返回时，need_to_create_worker() 保证为 %false，may_start_working() 保证为 %true。
1952  *
1953  * 锁定：
1954  * spin_lock_irq(pool->lock) 可能会释放和重新获取多次。
1955  * 执行 GFP_KERNEL 分配。
1956  * 仅从管理器调用。
1957  */
1958 static void maybe_create_worker(struct worker_pool *pool)
1959 __releases(&pool->lock)
1960 __acquires(&pool->lock)
1961 {
1962 restart:
1963     spin_unlock_irq(&pool->lock); /* 解锁线程池 */
1964 
1965     /* 如果在 MAYDAY_INITIAL_TIMEOUT 内没有取得进展，请求帮助 */
1966     mod_timer(&pool->mayday_timer, jiffies + MAYDAY_INITIAL_TIMEOUT);
1967 
1968     while (true) {
1969         if (create_worker(pool) || !need_to_create_worker(pool)) /* 创建一个工作线程或者不需要创建工作线程 */
1970             break;
1971 
1972         schedule_timeout_interruptible(CREATE_COOLDOWN); /* 在指定的时间内休眠 */
1973 
1974         if (!need_to_create_worker(pool)) /* 如果不需要创建工作线程 */
1975             break;
1976     }
1977 
1978     del_timer_sync(&pool->mayday_timer); /* 删除计时器 */
1979     spin_lock_irq(&pool->lock); /* 重新加锁线程池 */
1980     /*
1981      * 即使刚刚成功创建了一个新的工作线程，也需要这样做，
1982      * 因为 @pool->lock 已经被释放，新的工作线程可能已经变得繁忙。
1983      */
1984     if (need_to_create_worker(pool)) /* 如果需要创建工作线程 */
1985         goto restart; /* 重新开始 */
1986 }
```

##### process_one_work+0x1e8

```bash
./scripts/faddr2line vmlinux process_one_work+0x1e8
process_one_work+0x1e8/0x438:
arch_static_branch 于 arch/arm64/include/asm/jump_label.h:20
(已内连入)static_key_false 于 include/linux/jump_label.h:138
(已内连入)trace_workqueue_execute_end 于 include/trace/events/workqueue.h:114
(已内连入)process_one_work 于 kernel/workqueue.c:2158
```

```c
// vim kernel/workqueue.c +2158
// 2158行对应下方代码2133行

2032 /**
2033  * process_one_work - 处理单个工作项
2034  * @worker: 自身
2035  * @work: 要处理的工作项
2036  *
2037  * 处理 @work。此函数包含处理单个工作项所需的所有逻辑，包括与同一 CPU 上的其他工作线程同步和交互、排队和刷新。只要满足上下文要求，任何工作线程都可以调用此函数来处理工作项。
2038  *
2039  * 上下文：
2040  * spin_lock_irq(pool->lock) 在释放并重新获取。
2041  */
2042 static void process_one_work(struct worker *worker, struct work_struct *work)
2043 __releases(&pool->lock) /* 释放锁 */
2044 __acquires(&pool->lock) /* 获取锁 */
2045 {
2046     struct pool_workqueue *pwq = get_work_pwq(work); /* 获取工作项所属的 pool_workqueue 结构 */
2047     struct worker_pool *pool = worker->pool; /* 获取工作线程所属的 worker_pool 结构 */
2048     bool cpu_intensive = pwq->wq->flags & WQ_CPU_INTENSIVE; /* 判断工作队列是否为 CPU 密集型 */
2049     int work_color;
2050     struct worker *collision;
2051 #ifdef CONFIG_LOCKDEP
2052     /*
2053      * 在调用它的函数中释放 struct work_struct 是允许的，因此我们也需要考虑 lockdep。
2054      * 为了避免错误的“持有的锁被释放”警告以及在查看 work->lockdep_map 时出现问题，我们在这里复制并使用。
2055      */
2056     struct lockdep_map lockdep_map;
2057 
2058     lockdep_copy_map(&lockdep_map, &work->lockdep_map);
2059 #endif
2060     /* 确保我们在正确的 CPU 上 */
2061     WARN_ON_ONCE(!(pool->flags & POOL_DISASSOCIATED) &&
2062              raw_smp_processor_id() != pool->cpu);
2063 
2064     /*
2065      * 单个工作项不应该由单个 CPU 上的多个工作线程并发执行。
2066      * 检查是否有任何人已经在处理工作项。如果是，则推迟工作项到当前正在执行的工作线程。
2067      */
2068     collision = find_worker_executing_work(pool, work);
2069     if (unlikely(collision)) {
2070         move_linked_works(work, &collision->scheduled, NULL); /* 移动链接的工作项 */
2071         return;
2072     }
2073 
2074     /* 立即声明和出队 */
2075     debug_work_deactivate(work); /* 调试：取消激活工作项 */
2076     hash_add(pool->busy_hash, &worker->hentry, (unsigned long)work); /* 将工作项添加到 busy_hash 中 */
2077     worker->current_work = work; /* 设置当前正在处理的工作项 */
2078     worker->current_func = work->func; /* 设置当前正在处理的函数 */
2079     worker->current_pwq = pwq; /* 设置当前工作线程的 pool_workqueue 结构 */
2080     work_color = get_work_color(work); /* 获取工作项颜色 */
2081 
2082     /*
2083      * 记录工作队列名称以用于命令行和调试报告，可能通过 set_worker_desc() 被覆盖。
2084      */
2085     strscpy(worker->desc, pwq->wq->name, WORKER_DESC_LEN); /* 复制工作队列名称 */
2086 
2087     list_del_init(&work->entry); /* 从队列中移除工作项 */
2088 
2089     /*
2090      * CPU 密集型工作不参与并发管理。
2091      * 它们是调度器的责任。这将把 @worker 从并发管理中移除，下一个代码块将链式执行挂起的工作项。
2092      */
2093     if (unlikely(cpu_intensive))
2094         worker_set_flags(worker, WORKER_CPU_INTENSIVE); /* 设置工作线程标志为 CPU 密集型 */
2095 
2096     /*
2097      * 如果需要，唤醒另一个工作线程。对于普通的 per-cpu 工作线程，此条件始终为 false，因为此时 nr_running 通常为 >= 1。
2098      * 这用于链式执行挂起的工作项，对于 WORKER_NOT_RUNNING 工作线程（例如 UNBOUND 和 CPU_INTENSIVE），此条件始终为 true。
2099      */
2100     if (need_more_worker(pool))
2101         wake_up_worker(pool); /* 唤醒另一个工作线程 */
2102 
2103     /*
2104      * 记录最后的 pool 并清除 PENDING，这应该是 @work 的最后更新。
2105      * 此外，在 @pool->lock 中执行此操作，以便在禁用 IRQ 时同时进行 PENDING 和队列状态的更改。
2106      */
2107     set_work_pool_and_clear_pending(work, pool->id); /* 设置工作项所属的 pool 并清除 PENDING */
2108 
2109     spin_unlock_irq(&pool->lock); /* 解锁 */
2110 
2111     lock_map_acquire(&pwq->wq->lockdep_map); /* 获取工作队列的 lockdep_map */
2112     lock_map_acquire(&lockdep_map); /* 获取 lockdep_map */
2113     /*
2114      * 严格来说，我们应该在不持有任何锁的情况下标记不变状态，也就是说，在这两个 lock_map_acquire() 之前。
2115      *
2116      * 然而，这会导致：
2117      *
2118      *   A(W1)
2119      *   WFC(C)
2120      *      A(W1)
2121      *      C(C)
2122      *
2123      * 这将创建 W1->C->W1 依赖关系，即使实际上不存在死锁问题。有两种解决方案，一种是在工作（队列）'锁'上使用读递归获取，但是这将击中递归锁的 lockdep 限制，或者简单地丢弃这些锁。
2124      *
2125      * 据我所知，在 flush_work() 和 complete() 原语之间不存在可能的死锁场景（单线程工作队列除外），因此隐藏它们不是问题。
2126      */
2127     lockdep_invariant_state(true); /* 锁定状态 */
2128     trace_workqueue_execute_start(work); /* 跟踪工作项执行的开始 */
2129     worker->current_func(work); /* 执行工作项的函数 */
2130     /*
2131      * 虽然我们必须小心在此之后不要使用 "work"，但跟踪点仅记录其地址。
2132      */
2133     trace_workqueue_execute_end(work); /* 跟踪工作项执行的结束 */
2134     lock_map_release(&lockdep_map); /* 释放 lockdep_map */
2135     lock_map_release(&pwq->wq->lockdep_map); /* 释放工作队列的 lockdep_map */
2136 
2137     if (unlikely(in_atomic() || lockdep_depth(current) > 0)) {
2138         pr_err("BUG: workqueue leaked lock or atomic: %s/0x%08x/%d\n"
2139                "     last function: %pf\n",
2140                current->comm, preempt_count(), task_pid_nr(current),
2141                worker->current_func);
2142         debug_show_held_locks(current);
2143         dump_stack();
2144     }
2145 
2146     /*
2147      * 以下防止 kworker 在 !PREEMPT 内核中独占 CPU，在此情况下，等待某些事情发生的重新排队工作项可能会与 stop_machine 死锁，因为这样的工作项可能会无限期地重新排队，而所有其他 CPU 都被困在 stop_machine 中。同时，报告一个静态的 RCU 状态，因此相同的条件不会冻结 RCU。
2148      */
2149     cond_resched(); /* 条件调度 */
2150 
2151     spin_lock_irq(&pool->lock); /* 加锁 */
2152 
2153     /* 清除 CPU 密集型状态 */
2154     if (unlikely(cpu_intensive))
2155         worker_clr_flags(worker, WORKER_CPU_INTENSIVE); /* 清除工作线程标志为 CPU 密集型 */
2156 
2157     /* 完成，释放 */
2158     hash_del(&worker->hentry); /* 从 busy_hash 中删除 */
2159     worker->current_work = NULL; /* 清除当前工作项 */
2160     worker->current_func = NULL; /* 清除当前函数 */
2161     worker->current_pwq = NULL; /* 清除当前 pool_workqueue 结构 */
2162     pwq_dec_nr_in_flight(pwq, work_color); /* 减少在飞行中的工作项数量 */
2163 }
```

##### memcg_kmem_cache_create_func+0x1c

```bash
./scripts/faddr2line vmlinux memcg_kmem_cache_create_func+0x1c
memcg_kmem_cache_create_func+0x1c/0x90:
css_put 于 include/linux/cgroup.h:391
(已内连入)memcg_kmem_cache_create_func 于 mm/memcontrol.c:2509
```

```c
2500 static void memcg_kmem_cache_create_func(struct work_struct *w)
2501 {
2502     struct memcg_kmem_cache_create_work *cw =
2503         container_of(w, struct memcg_kmem_cache_create_work, work); /* 将结构体指针从工作项指针中获取 */
2504     struct mem_cgroup *memcg = cw->memcg; /* 获取内存控制组指针 */
2505     struct kmem_cache *cachep = cw->cachep; /* 获取内核内存高速缓存指针 */
2506 
2507     memcg_create_kmem_cache(memcg, cachep); /* 创建内存控制组的内核内存高速缓存 */
2508 
2509     css_put(&memcg->css); /* 减少内存控制组引用计数 */
2510     kfree(cw); /* 释放内存控制组的 kmem_cache 创建工作项 */
2511 }
```

###### __memcg_schedule_kmem_cache_create

```c
// vim mm/memcontrol.c +2529

2513 /*
2514  * Enqueue the creation of a per-memcg kmem_cache.
2515  * 将一个针对每个内存控制组的内核内存高速缓存的创建任务加入队列。
2516  */
2517 static void __memcg_schedule_kmem_cache_create(struct mem_cgroup *memcg,
2518                            struct kmem_cache *cachep)
2519 {
2520     struct memcg_kmem_cache_create_work *cw; /* 声明一个内存控制组 kmem_cache 创建工作项指针 */
2521 
2522     cw = kmalloc(sizeof(*cw), GFP_NOWAIT | __GFP_NOWARN); /* 分配内存控制组 kmem_cache 创建工作项 */
2523     if (!cw) /* 如果分配失败 */
2524         return; /* 直接返回 */
2525 
2526     css_get(&memcg->css); /* 增加内存控制组的引用计数 */
2527 
2528     cw->memcg = memcg; /* 设置内存控制组指针 */
2529     cw->cachep = cachep; /* 设置内核内存高速缓存指针 */
2530     INIT_WORK(&cw->work, memcg_kmem_cache_create_func); /* 初始化工作项 */
2531 
2532     queue_work(memcg_kmem_cache_wq, &cw->work); /* 将工作项加入工作队列 */
2533 }
```

![__memcg_schedule_kmem_cache_create](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240409164020.png)

###### memcg_schedule_kmem_cache_create

```c
// vim mm/memcontrol.c +2534
2534 static void memcg_schedule_kmem_cache_create(struct mem_cgroup *memcg,
2535                          struct kmem_cache *cachep)
2536 {
2537     /*
2538      * We need to stop accounting when we kmalloc, because if the
2539      * corresponding kmalloc cache is not yet created, the first allocation
2540      * in __memcg_schedule_kmem_cache_create will recurse.
2541      * 我们需要在进行 kmalloc 时停止计费，因为如果对应的 kmalloc 缓存尚未创建，
2542      * 那么 __memcg_schedule_kmem_cache_create 中的第一个分配会递归调用。
2543      *
2544      * However, it is better to enclose the whole function. Depending on
2545      * the debugging options enabled, INIT_WORK(), for instance, can
2546      * trigger an allocation. This too, will make us recurse. Because at
2547      * this point we can't allow ourselves back into memcg_kmem_get_cache,
2548      * the safest choice is to do it like this, wrapping the whole function.
2549      * 但是，最好将整个函数封装起来。取决于启用的调试选项，例如 INIT_WORK() 可能
2550      * 会触发一个分配。这也会导致我们递归调用。因为此时我们不能再允许自己回到
2551      * memcg_kmem_get_cache 中，所以最安全的选择是这样做，将整个函数封装起来。
2552      */
2553     current->memcg_kmem_skip_account = 1; /* 设置当前进程 memcg_kmem_skip_account 为 1 */
2554     __memcg_schedule_kmem_cache_create(memcg, cachep); /* 调用内部函数以创建 kmem_cache */
2555     current->memcg_kmem_skip_account = 0; /* 恢复当前进程 memcg_kmem_skip_account 为 0 */
2556 }
```

###### memcg_kmem_get_cache

```c
// vim mm/memcontrol.c +2621

2560 /**
2561  * memcg_kmem_get_cache: 选择正确的每个内存控制组（memcg）缓存进行分配
2562  * @cachep: 原始的全局 kmem 缓存
2563  *
2564  * 返回我们应该用于 slab 分配的 kmem_cache。
2565  * 我们尝试使用当前 memcg 的缓存版本。
2566  *
2567  * 如果缓存尚不存在，如果我们是其第一个用户，我们会在工作队列中异步创建它，并让当前分配通过原始缓存。
2568  *
2569  * 此函数对返回的缓存进行引用以确保在使用过程中不会被销毁。一旦调用者完成使用，必须调用 memcg_kmem_put_cache() 来释放引用。
2570  */
2571 struct kmem_cache *memcg_kmem_get_cache(struct kmem_cache *cachep)
2572 {
2573     struct mem_cgroup *memcg; /* 内存控制组 */
2574     struct kmem_cache *memcg_cachep; /* 内存控制组缓存 */
2575     int kmemcg_id; /* 内存控制组 ID */
2576 
2577     VM_BUG_ON(!is_root_cache(cachep)); /* 确保传入的缓存是根缓存 */
2578 
2579     if (memcg_kmem_bypass()) /* 如果绕过 memcg_kmem */
2580         return cachep; /* 直接返回原始缓存 */
2581 
2582     if (current->memcg_kmem_skip_account) /* 如果当前线程不用进行记账 */
2583         return cachep; /* 直接返回原始缓存 */
2584 
2585     memcg = get_mem_cgroup_from_current(); /* 从当前线程获取内存控制组 */
2586     kmemcg_id = READ_ONCE(memcg->kmemcg_id); /* 读取内存控制组的 ID */
2587     if (kmemcg_id < 0) /* 如果内存控制组 ID 小于 0 */
2588         goto out; /* 跳转到 out 标签 */
2589 
2590     memcg_cachep = cache_from_memcg_idx(cachep, kmemcg_id); /* 根据内存控制组 ID 获取缓存 */
2591     if (likely(memcg_cachep)) /* 如果成功获取到内存控制组缓存 */
2592         return memcg_cachep; /* 返回内存控制组缓存 */
2593 
2594     /*
2595      * 如果我们在安全的上下文中（可以等待，且不在中断上下文中），我们可以预测并立即返回。
2596      * 这将确保正在执行的分配已经属于新缓存。
2597      *
2598      * 但是，有些锁可能会引发冲突。
2599      * 例如，因为我们在执行 memcg_create_kmem_cache 时会获取 slab_mutex，这意味着在持有 slab_mutex 时不能进行进一步的分配。
2600      * 因此，最好推迟一切。
2601      */
2602     memcg_schedule_kmem_cache_create(memcg, cachep); /* 调度创建内存控制组缓存 */
2603 out:
2604     css_put(&memcg->css); /* 释放内存控制组的引用 */
2605     return cachep; /* 返回原始缓存 */
2606 }
```

用bpf追踪一下哪些进程会调用memcg_kmem_get_cache：

```bash
trace-bpfcc -t memcg_kmem_get_cache | tee memcg_kmem_get_cache1.log
```

输出结果如下：

```bash
TIME     PID     TID     COMM            FUNC             
1.620114 935     935     redis-server    memcg_kmem_get_cache 
1.620177 935     935     redis-server    memcg_kmem_get_cache 
1.620193 935     935     redis-server    memcg_kmem_get_cache 
1.620216 935     935     redis-server    memcg_kmem_get_cache 
1.720512 935     935     redis-server    memcg_kmem_get_cache 
1.720576 935     935     redis-server    memcg_kmem_get_cache 
1.720591 935     935     redis-server    memcg_kmem_get_cache 
1.720620 935     935     redis-server    memcg_kmem_get_cache 
1.787920 895     954     QDBusConnection memcg_kmem_get_cache 
1.787975 895     954     QDBusConnection memcg_kmem_get_cache 
1.788464 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788496 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788512 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788527 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788538 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788678 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788694 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788705 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788715 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788737 2975    2975    deepin-system-m memcg_kmem_get_cache 
1.788440 895     963     QThread         memcg_kmem_get_cache 
1.789300 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.789335 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.789422 895     965     QThread         memcg_kmem_get_cache 
1.789467 895     965     QThread         memcg_kmem_get_cache 
1.789586 895     965     QThread         memcg_kmem_get_cache 
1.789426 1205    1205    Xorg            memcg_kmem_get_cache 
1.789465 1205    1205    Xorg            memcg_kmem_get_cache 
1.789734 895     965     QThread         memcg_kmem_get_cache 
1.797259 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.797301 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.797425 1205    1205    Xorg            memcg_kmem_get_cache 
1.797450 1205    1205    Xorg            memcg_kmem_get_cache 
1.797629 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.797657 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.797735 1205    1205    Xorg            memcg_kmem_get_cache 
1.797751 1205    1205    Xorg            memcg_kmem_get_cache 
1.798109 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.798136 4405    4405    deepin-terminal memcg_kmem_get_cache 
1.803705 895     965     QThread         memcg_kmem_get_cache 
1.803754 895     965     QThread         memcg_kmem_get_cache 
1.803770 895     965     QThread         memcg_kmem_get_cache 
```

貌似每个进程都会调用memcg_kmem_get_cache函数，也就意味着每次分配都会在内存控制组缓存中创建一个新的缓存。

###### slab_pre_alloc_hook

```c
// vim mm/slab.h +429

414 static inline struct kmem_cache *slab_pre_alloc_hook(struct kmem_cache *s,
415                              gfp_t flags)
416 {
417     flags &= gfp_allowed_mask; /* 仅保留与允许的标志位相匹配的标志位 */
418 
419     fs_reclaim_acquire(flags); /* 获取文件系统回收锁 */
420     fs_reclaim_release(flags); /* 释放文件系统回收锁 */
421 
422     might_sleep_if(gfpflags_allow_blocking(flags)); /* 如果允许阻塞，则可能会休眠 */
423 
424     if (should_failslab(s, flags)) /* 如果应该失败分配 slab */
425         return NULL; /* 返回空指针 */
426 
427     if (memcg_kmem_enabled() && /* 如果启用了 memcg_kmem */
428         ((flags & __GFP_ACCOUNT) || (s->flags & SLAB_ACCOUNT))) /* 如果分配需要记账 */
429         return memcg_kmem_get_cache(s); /* 返回 memcg_kmem 获取的缓存 */
430 
431     return s; /* 返回原始的 kmem_cache */
432 } 

```

此处好像有个递归？？？

```c
slab_pre_alloc_hook               // mm/slab.h +414
  memcg_kmem_get_cache            // mm/slab.h +429
    kmem_cache_alloc              // mm/slub.c +2719
      slab_alloc_node             // mm/slub.c +2714
        slab_pre_alloc_hook       // mm/slub.c +2632
```

这个递归解释了memcg_schedule_kmem_cache_create函数注释中提到的可能存在递归的原因。

##### 修复方案

```c
From dfd4043beff95e15bad4207cc9db9b29940e20d4 Mon Sep 17 00:00:00 2001
From: yuanqiliang <yuanqiliang@uniontech.com>
Date: Tue, 16 Apr 2024 11:12:58 +0800
Subject: [PATCH] mm/memcg: very high memory usage due to kernfs_node_cache slabs

Bug: https://pms.uniontech.com/bug-view-238303.html

Log:
use command "slabtop -s c -o", kernfs_node_cache is ranked first
and continues to grow. SUnreclaim is also continuously increasing.

The current memory management code has significant differences from
upstream. This issue does not exist in upstream linux-4.19.y. Although
this commit fixes the problem, it is still recommended to sync with the
upstream code.

Check kernfs_node_cache allocation in /var/log/kern.log:

all trace:
dump_backtrace+0x0/0x190
show_stack+0x14/0x20
dump_stack+0xa8/0xcc
alloc_debug_processing+0x58/0x188
___slab_alloc.constprop.34+0x31c/0x388
kmem_cache_alloc+0x210/0x278
__kernfs_new_node+0x60/0x1f8
kernfs_new_node+0x24/0x48
kernfs_create_dir_ns+0x30/0x88
cgroup_mkdir+0x2f0/0x4e8
kernfs_iop_mkdir+0x58/0x88
vfs_mkdir+0xfc/0x1c0
do_mkdirat+0xec/0x100
__arm64_sys_mkdirat+0x1c/0x28
el0_svc_common+0x90/0x178
el0_svc_handler+0x9c/0xa8
el0_svc+0x8/0xc

To trace kernfs_new_node using BPF:

trace-bpfcc -tKU kernfs_new_node
3.796820 47      47      kworker/3:1     kernfs_new_node
kernfs_new_node+0x0 [kernel]
sysfs_add_file_mode_ns+0x9c [kernel]
internal_create_group+0x104 [kernel]
sysfs_create_group+0x14 [kernel]  # 上游4.19.306这里没走到
sysfs_slab_add+0xb8 [kernel]      # 上游4.19.306这里走到了
__kmem_cache_create+0x128 [kernel]
create_cache+0xcc [kernel]
memcg_create_kmem_cache+0xf8 [kernel]
memcg_kmem_cache_create_func+0x1c [kernel]
process_one_work+0x1e8 [kernel]
worker_thread+0x48 [kernel]
kthread+0x128 [kernel]
ret_from_fork+0x10 [kernel]
-14

Signed-off-by: yuanqiliang <yuanqiliang@uniontech.com>
Change-Id: Ia13f72f6a0b8e9944fd8dafb6f68c7170081a591
---

diff --git a/mm/slab_common.c b/mm/slab_common.c
index 1519956..93343e5 100644
--- a/mm/slab_common.c
+++ b/mm/slab_common.c
@@ -147,6 +147,7 @@
 
 	if (root_cache) {
 		s->memcg_params.root_cache = root_cache;
+		s->memcg_params.root_cache->memcg_kset = NULL;
 		s->memcg_params.memcg = memcg;
 		INIT_LIST_HEAD(&s->memcg_params.children_node);
 		INIT_LIST_HEAD(&s->memcg_params.kmem_caches_node);
```

linux-4.19.y上`*(s->memcg_params.root_cache->memcg_kset) = 0`， uos-arm-kernel-1053-tyy上`*(s->memcg_params.root_cache->memcg_kset)`是个随机值。

kmem_cache通过双向链表管理，kmem_cache父节点的memcg_kset会传导给子节点，故第一个非根节点的kset需要初始化为null。

本次问题虽然修复了，但是感觉uos-arm-kernel-1053-tyy上存在一个CVE。

![修复后跟上游对比](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_17132357917855.png)

#### systemd

```bash
TIME     PID     TID     COMM            FUNC             
3.781989 1       1       systemd         kernfs_new_node  
        kernfs_new_node+0x0 [kernel]
        cgroup_mkdir+0x2f0 [kernel]
        kernfs_iop_mkdir+0x58 [kernel]
        vfs_mkdir+0xfc [kernel]
        do_mkdirat+0xec [kernel]
        __arm64_sys_mkdirat+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        mkdir+0x14 [libc-2.28.so]
        [unknown] [libsystemd-shared-241.so (deleted)]
        [unknown] [libsystemd-shared-241.so (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [libsystemd-shared-241.so (deleted)]
        [unknown] [libsystemd-shared-241.so (deleted)]
        [unknown] [libsystemd-shared-241.so (deleted)]
        [unknown] [systemd (deleted)]
        [unknown] [systemd (deleted)]
        __libc_start_main+0xe4 [libc-2.28.so]
        [unknown] [systemd (deleted)]
```

```bash
bpftrace -l | grep cgroup_mkdir
tracepoint:cgroup:cgroup_mkdir
```

```bash
trace-bpfcc -tKU cgroup_mkdir | tee cgroup_mkdir.log
```

vim cgroup_mkdir.log:

```bash
TIME     PID     TID     COMM            FUNC             
11.08041 1       1       systemd         cgroup_mkdir     
        cgroup_mkdir+0x0 [kernel]
        vfs_mkdir+0xfc [kernel]
        do_mkdirat+0xec [kernel]
        __arm64_sys_mkdirat+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        [unknown] [libc-2.28.so (deleted)]
        cg_create.localalias.13+0x64 [libsystemd-shared-241.so]
        cg_create_everywhere+0x30 [libsystemd-shared-241.so]
        unit_create_cgroup+0xb4 [systemd]
        unit_realize_cgroup_now.lto_priv.665+0xa8 [systemd]
        unit_realize_cgroup+0x16c [systemd]
        unit_prepare_exec+0x18 [systemd]
        service_spawn.lto_priv.382+0x80 [systemd]
        service_start.lto_priv.56+0x144 [systemd]
        job_perform_on_unit.lto_priv.422+0x6b4 [systemd]
        manager_dispatch_run_queue.lto_priv.589+0x358 [systemd]
        source_dispatch+0x118 [libsystemd-shared-241.so]
        sd_event_dispatch+0x150 [libsystemd-shared-241.so]
        sd_event_run+0x90 [libsystemd-shared-241.so]
        invoke_main_loop+0xff4 [systemd]
        main+0x1660 [systemd]
        [unknown] [libc-2.28.so (deleted)]
        [unknown] [systemd]

11.08056 1       1       systemd         cgroup_mkdir     
        cgroup_mkdir+0x0 [kernel]
        vfs_mkdir+0xfc [kernel]
        do_mkdirat+0xec [kernel]
        __arm64_sys_mkdirat+0x1c [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        [unknown] [libc-2.28.so (deleted)]
        cg_create.localalias.13+0x64 [libsystemd-shared-241.so]
        cg_create.localalias.13+0xe8 [libsystemd-shared-241.so]
        cg_create_everywhere+0x30 [libsystemd-shared-241.so]
        unit_create_cgroup+0xb4 [systemd]
        unit_realize_cgroup_now.lto_priv.665+0xa8 [systemd]
        unit_realize_cgroup+0x16c [systemd]
        unit_prepare_exec+0x18 [systemd]
        service_spawn.lto_priv.382+0x80 [systemd]
        service_start.lto_priv.56+0x144 [systemd]
        job_perform_on_unit.lto_priv.422+0x6b4 [systemd]
        manager_dispatch_run_queue.lto_priv.589+0x358 [systemd]
        source_dispatch+0x118 [libsystemd-shared-241.so]
        sd_event_dispatch+0x150 [libsystemd-shared-241.so]
        sd_event_run+0x90 [libsystemd-shared-241.so]
        invoke_main_loop+0xff4 [systemd]
        main+0x1660 [systemd]
        [unknown] [libc-2.28.so (deleted)]
        [unknown] [systemd]
```

```c
1626 static int unit_create_cgroup( // vim src/core/cgroup.c +1626
1627                 Unit *u,
1628                 CGroupMask target_mask,
1629                 CGroupMask enable_mask,
1630                 ManagerState state) {
1631 
1632         bool created;
1633         int r;
1634 
1635         assert(u);
1636 
1637         if (!UNIT_HAS_CGROUP_CONTEXT(u))
1638                 return 0;
1639 
1640         /* Figure out our cgroup path */
1641         r = unit_pick_cgroup_path(u);
1642         if (r < 0)
1643                 return r;
1644 
1645         /* First, create our own group */
1646         r = cg_create_everywhere(u->manager->cgroup_supported, target_mask, u->cgroup_path);
1647         if (r < 0)
1648                 return log_unit_error_errno(u, r, "Failed to create cgroup %s: %m", u->cgroup_path);
1649         created = r;
```

### systemd log

```bash
grep -i failed syslog | grep systemd | wc -l
146225
```

```bash
grep -i failed syslog | grep systemd | grep .service | grep -v "Failed to start" > systemd.log
```

#### deepin-anything-monitor.service

```bash
grep deepin-anything-monitor.service systemd.log | wc -l
835
```

```bash
systemctl status deepin-anything-monitor.service
● deepin-anything-monitor.service - Deepin anything service
   Loaded: loaded (/lib/systemd/system/deepin-anything-monitor.service; enabled; vendor preset: enabled)
   Active: activating (auto-restart) (Result: exit-code) since Thu 2024-01-25 10:24:27 CST; 2s ago
  Process: 19672 ExecStartPre=/usr/sbin/modprobe vfs_monitor (code=exited, status=1/FAILURE)
  Process: 19674 ExecStopPost=/usr/sbin/rmmod vfs_monitor (code=exited, status=1/FAILURE)
```

#### logrotate.service

```bash
grep logrotate.service systemd.log | wc -l
568
```

```bash
2024-01-01 00:00:12 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 01:00:01 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 02:00:01 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 03:00:21 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
2024-01-01 04:00:31 0000000g-A8cUta6pu5 systemd[1]:  logrotate.service: Failed with result 'exit-code'.
```

```bash
systemctl status logrotate.service
● logrotate.service - Rotate log files
   Loaded: loaded (/lib/systemd/system/logrotate.service; static; vendor preset: enabled)
   Active: failed (Result: exit-code) since Thu 2024-01-25 09:00:05 CST; 53min ago
     Docs: man:logrotate(8)
           man:logrotate.conf(5)
  Process: 7067 ExecStart=/usr/sbin/logrotate /etc/logrotate.conf (code=exited, status=1/FAILURE)
 Main PID: 7067 (code=exited, status=1/FAILURE)

1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: clink-agent:8, unexpected text after }
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/apt/term.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set "su
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/apt/history.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set 
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/mirror/printer/printer.log" because parent directory has insecure permissions (It's world writable or writable by group which is not "
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/cups/access_log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set 
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/cups/error_log" because parent directory has insecure permissions (It's world writable or writable by group which is not "root") Set "
1月 25 09:00:04 0000000g-A8cUta6pu5 logrotate[7067]: error: skipping "/var/log/mirror/ctyunInstall/ctyunInstall.log" because parent directory has insecure permissions (It's world writable or writable by group whic
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: logrotate.service: Main process exited, code=exited, status=1/FAILURE
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: logrotate.service: Failed with result 'exit-code'.
1月 25 09:00:05 0000000g-A8cUta6pu5 systemd[1]: Failed to start Rotate log files.
```

#### avahi-daemon.service

```bash
uptime 
 10:36:02 up 18:29,  1 user,  load average: 0.18, 0.27, 0.25
```

```bash
grep dbus-org.freedesktop.Avahi.service systemd.log | wc -l
143146
```

```bash
2024-01-16 05:45:03 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:08 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:13 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
2024-01-16 05:45:18 0000000g-A8cUta6pu5 dbus-daemon[539]:  [system] Activation via systemd failed for unit 'dbus-org.freedesktop.Avahi.service': Unit dbus-org.freedesktop.Avahi.service not found.
```

5S重启一次，本次系统启动也就18个半钟头，跑了143146次。

```bash
systemctl cat avahi-daemon.service
# /lib/systemd/system/avahi-daemon.service
# This file is part of avahi.
#
# avahi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# avahi is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with avahi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

[Unit]
Description=Avahi mDNS/DNS-SD Stack
Requires=avahi-daemon.socket

[Service]
Type=dbus
BusName=org.freedesktop.Avahi
ExecStart=/usr/sbin/avahi-daemon -s
ExecReload=/usr/sbin/avahi-daemon -r
NotifyAccess=main

[Install]
WantedBy=multi-user.target
Also=avahi-daemon.socket
Alias=dbus-org.freedesktop.Avahi.service
```

```bash
apt show avahi-daemon
Package: avahi-daemon
Version: 0.7.6-1+dde
Priority: optional
Section: net
Source: avahi
Maintainer: Utopia Maintenance Team <pkg-utopia-maintainers@lists.alioth.debian.org>
Installed-Size: 263 kB
Depends: libavahi-common3 (>= 0.6.16), libavahi-core7 (>= 0.6.24), libc6 (>= 2.27), libcap2 (>= 1:2.10), libdaemon0 (>= 0.14), libdbus-1-3 (>= 1.9.14), libexpat1 (>= 2.0.1), adduser, dbus (>= 0.60), lsb-base (>= 3.0-6), bind9-host | host
Recommends: libnss-mdns (>= 0.11)
Suggests: avahi-autoipd
Homepage: http://avahi.org/
Download-Size: 89.4 kB
APT-Manual-Installed: no
APT-Sources: https://professional-packages.chinauos.com/desktop-professional eagle/main arm64 Packages
Description: Avahi mDNS/DNS-SD daemon
 Avahi is a fully LGPL framework for Multicast DNS Service Discovery.
 It allows programs to publish and discover services and hosts
 running on a local network with no specific configuration. For
 example you can plug into a network and instantly find printers to
 print to, files to look at and people to talk to.
 .
 This package contains the Avahi Daemon which represents your machine
 on the network and allows other applications to publish and resolve
 mDNS/DNS-SD records.
```

### 解决方案

```bash
sudo apt reinstall avahi-daemon
sudo systemctl daemon-reload
sudo systemctl restart avahi-daemon
```

## More

- [very high memory usage due to kernfs_node_cache slabs #1927](https://github.com/coreos/bugs/issues/1927)
  - [socket activation: dentry slab cache keeps increasing #6567](https://github.com/systemd/systemd/issues/6567)
- [cgroup 内存泄露问题排查记录](https://www.jianshu.com/p/cae1525ffe5a)
  - [Bug 1507149 - [LLNL 7.5 Bug] slab leak causing a crash when using kmem control group](https://bugzilla.redhat.com/show_bug.cgi?id=1507149)
- <https://lore.kernel.org/linux-mm/CA+CK2bCQcnTpzq2wGFa3D50PtKwBoWbDBm56S9y8c+j+pD+KSw@mail.gmail.com/t/>
- [ [Solved] dbus-org.freedesktop.Avahi.service missing from distribution?](https://bbs.archlinux.org/viewtopic.php?id=261924)

### patch

- [一个疑似slab泄漏问题排查](https://blog.csdn.net/kaka__55/article/details/124063773)
- [[PATCH v3 01/19] mm: memcg: factor out memcg- and lruvec-level changes out of __mod_lruvec_state() Roman Gushchin](https://lore.kernel.org/lkml/20200422204708.2176080-1-guro@fb.com/t/)
- [The new cgroup slab memory controller](https://lwn.net/Articles/824216/)
- [[PATCH v7 12/19] mm: memcg/slab: use a single set of kmem_caches for all accounted allocations](https://www.mail-archive.com/linux-kernel@vger.kernel.org/msg2206757.html)
