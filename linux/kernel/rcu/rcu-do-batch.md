# RCU 回调与 RCU_SOFTIRQ — rcu_do_batch 与回调生命周期

> 源码：`kernel/rcu/tree.c` `kernel/softirq.c` | 📌 同系列 [RCU 内核源码深度解析](../rcu/rcu-deep-dive/README.md)

---

## 1. RCU 回调与 SOFTIRQ 的关系

RCU_SOFTIRQ 是 10 种 softirq 之一 (`include/linux/interrupt.h:558`)，优先级倒数第二（仅高于 HRTIMER_SOFTIRQ）。它的处理函数 `rcu_core_si` 通过 `open_softirq(RCU_SOFTIRQ, rcu_core_si)` 注册（`kernel/rcu/tree.c:4886`），但这个函数只做上下文无关的 housekeeping——真正的回调处理在 `rcu_core` 和 `rcu_do_batch` 中。

```
irq_exit() → invoke_softirq()
  → __do_softirq()
    → check pending: RCU_SOFTIRQ?
      → rcu_core_si()
        → raise_softirq(RCU_SOFTIRQ)? (nested pending? nope)
        → OR invoke_rcu_core() → rcu_core() → rcu_do_batch()
```

**关键**：`rcu_core_si` 不直接调用 `rcu_do_batch`。回调在 `rcu_core` 中执行，`rcu_core_si` 只是一个从 softirq 上下文触发 `rcu_core` 的桥梁。

---

## 2. 回调分段链表

每个 CPU 的 `struct rcu_data` 有一个 `struct rcu_segcblist cblist`（`kernel/rcu/tree.h:189-297`），通过 `rcu_segcblist.c` 管理。

```
     [DONE] → [NEXT] → [WAITING]  → ...
     ↑ 已过GP    ↑ 当前GP   ↑ 等下一GP
     └─ rcu_do_batch 从这里取回调
```

- **DONE** 段：GP 已经过了，回调可以安全执行。`rcu_do_batch` 遍历这一段，逐个调用 `head->func(head)`。
- **NEXT** 段：GP 正在进行中。GP 完成时整段搬到 DONE。
- **WAITING** 段：还未关联到 GP。新增的 `call_rcu` 请求先到这，GP 启动时搬到 NEXT。

---

## 3. rcu_core — 回调调度的中枢

```c
// kernel/rcu/tree.c:2835
static __latent_entropy void rcu_core(void)
{
    struct rcu_data *rdp = raw_cpu_ptr(&rcu_data);

    // ① 检查 GP 是否需要推进（fqs loop, QS report）
    rcu_check_quiescent_state(rdp);            // line 2839

    // ② 如果当前 CPU 有 RCU 工作需要做
    if (rcu_pending()) {                       // tree.c:516 函数
        // ...
    }

    // ③ ★ 处理回调
    rcu_do_batch(rdp);                         // line 2869
}
```

`rcu_core_si` 内部调用 `invoke_rcu_core()`（tree.c:1408），后者检查是否需要运行 `rcu_core`，需要则调用它。

---

## 4. rcu_do_batch — 真正执行回调

```c
// kernel/rcu/tree.c:2540
static void rcu_do_batch(struct rcu_data *rdp)
{
    long count = 0;                             // 本次处理的回调数
    long tlimit;                                // 时间限制（3ms）
    long jlimit_check, jlimit;                  // 最大回调数上限

    // 时间限制：最多执行 3ms，然后让出 CPU
    tlimit = DIV_ROUND_UP(3 * HZ, 1000);        // line 2545 — 3ms

    // 数量限制：最多 MAX_BATCH = 4096 个回调
    jlimit = MAX_BATCH;                         // 默认 4096

    if (unlikely(jlimit <= 0)) {
        // 如果 jlimit <= 0，一次只处理一个回调
        count = 1;
        goto handle_one;
    }

    // ★ 主循环：从 DONE 段逐个取回调
    list = rcu_segcblist_dequeue(&rdp->cblist);  // 取出第一个

    while (list) {
        struct rcu_head *rhp = container_of(list, struct rcu_head, next);

        // 执行回调
        rhp->func(rhp);                          // ★ 调用 call_rcu 注册的函数

        count++;

        // 每 count % jlimit_check == 0 时检查以下条件：
        if (rcu_do_batch_check_time(count, tlimit, jlimit_check, jlimit)) {
            // ① 时间超过 3ms → 停止
            // ② 回调数超过 MAX_BATCH → 停止
            // ③ 有高优先级任务需要 CPU → 停止
            invoke_rcu_core();                   // 触发下一轮 rcu_core
            return;
        }

        list = rcu_segcblist_dequeue(&rdp->cblist);
    }
}
```

三条退出条件（`rcu_do_batch_check_time`）：
1. **时间限制**：超过 3ms，让出 CPU 给其他任务
2. **数量限制**：超过 `MAX_BATCH`（4096 个回调），分多轮处理
3. **优先级检查**：有更高优先级任务需要 CPU（`need_resched()` 被置位）

---

## 5. 完整生命周期

```
call_rcu(head, func)
  → __call_rcu_common(head, func)            [tree.c:3102]
    → rcu_segcblist_enqueue(&rdp->cblist, head)   // 进入 WAITING 段
    → 如果没有活跃的 GP → 启动 GP               // 标记 RCU_GP_FLAG_INIT
    → 返回

rcu_gp_kthread:
  → rcu_gp_init(): WAITING → NEXT              // GP 开始
  → rcu_gp_fqs_loop(): 等 QS                  // GP 进行中
  → rcu_gp_cleanup(): NEXT → DONE             // GP 结束
    → raise_softirq(RCU_SOFTIRQ)               // 通知有回调可以处理

irq_exit → __do_softirq:
  → RCU_SOFTIRQ pending → rcu_core_si()
    → invoke_rcu_core()
      → rcu_core()
        → rcu_do_batch(rdp)                    // ★ 逐个调用 func(head)
          → head->func(head)                   // 回调执行
          → kfree(ptr) or whatever             // 释放内存
```

---

## 6. 与 rcu_barrier 的关系

`rcu_barrier`（tree.c:3817）等所有 CPU 的回调完成。它在每个 CPU 上排队一个 barrier 回调，然后用 completion 等所有 barrier 回调执行完毕。这不同于 `synchronize_rcu`（等一次 GP 完成）——barrier 等的是回调执行完成。

```c
// 关键区别
synchronize_rcu():  GP 完成 → 返回。但之前的 call_rcu 回调可能还没在 DONE 段被执行。
rcu_barrier():     所有之前的 call_rcu 回调都已执行完毕 → 返回。
```

---

## 7. 总结

| 阶段 | 函数 | 说明 |
|------|------|------|
| **注册回调** | `call_rcu` / `__call_rcu_common` | 挂入 WAITING 段 |
| **GP 推进** | `rcu_gp_kthread` | WAITING → NEXT → DONE |
| **触发处理** | `rcu_gp_cleanup` | GP 结束时 raise RCU_SOFTIRQ |
| **调度执行** | `rcu_core_si` → `rcu_core` | 从 softirq 调度处理 |
| **真正执行** | `rcu_do_batch` | 遍历 DONE 段，逐个调用 |
| **限制** | 3ms/4096 个/need_resched | 防止 softirq 占 CPU 太久 |
