---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# Desktop Freeze on UNIS D3812 D2000

<https://pms.uniontech.com/bug-view-234167.html>

UNIS D3812 D2000 系统资源占用过多，存在影院崩溃的情况。

桌面可以显示，但是右下角时间不动。

鼠标可以移动，键盘灯常亮，按键无响应。

新插入别的usb设备，也无法响应。

拔掉键盘再插入，键盘灯常亮，按键无响应。

拔掉鼠标后再插入，拖动鼠标无响应。

## 初步分析

后台一直跑着自制的性能监测小工具，发现系统并没有死掉，但是桌面卡住了。截取部分top日志：

```bash
sed -n '801165,802645'p top_20231218181114.log > top_20231219224132.log
```

内核版本：

```bash
2023-12-18 17:56:54 username kernel: [    0.000000] Linux version 4.19.0-arm64-desktop (uosserver@uosserver-PC) (gcc version 8.3.0 (Uos 8.3.0.3-3+rebuild)) (74123f25fea1) #6035 SMP Wed Jul 26 14:34:54 CST 2023
```

将桌面卡主附近的kern.log中oops相关日志导出到oops_gfx.log中：

```bash
3753 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596004] Modules linked in: bnep vfs_monitor(O) udl nfnetlink_queue nfnetlink_log nfnetlink fuse st bluetooth ecdh_generic rfkill cpufreq_powersave nls_iso8859_1 nls     _cp437 aes_ce_blk crypto_simd cryptd aes_ce_cipher crct10dif_ce ghash_ce aes_arm64 sha2_ce sha256_arm64 ppdev hid_generic serio_raw sha1_ce parport_serial parport_pc parport at24 scpi_hwmon uos_resources(O) u     os_bluetooth_connection_control(O) lightorange lightnut lightfig lightcherry filearmor(O) smc_dri(O) binder_linux(O) ashmem_linux(O) efivarfs ip_tables x_tables usbkbd amdgpu usbhid chash gpu_sched radeon phy     tium_sdci mmc_core motorcomm
3754 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596085] CPU: 1 PID: 301 Comm: gfx Tainted: G           O      4.19.0-arm64-desktop #6035
3755 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596090] Hardware name: UNIS UNIS D3812 G2 0001/MF266B, BIOS DF266AB010ZG 08/23/2023 09:09:48
3756 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596096] pstate: 60000085 (nZCv daIf -PAN -UAO)
3757 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596103] pc : __queue_delayed_work+0x94/0xb0
3758 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596109] lr : queue_delayed_work_on+0x50/0x68
3759 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596113] sp : ffff8020e367fd90
3760 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596117] x29: ffff8020e367fd90 x28: ffff8020e2624d10
3761 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596123] x27: ffff8020e2624df8 x26: ffff8020e2624d10
3762 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596129] x25: ffff80005a3d2d88 x24: ffff000000e50940
3763 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596134] x23: ffff000000e50b98 x22: ffff8020e2624d10
3764 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596139] x21: ffff801ff0544c00 x20: ffff80005a14fc00
3765 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596144] x19: 0000000000000000 x18: 0000000000000000
3766 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596149] x17: 0000000000000000 x16: 0000000000000000
3767 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596154] x15: 0000000000000000 x14: 000000806167536a
3768 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596160] x13: 0000000000000155 x12: 0000000000000400
3769 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596165] x11: 0000000000000003 x10: 00000000000008e0
3770 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596170] x9 : 00000000005d5564 x8 : 0000000000000000
3771 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596175] x7 : 0000000000000000 x6 : ffff0000080f2b18
3772 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596179] x5 : ffff801ff0547080 x4 : ffff801ff0544c80
3773 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596184] x3 : 00000000000009c4 x2 : ffff801ff0544c60
3774 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596189] x1 : ffff8020f6c08600 x0 : 0000000000000100
3775 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596195] Call trace:
3776 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596201]  __queue_delayed_work+0x94/0xb0
3777 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596207]  queue_delayed_work_on+0x50/0x68
3778 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596222]  drm_sched_main+0x44c/0x4b0 [gpu_sched]
3779 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596228]  kthread+0x128/0x130
3780 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596235]  ret_from_fork+0x10/0x18
3781 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596239] ---[ end trace 3eac7548d3296dca ]---
3782 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596246] ------------[ cut here ]------------
3783 2023-12-19 22:41:36 hbyc-PC kernel: [102629.596250] kernel BUG at kernel/time/timer.c:1138!
3784 2023-12-19 22:41:36 hbyc-PC kernel: [102629.601214] Internal error: Oops - BUG: 0 [#1] SMP
3785 2023-12-19 22:41:36 hbyc-PC kernel: [102629.606086] Modules linked in: bnep vfs_monitor(O) udl nfnetlink_queue nfnetlink_log nfnetlink fuse st bluetooth ecdh_generic rfkill cpufreq_powersave nls_iso8859_1 nls     _cp437 aes_ce_blk crypto_simd cryptd aes_ce_cipher crct10dif_ce ghash_ce aes_arm64 sha2_ce sha256_arm64 ppdev hid_generic serio_raw sha1_ce parport_serial parport_pc parport at24 scpi_hwmon uos_resources(O) u     os_bluetooth_connection_control(O) lightorange lightnut lightfig lightcherry filearmor(O) smc_dri(O) binder_linux(O) ashmem_linux(O) efivarfs ip_tables x_tables usbkbd amdgpu usbhid chash gpu_sched radeon phy     tium_sdci mmc_core motorcomm
3786 2023-12-19 22:41:36 hbyc-PC kernel: [102629.659813] Process gfx (pid: 301, stack limit = 0x000000006119d7ac)
3787 2023-12-19 22:41:36 hbyc-PC kernel: [102629.666248] CPU: 1 PID: 301 Comm: gfx Tainted: G        W  O      4.19.0-arm64-desktop #6035
3788 2023-12-19 22:41:36 hbyc-PC kernel: [102629.674764] Hardware name: UNIS UNIS D3812 G2 0001/MF266B, BIOS DF266AB010ZG 08/23/2023 09:09:48
3789 2023-12-19 22:41:36 hbyc-PC kernel: [102629.683630] pstate: 60000085 (nZCv daIf -PAN -UAO)
3790 2023-12-19 22:41:36 hbyc-PC kernel: [102629.688503] pc : add_timer+0x20/0x28
3791 2023-12-19 22:41:36 hbyc-PC kernel: [102629.692159] lr : __queue_delayed_work+0x68/0xb0
3792 2023-12-19 22:41:36 hbyc-PC kernel: [102629.696768] sp : ffff8020e367fd80
3793 2023-12-19 22:41:36 hbyc-PC kernel: [102629.700161] x29: ffff8020e367fd80 x28: ffff8020e2624d10
3794 2023-12-19 22:41:36 hbyc-PC kernel: [102629.705554] x27: ffff8020e2624df8 x26: ffff8020e2624d10
3795 2023-12-19 22:41:36 hbyc-PC kernel: [102629.710947] x25: ffff80005a3d2d88 x24: ffff000000e50940
3796 2023-12-19 22:41:36 hbyc-PC kernel: [102629.716339] x23: ffff000000e50b98 x22: ffff8020e2624d10
3797 2023-12-19 22:41:36 hbyc-PC kernel: [102629.721731] x21: ffff801ff0544c00 x20: ffff80005a14fc00
3798 2023-12-19 22:41:36 hbyc-PC kernel: [102629.727123] x19: 0000000000000000 x18: 0000000000000000
3799 2023-12-19 22:41:36 hbyc-PC kernel: [102629.732514] x17: 0000000000000000 x16: 0000000000000000
3800 2023-12-19 22:41:36 hbyc-PC kernel: [102629.737906] x15: 0000000000000000 x14: 000000806167536a
3801 2023-12-19 22:41:36 hbyc-PC kernel: [102629.743298] x13: 0000000000000155 x12: 0000000000000400
3802 2023-12-19 22:41:36 hbyc-PC kernel: [102629.748689] x11: 0000000000000003 x10: 00000000000008e0
3803 2023-12-19 22:41:36 hbyc-PC kernel: [102629.754082] x9 : 00000000005d5564 x8 : 0000000000000000
3804 2023-12-19 22:41:36 hbyc-PC kernel: [102629.759473] x7 : 0000000000000000 x6 : ffff801ff0544c68
3805 2023-12-19 22:41:36 hbyc-PC kernel: [102629.764865] x5 : ffff000009807000 x4 : ffff801ff0544c80
3806 2023-12-19 22:41:36 hbyc-PC kernel: [102629.770256] x3 : 00000000000009c4 x2 : ffff801ff0547080
3807 2023-12-19 22:41:36 hbyc-PC kernel: [102629.775648] x1 : 00000001018664b3 x0 : ffff801ff0544c80
3808 2023-12-19 22:41:36 hbyc-PC kernel: [102629.781039] Call trace:
3809 2023-12-19 22:41:36 hbyc-PC kernel: [102629.783566]  add_timer+0x20/0x28
3810 2023-12-19 22:41:36 hbyc-PC kernel: [102629.786874]  __queue_delayed_work+0x68/0xb0
3811 2023-12-19 22:41:36 hbyc-PC kernel: [102629.791138]  queue_delayed_work_on+0x50/0x68
3812 2023-12-19 22:41:36 hbyc-PC kernel: [102629.795493]  drm_sched_main+0x44c/0x4b0 [gpu_sched]
3813 2023-12-19 22:41:36 hbyc-PC kernel: [102629.800450]  kthread+0x128/0x130
3814 2023-12-19 22:41:36 hbyc-PC kernel: [102629.803758]  ret_from_fork+0x10/0x18
3815 2023-12-19 22:41:36 hbyc-PC kernel: [102629.807415] Code: f9400801 97ffff21 a8c17bfd d65f03c0 (d4210000)
3816 2023-12-19 22:41:36 hbyc-PC kernel: [102629.813590] ---[ end trace 3eac7548d3296dcb ]---
3817 2023-12-19 22:41:45 hbyc-PC kernel: [102639.646128] [drm:amdgpu_job_timedout [amdgpu]] *ERROR* ring gfx timeout, signaled seq=27126054, emitted seq=27126054
3818 2023-12-19 22:41:45 hbyc-PC kernel: [102639.656784] [drm] GPU recovery disabled.
```

