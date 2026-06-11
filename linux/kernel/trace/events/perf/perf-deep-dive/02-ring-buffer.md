# 第2篇：Ring Buffer — 用户态 mmap 与采样数据写出

> 源码：`kernel/events/core.c` `kernel/events/ring_buffer.c` `kernel/events/internal.h`

---

## 1. ring buffer 的布局

perf 用两个独立的 buffer 存放数据：

| Buffer | 用途 | 数据量 | 典型写入点 |
|--------|------|--------|-----------|
| **主 ring buffer** | 采样记录（每个采样一条 `PERF_RECORD_SAMPLE`）+ metadata（COMM/MMAP/FORK/EXIT） | ~512KB-8MB | `perf_output_sample` / `perf_output_begin` |
| **AUX buffer** | 大块 trace 数据（Intel PT、ARM Coresight） | 几 MB-几 GB | `perf_aux_output_begin/end` |

两个 buffer 都通过 `mmap(fd)` 映射给用户态。关键数据结构：

```c
// include/linux/perf_event.h
struct perf_event_mmap_page {
    __u32   version;           // ★ 结构版本号（当前 1）
    __u32   compat_version;
    __u32   lock;              // ★ seqlock：更新 data_head/data_tail 时递增
    __u32   index;

    __s64   offset;            // ★ 从 map 开始的偏移（是负数！指向实际 ring buffer 区域）
    __u64   time_enabled;      // 事件启用的实际 CPU 时间（ns）
    __u64   time_running;      // 事件在启用期间实际运行的时间（ns）

    __u64   data_head;         // ★ 内核写入位置（内核更新，用户态只读）
    __u64   data_tail;         // ★ 用户态读完位置（用户态更新，内核只读）
    __u64   data_offset;       // 第一个 data page 相对于 metadata page 的偏移
    __u64   data_size;         // ring buffer 总大小（2^n, n 次幂）
    __u64   aux_head;          // AUX buffer 的写入位置
    __u64   aux_tail;
    __u64   aux_offset;
    __u64   aux_size;
};
```

**内存布局**（mmap 下来的区域）：

```
┌─────────────────────────────────────┐   ← mmap base (用户态地址)
│  struct perf_event_mmap_page       │   第 1 页（metadata page）
├─────────────────────────────────────┤
│  ring buffer data                  │   2^n 页（data pages）
│  ┌────────────────────────────┐    │
│  │ data_tail                  │    │   ← 用户态已读完的位置
│  │                            │    │
│  │  ██████████ (待处理采样) ███│    │
│  │                            │    │
│  │ data_head                  │    │   ← 内核正在写入的位置
│  └────────────────────────────┘    │
└─────────────────────────────────────┘
```

内核写入时 `data_head++`。用户态读完后 `data_tail = data_head`（向上走）。两者形成一个环。

---

## 2. 用户态如何零系统调用地读取

```c
// perf 工具的用户态代码（libperf）
static int read_ring_buffer(struct perf_evsel *evsel) {
    struct perf_event_mmap_page *pc = evsel->mmap_addr;  // mmap 下来的元数据页
    u64 head = READ_ONCE(pc->data_head);                  // 原子读
    u64 old = pc->data_tail;

    while (old != head) {
        // ① 根据 old 计算 ring buffer 中的位置
        char *data = evsel->mmap_addr + pc->data_offset;
        struct perf_record_header *hdr = (void *)(data + (old & pc->data_size));

        // ② 解析记录
        process_record(evsel, hdr);

        // ③ 更新 tail（内核下次写入时跳过这段已处理的区域）
        old += hdr->size;
        WRITE_ONCE(pc->data_tail, old);
    }

    // ★ 整个循环没有一次系统调用
}
```

内核侧对应的更新：

```c
// kernel/events/core.c:6825
void perf_event_update_userpage(struct perf_event *event)
{
    struct perf_event_mmap_page *userpg = event->rb->user_page;

    // ① 递增 seqlock（用户态通过 perf_event_read_begin/retry 检测更新）
    userpg->lock++;

    // ② 更新位置信息
    userpg->data_head = local64_read(&event->rb->head);      // data_head
    userpg->time_enabled = event->total_time_enabled;        // 总启用时间
    userpg->time_running = event->total_time_running;        // 实际运行时间

    userpg->lock++;
}
```

---

## 3. 内核写入路径 — perf_output_begin

```c
// kernel/events/ring_buffer.c
// 从 ring buffer 分配一段空间，写入采样记录
int perf_output_begin(struct perf_output_handle *handle,
                      struct perf_sample_data *data,
                      struct perf_event *event, unsigned int size)
{
    struct ring_buffer *rb = event->rb;

    // ① 自旋锁保护（防止同一 CPU 上多个事件并发写）
    local_inc(&rb->nest);
    handle->rb = rb;

    do {
        // ② 计算空闲空间
        head = local_read(&rb->head);
        tail = rb->user_page->data_tail;         // 用户态推进的 tail

        // ③ 如果 ring buffer 不够大（head - tail 超过 buffer 大小），跳过本次记录
        if (head - tail > rb->data_size) {
            // 设置 lost 标志，返回 no space
        }

        // ④ 分配 size 字节
        handle->head = head;
        handle->size = size;
        handle->wakeup = local_read(&rb->wakeup_head);
        head += size;
    } while (local_cmpxchg(&rb->head, head, old_head) != head);
    // ★ 原子推进 head 指针

    return 0;
}
```

---

## 4. AUX Buffer — 大块 trace 数据

Intel Processor Trace 或 ARM Coresight 需要写入 MB 级连续的 trace 数据。AUX buffer 专门支持这种场景：

```c
// kernel/events/core.c 和 internal.h
struct perf_buffer {
    void *aux_priv;                          // PMU 特定的 AUX buffer 状态
    unsigned long aux_head;                  // 内核写入位置
    long aux_wakeup;                         // 唤醒位置
    unsigned long aux_size;                  // AUX buffer 大小
    int aux_nr_pages;                        // AUX buffer 的 page 数
    struct page **aux_pages;                 // ★ AUX buffer 的物理页数组
};
```

AUX buffer 通过独立的 `perf_aux_output_begin/end` 操作，不与主 ring buffer 竞争。读取时用户态通过 `PERF_RECORD_AUX` 记录知道在哪里取数据。

---

## 5. 总结

| 组件 | 关键函数 | 核心作用 |
|------|---------|---------|
| 用户态映射 | `mmap(fd, ...)` → `perf_mmap` 返回 `perf_event_mmap_page *` |
| 零系统调用轮询 | `READ_ONCE(pc->data_head)` | 用户态原子读 data_head |
| 内核写入 | `perf_output_begin` → `perf_output_end` | 原子推进 head，写入采样记录 |
| AUX 大块 trace | `perf_aux_output_begin` → `perf_aux_output_end` | 写入 Intel PT / ARM Coresight |
| 用户态确认读取 | `WRITE_ONCE(pc->data_tail, new)` | 用户态推进 tail，内核可重用这部分 |
