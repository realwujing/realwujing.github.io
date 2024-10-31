---
date: 2024/09/25 15:16:11
updated: 2024/09/25 15:16:11
---

# ext4_inode_info double free

<https://pms.uniontech.com/bug-view-233267.html>

## dmesg

dmesg.202312111431

### warning

```c
[201866.888630] WARNING: CPU: 2 PID: 67 at fs/dcache.c:338 dentry_free+0x24/0xa8
[201866.888632] Modules linked in: tun uvcvideo videobuf2_vmalloc videobuf2_memops videobuf2_v4l2 videobuf2_common videodev media arc4 md4 sha512_generic sha512_arm64 nls_utf8 cifs(O) ccm dns_resolver uinput nfnetlink_queue nfnet
link_log nfnetlink fuse dlp_fcore(O) bnep st bluetooth ecdh_generic rfkill vfs_monitor(O) clink_vhci_hcd(O) clink_usbip_core(O) aes_ce_blk crypto_simd cryptd nls_iso8859_1 aes_ce_cipher crct10dif_ce nls_cp437 ghash_ce aes_arm64 s
ha2_ce sha256_arm64 sha1_ce hid_generic usbkbd usbhid snd_intel8x0 virtio_balloon snd_ac97_codec qemu_fw_cfg uos_resources(O) uos_bluetooth_connection_control(O) rdma_cm iw_cm ib_cm ib_core efivarfs ip_tables x_tables btrfs xor r
aid6_pq virtio_blk virtio_net net_failover failover button virtio_mmio
[201866.888856] CPU: 2 PID: 67 Comm: kswapd0 Kdump: loaded Tainted: G           O      4.19.0-arm64-desktop #5312
[201866.888857] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
[201866.888860] pstate: 40c00005 (nZcv daif +PAN +UAO)
[201866.888862] pc : dentry_free+0x24/0xa8
[201866.888863] lr : dentry_free+0x24/0xa8
[201866.888864] sp : ffff8003dd467a70
[201866.888865] x29: ffff8003dd467a70 x28: 000000000000040e 
[201866.888867] x27: ffff800069db41d8 x26: 0000000000000000 
[201866.888868] x25: ffff8003dd467b70 x24: ffff800069db4180 
[201866.888870] x23: ffff800069e49e70 x22: ffff800069db40c0 
[201866.888871] x21: ffff800069db4118 x20: ffff800069db4000 
[201866.888873] x19: ffff800069db40c0 x18: ffff000009824000 
[201866.888874] x17: 0000000000000000 x16: 0000000000000000 
[201866.888875] x15: 00000000fffffff0 x14: ffff000009959752 
[201866.888880] x13: 0000000000061498 x12: ffff000009958000 
[201866.888881] x11: ffff000009824000 x10: ffff000009958da8 
[201866.888883] x9 : 0000000000000000 x8 : 0000000000000004 
[201866.888884] x7 : ffff000009958000 x6 : 00000000000026ff 
[201866.888885] x5 : 0000000000000000 x4 : 0000000000000000 
[201866.888886] x3 : 0000000000000000 x2 : ffff8003ffe4ac88 
[201866.888888] x1 : 00008003f6995000 x0 : 0000000000000024 
[201866.888890] Call trace:
[201866.888892]  dentry_free+0x24/0xa8
[201866.888894]  __dentry_kill+0x148/0x1d0
[201866.888895]  dentry_kill+0x1cc/0x270
[201866.888897]  shrink_dentry_list+0x1d0/0x2c0
[201866.888899]  prune_dcache_sb+0x44/0x58
[201866.888904]  super_cache_scan+0xcc/0x160
[201866.888917]  do_shrink_slab+0x188/0x320
[201866.888918]  shrink_slab+0x1f8/0x2b0
[201866.888920]  shrink_node+0xb4/0x480
[201866.888921]  kswapd+0x3dc/0x760
[201866.888934]  kthread+0x128/0x130
[201866.888940]  ret_from_fork+0x10/0x18
[201866.888941] ---[ end trace 8c73588fb79b6328 ]---
```

warning出现前：

```bash
grep 'this file format is not right , so can not luopan ! ' dmesg.202312111431 -inr --color | wc -l
364
```

`pc : dentry_free+0x24/0xa8`总共出现了146次，`CPU 2`、`CPU 6` 调度过内核线程`PID: 67 Comm: kswapd0`。

kswapd 是一个系统启动后一直运行的内核线程，它在系统运行期间监视内存使用情况，并根据需要执行内存回收和页面交换等操作。

### oops

