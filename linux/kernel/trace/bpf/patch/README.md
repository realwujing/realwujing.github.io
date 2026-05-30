# BPF 上游补丁

| # | 提交日期 | Subject | Patch |
|---|---------|---------|-------|
| 1 | 2026-01-20 | bpf/verifier: Optimize ID mapping reset in states_equal | [0001-bpf-verifier-Optimize-ID-mapping-reset-in-states_equ.patch](./0001-bpf-verifier-Optimize-ID-mapping-reset-in-states_equ.patch) |

### 详情

优化 BPF verifier 中 `states_equal()` 的 ID 映射重置。原实现每次调用 `states_equal()` 时都对 4.7KB 的 idmap 做 `memset()`，改用计数器跟踪已用映射，将 O(N) 的 memset 替换为 O(1) 的重置，并限制 `check_ids()` 中的搜索循环范围。

Signed-off-by: Qiliang Yuan <realwujing@gmail.com>
Acked-by: Eduard Zingerman <eddyz87@gmail.com>
Merged by: Andrii Nakryiko <andrii@kernel.org>
