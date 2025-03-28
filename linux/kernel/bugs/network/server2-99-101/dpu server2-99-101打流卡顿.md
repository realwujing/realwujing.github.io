# dpu server2-99-101打流卡顿

- WVVoU01HTklUVFpNZVRrelpETmpkV016U210Wk1uaDJaRmRSZFZreU5IWmxibWd6WVZjd2RrMVVXVEZOVkZWMldWZDRjMVl5T1hsaE1Hd3dXbGN4ZWt3eGNFdFZNRFZFVEZSRmQwMTZWVDA9

## 环境

TlZsaFJqWktTMXBOUkdwc2NFcHdRbGQxWVRGcEsybDJiRk5CZUUxRE5IbE9SRmwxVFZSVk0weHFTVEJQUVc5TFQxUlJkVTFVUVhoRFozQjZaRmRTZGtsRE1YcERaM0JwV1ZoT2IwbElUbnBoUVQwOQ==

## top

![top_server2-99-101-2](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/top_server2-99-101-2.png)

![top_server2-99-101](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/top_server2-99-101.png)

### 进程状态D与S对比

在操作系统中，进程状态 **D** 和 **S** 是常见的进程状态，通常出现在类 Unix 系统（如 Linux）的进程管理中。这两个状态都与进程的等待有关，但等待的内容和含义有所不同。以下是它们的区别：

#### 1. **进程状态 D（Uninterruptible Sleep，不可中断睡眠）**
- **含义**：进程处于一种深度睡眠状态，通常是因为它正在等待某个关键的资源（例如 I/O 操作，如磁盘读写、网络数据传输等），且这个等待是不可中断的。
- **等待的内容**：进程在等待硬件或内核级的资源完成操作。例如，进程可能在等待磁盘 I/O、网络数据包到达或某些设备驱动程序的响应。
- **特点**：
  - 不可被信号（如 SIGINT 或 SIGKILL）中断，除非资源可用或操作完成。
  - 通常是短暂的，但如果资源长时间不可用（例如硬件故障），进程可能长时间停留在此状态。
  - 在 `ps` 命令中显示为 `D`。
- **示例场景**：一个进程正在从硬盘读取大文件，等待数据从物理设备传输到内存。

#### 2. **进程状态 S（Interruptible Sleep，可中断睡眠）**
- **含义**：进程处于一种较轻的睡眠状态，通常是因为它在等待某个事件（例如用户输入、定时器到期、其他进程的信号等），且这个等待是可以被信号中断的。
- **等待的内容**：进程在等待非关键资源或事件，例如等待用户输入、等待子进程结束、等待信号量释放等。
- **特点**：
  - 可以被信号（如 SIGINT 或 SIGTERM）唤醒或终止。
  - 进程处于可调度状态，一旦等待的事件发生，进程会迅速被唤醒并进入就绪状态。
  - 在 `ps` 命令中显示为 `S`。
- **示例场景**：一个进程在终端等待用户输入命令，或者一个服务进程在等待客户端的网络请求。

#### **主要区别**
| 特性                | 状态 D (不可中断睡眠)            | 状态 S (可中断睡眠)            |
|---------------------|----------------------------------|---------------------------------|
| **等待的内容**      | 硬件或内核级资源（如 I/O）       | 事件或非关键资源（如信号、输入） |
| **可中断性**        | 不可被信号中断                  | 可被信号中断                   |
| **持续时间**        | 通常较短（依赖资源可用性）       | 可长可短（依赖事件触发）        |
| **典型场景**        | 磁盘读写、网络传输              | 等待用户输入、定时器            |

#### **总结**
- **D 状态** 是进程在等待底层资源时的“强制等待”，对外部信号无响应，优先级较高。
- **S 状态** 是进程在等待一般事件时的“自愿等待”，可以被外部信号打断，更加灵活。

## perf

![perf_server2-99-101](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/perf_server2-99-101.png)

## mpstat -P ALL

### **命令解释**
- **`mpstat`**：是 `sysstat` 工具集中的一个命令，用于显示 CPU 的使用统计信息。
- **`-P ALL`**：表示显示所有 CPU 核心的统计信息（包括每个单独核心和整体平均值）。如果不加 `-P ALL`，默认只显示所有 CPU 的平均统计。