```bash
modinfo gpu_sched

filename:       /lib/modules/4.19.0-arm64-desktop/kernel/drivers/gpu/drm/scheduler/gpu-sched.ko
license:        GPL and additional rights
description:    DRM GPU scheduler
depends:        
intree:         Y
name:           gpu_sched
vermagic:       4.19.0-arm64-desktop SMP mod_unload modversions aarch64
```

```bash
vim drivers/gpu/drm/scheduler/Makefile +25

obj-$(CONFIG_DRM_SCHED) += gpu-sched.o
```

```bash
grep CONFIG_DRM_SCHED .config
CONFIG_DRM_SCHED=m
```

```bash
 ~/code/arm-kernel/scripts/faddr2line gpu-sched.ko drm_sched_main+0x44c/0x4b0
drm_sched_main+0x44c/0x4b0:
queue_delayed_work 于 /data3/home/yuanqiliang/code/arm-kernel/./include/linux/workqueue.h:527
515 /**
516  * queue_delayed_work - queue work on a workqueue after delay
517  * @wq: workqueue to use
518  * @dwork: delayable work to queue
519  * @delay: number of jiffies to wait before queueing
520  *
521  * Equivalent to queue_delayed_work_on() but tries to use the local CPU.
522  */
523 static inline bool queue_delayed_work(struct workqueue_struct *wq,
524                       struct delayed_work *dwork,                                                                                                                                   
525                       unsigned long delay)                                                                                                           
526 {                                                                                                                                                    
527     return queue_delayed_work_on(WORK_CPU_UNBOUND, wq, dwork, delay);   // WORK_CPU_UNBOUND 表示不绑定到特定 CPU
528 }

(已内连入)schedule_delayed_work 于 /data3/home/yuanqiliang/code/arm-kernel/./include/linux/workqueue.h:628
617 /**
618  * schedule_delayed_work - put work task in global workqueue after delay
619  * @dwork: job to be done
620  * @delay: number of jiffies to wait or 0 for immediate execution
621  *
622  * After waiting for a given time this puts a job in the kernel-global
623  * workqueue.
624  */
625 static inline bool schedule_delayed_work(struct delayed_work *dwork,
626                      unsigned long delay)
627 {
628     return queue_delayed_work(system_wq, dwork, delay);                                                                                                                             
629 }

(已内连入)drm_sched_job_begin 于 /data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:598
586 static void drm_sched_job_begin(struct drm_sched_job *s_job)
587 {
588     struct drm_gpu_scheduler *sched = s_job->sched;
589 
590     dma_fence_add_callback(&s_job->s_fence->finished, &s_job->finish_cb,
591                    drm_sched_job_finish_cb);
592 
593     spin_lock(&sched->job_list_lock);
594     list_add_tail(&s_job->node, &sched->ring_mirror_list);
595     if (sched->timeout != MAX_SCHEDULE_TIMEOUT &&
596         list_first_entry_or_null(&sched->ring_mirror_list,
597                      struct drm_sched_job, node) == s_job)
598         schedule_delayed_work(&s_job->work_tdr, sched->timeout);                                                                                                                    
599     spin_unlock(&sched->job_list_lock);
600 }

(已内连入)drm_sched_main 于 /data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:876
840 /**
841  * drm_sched_main - main scheduler thread                                                                                                                                           
842  *
843  * @param: scheduler instance
844  *
845  * Returns 0.
846  */
847 static int drm_sched_main(void *param)
848 {
849     struct sched_param sparam = {.sched_priority = 1};
850     struct drm_gpu_scheduler *sched = (struct drm_gpu_scheduler *)param;
851     int r;
852 
853     sched_setscheduler(current, SCHED_FIFO, &sparam);
854 
855     while (!kthread_should_stop()) {
856         struct drm_sched_entity *entity = NULL;
857         struct drm_sched_fence *s_fence;
858         struct drm_sched_job *sched_job;
859         struct dma_fence *fence;
860 
861         wait_event_interruptible(sched->wake_up_worker,
862                      (!drm_sched_blocked(sched) &&
863                       (entity = drm_sched_select_entity(sched))) ||
864                      kthread_should_stop());
865 
866         if (!entity)
867             continue;
868 
869         sched_job = drm_sched_entity_pop_job(entity);
870         if (!sched_job)
871             continue;
872 
873         s_fence = sched_job->s_fence;
874 
875         atomic_inc(&sched->hw_rq_count);
876         drm_sched_job_begin(sched_job);
877 
878         fence = sched->ops->run_job(sched_job);
879         drm_sched_fence_scheduled(s_fence);
880 
881         if (fence) {
882             s_fence->parent = dma_fence_get(fence);
883             r = dma_fence_add_callback(fence, &s_fence->cb,
884                            drm_sched_process_job);
885             if (r == -ENOENT)
886                 drm_sched_process_job(fence, &s_fence->cb);
887             else if (r)
888                 DRM_ERROR("fence add callback failed (%d)\n",
889                       r);
890             dma_fence_put(fence);
891         } else {
892             drm_sched_process_job(NULL, &s_fence->cb);
893         }
894 
895         wake_up(&sched->job_scheduled);
896     }
897     return 0;
898 }
```

