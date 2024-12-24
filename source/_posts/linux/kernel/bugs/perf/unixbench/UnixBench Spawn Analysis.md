---
date: 2024/08/30 16:11:13
updated: 2024/08/30 16:11:13
---

# UnixBench Spawn Analysis

```bash
./Run spawn -c 1
```

![./Run spawn -c 1](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830112600.png)

```bash
1 x Process Creation  1 2 3

========================================================================
   BYTE UNIX Benchmarks (Version 5.1.3)

   System: ctyun: GNU/Linux
   OS: GNU/Linux -- 4.19.90-2102.2.0.0070.1.all.rc5.bbm.test.rename1.ctl2.aarch64 -- #1 SMP Wed Aug 21 15:57:25 CST 2024
   Machine: aarch64 (aarch64)
   Language: en_US.utf8 (charmap="UTF-8", collate="UTF-8")
   11:19:35 up 8 days, 18:47,  1 user,  load average: 4.00, 4.02, 4.07; runlevel 3

------------------------------------------------------------------------
Benchmark Run:  8 30 2024 11:19:35 - 11:21:14
0 CPUs in system; running 1 parallel copy of tests

Process Creation                               5287.2 lps   (30.0 s, 2 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
Process Creation                                126.0       5287.2    419.6
                                                                   ========
System Benchmarks Index Score (Partial Only)                          419.6
```

```bash
perf record -F 99 -g pgms/spawn 30
```

```bash
perf report
```

![perf record -F 99 -g pgms/spawn 30](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240830113233.png)

生成perf火焰图:

```bash
perf script | /home/wujing/code/FlameGraph/stackcollapse-perf.pl | /home/wujing/code/FlameGraph/flamegraph.pl > spawn.svg
```

![spawn](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/spawn.svg)
