# fs/file: optimize close_range() complexity from O(N) to O(Sparse)

## 问题背景

`close_range(fd, max_fd, flags)` 系统调用用于批量关闭文件描述符。传统实现中，`__range_close()` 在 `[fd, max_fd]` 区间内执行线性扫描：

```c
for (; fd <= max_fd; fd++) {
    file = file_close_fd_locked(files, fd);
    ...
}
```

对于拥有稀疏文件描述符表（大量空洞）的进程，这会导致 O(范围大小) 的复杂度。例如，当 `fd=0, max_fd=1000000` 但实际只打开了 10 个 FD 时，仍然需要遍历 100 万次循环，其中绝大多数迭代都在检查未分配的槽位。

## 优化方案

使用 `find_next_bit()` 在 `open_fds` 位图上查找下一个已分配的文件描述符，跳过所有空洞：

```c
for (fd = find_next_bit(fdt->open_fds, max_fd + 1, fd);
     fd <= max_fd;
     fd = find_next_bit(fdt->open_fds, max_fd + 1, fd + 1)) {
    file = file_close_fd_locked(files, fd);
    ...
}
```

算法复杂度从 **O(范围大小)** 降至 **O(活跃 FD 数)**。

由于 `file_close_fd_locked()` 和 `cond_resched()` 期间可能发生 `expand_files()` 导致 `fdt` 指针失效，每次解锁/重锁后需要刷新 `fdt`：

```c
filp_close(file, files);
cond_resched();
spin_lock(&files->file_lock);
fdt = files_fdtable(files);  // 刷新指针
```

## 代码变更

| 文件 | 变更 |
|---|---|
| `fs/file.c` | `__range_close()` 用 `find_next_bit()` 替代线性扫描，FAIL 后刷新 `fdt` |

## 上游状态

- **Lore**: https://patch.msgid.link/20260123081221.659125-1-realwujing@gmail.com
- **合入**: torvalds/master `fc94368bcee5`
- **合入者**: Christian Brauner <brauner@kernel.org>