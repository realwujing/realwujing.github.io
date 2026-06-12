# 第5篇：Linux 内核零信任 — Lockdown、Yama、BPF LSM、Landlock

> 源码：`security/lockdown/` `security/yama/` `security/landlock/` `kernel/bpf/bpf_lsm.c` | 头文件：`include/linux/lsm_hooks.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. Lockdown LSM — 内核功能完整性

Lockdown 限制**甚至是 root 也不能执行的操作**。它有两个模式：

```
/sys/kernel/security/lockdown → "none" / "integrity" / "confidentiality"

integrity:  阻止修改运行中内核映像
            禁止 MSR 访问、kexec、休眠、模块参数修改

confidentiality: integrity 的全部 + 禁止提取内核内存
            禁止 /dev/mem, /dev/kmem, /proc/kcore, kprobe, BPF 读取内核内存
```

### 内核实现（`security/lockdown/lockdown.c`）：

```c
static int lockdown_is_locked_down(enum lockdown_reason what)
{
    // 检查内核参数: lock_down_reason != LOCKDOWN_NONE
    if (kernel_locked_down >= what)
        return -EPERM;
    return 0;
}

// 注册到 LSM 框架
static struct security_hook_list lockdown_hooks[] = {
    LSM_HOOK_INIT(locked_down, lockdown_is_locked_down),
};
```

**检查链**：例如 `ioperm()` 或其底层实现调用：

```c
int security_locked_down(enum lockdown_reason what)
{
    return call_int_hook(locked_down, 0, what);  // → lockdown_is_locked_down
}
```

---

## 2. Yama LSM — ptrace 范围限制

防止恶意进程通过 `ptrace` 从非相关进程提取数据或注入代码。

```c
// security/yama/yama_lsm.c
static int yama_ptrace_access_check(struct task_struct *child,
                                    unsigned int mode)
{
    switch (ptrace_scope) {
    case YAMA_SCOPE_DISABLED:   // 无限制，传统行为
        return 0;
    case YAMA_SCOPE_RELATIONAL: // ptrace 只允许父子关系或 CAP_SYS_PTRACE
        if (task_is_descendant(child, current))
            return 0;
        return -EPERM;
    case YAMA_SCOPE_RESTRICTED: // ta 还需要开启 CAP_SYS_PTRACE，更严格
        if (!ns_capable(__task_cred(child)->user_ns, CAP_SYS_PTRACE))
            return -EPERM;
        // ...
    }
}
```

**ptrace_scope 值**（`/proc/sys/kernel/yama/ptrace_scope`）：

| 值 | 行为 |
|----|------|
| 0 | `YAMA_SCOPE_DISABLED` — 无限制 |
| 1 | `YAMA_SCOPE_RELATIONAL` — 只允许祖先↔后代 ptrace（默认） |
| 2 | `YAMA_SCOPE_RESTRICTED` — 还需要 CAP_SYS_PTRACE |
| 3 | 完全禁用 ptrace |

---

## 3. BPF LSM — eBPF 驱动的安全策略

BPF LSM 允许用 BPF 程序**动态检查**任何 LSM hook 点，无需修改内核或加载内核模块。

```c
// kernel/bpf/bpf_lsm.c
const struct bpf_prog_ops lsm_prog_ops = {
    .test_run = bpf_lsm_test_run,
};

static bool bpf_lsm_is_sleepable_hook(u32 btf_id)
{
    // 标记某些 hook 可以睡眠（例如 bprm_creds_for_exec）
    // 允许 BPF 程序在这些 hook 中访问更多 helper
}

