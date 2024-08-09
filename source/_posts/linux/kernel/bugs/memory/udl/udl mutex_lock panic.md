---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# udl mutex_lock panic

bug: <https://pms.uniontech.com/bug-view-229783.html>

## panic 日志

```c
<1>[ 1264.589996s][pid:18926,cpu6,lshw-bak,5]Unable to handle kernel paging request at virtual address 0000080100000058
<1>[ 1264.590026s][pid:18926,cpu6,lshw-bak,6]Mem abort info:
<1>[ 1264.590026s][pid:18926,cpu6,lshw-bak,7]  ESR = 0x96000004
<1>[ 1264.590026s][pid:18926,cpu6,lshw-bak,8]  Exception class = DABT (current EL), IL = 32 bits
<1>[ 1264.590026s][pid:18926,cpu6,lshw-bak,9]  SET = 0, FnV = 0
<1>[ 1264.590057s][pid:18926,cpu6,lshw-bak,0]  EA = 0, S1PTW = 0
<1>[ 1264.590057s][pid:18926,cpu6,lshw-bak,1]Data abort info:
<1>[ 1264.590057s][pid:18926,cpu6,lshw-bak,2]  ISV = 0, ISS = 0x00000004
<1>[ 1264.590057s][pid:18926,cpu6,lshw-bak,3]  CM = 0, WnR = 0
<1>[ 1264.590087s][pid:18926,cpu6,lshw-bak,4][0000080100000058] address between user and kernel address ranges
<1>[ 1264.590087s][pid:18926,cpu6,lshw-bak,5]printk_level_setup: 7
<0>[  123.175201s][pid:6550,cpu6,lshw-bak,9]Internal error: Oops: 96000004 [#1] PREEMPT SMP
<4>[  123.175231s][pid:6550,cpu6,lshw-bak,0]Modules linked in: vfs_monitor(O) udl wifi_1105 plat_1105 uos_resources(O) uos_bluetooth_connection_control(O) lightorange lightnut lightfig lightcherry filearmor(O) deepin_netmonitor(O)
<0>[  123.175231s][pid:6550,cpu6,lshw-bak,1]Process lshw-bak (pid: 6550, stack limit = 0x0000000094118db9)
<4>[  123.175262s][pid:6550,cpu6,lshw-bak,2]CPU: 6 PID: 6550 Comm: lshw-bak Tainted: G    B      O      4.19.71-arm64-desktop #6200.57055.5.test
<4>[  123.175262s][pid:6550,cpu6,lshw-bak,3]Hardware name: HUAWEI HUAWEI L410 KLVU-WDU0/SP1KVUM, BIOS 1.00.53 03/24/2021
<4>[  123.175262s][pid:6550,cpu6,lshw-bak,4]pstate: 40400005 (nZcv daif +PAN -UAO)
<4>[  123.175262s][pid:6550,cpu6,lshw-bak,5]pc : __mutex_lock.isra.1+0x608/0x700
<4>[  123.175292s][pid:6550,cpu6,lshw-bak,6]lr : __mutex_lock.isra.1+0x608/0x700
<4>[  123.175292s][pid:6550,cpu6,lshw-bak,7]sp : ffffffc376a57690
<4>[  123.175292s][pid:6550,cpu6,lshw-bak,8]x29: ffffffc376a57690 x28: ffffff9027dfeac8 
<4>[  123.175323s][pid:6550,cpu6,lshw-bak,9]x27: ffffffc349115630 x26: ffffffc349115528 
<4>[  123.175323s][pid:6550,cpu6,lshw-bak,0]x25: ffffffc3023a6010 x24: 0000000000000002 
<4>[  123.175323s][pid:6550,cpu6,lshw-bak,1]x23: 0000000000000001 x22: ffffff902b2cb688 
<4>[  123.175323s][pid:6550,cpu6,lshw-bak,2]x21: 0008070000000000 x20: 1ffffff86ed4aee0 
<4>[  123.175354s][pid:6550,cpu6,lshw-bak,3]x19: ffffffc37eee5110 x18: 0000000000000000 
<4>[  123.175354s][pid:6550,cpu6,lshw-bak,4]x17: 0000000000000000 x16: 0000000000000000 
<4>[  123.175354s][pid:6550,cpu6,lshw-bak,5]x15: 00000000000007e6 x14: 00000000000001fe 
<4>[  123.175354s][pid:6550,cpu6,lshw-bak,6]x13: 0000000000000039 x12: ffffff886ed4ae5c 
<4>[  123.175384s][pid:6550,cpu6,lshw-bak,7]x11: 1ffffff86ed4ae5b x10: ffffff886ed4ae5b 
<4>[  123.175384s][pid:6550,cpu6,lshw-bak,8]x9 : 0000000000000000 x8 : ffffffc376a572e0 
<4>[  123.175384s][pid:6550,cpu6,lshw-bak,9]x7 : 0000000000000000 x6 : 0000000041b58ab3 
<4>[  123.175384s][pid:6550,cpu6,lshw-bak,0]x5 : ffffff886ed4aee0 x4 : dfffff9000000000 
<4>[  123.175415s][pid:6550,cpu6,lshw-bak,1]x3 : ffffff902945cd40 x2 : 0000000000000000 
<4>[  123.175415s][pid:6550,cpu6,lshw-bak,2]x1 : e76abf5519f47e00 x0 : 0000000000000000 
<4>[  123.175415s][pid:6550,cpu6,lshw-bak,3]
<4>[  123.175415s]SP: 0xffffffc376a57610:
<4>[  123.175415s][pid:6550,cpu6,lshw-bak,4]7610  00000002 00000000 023a6010 ffffffc3 49115528 ffffffc3 49115630 ffffffc3
<4>[  123.175445s][pid:6550,cpu6,lshw-bak,5]7630  27dfeac8 ffffff90 76a57690 ffffffc3 2945cd40 ffffff90 76a57690 ffffffc3
<4>[  123.175445s][pid:6550,cpu6,lshw-bak,6]7650  2945cd40 ffffff90 40400005 00000000 00000001 00000000 00000002 00000000
<4>[  123.175476s][pid:6550,cpu6,lshw-bak,7]7670  ffffffff 0000007f 19f47e00 e76abf55 76a57690 ffffffc3 2945cd40 ffffff90
<4>[  123.175476s][pid:6550,cpu6,lshw-bak,8]7690  76a57780 ffffffc3 2945ce5c ffffff90 7eee5110 ffffffc3 79255fd0 ffffffc3
<4>[  123.175506s][pid:6550,cpu6,lshw-bak,9]76b0  49115500 ffffffc3 7eee5100 ffffffc3 2993cea0 ffffff90 7eee5110 ffffffc3
<4>[  123.175506s][pid:6550,cpu6,lshw-bak,0]76d0  49115500 ffffffc3 49115528 ffffffc3 00000100 00000000 000000e8 00000000
<4>[  123.175537s][pid:6550,cpu6,lshw-bak,1]76f0  41b58ab3 00000000 2a150ef8 ffffff90 41b58ab3 00000000 2a15e5a0 ffffff90
<4>[  123.175537s][pid:6550,cpu6,lshw-bak,2]
<4>[  123.175537s]X8: 0xffffffc376a57260:
<4>[  123.175537s][pid:6550,cpu6,lshw-bak,3]7260  00000000 00000000 00000000 00000000 2948afc0 ffffff90 76a57650 ffffffc3
<4>[  123.175567s][pid:6550,cpu6,lshw-bak,4]7280  49115630 ffffffc3 023a6000 ffffffc3 76a57330 ffffffc3 76a57330 00000006
<4>[  123.175567s][pid:6550,cpu6,lshw-bak,5]72a0  29488600 ffffff90 00000000 00000000 41b58ab3 00000000 2a150ee8 ffffff90
<4>[  123.175598s][pid:6550,cpu6,lshw-bak,6]72c0  27a8a488 ffffff90 00000000 00000000 41003858 00000000 2a164498 ffffff90
<4>[  123.175598s][pid:6550,cpu6,lshw-bak,7]72e0  27c2d0bc ffffff90 19f47e00 e76abf55 00294f28 ffffffc3 00000010 00000000
<4>[  123.175628s][pid:6550,cpu6,lshw-bak,8]7300  023a67c8 ffffffc3 19f47e00 e76abf55 76a57330 ffffffc3 27a944a8 ffffff90
<4>[  123.175628s][pid:6550,cpu6,lshw-bak,9]7320  2b2eda60 ffffff90 00000000 00000000 76a57390 ffffffc3 27aa9b40 ffffff90
<4>[  123.175659s][pid:6550,cpu6,lshw-bak,0]7340  2949b480 ffffff90 96000004 00000000 00000058 00080700 76a57550 ffffffc3
<4>[  123.175659s][pid:6550,cpu6,lshw-bak,1]
<4>[  123.175659s]X19: 0xffffffc37eee5090:
<4>[  123.175659s][pid:6550,cpu6,lshw-bak,2]5090  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.175689s][pid:6550,cpu6,lshw-bak,3]50b0  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.175689s][pid:6550,cpu6,lshw-bak,4]50d0  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.175720s][pid:6550,cpu6,lshw-bak,5]50f0  00000000 00000000 00000000 00000000 bbe070fa 9fa26359 00000000 00000000
<4>[  123.175720s][pid:6550,cpu6,lshw-bak,6]5110  00000007 00080700 00027003 ffffffff 00000000 ffffffff 05a48f48 00000000
<4>[  123.175750s][pid:6550,cpu6,lshw-bak,7]5130  00000000 00000000 32267500 ffffffc3 7eee5140 ffffffc3 7eee5140 ffffffc3
<4>[  123.175750s][pid:6550,cpu6,lshw-bak,8]5150  7eee5150 ffffffc3 7eee5150 ffffffc3 7eee5160 ffffffc3 00000000 00000000
<4>[  123.175781s][pid:6550,cpu6,lshw-bak,9]5170  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.175781s][pid:6550,cpu6,lshw-bak,0]
<4>[  123.175781s]X25: 0xffffffc3023a5f90:
<4>[  123.175781s][pid:6550,cpu6,lshw-bak,1]5f90  aedbc4b8 0000007f aed620f8 0000007f aee0d180 0000007f a8c59e70 0000007f
<4>[  123.175811s][pid:6550,cpu6,lshw-bak,2]5fb0  aedab940 0000007f 00014c30 00000000 00000000 00000000 aed76490 0000007f
<4>[  123.175811s][pid:6550,cpu6,lshw-bak,3]5fd0  a8c589a0 0000007f aee108e0 0000007f 00000000 00000000 afa43780 0000007f
<4>[  123.175842s][pid:6550,cpu6,lshw-bak,4]5ff0  aee108a0 0000007f 00000000 00000000 00000220 00000000 ffffffff ffffffff
<4>[  123.175842s][pid:6550,cpu6,lshw-bak,5]6010  00000003 00000000 00000000 00000000 76a50000 ffffffc3 00000002 00404100
<4>[  123.175872s][pid:6550,cpu6,lshw-bak,6]6030  00000000 00000000 023a6038 ffffffc3 023a6038 ffffffc3 00000000 00000000
<4>[  123.175872s][pid:6550,cpu6,lshw-bak,7]6050  00000000 00000000 00000001 00000006 000000bd 00000000 ffff41ac 00000000
<4>[  123.175903s][pid:6550,cpu6,lshw-bak,8]6070  2dc12000 ffffffc3 00000000 00000000 00000000 00000007 00000006 00000001
<4>[  123.175903s][pid:6550,cpu6,lshw-bak,9]
<4>[  123.175903s]X26: 0xffffffc3491154a8:
<4>[  123.175903s][pid:6550,cpu6,lshw-bak,0]54a8  491154a0 ffffffc3 6cddbb70 ffffffc3 00000000 00000000 0000182d 84000104linux-image-4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep-dbg_4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep-6200.57055.6.test_arm64.deb
<4>[  123.175933s][pid:6550,cpu6,lshw-bak,1]54c8  00000030 db400074 491154d0 ffffffc3 491154d0 ffffffc3 00000000 00000000
<4>[  123.175933s][pid:6550,cpu6,lshw-bak,2]54e8  00000000 00000000 491154f0 ffffffc3 491154f0 ffffffc3 00000000 00000000
<4>[  123.175964s][pid:6550,cpu6,lshw-bak,3]5508  00000000 00000000 950caa20 ffffffc3 c0f654c8 ffffffc2 79255fd0 ffffffc3
<4>[  123.175964s][pid:6550,cpu6,lshw-bak,4]5528  2993d320 ffffff90 00000000 dead4ead ffffffff 00000000 ffffffff ffffffff
<4>[  123.175994s][pid:6550,cpu6,lshw-bak,5]5548  00000000 00000000 00000001 00000000 00020000 0000001d 00000000 00000000
<4>[  123.175994s][pid:6550,cpu6,lshw-bak,6]5568  00000000 dead4ead ffffffff 00000000 ffffffff ffffffff 00000000 00000000
<4>[  123.175994s][pid:6550,cpu6,lshw-bak,7]5588  49115588 ffffffc3 49115588 ffffffc3 00000000 00000000 00000000 00000000
<4>[  123.176025s][pid:6550,cpu6,lshw-bak,8]
<4>[  123.176025s]X27: 0xffffffc3491155b0:
<4>[  123.176025s][pid:6550,cpu6,lshw-bak,9]55b0  ffffffff ffffffff 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.176055s][pid:6550,cpu6,lshw-bak,0]55d0  25315300 ffffffc3 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.176055s][pid:6550,cpu6,lshw-bak,1]55f0  00000000 00000000 00000000 00000000 302d8f00 ffffffc2 00000000 00000000
<4>[  123.176086s][pid:6550,cpu6,lshw-bak,2]5610  49115610 ffffffc3 49115610 ffffffc3 49115620 ffffffc3 49115620 ffffffc3
<4>[  123.176086s][pid:6550,cpu6,lshw-bak,3]5630  79256178 ffffffc3 00000000 00000000 00001996 84000104 00000030 db400074
<4>[  123.176086s][pid:6550,cpu6,lshw-bak,4]5650  49115648 ffffffc3 8d8aec00 ffffffc3 40000000 00000000 40000000 00000000
<4>[  123.176116s][pid:6550,cpu6,lshw-bak,5]5670  00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
<4>[  123.176116s][pid:6550,cpu6,lshw-bak,6]5690  954e2420 ffffffc3 93fac578 ffffffc3 e6b2a6a0 ffffffc2 29530320 ffffff90
<4>[  123.176147s][pid:6550,cpu6,lshw-bak,7]
<4>[  123.176147s]X29: 0xffffffc376a57610:
<4>[  123.176147s][pid:6550,cpu6,lshw-bak,8]7610  00000002 00000000 023a6010 ffffffc3 49115528 ffffffc3 49115630 ffffffc3
<4>[  123.176177s][pid:6550,cpu6,lshw-bak,9]7630  27dfeac8 ffffff90 76a57690 ffffffc3 2945cd40 ffffff90 76a57690 ffffffc3
<4>[  123.176177s][pid:6550,cpu6,lshw-bak,0]7650  2945cd40 ffffff90 40400005 00000000 00000001 00000000 00000002 00000000
<4>[  123.176177s][pid:6550,cpu6,lshw-bak,1]7670  ffffffff 0000007f 19f47e00 e76abf55 76a57690 ffffffc3 2945cd40 ffffff90
<4>[  123.176208s][pid:6550,cpu6,lshw-bak,2]7690  76a57780 ffffffc3 2945ce5c ffffff90 7eee5110 ffffffc3 79255fd0 ffffffc3
<4>[  123.176208s][pid:6550,cpu6,lshw-bak,3]76b0  49115500 ffffffc3 7eee5100 ffffffc3 2993cea0 ffffff90 7eee5110 ffffffc3
<4>[  123.176239s][pid:6550,cpu6,lshw-bak,4]76d0  49115500 ffffffc3 49115528 ffffffc3 00000100 00000000 000000e8 00000000
<4>[  123.176239s][pid:6550,cpu6,lshw-bak,5]76f0  41b58ab3 00000000 2a150ef8 ffffff90 41b58ab3 00000000 2a15e5a0 ffffff90
<4>[  123.176269s][pid:6550,cpu6,lshw-bak,6]
<4>[  123.176269s][pid:6550,cpu6,lshw-bak,7]Call trace:
<4>[  123.176269s][pid:6550,cpu6,lshw-bak,8] __mutex_lock.isra.1+0x608/0x700
<4>[  123.176269s][pid:6550,cpu6,lshw-bak,9] __mutex_lock_slowpath+0x24/0x30
<4>[  123.176300s][pid:6550,cpu6,lshw-bak,0] mutex_lock+0x50/0x58
<4>[  123.176300s][pid:6550,cpu6,lshw-bak,1] fb_open+0x104/0x248
<4>[  123.176300s][pid:6550,cpu6,lshw-bak,2] chrdev_open+0x144/0x290
<4>[  123.176300s][pid:6550,cpu6,lshw-bak,3] do_dentry_open+0x1dc/0x5d8
<4>[  123.176330s][pid:6550,cpu6,lshw-bak,4] vfs_open+0x58/0x68
<4>[  123.176330s][pid:6550,cpu6,lshw-bak,5] path_openat+0x530/0x1bc8
<4>[  123.176330s][pid:6550,cpu6,lshw-bak,6] do_filp_open+0xf0/0x180
<4>[  123.176330s][pid:6550,cpu6,lshw-bak,7] do_sys_open+0x208/0x378
<4>[  123.176361s][pid:6550,cpu6,lshw-bak,8] __arm64_sys_openat+0x5c/0x70
<4>[  123.176361s][pid:6550,cpu6,lshw-bak,9] el0_svc_common+0xd4/0x1d8
<4>[  123.176361s][pid:6550,cpu6,lshw-bak,0] el0_svc_handler+0x9c/0xc0
<4>[  123.176361s][pid:6550,cpu6,lshw-bak,1] el0_svc+0x8/0xc
<0>[  123.176391s][pid:6550,cpu6,lshw-bak,2]Code: 97fff9dc 17ffffe9 910162a0 97a58b81 (b9405ab5) 
<4>[  123.176391s][pid:6550,cpu6,lshw-bak,3]---[ end trace 3c5fedb524b1a208 ]---
<0>[  123.176391s][pid:6550,cpu6,lshw-bak,4]Kernel panic - not syncing: Fatal exception
```

