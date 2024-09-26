---
date: 2023/07/07 16:59:37
updated: 2023/07/07 16:59:37
---

# echo g > /proc/sysrq_trigger

本次调试环境：

- 宿主机：amd64 debian12
- 虚拟机：qemu虚拟化debian12
- linux kernel：6.1.27
- 宿主机与虚拟机通过virt-manager提供的串口设备/dev/pts/4进行kgdb调试

## 核心断点

```text
drivers/tty/sysrq.c:1155 static ssize_t write_sysrq_trigger(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
drivers/tty/sysrq.c:1163 __handle_sysrq(c, false);
drivers/tty/sysrq.c:572 void __handle_sysrq(int key, bool check_mask)
drivers/tty/sysrq.c:602 op_p->handler(key);
kernel/debug/debug_core.c:971 static void sysrq_handle_dbg(int key)
kernel/debug/debug_core.c:977 if (!kgdb_connected) {
kernel/debug/debug_core.c:986 kgdb_breakpoint();
drivers/tty/sysrq.c:1166 return count;
```

### 参考断点

`save breakpoints sysrq_trigger.break`结果如下：

```text
break drivers/tty/sysrq.c:572
break drivers/tty/sysrq.c:593
break drivers/tty/sysrq.c:594
break drivers/tty/sysrq.c:599
break drivers/tty/sysrq.c:608
break drivers/tty/sysrq.c:602
break kernel/debug/debug_core.c:973
break kernel/debug/debug_core.c:977
break kernel/debug/debug_core.c:986
break kernel/debug/debug_core.c:1214
break drivers/tty/sysrq.c:625
break drivers/tty/sysrq.c:1166
break drivers/tty/sysrq.c:631
break drivers/tty/sysrq.c:1155
break drivers/tty/sysrq.c:1166
```

使用`source breakpoints sysrq_trigger.break`可恢复断点，方便调试。

## sysrq_handle_dbg与魔法键g

```c
static const struct sysrq_key_op sysrq_dbg_op = {   // kernel/debug/debug_core.c:989
	.handler	= sysrq_handle_dbg,
	.help_msg	= "debug(g)",
	.action_msg	= "DEBUG",
};
```

```c
#ifdef CONFIG_MAGIC_SYSRQ   // kernel/debug/debug_core.c:1075
		register_sysrq_key('g', &sysrq_dbg_op);
#endif
```
