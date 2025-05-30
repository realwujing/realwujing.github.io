# k8s pod vm causes host to lag

## perf sched record sleep 10

```bash
perf sched record -- sleep 10
```

### perf sched latency

```bash
perf sched latency
```

```bash
[root@kvm-10e63e2e11 sched]# perf sched latency | head -n50

 -------------------------------------------------------------------------------------------------------------------------------------------
  Task                  |   Runtime ms  | Switches | Avg delay ms    | Max delay ms    | Max delay start           | Max delay end          |
 -------------------------------------------------------------------------------------------------------------------------------------------
  kube-rbac-proxy:(10)  |    246.604 ms |       11 | avg: 235.994 ms | max:2595.940 ms | max start:  1018.904883 s | max end:  1021.500822 s
  pmdaproc:37227        |     44.210 ms |        8 | avg: 220.555 ms | max: 396.114 ms | max start:  1030.104816 s | max end:  1030.500930 s
  systemd-journal:1434  |     41.486 ms |       11 | avg: 217.773 ms | max:1200.254 ms | max start:  1015.601925 s | max end:  1016.802179 s
  handler802:3535       |      3.128 ms |        3 | avg: 199.299 ms | max: 397.844 ms | max start:  1013.803633 s | max end:  1014.201477 s
  kubelet:(57)          |   5405.373 ms |     1074 | avg: 189.415 ms | max:10699.952 ms | max start:  1014.301035 s | max end:  1025.000986 s
  top:38672             |    130.657 ms |       28 | avg: 188.341 ms | max: 396.074 ms | max start:  1016.004921 s | max end:  1016.400996 s
  lldpad:3694           |     66.213 ms |       13 | avg: 165.613 ms | max: 399.849 ms | max start:  1025.401187 s | max end:  1025.801037 s
  python3:39053         |    223.894 ms |       49 | avg: 158.652 ms | max: 496.048 ms | max start:  1023.304831 s | max end:  1023.800879 s
  ps:(4)                |     26.391 ms |        8 | avg: 147.912 ms | max: 397.345 ms | max start:  1028.303656 s | max end:  1028.701001 s
  irqbalance:2476       |     17.671 ms |        5 | avg: 136.826 ms | max: 196.212 ms | max start:  1026.004821 s | max end:  1026.201034 s
  tuned:(4)             |    183.534 ms |       38 | avg: 129.242 ms | max:1664.892 ms | max start:  1015.501369 s | max end:  1017.166260 s
  handler829:3562       |      4.398 ms |        4 | avg: 125.093 ms | max: 300.118 ms | max start:  1024.701370 s | max end:  1025.001487 s
  perf:(2)              |    164.055 ms |        9 | avg: 120.357 ms | max: 199.819 ms | max start:  1015.101156 s | max end:  1015.300974 s
  telegraf:(32)         |    984.237 ms |      412 | avg: 117.740 ms | max:1796.327 ms | max start:  1015.604875 s | max end:  1017.401202 s
  handler818:3551       |      2.872 ms |        4 | avg:  99.815 ms | max: 399.213 ms | max start:  1013.801800 s | max end:  1014.201013 s
  handler782:3515       |      4.189 ms |        2 | avg:  99.616 ms | max: 199.231 ms | max start:  1013.602803 s | max end:  1013.802034 s
  mktemp:(3)            |      4.570 ms |        5 | avg:  99.244 ms | max: 199.597 ms | max start:  1016.401266 s | max end:  1016.600864 s
  chown:39132           |      1.413 ms |        1 | avg:  96.869 ms | max:  96.869 ms | max start:  1018.104109 s | max end:  1018.200978 s
  systemd-udevd:(14)    |     30.763 ms |       14 | avg:  87.758 ms | max: 420.028 ms | max start:  1031.280755 s | max end:  1031.700784 s
  multus:(25)           |    371.535 ms |      191 | avg:  85.114 ms | max:1299.401 ms | max start:  1016.201574 s | max end:  1017.500975 s
  cat:(5)               |     11.147 ms |        6 | avg:  82.681 ms | max: 199.529 ms | max start:  1026.801322 s | max end:  1027.000852 s
  containerd-shim:(464) |   3962.754 ms |     1476 | avg:  81.516 ms | max:12184.772 ms | max start:  1014.101112 s | max end:  1026.285884 s
  dbus-daemon:2413      |     29.356 ms |       10 | avg:  78.409 ms | max: 384.632 ms | max start:  1030.016759 s | max end:  1030.401391 s
  handler785:3518       |      3.349 ms |        9 | avg:  77.741 ms | max: 599.427 ms | max start:  1013.602320 s | max end:  1014.201747 s
  handler814:3547       |      3.874 ms |        4 | avg:  75.291 ms | max: 201.878 ms | max start:  1013.601496 s | max end:  1013.803374 s
  pmgetopt:(2)          |      9.016 ms |        4 | avg:  73.066 ms | max: 196.069 ms | max start:  1027.104831 s | max end:  1027.300900 s
  handler836:(2)        |      3.244 ms |        6 | avg:  66.248 ms | max: 198.991 ms | max start:  1018.502154 s | max end:  1018.701144 s
  gawk:(2)              |      4.963 ms |        3 | avg:  65.379 ms | max: 196.131 ms | max start:  1030.804774 s | max end:  1031.000905 s
  revalidator861:3594   |      2.547 ms |       11 | avg:  63.653 ms | max: 700.082 ms | max start:  1025.301143 s | max end:  1026.001225 s
  handler824:3557       |      4.141 ms |        5 | avg:  60.055 ms | max: 300.240 ms | max start:  1021.801203 s | max end:  1022.101442 s
  handler837:3570       |      4.038 ms |        5 | avg:  60.052 ms | max: 199.196 ms | max start:  1016.801851 s | max end:  1017.001047 s
  cilium-cni:(18)       |     96.636 ms |       72 | avg:  58.113 ms | max: 297.290 ms | max start:  1021.403731 s | max end:  1021.701021 s
  which:(8)             |      6.901 ms |        7 | avg:  56.997 ms | max: 299.564 ms | max start:  1030.301215 s | max end:  1030.600778 s
  pmlogger_daily:(8)    |     19.448 ms |        9 | avg:  54.123 ms | max: 196.029 ms | max start:  1028.704796 s | max end:  1028.900824 s
  sed:(12)              |     22.260 ms |       13 | avg:  52.820 ms | max: 196.266 ms | max start:  1015.104776 s | max end:  1015.301041 s
  handler806:3539       |      2.879 ms |        2 | avg:  49.976 ms | max:  99.952 ms | max start:  1019.301045 s | max end:  1019.400997 s
  handler827:3560       |      3.656 ms |        4 | avg:  49.962 ms | max: 199.815 ms | max start:  1021.701333 s | max end:  1021.901148 s
  auditd:(2)            |      2.516 ms |        4 | avg:  49.915 ms | max: 199.658 ms | max start:  1028.501204 s | max end:  1028.700862 s
  handler809:3542       |      2.675 ms |        2 | avg:  49.885 ms | max:  99.771 ms | max start:  1016.901251 s | max end:  1017.001022 s
  revalidator849:3582   |      2.625 ms |       12 | avg:  49.869 ms | max: 400.179 ms | max start:  1023.301025 s | max end:  1023.701204 s
  awk:(2)               |      4.952 ms |        2 | avg:  49.613 ms | max:  99.221 ms | max start:  1025.102117 s | max end:  1025.201338 s
  containerd:(26)       |    560.430 ms |      487 | avg:  48.240 ms | max:6000.147 ms | max start:  1014.701004 s | max end:  1020.701151 s
  dirname:39091         |      1.315 ms |        2 | avg:  48.203 ms | max:  96.403 ms | max start:  1017.404864 s | max end:  1017.501267 s
  stepInstanceId_:39842 |      1.809 ms |        2 | avg:  48.121 ms | max:  96.237 ms | max start:  1028.104792 s | max end:  1028.201029 s
  ntpd:2505             |      9.685 ms |        2 | avg:  48.068 ms | max:  96.136 ms | max start:  1023.704987 s | max end:  1023.801124 s
  revalidator856:(2)    |      4.870 ms |       17 | avg:  47.021 ms | max: 400.377 ms | max start:  1021.401130 s | max end:  1021.801507 s
```

## perf sched latency -s max