### 初步定位报错代码

```c
./scripts/faddr2line vmlinux __mutex_lock.isra.1+0x608/0x700
__mutex_lock.isra.1+0x608/0x700:
mutex_can_spin_on_owner 于 kernel/locking/mutex.c:578
(已内连入)mutex_optimistic_spin 于 kernel/locking/mutex.c:622
(已内连入)__mutex_lock_common 于 kernel/locking/mutex.c:928
(已内连入)__mutex_lock 于 kernel/locking/mutex.c:1072
```

```c
kernel/locking/mutex.c
 562 static inline int mutex_can_spin_on_owner(struct mutex *lock)
 563 {
 564         struct task_struct *owner;
 565         int retval = 1;
 566
 567         if (need_resched())
 568                 return 0;
 569
 570         rcu_read_lock();
 571         owner = __mutex_owner(lock);   // 代码真正出错在这一行
 572
 573         /*
 574          * As lock holder preemption issue, we both skip spinning if task is not
 575          * on cpu or its cpu is preempted
 576          */
 577         if (owner)
 578                 retval = owner->on_cpu && !vcpu_is_preempted(task_cpu(owner));
 579         rcu_read_unlock();
 580
 581         /*
 582          * If lock->owner is not set, the mutex has been released. Return true
 583          * such that we'll trylock in the spin path, which is a faster option
 584          * than the blocking slow path.
 585          */
 586         return retval;
 587 }
```

