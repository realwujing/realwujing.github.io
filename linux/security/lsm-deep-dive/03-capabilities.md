# 第3篇：POSIX Capabilities 内核实现 — 能力集位图、exec 转换与 securebits

> 源码：`security/commoncap.c` `include/linux/cred.h` | 头文件：`include/uapi/linux/capability.h` `include/uapi/linux/securebits.h`

---

## 1. 能力集 — 精细化的 root 权限

传统 Unix 只有两级权限：uid=0=root，非0=普通用户。POSIX capabilities 将"root 能做的事"拆分为 41 个独立位：

```c
// include/uapi/linux/capability.h
// 总共 CAP_LAST_CAP = 40 (line 423)
#define CAP_CHOWN              0   // 改变文件所有者
#define CAP_DAC_OVERRIDE       1   // 绕过文件权限检查
#define CAP_DAC_READ_SEARCH    2   // 绕过文件读权限  
#define CAP_FOWNER             3   // 改变文件所有者
#define CAP_KILL               5   // 发信号给任意进程
#define CAP_SETUID             7   // 任意 setuid 操作
#define CAP_NET_RAW           13   // 使用原始 socket
#define CAP_SYS_ADMIN         21   // ★ 传统的 root-only 操作
#define CAP_SYS_PTRACE        19   // 追踪任意进程
#define CAP_SYS_RESOURCE      24   // 超越资源限制
#define CAP_AUDIT_CONTROL     30   // 控制内核审计系统
#define CAP_BPF               39   // 加载 BPF 程序
#define CAP_PERFMON           38   // 使用 perf_event
#define CAP_CHECKPOINT_RESTORE 40  // ★ 最后一个能力
```

**五种能力集（`struct cred` 中）**：

```c
// include/linux/cred.h:126-130
struct cred {
    kernel_cap_t cap_inheritable; /* ★ 可在 exec 时传递给新程序的能力 */
    kernel_cap_t cap_permitted;  /* ★ 任务允许拥有的能力上限 */
    kernel_cap_t cap_effective;  /* ★ 当前生效的能力 — 所有权限检查都看它 */
    kernel_cap_t cap_bset;       /* ★ 能力绑定集 — 继承帽 */
    kernel_cap_t cap_ambient;    /* ★ 环境能力集 — 非特权程序 exec 时可继承 */
};
```

| 集合 | 含义 | exec 时行为 |
|------|------|-----------|
| `cap_effective` | **当前生效**的能力 | 重新计算 |
| `cap_permitted` | 任务**允许拥有**的上限 | 由文件能力 + pP + pI 计算 |
| `cap_inheritable` | 可在 exec 时**传递**给新程序 | 参与新 pP 计算 |
| `cap_bset` | 能力绑定集——全局上限 | 不可增长 |
| `cap_ambient` | 环境能力集——非特权也可继承 | 保留到新 effective 中 |

---

## 2. `kernel_cap_t` — 位图存储

```c
// include/uapi/linux/capability.h
#define CAP_LAST_CAP  40
#define _LINUX_CAPABILITY_U32S_3  2   // 需要两个 u32 存储 41 位

typedef struct kernel_cap_struct {
    __u32 cap[_LINUX_CAPABILITY_U32S_3];   // 64-bit = 两个 u32
} kernel_cap_t;
```

**位操作**：
```c
#define CAP_TO_MASK(x)   (1u << ((x) & 31))
#define cap_raised(c, cap)   ((c).cap[CAP_TO_INDEX(cap)] & CAP_TO_MASK(cap))
```

---

## 3. `cap_capable` — 实际能力检查

```c
// security/commoncap.c:124-132
int cap_capable(const struct cred *cred, struct user_namespace *target_ns,
                int cap, unsigned int opts)
{
    const struct user_namespace *cred_ns = cred->user_ns;
    int ret = cap_capable_helper(cred, target_ns, cred_ns, cap);
    return ret;
}
```

```c
// security/commoncap.c:67-100 — cap_capable_helper 内部遍历 user namespace 链
static inline int cap_capable_helper(const struct cred *cred,
                                     struct user_namespace *target_ns,
                                     const struct user_namespace *cred_ns,
                                     int cap)
{
    struct user_namespace *ns = target_ns;

    for (;;) {
        // ★ 在同一个 namespace: 直接检查 cap_effective 位
        if (likely(ns == cred_ns))
            return cap_raised(cred->cap_effective, cap) ? 0 : -EPERM;

        // ★ 如果已经走到底层 → 没有权限
        if (ns->level <= cred_ns->level)
            return -EPERM;

        // ★ parent namespace 的 owner 拥有所有能力
        if ((ns->parent == cred_ns) && uid_eq(ns->owner, cred->euid))
            return 0;

        // 继续向上遍历 user namespace 树
        ns = ns->parent;
    }
}
```

**user namespace 链**：创建子 namespace 的进程在该 namespace 内拥有所有能力，但对父 namespace 没有权限。

---

## 4. `cap_bprm_creds_from_file` — Exec 时的能力转换

