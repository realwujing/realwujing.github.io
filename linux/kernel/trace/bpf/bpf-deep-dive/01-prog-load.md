# 第1篇：BPF 程序加载 — struct bpf_prog 与 BPF_PROG_LOAD

> 源码：`kernel/bpf/syscall.c` | 头文件：`include/linux/bpf.h` `include/uapi/linux/bpf.h`

系列目录：[eBPF 内核源码深度解析](./README.md)

---

## 1. struct bpf_prog — 内核中的程序表示

```c
// include/linux/bpf.h
struct bpf_prog {
    u16                     pages;          // 占用的页面数
    u16                     jited:1;        // 是否已 JIT 编译
    u16                     jit_requested:1;// 请求 JIT
    u32                     len;            // ★ 字节码指令数
    u32                     jited_len;      // JIT 生成本地代码的字节数
    u8                      tag[BPF_TAG_SIZE]; // SHA 哈希标签（用于识别程序版本）
    struct bpf_prog_aux     *aux;           // ★ 辅助信息（verifier logs, map refs, etc.）
    struct sock_fprog_kern  *orig_prog;     // 原始的未修改字节码
    unsigned int            (*bpf_func)(const void *ctx, const struct bpf_insn *insn);
                                            // ★ 入口函数：JIT 后的函数或解释器入口
    union {
        struct bpf_insn     insns[0];       // ★ 字节码指令数组（可变长）
        struct bpf_insn     insnsi[];       // 静态大小的变体
    };
};
```

**`aux` 是复杂的辅助结构**：

```c
struct bpf_prog_aux {
    atomic64_t              refcnt;         // 引用计数
    u32                     used_map_cnt;   // 程序引用的 map 数
    struct bpf_map          **used_maps;    // ★ 程序引用的 map 数组
    u32                     max_ctx_offset; // CTX 访问的最大位移
    u32                     max_pkt_offset; // 包访问的最大位移（只在 XDP/skb 程序中有意义）
    u32                     stack_depth;    // ★ 栈深度（verifier 追踪）
    struct bpf_verifier_log log;            // ★ verifier 日志（包含错误消息）
    struct bpf_prog_stats   __percpu *stats; // 每 CPU 统计
    struct bpf_prog_offload *offload;       // 硬件卸载信息（SmartNIC 等）
    struct btf              *btf;           // BTF 类型信息
    struct bpf_func_info    *func_info;     // 函数级元数据
    struct bpf_line_info    *linfo;         // 源码行号信息
    /* ... 更多字段 ... */
};
```

---

## 2. BPF_PROG_LOAD — 加载入口

```c
// kernel/bpf/syscall.c
static int bpf_prog_load(union bpf_attr *attr, bpfptr_t uattr)
{
    struct bpf_prog *prog;
    enum bpf_prog_type type;
    char license[128];
    bool is_gpl;
    int err;

    // ① 验证: 检查 attr->insn_cnt 不为 0
    // 检查 license（GPL 或者 non-GPL help 的 access 限制）

    // ② 分配 struct bpf_prog（含 bpf_insn 数组）
    prog = bpf_prog_alloc(bpf_prog_size(attr->insn_cnt), GFP_USER);

    // ③ 从用户态拷贝字节码
    copy_from_bpfptr(prog->insns, uattr, prog->len * sizeof(struct bpf_insn));

    // ④ 验证许可（根据 prog_type 检查 bpf_prog_type_list）
    type = attr->prog_type;
    is_gpl = license_is_gpl_compatible(license);

    // ⑤ ★★★ Verifier 验证 ★★★
    err = bpf_check(prog, attr, uattr);        // 核心！走 verifier.c

    // ⑥ 可选: JIT 编译
    if (err == 0 && prog->jit_requested)
        err = bpf_prog_select_runtime(prog, &err);

    // ⑦ 分配文件描述符和安装到 fd table
    err = bpf_prog_new_fd(prog);

    // ⑧ 根据 prog_type 执行附加逻辑（在 prog->aux->attach_btf 等中存储 hook 信息）
    // 返回 fd 给用户态
}
```

