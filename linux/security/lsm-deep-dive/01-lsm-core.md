# 第1篇：LSM 框架与 Hook 调用链 — 从宏到静态调用

> 源码：`security/security.c` | 头文件：`include/linux/lsm_hook_defs.h` `include/linux/lsm_hooks.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. LSM 的核心理念

Linux 内核中对每类关键操作（打开文件、发送信号、挂载文件系统）都有一个 **Hook 点**。LSM 框架在 Hook 点调用注册的模块，任何模块返回非零 → 操作被拒绝。

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
// include/linux/lsm_hook_defs.h — 共 277 个 hook（lines 29-478）
LSM_HOOK(int, 0, capable, const struct cred *cred,
         struct user_namespace *ns, int cap, unsigned int opts)

LSM_HOOK(int, 0, file_permission, struct file *file, int mask)

LSM_HOOK(int, 0, inode_permission, struct inode *inode, int mask)

LSM_HOOK(int, 0, bprm_creds_for_exec, struct linux_binprm *bprm)

LSM_HOOK(int, 0, ptrace_access_check, struct task_struct *child,
         unsigned int mode)

LSM_HOOK(int, 0, socket_create, int family, int type, int protocol, int kern)

LSM_HOOK(int, 0, bdev_alloc_security, struct block_device *bdev)
```

**第一个 hook** (`lsm_hook_defs.h:29`)：`binder_set_context_mgr`  
**最后一个 hook** (`lsm_hook_defs.h:477-478`)：`bdev_setintegrity`  
**总计**：277 个 hook

**参数含义**：
- `int`：返回类型（绝大部分是 int，返回 0=允许，负值=拒绝）
- `0`：默认返回值（如果没有模块注册，各 hook 返回 0=允许）
- `XXX`：hook 名称
- 其余：hook 的参数列表

---

## 3. 宏展开——生成三层代码结构

LSM_HOOK 在 `security/security.c` 中被 `#include` 了 **三次**，每次都在不同的宏定义下展开，生成三个不同的代码块。

### 3.1 展开一：static call 静态跳转

```c
// security/security.c:107-126
#define DEFINE_LSM_STATIC_CALL(NUM, NAME, RET, ...)                  \
    DEFINE_STATIC_CALL_NULL(LSM_STATIC_CALL(NAME, NUM),              \
                            *((RET(*)(__VA_ARGS__))NULL));           \
    static DEFINE_STATIC_KEY_FALSE(SECURITY_HOOK_ACTIVE_KEY(NAME, NUM));

#define LSM_HOOK(RET, DEFAULT, NAME, ...)                            \
    LSM_DEFINE_UNROLL(DEFINE_LSM_STATIC_CALL, NAME, RET, __VA_ARGS__)
#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
```

生成 277 个 `DEFINE_STATIC_CALL_NULL` + 277 个 `DEFINE_STATIC_KEY_FALSE`。每个 hook 有 `MAX_LSM_COUNT` 个槽位（每个注册的模块占用一个槽）。

**重要**：`LSM_DEFINE_UNROLL` 对每个 hook 生成 `MAX_LSM_COUNT` 次 `DEFINE_STATIC_CALL` 调用（每个加载的模块一个）。这是单模块下的零间接跳转优化——直接通过 `arch_static_call` 跳转到模块的回调。

### 3.2 展开二：static call 表快照

```c
// security/security.c:139-154
struct lsm_static_calls_table
    static_calls_table __ro_after_init = {
#define INIT_LSM_STATIC_CALL(NUM, NAME)                              \
    (struct lsm_static_call) {                                       \
        .key = &STATIC_CALL_KEY(LSM_STATIC_CALL(NAME, NUM)),        \
        .trampoline = LSM_HOOK_TRAMP(NAME, NUM),                     \
        .active = &SECURITY_HOOK_ACTIVE_KEY(NAME, NUM),             \
    },
#define LSM_HOOK(RET, DEFAULT, NAME, ...)                            \
    .NAME = {                                                        \
        LSM_DEFINE_UNROLL(INIT_LSM_STATIC_CALL, NAME)                \
    },
#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
};
```

生成一个 `struct lsm_static_calls_table` 结构体，每个 hook name 是一个 `struct lsm_static_call[MAX_LSM_COUNT]` 数组。这个表在模块注册时由 `security_add_hooks` 更新跳转目标。

### 3.3 展开三：`security_xxx()` 包装函数

```c
// security/security.c:447-455
#define LSM_RET_DEFAULT(NAME) (NAME##_default)
#define DECLARE_LSM_RET_DEFAULT_void(DEFAULT, NAME)
#define DECLARE_LSM_RET_DEFAULT_int(DEFAULT, NAME) \
    static const int __maybe_unused LSM_RET_DEFAULT(NAME) = (DEFAULT);
#define LSM_HOOK(RET, DEFAULT, NAME, ...) \
    DECLARE_LSM_RET_DEFAULT_##RET(DEFAULT, NAME)

#include <linux/lsm_hook_defs.h>
#undef LSM_HOOK
```

