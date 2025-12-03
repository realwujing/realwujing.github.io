---
title: 'fio_ceph'
date: '2025/04/01 09:50:38'
updated: '2025/04/01 09:50:38'
---

# fio_ceph

## 根因定位

确认文件的底层设备:
```bash
df -h /data/rbd_test
Filesystem      Size  Used Avail Use% Mounted on
/dev/vda1        40G   31G  9.9G  76% /
```

### bs=226

运行 fio 测试（使用 /data/rbd_test）:
```bash
fio -direct=1 -iodepth=4 -rw=randwrite -ioengine=libaio -bs=226k -thread -numjobs=1 -runtime=30 -group_reporting -filename=/data/rbd_test -name=1x -size=10G -time_based -ramp_time=3
```

```bash
1x: (g=0): rw=randwrite, bs=(R) 226KiB-226KiB, (W) 226KiB-226KiB, (T) 226KiB-226KiB, ioengine=libaio, iodepth=4
fio-3.29
Starting 1 thread
Jobs: 1 (f=1): [w(1)][100.0%][w=88.1MiB/s][w=399 IOPS][eta 00m:00s]
1x: (groupid=0, jobs=1): err= 0: pid=13949: Mon Mar 31 19:48:55 2025
  write: IOPS=400, BW=88.5MiB/s (92.8MB/s)(2655MiB/30001msec); 0 zone resets
    slat (usec): min=1521, max=14131, avg=2490.98, stdev=730.96
    clat (usec): min=2, max=18944, avg=7483.45, stdev=1286.01
     lat (usec): min=2219, max=23386, avg=9974.92, stdev=1481.33
    clat percentiles (usec):
     |  1.00th=[ 5997],  5.00th=[ 6259], 10.00th=[ 6390], 20.00th=[ 6587],
     | 30.00th=[ 6718], 40.00th=[ 6915], 50.00th=[ 7046], 60.00th=[ 7242],
     | 70.00th=[ 7570], 80.00th=[ 8160], 90.00th=[ 9372], 95.00th=[10290],
     | 99.00th=[11994], 99.50th=[12649], 99.90th=[14353], 99.95th=[15664],
     | 99.99th=[18744]
   bw (  KiB/s): min=84524, max=95372, per=100.00%, avg=90636.82, stdev=2215.92, samples=60
   iops        : min=  374, max=  422, avg=400.90, stdev= 9.76, samples=60
  lat (usec)   : 4=0.01%
  lat (msec)   : 4=0.01%, 10=93.70%, 20=6.31%
  cpu          : usr=0.48%, sys=0.82%, ctx=12027, majf=0, minf=0
  IO depths    : 1=0.0%, 2=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,12027,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=4

Run status group 0 (all jobs):
  WRITE: bw=88.5MiB/s (92.8MB/s), 88.5MiB/s-88.5MiB/s (92.8MB/s-92.8MB/s), io=2655MiB (2784MB), run=30001-30001msec

Disk stats (read/write):
  vda: ios=0/13203, merge=0/0, ticks=0/32673, in_queue=32690, util=99.73%
```

#### blktrace I/O

对底层设备运行 blktrace:
```bash
blktrace -d /dev/vda1 -o fio_trace
```

停止 blktrace：
```bash
killall blktrace
```

合并所有 CPU 核心的日志文件:
```bash
blkparse -i fio_trace -o merged_trace.txt
```

生成二进制格式（供 btt 分析）:
```bash
blkparse -i fio_trace -d fio_trace.bin
```

计算 I/O 延迟分布（Q2Q, D2C 等）:
```bash
btt -i fio_trace.bin -l latency.log
```

查看 merged_trace.txt 中的典型请求：
```bash
head -n 20 merged_trace.txt
```

从 `merged_trace.txt` 前20行内容来看，这是 `blktrace` 记录的完整I/O请求生命周期。以下是逐行解析和关键分析点：

---

