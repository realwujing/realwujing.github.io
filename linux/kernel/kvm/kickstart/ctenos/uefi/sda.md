# sda

```bash
[root@centos ~]# df -h
文件系统                        容量  已用  可用 已用% 挂载点
/dev/mapper/centos_centos-root   50G  4.9G   46G   10% /
devtmpfs                        3.8G     0  3.8G    0% /dev
tmpfs                           3.9G     0  3.9G    0% /dev/shm
tmpfs                           3.9G   10M  3.8G    1% /run
tmpfs                           3.9G     0  3.9G    0% /sys/fs/cgroup
/dev/sda2                      1014M  163M  852M   17% /boot
/dev/sda1                       200M   12M  189M    6% /boot/efi
/dev/mapper/centos_centos-home   69G   39M   69G    1% /home
tmpfs                           780M  4.0K  780M    1% /run/user/42
tmpfs                           780M   60K  780M    1% /run/user/1000
tmpfs                           780M     0  780M    0% /run/user/0
```

```bash
[root@centos ~]# lsblk
NAME                   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
sda                      8:0    0   128G  0 disk
├─sda1                   8:1    0   200M  0 part /boot/efi
├─sda2                   8:2    0     1G  0 part /boot
└─sda3                   8:3    0 126.8G  0 part
  ├─centos_centos-root 253:0    0    50G  0 lvm  /
  ├─centos_centos-swap 253:1    0   7.9G  0 lvm  [SWAP]
  └─centos_centos-home 253:2    0  68.9G  0 lvm  /home
```
