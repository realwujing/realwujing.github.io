# Linux 内核补丁管理利器：b4 使用教程

在 Linux 内核开发中，传统的 `git format-patch` 和 `git send-email` 虽然强大，但在管理复杂的补丁系列（Patch Series）、维护封面信（Cover Letter）以及自动收集维护者（Maintainers）方面存在诸多不便。`b4` 是一款专为内核开发者设计的工具，极大地简化了这些流程。

---

## 1. 安装与配置

### 安装
在 Debian/Ubuntu 上可以直接安装：
```bash
sudo apt install b4
```

### 配置 SMTP
`b4` 依赖 `git send-email` 的配置。确保你的 `~/.gitconfig` 中已配置好 SMTP 服务器：
```ini
[sendemail]
    smtpserver = smtp.gmail.com
    smtpserverport = 587
    smtpencryption = tls
    smtpuser = your-email@gmail.com
    smtppass = your-app-password
```

---

## 2. 开启一个新的补丁系列

在你的开发分支上，首先将该分支注册到 `b4` 的管理系统中：

```bash
# 进入内核源码目录并切换到你的开发分支
b4 prep --enroll [fork-point]
```
*   `fork-point`：补丁系列的基准点。可以是上游分支名（如 `origin/master`）、标签（Tag），或者一个**具体的 Commit ID**。`b4` 会以此为起点识别你的所有提交。

**实例演示：**
如果你当前的分支是基于 `v6.6` 内核标签开始开发的：
```bash
# 使用标签作为基准点
b4 prep --enroll v6.6

# 或者使用具体的 Commit ID
b4 prep --enroll 58390c8f50aa
```

此时，`b4` 会在分支的起始点创建一个特殊的“虚拟提交”，用于存储**封面信（Cover Letter）**和元数据。

---

## 3. 编辑封面信

封面信是 Patch Series 的第 0 封邮件，用于解释整个系列的背景和设计。

### 交互式编辑
```bash
b4 prep --edit-cover
```
这会调用你的默认编辑器。注意：封面信下方包含 `--- b4-submit-tracking ---` 元数据，**千万不要删除它**。

### 非交互式更新（进阶）
如果你想在 VS Code 等编辑器中编辑内容，可以通过手动更新 commit 的方式：
1. 提取内容到文件：`git log --format=%B -n 1 [cover-commit-id] > cover.txt`
2. 编辑 `cover.txt`。
3. 创建新提交并重基：
   ```bash
   NEW_COVER=$(git commit-tree [cover-id]^{tree} -p [cover-id]^ -F cover.txt)
   git rebase --onto $NEW_COVER [cover-id] [branch-name]
   ```

**实例演示：**
假设当前封面信 commit ID 为 `a1b2c3d`，分支名为 `feature-dhei`：
```bash
# 1. 导出文字
git log --format=%B -n 1 a1b2c3d > cover.txt

# 2. (在 VS Code 中修改 cover.txt 内容)

# 3. 注入新内容并重挂载系列
# 这里 a1b2c3d^ 表示封面信的父节点（即基准点）
NEW_COVER=$(git commit-tree a1b2c3d^{tree} -p a1b2c3d^ -F cover.txt)
git rebase --onto $NEW_COVER a1b2c3d feature-dhei
```

---

## 4. 自动收集维护者 (To/Cc)

这是 `b4` 最强大的功能之一。它会自动运行 `get_maintainer.pl` 扫描你系列中每个 patch 修改的文件，并将相关维护者合并到封面信中。

```bash
b4 prep --auto-to-cc
```

执行后，封面信会增加大量的 `To:` 和 `Cc:` 字段。

---

## 5. 导出与预览

### 导出所有补丁
将系列转换成 `.eml` 文件保存到特定目录：
```bash
b4 send -o ./patches_v1 --no-sign
```

### 发送预览（镜像发送）
在正式提交给上游前，强烈建议先**发给自己**检查一遍：
```bash
b4 send --reflect --no-sign
```
*   `--reflect`：邮件只会发到你的发件地址，不会发给维护者。

---

## 6. 正式提交给上游

一切准备就绪后，临门一脚：
```bash
b4 send --no-sign
```
*   `b4` 会正式向所有的 `To` 和 `Cc` 地址投递邮件。
*   发送成功后，`b4` 会自动将本地版本标记为 v1，并准备好 v2 的开发环境（包括更新元数据中的 `revision`）。

## 7. 版本迭代 (v1 -> v2)

