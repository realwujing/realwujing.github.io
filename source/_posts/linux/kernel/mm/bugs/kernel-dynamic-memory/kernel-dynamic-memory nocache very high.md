---
title: 'kernel-dynamic-memory nocache very high'
date: '2025/04/11 21:07:34'
updated: '2025/04/11 21:07:34'
---

# kernel-dynamic-memory nocache very high

## 环境

内存占用过高，怀疑有10 GB内存丢失。

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# uname -a
Linux gz03-SDK-server-10e50e63e22 3.10.0-957.el7.x86_64 #1 SMP Thu Nov 8 23:39:32 UTC 2018 x86_64 x86_64 x86_64 GNU/Linux
```

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# cat /etc/os-release
NAME="CentOS Linux"
VERSION="7 (Core)"
ID="centos"
ID_LIKE="rhel fedora"
VERSION_ID="7"
PRETTY_NAME="CentOS Linux 7 (Core)"
ANSI_COLOR="0;31"
CPE_NAME="cpe:/o:centos:centos:7"
HOME_URL="https://www.centos.org/"
BUG_REPORT_URL="https://bugs.centos.org/"

CENTOS_MANTISBT_PROJECT="CentOS-7"
CENTOS_MANTISBT_PROJECT_VERSION="7"
REDHAT_SUPPORT_PRODUCT="centos"
REDHAT_SUPPORT_PRODUCT_VERSION="7"
```

## 根因定位

### free -h

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# free -h
              total        used        free      shared  buff/cache   available
Mem:            31G         23G        5.3G        1.0G        2.0G        5.8G
Swap:            0B          0B          0B
```

### top

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# top
top - 15:16:17 up 240 days, 16:59,  1 user,  load average: 0.00, 0.05, 0.05
Tasks: 422 total,   1 running, 421 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0.1 us,  0.1 sy,  0.0 ni, 99.8 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st
KiB Mem : 32761856 total,  5595952 free, 25024292 used,  2141612 buff/cache
KiB Swap:        0 total,        0 free,        0 used.  6071504 avail Mem

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU %MEM     TIME+ COMMAND
      1 root      20   0 1080916 889928   1736 S   0.0  2.7 215:42.26 systemd
  88707 root      20   0 3717704 387888   6328 S   0.0  1.2   0:59.11 uwsgi
  88698 root      20   0 3739964 169808   6316 S   0.0  0.5   0:27.72 uwsgi
 264328 root      20   0  755228 161684   3000 S   3.0  0.5   1999:43 titanagent
  88697 root      20   0 3713292 140496   6312 S   0.0  0.4   0:20.30 uwsgi
  88699 root      20   0 3712568 132564   6236 S   0.0  0.4   0:09.40 uwsgi
 427892 root      20   0 3714128  90012   6324 S   0.0  0.3   0:29.36 uwsgi
 432638 root      20   0 3712864  75784   6316 S   0.0  0.2   0:32.84 uwsgi
 430059 root      20   0 3712328  75384   6312 S   0.0  0.2   0:33.01 uwsgi
 442099 root      20   0 3713412  75236   6316 S   0.0  0.2   0:33.49 uwsgi
 431569 root      20   0 3712448  75024   6324 S   0.0  0.2   0:31.07 uwsgi
 435775 root      20   0 3712332  74780   6324 S   0.0  0.2   0:32.13 uwsgi
 450706 root      20   0 3712448  74568   6316 S   0.0  0.2   0:26.77 uwsgi
 426368 root      20   0 3712340  74556   6324 S   0.0  0.2   0:25.44 uwsgi
 429462 root      20   0 3712912  74496   6324 S   0.0  0.2   0:31.80 uwsgi
 427636 root      20   0 3712352  74488   6324 S   0.0  0.2   0:27.87 uwsgi
 428704 root      20   0 3713132  74264   6304 S   0.0  0.2   0:30.53 uwsgi
 424777 root      20   0 3712644  73920   6324 S   0.0  0.2   0:22.61 uwsgi
 426223 root      20   0 3711940  73060   6324 S   0.0  0.2   0:24.35 uwsgi
 423904 root      20   0 3712632  72284   6324 S   0.0  0.2   0:22.62 uwsgi
  11945 root      20   0  913036  50804   8312 S   0.0  0.2   1848:42 telegraf
  40299 root      20   0 2452284  38236   7076 S   0.0  0.1  20:36.14 dockerd
  84662 root      20   0   55092  36380   9660 S   0.0  0.1   0:46.21 uwsgi
  87040 998       20   0   39528  29124    752 S   0.0  0.1   1:54.56 nginx
  87042 998       20   0   39404  28788    668 S   0.0  0.1   0:02.01 nginx
  87048 998       20   0   39404  28788    668 S   0.0  0.1   0:10.02 nginx
  87053 998       20   0   39404  28788    668 S   0.0  0.1   0:01.92 nginx
  87060 998       20   0   39404  28788    668 S   0.0  0.1   0:16.18 nginx
  87062 998       20   0   39404  28788    668 S   0.0  0.1   0:17.94 nginx
  87067 998       20   0   39404  28304    308 S   0.0  0.1   0:01.96 nginx
  87072 998       20   0   39404  28304    308 S   0.0  0.1   0:11.70 nginx
3689400 nginx     20   0   75060  27852    240 S   0.0  0.1   7:33.00 nginx
3689401 nginx     20   0   75060  27844    232 S   0.0  0.1   7:27.37 nginx
  30084 root      20   0 2792044  26672   4176 S   0.0  0.1  36:55.66 containerd
   5677 root      20   0   55848  18380  17992 S   0.0  0.1  30:51.40 systemd-journal
3402461 root      20   0  787500  14000  11848 S   0.0  0.0   3:02.01 rsyslogd
  83054 root      20   0   38688  13932  12956 S   0.0  0.0   0:05.77 systemd-journal
  11944 root      20   0  717708  11980   3964 S   0.0  0.0  63:21.16 eAgent
  11950 root      20   0  573816  11776    620 S   0.0  0.0  64:48.91 tuned
  90821 root      20   0  719620  11488   3828 S   0.0  0.0  57:09.81 vnet-proxy
  11942 root      20   0  728600   9628   2804 S   0.0  0.0  29:47.50 monitor_exporte
  10934 polkitd   20   0  612472   8780   1004 S   0.0  0.0   8:19.54 polkitd
  82993 root      20   0   23052   7224   4988 S   0.0  0.0   0:06.80 systemd
  67000 haproxy   20   0  736924   7216    128 S   0.3  0.0 619:39.23 haproxy
  82972 root      20   0  712376   7124   4468 S   0.0  0.0   0:35.95 containerd-shim
  83662 root      20   0   30060   4956   3352 S   0.0  0.0   0:00.68 systemd-udevd
 496355 root      20   0  115772   4672   3616 S   0.0  0.0   0:00.00 sshd
  83695 systemd+  20   0   18908   4400   3472 S   0.0  0.0   0:00.23 systemd-network
  11094 root      20   0  271792   4320   1200 S   0.0  0.0 377:14.87 vmtoolsd
 496387 root      20   0  218212   4196   3140 S   0.0  0.0   0:00.00 sudo
```

