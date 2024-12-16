---
date: 2024/07/26 18:35:29
updated: 2024/07/26 18:35:29
---

# fix kmemleak on get_cpu_name

## kmemleak

```bash
unreferenced object 0xffff8003955c6980 (size 128):
  comm "systemd", pid 1, jiffies 4294893016 (age 13460.512s)
  hex dump (first 32 bytes):
    76 69 72 74 2d 72 68 65 6c 37 2e 36 2e 30 00 00  virt-rhel7.6.0..
    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
  backtrace:
    [<00000000720211f3>] __kmalloc+0x154/0x2e0
    [<000000001b6361bf>] get_cpuname_by_dmi+0x9c/0xc8
    [<000000001f423e33>] dmi_decode_table+0xe8/0xf8
    [<00000000ea6aa39f>] dmi_walk+0x48/0x78
    [<000000005cd77eaf>] get_cpu_name+0x48/0x68
    [<0000000050013ad7>] os_run_evn_is_virt+0xc/0x30
    [<00000000620b1835>] c_show+0x240/0x420
    [<000000006759dfc8>] seq_read+0x140/0x448
    [<0000000004062a93>] proc_reg_read+0x60/0xd0
    [<00000000692249ad>] __vfs_read+0x20/0x158
    [<00000000c2fad215>] vfs_read+0x90/0x168
    [<0000000083dab445>] ksys_read+0x4c/0xb8
    [<0000000027155268>] __arm64_sys_read+0x18/0x20
    [<0000000081aeee94>] el0_svc_common+0x90/0x178
    [<000000002ed13754>] el0_svc_handler+0x9c/0xa8
    [<0000000078603995>] el0_svc+0x8/0xc
unreferenced object 0xffff8003955c6900 (size 128):
  comm "systemd", pid 1, jiffies 4294893016 (age 13460.512s)
  hex dump (first 32 bytes):
    76 69 72 74 2d 72 68 65 6c 37 2e 36 2e 30 00 00  virt-rhel7.6.0..
    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
  backtrace:
    [<00000000720211f3>] __kmalloc+0x154/0x2e0
    [<000000001b6361bf>] get_cpuname_by_dmi+0x9c/0xc8
    [<000000001f423e33>] dmi_decode_table+0xe8/0xf8
    [<00000000ea6aa39f>] dmi_walk+0x48/0x78
    [<000000005cd77eaf>] get_cpu_name+0x48/0x68
    [<0000000050013ad7>] os_run_evn_is_virt+0xc/0x30
    [<00000000620b1835>] c_show+0x240/0x420
    [<000000006759dfc8>] seq_read+0x140/0x448
    [<0000000004062a93>] proc_reg_read+0x60/0xd0
    [<00000000692249ad>] __vfs_read+0x20/0x158
    [<00000000c2fad215>] vfs_read+0x90/0x168
    [<0000000083dab445>] ksys_read+0x4c/0xb8
    [<0000000027155268>] __arm64_sys_read+0x18/0x20
    [<0000000081aeee94>] el0_svc_common+0x90/0x178
    [<000000002ed13754>] el0_svc_handler+0x9c/0xa8
    [<0000000078603995>] el0_svc+0x8/0xc
```

## 初始源码

```c
static char *cpu_name = NULL;
static bool get_cpu_flag = false;
static void get_cpuname_by_dmi(const struct dmi_header *dm, void *data)
{
	const char *bp;
	const u8 *nsp;
	char *sm ;
	char s;
	if(!dm)
		return;	
	if (dm->type != 4)
		return;

	bp = ((u8 *) dm) + dm->length;
	sm = (char *)dm;
	s = sm[0x10];

	if (s) {
		while (--s > 0 && *bp)
			bp += strlen(bp) + 1;

		/* Strings containing only spaces are considered empty */
		nsp = bp;
		while (*nsp == ' ')
			nsp++;
		if (*nsp != '\0'){
			cpu_name = dmi_alloc(strlen(bp) + 1);
			if (cpu_name != NULL)
				strcpy(cpu_name, bp);

			get_cpu_flag = true;
			return;
		}
	}

	return;
}

char * get_cpu_name(void)
{
	if(!get_cpu_flag)
		dmi_walk(get_cpuname_by_dmi, cpu_name);

	if(cpu_name == NULL) {
		/* Some BIOS don't support getting CPU name from DMI  */
/*		pr_info("Failed to get cpu name by dmi.\n"); */
		return "UNKNOWN-CPU";
	}

	return cpu_name;
}
EXPORT_SYMBOL(get_cpu_name);
```

