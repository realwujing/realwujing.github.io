# 第2篇：LSM 模块化与初始化 — DEFINE_LSM、security_add_hooks、ordered_lsm_init

> 源码：`security/lsm_init.c` `security/security.c` | 头文件：`include/linux/lsm_hooks.h`

系列目录：[Linux 安全子系统 内核源码深度解析](./README.md)

---

## 1. DEFINE_LSM — 注册 LSM 模块

每个 LSM 模块使用 `DEFINE_LSM` 宏在**独立的 ELF section** 中声明自己：

```c
// include/linux/lsm_hooks.h
#define DEFINE_LSM(lsm)                                    \
    static const struct lsm_info __lsm_##lsm                \
        __used __section(".lsm_info.init")                  \
        __aligned(sizeof(unsigned long))

// 每个模块的使用:
// SELinux:
DEFINE_LSM(selinux) = {
    .name = "selinux",
    .init = selinux_init,
    .order = LSM_ORDER_FIRST,      // ★ 最早初始化
    .flags = LSM_FLAG_EXCLUSIVE,    // ★ 独占，与 AppArmor 互斥
};

// AppArmor:
DEFINE_LSM(apparmor) = {
    .name = "apparmor",
    .init = apparmor_init,
    .order = LSM_ORDER_FIRST,      // ★ 与 SELinux 争夺第一（只能有一个成功）
    .flags = LSM_FLAG_EXCLUSIVE,
};

// BPF LSM:
DEFINE_LSM(bpf) = {
    .name = "bpf",
    .init = bpf_lsm_init,
    .order = LSM_ORDER_LAST,
    .flags = LSM_FLAG_LEGACY_MAJOR,
};
```

**关键标志**：

| Flag | 含义 |
|------|------|
| `LSM_FLAG_EXCLUSIVE` | 独占模块，只能有一个这样的 LSM 被激活 |
| `LSM_FLAG_LEGACY_MAJOR` | 传统大模块，按顺序初始化 |
| `LSM_FLAG_MOD` | 模块可以被 `security_module_enable()` 启用/禁用 |

---

## 2. security_add_hooks — 注册 hook 回调

```c
// security/security.c
void security_add_hooks(struct security_hook_list *hooks, int count,
                        const char *lsm)
{
    int i;

    for (i = 0; i < count; i++) {
        struct security_hook_list *hp = &hooks[i];
        struct hlist_head *head = hp->head;

        hp->lsm = lsm;
        // 插入到对应 hook 的 hlist 头部
        hlist_add_tail_rcu(&hp->list, head);
    }

    // 如果有静态调用能力 → 更新 static call 跳板
    // 如果多个模块使用 → 退回 hlist 遍历
}
```

典型使用（每个模块的 init 函数中调用）：

```c
static int __init selinux_init(void)
{
    security_add_hooks(selinux_hooks, ARRAY_SIZE(selinux_hooks), "selinux");
    return 0;
}

static int __init apparmor_init(void)
{
    security_add_hooks(apparmor_hooks, ARRAY_SIZE(apparmor_hooks), "apparmor");
    return 0;
}
```

**hook 链表的存储格式**：

```c
struct security_hook_list {
    struct hlist_node           list;    // 在 hlist 中的节点
    struct hlist_head           *head;   // ★ 指向 security_hook_heads.XXX
    union {
        void                    *hook;   // 一般的 hook 回调指针
        // ... 静态调用版本的变体 ...
    };
    const char                  *lsm;    // "selinux" / "apparmor" / "bpf"
};
```

---

## 3. LSM 初始化流程

```c
// security/lsm_init.c
static int __init ordered_lsm_init(void)
{
    struct lsm_info **lsm;

    // ① 遍历所有 LSM section 中的项
    for (lsm = ordered_lsms; *lsm; lsm++) {
        if ((*lsm)->flags & LSM_FLAG_EXCLUSIVE) {
            // 独占模块: 只能激活一个
            // e.g. SELinux 和 AppArmor 都有 LSM_FLAG_EXCLUSIVE
            // → CONFIG_LSM="selinux" 时选择 SELinux, 跳过 AppArmor
            if ((*lsm)->flags & LSM_FLAG_LEGACY_MAJOR)
                enable = security_module_enable(*lsm);
            if (enable)
                (*lsm)->init();
        } else {
            // 非独占模块: 总是激活
            (*lsm)->init();
        }
    }
    return 0;
}
late_initcall(ordered_lsm_init);
```

### LSM 启动顺序

由内核命令行 `lsm=` 和内核配置 `CONFIG_LSM` 决定：

```
CONFIG_LSM="landlock,lockdown,yama,integrity,selinux,bpf"
                 ↑ 最早        ↑ 中间    ↑ 最后

多步初始化的执行顺序:
  landlock → lockdown → yama → integrity → SELinux → BPF

LSM_FLAG_EXCLUSIVE: SELinux 和 AppArmor 不能同时启用
LSM_ORDER_FIRST: 在 ordered_lsm_init 中最早处理
LSM_ORDER_LAST: 在 ordered_lsm_init 中最迟处理
```

---

## 4. 模块之间的交互

**独占模块的冲突解决**：

```
如果 CONFIG_LSM="selinux,apparmor"  (矛盾！)：

内核尝试初始化 selinux → 成功 (LSM_FLAG_EXCLUSIVE)
内核检测 apparmor 也是 LSM_FLAG_EXCLUSIVE 且已有一个独占模块 → 跳过 apparmor

结果: 只有 selinux 激活
```

**叠加模块**（非独占）可以始终与独占模块共存：

```
landlock (非独占) + lockdown (非独占) + yama (非独占) + SELinux (独占)
→ 四者同时生效
```

---

## 5. 完整的 LSM 生命周期

```
启动时:
  DEFINE_LSM 声明 (编译时) → ELF section 存储模块信息
    ↓
  ordered_lsm_init (late_initcall)
    ↓
  每个模块的 init 函数:
    → init 自身状态 (策略加载, AVC 初始化等)
    → security_add_hooks(module_hooks, count, lsm_name)
      → 插入到 security_hook_heads.XXX hlist

运行时:
  内核执行操作 (e.g. open)
    → security_xxx()
      → call_int_hook → hlist 遍历
        → selinux_xxx → AppArmor_xxx → bpf_xxx ...
```

---

## 6. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| DEFINE_LSM | `lsm_hooks.h` | 模块注册到 ELF section |
| security_add_hooks | `security.c` | 模块注册 hook 回调到 hlist |
| ordered_lsm_init | `lsm_init.c` | 按顺序初始化模块 |
| LSMO_FLAG_EXCLUSIVE | `lsm_hooks.h` | SELinux/AppArmor 独占互斥 |
| CONFIG_LSM + lsm= | Kconfig + 命令行 | 控制模块选择顺序 |

## 下一篇文章

→ [第3篇：POSIX Capabilities 内核实现](./03-capabilities.md)