### smem -wk

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# smem -wk
Area                           Used      Cache   Noncache
firmware/hardware                 0          0          0
kernel image                      0          0          0
kernel dynamic memory         22.6G       1.7G      21.0G
userspace memory               3.1G      99.8M       3.0G
free memory                    5.5G       5.5G          0
```

`smem -wk` 是一个用于查看系统内存使用情况的命令，输出结果概要如下：

1. **各区域内存分布**：
   - `kernel dynamic memory`：内核动态内存占用 **22.6G**（缓存 1.7G + 非缓存 21.0G）
   - `userspace memory`：用户空间程序占用 **3.1G**（缓存 99.8M + 非缓存 3.0G）
   - `free memory`：剩余可用内存 **5.5G**（全部为缓存）

2. **关键指标**：
   - **总内存** ≈ 22.6G + 3.1G + 5.5G = **31.2G**（假设为32G物理内存）
   - **内核占用较高**（21G非缓存），可能处理大量内核态任务。
   - **用户空间内存**较少，系统可能主要运行内核服务。

3. **选项作用**：
   - `-w`：显示更宽的输出格式（含完整分类）。
   - `-k`：显示带单位的缩写后缀（例如 KB、MB 等）。

### smem -s rss -rtk

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# smem -s rss -rtk
  PID User     Command                         Swap      USS      PSS      RSS
    1 root     /usr/lib/systemd/systemd --        0   868.5M   868.6M   869.1M
88707 root     /bin/uwsgi --ini /opt/sdk_p        0   365.8M   366.6M   381.9M
88698 root     /bin/uwsgi --ini /opt/sdk_p        0   155.2M   156.0M   171.3M
264328 root     ./titanagent -d -b /etc/tit        0   158.5M   158.6M   159.3M
88697 root     /bin/uwsgi --ini /opt/sdk_p        0   127.3M   128.1M   143.3M
88699 root     /bin/uwsgi --ini /opt/sdk_p        0   119.4M   120.3M   135.6M
427892 root     /bin/uwsgi --ini /opt/sdk_p        0    78.0M    78.8M    94.1M
442099 root     /bin/uwsgi --ini /opt/sdk_p        0    63.6M    64.4M    79.7M
432638 root     /bin/uwsgi --ini /opt/sdk_p        0    63.5M    64.3M    79.5M
430059 root     /bin/uwsgi --ini /opt/sdk_p        0    63.3M    64.1M    79.3M
431569 root     /bin/uwsgi --ini /opt/sdk_p        0    63.1M    63.9M    79.2M
428704 root     /bin/uwsgi --ini /opt/sdk_p        0    63.0M    63.8M    79.0M
429462 root     /bin/uwsgi --ini /opt/sdk_p        0    62.8M    63.6M    78.8M
427636 root     /bin/uwsgi --ini /opt/sdk_p        0    62.7M    63.5M    78.7M
426368 root     /bin/uwsgi --ini /opt/sdk_p        0    62.6M    63.4M    78.7M
435775 root     /bin/uwsgi --ini /opt/sdk_p        0    62.2M    63.0M    78.3M
450706 root     /bin/uwsgi --ini /opt/sdk_p        0    62.0M    62.8M    78.1M
424777 root     /bin/uwsgi --ini /opt/sdk_p        0    61.7M    62.5M    77.8M
426223 root     /bin/uwsgi --ini /opt/sdk_p        0    61.3M    62.1M    77.3M
423904 root     /bin/uwsgi --ini /opt/sdk_p        0    61.1M    61.9M    77.2M
11945 root     /opt/app/telegraf/telegraf         0    52.9M    52.9M    52.9M
40299 root     /usr/bin/dockerd -H fd:// -        0    39.7M    39.7M    39.7M
84662 root     /bin/uwsgi --ini /opt/sdk_p        0    19.8M    21.4M    35.5M
87040 998      nginx: worker proces               0    26.9M    27.1M    28.4M
87060 998      nginx: worker proces               0    26.7M    26.9M    28.2M
87062 998      nginx: worker proces               0    26.7M    26.9M    28.2M
87053 998      nginx: worker proces               0    26.7M    26.8M    28.2M
87048 998      nginx: worker proces               0    26.7M    26.8M    28.2M
87042 998      nginx: worker proces               0    26.7M    26.8M    28.2M
87067 998      nginx: worker proces               0    26.6M    26.7M    27.9M
87072 998      nginx: worker proces               0    26.6M    26.7M    27.9M
30084 root     /usr/bin/containerd                0    27.4M    27.4M    27.4M
3689401 nginx    nginx: worker proces               0    26.2M    26.5M    27.2M
3689400 nginx    nginx: worker proces               0    26.2M    26.5M    27.2M
 5677 root     /usr/lib/systemd/systemd-jo        0     5.1M     9.9M    17.9M
3402461 root     /usr/sbin/rsyslogd -n              0     4.5M     8.0M    13.7M
83054 root     /usr/lib/systemd/systemd-jo        0     9.1M    10.3M    13.7M
11944 root     /data/web/eAgent/eAgent -p         0    13.1M    13.1M    13.1M
11950 root     /usr/bin/python2 -Es /usr/s        0    11.1M    11.4M    12.0M
90821 root     /usr/bin/vnet-proxy -conf /        0    11.8M    11.8M    11.8M
11942 root     /opt/app/monitor_exporter/m        0    10.5M    10.5M    10.5M
507468 root     python /bin/smem -s rss -rt        0     8.3M     8.6M     9.7M
10934 polkitd  /usr/lib/polkit-1/polkitd -        0     8.5M     8.6M     9.0M
67000 haproxy  /usr/sbin/haproxy -Ws -f /e        0     6.5M     6.9M     7.6M
82972 root     /usr/bin/containerd-shim-ru        0     7.5M     7.5M     7.5M
82993 root     /usr/sbin/init                     0     3.6M     4.2M     7.1M
83662 root     /usr/lib/systemd/systemd-ud        0     2.0M     2.4M     5.1M
496355 root     sshd: secure [priv]                0     1.1M     2.4M     4.6M
83695 systemd-network /usr/lib/systemd/systemd-ne        0     1.2M     1.7M     4.5M
86007 root     /usr/lib/systemd/systemd-lo        0     1.0M     1.5M     4.2M
11094 root     /usr/sbin/vmtoolsd                 0     3.8M     3.8M     4.2M
496387 root     sudo -s                            0     2.1M     2.7M     4.1M
84796 root     sshd: /usr/sbin/sshd -D [li        0     1.9M     2.2M     3.8M
82957 root     /usr/bin/docker-proxy -prot        0     1.7M     2.3M     3.2M
82950 root     /usr/bin/docker-proxy -prot        0     1.6M     2.2M     3.1M
496357 secure   sshd: secure@pts/1                 0   604.0K     1.3M     2.4M
84388 991      /usr/bin/dbus-daemon --syst        0     1.1M     1.2M     2.2M
496388 root     /bin/bash                          0   504.0K   987.0K     2.1M
496361 secure   -bash                              0   448.0K   935.0K     2.1M
 5698 root     /usr/lib/systemd/systemd-ud        0     1.4M     1.4M     1.7M
12117 root     /usr/sbin/keepalived -D -d         0   260.0K   584.0K     1.6M
11132 root     /usr/lib/vmware-vgauth/VGAu        0     1.4M     1.4M     1.4M
86326 root     /usr/sbin/crond -n                 0   416.0K   470.0K     1.3M
12116 root     /usr/sbin/keepalived -D -d         0   260.0K   559.0K     1.3M
10944 dbus     /usr/bin/dbus-daemon --syst        0     1.0M     1.0M     1.3M
10943 root     /usr/lib/systemd/systemd-lo        0   880.0K   907.0K     1.2M
10946 ntp      /usr/sbin/ntpd -u ntp:ntp -        0   804.0K   847.0K     1.2M
12115 root     /usr/sbin/keepalived -D -d         0   100.0K   413.0K     1.2M
264334 root     titan_monitor -p 264328 -l         0   744.0K   773.0K     1.1M
87030 root     nginx: master process /usr/        0   152.0K   236.0K     1.1M
11991 root     /usr/sbin/haproxy -Ws -f /e        0   212.0K   628.0K     1.0M
12112 root     sshd: /usr/sbin/sshd -f /et        0   664.0K   733.0K     1.0M
10909 root     /sbin/auditd                       0   712.0K   729.0K  1008.0K
11007 root     /usr/sbin/crond -n                 0   688.0K   704.0K  1004.0K
3689399 root     nginx: master process /usr/        0   160.0K   418.0K   988.0K
86484 root     /sbin/agetty -o -p -- \u --        0   176.0K   209.0K   836.0K
10942 root     /usr/sbin/irqbalance --fore        0   428.0K   440.0K   628.0K
90820 root     /bin/sh -c env GOTRACEBACK=        0   224.0K   224.0K   228.0K
-------------------------------------------------------------------------------
   78 10                                          0     3.1G     3.2G     3.5G
```

这个 `smem` 命令用于统计系统中各个进程的内存占用情况，并按内存使用量降序排列。以下是关键点解析：

---

#### **1. 命令参数**
```bash
smem -s rss -rtk
```
- `-s rss`：按 **RSS（常驻内存）** 降序排序
- `-r`：反向排序（此处与 `-t` 配合实际为降序）
- `-t`：显示汇总行（最后一行）
- `-k`：显示带单位的缩写后缀（例如 KB、MB 等）

---

#### **2. 关键字段**
| 字段       | 说明                                                                 |
|------------|----------------------------------------------------------------------|
| **PID**    | 进程 ID                                                             |
| **User**   | 进程所属用户                                                        |
| **Command**| 进程命令（可能截断）                                                |
| **Swap**   | 进程使用的 Swap 内存（全为 0，说明未启用 Swap）                     |
| **USS**    | 进程独占物理内存（不共享部分）                                      |
| **PSS**    | 实际物理内存占用（USS + 共享内存按比例分配）                        |
| **RSS**    | 常驻内存（含共享内存，可能重复计算）                                |

---

#### **3. 重点进程分析**
1. **高内存进程**：
   - `systemd`（PID 1）：**869MB RSS**（系统主进程）
   - 多个 `uwsgi` 进程：每个约 **70-380MB RSS**（Python Web 服务）
   - `titanagent`：**159MB RSS**（可能是自定义服务）
   - `nginx` worker：每个约 **28MB RSS**（Web 服务）

2. **其他服务**：
   - `telegraf`（监控）：**53MB RSS**
   - `dockerd`：**40MB RSS**
   - `containerd`：**27MB RSS**

3. **汇总**（最后一行）：
   - **78 个进程**，总计占用 **3.5GB RSS**（含共享内存重复计算）

---

#### **4. 内存类型对比**
- **RSS**（3.5G） > **PSS**（3.2G） > **USS**（3.1G）  
  说明系统中存在较多共享内存（如 `uwsgi`/`nginx` 可能共享库）。

---

#### **5. 优化建议**
1. **检查 `uwsgi` 进程**：
   - 多个同类进程占用较高（总计约 2GB+），确认是否需优化配置或减少实例数。
2. **监控 `systemd`**：
   - 主进程占用 869MB，检查是否有异常服务依赖。
3. **共享内存**：
   - 若需精确统计内存压力，应以 **PSS** 为准（3.2G 更真实）。

---

#### **6. 扩展命令**
- 按 **PSS** 排序（更真实）：
  ```bash
  smem -s pss -tk -p
  ```
- 按用户统计：
  ```bash
  smem -u -k
  ```

