# 第5篇：Linux 内核零信任 — Lockdown、Yama、BPF LSM、Landlock

> 源码：`security/lockdown/lockdown.c` `security/yama/yama_lsm.c` `kernel/bpf/bpf_lsm.c` `security/landlock/fs.c` `security/landlock/ruleset.c` `security/landlock/cred.h` | 头文件：`include/linux/lsm_hooks.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. Lockdown LSM — 内核功能完整性

Lockdown 限制**即使是 root 也不能执行的操作**。两个模式：

### 1.1 内核实现

```c
// security/lockdown/lockdown.c
static int lockdown_is_locked_down(enum lockdown_reason what)
{
    if (kernel_locked_down >= what)
        return -EPERM;
    return 0;
}

static struct security_hook_list lockdown_hooks[] = {
    LSM_HOOK_INIT(locked_down, lockdown_is_locked_down),
};
```

### 1.2 检查链

例如 `ioperm()` 或其它调用调用：

```c
int security_locked_down(enum lockdown_reason what)
{
    return call_int_hook(locked_down, 0, what);
    // → lockdown_is_locked_down(what) → kernel_locked_down >= what?
}
```

```
/sys/kernel/security/lockdown → "none" / "integrity" / "confidentiality"

integrity:  阻止修改运行中内核映像
            禁止 MSR 访问、kexec、休眠、模块参数修改

confidentiality: integrity 的全部 + 禁止提取内核内存
            禁止 /dev/mem, /dev/kmem, /proc/kcore, kprobe, BPF 读取内核内存
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
    case YAMA_SCOPE_DISABLED:   // 0 — 无限制，传统行为
        return 0;
    case YAMA_SCOPE_RELATIONAL: // 1 — 只允许祖先↔后代 ptrace（默认）
        if (task_is_descendant(child, current))
            return 0;
        if (!ns_capable(current_user_ns(), CAP_SYS_PTRACE))
            return -EPERM;
        return 0;
    case YAMA_SCOPE_RESTRICTED: // 2 — 还需要 CAP_SYS_PTRACE
        if (task_is_descendant(child, current) && !ns_capable(child_cred->user_ns, CAP_SYS_PTRACE))
            return -EPERM;
        return 0;
    }
}
```

**`/proc/sys/kernel/yama/ptrace_scope` 值**：

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
    // 某些 hook 可以睡眠（例如 bprm_creds_for_exec）
}

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
        return -EPERM;   // ★ 拒绝以 "mal" 开头的进程打开任何文件
    return 0;
}
```

**注册链**：BPF LSM 用 `bpf_lsm_init` 注册到 LSM 框架，然后 `bpftool prog load ... type lsm` 将一个 BPF 程序附加到对应的 LSM hook。每次 `security_xxx()` 调用时，`call_int_hook` 通过 `static_call` 调用 BPF 程序的 callback。

---

## 4. Landlock — 非特权自主限制

Landlock 是唯一一个**非特权用户自己可以使用的 LSM**（不需要 root）。进程可以给自己创建规则集，限制自己之后能访问的文件和网络。

### 4.1 用户态使用流程

```c
// 不需要 root!
struct landlock_ruleset_attr attr = { .handled_access_fs = ACCESS_FS_ROUGHLY_READ };
int ruleset_fd = landlock_create_ruleset(&attr, 0, 0);

struct landlock_path_beneath_attr path_beneath = {
    .allowed_access = ACCESS_FS_READ_FILE | ACCESS_FS_READ_DIR,
    .parent_fd = open("/home", O_PATH),
};
landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0);
prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
landlock_restrict_self(ruleset_fd, 0);
// ★ 生效! 当前进程再也读不到 /home 以外的文件
```

### 4.2 内核实现

**规则集创建**：

```c
// security/landlock/ruleset.c:56-76
struct landlock_ruleset *
landlock_create_ruleset(const access_mask_t fs_access_mask,
                        const access_mask_t net_access_mask,
                        const access_mask_t scope_mask)
{
    if (!fs_access_mask && !net_access_mask && !scope_mask)
        return ERR_PTR(-ENOMSG);  // ★ 空规则集无意义, 拒绝
    new_ruleset = create_ruleset(1);  // 分配 1 层深度的规则集
    if (fs_access_mask)
        landlock_add_fs_access_mask(new_ruleset, fs_access_mask, 0);
    return new_ruleset;
}
```

