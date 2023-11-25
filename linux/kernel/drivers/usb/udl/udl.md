
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

