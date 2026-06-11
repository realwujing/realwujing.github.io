# 第2篇：Verifier — 循环检测、类型追踪与边界检查

> 源码：`kernel/bpf/verifier.c` | 头文件：`include/uapi/linux/bpf.h`

系列目录：[eBPF 内核源码深度解析](./README.md)

---

## 1. 为什么需要 Verifier

BPF 程序运行在内核态，不能信任用户态的字节码。Verifier 确保每条指令只访问它的自有栈和已验证的 map，不产生无限循环，不进行越界访问。

**Verifier 保证**：

| 验证项 | 机制 | 拒绝的原因 |
|--------|------|-----------|
| 无限循环 | CFG (Control Flow Graph) + 最大指令数 (1M) | `back-edge from insn X to Y exceeds 1M` |
| 栈越界 | 追踪 `fp - N` 访问 + `max_stack_depth` | `invalid stack off=X size=Y` |
| 未初始化读取 | 寄存器/栈槽的 liveness 追踪 | `R0 !read_ok` |
| 类型不匹配 | r1 的类型（ctx、packet、map_value）检查 | `R1 type=ctx expected=packet` |
| Null 指针 | 指针类型 PTN → PTR_TO_NULL → 检查 | `dereference of modified ptr_or_null` |
| 越界 map 访问 | 键/值大小 vs map 定义的检查 | `invalid access to map value, value_size=X off=Y size=Z` |
| 不允许的 helper | 只当前 prog_type 注册的 helper | `unknown func bpf_xxx#N` |

---

## 2. Verifier 的入口 — bpf_check

```c
// kernel/bpf/verifier.c
int bpf_check(struct bpf_prog **prog, union bpf_attr *attr, bpfptr_t uattr)
{
    struct bpf_verifier_env *env;
    int ret;

    // ① 分配 verifier env（包含日志，寄存器状态，CFG 状态）
    env = kzalloc(sizeof(*env), GFP_KERNEL);

    // ② 执行阶段 1: 检测指令，构建基础块和边
    ret = bpf_v_check_build_cfg(env);       // 构建 Control Flow Graph
    if (ret < 0)
        goto skip_full_check;

    // ③ 执行阶段 2: 模拟每条指令，验证类型和边界
    ret = do_check(env);                    // ★ 核心验证循环
    if (ret < 0)
        goto skip_full_check;

    // ④ 如果有 JIT 请求，验证通过后调用 JIT
    // ⑤ 清理

skip_full_check:
    // 将日志返回给用户态（如果 attr->log_level >= VERBOSE）
    if (attr->log_level) {
        // 将 env->log 内容复制到用户态缓冲区
    }
    bpf_verifier_env_free(env);
    return ret;
}
```

**第一阶段 (CFG)**：遍历每条指令，识别基本块、前驱和后继。如果发现 backward edge (循环)，检查最大指令数是否在 1M 限制内。

**第二阶段 (静态分析)**：以符号方式模拟每条指令执行后的**寄存器类型 + 栈状态**。如果遇到非法操作，立即拒绝。

---

## 3. 寄存器类型追踪

Verifier 维护每个寄存器的类型（通过枚举 `enum bpf_reg_type`）：

```c
enum bpf_reg_type {
    NOT_INIT,           // 0 — 不能读取的寄存器
    SCALAR_VALUE,       // 1 — 已知范围 [min, max]
    PTR_TO_CTX,         // 2 — 指向当前 prog_type 的 ctx 结构
    CONST_PTR_TO_MAP,   // 3 — 指向常量 map
    PTR_TO_MAP_VALUE,   // 4 — 指向 map 元素的指针
    PTR_TO_MAP_KEY,     // 5 — 指向 map 键的指针
    PTR_TO_STACK,       // 6 — 指向当前栈帧
    PTR_TO_PACKET_DATA, // 7 — 指向数据包数据（可修改边界）
    PTR_TO_PACKET_END,  // 8 — 数据包结束指针
    PTR_TO_MEM,         // 10 — 指向已验证的内存的指针
    PTR_TO_BUF,         // 12 — 指向 dynptr 的缓冲区
    PTR_TO_FUNC,        // 15 — 指向 BPF subprog
    PTR_TO_BTF_ID,      // 13 — 指向内核类型（用 BTF 验证）
};
```

**每执行一条指令，Verifier 更新寄存器类型**：

