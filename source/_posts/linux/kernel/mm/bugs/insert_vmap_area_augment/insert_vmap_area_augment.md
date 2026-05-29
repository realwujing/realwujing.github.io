---
title: 'vmalloc_init初始化失败导致phtyium无法启动'
date: '2025/12/23 15:24:29'
updated: '2025/12/23 15:24:29'
---

# vmalloc_init初始化失败导致phtyium无法启动

## 问题描述

1. 故障时间：2025-12-22 15:15:54
2. 故障节点：21.98.142.12
3. 故障现象：Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
4. 操作系统：6.6.0-0008.rc1.ct14.aarch64 #1
5. cpu: phytium

## 问题分析

![17664686069629](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17664686069629.png)

```c
find_va_links
insert_vmap_area_augment
vmap_init_free_space
vmalloc_init    // mm/vmalloc.c:6094
```

![17664721036353](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17664721036353.png)

![17664690112706](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17664690112706.png)

## 解决方案

![17664066429551](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/17664066429551.png)

硬件不同，占用了更多的地址空间。
