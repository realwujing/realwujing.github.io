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

## 2. 开启一个新的补丁系列 (Enrollment)

在你的开发分支上，首先将该分支注册到 `b4` 的管理系统中。这一步被称为 **Enroll**，它是 `b4` 接管分支的起点。

### 核心命令
```bash
b4 prep --enroll [fork-point]
```
*   `fork-point`：补丁系列的基准点。可以是上游分支名（如 `origin/master`）、标签（Tag），或者一个**具体的 Commit ID**。

### 它在底层做了什么？
1.  **界定范围**：告诉 `b4` 你的补丁系列从哪个 commit 开始。
2.  **插入“元数据提交”**：这是最关键的操作。`b4` 会在基准点处插件一个空的“虚拟提交”。这个提交不包含代码改动，专门用于存放**封面信（Cover Letter）**和**控制 JSON**（如版本号、Change-ID）。
3.  **分配身份证 (Change-Id)**：生成一个唯一的 ID 用于跨版本追踪。

**关键点：** 如果你提供具体的 Commit ID，它必须是你想要提交的第一个补丁的**上一个 commit（父节点）**。

**实例演示：**
假设你的分支上只有一个提交 `f34ecc590516`，它是基于 `b54345928fa1` 开发的：
```bash
# 1. 查看 log，确认基准点（父节点）
git log --oneline -2
# f34ecc590516 (HEAD) mm/page_alloc: boost watermarks...
# b54345928fa1 Merge tag 'gfs2-for-6.19-rc6'...

# 2. 对父节点执行 enroll
b4 prep --enroll b54345928fa1

# 3. 再次查看 log，你会发现 b4 插入了一个“虚拟封面信”提交
git log --oneline -3
# 2f5adb4a45f4 (HEAD) mm/page_alloc: boost watermarks... (你的补丁被重定位了)
# 36bc6e58305b EDITME: cover title... (b4 插入的封面信)
# b54345928fa1 Merge tag 'gfs2-for-6.19-rc6'... (原始基准点)
```

---

## 3. 编辑封面信

封面信是 Patch Series 的第 0 封邮件，用于解释整个系列的背景和设计。

### 交互式编辑
```bash
b4 prep --edit-cover
```
这会调用你的默认编辑器。
**注意：** 在编辑器中你只能看到和编辑标题、正文和 `Signed-off-by`。`b4` 会自动隐藏底部的 JSON 元数据以防误删。

---

## 4. 调整版本号与前缀

如果你需要修改版本号（例如接管系列）或添加 `RFC` 等前缀，**推荐直接使用命令行参数**，而不是手动去翻 commit 里的 JSON。

### 修改版本号 (Revision)
如果你想把当前系列的版本强制改为 v8：
```bash
b4 prep --force-revision 8
```

### 设置前缀 (Prefixes)
如果你想在 `[PATCH]` 中加入 `RFC`：
```bash
# 设置为 [PATCH RFC]
b4 prep --set-prefixes RFC

# 如果要设置多个，比如 [PATCH RFC v2] (v2 建议由 revision 控制，这里仅演示多个前缀)
b4 prep --set-prefixes RFC draft
```

### 确认修改结果
运行以下命令查看当前状态：
```bash
b4 prep --show-info
```

---

## 5. 自动收集维护者 (To/Cc)

这是 `b4` 最强大的功能之一。它会自动运行 `get_maintainer.pl` 扫描你系列中每个 patch 修改的文件，并将相关维护者合并到封面信中。

```bash
b4 prep --auto-to-cc
```

执行后，封面信会增加大量的 `To:` 和 `Cc:` 字段。

### 手动添加抄送人 (Manual CC)
如果你想额外手动添加某个人（比如你的同事或领导），有几种方式：

1.  **方式 A：直接修改封面信（最推荐）**：
    运行 `b4 prep --edit-cover`，在生成的 `To:` 或 `Cc:` 列表下方，按同样的格式手动添加：
    ```text
    Cc: Colleague Name <colleague@example.com>
    ```
    *注意：如果是单补丁系列（没有封面信），手动添加的 `Cc` 会被 `b4` 自动移动到补丁的 `---` 分割线之后。*

