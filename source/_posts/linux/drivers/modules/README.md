---
title: 'Linux内核模块开发'
date: '2025/11/21 18:06:27'
updated: '2025/11/21 18:06:27'
---

# Linux内核模块开发

## 编译

```bash
make
```

## 加载

```bash
sudo insmod a/a.ko
sudo insmod b/b.ko -f // 在debian10上不加-f报错Invalid parameters
```

日志：

```bash
sudo dmesg | tail
```

## 卸载

```bash
sudo rmmod b/b.ko
sudo rmmod a/a.ko
```
