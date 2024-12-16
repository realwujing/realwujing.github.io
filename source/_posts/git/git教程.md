---
date: 2023/04/21 15:49:27
updated: 2024/11/04 18:22:58
---

# git教程

## 密码

- [GitLab使用手册配置Git永久记住密码，免去重复输入的烦恼 - 心向阳 - 博客园 (cnblogs.com)](https://www.cnblogs.com/qdlhj/p/13215158.html)
- [向gitlab提交代码时记住用户名和密码_胡桃夹夹子的博客-CSDN博客_gitlab记住密码](https://blog.csdn.net/weixin_42050406/article/details/115251109)

- [<font color=Red>为git设置代理解决远程仓库无法连接问题</font>](https://www.donnadie.top/set-git-proxy)
- [github配置了公钥依旧提示git@github.com‘s password: Permission denied, please try again.的解决办法](https://blog.csdn.net/yuzhiqiang_1993/article/details/127032178)
- [Git报错：git@github.com: Permission denied (publickey)](https://blog.csdn.net/syzdev/article/details/126921031)

### Github.token

- [<font color=Red>git client 配置以及连接GitHub、Bitbucket_purplego的博客-CSDN博客</font>](https://blog.csdn.net/purplego/article/details/78657521)
- [【VSCode 笔记】Git： Host key verification failed](https://www.wdssmq.com/post/20201216004.html)
- [[ github ] github clone private repo克隆私有项目详细-简书(jianshu.com)](https://www.jianshu.com/p/0503722f69af)
- [git client 配置以及连接GitHub、Bitbucket](https://blog.csdn.net/purplego/article/details/78657521)
- [github项目如何快速git clone https](https://www.jianshu.com/p/affe1af6781c)

## Git 仓库配置用户和邮箱

### 特定仓库

要为一个特定的 Git 仓库配置用户和邮箱，可以使用以下命令：

1. **进入你要配置的 Git 仓库目录**：

   ```bash
   cd /path/to/your/repo
   ```

2. **配置用户名称**：

   ```bash
   git config user.name "Your Name"
   ```

3. **配置用户邮箱**：

   ```bash
   git config user.email "your.email@example.com"
   ```

可以通过查看 `.git/config` 文件来确认配置是否已被成功写入：

```bash
cat .git/config
```

在文件中，你应该能看到类似以下的配置：

```ini
[user]
    name = Your Name
    email = your.email@example.com
```

### 全局配置用户和邮箱

这表示 `user.name` 和 `user.email` 已经被成功配置到该仓库的本地配置文件中。

这些配置只会影响当前仓库，不会影响全局配置。如果你需要配置全局的用户和邮箱，可以使用 `--global` 选项：

```bash
git config --global user.name "Your Global Name"
git config --global user.email "your.global.email@example.com"
```

全局配置的信息会存储在用户主目录下的 Git 配置文件中，具体路径是：

- **`~/.gitconfig`** 或 **`~/.config/git/config`** 文件：这个文件包含了你为所有 Git 仓库配置的全局设置。

当你使用 `git config --global user.name "Your Name"` 或 `git config --global user.email "your.email@example.com"` 时，这些配置信息会被写入到 `~/.gitconfig` 文件中。

你可以通过以下命令查看全局配置文件内容：

```bash
cat ~/.gitconfig
```

这个文件中的配置对系统中所有的 Git 仓库都有效。

## 命令

- [<font color=Red>Git 不要只会 pull 和 push，试试这 5 条提高效率的命令</font>](https://mp.weixin.qq.com/s?__biz=MzU3NTgyODQ1Nw==&mid=2247522045&idx=2&sn=f97685dcac7470d9b8fc1990aba7a7a8&chksm=fd1fee7bca68676db0e438c67294cf0975958f9c1fbf04caeb25d3cafd5b64ac18f4d1003b7a&mpshare=1&scene=1&srcid=0319RvIZ2fucLHLShthVdegk&sharer_sharetime=1648042404990&sharer_shareid=2dfdf936388dba04865b3e883d5a3225&version=3.1.12.6001&platform=win#rd)
- [如何在大型项目中使用Git子模块开发](https://juejin.cn/post/6844903746166587405)
- [Git 各指令的本质，真是通俗易懂啊 (qq.com)](https://mp.weixin.qq.com/s/0g1RDnKaSw7WYPfcp_gmKw)
- [<font color=Red>我用四个命令，总结了 Git 的所有套路</font>](https://mp.weixin.qq.com/s?__biz=MzAxODQxMDM0Mw==&mid=2247485544&idx=1&sn=afc9d9f72d811ec847fa64108d5c7412&scene=21#wechat_redirect)

- [<font color=Red>git常用命令  1.分支开发  2.代码冲突处理</font>](https://blog.csdn.net/weixin_46087056/article/details/124741158)

## reset

- [git reset 命令](https://www.runoob.com/git/git-reset.html)
- [git如何恢复本地删除的文件夹](https://blog.csdn.net/qq_32077121/article/details/111150662)

- [<font color=Red>git 回退一个文件的版本</font>](https://blog.csdn.net/weixin_39580031/article/details/123826439)
- [<font color=Red>使用git checkout和git reset覆盖本地修改</font>](https://www.toutiao.com/article/6752851057765794308)

在本地修改文件、或者删除文件后，如果想恢复这些文件内容为git仓库保存的版本，可以使用下面几个命令：

- `git checkout [--] <filepath>`：可以恢复还没有执行 `git add` 的文件，但不能恢复已经执行过 `git add` 的文件
- `git reset [--] <filepath>`：把文件从git的staged区域移除，即取消`git add`，再使用 `git checkout` 进行恢复
- `git reset --hard`：恢复整个git仓库的文件内容为当前分支的最新版本

## commit

- [<font color=Red>Git怎样合并最近两次commit</font>](https://blog.csdn.net/keeplook/article/details/39324971)

```bash
git rebase -i HEAD~2
```

根据提示，把第二个“pick”改成“squash”，这样就可以把第二个commit合并到到第一个里。

- [对之前的commit 提交进行修改](https://www.jianshu.com/p/7d40838883af)
- [<font color=Red>Git 修改已提交 commit 的信息</font>](https://cloud.tencent.com/developer/article/1730774)
- [<font color=Red>git只合并某一个分支的某个commit</font>](https://www.cnblogs.com/boshen-hzb/p/9764835.html)
- [<font color=Red>Git合并特定commits 到另一个分支</font>](https://blog.csdn.net/ybdesire/article/details/42145597)

### git 修改最新commit的作者

使用 `git commit --amend --author` 修改提交的作者信息：

```bash
git commit --amend --author="新作者名 <新作者邮箱>"
```

```bash
git commit --amend --author="realwujing <realwujing@qq.com>"
```

### git 修改指定commit的作者

要修改指定提交（commit）的作者信息，你可以使用以下步骤：

1. **找到你要修改的提交的哈希值**：
   你可以通过 `git log` 来查看提交历史，并找到需要修改的提交哈希值。

2. **使用交互式变基来修改提交**：

   执行以下命令以启动交互式变基，从而修改指定的提交：

   ```bash
   git rebase -i <commit-hash>^
   ```

   其中 `<commit-hash>` 是你要修改的提交的哈希值。`^` 表示从前一个提交开始。

3. **标记为编辑（edit）**：

   在进入交互式变基编辑界面后，将要修改的提交前面的 `pick` 改为 `edit`，然后保存并退出。

4. **修改作者信息**：

   运行以下命令修改提交的作者信息：

   ```bash
   git commit --amend --author="New Author Name <new.email@example.com>"
   ```

   这会修改该提交的作者为你指定的新作者。

5. **继续变基**：

   修改完作者信息后，继续执行变基过程：

   ```bash
   git rebase --continue
   ```

6. **强制推送到远程仓库**（如果提交已推送到远程仓库）：

   由于你修改了历史提交，你需要强制推送到远程仓库：

   ```bash
   git push --force
   ```

**注意**：强制推送会覆盖远程仓库的历史，可能影响其他协作者，因此应谨慎使用。

## 查看文件每次提交的diff

- [Git 查看某个文件的修改记录](https://blog.csdn.net/sunshine_505/article/details/92795152)

```bash
git log -p kernel/sched/fair.c
```

## git diff查看改动内容

- **查看已暂存文件的改动**：使用 `git diff --cached <file>`。
- **查看工作目录中未暂存文件的改动**：使用 `git diff <file>`。

将 `<file>` 替换为具体的文件路径。

## 查找某句代码在哪个提交中出现

- [Git搜索Git历史记录中的字符串](https://geek-docs.com/git/git-questions/210_git_search_all_of_git_history_for_a_string.html)

```bash
git log -S 'cpumask_test_cpu(cpu, sched_domain_span(sd))' --oneline kernel/sched/fair.c | cat

8aeaffef8c6e sched/fair: Take the scheduling domain into account in select_idle_smt()
3e6efe87cd5c sched/fair: Remove redundant check in select_idle_smt()
3e8c6c9aac42 sched/fair: Remove task_util from effective utilization in feec()
c722f35b513f sched/fair: Bring back select_idle_smt(), but differently
6cd56ef1df39 sched/fair: Remove select_idle_smt()
df3cb4ea1fb6 sched/fair: Fix wrong cpu selecting from isolated domain
```

`kernel/sched/fair.c`参数可以去掉。

进一步使用git show 查看上述所有commit的具体内容：

```bash
git log -S 'cpumask_test_cpu(cpu, sched_domain_span(sd))' --oneline kernel/sched/fair.c | awk {'print $1'} | xargs git show > sched_domain_span.log
```

`kernel/sched/fair.c`参数可以去掉。

## 查找所有使用 @example.com 域名的提交作者

```bash
git log --pretty=format:"%an <%ae>" | grep "@example.com" | awk '!seen[$0]++'
```

- `awk '!seen[$0]++'`：`awk` 命令的这个用法可以用来去除重复行。`seen[$0]++` 是一个关联数组，用于跟踪已经看到的行，如果行第一次出现，它会被打印；如果是重复的行，则不会被打印。

这个命令可以帮助确保每个作者的邮箱地址只显示一次。

### 查找所有使用 @example.com 域名的提交

在 `git log` 中显示简短的提交信息（`--oneline` 格式），同时包含作者的姓名、邮箱以及提交的 `commit id`：

```bash
git log --pretty=format:"%h %s %an <%ae>" | grep "@example.com"
```

- `%h`：简短的 `commit id`。
- `%s`：提交信息的简短描述（相当于 `--oneline` 格式的描述）。
- `%an`：作者的名字。
- `%ae`：作者的邮箱。

通过这种方式，你将获得包含简短 `commit id`、提交信息、作者姓名和邮箱的输出，并且只显示 `@example.com` 域名的提交记录。

## 查看某个补丁在内核哪些版本中有

在Linux kernel stable tree mirror中查找某个提交：

```bash
git remote -v
origin  https://github.com/gregkh/linux.git (fetch)
origin  https://github.com/gregkh/linux.git (push)
```

```bash
git log --oneline | grep "arm64: implement ftrace with regs"
3b23e4991fb6 arm64: implement ftrace with regs
```

在Linux kernel source tree中查找这个提交：

<https://github.com/torvalds/linux/commit/3b23e4991fb6>

将`3b23e4991fb6`替换成要查找的commit:

![3b23e4991fb6最早出现在linux-v5.5-rc1上](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240418101723.png)

![3b23e4991fb6最早出现在linux-v5.5-rc1上](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20240418101819.png)

## head

- [git深入理解(二)：HEAD是什么](https://blog.csdn.net/raoxiaoya/article/details/110862360)
- [Git – Head](https://geek-docs.com/git/git-cmds/git-head.html)

## statsh

- [git stash详解](https://www.cnblogs.com/shoshana-kong/p/11194256.html)

## rebase merge

- [解决 Git 变基后的合并冲突](https://docs.github.com/cn/get-started/using-git/resolving-merge-conflicts-after-a-git-rebase)

    ```bash
    git pull origin master rebase
    ```

- [GIT使用 rebase 和 merge 的正确姿势（转）](https://www.jianshu.com/p/025aaa1a2c34)
- [Linux :关于git：您尚未完成合并(MERGE_HEAD存在)](https://blog.csdn.net/textdemo123/article/details/107867211)
- [使用交互式 git rebase 工具压缩 Git 提交](https://zhuanlan.zhihu.com/p/462530860)
- [<font color=Red>git rebase调整commit之间顺序</font>](http://quibbler.cn/?thread-232.htm&user=1)

- [git强制合并分支](https://www.jianshu.com/p/f247827d60bb)
- [refusing to merge unrelated histories的解决方案（本地/远程）综合](https://blog.csdn.net/junruitian/article/details/88361895)

## pull

- [如何使用 Git Pull 覆盖本地文件](https://man7.org/linux/man-pages/man3/pthread_create.3.html)
- [Git 合并到底使用Merge还是Rebase](https://mp.weixin.qq.com/s/3jxG4pdjSiCA1tgo5zau3g)

## pull request

- [Pull Request 的命令行管理](https://www.ruanyifeng.com/blog/2017/07/pull_request.html)

## push

- [Gitlab强制推送提示"You are not allowed to force push code to a protected branch on this project." · Issue #11 · LeachZhou/blog (github.com)](https://github.com/LeachZhou/blog/issues/11)
- [Github远程推送一直Everything up-to-date - sinatJ - 博客园 (cnblogs.com)](https://www.cnblogs.com/zishu/p/9191815.html)
- [submodule 子项目提交代码后无法push到远程仓库_三叔的负能量的博客-CSDN博客](https://blog.csdn.net/weixin_30304375/article/details/99984494)
- [error: failed to push some refs to如何解决_尘客-追梦的博客-CSDN博客](https://blog.csdn.net/qq_45893999/article/details/106273214)

- [<font color=Red>git 仓库提交分支到另外一个仓库</font>](https://blog.csdn.net/D939030515/article/details/105490835)

- [GIT 清除本地 REMOTES/ORIGIN/*](https://www.freesion.com/article/2366930198/)
- [git-警告：忽略损坏的ref refs / remotes / origin / HEAD](https://www.itranslater.com/qa/details/2583713236583449600)

## tag

- [<font color=Red>2.6 Git基础-打标签</font>](https://git-scm.com/book/zh/v2/Git-%E5%9F%BA%E7%A1%80-%E6%89%93%E6%A0%87%E7%AD%BE)
- [git tag创建、远程推送、回退以及强推push -f](https://blog.csdn.net/QH_JAVA/article/details/77979622)

要查看某个提交是否被标签包含，可以使用以下命令：

```bash
git tag --contains <commit-id>
```

### 示例

检查提交 `3b23e4991fb6` 是否被任何标签包含：

```bash
git tag --contains 3b23e4991fb6 | head -n10
v5.10
v5.10-rc1
v5.10-rc2
v5.10-rc3
v5.10-rc4
v5.10-rc5
v5.10-rc6
v5.10-rc7
v5.10.1
v5.10.10
```

## branch

1. **查看远程仓库的分支**：

   - 查看所有远程仓库的分支：

     ```bash
     git branch -r
     ```

   - 查看已知的远程仓库及其分支：

     ```bash
     git remote show <remote-name>
     ```

2. **重命名远程仓库**：
   - 将远程仓库重命名：

      ```bash
      git remote rename <旧名称> <新名称>
      ```

     例如，将 `origin` 重命名为 `upstream`：

      ```bash
      git remote rename origin upstream
      ```

      执行完这个命令后，你的远程仓库名称就会从 `origin` 改为 `upstream`。

3. **克隆特定远程分支**：

   - 获取特定远程分支而不影响其他分支：

     ```bash
     git fetch <远程仓库名> <远程分支名>:refs/remotes/<远程仓库名>/<远程分支名>
     ```

   - 将该远程分支检出为本地分支：

     ```bash
     git checkout -b <本地分支名> <远程仓库名>/<远程分支名>
     ```

4. **查看当前分支对应的远程分支**：

   - 查看所有本地分支及其对应的远程分支：

     ```bash
     git branch -vv
     ```

   - 查看当前分支对应的远程分支：

     ```bash
     git rev-parse --abbrev-ref --symbolic-full-name @{u}
     ```

5. **查看包含指定提交的本地分支**：

   - 要查看包含特定提交（如 `3b23e4991fb6`）的所有分支，可以使用以下 Git 命令：

      ```bash
      git branch --contains 3b23e4991fb6
      ```

      `git branch --contains <commit>`：这个命令将列出所有包含指定提交的本地分支。

   - 如果你想查看所有远程和本地分支中包含该提交的分支，可以使用以下命令：

      ```bash
      git branch -a --contains 3b23e4991fb6
      ```

      运行以上命令后，输出可能类似于：

      ```bash
      * develop
      feature-branch
      remotes/origin/master
      remotes/origin/develop
      ```

      在输出中，带有 `*` 的分支表示当前所在的分支，其他分支则表示包含指定提交的所有分支。如果输出为空，则表示没有分支包含该提交。

## name-rev

使用 `git name-rev` 命令可以帮助开发者更方便地识别提交在版本历史中的位置，提供一个更人性化的提交引用方式。

### 用法示例

1. **查找提交的符号名称**

   ```bash
   git name-rev 1b06a0
   ```

   **输出示例**：

   ```bash
   1b06a0 tags/release-0074~744
   ```

   **解释**：
   - 该命令查找提交 `1b06a0` 的符号名称。
   - 输出显示 `1b06a0` 距离标签 `release-0074` 有 744 次提交。

## submodule

- [<font color=Red>git中子模块/子仓库的使用</font>](https://blog.csdn.net/weixin_43455581/article/details/120174578)
- [<font color=Red>Git submodule 知识总结</font>](https://knightyun.github.io/2021/03/21/git-submodule)
- [7.11 Git工具-子模块](https://git-scm.com/book/zh/v2/Git-%E5%B7%A5%E5%85%B7-%E5%AD%90%E6%A8%A1%E5%9D%97)
- [git submodule 使用小结 - 简书 (jianshu.com)](https://www.jianshu.com/p/f8a55b972972/)
- [git submodule 的使用 - 简书 (jianshu.com)](https://www.jianshu.com/p/e27a978ddb88)
- [Git Submodule管理项目子模块 - nicksheng - 博客园 (cnblogs.com)](https://www.cnblogs.com/nicksheng/p/6201711.html)
- [git submodule update 游离分支 临时分支问题记录 解决办法](https://blog.csdn.net/qq_38292379/article/details/124950163)
- [git添加submodule以及更名](https://www.cnblogs.com/hustcpp/p/13092625.html)
- [Git修改.Submodule文件url生效](https://blog.csdn.net/weixin_45115705/article/details/104303730)
- [submodule切换分支_Git submodule-切换submodule的分支](https://blog.csdn.net/weixin_39673293/article/details/111786946)
- [git submodule 分支是否与主项目的分支一起切换？](https://qa.1r1g.com/sf/ask/2815344661/)

    设置永久记住密码

    ```bash
    # 设置永久记住密码

    git config --global credential.helper store
    git pull


    # 拉取所有子模块

    git submodule update --init --recursive




    git submodule foreach git pull origin master




    git submodule foreach git checkout master




    # 取消永久记住密码

    # git config --global --unset credential.helper
    ```

- [Git详解10-Git子库：submodule与subtree](https://juejin.cn/post/6934107291621228558)
- [将git仓库从submodule转换为subtree](https://zhuanlan.zhihu.com/p/594835463)
- [Git - - subtree与submodule - Anliven -博客园(cnblogs.com)](https://www.cnblogs.com/anliven/p/13681894.html)
- [Git SubTree使用](https://juejin.cn/post/6936459179049615397)

## .gitignore

- [.gitignore 在已忽略文件夹中不忽略指定文件、文件夹... | Laravel China 社区 (learnku.com)](https://learnku.com/articles/18380)

## .gitkeep

- [git提交空文件夹_fengchao2016的博客-CSDN博客_git空文件夹不能提交](https://blog.csdn.net/fengchao2016/article/details/52769151)
- [如何向git仓库提交空文件夹？.gitignore和.gitkeep配合 - everfight - 博客园 (cnblogs.com)](https://www.cnblogs.com/everfight/p/keep_empty_dir_in_git_repo.html)

## gerrit

- [Gerrit - not clone with commit-msg hook](https://www.jianshu.com/p/e1b466df12de)

    ```bash
    git review dev -r origin
    ```

    ```bash
    git cherry-pick c6611ca831bb97ac646dbd22bbf9f5216741c09d
    ```

- [gerrit 将代码从一个分支合并到另外一个分支 Cherry Pick的使用](https://blog.csdn.net/weixin_38419133/article/details/113600907)
- [gerrit 使用教程（一）](https://www.cnblogs.com/111testing/p/9450530.html)

- [gerrit 缺少change-Id](https://blog.csdn.net/yangshujuan91/article/details/113741972)

- [VSCode_git&svn 的冲突译文](https://www.cnblogs.com/tsalita/p/16500429.html)

## Github Action

- [通过 GitHub Actions 将 GitHub 仓库自动备份到 Gitee、GitLab](https://www.it610.com/article/1527116916244676608.htm)
- [bash - Github Action - Error: Process completed with exit code 1 - Stack Overflow](https://stackoverflow.com/questions/66626814/github-action-error-process-completed-with-exit-code-1)
- [<font color=Red>Github actions git log only output one line</font>](https://github.com/orgs/community/discussions/26928)

## patch

- [使用Git生成patch和应用patch，看完这一篇文章就全懂了](https://www.toutiao.com/article/6652488964823319052)
- [git生成patch和打patch的操作命令](https://blog.csdn.net/qq_30624591/article/details/89474571)

### 将某个作者的所有提交打成序列号递增的patch

```bash
#!/bin/bash

# 定义补丁目录变量
PATCH_DIR="../patchs"

# 生成补丁文件到指定目录
git log --oneline --reverse --author="^realwujing" | awk '{print $1}' | xargs -I {} git format-patch -1 {} -o "$PATCH_DIR"

# 切换到补丁目录
cd "$PATCH_DIR" || { echo "Directory $PATCH_DIR does not exist"; exit 1; }

# 计数器初始化
i=1

# 遍历所有 .patch 文件，按照当前目录中的排序方式处理
for patch in $(ls -lt | tac | awk '{print $NF}' | grep '\.patch$'); do
    # 使用 sed 删除原有的序号部分
    base_name=$(echo "$patch" | sed 's/^[0-9]\{4\}-//')

    # 使用 printf 格式化为四位数字，不足前面补零
    new_name=$(printf "%04d-%s" $i "$base_name")

    # 显示原始文件名和新的文件名
    echo "Renaming $patch to $new_name"

    # 重命名文件
    mv "$patch" "$new_name"

    # 增加计数器
    i=$((i + 1))
done
```

1. **定义补丁目录变量**：`PATCH_DIR="../patchs"` 将目标目录存储在一个变量中。

2. **生成补丁文件到指定目录**：`git format-patch` 命令将补丁文件生成到 `"$PATCH_DIR"` 目录。

3. **切换到补丁目录**：`cd "$PATCH_DIR"` 进入指定目录。如果目录不存在，则输出错误信息并退出。

4. **遍历并重命名文件**：
   - 使用 `sed` 删除原有的序号部分，得到文件的基本名称。
   - 使用 `printf` 格式化新的文件名，包括递增的序号。
   - **增加对比输出**：使用 `echo` 显示每个文件的原始名称和新的名称。
   - 使用 `mv` 重命名文件。

5. **计数器递增**：每处理一个文件，计数器 `i` 递增。

这个脚本在重命名过程中会输出每个文件的原始名称和新的名称，便于跟踪文件的变化。

使用git am，可以将上述一系列补丁应用到某个分支上，如果存在冲突，则会报错并生成reject文件，需要手动处理冲突:

```bash
git am ../patchs/*.patch --reject
```

## send-email

- [提交内核补丁到Linux社区的步骤](https://www.cnblogs.com/gmpy/p/12200609.html)
- [Git邮件向Linux社区提交内核补丁教程](https://blog.csdn.net/Guet_Kite/article/details/117997036)

`.gitconfig`中的配置如下：

```text
[user]
	email = realwujing@qq.com
	name = wujing
[url "git@github.com:"]
	insteadOf = https://github.com/
[commit]
	template = ~/.gitcommit_template
[sendemail]
    smtpServer = smtp.qq.com
    smtpServerPort = 587
    smtpEncryption = tls
    smtpUser = realwujing@qq.com
    smtpPass = kpdkskfgpbaxsrnwbifi
```

测试连接和认证:

```bash
git send-email -v --to="realwujing@qq.com" HEAD^
```

```text
/var/folders/hh/0wpdzj8s79scygrdntrwy32h0000gn/T/xctMCK9Q3r/v-to-realwujing-qq.com-0001-git-send-email-and-Signed-off.patch
The following files are 8bit, but do not declare a Content-Transfer-Encoding.
    /var/folders/hh/0wpdzj8s79scygrdntrwy32h0000gn/T/xctMCK9Q3r/v-to-realwujing-qq.com-0001-git-send-email-and-Signed-off.patch
Which 8bit encoding should I declare [UTF-8]?
To whom should the emails be sent (if anyone)? realwujing@qq.com
Message-ID to be used as In-Reply-To for the first email (if any)?
(mbox) Adding cc: wujing <realwujing@qq.com> from line 'From: wujing <realwujing@qq.com>'
(body) Adding cc: wujing <realwujing@qq.com> from line 'Signed-off-by: wujing <realwujing@qq.com>'

From: wujing <realwujing@qq.com>
To: realwujing@qq.com
Subject: [PATCH v--to=realwujing@qq.com] git: send-email and Signed-off-by
Date: Sat,  6 Jul 2024 19:19:17 +0800
Message-ID: <20240706111927.56825-1-realwujing@qq.com>
X-Mailer: git-send-email 2.45.2
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 8bit

    The Cc list above has been expanded by additional
    addresses found in the patch commit message. By default
    send-email prompts before sending whenever this occurs.
    This behavior is controlled by the sendemail.confirm
    configuration setting.

    For additional information, run 'git send-email --help'.
    To retain the current behavior, but squelch this message,
    run 'git config --global sendemail.confirm auto'.

Send this email? ([y]es|[n]o|[e]dit|[q]uit|[a]ll): y
OK. Log says:
Server: smtp.qq.com
MAIL FROM:<realwujing@qq.com>
RCPT TO:<realwujing@qq.com>
From: wujing <realwujing@qq.com>
To: realwujing@qq.com
Subject: [PATCH v--to=realwujing@qq.com] git: send-email and Signed-off-by
Date: Sat,  6 Jul 2024 19:19:17 +0800
Message-ID: <20240706111927.56825-1-realwujing@qq.com>
X-Mailer: git-send-email 2.45.2
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 8bit

Result: 250
```
