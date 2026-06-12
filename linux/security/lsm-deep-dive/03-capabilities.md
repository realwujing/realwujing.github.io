# 第3篇：POSIX Capabilities 内核实现 — 能力集、位图与 exec 转换

> 源码：`security/commoncap.c` | 头文件：`include/linux/capability.h` `include/uapi/linux/capability.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. 能力集 — 精细化的 root 权限

传统 Unix 只有两级权限：0=root, 非0=普通用户。`sudo` 解决了一部分问题但不够。POSIX capabilities 将"root 能做的事"拆分为几十个独立位：

```c
// include/uapi/linux/capability.h
// 示例能力（总共 CAP_LAST_CAP=42 个）
#define CAP_CHOWN            0   // 改变文件所有者
#define CAP_DAC_OVERRIDE     1   // 绕过文件权限检查
#define CAP_DAC_READ_SEARCH  2   // 绕过文件读权限
#define CAP_FOWNER           3   // 改变文件所有者
#define CAP_KILL             5   // 发信号给任意进程
#define CAP_SETUID           7   // 任意 setuid 操作
#define CAP_NET_RAW          13  // 使用原始 socket
#define CAP_SYS_ADMIN        21  // ★ 传统的 root-only 操作
#define CAP_SYS_PTRACE       19  // 追踪任意进程
#define CAP_SYS_RESOURCE     24  // 超越资源限制
#define CAP_AUDIT_CONTROL    30  // 控制内核审计系统
#define CAP_BPF              39  // 加载 BPF 程序
#define CAP_PERFMON          38  // 使用 perf_event
```

**三种能力集（`struct cred` 中）**：

| 集合 | 含义 | 何时生效 |
|------|------|---------|
| `cap_effective` | **当前生效**的能力 | 所有权限检查都看它 |
| `cap_permitted` | 任务**允许拥有**的能力 | 能力的上限集 |
| `cap_inheritable` | 可在 exec 时**传递**给新程序的能力 | `setcap cap_net_bind_service+ei` |

---

## 2. kernel_cap_t — 位图存储

```c
// include/uapi/linux/capability.h
#define CAP_LAST_CAP  42
#define _LINUX_CAPABILITY_U32S_3  2   // 需要两个 u32 存储 42+ 位

typedef struct kernel_cap_struct {
    __u32 cap[_LINUX_CAPABILITY_U32S_3];   // 64 位 = 两个 u32
} kernel_cap_t;
```

**位操作**：

```c
#define CAP_TO_MASK(x)   (1u << ((x) & 31))   // cap 位 → u32 掩码
#define CAP_FS_MASK_B0() (1 << CAP_FSETID)     // 文件系统相关的 cap 集合

// 检查能力:
static inline bool ns_capable(struct user_namespace *ns, int cap)
{
    return security_capable(current_cred(), ns, cap, CAP_OPT_NONE) == 0;
}

// 进一步展开:
int security_capable(const struct cred *cred, ...) {
    // ① 检查 effective 位是否有 cap
    // ② 如果 effective 为 0 → 检查 permitted 和 inheritable
    // ③ LSM 检查: selinux_capable / apparmor_capable → 可拒绝
    // ④ 返回 0 (允许) 或 -EPERM
}
```

---

## 3. Exec 时的能力转换

**文件能力 (file capabilities)** 存储在文件的 `security.capability` xattr 中。`commoncap.c` 的 `cap_bprm_creds_for_exec` 在 exec 时读取：

```c
// security/commoncap.c
int cap_bprm_creds_for_exec(struct linux_binprm *bprm)
{
    // ① ★ 计算新进程的能力集
    //   pP' = (old P & old B) | (fI & new pI)
    //   pE' = pP'  (if SECBIT_NO_SETUID_FIXUP 未置)
    //   pI' = pI  (不变)

    // ② 如果 binary 有 setuid/setgid:
    //   清除所有的 cap_effective
    //   如果新 uid=0 → 可能恢复 CAP_SETUID 等

    // ③ 如果内核命令行有 no_file_caps → 直接返回

    // ④ ★ 应用 LSM 策略
    security_bprm_creds_for_exec(bprm);
}
```

**能力继承链**：

```
setcap cap_net_bind_service+ep /path/to/binary
    ↓
文件 xattr: cap_net_bind_service=ep

exec binary:
  cap_effective |= cap_net_bind_service      ← effective + permitted 都被挂载
  cap_permitted  |= cap_net_bind_service

子进程继承:
  子进程的 cap_permitted = cap_effective = {cap_net_bind_service}
  子进程可以用这个能力 bind 低端口，但不需要 root
```

---

## 4. User Namespace 隔离

```c
// include/linux/capability.h
static inline bool ns_capable(struct user_namespace *ns, int cap)
{
    // ★ user namespace 隔离:
    // 在一个 user namespace 内拥有 CAP_SYS_ADMIN 不代表在所有 namespace 拥有它
}

// 检查是否"真正" root (初始 user namespace 的):
bool capable(int cap)                       // 检查初始 user namespace
bool ns_capable_setid(int cap)              // 检查 + 允许 setid
```

**创建新 user namespace 时**：进程获得新 namespace 内的所有能力，但这些能力**仅在**该 namespace 内有效——不能影响 host。

---

## 5. 完整能力检查路径

```
用户态: bind 到端口 80
  → sys_bind
    → inet_bind
      → ns_capable(sock_net(sk)->user_ns, CAP_NET_BIND_SERVICE)  ← ★ 能力检查
        → security_capable(cred, ns, CAP_NET_BIND_SERVICE, ...)
          → cap_capable (commoncap.c):
            if (cred->cap_effective & CAP_NET_BIND_SERVICE) {
                return 0;  // 允许
            } else {
                return -EPERM;  // 拒绝
            }
```

**这条路径没有 LSM 干预**——能力检查本身就是 LSM 模块 `commoncap.c` 的工作。

---

## 6. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| CAP 定义 | `uapi/linux/capability.h` | 42 个能力位 |
| kernel_cap_t | `capability.h` | 2 个 u32 位图 |
| cap_effective | `struct cred` | 当前拥有的能力 |
| cap_permitted | `struct cred` | 能力上限 |
| file caps | xattr `security.capability` | exec 时附加 |
| cap_bprm_creds_for_exec | `commoncap.c` | exec 转换公式 |
| ns_capable | `capability.h` | user namespace 隔离的能力检查 |

## 下一篇文章

→ [第4篇：SELinux 与 AppArmor 强制访问控制](./04-mac.md)
