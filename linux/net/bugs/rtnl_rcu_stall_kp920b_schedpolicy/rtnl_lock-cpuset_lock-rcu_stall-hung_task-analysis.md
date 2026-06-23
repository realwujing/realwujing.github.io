# Kunpeng 920B stress-ng schedpolicy 用例卡死分析：rtnl_lock + cpuset_lock + RCU stall 三板斧

## 测试环境

| 项目 | 值 |
|------|-----|
| 硬件 | Huawei S920X20 / BC83AMDA01, BIOS 09.04.02.01.13 01/11/2025 |
| CPU | Kunpeng 920B, 256 核 (ncpus=256) |
| 架构 | aarch64 |
| 内核 | 6.6.0-0010.rc2.ctl4 (Tainted: G O) |
| 环境 | 北七家，10.15.8.31 |
| 测试用例 | scheduler 用例集 → schedpolicy |
| 现象 | 运行一段时间后系统卡死，所有进程状态 D |

## 现象概述

stress-ng schedpolicy 测试运行后，系统出现大规模 D 状态进程，hung task detector 反复触发，RCU stall 告警不断。关键时间段：

```
04:28:12  DL replenish lagged too much    ← DL 调度告警
04:32:20  RT throttling activated          ← RT 限流触发
04:51:35  hung task: kworker blocked for 1310s on rtnl_lock
05:09:33  RCU stall detected, rcuc_sched kthread starved
05:13:32  更多 rtnl_lock / cpuset_lock 阻塞任务出现
07:31:21  RCU self-detected stall on CPU 53 (fqdir_free_fn)
14:54:26  sysrq Show Blocked State (~100 stress-ng-sched 任务全部 D)
```

## D 状态任务分类

### 类型 A：阻塞在 rtnl_lock

**addrconf_verify_work** (ipv6_addrconf workqueue)：
```
 task:kworker/126:2  state:D  pid:32445
 Workqueue: ipv6_addrconf addrconf_verify_work
  __mutex_lock.constprop.0
  mutex_lock
  rtnl_lock
  addrconf_verify_work+0x20/0x48          ← net/ipv6/addrconf.c:4705
```

**default_device_exit_batch** (netns cleanup_net workqueue)：
```
 task:kworker/u512:1  state:D  pid:300036
 Workqueue: netns cleanup_net
  __mutex_lock.constprop.0
  mutex_lock
  rtnl_lock
  default_device_exit_batch+0x48/0x328    ← net/core/dev.c:11639
```

`addrconf_verify_work` 和 `default_device_exit_batch` 都在等 **同一个人释放 rtnl_lock**，但该锁持有者迟迟不释放。

### 类型 B：阻塞在 cpuset_lock

~100 个 `stress-ng-sched` 进程全部阻塞在同一处：
```
 task:stress-ng-sched  state:D  pid:2938931~2939186
  __mutex_lock.constprop.0
  mutex_lock
  cpuset_lock
  __sched_setscheduler+0x404/0x980        ← kernel/sched/core.c:8244-8245
  __arm64_sys_sched_setattr
```

这些任务都在 `sched_setattr()` 系统调用中尝试拿 `cpuset_mutex`。

## 根因分析：三把锁的连环死锁

### 锁排序关系

```
顺向路径 (合法):
  rtnl_lock → unregister_netdevice_many → synchronize_net → 等 RCU GP

逆向路径 (大量并发):
  __sched_setscheduler → cpuset_lock → (持有 cpuset_mutex 的 CPU 可能被 RT/DL 任务抢占)

RCU GP 依赖 (雪崩):
  synchronize_net 等 RCU GP → RCU GP 等所有 CPU 经过 quiescent state
   → 某些 CPU 上 busy-waiting rwlock (mm_update_next_owner 大量 exit 争锁)
   → RCU GP kthread 得不到 CPU 时间 (starved)
   → rtnl_lock 永远不释放
   → 所有需要 rtnl_lock 的任务 D 住
   → 包括 cleanup_net → default_device_exit_batch 自己可能也 D 在下一轮
```

### 详细路径展开

#### Step 1: rtnl_lock 被持有，synchronize_net 卡住

rtnl_lock 的持有者在 `default_device_exit_batch` 中（`net/core/dev.c:11639-11654`）：

```c
static void __net_exit default_device_exit_batch(struct list_head *net_list)
{
    ...
    rtnl_lock();                            // line 11639
    // ... unregister all net devices ...
    unregister_netdevice_many(&dev_kill_list);  // line 11653
    rtnl_unlock();
}
```

`unregister_netdevice_many` 内部调用 `synchronize_net()`（本质是 `synchronize_rcu()`），必须等待 RCU grace period 完成才能返回。

#### Step 2: RCU grace period 无法完成

dmesg 中 RCU stall 证据：

```
05:09:33  rcu_sched kthread starved for 10920 jiffies!
          rcuc_sched state:R running task  pid:16  cpu:40
          CPU 40: stress-ng-exit- pid:3916367
            native_queued_spin_lock_slowpath
            queued_read_lock_slowpath
            mm_update_next_owner              ← rwlock 争用
            do_exit → do_group_exit → exit_group
```

```
07:31:21  RCU self-detected stall on CPU 53 (t=15000 jiffies)
          CPU 53: kworker/53:2  pid:2098
          Workqueue: events fqdir_free_fn
            smp_call_function_single           ← 卡死在跨 CPU 调用
            rcu_barrier
            fqdir_free_fn
```

