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