这里只声明了默认返回值常量。实际的 `security_xxx()` 函数由 `lsm_hook_defs.h` 在**调用点（`security.c` 主文件底部）**使用 `call_int_hook` 或 `call_void_hook` 包装。

---

## 4. `call_int_hook` 和 `call_void_hook` — 静态调用引擎

```c
// security/security.c:466-496
#define __CALL_STATIC_INT(NUM, R, HOOK, LABEL, ...)                 \
do {                                                                 \
    if (static_branch_unlikely(&SECURITY_HOOK_ACTIVE_KEY(HOOK, NUM))) { \
        R = static_call(LSM_STATIC_CALL(HOOK, NUM))(__VA_ARGS__);   \
        if (R != LSM_RET_DEFAULT(HOOK))                              \
            goto LABEL;                                              \
    }                                                                \
} while (0);

#define call_int_hook(HOOK, ...)                                     \
({                                                                   \
    __label__ OUT;                                                   \
    int RC = LSM_RET_DEFAULT(HOOK);                                  \
    LSM_LOOP_UNROLL(__CALL_STATIC_INT, RC, HOOK, OUT, __VA_ARGS__); \
OUT:                                                                 \
    RC;                                                              \
})
```

**工作原理**：
1. `static_branch_unlikely` 检查这个模块是否激活
2. 如果激活 → `static_call` 直接跳转到模块的回调（零开销间接跳转）
3. `LSM_LOOP_UNROLL` 展开 `MAX_LSM_COUNT` 次——轮询所有模块
4. 任何模块返回非默认值 → `goto OUT` 立即返回

**单模块优化**：只有一个模块注册时，`static_call` 就是直接跳转，没有 hlist 遍历的开销。

---

## 5. `security_add_hooks` — 模块注册

```c
// security/lsm_init.c:369-380
void __init security_add_hooks(struct security_hook_list *hooks, int count,
                               const struct lsm_id *lsmid)
{
    int i;

    for (i = 0; i < count; i++) {
        hooks[i].lsmid = lsmid;
        if (lsm_static_call_init(&hooks[i]))
            panic("exhausted LSM callback slots with LSM %s\n",
                  lsmid->name);
    }
}
```

**每个 hook 的注册通过 `lsm_static_call_init` 更新 static_call 表**：
- 找到该 hook 在 `static_calls_table` 中的槽位
- 设置 `static_call_update` 指向模块的回调
- 设置 `static_key` 使 `call_int_hook` 中的 `static_branch_unlikely` 为真

---

## 6. `struct lsm_info` — 模块声明

```c
// include/linux/lsm_hooks.h:171-185
struct lsm_info {
    const struct lsm_id *id;       // 模块名 + 唯一 ID
    enum lsm_order order;          // LSM_ORDER_FIRST / LSM_ORDER_LAST
    unsigned long flags;           // LSM_FLAG_EXCLUSIVE / LSM_FLAG_LEGACY_MAJOR / ...
    struct lsm_blob_sizes *blobs;  // 每个对象分配的 blob 大小
    int *enabled;                  // 指向模块的"启用"变量
    int (*init)(void);             // ★ 初始化回调
};
```

```c
// include/linux/lsm_hooks.h:137-141
#define LSM_HOOK_INIT(NAME, HOOK)            \
    {                                        \
        .scalls = static_calls_table.NAME,   \
        .hook = { .NAME = HOOK }             \
    }
```

模块的 hooks 数组使用 `LSM_HOOK_INIT` 宏定义，`.scalls` 指向该 hook 在 static_calls_table 中的槽位数组。

---

## 7. LSM hook 在全局调用链中的位置

```
用户态: open("/etc/shadow", O_RDONLY)
  → do_sys_open → do_file_open → path_openat
    → may_open(inode, MAY_READ, ...)
      → security_inode_permission(inode, MAY_READ)     ← ★ LSM hook
        → call_int_hook(inode_permission, ...)
          → LSM_LOOP_UNROLL: 依次 static_call 每个模块
            → selinux_inode_permission(inode, mask)      Slot 0
              → check SELinux policy (type enforcement)
            → apparmor_inode_permission(inode, mask)     Slot 1  
              → check AppArmor profile (path-based)
            → bpf_lsm_run(...)                            Slot 2
              → BPF program decides
            → 如果有任何一个返回非默认值 → goto OUT → 返回
```

---

## 8. 总结

| 组件 | 关键位置 | 大小/数量 | 作用 |
|------|---------|---------|------|
| LSM_HOOK 宏 | `lsm_hook_defs.h:29-478` | 277 个 | 统一定义所有 hook |
| static call 生成 | `security.c:107-126` | 277×MAX_LSM_COUNT | 零开销跳转 |
| call_int_hook | `security.c:488-496` | — | 轮询所有模块的回调 |
| security_add_hooks | `lsm_init.c:369-380` | — | 注册到 static_call 表 |
| LSM_HOOK_INIT | `lsm_hooks.h:137-141` | — | 模块 hook 数组的声明宏 |

## 下一篇文章

→ [第2篇：LSM 模块化与初始化 — DEFINE_LSM、lsm_order_append、ordered_lsm_init](./02-lsm-init.md)
