# vim

- [设置默认文本编辑器](https://www.debian.org/doc/manuals/debian-reference/ch01.zh-cn.html#_setting_a_default_text_editor)
- [<font color=Red>Linux vi/vim - 菜鸟教程</font>](https://www.runoob.com/linux/linux-vim.html)

- [<font color=Red>vi 和vim 的区别</font>](https://blog.csdn.net/qq_34306360/article/details/78720090)

- [<font color=Red>VIM常用命令</font>](https://blog.csdn.net/weixin_44441367/article/details/124810525)

- [12 个超实用的 vim 编辑技巧](https://mp.weixin.qq.com/s/MyV3ZC7A7vdWpQ1lt3d_dg)

## 多窗口

- [vim操作之多窗格,多文件的编辑和操作](https://www.cnblogs.com/yukina/p/16401529.html)
- [在Vim中同时打开多个文件的相关操作技巧](https://www.bilibili.com/read/cv16390641)

## 多行操作

- [<font color=Red>vim如何删除全文</font>](https://blog.csdn.net/weixin_54227557/article/details/122782139)
- [vim小技巧：多行行首插入、删除、替换](https://www.toutiao.com/article/7167342849502446114/)
- [<font color=Red>vim 多行操作(7)</font>](https://waliblog.com/2019/05/06/vim-7.html)
- [Linux——VIM学习选取多行（转）](https://blog.csdn.net/sinat_36053757/article/details/78183506)
- [【Linux】Vim编辑器-批量注释与反注释](https://blog.csdn.net/xiajun07061225/article/details/8488210)

## 查找替换

- [vim 基本命令查找和替换](https://www.cnblogs.com/woshimrf/p/vim.html)

(命令模式)搜索和替换

命令模式下(esc退出插入模式):

```bash
/keyword     //向光标下搜索keyword字符串，keyword可以是正则表达式
?keyword     //向光标上搜索keyword字符串
n           //向下搜索前一个搜素动作
N         //向上搜索前一个搜索动作

*(#)      //当光标停留在某个单词上时, 输入这条命令表示查找与该单词匹配的下(上)一个单词. 同样, 再输入 n 查找下一个匹配处, 输入 N 反方向查找.

g*(g#)        //此命令与上条命令相似, 只不过它不完全匹配光标所在处的单词, 而是匹配包含该单词的所有字符串.

:s/old/new      //用new替换行中首次出现的old
:s/old/new/g         //用new替换行中所有的old
:n,m s/old/new/g     //用new替换从n到m行里所有的old
:%s/old/new/g      //用new替换当前文件里所有的old
```

## 返回终端

- [Linux下快速收起vim 并暂存修改](https://blog.csdn.net/lambert310/article/details/77659417)
- [vim使用技巧-如何暂时返回终端](https://blog.csdn.net/vastz/article/details/120064236)

## 忽略大小写

- [vim 技巧 – 查找的时候忽略大小写](https://xwsoul.com/posts/472)

## 鼠标模式

- [vim鼠标模式打开与关闭](https://shagain.club/index.php/archives/382/)
- [彻底解决基于Debian发行系统的vim鼠标模式(可视模式)问题](https://blog.51cto.com/u_15083238/2600714)

## sudo保存文件

- [技巧045：以超级用户权限保存文件](https://blog.csdn.net/weixin_44531336/article/details/126187768)

    ```bash
    :w !sudo tee % > /dev/null
    ```

- [vim中使用sudo保存文件](https://www.cnblogs.com/jackie-astro/p/13295584.html)

## 插件

- [<font color=Red>vim配置</font>](https://blog.csdn.net/tiantianhaoxinqing__/article/details/123593749)

- [<font color=Red>https://vimawesome.com/</font>](https://vimawesome.com/)
- [（转）Vim十大必备插件](https://blog.51cto.com/u_15905375/5919878)
- [Ubuntu 下 vim+Ctags+Taglist+WinManager工具的安装](https://blog.csdn.net/eric_sunah/article/details/51028874)
- [2021-11-11 vim 显示函数列表](https://blog.csdn.net/weihua1643/article/details/121264521)
- [linux安装vim插件 NERDTree、taglist 、winmanager（类似source insight）](https://blog.csdn.net/qq_36754075/article/details/100030981)
- [<font color=Red>https://github.com/VundleVim/Vundle.vim</font>](https://github.com/VundleVim/Vundle.vim)
- [https://github.com/preservim/nerdtree](https://github.com/preservim/nerdtree)
- [https://github.com/yegappan/taglist](https://github.com/yegappan/taglist)

```bash
sudo apt install vim vim-gtk ctags cscope
git clone https://github.com/VundleVim/Vundle.vim.git ~/.vim/bundle/Vundle.vim
wget -O ~/.vimrc https://raw.githubusercontent.com/realwujing/realwujing.github.io/main/linux/vim/.vimrc
vim .
:PluginInstall
```

## 格式化

### 1. 格式化整个文件

1. **跳转到第一行**：

   ```vim
   gg
   ```

2. **进入行可视模式**：

   ```vim
   V
   ```

3. **选中到文件末尾**：

   ```vim
   G
   ```

4. **应用格式化**：

   ```vim
   =
   ```

### 2. 行可视化模式化选定范围

1. **跳转到第一行**：

   ```vim
   gg
   ```

2. **进入行可视模式**：

   ```vim
   V
   ```

3. **选中格式化范围**：

   ```vim
   按下键盘方向键下键选定范围
   ```

4. **应用格式化**：

   ```vim
   =
   ```

### 3. 格式化指定行范围

1. **退出编辑模式**：

   ```vim
   ESC
   ```

2. **输入行范围格式化命令**：

   ```vim
   nG=mG
   ```

   例如，格式化第 5 行到第 10 行：

   ```vim
   5G=10G
   ```

## 折叠

在 Vim 中如何使用缩进折叠的方法:

1. 开启缩进折叠:
   - 在 Vim 的配置文件中添加 `set foldmethod=indent`
   - 这样 Vim 就会根据代码的缩进深浅自动进行折叠

2. 控制折叠深度:
   - 使用 `set foldlevel=n` 命令来设置初始的折叠深度
   - `n` 表示只会折叠缩进深度小于等于 `n` 的代码块
   - 例如 `set foldlevel=2` 表示只会折叠缩进深度小于等于 2 的代码块

3. 常用的折叠控制命令(按下Esc进入普通模式执行下方命令):
   - `za`: 打开/关闭当前折叠
   - `zo`: 打开当前折叠
   - `zc`: 关闭当前折叠
   - `zR`: 打开所有折叠
   - `zM`: 关闭所有折叠

4. 一些技巧:
   - 可以在编辑时使用 `>>` 和 `<<` 命令来增加或减少代码缩进,这样会相应地创建或删除折叠
   - 可以设置 `foldnestmax` 来限制最大折叠深度,避免过度折叠
   - 可以设置 `foldcolumn` 来显示折叠状态的列,方便查看

- [<font color=Red>vim中常用折叠命令</font>](https://www.cnblogs.com/litifeng/p/11675547.html)

- [Vim折叠操作(折叠代码、折叠函数、方法、类等)](https://blog.csdn.net/weixin_43971252/article/details/123218379)

## 前进后退

好的,让我来总结一下 Vim 中常用的前进后退快捷键:

1. **前进**:
   - `Ctrl + ]`: 跳转到光标下符号的定义处
   - `g Ctrl + ]`: 打开光标下符号的定义,但不切换buffer
   - `Ctrl + o`: 跳转到上一个位置(前进历史)

2. **后退**:
   - `Ctrl + t`: 返回到上一个跳转位置
   - `Ctrl + i`: 前进到下一个跳转位置(与 `Ctrl + o` 配合使用)
   - `uUUu`: 撤销/重做操作

3. **书签**:
   - `m{a-zA-Z}`: 设置书签
   - `'{a-zA-Z}`: 跳转到书签处
   - `` `{a-zA-Z} ``: 精确跳转到书签处

4. **位置列表**:
   - `:lprevious`/`:lnext`: 在位置列表中前后移动
   - `:lopen`/`:lclose`: 打开/关闭位置列表窗口

### 函数跳转

- [ubuntu下vim安装ctags工具](https://blog.csdn.net/m0_37624499/article/details/90705658)
- [Linux——vim插件之ctags的安装与配置](https://www.cnblogs.com/oddcat/articles/9678044.html)
- [vim多窗口，常用命令集](http://t.zoukankan.com/quant-lee-p-6659696.html)
- [<font color=Red>vim之函数跳转功能</font>](https://blog.csdn.net/ballack_linux/article/details/71036072)
- [vim -t 选项](https://blog.csdn.net/qwaszx523/article/details/77838855)

## cscope

先在源码目录中生成索引数据:

```bash
    cscope -Rbq
```

Vim 中 cscope 相关的 Ctrl 快捷键:

1. `Ctrl + \` 系列快捷键:
   - `Ctrl + \` `s`: 查找C语言符号
   - `Ctrl + \` `g`: 查找函数、宏、枚举等定义的位置
   - `Ctrl + \` `d`: 查找本函数调用的函数
   - `Ctrl + \` `c`: 查找调用本函数的函数
   - `Ctrl + \` `t`: 查找指定的文本字符串
   - `Ctrl + \` `e`: 查找egrep模式
   - `Ctrl + \` `f`: 查找并打开指定的文件
   - `Ctrl + \` `i`: 查找包含本文件的文件

2. 其他 cscope 快捷键:
   - `Ctrl + ]`: 跳转到光标下符号的定义处
   - `Ctrl + T`: 返回上一个位置

## buffers

在 Vim 中,查看和管理 buffers 的常用简写命令如下:

1. `:ls` or `:buffers` - 列出所有已打开的 buffers。

2. `:b <buffer_number>` - 切换到指定的 buffer。例如 `:b 2` 切换到 buffer 2。

3. `:bn` - 切换到下一个 buffer。

4. `:bp` - 切换到上一个 buffer。

5. `:bd` - 关闭当前 buffer。

6. `:bd <buffer_number>` - 关闭指定的 buffer。

7. `:bufdo <command>` - 对所有打开的 buffers 执行指定的命令。例如 `:bufdo w` 保存所有 buffers。

这些是使用 Vim 处理 buffers 时最常用的简写命令。如果您需要更多关于 Vim buffers 的信息,欢迎随时询问我。

## Quickfix

你说得很对,`:cnext` 这个命令可以进一步简写。那么让我再总结一下 Vim 中使用 Quickfix 窗口的常用快捷键简写:

1. `:copen` - 打开 Quickfix 窗口

2. `:cclose` - 关闭 Quickfix 窗口

3. `:cl` - 列出所有 Quickfix 条目，是`:clist` 命令的简写形式，进入选择列表后`d`向下翻页，`u`向上翻页

4. `:cc [nr]` - 跳转到指定编号的 Quickfix 条目

5. `:cn` - 跳转到下一个 Quickfix 条目，是`:cnext` 命令的简写形式

6. `:cp` - 跳转到上一个 Quickfix 条目，是`:cprev` 命令的简写形式

7. `:cfirst` - 跳转到第一个 Quickfix 条目

8. `:clast` - 跳转到最后一个 Quickfix 条目

9. `:colder` - 切换到较早的 Quickfix 列表

10. `:cnewer` - 切换到较新的 Quickfix 列表

11. `:cdo <command>` - 对所有 Quickfix 条目执行指定命令

## 其它

### nano

- [4.4 超简单文书编辑器： nano](https://www.wenjiangs.com/doc/8ij4xm4z)