---

## 3. BPF 指令集（基础）

ebpf 有 11 个 64-bit 寄存器：R0（返回值）、R1-R5（函数参数）、R6-R9（callee-saved）、R10（只读帧指针）。

每条指令是 64-bit 的固定大小：

```c
// include/uapi/linux/bpf.h
struct bpf_insn {
    __u8    code;       // 操作码分类: BPF_LD/BPF_LDX/BPF_ST/BPF_STX/BPF_ALU/BPF_JMP
    __u8    dst_reg:4;  // 目标寄存器 (0-10)
    __u8    src_reg:4;  // 源寄存器 (0-10)
    __s16   off;        // 有符号偏移 (用于跳转、栈访问、map 操作)
    __s32   imm;        // 立即数 / 有符号常量 (用于 BPF_ALU、BPF_LD、BPF_JMP)
};
```

**关键指令类**：

| 类 | code 位 2-0 | 含义 |
|----|------------|------|
| `BPF_LD` | 0x00 | 加载: `r0 = *(u32 *)(r1 + off)` |
| `BPF_LDX` | 0x01 | 加载到寄存器: `r0 = r1` |
| `BPF_ST` | 0x02 | 存储: `*(u32 *)(r10 - 4) = imm` |
| `BPF_STX` | 0x03 | 寄存器存储: `*(u32 *)(r1 + off) = r0` |
| `BPF_ALU` | 0x04 | ALU 操作: `r0 += 1`, `r0 &= r1` |
| `BPF_JMP` | 0x05 | 跳转: `if r0 == 0 goto +off` |
| `BPF_JMP32` | 0x06 | 32-bit 跳转 |
| `BPF_ALU64` | 0x07 | 64-bit ALU |

---

## 4. BPF 程序类型 (prog_type)

```c
// include/uapi/linux/bpf.h
enum bpf_prog_type {
    BPF_PROG_TYPE_UNSPEC,           // 0
    BPF_PROG_TYPE_SOCKET_FILTER,    // 1 — classic BPF (cBPF) 的 eBPF 等价物
    BPF_PROG_TYPE_KPROBE,           // 2 — kprobe 附加
    BPF_PROG_TYPE_SCHED_CLS,        // 3 — tc 分类器
    BPF_PROG_TYPE_SCHED_ACT,        // 4 — tc 动作
    BPF_PROG_TYPE_TRACEPOINT,       // 5 — tracepoint
    BPF_PROG_TYPE_XDP,              // 6 — XDP 快速路径
    BPF_PROG_TYPE_PERF_EVENT,       // 7 — perf_event 溢出
    BPF_PROG_TYPE_CGROUP_SKB,       // 8 — cgroup 包过滤
    BPF_PROG_TYPE_CGROUP_SOCK,      // 9 — cgroup socket 创建
    BPF_PROG_TYPE_LWT_IN,           // 10 — 轻量隧道入口
    BPF_PROG_TYPE_LWT_OUT,          // 11
    BPF_PROG_TYPE_LWT_XMIT,         // 12
    BPF_PROG_TYPE_SOCK_OPS,         // 13 — socket 操作
    BPF_PROG_TYPE_SK_SKB,           // 14 — socket 到 socket 转发
    BPF_PROG_TYPE_CGROUP_DEVICE,    // 15 — 设备 cgroup
    BPF_PROG_TYPE_SK_MSG,           // 16 — socket 消息
    BPF_PROG_TYPE_RAW_TRACEPOINT,   // 17 — 原始 tracepoint
    BPF_PROG_TYPE_CGROUP_SOCK_ADDR, // 18 — socket 地址 cgroup
    BPF_PROG_TYPE_LIRC_MODE2,       // 19 — LIRC
    BPF_PROG_TYPE_SK_REUSEPORT,     // 20 — reuseport
    BPF_PROG_TYPE_FLOW_DISSECTOR,   // 21 — 流解剖器
    BPF_PROG_TYPE_CGROUP_SYSCTL,    // 22 — sysctl cgroup
    BPF_PROG_TYPE_STRUCT_OPS,       // 23 — struct_ops (sched_ext 用这个)
    BPF_PROG_TYPE_NETFILTER,        // 24 — netfilter 钩子
    BPF_PROG_TYPE_SYSCALL,          // 25 — syscall BPF 程序
};
```