```c
gdb vmlinux
disassemble /m __mutex_lock
562     static inline int mutex_can_spin_on_owner(struct mutex *lock)
563     {
564             struct task_struct *owner;
565             int retval = 1;
566                                                                                                                                                                                           
567             if (need_resched())
568                     return 0;
569
570             rcu_read_lock();
571             owner = __mutex_owner(lock);
572
573             /*
574              * As lock holder preemption issue, we both skip spinning if task is not
575              * on cpu or its cpu is preempted
576              */
577             if (owner)
   0xffffff9009a58680 <+1048>:  ands    x21, x0, #0xfffffffffffffff8
   0xffffff9009a58684 <+1052>:  b.ne    0xffffff9009a58868 <__mutex_lock+1536>  // b.any
578                     retval = owner->on_cpu && !vcpu_is_preempted(task_cpu(owner));
   0xffffff9009a58868 <+1536>:  add     x0, x21, #0x58
   0xffffff9009a5886c <+1540>:  bl      0xffffff90083bf828 <__asan_load4>
   0xffffff9009a58870 <+1544>:  ldr     w21, [x21, #88]
```

```c
add x0, x21, #0x58: 将 x0 寄存器的值设置为 x21 寄存器的值加上 0x58。这通常是在计算结构体或对象的字段的地址。

bl 0xffffff90083bf828 <__asan_load4>: 这是一个分支跳转指令，调用地址为 0xffffff90083bf828 处的函数（可能是内存访问相关的函数）。bl 意味着它是一个分支链接指令，会将返回地址保存在链接寄存器中。

ldr w21, [x21, #88]: 从地址 x21 + 88 处加载一个32位的值到 w21 寄存器。这似乎是加载一个结构体或对象的字段，因为它在 add 指令中计算了一个偏移量。

整个代码段的目的是计算某个结构体（通过 x21 寄存器指向的地址）中的某个字段（通过偏移量 88）的值，并在此基础上执行一些比较和逻辑操作，将结果存储在 retval 变量中。这可能是在检查某个虚拟处理单元（vCPU）是否正在被调度，具体实现可能涉及到处理调度和预emption的相关逻辑。
```

```c
p ((struct task_struct *)0)->on_cpu
Cannot access memory at address 0x58
```

```c
(gdb) p &((struct task_struct *)0)->on_cpu
$1 = (int *) 0x58
```

```c
include/linux/sched.h
711 struct task_struct {
 712 #ifdef CONFIG_THREAD_INFO_IN_TASK
 713         /*
 714          * For reasons of header soup (see current_thread_info()), this
 715          * must be the first element of task_struct.
 716          */
 717         struct thread_info              thread_info;
 718 #endif
 719         /* -1 unrunnable, 0 runnable, >0 stopped: */
 720         volatile long                   state;
 721
 722         /*
 723          * This begins the randomizable portion of task_struct. Only
 724          * scheduling-critical items should be added above here.
 725          */
 726         randomized_struct_fields_start
 727
 728         void                            *stack;
 729         atomic_t                        usage;
 730         /* Per task flags (PF_*), defined further below: */
 731         unsigned int                    flags;
 732         unsigned int                    ptrace;
 733
 734 #ifdef CONFIG_HUAWEI_SCHED_VIP
 735         unsigned int vip_prio;
 736         struct list_head hisi_vip_entry;
 737         u64 hisi_vip_last_queued;
 738 #endif
 739 #ifdef CONFIG_SMP
 740         struct llist_node               wake_entry;
 741         int                             on_cpu;
 742 #ifdef CONFIG_THREAD_INFO_IN_TASK
 743         /* Current CPU: */
 744         unsigned int                    cpu;
 745 #endif
 746         unsigned int                    wakee_flips;
 747         unsigned long                   wakee_flip_decay_ts;
 748         struct task_struct              *last_wakee;
 749
 750         struct task_struct      *waker;
 751         unsigned int            first_awakend;
 752         /*
```

```c
include/linux/mutex.h
73 static inline struct task_struct *__mutex_owner(struct mutex *lock)
 74 {
 75         return (struct task_struct *)(atomic_long_read(&lock->owner) & ~0x07);
 76 }
```

```c
假设 &lock->owner 是一个 64 位地址，那么 &lock->owner 的十六进制表示形式为 0008070000000000。

对 &lock->owner 进行与 ~0x07 的按位与操作，即 0008070000000000 & ~0x07，结果为 0008070000000000 & FFFFFFFFFFFFFFF8，最终值仍然是 0008070000000000。

所以，&lock->owner 等于 0008070000000000。
```

```c
(gdb) p ((struct mutex *)0)->owner
Cannot access memory at address 0x0
```

进一步表明lock地址为0008070000000000。

```c
(gdb) p ((struct fb_info *)0)->lock
Cannot access memory at address 0x10
```

fb_info指向的虚拟地址为：0x0008070000000000 - 0x10 = 0x000806FFFFFFFFF0，0x000806FFFFFFFFF0位于虚拟地址内核空间与用户空间之间，说明此地址大概率被谁释放了。

```c
cd /home/uos/lshw-02.18.85.5
gdb /usr/bin/lshw-bak
set substitute-path /build/lshw-02.18.85.5/src /home/uos/lshw-02.18.85.5/src
(gdb) i b
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x0000000000404580 in main(int, char**) at lshw.cc:80
        breakpoint already hit 1 time
2       breakpoint     keep y   0x0000000000459ec8 in scan_fb(hwNode&) at fb.cc:230
        breakpoint already hit 1 time
3       breakpoint     keep y   0x0000000000459f08 in mknod at fb.cc:207
        breakpoint already hit 1 time
```

当前已用slub_debug、kasan排查内存错误问题，没发现问题。

华为klua-kernel开启lockdep检测死锁，也没观察到死锁问题。

接下来进一步分析方向只能转向udl驱动模块代码执行流程。

## 1060

此问题在1060上不存在。

