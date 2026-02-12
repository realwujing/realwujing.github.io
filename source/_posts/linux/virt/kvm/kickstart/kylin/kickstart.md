---
title: '麒麟Kickstart自动化安装'
date: '2025/11/25 17:15:02'
updated: '2025/11/25 17:15:02'
---

# 麒麟Kickstart自动化安装

```bash
virt-install --virt-type kvm \
--initrd-inject=/mnt/sdd1/yql/ks1.cfg \
--name wujing-zy-xfs \
--memory 32768 \
--vcpus=64 \
--location /mnt/sdd1/yql/ctyunos-2-24.07-240818-test-aarch64-dvd.iso \
--disk path=/mnt/sdd1/yql/wujing-zy-xfs.qcow2,size=128,device=disk,bus=virtio \
--network network=default \
--os-type=linux \
--os-variant=centos7.0 \
--graphics none \
--extra-args="console=ttyS0 inst.ks=file:/ks1.cfg"
```