```c
objdump -d -l -S gpu-sched.ko > gpu-sched.objdump

vim gpu-sched.objdump +2821

0000000000001448 <drm_sched_main>:  // 0x1448+0x44c=0x1894
drm_sched_main():
/data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:848
{
    1448:	a9b67bfd 	stp	x29, x30, [sp, #-160]!
/data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:849
	struct sched_param sparam = {.sched_priority = 1};
    144c:	52800023 	mov	w3, #0x1                   	// #1
/data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:853
	sched_setscheduler(current, SCHED_FIFO, &sparam);
    1450:	2a0303e1 	mov	w1, w3
/data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:848
{
    1454:	910003fd 	mov	x29, sp
    1458:	a9025bf5 	stp	x21, x22, [sp, #32]
    145c:	aa0003f6 	mov	x22, x0
......
......
......
/data3/home/yuanqiliang/code/arm-kernel/drivers/gpu/drm/scheduler/gpu_scheduler.c:886
				drm_sched_process_job(fence, &s_fence->cb);
    1870:	aa1503e1 	mov	x1, x21
    1874:	aa1303e0 	mov	x0, x19
    1878:	97fffd58 	bl	dd8 <drm_sched_process_job>
    187c:	17ffffab 	b	1728 <drm_sched_main+0x2e0>
schedule_delayed_work():
/data3/home/yuanqiliang/code/arm-kernel/./include/linux/workqueue.h:628
	return queue_delayed_work(system_wq, dwork, delay);
    1880:	90000001 	adrp	x1, 0 <system_wq>
queue_delayed_work():
/data3/home/yuanqiliang/code/arm-kernel/./include/linux/workqueue.h:527
	return queue_delayed_work_on(WORK_CPU_UNBOUND, wq, dwork, delay);
    1884: 91004042  add x2, x2, #0x10    ; 将 x2 寄存器的值加上 0x10
    1888: 52802000  mov w0, #0x100      ; 将 w0 寄存器的值设置为 0x100
    188c: f9400021  ldr x1, [x1]        ; 从存储在 x1 寄存器中的地址处加载一个 64 位值到 x1 寄存器
    1890: 94000000  bl 0 <queue_delayed_work_on>  ; 调用一个函数，函数地址为 0，即 queue_delayed_work_on
    1894: 17ffff8e  b 16cc <drm_sched_main+0x284>  ; 无条件分支到地址 16cc  // 0x1448+0x44c=0x1894
```

