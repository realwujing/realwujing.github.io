# 第4篇：SELinux 与 AppArmor — 强制访问控制的内核视角

> 源码：`security/selinux/hooks.c` `security/selinux/avc.c` `security/selinux/ss/services.c` `security/apparmor/lsm.c` `security/apparmor/file.c` `security/apparmor/path.c` | 头文件：`security/selinux/include/security.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. 标签 vs 路径 — 两者的根本分歧

| | SELinux | AppArmor |
|---|---|---|
| **决策依据** | 文件的 **安全上下文标签**（存储在 inode xattr） | 文件的 **绝对路径**（通过路径名匹配规则） |
| **策略存储** | 二进制策略文件（`policy.33`），编译自 `refpolicy` | 文本规则文件（`/etc/apparmor.d/`），人类可读 |
| **应用绑定** | **基于类型**的全部操作，由对象类别定义 | **基于路径** + 每个程序绑定的 profile |
| **A/B 可预测性** | 给定相同的标签和策略→相同的结果 | 不同系统上相同的路径可能有不同的结果 |
| **典型命令** | `ls -Z`, `ps -eZ`, `chcon`, `restorecon` | `aa-status`, `aa-complain`, `aa-enforce` |

---

## 2. SELinux — Type Enforcement

### 2.1 初始化与 hook 注册

SELinux 使用 `DEFINE_LSM(selinux)` 声明，`selinux_init` 由 `ordered_lsm_init` 调用。hook 数组定义：

```c
// security/selinux/hooks.c:7533-7858 — 约 170+ 个 LSM_HOOK_INIT 项
static struct security_hook_list selinux_hooks[] __ro_after_init = {
    LSM_HOOK_INIT(file_permission, selinux_file_permission),
    LSM_HOOK_INIT(inode_permission, selinux_inode_permission),
    LSM_HOOK_INIT(capable, selinux_capable),
    LSM_HOOK_INIT(bprm_creds_for_exec, selinux_bprm_creds_for_exec),
    // ... 170+ hooks ...
};
security_add_hooks(selinux_hooks, ARRAY_SIZE(selinux_hooks), &selinux_lsmid);
```

### 2.2 Type Enforcement 决策链

```c
// security/selinux/hooks.c
static int selinux_inode_permission(struct inode *inode, int mask)
{
    u32 ssid = current_sid();          // ② 主体: 当前进程的 SID
    u32 tsid = inode->i_security->sid; // ③ 客体: 文件 inode 的 SID
    return avc_has_perm(ssid, tsid, SECCLASS_FILE,
                        file_mask_to_av(mask), NULL);
}
```

**TE 规则格式**：

```
# allow <源类型> <目标类型>:<对象类别> { 操作集 };

allow sshd_t etc_t:file { read getattr open };
allow httpd_t httpd_t:process { fork transition sigkill };
```

### 2.3 AVC 缓存 (Access Vector Cache)

```c
// security/selinux/avc.c:48-54
struct avc_entry {
    u32                ssid;      // 源安全 ID
    u32                tsid;      // 目标安全 ID  
    u16                tclass;    // 对象类别 (SECCLASS_FILE=2, etc.)
    struct av_decision avd;       // ★ 预计算的允许集合（位掩码）
};

// security/selinux/avc.c:1189-1203 — 核心入口
int avc_has_perm(u32 ssid, u32 tsid, u16 tclass,
                 u32 requested, struct common_audit_data *auditdata)
{
    struct av_decision avd;
    int rc = avc_has_perm_noaudit(ssid, tsid, tclass, requested, 0, &avd);
    // ★ 先查 AVC 缓存 — 缓存命中就返回
    // ★ 缓存 miss → 查询二进制策略 → 构建新 entry
    return avc_audit(ssid, tsid, tclass, requested, &avd, rc, auditdata);
}
```

### 2.4 策略查询 — `security_compute_av`

```c
// security/selinux/ss/services.c:1123-1189
void security_compute_av(u32 ssid, u32 tsid, u16 orig_tclass,
                         struct av_decision *avd, ...)
{
    scontext = sidtab_search(sidtab, ssid); // 查 SID 表 → 源上下文
    tcontext = sidtab_search(sidtab, tsid); // 查 SID 表 → 目标上下文