### smem -m -s pss -rtk

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# smem -m -s pss -rtk
Map                                       PIDs   AVGPSS      PSS
<anonymous>                                 78    26.3M     2.0G
[heap]                                      73    14.4M     1.0G
/usr/bin/dockerd                             1    20.6M    20.6M
/usr/bin/containerd                          1    14.5M    14.5M
/run/log/journal/8db494489b22414a8f3baba     2     6.3M    12.5M
/dev/zero                                   31   368.0K    11.2M
/run/log/journal/d92103d7ffe243ff8b086ee     1     8.7M     8.7M
/opt/app/telegraf/telegraf                   1     8.5M     8.5M
/usr/lib64/python2.7/site-packages/crypt    18   353.0K     6.2M
/usr/lib64/libpython2.7.so.1.0              21   263.0K     5.4M
/usr/lib64/python2.7/site-packages/lxml/    18   292.0K     5.1M
/usr/bin/containerd-shim-runc-v2             1     4.6M     4.6M
/data/web/eAgent/eAgent                      1     4.4M     4.4M
/usr/bin/vnet-proxy                          1     4.0M     4.0M
/opt/app/monitor_exporter/monitor_export     1     3.1M     3.1M
/usr/lib64/libcrypto.so.1.1.1f              34    78.0K     2.6M
/usr/lib64/libcrypto.so.1.0.2k              15   171.0K     2.5M
/usr/lib/systemd/systemd                     2     1.2M     2.4M
[stack]                                     78    30.0K     2.3M
/usr/bin/docker-proxy                        2     1.0M     2.0M
/usr/lib/systemd/libsystemd-shared-243.s     5   403.0K     2.0M
/usr/lib64/libc-2.17.so                     36    50.0K     1.8M
/usr/lib64/libc-2.28.so                     37    34.0K     1.2M
/usr/sbin/sshd                               4   228.0K   913.0K
/usr/lib64/libkrb5.so.3.3                   15    59.0K   898.0K
/usr/sbin/nginx                             12    66.0K   797.0K
/usr/bin/bash                                3   264.0K   792.0K
/usr/bin/uwsgi                              19    41.0K   790.0K
/titan/agent/plugin/libAuditMonitor.so       1   748.0K   748.0K
/titan/agent/plugin/libRadar.so              1   732.0K   732.0K
/usr/lib64/python2.7/site-packages/_cffi    18    35.0K   630.0K
/usr/lib/systemd/systemd-logind              2   302.0K   604.0K
/usr/sbin/rsyslogd                           1   556.0K   556.0K
/titan/agent/plugin/libLogWatcher.so         1   532.0K   532.0K
/usr/sbin/haproxy                            2   258.0K   516.0K
/usr/lib/systemd/systemd-udevd               2   252.0K   504.0K
/titan/agent/plugin/libDiting.so             1   504.0K   504.0K
/usr/lib64/libxml2.so.2.9.10                19    24.0K   458.0K
/usr/lib64/python2.7/site-packages/nacl/    18    25.0K   450.0K
/usr/lib64/libssl.so.1.1.1f                 33    13.0K   433.0K
/usr/lib64/python2.7/lib-dynload/_io.so     20    21.0K   432.0K
/usr/lib64/python2.7/site-packages/lxml/    18    23.0K   414.0K
/usr/lib/systemd/systemd-journald            2   200.0K   400.0K
/usr/libexec/sudo/sudoers.so                 1   396.0K   396.0K
/usr/lib/systemd/systemd-networkd            1   384.0K   384.0K
/usr/lib64/ld-2.17.so                       36    10.0K   382.0K
/usr/lib/vmware-tools/lib64/libcrypto.so     2   180.0K   360.0K
/usr/bin/dbus-daemon                         2   174.0K   348.0K
/usr/lib64/libgio-2.0.so.0.5600.1            2   170.0K   340.0K
/usr/lib64/libssl.so.1.0.2k                 10    33.0K   332.0K
/usr/lib64/libmount.so.1.1.0                 8    39.0K   317.0K
/usr/lib/vmware-tools/lib64/libvmtools.s     1   300.0K   300.0K
/usr/lib64/libpthread-2.17.so               32     9.0K   298.0K
/usr/lib64/libglib-2.0.so.0.5600.1           5    59.0K   296.0K
/usr/lib64/libselinux.so.1                  31     9.0K   292.0K
/usr/lib64/ld-2.28.so                       37     7.0K   278.0K
/usr/lib64/libgcrypt.so.20.2.6               7    39.0K   273.0K
/usr/lib64/python2.7/site-packages/lxml/    18    15.0K   270.0K
/usr/lib64/libdbus-1.so.3.19.11              1   264.0K   264.0K
/titan/agent/plugin/libFileMonitor.so        1   252.0K   252.0K
/usr/sbin/ntpd                               1   244.0K   244.0K
/usr/lib64/python2.7/lib-dynload/itertoo    21    11.0K   244.0K
/usr/lib/vmware-tools/lib64/libglib-2.0.     2   118.0K   236.0K
/usr/lib64/libpthread-2.28.so               36     6.0K   232.0K
/usr/lib64/libnss_files-2.17.so             27     8.0K   231.0K
/usr/lib64/security/pam_systemd.so           2   112.0K   224.0K
/usr/lib64/python2.7/lib-dynload/termios    18    12.0K   216.0K
/usr/lib64/libblkid.so.1.1.0                 9    24.0K   216.0K
/usr/lib64/libdbus-1.so.3.14.14              2   106.0K   212.0K
/usr/lib64/libgssapi_krb5.so.2.2            15    14.0K   210.0K
/usr/lib64/libdl-2.17.so                    32     6.0K   210.0K
/usr/lib64/libm-2.17.so                     19    10.0K   202.0K
/usr/lib64/librt-2.28.so                    25     8.0K   200.0K
/usr/lib64/liblzma.so.5.2.2                 15    13.0K   196.0K
/usr/lib64/libsystemd.so.0.6.0               7    27.0K   191.0K
/usr/lib64/libtinfo.so.5.9                   3    61.0K   184.0K
/usr/lib64/libm-2.28.so                     33     5.0K   181.0K
/usr/lib64/libaudit.so.1.0.0                15    11.0K   177.0K
/usr/lib64/libsystemd.so.0.27.0              2    86.0K   172.0K
/usr/lib64/libgobject-2.0.so.0.5600.1        2    86.0K   172.0K
/usr/lib64/libunistring.so.2.1.0             5    34.0K   170.0K
/usr/lib64/libpcre.so.1.2.0                 26     6.0K   162.0K
/usr/sbin/keepalived                         3    52.0K   157.0K
/usr/lib64/librt-2.17.so                    22     7.0K   156.0K
/usr/lib64/libgcc_s-4.8.5-20150702.so.1     16     9.0K   156.0K
/usr/lib64/libnss3.so                        4    38.0K   152.0K
/usr/lib64/python2.7/lib-dynload/operato    21     7.0K   148.0K
/usr/lib64/python2.7/lib-dynload/_ctypes    20     7.0K   148.0K
/usr/lib64/libgcrypt.so.11.8.2               8    18.0K   148.0K
/usr/lib64/libk5crypto.so.3.1               15     9.0K   146.0K
/usr/lib64/libdevmapper.so.1.02              6    23.0K   141.0K
/usr/lib64/python2.7/lib-dynload/datetim    20     6.0K   139.0K
/usr/lib64/python2.7/lib-dynload/_socket    20     6.0K   139.0K
/usr/lib64/libexpat.so.1.6.11                1   136.0K   136.0K
/usr/lib64/libdw-0.172.so                    9    15.0K   136.0K
/usr/lib64/libcap-ng.so.0.0.0               16     8.0K   134.0K
/usr/lib64/python2.7/lib-dynload/_collec    21     6.0K   132.0K
/titan/agent/titan_monitor                   1   132.0K   132.0K
/usr/lib64/libnss_myhostname.so.2           19     6.0K   128.0K
/usr/lib64/libz.so.1.2.7                    21     5.0K   125.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1   124.0K   124.0K
/usr/bin/sudo                                1   124.0K   124.0K
/usr/lib64/libcryptsetup.so.12.6.0           5    23.0K   115.0K
/usr/lib64/libnssutil3.so                    4    28.0K   114.0K
/usr/lib64/libseccomp.so.2.5.0               5    22.0K   113.0K
/var/lib/containerd/io.containerd.metada     1   112.0K   112.0K
/usr/lib64/libattr.so.1.1.0                 17     6.0K   110.0K
/usr/sbin/crond                              2    54.0K   108.0K
/usr/lib64/libresolv-2.17.so                18     5.0K   107.0K
/usr/lib64/python2.7/lib-dynload/_json.s    19     5.0K   106.0K
/usr/lib64/python2.7/lib-dynload/_functo    21     4.0K   104.0K
/usr/lib64/libkrb5support.so.0.1            15     6.0K   104.0K
/usr/lib/vmware-tools/sbin64/vmtoolsd        1   100.0K   100.0K
/usr/lib/vmware-tools/lib64/libDeployPkg     1   100.0K   100.0K
/usr/lib/vmware-tools/bin64/appLoader        1   100.0K   100.0K
/usr/lib/vmware-tools/lib64/libpcre.so.1     2    48.0K    96.0K
/usr/lib64/libcap.so.2.22                   15     6.0K    94.0K
/usr/libexec/sudo/libsudo_util.so.0.0.0      1    92.0K    92.0K
/usr/lib64/libmozjs-17.0.so                  1    92.0K    92.0K
/usr/lib/vmware-tools/lib64/libgobject-2     1    92.0K    92.0K
/run/log/journal/8db494489b22414a8f3baba     1    92.0K    92.0K
/usr/lib64/python2.7/site-packages/.libs    18     5.0K    90.0K
/usr/lib64/libuuid.so.1.3.0                 29     3.0K    88.0K
/usr/lib64/libcom_err.so.2.1                15     5.0K    88.0K
/usr/lib/vmware-tools/lib64/libssl.so.1.     2    44.0K    88.0K
/run/log/journal/8db494489b22414a8f3baba     1    88.0K    88.0K
/run/log/journal/8db494489b22414a8f3baba     1    88.0K    88.0K
/run/log/journal/8db494489b22414a8f3baba     1    88.0K    88.0K
/usr/lib64/liblz4.so.1.9.2                   7    12.0K    87.0K
/usr/lib64/libpam.so.0.83.1                  6    14.0K    85.0K
/usr/lib64/libnspr4.so                       5    17.0K    85.0K
/usr/lib64/libgpg-error.so.0.29.0            7    12.0K    84.0K
/run/log/journal/8db494489b22414a8f3baba     1    84.0K    84.0K
/run/log/journal/8db494489b22414a8f3baba     1    84.0K    84.0K
/run/log/journal/8db494489b22414a8f3baba     1    84.0K    84.0K
/run/log/journal/8db494489b22414a8f3baba     1    84.0K    84.0K
/usr/lib64/libkmod.so.2.3.5                  5    16.0K    83.0K
/usr/lib64/python2.7/lib-dynload/_ssl.so    20     4.0K    80.0K
/usr/lib64/libstdc++.so.6.0.19               2    40.0K    80.0K
/usr/lib64/libpcap.so.1.9.1                  5    16.0K    80.0K
/run/log/journal/8db494489b22414a8f3baba     1    80.0K    80.0K
/run/log/journal/8db494489b22414a8f3baba     1    80.0K    80.0K
/usr/lib64/libsepol.so.1                     6    13.0K    78.0K
/usr/lib64/libelf-0.172.so                  12     6.0K    78.0K
/usr/lib64/python2.7/lib-dynload/_struct    21     3.0K    76.0K
/usr/lib64/libldap-2.4.so.2.10.7             1    76.0K    76.0K
/run/log/journal/8db494489b22414a8f3baba     1    76.0K    76.0K
/usr/lib64/python2.7/site-packages/crypt    18     4.0K    72.0K
/usr/lib64/python2.7/site-packages/bcryp    18     4.0K    72.0K
/usr/lib64/libssl3.so                        1    72.0K    72.0K
/usr/lib64/libpcre2-8.so.0.10.0             17     4.0K    72.0K
/run/log/journal/8db494489b22414a8f3baba     1    72.0K    72.0K
/run/log/journal/8db494489b22414a8f3baba     1    72.0K    72.0K
/usr/lib64/libbz2.so.1.0.6                  12     5.0K    70.0K
/usr/lib64/liblz4.so.1.7.5                   8     8.0K    68.0K
/usr/lib64/libkeyutils.so.1.5               14     4.0K    68.0K
/run/log/journal/8db494489b22414a8f3baba     1    68.0K    68.0K
/usr/lib64/libutil-2.17.so                  10     6.0K    66.0K
/usr/sbin/auditd                             1    64.0K    64.0K
/usr/lib64/libgpg-error.so.0.10.0            8     8.0K    64.0K
/usr/lib64/libgcc_s-7.3.0-20190804.so.1     35     1.0K    64.0K
/usr/lib64/libdl-2.28.so                    36     1.0K    64.0K
/usr/lib/polkit-1/polkitd                    1    64.0K    64.0K
/run/log/journal/8db494489b22414a8f3baba     1    64.0K    64.0K
/usr/lib/locale/locale-archive               8     7.0K    63.0K
/usr/lib64/libnetsnmp.so.31.0.2              3    20.0K    61.0K
/usr/lib64/python2.7/lib-dynload/stropmo    21     2.0K    60.0K
/run/log/journal/8db494489b22414a8f3baba     1    60.0K    60.0K
/run/log/journal/8db494489b22414a8f3baba     1    60.0K    60.0K
/usr/sbin/agetty                             1    56.0K    56.0K
/usr/lib64/python2.7/lib-dynload/cPickle    20     2.0K    56.0K
/usr/lib64/libudev.so.1.6.2                  2    28.0K    56.0K
/usr/lib64/libsmime3.so                      1    56.0K    56.0K
/usr/lib64/liblzma.so.5.2.5                 26     2.0K    56.0K
/run/log/journal/8db494489b22414a8f3baba     1    56.0K    56.0K
/run/log/journal/8db494489b22414a8f3baba     1    56.0K    56.0K
/run/log/journal/8db494489b22414a8f3baba     1    56.0K    56.0K
/usr/lib64/libudev.so.1.6.15                 5    11.0K    55.0K
/usr/lib64/libjson-c.so.5.1.0                5    11.0K    55.0K
/usr/lib64/libfreebl3.so                    13     4.0K    55.0K
/usr/lib64/libcrypt-2.17.so                 13     4.0K    55.0K
/usr/lib64/libstdc++.so.6.0.24               9     6.0K    54.0K
/usr/lib64/python2.7/site-packages/_dbus     1    52.0K    52.0K
/usr/lib64/python2.7/lib-dynload/_locale    21     2.0K    52.0K
/usr/lib64/python2.7/lib-dynload/_heapq.    21     2.0K    52.0K
/usr/lib64/libresolv-2.28.so                20     2.0K    52.0K
/titan/agent/plugin/libLogCollector.so       1    52.0K    52.0K
/run/log/journal/8db494489b22414a8f3baba     1    52.0K    52.0K
/usr/lib64/security/pam_unix.so              3    16.0K    50.0K
/usr/lib64/libnss_files-2.28.so             34     1.0K    50.0K
/usr/lib64/libnsl-2.17.so                    7     7.0K    50.0K
/usr/lib64/libcap.so.2.32                    5     9.0K    49.0K
/usr/lib64/python2.7/lib-dynload/timemod    21     2.0K    48.0K
/usr/lib64/libz.so.1.2.11                   34     1.0K    48.0K
/usr/lib64/libshellaudit.so                  3    16.0K    48.0K
/usr/lib64/libip4tc.so.2.0.0                 5     9.0K    45.0K
/usr/lib64/libidn2.so.0.3.7                  5     9.0K    45.0K
/usr/lib64/libattr.so.1.1.2448               5     9.0K    45.0K
/usr/lib64/libacl.so.1.1.2253                5     9.0K    45.0K
/usr/lib64/libpolkit-gobject-1.so.0.0.0      1    44.0K    44.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1    44.0K    44.0K
/usr/sbin/irqbalance                         1    40.0K    40.0K
/usr/lib64/python2.7/lib-dynload/math.so    20     2.0K    40.0K
/usr/lib64/libargon2.so.1                    5     8.0K    40.0K
/usr/lib/vmware-tools/lib64/libxml2.so.2     1    40.0K    40.0K
/usr/lib64/libnetsnmpmibs.so.31.0.2          3    13.0K    39.0K
/usr/lib64/python2.7/site-packages/gi/_g     1    36.0K    36.0K
/usr/lib64/python2.7/lib-dynload/grpmodu    21     1.0K    36.0K
/usr/lib64/python2.7/lib-dynload/arraymo    19     1.0K    36.0K
/usr/lib64/perl5/CORE/libperl.so             3    12.0K    36.0K
/usr/lib64/libpam.so.0.85.1                  3    12.0K    36.0K
/usr/lib64/libexpat.so.1.6.0                 3    12.0K    36.0K
/usr/lib64/libdb-5.3.so                      3    12.0K    36.0K
/usr/lib/vmware-tools/lib64/libvmtoolsd.     1    36.0K    36.0K
/usr/lib64/security/pam_namespace.so         2    16.0K    32.0K
/usr/lib64/security/pam_limits.so            3    10.0K    32.0K
/usr/lib64/rsyslog/imuxsock.so               1    32.0K    32.0K
/usr/lib64/python2.7/lib-dynload/selectm    20     1.0K    32.0K
/usr/lib64/python2.7/lib-dynload/_hashli    20     1.0K    32.0K
/usr/lib64/libkmod.so.2.2.10                 2    16.0K    32.0K
/usr/lib64/libffi.so.6.0.1                   2    16.0K    32.0K
/usr/lib64/libcrack.so.2.9.0                 3    10.0K    32.0K
/usr/lib/vmware-tools/lib64/libdnet.so.1     1    32.0K    32.0K
/usr/lib64/libplc4.so                        5     6.0K    30.0K
/usr/lib64/libacl.so.1.1.0                   6     5.0K    30.0K
/usr/lib64/rsyslog/imudp.so                  1    28.0K    28.0K
/usr/lib64/rsyslog/imjournal.so              1    28.0K    28.0K
/usr/lib64/libsasl2.so.3.0.0                 1    28.0K    28.0K
/usr/lib64/libfastjson.so.4.0.0              1    28.0K    28.0K
/usr/lib64/gconv/gconv-modules.cache         5     5.0K    28.0K
/usr/lib64/libplds4.so                       5     5.0K    26.0K
/usr/lib64/security/pam_env.so               3     8.0K    25.0K
/usr/lib64/security/pam_succeed_if.so        3     8.0K    24.0K
/usr/lib64/security/pam_selinux.so           2    12.0K    24.0K
/usr/lib64/security/pam_keyinit.so           3     8.0K    24.0K
/usr/lib64/rsyslog/lmnet.so                  1    24.0K    24.0K
/usr/lib64/python2.7/lib-dynload/fcntlmo    20     1.0K    24.0K
/usr/lib64/python2.7/lib-dynload/bz2.so     19     1.0K    24.0K
/usr/lib64/python2.7/lib-dynload/_random    20     1.0K    24.0K
/usr/lib64/libpwquality.so.1.0.2             3     8.0K    24.0K
/usr/lib64/liblber-2.4.so.2.10.7             1    24.0K    24.0K
/usr/lib/vmware-tools/plugins64/common/l     1    24.0K    24.0K
/usr/lib64/security/pam_tally2.so            1    20.0K    20.0K
/usr/lib64/security/pam_pwquality.so         3     6.0K    20.0K
/usr/lib64/security/pam_permit.so            3     6.0K    20.0K
/usr/lib64/security/pam_localuser.so         3     6.0K    20.0K
/usr/lib64/security/pam_lastlog.so           2    10.0K    20.0K
/usr/lib64/security/pam_deny.so              3     6.0K    20.0K
/usr/lib64/python2.7/lib-dynload/zlibmod    19     1.0K    20.0K
/usr/lib64/python2.7/lib-dynload/cString    20     1.0K    20.0K
/usr/lib64/python2.7/lib-dynload/_multip    19     1.0K    20.0K
/usr/lib64/libkeyutils.so.1.10               1    20.0K    20.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1    20.0K    20.0K
/usr/bin/python2.7                           2    10.0K    20.0K
/usr/lib64/security/pam_faildelay.so         3     6.0K    19.0K
/usr/lib64/librpm.so.3.2.2                   3     6.0K    18.0K
/usr/lib64/libnl-3.so.200.23.0               3     6.0K    18.0K
/usr/lib64/security/pam_sepermit.so          2     8.0K    16.0K
/usr/lib64/security/pam_loginuid.so          2     8.0K    16.0K
/usr/lib64/python2.7/lib-dynload/binasci    20        0    16.0K
/usr/lib64/libpam_misc.so.0.82.0             2     8.0K    16.0K
/usr/lib64/libgmodule-2.0.so.0.5600.1        2     8.0K    16.0K
/usr/lib64/libfipscheck.so.1.2.1             1    16.0K    16.0K
/usr/lib64/libestr.so.0.0.0                  1    16.0K    16.0K
/usr/lib64/libcrypt.so.1.1.0                29        0    16.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1    16.0K    16.0K
/usr/lib/vmware-tools/plugins64/common/l     1    16.0K    16.0K
/usr/lib/vmware-tools/lib64/libz.so.1/li     2     8.0K    16.0K
/usr/lib/vmware-tools/lib64/libxmlsec1.s     1    16.0K    16.0K
/usr/lib/vmware-tools/lib64/libxmlsec1-o     1    16.0K    16.0K
/usr/lib/vmware-tools/lib64/libiconv.so.     2     8.0K    16.0K
/usr/lib64/libnetsnmpagent.so.31.0.2         3     5.0K    15.0K
/usr/lib64/libwrap.so.0.7.6                  4     3.0K    14.0K
/var/lib/docker/volumes/metadata.db          1    12.0K    12.0K
/usr/lib64/security/pam_nologin.so           2     6.0K    12.0K
/usr/lib64/security/pam_cracklib.so          1    12.0K    12.0K
/usr/lib64/python2.7/lib-dynload/unicode    19        0    12.0K
/usr/lib64/python2.7/lib-dynload/pyexpat     1    12.0K    12.0K
/usr/lib64/python2.7/lib-dynload/_bisect    19        0    12.0K
/usr/lib64/libutil-2.28.so                  20        0    12.0K
/usr/lib64/librpmio.so.3.2.2                 3     4.0K    12.0K
/usr/lib64/libopts.so.25.15.0                1    12.0K    12.0K
/usr/lib64/libnss_dns-2.28.so               19        0    12.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1    12.0K    12.0K
/usr/lib64/libnl-genl-3.so.200.23.0          3     3.0K    11.0K
/usr/lib64/liblua-5.1.so                     3     3.0K     9.0K
/usr/lib64/python2.7/site-packages/marku    19        0     8.0K
/usr/lib64/python2.7/site-packages/_dbus     1     8.0K     8.0K
/usr/lib64/libpcreposix.so.0.0.1             2     4.0K     8.0K
/usr/lib64/libnuma.so.1                      1     8.0K     8.0K
/usr/lib64/libgthread-2.0.so.0.5600.1        1     8.0K     8.0K
/usr/lib64/libgirepository-1.0.so.1.0.0      1     8.0K     8.0K
/usr/lib64/libffi.so.7.1.0                  19        0     8.0K
/usr/lib64/libdbus-glib-1.so.2.2.2           1     8.0K     8.0K
/usr/lib64/libbz2.so.1.0.8                  19        0     8.0K
/usr/lib64/libauparse.so.0.0.0               1     8.0K     8.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1     8.0K     8.0K
/usr/lib/vmware-tools/plugins64/vmsvc/li     1     8.0K     8.0K
/usr/lib/vmware-tools/lib64/libhgfs.so/l     1     8.0K     8.0K
/usr/lib/vmware-tools/lib64/libffi.so.6/     2     4.0K     8.0K
/usr/lib/vmware-tools/lib64/libVGAuthSer     1     8.0K     8.0K
/run/systemd/journal/kernel-seqnum           2     4.0K     8.0K
/usr/lib64/libxtables.so.10.0.0              3     2.0K     6.0K
/usr/lib64/libsensors.so.4.4.0               3     2.0K     6.0K
/usr/lib64/libpopt.so.0.0.0                  3     2.0K     6.0K
/usr/lib64/libip6tc.so.0.1.0                 3     2.0K     6.0K
/usr/lib64/libip4tc.so.0.1.0                 3     2.0K     6.0K
/var/titanagent/dns_shmem                    1     4.0K     4.0K
/usr/lib/vmware-tools/lib64/libmspack.so     1     4.0K     4.0K
/usr/lib/vmware-tools/lib64/libgthread-2     1     4.0K     4.0K
/usr/lib/vmware-tools/lib64/libgmodule-2     1     4.0K     4.0K
/titan/agent/titanagent                      1     4.0K     4.0K
/etc/udev/hwdb.bin                           2     2.0K     4.0K
[vsyscall]                                  78        0        0
[vdso]                                      78        0        0
/var/lib/docker/buildkit/snapshots.db        1        0        0
/var/lib/docker/buildkit/metadata_v2.db      1        0        0
/var/lib/docker/buildkit/containerdmeta.     1        0        0
/var/lib/docker/buildkit/cache.db            1        0        0
/usr/lib64/libunwind.so.8.0.1                9        0        0
/usr/lib64/libprofiler.so.0.5.0              9        0        0
/usr/lib64/girepository-1.0/GLib-2.0.typ     1        0        0
/usr/lib/vmware-tools/icu/icudt44l.dat       1        0        0
/usr/lib/modules/3.10.0-957.el7.x86_64/m     1        0        0
/usr/lib/modules/3.10.0-957.el7.x86_64/m     1        0        0
/usr/lib/modules/3.10.0-957.el7.x86_64/m     1        0        0
/usr/lib/modules/3.10.0-957.el7.x86_64/m     1        0        0
/[aio]                                       8        0        0
-----------------------------------------------------------------
328                                       2997   132.8M     3.2G
```

这个 `smem -m -s pss -rtk` 命令是用来分析 Linux 系统内存使用情况的工具，下面是对命令和输出的解释：

#### 命令解释：
- `smem`：内存统计工具，可以显示实际内存使用情况
- `-m`：按内存映射（memory mappings）分组显示
- `-s pss`：按 PSS（Proportional Set Size）排序
- `-r`：反向排序（从大到小）
- `-t`：显示总计
- `-k`：以 KB 为单位显示（默认），但实际输出中显示的是 MB/GB

#### 输出解释：
输出分为三列：
1. **Map**：内存映射的文件或区域名称
2. **PIDs**：使用该映射的进程数量
3. **AVGPSS**：平均每个进程的 PSS
4. **PSS**：总 PSS（ Proportional Set Size，按共享比例计算的实际内存使用量）

#### 关键信息：
1. **匿名内存**（anonymous）占用最多：
   - 78 个进程共享
   - 平均每个进程 26.3MB
   - 总共 2.0GB

2. **[heap]** 堆内存：
   - 73 个进程共享
   - 总共 1.0GB

3. **重要进程**：
   - dockerd：20.6MB
   - containerd：14.5MB
   - telegraf：8.5MB

4. **总计**（最后一行）：
   - 328 个不同的内存映射
   - 2997 个进程
   - 平均每个进程 132.8MB
   - 总内存使用 3.2GB

#### PSS 说明：
PSS（Proportional Set Size）是更准确的内存统计方式：
- 私有内存：全部计入
- 共享内存：按共享进程数平分计入
- 比 RSS（Resident Set Size）更能反映实际内存使用

#### 常见映射类型：
- `<anonymous>`：匿名映射（如程序堆、栈等）
- `[heap]`：进程堆内存
- `[stack]`：进程栈内存
- `[vdso]`：虚拟动态共享对象
- 文件路径：共享库或内存映射文件

### /proc/meminfo

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# cat /proc/meminfo
MemTotal:       32761856 kB
MemFree:         5752900 kB
MemAvailable:    6140044 kB
Buffers:              48 kB
Cached:          1756500 kB
SwapCached:            0 kB
Active:          3502352 kB
Inactive:        1411756 kB
Active(anon):    3172812 kB
Inactive(anon):  1085016 kB
Active(file):     329540 kB
Inactive(file):   326740 kB
Unevictable:           0 kB
Mlocked:               0 kB
SwapTotal:             0 kB
SwapFree:              0 kB
Dirty:                28 kB
Writeback:             0 kB
AnonPages:       3157792 kB
Mapped:           101528 kB
Shmem:           1100268 kB
Slab:             295756 kB
SReclaimable:      98300 kB
SUnreclaim:       197456 kB
KernelStack:       27600 kB
PageTables:        23808 kB
NFS_Unstable:          0 kB
Bounce:                0 kB
WritebackTmp:          0 kB
CommitLimit:    16380928 kB
Committed_AS:   12744344 kB
VmallocTotal:   34359738367 kB
VmallocUsed:      231068 kB
VmallocChunk:   34359310332 kB
HardwareCorrupted:     0 kB
AnonHugePages:      4096 kB
CmaTotal:              0 kB
CmaFree:               0 kB
HugePages_Total:       0
HugePages_Free:        0
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
DirectMap4k:     3018560 kB
DirectMap2M:    30535680 kB
DirectMap1G:     2097152 kB
```