```c
/data3/home/yuanqiliang/code/arm-kernel/./include/asm-generic/qspinlock.h:101
	smp_store_release(&lock->locked, 0);
    16cc:	52800000 	mov	w0, #0x0                   	// #0
    16d0:	91050261 	add	x1, x19, #0x140
    16d4:	089ffc20 	stlrb	w0, [x1]
```

```bash
./scripts/faddr2line vmlinux add_timer+0x20/0x28
add_timer+0x20/0x28:
add_timer 于 kernel/time/timer.c:1138
(已内连入)add_timer 于 kernel/time/timer.c:1136
```

```c
void add_timer(struct timer_list *timer)  // vim kernel/time/timer.c +1138
{
	BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}
EXPORT_SYMBOL(add_timer);
```

```c
#define BUG_ON(condition) do { if (unlikely(condition)) BUG(); } while (0)
```

该宏接受一个条件表达式作为参数。如果条件表达式为真（即非零），则会调用BUG()宏，导致程序终止并生成错误报告。

timer_pending(timer) 是一个函数或宏，用于检查指定的定时器是否处于挂起状态。如果定时器已经被挂起，表示存在错误的状态，那么BUG_ON()宏就会触发一个BUG，导致程序终止执行。

```c
objdump -d -l -S timer.o > timer.objdump

vim timer.objdump +5179

5179 00000000000027c8 <add_timer>:  // 0x27c8+0x20=0x27e8
5180 add_timer():
5181 /data3/home/yuanqiliang/code/arm-kernel/kernel/time/timer.c:1137
5182 {
5183     27c8:   a9bf7bfd    stp x29, x30, [sp, #-16]!
5184     27cc:   910003fd    mov x29, sp
5185 /data3/home/yuanqiliang/code/arm-kernel/kernel/time/timer.c:1138
5186     BUG_ON(timer_pending(timer));
5187     27d0:   f9400402    ldr x2, [x0, #8]
5188     27d4:   b50000a2    cbnz    x2, 27e8 <add_timer+0x20>
5189 /data3/home/yuanqiliang/code/arm-kernel/kernel/time/timer.c:1139
5190     mod_timer(timer, timer->expires);
5191     27d8:   f9400801    ldr x1, [x0, #16]
5192     27dc:   94000000    bl  2460 <mod_timer>
5193 /data3/home/yuanqiliang/code/arm-kernel/kernel/time/timer.c:1140
5194 }
5195     27e0:   a8c17bfd    ldp x29, x30, [sp], #16
5196     27e4:   d65f03c0    ret 
5197 /data3/home/yuanqiliang/code/arm-kernel/kernel/time/timer.c:1138
5198     BUG_ON(timer_pending(timer));
5199     27e8:   d4210000    brk #0x800  // 0x27c8+0x20=0x27e8
5200     27ec:   d503201f    nop 
```

