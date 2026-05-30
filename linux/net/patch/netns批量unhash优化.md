# netns: optimize netns cleaning by batching unhash_nsid calls

## 问题背景

当多个网络命名空间同时被销毁时，`cleanup_net()` 对每个即将消亡的 netns 调用 `unhash_nsid(net, last)`。该函数遍历所有存活的命名空间，通过 `__peernet2id()` 在每个存活 netns 的 IDR 中线性搜索消亡 netns 的 ID：

```c
// 原有流程 (per-dying-netns)
for each dying netns:
    for each alive netns:       // O(M_alive)
        for each nsid in IDR:   // O(N_id) — __peernet2id() is linear
            if nsid points to dying:
                idr_remove(&tmp->netns_ids, id)
```

复杂度：**O(L_dying_net ⨉ M_alive_net ⨉ N_id)**

在大量 netns 并发销毁的场景（如容器密集销毁），这导致显著的延迟。

## 优化方案

**批量化 unhashing：** 将 `unhash_nsid()` 从逐 netns 循环中提出来，改为单次遍历：

1. 在 `cleanup_net()` 中，先标记所有消亡 netns 为 `is_dying`（在 `net_rwsem` 写锁保护下）
2. 调用一次 `unhash_nsid(last)`，遍历所有存活 netns：
   - 用 `idr_get_next()` 遍历每个存活 netns 的 IDR
   - 遇到 `peer->is_dying == true` 的条目则删除
3. 每个消亡 netns 只销毁自己的 IDR（`idr_destroy`），不再遍历其他 namespace

**`is_dying` 标记：** 在 `struct net` 的 `hash_mix` 字段后的空洞位置新增 `bool is_dying`，不增加结构体大小。

**`idr_get_next()` vs `idr_for_each()`：** 使用可重启的 `idr_get_next()` 替代回调式的 `idr_for_each()`：
- `idr_for_each()` 回调内不允许释放锁
- `idr_get_next()` 可以在锁外安全调用 `rtnl_net_notifyid()`（sleepy 调用），然后重新获取锁继续迭代

## 代码变更

| 文件 | 变更 |
|---|---|
| `include/net/net_namespace.h` | `struct net` 新增 `bool is_dying` 字段 |
| `net/core/net_namespace.c` | `unhash_nsid()` 改为批量化实现；`cleanup_net()` 先标记 is_dying 再统一 unhash |

## 上游状态

- **Lore**: https://patch.msgid.link/20260204074854.3506916-1-realwujing@gmail.com
- **合入**: torvalds/master `7acee67a6bce`
- **合入者**: Jakub Kicinski <kuba@kernel.org>
- **Reviewed-by**: Kuniyuki Iwashima <kuniyu@google.com>