```c
[201866.936912] Unable to handle kernel NULL pointer dereference at virtual address 00000000000000d0
[201866.939427] Mem abort info:
[201866.940177]   ESR = 0x96000005
[201866.941018]   Exception class = DABT (current EL), IL = 32 bits
[201866.942561]   SET = 0, FnV = 0
[201866.943377]   EA = 0, S1PTW = 0
[201866.944163] Data abort info:
[201866.944995]   ISV = 0, ISS = 0x00000005
[201866.946019]   CM = 0, WnR = 0
[201866.946820] user pgtable: 4k pages, 48-bit VAs, pgdp = 000000009bba10c9
[201866.948553] [00000000000000d0] pgd=000000014eba4003, pud=0000000000000000
[201866.950597] Internal error: Oops: 96000005 [#1] SMP
[201866.951888] Modules linked in: tun uvcvideo videobuf2_vmalloc videobuf2_memops videobuf2_v4l2 videobuf2_common videodev media arc4 md4 sha512_generic sha512_arm64 nls_utf8 cifs(O) ccm dns_resolver uinput nfnetlink_queue nfnetlink_log nfnetlink fuse dlp_fcore(O) bnep st bluetooth ecdh_generic rfkill vfs_monitor(O) clink_vhci_hcd(O) clink_usbip_core(O) aes_ce_blk crypto_simd cryptd nls_iso8859_1 aes_ce_cipher crct10dif_ce nls_cp437 ghash_ce aes_arm64 sha2_ce sha256_arm64 sha1_ce hid_generic usbkbd usbhid snd_intel8x0 virtio_balloon snd_ac97_codec qemu_fw_cfg uos_resources(O) uos_bluetooth_connection_control(O) rdma_cm iw_cm ib_cm ib_core efivarfs ip_tables x_tables btrfs xor raid6_pq virtio_blk virtio_net net_failover failover button virtio_mmio
[201866.971480] Process Isolated Web Co (pid: 29060, stack limit = 0x0000000033ac522b)
[201866.971484] CPU: 6 PID: 29060 Comm: Isolated Web Co Kdump: loaded Tainted: G        W  O      4.19.0-arm64-desktop #5312
[201866.971485] Hardware name: RDO OpenStack Compute, BIOS 0.0.0 02/06/2015
[201866.971487] pstate: a0400005 (NzCv daif +PAN -UAO)
[201866.971499] pc : kmem_cache_free+0xfc/0x1c8
[201866.971518] lr : ext4_i_callback+0x18/0x20
[201866.971519] sp : ffff8003ffec1640
[201866.971520] x29: ffff8003ffec1640 x28: 000000000000000a 
[201866.971522] x27: ffff8003ffec1700 x26: ffff000009809810 
[201866.971523] x25: 00008003f6a05000 x24: ffff0000098096d8 
[201866.971525] x23: ffff0000094ac018 x22: ffff000009825680 
[201866.971526] x21: ffff8003dd4a9980 x20: ffff000008383800 
[201866.971527] x19: ffff8000791ac380 x18: 0000000000000000 
[201866.971529] x17: 0000000000000000 x16: 0000000000000000 
[201866.971530] x15: 0000000000000000 x14: 0000000000000000 
[201866.971531] x13: ffff000008c17740 x12: ffff000008c17748 
[201866.971533] x11: 0000000000000092 x10: 0000000000693e94 
[201866.971534] x9 : 0000000000000004 x8 : 00000003fbd70000 
[201866.971536] x7 : 0000000000000018 x6 : ffff000009984d20 
[201866.971537] x5 : ffff000009984d20 x4 : 00000000f0000000 
[201866.971538] x3 : ffff7e0001e46c00 x2 : ffffffffffffffff 
[201866.971539] x1 : 0000000000000000 x0 : 0000000000000000 
[201866.971541] Call trace:
[201866.971544]  kmem_cache_free+0xfc/0x1c8
[201866.971546]  ext4_i_callback+0x18/0x20
[201866.971561]  rcu_process_callbacks+0x2d8/0x500
[201866.971563]  __do_softirq+0x110/0x2e8
[201866.971573]  irq_exit+0x9c/0xb8
[201866.971578]  __handle_domain_irq+0x64/0xb8
[201866.971580]  gic_handle_irq+0x7c/0x178
[201866.971583]  el1_irq+0xb0/0x140
[201867.017634]  clear_page+0x10/0x24
[201867.017644]  clear_subpage+0x3c/0x58
[201867.019834]  clear_huge_page+0x80/0x218
[201867.021046]  do_huge_pmd_anonymous_page+0x180/0x770
[201867.022532]  __handle_mm_fault+0x788/0x908
[201867.023764]  handle_mm_fault+0xec/0x1b0
[201867.024943]  do_page_fault+0x164/0x480
[201867.026089]  do_translation_fault+0x58/0x60
[201867.027359]  do_mem_abort+0x3c/0xd0
[201867.028442]  el0_da+0x20/0x24
[201867.029366] Code: 9a820000 f9400c00 eb0002bf 54fff980 (f9406801) 
```

## 初步定位

```c
./scripts/faddr2line vmlinux kmem_cache_free+0xfc/0x1c8
kmem_cache_free+0xfc/0x1c8:
slab_equal_or_root 于 mm/slab.h:229
(已内连入)cache_from_obj 于 mm/slab.h:375
(已内连入)kmem_cache_free 于 mm/slub.c:2966
```

```bash
objdump -d -l -S mm/slub.o > slub.objdump
```

```c
 5246 slab_equal_or_root(): // vim slub.objdump +5246
 5247 /data3/home/yuanqiliang/code/arm-kernel/mm/slab.h:229
 5248     return p == s || p == s->memcg_params.root_cache;
 5249     238c:   eb0002bf    cmp x21, x0
 5250     2390:   54fff980    b.eq    22c0 <kmem_cache_free+0x28>  // b.none
 5251     2394:   f9406801    ldr x1, [x0, #208] // 将一个 64 位的寄存器（x1）的值设置为从地址为 x0 加上 208 字节的内存位置读取的值。
 5252     2398:   eb0102bf    cmp x21, x1 
 5253     239c:   540004e0    b.eq    2438 <kmem_cache_free+0x1a0>  // b.none
```

```bash
p ((struct kmem_cache *)0)->memcg_params
Cannot access memory at address 0xd0
```

```bash
p &((struct kmem_cache *)0)->memcg_params
$1 = (struct memcg_cache_params *) 0xd0
```

## 源码

```bash
git clone "http://gerrit.uniontech.com/kernel/arm-kernel"
```

```bash
git log --oneline | head -n1
b3ea87eb1d45 elfverify: stricter security check for executable section
```

