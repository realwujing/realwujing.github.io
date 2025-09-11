---
date: 2023/12/05 14:50:57
updated: 2023/12/05 14:50:57
---

# 基于主线linux4.19合入飞腾补丁

飞腾Linux内核补丁基于主线 tag v4.19:

```bash
git clone https://github.com/torvalds/linux.git
git check -b v4.19 v4.19
```

检查patch:

```bash
git apply --stat ../飞腾Linux内核补丁4.19.11发行/patch-phytium-4.19.11
```

检查patch能否应用成功:

```bash
git apply --check ../飞腾Linux内核补丁4.19.11发行/patch-phytium-4.19.11
```

打补丁,由于此补丁没有commit信息,故git am命令无法使用,建议使用git
apply或patch命令:

```bash
git apply ../飞腾Linux内核补丁4.19.11发行/patch-phytium-4.19.11

patch -p1 < ../飞腾Linux内核补丁4.19.11发行/patch-phytium-4.19.11
```
