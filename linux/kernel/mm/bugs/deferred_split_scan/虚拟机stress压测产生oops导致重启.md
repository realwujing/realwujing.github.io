# 虚拟机stress压测产生oops导致重启

## 问题信息

1. 故障时间：2025-05-11-23:11:38
2. 故障节点：17.27 vm12
3. 故障现象：BUG: unable to handle kernel NULL pointer dereference at 0000000000000009
4. 内核版本：4.19.90-2102.2.0.0068.ctl2.x86_64

## 问题分析

git log openeuler/OLK-5.10 --grep='zap_huge_pmd' > zap_huge_pmd.log

deferred_split_scan

## 问题结果