```c
// security/commoncap.c:919+ (核心函数)
int cap_bprm_creds_from_file(struct linux_binprm *bprm, const struct file *file)
{
    // ★ 新 pP' = (old pP & old pB) | (fP & old pB) | (fI & old pI)
    // ★ 如果 fE (file effective) 存在 → 新 pE' = pP'
    // ★ 否则 → 新 pE' = old pE & old pP (不变)

    // ① 读取文件的 security.capability xattr
    ret = get_vfs_caps_from_disk(mnt_idmap(file->f_path.mnt),
                                  file->f_path.dentry, &vcaps);

    // ② 如果有文件能力:
    //   new->cap_permitted = (old.cap_bset & old.cap_permitted) |
    //                        (old.cap_bset & file.cap_permitted) |
    //                        (old.cap_inheritable & file.cap_inheritable)

    // ③ 如果有文件能力+fE (effective):
    //   new->cap_effective = new->cap_permitted

    // ④ 检查 securebits: SECBIT_NO_SETUID_FIXUP, SECBIT_KEEP_CAPS
    // ⑤ 处理 setuid root (传统 root upgrade)
    // ⑥ ★ 应用 ambient caps (cap_ambient → 直接加到 pE)
}
```

**能力继承链示例**：

```
setcap cap_net_bind_service+ep /path/to/binary
    ↓
文件 xattr: security.capability = {fP=cap_net_bind_service, fE=cap_net_bind_service}

exec binary:
  cap_permitted  |= cap_net_bind_service       ← fE 直接提升
  cap_effective  |= cap_net_bind_service       ← fE 直接提升

子进程:
  可以用这个能力 bind 低端口，但不需要 root
```

---

## 5. `securebits` — root 权限的特殊控制

```c
// include/uapi/linux/securebits.h
#define SECBIT_NOROOT                       22  // UID 0 没有特殊权限
#define SECBIT_NOROOT_LOCKED                23  // 锁定版本
#define SECBIT_NO_SETUID_FIXUP              32  // setuid 不触发能力修复
#define SECBIT_NO_SETUID_FIXUP_LOCKED       33
#define SECBIT_KEEP_CAPS                    44  // uid 转换后保留能力
#define SECBIT_KEEP_CAPS_LOCKED             45
#define SECBIT_NO_CAP_AMBIENT_RAISE         51  // 不能增加 ambient caps
```

**场景含义**：
- `SECBIT_NOROOT`：即便 uid=0 也不给全能力——只能通过 file capabilities 获取需要的
- `SECBIT_KEEP_CAPS`：`setuid()` 不自动清空有效能力集
- `SECBIT_NO_SETUID_FIXUP`：`setuid-root` 二进制不触发能力修复
- 每个 `_LOCKED` 版本：对应位不可被后续 `exec` 修改

---

## 6. capability LSM 的 hooks 注册

```c
// security/commoncap.c:1490-1508
static struct security_hook_list capability_hooks[] __ro_after_init = {
    LSM_HOOK_INIT(capable, cap_capable),                   // ★ 核心检查
    LSM_HOOK_INIT(settime, cap_settime),
    LSM_HOOK_INIT(ptrace_access_check, cap_ptrace_access_check),
    LSM_HOOK_INIT(ptrace_traceme, cap_ptrace_traceme),
    LSM_HOOK_INIT(capget, cap_capget),
    LSM_HOOK_INIT(capset, cap_capset),
    LSM_HOOK_INIT(bprm_creds_from_file, cap_bprm_creds_from_file), // ★ exec 转换
    LSM_HOOK_INIT(inode_need_killpriv, cap_inode_need_killpriv),
    LSM_HOOK_INIT(inode_killpriv, cap_inode_killpriv),
    LSM_HOOK_INIT(inode_getsecurity, cap_inode_getsecurity),
    LSM_HOOK_INIT(mmap_addr, cap_mmap_addr),
    LSM_HOOK_INIT(task_fix_setuid, cap_task_fix_setuid),
    LSM_HOOK_INIT(task_prctl, cap_task_prctl),
    LSM_HOOK_INIT(task_setscheduler, cap_task_setscheduler),
    LSM_HOOK_INIT(task_setioprio, cap_task_setioprio),
    LSM_HOOK_INIT(task_setnice, cap_task_setnice),
    LSM_HOOK_INIT(vm_enough_memory, cap_vm_enough_memory),
};
```

---

## 7. 完整能力检查路径

```
用户态: bind 到端口 80
  → sys_bind → inet_bind
    → ns_capable(sock_net(sk)->user_ns, CAP_NET_BIND_SERVICE)
      → security_capable(cred, ns, CAP_NET_BIND_SERVICE, ...)
        → call_int_hook(capable, ...)
          → ★ cap_capable(cred, ns, CAP_NET_BIND_SERVICE, ...) [commoncap.c:124]
            → cap_capable_helper: cap_raised(cred->cap_effective, CAP_NET_BIND_SERVICE)?
              → yes → 0 (允许)
              → no → -EPERM (拒绝)
```

---

## 8. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| 能力定义 | `uapi/linux/capability.h:423` | 41 个能力位 (0-40) |
| kernel_cap_t | `capability.h` | 2 个 u32 位图 (64-bit) |
| cap_effective | `cred.h:128` | 当前拥有的能力 |
| cap_permitted | `cred.h:127` | 能力上限 |
| cap_bset | `cred.h:129` | 绑定集——全局上限 |
| cap_ambient | `cred.h:130` | 环境能力——非特权继承 |
| cap_capable | `commoncap.c:124-132` | 能力检查的 LSM 入口 |
| cap_bprm_creds_from_file | `commoncap.c:919+` | exec 时的能力转换 |
| securebits | `securebits.h:22-69` | 特殊行为的位开关 |

## 下一篇文章

→ [第4篇：SELinux 与 AppArmor 强制访问控制](./04-mac.md)
