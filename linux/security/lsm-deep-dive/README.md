# Linux 安全子系统 内核源码深度解析

> 📌 源码：git@github.com:torvalds/linux.git, torvalds/master, `eb3f4b7426cf` (v7.1-rc5-26)
> 源码：`security/security.c` `security/commoncap.c` | 头文件：`include/linux/lsm_hook_defs.h` `include/linux/lsm_hooks.h`

---

Linux 安全子系统通过 **LSM (Linux Security Modules)** 框架，将安全策略从内核主逻辑中解耦。本系列从 LSM Hook 宏展开、模块初始化，到 POSIX 能力、SELinux/AppArmor 强制访问控制，再到 modern zero-trust (lockdown, BPF, Landlock)，逐层解剖。

## 文章目录

| 篇 | 文件 | 内容 |
|---|------|------|
| 1 | [LSM 框架与 Hook 调用链](./01-lsm-core.md) | `LSM_HOOK` 宏展开、static call 优化、全局 hook head 链表 |
| 2 | [LSM 模块化与初始化](./02-lsm-init.md) | `DEFINE_LSM` 宏、`security_add_hooks`、ordered_lsm_init |
| 3 | [POSIX Capabilities 内核实现](./03-capabilities.md) | `kernel_cap_t` 位图、能力集算术、user namespace 隔离 |
| 4 | [SELinux 与 AppArmor 强制访问控制](./04-mac.md) | Type Enforcement、AVC 缓存、path-based matching、标签 vs 路径 |
| 5 | [Linux 内核零信任](./05-zero-trust.md) | Lockdown LSM、Yama LSM、BPF LSM、Landlock 非特权自主限制 |

## 前置知识

- 知道 `ls -Z` 或 `lsm=selinux` 内核启动参数的用法
- 掌握 kernel module loading 和 ELF section 概念
- 熟悉 `struct file`、`struct inode`、`struct cred` 等基础结构体
- 阅读过本仓库的 VFS / sched 系列更佳

## 架构总览

```
  用户态：chmod, setcap, SELinux 策略, BPF LSM 程序
        │
   ┌────▼────────────────────────────────────────┐
   │  security/security.c                        │
   │  LSM_HOOK 生成的 security_xxx() 包装函数     │
   │  ┌──────────────────────────────────────── ┐│
   │  │  call_int_hook / call_void_hook         ││ ← 遍历 hlist
   │  │  或者 arch_static_call (如果启用)        ││ ← 零间接跳转
   │  └─────────────────────────────────────────┘│
   └────┬─────────────────┬──────────────────┬───┘
        │                 │                  │
   ┌────▼─────┐  ┌────────▼──────┐  ┌───────▼──────────────┐
   │ SELinux  │  │ AppArmor      │  │ BPF / Landlock      │
   │ rights   │  │ path-based    │  │ user-driven         │
   └──────────┘  └───────────────┘  └────────────────────┘
        │                 │                  │
        └─────────────────┴──────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │  Common Capabilities (commoncap.c)      │
   │  POSIX CAP: bprm->cred 预处理           │
   └─────────────────────────────────────────┘
```