```bash
[root@kvm-10e63e2e11 sched]# perf sched latency -s max | head -n50

 -------------------------------------------------------------------------------------------------------------------------------------------
  Task                  |   Runtime ms  | Switches | Avg delay ms    | Max delay ms    | Max delay start           | Max delay end          |
 -------------------------------------------------------------------------------------------------------------------------------------------
  containerd-shim:(464) |   3962.754 ms |     1476 | avg:  81.516 ms | max:12184.772 ms | max start:  1014.101112 s | max end:  1026.285884 s
  kubelet:(57)          |   5405.373 ms |     1074 | avg: 189.415 ms | max:10699.952 ms | max start:  1014.301035 s | max end:  1025.000986 s
  containerd:(26)       |    560.430 ms |      487 | avg:  48.240 ms | max:6000.147 ms | max start:  1014.701004 s | max end:  1020.701151 s
  prio-rpc-virtqe:(203) |   1883.109 ms |      215 | avg:  12.670 ms | max:2721.486 ms | max start:  1017.960502 s | max end:  1020.681988 s
  kube-rbac-proxy:(10)  |    246.604 ms |       11 | avg: 235.994 ms | max:2595.940 ms | max start:  1018.904883 s | max end:  1021.500822 s
  telegraf:(32)         |    984.237 ms |      412 | avg: 117.740 ms | max:1796.327 ms | max start:  1015.604875 s | max end:  1017.401202 s
  tuned:(4)             |    183.534 ms |       38 | avg: 129.242 ms | max:1664.892 ms | max start:  1015.501369 s | max end:  1017.166260 s
  multus:(25)           |    371.535 ms |      191 | avg:  85.114 ms | max:1299.401 ms | max start:  1016.201574 s | max end:  1017.500975 s
  systemd-journal:1434  |     41.486 ms |       11 | avg: 217.773 ms | max:1200.254 ms | max start:  1015.601925 s | max end:  1016.802179 s
  urcu5:(3)             |     15.630 ms |      164 | avg:  15.265 ms | max:1099.983 ms | max start:  1024.901423 s | max end:  1026.001406 s
  revalidator861:3594   |      2.547 ms |       11 | avg:  63.653 ms | max: 700.082 ms | max start:  1025.301143 s | max end:  1026.001225 s
  handler785:3518       |      3.349 ms |        9 | avg:  77.741 ms | max: 599.427 ms | max start:  1013.602320 s | max end:  1014.201747 s
  runc:[0:PARENT]:(18)  |     49.914 ms |       28 | avg:  42.476 ms | max: 596.020 ms | max start:  1024.704822 s | max end:  1025.300842 s
  python3:39053         |    223.894 ms |       49 | avg: 158.652 ms | max: 496.048 ms | max start:  1023.304831 s | max end:  1023.800879 s
  etcd:(48)             |    879.084 ms |      553 | avg:  19.794 ms | max: 492.110 ms | max start:  1023.308816 s | max end:  1023.800927 s
  systemd-udevd:(14)    |     30.763 ms |       14 | avg:  87.758 ms | max: 420.028 ms | max start:  1031.280755 s | max end:  1031.700784 s
  revalidator843:3576   |      5.648 ms |       25 | avg:  39.871 ms | max: 400.761 ms | max start:  1021.401145 s | max end:  1021.801906 s
  revalidator856:(2)    |      4.870 ms |       17 | avg:  47.021 ms | max: 400.377 ms | max start:  1021.401130 s | max end:  1021.801507 s
  revalidator849:3582   |      2.625 ms |       12 | avg:  49.869 ms | max: 400.179 ms | max start:  1023.301025 s | max end:  1023.701204 s
  revalidator853:3586   |      3.227 ms |       14 | avg:  28.639 ms | max: 399.989 ms | max start:  1025.601251 s | max end:  1026.001241 s
  lldpad:3694           |     66.213 ms |       13 | avg: 165.613 ms | max: 399.849 ms | max start:  1025.401187 s | max end:  1025.801037 s
  handler818:3551       |      2.872 ms |        4 | avg:  99.815 ms | max: 399.213 ms | max start:  1013.801800 s | max end:  1014.201013 s
  rs:main Q:Reg:5548    |     11.456 ms |      129 | avg:   6.958 ms | max: 398.619 ms | max start:  1029.002725 s | max end:  1029.401344 s
  handler802:3535       |      3.128 ms |        3 | avg: 199.299 ms | max: 397.844 ms | max start:  1013.803633 s | max end:  1014.201477 s
  ps:(4)                |     26.391 ms |        8 | avg: 147.912 ms | max: 397.345 ms | max start:  1028.303656 s | max end:  1028.701001 s
  pmdaproc:37227        |     44.210 ms |        8 | avg: 220.555 ms | max: 396.114 ms | max start:  1030.104816 s | max end:  1030.500930 s
  top:38672             |    130.657 ms |       28 | avg: 188.341 ms | max: 396.074 ms | max start:  1016.004921 s | max end:  1016.400996 s
  dbus-daemon:2413      |     29.356 ms |       10 | avg:  78.409 ms | max: 384.632 ms | max start:  1030.016759 s | max end:  1030.401391 s
  handler824:3557       |      4.141 ms |        5 | avg:  60.055 ms | max: 300.240 ms | max start:  1021.801203 s | max end:  1022.101442 s
  handler796:3529       |      3.368 ms |        7 | avg:  42.906 ms | max: 300.156 ms | max start:  1016.201007 s | max end:  1016.501163 s
  runc:(140)            |    208.196 ms |      305 | avg:  20.523 ms | max: 300.124 ms | max start:  1024.601065 s | max end:  1024.901190 s
  handler829:3562       |      4.398 ms |        4 | avg: 125.093 ms | max: 300.118 ms | max start:  1024.701370 s | max end:  1025.001487 s
  revalidator862:3595   |      3.359 ms |       27 | avg:  14.825 ms | max: 300.010 ms | max start:  1025.001550 s | max end:  1025.301561 s
  revalidator854:3587   |      2.663 ms |       16 | avg:  43.436 ms | max: 299.596 ms | max start:  1016.201641 s | max end:  1016.501237 s
  which:(8)             |      6.901 ms |        7 | avg:  56.997 ms | max: 299.564 ms | max start:  1030.301215 s | max end:  1030.600778 s
  expr:(89)             |    141.428 ms |      103 | avg:  39.680 ms | max: 298.225 ms | max start:  1021.402627 s | max end:  1021.700852 s
  spiderpool:(12)       |     42.566 ms |       59 | avg:  28.363 ms | max: 297.334 ms | max start:  1027.103758 s | max end:  1027.401093 s
  cilium-cni:(18)       |     96.636 ms |       72 | avg:  58.113 ms | max: 297.290 ms | max start:  1021.403731 s | max end:  1021.701021 s
  ovs-vswitchd:(3)      |     63.690 ms |      136 | avg:  16.631 ms | max: 296.212 ms | max start:  1023.304862 s | max end:  1023.601073 s
  ovs:(9)               |     20.222 ms |       23 | avg:  25.588 ms | max: 288.744 ms | max start:  1025.412274 s | max end:  1025.701018 s
  handler772:3505       |      9.531 ms |       21 | avg:  19.745 ms | max: 284.354 ms | max start:  1014.916976 s | max end:  1015.201330 s
  handler774:3507       |     10.248 ms |       32 | avg:  22.051 ms | max: 273.012 ms | max start:  1016.128128 s | max end:  1016.401139 s
  pmlogger_check:(18)   |     88.010 ms |       26 | avg:  44.451 ms | max: 263.276 ms | max start:  1014.738122 s | max end:  1015.001398 s
  runc:[2:INIT]:(77)    |     22.736 ms |       79 | avg:  16.537 ms | max: 225.176 ms | max start:  1025.701581 s | max end:  1025.926757 s
  handler814:3547       |      3.874 ms |        4 | avg:  75.291 ms | max: 201.878 ms | max start:  1013.601496 s | max end:  1013.803374 s
  in:imjournal:5547     |     28.090 ms |       71 | avg:  12.605 ms | max: 200.042 ms | max start:  1029.200872 s | max end:  1029.400914 s
```

### perf sched latency -s avg

