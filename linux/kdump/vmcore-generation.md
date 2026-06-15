# Linux 内核 vmcore 生成机制深度解析 — 从 panic 到 crash dump

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`kernel/kexec_core.c` `kernel/crash_core.c` `kernel/vmcore_info.c` `kernel/kexec_internal.h` `arch/x86/kernel/crash.c` `arch/x86/kernel/machine_kexec_64.c`

---

## 1. kdump 的核心机制

kdump 本质上是一个 **双内核方案**：

```
正常内核 (production kernel)
  │
  ├─ 启动时: 预留 crashkernel=256M 的内存区域 (不能用于正常运行)
  │        → 将第二个内核 (capture kernel) 加载到这个保留区域
  │
  ├─ 运行时: 每个内核线程、每个用户进程照常运行
  │
  ├─ panic() 时:
  │   ① crash_shutdown: 所有 CPU 停止正常操作
  │   ② crash_save_cpu: 每颗 CPU 的寄存器 + 栈快照保存到 ELF note
  │   ③ crash_save_vmcoreinfo: 保存内核关键常量 (PAGE_SHIFT, kmalloc_size, etc.)
  │   ④ machine_kexec: 跳转到 capture kernel 继续运行
  │
  └─ capture kernel:
       ① 启动 → 挂载 root → makedumpfile 或 vmcore-dmesg
       ② 将 crash dump 区域写为 /proc/vmcore → 压缩 → 保存到 /var/crash/
       ③ 重启系统
```

---

## 2. 关键数据结构

### 2.1 `crash_save_cpu` — 每颗 CPU 的寄存器保存

```c
// arch/x86/kernel/crash.c:56-62
static void kdump_nmi_callback(int cpu, struct pt_regs *regs)
{
    crash_save_cpu(regs, cpu);  // ★ 保存 CPU 的寄存器到 ELF note 区域

    // 禁用所有 CPU 的中断 (防止 IOAPIC/HPET 死锁 machine_kexec)
    disable_local_APIC();
}
```

panic 发生后，主 CPU 通过 `nmi_shootdown_cpus(kdump_nmi_callback)` 向所有其他 CPU 发送 NMI，使它们停止运行并把寄存器状态保存下来。

### 2.2 `machine_crash_shutdown` — 系统停止

```c
// arch/x86/kernel/crash.c:100-145
void native_machine_crash_shutdown(struct pt_regs *regs)
{
    // ① 停止所有其他 CPU 并保存它们的寄存器
    nmi_shootdown_cpus(kdump_nmi_callback);

    // ② 禁用本地中断和 IOAPIC (防止 kexec 时死锁)
    lapic_shutdown();

    // ③ ★ 最后保存主 CPU 的寄存器
    crash_save_cpu(regs, smp_processor_id());
}
```

### 2.3 `vmcoreinfo` — 内核关键常量

```c
// kernel/vmcore_info.c:84-93
void crash_save_vmcoreinfo(void)
{
    // ★ 以下所有都保存在 vmcore 的 ELF note 中
    vmcoreinfo_append_str("CRASHTIME=%lld\n", ktime_get_real_seconds());

    // 页大小、物理基址等
    VMCOREINFO_NUMBER(PAGE_SHIFT);
    VMCOREINFO_NUMBER(PAGE_SIZE);
    VMCOREINFO_NUMBER(PAGE_OFFSET);

    // SLUB 分配器常量 (crash 工具需要读 kmalloc caches)
    VMCOREINFO_NUMBER(KMALLOC_MIN_SIZE);
    VMCOREINFO_NUMBER(KMALLOC_SHIFT_HIGH);

    // 内核链接基址
    VMCOREINFO_NUMBER(KERNELOFFSET);

    // 内存布局常量
    VMCOREINFO_NUMBER(VMALLOC_START);
    VMCOREINFO_NUMBER(VMEMMAP_START);

    // 内核符号表
    vmcoreinfo_append_str("SYMBOL(init_uts_ns)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(node_online_map)=%lx\n", ...);
    vmcoreinfo_append_str("SYMBOL(swapper_pg_dir)=%lx\n", ...);

    // ... 还有 30+ 条 VMCOREINFO 记录 ...
}
```

```c
// kernel/vmcore_info.c:140-150 — vmcoreinfo 的物理存储
static int __init crash_save_vmcoreinfo_init(void)
{
    order = get_order(VMCOREINFO_BYTES);  // VMCOREINFO_BYTES=4096

    // 分配一个物理页存储 VMCOREINFO note
    vmcoreinfo_note = alloc_pages_exact(VMCOREINFO_NOTE_SIZE, GFP_KERNEL);
    buf = append_elf_note(buf, VMCOREINFO_NOTE_NAME, 0, vmcoreinfo_data,
                          vmcoreinfo_size);  // 将之前 append 的数据打包为 ELF note

    // 这个 page 在 crash 时不会被 capture kernel 覆盖
    // crash 工具启动后读这个物理地址就是 VMCOREINFO 内容
}
```

---

## 3. vmcore 中的 ELF 结构

kdump 生成的 vmcore 文件是一个**完整的 ELF 文件**。其包含的 ELF 段：

```
ELF Header
  ├── Program Header 1: NULL (padding)
  ├── Program Header 2: PT_NOTE — VMCOREINFO (重要的内核常量, 4KB)
  ├── Program Header 3: PT_NOTE — per-CPU registers (每颗 CPU 一个)
  ├── Program Header 4: PT_LOAD — 普通页 (内存中的真实内容)
  │     offset=X, virt=X, phys=X, filesz=N, memsz=N
  │     ★ 这部分的 offset 在内存中, phys/virt 在 crash 前的物理地址
  ├── Program Header 5: PT_LOAD — ZONE_DMA32 区域
  ├── ...
  └── Program Header N: PT_NOTE — kernel map / sysrq key
```

