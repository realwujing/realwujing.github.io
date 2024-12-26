---
date: 2023/09/26 11:21:32
updated: 2024/12/23 10:54:11
---

# vim

- [Vim实用技巧（第2版）.pdf](https://gitee.com/tulip-kung/books/blob/main/Vim%E5%AE%9E%E7%94%A8%E6%8A%80%E5%B7%A7%EF%BC%88%E7%AC%AC2%E7%89%88%EF%BC%89.pdf)
- [设置默认文本编辑器](https://www.debian.org/doc/manuals/debian-reference/ch01.zh-cn.html#_setting_a_default_text_editor)
- [<font color=Red>Linux vi/vim - 菜鸟教程</font>](https://www.runoob.com/linux/linux-vim.html)

- [<font color=Red>vi 和vim 的区别</font>](https://blog.csdn.net/qq_34306360/article/details/78720090)

- [<font color=Red>VIM常用命令</font>](https://blog.csdn.net/weixin_44441367/article/details/124810525)

- [12 个超实用的 vim 编辑技巧](https://mp.weixin.qq.com/s/MyV3ZC7A7vdWpQ1lt3d_dg)
- [<font color=Red>VIM常用快捷键</font>](https://blog.csdn.net/xiaoxinyu316/article/details/44061173)
- [Linux 文本编辑器Vim（1）——初识模式以及基本操作介绍](https://blog.csdn.net/SkyDream999/article/details/106741448)
- [vim操作大全](https://www.toutiao.com/article/6732007685937431044)
- [vim进阶:200个终身受益的命令](https://mp.weixin.qq.com/s/wxJMpanpYaNHzBKV0Uiy4A)
- [从新手到大师：优雅的Vim熟练之旅（万文详解）](https://mp.weixin.qq.com/s/5cMLQkD04bGGvLon0Mx97Q)

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

## 快速执行上一条命令

- **`@:`**：在普通模式下，重复上一条 Ex 命令。
- **命令行模式**：在 `:` 命令行模式下，按 `↑` 查看和执行上一条命令。
- **`@@`**：在普通模式下，重复上一个宏。

- **`.` 命令**：
  - **作用**：在普通模式下，重复最后一次普通模式命令或插入模式操作。
  - **示例**：
    - **删除多行**：`dd`（删除当前行），然后按 `.` 继续删除下一行。
    - **插入文本**：`iHello<ESC>`（插入文本并返回普通模式），然后按 `j.` 重复插入。
    - **替换字符**：`rA`（替换字符），然后按 `.` 重复替换下一个字符。

## 多窗口

- [vim操作之多窗格,多文件的编辑和操作](https://www.cnblogs.com/yukina/p/16401529.html)
- [在Vim中同时打开多个文件的相关操作技巧](https://www.bilibili.com/read/cv16390641)

## 跳转翻页

### **跳转到指定行**

- **直接输入行号**：在命令模式下直接输入行号即可跳转到该行，例如输入 `500` 跳转到第 500 行。
- `:行号`：跳转到指定行，例如 `:101` 跳转到第 101 行。

### **翻页操作**

- `Ctrl+f`：向下翻一整页。
- `Ctrl+b`：向上翻一整页。
- `Ctrl+d`：向下翻半页。
- `Ctrl+u`：向上翻半页。

### **滚动操作**

- `Ctrl+e`：向下滚动一行。
- `Ctrl+y`：向上滚动一行。

### **位置跳转**

- `n%`：跳转到文件的 n% 位置，例如 `50%` 跳转到文件中间。

### **屏幕显示调整**

- `zz`：将当前行移动到屏幕中央。
- `zt`：将当前行移动到屏幕顶端。
- `zb`：将当前行移动到屏幕底端。

这些命令可以帮助你更有效地在 Vim 中导航和调整视图，适用于快速查找和定位代码或文本。

## 多行操作

- [<font color=Red>vim如何删除全文</font>](https://blog.csdn.net/weixin_54227557/article/details/122782139)
- [vim小技巧：多行行首插入、删除、替换](https://www.toutiao.com/article/7167342849502446114/)
- [<font color=Red>vim 多行操作(7)</font>](https://waliblog.com/2019/05/06/vim-7.html)
- [Linux——VIM学习选取多行（转）](https://blog.csdn.net/sinat_36053757/article/details/78183506)
- [【Linux】Vim编辑器-批量注释与反注释](https://blog.csdn.net/xiajun07061225/article/details/8488210)

## 移动

- [vim移动、定位命令与快捷键速查表(简练通俗)](https://blog.csdn.net/QQ245671051/article/details/53228752)
- [<font color=Red>VIM常用快捷键</font>](https://blog.csdn.net/xiaoxinyu316/article/details/44061173)

以下是 Vim 中与光标移动相关的常用命令的总结：

- **`w`**：将光标移动到下一个单词的开头（以空格或标点符号为分隔符）。
- **`W`**：将光标移动到下一个以空格为分隔符的单词的开头。
- **`b`**：将光标移动到当前单词的开头，或者到前一个单词的开头（以空格或标点符号为分隔符）。
- **`B`**：将光标移动到当前以空格为分隔符的单词的开头，或者到前一个以空格为分隔符的单词的开头。
- **`e`**：将光标移动到当前单词的末尾（以空格或标点符号为分隔符）。
- **`E`**：将光标移动到当前以空格为分隔符的单词的末尾。
- **`f` + 字符：** 将光标向右移动到指定字符的位置。
  - 按下`;`键，将光标跳转到下一个相同的字符处。
  - 按下`,`键，将光标跳转到上一个相同的字符处。
- **`F` + 字符：** 将光标向左移动到指定字符的位置。
- **`t` + 字符：** 将光标向右移动到指定字符的前一个位置。
- **`T` + 字符：** 将光标向左移动到指定字符的前一个位置。
- **`n` + `G`：** 将光标移动到第 `n` 行。
- **`%`：** 匹配括号定位：将光标置于某个括号上时，按下%键，Vim 将会跳转到配对的对应括号上。

这些命令可以帮助你在 Vim 中高效地移动光标，进行文本编辑。

## 删除

- **`D`**：删除到行尾，相当于`d$`

- **`d+G`**：删除到文档尾（包括当前行）

- **`d+g+g`**：删除到文档头（包括当前行）

- **`d+0`**：删除从光标前一个字符到行首

- **`n+x`**：向右删除n个 不包括光标所在

- **`n+X`**：向左删除n个 不包括光标所在

- **`d+w`**：当光标在一个单词首时，删除一个单词，不在单词首是，从光标开始删除至单词结尾。不进入插入模式

- **`d+a+w`**：删除一个单词(包括空白字符)，光标在单词内部时，不进入插入模式

## 复制和移动行

### `:t` 命令（复制行）

- **语法**: `:[range]t[target]`
- **示例**:
  - 复制当前行到第 10 行后：`:t10`
  - 复制第 3 行到第 10 行后：`:3t10`
  - 复制第 1 到第 3 行到第 10 行后：`:1,3t10`

### `:m` 命令（移动行）

- **语法**: `:[range]m[ove] [target]`
- **示例**:
  - 移动当前行到第 10 行后：`:m10`
  - 移动第 3 行到第 10 行后：`:3m10`
  - 移动第 1 到第 3 行到第 10 行后：`:1,3m10`

### 实用技巧

- 复制当前行到文件末尾：`:t$`
- 移动当前行到文件末尾：`:m$`

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

### 快速选择单词并按其查找

- 将光标移动到单词的任意字母上，按 `*` 键向下查找并高亮显示该单词的所有匹配项，按 `n` 键跳转到下一个匹配项，`N` 键跳转到上一个匹配项。
- `#` 键功能类似 `*` 键但向上查找。
- 按 `g+d` 键可高亮显示单词，然后按 `n` 键查找。
- 对于前后无空白字符或标点的单词，可用 `g*` 键查找，包含单词字符序列的内容都会被匹配。

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

## 格式化与缩进

### 格式化命令

1. **格式化整个文件**（在正常模式下）：
   - `ggVG=`：跳转到文件开头，选中整个文件，并应用格式化。

2. **格式化选定范围**（在可视模式下）：
   - `shfit + v`进入行可视模式后，使用方向键选择需要格式化的行，然后按 `=` 进行格式化。

3. **格式化指定行范围**（在正常模式下）：
   - `nG=mG`：格式化从第 `n` 行到第 `m` 行，例如 `5G=10G` 格式化第 5 行到第 10 行。

### 缩进命令

1. **增加缩进**：
   - `>`：在正常模式下，按 `>` 后需要按 `Enter` 来增加当前行的缩进。
   - `>>`：在正常模式下，直接增加当前行的缩进（无需按 `Enter`）。

   - **增加多行缩进**：
     1. 进入行可视模式：按 `shift + v`。
     2. 扩展选定范围：使用方向键选择多行。
     3. 增加缩进：按 `>`。

2. **减少缩进**：
   - `<`：在正常模式下，按 `<` 后需要按 `Enter` 来减少当前行的缩进。
   - `<<`：在正常模式下，直接减少当前行的缩进（无需按 `Enter`）。

   - **减少多行缩进**：
     1. 进入行可视模式：按 `shift + v`。
     2. 扩展选定范围：使用方向键选择多行。
     3. 减少缩进：按 `<`。

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

## 跳转

### **Vim 跳转相关快捷键及其配合使用：**

1. **`Ctrl + ]`**：跳转到光标下符号的定义处。
   - **常用场景**：当你在代码中看到一个函数或变量名，使用 `Ctrl + ]` 可以快速跳转到其定义位置。

2. **`Ctrl + t`**：返回到上一个跳转位置。
   - **配合使用**：如果你用 `Ctrl + ]` 跳转到定义处之后想返回到跳转前的位置，就可以使用 `Ctrl + t`。

3. **`Ctrl + o`**：后退到上一个跳转位置。
   - **常用场景**：当你在文件中多次跳转（例如通过搜索、跳转到定义等），使用 `Ctrl + o` 可以依次返回之前的跳转位置。
   - **配合使用**：这个命令适合在你连续进行了多次跳转后，想要逐步回顾之前的位置时使用。

4. **`Ctrl + i`**：前进到下一个跳转位置。
   - **常用场景**：在使用 `Ctrl + o` 后，如果你想再跳回到之前的某个位置，可以使用 `Ctrl + i` 向前移动。
   - **配合使用**：`Ctrl + i` 和 `Ctrl + o` 是配对使用的，`Ctrl + o` 用于回退，`Ctrl + i` 用于前进，帮助你在跳转历史中自由导航。

5. **`:jumps`**：列出跳转历史。
   - **常用场景**：当你不确定自己跳转过哪些位置，或想查看完整的跳转路径时，可以使用 `:jumps` 来显示跳转历史列表。

### **使用示例：**

1. 你在代码中使用 `Ctrl + ]` 跳转到一个函数的定义。
2. 查看完定义后，想返回之前的位置，按 `Ctrl + t`。
3. 你继续编辑文件，并多次跳转（例如使用搜索或其他跳转命令）。
4. 需要逐步回顾之前的位置时，按 `Ctrl + o` 逐步后退。
5. 如果后退过头，按 `Ctrl + i` 前进到下一个跳转位置。

通过这些命令的配合，你可以高效地在代码中导航、跳转和回溯，提高编辑效率。

### #ifdef

1. **跳转到匹配的 `#else` 或 `#endif`**
   - **`%`**：在 `#ifdef`、`#else`、`#endif` 之间跳转，返回到匹配项。

2. **返回到 `#ifdef` 或 `#if` 的开始处**
   - **`[#`**：跳转到上一个 `#ifdef` 或 `#if` 块的开始（如果光标在块内）。

3. **跳转到下一个 `#else` 或 `#endif`**
   - **`]#`**：跳转到下一个 `#else` 或 `#endif`，跳过中间的 `#ifdef` 或 `#if` 块。

#### 例子说明

- 如果光标在 `#ifdef` 处，按 `%` 会跳转到对应的 `#else`。
- 继续按 `%` 会跳转到 `#endif`，再按一次 `%` 会回到原来的 `#ifdef`。
- 在一个 `#ifdef` - `#endif` 块内的某个位置，按 `[#` 会移动到对应的 `#ifdef` 或 `#if`。
- 按 `]#` 会跳转到下一个 `#else` 或 `#endif`，并跳过当前块。

#### 注意事项

- 如果光标不在 `#if` 或 `#ifdef` 的后面位置，按 `[#` 时 Vim 会鸣音（发出错误提示）。
- 确保 Vim 的文件类型检测正确，以便它能够正确识别 C/C++ 代码中的这些预处理指令。

### 函数跳转

- [ubuntu下vim安装ctags工具](https://blog.csdn.net/m0_37624499/article/details/90705658)
- [Linux——vim插件之ctags的安装与配置](https://www.cnblogs.com/oddcat/articles/9678044.html)
- [vim多窗口，常用命令集](http://t.zoukankan.com/quant-lee-p-6659696.html)
- [<font color=Red>vim之函数跳转功能</font>](https://blog.csdn.net/ballack_linux/article/details/71036072)
- [vim -t 选项](https://blog.csdn.net/qwaszx523/article/details/77838855)

#### 回到函数开头

1. **使用 `[{` 命令**:
   - 将光标放在函数体的任意位置，然后输入 `[{`。这个命令会将光标移动到当前函数的开头。

#### 回到函数结尾

1. **使用 `]}` 命令**:
   - 将光标放在函数体的任意位置，然后输入 `]}`。这个命令会将光标移动到当前函数的开头。

## cscope

先在源码目录中生成索引数据:

```bash
    cscope -Rbq
```

Vim 中 cscope 相关的 Ctrl 快捷键:

1. `Ctrl + \` 系列快捷键:
   - `Ctrl + \` `s`: 查找C语言符号，等同于normal模式下的`:cs f s <symbol>`
   - `Ctrl + \` `g`: 查找函数、宏、枚举等定义的位置，等同于normal模式下的`:cs f g <symbol>`
   - `Ctrl + \` `d`: 查找本函数调用的函数，等同于normal模式下的`:cs f d <symbol>`
   - `Ctrl + \` `c`: 查找调用本函数的函数，等同于normal模式下的`:cs f c <symbol>`
   - `Ctrl + \` `t`: 查找指定的文本字符串，等同于normal模式下的`:cs f t <string>`
   - `Ctrl + \` `e`: 查找egrep模式，等同于normal模式下的`:cs f e <pattern>`
   - `Ctrl + \` `f`: 查找并打开指定的文件，等同于normal模式下的`:cs f f <filename>`
   - `Ctrl + \` `i`: 查找包含本文件的文件，等同于normal模式下的`:cs f i <filename>`

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

- [Vim/Neovim 中的快速修复列表和位置列表](https://blog.csdn.net/qq_39785418/article/details/125680659)

Vim 中使用 Quickfix 窗口的常用快捷键简写:

1. `:copen` - 打开 Quickfix 窗口

   - 命令模式下执行`:TlistToggle`关闭函数及变量列表，不关闭会影响Quickfix 窗口聚焦。
   - 当窗口聚焦时，您可以使用 j 或 k 来高亮显示下一个或上一个列表项，使用 enter 将光标移动到高亮显示的文件上和位置上。

2. `:ccl` - 关闭 Quickfix 窗口，是`:cclose`命令的简写形式

3. `:cl` - 列出所有 Quickfix 条目，是`:clist` 命令的简写形式，进入选择列表后`d`向下翻页，`u`向上翻页

4. `:cc [nr]` - 跳转到指定编号的 Quickfix 条目

5. `:cn` - 跳转到下一个 Quickfix 条目，是`:cnext` 命令的简写形式

6. `:cp` - 跳转到上一个 Quickfix 条目，是`:cprev` 命令的简写形式

7. `:cfirst` - 跳转到第一个 Quickfix 条目

8. `:clast` - 跳转到最后一个 Quickfix 条目

### 导航旧Quickfix

Vim 在每个会话中保留多达 10 个快速修复列表，在每个窗口中保留 10 个位置列表。

cscope、grep等都可以创建新的快速修复列表。

如`vim -t start_kernel`打开init/main.c后，在命令行模式下执行:

```bash
:grep start_kernel include/linux -inr --include='*.h'
```

可以创建一个新的Quickfix。

1. `:col` - 切换到较早的 Quickfix 列表，是`:colder` 命令的简写形式

2. `:cnew` - 切换到较新的 Quickfix 列表，是`:cnewer` 命令的简写形式

3. `:cdo <command>` - 对所有 Quickfix 条目执行指定命令

## 位置列表

如`vim -t start_kernel`打开init/main.c后，在命令行模式下执行:

```bash
:lgrep start_kernel include/linux -inr --include='*.h'
```

可以创建一个新的位置列表。

导航位置列表的一些命令：

1. `:lopen` - 打开位置列表窗口。

2. `:lcl` 或 `:lclose` - 关闭位置列表窗口。

3. `:lnext` - 跳到列表的下一项。

4. `:lprev` - 跳到列表的前一项。

5. `:lfirst` - 跳到列表的第一项。

6. `:llast` - 跳到列表的最后一项。

7. `ll [n]` - 跳到第 n 项。

位置命令与它们的快速修复列表几乎完全相同，除了c 被替换成 l 。

## NERDTree

以下是 Vim 的 NERDTree 插件中常用的快捷键总结：

`NERDTree` 是 Vim 的一个插件，用于在 Vim 中管理和浏览文件和目录。以下是一些常用的 `NERDTree` 命令和操作：

### 基本命令

- **`:NERDTree`**：打开或关闭 `NERDTree` 窗口。
- **`:NERDTreeToggle`**：切换 `NERDTree` 窗口的显示或隐藏状态（如果打开则关闭，关闭则打开）。
- **`:NERDTreeFocus`**：将焦点切换到已经打开的 `NERDTree` 窗口。
- **`:NERDTreeFind`**：定位 `NERDTree` 到当前正在编辑的文件，并将焦点移动到该文件上。

### 基本导航

- **`o`**: 打开/关闭选中文件或目录。
- **`Enter`**: 打开文件，或者进入目录。
- **`s`**: 水平分割窗口并打开选中的文件。
- **`i`**: 垂直分割窗口并打开选中的文件。
- **`t`**: 在新标签页中打开选中的文件。
- **`p`**: 转到当前节点的父目录。

### 文件与目录操作

- **`m`**: 打开文件操作菜单，可以选择创建、删除、重命名文件或目录。
  - **`a`**: 添加文件或目录。
  - **`d`**: 删除选中的文件或目录。
  - **`r`**: 重命名文件或目录。

### 切换视图

- **`I`**: 切换显示隐藏文件（以 `.` 开头的文件）。
- **`C`**: 将当前目录设置为 NERDTree 的根目录。
- **`u`**: 返回到上一级目录。
- **`R`**: 刷新目录树。

### 帮助与其他

- **`?`**: 打开 NERDTree 的帮助文档。
- **`q`**: 关闭 NERDTree 窗口。

这些快捷键可以帮助你更高效地在 NERDTree 中进行文件和目录的导航和操作。

## 其它

### nano

- [4.4 超简单文书编辑器： nano](https://www.wenjiangs.com/doc/8ij4xm4z)

### Source Insight

- [Source Insight 查看函数调用关系使用技巧](https://blog.csdn.net/lyndon_li/article/details/121504277)