当收到社区反馈需要修改代码时：
1. 直接在分支上修改代码并交互式重基：`git rebase -i`。
2. 更新封面信中的 `Changelog`。`b4` 会在元数据中追踪前序版本的 Lore 链接，平衡上下文连贯性。
3. 再次运行 `b4 send`：
   - **自动升级版本**：它会自动带上 `[PATCH v2]` 前缀。
   - **开启新线程（推荐）**：默认情况下，`b4` 会为新版本开启一个全新的邮件线程，避免讨论树过深。这是 Linus 和大多数维护者（如 Greg KH）推荐的做法。

## 8. 接管现有的 Patch 系列（例如从 v8 开始）

如果你之前的 v1-v7 是通过 `git send-email` 手动发送的，现在想改用 `b4` 发送 v8，可以按照以下步骤“接管”：

1. **注册分支**：
   ```bash
   b4 prep --enroll [fork-point]
   ```
2. **手动强制修改版本号**：
   运行 `b4 prep --edit-cover`，在底部的 JSON 元数据中，将 `revision` 改为 `8`：
   ```json
   "series": {
     "revision": 8,
     "change-id": "...",
     "prefixes": []
   }
   ```
3. **关联前序版本（可选）**：
   如果你想让 `b4` 知道 v7 的存在（以便在封面信生成 Lore 链接），可以在 JSON 中手动添加 `history` 记录（虽然略显繁琐），或者直接在封面信正文中手动贴上 v7 的 Lore 链接。

**实例演示：**
假设你的 Patch 系列已经手动发到了 v7，现在需要在 `feature-v8` 分支上开始 v8 的开发：
```bash
# 1. 注册并初始化 b4 追踪（假设基准是 v6.6 标签）
b4 prep --enroll v6.6

# 2. 修改版本号
# 执行 b4 prep --edit-cover，在编辑器中找到 JSON 元数据部分：
# 将 "revision": 1 修改为 "revision": 8

# 3. (建议) 在封面信正文中手动注明前序版本的 Lore 链接
# v7: https://lore.kernel.org/all/20260120-patch-v7@gmail.com/
```

## 9. 单补丁处理（不需要封面信时）

对于只有 1 个 Patch 的任务，通常不需要单独的第 0 封封面信。

- **b4 的行为**：如果你在 `b4 prep --enroll` 后的分支上只有一个功能提交，执行 `b4 send` 时，它会识别出这是一个单补丁系列，并自动**忽略封面信**。
- **元数据依然重要**：底部的 `--- b4-submit-tracking ---` 依然存在于那个“虚拟提交”中，用于追踪版本号和 Change-ID，但它本身不会作为一个独立的邮件发出。
- **如何记录版本变化 (Changelog)**：即使没有封面信邮件，你依然可以运行 `b4 prep --edit-cover` 在“虚拟封面信”中编写版本变化说明。当发送单补丁时，`b4` 会非常聪明地将这些说明自动插入到 Patch 的 `---` 分割线下方。

**实例演示：**
如果你只想提交一个简单的拼写修复且不需要封面信：
```bash
# 1. 注册基准点
b4 prep --enroll origin/master

# 2. 正常提交你的修复补丁
git commit -m "docs: fix typo in isolation.c"

# 3. 自动收集补丁对应的维护者
b4 prep --auto-to-cc

# 4. 发送预览（仅发给自己检查）
b4 send --reflect --no-sign

# 5. 正式发送（b4 会识别为单补丁，不生成 0/N 封面信邮件）
b4 send --no-sign
```

**操作步骤（以 v1 迭代到 v2 为例）：**

1.  **修改代码**：
    在分支上直接修改代码并提交，或者使用 `git commit --amend` / `git rebase -i` 确保你的提交是最新的。
2.  **打开“虚拟封面信”**：
    ```bash
    b4 prep --edit-cover
    ```
3.  **编写 Changelog**：
    当你运行 `b4 prep --edit-cover` 时，编辑器会打开一个临时文件。
    **请直接从该文件的第一行开始编写你的版本说明**。由于是单补丁，这部分内容在发送时会被 `b4` 自动插入到补丁邮件的 `---` 分割线下方。

    **编辑器中的内容应如下排列：**
    ```text
    v2:
      - Fixed a secondary typo in the same file found during review.
      - Updated the description to be more concise.
      - Link to v1: https://lore.kernel.org/all/xxxx@gmail.com/

    --- b4-submit-tracking ---
    {
      "series": {
        "revision": 2,
        ...
      }
    }
    ```
    *注意：Changelog 必须写在 `--- b4-submit-tracking ---` 分割线**之上**。*
4.  **自动更新维护者（可选）**：
    如果修改了文件导致维护者变化，运行 `b4 prep --auto-to-cc`。
