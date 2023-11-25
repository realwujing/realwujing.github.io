# udl mutex_lock panic

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
<4>[  123.175903s][pid:6550,cpu6,lshw-bak,0]54a8  491154a0 ffffffc3 6cddbb70 ffffffc3 00000000 00000000 0000182d 84000104
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

```c
[一 11月 27 15:03:12 2023] [pid:84,cpu6,kworker/6:1,7]file: drivers/video/fbdev/core/fbmem.c, line: 1704, fun: do_register_framebuffer, fb->info: ffffffd087c3c880, &fb_info->lock:ffffffd087c3c890, &fb_info->lock.owner:ffffffd087c3c890.
```

```c
1603 <4>[  317.052642s][pid:8203,cpu6,lshw-bak,8]file: drivers/video/fbdev/core/fbmem.c, line: 1467, fun: fb_open, inode:ffffffd08aca0eb0, file: ffffffd071b4b940, info:ffffffd084cd1880, fbidx:0.
1604 <4>[  317.052642s][pid:8203,cpu6,lshw-bak,9]file: drivers/video/fbdev/core/fbmem.c, line: 1488, fun: fb_open, inode:ffffffd08aca0eb0, file: ffffffd071b4b940, info:ffffffd084cd1880, fbidx: 0, &info->lock: ffffffd084cd1890.
1605 <4>[  317.052856s][pid:8203,cpu6,lshw-bak,0]file: drivers/video/fbdev/core/fbmem.c, line: 1467, fun: fb_open, inode:ffffffd08aca3de0, file: ffffffd07a07f940, info:ffffffd087c3c880, fbidx:1.
1606 <4>[  317.052856s][pid:8203,cpu6,lshw-bak,1]file: drivers/video/fbdev/core/fbmem.c, line: 1488, fun: fb_open, inode:ffffffd08aca3de0, file: ffffffd07a07f940, info:ffffffd087c3c880, fbidx: 1, &info->lock: ffffffd087c3c890.
```

当前已用slub_debug、kasan排查内存错误问题，没发现问题。

华为klua-kernel无法开启lockdep检测死锁，当前比较怀疑死锁导致oops。
