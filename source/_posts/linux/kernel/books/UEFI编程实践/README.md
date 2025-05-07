---
date: 2023/10/08 00:14:09
updated: 2023/10/12 22:30:05
---

# UEFI编程实践.pdf

## split cat
### 分割

```bash
split -d -b 10m UEFI编程实践.pdf UEFI编程实践.pdf_
```

### 合并

```bash
cat UEFI编程实践.pdf_* > UEFI编程实践.pdf
```

## rar

### 分卷压缩

```bash
rar a -v10m UEFI编程实践.pdf.rar UEFI编程实践.pdf
```

### 合并解压

```bash
unrar x UEFI编程实践.pdf.part01.rar
```
