# 文件系统 上游补丁

| # | 提交日期 | Subject | Patch |
|---|---------|---------|-------|
| 1 | 2026-01-23 | fs/file: optimize close_range() complexity from O(N) to O(Sparse) | [0001-fs-file-optimize-close_range-complexity-from-O-N-to-.patch](./0001-fs-file-optimize-close_range-complexity-from-O-N-to-.patch) |

### 详情

优化 `close_range()`，对稀疏 FD 表使用 `find_next_bit()` 跳过空洞，将复杂度从 O(范围大小) 降至 O(活跃 FD 数)。`close_range()` 原本在 [fd, max_fd] 区间内线性扫描，对于稀疏的文件描述符表效率极低。

Signed-off-by: Qiliang Yuan <realwujing@gmail.com>
Merged by: Christian Brauner <brauner@kernel.org>