2.  **方式 B：在代码 Commit 信息中添加**：
    在你的补丁 `Signed-off-by` 附近添加 `Cc: Name <email>`，`b4` 在发送时会自动识别并加入到邮件头中。

3.  **方式 C：发送时临时指定**：
    如果你只想在发送这一瞬间临时加人，可以使用命令行：
    ```bash
    b4 send --cc colleague@example.com --no-sign
    ```

---

## 6. 导出与预览

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

## 7. 正式提交给上游

一切准备就绪后，临门一脚：
```bash
b4 send --no-sign
```
*   `b4` 会正式向所有的 `To` 和 `Cc` 地址投递邮件。
*   发送成功后，`b4` 会自动将本地版本标记为 v1，并准备好 v2 的开发环境（包括更新元数据中的 `revision`）。

## 8. 版本迭代 (v1 -> v2)

当收到社区反馈需要修改代码时：
1. 直接在分支上修改代码并交互式重基：`git rebase -i`。
2. 更新封面信中的 `Changelog`。`b4` 会在元数据中追踪前序版本的 Lore 链接，平衡上下文连贯性。
3. 再次运行 `b4 send`：
   - **自动升级版本**：它会自动带上 `[PATCH v2]` 前缀。
   - **开启新线程（推荐）**：默认情况下，`b4` 会为新版本开启一个全新的邮件线程，避免讨论树过深。这是 Linus 和大多数维护者（如 Greg KH）推荐的做法。

## 9. 接管现有的 Patch 系列（例如从 v8 开始）

如果你之前的 v1-v7 是通过 `git send-email` 手动发送的，现在想改用 `b4` 发送 v8，可以按照以下步骤“接管”：

1. **注册分支**：
   ```bash
   b4 prep --enroll [fork-point]
   ```
2. **手动强制修改版本号**：
   使用 `b4` 专门的命令将版本号设为 8：
   ```bash
   b4 prep --force-revision 8
   ```
3. **关联前序版本（可选）**：
   如果你想让 `b4` 知道 v7 的存在（以便在封面信生成 Lore 链接），可以直接在封面信正文中手动贴上 v7 的 Lore 链接。

**实例演示：**
假设你的 Patch 系列已经手动发到了 v7，现在需要在 `feature-v8` 分支上开始 v8 的开发：
```bash
# 1. 注册并初始化 b4 追踪（假设基准是 v6.6 标签）
b4 prep --enroll v6.6

# 2. 修改版本号
# 执行 b4 prep --force-revision 8
# 将 "revision": 1 修改为 "revision": 8

# 3. (建议) 在封面信正文中手动注明前序版本的 Lore 链接
# v7: https://lore.kernel.org/all/20260120-patch-v7@gmail.com/
```

## 10. 单补丁处理（不需要封面信时）

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

## 11. 重发当前版本 (Resend)

在一个标准的 `b4` 工作流中，重发的逻辑是基于 **Git Tag** 的快照机制。

### 标准 Resend 流程
1.  **正式发送 (v1)**：
    运行 `b4 send --no-sign`。发送成功后，`b4` 会自动在本地打一个记录标签，格式如：`sent/20260126-subject-v1`。
2.  **发现问题**：
    你发现 v1 少抄送了一个直属领导，或者某个邮件列表没发进去。
3.  **执行重发**：
    运行 `b4 send --resend --no-sign`。
4.  **b4 的行为**：
    *   **回溯快照**：它不会读取你当前分支的 HEAD（因为你可能已经开始改 v2 的逻辑了），而是去寻找最近的那个 `sent/...-v1` 标签。
    *   **生成补丁**：基于标签里的快照重新生成补丁。
    *   **添加标记**：在主题中自动加入 `RESEND` 字样，如 `[PATCH v1 RESEND]`。

### 为什么有时会失败？
- **没有发送记录**：如果你之前的 v1 是用 `git send-email` 发的，`b4` 找不到 `sent/` 标签。
- **已被清理**：运行了 `b4 prep --cleanup`，该命令会抹除所有历史发送记录。

