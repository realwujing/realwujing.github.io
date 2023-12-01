# vim

## vim

- [设置默认文本编辑器](https://www.debian.org/doc/manuals/debian-reference/ch01.zh-cn.html#_setting_a_default_text_editor)
- [<font color=Red>Linux vi/vim - 菜鸟教程</font>](https://www.runoob.com/linux/linux-vim.html)

- [<font color=Red>vi 和vim 的区别</font>](https://blog.csdn.net/qq_34306360/article/details/78720090)

- [<font color=Red>VIM常用命令</font>](https://blog.csdn.net/weixin_44441367/article/details/124810525)

- [<font color=Red>vim如何删除全文</font>](https://blog.csdn.net/weixin_54227557/article/details/122782139)

- [12 个超实用的 vim 编辑技巧](https://mp.weixin.qq.com/s/MyV3ZC7A7vdWpQ1lt3d_dg)

## 函数跳转

- [ubuntu下vim安装ctags工具](https://blog.csdn.net/m0_37624499/article/details/90705658)
- [Linux——vim插件之ctags的安装与配置](https://www.cnblogs.com/oddcat/articles/9678044.html)
- [vim多窗口，常用命令集](http://t.zoukankan.com/quant-lee-p-6659696.html)
- [<font color=Red>vim之函数跳转功能</font>](https://blog.csdn.net/ballack_linux/article/details/71036072)
- [vim -t 选项](https://blog.csdn.net/qwaszx523/article/details/77838855)

## 折叠

- [Vim折叠操作(折叠代码、折叠函数、方法、类等)](https://blog.csdn.net/weixin_43971252/article/details/123218379)

- [【Linux基础】vim如何显示文件名称](http://t.zoukankan.com/happyamyhope-p-11906183.html)

- [vim小技巧：多行行首插入、删除、替换](https://www.toutiao.com/article/7167342849502446114/)
- [vim 基本命令查找和替换](https://www.cnblogs.com/woshimrf/p/vim.html)
- [<font color=Red>vim 多行操作(7)</font>](https://waliblog.com/2019/05/06/vim-7.html)
- [Linux——VIM学习选取多行（转）](https://blog.csdn.net/sinat_36053757/article/details/78183506)
- [【Linux】Vim编辑器-批量注释与反注释](https://blog.csdn.net/xiajun07061225/article/details/8488210)

- [Linux下快速收起vim 并暂存修改](https://blog.csdn.net/lambert310/article/details/77659417)
- [vim使用技巧-如何暂时返回终端](https://blog.csdn.net/vastz/article/details/120064236)

- [vim 技巧 – 查找的时候忽略大小写](https://xwsoul.com/posts/472)

- [vim鼠标模式打开与关闭](https://shagain.club/index.php/archives/382/)
- [彻底解决基于Debian发行系统的vim鼠标模式(可视模式)问题](https://blog.51cto.com/u_15083238/2600714)

- [vim操作之多窗格,多文件的编辑和操作](https://www.cnblogs.com/yukina/p/16401529.html)
- [在Vim中同时打开多个文件的相关操作技巧](https://www.bilibili.com/read/cv16390641)

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
wget -O ~/.vimrc https://github.com/realwujing/realwujing.github.io/blob/main/linux/vim/.vimrc
vim .
:PluginInstall
```

## nano

- [4.4 超简单文书编辑器： nano](https://www.wenjiangs.com/doc/8ij4xm4z)
