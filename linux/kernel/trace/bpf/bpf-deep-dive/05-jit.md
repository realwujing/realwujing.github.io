# 第7篇：BPF JIT 编译器 — 从 bytecode 到 x86 原生代码

> 源码：`arch/x86/net/bpf_jit_comp.c` `kernel/bpf/core.c` | 头文件：`include/linux/bpf.h`

系列目录：[eBPF 内核源码深度解析](./README.md)

---

## 1. JIT 的入口

```c
// arch/x86/net/bpf_jit_comp.c:3718
struct bpf_prog *bpf_int_jit_compile(struct bpf_verifier_env *env,
                                     struct bpf_prog *prog)
{
    struct jit_context ctx = {};
    u8 *image = NULL;
    int *addrs;
    int proglen;

    // ① 为每条 BPF 指令分配 addrs 数组（追踪每条指令的 x86 偏移）
    addrs = kvcalloc(prog->len + 1, sizeof(*addrs), GFP_KERNEL);

    // ② JIT 可能需要多个 pass（因为跳转偏移长度在第一次 pass 未知）
    for (pass = 0; pass < MAX_PASSES || !image; pass++) {
        // ③ 分配/重新分配输出缓冲区（rw_image 用于写，image 用于执行）
        if (!image) {
            image = bpf_prog_pack_alloc(proglen);
        }

        // ④ ★ 核心编译
        proglen = do_jit(env, prog, addrs, image, rw_image,
                         oldproglen, &ctx, padding);    // line 3796

        // ⑤ 如果 proglen 不变，pass 完成（跳转偏移已收敛）
        if (proglen == oldproglen) break;
    }

    // ⑥ 如果有子程序，也 JIT 编译子程序
    // ⑦ 设置 prog->bpf_func = image（将 image 设为程序的入口函数）
    prog->bpf_func = (void *)image;
    prog->jited = 1;

    return prog;
}
```

---

## 2. do_jit — 核心编译循环

```c
// arch/x86/net/bpf_jit_comp.c:1652
static int do_jit(struct bpf_verifier_env *env, struct bpf_prog *bpf_prog,
                  int *addrs, u8 *image, u8 *rw_image,
                  int oldproglen, struct jit_context *ctx, bool jmp_padding)
{
    // ① 生成 x86 函数序言
    emit_prologue(&prog, bpf_prog, stack_depth, seen, ...);

    // ② 遍历每条 BPF 指令，逐条编译
    for (i = 0; i < prog->len; i++) {
        insn = &prog->insnsi[i];
        addrs[i] = prog - (u8 *)ctx->image;  // 记录这条指令的 x86 偏移

        switch (insn->code) {
        case BPF_ALU | BPF_ADD | BPF_X:
            // r1 += r2 → add r1d, r2d
            EMIT2(0x01, add_2reg(0xC0, dst_reg, src_reg));
            break;

        case BPF_ALU64 | BPF_ADD | BPF_X:
            // r1 += r2 → add r1, r2
            EMIT3(add_2mod(0x48, dst_reg, src_reg), 0x01,
                  add_2reg(0xC0, dst_reg, src_reg));
            break;

        case BPF_JMP | BPF_JEQ | BPF_K:
            // if r1 == imm goto +off
            EMIT3(0x48, 0x83, 0xF8 + dst_reg);  // cmp r1, imm
            EMIT1_off32(0x0F, 0x84, off);        // je target
            break;

        case BPF_LD | BPF_DW | BPF_IMM:
            // r1 = 64-bit imm (map_fd → 真正的 map 指针)
            EMIT1_off32(0x48, 0xB8 + dst_reg, imm_lo);  // mov r1, imm64
            EMIT(imm_hi, 4);
            break;

        // ... 所有 BPF 指令类映射 ...

        case BPF_JMP | BPF_CALL:
            // 调用 BPF helper 函数
            func = (u8 *)__bpf_call_base + imm;
            EMIT1_off32(0xE8, func - (image + addrs[i] - 1), ...);
            break;

        case BPF_JMP | BPF_EXIT:
            // 函数返回
            emit_epilogue(&prog);
            break;
        }
    }

    // ③ 如果有异常处理程序（COND_BRANCH_JIT），在此插入额外的代码
    // ④ 返回生成的总长度
    return prog - (u8 *)ctx->image;
}
```

