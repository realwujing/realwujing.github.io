---
title: '虚拟机stress压测产生oops导致重启'
date: '2025/06/09 20:54:41'
updated: '2025/06/18 09:51:44'
---

# 虚拟机stress压测产生oops导致重启

## 问题信息

1. 故障时间：2025-05-11-23:11:38
2. 故障节点：17.27 vm12
3. 故障现象：BUG: unable to handle kernel NULL pointer dereference at 0000000000000009
4. 内核版本：4.19.90-2102.2.0.0068.ctl2.x86_64

## 问题分析

```bash
git log openeuler/OLK-5.10 --grep='zap_huge_pmd' > zap_huge_pmd.log
```

```bash
deferred_split_scan
```

```bash
root@wujing:~/code/linux# git branch --contains c5e3a0eda5fb
  ctkernel-4.19/publish-ALL
  ctkernel-lts-4.19/develop
  openeuler-4-19/master
  openeuler-4-19/tag/release-0062.y
* openeuler-4-19/tag/release-0068
  openeuler-4-19/yuanql9/bugfix-pr-assert_list_leaf_cfs_rq
  openeuler/openEuler-1.0-LTS
```

## 问题结果