```bash
[root@kvm-10e63e2e11 sched]# perf sched latency -s avg | head -n50

 -------------------------------------------------------------------------------------------------------------------------------------------
  Task                  |   Runtime ms  | Switches | Avg delay ms    | Max delay ms    | Max delay start           | Max delay end          |
 -------------------------------------------------------------------------------------------------------------------------------------------
  kube-rbac-proxy:(10)  |    246.604 ms |       11 | avg: 235.994 ms | max:2595.940 ms | max start:  1018.904883 s | max end:  1021.500822 s
  pmdaproc:37227        |     44.210 ms |        8 | avg: 220.555 ms | max: 396.114 ms | max start:  1030.104816 s | max end:  1030.500930 s
  systemd-journal:1434  |     41.486 ms |       11 | avg: 217.773 ms | max:1200.254 ms | max start:  1015.601925 s | max end:  1016.802179 s
  handler802:3535       |      3.128 ms |        3 | avg: 199.299 ms | max: 397.844 ms | max start:  1013.803633 s | max end:  1014.201477 s
  kubelet:(57)          |   5405.373 ms |     1074 | avg: 189.415 ms | max:10699.952 ms | max start:  1014.301035 s | max end:  1025.000986 s
  top:38672             |    130.657 ms |       28 | avg: 188.341 ms | max: 396.074 ms | max start:  1016.004921 s | max end:  1016.400996 s
  lldpad:3694           |     66.213 ms |       13 | avg: 165.613 ms | max: 399.849 ms | max start:  1025.401187 s | max end:  1025.801037 s
  python3:39053         |    223.894 ms |       49 | avg: 158.652 ms | max: 496.048 ms | max start:  1023.304831 s | max end:  1023.800879 s
  ps:(4)                |     26.391 ms |        8 | avg: 147.912 ms | max: 397.345 ms | max start:  1028.303656 s | max end:  1028.701001 s
  irqbalance:2476       |     17.671 ms |        5 | avg: 136.826 ms | max: 196.212 ms | max start:  1026.004821 s | max end:  1026.201034 s
  tuned:(4)             |    183.534 ms |       38 | avg: 129.242 ms | max:1664.892 ms | max start:  1015.501369 s | max end:  1017.166260 s
  handler829:3562       |      4.398 ms |        4 | avg: 125.093 ms | max: 300.118 ms | max start:  1024.701370 s | max end:  1025.001487 s
  perf:(2)              |    164.055 ms |        9 | avg: 120.357 ms | max: 199.819 ms | max start:  1015.101156 s | max end:  1015.300974 s
  telegraf:(32)         |    984.237 ms |      412 | avg: 117.740 ms | max:1796.327 ms | max start:  1015.604875 s | max end:  1017.401202 s
  handler818:3551       |      2.872 ms |        4 | avg:  99.815 ms | max: 399.213 ms | max start:  1013.801800 s | max end:  1014.201013 s
  handler782:3515       |      4.189 ms |        2 | avg:  99.616 ms | max: 199.231 ms | max start:  1013.602803 s | max end:  1013.802034 s
  mktemp:(3)            |      4.570 ms |        5 | avg:  99.244 ms | max: 199.597 ms | max start:  1016.401266 s | max end:  1016.600864 s
  chown:39132           |      1.413 ms |        1 | avg:  96.869 ms | max:  96.869 ms | max start:  1018.104109 s | max end:  1018.200978 s
  systemd-udevd:(14)    |     30.763 ms |       14 | avg:  87.758 ms | max: 420.028 ms | max start:  1031.280755 s | max end:  1031.700784 s
  multus:(25)           |    371.535 ms |      191 | avg:  85.114 ms | max:1299.401 ms | max start:  1016.201574 s | max end:  1017.500975 s
  cat:(5)               |     11.147 ms |        6 | avg:  82.681 ms | max: 199.529 ms | max start:  1026.801322 s | max end:  1027.000852 s
  containerd-shim:(464) |   3962.754 ms |     1476 | avg:  81.516 ms | max:12184.772 ms | max start:  1014.101112 s | max end:  1026.285884 s
  dbus-daemon:2413      |     29.356 ms |       10 | avg:  78.409 ms | max: 384.632 ms | max start:  1030.016759 s | max end:  1030.401391 s
  handler785:3518       |      3.349 ms |        9 | avg:  77.741 ms | max: 599.427 ms | max start:  1013.602320 s | max end:  1014.201747 s
  handler814:3547       |      3.874 ms |        4 | avg:  75.291 ms | max: 201.878 ms | max start:  1013.601496 s | max end:  1013.803374 s
  pmgetopt:(2)          |      9.016 ms |        4 | avg:  73.066 ms | max: 196.069 ms | max start:  1027.104831 s | max end:  1027.300900 s
  handler836:(2)        |      3.244 ms |        6 | avg:  66.248 ms | max: 198.991 ms | max start:  1018.502154 s | max end:  1018.701144 s
  gawk:(2)              |      4.963 ms |        3 | avg:  65.379 ms | max: 196.131 ms | max start:  1030.804774 s | max end:  1031.000905 s
  revalidator861:3594   |      2.547 ms |       11 | avg:  63.653 ms | max: 700.082 ms | max start:  1025.301143 s | max end:  1026.001225 s
  handler824:3557       |      4.141 ms |        5 | avg:  60.055 ms | max: 300.240 ms | max start:  1021.801203 s | max end:  1022.101442 s
  handler837:3570       |      4.038 ms |        5 | avg:  60.052 ms | max: 199.196 ms | max start:  1016.801851 s | max end:  1017.001047 s
  cilium-cni:(18)       |     96.636 ms |       72 | avg:  58.113 ms | max: 297.290 ms | max start:  1021.403731 s | max end:  1021.701021 s
  which:(8)             |      6.901 ms |        7 | avg:  56.997 ms | max: 299.564 ms | max start:  1030.301215 s | max end:  1030.600778 s
  pmlogger_daily:(8)    |     19.448 ms |        9 | avg:  54.123 ms | max: 196.029 ms | max start:  1028.704796 s | max end:  1028.900824 s
  sed:(12)              |     22.260 ms |       13 | avg:  52.820 ms | max: 196.266 ms | max start:  1015.104776 s | max end:  1015.301041 s
  handler806:3539       |      2.879 ms |        2 | avg:  49.976 ms | max:  99.952 ms | max start:  1019.301045 s | max end:  1019.400997 s
  handler827:3560       |      3.656 ms |        4 | avg:  49.962 ms | max: 199.815 ms | max start:  1021.701333 s | max end:  1021.901148 s
  auditd:(2)            |      2.516 ms |        4 | avg:  49.915 ms | max: 199.658 ms | max start:  1028.501204 s | max end:  1028.700862 s
  handler809:3542       |      2.675 ms |        2 | avg:  49.885 ms | max:  99.771 ms | max start:  1016.901251 s | max end:  1017.001022 s
  revalidator849:3582   |      2.625 ms |       12 | avg:  49.869 ms | max: 400.179 ms | max start:  1023.301025 s | max end:  1023.701204 s
  awk:(2)               |      4.952 ms |        2 | avg:  49.613 ms | max:  99.221 ms | max start:  1025.102117 s | max end:  1025.201338 s
  containerd:(26)       |    560.430 ms |      487 | avg:  48.240 ms | max:6000.147 ms | max start:  1014.701004 s | max end:  1020.701151 s
  dirname:39091         |      1.315 ms |        2 | avg:  48.203 ms | max:  96.403 ms | max start:  1017.404864 s | max end:  1017.501267 s
  stepInstanceId_:39842 |      1.809 ms |        2 | avg:  48.121 ms | max:  96.237 ms | max start:  1028.104792 s | max end:  1028.201029 s
  ntpd:2505             |      9.685 ms |        2 | avg:  48.068 ms | max:  96.136 ms | max start:  1023.704987 s | max end:  1023.801124 s
  revalidator856:(2)    |      4.870 ms |       17 | avg:  47.021 ms | max: 400.377 ms | max start:  1021.401130 s | max end:  1021.801507 s
```

### perf sched latency -s switch

```bash
[root@kvm-10e63e2e11 sched]# perf sched latency -s switch | head -n50

 -------------------------------------------------------------------------------------------------------------------------------------------
  Task                  |   Runtime ms  | Switches | Avg delay ms    | Max delay ms    | Max delay start           | Max delay end          |
 -------------------------------------------------------------------------------------------------------------------------------------------
  containerd-shim:(464) |   3962.754 ms |     1476 | avg:  81.516 ms | max:12184.772 ms | max start:  1014.101112 s | max end:  1026.285884 s
  kubelet:(57)          |   5405.373 ms |     1074 | avg: 189.415 ms | max:10699.952 ms | max start:  1014.301035 s | max end:  1025.000986 s
  tp_osd_tp:(32)        |    447.397 ms |      715 | avg:   0.004 ms | max:   0.105 ms | max start:  1025.851926 s | max end:  1025.852031 s
  virt-launcher:(426)   |    798.387 ms |      699 | avg:   0.008 ms | max:   0.641 ms | max start:  1020.669281 s | max end:  1020.669922 s
  rpc-virtqemud:(210)   |  12922.765 ms |      661 | avg:   0.017 ms | max:   2.285 ms | max start:  1020.704835 s | max end:  1020.707120 s
  virt-handler:(78)     |    312.861 ms |      565 | avg:   0.025 ms | max:   0.430 ms | max start:  1020.664797 s | max end:  1020.665227 s
  etcd:(48)             |    879.084 ms |      553 | avg:  19.794 ms | max: 492.110 ms | max start:  1023.308816 s | max end:  1023.800927 s
  vm-yue_yue-2025:(40)  |    140.257 ms |      492 | avg:   0.006 ms | max:   0.300 ms | max start:  1020.668261 s | max end:  1020.668561 s
  containerd:(26)       |    560.430 ms |      487 | avg:  48.240 ms | max:6000.147 ms | max start:  1014.701004 s | max end:  1020.701151 s
  kube-apiserver:(86)   |   3136.810 ms |      434 | avg:   0.066 ms | max:   8.600 ms | max start:  1028.376793 s | max end:  1028.385393 s
  telegraf:(32)         |    984.237 ms |      412 | avg: 117.740 ms | max:1796.327 ms | max start:  1015.604875 s | max end:  1017.401202 s
  worker:(325)          |     59.797 ms |      323 | avg:   0.009 ms | max:   2.462 ms | max start:  1017.543865 s | max end:  1017.546327 s
  runc:(140)            |    208.196 ms |      305 | avg:  20.523 ms | max: 300.124 ms | max start:  1024.601065 s | max end:  1024.901190 s
  CPU 0/KVM:(41)        |   3272.451 ms |      240 | avg:   0.012 ms | max:   0.987 ms | max start:  1021.001100 s | max end:  1021.002087 s
  cilium-agent:(59)     |    840.610 ms |      222 | avg:   0.004 ms | max:   0.177 ms | max start:  1030.120610 s | max end:  1030.120787 s
  prio-rpc-virtqe:(203) |   1883.109 ms |      215 | avg:  12.670 ms | max:2721.486 ms | max start:  1017.960502 s | max end:  1020.681988 s
  msgr-worker-0:(10)    |    148.101 ms |      197 | avg:   0.005 ms | max:   0.048 ms | max start:  1023.114982 s | max end:  1023.115030 s
  multus:(25)           |    371.535 ms |      191 | avg:  85.114 ms | max:1299.401 ms | max start:  1016.201574 s | max end:  1017.500975 s
  vhost-34707:34726     |     28.715 ms |      179 | avg:   0.005 ms | max:   0.213 ms | max start:  1022.801751 s | max end:  1022.801965 s
  log-agent:(6)         |    131.918 ms |      165 | avg:   0.017 ms | max:   1.902 ms | max start:  1028.378870 s | max end:  1028.380772 s
  urcu5:(3)             |     15.630 ms |      164 | avg:  15.265 ms | max:1099.983 ms | max start:  1024.901423 s | max end:  1026.001406 s
  qemu-kvm:(41)         |    920.973 ms |      143 | avg:   0.004 ms | max:   0.033 ms | max start:  1020.673388 s | max end:  1020.673421 s
  kube-proxy:(46)       |    425.774 ms |      139 | avg:   0.007 ms | max:   0.218 ms | max start:  1028.203056 s | max end:  1028.203273 s
  ovs-vswitchd:(3)      |     63.690 ms |      136 | avg:  16.631 ms | max: 296.212 ms | max start:  1023.304862 s | max end:  1023.601073 s
  rs:main Q:Reg:5548    |     11.456 ms |      129 | avg:   6.958 ms | max: 398.619 ms | max start:  1029.002725 s | max end:  1029.401344 s
  cephcsi:(63)          |    191.809 ms |      129 | avg:   0.003 ms | max:   0.084 ms | max start:  1028.470169 s | max end:  1028.470253 s
  msgr-worker-1:(10)    |    188.638 ms |      121 | avg:   0.005 ms | max:   0.047 ms | max start:  1023.501487 s | max end:  1023.501534 s
  msgr-worker-2:(10)    |    129.550 ms |      110 | avg:   1.168 ms | max: 127.963 ms | max start:  1028.330663 s | max end:  1028.458626 s
  expr:(89)             |    141.428 ms |      103 | avg:  39.680 ms | max: 298.225 ms | max start:  1021.402627 s | max end:  1021.700852 s
  etcdctl:(48)          |    118.768 ms |      100 | avg:   0.004 ms | max:   0.030 ms | max start:  1022.925185 s | max end:  1022.925215 s
  virtqemud:(42)        |    126.830 ms |       80 | avg:   0.002 ms | max:   0.014 ms | max start:  1024.705444 s | max end:  1024.705458 s
  runc:[2:INIT]:(77)    |     22.736 ms |       79 | avg:  16.537 ms | max: 225.176 ms | max start:  1025.701581 s | max end:  1025.926757 s
  haproxy:(64)          |     47.614 ms |       73 | avg:   0.001 ms | max:   0.008 ms | max start:  1027.103650 s | max end:  1027.103658 s
  cilium-cni:(18)       |     96.636 ms |       72 | avg:  58.113 ms | max: 297.290 ms | max start:  1021.403731 s | max end:  1021.701021 s
  in:imjournal:5547     |     28.090 ms |       71 | avg:  12.605 ms | max: 200.042 ms | max start:  1029.200872 s | max end:  1029.400914 s
  ipset:(68)            |     69.615 ms |       68 | avg:   0.004 ms | max:   0.007 ms | max start:  1028.232748 s | max end:  1028.232755 s
  log:(16)              |     11.338 ms |       65 | avg:   0.003 ms | max:   0.008 ms | max start:  1028.468166 s | max end:  1028.468174 s
  virt-chroot:(63)      |     82.242 ms |       64 | avg:   0.042 ms | max:   2.424 ms | max start:  1017.735276 s | max end:  1017.737700 s
  NetworkManager:3681   |     51.473 ms |       60 | avg:  13.193 ms | max: 196.123 ms | max start:  1022.504831 s | max end:  1022.700954 s
  spiderpool:(12)       |     42.566 ms |       59 | avg:  28.363 ms | max: 297.334 ms | max start:  1027.103758 s | max end:  1027.401093 s
  bstore_kv_sync:(2)    |     88.371 ms |       57 | avg:   0.004 ms | max:   0.012 ms | max start:  1023.095018 s | max end:  1023.095030 s
  ms_dispatch:(9)       |     90.278 ms |       56 | avg:   0.003 ms | max:   0.013 ms | max start:  1016.173230 s | max end:  1016.173244 s
  CPU 5/KVM:34745       |    160.340 ms |       50 | avg:   0.009 ms | max:   0.142 ms | max start:  1013.802597 s | max end:  1013.802739 s
  python3:39053         |    223.894 ms |       49 | avg: 158.652 ms | max: 496.048 ms | max start:  1023.304831 s | max end:  1023.800879 s
  IO mon_iothread:(41)  |    218.127 ms |       47 | avg:   0.003 ms | max:   0.047 ms | max start:  1020.676809 s | max end:  1020.676856 s
  radosgw:(44)          |      2.479 ms |       45 | avg:   0.000 ms | max:   0.004 ms | max start:  1025.756298 s | max end:  1025.756301 s
```