**辅助函数 `EMIT` 宏**（`bpf_jit_comp.c:38-55`）：
```c
#define EMIT1(b1)       (*prog++ = (b1))
#define EMIT2(b1, b2)   (*prog++ = (b1); *prog++ = (b2))
#define EMIT3(b1, b2, b3) (*prog++ = (b1); *prog++ = (b2); *prog++ = (b3))
// EMIT1_off32 用于带有 32-bit 跳转偏移的指令
```

---

## 3. 多 pass 编译

BPF 的跳转使用 16-bit 有符号偏移（在 `insn.off` 中）。但 JIT 不知道跳转目标的确切位置，只能推测。所以 `do_jit` 被循环多次：

```
pass 1: 所有 16-bit 偏移 → all converted to 32-bit x86 jump → 如果实际距离 > 32-bit 范围, 标记为"extra pass needed"
pass 2: 用更大的偏移量重编译 → 引入 5-byte NOP 填充 → 确保全部跳转目标可达
pass N: 偏移收敛 → 完成
```

最多 `MAX_PASSES` 次（当前为 20，第 3783 行 `bpf_jit_comp.c`）。每个 pass 使用 `addrs` 数组（记录每条 BPF 指令的 x86 位置）来确定跳转目标。

---

## 4. __bpf_call_base — BPF helper 调用

```c
// kernel/bpf/core.c
// 指向 helper 函数数组的基地址
void *__bpf_call_base = __bpf_call_base_functions;

// helper 函数通过 imm 值索引:
// imm = BPF_FUNC_map_lookup_elem → 索引到数组第 [BPF_FUNC_map_lookup_elem] 项
// JIT 代码中: call *(__bpf_call_base + imm * 8)
```

---

## 5. bpf_prog_select_runtime — JIT vs 解释器决策

```c
// kernel/bpf/core.c:2668
struct bpf_prog *bpf_prog_select_runtime(struct bpf_prog *fp, int *err)
{
    // ① 如果 CONFIG_BPF_JIT_ALWAYS_ON → 总是 JIT
    // ② 如果没有 JIT 可用 → 使用解释器
    // ③ 调用 bpf_prog_jit_compile(env, fp) → 调用架构特定的 JIT
    // ④ 设置 prog->bpf_func = JIT 后的入口或解释器入口
}

// kernel/bpf/core.c:2601
struct bpf_prog *__bpf_prog_select_runtime(struct bpf_verifier_env *env,
                                           struct bpf_prog *fp, int *err)
```

---

## 6. Verifier → JIT handoff

```c
// kernel/bpf/verifier.c:20189
env->prog = __bpf_prog_select_runtime(env, env->prog, &ret);
// verifier 验证通过后，立即调用 JIT
// 如果 JIT 失败 → 退回解释器
```

---

## 7. 总结

| 组件 | 关键函数 | 核心作用 |
|------|---------|---------|
| JIT 入口 | `bpf_int_jit_compile` (`bpf_jit_comp.c:3718`) | 分配缓冲区 + 多 pass 编译 |
| 核心编译 | `do_jit` (`bpf_jit_comp.c:1652`) | 逐条 BPF → x86 指令映射 |
| EMIT 宏 | `EMIT1/2/3/4/5` (`bpf_jit_comp.c:38-55`) | x86 指令字节码写入 |
| Multi-pass | `MAX_PASSES=20` | 跳转偏移收敛 |
| Helper 调用 | `__bpf_call_base` (`core.c`) | 从查表找到 helper 地址 |
| 运行时选择 | `bpf_prog_select_runtime` (`core.c:2668`) | JIT vs 解释器决策 |