### **输出字段含义**
1. **`CPU`**：表示 CPU 核心编号，`all` 是所有 CPU 的平均值，后面是每个核心（如 0、1、2、3）。
2. **`%usr`**：用户态进程占用 CPU 的百分比。
3. **`%nice`**：低优先级（nice 值调整过的）用户态进程占用 CPU 的百分比。
4. **`%sys`**：内核态（系统）占用 CPU 的百分比。
5. **`%iowait`**：CPU 等待 I/O 操作（如磁盘、网络）完成的百分比。
6. **`%irq`**：处理硬件中断的 CPU 使用百分比。
7. **`%soft`**：处理软中断的 CPU 使用百分比。
8. **`%steal`**：虚拟化环境中被其他虚拟机“偷走”的 CPU 时间百分比。
9. **`%guest`**：虚拟机中运行的客户操作系统占用 CPU 的百分比。
10. **`%gnice`**：虚拟机中低优先级客户进程占用 CPU 的百分比。
11. **`%idle`**：CPU 空闲时间的百分比。

![mpstat_server2-99-101](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/mpstat_server2-99-101.png)

## kernel.softlockup_panic

```bash
sysctl -w kernel.softlockup_panic=1
```

![kdump_17430453156426](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/kdump_17430453156426.png)


## crash

### foreach bt

```bash
crash> foreach bt

PID: 73098  TASK: ffff88d26cbf0000  CPU: 63  COMMAND: "server2-4M"
 #0 [fffffe0000e92e50] crash_nmi_callback at ffffffffa785ea01
 #1 [fffffe0000e92e58] nmi_handle at ffffffffa782a068
 #2 [fffffe0000e92ea0] default_do_nmi at ffffffffa82cc2a2
 #3 [fffffe0000e92ec8] exc_nmi at ffffffffa82cc4c3
 #4 [fffffe0000e92ef0] end_repeat_nmi at ffffffffa8401508
    [exception RIP: tcp_poll+149]
    RIP: ffffffffa81792b5  RSP: ffffac0e1a79ba60  RFLAGS: 00000293
    RAX: 0000000000000001  RBX: ffff88d131227500  RCX: 0000000000000001
    RDX: 0000000000000000  RSI: 0000000000000000  RDI: ffff894fa46030c0
    RBP: ffff894fa46030c0   R8: ffffac0e5100c270   R9: 0000000000000001
    R10: 0000000000000001  R11: 0000000000000000  R12: 0000000000000000
    R13: ffff894fa156e800  R14: ffff894fa46030c0  R15: ffff894f9218f4dc
    ORIG_RAX: ffffffffffffffff  CS: 0010  SS: 0018
--- <NMI exception stack> ---
 #5 [ffffac0e1a79ba60] tcp_poll at ffffffffa81792b5
 #6 [ffffac0e1a79ba78] sock_poll at ffffffffa80a1b97
 #7 [ffffac0e1a79baa0] do_poll.constprop.0 at ffffffffa7bac102
 #8 [ffffac0e1a79bb40] do_sys_poll at ffffffffa7bac53e
 #9 [ffffac0e1a79bf08] __se_sys_poll at ffffffffa7bacbd7
#10 [ffffac0e1a79bf38] do_syscall_64 at ffffffffa82cac00
#11 [ffffac0e1a79bf50] entry_SYSCALL_64_after_hwframe at ffffffffa8400099
    RIP: 00007f2ed013504f  RSP: 00007f2ecc8164f0  RFLAGS: 00000293
    RAX: ffffffffffffffda  RBX: 00007f2ecc82a640  RCX: 00007f2ed013504f
    RDX: 0000000000000064  RSI: 0000000000000175  RDI: 00007f2ecc816570
    RBP: 00007f2ecc829e50   R8: 0000000000000000   R9: 0000000000000000
    R10: 00007f2ed01d2300  R11: 0000000000000293  R12: 00007f2ecc82a640
    R13: 0000000000000000  R14: 00007f2ed00bf430  R15: 00007fff5fd99190
    ORIG_RAX: 0000000000000007  CS: 0033  SS: 002b
```