RCU GP 无法完成的三个原因叠加：
1. **CPU 40** 上大量 `stress-ng-exit-` 进程退出，在 `mm_update_next_owner` 中争抢 `tasklist_lock` (rwlock)，无法进入 RCU quiescent state
2. **CPU 53** 上 `fqdir_free_fn` 调用 `rcu_barrier()`，而 `rcu_barrier` 需要通过 `smp_call_function_single` 通知所有 CPU，但某些 CPU 正忙
3. **256 核** 大规模并发 exit + SCHED_DEADLINE 任务抢 CPU，`rcu_sched` kthread 得不到足够 CPU 时间 (starved for 10920 jiffies ~= 43s @250Hz)

#### Step 3: cpuset_lock 雪崩

stress-ng schedpolicy 测试创建大量 SCHED_DEADLINE 任务，`__sched_setscheduler` 中对 DL 任务强制拿 cpuset_lock（`kernel/sched/core.c:8243-8245`）：

```c
if (dl_policy(policy) || dl_policy(p->policy)) {
    cpuset_locked = true;
    cpuset_lock();        // mutex_lock(&cpuset_mutex)
}
```

cpuset_mutex 是全局互斥锁，大量 DL 任务并发串行化在这个锁上。持有 cpuset_mutex 的进程如果所在 CPU 正忙于处理 exit/RCU/rtnl 相关操作，迟迟不释放 → 后面所有 DL 任务全部排队 → D 状态雪崩。

#### Step 4: 三板斧闭合

```
                     ┌─────────────────────────────┐
                     │   rtnl_lock 持有者            │
                     │  (default_device_exit_batch)  │
                     │   → synchronize_net()         │
                     └──────────────┬──────────────┘
                                    │ 等待
                     ┌──────────────▼──────────────┐
                     │      RCU grace period        │
                     │  被 starved/blocked          │
                     │  - CPU40: mm_update_next_owner│
                     │  - CPU53: fqdir_free_fn       │
                     │  - rcuc_sched starved 43s     │
                     └──────────────┬──────────────┘
                                    │ 导致
        ┌───────────────────────────┼───────────────────────────┐
        │                           │                           │
        ▼                           ▼                           ▼
  rtnl_lock 等锁者             rtnl_lock 等锁者           cpuset_lock 等锁者
  (addrconf_verify_work)   (cleanup_net next batch)    (~100 stress-ng-sched)
```

### 源码级锁分析

| 锁 | 持有者路径 | 等待者路径 |
|----|-----------|-----------|
| rtnl_mutex | `default_device_exit_batch` → `unregister_netdevice_many` → `synchronize_net` | `addrconf_verify_work`, 更多 `cleanup_net` |
| rtnl_mutex 所等 | RCU grace period (因 `synchronize_net`) | — |
| cpuset_mutex | 某个在 CPU 上跑但被 RT/DL 抢占的 `__sched_setscheduler` 调用 | ~100 `stress-ng-sched` 进程 |

注意：`cpuset_lock()` 实际是 `mutex_lock(&cpuset_mutex)`（`kernel/cgroup/cpuset.c:467`），而 `cpuset_mutex` 和 `rtnl_mutex` 在正常路径中并**不直接存在 ABBA 依赖**，但在这里通过 **RCU stall → CPU 饥饿** 间接形成了**时序依赖**。

## 关键 dmesg 摘要

```
# 1. RT/DL 调度压力告警
04:28:12  sched: DL replenish lagged too much
04:32:20  sched: RT throttling activated

# 2. 第一个 hung task 检测 (阻塞 1310s)
04:51:35  task:kworker/118:5  blocked for 1310s
          Workqueue: ipv6_addrconf addrconf_verify_work
          → rtnl_lock

# 3. RCU stall 检测
05:09:33  rcu_sched kthread starved for 10920 jiffies!
          CPU 40: stress-ng-exit → mm_update_next_owner (rwlock)
          q=81868 ← RCU callback 积压严重

# 4. 更多 rtnl_lock 阻塞者
05:13:32  task:kworker/126:2  blocked for 1316s (addrconf_verify_work)
          task:kworker/118:5  blocked for 2627s (addrconf_verify_work)
          task:kworker/u512:1 blocked for 1316s (default_device_exit_batch)

# 5. 第二个 RCU stall
07:31:21  RCU self-detected stall on CPU 53
          CPU 53: fqdir_free_fn → rcu_barrier → smp_call_function_single
          q=8869

# 6. sysrq 显示 ~100 stress-ng-sched 全部 D 在 cpuset_lock
14:54:26  sysrq: Show Blocked State
          所有 stress-ng-sched: cpuset_lock → __sched_setscheduler
```

## 结论与建议

### 直接原因

`rtnl_lock` 持有者 (`default_device_exit_batch`) 在 `synchronize_net()` 中等待 RCU grace period，但 RCU GP 由于：
1. 大量进程 exit 导致 CPU 上 rwlock 争用（`mm_update_next_owner`）
2. `fqdir_free_fn` 中 `rcu_barrier` 卡在 `smp_call_function_single`
3. 256 核系统上 `rcu_sched` kthread 得不到足够 CPU 时间

三重因素叠加，RCU GP 长时间无法完成 → rtnl_lock 不释放 → 整个系统 D 住。

### 建议措施

1. **短期 workaround**：限制并发 netns 数量，避免 `cleanup_net` workqueue 积压触发 `default_device_exit_batch` 大规模调用 `synchronize_net()`
2. **SCHED_DEADLINE cpuset_lock 优化**：`__sched_setscheduler` 对 DL 任务的 `cpuset_lock()` 是已有上游讨论的性能瓶颈，可参考社区 patch 减少 DL bandwidth check 对 cpuset_mutex 的依赖
3. **RCU 参数调优**：在 256 核系统上可考虑增大 `rcu_task_stall_timeout` 或调整 `jiffies_till_next_fqs`，避免过激的 stall 检测
4. **测试场景优化**：schedpolicy 测试创建大量 DL 任务的同时会触发 netns 创建/销毁，建议隔离网络压力和调度压力测试