    if (!selinux_initialized()) goto allow;  // 策略未加载 → 全允许

    if (ebitmap_get_bit(&policydb->permissive_map, scontext->type))
        avd->flags |= AVD_FLAGS_PERMISSIVE;  // ★ 许可域

    // ★ 核心: context_struct_compute_av 遍历 TE 规则表
    context_struct_compute_av(policydb, scontext, tcontext,
                              tclass, avd, xperms);
}
```

**性能关键**：

```
99%+ 的检查在 AVC 缓存层命中 (rb-tree, ~10% 统计空间)
  → 零策略查询，直接返回预计算的允许掩码

~1% AVC miss → security_compute_av 遍历 TE 规则表
  → 构建新 AVC entry → 缓存 → 返回
```

---

## 3. AppArmor — 路径匹配

### 3.1 核心架构

```c
// security/apparmor/lsm.c:541-544
static int apparmor_file_permission(struct file *file, int mask)
{
    return common_file_perm(OP_FPERM, file, mask);
}
```

进入 `common_file_perm` → `aa_file_perm`：

```c
// security/apparmor/file.c:619-671
int aa_file_perm(const char *op, const struct cred *subj_cred,
                 struct aa_label *label, struct file *file,
                 u32 request, bool in_atomic)
{
    // ① ★ 快速路径: 先从文件缓存中检查
    denied = request & ~fctx->allow;     // fctx->allow = 预先计算的允许掩码
    if (unconfined(label) || __file_is_delegated(flabel) ||
        !denied || __aa_subj_label_is_cached(label, flabel)) {
        goto done;  // ★ 缓存命中 → 允许
    }

    // ② ★ 慢速路径: 重新验证访问
    flabel = aa_get_newest_label(flabel);
    if (path_mediated_fs(file->f_path.dentry))
        error = __file_path_perm(op, subj_cred, label, flabel,
                                 file, request, denied, in_atomic);
    aa_put_label(flabel);
    return error;
}
```

### 3.2 路径解析

```c
// security/apparmor/path.c:202-222
int aa_path_name(const struct path *path, int flags, char *buffer,
                 const char **name, const char **info, ...)
{
    char *str = NULL;
    int error = d_namespace_path(path, buffer, &str, flags, ...);
    *name = str;                     // ★ 输出: 文件的绝对路径
    return error;
}
```

### 3.3 AppArmor profile 示例

```
/usr/sbin/sshd {
    /etc/ssh/sshd_config r,
    /etc/passwd r,
    /etc/shadow r,
    /var/log/auth.log w,
    /tmp/ssh-* w,
    deny /home/** rw,
}
```

---

## 4. SELinux vs AppArmor — 内核实现对比

| | SELinux | AppArmor |
|---|---|---|
| **hook 注册** | ~170+ hooks | ~中等数量 |
| **主源文件** | `hooks.c` (7,979 行) | `lsm.c` (2,574 行) |
| **缓存机制** | AVC rb-tree (rb_root) | 文件→profile 哈希 + per-file allow 缓存 |
| **策略加载** | `selinuxfs/load` → 二进制 blob | 每程序加载 profile 文本 |
| **标签存储** | `security.selinux` xattr | 无 (路径匹配不需要) |
| **类型迁移** | `type_transition` → 自动设置新对象类型 | 无 |
| **MLS** | 敏感度 + 类别 (s0-s15:c0.c1023) | 无 |
| **首次命中率** | ~99%+ (AVC) | ~95%+ (per-file allow 缓存) |

---

## 5. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| SELinux hooks | `hooks.c:7533-7858` | ~170+ 个 LSM_HOOK_INIT |
| AVC 缓存 | `avc.c:48-60,1189-1203` | 访问向量预计算, 99%+ 命中 |
| security_compute_av | `services.c:1123-1189` | SID 表查询 + TE 规则遍历 |
| AppArmor 入口 | `lsm.c:541-544` | common_file_perm 调度 |
| aa_file_perm | `file.c:619-671` | fast path (per-file allow) + slow path 重新验证 |
| aa_path_name | `path.c:202-222` | d_namespace_path 解析绝对路径 |

## 下一篇文章

→ [第5篇：Linux 内核零信任 — Lockdown、Yama、BPF LSM、Landlock](./05-zero-trust.md)