```bash
crash> foreach bt

PID: 73109  TASK: ffff88d01a2fc000  CPU: 78  COMMAND: "server2-4M"
 #0 [fffffe0001207e50] crash_nmi_callback at ffffffffa785ea01
 #1 [fffffe0001207e58] nmi_handle at ffffffffa782a068
 #2 [fffffe0001207ea0] default_do_nmi at ffffffffa82cc2a2
 #3 [fffffe0001207ec8] exc_nmi at ffffffffa82cc4c3
 #4 [fffffe0001207ef0] end_repeat_nmi at ffffffffa8401508
    [exception RIP: tcp_poll+74]
    RIP: ffffffffa817926a  RSP: ffffac0e1ae4fa60  RFLAGS: 00000293
    RAX: 0000000000000001  RBX: ffff88d30c5ceb40  RCX: 0000000000000058
    RDX: 0000000000000000  RSI: ffff88d2d06e6180  RDI: ffff88d253c95540
    RBP: ffff88d253c95540   R8: ffffac0e50c2c418   R9: 0000000000000001
    R10: 0000000000000001  R11: 0000000000000000  R12: ffffac0e1ae4fc58
    R13: ffff88d2d06e6180  R14: ffff88d253c95540  R15: ffff896fb6af5154
    ORIG_RAX: ffffffffffffffff  CS: 0010  SS: 0018
--- <NMI exception stack> ---
 #5 [ffffac0e1ae4fa60] tcp_poll at ffffffffa817926a
 #6 [ffffac0e1ae4fa78] sock_poll at ffffffffa80a1b97
 #7 [ffffac0e1ae4faa0] do_poll.constprop.0 at ffffffffa7bac102
 #8 [ffffac0e1ae4fb40] do_sys_poll at ffffffffa7bac53e
 #9 [ffffac0e1ae4ff08] __se_sys_poll at ffffffffa7bacbd7
#10 [ffffac0e1ae4ff38] do_syscall_64 at ffffffffa82cac00
#11 [ffffac0e1ae4ff50] entry_SYSCALL_64_after_hwframe at ffffffffa8400099
    RIP: 00007f2ed013504f  RSP: 00007f2ec700b4f0  RFLAGS: 00000293
    RAX: ffffffffffffffda  RBX: 00007f2ec701f640  RCX: 00007f2ed013504f
    RDX: 0000000000000064  RSI: 00000000000001ff  RDI: 00007f2ec700b570
    RBP: 00007f2ec701ee50   R8: 0000000000000000   R9: 0000000000000000
    R10: 00007f2ed01d2300  R11: 0000000000000293  R12: 00007f2ec701f640
    R13: 0000000000000000  R14: 00007f2ed00bf430  R15: 00007fff5fd99190
    ORIG_RAX: 0000000000000007  CS: 0033  SS: 002b
```

```bash
crash> foreach bt

PID: 73155  TASK: ffff88d037474000  CPU: 207  COMMAND: "server2-4M"
 #0 [fffffe0002fc2e50] crash_nmi_callback at ffffffffa785ea01
 #1 [fffffe0002fc2e58] nmi_handle at ffffffffa782a068
 #2 [fffffe0002fc2ea0] default_do_nmi at ffffffffa82cc2a2
 #3 [fffffe0002fc2ec8] exc_nmi at ffffffffa82cc4c3
 #4 [fffffe0002fc2ef0] end_repeat_nmi at ffffffffa8401508
    [exception RIP: sock_poll+33]
    RIP: ffffffffa80a1b41  RSP: ffffac0e1bcbba80  RFLAGS: 00000282
    RAX: ffffffffa89226a0  RBX: 0000000000000000  RCX: ffff88d17b8a5cc1
    RDX: 0000000000000058  RSI: ffffac0e1bcbbc58  RDI: ffff88d17b8a5cc0
    RBP: ffff88d17b8a5cc0   R8: ffffac0e50f237c8   R9: 0000000000000001
    R10: 0000000000000001  R11: 0000000000000000  R12: ffffac0e1bcbbc58
    R13: ffff88d24948db00  R14: ffff88d17b8a5cc0  R15: ffff898fd1f905ec
    ORIG_RAX: ffffffffffffffff  CS: 0010  SS: 0018
--- <NMI exception stack> ---
 #5 [ffffac0e1bcbba80] sock_poll at ffffffffa80a1b41
 #6 [ffffac0e1bcbbaa0] do_poll.constprop.0 at ffffffffa7bac102
 #7 [ffffac0e1bcbbb40] do_sys_poll at ffffffffa7bac53e
 #8 [ffffac0e1bcbbf08] __se_sys_poll at ffffffffa7bacbd7
 #9 [ffffac0e1bcbbf38] do_syscall_64 at ffffffffa82cac00
#10 [ffffac0e1bcbbf50] entry_SYSCALL_64_after_hwframe at ffffffffa8400099
    RIP: 00007f2ed013504f  RSP: 00007f2eaffdd4f0  RFLAGS: 00000293
    RAX: ffffffffffffffda  RBX: 00007f2eafff1640  RCX: 00007f2ed013504f
    RDX: 0000000000000064  RSI: 0000000000000219  RDI: 00007f2eaffdd570
    RBP: 00007f2eafff0e50   R8: 0000000000000000   R9: 0000000000000000
    R10: 00007f2ed01d2300  R11: 0000000000000293  R12: 00007f2eafff1640
    R13: 0000000000000000  R14: 00007f2ed00bf430  R15: 00007fff5fd99190
    ORIG_RAX: 0000000000000007  CS: 0033  SS: 002b
```

