---
date: 2023/04/21 15:49:27
updated: 2023/04/21 15:49:27
---

```bash
ostree --repo=/deepin/linglong/repo/repo-test refs --delete com.qq.weixin.work.deepin/4.0.0.6007/x86_64 && ostree --repo=/deepin/linglong/repo/repo-test prune --refs-only

ostree --repo=/deepin/linglong/repo/repo-test pull --mirror repo:com.qq.weixin.work.deepin/4.0.0.6007/x86_64
```