```c
79 /*  
 80  * Slab cache management.          
 81  */ 
 82 struct kmem_cache { // vim include/linux/slub_def.h +112
 83     struct kmem_cache_cpu __percpu *cpu_slab;  
 84     /* Used for retriving partial slabs etc */ 
 85     slab_flags_t flags;         
 86     unsigned long min_partial;  
 87     unsigned int size;  /* The size of an object including meta data */    
 88     unsigned int object_size;/* The size of an object without meta data */ 
 89     unsigned int offset;    /* Free pointer offset. */        
 90 #ifdef CONFIG_SLUB_CPU_PARTIAL  
 91     /* Number of per cpu partial objects to keep around */    
 92     unsigned int cpu_partial;   
 93 #endif             
 94     struct kmem_cache_order_objects oo;        
 95     
 96     /* Allocation and freeing of slabs */      
 97     struct kmem_cache_order_objects max;       
 98     struct kmem_cache_order_objects min;       
 99     gfp_t allocflags;   /* gfp flags to use on each alloc */  
100     int refcount;       /* Refcount for slab cache destroy */ 
101     void (*ctor)(void *);       
102     unsigned int inuse;     /* Offset to metadata */          
103     unsigned int align;     /* Alignment */    
104     unsigned int red_left_pad;  /* Left redzone padding size */            
105     const char *name;   /* Name (only for display!) */        
106     struct list_head list;  /* List of slab caches */         
107 #ifdef CONFIG_SYSFS
108     struct kobject kobj;    /* For sysfs */    
109     struct work_struct kobj_remove_work;       
110 #endif             
111 #ifdef CONFIG_MEMCG
112     struct memcg_cache_params memcg_params;   
```

```c
./scripts/faddr2line vmlinux ext4_i_callback+0x18/0x20
ext4_i_callback+0x18/0x20:
ext4_i_callback 于 fs/ext4/super.c:1114
```

```c
1110 static void ext4_i_callback(struct rcu_head *head) // vim fs/ext4/super.c +1114
1111 {
1112     struct inode *inode = container_of(head, struct inode, i_rcu);
1113     kmem_cache_free(ext4_inode_cachep, EXT4_I(inode));        
1114 } 
```

```c
21 #define EXT4_I(inode) (container_of(inode, struct ext4_inode_info, vfs_inode))   // vim include/trace/events/ext4.h +21
```

在 Ext4 文件系统中，每个文件或目录对应于一个唯一的 inode。
每个 inode 在内核中都有一个对应的 struct ext4_inode_info 结构体。

```c
940 /*
 941  * fourth extended file system inode data in memory
 942  */                                                                                                                                                                  
 943 struct ext4_inode_info {   // vim fs/ext4/ext4.h +1010
 944     __le32  i_data[15]; /* unconverted */
 945     __u32   i_dtime;
 946     ext4_fsblk_t    i_file_acl;
......
 1001     /*
1002      * i_mmap_sem is for serializing page faults with truncate / punch hole
1003      * operations. We have to make sure that new page cannot be faulted in
1004      * a section of the inode that is being punched. We cannot easily use
1005      * i_data_sem for this since we need protection for the whole punch
1006      * operation and i_data_sem ranks below transaction start so we have
1007      * to occasionally drop it.
1008      */
1009     struct rw_semaphore i_mmap_sem;
1010     struct inode vfs_inode;
1011     struct jbd2_inode *jinode;
```

```c
2964 void kmem_cache_free(struct kmem_cache *s, void *x)    // vim mm/slub.c +2966
2965 {
2966     s = cache_from_obj(s, x);
2967     if (!s)
2968         return;
2969     slab_free(s, virt_to_head_page(x), x, NULL, 1, _RET_IP_);
2970     trace_kmem_cache_free(_RET_IP_, x);
2971 }
2972 EXPORT_SYMBOL(kmem_cache_free);
```

```c
357 static inline struct kmem_cache *cache_from_obj(struct kmem_cache *s, void *x)  // vim mm/slab.h +375
358 {
359     struct kmem_cache *cachep;
360     struct page *page;
361 
362     /*
363      * When kmemcg is not being used, both assignments should return the
364      * same value. but we don't want to pay the assignment price in that
365      * case. If it is not compiled in, the compiler should be smart enough
366      * to not do even the assignment. In that case, slab_equal_or_root
367      * will also be a constant.
368      */
369     if (!memcg_kmem_enabled() &&
370         !unlikely(s->flags & SLAB_CONSISTENCY_CHECKS))
371         return s;
372 
373     page = virt_to_head_page(x);    // 真正出错在这一行
374     cachep = page->slab_cache;  // page->slab_cache指向了nullptr，mm/slab.h +229解引用就oops了
375     if (slab_equal_or_root(cachep, s))
376         return cachep;
377 
378     pr_err("%s: Wrong slab cache. %s but object is from %s\n",
379            __func__, s->name, cachep->name);
380     WARN_ON_ONCE(1);
381     return s;       
382 }
```

```c
 651 static inline struct page *virt_to_head_page(const void *x)    // vim include/linux/mm.h +651
 652 {
 653     struct page *page = virt_to_page(x);
 654 
 655     return compound_head(page);
 656 }
```

```c
73 #define virt_to_page(kaddr) pfn_to_page(__pa(kaddr) >> PAGE_SHIFT) // vim include/asm/mmzone.h +73
```

```c
108 # define pfn_to_page(pfn)   (vmem_map + (pfn))  // vim include/asm/page.h +108
```

```c
226 static inline bool slab_equal_or_root(struct kmem_cache *s, // vim mm/slab.h +229
227                       struct kmem_cache *p)
228 {
229     return p == s || p == s->memcg_params.root_cache;                                                                                                                 
230 }
```

问题基本确定了，这是一个double free的问题。