**获取当前 domain**：

```c
// security/landlock/cred.h:80-83
static inline struct landlock_ruleset *landlock_get_current_domain(void)
{
    return landlock_cred(current_cred())->domain; // ★ 从 current->security 取
}

// 检查是否有 Landlock:
static inline bool landlocked(const struct task_struct *const task)
{
    if (task == current)
        return !!landlock_get_current_domain();
    // ...
}
```

**`hook_file_open`** — 文件打开检查：

```c
// security/landlock/fs.c:1740-1806
static int hook_file_open(struct file *const file)
{
    // ① 获取当前进程的 Landlock domain
    const struct landlock_cred_security *const subject =
        landlock_get_applicable_subject(file->f_cred, any_fs, NULL);
    if (!subject) return 0;  // 无 domain → 不检查

    // ② 计算所需权限 (open 请求 + 后续可选操作)
    open_access_request = get_required_file_open_access(file);
    full_access_request = open_access_request | LANDLOCK_ACCESS_FS_TRUNCATE;

    // ③ ★ 核心: 检查 inode 层 (文件在允许的 hierarchy 下?)
    if (is_access_to_paths_allowed(subject->domain, &file->f_path, ...))
        allowed_access = full_access_request;

    // ④ 如果请求不在允许集合中 → 拒绝
    if (!access_mask_subset(open_access_request, allowed_access))
        return -EACCES;

    return 0;
}
```

**Landlock 在 LSM 中的注册**：

```c
// fs.c:1970
LSM_HOOK_INIT(file_open, hook_file_open),
```

### 4.3 Landlock 的设计原则

```
① 由调用者限制自己 — 不需要 root, 不需要 CAP_SYS_ADMIN
② 累加限制 — 只能加规则, 不能撤销
③ 规则集绑定到文件层次 — /home 的子孙都被限制
④ 基于路径 + inode — 不依赖标签 (无需 SELinux/AppArmor)
⑤ 可选 — 可选择性加载 (默认开启，但每个进程自己决定是否使用)
⑥ 无状态影响 — Landlock 不改变任何文件/进程的标签
```

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

5 篇文章从 LSM_HOOK 宏展开 + static_call 零开销跳转 + call_int_hook 引擎 → DEFINE_LSM/ordered_lsm_init 模块化初始化 → POSIX Capabilities 的 41 个能力位 + 5 种能力集 + file caps + exec 转换 + securebits → SELinux 的 Type Enforcement + AVC 缓存(99% 命中率) + security_compute_av 策略查询 + AppArmor 的路径匹配 + per-file allow 缓存 → Lockdown(完整性+机密性) + Yama(ptrace范围) + BPF LSM(动态 eBPF 策略) + Landlock(非特权自主限制)，完整覆盖了 Linux 内核安全子系统从框架设计到具体实现的完整技术栈。

| 篇 | 内容 | 核心文件 |
|---|------|---------|
| 01-lsm-core | LSM_HOOK 宏 × 3 展开 + static_call + call_int_hook | `security.c:107-496`, `lsm_hook_defs.h:1-478` |
| 02-lsm-init | DEFINE_LSM + lsm_order_append + ordered_lsm_init | `lsm_init.c:153-393`, `lsm_hooks.h:187-195` |
| 03-capabilities | cap_capable (47 行函数体) + file caps + exec 转换 + securebits | `commoncap.c:67-132,919+`, `cred.h:126-130`, `securebits.h:22-69` |
| 04-mac | SELinux TE + AVC 缓存(avc_has_perm, 1189-1203) + AppArmor 路径匹配(aa_file_perm, 619-671) | `hooks.c:7533-7858`, `avc.c:48-60`, `services.c:1123-1189`, `file.c:619-671`, `path.c:202-222` |
| 05-zero-trust | Lockdown/Yama/BPF LSM/Landlock hook_file_open(1740-1806) | `lockdown.c`, `yama_lsm.c`, `bpf_lsm.c`, `landlock/fs.c:1740-1806`, `landlock/ruleset.c:56-76`, `landlock/cred.h:80-83` |