### perf sched latency -s runtime

```bash
[root@kvm-10e63e2e11 sched]# perf sched latency -s runtime | head -n50

 -------------------------------------------------------------------------------------------------------------------------------------------
  Task                  |   Runtime ms  | Switches | Avg delay ms    | Max delay ms    | Max delay start           | Max delay end          |
 -------------------------------------------------------------------------------------------------------------------------------------------
  rpc-virtqemud:(210)   |  12922.765 ms |      661 | avg:   0.017 ms | max:   2.285 ms | max start:  1020.704835 s | max end:  1020.707120 s
  kubelet:(57)          |   5405.373 ms |     1074 | avg: 189.415 ms | max:10699.952 ms | max start:  1014.301035 s | max end:  1025.000986 s
  containerd-shim:(464) |   3962.754 ms |     1476 | avg:  81.516 ms | max:12184.772 ms | max start:  1014.101112 s | max end:  1026.285884 s
  CPU 0/KVM:(41)        |   3272.451 ms |      240 | avg:   0.012 ms | max:   0.987 ms | max start:  1021.001100 s | max end:  1021.002087 s
  kube-apiserver:(86)   |   3136.810 ms |      434 | avg:   0.066 ms | max:   8.600 ms | max start:  1028.376793 s | max end:  1028.385393 s
  prio-rpc-virtqe:(203) |   1883.109 ms |      215 | avg:  12.670 ms | max:2721.486 ms | max start:  1017.960502 s | max end:  1020.681988 s
  ceph:(13)             |   1591.173 ms |       24 | avg:   0.007 ms | max:   0.034 ms | max start:  1026.012789 s | max end:  1026.012822 s
  systemd:1             |   1085.703 ms |       16 | avg:   0.124 ms | max:   0.540 ms | max start:  1028.580769 s | max end:  1028.581309 s
  telegraf:(32)         |    984.237 ms |      412 | avg: 117.740 ms | max:1796.327 ms | max start:  1015.604875 s | max end:  1017.401202 s
  qemu-kvm:(41)         |    920.973 ms |      143 | avg:   0.004 ms | max:   0.033 ms | max start:  1020.673388 s | max end:  1020.673421 s
  etcd:(48)             |    879.084 ms |      553 | avg:  19.794 ms | max: 492.110 ms | max start:  1023.308816 s | max end:  1023.800927 s
  cilium-agent:(59)     |    840.610 ms |      222 | avg:   0.004 ms | max:   0.177 ms | max start:  1030.120610 s | max end:  1030.120787 s
  virt-launcher:(426)   |    798.387 ms |      699 | avg:   0.008 ms | max:   0.641 ms | max start:  1020.669281 s | max end:  1020.669922 s
  containerd:(26)       |    560.430 ms |      487 | avg:  48.240 ms | max:6000.147 ms | max start:  1014.701004 s | max end:  1020.701151 s
  tp_osd_tp:(32)        |    447.397 ms |      715 | avg:   0.004 ms | max:   0.105 ms | max start:  1025.851926 s | max end:  1025.852031 s
  kube-proxy:(46)       |    425.774 ms |      139 | avg:   0.007 ms | max:   0.218 ms | max start:  1028.203056 s | max end:  1028.203273 s
  multus:(25)           |    371.535 ms |      191 | avg:  85.114 ms | max:1299.401 ms | max start:  1016.201574 s | max end:  1017.500975 s
  rbd:(12)              |    343.863 ms |       32 | avg:   0.007 ms | max:   0.077 ms | max start:  1028.436784 s | max end:  1028.436861 s
  virt-handler:(78)     |    312.861 ms |      565 | avg:   0.025 ms | max:   0.430 ms | max start:  1020.664797 s | max end:  1020.665227 s
  kube-rbac-proxy:(10)  |    246.604 ms |       11 | avg: 235.994 ms | max:2595.940 ms | max start:  1018.904883 s | max end:  1021.500822 s
  python3:39053         |    223.894 ms |       49 | avg: 158.652 ms | max: 496.048 ms | max start:  1023.304831 s | max end:  1023.800879 s
  IO mon_iothread:(41)  |    218.127 ms |       47 | avg:   0.003 ms | max:   0.047 ms | max start:  1020.676809 s | max end:  1020.676856 s
  runc:(140)            |    208.196 ms |      305 | avg:  20.523 ms | max: 300.124 ms | max start:  1024.601065 s | max end:  1024.901190 s
  cephcsi:(63)          |    191.809 ms |      129 | avg:   0.003 ms | max:   0.084 ms | max start:  1028.470169 s | max end:  1028.470253 s
  msgr-worker-1:(10)    |    188.638 ms |      121 | avg:   0.005 ms | max:   0.047 ms | max start:  1023.501487 s | max end:  1023.501534 s
  tuned:(4)             |    183.534 ms |       38 | avg: 129.242 ms | max:1664.892 ms | max start:  1015.501369 s | max end:  1017.166260 s
  perf:(2)              |    164.055 ms |        9 | avg: 120.357 ms | max: 199.819 ms | max start:  1015.101156 s | max end:  1015.300974 s
  CPU 5/KVM:34745       |    160.340 ms |       50 | avg:   0.009 ms | max:   0.142 ms | max start:  1013.802597 s | max end:  1013.802739 s
  msgr-worker-0:(10)    |    148.101 ms |      197 | avg:   0.005 ms | max:   0.048 ms | max start:  1023.114982 s | max end:  1023.115030 s
  expr:(89)             |    141.428 ms |      103 | avg:  39.680 ms | max: 298.225 ms | max start:  1021.402627 s | max end:  1021.700852 s
  vm-yue_yue-2025:(40)  |    140.257 ms |      492 | avg:   0.006 ms | max:   0.300 ms | max start:  1020.668261 s | max end:  1020.668561 s
  log-agent:(6)         |    131.918 ms |      165 | avg:   0.017 ms | max:   1.902 ms | max start:  1028.378870 s | max end:  1028.380772 s
  top:38672             |    130.657 ms |       28 | avg: 188.341 ms | max: 396.074 ms | max start:  1016.004921 s | max end:  1016.400996 s
  msgr-worker-2:(10)    |    129.550 ms |      110 | avg:   1.168 ms | max: 127.963 ms | max start:  1028.330663 s | max end:  1028.458626 s
  virtqemud:(42)        |    126.830 ms |       80 | avg:   0.002 ms | max:   0.014 ms | max start:  1024.705444 s | max end:  1024.705458 s
  etcdctl:(48)          |    118.768 ms |      100 | avg:   0.004 ms | max:   0.030 ms | max start:  1022.925185 s | max end:  1022.925215 s
  C2 CompilerThre:31370 |    115.743 ms |        3 | avg:   0.010 ms | max:   0.027 ms | max start:  1016.768854 s | max end:  1016.768882 s
  cilium-cni:(18)       |     96.636 ms |       72 | avg:  58.113 ms | max: 297.290 ms | max start:  1021.403731 s | max end:  1021.701021 s
  ms_dispatch:(9)       |     90.278 ms |       56 | avg:   0.003 ms | max:   0.013 ms | max start:  1016.173230 s | max end:  1016.173244 s
  bstore_kv_sync:(2)    |     88.371 ms |       57 | avg:   0.004 ms | max:   0.012 ms | max start:  1023.095018 s | max end:  1023.095030 s
  pmlogger_check:(18)   |     88.010 ms |       26 | avg:  44.451 ms | max: 263.276 ms | max start:  1014.738122 s | max end:  1015.001398 s
  healthcheck.sh:(22)   |     83.335 ms |       29 | avg:   6.777 ms | max: 195.923 ms | max start:  1020.404993 s | max end:  1020.600916 s
  virt-chroot:(63)      |     82.242 ms |       64 | avg:   0.042 ms | max:   2.424 ms | max start:  1017.735276 s | max end:  1017.737700 s
  umount:(13)           |     77.956 ms |       13 | avg:   0.005 ms | max:   0.009 ms | max start:  1028.703203 s | max end:  1028.703212 s
  admin_socket:(5)      |     73.038 ms |        5 | avg:   0.000 ms | max:   0.000 ms | max start:     0.000000 s | max end:     0.000000 s
  sh:(16)               |     71.880 ms |       20 | avg:  39.076 ms | max: 199.415 ms | max start:  1025.401563 s | max end:  1025.600978 s
```