brk #0x800 触发了一个软中断，参数为 0x800。软中断通常被用于在程序中插入调试点，以便在这里停下程序的执行，以便进行调试。

```c
144 /**
145  * timer_pending - is a timer pending?
146  * @timer: the timer in question
147  *
148  * timer_pending will tell whether a given timer is currently pending,
149  * or not. Callers must ensure serialization wrt. other operations done
150  * to this timer, eg. interrupt contexts, or other CPUs on SMP.
151  *
152  * return value: 1 if the timer is pending, 0 if not.
153  */
154 static inline int timer_pending(const struct timer_list * timer)
155 {
156     return timer->entry.pprev != NULL;
157 }
```

timer_pending函数检查给定的定时器是否处于等待状态。当 timer->entry.pprev 不为 NULL 时，表示定时器当前处于等待状态，返回 1；否则，返回 0。

```c
 11 struct timer_list { // vim include/linux/timer.h +11
 12     /*
 13      * All fields that change during normal runtime grouped to the
 14      * same cacheline 所有在正常运行时发生变化的字段被分组到相同的缓存行中
 15      */
 16     struct hlist_node   entry;  // 链入定时器特定哈希表的节点
 17     unsigned long       expires;  // 定时器到期时间，以节拍（jiffies）为单位
 18     void            (*function)(struct timer_list *);  // 定时器到期时执行的回调函数
 19     u32         flags;  // 定时器标志
 20 
 21 #ifdef CONFIG_LOCKDEP
 22     struct lockdep_map  lockdep_map;  // 用于调试的锁依赖图
 23 #endif
 24 }; 
```