以下是标记后的 `/proc/meminfo` 数据解析，**右侧标注每项的作用及是否明确占用物理内存**：  

---

#### **`/proc/meminfo` 详细解析**
| 指标                 | 值 (KB)    | **作用与物理内存分配情况**                                                                 |
|----------------------|------------|-------------------------------------------------------------------------------------------|
| **MemTotal**         | 32761856   | **总物理内存**（32GB），硬件实际容量。                                                   |
| **MemFree**          | 5752900    | **完全空闲内存**（未分配，可立即使用）。✅ **未占用**。                                   |
| **MemAvailable**     | 6140044    | **系统实际可用内存**（含可回收缓存）。✅ **未完全占用**。                                 |
| **Buffers**          | 48         | **块设备缓存**（如磁盘读写缓冲）。⚠️ **可回收**，不算占用。                              |
| **Cached**           | 1756500    | **文件缓存**（加速文件访问）。⚠️ **可回收**，不算占用。                                   |
| **SwapCached**       | 0          | **交换缓存**（未启用 Swap，无数据）。🚫 **无占用**。                                     |
| **Active**           | 3502352    | **活跃内存**（近期被访问）。含 `Active(anon)` 和 `Active(file)`。                        |
| **Inactive**         | 1411756    | **非活跃内存**（可能被回收）。含 `Inactive(anon)` 和 `Inactive(file)`。                 |
| **Active(anon)**     | 3172812    | **活跃匿名页**（如进程堆、栈）。✅ **明确占用物理内存**。                                |
| **Inactive(anon)**   | 1085016    | **非活跃匿名页**（可能被回收）。⚠️ **暂占用，但可释放**。                               |
| **Active(file)**     | 329540     | **活跃文件缓存**（如被频繁读写的文件）。⚠️ **可回收**。                                  |
| **Inactive(file)**   | 326740     | **非活跃文件缓存**（如未使用的文件缓存）。⚠️ **可回收**。                               |
| **Unevictable**      | 0          | **不可回收内存**（如锁定的用户空间内存）。🚫 **无占用**。                                 |
| **Mlocked**          | 0          | **用户空间锁定内存**（防止被交换）。🚫 **无占用**。                                       |
| **SwapTotal**        | 0          | **Swap 空间总量**（未启用）。🚫 **无占用**。                                              |
| **SwapFree**         | 0          | **空闲 Swap 空间**（未启用）。🚫 **无占用**。                                            |
| **Dirty**            | 28         | **待写入磁盘的脏页**。⚠️ **暂占用，写入后释放**。                                         |
| **Writeback**        | 0          | **正在回写磁盘的内存**。🚫 **无占用**。                                                   |
| **AnonPages**        | 3157792    | **已映射的匿名内存**（如进程堆、栈）。✅ **明确占用物理内存**。                           |
| **Mapped**           | 101528     | **文件映射到内存**（如动态库）。⚠️ **部分可回收**。                                       |
| **Shmem**            | 1100268    | **共享内存**（如 `tmpfs`、IPC）。✅ **明确占用物理内存**。                                |
| **Slab**             | 295756     | **内核对象缓存**（如 dentry、inode）。含 `SReclaimable`（可回收）和 `SUnreclaim`（不可回收）。 |
| **SReclaimable**     | 98300      | **可回收 Slab**（如文件系统缓存）。⚠️ **可回收**。                                        |
| **SUnreclaim**       | 197456     | **不可回收 Slab**（如网络栈、驱动占用）。✅ **明确占用物理内存**。                         |
| **KernelStack**      | 27600      | **内核栈**（每个线程的内核态栈）。✅ **明确占用物理内存**。                                |
| **PageTables**       | 23808      | **页表映射**（虚拟内存管理）。✅ **明确占用物理内存**。                                    |
| **NFS_Unstable**     | 0          | **NFS 不稳定页**（等待提交）。🚫 **无占用**。                                              |
| **Bounce**           | 0          | **缓冲高地址内存**（旧硬件兼容）。🚫 **无占用**。                                         |
| **WritebackTmp**     | 0          | **FUSE 临时回写缓存**。🚫 **无占用**。                                                    |
| **CommitLimit**      | 16380928   | **系统允许的提交内存上限**（无 Swap 时为 `MemTotal/2`）。📊 **统计值，非占用**。          |
| **Committed_AS**     | 12744344   | **已提交的虚拟内存**（可能未全部映射物理内存）。📊 **统计值，非占用**。                   |
| **VmallocTotal**     | 34359738367| **虚拟地址空间总量**（理论值）。📊 **统计值，非占用**。                                   |
| **VmallocUsed**      | 231068     | **已用虚拟地址空间**（如内核模块）。⚠️ **部分占用物理内存**。                             |
| **VmallocChunk**     | 34359310332| **最大连续虚拟地址空间**。📊 **统计值，非占用**。                                         |
| **HardwareCorrupted**| 0          | **硬件损坏内存**（被内核标记为不可用）。🚫 **无占用**。                                  |
| **AnonHugePages**    | 4096       | **透明大页（THP）匿名内存**。✅ **明确占用物理内存**。                                     |
| **CmaTotal**         | 0          | **连续内存分配器（CMA）预留**。🚫 **无占用**。                                            |
| **CmaFree**          | 0          | **空闲 CMA 内存**。🚫 **无占用**。                                                        |
| **HugePages_Total**  | 0          | **大页内存总量**（未配置）。🚫 **无占用**。                                               |
| **HugePages_Free**   | 0          | **空闲大页内存**（未配置）。🚫 **无占用**。                                               |
| **Hugepagesize**     | 2048       | **单个大页大小**（2MB）。📊 **静态信息，非占用**。                                        |
| **DirectMap4k**      | 3018560    | **4KB 直接映射内存**（内核线性映射）。📊 **统计值，非独立占用**。                         |
| **DirectMap2M**      | 30535680   | **2MB 直接映射内存**（内核大页映射）。📊 **统计值，非独立占用**。                         |
| **DirectMap1G**      | 2097152    | **1GB 直接映射内存**（内核巨页映射）。📊 **统计值，非独立占用**。                         |