## perf stat -e 'sched:sched_switch' -a -- sleep 10

```bash
[root@kvm-10e63e2e11 sched]# perf stat -e 'sched:sched_switch' -a -- sleep 10

 Performance counter stats for 'system wide':

           608,122      sched:sched_switch

      10.003854148 seconds time elapsed
```

### **分析 `perf stat -e 'sched:sched_switch'` 输出**

#### **1. 关键指标**
- **`sched:sched_switch` 次数**：608,122 次（10 秒内）
- **上下文切换频率**：约 **60,812 次/秒**

#### **2. 正常范围参考**
- **一般服务器**：通常 **1,000~10,000 次/秒**（取决于负载）。
- **高负载系统**：可能达到 **20,000~50,000 次/秒**。
- **当前值（60k/s）**：**显著偏高**，可能存在调度瓶颈。

---

## vmstat 1

```bash
[root@kvm-10e63e2e11 perf]# vmstat 1
procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
 2  1      0 130696016 2448844 10975628    0    0   285   139  390  548  1  1 97  0  0
28  0      0 130696016 2448844 10975628    0    0   908  1623 30614 49237  1  0 99  0  0
14  0      0 130696016 2448844 10975628    0    0 14252  2231 45793 75704  1  1 98  0  0
14  0      0 130696016 2448844 10975628    0    0 11224  2683 50652 86420  1  1 98  0  0
23  0      0 130696016 2448844 10975628    0    0   884  1726 36250 57294  1  1 98  0  0
 4  0      0 130696016 2448844 10975628    0    0    40  1633 32425 48965  1  0 99  0  0
 2  0      0 130696016 2448844 10975628    0    0   848  1307 30691 48112  1  1 99  0  0
 8  0      0 130696016 2448844 10975628    0    0   620  2271 29578 44909  1  0 99  0  0
 1  0      0 130696016 2448844 10975628    0    0   260  3307 30759 46740  1  1 98  0  0
36  0      0 130696016 2448844 10975628    0    0     0 35976 44374 54982  1  1 99  0  0
```

从 `vmstat 1` 的输出来看，系统的整体负载和资源使用情况似乎较为正常，但结合之前 `perf sched latency` 中观测到的高调度延迟（如 `kubelet` 最大延迟 **10.7 秒**），需要进一步深入分析。以下是关键点解读和排查建议：

---

### **1. `vmstat` 关键指标分析**
#### **CPU 使用率（`us`/`sy`/`id`）**
- **`us`（用户态）**：1% 左右，正常。
- **`sy`（内核态）**：1% 左右，无异常飙升。
- **`id`（空闲）**：98%~99%，CPU 资源充足。  
  **矛盾点**：CPU 空闲率高，但 `perf sched` 显示高调度延迟，可能是 **锁竞争** 或 **进程阻塞在 I/O 等待**（尽管 `wa` 为 0）。

#### **上下文切换（`cs`）**
- **`cs`**：约 40k~80k 次/秒，**偏高**（通常建议低于 10k/秒）。  
  **可能原因**：  
  - 大量线程频繁切换（如 Kubernetes 相关进程）。
  - 锁竞争导致无效切换。

#### **中断（`in`）**
- **`in`**：30k~50k 次/秒，**非常高**（正常应低于 1k/秒）。  
  **可能原因**：  
  - 网络或磁盘中断风暴（如高频率网络包或磁盘 I/O）。
  - 虚拟化层（如 KVM）的中断处理瓶颈。

#### **I/O 情况（`bi`/`bo`）**
- **`bi`（块设备读取）**：波动较大（0~14k blocks/s），可能存在突发磁盘读操作。  
- **`bo`（块设备写入）**：偶发峰值（如 35k blocks/s），需检查日志或持久化操作。

## dstat -tam

```bash
[root@kvm-10e63e2e11 perf]# dstat -tam
----system---- ----total-usage---- -dsk/total- -net/total- ---paging-- ---system-- ------memory-usage-----
     time     |usr sys idl wai stl| read  writ| recv  send|  in   out | int   csw | used  free  buf   cach
01-04 23:08:27|                   |           |           |           |           |  50G  125G 2394M 9929M
01-04 23:08:28|  1   1  98   0   0| 945k 3316k|4938k 2649k|   0     0 |  32k   50k|  50G  125G 2394M 9929M
01-04 23:08:29|  1   0  98   0   0| 145k 1217k| 602k  868k|   0     0 |  35k   54k|  50G  125G 2394M 9929M
01-04 23:08:30|  2   1  97   0   0|9747B 1391k| 650k  496k|   0     0 |  27k   40k|  50G  125G 2395M 9929M
01-04 23:08:31|  1   0  98   0   0| 344k 3850k|5823k   16M|   0     0 |  39k   65k|  50G  125G 2395M 9929M
01-04 23:08:33|  1   0  97   0   0|  29M 2261k|  47M 1372k|   0     0 |  48k   73k|  50G  125G 2395M 9929M
01-04 23:08:33|  2   1  97   0   0|  15M 2577k|  16M 7680k|   0     0 |  46k   77k|  50G  125G 2395M 9929M
01-04 23:08:34|  1   0  90   0   0| 478k 2212k|1752k 4507k|   0     0 |  40k   65k|  50G  124G 2395M 9929M
```

从 `dstat -tam` 的输出来看，系统的整体资源使用率较低（CPU 空闲 `idl` 在 90%~98%），但结合之前的 `perf sched latency` 和 `vmstat` 数据，**高调度延迟问题可能源于局部资源争用或内核态瓶颈**。以下是关键分析和排查建议：

---

### **1. 关键指标解读**
#### **(1) CPU 使用率**
- **`usr`（用户态）**：1%~2%，正常。
- **`sys`（内核态）**：0%~1%，无异常。
- **`idl`（空闲）**：90%~98%，**CPU 资源充足**。  
  **矛盾点**：高空闲率与高调度延迟并存，说明问题可能发生在 **内核调度器、锁竞争或中断处理** 等非 CPU 算力层面。

#### **(2) 内存和 I/O**
- **内存**：  
  - `used`：50GB，`free`：125GB，**无内存压力**。  
  - `buf/cach`：约 2.4GB/9.9GB，正常。
- **磁盘 I/O**（`dsk/total`）：  
  - 读/写波动较大（如 29MB/s 读、3.8MB/s 写），但 `wai`（I/O 等待）为 0%，**磁盘无阻塞**。
- **网络**（`net/total`）：  
  - 流量峰值较高（如 47MB/s 接收、16MB/s 发送），可能触发网络中断。

#### **(3) 系统调用和中断**
- **`int`（中断）**：27k~48k 次/秒，**偏高**（正常应 <1k/秒）。  
- **`csw`（上下文切换）**：40k~77k 次/秒，**偏高**。  
  **可能原因**：  
  - 高频网络中断（如 Pod 间通信或 Kube API 调用）。  
  - 锁竞争导致线程频繁切换（如 `kubelet` 或 `containerd` 内部锁）。

---

## perf stat -e 'kvm:*' -a -- sleep 10

```bash
perf stat -e 'kvm:*' -a -- sleep 10
```