```
R1 = *(u32 *)(r10 - 4):
  如果 r10-4 处栈槽 TYPE=SCALAR_VALUE:
    R1 变为 SCALAR_VALUE [范围从栈槽的 min/max]

R1 = map_value:
  如果 map BTF 验证通过:
    R1 变为 PTR_TO_BTF_ID with BTF type_id
  否则:
    R1 变为 PTR_TO_MAP_VALUE

if r0 == 0 goto +1:
  THEN 分支: R0 = 0 (已知等于 0)
  ELSE 分支: R0 = SCALAR_VALUE [范围 ≠ 0]
```

---

## 4. BPF_MAP_VALUE 追踪

最关键的验证类型：PTR_TO_MAP_VALUE。程序 load 某 map 的元素，然后从这个指针偏移访问字段。Verifier 必须保证偏移不越界。

```c
struct bpf_reg_state {
    enum bpf_reg_type type;
    union {
        struct {
            struct bpf_map *map_ptr;     // ★ 指向的 map
            u32 map_uid;                 // map 的唯一 ID
        } map_ptr;
    };
    s32 off;                             // 指针的偏移
    u32 range;                           // 允许的访问大小
    // ... 范围追踪 ...
    struct tnum var_off;                 // 已知/未知位
    s64 smin_value, smax_value;
    u64 umin_value, umax_value;
    s32 s32_min_value, s32_max_value;
    u32 u32_min_value, u32_max_value;
};
```

**越界访问检测**：

```
R1 = bpf_map_lookup_elem(&my_map, &key);  → R1 = PTR_TO_MAP_VALUE with map_ptr, range=sizeof(value)

// 下面全是安全的（在 range 之内）
R2 = *(u32 *)(r1 + 0);   // 偏移在 range 内，安全
*(r1 + 4) = r3;          // 偏移在 range 内，安全

// 下面不安全 → Verifier 拒绝
*(r1 + 256) = r3;        // 偏移超过 range → 拒绝: "invalid access to map value"
```

---

## 5. 循环检测 — 反向边分析

Verifier 不做完整的循环分析（不会碰 `while(true)` 的边界），但做两件事：

1. **最大指令数限制**：任何程序最多执行 1M 条指令（防止无限循环消耗 verifier 本身）
2. **反向边计数**：每条 backward edge 重复次数有限

```c
// verifier 遇到 while(x < 100) 循环:
for (i = 0; i < 100; i++) { ... }:
  每条迭代 verifier 经过 3 次（按需）。如果边界信息足够，verifier 一次就验证整个循环。
  但如果 verifier 状态不够：会对循环展开有限次（默认状态剪枝）

// 死循环:
while (true) {}:
  Verifier 检测到 backward edge + 无 boundary → 拒绝
```

**状态剪枝**：两个 verifier 状态相同时，verifier 跳过重复验证，避免指数爆炸。

---

## 6. BPF 子程序调用

eBPF 支持递归和静态链接（通过 `BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_xxx)`）：

```c
// 主程序调用 subprog:
BPF_CALL_REL(offset_to_subprog)

// verifier 处理:
→ func_info → 识别为函数调用来处理
  → 验证主程序和子程序的函数签名
  → 验证参数传递: r1..r5（最多 5 个参数由调用者传递给被调用者）
  → r0 被调用者返回到调用者
  → 子程序的栈独立于主程序（每个 func 有自己的 fp 范围）
```

---

## 7. 总结

| 组件 | 关键函数 | 核心作用 |
|------|---------|---------|
| 入口 | `bpf_check` (verifier.c) | 分配 env，执行两个阶段 |
| CFG 分析 | `bpf_v_check_build_cfg` (verifier.c) | 构建基本块，检测 loop |
| 指令模拟 | `do_check_main` → `do_check_common` (verifier.c) | 逐条验证类型和边界 |
| 寄存器追踪 | `enum bpf_reg_type` (verifier/env) | 每个寄存器的已知类型和范围 |
| 栈验证 | `check_stack_read/write` (verifier.c) | 检查栈读写不越界 |
| map 访问 | `check_map_access` (verifier.c) | 验证 map 偏移在 range 内 |
| 循环检测 | backward edge 计数 + 1M 指令限制 | 防止无限循环挂起 verifier |

## 下一篇文章

→ [第3篇：BPF Maps — BPF_MAP_CREATE、lookup/update 与 map 类型](./03-maps.md)