第一眼看上述代码应该不存在内存泄露，每次都会判断get_cpu_flag是否为true，如果为true，则直接返回cpu_name，否则调用dmi_walk。

## trace-bpfcc

```bash
trace-bpfcc -tKU get_cpu_name
TIME     PID     TID     COMM            FUNC
5.135070 19084   19084   lshw            get_cpu_name
        get_cpu_name+0x0 [kernel]
        c_show+0x240 [kernel]
        seq_read+0x140 [kernel]
        proc_reg_read+0x60 [kernel]
        __vfs_read+0x20 [kernel]
        vfs_read+0x90 [kernel]
        ksys_read+0x4c [kernel]
        __arm64_sys_read+0x18 [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        __read+0x2c [libc-2.28.so]
        [unknown] [lshw]
        [unknown] [lshw]
        [unknown] [lshw]
        __libc_start_main+0xe4 [libc-2.28.so]
        [unknown] [lshw]

5.135083 19084   19084   lshw            get_cpu_name
        get_cpu_name+0x0 [kernel]
        seq_read+0x140 [kernel]
        proc_reg_read+0x60 [kernel]
        __vfs_read+0x20 [kernel]
        vfs_read+0x90 [kernel]
        ksys_read+0x4c [kernel]
        __arm64_sys_read+0x18 [kernel]
        el0_svc_common+0x90 [kernel]
        el0_svc_handler+0x9c [kernel]
        el0_svc+0x8 [kernel]
        __read+0x2c [libc-2.28.so]
        [unknown] [lshw]
        [unknown] [lshw]
        [unknown] [lshw]
        __libc_start_main+0xe4 [libc-2.28.so]
        [unknown] [lshw]
```

很明显这是一个线程安全问题，多个线程访问get_cpu_flag都为false时存在多次kmalloc的情况。

## 修复方案

```c
static char cpu_name[256];
static bool get_cpu_flag = false;
static DEFINE_SPINLOCK(cpu_name_lock);

static void get_cpuname_by_dmi(const struct dmi_header *dm, void *data)
{
	const char *bp;
	const u8 *nsp;
	char *sm ;
	char s;
	if(!dm)
		return;	
	if (dm->type != 4)
		return;

	bp = ((u8 *) dm) + dm->length;
	sm = (char *)dm;
	s = sm[0x10];

	if (s) {
		while (--s > 0 && *bp)
			bp += strlen(bp) + 1;

		/* Strings containing only spaces are considered empty */
		nsp = bp;
		while (*nsp == ' ')
			nsp++;
		if (*nsp != '\0') {
			spin_lock(&cpu_name_lock);
			if (!get_cpu_flag) {  // Check again inside the lock
				strcpy(cpu_name, bp);
				get_cpu_flag = true;
			}
			spin_unlock(&cpu_name_lock);
			return;
		}
	}

	return;
}

char * get_cpu_name(void)
{
	spin_lock(&cpu_name_lock);
	if(!get_cpu_flag) {
		spin_unlock(&cpu_name_lock);
		dmi_walk(get_cpuname_by_dmi, cpu_name);
	} else {
		spin_unlock(&cpu_name_lock);
	}

	if(cpu_name == NULL) {
		/* Some BIOS don't support getting CPU name from DMI  */
/*		pr_info("Failed to get cpu name by dmi.\n"); */
		return "UNKNOWN-CPU";
	}

	return cpu_name;
}
EXPORT_SYMBOL(get_cpu_name);
```

自旋锁上下文中不能休眠，但是原始代码中的dmi_alloc底层是kmalloc，内存承压过大会触发中断，故此处需要进一步细化。

方案一：