`ext4_i_callback` 函数是 inode_operations 结构中的一个回调函数，定义在 fs/ext4/inode.c 文件中。这个函数在 inode 被释放时被调用，用于执行一些清理和处理工作。

如果多个线程同时尝试删除同一个文件，而删除文件的过程中会触发 ext4_i_callback，那么由于竞态条件，可能导致 double free 错误。

ext4_inode_info是ext4文件系统挂载的时候生成的，不掉盘的话ext4_inode_info对应的kmem_cache不可能指向null。

这里给出bpftrace参考语句：

```bash
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_unlink { printf("%s deleted by command '%s' with pid %d\n", str(args->pathname), comm, pid);} tracepoint:syscalls:sys_enter_unlinkat { printf("%s deleted by command '%s' with pid %d\n", str(args->pathname), comm, pid);}'
```

- [谁删了我的文件？Linux下用bpftrace轻松抓到元凶](https://zhuanlan.zhihu.com/p/191942583)

### 尝试修复

- [[RFC,V2] ext4: increase the protection of drop nlink and ext4 inode destroy](https://patchwork.ozlabs.org/project/linux-ext4/patch/1482994539-48559-1-git-send-email-yi.zhang@huawei.com/#1545987)

### ext4日志

- [Linux挂载ext4根文件系统为journal模式](https://blog.csdn.net/SweeNeil/article/details/88948646)

    grub中增加参数：

    ```bash
    rootflags=data=journal
    ```

## crash

```bash
crash /lib/debug/vmlinux-4.19.0-arm64-desktop-tyy-5312 /home/uos/202312111431/dump.202312111431
```

### ps

```bash
crash> ps
      PID    PPID  CPU       TASK        ST  %MEM      VSZ      RSS  COMM
>       0       0   0  ffff0000098131c0  RU   0.0        0        0  [swapper/0]
        0       0   1  ffff8003e85a2700  RU   0.0        0        0  [swapper/1]
        0       0   2  ffff8003e85a3400  RU   0.0        0        0  [swapper/2]
>       0       0   3  ffff8003e85a4100  RU   0.0        0        0  [swapper/3]
        0       0   4  ffff8003e85a4e00  RU   0.0        0        0  [swapper/4]
        0       0   5  ffff8003e85a5b00  RU   0.0        0        0  [swapper/5]
        0       0   6  ffff8003e85a6800  RU   0.0        0        0  [swapper/6]
>       0       0   7  ffff8003e85d8000  RU   0.0        0        0  [swapper/7]
>     403      -1   2  ffff8003e80b4100  RU   0.6   214476   111464  systemd-journal
>     961      -1   1  ffff800156d0db00  RU   0.2    44032    29004  clink-iotop
>    1004      -1   5  ffff80006cf93400  RU   0.0     6620     3792  smartctl
>    5310      -1   4  ffff8000306e2700  RU   1.1  4304016   190780  deepin-voice-no
>   29060      -1   6  ffff800035462700  RU   3.6  3060304   633720  Isolated Web Co
```

Firefox中的进程Isolated Web Co占用内存过多，且还在请求巨型匿名页面。

- VSZ（Virtual Set Size）：3060304KB/1024 ≈ 2984.75MB

  - VSZ 表示进程的虚拟内存大小，即分配给进程的总虚拟内存空间。

  - 它包括进程使用的所有内存区域，包括实际物理内存、交换空间和映射文件等。

  - VSZ 的值并不反映实际的物理内存占用情况，而是显示了进程可以访问的所有虚拟地址空间的总和。

- RSS（Resident Set Size）：633720KB/1024 ≈ 618.75MB

  - RSS 表示进程的驻留集大小，即实际占用的物理内存大小。

  - 它是进程当前正在使用的物理内存量，不包括交换空间或映射文件等。

  - RSS 提供了一个更准确的物理内存使用指标，反映了进程实际占用的系统内存。

### bt

```bash
crash> bt -l                                                           
PID: 29060    TASK: ffff800035462700  CPU: 6    COMMAND: "Isolated Web Co"
 #0 [ffff8003ffec1290] crash_kexec at ffff000008170728                 
    /data3/home/yuanqiliang/code/arm-kernel/kernel/kexec_core.c: 977   
 #1 [ffff8003ffec12c0] die at ffff00000808d7f4                         
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/kernel/traps.c: 202 
 #2 [ffff8003ffec1300] die_kernel_fault at ffff0000080a0fb4
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 258     
 #3 [ffff8003ffec1330] __do_kernel_fault at ffff0000080a104c
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 286    
 #4 [ffff8003ffec1360] do_page_fault at ffff0000080a1284  
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 588
 #5 [ffff8003ffec1440] do_translation_fault at ffff000008aec17c      
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 597
 #6 [ffff8003ffec1450] do_mem_abort at ffff000008081250     
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 728
 #7 [ffff8003ffec1630] el1_ia at ffff0000080830cc         
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/kernel/entry.S: 562
     PC: ffff000008268b5c  [kmem_cache_free+252]        
     LR: ffff000008383800  [ext4_i_callback+24]                       
     SP: ffff8003ffec1640  PSTATE: a0400005                    
    X29: ffff8003ffec1640  X28: 000000000000000a  X27: ffff8003ffec1700
    X26: ffff000009809810  X25: 00008003f6a05000  X24: ffff0000098096d8
    X23: ffff0000094ac018  X22: ffff000009825680  X21: ffff8003dd4a9980
    X20: ffff000008383800  X19: ffff8000791ac380  X18: 0000000000000000
    X17: 0000000000000000  X16: 0000000000000000  X15: 0000000000000000   
    X14: 0000000000000000  X13: ffff000008c17740  X12: ffff000008c17748
    X11: 0000000000000092  X10: 0000000000693e94   X9: 0000000000000004
     X8: 00000003fbd70000   X7: 0000000000000018   X6: ffff000009984d20
     X5: ffff000009984d20   X4: 00000000f0000000   X3: ffff7e0001e46c00
     X2: ffffffffffffffff   X1: 0000000000000000   X0: 0000000000000000
    /data3/home/yuanqiliang/code/arm-kernel/mm/slab.h: 229             
 #8 [ffff8003ffec1640] kmem_cache_free at ffff000008268b58             
    /data3/home/yuanqiliang/code/arm-kernel/mm/slab.h: 229             
 #9 [ffff8003ffec1670] ext4_i_callback at ffff0000083837fc             
    /data3/home/yuanqiliang/code/arm-kernel/fs/ext4/super.c: 1113      
#10 [ffff8003ffec1680] rcu_process_callbacks at ffff000008144134       
    /data3/home/yuanqiliang/code/arm-kernel/kernel/rcu/rcu.h: 236   
#11 [ffff8003ffec1720] __softirqentry_text_start at ffff000008081964
    /data3/home/yuanqiliang/code/arm-kernel/kernel/softirq.c: 292
#12 [ffff8003ffec17b0] irq_exit at ffff0000080dc420
    /data3/home/yuanqiliang/code/arm-kernel/./include/linux/interrupt.h: 504
#13 [ffff8003ffec17c0] __handle_domain_irq at ffff000008131a70
    /data3/home/yuanqiliang/code/arm-kernel/kernel/irq/irqdesc.c: 685
#14 [ffff8003ffec1800] gic_handle_irq at ffff0000080815e0
    /data3/home/yuanqiliang/code/arm-kernel/./include/linux/irqdesc.h: 173
--- <IRQ stack> ---
#15 [ffff8001dd367b30] el1_irq at ffff00000808332c
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/kernel/entry.S: 611
     PC: ffff000008acb250  [clear_page+16]
     LR: ffff0000080a1cc4  [__cpu_clear_user_page+12]
     SP: ffff8001dd367b40  PSTATE: 80400005
    X29: ffff8001dd367b40  X28: ffff8003df6234e0  X27: 0000000000000002
    X26: 0000000000000000  X25: 0000000000000000  X24: 0000000000000000
    X23: ffff7e0002310000  X22: 0000000000001000  X21: 0000ffff7c600000
    X20: 0000000000000000  X19: ffff800035462700  X18: 0000000000000000
    X17: 0000000000000000  X16: 0000000000000000  X15: 0000000000000400
    X14: 0000000000000400  X13: 00000000000003f9  X12: 0000000000000001
    X11: 0000000000000001  X10: 00000000000008e0   X9: ffff8001dd367ab0
     X8: ffff800035463040   X7: ffff8003ffeb17b8   X6: ffff8003e1a3f900
     X5: 000000000000a7f6   X4: ffff000009809000   X3: 0000000000004600
     X2: 0000000000000004   X1: 0000000000000040   X0: ffff80008c518000
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/lib/clear_page.S: 23
#16 [ffff8001dd367b40] clear_page at ffff000008acb24c
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/lib/clear_page.S: 21
#17 [ffff8001dd367b50] clear_subpage at ffff00000823a910
    /data3/home/yuanqiliang/code/arm-kernel/./include/linux/highmem.h: 137
#18 [ffff8001dd367b70] clear_huge_page at ffff0000082410bc
    /data3/home/yuanqiliang/code/arm-kernel/mm/memory.c: 4679
#19 [ffff8001dd367bd0] do_huge_pmd_anonymous_page at ffff00000826e964
    /data3/home/yuanqiliang/code/arm-kernel/mm/huge_memory.c: 578
#20 [ffff8001dd367c30] __handle_mm_fault at ffff000008240994
    /data3/home/yuanqiliang/code/arm-kernel/mm/memory.c: 3942
#21 [ffff8001dd367cf0] handle_mm_fault at ffff000008240c00
    /data3/home/yuanqiliang/code/arm-kernel/mm/memory.c: 4212
#22 [ffff8001dd367d20] do_page_fault at ffff0000080a1208
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 399
#23 [ffff8001dd367e00] do_translation_fault at ffff000008aec17c
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 597
#24 [ffff8001dd367e10] do_mem_abort at ffff000008081250
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/mm/fault.c: 728
#25 [ffff8001dd367ff0] el0_da at ffff0000080839cc
    /data3/home/yuanqiliang/code/arm-kernel/arch/arm64/kernel/entry.S: 724
     PC: 0000ffff990e9548   LR: 0000aaaac18625f8   SP: 0000ffffe16bba40
    X29: 0000ffffe16bd560  X28: 0000000000000000  X27: 0000ffff66a29318
    X26: 0000000000000001  X25: 0000000000000001  X24: 0000ffff726478d0
    X23: 0000ffff7c600000  X22: 0000000000200000  X21: 0000000000100000
    X20: 0000ffff98e00000  X19: 0000ffff80000000  X18: 0000000000000000
    X17: 0000ffff990e9450  X16: 0000aaaac190c020  X15: 0000ffff80000000
    X14: 0000000000000000  X13: ff000178005d4748  X12: 0000000000000000
    X11: 0000000004d00000  X10: 00000000039e8000   X9: 0aa90b643fdb4900
     X8: 0aa90b643fdb4900   X7: 3133303131333230   X6: 32000000110000ff
     X5: 0000ffff7c700000   X4: 0000ffff80100000   X3: 0000ffff7c600000
     X2: 0000000000100000   X1: 0000ffff80000000   X0: 0000ffff7c600000
    ORIG_X0: 0000ffff7c800000  SYSCALLNO: ffffffff  PSTATE: 20001000
```

- [Firefox: "isolate+" process CPU usage 100%](https://askubuntu.com/questions/1466241/firefox-isolate-process-cpu-usage-100)

### mount

```bash
crash> mount
     MOUNT           SUPERBLK     TYPE   DEVNAME   DIRNAME
ffff8003e80a4000 ffff8003e80b7000 rootfs rootfs    /
ffff80039444e000 ffff800394450000 sysfs  sysfs     /sys
ffff8003e80a4c00 ffff8003e80da800 proc   proc      /proc
ffff8003944a0000 ffff8003e7ce0000 devtmpfs udev    /dev
ffff8003e80a4d80 ffff800394b2c000 devpts devpts    /dev/pts
ffff80039452c000 ffff800394538000 tmpfs  tmpfs     /run
ffff80039452c180 ffff80039453d000 ext4   /dev/vda2 /
ffff80039452c300 ffff8003e80db800 securityfs securityfs /sys/kernel/security
ffff80039452c600 ffff80039453f000 tmpfs  tmpfs     /dev/shm
ffff80039452c480 ffff80039453f800 tmpfs  tmpfs     /run/lock
ffff80039452c780 ffff80039453c800 tmpfs  tmpfs     /sys/fs/cgroup
ffff80039452c900 ffff800393f75800 cgroup2 cgroup2  /sys/fs/cgroup/unified
ffff80039452ca80 ffff800393f74800 cgroup cgroup    /sys/fs/cgroup/systemd
ffff80039452cc00 ffff800393f73800 pstore pstore    /sys/fs/pstore
ffff80039452cd80 ffff800393f75000 efivarfs efivarfs /sys/firmware/efi/efivars
ffff80039452cf00 ffff800393f70800 bpf    bpf       /sys/fs/bpf
ffff80039452d080 ffff800393f71800 cgroup cgroup    /sys/fs/cgroup/net_cls,net_prio
ffff80039452d200 ffff800393f70000 cgroup cgroup    /sys/fs/cgroup/cpu,cpuacct
ffff80039452d380 ffff800393f77800 cgroup cgroup    /sys/fs/cgroup/blkio
ffff80039452d500 ffff800393f74000 cgroup cgroup    /sys/fs/cgroup/cpuset
ffff80039452d680 ffff800393f72800 cgroup cgroup    /sys/fs/cgroup/hugetlb
ffff80039452d800 ffff800393f77000 cgroup cgroup    /sys/fs/cgroup/memory
ffff80039452d980 ffff800393f76800 cgroup cgroup    /sys/fs/cgroup/freezer
ffff80039452db00 ffff800393f76000 cgroup cgroup    /sys/fs/cgroup/pids
ffff80039452dc80 ffff800393f73000 cgroup cgroup    /sys/fs/cgroup/devices
ffff80039452de00 ffff800393f72000 cgroup cgroup    /sys/fs/cgroup/rdma
ffff8003dcf82000 ffff8003935f9800 autofs systemd-1 /proc/sys/fs/binfmt_misc
ffff8003944a0180 ffff8003dd0c0800 mqueue mqueue    /dev/mqueue
ffff8003dcf82180 ffff8003e80dc000 debugfs debugfs  /sys/kernel/debug
ffff8003e7cc8d80 ffff8003931e9800 hugetlbfs hugetlbfs /dev/hugepages
ffff8003e16d2000 ffff8003dd0c1800 configfs configfs /sys/kernel/config
ffff8003dd0b4600 ffff8003947da000 squashfs /dev/loop0 /snap/core22/586
ffff8003e16d2180 ffff8003946b5800 squashfs /dev/loop1 /snap/core/15515
ffff8003e7cc8f00 ffff800393a2e800 squashfs /dev/loop2 /snap/core/16204
ffff8003944a0300 ffff800393a96000 squashfs /dev/loop3 /snap/lxd/24646
ffff8003e16d2900 ffff8003946b3000 squashfs /dev/loop4 /snap/core/15425
ffff8003944a0600 ffff8003df4c0800 vfat   /dev/vda1 /boot/efi
ffff8003944f3c80 ffff8003944bb000 binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc/
ffff8003dd0b4a80 ffff800393a80800 ext4   /dev/vdb  /mnt/vdb
ffff8003e090f800 ffff800394538000 tmpfs  tmpfs     /run/snapd/ns
ffff8003dfb55e00 ffff8003e80da000 nsfs   nsfs      /run/snapd/ns/snapd/ns/lxd.mnt
ffff8003e80a4f00 ffff8003e04b1800 tmpfs  tmpfs     /run/user/1000
ffff8003a4613080 ffff80037a40b800 fusectl fusectl  /sys/fs/fuse/connections
ffff8003a790e900 ffff8003e04b6800 fuse   gvfsd-fuse /run/user/1000/gvfs
ffff8002c2e08f00 ffff80012834c000 cifs   //127.0.0.1/WANGZHIQUAN-PC C /media/WANGZHIQUAN-PC C     
```

### swap

```bash
crash> swap
SWAP_INFO_STRUCT    TYPE       SIZE       USED     PCT  PRI  FILENAME
```

### kmem

```bash
crash> kmem -i
                 PAGES        TOTAL      PERCENTAGE
    TOTAL MEM  3967057      15.1 GB         ----
         FREE    28799     112.5 MB    0% of TOTAL MEM
         USED  3938258        15 GB   99% of TOTAL MEM
       SHARED   743953       2.8 GB   18% of TOTAL MEM
      BUFFERS   102855     401.8 MB    2% of TOTAL MEM
       CACHED  1387497       5.3 GB   34% of TOTAL MEM  # dmesg中可以看到下载了很多文件，占用很多缓存
         SLAB   199579     779.6 MB    5% of TOTAL MEM  # dmesg中可以看到下载了很多文件，占用很多缓存

   TOTAL HUGE        0            0         ----
    HUGE FREE        0            0    0% of TOTAL HUGE

   TOTAL SWAP        0            0         ----
    SWAP USED        0            0    0% of TOTAL SWAP
    SWAP FREE        0            0    0% of TOTAL SWAP

 COMMIT LIMIT  1983528       7.6 GB         ----
    COMMITTED  7399654      28.2 GB  373% of TOTAL LIMIT
```

这些信息反映了系统当前的内存使用情况，包括空闲内存、已用内存、缓存等。请注意，COMMITTED 显示的内存值高于 COMMIT LIMIT，这可能表明系统当前存在一些超过物理内存和交换空间总和的内存压力。在这种情况下，系统可能会开始使用 OOM（Out of Memory）策略，例如终止某些进程来释放内存。

在 crash 输出中，COMMIT LIMIT 和 COMMITTED 是有关系统内存提交（commit）的两个关键指标。

COMMIT LIMIT:

COMMIT LIMIT 表示系统在当前配置下允许的最大内存提交限制，即系统允许分配给进程的虚拟内存和交换空间的总和。

在这个例子中，COMMIT LIMIT 为 7.6 GB，表示系统限制了进程可以使用的总虚拟内存和交换空间的量。

COMMITTED:

COMMITTED 表示当前已经由系统分配和使用的虚拟内存和交换空间的总和。

在这个例子中，COMMITTED 为 28.2 GB，表示系统当前已经分配并使用的总虚拟内存和交换空间的量。

分析：

COMMITTED 大于 COMMIT LIMIT，这表明系统当前分配的虚拟内存和交换空间总量已经超过了系统的限制。

当 COMMITTED 接近或超过 COMMIT LIMIT 时，系统可能会面临内存压力，并且可能会采取 OOM（Out of Memory）策略，例如终止某些进程以释放内存。

这种情况下，你可能需要进一步分析系统的进程、内存使用情况以及是否存在内存泄漏等问题，以找出为何 COMMITTED 超过了 COMMIT LIMIT 的原因。

### 资源耗尽导致slub回收崩溃

SLUB回收过程需要分配和释放内存对象，以及对内存数据结构进行操作。如果系统内存已经耗尽，无法满足回收过程的内存需求，就会导致回收过程失败或异常。

此外，SLUB回收可能还涉及到其他系统资源，例如锁、CPU时间等。如果这些资源也耗尽，可能会导致回收过程无法继续进行，从而引发崩溃。

在进行SLUB回收压力测试时，如果没有足够的资源可用，例如内存资源不足，可能会导致回收过程无法正常进行，从而出现崩溃或异常行为。

为了预防资源耗尽引发SLUB回收崩溃，可以采取以下措施：

- 资源评估和监控：在进行压力测试之前，评估系统资源的可用性，并监控资源的使用情况。确保有足够的可用资源来执行回收过程。

- 调整资源限制：根据压力测试的需求，可能需要调整系统的资源限制，例如增加可用内存大小、调整锁的数量或调度策略等，以确保回收过程有足够的资源可用。

- 优化回收策略：对SLUB回收策略进行优化，例如调整回收频率、调整回收算法等，以减少对资源的需求或提高资源利用效率。

- 合理设计测试用例：设计合理的测试用例，确保测试过程中的资源使用情况在可控范围内，并避免过度消耗系统资源的操作。

### overcommit_memory

```bash
cat /proc/sys/vm/overcommit_memory
```

输出的值有以下几种可能：

- 0  – Heuristic overcommit handling. 这是缺省值，它允许overcommit，但过于明目张胆的overcommit会被拒绝，比如malloc一次性申请的内存大小就超过了系统总内存。Heuristic的意思是“试  探式的”，内核利用某种算法猜测你的内存申请是否合理，它认为不合理就会拒绝overcommit。
- 1  – Always overcommit. 允许overcommit，对内存申请来者不拒。内核执行无内存过量使用处理。使用这个设置会增大内存超载的可能性，但也可以增强大量使用内存任务的性能。
- 2  – Don’t overcommit. 禁止overcommit。 内存拒绝等于或者大于总可用 swap 大小以及overcommit_ratio 指定的物理 RAM 比例的内存请求。如果希望减小内存过度使用的风险，这个设置就是最好的。

#### overcommit_ratio

overcommit_ratio、overcommit_kbytes在overcommit_memory=2时才有用，overcommit_kbytes非0时，overcommit_ratio无效。

```bash
cat /proc/sys/vm/overcommit_ratio
```

#### overcommit_kbytes

```bash
cat /proc/sys/vm/overcommit_kbytes
```

#### 当前机器overcommit_memory配置

```bash
cat /proc/sys/vm/overcommit_memory
0
cat /proc/sys/vm/overcommit_ratio
50
cat /proc/sys/vm/overcommit_kbytes
0
```

单次申请的内存大小不能超过以下值，否则本次申请就会失败。

free memory + free swap + pagecache的大小 + SLAB

Linux对大部分申请内存的请求都回复"yes"，以便能跑更多更大的程序。因为申请内存后，并不会马上使用内存。这种技术叫做Overcommit。当linux发现内存不足时，会发生OOM killer(OOM=out-of-memory)。它会选择杀死一些进程(用户态进程，不是内核线程)，以便释放内存。

当oom-killer发生时，linux会选择杀死哪些进程？选择进程的函数是oom_badness函数(在mm/oom_kill.c中)，该函数会计算每个进程的点数(0~1000)。点数越高，这个进程越有可能被杀死。每个进程的点数跟oom_score_adj有关，而且oom_score_adj可以被设置(-1000最低，1000最高)。

### min_free_kbytes

```bash
cat /proc/sys/vm/min_free_kbytes
67584
```

通常情况下 WMARK_LOW 的值是 WMARK_MIN 的 1.25 倍，WMARK_HIGH 的值是 WMARK_LOW 的 1.5 倍。而 WMARK_MIN 的数值就是由这个内核参数 min_free_kbytes 来决定的。

WMARK_MIN = 67584KB / 1024 = 66MB

WMARK_LOW = 66MB * 1.25 = 82.5MB

WMARK_HIGH = 82.5MB * 1.5 = 123.75MB

当前free memory等于112.5MB，在 WMARK_LOW 与 WMARK_HIGH 之间，内存在正常范围内，内存回收kswpd、内存规整kcompactd被正常的周期性调度，故不会触发 OOM。

当可用物理内存低于 WMARK_MIN 会触发下方操作：

- 直接内存回收
- 直接内存规整
- 产生 OOM

内核也不会直接开始 OOM，而是进入到重试流程，在重试流程开始之前内核需要调用 should_reclaim_retry 判断是否应该进行重试，重试标准：

如果内核已经重试了 MAX_RECLAIM_RETRIES (16) 次仍然失败，则放弃重试执行后续 OOM。

如果内核将所有可选内存区域中的所有可回收页面全部回收之后，仍然无法满足内存的分配，那么放弃重试执行后续 OOM。

![__alloc_pages](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/__alloc_pages.png)

- [深入理解 Linux 物理内存分配全链路实现](https://mp.weixin.qq.com/s/llZXDRG99NUXoMyIAf00ig)
- [Overcommitting Memory （过度使用内存）](https://blog.csdn.net/zyqash/article/details/122860393)
- [内存不足：OOM](https://zhangzhuo.ltd/articles/2021/08/10/1628565705959.html)
- [内存分配策略：overcommit_memory](https://blog.csdn.net/xsxb_yl/article/details/121412094)

## 初步结论

没有使用交换分区。

当前机器内核参数`kernel.panic = 0` `kernel.panic_on_oops = 1`。

内核发现内存不足时可以通过软中断触发内存回收kswapd、内存规整kcompactd被调度，kswapd会调用shrink_slab进行slab回收。

本次oops中是ext4_inode_info被删除了，也就是某个文件被删除了，导致ext4_i_callback回调函数被调用，ext4_i_callback会调用kmem_cache_free释放slab中的ext4_inode_info对象，发生了double free，导致了oops，进一步导致panic，应该是kernel.panic_on_oops=1导致kdump触发。

共计分析了4个kdump，`kmem -i`都存在内核耗尽，基本都只剩下100M了，每次oops的堆栈都跟回收slub缓存相关。

用户的使用场景应该是在不断下载文件并删除文件，同时在使用firefox、deepin-voice-note等。

### 疑点

某个第三方模块多个线程删除同一个文件时存在竞态。

某个第三方模块打开多个文件未关闭，导致cache占用很多，导致cache内存无法回收。

故接下来排查方向需要为啥为啥slub回收的时候double free。

ext4文件系统稳定性问题？

## 进一步排查

### kdump

在grub中调整crashkernel参数，看是否能获取完整的kdump：

```bash
crashkernel=2G-4G:320M,4G-32G:512M,32G-64G:1024M,64G-128G:2048M,128G-:4096M
```

### ext4 journal

可以到系统底层的宿主机上执行一下ps aux | grep -i qemu，我想看看ext4文件系统对应的磁盘是通过virtio or vfio 挂载上去的？

ext4_inode_info是ext4文件系统挂载的时候生成的，不掉盘的话ext4_inode_info对应的kmem_cache不可能指向null。

grub中更改ext4根文件系统日志为journal模式：

```bash
rootflags=data=journal
```

### slub_debug

打下slub_debug相关内核编译选项：

```text
CONFIG_SLUB=y

CONFIG_SLUB_DEBUG=y

CONFIG_SLUB_DEBUG_ON=y

CONFIG_SLUB_STATS=y
```

grub中增加slub_debug=UFPZ参数。

### hcache

使用hcache查看Buffer&Cache使用率高的进程：

```bash
hcache -top 50
```

### bpftrace

bpftrace追踪ext4磁盘稳定性问题。

### 合入patch

- [[RFC,V2] ext4: increase the protection of drop nlink and ext4 inode destroy](https://patchwork.ozlabs.org/project/linux-ext4/patch/1482994539-48559-1-git-send-email-yi.zhang@huawei.com/#1545987)

## 临时优化方案

启用swap分区：

```bash
sudo dd if=/dev/zero of=/tmp/swap bs=1M count=32768
sudo chmod 600 /tmp/swap
sudo mkswap /tmp/swap
sudo swapon /tmp/swap
```

```bash
sudo cat << EOF >> /etc/sysctl.conf
vm.dirty_background_ratio=3 # 当脏页面占系统内存的3%时触发后台写回
vm.dirty_ratio=80 # 当脏页面占系统内存的80%时停止用户进程，开始写回
vm.dirty_writeback_centisecs=100 # 每100毫秒检查一次是否需要写回脏页面
vm.nr_hugepages=128 # 系统中的大页面数量设置为128
vm.overcommit_memory=2 # 启用严格的内存分配机制
vm.overcommit_ratio=90 # 允许系统内存超过物理内存大小的80%
net.ipv4.tcp_fastopen=3 # 启用 TCP Fast Open 并使用 Cookie 来防止连接污染攻击
EOF
```

```bash
sudo sysctl -p
```
