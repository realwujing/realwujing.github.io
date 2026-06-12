# 第1篇：LSM 框架与 Hook 调用链 — 从宏到静态调用

> 源码：`security/security.c` | 头文件：`include/linux/lsm_hook_defs.h` `include/linux/lsm_hooks.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. LSM 的核心理念

Linux 内核中对每类关键操作（打开文件、发送信号、挂载文件系统）都有一个 **Hook 点**。LSM 框架在 Hook 点调用注册的模块，任何模块返回 `-EPERM` → 操作被拒绝。

```
VFS: do_open → may_open()
                ↓
              security_file_permission(file, MAY_READ)   ← LSM hook
                ↓
              call_int_hook(file_permission, 0, file, MAY_READ)
                ↓
          ┌────┴────┬─────────┬──────────┐
          │ SELinux │AppArmor │ Landlock │  ← 依次调用，任何一个可返回 -EPERM
          └─────────┴─────────┴──────────┘
```

---

## 2. LSM_HOOK 宏 — 一行定义一个钩子

```c
// include/linux/lsm_hook_defs.h (~478行, 250+ hooks)
LSM_HOOK(int, 0, capable, const struct cred *cred,
         struct user_namespace *ns, int cap, unsigned int opts)

LSM_HOOK(int, 0, file_permission, struct file *file, int mask)

LSM_HOOK(int, 0, inode_permission, struct inode *inode, int mask)

LSM_HOOK(int, 0, bprm_creds_for_exec, struct linux_binprm *bprm)

LSM_HOOK(int, 0, ptrace_access_check, struct task_struct *child,
         unsigned int mode)

LSM_HOOK(int, 0, socket_create, int family, int type, int protocol, int kern)

LSM_HOOK(int, 0, task_getpgid, struct task_struct *p)

// ... 250+ 个 hook 定义 ...
```

**参数含义**：
- `int`：返回类型（绝大部分是 int，返回 0=允许，负值=拒绝）
- `0`：默认返回值（如果没有模块注册，各 hook 返回 0=允许）
- `XXX`：hook 名称
- 其余：hook 的参数列表

---

## 3. 宏展开——生成三层代码结构

LSM_HOOK 在 `security/security.c` 中被 `#include` 了 **三次**，每次都在不同的宏定义下展开，生成三个不同的代码块：

### 3.1 展开一：生成全局 hook 链表头

```c
// security/security.c:122-125
// 宏定义为:
#define LSM_HOOK(RET, DEFAULT, NAME, ...)       \
    struct hlist_head NAME;

#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
```

展开后生成全局的 `struct security_hook_heads` 结构体：

```c
struct security_hook_heads {
    struct hlist_head capable;         // ← 由 LSM_HOOK(int, 0, capable, ...) 生成
    struct hlist_head file_permission;
    struct hlist_head inode_permission;
    struct hlist_head bprm_creds_for_exec;
    struct hlist_head ptrace_access_check;
    struct hlist_head socket_create;
    struct hlist_head task_getpgid;
    // ... 250 个 hlist_head ...
};

// security/security.c:140
struct security_hook_heads security_hook_heads __ro_after_init;
```

每个 `hlist_head` 是一个 LSM hook 的注册回调链表——SELinux、AppArmor、BPF 各注册自己的回调到一个链表里。

### 3.2 展开二：生成 static call 跳板

```c
// security/security.c:144-156
// 宏定义为:
#define LSM_HOOK(RET, DEFAULT, NAME, ...)                \
    struct lsm_static_call NAME = {                       \
        .trampoline = LSM_HOOK_TRAMP(NAME, NUM),          \
    };

#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
```

生成 250 个 `lsm_static_call` 结构体，每个包含一个 **跳板地址**。如果只有一个模块注册了这个 hook，内核用 `arch_static_call` 零开销直接跳转到模块的回调——省去了 hlist 遍历。

### 3.3 展开三：生成 security_xxx() 包装函数

```c
// security/security.c:451-455
// 宏定义为:
#define LSM_HOOK(RET, DEFAULT, NAME, ...) \
RET security_##NAME(__VA_ARGS__)

#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
```

展开后生成 250 个模块暴露的 `security_xxx()` 函数：

```c
int security_capable(const struct cred *cred, struct user_namespace *ns,
                     int cap, unsigned int opts)
{
    // ① 优先走 static call —— 单槽优化
    // ② 如果有多个模块注册 → 回退到 call_int_hook 遍历 hlist
    return call_int_hook(capable, 0, cred, ns, cap, opts);
}

int security_file_permission(struct file *file, int mask) { ... }
int security_inode_permission(struct inode *inode, int mask) { ... }
int security_socket_create(int family, int type, int protocol, int kern) { ... }
```

---

## 4. call_int_hook — 遍历回调链

```c
// security/security.c:439-448
#define call_int_hook(FUNC, ...)                                          \
({                                                                        \
    int RC = LSM_RET_DEFAULT(FUNC);                                       \
    do {                                                                   \
        struct security_hook_list *P;                                      \
        hlist_for_each_entry(P, &security_hook_heads.FUNC, list) {        \
            RC = P->hook.FUNC(__VA_ARGS__);                                \
            if (RC != 0)                                                    \
                break;           // ★ 任一模块拒绝 → 立即返回               \
        }                                                                  \
    } while (0);                                                           \
    RC;                                                                    \
})
```

**调用链顺序**：按 LSM 注册顺序（通常 SELinux 第一，AppArmor 第二，BPF 第三），调用每个模块的回调。任何模块返回非零 → 立即停止遍历，返回拒绝。

---

## 5. LSM hook 在全局调用链中的位置

```
用户态: open("/etc/shadow", O_RDONLY)
  → do_sys_open → do_file_open → path_openat
    → may_open(inode, MAY_READ, ...)
      → security_inode_permission(inode, MAY_READ)     ← ★ LSM hook
        → selinux_inode_permission(inode, MAY_READ)     │
          → 检查 SELinux 策略 (type enforcement)         │ 三个模块
        → apparmor_file_permission(file, MAY_READ)      │ 依次调用
          → 检查 AppArmor profile (path-based)           │
        → bpf_lsm_run(inode, ...)                       │
          → BPF 程序决定                                 │
        → 如果有任何一个返回 -EPERM → 立即返回用户态       │
```

---

## 6. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| LSM_HOOK 宏 | `lsm_hook_defs.h` (478行) | 250+ hook 的统一定义 |
| security_hook_heads | `security.c:140` | 每个 hook 的 hlist 链表 |
| static call 跳板 | `security.c:144-156` | 单模块的零开销直接跳转 |
| security_xxx() 包装 | `security.c:451-455` | 对内核暴露的调用入口 |
| call_int_hook | `security.c:439` | 遍历 hlist，任何模块拒绝即停止 |

## 下一篇文章

→ [第2篇：LSM 模块化与初始化 — DEFINE_LSM、security_add_hooks、ordered_lsm_init](./02-lsm-init.md)
