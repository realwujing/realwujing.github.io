---
date: 2024/08/30 16:11:13
updated: 2024/09/09 10:16:43
---

# UnixBench Pipe Analysis

```bash
./Run pipe -c 1
./Run context1 -c 1
./Run spawn -c 1
```

![20240830153058](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830153058.png)

```bash
./Run pipe -c 1
```

![./Run pipe -c 1](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830104454.png)

```bash
1 x Pipe Throughput  1 2 3 4 5 6 7 8 9 10

========================================================================
   BYTE UNIX Benchmarks (Version 5.1.3)

   System: ctyun: GNU/Linux
   OS: GNU/Linux -- 4.19.90-2102.2.0.0070.1.all.rc5.bbm.test.rename1.ctl2.aarch64 -- #1 SMP Wed Aug 21 15:57:25 CST 2024
   Machine: aarch64 (aarch64)
   Language: en_US.utf8 (charmap="UTF-8", collate="UTF-8")
   10:59:14 up 8 days, 18:27,  1 user,  load average: 4.00, 4.00, 4.00; runlevel 3

------------------------------------------------------------------------
Benchmark Run:  8 30 2024 10:59:14 - 11:01:25
0 CPUs in system; running 1 parallel copy of tests

Pipe Throughput                             1543364.7 lps   (10.0 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
Pipe Throughput                               12440.0    1543364.7   1240.6
                                                                   ========
System Benchmarks Index Score (Partial Only)                         1240.6
```

```bash
perf record -F 99 -g pgms/pipe 10
```

```bash
perf report
```

![perf record -F 99 -g pgms/pipe 10](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/企业微信截图_79ad3eac-52e6-4f54-bd28-cb8fc2b0a0eb.png)

```bash
perf script | /home/wujing/code/FlameGraph/stackcollapse-perf.pl | /home/wujing/code/FlameGraph/flamegraph.pl > pipe.svg
```

![pipe](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/pipe.svg)

![kylin-pipe](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/kylin-pipe.svg)

```bash
:grep '(struct wait_queue_entry *wq_entry, unsigned mode, int flags, void *key)' . -inrF --include=*.h

:cl
 1 ./include/linux/wait.h:16: typedef int (*wait_queue_func_t)(struct wait_queue_entry *wq_entry, unsigned mode, int flags, void *key);
 2 ./include/linux/wait.h:17: int default_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int flags, void *key);
```

```bash
:cs f t wake_function(wait_queue_entry_t

:cl
 1 fs/gfs2/glock.c:93: <<<unknown>>> static int glock_wake_function(wait_queue_entry_t *wait, unsigned int mode,
 2 fs/userfaultfd.c:106: <<<unknown>>> static int userfaultfd_wake_function(wait_queue_entry_t *wq, unsigned mode,
 3 kernel/sched/core.c:4172: <<<unknown>>> int default_wake_function(wait_queue_entry_t *curr, unsigned mode, int wake_flags,
 4 mm/memcontrol.c:1890: <<<unknown>>> static int memcg_oom_wake_function(wait_queue_entry_t *wait,
 5 mm/shmem.c:2005: <<<unknown>>> static int synchronous_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
 6 net/core/datagram.c:72: <<<unknown>>> static int receiver_wake_function(wait_queue_entry_t *wait, unsigned int mode, int sync,
```


```bash
git log -S 'wake_up_interruptible_sync_poll' --oneline fs/pipe.c | cat

6551d5c56eb0 pipe: make sure to wake up everybody when the last reader/writer closes
0ddad21d3e99 pipe: use exclusive waits when reading or writing
f467a6a66419 pipe: fix and clarify pipe read wakeup logic
1b6b26ae7053 pipe: fix and clarify pipe write wakeup logic
3c0edea9b29f pipe: Remove sync on wake_ups
7e25a73f1a52 pipe: Remove redundant wakeup from pipe_write()
a194dfe6e6f6 pipe: Rearrange sequence in pipe_write() to preallocate slot
8446487feba9 pipe: Conditionalise wakeup in pipe_read()
b667b8673443 pipe: Advance tail pointer inside of wait spinlock in pipe_read()
```

```c
// vim fs/pipe.c +106

106 void pipe_wait(struct pipe_inode_info *pipe)
107 {
108     DEFINE_WAIT(wait);
109
110     /*
111      * Pipes are system-local resources, so sleeping on them
112      * is considered a noninteractive wait:
113      */
114     prepare_to_wait(&pipe->wait, &wait, TASK_INTERRUPTIBLE);
115     pipe_unlock(pipe);
116     schedule();
117     finish_wait(&pipe->wait, &wait);
118     pipe_lock(pipe);
119 }
```