```bash
crash> foreach bt

PID: 6892   TASK: ffff892f92c18000  CPU: 157  COMMAND: "ctcss-agentd"
 #0 [fffffe000243ce50] crash_nmi_callback at ffffffffa785ea01
 #1 [fffffe000243ce58] nmi_handle at ffffffffa782a068
 #2 [fffffe000243cea0] default_do_nmi at ffffffffa82cc2a2
 #3 [fffffe000243cec8] exc_nmi at ffffffffa82cc4c3
 #4 [fffffe000243cef0] end_repeat_nmi at ffffffffa8401508
    [exception RIP: xfs_trans_log_inode+65]
    RIP: ffffffffc0956001  RSP: ffffac0e32a37a30  RFLAGS: 00000246
    RAX: 0000000000000000  RBX: ffff892f9498f740  RCX: 0000000000000000
    RDX: 0000000000000001  RSI: ffff892f94a01100  RDI: ffff892f917e27e0
    RBP: ffff892f94a01100   R8: ffff892f917e2890   R9: ffff892f917e2890
    R10: ffff892f94a01100  R11: 0000000000000000  R12: 0000000000000001
    R13: ffff88d01b05c000  R14: 0000000000000001  R15: ffff892f917e27e0
    ORIG_RAX: ffffffffffffffff  CS: 0010  SS: 0018
--- <NMI exception stack> ---
 #5 [ffffac0e32a37a30] xfs_trans_log_inode at ffffffffc0956001 [xfs]
 #6 [ffffac0e32a37a88] xfs_ialloc at ffffffffc09728bb [xfs]
 #7 [ffffac0e32a37b08] xfs_dir_ialloc at ffffffffc09750b4 [xfs]
 #8 [ffffac0e32a37b90] xfs_create at ffffffffc09768b6 [xfs]
 #9 [ffffac0e32a37c28] xfs_generic_create at ffffffffc097214a [xfs]
#10 [ffffac0e32a37ca0] lookup_open at ffffffffa7ba1571
#11 [ffffac0e32a37d08] open_last_lookups at ffffffffa7ba31d6
#12 [ffffac0e32a37d68] path_openat at ffffffffa7ba3e28
#13 [ffffac0e32a37db8] do_filp_open at ffffffffa7ba68c0
#14 [ffffac0e32a37ec8] do_sys_openat2 at ffffffffa7b8dc07
#15 [ffffac0e32a37f10] __x64_sys_openat at ffffffffa7b8e2d4
#16 [ffffac0e32a37f38] do_syscall_64 at ffffffffa82cac00
#17 [ffffac0e32a37f50] entry_SYSCALL_64_after_hwframe at ffffffffa8400099
    RIP: 00007f026208fe44  RSP: 00007f025b7fb450  RFLAGS: 00000293
    RAX: ffffffffffffffda  RBX: 00007f0254000b70  RCX: 00007f026208fe44
    RDX: 0000000000000241  RSI: 00007f025b7fb5c0  RDI: 00000000ffffff9c
    RBP: 00007f025b7fb5c0   R8: 0000000000000000   R9: 0000000000000001
    R10: 00000000000001b6  R11: 0000000000000293  R12: 0000000000000241
    R13: 00007f0254000b70  R14: 0000000000000001  R15: 00007ffed6b023d0
    ORIG_RAX: 0000000000000101  CS: 0033  SS: 002b
```