5.  **发送预览**：
    发送给真实维护者前，先运行 `b4 send --reflect --no-sign` 发给自己检查一遍。
6.  **正式发送单补丁**：
    ```bash
    b4 send --no-sign
    ```

### 如何确认当前已发送到哪个版本及状态？

在长期维护一个补丁系列时，确认上游已经收到哪个版本及状态非常重要。`b4` 会区分**版本号 (Revision)** 和 **前缀 (Prefix, 如 RFC)**：

1.  **本地查看**：
    运行 `b4 prep --show-revision`。
    **该输出会列出历史已发送的具体版本和链接**：
    ```text
    v2       <-- 当前正在准备的版本（下一次发送将使用 v2）
    ---
      v1: https://lore.kernel.org/r/...
    ```
2.  **查看元数据 (JSON)**：
    执行 `b4 prep --edit-cover`，在底部的 JSON 中查看 `prefixes`：
    - 如果 `"prefixes": ["RFC"]`，则生成的邮件主题会是 `[PATCH RFC 00/XX]`（注意 `RFC` 会紧跟在 `PATCH` 后面）。
    - **转正**：准备 v2 时，只需在 JSON 中将 `"RFC"` 从 `prefixes` 数组中删掉，发送时主题就会变为 `[PATCH v2 00/XX]`。
3.  **最终确认 (Lore)**：
    直接点击 `show-revision` 输出链接。你会发现你刚才发出的真实主题是：
    `Subject: [PATCH RFC 00/12] Implementation of Dynamic Housekeeping & Enhanced Isolation (DHEI)`

---

**单补丁 Changelog 在邮件中呈现的样子：**
如果你按照上述步骤在 `b4 prep --edit-cover` 中写了 v2 的说明，发出的补丁邮件将包含完整的 `Subject`、正文、`Signed-off-by`、Changelog、`diffstat` 和代码变更：

```text
From: Your Name <your-email@gmail.com>
Subject: [PATCH v2] docs: fix typo in isolation.c
Date: Fri, 06 Feb 2026 10:00:00 +0800
Message-Id: <20260206-fix-typo-v2-1-a1b2c3d4e5f6@gmail.com>

Correct the spelling of 'housekeeping' in the comments of isolation.c.

Signed-off-by: Your Name <your-email@gmail.com>
---
v2:
 - Fixed a secondary typo in the same file found during review.
 - Updated the description to be more concise.
 - Link to v1: https://lore.kernel.org/all/20260205-fix-typo-v1-1-xxyyzz@gmail.com/

 drivers/base/isolation.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)

diff --git a/drivers/base/isolation.c b/drivers/base/isolation.c
index 8168739aaf9a..c9d10d385da8 100644
--- a/drivers/base/isolation.c
+++ b/drivers/base/isolation.c
@@ -10,7 +10,7 @@
  * This file implements the core logic for dynamic housekeeping.
  */
 
-/* This is a typing eror */
+/* This is a typing error */
 int isolation_init(void)
 {
 	return 0;
-- 
2.43.0
```

---

## 10. 重发当前版本 (Resend)

有时候你可能已经发出了 v1，但突然发现漏掉了一个重要的邮件列表，或者因为网络问题发送失败。此时你并不想升级到 v2，而是想**原封不动地重发 v1**。

- **操作命令**：
  ```bash
  b4 send --resend --no-sign
  ```
- **b4 的行为**：
  - 它不会触发版本升级（revision 保持不变）。
  - 它会保持相同的 `Change-Id`。
  - 主题中通常会标记为 `[PATCH v1 RESEND]`。
- **适用场景**：
  - 补充抄送（Cc）给之前漏掉的维护者。
  - 修复之前由于网关拒信导致的投递失败。

---

## 常用 Tips
- **查看状态**：`b4 prep --show-revision`。
- **添加 RFC 前缀**：在封面信底部的 `--- b4-submit-tracking ---` 元数据中，将 `"RFC"` 添加到 `prefixes` 数组中。
  **示例：**
  ```json
  --- b4-submit-tracking ---
  {
    "series": {
      "revision": 1,
      "change-id": "20260206-feature-dynamic-isolcpus-dhei",
      "prefixes": ["RFC"]
    }
  }
  ```
- **关于嵌套（Threading）**：
  - **系列内**：Patch 1-N 永远会挂在封面信下方，形成一个清晰的整体。
  - **版本间**：如果你确实需要将 v2 回复给 v1（虽然不推荐），可以使用 `b4 send --in-reply-to [v1-msg-id]`。
- **清理重试**：如果 `git-filter-repo` 报错，可以尝试 `rm -rf .git/filter-repo`。