```bash
            80,067      kvm:vgic_update_irq_pending      # 虚拟中断控制器（GIC）中断注入次数，反映虚拟设备（如网卡）的中断频率
            40,696      kvm:kvm_trap_enter               # 虚拟机陷入 Hypervisor 的次数（特权指令或异常）
            40,696      kvm:kvm_trap_exit                # 虚拟机从 Hypervisor 返回的次数
            34,476      kvm:kvm_wfx_arm64                # Guest 执行 WFI/WFE 低功耗指令次数，可能引发不必要的 VM-Exit
                 0      kvm:kvm_hvc_arm64                # Hypervisor 调用（HVC）次数，此处为 0 表示无调用
            41,401      kvm:kvm_arm_setup_debug          # 调试寄存器设置次数（如单步调试）
            41,401      kvm:kvm_arm_clear_debug          # 调试寄存器清除次数
            82,802      kvm:kvm_arm_set_dreg32           # 32 位调试寄存器写入次数
                 0      kvm:kvm_arm_set_regset           # 寄存器组操作次数，此处为 0
                 0      kvm:trap_reg                     # 寄存器陷阱次数，此处为 0
               128      kvm:kvm_handle_sys_reg           # 系统寄存器处理次数
               128      kvm:kvm_sys_access               # 系统寄存器访问次数
                 0      kvm:kvm_set_guest_debug          # 设置 Guest 调试次数，此处为 0
            41,401      kvm:kvm_entry                    # 进入虚拟机的次数（VM-Entry）
            41,401      kvm:kvm_exit                     # 退出虚拟机的次数（VM-Exit）
             6,092      kvm:kvm_guest_fault              # Guest 缺页异常次数
                 0      kvm:kvm_access_fault             # 内存访问异常次数，此处为 0
               112      kvm:kvm_irq_line                 # 虚拟中断线触发次数
                 0      kvm:kvm_mmio_emulate             # MMIO 模拟次数，此处为 0
               196      kvm:kvm_unmap_hva_range          # 取消 Guest 内存映射范围操作次数
                 0      kvm:kvm_set_spte_hva             # 设置影子页表项次数，此处为 0
                 0      kvm:kvm_age_hva                  # 内存页老化检查次数，此处为 0
                 0      kvm:kvm_test_age_hva             # 测试内存页老化次数，此处为 0
                 0      kvm:kvm_set_way_flush            # Cache 刷新次数，此处为 0
                 0      kvm:kvm_toggle_cache             # Cache 开关次数，此处为 0
            79,955      kvm:kvm_timer_update_irq         # 虚拟定时器中断触发次数
           667,282      kvm:kvm_get_timer_map            # 获取定时器映射次数，频率极高，可能需优化
            79,594      kvm:kvm_timer_save_state         # 保存定时器状态次数
            79,594      kvm:kvm_timer_restore_state      # 恢复定时器状态次数
                 0      kvm:kvm_timer_hrtimer_expire     # 高精度定时器到期次数，此处为 0
                 0      kvm:kvm_timer_emulate            # 定时器模拟次数，此处为 0
                 0      kvm:kvm_pvsched_kick_vcpu        # 调度器唤醒 vCPU 次数，此处为 0
             5,642      kvm:kvm_userspace_exit           # 退出到用户态（QEMU）处理次数
            34,476      kvm:kvm_vcpu_wakeup              # vCPU 唤醒次数
                 0      kvm:kvm_set_irq                  # 设置虚拟中断次数，此处为 0
                56      kvm:kvm_ack_irq                  # 中断确认次数
             6,078      kvm:kvm_mmio                     # MMIO 访问次数（如 virtio 设备寄存器操作）
                 0      kvm:kvm_fpu                      # FPU 操作次数，此处为 0
                 0      kvm:kvm_age_page                 # 内存页老化次数，此处为 0
            13,989      kvm:kvm_halt_poll_ns             # vCPU 空闲轮询时间（纳秒）
                 0      kvm:kvm_dirty_ring_push          # 脏页环推送次数，此处为 0
                 0      kvm:kvm_dirty_ring_reset         # 脏页环重置次数，此处为 0
                 0      kvm:kvm_dirty_ring_exit          # 脏页环退出次数，此处为 0
```

## perf kvm stat live

```bash
perf kvm stat live

Analyze events for all VMs, all VCPUs:

             VM-EXIT    Samples  Samples%     Time%    Min Time    Max Time         Avg time

                TRAP       2364    96.41%    99.80%      2.25us 3231478.32us  31501.14us ( +-   7.91% )
                 IRQ         88     3.59%     0.20%      0.58us  75989.36us   1680.41us ( +-  70.29% )

Total Samples:2452, Total events handled time:74616563.87us.
```

## cat /prco/interrupts

```bash
IPI0:    1449728    1115266    1148554    1170972    1174974    1225131    1234209    1782661     309684     176214     558563     591291     499808     458382     408056     433070     419174     487123     425272     458853     471224     439823     432672     412559     384178     361491     371766     344458     335931     346742     349235     326175     344445     335024     328566     332298     343638     327689     333321     329433  338813     342158     340669     337395     324956     335545     340273     327280     314348     316573     295516     308667     290575     271820     277494     272491     270324     281824     255949     274963     270378     264259     266274     271223     270870     277730     254338     282620     272842     251769     245818     273469     314448     305791     294721     282902     292532     288343     278390     283118     274767     288082     275545     276124     278328     277555     274271     274951     280005     288190     285244     281818     291724     274545     266916     274161       Rescheduling interrupts
IPI1:     633909     163442      83823      74418      78880      82881      77654      70101      72000      75312      74556     224840     204240     195057     170517     187447     206150     184799     170952     199937     192203     190046     193236     177100     318958     283925     260917     235225     225976     217438     215182     205945     205385     195306     193497     191387     190575     186076     192689     192932  189770     194039     196629     185778     185604     185113     191727     190123     281282     247300     213470     199416     177983     177354     167831     165879     162664     158592     156382     148266     160485     158664     161603     168322     159058     161241     154816     157340     157843     153060     160882     157308     231006     207778     183949     172200     163945     159546     158249     157118     150748     158845     154623     155088     149079     152512     151860     157985     154298     157118     162425     162646     163406     158238     148972     152065       Function call interrupts
```

## /proc/softirqs

```bash
[root@kvm-10e63e2e11 sched]# cat /proc/softirqs
                    CPU0       CPU1       CPU2       CPU3       CPU4       CPU5       CPU6       CPU7       CPU8       CPU9       CPU10      CPU11      CPU12      CPU13      CPU14      CPU15      CPU16      CPU17      CPU18      CPU19      CPU20      CPU21      CPU22      CPU23   CPU24      CPU25      CPU26      CPU27      CPU28      CPU29      CPU30      CPU31      CPU32      CPU33      CPU34      CPU35      CPU36      CPU37      CPU38      CPU39      CPU40      CPU41      CPU42      CPU43      CPU44      CPU45      CPU46      CPU47      CPU48      CPU49      CPU50      CPU51      CPU52      CPU53      CPU54      CPU55      CPU56      CPU57      CPU58      CPU59      CPU60      CPU61      CPU62      CPU63      CPU64      CPU65      CPU66      CPU67      CPU68      CPU69      CPU70      CPU71      CPU72      CPU73      CPU74  CPU75      CPU76      CPU77      CPU78      CPU79      CPU80      CPU81      CPU82      CPU83      CPU84      CPU85      CPU86      CPU87      CPU88      CPU89      CPU90      CPU91      CPU92      CPU93      CPU94      CPU95
          HI:          3          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          2          0          0          0          0          0          0          0      0          0          0          0          0          0          0          0          0          0          0          0          0          1          0          0          0          0          0          0          0          0          0          0          40          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0     0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0
       TIMER:      61963      32296      53299      43466      52370      72007      57481      42226      47769      68079      54721      49674      46536      46930      58072      45314      89191      37815      64884      86577      47175      80946      69274      36842  31660      41008      22703      22063      19134      19272      19094      21899      18400      27169      39347      18078      19927      22708      18678      21883      21231      18633      20430      35441      22210      22912      17631      23017      45452      28201      24809     102649      31948      21009      18877      16780      28223      16618      15671      17469      17431      16711      18521     161222      24226      21887      22485      17863      24055      20723      20721      21761      23021      22393      25592 20767      18393      18606      15891      16569      23184      22151      17494      19743      17852      18450      26767      15546      21673      16697      21552      18923      16881      25624      20035      22547
      NET_TX:          8          4          3         10        756          8        750          2          6         44         11          5          4          7          6         14         10          5         14          6          4          6         10          3      9          3          7          4          2          5          3          2          4          3          6          3          4          0          6          6          3          4         15          2          6          3          3          1          67          4         15          2          1          2          5          6          8         11          4          7          5          2         10          6          5          0          3          3          3          6          3        737          0          2     1          1          8          8          3          3          3        735          2          3        775          3          3          2          3          4          4          1          5          8          5
      NET_RX:     373108      67197    1467851     941915     756695     925196    2068894    2544705     929104    3230100      88674     496823     378406     585269     704904      32411    2420905     733339    2257657    1305192      55337    1510919     666434      40649  21449      18823      17521      18392      18180      17262      16917      16176      16711      16735      17229      16715      16999      16309      16689      16996      15883      16209      18263      17051      16854      16309      17023      17209      50552      12778      11402      11545      14485      11478      12153      10513      10923      11431      10927      31785      13946      11014      11670      10875      11807      10412      10890       9792       9863      10479      10935      10597      19504      17731      17537 16967      16413      17012      16836      16378      16237      16034      16528      16144      15988      16694      17111      16411      16638      16364      15215      15596      15920      16088      16142      15534
       BLOCK:          0          0          3          0          0          0          0          0          0          0          0          0          1          0          0          0          1          0          0          0          2          0        137          0      0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          00          0          0          0          0          0          0          0          0          0          0          0          0        130          0          0          0          0          0          4          0          0          0          0          0          0     0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0
    IRQ_POLL:          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0      0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          00          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0     0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0
     TASKLET:        145         17        191        858        111       1249      15454      37074        268       3161         11       4521        530         54       5935          4       9783          1      19129        299         44      32937       6550          0      0          1          1          0          0          0          0          0          0          2          1          0          0          0          1          0          0          0          1          0          0          0          1          3         601          6          0          0          1          0          0          0          0         32          7         14          0          0          0          6         27         14          0          0          0          0          0          0          1          0     0          0          1          0          0          0          1          2          0          0          0          0          0          0          0          0          0          0          0          0          0
       SCHED:     444257     133281      68994      61976      65789      70661      94576      56100      36423     151347      41421      54053      50934      50694      54160      45690      65403      43040      56375      73996      47343      88322      67248      45953  47661      44037      41588      40661      37853      38619      38249      39773      38175      40869      44617      38259      38851      40660      37883      38540      38438      39110      38238      45117      39191      39963      38173      40197      56103      46348      42071      64257      44841      39249      38275      37654      42589      40669      37780      40008      39015      37726      39365      73234      42068      43725      39834      40787      42966      38479      38338      42582      48028      46502      43739 40435      41182      40289      39374      39011      41553      42913      41340      43421      40992      44026      46290      40224      42608      42201      44381      42554      43008      47543      42875      43624
     HRTIMER:          0          1          1          0          0          1          0         11          0          0          0          0          0          0          0          2          0          2          0          0          0          1          0          0      0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          1          0          0          0          0          0          10          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0     0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0
         RCU:     204016     124068     153752     157630     169350     181927     259948     144236      99283     371779     112979     152152     145016     147123     156942     128531     184110     122809     159523     215168     134689     278200     196978     129598 174999     135511     127924     118566     114551     110474     107720     118337     109801     114604     130121     108494     110086     119098     107257     110518     110418     111412     107799     124752     108129     112062     107941     112723     163900     134116     119165     180809     125511     111608     107553     103897     118394     113081     107495     113454     107581     103311     107258     205257     111401     116550     105865     108317     117306     107537     105004     112184     132684     129477     117670109649     111058     110569     104883     105035     117540     118984     115169     119202     114268     123586     126718     111736     114278     113289     120425     112261     118893     132094     119995     118174
```