```bash
root@uos-PC:/home/uos# uname -a
Linux uos-PC 4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1 SMP PREEMPT Fri Nov 24 09:13:36 CST 2023 aarch64 GNU/Linux
root@uos-PC:/home/uos# 
root@uos-PC:/home/uos# 
root@uos-PC:/home/uos# cat /etc/os-release 
PRETTY_NAME="UOS Desktop 20 Pro"
NAME="uos"
VERSION_ID="20"
VERSION="20"
ID=uos
HOME_URL="https://www.chinauos.com/"
BUG_REPORT_URL="http://bbs.chinauos.com"
VERSION_CODENAME=eagle
root@uos-PC:/home/uos# cat /etc/os-version 
[Version] 
SystemName=UOS Desktop
SystemName[zh_CN]=统信桌面操作系统
ProductType=Desktop
ProductType[zh_CN]=桌面
EditionName=Professional
EditionName[zh_CN]=专业版
MajorVersion=20
MinorVersion=1060
OsBuild=11014.102.100
root@uos-PC:/home/uos# cat /etc/pro
product-info       profile            profile.d/         protocols          proxychains4.conf  
root@uos-PC:/home/uos# cat /etc/pro
product-info       profile            profile.d/         protocols          proxychains4.conf  
root@uos-PC:/home/uos# cat /etc/product-info 
20231123.build54124
```

### 拔掉udl设备

#### usb_disconnect

```c
[三 11月 29 16:29:24 2023] [2023:11:29 16:29:03][pid:5,cpu0,kworker/0:0,4]usb 1-1.1: USB disconnect, device number 4
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,5]usb 1-1.1.1: USB disconnect, device number 5
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,6]shy ==> udl_usb_disconnect
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,7]shy ==> file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_unplug, line: 503, dev->dev->init_name: 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8]CPU: 0 PID: 5 Comm: kworker/0:0 Tainted: G    B             4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9]Hardware name: HUAWEI L410 KLVU-WDU0/SP1KVUM, BIOS 1.00.73 07/15/2023
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0]Workqueue: usb_hub_wq hub_event
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,1]Call trace:
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,2] dump_backtrace+0x0/0x2e0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,3] show_stack+0x24/0x30
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,4] dump_stack+0xcc/0x10c
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,5] unbind_console+0x9c/0x210
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,6] unlink_framebuffer+0x30/0x40
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,7] drm_fb_helper_unlink_fbi+0x30/0x40
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8] udl_fbdev_unplug+0x60/0x6c [udl]
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9] udl_usb_disconnect+0x44/0x60 [udl]
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0] usb_unbind_interface+0xcc/0x378
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,1] device_release_driver_internal+0x258/0x338
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,2] device_release_driver+0x28/0x38
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,3] bus_remove_device+0x17c/0x248
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,4] device_del+0x240/0x560
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,5] usb_disable_device+0x114/0x338
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,6] usb_disconnect+0x15c/0x420
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,7] usb_disconnect+0x124/0x420
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8] hub_event+0x9ac/0x1bd0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9] process_one_work+0x4fc/0xc80
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0] worker_thread+0x80/0x700
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,1] kthread+0x208/0x218
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,2] ret_from_fork+0x10/0x1c
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,3]file: drivers/video/fbdev/core/fbmem.c, line: 1774, fun: unbind_console, fb->info: ffffffc1b957a880, &fb_info->lock:ffffffc1b957a890, &fb_info->lock.owner :ffffffc1b957a890, i: 1, FB_MAX: 32, registered_fb[i]: ffffffc1b957a880
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,4]file: drivers/video/fbdev/core/fbmem.c, line: 1790, fun: unbind_console, fb->info: ffffffc1b957a880, &fb_info->lock:ffffffc1b957a890, &fb_info->lock.owner: ffffffc1b957a890, i: 1, ret: 1
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,3][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,4][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,5][USB3][xhci_notifier_fn]-
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,6]usb 1-1.1.2: USB disconnect, device number 6
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,7]usb 1-1.1.2.1: USB disconnect, device number 8
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0][USB3][xhci_notifier_fn]-
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8][HISI_DRM I]:hisi_dp_connector_detect: [DP] hisi dp connector detect -:2
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,1][USB3][xhci_notifier_fn]-
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,2]usb 1-1.1.5: USB disconnect, device number 7
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0][HISI_DRM I]:mipi2edp_backlight_update: +
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,1][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,2][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,3][USB3][xhci_notifier_fn]-
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,4]usb 1-1.1.7: USB disconnect, device number 9
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,5][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,6][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,7][USB3][xhci_notifier_fn]-
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,8][usb_hub]: USB_BUS_ADD busnum = 0
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,9][USB3][xhci_notifier_fn]+
[三 11月 29 16:29:24 2023] [pid:5,cpu0,kworker/0:0,0][USB3][xhci_notifier_fn]-
```

## 1070

1070上插拔udl设备内核panic。

```bash
root@uos-PC:/home/uos# uname -a
Linux uos-PC 4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1 SMP PREEMPT Fri Nov 24 09:13:36 CST 2023 aarch64 GNU/Linux
root@uos-PC:/home/uos# cat /etc/os-release 
PRETTY_NAME="UOS Desktop 20 Pro"
NAME="uos"
VERSION_ID="20"
VERSION="20"
ID=uos
HOME_URL="https://www.chinauos.com/"
BUG_REPORT_URL="http://bbs.chinauos.com"
VERSION_CODENAME=eagle
root@uos-PC:/home/uos# cat /etc/product-info 
20231118.build54124
```

```bash
dmesg -C && (dmesg -w -T | tee udl_1070_5.log)
```

### 插入udl设备

#### losf查看设备

```bash
uos@uos-PC:/sys/class/graphics$ lsof /dev/dri/card0
lsof: WARNING: can't stat() vfat file system /boot/efi
      Output information may be incomplete.
COMMAND    PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
kwin_wayl 3848  uos   19u   CHR  226,0      0t0 2539 /dev/dri/card0
Xwayland  3987  uos    6u   CHR  226,0      0t0 2539 /dev/dri/card0
dde-deskt 4055  uos   45u   CHR  226,0      0t0 2539 /dev/dri/card0
dde-dock  4125  uos   33u   CHR  226,0      0t0 2539 /dev/dri/card0
dde-lock  4800  uos   31u   CHR  226,0      0t0 2539 /dev/dri/card0
dde-file- 5442  uos   35u   CHR  226,0      0t0 2539 /dev/dri/card0

uos@uos-PC:/sys/class/graphics$ lsof /dev/dri/card1
lsof: WARNING: can't stat() vfat file system /boot/efi
      Output information may be incomplete.
COMMAND    PID USER   FD   TYPE DEVICE SIZE/OFF   NODE NAME
kwin_wayl 3848  uos  mem    CHR  226,1          132758 /dev/dri/card1
kwin_wayl 3848  uos   99u   CHR  226,1      0t0 132758 /dev/dri/card1
Xwayland  3987  uos   52u   CHR  226,1      0t0 132758 /dev/dri/card1

uos@uos-PC:/sys/class/graphics$ lsof /dev/fb1
lsof: WARNING: can't stat() vfat file system /boot/efi
      Output information may be incomplete.
```

#### do_register_framebuffer

插入udl设备时，/dev/fb1注册流程如下：

