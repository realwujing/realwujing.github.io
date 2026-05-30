# 网络 上游补丁

| # | 提交日期 | Subject | Patch |
|---|---------|---------|-------|
| 1 | 2026-02-04 | netns: optimize netns cleaning by batching unhash_nsid calls | [0001-netns-optimize-netns-cleaning-by-batching-unhash_nsi.patch](./0001-netns-optimize-netns-cleaning-by-batching-unhash_nsi.patch) |

### 详情

优化 netns 清理流程，将 `unhash_nsid()` 从逐 netns 循环中提出来，改为单次遍历存活的命名空间。通过 `is_dying` 标记识别正在销毁的 netns，消除 O(L_dying * M_alive) 的乘法复杂度，降至 O(M_alive)。同时将 `idr_for_each()` 替换为可重启的 `idr_get_next()` 循环以安全调用 `rtnl_net_notifyid()`。

Signed-off-by: Qiliang Yuan <realwujing@gmail.com>
Reviewed-by: Kuniyuki Iwashima <kuniyu@google.com>
Merged by: Jakub Kicinski <kuba@kernel.org>
