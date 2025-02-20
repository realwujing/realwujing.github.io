---
date: 2024/08/30 16:11:13
updated: 2024/08/30 16:11:13
---

# UnixBench Context1 Analysis

```bash
./Run pipe -c 1
./Run context1 -c 1
./Run spawn -c 1
```

![./Run context1 -c 1](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830104823.png)

```bash
1 x Pipe-based Context Switching  1 2 3 4 5 6 7 8 9 10

========================================================================
   BYTE UNIX Benchmarks (Version 5.1.3)

   System: ctyun: GNU/Linux
   OS: GNU/Linux -- 4.19.90-2102.2.0.0070.1.all.rc5.bbm.test.rename1.ctl2.aarch64 -- #1 SMP Wed Aug 21 15:57:25 CST 2024
   Machine: aarch64 (aarch64)
   Language: en_US.utf8 (charmap="UTF-8", collate="UTF-8")
   11:03:52 up 8 days, 18:32,  1 user,  load average: 4.06, 4.16, 4.08; runlevel 3

------------------------------------------------------------------------
Benchmark Run:  8 30 2024 11:03:52 - 11:06:02
0 CPUs in system; running 1 parallel copy of tests

Pipe-based Context Switching                 236058.2 lps   (10.0 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
Pipe-based Context Switching                   4000.0     236058.2    590.1
                                                                   ========
System Benchmarks Index Score (Partial Only)                          590.1
```

```bash
perf record -F 99 -g pgms/context1 10
```

```bash
perf report
```

![perf record -F 99 -g pgms/context1 10](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830105627.png)

```bash
perf script | /home/wujing/code/FlameGraph/stackcollapse-perf.pl | /home/wujing/code/FlameGraph/flamegraph.pl > context1.svg
```

![context1](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/context1.svg)