- 将dmi_alloc(size) 替换成 kmalloc(size, GFP_ATOMIC)
  - GFP_ATOMIC 是掩码 __GFP_HIGH，__GFP_ATOMIC，__GFP_KSWAPD_RECLAIM 的组合，表示内存分配行为必须是原子的，是高优先级的。在任何情况下都不允许睡眠，如果空闲内存不够，则会从紧急预留内存中分配。该标志适用于中断程序，以及持有自旋锁的进程上下文中。

方案二：

- 将cpu_name从静态字符指针变为静态字符数组，在 Linux 中，文件级别的静态字符数组通常位于程序的数据段（data segment）中。

### patch

修复前后代码差异对比：

```c
---
 arch/arm64/kernel/cpuinfo.c | 22 +++++++++++++++-------
 1 file changed, 15 insertions(+), 7 deletions(-)

diff --git a/arch/arm64/kernel/cpuinfo.c b/arch/arm64/kernel/cpuinfo.c
index 6c06c9034f5c..c8fdeb837adb 100644
--- a/arch/arm64/kernel/cpuinfo.c
+++ b/arch/arm64/kernel/cpuinfo.c
@@ -149,8 +149,10 @@ static char *midr_to_desc(u32 midr)
 	return "Arm";
 }
 
-static char *cpu_name = NULL;
+static char cpu_name[256];
 static bool get_cpu_flag = false;
+static DEFINE_SPINLOCK(cpu_name_lock);
+
 static void get_cpuname_by_dmi(const struct dmi_header *dm, void *data)
 {
 	const char *bp;
@@ -174,12 +176,13 @@ static void get_cpuname_by_dmi(const struct dmi_header *dm, void *data)
 		nsp = bp;
 		while (*nsp == ' ')
 			nsp++;
-		if (*nsp != '\0'){
-			cpu_name = dmi_alloc(strlen(bp) + 1);
-			if (cpu_name != NULL)
+		if (*nsp != '\0') {
+			spin_lock(&cpu_name_lock);
+			if (!get_cpu_flag) {  // Check again inside the lock
 				strcpy(cpu_name, bp);
-
-			get_cpu_flag = true;
+				get_cpu_flag = true;
+			}
+			spin_unlock(&cpu_name_lock);
 			return;
 		}
 	}
@@ -189,8 +192,13 @@ static void get_cpuname_by_dmi(const struct dmi_header *dm, void *data)
 
 char * get_cpu_name(void)
 {
-	if(!get_cpu_flag)
+	spin_lock(&cpu_name_lock);
+	if(!get_cpu_flag) {
+		spin_unlock(&cpu_name_lock);
 		dmi_walk(get_cpuname_by_dmi, cpu_name);
+	} else {
+		spin_unlock(&cpu_name_lock);
+	}
 
 	if(cpu_name == NULL) {
 		/* Some BIOS don't support getting CPU name from DMI  */
-- 
2.20.1
```

### 难点

check again 的好处在于确保在获取 CPU 名称时的并发情况下，多个线程或者进程同时调用 get_cpu_name 函数时能够正确地获取到 CPU 名称并避免竞态条件。

具体来说，在 get_cpu_name 函数中，首先会获取 cpu_name_lock 的自旋锁。然后会检查 get_cpu_flag 变量，如果它为假（即还未获取到 CPU 名称），则会解锁自旋锁，并尝试通过 dmi_walk 函数获取 CPU 名称。在这个过程中，可能会有其他线程或者进程同时进入 get_cpu_name 函数并尝试获取 CPU 名称。如果另一个线程在此期间成功获取了 CPU 名称，那么当前线程就没有必要再次获取了。这就是 check again 的作用。

然后，再次检查 get_cpu_flag 变量的好处在于，在多线程环境中，可能会出现一种情况，即在当前线程获取到锁之前，另一个线程已经成功获取 CPU 名称并将 get_cpu_flag 设置为真。如果没有在解锁之前再次检查 get_cpu_flag，那么当前线程可能会误以为自己获取到了 CPU 名称，从而不再尝试获取。但实际上，由于竞态条件，当前线程获取到的可能是已经过时的 CPU 名称。因此，通过在解锁之前再次检查 get_cpu_flag，可以避免这种竞态条件，确保在当前线程获取到锁之前，已经成功获取了 CPU 名称的线程已经将 get_cpu_flag 设置为真。
