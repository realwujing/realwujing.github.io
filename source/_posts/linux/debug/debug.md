---
date: 2024/07/23 10:29:55
updated: 2025/01/14 20:48:49
---

# debug

## 调试工具

- [<font color=Red>Linux 程序开发常用调试工具合集</font>](https://mp.weixin.qq.com/s?__biz=MzUxMjEyNDgyNw==&mid=2247483984&idx=2&sn=2672096af3ab1844e7b6d0b6e9de802b&chksm=f96870a4ce1ff9b2183f17c2873f1a35b418469dde92eb4e49f9d3de073aeafb87fe4c5b0002&scene=21#wechat_redirect)
- [Linux内核调试方法](https://blog.csdn.net/u013253075/article/details/120569270)
- [全面剖析Linux kernel的调试debug技术](https://zhuanlan.zhihu.com/p/543103513)
- [内核earlyprintk选项](https://blog.csdn.net/sinat_20184565/article/details/111875638)

## fadd2line

- [<font color=Red>https://github.com/torvalds/linux/blob/master/scripts/faddr2line</font>](https://github.com/torvalds/linux/blob/master/scripts/faddr2line)

## Binutils

- [GNU binutils 里的九种武器](https://zhuanlan.zhihu.com/p/85913402)
- [GNU Binutils简介及基本用法](https://www.cnblogs.com/tocy/p/gnu-binutils-simple-usage.html)

### addr2line

- [Linux下addr2line命令用法](https://blog.csdn.net/fengbingchun/article/details/119980076)

### nm

- [linux——nm命令：查看符号表](https://blog.csdn.net/lgfun/article/details/103600880)

#### symbols

- [deb debug package-标准的 debian / ubuntu 打 deb 包，通过将可执行文件的符号表通过剥离成独立的 dbg 包，称为 debug package](http://sunyongfeng.com/201802/linux/debian_debug_package)

- [Ubuntu 21.10 安装调试符号](https://blog.csdn.net/dwh0403/article/details/123551691)

#### 内核符号表

- [Linux System.map文件](https://blog.csdn.net/ysbj123/article/details/51233618)
- [linux之vmlinux、vmlinuz、System.map和/proc/kallsyms简介](https://blog.csdn.net/weixin_45030965/article/details/125055828)
- [Linux内核：符号表详解](https://zhuanlan.zhihu.com/p/445864686)

#### strip

- [使用strip, eu-strip, objcopy等剥离与导回符号表及调试信息](https://blog.csdn.net/nirendao/article/details/104107608)
- [17GDB使用符号表调试release程序](https://www.cnblogs.com/qiumingcheng/p/15821919.html)
- [使用GDB调试将符号表与程序分离后的可执行文件](https://www.cnblogs.com/dongc/p/9690754.html)

### strings

- <font color=Red>在当前目录下递归查找so文件中的目标字符串</font>

    ```bash
    find . -type f -name "*.so*" -exec sh -c 'strings "$0" | grep "target_string" && echo "$0"' {} \;
    ```

    ```text
    这个命令会在当前目录下递归查找所有后缀名为 .so 或者 .so.* 的文件，并使用 strings 命令提取其中的字符串，然后使用 grep 命令在字符串中查找目标字符串，如果目标字符串出现了，就输出包含目标字符串的文件名。

    具体来说，该命令使用 find 命令在当前目录下查找所有类型为文件（-type f）且文件名匹配 *.so* 的文件，然后使用 -exec 选项执行后面的命令。

    后面的命令使用 sh -c 执行，将每个匹配到的文件名（{}）传递给了 $0 变量。在命令中，先使用 strings 命令提取文件中的字符串，然后使用 grep 命令查找目标字符串是否存在，如果存在，就使用 echo 命令输出包含目标字符串的文件名。

    需要注意的是，命令中使用了单引号包围命令，以避免 Shell 解析命令中的 $0 变量和 {} 字符。同时，在命令的末尾需要使用 \; 来表示命令的结束，而不是使用分号 ;，因为分号 ; 是 Shell 的保留字符。
    ```

- [linux中的strings命令简介](https://zhuanlan.zhihu.com/p/383038723)

### objdump

- [<font color=Red>objdump反汇编用法示例</font>](https://blog.csdn.net/zoomdy/article/details/50563680)
- [反汇编代码格式](https://blog.csdn.net/kunkliu/article/details/82992361)

- objdump 反汇编代码带行号:

    ```bash
    objdump -d -l -S your_binary_file
    ```

    这个命令中的参数含义如下：

    -d：表示进行反汇编。

    -l：表示生成包含行号信息的输出。

    -S：表示同时输出源代码。

    -C：如果二进制文件包含 C++ 符号，使用 C++ 符号名进行显示，而不是使用原始符号名。

    将 your_binary_file 替换为你要反汇编的二进制文件的路径。

    这将生成一个包含反汇编代码和源代码行号信息的输出。你可以查看这个输出来分析程序的汇编代码。

  - 注意事项:[cmake-objdump](cpp/cmake-objdump)
    - cmake中使用参数-DCMAKE_BUILD_TYPE=Release编译出来的版本在coredump时生成的dmesg报错中的ip地址与使用-DCMAKE_BUILD_TYPE=Debug编出来的版本再使用objdump反汇编得到的汇编代码对不上。
    - cmkke中-DCMAKE_BUILD_TYPE=Release编译出来的版本在coredump时生成的dmesg报错中的ip地址与在CMakeLists.txt中额外添加set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")编译选项后再使用objdump反汇编得到的汇编代码对应的上。
    - cmake中-DCMAKE_BUILD_TYPE=Debug编译出来的版本后用strip得到的不含调试包的版本在coredump时生成的dmesg报错中的ip地址与使用-DCMAKE_BUILD_TYPE=Debug编出来的版本再使用objdump反汇编得到的汇编代码对应的上。

## gdb

- [<font color=Red>gdb调试常用命令</font>](https://www.cnblogs.com/tzj-kernel/p/14909077.html)
- [GDB调试](https://blog.codekissyoung.com/C%E7%A8%8B%E5%BA%8F%E8%AE%BE%E8%AE%A1/GDB%E8%B0%83%E8%AF%95%E5%99%A8)
- [了解和使用GDB调试-基础](https://www.cnblogs.com/tlam/p/15612774.html)
- [GDB调试指南(入门，看这篇够了)](https://blog.csdn.net/chen1415886044/article/details/105094688)
- [本文是一篇GDB学习笔记，总结了GDB常用命令，方便以后查阅。](http://xnzaa.github.io/2016/07/20/GDB%E8%B0%83%E8%AF%95%E5%8F%8A%E5%91%BD%E4%BB%A4%E6%B1%87%E6%80%BB/)
- [GDB调试教程：1小时玩转Linux gdb命令](http://c.biancheng.net/gdb/)

- [<font color=Red>100个gdb小技巧</font>](https://wizardforcel.gitbooks.io/100-gdb-tips/content/index.html)
- [<font color=Red>GDB实用命令</font>](https://blog.csdn.net/ljss321/article/details/104304591)
- [GDB 调试指令](https://blog.csdn.net/evilswords/article/details/18353181)
- [Linux下GDB调试指令汇总](https://zhuanlan.zhihu.com/p/71519244)
- [linux gdb详解](https://www.jianshu.com/p/adcf474f5561)
- [LINUX下GDB的使用方法(简单说说)_longfan的博客-CSDN博客_gdb](https://blog.csdn.net/awm_kar98/article/details/82840811)
- [Linux C/C++ 调试的那些“歪门邪道”](https://mp.weixin.qq.com/s/t0BidHMCGqTftchGUU5otw)

#### gef

- [<font color=Red>gdb exhanced features(GEF)工具的使用</font>](https://mp.weixin.qq.com/s/RQD1Mge_AJIOdfKMEa9ZwQ)

#### continue

- [<font color=Red>跳至下一个断点_GDB断点调试详解</font>](https://blog.csdn.net/weixin_39880632/article/details/112621585)
- [<font color=Red>linux gdb 跳出函数,gdb调试程序时跳进函数和跳出函数</font>](https://blog.csdn.net/weixin_35197990/article/details/116710685)

#### ptype

查看某个结构体或变量的类型定义:

```bash
ptype filep
```

查看某个结构体或变量的类型定义且包含偏移量信息:

```bash
ptype /o struct file
```

- [GDB调试之查看变量类型信息(十三)](https://www.cnblogs.com/TechNomad/p/17965838)
- [用gdb的ptype命令查看结构体成员偏移量](https://blog.csdn.net/zqwone/article/details/123769845)

#### print

- [<font color=Red>GDB print命令高级用法</font>](http://c.biancheng.net/view/8252.html)
- [gdb设置显示选项](https://blog.csdn.net/dai_jing/article/details/36896215)

#### 打印内存值

- [打印内存的值](https://wizardforcel.gitbooks.io/100-gdb-tips/content/examine-memory.html)
- [GDB调试查看内存数据](https://blog.csdn.net/u014470361/article/details/102230583)
- [GDB打印内存的值](https://blog.csdn.net/weixin_44395686/article/details/104727584)
- [打印ASCII和宽字符字符串](https://wizardforcel.gitbooks.io/100-gdb-tips/content/print-ascii-and-wide-string.html)

#### C++对象布局

- [GDB查看C++对象布局_tmhanks的博客-CSDN博客](https://blog.csdn.net/tmhanks/article/details/89110833)

#### list

- [gdb中list命令使用](https://blog.csdn.net/Mormont/article/details/53037978)

#### 多进程

- [为fork调用设置catchpoint](https://wizardforcel.gitbooks.io/100-gdb-tips/content/catch-fork.html)
- [同时调试父进程和子进程](https://wizardforcel.gitbooks.io/100-gdb-tips/content/set-detach-on-fork.html)
- [【工欲善其事，必先利其器】之gdb五大高级用法](https://blog.csdn.net/e21105834/article/details/118515137)

#### gdb tui

- [gdb TUI界面快捷键](https://blog.csdn.net/xiaozi0221/article/details/90512751)
- [<font color=Red>gdb调试的layout使用</font>](https://blog.csdn.net/whlloveblog/article/details/48090567)

#### gdb assembly

- [gdb反汇编disassemble](https://blog.csdn.net/qq_28499879/article/details/120670684)

    ```bash
    (gdb) disassemble /m main
    ```

- [GDB 单步调试汇编](https://www.bbsmax.com/A/mo5kQN04zw/)
- [<font color=Red>汇编语言和gdb调试汇编</font>](https://zhuanlan.zhihu.com/p/410215049)
- [静态链接符号地址重定位直观描述](https://mp.weixin.qq.com/s/cq0iHepG_xXKXHLlQpPGZw)

#### gdb设置源码路径

- [https://sourceware.org/gdb/current/onlinedocs/gdb/Source-Path.html#index-set-substitute_002dpath](https://sourceware.org/gdb/current/onlinedocs/gdb/Source-Path.html#index-set-substitute_002dpath)

- [设置源文件查找路径 dir只识别相对路径](https://wizardforcel.gitbooks.io/100-gdb-tips/content/directory.html)
- [GDB指定和修改搜素源码文件的路径（set substitute-path）](https://blog.csdn.net/jiafu1115/article/details/31790757)
- [<font color=Red>源码路径查看与设置</font>](https://www.jianshu.com/p/9c211e92d25e)
- [替换查找源文件的目录 set substitute-path from-path to-path](https://wizardforcel.gitbooks.io/100-gdb-tips/content/substitute-path.html)

    ```bash
    cd lshw-02.18.85.3
    gdb lshw
    b main
    gef➤  l
    75      in lshw.cc
    gef➤  info source
    Current source file is lshw.cc
    Compilation directory is /build/lshw-02.18.85.3/src
    Located in /build/lshw-02.18.85.3/src/lshw.cc
    Source language is c++.
    Producer is GNU C++14 8.3.0 -mtune=generic -march=x86-64 -g -O2.
    Compiled with DWARF 2 debugging format.
    Does not include preprocessor macro info.
    gef➤  set substitute-path /build/lshw-02.18.85.3/src ./src
    gef➤  l
    warning: Source file is more recent than executable.
    75      int main(int argc,
    76      char **argv)
    77      {
    78
    79      #ifndef NONLS
    80        setlocale (LC_ALL, "");
    81        bindtextdomain (PACKAGE, LOCALEDIR);
    82        bind_textdomain_codeset (PACKAGE, "UTF-8");
    83        textdomain (PACKAGE);
    84      #endif
    ```

- [<font color=Red>gdb调试解决找不到源代码的问题</font>](https://blog.csdn.net/nicholas_duan/article/details/117515155)
- [gdb分析core文件找不到源码](https://blog.csdn.net/jackgo73/article/details/120431609)
- [gdb调试解决找不到源代码的问题](https://blog.csdn.net/albertsh/article/details/107437084)

#### gdb打印qt数据类型

- [https://github.com/Lekensteyn/qt5printers](https://github.com/Lekensteyn/qt5printers)

- [gdb调试qt程序时打印qt特有的类型数据](https://listenerri.com/2018/10/23/gdb%E8%B0%83%E8%AF%95qt%E7%A8%8B%E5%BA%8F%E6%97%B6%E6%89%93%E5%8D%B0qt%E7%89%B9%E6%9C%89%E7%9A%84%E7%B1%BB%E5%9E%8B%E6%95%B0%E6%8D%AE/)
- [解决GDB输出Qt内置类型的显示问题](https://www.cnblogs.com/rickyk/p/4184912.html)
- [GDB && QString](https://www.cnblogs.com/Braveliu/p/8426945.html)
- [007 - 配置 Clion 调试显示 Qt 变量-爱代码爱编程](https://icode.best/i/64401545957413)

#### gdb print errno

- [errno全局变量及使用细则，C语言errno全局变量完全攻略](http://c.biancheng.net/c/errno/)
- [Linux(程序设计):08-perror、strerror函数(errno全局变量)](https://blog.51cto.com/u_15346415/5094459)
- [Linux errno 错误对照表](https://blog.csdn.net/Gpengtao/article/details/7553307)
- [【博客272】errno错误对照表](https://blog.csdn.net/qq_43684922/article/details/106440542)
- [gcc 7.1.0下gdb无法prinf查看errno解决](https://blog.csdn.net/liuhhaiffeng/article/details/104040174)

#### gdb远程调试

- [40.Linux应用调试-使用gdb和gdbserver](https://cloud.tencent.com/developer/article/1015873)
- [使用gdbserver远程调试](https://www.jianshu.com/p/d532d196c89f)
- [<font color=Red>服务/软件管理：38-gdb+gdbserver的使用</font>](https://blog.51cto.com/u_15346415/3678651)
- [gdb远程及本地调试的一些技巧](https://www.cnblogs.com/seven-sky/p/4730225.html)
- [使用GDB进行嵌入式远程调试](https://blog.csdn.net/lvwx369/article/details/121490883)

#### gcc

- [Dwarf Error: wrong version in compilation unit header (is 4, should be 2) \[in module /dawnfs/users/](https://blog.csdn.net/fandroid/article/details/32914203)

#### libtool

- [gdb调试libtool封装的可执行文件](https://www.cnblogs.com/ericsun/p/3168842.html)
- [使用 GNU Libtool 创建库](https://blog.csdn.net/rainharder/article/details/8057819)
- [gdb not in executable format file format not recognized](https://blog.csdn.net/abcd1f2/article/details/49816751)

#### vscode gdb

- [一步一步学CMake 之 VSCode+CMakeLists 调试 C++ 工程_wanzew的博客-CSDN博客](https://blog.csdn.net/wanzew/article/details/83097457)
- [VSCode 无法打开 libc-start.c - Zijian/TENG - 博客园 (cnblogs.com)](https://www.cnblogs.com/tengzijian/p/vscode-cannot-find-libc-start-c.html)
- [visualstudio-launch-json-reference](https://code.visualstudio.com/docs/cpp/launch-json-reference)
- [CentOS下sudo免密配置](https://www.jianshu.com/p/22effba56f7e)
- [VS Code 下以 root 用户调试程序](https://www.jianshu.com/p/368e5de24cc9)
- [VS Code 下以 root 用户调试程序](https://www.mycat.wiki/archives/769)
- [Linux deepin下普通用户免密切换至root用户](https://zhangxueliang.blog.csdn.net/article/details/110701868)
- [process - Visual Studio Code，调试子进程不起作用](https://www.coder.work/article/7603523)
- [VSCode 同时调试2个或多个程序](https://blog.csdn.net/leon_zeng0/article/details/107438624)
- [在vscode中调试nginx源码](https://blog.hufeifei.cn/2021/10/C-C++/vscode-debug-nginx/index.html)

- [设置vscode命令行其缓冲区中保留的最大行数](https://blog.csdn.net/wzp20092009/article/details/118327205)

#### vscode调试linux内核

- [<font color=Red>调试 Linux 最早期的代码</font>](https://mp.weixin.qq.com/s/cx_vaRTcC29h0pWkJPpqQQ)
- [<font color=Red>https://github.com/yuan-xy/Linux-0.11</font>](https://github.com/yuan-xy/Linux-0.11)
- [<font color=Red>Linux 0.11 vscode + gdb调试环境搭建</font>](https://www.modb.pro/db/422613)
- [利用vscode远程调试Linux内核](https://mp.weixin.qq.com/s/vb1SiI0Uc5KpU2yGwJBRmg)

## 串口

- [<font color=Red>Linux下常用的串口助手 —— minicom、putty、cutecom</font>](https://blog.csdn.net/Mculover666/article/details/87647810)

    主机上执行：

    ```bash
    sudo minicom -s -D /dev/ttyUSB0
    ```

    `ls -l /dev/ttyUSB0`找不到设备时可以尝试手动添加:

    ```bash
    lsusb
    ```

    ![ls -l /dev/ttyUSB0](https://cdn.jsdelivr.net/gh/realwujing/picture-bed/20231206170018.png)

    ```bash
    echo '067b 23a3' > /sys/bus/usb-serial/drivers/generic/new_id
    ```

    被调式机上执行：

    ```bash
    sudo cutecom
    ```

- 查询串口波特率：

    ```bash
    stty -F /dev/ttyS0
    ```

    其中 /dev/ttyS0 是串口设备文件的路径。你可以将路径更改为你所关心的特定串口。

    这将返回串口的配置，包括波特率、数据位、校验位、停止位等。通常，波特率会显示在配置输出中，如下所示：

    ```text
    speed 9600 baud; line = 0;
    ```

- 设置串口波特率

    ```bash
    stty -F /dev/ttyS0 9600
    ```

    在上面的命令中：

    /dev/ttyS0 是串口设备文件的路径，你可以根据需要更改为你使用的串口。

    9600 是所需的波特率。你可以将其替换为你想要的任何波特率。

    运行此命令后，串口 /dev/ttyS0 的波特率将设置为 9600。确保串口没有被其他程序占用，以便成功设置波特率。

## kgdb

- [linux内核调试（七）使用kdb/kgdb调试内核](https://zhuanlan.zhihu.com/p/546416941)
- [Using kgdb, kdb and the kernel debugger internals](https://www.kernel.org/doc/html/latest/dev-tools/kgdb.html?highlight=kgdb)
- [使用KGDB调试Linux驱动（以imx6ull开发板为例）](https://blog.csdn.net/weixin_38832162/article/details/115347640)
- [用 kGDB 调试 Linux 内核 ](https://www.cnblogs.com/qiynet/p/6432717.html)
- [使用kgdb调试Vagrant guest kernel](https://www.jianshu.com/p/5418908bc883)

- [kgdb调试linux内核以及驱动模块](https://blog.csdn.net/weixin_39871788/article/details/120313821)
- [内核调试- vmlinux-gdb.py无法在gdb上运行](https://cloud.tencent.com/developer/ask/sof/249935)
- [kgdb调试linux内核以及驱动模块](https://quard-star-tutorial.readthedocs.io/zh_CN/latest/ext1.html)

### kgdboe

- [How to use kgdb over ethernet (kgdboe)?](https://stackoverflow.com/questions/21300420/how-to-use-kgdb-over-ethernet-kgdboe)

## ptrace

- [Linux GDB的实现原理](https://www.toutiao.com/article/7199644016760554018)
- [一文带你看透 GDB 的 实现原理  ptrace真香](https://blog.csdn.net/Z_Stand/article/details/108395906)
- [断点原理与实现](https://zhuanlan.zhihu.com/p/110793460?utm_id=0)
- [硬件断点和软件断点的区别](https://blog.csdn.net/D_R_L_T/article/details/80632311)
- [ROM, FLASH和RAM的区别](https://zhuanlan.zhihu.com/p/38339306?utm_id=0)
- [调试器工作原理CPU软件断点/硬件断点/单步执行标识](https://blog.csdn.net/gengzhikui1992/article/details/111856016)
- [硬件断点](https://cataloc.gitee.io/blog/2020/09/19/%E7%A1%AC%E4%BB%B6%E6%96%AD%E7%82%B9/)
- [内存断点](https://cataloc.gitee.io/blog/2020/09/18/%E5%86%85%E5%AD%98%E6%96%AD%E7%82%B9/)
- [软件断点](https://cataloc.gitee.io/blog/2020/09/17/%E8%BD%AF%E4%BB%B6%E6%96%AD%E7%82%B9/)
- [Linux ptrace系统调用详解：利用 ptrace 设置硬件断点](https://blog.csdn.net/Rong_Toa/article/details/112155847)
- [watch 命令实现监控机制的方式有 2 种，一种是为目标变量（表达式）设置硬件观察点，另一种是为目标变量（表达式）设置软件观察点。](http://c.biancheng.net/view/8191.html)
- [Linux内核：自己动手写一个GDB设置断点（原理篇）](https://www.toutiao.com/article/7127950932549272094)

## core文件

- [Linux下使用gdb调试core文件](https://cloud.tencent.com/developer/article/1177442)
- [ulimit命令 用来限制系统用户对shell资源的访问](https://wangchujiang.com/linux-command/c/ulimit.html)
- [linux：永久打开core文件功能](https://www.cnblogs.com/wangcp-2014/p/15466518.html)
- [Linux生成core文件、core文件路径设置](https://blog.csdn.net/u011417820/article/details/71435031)
- [golang-进程崩溃后如何输出错误日志？](https://blog.csdn.net/xmcy001122/article/details/105665732)
- [【Linux】core文件存储位置和命名](https://zhengyun.blog.csdn.net/article/details/107365187)
- [一文读懂 | coredump文件是如何生成的](https://cloud.tencent.com/developer/article/1860631)
- [linux下产生core文件以及不产生core文件的条件](https://www.cnblogs.com/huixinquan/p/14941880.html)
- [<font color=Red>程序死掉了，没有 core 文件怎么查？</font>](https://blog.csdn.net/qq_36580990/article/details/125540487)

    ```c
    core[1161856]: segfault at 0 ip 0000000000401136 sp 00007ffcdbaf3240 error 6 in core[401000+1000] likely on CPU 2 (core 2, socket 0)
    ```

  - core[1161856]: 这可能是指示生成的 core dump 文件名。在这里，core 文件名为 "core"，后缀为 "1161856"，这可能是一个进程 ID 或其他唯一标识符。

  - segfault at 0: 这表示一个段错误（Segmentation Fault）发生在地址 0 处。这通常是因为程序试图访问一个无效的内存地址。

  - ip 0000000000401136: 这是导致段错误的指令的地址。在这里，ip（Instruction Pointer）指向地址 0000000000401136 的指令。

  - sp 00007ffcdbaf3240: 这是栈指针（Stack Pointer），指向栈的当前顶部。在这里，sp 指向地址 00007ffcdbaf3240。

  - error 6: 这是一个错误代码，表示导致段错误的原因。在 Linux 中，错误代码 6 通常表示尝试执行非法的指令。

  - core[401000+1000]: 这表示 core dump 文件的内存范围。在这里，core 文件捕获了从地址 401000 开始的 1000 个字节的内存内容，1000一般指向了_init符号。

  - likely on CPU 2 (core 2, socket 0): 这是指示错误可能发生在第二个 CPU 核心上，该核心位于第一个物理 CPU 插槽上。

- [Linux如何使用gdb查看core堆栈信息](https://www.cnblogs.com/lynsen/p/8439550.html)
- [<font color=Red>golang-进程崩溃后如何输出错误日志？core dump</font>](https://blog.csdn.net/xmcy001122/article/details/105665732)

### coredumpctl

- [coredumpctl - Retrieve and process saved core dumps and metadata](https://www.man7.org/linux/man-pages/man1/coredumpctl.1.html)
- [使用systemd的coredump工具分析程序崩溃问题](https://blog.csdn.net/wentian901218/article/details/119845991)
- [18 使用 systemd-coredump 针对应用程序崩溃进行调试](https://documentation.suse.com/zh-cn/sles/15-SP2/html/SLES-all/cha-tuning-systemd-coredump.html)

## dmesg

- [Linux 命令： dmesg | uname](https://mp.weixin.qq.com/s/sQIoEE0iTxlVQDxhgBbY3A)
- [Linux命令之dmesg命令](https://blog.csdn.net/carefree2005/article/details/120737841)
- [Linux dmesg命令](https://mp.weixin.qq.com/s/6qpMiy6L5qIazmqNopCd0A)

## dump_stack

- [【Linux内核调试-dump_stack】](https://blog.csdn.net/cddchina/article/details/125175118)
- [dump_stack 实现分析](https://blog.csdn.net/sunshineywz/article/details/105763755)

## sysctl

- [内核参数说明](https://www.cnblogs.com/tolimit/p/5065761.html)

## oops

- [<font color=Red>Linux 死机复位(oops、panic)问题定位指南</font>](https://blog.csdn.net/pwl999/article/details/106931608)
- [Linux内核调试的方式以及工具集锦](https://github.com/gatieme/LDD-LinuxDeviceDrivers/blob/master/study/debug/tools/systemtap/01-install/README.md)
- [如何开机进入Linux命令行](https://www.linuxprobe.com/boot-into-linuxcli.html)
- [kernel oops (Unable to handle kernel paging request at virtual address )三种内存访问异常](https://blog.csdn.net/xl19862005/article/details/107605906)
- [LSM Oops 内存错误根因分析与解决](https://www.toutiao.com/article/6868133266415845892)

- [【Linux】内核oops/缺页异常分析](https://blog.csdn.net/qq_21688871/article/details/131114564)
- [ARM Linux Oops使用小结](http://blog.chinaunix.net/uid-27159438-id-3280213.html)

## panic

- [<font color=Red>[linux][异常检测] hung task, soft lockup, hard lockup, workqueue stall</font>](https://blog.csdn.net/weixin_38184628/article/details/136208515)
- [<font color=Red>Softlockup与hardlockup检测机制(又名:nmi_watchdog)</font>](https://www.kernel.org/doc/html/latest/translations/zh_CN/admin-guide/lockup-watchdogs.html)
- [一起学习64位ARM平台稳定性分析：遇见内核 panic](https://mp.weixin.qq.com/s/d0vJgQYVDB3ZiQRMoDvvDw)
- [Linux内核故障分类和排查](https://blog.csdn.net/rikeyone/article/details/105691909)
- [<font color=Red>这是你了解的空指针吗？</font>](https://mp.weixin.qq.com/s/RPS_QernHHgBBKBlFJksHA)
- [Linux内核panic核心执行逻辑](https://blog.csdn.net/tugouxp/article/details/128834562)

更改 sysctl 中的某些内核参数可能导致系统不稳定，甚至触发 panic。以下是一些可能会导致系统问题的内核参数，慎重修改：

- kernel.panic

    作用： 设置系统 panic 的延迟时间。

    潜在风险： 如果将其设置得太低，系统可能会在不必要的情况下触发 panic。

    ```bash
    sysctl -w kernel.panic=10
    ```

    内核编译选项：CONFIG_PANIC_TIMEOUT=0

  - [<font color=Red>Linux kernel Panic后自动重启机器的设置</font>](https://blog.csdn.net/yihui8/article/details/46480627)
  - [解读内核 sysctl 配置中 panic、oops 相关项目](https://blog.csdn.net/Longyu_wlz/article/details/126565853)

- kernel.panic_on_oops

    作用： 控制在发生内核 oops（可修复的内核错误）时是否触发 panic。

    潜在风险： 如果启用，系统在 oops 时会触发 panic。

    ```bash
    sysctl -w kernel.panic_on_oops=1
    ```

    内核编译选项：PANIC_ON_OOPS=y

- kernel.hung_task_panic

    作用： 控制在系统检测到“挂起”任务（可能是由于死锁）时是否触发 panic。

    潜在风险： 如果启用，系统在检测到挂起任务时会触发 panic。

    ```bash
    sysctl -w kernel.hung_task_panic=1
    ```

    内核编译选项：CONFIG_DETECT_HUNG_TASK=y CONFIG_BOOTPARAM_HUNG_TASK_PANIC=y

- kernel.hung_task_timeout_secs：

    作用： 该参数用于设置系统在检测到任务挂起（hanging task）的超时时间。如果系统中的某个任务在指定的时间内没有恢复正常运行，内核将记录信息并采取相应的措施。

    默认值： 通常情况下，kernel.hung_task_timeout_secs 的默认值是 120 秒（2 分钟）。

    潜在风险： 如果将其设置得太低，系统可能会对一些正常但需要一定时间来完成的任务误报为挂起状态。

    ```bash
    sysctl -w kernel.hung_task_timeout_secs=60
    ```

    内核编译选项：CONFIG_DEFAULT_HUNG_TASK_TIMEOUT=120

- vm.panic_on_oom

    作用： 控制在发生内存耗尽（OOM）时是否触发 panic。

    潜在风险： 如果启用，系统在内存不足时会触发 panic。

    ```bash
    sysctl -w vm.panic_on_oom=1
    ```

- kernel.softlockup_panic

    作用： 软死锁是指内核中的一个任务（线程）由于长时间未能释放CPU而导致系统失去响应时是否触发 panic。

    潜在风险： 如果启用，系统在检测到软锁定时会触发 panic。

    ```bash
    sysctl -w kernel.softlockup_panic=1
    ```

    内核编译选项：CONFIG_SOFTLOCKUP_DETECTOR=y BOOTPARAM_SOFTLOCKUP_PANIC=y

- kernel.panic_on_warn

  作用： 这个参数控制内核在遇到某些非致命性错误（例如警告）时是否触发 panic。

  0： 禁用自动重启。内核在遇到警告时不会触发系统重启，而是继续正常运行。

  1： 启用自动重启。当内核发生某些警告时，系统会自动重启。

  ```bash
    sysctl -w kernel.panic_on_warn=1
    ```

  在调试或特殊情况下，禁用自动重启可能有助于保留内核警告的信息以进行故障排除。在生产环境中，通常将其设置为非0，以确保系统在遇到某些严重问题时能够自动重启，尽早恢复正常运行状态。

`sysctl -w kernel.hung_task_panic=1`、`echo 1 > /proc/sys/kernel/hung_task_panic`这两种方法都是在运行时直接生效的，而且都是暂时性的修改。

如果你希望修改是持久的，可以将相应的配置写入 `/etc/sysctl.conf` 文件，并使用 `sysctl -p` 命令使其生效。

```bash
sudo cat << EOF >> /etc/sysctl.conf

# sysrq

# CONFIG_MAGIC_SYSRQ=y
kernel.sysrq=1

# panic

# CONFIG_PANIC_TIMEOUT=0
kernel.panic=10

# CONFIG_PANIC_ON_OOPS=y
kernel.panic_on_oops=1

# CONFIG_DETECT_HUNG_TASK=y CONFIG_BOOTPARAM_HUNG_TASK_PANIC=y
kernel.hung_task_panic=1

# CONFIG_DEFAULT_HUNG_TASK_TIMEOUT=120
kernel.hung_task_timeout_secs=60

vm.panic_on_oom=1

# CONFIG_SOFTLOCKUP_DETECTOR=y BOOTPARAM_SOFTLOCKUP_PANIC=y
kernel.softlockup_panic=1

kernel.panic_on_warn=1
EOF
```

```bash
sysctl -p
```

### 任务挂起和睡眠的区别？

任务挂起（hanging task）和睡眠（sleep）是两个概念，涉及到系统中运行的进程和线程的状态。

任务挂起（Hanging Task）：

- 定义： 任务挂起指的是系统中的某个任务（通常是一个进程或线程）由于某种原因而无法继续正常执行，长时间处于一种不响应的状态。

- 原因： 挂起可能是由于死锁、资源争用、错误的程序行为或其他系统问题引起的。

- 检测： 内核通过监视任务的执行状态和超时机制来检测是否有任务挂起。

- 处理： 一旦检测到任务挂起，系统可能会采取相应的措施，例如记录相关信息、触发系统 panic，以及尝试恢复任务的正常执行。

睡眠（Sleep）：

- 定义： 睡眠指的是一个任务主动放弃 CPU 并进入一种等待状态，等待某个事件的发生。在睡眠期间，任务通常不占用 CPU 时间，并允许其他任务执行。

- 原因： 任务可能进入睡眠状态以等待外部事件，例如等待 I/O 操作完成、等待定时器触发、等待信号量或锁的释放等。

- 检测： 睡眠通常是由任务自己通过系统调用（如 sleep、wait）或内核操作引起的，不同于挂起的 passivity（被动性）。

- 处理： 睡眠是一种正常的任务状态，系统不会像在检测到挂起时那样采取紧急措施。任务会在等待的事件发生时被唤醒，继续执行。

总的来说，任务挂起通常是一种异常状态，可能导致系统不稳定，而睡眠是一种正常的、被控制的状态，允许任务在需要时主动放弃 CPU 并等待特定条件的发生。

## watchdog

- [禁用watchdog方法汇总](https://cloud.tencent.com/developer/article/1843976)
- [Linux watchdog配置](https://blog.csdn.net/jiexijihe945/article/details/128021600)
- [Linux禁用watchdog](https://blog.csdn.net/qq_28278079/article/details/104218588)

  ```text
  nmi_watchdog=   [KNL,BUGS=X86] Debugging features for SMP kernels
                        Format: [panic,][nopanic,][num]
                        Valid num: 0 or 1
                        0 - turn hardlockup detector in nmi_watchdog off
                        1 - turn hardlockup detector in nmi_watchdog on
                        When panic is specified, panic when an NMI watchdog
                        timeout occurs (or 'nopanic' to not panic on an NMI
                        watchdog, if CONFIG_BOOTPARAM_HARDLOCKUP_PANIC is set)
                        To disable both hard and soft lockup detectors,
                        please see 'nowatchdog'.
                        This is useful when you use a panic=... timeout and
                        need the box quickly up again.

                        These settings can be accessed at runtime via
                        the nmi_watchdog and hardlockup_panic sysctls.
  ```

## kdump

- [<font color=Red>Documentation for Kdump - The kexec-based Crash Dumping Solution</font>](https://www.kernel.org/doc/html/latest/admin-guide/kdump/kdump.html)
- [<font color=Red>How to on enable kernel crash dump on Debian Linux</font>](https://www.cyberciti.biz/faq/how-to-on-enable-kernel-crash-dump-on-debian-linux/)
- [15.10. 防止内核驱动程序为 kdump 加载](https://docs.redhat.com/zh-cn/documentation/red_hat_enterprise_linux/8/html/managing_monitoring_and_updating_the_kernel/preventing-kernel-drivers-from-loading-for-kdump_configuring-kdump-on-the-command-line#preventing-kernel-drivers-from-loading-for-kdump_configuring-kdump-on-the-command-line)

    ```bash
    sudo apt install kdump-tools crash kexec-tools makedumpfile linux-image-$(uname -r)-dbg
    ```

    ```bash
    uname -a
    Linux wujing-PC 5.15.77-amd64-desktop #2 SMP Thu Jun 15 16:06:18 CST 2023 x86_64 GNU/Linux
    cat /etc/default/grub.d/kdump-tools.cfg
    GRUB_CMDLINE_LINUX_DEFAULT="$GRUB_CMDLINE_LINUX_DEFAULT crashkernel=384M-:128M"
    ```

    ```bash
    uname -a
    Linux wujing-PC 4.19.0-arm64-desktop-tyy-5312 #5312 SMP Tue Dec 12 10:52:22 CST 2023 aarch64 GNU/Linux
    cat /etc/default/grub.d/kdump-tools.cfg
    GRUB_CMDLINE_LINUX_DEFAULT="$GRUB_CMDLINE_LINUX_DEFAULT crashkernel=2G-4G:320M,4G-32G:512M,32G-64G:1024M,64G-128G:2048M,128G-:4096M"
    ```

    ```bash
    echo 1 > /proc/sys/kernel/sysrq
    echo c > /proc/sysrq-trigger
    ```

- [详解Linux内核态调试工具kdump](https://blog.csdn.net/chenlycly/article/details/126074433)
- [<font color=Red>Linux内核转储-Kdump，Crash使用介绍</font>](https://blog.csdn.net/qq_41782149/article/details/129021833)
- [<font color=Red>Ubuntu 20.04 Kdump + Crash 初体验</font>](https://ebpf.top/post/ubuntu_kdump_crash/)
- [ubuntu 20.04 启用kdump服务及下载vmlinux](https://blog.csdn.net/weixin_42915431/article/details/112555690)
- [Linux内核调试之kdump](https://blog.csdn.net/yhb1047818384/article/details/104115915)
- [Linux Kdump内核崩溃转储部署详解](https://blog.csdn.net/ludaoyi88/article/details/114194687)
- [Ubuntu的内核转储工具](https://www.cnblogs.com/wwang/archive/2010/11/19/1881304.html)
- [<font color=Red>centos7 kdump、crash调试内核</font>](https://blog.csdn.net/weixin_45030965/article/details/124960224)
- [Linux Kdump 机制详解](https://www.toutiao.com/article/7103352500777910821/)
- [x86 and x86_64 - Some systems can take advantage of the nmi watchdog. Add nmi_watchdog=1 to the boot commandline to turn on the watchdog. The nmi interrupt will call panic if activated.](https://manpages.debian.org/testing/kdump-tools/kdump-tools.5.en.html)

## carsh

crash 是一个用于分析 Linux 内核转储文件（core dump）的工具，允许在不中断系统运行的情况下进行诊断。以下是 crash 的一些基本原理：

- 核心文件： crash 的核心原理是基于核心文件的分析。内核转储文件（core dump）记录了系统在发生关键错误时的内存和寄存器状态。crash 使用这些核心文件进行分析。

- 调试信息： 为了正确解析核心文件，crash 需要访问内核二进制文件（通常是 vmlinux）。这个文件包含有关内核符号、数据结构和函数的调试信息。在使用 crash 之前，你需要确保有匹配正在运行内核的 vmlinux 文件。

- 在线调试： crash 允许在线调试运行中的内核，而无需停止系统。它通过访问 /proc/kcore 文件获取正在运行内核的内存映像，结合核心文件和调试信息，提供了对内核状态的深入分析。

- 命令式界面： crash 提供了一个交互式的命令行界面，用户可以在其中运行各种命令以查看内核状态、进程信息、调度器信息等。这样用户可以动态地检查系统状态。

- 插件系统： crash 具有可扩展的插件系统，可以通过插件添加额外的命令和功能。这使得用户可以根据需要定制分析环境。

- 安全性和谨慎使用： 尽管 crash 在不中断系统的情况下进行诊断，但仍需注意，对于生产环境，谨慎使用是很重要的。某些 crash 命令可能会对系统产生影响，因此建议在测试环境中使用。

总的来说，crash 提供了一种非常强大的工具，用于在不影响系统运行的情况下进行 Linux 内核的调试和分析。

```bash
git clone https://github.com/crash-utility/crash.git
cd crash/
git checkout -b 8.0.4 8.0.4
make -j16
sudo make install
```

- [<font color=Red>【调试】crash使用方法</font>](https://blog.csdn.net/qq_16933601/article/details/130446840)
- [Linux crash 调试环境搭建](https://blog.csdn.net/qq_42931917/article/details/108236139)
- [linux内核学习-Linux内核程序调试工具Crash的安装](https://www.cnblogs.com/ssyfj/p/16278883.html)
- [dump分析工具_ubantu18.04内核奔溃调试工具Crash的搭建](https://blog.csdn.net/weixin_39545102/article/details/111215997)
- [<font color=Red>用crash tool观察ARM64 Linux地址转换</font>](https://mp.weixin.qq.com/s/Hp-n9n3M9zy92Xm-dnRUZQ)
- [CRASH安装和调试](https://www.toutiao.com/article/6903790073377063428/)
- [Linux crash Dump分析](https://mp.weixin.qq.com/s/ReTyAr8gSWV-ee4mvPpGYg)

### fedora

- [fedora How to use kdump to debug kernel crashes](https://fedoraproject.org/wiki/How_to_use_kdump_to_debug_kernel_crashes)
- [fedora Linux kernel debug](https://discussion.fedoraproject.org/t/linux-kernel-debug/84998/4)

仅为一个命令启用存储库：

```bash
sudo dnf --enablerepo=fedora-debuginfo,updates-debuginfo install kernel-debuginfo
```

永久启用存储库（可能不是您想要的）：

```bash
sudo dnf config-manager --set-enabled fedora-debuginfo updates-debuginfo
```

禁用存储库：

```bash
sudo dnf config-manager --set-disabled fedora-debuginfo updates-debuginfo
```

推荐使用`sudo dnf debuginfo-install kernel`安装内核调试包：

```bash
sudo dnf debuginfo-install kernel
```

### eval命令

- [crash —— 自带的计算器和转换器](https://www.cnblogs.com/pengdonglin137/p/17725310.html)

有时我们想知道一个数字的那些位是1，那么需要在数字前面使用-b参数:

```bash
crash> cpumask 0xffff88b48aa23f20 -x
struct cpumask {
  bits = {0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x90004000, 0x0, 0x1, 0xffffc9000d20c000, 0x420804000000002, 0x0, 0x0, 0x3700000000, 0x4, 0xfffb706a, 0xffff88ea8be9c000, 0x3700000037, 0x7800000000, 0x7800000078, 0x0, 0xffffffff82260540, 0x100000, 0x400000, 0x100000, 0x1, 0x0, 0x0, 0xffff88b48aa240b0, 0xffff88b48aa240b0, 0x0, 0xd4da92e6, 0x26554, 0xffffffffff3b4a54, 0x1fe63, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff88b492fe9a00, 0x0, 0x0, 0x0, 0x0, 0xd4da9000, 0x1a, 0x1a, 0x2a400006be6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff88b48aa24280, 0xffff88b48aa24280, 0x0, 0x0, 0x64, 0x0, 0x0, 0xffff88b492fe9bc0, 0x0, 0xffffffff83149580, 0xffff88b48aa242d0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
}
crash> eval -b 0x3  //vcpu0的SMT调度域包含vcpu0和vcpu1一对超线程
   bits set: 1 0
crash>
```

### kmem

查看内存的使用统计信息:

```bash
kmem -i
```

查看slab的信息:

```bash
kmem -s > kmem_s.log
```

按slabs降序：

```bash
cat kmem_s.log | sort -k5,5nr | head -n10
CACHE             OBJSIZE  ALLOCATED     TOTAL  SLABS  SSIZE  NAME
ffff8003ff689900      304    3563564   7039393  282756    16k  kmemleak_object
ffff8003ad115800      192    1597195   1601956  51676    16k  dentry(664:aTrustDaemon.service)
ffff80037d372700     1072     409123    409620  17826    32k  ext4_inode_cache(904:session-1.scope)
ffff80037d375800      192     307034    310121  10007    16k  dentry(904:session-1.scope)
ffff8003e87c8b00      128     332086    332185   9491    16k  kernfs_node_cache
ffff80037d375480      144     147286    172328   5213    16k  buffer_head(904:session-1.scope)
ffff8003ff688400      128      86102     86450   3458    16k  kmalloc-128
ffff80037d375b80      192      63328     63365   2051    16k  vm_area_struct(904:session-1.scope)
ffff80037d374680       64      37358     37400   1873     8k  anon_vma_chain(904:session-1.scope)
ffff8003954a1580       40      34932     34944   1664     8k  ext4_extent_status
```

## sysrq-trigger

- [<font color=Red>Linux Magic System Request Key Hacks</font>](https://www.kernel.org/doc/html/latest/translations/zh_CN/admin-guide/sysrq.html)
- [【调试】sysRq按键使用方法](https://zhuanlan.zhihu.com/p/608948166)
- [/proc/sysrq-trigger 详解](https://cloud.tencent.com/developer/article/2139743)
- [Linux死机解决办法](https://blog.csdn.net/openswc/article/details/9105071)

“Alt+PrtSc+C”：手动触发kdump，触发后服务器会自动重启。（正常情况下勿按该组合键。）

## pstore

- [linux获取oops的dmesg之ramoops](https://blog.csdn.net/faxiang1230/article/details/103778193)
- [Linux pstore 实现自动“抓捕”内核崩溃日志](https://www.toutiao.com/article/6888542275027075591/)

## windebug

- [windebug快速使用及调试注意事项](https://blog.csdn.net/pathfinder1987/article/details/86620985)
- [windebug指令详解](https://blog.csdn.net/sunboyhch/article/details/37914727)

## 其他

- [https://wiki.archlinux.org/title/Debugging](https://wiki.archlinux.org/title/Debugging)
- [ASCII码一览表，ASCII码对照表](http://c.biancheng.net/c/ascii/)
- [16进制到文本字符串](https://www.bejson.com/convert/ox2str/)
