# uts_namespace内核实现

本次调试基于linux v5.10-rc7，具体参加下方链接：

- [基于qemu桥接网络、debootstrap 调试内核、根文件系统](https://github.com/realwujing/linux-learning/blob/main/debug/kernel/qemu/%E5%9F%BA%E4%BA%8Eqemu%E6%A1%A5%E6%8E%A5%E7%BD%91%E7%BB%9C%E3%80%81debootstrap%20%E8%B0%83%E8%AF%95%E5%86%85%E6%A0%B8%E3%80%81%E6%A0%B9%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F.md)

## 断点

```text
(gdb) save breakpoints ust_namespace.breakpoint
Saved to file 'ust_namespace.breakpoint'.
```

```text
break fs/exec.c:do_execveat_common
break kernel/fork.c:2573
break kernel/fork.c:kernel_clone
break kernel/fork.c:2456
break kernel/fork.c:copy_process
break kernel/fork.c:1929
break kernel/fork.c:2098
break kernel/nsproxy.c:copy_namespaces
break kernel/nsproxy.c:162
break include/linux/nsproxy.h:109
```

```bash
source ust_namespace.breakpoint
i b
```

## uts_namespace_demo

通过qemu进入系统后：

```bash
cd build
./uts_namespace_demo
```

## 命名空间复制

```text
14      breakpoint     keep y   0xffffffff8105ea0d in copy_process at kernel/fork.c:2098
retval = copy_namespaces(clone_flags, p);
```

```text
b kernel/nsproxy.c:copy_namespaces 
Breakpoint 15 at 0xffffffff81084c50: file kernel/nsproxy.c, line 153.
```

```c
int copy_namespaces(unsigned long flags, struct task_struct *tsk)   // kernel/nsproxy.c:151
{
	struct nsproxy *old_ns = tsk->nsproxy;  // task_struct有namespace相关数据结构
	struct user_namespace *user_ns = task_cred_xxx(tsk, user_ns); // user_namespace
	struct nsproxy *new_ns;
	int ret;

	if (likely(!(flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC |
			      CLONE_NEWPID | CLONE_NEWNET |
			      CLONE_NEWCGROUP | CLONE_NEWTIME)))) {         // CLONE_NEWUTS符合条件，此处为true
		if (likely(old_ns->time_ns_for_children == old_ns->time_ns)) {  // 此处也为true
			get_nsproxy(old_ns);
			return 0;   // 此处函数返回
		}
	} else if (!ns_capable(user_ns, CAP_SYS_ADMIN))     // 根据usernamesapce鉴权
		return -EPERM;
...
}
```

```text
(gdb) b kernel/nsproxy.c:162
Breakpoint 16 at 0xffffffff81084c90: file kernel/nsproxy.c, line 163.
```

```text
(gdb) b include/linux/nsproxy.h:109
Breakpoint 17 at 0xffffffff81229df2: file ./include/linux/nsproxy.h, line 109.
```

## nsproxy数据结构

```c
struct nsproxy {    // include/linux/nsproxy.h:31
	atomic_t count;
	struct uts_namespace *uts_ns;
	struct ipc_namespace *ipc_ns;
	struct mnt_namespace *mnt_ns;
	struct pid_namespace *pid_ns_for_children;
	struct net 	     *net_ns;
	struct time_namespace *time_ns;
	struct time_namespace *time_ns_for_children;
	struct cgroup_namespace *cgroup_ns;
};
extern struct nsproxy init_nsproxy;
```

## unshare系统调用加入新的namespace

```c
/*
 * Called from unshare. Unshare all the namespaces part of nsproxy.
 * On success, returns the new nsproxy.
 */
int unshare_nsproxy_namespaces(unsigned long unshare_flags,
	struct nsproxy **new_nsp, struct cred *new_cred, struct fs_struct *new_fs)  // kernel/nsproxy.c:262
{
	struct user_namespace *user_ns;
	int err = 0;

	if (!(unshare_flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC |
			       CLONE_NEWNET | CLONE_NEWPID | CLONE_NEWCGROUP |
			       CLONE_NEWTIME)))
		return 0;

	user_ns = new_cred ? new_cred->user_ns : current_user_ns();
	if (!ns_capable(user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	*new_nsp = create_new_namespaces(unshare_flags, current, user_ns,
					 new_fs ? new_fs : current->fs);
	if (IS_ERR(*new_nsp)) {
		err = PTR_ERR(*new_nsp);
		goto out;
	}

out:
	return err;
}
```