```c
44:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,7]usb 1-1.1.1: new high-speed USB device number 5 using xhci-hcd
53:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,6]usb 1-1.1.1: New USB device found, idVendor=17e9, idProduct=03c1, bcdDevice= 4.41
54:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,7]usb 1-1.1.1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
55:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,8]usb 1-1.1.1: Product: USB LCD
56:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,9]usb 1-1.1.1: Manufacturer: DisplayLink
57:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,0]usb 1-1.1.1: SerialNumber: C20220115
58:[三 11月 29 19:09:31 2023] [pid:494,cpu7,kworker/7:2,1][drm] vendor descriptor length:1b data:1b 5f 01 00 19 05 00 01 03 00 04
59:[三 11月 29 19:09:32 2023] [2023:11:29 19:09:14][pid:494,cpu7,kworker/7:2,2]CPU: 7 PID: 494 Comm: kworker/7:2 Tainted: G    B             4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1
60:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,3]Hardware name: HUAWEI HUAWEI L410 KLVU-WDU0/SP1KVUM, BIOS 1.00.53 03/24/2021
61:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,4]Workqueue: usb_hub_wq hub_event
62:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5]Call trace:
63:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6] dump_backtrace+0x0/0x2e0
64:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7] show_stack+0x24/0x30
65:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8] dump_stack+0xcc/0x10c
66:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9] register_framebuffer+0xac/0x494      // drivers/video/fbdev/core/fbmem.c:1889 1994
67:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,0] __drm_fb_helper_initial_config_and_unlock+0x3bc/0x6c0        // drivers/gpu/drm/drm_fb_helper.c:2652 2687
68:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,1] drm_fb_helper_initial_config+0x50/0x60       // drivers/gpu/drm/drm_fb_helper.c:2757 2754
69:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,2] udl_fbdev_init+0xcc/0xe8 [udl]       // drivers/gpu/drm/udl/udl_fb.c:452 478
70:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,3] udl_init+0x500/0x710 [udl]   // drivers/gpu/drm/udl/udl_main.c:313 341




udl_driver_create drivers/gpu/drm/udl/udl_drv.c:83 102




71:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,4] udl_usb_probe+0x94/0x150 [udl]       // drivers/gpu/drm/udl/udl_drv.c:113 119->udl_driver_create




72:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5] usb_probe_interface+0x178/0x3e0
73:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6] really_probe+0x280/0x520
74:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7] driver_probe_device+0x88/0x1a8
75:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8] __device_attach_driver+0x10c/0x170
76:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9] bus_for_each_drv+0xf8/0x158
77:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,0] __device_attach+0x15c/0x1e8
78:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,1] device_initial_probe+0x24/0x30
79:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,2] bus_probe_device+0xf0/0x100
80:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,3] device_add+0x538/0x958
81:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,4] usb_set_configuration+0x730/0xca0
82:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5] generic_probe+0x70/0xa0
83:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6] usb_probe_device+0x68/0xb0
84:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7] really_probe+0x280/0x520
85:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8] driver_probe_device+0x88/0x1a8
86:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9] __device_attach_driver+0x10c/0x170
87:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,0] bus_for_each_drv+0xf8/0x158
88:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,1] __device_attach+0x15c/0x1e8
89:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,2] device_initial_probe+0x24/0x30
90:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,3] bus_probe_device+0xf0/0x100
91:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,4] device_add+0x538/0x958
92:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5] usb_new_device+0x424/0x998
93:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6] hub_event+0xd08/0x1bd0
94:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7] process_one_work+0x4fc/0xc80
95:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8] worker_thread+0x80/0x700
96:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9] kthread+0x208/0x218
97:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,0] ret_from_fork+0x10/0x1c
98:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,1]file: drivers/video/fbdev/core/fbmem.c, line: 1704, fun: do_register_framebuffer, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner:ffffffd0efecda90.
99:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,2]udl 1-1.1.1:1.0: fb1: udldrmfb frame buffer device
100:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,3][drm] Initialized udl 0.0.1 20120220 for 1-1.1.1:1.0 on minor 1
101:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,4][drm] Initialized udl on minor 1
102:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5][usb_hub]: USB_BUS_ADD busnum = 0
103:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6][USB3][xhci_notifier_fn]+
104:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7][USB3][xhci_notifier_fn]-
112:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,5][HISI_DRM I]:hisi_dp_connector_detect: [DP] hisi dp connector detect -:2
113:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,6]usb 1-1.1.2: new high-speed USB device number 6 using xhci-hcd
114:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,7]usb 1-1.1.2: New USB device found, idVendor=1a40, idProduct=0101, bcdDevice= 1.11
115:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8]usb 1-1.1.2: New USB device strings: Mfr=0, Product=1, SerialNumber=0
116:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9]usb 1-1.1.2: Product: USB 2.0 Hub
125:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,8]hub 1-1.1.2:1.0: USB hub found
126:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,9]hub 1-1.1.2:1.0: 4 ports detected
127:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,0][usb_hub]: USB_BUS_ADD busnum = 0
128:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,1][USB3][xhci_notifier_fn]+
129:[三 11月 29 19:09:32 2023] [pid:494,cpu7,kworker/7:2,2][USB3][xhci_notifier_fn]usb hub don't notify
130:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,3]usb 1-1.1.5: new full-speed USB device number 7 using xhci-hcd
131:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,4]usb 1-1.1.5: New USB device found, idVendor=05af, idProduct=0060, bcdDevice= 0.00
132:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,5]usb 1-1.1.5: New USB device strings: Mfr=5, Product=6, SerialNumber=0
133:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,6]usb 1-1.1.5: Product: HandWrite Series
134:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,7]usb 1-1.1.5: Manufacturer: Sunrex
135:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,8]hid-generic 0003:05AF:0060.0003: hidraw2: USB HID v1.11 Device [Sunrex HandWrite Series] on usb-xhci-hcd.0.auto-1.1.5/input0
136:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,9]input: Sunrex HandWrite Series Mouse as /devices/platform/hisi_usb@f8480000/f8400000.dwc3/xhci-hcd.0.auto/usb1/1-1/1-1.1/1-1.1.5/1-1.1.5:1.1/0003:05AF:0060.0004/input/input13
137:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,0]hid-generic 0003:05AF:0060.0004: input,hidraw3: USB HID v1.11 Mouse [Sunrex HandWrite Series] on usb-xhci-hcd.0.auto-1.1.5/input1
138:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,1][usb_hub]: USB_BUS_ADD busnum = 0
139:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,2][USB3][xhci_notifier_fn]+
140:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,3][USB3][xhci_notifier_fn]-
147:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,0]usb 1-1.1.7: new full-speed USB device number 9 using xhci-hcd
176:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,9]usb 1-1.1.7: New USB device found, idVendor=058f, idProduct=9540, bcdDevice= 1.20
177:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,0]usb 1-1.1.7: New USB device strings: Mfr=1, Product=2, SerialNumber=0
178:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,1]usb 1-1.1.7: Product: EMV Smartcard Reader
179:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,2]usb 1-1.1.7: Manufacturer: Generic
180:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,3][usb_hub]: USB_BUS_ADD busnum = 0
181:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,4][USB3][xhci_notifier_fn]+
182:[三 11月 29 19:09:33 2023] [pid:494,cpu7,kworker/7:2,5][USB3][xhci_notifier_fn]-
```

### 拔掉udl设备

#### usb_disconnect

```c
27:[三 11月 29 19:09:31 2023] [2023:11:29 19:09:13][pid:5,cpu0,kworker/0:0,0]usb 1-1.1: new high-speed USB device number 4 using xhci-hcd
36:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,9]usb 1-1.1: New USB device found, idVendor=1a40, idProduct=0201, bcdDevice= 1.00
37:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,0]usb 1-1.1: New USB device strings: Mfr=0, Product=1, SerialNumber=0
38:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,1]usb 1-1.1: Product: USB 2.0 Hub [MTT]
39:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,2]hub 1-1.1:1.0: USB hub found
40:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,3]hub 1-1.1:1.0: 7 ports detected
41:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,4][usb_hub]: USB_BUS_ADD busnum = 0
42:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,5][USB3][xhci_notifier_fn]+
43:[三 11月 29 19:09:31 2023] [pid:5,cpu0,kworker/0:0,6][USB3][xhci_notifier_fn]usb hub don't notify
1880:[三 11月 29 19:09:55 2023] [2023:11:29 19:09:37][pid:5,cpu0,kworker/0:0,3]usb 1-1.1: USB disconnect, device number 4
1881:[三 11月 29 19:09:55 2023] [pid:5,cpu0,kworker/0:0,4]usb 1-1.1.1: USB disconnect, device number 5
1882:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,5]shy ==> udl_usb_disconnect
1883:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,6]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_unplug, line: 507, dev->dev->init_name: (null)
1884:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,7]CPU: 0 PID: 5 Comm: kworker/0:0 Tainted: G    B             4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1
1885:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,8]Hardware name: HUAWEI HUAWEI L410 KLVU-WDU0/SP1KVUM, BIOS 1.00.53 03/24/2021
1886:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,9]Workqueue: usb_hub_wq hub_event
1887:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,0]Call trace:
1888:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1] dump_backtrace+0x0/0x2e0
1889:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2] show_stack+0x24/0x30
1890:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3] dump_stack+0xcc/0x10c
1891:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,4] unbind_console+0x9c/0x210
1892:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,5] unlink_framebuffer+0x30/0x40         // drivers/video/fbdev/core/fbmem.c:1853 1857 1861
1893:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,6] drm_fb_helper_unlink_fbi+0x30/0x40   // drivers/gpu/drm/drm_fb_helper.c:1024 1027
1894:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,7] udl_fbdev_unplug+0x60/0x6c [udl]     // drivers/gpu/drm/udl/udl_fb.c:506 515
1895:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,8] udl_usb_disconnect+0x44/0x60 [udl]   // drivers/gpu/drm/udl/udl_drv.c:138 144
1896:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,9] usb_unbind_interface+0xcc/0x378
1897:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,0] device_release_driver_internal+0x258/0x338
1898:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1] device_release_driver+0x28/0x38
1899:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2] bus_remove_device+0x17c/0x248
1900:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3] device_del+0x240/0x560
1901:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,4] usb_disable_device+0x114/0x338
1902:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,5] usb_disconnect+0x15c/0x420
1903:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,6] usb_disconnect+0x124/0x420
1904:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,7] hub_event+0x9ac/0x1bd0
1905:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,8] process_one_work+0x4fc/0xc80
1906:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,9] worker_thread+0x80/0x700
1907:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,0] kthread+0x208/0x218
1908:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1] ret_from_fork+0x10/0x1c



1909:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2]file: drivers/video/fbdev/core/fbmem.c, line: 1774, fun: unbind_console, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner :ffffffd0efecda90, i: 1, FB_MAX: 32, registered_fb[i]: ffffffd0efecda80

1910:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3]file: drivers/video/fbdev/core/fbmem.c, line: 1790, fun: unbind_console, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner: ffffffd0efecda90, i: 1, ret: 1


./scripts/faddr2line vmlinux unlink_framebuffer+0x30/0x40
unlink_framebuffer+0x30/0x40:
unlink_framebuffer 于 drivers/video/fbdev/core/fbmem.c:1863

1852 int unlink_framebuffer(struct fb_info *fb_info)
1853 {
1854         int ret;
1855 
1856         ret = __unlink_framebuffer(fb_info);
1857         if (ret)
1858                 return ret;
1859 
1860         unbind_console(fb_info);
1861 
1862         return 0;
1863 }
1864 EXPORT_SYMBOL(unlink_framebuffer);




1919:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2][usb_hub]: USB_BUS_ADD busnum = 0
1920:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3][USB3][xhci_notifier_fn]+
1921:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,4][USB3][xhci_notifier_fn]-
1922:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,5]usb 1-1.1.2: USB disconnect, device number 6
1923:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,6]usb 1-1.1.2.1: USB disconnect, device number 8
1924:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,7][usb_hub]: USB_BUS_ADD busnum = 0
1925:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,8][USB3][xhci_notifier_fn]+
1926:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,9][USB3][xhci_notifier_fn]-
1927:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,0][usb_hub]: USB_BUS_ADD busnum = 0
1928:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1][USB3][xhci_notifier_fn]+
1929:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2][USB3][xhci_notifier_fn]-
1930:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3]usb 1-1.1.5: USB disconnect, device number 7
1938:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1][HISI_DRM I]:hisi_dp_connector_detect: [DP] hisi dp connector detect -:2
1939:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,2][usb_hub]: USB_BUS_ADD busnum = 0
1940:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,3][USB3][xhci_notifier_fn]+
1941:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,4][USB3][xhci_notifier_fn]-
1942:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,5]usb 1-1.1.7: USB disconnect, device number 9
1943:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,6][usb_hub]: USB_BUS_ADD busnum = 0
1944:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,7][USB3][xhci_notifier_fn]+
1945:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,8][USB3][xhci_notifier_fn]-
1946:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,9][usb_hub]: USB_BUS_ADD busnum = 0
1947:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,0][USB3][xhci_notifier_fn]+
1948:[三 11月 29 19:09:56 2023] [pid:5,cpu0,kworker/0:0,1][USB3][xhci_notifier_fn]-
```