---

#### **物理内存占用分类总结**
| **类别**               | **包含字段**                                                                 | **是否明确占用物理内存** |
|------------------------|-----------------------------------------------------------------------------|--------------------------|
| **用户进程占用**       | `AnonPages` + `Shmem` + `Mapped`（部分）                                   | ✅ **是**                |
| **内核占用**           | `SUnreclaim` + `KernelStack` + `PageTables` + `VmallocUsed`（部分）        | ✅ **是**                |
| **可回收内存**         | `Cached` + `Buffers` + `SReclaimable` + `Active(file)` + `Inactive(file)`  | ❌ **否**                |
| **空闲内存**           | `MemFree`                                                                  | ❌ **否**                |
| **统计/预留值**        | `MemTotal`、`DirectMap*`、`CommitLimit` 等                                 | ❌ **否**                |

---

#### **关键结论**
1. **明确占用物理内存**的字段：  
   - `AnonPages`（进程匿名内存）  
   - `Shmem`（共享内存/tmpfs）  
   - `SUnreclaim`（不可回收 Slab）  
   - `KernelStack`（内核栈）  
   - `PageTables`（页表）  
   - `AnonHugePages`（透明大页）  

2. **总和计算**：  
   - **用户进程** ≈ `AnonPages (3157792) + Shmem (1100268)` ≈ **4.26GB**  
   - **内核** ≈ `SUnreclaim (197456) + KernelStack (27600) + PageTables (23808)` ≈ **248MB**  
   - **总计** ≈ **4.51GB**（实际可能更高，需结合内核动态分配）。  