##### **1. 日志字段说明**
每行格式为：  
`设备号 CPU核心 事件序号 时间戳(秒) PID 动作类型 读写类型 起始扇区 + 扇区数 [进程名]`

| 字段          | 示例值         | 说明                                                                 |
|---------------|----------------|----------------------------------------------------------------------|
| **设备号**    | `253,0`        | 主设备号253,次设备号0（可能是Ceph RBD设备）                          |
| **动作类型**  | `A/Q/G/I/D/C`  | 完整生命周期：`A`(分配)→`Q`(入队)→`G`(合并)→`I`(插入)→`D`(下发)→`C`(完成) |
| **读写类型**  | `WS`           | 表示写操作（Write Synchronous）                                      |
| **扇区信息**  | `9580600 + 452`| 起始扇区9580600，连续452个扇区（452*512B=226KB，与fio的`bs=226k`匹配） |

---

##### **2. 单次I/O请求生命周期示例**
以第一个请求为例：
```plaintext
253,0    2        1     0.000000000 10936  A  WS 9580600 + 452 <- (253,1) 9578552  # (1) 分配请求
253,1    2        2     0.000001375 10936  Q  WS 9580600 + 452 [fio]               # (2) 进入队列
253,1    2        3     0.000006249 10936  G  WS 9580600 + 452 [fio]               # (3) 尝试合并
253,1    2        4     0.000006803 10936  P   N [fio]                            # (4) 插件处理
253,1    2        5     0.000007635 10936  U   N [fio] 1                          # (5) 解锁操作
253,1    2        6     0.000008354 10936  I  WS 9580600 + 452 [fio]              # (6) 插入调度队列
253,1    2        7     0.000011215 10936  D  WS 9580600 + 452 [fio]              # (7) 下发到设备
253,1    3        1     0.002354230     0  C  WS 9580600 + 452 [0]                # (8) 完成（耗时2.35ms）
```

---

##### **3. 关键分析结论**
###### **(1) I/O 请求大小确认**
- 所有请求均为 **226KB**（`+ 452` 扇区，452*512B=226KB），与 `fio` 参数 `-bs=226k` 一致。
- **无请求拆分**：未出现类似 `+ 256 + 196` 的分片记录，说明内核已完整处理226KB请求。

###### **(2) 延迟分布**
- **D2C (下发→完成) 延迟**：  
  - 第一个请求：`0.002354230 - 0.000011215 ≈ 2.34ms`  
  - 第二个请求：`0.004724208 - 0.002401227 ≈ 2.32ms`  
  - **结论**：延迟稳定，无明显异常。

###### **(3) 请求合并情况**
- **`G` (合并) 事件存在**：  
  每个请求都经历了 `G` 阶段，但未与其他请求合并（因无相邻扇区请求）。  
  **优化建议**：增加 `fio` 的 `-iodepth` 可能提升合并机会。

###### **(4) 设备号差异**
- **253,0 和 253,1**：  
  - `253,0` 仅记录 `A` (分配) 动作，实际I/O由 `253,1` 处理。  
  - **可能场景**：`253,0` 是虚拟设备，`253,1` 是物理设备或Ceph OSD后端。

###### **(5) 同步写入 (`WS`)**
- **`WS` 标志**：表示同步写（Write Synchronous），可能因 `fio` 的 `-direct=1` 绕过缓存导致。  
  **影响**：延迟高于异步写，但数据安全性更高。

---

计算平均/最大/95分位延迟（单位：毫秒）:
```bash
awk '{sum+=$2; if($2>max)max=$2; a[i++]=$2} END {
  print "avg=",sum/NR*1000,"ms", "max=",max*1000,"ms";
  asort(a); print "95%=",a[int(NR*0.95)]*1000,"ms"
}' latency.log_253,1_d2c.dat
```

根据您的 `awk` 分析结果，`latency.log_253,1_d2c.dat` 中的 D2C（Dispatch to Complete）延迟统计如下：

