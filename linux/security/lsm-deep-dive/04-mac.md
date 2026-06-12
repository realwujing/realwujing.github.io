# 第4篇：SELinux 与 AppArmor — 强制访问控制的内核视角

> 源码：`security/selinux/` `security/apparmor/` | 头文件：`security/selinux/include/security.h`

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

### 2.1 标签体系

```
# 文件标签
-rw-r--r--. root root system_u:object_r:etc_t:s0 /etc/passwd

# 进程标签
ps -eZ: system_u:system_r:sshd_t:s0-s0:c0.c1023

# 格式: user:role:type:level
#   user   = SELinux user (system_u, user_u)
#   role   = 角色限制可进入的域 (object_r, system_r)
#   type   = ★ Type Enforcement 的核心 (sshd_t, httpd_t, etc_t)
#   level  = MLS/MCS 敏感度/类别
```

### 2.2 Type Enforcement 决策链

```c
// security/selinux/hooks.c
static int selinux_inode_permission(struct inode *inode, int mask)
{
    // ① 获取主体标签: 当前进程 → struct task_security_struct → sid
    u32 ssid = current_sid();

    // ② 获取客体标签: 文件 inode → struct inode_security_struct → sid
    u32 tsid = inode->i_security->sid;

    // ③ ★ AVC 检查: 查询 Type Enforcement 规则
    //    allow sshd_t etc_t:file { read };
    //    → allow ssid(sshd_t) tsid(etc_t) "file" "read"
    return avc_has_perm(ssid, tsid, SECCLASS_FILE,
                        FILE__READ, NULL);
}
```

**TE 规则格式**：

```
# allow <源类型 (进程)> <目标类型 (文件/进程/网络等)>:<对象类别> { 操作集 };

allow sshd_t etc_t:file { read getattr open };
allow httpd_t httpd_t:process { fork transition sigkill };
allow unconfined_t port_type:tcp_socket { name_bind name_connect };
```

### 2.3 AVC 缓存 (Access Vector Cache)

```c
// security/selinux/avc.c
struct avc_entry {
    u32 ssid;           // 源安全 ID
    u32 tsid;           // 目标安全 ID
    u16 tclass;         // 对象类别 (SECCLASS_FILE=2, SECCLASS_PROCESS=4, etc.)
    u32 allowed;        // ★ 预先计算的允许集合（位掩码）
    u32 audit_allow;    // 需要审计的操作
    u32 audit_deny;     // 显式拒绝的操作
};

int avc_has_perm(u32 ssid, u32 tsid, u16 tclass, u32 requested, ...)
{
    // ① 先查 AVC 缓存 — 如果有且 allowed 位包含 requested → 允许
    struct avc_entry *entry = avc_lookup(ssid, tsid, tclass);
    if (entry) {
        if ((entry->allowed & requested) == requested)
            return 0;  // ★ 缓存命中，零策略查询
    }

    // ② 缓存 miss → 查询二进制策略 → 构建新的 AVC entry
    entry = avc_insert(ssid, tsid, tclass, ...);
    return (entry->allowed & requested) == requested ? 0 : -EACCES;
}
```

**性能关键**：AVC 缓存使得 99%+ 的 SELinux 检查都在指针语义内完成——规则表查一次，后续同类检查全是缓存命中。

---

## 3. AppArmor — 路径匹配

```c
// security/apparmor/lsm.c
static int apparmor_file_permission(struct file *file, int mask)
{
    // ① 获取文件的绝对路径
    char *path = aa_path_name(file->f_path.dentry, ...);

    // ② 加载当前进程的 profile
    struct aa_profile *profile = aa_current_profile();

    // ③ ★ 文件名匹配 vs profile 规则
    //    profile 中的规则: "/etc/passwd r"
    //    当前操作: read /etc/passwd → 匹配 → 允许
    return aa_file_perm(profile, OP_READ, file, path, mask);
}
```

**AppArmor profile 示例**：

```
/usr/sbin/sshd {
    /etc/ssh/sshd_config r,
    /etc/passwd r,
    /etc/shadow r,
    /var/log/auth.log w,
    /tmp/ssh-* w,
    deny /home/** rw,        # 黑名单
}
```

---

## 4. SELinux vs AppArmor — 内核实现对比

| | SELinux | AppArmor |
|---|---|---|
| **hook 注册** | `selinux_hooks[]` (大量 hooks) | `apparmor_hooks[]` (中等) |
| **主源文件** | `hooks.c` (9,045+ 行) | `lsm.c` |
| **缓存机制** | AVC 缓存 (rb-tree, ~10% 统计空间) | 文件名→规则哈希 |
| **策略加载** | `selinuxfs/load` → 二进制 blob → 解析 | 每程序加载 profile 文本 |
| **标签存储** | `security.selinux` xattr (每个 inode) | 无 (路径匹配，不需要标签) |
| **类型迁移** | `type_transition` → 创建新对象时自动设置类型 | 无 |
| **MLS** | 敏感度 + 类别 (s0-s15:c0.c1023) | 无 |

---

## 5. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| SELinux 标签 | `selinux/hooks.c` | 每个操作检查 TE 规则 |
| AVC 缓存 | `selinux/avc.c` | 访问向量预计算，99%+ 命中率 |
| AppArmor 路径 | `apparmor/lsm.c` | 文件名匹配 vs profile 规则 |
| SELinux 策略 | `selinux/ss/services.c` | 二进制 TE 规则表解析 |
| AppArmor Profile | `apparmor/policy.c` | 每程序文本规则 |

## 下一篇文章

→ [第5篇：Linux 内核零信任 — Lockdown、Yama、BPF LSM、Landlock](./05-zero-trust.md)
