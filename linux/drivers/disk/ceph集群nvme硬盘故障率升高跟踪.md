# ceph集群nvme硬盘故障率升高跟踪

问题背景：

- 测试环境中三次出现nvme热插拔crash，目前仅提供了一份vmcore。

- 线上环境目前遇到nvme坏盘热插拔问题，担心crash。

## 线上环境-2025年-IT上云4.0-福建福州IT上云7-可用区3 28e254e132e33

关键日志如下：

![17623955283750](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17623955283750.png)

![17623959636836](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17623959636836.png)

![17623959796621](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17623959796621.png)

1. 设备异常：设备之前处于故障状态。
2. 设备重置/重现：系统重新枚举了该设备，内核的标准 nvme 驱动正常启动初始化流程。
3. 驱动绑定冲突：一个自动化的管理脚本干预了这个过程，强行将设备绑定到 uio_pci_generic 驱动上。这个驱动通常用于DPDK、SPDK等用户态IO加速方案，它会阻止内核的标准驱动接管设备。
4. 导致结果：标准 nvme 驱动的初始化流程被中断，导致您之前看到的“驱动绑定失败”和“设备忙（-16）”的错误。

## 测试环境-京津翼公共跳板机-Linux 10e255e251e74

![17624161036586](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17624161036586.png)

反应此问题不是软件或驱动问题，而是一个严重的硬件级错误。