// BPF 程序的 helper 集合:
bpf_lsm_func_proto(enum bpf_func_id func_id, ...)
{
    // 限制: 只允许 map 操作和少数几个辅助函数
    // 禁止: 不能直接修改内核状态（tracing 类型）
}
```

**使用方式**：

```c
SEC("lsm/file_open")
int BPF_PROG(restrict_open, struct file *file)
{
    char comm[16];
    bpf_get_current_comm(&comm, sizeof(comm));
    if (comm[0] == 'm' && comm[1] == 'a' && comm[2] == 'l')
        return -EPERM;          // ★ 拒绝以 "mal" 开头的进程打开任何文件
    return 0;
}
```

**注册链**：BPF LSM 用 `bpf_lsm_init` 注册到 LSM 框架，然后写 `bpftool prog load ... type lsm` 将 BPF 程序附加到对应的 LSM hook。每次 `security_xxx()` 调用时，`call_int_hook` 会调用 BPF 程序的 callback。

---

## 4. Landlock — 非特权自主限制

Landlock 是唯一一个**非特权用户自己可以使用的 LSM**（不需要 root）。进程可以给自己创建规则集，禁止自己访问某些文件或网络。

```c
// 用户态使用 (无需 root!)
struct landlock_ruleset_attr attr = { .handled_access_fs = ACCESS_FS_ROUGHLY_READ };
int ruleset_fd = landlock_create_ruleset(&attr, 0, 0);

struct landlock_path_beneath_attr path_beneath = {
    .allowed_access = ACCESS_FS_READ_FILE | ACCESS_FS_READ_DIR,
    .parent_fd = open("/home", O_PATH),
};
landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0);
prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
landlock_restrict_self(ruleset_fd, 0);   // ★ 生效！当前进程再也读不到 /home 以外的文件
```

**内核实现**（`security/landlock/`）：

```c
// security/landlock/ruleset.c
static int hook_file_open(struct file *file)
{
    // ① 获取当前进程的 Landlock domain
    const struct landlock_ruleset *dom = landlock_get_current_domain();

    // ② 检查路径是否在规则的允许范围内
    // 如果路径在 /home 下 → 操作被允许
    // 如果路径在 /etc 下 → 拒绝
    return check_access_path(dom, path, access_request);
}
```

**核心特性——由调用者限制自己**：
- 不需要 root、不需要 `CAP_SYS_ADMIN`
- 是累加限制：只能加规则，不能撤销
- 规则集与文件层次绑定：`/home` 的子孙都被限制

---

## 5. 总结 — 五层安全深度

```
Level 0: DAC (传统 Unix rwx)
Level 1: POSIX Capabilities (root 权限拆分)
Level 2: Lockdown (禁止 root 修改运行中内核)
Level 3: Yama (ptrace 隔离)
Level 4: MAC (SELinux/AppArmor) — 标签/路径强制访问控制
Level 5: BPF LSM + Landlock — 用户态动态安全策略

Level 0-2 是系统管理员策略 (需要 root)
Level 3-4 是系统管理员 + 安全工程师策略
Level 5 允许用户自己限制自己 (不需要 root)
```

## 系列结语

5 篇文章从 LSM_HOOK 宏展开 + static call 优化 + 模块初始化，到 POSIX Capabilities 的能力集位图 + exec 转换，到 SELinux 的 Type Enforcement + AVC 缓存 + AppArmor 的路径匹配，再到 Lockdown / Yama / BPF LSM / Landlock 四个现代零信任机制，完整覆盖了 Linux 内核安全子系统从框架设计到具体实现的全栈。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-lsm-core | LSM_HOOK 宏 + static call + call_int_hook | `security.c`, `lsm_hook_defs.h` |
| 02-lsm-init | DEFINE_LSM + security_add_hooks + ordered_lsm_init | `lsm_init.c` |
| 03-capabilities | POSIX Cap + file caps + exec 转换 | `commoncap.c`, `capability.h` |
| 04-mac | SELinux TE + AVC + AppArmor 路径 | `selinux/hooks.c`, `apparmor/lsm.c` |
| 05-zero-trust | Lockdown/Yama/BPF LSM/Landlock | `lockdown/`, `yama/`, `bpf_lsm.c`, `landlock/` |
