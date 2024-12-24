---
date: 2023/07/11 18:07:17
updated: 2023/08/11 16:11:05
---

# uos-1060 kgdb 修复

## 修复代码

将`x86-kernel` 分支`4.19-rc8`中的`kernel/debug`目录、`include/linux/sysrq.h`、`drivers/tty/sysrq.c`覆盖`4.19内核 6026`对应位置文件。
编译过程中会遇到`include/linux/security2.h`中的`void security_set_audit_started(int started)`重复定义，注释掉154-156行即可。

![涉及到的源码](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20230711164047.png)

- `4.19-rc8` 对应commit id：35a7f35ad1b1
- `4.19内核 6026`对应commit id：69a08e713fb2

### 生成patch

```bash
git format-patch -1 c907106159ca -o kgdb_not_work.patch
```

patch下载地址：[kgdb_not_work.patch](https://github.com/realwujing/realwujing.github.io/tree/main/linux/kernel/sysrq_trigger)

### 应用patch

先检查patch文件：

```bash
git apply --stat kgdb_not_work.patch
```

检查能否应用成功：

```bash
git apply --check kgdb_not_work.patch
```

打补丁：

```bash
git am --signoff < kgdb_not_work.patch
```

## 内核kgdb编译选项

### kgdb相关

```text
CONFIG_KGDB = y     # 加入KGDB支持
CONFIG_KGDB_SERIAL_CONSOLE = y      # 使KGDB通过串口与主机通信(打开这个选项，默认会打开CONFIG_CONSOLE_POLL和CONFIG_MAGIC_SYSRQ)
CONFIG_KGDB_KDB = y     # 加入KDB支持
CONFIG_DEBUG_KERNEL = y     #包含驱动调试信息
CONFIG_DEBUG_INFO = y       # 使内核包含基本调试信息
```

### vmlinux-gdb.py相关

```text
CONFIG_GDB_SCRIPTS = y # gdb脚本
```

### 关闭内核随机地址选项

```text
CONFIG_RANDOMIZE_BASE = n
```

## 编译内核deb包

```bash
make bindeb-pkg -j`expr $(nproc) / 2`
```
