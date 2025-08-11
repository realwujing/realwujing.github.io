---
date: 2024/06/06 14:20:45
updated: 2024/06/06 14:20:45
---

# modules

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