```plaintext
avg= 2.42779 ms  # 平均延迟
max= 18.679 ms   # 最大延迟
95%= 3.683 ms    # 95分位延迟
```

---

**1. 关键指标解读**
| **指标**   | **值**      | **分析结论**                                                                 |
|------------|-------------|-----------------------------------------------------------------------------|
| **平均延迟** | 2.43 ms     | 属于正常范围（NVMe SSD或高速Ceph集群的典型延迟）                             |
| **95%延迟** | 3.68 ms     | 大多数请求延迟可控，但仍有5%请求超过此值                                     |
| **最大延迟** | 18.68 ms    | **异常值**，可能是因Ceph OSD负载波动、网络抖动或宿主机调度导致               |

---

### bs=256

对底层设备运行 blktrace:
```bash
blktrace -d /dev/vda1 -o fio_trace
```

运行 fio 测试（使用 /data/rbd_test）:
```bash
fio -direct=1 -iodepth=4 -rw=randwrite -ioengine=libaio -bs=256k -thread -numjobs=1 -runtime=30 -group_reporting -filename=/data/rbd_test -name=1x -size=10G -time_based -ramp_time=3
```

输出如下：

```bash
1x: (g=0): rw=randwrite, bs=(R) 256KiB-256KiB, (W) 256KiB-256KiB, (T) 256KiB-256KiB, ioengine=libaio, iodepth=4
fio-3.29
Starting 1 thread
Jobs: 1 (f=1): [w(1)][100.0%][w=120MiB/s][w=480 IOPS][eta 00m:00s]
1x: (groupid=0, jobs=1): err= 0: pid=14032: Mon Mar 31 19:54:07 2025
  write: IOPS=479, BW=120MiB/s (126MB/s)(3601MiB/30008msec); 0 zone resets
    slat (usec): min=6, max=140, avg=22.37, stdev= 6.99
    clat (usec): min=1540, max=44540, avg=8317.85, stdev=13369.98
     lat (usec): min=1567, max=44566, avg=8340.39, stdev=13368.29
    clat percentiles (usec):
     |  1.00th=[ 1778],  5.00th=[ 1909], 10.00th=[ 1991], 20.00th=[ 2073],
     | 30.00th=[ 2147], 40.00th=[ 2212], 50.00th=[ 2311], 60.00th=[ 2376],
     | 70.00th=[ 2540], 80.00th=[ 3392], 90.00th=[38011], 95.00th=[38536],
     | 99.00th=[39584], 99.50th=[40109], 99.90th=[41681], 99.95th=[42206],
     | 99.99th=[44303]
   bw (  KiB/s): min=122368, max=123126, per=100.00%, avg=122908.02, stdev=122.92, samples=60
   iops        : min=  478, max=  480, avg=479.93, stdev= 0.36, samples=60
  lat (msec)   : 2=11.20%, 4=70.03%, 10=2.10%, 50=16.69%
  cpu          : usr=0.53%, sys=0.82%, ctx=13333, majf=0, minf=0
  IO depths    : 1=0.0%, 2=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,14399,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=4

Run status group 0 (all jobs):
  WRITE: bw=120MiB/s (126MB/s), 120MiB/s-120MiB/s (126MB/s-126MB/s), io=3601MiB (3775MB), run=30008-30008msec

Disk stats (read/write):
  vda: ios=0/16335, merge=0/0, ticks=0/131543, in_queue=131578, util=99.53%
```

停止 blktrace：
```bash
killall blktrace
```

合并所有 CPU 核心的日志文件:
```bash
blkparse -i fio_trace -o merged_trace.txt
```

生成二进制格式（供 btt 分析）:
```bash
blkparse -i fio_trace -d fio_trace.bin
```

计算 I/O 延迟分布（Q2Q, D2C 等）:
```bash
btt -i fio_trace.bin -l latency.log
```