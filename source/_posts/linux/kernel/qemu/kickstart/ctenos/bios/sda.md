---
date: 2024/08/19 11:53:58
updated: 2024/08/19 11:53:58
---

# sda

```bash
df -h
文件系统                        容量  已用  可用 已用% 挂载点
/dev/mapper/centos_centos-root   50G  4.9G   46G   10% /
devtmpfs                        3.9G     0  3.9G    0% /dev
tmpfs                           3.9G     0  3.9G    0% /dev/shm
tmpfs                           3.9G   10M  3.9G    1% /run
tmpfs                           3.9G     0  3.9G    0% /sys/fs/cgroup
/dev/sda1                      1014M  179M  836M   18% /boot
/dev/mapper/centos_centos-home   70G   38M   70G    1% /home
tmpfs                           782M  4.0K  782M    1% /run/user/42
tmpfs                           782M   60K  782M    1% /run/user/1000
tmpfs                           782M     0  782M    0% /run/user/0
```

```bash
[root@centos ~]# lsblk
NAME                   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINT
sda                      8:0    0  128G  0 disk
├─sda1                   8:1    0    1G  0 part /boot
└─sda2                   8:2    0  127G  0 part
  ├─centos_centos-root 253:0    0   50G  0 lvm  /
  ├─centos_centos-swap 253:1    0  7.9G  0 lvm  [SWAP]
  └─centos_centos-home 253:2    0 69.1G  0 lvm  /home
```
