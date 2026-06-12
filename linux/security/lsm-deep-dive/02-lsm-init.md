# 第2篇：LSM 模块化与初始化 — DEFINE_LSM、lsm_order_append、ordered_lsm_init

> 源码：`security/lsm_init.c` `security/security.c` | 头文件：`include/linux/lsm_hooks.h`

---

## 1. DEFINE_LSM — 注册 LSM 模块

每个 LSM 模块使用 `DEFINE_LSM` 宏在**独立的 ELF section** 中声明自己：

```c
// include/linux/lsm_hooks.h:187-195
#define DEFINE_LSM(lsm)                                                \
    static struct lsm_info __lsm_##lsm                                 \
        __used __section(".lsm_info.init")                             \
        __aligned(sizeof(unsigned long))

#define DEFINE_EARLY_LSM(lsm)                                          \
    static struct lsm_info __early_lsm_##lsm                           \
        __used __section(".early_lsm_info.init")                       \
        __aligned(sizeof(unsigned long))
```

**每个模块的使用示例**：

```c
// SELinux (安全标签强制访问控制):
DEFINE_LSM(selinux) = {
    .id = &selinux_lsmid,          // LSM 唯一名称 + ID
    .order = LSM_ORDER_FIRST,      // ★ 最早初始化
    .flags = LSM_FLAG_EXCLUSIVE,    // ★ 独占，与 AppArmor 互斥
    .init = selinux_init,           // 初始化回调
    .enabled = &selinux_enabled,
    .blobs = &selinux_blob_sizes,
};

// AppArmor (路径匹配强制访问控制):
DEFINE_LSM(apparmor) = {
    .id = &apparmor_lsmid,
    .order = LSM_ORDER_FIRST,      // ★ 与 SELinux 争夺第一
    .flags = LSM_FLAG_EXCLUSIVE,    // ★ 互斥
    .init = apparmor_init,
};

// BPF LSM (动态 eBPF 策略):
DEFINE_LSM(bpf) = {
    .id = &bpf_lsmid,
    .order = LSM_ORDER_LAST,
    .flags = LSM_FLAG_LEGACY_MAJOR,
    .init = bpf_lsm_init,
};
```

**关键标志**：

| Flag | 含义 | 影响 |
|------|------|------|
| `LSM_FLAG_EXCLUSIVE` | 独占模块 | 只能有一个这样的 LSM 被激活——SELinux 和 AppArmor 互斥 |
| `LSM_FLAG_LEGACY_MAJOR` | 传统大模块 | 按顺序初始化，排在非独占模块之后 |

---

## 2. `lsm_order_append` — 决定模块加载顺序

```c
// security/lsm_init.c:153-191
static void __init lsm_order_append(struct lsm_info *lsm, const char *src)
{
    // ① 忽略重复项
    if (lsm_order_exists(lsm))
        return;

    // ② 跳过显式禁用的模块（lsm= 命令行或 CONFIG_LSM 白名单之外的）
    if (lsm->enabled && !lsm_is_enabled(lsm)) {
        lsm_pr_dbg("skip previously disabled LSM %s:%s\n", src, lsm->id->name);
        return;
    }

    // ③ 检查总数——不能超过 MAX_LSM_COUNT
    if (lsm_active_cnt == MAX_LSM_COUNT) {
        pr_warn("exceeded maximum LSM count on %s:%s\n", src, lsm->id->name);
        return;
    }

    // ④ ★ 独占模块互斥检查
    if (lsm->flags & LSM_FLAG_EXCLUSIVE) {
        if (lsm_exclusive) {
            // ★ 已有一个独占模块被选中 → 跳过当前
            lsm_enabled_set(lsm, false);
            return;
        } else {
            // ★ 设置为独占模块
            lsm_exclusive = lsm;
        }
    }

    // ⑤ 添加到 lsm_order[] 数组
    lsm_enabled_set(lsm, true);
    lsm_order[lsm_active_cnt] = lsm;
    lsm_idlist[lsm_active_cnt++] = lsm->id;
}
```

**加载顺序由这些调用点决定**：

```c
// lsm_init.c:224 — 最先
lsm_order_append(lsm, "first");            // → capability 总是第一个

// lsm_init.c:235 — 然后是用户指定的
lsm_order_append(lsm, "cmdline:...");      // → 来自 lsm= 内核参数

// lsm_init.c:244 — 接下来是内置的
lsm_order_append(lsm, "builtin:...");      // → 来自 CONFIG_LSM

// lsm_init.c:251 — 最后
lsm_order_append(lsm, "list");             // → integrity, lsm_landlock 等
```

---

## 3. `ordered_lsm_init` — 执行模块初始化

```c
// security/lsm_init.c:393 — 由 late_initcall 调用
static int __init ordered_lsm_init(void)
{
    // 遍历 lsm_order[] 数组（lsm_order_append 填充的内容）
    for (int i = 0; i < lsm_active_cnt; i++)
        lsm_order[i]->init();   // ★ 调用每个模块的 init
}

late_initcall(ordered_lsm_init);
```

---

## 4. 完整的 LSM 生命周期

```
编译时:
  DEFINE_LSM 宏 → 每个模块存放到 .lsm_info.init / .early_lsm_info.init ELF section
    ↓

启动时 early_init:
  lsm_order_append("first"): capability 最先
  lsm_order_append("cmdline"): lsm= 内核参数
  lsm_order_append("builtin"): CONFIG_LSM 内置选择
  lsm_order_append("list"): integrity 等最后添加
    ↓

late_initcall:
  ordered_lsm_init
    ↓
  每个模块的 init 回调:
    → 初始化自身状态 (策略加载, AVC 初始化, profile 编译)
    → security_add_hooks(module_hooks, count, lsmid)
      → 更新 static_calls_table 中的静态调用目标
      → 现在 call_int_hook 可以通过 static_call 跳转到这里

运行时:
  内核执行操作 (e.g. open)
    → security_xxx()
      → call_int_hook → static_call(Slot0) → Slot1 → Slot2
        → 任何非默认返回值 → 立即返回
```

---

## 5. 总结

| 组件 | 关键位置 | 作用 |
|------|---------|------|
| DEFINE_LSM | `lsm_hooks.h:187-190` | 模块注册到 ELF section |
| lsm_order_append | `lsm_init.c:153-191` | 决定模块顺序, 独占互斥检查 |
| ordered_lsm_init | `lsm_init.c:393` | late_initcall 执行每个模块的 init |
| security_add_hooks | `lsm_init.c:369-380` | 更新 static_call 跳转表 |
| LSM_FLAG_EXCLUSIVE | — | SELinux/AppArmor 独占互斥 |
| CONFIG_LSM + lsm= | Kconfig + 命令行 | 控制模块选择顺序 |
| MAX_LSM_COUNT | lsm_init.c 定义 | 限制模块总数 |

## 下一篇文章

→ [第3篇：POSIX Capabilities 内核实现 — 能力集位图与 exec 转换](./03-capabilities.md)