3. **其余内存**（如 `Cached`、`MemFree`）为**可回收或空闲**，**不视为占用**。
   - **缓存/可回收内存**：约 **1.85GB**  
   - **空闲内存**：约 **5.75GB**  
4. **剩余内存用途**：可能被内核动态管理或未显式统计（如 `smem` 中的 `kernel dynamic memory` 21GB 非缓存部分）。  

> 📌 **注意**：若 `smem` 显示内核占用 21GB 非缓存，而 `meminfo` 未明确统计，可能是内核预留内存（如 DMA、驱动占用等），需结合具体系统用途分析。

### numastat -m

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# numastat -m

Per-node system memory usage (in MBs):
                          Node 0           Total
                 --------------- ---------------
MemTotal                32767.43        32767.43
MemFree                  5612.25         5612.25
MemUsed                 27155.17        27155.17
Active                   3429.36         3429.36
Inactive                 1377.34         1377.34
Active(anon)             3104.48         3104.48
Inactive(anon)           1059.59         1059.59
Active(file)              324.88          324.88
Inactive(file)            317.75          317.75
Unevictable                 0.00            0.00
Mlocked                     0.00            0.00
Dirty                       0.13            0.13
Writeback                   0.00            0.00
FilePages                1717.17         1717.17
Mapped                     99.20           99.20
AnonPages                3089.93         3089.93
Shmem                    1074.48         1074.48
KernelStack                26.98           26.98
PageTables                 22.70           22.70
NFS_Unstable                0.00            0.00
Bounce                      0.00            0.00
WritebackTmp                0.00            0.00
Slab                      289.02          289.02
SReclaimable               96.02           96.02
SUnreclaim                193.00          193.00
AnonHugePages               4.00            4.00
HugePages_Total             0.00            0.00
HugePages_Free              0.00            0.00
HugePages_Surp              0.00            0.00
```

以下是 **`numastat -m`** 命令输出的详细解析，在原始数据右侧标注每项的作用及是否明确占用物理内存：

---

#### **`numastat -m` 内存统计解析**
| 指标                  | 值 (MB)      | **作用与物理内存分配情况**                                                                 |
|-----------------------|--------------|-------------------------------------------------------------------------------------------|
| **MemTotal**          | 32767.43     | **总物理内存**（32GB），硬件实际容量。                                                   |
| **MemFree**           | 5612.25      | **完全空闲内存**（未分配，可立即使用）。✅ **未占用**。                                   |
| **MemUsed**           | 27155.17     | **已使用内存**（含缓存、内核、用户进程）。⚠️ **包含可回收部分**。                         |
| **Active**            | 3429.36      | **活跃内存**（近期被访问）。含 `Active(anon)` 和 `Active(file)`。                         |
| **Inactive**          | 1377.34      | **非活跃内存**（可能被回收）。含 `Inactive(anon)` 和 `Inactive(file)`。                  |
| **Active(anon)**      | 3104.48      | **活跃匿名页**（如进程堆、栈）。✅ **明确占用物理内存**。                                 |
| **Inactive(anon)**    | 1059.59      | **非活跃匿名页**（可能被回收）。⚠️ **暂占用，但可释放**。                                |
| **Active(file)**      | 324.88       | **活跃文件缓存**（如频繁读写的文件）。⚠️ **可回收**。                                   |
| **Inactive(file)**    | 317.75       | **非活跃文件缓存**（如未使用的文件缓存）。⚠️ **可回收**。                                |
| **Unevictable**       | 0.00         | **不可回收内存**（如锁定的用户空间内存）。🚫 **无占用**。                                 |
| **Mlocked**           | 0.00         | **用户空间锁定内存**（防止被交换）。🚫 **无占用**。                                       |
| **Dirty**             | 0.13         | **待写入磁盘的脏页**。⚠️ **暂占用，写入后释放**。                                         |
| **Writeback**         | 0.00         | **正在回写磁盘的内存**。🚫 **无占用**。                                                   |
| **FilePages**         | 1717.17      | **文件缓存**（加速文件访问）。⚠️ **可回收**。                                             |
| **Mapped**            | 99.20        | **文件映射到内存**（如动态库）。⚠️ **部分可回收**。                                       |
| **AnonPages**         | 3089.93      | **已映射的匿名内存**（如进程堆、栈）。✅ **明确占用物理内存**。                           |
| **Shmem**             | 1074.48      | **共享内存**（如 `tmpfs`、IPC）。✅ **明确占用物理内存**。                                |
| **KernelStack**       | 26.98        | **内核栈**（每个线程的内核态栈）。✅ **明确占用物理内存**。                               |
| **PageTables**        | 22.70        | **页表映射**（虚拟内存管理）。✅ **明确占用物理内存**。                                    |
| **NFS_Unstable**      | 0.00         | **NFS 不稳定页**（等待提交）。🚫 **无占用**。                                             |
| **Bounce**            | 0.00         | **缓冲高地址内存**（旧硬件兼容）。🚫 **无占用**。                                         |
| **WritebackTmp**      | 0.00         | **FUSE 临时回写缓存**。🚫 **无占用**。                                                    |
| **Slab**              | 289.02       | **内核对象缓存**（如 dentry、inode）。含 `SReclaimable` 和 `SUnreclaim`。                |
| **SReclaimable**      | 96.02        | **可回收 Slab**（如文件系统缓存）。⚠️ **可回收**。                                        |
| **SUnreclaim**        | 193.00       | **不可回收 Slab**（如网络栈、驱动占用）。✅ **明确占用物理内存**。                        |
| **AnonHugePages**     | 4.00         | **透明大页（THP）匿名内存**。✅ **明确占用物理内存**。                                     |
| **HugePages_Total**   | 0.00         | **大页内存总量**（未配置）。🚫 **无占用**。                                              |
| **HugePages_Free**    | 0.00         | **空闲大页内存**（未配置）。🚫 **无占用**。                                               |
| **HugePages_Surp**    | 0.00         | **超额大页内存**（未配置）。🚫 **无占用**。                                               |

---

#### **物理内存占用分类总结**
| **类别**               | **包含字段**                                                                 | **是否明确占用物理内存** |
|------------------------|-----------------------------------------------------------------------------|--------------------------|
| **用户进程占用**       | `AnonPages` + `Shmem` + `Mapped`（部分）                                   | ✅ **是**                |
| **内核占用**           | `SUnreclaim` + `KernelStack` + `PageTables`                                | ✅ **是**                |
| **可回收内存**         | `FilePages` + `Active(file)` + `Inactive(file)` + `SReclaimable`           | ❌ **否**                |
| **空闲内存**           | `MemFree`                                                                  | ❌ **否**                |
| **统计/预留值**        | `MemTotal`、`HugePages_*` 等                                               | ❌ **否**                |

---

#### **关键结论**
1. **明确占用物理内存**的字段：  
   - `AnonPages`（进程匿名内存）  
   - `Shmem`（共享内存/tmpfs）  
   - `SUnreclaim`（不可回收 Slab）  
   - `KernelStack`（内核栈）  
   - `PageTables`（页表）  
   - `AnonHugePages`（透明大页）  

2. **总和计算**：  
   - **用户进程** ≈ `AnonPages (3089.93) + Shmem (1074.48)` ≈ **4.16GB**  
   - **内核** ≈ `SUnreclaim (193.00) + KernelStack (26.98) + PageTables (22.70)` ≈ **242.68MB**  
   - **总计** ≈ **4.4GB**（实际可能更高，需结合内核动态分配）。  

3. **其余内存**（如 `FilePages`、`MemFree`）为**可回收或空闲**，**不视为占用**。  

4. **NUMA 节点**：  
   - 当前仅 **Node 0** 有数据，说明系统为单节点（无 NUMA 跨节点访问问题）。  

> 📌 **注意**：若与 `smem` 或 `/proc/meminfo` 数据冲突，可能是统计口径差异（如内核预留内存未完全显示）。

### /proc/buddyinfo

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# cat /proc/buddyinfo
Node 0, zone      DMA      0      0      0      0      2      1      1      0      1      1      3
Node 0, zone    DMA32  15436  18684   7851   2302    720    243     80     32     21      0      0
Node 0, zone   Normal 127559 175569  74035  31054  12025   2271    101      0      0      0      0
```