### `struct elf_prstatus` — 每颗 CPU 的寄存器快照

```c
// include/uapi/linux/elfcore.h
struct elf_prstatus {
    struct elf_siginfo pr_info;
    short pr_cursig;
    unsigned long pr_sigpend;
    unsigned long pr_sighold;
    pid_t pr_pid;
    // ...
    struct elf_siginfo pr_reg;    // ★ 完整的 pt_regs — 所有 GPR, rip, cs, flags
    int pr_fpvalid;
};
```

**crash 工具的第一件事就是读这个结构**——拿到 panic 时的 `rip`、`rsp`、`rflags` 等，才开始栈回溯。

---

## 4. 从 panic 到 kexec — 完整流程

```
panic()                          // kernel/panic.c
  ├── smp_send_stop()            // 停止其他 CPU
  ├── 1秒延迟 (留给用户看到 panic 消息)
  └── __crash_kexec(regs)        // ★ 发起 kexec
        │
        ├── kexec_trylock()      // 原子锁，防止多个 CPU 并发 kexec
        │
        ├── 保存 crashing CPU 的寄存器:
        │     crash_setup_regs(&fixed_regs, regs)  // 保存到 fixed_regs
        │
        └── machine_crash_shutdown(&fixed_regs)
              │
              ├── nmi_shootdown_cpus(kdump_nmi_callback)
              │     └── 对每颗 CPU 发送 NMI
              │         └── kdump_nmi_callback → crash_save_cpu
              │               ★ 每颗 CPU 的 elf_prstatus 被保存
              │
              ├── lapic_shutdown()
              │
              └── machine_kexec(kexec_crash_image)
                    │
                    ├── ① 保存 FPU 状态
                    ├── ② 清除 CR4 中的 PGE 标志 (防止 TLB 缓存干扰)
                    ├── ③ 加载 crash 内核的 IDT/页表
                    ├── ④ 写 CR3 = crash 内核的页表基址
                    ├── ⑤ 清除所有段寄存器
                    └── ⑥ ★ JMP 到 crash 内核的入口地址
```

---

## 5. crash 内核的页表设置 — 为什么它能读到生产内核的内存

这是 kdump 最关键的安全保证。生产内核通过 `crashkernel=256M` 保留了一片物理内存，这 256M 在正常运行时从未被分配、从未被访问。**crash 内核的页表直接映射物理 RAM**——但只在 crash 保留区工作。

**生产内核的内存映射**：
```
正常运行时: 
  ［crash 保留区 256M］  ← 未分配
  ［内核代码+数据      ］  ← CPU 寻址和操作
  ［用户空间           ］
```

**crash 内核的页表**：
```
machine_kexec 后:
  之前的 crash 保留区 → 现在成为 crash 内核的主要内存
  crash 内核通过 /proc/vmcore 伪文件可以 "看到" 并读取完整的物理地址范围
  包括生产内核数据、CPU 寄存器、vmcoreinfo 等
```

**为什么安全**：crash 内核只运行在 256M 保留区中。它通过 `/proc/vmcore` 读取 ELF 段映射到更大的物理地址范围，但它自己的 `.text`/`.data`/`.stack` 只在 crash 保留区内——所以不会覆盖生产内核的原始数据。

---

## 6. 关键文件与行号速查

| 组件 | 文件 | 行号/区域 | 作用 |
|------|------|---------|------|
| panic 入口 | `kernel/panic.c` | panic() → `__crash_kexec(regs)` | 系统 panic 后触发 kdump |
| kexec 锁 | `kernel/kexec_internal.h` | 26-38 | 原子锁保护 crash_kexec |
| crash 关闭 | `arch/x86/kernel/crash.c` | 100-145 | NMI 射杀 + APIC 关闭 + 主 CPU 寄存器保存 |
| CPU 寄存器保存 | `arch/x86/kernel/crash.c` | 56-62 | `kdump_nmi_callback` → `crash_save_cpu` |
| vmcoreinfo | `kernel/vmcore_info.c` | 84-93, 97-115, 140-155 | `crash_save_vmcoreinfo` 保存关键常量 |
| 跳转 | `arch/x86/kernel/machine_kexec_64.c` | — | `machine_kexec` → 新内核 |
| elfcorehdr | `kernel/crash_core.c` | 46-53, 232-233 | VMCOREINFO 页分配 + ELF PHDR 生成 |

---

## 7. 常见排查点

| 故障 | 原因 | 解决方法 |
|------|------|---------|
| `kdump.service: failed to start` | crashkernel 预留不够大 | 增大 `crashkernel=` 参数 |
| capture kernel 启动后 OOM | 保留区太小 | 增加保留区大小到 512M 或 1G |
| `makedumpfile` 超时 | 内存太大需要太多时间 | `-c` 压缩选项, `-d 31` 删除零页 |
| `crash` 工具 "cannot read vmcoreinfo" | crash 保留区被 memcpy 覆盖 | 检查 crash 内核启动日志, 看 ELF note 是否正确 |
| CPU 寄存器为空 | NMI 未被正确处理 | 检查 IOAPIC/NMI 配置 |
| vmcore 小于物理内存 | zero-page 过滤 (`-d31`), 或 IOMMU 区域未映射 | 检查 IOMMU 映射表 |