##### unbind_console

fb_notifier_call_chain这里return 1, 导致后续流程终止。

```c
1767 static int unbind_console(struct fb_info *fb_info)
1768 {
1769         dump_stack();
1770         struct fb_event event;
1771         int ret;
1772         int i = fb_info->node;
1773 
1774         printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner :%px, i: %d, FB_MAX: %d, registered_fb[i]: %px\n", __FILE__, __LINE__, __FUNCTION__,      fb_info, &fb_info->lock, &fb_info->lock.owner, i, FB_MAX, registered_fb[i]);
1775 
1776         if (i < 0 || i >= FB_MAX || registered_fb[i] != fb_info) {
1777                 printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner :%px, i: %d\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &f     b_info->lock.owner, i);
1778                 return -EINVAL;
1779         }
1780 
1781         console_lock();
1782         if (!lock_fb_info(fb_info)) {
1783                 printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner :%px, i: %d\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &f     b_info->lock.owner, i);
1784                 console_unlock();
1785                 return -ENODEV;
1786         }
1787 
1788         event.info = fb_info;
1789         ret = fb_notifier_call_chain(FB_EVENT_FB_UNBIND, &event);  // 拔掉udl设备的时候 ret = 1
1790         printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner: %px, i: %d, ret: %d\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &     fb_info->lock.owner, i, ret);
1791         unlock_fb_info(fb_info);
1792         console_unlock();
1793 
1794         return ret;
1795 }
```

##### do_unregister_framebuffer

```c
1799 static int do_unregister_framebuffer(struct fb_info *fb_info)
1800 {
1801         dump_stack();
1802         // printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px.", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &fb_info->lock.owner);
1803         printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px, fb_info->fbops: %px, fb_info->fbops->fb_destroy:%px\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &fb     _info->lock.owner, fb_info->fbops, fb_info->fbops->fb_destroy);
1804         struct fb_event event;
1805         int ret;
1806 
1807         ret = unbind_console(fb_info);
1808 
1809         if (ret) {
1810                 printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px, fb_info->fbops: %px, fb_info->fbops->fb_destroy:%px, ret: %d\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &f     b_info->lock, &fb_info->lock.owner, fb_info->fbops, fb_info->fbops->fb_destroy, ret);
1811                 return -EINVAL;    // 这里就结束了
1812         }
1813 
1814         pm_vt_switch_unregister(fb_info->dev);
1815 
1816         __unlink_framebuffer(fb_info);
1817         printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px, fb_info->fbops: %px, fb_info->fbops->fb_destroy:%px\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &fb     _info->lock.owner, fb_info->fbops, fb_info->fbops->fb_destroy);
1818         if (fb_info->pixmap.addr &&
1819             (fb_info->pixmap.flags & FB_PIXMAP_DEFAULT))
1820                 kfree(fb_info->pixmap.addr);
1821         fb_destroy_modelist(&fb_info->modelist);
1822         registered_fb[fb_info->node] = NULL;
1823         num_registered_fb--;
1824         fb_cleanup_device(fb_info);
1825         printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px, fb_info->fbops: %px, fb_info->fbops->fb_destroy:%px\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &fb     _info->lock.owner, fb_info->fbops, fb_info->fbops->fb_destroy);
1826         event.info = fb_info;
1827         console_lock();
1828         fb_notifier_call_chain(FB_EVENT_FB_UNREGISTERED, &event);
1829         console_unlock();
1830 
1831         /* this may free fb info */
1832         put_fb_info(fb_info);
1833         return 0;
1834 }
```

#### drm_release

```c
drm_release // drivers/gpu/drm/drm_file.c:466 487
└──drm_minor_release // drivers/gpu/drm/drm_drv.c: 259 261 698 701 660 665
    └──udl_driver_release // drivers/gpu/drm/udl/udl_drv.c:50 52
        └──udl_fini // drivers/gpu/drm/udl/udl_main.c:366 375
                └──udl_fbdev_cleanup // drivers/gpu/drm/udl/udl_fb.c:501
                        ├────udl_fbdev_destroy // drivers/gpu/drm/udl/udl_fb.c:438
                               ├────drm_fb_helper_unregister_fbi // drivers/gpu/drm/drm_fb_helper.c:959
                               │        └──unregister_framebuffer // drivers/video/fbdev/core/fbmem.c:1923
                               │                ├──do_unregister_framebuffer // drivers/video/fbdev/core/fbmem.c: 1803 1807
                               │                │        └──unbind_console // drivers/video/fbdev/core/fbmem.c:1774 1790
                               │                └──do_unregister_framebuffer // drivers/video/fbdev/core/fbmem.c:1810         // 1811 这里就结束了
                               │
                               ├────udl_fbdev_destroy // drivers/gpu/drm/udl/udl_fb.c:439
                               │        └──drm_fb_helper_fini // drivers/gpu/drm/drm_fb_helper.c:970
                               ├────udl_fbdev_destroy // drivers/gpu/drm/udl/udl_fb.c:442 444 446

```