每种类型有一个对应的 `ctx` 结构（上下文），程序可以读取但不能修改。例如：
- `BPF_PROG_TYPE_XDP` → `struct xdp_md`（包含 data, data_end, data_meta）
- `BPF_PROG_TYPE_TRACEPOINT` → 该 tracepoint 的 `__entry` 指针

---

## 5. BPF helper 函数

BPF 程序通过 `BPF_CALL` 指令调用内核提供的 helper 函数：

```c
// 例子: bpf_map_lookup_elem
BPF_CALL_2(bpf_map_lookup_elem, struct bpf_map *, map, void *, key)
{
    void *value = map->ops->map_lookup_elem(map, key);
    return (unsigned long)value;
}

// 用户态 BPF 程序调用:
// r0 = bpf_map_lookup_elem(&my_map, &key)
// 汇编: BPF_LD_MAP_FD(r1, map_fd)
//       BPF_MOV64_REG(r2, r10)
//       BPF_ALU64_IMM(BPF_ADD, r2, -4)
//       BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem)

// 在 verifier 验证时解析 fd → struct bpf_map 指针
```

可用的 helper 函数在每个 `BPF_PROG_TYPE` 中不同——XDP 程序不能调用 `bpf_get_current_pid_tgid`（因为没有当前任务上下文）。

---

## 6. 程序生命周期

```
bpf(BPF_PROG_LOAD, &attr)
  → bpf_prog_load(syscall.c)
    → bpf_prog_alloc (分配)
    → bpf_check (verifier 验证)
    → bpf_prog_select_runtime (JIT)
    → 返回 fd

程序附加:
  for XDP:     bpf_set_link_xdp_fd(ifindex, prog_fd, flags)
  for kprobe:  perf_event_open(attr, ..., bpf_fd)
  for cgroup:  bpf(BPF_PROG_ATTACH, ...)

程序执行:
  硬件触发的 XDP:              网卡驱动 → bpf_prog_run_xdp
  内核触发的 tracepoint:      trace_xxx → bpf_prog_run
  socket 操作:                  sk_filter_trim_cap → bpf_prog_run_save_cb

程序释放:
  关闭 fd, 或者 unpin 后 bpf_prog_put → 引用计数为 0 → bpf_prog_free
```

---

## 7. 总结

| 组件 | 关键函数/结构 | 核心作用 |
|------|-------------|---------|
| BPF 程序存储 | `struct bpf_prog` (include/linux/bpf.h) | 字节码 + aux + JIT 代码 |
| 加载入口 | `bpf_prog_load` (syscall.c) | 分配→验证→JIT→fd |
| 指令格式 | `struct bpf_insn` (uapi/linux/bpf.h) | 64-bit 固定大小指令 |
| 程序类型 | `enum bpf_prog_type` (uapi/linux/bpf.h) | XDP, kprobe, tracepoint, cgroup, etc. |
| helper 函数 | `BPF_CALL_x` (include/linux/bpf.h) | 内核导出给 BPF 程序的函数 |
| 附加点 | `bpf_prog_attach` / `bpf_set_link_xdp_fd` | 用户态激活程序 |

## 下一篇文章

→ [第2篇：Verifier — 循环检测、类型追踪与边界检查](./02-verifier.md)
