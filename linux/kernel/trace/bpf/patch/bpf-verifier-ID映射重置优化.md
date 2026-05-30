# bpf/verifier: Optimize ID mapping reset in states_equal

## 问题背景

BPF verifier 在进行状态等价性比较 (`states_equal()`) 时，需要维护一个 ID 映射表 (`bpf_idmap`) 来跟踪两个状态中的寄存器 ID 对应关系。每次调用 `states_equal()` 时，`reset_idmap_scratch()` 会对整个 4.7KB 的映射表执行 `memset()` 清零操作。

在高频路径中（例如程序包含大量分支和循环），`states_equal()` 可能被调用数万甚至数十万次，每次 4.7KB 的 memset 累积起来成为 verifier 的主要性能瓶颈之一。

## 优化方案

引入 `cnt` 计数器追踪 idmap 中已使用的槽位数：

**`check_ids()` 优化：**
- 原来：固定遍历 `BPF_ID_MAP_SIZE`（1024）个槽位
- 现在：只遍历 `idmap->cnt` 个已使用的槽位
- 新 ID 对直接追加到 `map[idmap->cnt]`，然后 `cnt++`

**`reset_idmap_scratch()` 优化：**
- 原来：`memset(&env->idmap_scratch.map, 0, 4.7KB)`
- 现在：`idmap->cnt = 0` （O(1) 重置）

## 代码变更

| 文件 | 变更 |
|---|---|
| `include/linux/bpf_verifier.h` | `struct bpf_idmap` 新增 `u32 cnt` 字段 |
| `kernel/bpf/verifier.c` | `check_ids()` 搜索循环受限于 `cnt`；`reset_idmap_scratch()` 用 `cnt=0` 替代 `memset()` |

```c
// 优化前
static void reset_idmap_scratch(struct bpf_verifier_env *env)
{
    env->idmap_scratch.tmp_id_gen = env->id_gen;
    memset(&env->idmap_scratch.map, 0, sizeof(env->idmap_scratch.map));
}

// 优化后
static void reset_idmap_scratch(struct bpf_verifier_env *env)
{
    struct bpf_idmap *idmap = &env->idmap_scratch;
    idmap->tmp_id_gen = env->id_gen;
    idmap->cnt = 0;
}
```

## 上游状态

- **Lore**: https://lore.kernel.org/bpf/20260120023234.77673-1-realwujing@gmail.com
- **合入**: torvalds/master `f81c07a6e98e`
- **合入者**: Andrii Nakryiko <andrii@kernel.org>
- **Acked-by**: Eduard Zingerman <eddyz87@gmail.com>