- [笔记之内核定时器（timer_list）](https://blog.csdn.net/hxhardway/article/details/79037332)

在 GPU 调度器（gpu_sched）中，如果 timer_list 为空，通常表示当前没有定时器在等待执行。这可能是正常的状态，尤其是当没有 GPU 任务或者所有 GPU 任务都已经及时完成时。

具体来说，timer_list 一般用于跟踪 GPU 调度器中的计时器，这些计时器可能用于任务的超时、延迟或其他与时间相关的操作。当没有任务等待执行，或者已经按时完成，timer_list 可能为空。

如果你观察到 timer_list 为空，但 GPU 任务并未按预期执行，可能需要进一步调查原因。可能的原因包括：

任务完成： 当 GPU 任务按时完成时，相关的计时器可能会被取消，从而导致 timer_list 为空。

任务延迟： 如果任务因某些原因而延迟执行，相关的计时器可能还在等待。在这种情况下，你可能需要检查任务执行的日志或其他调试信息，以找到延迟的原因。

调度器问题： 如果 GPU 调度器本身存在问题，可能导致计时器未正确添加或处理。在这种情况下，你可能需要查看 GPU 调度器的源代码、文档或相关的日志信息。

总的来说，timer_list 为空并不一定表示问题，这可能是 GPU 调度器正常运行的一部分。然而，如果你怀疑有问题，最好检查相关的调试信息以了解更多上下文。

```bash
3817 2023-12-19 22:41:45 hbyc-PC kernel: [102639.646128] [drm:amdgpu_job_timedout [amdgpu]] *ERROR* ring gfx timeout, signaled seq=27126054, emitted seq=27126054
```

```c
 215 /**
 216  * DOC: lockup_timeout (int)
 217  * Set GPU scheduler timeout value in ms. Value 0 is invalidated, will be adjusted to 10000.
 218  * Negative values mean 'infinite timeout' (MAX_JIFFY_OFFSET). The default is 10000.
 219  */
 220 MODULE_PARM_DESC(lockup_timeout, "GPU lockup timeout in ms > 0 (default 10000)");
 221 module_param_named(lockup_timeout, amdgpu_lockup_timeout, int, 0444);
```

```bash
cat /sys/module/amdgpu/parameters/lockup_timeout
10000
```

amdgpu_job_timedout: 当调度器执行任务超时，调用的超时处理回调函数。

## 继续排查

将`lockup_timeout`参数调大:

```bash
echo 20000 > /sys/module/your_module/parameters/lockup_timeout
```

- [[SOLVED?] Frequent Freezes/Crashes with AMD 5700 XT](https://bbs.archlinux.org/viewtopic.php?id=253848)
- [关于amdgpu驱动崩溃问题](https://bbs.loongarch.org/d/278-amdgpu/10)

cpu动态调频开启没？

gpu调频开启没？

```bash
trace-bpfcc -tUK amdgpu_set_dpm_forced_performance_level
```
