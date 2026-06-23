# sched_setattr 中 cpuset_mutex 竞争导致进程 D 状态

## 现象

`stress-ng-sched` 多线程并发调用 `sched_setattr` 设置 SCHED_DEADLINE 策略时，大量线程进入 D 状态，卡在 `cpuset_mutex` 上。

### 调用栈

```
PID: 22884    TASK: ffff29005e23c200    CPU: 180    COMMAND: "stress-ng-sched"
#0  switch_to
#1  schedule
#2  schedule
#3  schedule_preempt_disabled
#4  mutex_lock_constprop_0
#5  mutex_lock_slowpath
#6  mutex_lock
#7  cpuset_lock
#8  sched_setscheduler
#9  arm64_sys_sched_setattr
#10 invoke_syscall
#11 el0_svc_common
#12 do_el0_svc
#13 el0_svc
#14 el0_t6_sync_handler
#15 el0_t6_sync
```

所有等锁的进程都是 `stress-ng-sched`，都在抢同一把 `cpuset_mutex`，其余没获取到 mutex 的进程自旋等待，状态为 D。

## 分析

### 根因

全局 `cpuset_mutex` 是 `__sched_setscheduler()` 中为保障 SCHED_DEADLINE 带宽计费正确性而获取的锁，所有并发 DL 策略设置操作被该锁串行化。

### 代码路径

**`cpuset_mutex` 的定义** — `kernel/cgroup/cpuset.c:465`:

```c
static DEFINE_MUTEX(cpuset_mutex);

void cpuset_lock(void)
{
    mutex_lock(&cpuset_mutex);
}
```

**触发条件** — `kernel/sched/core.c:8243-8246`:

```c
/*
 * SCHED_DEADLINE bandwidth accounting relies on stable cpusets
 * information.
 */
if (dl_policy(policy) || dl_policy(p->policy)) {
    cpuset_locked = true;
    cpuset_lock();
}
```

只有当前策略或新策略为 `SCHED_DEADLINE` 时才会获取 `cpuset_mutex`。

**持锁范围长**：获取 `cpuset_mutex` 后（8245行），紧接着获取 rq lock（8255行），然后执行以下操作，全部在持锁状态下：

- rq stop 检查
- 策略校验（RT/VMS/公平策略组带宽检查）
- 亲和性检查
- policy 重检查及可能的 recheck 路径
- DL 带宽溢出检查
- dequeue / enqueue 操作
- class change 通知

直到函数出口才释放锁（8432-8436行）。

**解锁路径**：函数有三个出口，均由 `cpuset_locked` 标志控制解锁：

1. `unlock` 标签（8432-8436行）— 错误返回路径
2. `recheck` 路径（8348-8354行）— policy 变更重试时先解锁再跳回 `recheck`，重新走判断逻辑再获取
3. 正常返回路径（8420-8424行）— 仅在 pi 模式下解锁，非 pi 模式由 unlock 标签统一处理

## 结论

**这不是内核 bug，是设计预期行为。**

- `cpuset_mutex` 的注释（`cpuset.c:427-434`）明确说明：DL 带宽计费依赖稳定的 cpuset 信息，必须用 mutex 串行化以确保 cpuset 在 DL 任务调度属性变更期间不变
- 锁的获取/释放在所有路径上都是正确配对的，不存在泄漏或死锁问题
- 大量并发 DL `sched_setattr` 调用必然在此处排队，进程进入 D 状态等待 mutex 是正常的竞态表现

## 解决方法

1. **减少测试并发度** — `stress-ng` 限制 DL 策略相关的并发线程数
2. **非 DL 策略变更不受影响** — 如果不需要 DL 带宽计费，使用 RT/FIFO 等其他策略不会触发 `cpuset_mutex` 竞争
3. **内核层面优化（上游工作）** — 将 cpuset 状态改为 RCU 保护的快照方式，减少持锁范围，但涉及 DL 带宽计费语义的较大改动