```c
1958:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,1]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_cleanup, line: 494, dev->dev->init_name: (null)
1959:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,2]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_destroy, line: 437, dev->dev->init_name: (null), &ufbdev->helper: ffffffd0fa999280, ufbdev->ufb.obj: ffffffd0fa1b6c80, &ufbdev->ufb.base: ffffffd0fa9994a0

//  udl_fbdev_cleanup
        udl_fbdev_destroy
                drm_fb_helper_unregister_fbi
                        unregister_framebuffer
                                do_unregister_framebuffer
1960:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,3]file: drivers/video/fbdev/core/fbmem.c, line: 1803, fun: do_unregister_framebuffer, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner:ffffffd0efecda90, fb_info->fbops: ffffff90025f3160, fb_info->fbops->fb_destroy:0000000000000000


1961:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,4]CPU: 6 PID: 2867 Comm: Xwayland Tainted: G    B             4.19.71-arm64-desktop-udl-slub-debug-kasan-lockdep #1
1962:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,5]Hardware name: HUAWEI HUAWEI L410 KLVU-WDU0/SP1KVUM, BIOS 1.00.53 03/24/2021


1963:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,6]Call trace:
1964:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,7] dump_backtrace+0x0/0x2e0
1965:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,8] show_stack+0x24/0x30
1966:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,9] dump_stack+0xcc/0x10c
1967:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,0] unbind_console+0x9c/0x210
1968:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,1] do_unregister_framebuffer+0xe8/0x2a4
1969:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,2] unregister_framebuffer+0x34/0x50
1970:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,3] drm_fb_helper_unregister_fbi+0x30/0x40
1971:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,4] udl_fbdev_cleanup+0xdc/0x278 [udl]
1972:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,5] udl_fini+0x38/0x60 [udl]
1973:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,6] udl_driver_release+0x20/0x48 [udl]
1974:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,7] drm_dev_put.part.0+0x58/0x78
1975:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,8] drm_minor_release+0x2c/0x38
1976:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,9] drm_release+0x114/0x140
1977:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,0] __fput+0xf4/0x2c8
1978:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,1] ____fput+0x20/0x30
1979:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,2] task_work_run+0xcc/0xf0
1980:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,3] do_notify_resume+0x17c/0x180
1981:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,4] work_pending+0x8/0x14



1982:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,5]file: drivers/video/fbdev/core/fbmem.c, line: 1774, fun: unbind_console, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner :ffffffd0efecda90, i: 1, FB_MAX: 32, registered_fb[i]: ffffffd0efecda80
1983:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,6]file: drivers/video/fbdev/core/fbmem.c, line: 1790, fun: unbind_console, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner: ffffffd0efecda90, i: 1, ret: 1
1984:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,7]file: drivers/video/fbdev/core/fbmem.c, line: 1810, fun: do_unregister_framebuffer, fb->info: ffffffd0efecda80, &fb_info->lock:ffffffd0efecda90, &fb_info->lock.owner:ffffffd0efecda90, fb_info->fbops: ffffff90025f3160, fb_info->fbops->fb_destroy:0000000000000000, ret: 1


1985:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,8]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_destroy, line: 441, dev->dev->init_name: (null), &ufbdev->helper: ffffffd0fa999280, ufbdev->ufb.obj: ffffffd0fa1b6c80, &ufbdev->ufb.base: ffffffd0fa9994a0
1986:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,9]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_destroy, line: 443, dev->dev->init_name: (null), &ufbdev->helper: ffffffd0fa999280, ufbdev->ufb.obj: ffffffd0fa1b6c80, &ufbdev->ufb.base: ffffffd0fa9994a0
1987:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,0]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_destroy, line: 445, dev->dev->init_name: (null), &ufbdev->helper: ffffffd0fa999280, ufbdev->ufb.obj: ffffffd0fa1b6c80, &ufbdev->ufb.base: ffffffd0fa9994a0, &ufbdev->ufb.obj->base: ffffffd0fa1b6c80
1988:[三 11月 29 19:09:57 2023] [pid:2867,cpu6,Xwayland,1]file: drivers/gpu/drm/udl/udl_fb.c, func: udl_fbdev_destroy, line: 447, dev->dev->init_name: (null), &ufbdev->helper: ffffffd0fa999280, ufbdev->ufb.obj: ffffffd0fa1b6c80, &ufbdev->ufb.base: ffffffd0fa9994a0, &ufbdev->ufb.obj->base: ffffffd0fa1b6c80
```

```c
./scripts/faddr2line ./drivers/gpu/drm/udl/udl_drv.o udl_driver_release+0x20/0x48
udl_driver_release+0x20/0x48:
udl_driver_release 于 /data3/home/yuanqiliang/code/klua-kernel/drivers/gpu/drm/udl/udl_drv.c:53
```

```c
./scripts/faddr2line ./drivers/gpu/drm/udl/udl_main.o udl_fini+0x38/0x60
udl_fini+0x38/0x60:
udl_fini 于 /data3/home/yuanqiliang/code/klua-kernel/drivers/gpu/drm/udl/udl_main.c:375
```

```c
./scripts/faddr2line drivers/gpu/drm/udl/udl_fb.o udl_fbdev_cleanup+0xa8/0xd8
skipping udl_fbdev_cleanup address at 0x68c due to size mismatch (0xd8 != 0x3c)
no match for udl_fbdev_cleanup+0xa8/0xd8
```

```c
./scripts/faddr2line vmlinux unregister_framebuffer+0x34/0x50
unregister_framebuffer+0x34/0x50:
unregister_framebuffer 于 drivers/video/fbdev/core/fbmem.c:1923
```

### 踩内存根因定位

#### udl_device

```c
struct udl_device { // drivers/gpu/drm/udl/udl_drv.h:52
	struct drm_device drm;
	struct device *dev;
	struct usb_device *udev;
	struct drm_crtc *crtc;

	struct mutex gem_lock;

	int sku_pixel_limit;

	struct urb_list urbs;
	atomic_t lost_pixels; /* 1 = a render op failed. Need screen refresh */

	struct udl_fbdev *fbdev;
	char mode_buf[1024];
	uint32_t mode_buf_len;
	atomic_t bytes_rendered; /* raw pixel-bytes driver asked to render */
	atomic_t bytes_identical; /* saved effort with backbuffer comparison */
	atomic_t bytes_sent; /* to usb, after compression including overhead */
	atomic_t cpu_kcycles_used; /* transpired during pixel processing */
};
```

```c
struct udl_device *udl
        -> struct drm_device drm       // /dev/dri/card1
        // drivers/gpu/drm/udl/udl_fb.c:454 struct udl_device *udl = to_udl(dev); 根据drm_device获取udl设备
        -> struct udl_fbdev *fbdev
                -> struct drm_fb_helper helper
                        -> struct fb_info *fbdev // dev/fb1
```

#### fb_info内存分配

```c
916 struct fb_info *drm_fb_helper_alloc_fbi(struct drm_fb_helper *fb_helper) // drivers/gpu/drm/drm_fb_helper.c:923
 917 {
 918         dump_stack();
 919         struct device *dev = fb_helper->dev->dev;
 920         struct fb_info *info;
 921         int ret;
 922 
 923         info = framebuffer_alloc(0, dev);
 924         if (!info)
 925                 return ERR_PTR(-ENOMEM);
 926 
 927         ret = fb_alloc_cmap(&info->cmap, 256, 0);
 928         if (ret)
 929                 goto err_release;
 930 
 931         info->apertures = alloc_apertures(1);
 932         if (!info->apertures) {
 933                 ret = -ENOMEM;
 934                 goto err_free_cmap;
 935         }
 936 
 937         fb_helper->fbdev = info;
 938 
 939         return info;
 940 
 941 err_free_cmap:
 942         fb_dealloc_cmap(&info->cmap);
 943 err_release:
 944         framebuffer_release(info);
 945         return ERR_PTR(ret);
 946 }
 947 EXPORT_SYMBOL(drm_fb_helper_alloc_fbi);
```

```c
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,8]Call trace:
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,9] dump_backtrace+0x0/0x2e0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,0] show_stack+0x24/0x30
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,1] dump_stack+0xcc/0x10c
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,2] drm_fb_helper_alloc_fbi+0x24/0x108
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,3] udlfb_create+0x134/0x358 [udl]
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,4] __drm_fb_helper_initial_config_and_unlock+0x354/0x6c0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,5] drm_fb_helper_initial_config+0x50/0x5c
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,6] udl_fbdev_init+0xf8/0x158 [udl]
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,7] udl_init+0x5f4/0x750 [udl]
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,8] udl_usb_probe+0x158/0x1b8 [udl]
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,9] usb_probe_interface+0x178/0x3e0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,0] really_probe+0x280/0x520
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,1] driver_probe_device+0x88/0x1a8
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,2] __device_attach_driver+0x10c/0x170
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,3] bus_for_each_drv+0xf8/0x158
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,4] __device_attach+0x15c/0x1e8
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,5] device_initial_probe+0x24/0x30
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,6] bus_probe_device+0xf0/0x100
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,7] device_add+0x538/0x958
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,8] usb_set_configuration+0x730/0xca0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,9] generic_probe+0x70/0xa0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,0] usb_probe_device+0x68/0xb0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,1] really_probe+0x280/0x520
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,2] driver_probe_device+0x88/0x1a8
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,3] __device_attach_driver+0x10c/0x170
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,4] bus_for_each_drv+0xf8/0x158
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,5] __device_attach+0x15c/0x1e8
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,6] device_initial_probe+0x24/0x30
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,7] bus_probe_device+0xf0/0x100
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,8] device_add+0x538/0x958
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,9] usb_new_device+0x424/0x998
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,0] hub_event+0xd08/0x1bd0
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,1] process_one_work+0x4fc/0xc80
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,2] worker_thread+0x80/0x700
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,3] kthread+0x208/0x218
[五 12月  1 09:41:21 2023] [pid:49,cpu7,kworker/7:0,4] ret_from_fork+0x10/0x1c
```

#### fb_info内存释放