1. **NET_RX** (网络接收) 的计数在多个 CPU 上特别高，尤其是 CPU 9（`3230100`）和 CPU 7（`2544705`），说明网络流量主要由这些 CPU 处理。
2. **SCHED** (调度) 在所有 CPU 上的计数较高，说明进程切换较频繁。
3. **TIMER** (定时器) 在所有 CPU 上都很活跃，这与定时器事件的触发相关。
4. **TASKLET** 主要集中在 CPU 6 和 CPU 7，可能与网络或存储驱动相关。
5. **BLOCK** (块设备) 的计数相对较低，说明磁盘 I/O 负载不高。
6. **HI、HRTIMER、IRQ_POLL** 的计数基本为零或很低，说明高优先级中断、定时器触发的软中断和中断轮询都不是系统的主要负载。

```bash
fio --name=test --filename=/root/wujing/testfile --size=1G --direct=1 --rw=randwrite --bs=4k --numjobs=4 --iodepth=32
```

```bash
dd if=/dev/zero of=/dev/sdb4 bs=1M count=1024 oflag=direct
```

![dd_17435652935867](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/dd_17435652935867.png)

```bash
[secure@kvm-10e63e2e11 sa]$ sar -s 14:00:00 -e 14:20:00 -f sa02
Linux 5.10.0-136.12.0.88.ctl3.aarch64 (kvm-10e63e2e11) 	04/02/2025 	_aarch64_	(96 CPU)

02:00:01 PM     CPU     %user     %nice   %system   %iowait    %steal     %idle
02:10:05 PM     all      1.57      0.00      1.58     23.74      0.00     73.12
02:20:00 PM     all      1.29      0.00      0.65      0.00      0.00     98.05
Average:        all      1.43      0.00      1.12     11.94      0.00     85.51
```

```bash
[secure@kvm-10e63e2e11 sa]$ sar -d -p -s 14:00:00 -e 14:20:00 -f sa02 | grep -E "DEV|sdb"
02:00:01 PM       tps     rkB/s     wkB/s     dkB/s   areq-sz    aqu-sz     await     %util DEV
02:10:05 PM     92.36      0.22   2639.90      0.00     28.59      0.01      0.13      9.37 sdb
02:20:00 PM     84.73    118.43   2096.49      0.00     26.14      0.01      0.14      5.28 sdb
Average:        88.57     58.88   2370.23      0.00     27.43      0.01      0.13      7.34 sdb
```

```bash
[secure@kvm-10e63e2e11 sa]$ sar -d -p -s 14:00:00 -e 14:20:00 -f sa02 | grep -E "DEV|nvme"
02:00:01 PM       tps     rkB/s     wkB/s     dkB/s   areq-sz    aqu-sz     await     %util DEV
02:10:05 PM    588.72  43791.51   3128.93      0.00     79.70      0.21      0.35     29.66 nvme0n1
02:10:05 PM      1.07      0.00      4.21      0.00      3.94      0.00      0.06      0.15 nvme1n1
02:10:05 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-region_gw_etcd_lv1
02:10:05 PM      0.99      0.00      3.87      0.00      3.93      0.00      0.03      0.14 local_nvme_vg-region_gw_etcd_lv2
02:10:05 PM      0.08      0.00      0.33      0.00      4.27      0.00      0.00      0.02 local_nvme_vg-mysql_sys
02:10:05 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-mysql_data
02:10:05 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-local_disk
02:20:00 PM     80.75      6.67   3912.14      0.00     48.53      0.01      0.16      3.26 nvme0n1
02:20:00 PM      1.39      0.86      5.39      0.00      4.50      0.00      0.04      0.20 nvme1n1
02:20:00 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-region_gw_etcd_lv1
02:20:00 PM      1.30      0.00      5.08      0.00      3.92      0.00      0.06      0.19 local_nvme_vg-region_gw_etcd_lv2
02:20:00 PM      0.08      0.00      0.31      0.00      3.67      0.00      0.08      0.02 local_nvme_vg-mysql_sys
02:20:00 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-mysql_data
02:20:00 PM      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-local_disk
Average:       336.64  22063.40   3517.60      0.00     75.99      0.11      0.33     16.56 nvme0n1
Average:         1.23      0.43      4.79      0.00      4.25      0.00      0.05      0.18 nvme1n1
Average:         0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-region_gw_etcd_lv1
Average:         1.14      0.00      4.47      0.00      3.93      0.00      0.05      0.16 local_nvme_vg-region_gw_etcd_lv2
Average:         0.08      0.00      0.32      0.00      3.96      0.00      0.04      0.02 local_nvme_vg-mysql_sys
Average:         0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-mysql_data
Average:         0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00 local_nvme_vg-local_disk
```

```bash
[secure@kvm-10e63e2e11 sa]$ sar -d -p -s 14:00:00 -e 14:20:00 -f /var/log/sa/sa02 | awk '/DEV/ || ($9 > 10 && $9 != "await")'
02:00:01 PM       tps     rkB/s     wkB/s     dkB/s   areq-sz    aqu-sz     await     %util DEV
02:10:05 PM      0.44      6.18      2.22      0.00     18.93      1.24   2805.88     31.08 rbd11
02:10:05 PM      0.42      6.18      2.03      0.00     19.52      0.54   1274.58     31.09 rbd13
02:10:05 PM      0.38      5.95      1.81      0.00     20.49      0.60   1577.51     31.05 rbd17
02:10:05 PM      0.42      5.95      2.16      0.00     19.44      1.08   2578.90     31.36 dev252-304
02:10:05 PM      0.46      6.18      2.30      0.00     18.62      1.40   3071.21     29.50 dev252-320
02:10:05 PM      0.38      5.95      1.82      0.00     20.57      0.32    849.13     30.53 dev252-352
02:10:05 PM      0.49      6.41      2.61      0.00     18.61      0.57   1174.83     28.50 dev252-400
02:10:05 PM      0.41      6.18      2.29      0.00     20.89      0.71   1740.73     31.55 dev252-480
02:10:05 PM      0.50      6.18      3.35      0.00     18.87      1.06   2098.72     30.53 dev252-496
02:10:05 PM      0.54      6.18      3.50      0.00     17.82      2.55   4699.43     30.95 dev252-528
02:10:05 PM      0.46      6.18      2.78      0.00     19.39      2.33   5037.73     29.37 dev252-544
02:10:05 PM      0.58      6.18      5.51      0.00     20.01      0.42    720.29     28.18 dev252-816
02:10:05 PM      0.46      6.18      2.67      0.00     19.44      1.30   2847.09     29.32 dev252-848
02:10:05 PM      0.43      6.41      2.52      0.00     20.99      0.94   2207.00     30.08 dev252-880
02:10:05 PM      0.46      6.18      2.39      0.00     18.62      0.64   1385.25     31.93 dev252-912
02:10:05 PM      0.44      6.18      2.58      0.00     19.89      2.40   5449.10     31.43 dev252-960
02:10:05 PM      0.51      6.15      3.76      0.00     19.38      0.53   1039.06     31.88 dev252-1072
02:10:05 PM      0.46      6.18      2.35      0.00     18.54      0.93   2010.86     30.78 dev252-1056
02:10:05 PM      0.48      6.18      2.40      0.00     18.07      0.87   1839.01     31.88 dev252-1120
02:10:05 PM      0.46      6.18      2.42      0.00     18.89      0.38    832.00     31.52 dev252-1216
02:10:05 PM      0.45      6.18      2.39      0.00     19.02      1.95   4326.52     30.13 dev252-1328
02:10:05 PM      0.47      6.18      2.41      0.00     18.47      0.83   1783.90     32.43 dev252-1952
02:10:05 PM      0.45      6.18      2.29      0.00     18.95      0.40    895.28     31.37 dev252-2032
02:10:05 PM      0.46      6.18      2.45      0.00     18.74      0.93   2021.78     30.00 dev252-2048
02:10:05 PM      0.42      6.18      2.40      0.00     20.64      1.04   2510.16     30.87 dev252-560
02:10:05 PM      0.42      6.18      2.31      0.00     20.19      0.89   2126.02     31.64 dev252-576
02:10:05 PM      0.41      6.18      1.90      0.00     19.93      0.71   1743.30     30.85 dev252-592
02:10:05 PM      0.47      6.18      2.37      0.00     18.38      0.83   1781.44     30.70 dev252-656
02:10:05 PM      0.50      6.18      3.95      0.00     20.25      0.47    942.15     31.50 dev252-976
02:10:05 PM      0.42      6.18      1.98      0.00     19.41      0.79   1881.57     30.49 dev252-1008
02:10:05 PM      0.39      5.95      1.92      0.00     19.96      1.27   3225.95     31.83 dev252-1040
02:10:05 PM      0.46      6.18      3.11      0.00     20.40      0.49   1077.40     28.83 dev252-1136
02:10:05 PM      0.40      6.18      1.83      0.00     20.25      0.43   1088.12     32.28 dev252-1296
02:10:05 PM      0.41      6.18      2.01      0.00     20.11      0.60   1470.38     32.45 dev252-1344
02:10:05 PM      0.41      6.18      1.92      0.00     19.72      0.60   1458.92     30.45 dev252-1424
02:10:05 PM      0.44      6.18      2.14      0.00     18.75      0.82   1842.94     32.72 dev252-1504
02:10:05 PM      0.41      6.18      1.89      0.00     19.67      1.11   2697.52     30.45 dev252-1536
02:10:05 PM      0.39      6.18      1.91      0.00     20.61      1.17   2972.74     32.59 dev252-1568
02:10:05 PM      0.41      6.18      2.02      0.00     20.21      1.10   2714.19     30.79 dev252-1616
02:10:05 PM      0.42      6.18      1.96      0.00     19.60      0.57   1370.62     28.31 dev252-1632
02:10:05 PM      0.43      6.18      2.20      0.00     19.70      0.41    965.86     31.04 dev252-1872
02:10:05 PM      0.43      6.18      2.15      0.00     19.28      0.37    866.54     27.40 dev252-1888
02:10:05 PM      0.45      6.41      2.17      0.00     19.20      0.53   1179.37     31.87 dev252-1920
02:10:05 PM      0.42      6.18      2.03      0.00     19.37      0.54   1268.51     31.85 dev252-512
02:10:05 PM      0.39      6.18      2.16      0.00     21.26      0.48   1215.08     33.19 dev252-704
02:10:05 PM      0.51      6.95      4.09      0.00     21.44      0.51    984.95     31.89 dev252-800
02:10:05 PM      0.43      6.18      2.36      0.00     20.06      0.51   1195.97     29.69 dev252-832
02:10:05 PM      0.41      6.18      1.98      0.00     20.04      0.72   1760.10     30.30 dev252-1232
02:10:05 PM      0.41      6.05      2.27      0.00     20.18      0.61   1468.20     30.99 dev252-1248
02:10:05 PM      0.44      6.18      2.22      0.00     18.94      0.43    979.06     29.47 dev252-1360
02:10:05 PM      0.43      6.41      2.08      0.00     19.66      0.47   1088.59     31.65 dev252-1440
02:10:05 PM      0.41      6.18      1.92      0.00     19.96      0.91   2249.93     31.68 dev252-1472
02:10:05 PM      0.42      6.41      1.98      0.00     19.95      0.82   1948.15     28.52 dev252-1680
02:10:05 PM      0.42      6.18      1.95      0.00     19.57      0.68   1632.39     29.70 dev252-1712
02:10:05 PM      0.43      6.18      1.99      0.00     19.21      0.66   1550.49     31.42 dev252-1760
02:10:05 PM      0.41      6.18      1.90      0.00     19.92      1.19   2938.57     31.74 dev252-1840
02:10:05 PM      0.42      6.18      2.01      0.00     19.32      0.49   1162.93     31.09 dev252-2000
Average:       336.64  22063.40   3517.60      0.00     75.99      0.11      0.33     16.56 nvme0n1
Average:       475.63  22023.42   1828.53      0.00     50.15      0.18      0.38     15.19 ceph--1c8590ef--0f72--4bb5--bb5f--79fbdcd078fc-osd--block--d04a90ab--d116--4fc5--b61e--bf30ab5894e3
```