以下是 **`/proc/buddyinfo` 数据的可读化转换**，将原始数字按内存块大小分类，并标注实际物理内存量（假设页大小为 **4KB**）：

---

#### **1. 数据转换表**
##### **列定义**（单位：连续内存块数量）
| 块大小（页数） | 实际大小  | Node 0 (DMA) | Node 0 (DMA32) | Node 0 (Normal) |
|---------------|----------|--------------|----------------|-----------------|
| **2^0**       | 4KB      | 0            | 15436          | 127559          |
| **2^1**       | 8KB      | 0            | 18684          | 175569          |
| **2^2**       | 16KB     | 0            | 7851           | 74035           |
| **2^3**       | 32KB     | 0            | 2302           | 31054           |
| **2^4**       | 64KB     | 2            | 720            | 12025           |
| **2^5**       | 128KB    | 1            | 243            | 2271            |
| **2^6**       | 256KB    | 1            | 80             | 101             |
| **2^7**       | 512KB    | 0            | 32             | 0               |
| **2^8**       | 1MB      | 1            | 21             | 0               |
| **2^9**       | 2MB      | 1            | 0              | 0               |
| **2^10**      | 4MB      | 3            | 0              | 0               |

---

#### **2. 按区域的可读化说明**
##### **① DMA 区域（16MB 以下）**
- **剩余内存块**：  
  - **64KB**：2 块 → **128KB**  
  - **128KB**：1 块 → **128KB**  
  - **256KB**：1 块 → **256KB**  
  - **1MB**：1 块 → **1MB**  
  - **2MB**：1 块 → **2MB**  
  - **4MB**：3 块 → **12MB**  
- **总计空闲**：≈ **15.5MB**（但极度碎片化，无连续大块）。  

##### **② DMA32 区域（4GB 以下）**
- **剩余内存块**：  
  - **≤64KB**：15436 (4KB) + 18684 (8KB) + ... + 720 (64KB) → **≈1.2GB**（碎片化严重）  
  - **≥128KB**：243 (128KB) + 80 (256KB) + ... + 21 (1MB) → **≈80MB**（大块稀缺）  
- **关键问题**：**1MB 以上块仅 21 个**，可能影响大内存需求应用。  

##### **③ Normal 区域（全部内存）**
- **剩余内存块**：  
  - **≤64KB**：127559 (4KB) + 175569 (8KB) + ... + 12025 (64KB) → **≈5.8GB**（碎片化）  
  - **≥128KB**：2271 (128KB) + 101 (256KB) → **≈350MB**  
  - **≥1MB**：**0 块**（无法直接分配超大连续内存）  

---

#### **3. 可视化碎片分布**
##### **DMA 区域**
```plaintext
[4MB][4MB][4MB] [2MB] [1MB] [256KB] [128KB] [128KB] [64KB][64KB]
```
→ **大块（≥1MB）占比高**，但总量小（仅 16MB 区域）。

##### **DMA32/Normal 区域**
```plaintext
[4KB][4KB][8KB][16KB]...[64KB]...[128KB]...[256KB]...（极少 1MB+）
```
→ **90% 内存为 ≤64KB 小块**，大块资源不足。

---

#### **4. 碎片化总结**
| 区域      | 总空闲内存  | 主要块大小       | 碎片化程度 | 可能影响                     |
|-----------|------------|------------------|------------|------------------------------|
| **DMA**   | ~15.5MB    | 4MB/2MB/1MB      | 🔥 **高**  | 旧设备驱动可能分配失败        |
| **DMA32** | ~1.28GB    | 4KB~64KB         | ⚠️ **中**  | 大内存应用（如数据库）性能降级 |
| **Normal**| ~6.15GB    | 4KB~256KB        | ⚠️ **中**  | 需 1MB+ 连续内存的应用异常    |

---

#### **5. 用户友好提示**
- **查看大块内存需求**：  
  ```bash
  grep -i huge /proc/meminfo  # 检查透明大页使用情况
  ```
- **主动缓解碎片**：  
  ```bash
  echo 1 > /proc/sys/vm/compact_memory  # 手动触发内存规整
  ```

> 📌 **关键结论**：系统内存以**小块（≤256KB）为主**，**DMA32/Normal 区域需关注大块分配问题**，但常规应用影响有限。

### /proc/pagetypeinfo

```bash
[root@gz03-SDK-server-10e50e63e22 secure]# cat /proc/pagetypeinfo
Page block order: 9
Pages per block:  512

Free pages count per migrate type at order       0      1      2      3      4      5      6      7      8      9     10
Node    0, zone      DMA, type    Unmovable      0      0      0      0      2      1      1      0      1      0      0
Node    0, zone      DMA, type  Reclaimable      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone      DMA, type      Movable      0      0      0      0      0      0      0      0      0      1      3
Node    0, zone      DMA, type      Reserve      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone      DMA, type          CMA      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone      DMA, type      Isolate      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone    DMA32, type    Unmovable   3441   2551    862    156     49     37     28     10      6      0      0
Node    0, zone    DMA32, type  Reclaimable     21     21    182    149     97     59     33     18     15      0      0
Node    0, zone    DMA32, type      Movable  11625  16116   6805   1996    574    147     19      4      0      0      0
Node    0, zone    DMA32, type      Reserve      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone    DMA32, type          CMA      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone    DMA32, type      Isolate      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone   Normal, type    Unmovable  15016   9903   5654   2105    973    340     15      0      0      0      0
Node    0, zone   Normal, type  Reclaimable   4465   5246   4104   2644   1275    475     41      0      0      0      0
Node    0, zone   Normal, type      Movable 106989 160315  64278  26256   9777   1456     45      0      0      0      0
Node    0, zone   Normal, type      Reserve      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone   Normal, type          CMA      0      0      0      0      0      0      0      0      0      0      0
Node    0, zone   Normal, type      Isolate      0      0      0      0      0      0      0      0      0      0      0

Number of blocks type     Unmovable  Reclaimable      Movable      Reserve          CMA      Isolate
Node 0, zone      DMA            1            0            7            0            0            0
Node 0, zone    DMA32          961           43          524            0            0            0
Node 0, zone   Normal        10261          217         4370            0            0            0
```

---

下面是对/proc/pagetypeinfo 的一次完整、结构化、清晰可读的汇总整理，包括：

#### 基础信息

- **Page block order**: `9`  
  → 表示一个 page block 包含 `2^9 = 512` 个页（pages）  
- **每页大小**: `4KB`（通常的页大小）  
- **每个 page block 大小** = `512 pages × 4KB` = **2MB**  
  ✅ **计算方法：**  
  ```
  2^9 × 4KB = 512 × 4KB = 2048KB = 2MB
  ```

---

#### Page Block Summary by Zone & MigrateType（页块数量与内存占用）