### 进阶：手动添加 RESEND 标签（及如何取消）
如果你无法使用标准的 `--resend`（例如接管了外部系列，或者清理了 Tag），可以直接利用前缀功能。

1.  **添加 RESEND 前缀**：
    ```bash
    b4 prep --set-prefixes RESEND
    ```
    *效果：生成的补丁主题会变为 `[PATCH RESEND v8 0/1] ...`。*

2.  **发送后取消 RESEND 标记**：
    由于 `b4` 会持久化存储前缀，一旦发送完成准备下一版（v9）时，**必须手动清理**，否则下一版还会带着 `RESEND`。
    ```bash
    # 清空所有自定义前缀
    b4 prep --set-prefixes ""

    # 或者恢复为 RFC
    b4 prep --set-prefixes RFC
    ```

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

## 12. 取消 b4 托管与清理

当你完成了一个系列的开发并已被合并，或者单纯想让分支重回“普通状态”时，需要进行清理。

### 彻底删除分支与标签 (Cleanup)
**警告：此操作会直接删除本地分支！** 仅当你确信该系列已经合入上游，或者你打算彻底放弃该分支时才使用。它会移除开发分支以及所有由 `b4` 生成的 `sent/` 标签。
```bash
b4 prep --cleanup [branch-name]
```
**注意**：
- 你不能在当前分支执行该命令（会报错 `is currently checked out`）。
- **误删恢复**：如果不小心删错了，可以通过 `git reflog` 找回最后的 Commit ID，然后用 `git branch <name> <commit-id>` 恢复。

### 仅取消托管（恢复为普通分支）
如果你不想删除分支，只想去掉 `b4` 插入的那个“虚拟封面信”提交，让它变回普通分支：
1.  **确定基准点 (Fork-point)**：
    运行 `b4 prep --show-info` 查看 `base-commit`。
2.  **手动删除封面信提交**：
    通常 `b4` 的封面信提交就是在基准点之上的第一个 commit。直接使用交互式重基删除它即可：
    ```bash
    git rebase --onto [base-commit] [cover-letter-commit] [branch-name]
    ```
    *例如：*
    ```bash
    git rebase --onto b54345928fa1 2f5aff8d7919 wujing/mm/page_alloc-v8
    ```
3.  **验证结果**：
    运行 `b4 prep --show-info`，如果显示 `CRITICAL: This is not a prep-managed branch.`，说明已成功取消托管。
4.  **清理 Change-ID（建议）**：
    如果你在 Commit 正文中也插入了 `change-id` 字段，记得一并删掉。

---

## 常用 Tips
- **查看状态**：`b4 prep --show-revision`。
- **修改版本号**：`b4 prep --force-revision N`。
- **添加 RFC 前缀**：`b4 prep --set-prefixes RFC`。
- **关于嵌套（Threading）**：
  - **系列内**：Patch 1-N 永远会挂在封面信下方，形成一个清晰的整体。
  - **版本间**：如果你确实需要将 v2 回复给 v1（虽然不推荐），可以使用 `b4 send --in-reply-to [v1-msg-id]`。
- **清理重试**：如果 `git-filter-repo` 报错，可以尝试 `rm -rf .git/filter-repo`。
---

## 12. 常见问题排查 (Troubleshooting)

### JSON 报错：`json.decoder.JSONDecodeError: Extra data`
- **现象**：运行 `b4 prep --show-info` 或其他命令时崩溃。
- **原因**：手动编辑封面信时，不小心在底部的 `--- b4-submit-tracking ---` 块中多打了括号 `}` 或引入了非法字符。
- **解决方法**：
  1. 运行 `git log -1` 查看封面信提交。
  2. 使用 `git commit --amend` 再次编辑该提交。
  3. 确保 JSON 块结构严谨（大括号成对），且末尾没有多余内容。

### 命令报错：`CRITICAL: ... is currently checked out`
- **原因**：试图对当前所在的分支执行 `--cleanup`。
- **解决方法**：请先切换到其他分支（如 `git checkout master`），然后再执行 `b4 prep --cleanup <target-branch>`。