```bash
[secure@kvm-10e63e2e11 sa]$ lsblk -o NAME,MAJ:MIN,TYPE,MOUNTPOINT | grep 252:
rbd0                                                                                                  252:0   disk /var/lib/kubelet/pods/35815a44-8139-4993-82da-f38237704767/volumes/kubernetes.io~csi/pvc-e3850956-fae3-477b-9add-19bba1fd30c2/mount
rbd1                                                                                                  252:16  disk /var/lib/kubelet/pods/35815a44-8139-4993-82da-f38237704767/volumes/kubernetes.io~csi/pvc-d435a31b-4bf0-48bd-81c8-46323d94f73e/mount
rbd2                                                                                                  252:32  disk /var/lib/kubelet/pods/92f494d8-b61d-4e8f-9bc7-0bce9d49cf1f/volumes/kubernetes.io~csi/pvc-8f5a0290-94de-453f-9ec8-c509d9a02e29/mount
rbd3                                                                                                  252:48  disk /var/lib/kubelet/pods/ea0d54db-9d80-457d-a9b0-400963b0f03f/volumes/kubernetes.io~csi/pvc-26804d0f-f2cb-48db-b5a0-61a4b770a1b0/mount
rbd4                                                                                                  252:64  disk /var/lib/kubelet/pods/f104816e-0446-4d59-b5c8-782c04c1874b/volumes/kubernetes.io~csi/pvc-643db8f7-9bf2-4d56-9919-b82585c0685c/mount
rbd5                                                                                                  252:80  disk /var/lib/kubelet/pods/f104816e-0446-4d59-b5c8-782c04c1874b/volumes/kubernetes.io~csi/pvc-63583da4-1481-4e10-8f13-c505ab4949e9/mount
rbd6                                                                                                  252:96  disk /var/lib/kubelet/pods/ea0d54db-9d80-457d-a9b0-400963b0f03f/volumes/kubernetes.io~csi/pvc-1b470483-32bd-4095-a02d-0ce2e3223d09/mount
rbd7                                                                                                  252:112 disk /var/lib/kubelet/pods/92f494d8-b61d-4e8f-9bc7-0bce9d49cf1f/volumes/kubernetes.io~csi/pvc-129ecaca-c5f1-41b5-8ee9-8564c232f306/mount
rbd8                                                                                                  252:128 disk /var/lib/kubelet/pods/81060019-1b4c-49ba-b1b7-d2d353e5d402/volumes/kubernetes.io~csi/pvc-4b27964d-2c5f-4cde-96e4-0af91d04c72a/mount
rbd9                                                                                                  252:144 disk /var/lib/kubelet/pods/947c4f1b-67b2-4ce9-8f93-8505435ffcfe/volumes/kubernetes.io~csi/pvc-5faa2295-c147-429e-9bea-4bc5172e488f/mount
rbd10                                                                                                 252:160 disk /var/lib/kubelet/pods/5a852c2b-3150-4cf9-9535-f289fefad7b1/volumes/kubernetes.io~csi/pvc-8bdad155-863c-4ae8-9a2e-514921fe50df/mount
rbd11                                                                                                 252:176 disk /var/lib/kubelet/pods/5a852c2b-3150-4cf9-9535-f289fefad7b1/volumes/kubernetes.io~csi/pvc-c91abd82-9943-4b4f-a0e5-ef6c7fefa141/mount
rbd12                                                                                                 252:192 disk /var/lib/kubelet/pods/304d6004-43e6-4146-9485-070055276f3c/volumes/kubernetes.io~csi/pvc-0ec9af5d-09c0-4265-aaf9-2f2230e30105/mount
rbd13                                                                                                 252:208 disk /var/lib/kubelet/pods/85d1f86b-ed88-445b-a690-f61f20319d0b/volumes/kubernetes.io~csi/pvc-7e197cd0-d70b-4ed8-8a92-089abd5eb899/mount
rbd14                                                                                                 252:224 disk /var/lib/kubelet/pods/8a650674-3357-45d5-a606-f85104a07031/volumes/kubernetes.io~csi/pvc-7b09cb51-99b6-4a5a-97d9-a6532164354f/mount
rbd15                                                                                                 252:240 disk /var/lib/kubelet/pods/85d1f86b-ed88-445b-a690-f61f20319d0b/volumes/kubernetes.io~csi/pvc-3fa392e8-5b0d-4636-a398-e0b8f5db9af5/mount
rbd16                                                                                                 252:256 disk /var/lib/kubelet/pods/8a650674-3357-45d5-a606-f85104a07031/volumes/kubernetes.io~csi/pvc-7e57d094-09d4-4649-bcb9-2cc3264c680d/mount
rbd17                                                                                                 252:272 disk /var/lib/kubelet/pods/304d6004-43e6-4146-9485-070055276f3c/volumes/kubernetes.io~csi/pvc-fcb837ad-879d-41bd-b97d-0eda25df78d8/mount
```

```bash
[secure@kvm-10e63e2e11 sa]$ sar -u -s 15:50:00 -e 16:20:00 -f /var/log/sa/sa02
Linux 5.10.0-136.12.0.88.ctl3.aarch64 (kvm-10e63e2e11) 	04/02/2025 	_aarch64_	(96 CPU)

03:50:01 PM     CPU     %user     %nice   %system   %iowait    %steal     %idle
04:00:02 PM     all      1.46      0.00      1.47      4.50      0.00     92.57
04:10:02 PM     all      1.12      0.00      1.04      7.64      0.00     90.19
Average:        all      1.29      0.00      1.26      6.07      0.00     91.39
```

```bash
[secure@kvm-10e63e2e11 sa]$ ps -eo pid,ppid,user,state,cmd | grep '^ *[0-9]\+ *[0-9]\+ *[a-z]\+ *[DZ]'
 5881 41444 root     Z [etcdctl] <defunct>
13272 12301 root     Z [rm] <defunct>
[secure@kvm-10e63e2e11 sa]$
[secure@kvm-10e63e2e11 sa]$
[secure@kvm-10e63e2e11 sa]$ ps -eo pid,ppid,user,state,cmd | grep '^ *[0-9]\+ *[0-9]\+ *[a-z]\+ *[DZ]'
 5881 41444 root     Z [etcdctl] <defunct>
13272 12301 root     Z [rm] <defunct>
[secure@kvm-10e63e2e11 sa]$ ps -eo pid,ppid,user,state,cmd | grep '^ *[0-9]\+ *[0-9]\+ *[a-z]\+ *[DZ]'
 5881 41444 root     Z [etcdctl] <defunct>
13272 12301 root     Z [rm] <defunct>
48984 48904 root     D /opt/cni/bin/cilium-cni
[secure@kvm-10e63e2e11 sa]$ ps -eo pid,ppid,user,state,cmd | grep '^ *[0-9]\+ *[0-9]\+ *[a-z]\+ *[DZ]'
 5881 41444 root     Z [etcdctl] <defunct>
13272 12301 root     Z [rm] <defunct>
```