| Node | Zone   | Migrate Type | Block 数量 | 总内存 (约)                       | 说明 |
|------|--------|--------------|------------|-----------------------------------|------|
| 0    | DMA    | Unmovable    | 1          | 1 block × 2MB = 2 MB             | **不可移动**：<br>- **内核早期代码区**：存放在内核启动阶段运行的代码，如内核初始化代码。<br>- **DMA 驱动驻留区**：用于直接内存访问（DMA）的驱动程序常驻内存区域，不会被交换或回收。<br>- 常驻内存不可回收的内核对象。 |
| 0    | DMA    | Reclaimable  | 0          | 0                                 | **可回收**：没有可回收的内存块，表示该区域当前没有标记为可回收的内存。 |
| 0    | DMA    | Movable      | 7          | 7 blocks × 2MB = 14 MB           | **可移动**：<br>- **DMA 区域少量可移动页**：DMA 区的内存空间，某些页可以在需要时被回收或移除。<br>- **可移动页**：内存中的某些部分可以被交换到磁盘或移至其他区域。 |
| 0    | DMA    | Reserve      | 0          | 0                                 | **预留**：没有保留的内存块，表示该区域没有保留任何特殊用途的内存。 |
| 0    | DMA    | CMA          | 0          | 0                                 | **CMA**：未启用 CMA（Contiguous Memory Allocator），因此没有为连续内存分配区保留内存。 |
| 0    | DMA    | Isolate      | 0          | 0                                 | **隔离**：没有使用内存隔离区，表示没有特定的内存区域被标记为隔离区。 |
| 0    | DMA32  | Unmovable    | 961        | 961 blocks × 2MB = 1.88 GB       | **不可移动**：<br>- **驱动驻留内存**：内核驱动程序占用的内存区域，通常不被回收或交换。<br>- **vmalloc 区域**：动态分配的内存区域，通常用于较大的内存块，如大于 PAGE_SIZE 的分配。<br>- **slab 中不可回收对象**：slab 分配器分配的内存，用于存储内核对象，某些对象是不可回收的。 |
| 0    | DMA32  | Reclaimable  | 43         | 43 blocks × 2MB = 86 MB          | **可回收**：<br>- **slab 可回收对象**：例如 inode、dentry 等内核缓存，通常会在空闲时被回收。<br>- **文件页缓存**：用于缓存文件数据，空闲时可以回收以释放内存。 |
| 0    | DMA32  | Movable      | 524        | 524 blocks × 2MB = 1.02 GB       | **可移动**：<br>- **普通匿名页**：用户空间进程的匿名页，这些页不与文件系统映射，通常可以被交换。<br>- **文件缓存页**：操作系统用于缓存文件数据的内存，可以在需要时被移动或回收。<br>- 可以被回收或交换的内存页。 |
| 0    | DMA32  | Reserve      | 0          | 0                                 | **预留**：没有保留内存块，表示没有用于特殊目的的内存区域。 |
| 0    | DMA32  | CMA          | 0          | 0                                 | **CMA**：没有启用 CMA，表示没有为连续内存分配区保留内存。 |
| 0    | DMA32  | Isolate      | 0          | 0                                 | **隔离**：没有使用内存隔离区，表示没有特定的内存区域被标记为隔离区。 |
| 0    | Normal | Unmovable    | 10261      | 10261 blocks × 2MB = 20.04 GB    | **不可移动**：<br>- **内核代码段**：包含内核的主要代码，始终驻留在内存中。<br>- **内核模块**：动态加载的内核模块，它们需要常驻内存。<br>- **slab/slub 中不可回收对象**：slab 分配器分配的不可回收的内存对象，如内核对象。<br>- **vmalloc 映射区域**：由内核动态分配的虚拟内存区域。<br>- **固定映射页（fixmap）**：由内核在固定位置映射的内存区域，用于各种内核操作。 |
| 0    | Normal | Reclaimable  | 217        | 217 blocks × 2MB = 434 MB        | **可回收**：<br>- **可回收的 slab 对象**：如 dentry、inode 等内核缓存对象，空闲时可以被回收。<br>- **文件页缓存（page cache）**：用于缓存文件数据，空闲时可以回收。<br>- **inode/dentry 缓存**：文件系统缓存，包含元数据。 |
| 0    | Normal | Movable      | 4370       | 4370 blocks × 2MB = 8.54 GB      | **可移动**：<br>- **用户空间匿名页**：用户进程的匿名内存页，通常可以被交换。<br>- **文件映射页**：映射到用户空间的文件数据页。<br>- **KSM 页**：用于内存合并的页，KSM（Kernel Same-page Merging）技术可以将相同内容的页合并以节省内存。 |
| 0    | Normal | Reserve      | 0          | 0                                 | **预留**：没有保留内存块，表示没有用于特殊目的的内存区域。 |
| 0    | Normal | CMA          | 0          | 0                                 | **CMA**：没有启用 CMA，表示没有为连续内存分配区保留内存。 |
| 0    | Normal | Isolate      | 0          | 0                                 | **隔离**：没有使用内存隔离区，表示没有特定的内存区域被标记为隔离区。 |
---

#### 总体可视化统计（按 Zone）

| Zone   | Unmovable | Reclaimable | Movable | Total Blocks | Total Size (GB) |
|--------|-----------|-------------|---------|---------------|------------------|
| DMA    | 1         | 0           | 7       | 8             | 0.016 GB         |
| DMA32  | 961       | 43          | 524     | 1528          | 2.98 GB          |
| Normal | 10261     | 217         | 4370    | 14848         | 29.0 GB          |
| **总计** | **11223** | **260**     | **4901** | **16384 blocks** | **32.0 GB**（总内存） |

---

#### 重点观察

- **Normal 区是主力：**
  - Unmovable：`10261 blocks` ≈ 20.04 GB
  - Movable：`4370 blocks` ≈ 8.54 GB
- **DMA32 区也承担了一部分常驻内核内存**（如 Unmovable 961 blocks ≈ 1.88 GB）
- **未启用 CMA 和 Isolate**，说明没有使用大块连续内存预留和内存热拔插功能。

#### 额外信息来源及命令建议

| 信息类型 | 工具 / 命令 | 说明 |
|----------|--------------|------|
| Slab/Slub 分配占用 | `slabtop` / `cat /proc/slabinfo` | 查看内核结构分配情况 |
| 模块加载情况 | `lsmod` / `cat /proc/modules` | 检查 ko 占用内存 |
| vmalloc 区域分析 | `cat /proc/vmallocinfo` | 看分配了多少虚拟地址 |
| 页主跟踪 | `cat /sys/kernel/debug/page_owner` | 若开启 `CONFIG_PAGE_OWNER`，可精确定位占用源 |
| 内存碎片情况 | `cat /proc/buddyinfo` | 查看页分配碎片状态 |

### 找出 Normal 区中那 10261 个不可移动页块（约 20GB） 都是被谁占用的?

要找出 Normal 区中那 **10261 个不可移动页块（约 20GB）** 都是被谁占用的，可以从多个维度来排查，方法如下：

---

#### 一、先理解：“不可移动（Unmovable）” 包括哪些内容？

Unmovable 类型的页块，通常包含：

- 内核核心代码（text/data 段）
- 内核模块（ko 文件加载后常驻内存）
- 页表、vmalloc 区、固定映射（fixmap）
- slab/slub 分配器中的非回收对象（slab caches）
- 不可迁移的内核结构（如 task_struct、文件描述符等）
- 映射到设备的页（如 DMA 内存）

---

#### 二、定位这些页占用来源的方法

##### 1. **slab/slub 分配器：占了多少内存？哪些对象是 Unmovable？**

使用：
```bash
slabtop -s c -o | head -n20
```

或者：
```bash
cat /proc/slabinfo | sort -k 3 -nr | head -20
```

看哪些对象数量巨大，占用内存多，特别是 `allocs` 很多但 `frees` 很少的项（无法释放 = Unmovable）。

---

##### 2. **内核模块：是否加载了很多 ko 占据了 Unmovable 区？**

```bash
lsmod | sort -k2 -nr | head
```

查看模块的使用量。

配合：
```bash
cat /proc/modules
```
看大小字段。

---

##### 3. **vmalloc 区域**

查看 vmalloc 分配情况：
```bash
cat /proc/vmallocinfo | less
```

vmalloc 分配出来的地址空间大多数是 Unmovable（虽然页面本身可能有迁移可能性，但 pageblock 仍视作 Unmovable）。

---

##### 4. **page owner tracing（较高级）**

如果你打开了 `CONFIG_PAGE_OWNER`，可以直接查看页面是谁分配的：

```bash
cat /sys/kernel/debug/page_owner
```

然后配合筛选：
```bash
grep -A30 Unmovable /sys/kernel/debug/page_owner | less
```

或者想查 Normal 区页可这样：
```bash
grep Normal /sys/kernel/debug/page_owner | grep Unmovable
```

> 注：需要内核编译时打开 `CONFIG_PAGE_OWNER` 并在grub加内核参数启动：
```bash
page_owner=on
```

---

##### 5. **buddyinfo 辅助判断碎片分布**

```bash
cat /proc/buddyinfo
```

可以查看 Normal 区是否高度碎片化，是否还有可能回收页。

---

##### 6. **内核页映射分布分析（较高级）**

可以通过 `page-types` 工具配合 `page_owner` 分析页面用途。

比如：
```bash
sudo page-types -r /proc/kcore
```

可以尝试查看哪些物理页是 Unmovable。

---

#### 三、实用建议组合分析方法

1. 先用 `slabtop` 查看谁吃了最多的内存。
2. 看 `lsmod` 模块是否加载太多。
3. 检查 `vmallocinfo` 是否用了很多大分配。
4. 如果你能重启加内核参数，可以启用 `page_owner` 来精确找出分配源。
5. 结合 `buddyinfo` 看是否高度碎片化（意味着很多 page block 无法释放）。

---

## More

- [VMware non-cache 内存增加](https://www.jianshu.com/p/b25a4832f799)
- [升级服务器内核解决Kernel内存泄露](https://blog.csdn.net/weixin_39445556/article/details/103779384)
- [kernel dynamic memory leak](https://stackoverflow.com/questions/49824718/kernel-dynamic-memory-leak)