```c
 970 void drm_fb_helper_fini(struct drm_fb_helper *fb_helper)   // drivers/gpu/drm/drm_fb_helper.c
 971 {
 972         struct fb_info *info;
 973 
 974         if (!fb_helper)
 975                 return;
 976 
 977         fb_helper->dev->fb_helper = NULL;
 978 
 979         printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->fbdev, drm     _fbdev_emulation);
 980         if (!drm_fbdev_emulation) { // drm_fbdev_emulation = false 980行才会return，994行才会执行不到
 981                 printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->fb     dev, drm_fbdev_emulation);
 982                 return;
 983         }
 984 
 985         cancel_work_sync(&fb_helper->resume_work);
 986         cancel_work_sync(&fb_helper->dirty_work);
 987 
 988         info = fb_helper->fbdev;
 989         if (info) {
 990                 if (info->cmap.len)
 991                         fb_dealloc_cmap(&info->cmap);   // drivers/video/fbdev/core/fbcmap.c:147
 992                 printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb     _helper->fbdev, drm_fbdev_emulation, info);
 993                 framebuffer_release(info);      // drivers/video/fbdev/core/fbsysfs.c:89
 994                 printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb     _helper->fbdev, drm_fbdev_emulation, info);
 995         }
 996         fb_helper->fbdev = NULL; // 这里已经将fb_helper->fbdev设置为NULL
 997 
 998         printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper-     >fbdev, drm_fbdev_emulation, info);
 999 
1000         mutex_lock(&kernel_fb_helper_lock);
1001         if (!list_empty(&fb_helper->kernel_fb_list)) {
1002                 printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px, &fb_helper->kernel_fb_list: %px", __FILE__, __LI     NE__, __FUNCTION__, fb_helper, fb_helper->fbdev, drm_fbdev_emulation, info, &fb_helper->kernel_fb_list);
1003                 list_del(&fb_helper->kernel_fb_list);
1004                 if (list_empty(&kernel_fb_helper_list)) {
1005                         printk("file: %s, line: %d, fun: %s, dev: %px, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px, &kernel_fb_helper_list: %px", __FILE__,      __LINE__, __FUNCTION__, fb_helper, fb_helper->fbdev, drm_fbdev_emulation, info, &kernel_fb_helper_list);
1006                         unregister_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);
1007                 }
1008         }
1009         mutex_unlock(&kernel_fb_helper_lock);
1010 
1011         mutex_destroy(&fb_helper->lock);
1012         drm_fb_helper_crtc_free(fb_helper);
1013 
1014 }
1015 EXPORT_SYMBOL(drm_fb_helper_fini);
```

```c
494 void udl_fbdev_cleanup(struct drm_device *dev)      // drivers/gpu/drm/udl/udl_fb.c:501
495 {
496         struct udl_device *udl = to_udl(dev);
497         printk("file: %s, func: %s, line: %d, dev->dev->init_name: %s\n", __FILE__, __func__, __LINE__, dev->dev->init_name);
498         if (!udl->fbdev)
499                 return;
500 
501         udl_fbdev_destroy(dev, udl->fbdev);
502         kfree(udl->fbdev);
503         udl->fbdev = NULL;
504 }
```

```c
grep 'kfree(udl);' . -inr --color --include=*.c 
./drivers/gpu/drm/udl/udl_drv.c:95:             kfree(udl);
./drivers/gpu/drm/udl/udl_drv.c:107:            kfree(udl);
```

struct udl_device *udl 没有主动释放，存在内存泄漏，在udl驱动模块卸载后内核会自动回收内存。

```c
970 void drm_fb_helper_fini(struct drm_fb_helper *fb_helper) // drivers/gpu/drm/drm_fb_helper.c:970
 971 {
 972         struct fb_info *info;
 973 
 974         if (!fb_helper)
 975                 return;
 976 
 977         fb_helper->dev->fb_helper = NULL;
 978 
 979         printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->fbdev, drm_fbdev_emu     lation);
 980         if (!drm_fbdev_emulation) { // drm_fbdev_emulation = false 980行才会return，994行才会执行不到
 981                 printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->fbdev, drm_f     bdev_emulation);
 982                 return;
 983         }
 984 
 985         cancel_work_sync(&fb_helper->resume_work);
 986         cancel_work_sync(&fb_helper->dirty_work);
 987 
 988         info = fb_helper->fbdev;
 989         if (info) {
 990                 printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->f     bdev, drm_fbdev_emulation, info);
 991                 if (info->cmap.len)
 992                         fb_dealloc_cmap(&info->cmap);   // drivers/video/fbdev/core/fbcmap.c:147
 993                 printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->f     bdev, drm_fbdev_emulation, info);
 994                 framebuffer_release(info);      // drivers/video/fbdev/core/fbsysfs.c:89 这里释放内存了，但是drivers/video/fbdev/core/fbmem.c中的registered_fb指针数组还是指向这里，有进程改写这块内存，再次访问就会导致踩内存
 995                 printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->f     bdev, drm_fbdev_emulation, info);
 996         }
 997         fb_helper->fbdev = NULL;
 998 
 999         printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px", __FILE__, __LINE__, __FUNCTION__, fb_helper, fb_helper->fbdev, dr     m_fbdev_emulation, info);
1000 
1001         mutex_lock(&kernel_fb_helper_lock);
1002         if (!list_empty(&fb_helper->kernel_fb_list)) {
1003                 printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px, &fb_helper->kernel_fb_list: %px", __FILE__, __LINE__, __FU     NCTION__, fb_helper, fb_helper->fbdev, drm_fbdev_emulation, info, &fb_helper->kernel_fb_list);
1004                 list_del(&fb_helper->kernel_fb_list);
1005                 if (list_empty(&kernel_fb_helper_list)) {
1006                         printk("file: %s, line: %d, fun: %s, fb_helper: %px, fb_helper->fbdev: %px, drm_fbdev_emulation: %d, info: %px, &kernel_fb_helper_list: %px", __FILE__, __LINE__,      __FUNCTION__, fb_helper, fb_helper->fbdev, drm_fbdev_emulation, info, &kernel_fb_helper_list);
1007                         unregister_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);
1008                 }
1009         }
1010         mutex_unlock(&kernel_fb_helper_lock);
1011 
1012         mutex_destroy(&fb_helper->lock);
1013         drm_fb_helper_crtc_free(fb_helper);
1014 
1015 }
1016 EXPORT_SYMBOL(drm_fb_helper_fini);
```

drivers/gpu/drm/drm_fb_helper.c:994行的framebuffer_release(info)释放了内存，但是drivers/video/fbdev/core/fbmem.c中的registered_fb指针数组还是指向这里，有进程改写这块内存，再次访问就会导致踩内存。

usb_disconnect、drm_release都会执行到unbind_console，但是unbind_console返回值是1，导致在drivers/video/fbdev/core/fbmem.c:1811结束，导致1823行的registered_fb[fb_info->node] = NULL;无法执行。

执行lshw命令的时候，会通过registered_fb[i]得到指针*info, mutex_lock(&info->lock)加锁的时候刚好访问了被改写的内存，导致踩内存，进一步内核 oops panic。

#### 修复方案

```bash
git remote -v
origin  ssh://ut004487@gerrit.uniontech.com:29418/klua-kernel (fetch)
origin  ssh://ut004487@gerrit.uniontech.com:29418/klua-kernel (push)
```

```bash
git branch 
* klua-need-merge
```

```bash
git log --oneline 
81089c9ab (HEAD -> klua-need-merge) usb: udl: fix udl mutex_lock panic
3611cebb9 (origin/klua-need-merge) init/version.c: add linux_commitid_banner for init/main.c
```

0001-usb-udl-fix-udl-mutex_lock-panic.patch中有本次提交的补丁。

```c
commit 81089c9ab03834e2331711d0abeff4b39d596152 (HEAD -> klua-need-merge)
Author: yuanqiliang <yuanqiliang@uniontech.com>
Date:   Fri Dec 1 09:52:05 2023 +0800

    usb: udl: fix udl mutex_lock panic
    
    drivers/video/fbdev/core/fbmem.c: 1811
    
    1807         ret = unbind_console(fb_info);
    1808
    1809         if (ret) {
    1810                 printk("file: %s, line: %d, fun: %s, fb->info: %px, &fb_info->lock:%px, &fb_info->lock.owner:%px, fb_info->fbops: %px, fb_info->fbops->fb_destroy:%px, ret: %d\n", __FILE__, __LINE__, __FUNCTION__, fb_info, &fb_info->lock, &fb_info->lock.owner, fb_info->fbops, fb_info->fbops->fb_destroy, ret);
    1811                 // return -EINVAL;
    1812         }
    
    bug: https://pms.uniontech.com/bug-view-229783.html
    Change-Id: Idde9b9486bb4d559852b5d2ec4fd071bc3bd5534
    Signed-off-by: yuanqiliang <yuanqiliang@uniontech.com>
```
