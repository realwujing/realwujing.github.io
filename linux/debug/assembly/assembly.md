# 汇编语言学习资源

- [第2期 | x86指令集和ARM指令集的区别——从底层原理的角度来理解](https://mp.weixin.qq.com/s/qqjvXXKG-3TtUFeccQ7ZGQ)
- [汇编：函数以及函数参数传递](https://mp.weixin.qq.com/s/ntctTSDp9FigAbblK6LvgA)

## 汇编语言

- [<font color=Red>图文详解通俗易懂的汇编语言寄存器</font>](https://www.jb51.net/article/230062.htm)
- [<font color=Red>二进制分析实战：x86汇编快速入门</font>](https://www.toutiao.com/article/7039892076707807748/)

- [<font color=Red>Linux 下 64 位汇编语言</font>](https://blog.codekissyoung.com/%E7%AC%AC%E9%9B%B6%E5%B1%82/64%E4%BD%8DCPU%E6%B1%87%E7%BC%96%E8%AF%AD%E8%A8%80)
- [<font color=Red>32 位汇编语言</font>](https://blog.codekissyoung.com/%E7%AC%AC%E9%9B%B6%E5%B1%82/32%E4%BD%8DCPU%E6%B1%87%E7%BC%96%E8%AF%AD%E8%A8%80)
- [<font color=Red>16 位汇编语言</font>](https://blog.codekissyoung.com/%E7%AC%AC%E9%9B%B6%E5%B1%82/16%E4%BD%8DCPU%E6%B1%87%E7%BC%96%E8%AF%AD%E8%A8%80)

- [<font color=Red>Linux 汇编语言程序设计 (使用 NASM)</font>](https://www.dazhuanlan.com/alzheimers/topics/1114443)

- [开始在Linux下使用汇编语言](https://www.jianshu.com/p/1bae868660ae)
- [<font color=Red>汇编语言「Linux/x86-64」・第一步</font>](https://www.bxtkezhan.xyz/post/022-asmx64-first/)
- [汇编语言「Linux/x86-64」・代码解析](https://www.bxtkezhan.xyz/post/026-asmx64-explanation/)
- [汇编语言「Linux/x86-64」・调用C程序](https://www.bxtkezhan.xyz/post/049-asmx64-c/)

- [汇编语言入门教程：汇编语言程序设计指南（精讲版）](http://c.biancheng.net/asm/)
- [汇编语言入门教程简明版（阮一峰博客）](http://www.ruanyifeng.com/blog/2018/01/assembly-language-primer.html)
- [汇编语言入门教程](https://www.ruanyifeng.com/blog/2018/01/assembly-language-primer.html)
- [汇编语言](https://www.cnblogs.com/euler0525/p/16313494.html)

- [ARM汇编基础详解](https://blog.csdn.net/weixin_45309916/article/details/107837561)

- [<font color=Red>汇编语言 教程</font>](https://www.jc2182.com/assemblylanguage/assembly-language-jiaocheng.html)
- [<font color=Red>汇编语言Linux 汇编语言开发指南</font>](https://zhuanlan.zhihu.com/p/54853591)
- [<font color=Red>汇编语言基础</font>](https://github.com/fmw666/Assembly)
- [程序员需要了解的硬核知识之汇编语言(全)_程序员cxuan的个人主页-CSDN博客](https://blog.csdn.net/qq_36894974/article/details/103365258)
- [程序员需要了解的硬核知识之汇编语言(全)](https://www.cnblogs.com/cxuanBlog/p/11976084.html)

- [64位和32位在汇编语言中的区别](https://blog.csdn.net/qq_40148538/article/details/101355488)

### 汇编器（assembler）语法风格

在汇编语言中，是否使用 `%` 符号，取决于你使用的**汇编器（assembler）语法风格**。主要有两种风格：

---

#### ✅ AT\&T 语法（如 GNU `as` 使用的）

- 使用 `%` 表示寄存器
- 源操作数在前，目标操作数在后
- 常量以 `$` 开头
- 指令后缀表明操作数大小（如 `movb`, `movl`, `movq`）

**示例**：

```asm
movl %eax, %ebx    ; 把 eax 的值复制到 ebx 中
addl $1, %eax      ; eax += 1
```

---

#### ✅ Intel 语法（如 NASM、MASM 使用的）

- **不**使用 `%` 表示寄存器
- 目标操作数在前，源操作数在后（和 AT\&T 相反）
- 常量直接写，无 `$`
- 指令大小通常通过寄存器推断，或通过关键字（如 `byte`, `word`）指定

**示例**：

```asm
mov ebx, eax       ; 把 eax 的值复制到 ebx 中
add eax, 1         ; eax += 1
```

---

#### 🔁 小结对比表

| 区别项    | AT\&T 语法        | Intel 语法         |
| ------ | --------------- | ---------------- |
| 寄存器前缀  | `%eax`          | `eax`            |
| 常量前缀   | `$1`            | `1`              |
| 操作数顺序  | 源, 目标           | 目标, 源            |
| 汇编器工具  | GNU `as`, GAS   | NASM, MASM, FASM |
| 指令后缀大小 | `movl`, `movb`等 | `mov` + 指令大小     |

---

如果是在 `gcc` 编译过程中使用嵌入汇编（如 `asm` 或 `__asm__`），那通常是 **AT\&T 语法**。而 Windows 下的汇编教程，很多是基于 **Intel 语法** 的。

#### crash中一般是intel语法风格吧？

在 `crash` 工具中，**通常采用的是 Intel 风格语法的反汇编格式（即你说的“后者”）**。

---

##### ✅ 证据说明

###### 1. **寄存器没有 `%` 前缀**

```bash
crash> dis rcu_sched
0xffffffff810abcde:  mov    rax, rbx
```

- `rax`, `rbx` 没有 `%`
- `mov` 是 Intel 风格，目标在前，源在后

###### 2. **立即数不带 `$`**

```bash
mov    eax, 1
```

- 如果是 AT\&T，应写作：`movl $1, %eax`

###### 3. **反汇编顺序为目标, 源**

```bash
mov    rax, qword ptr [rbx + 8]
```

- 目标在前，源在后 ⇒ Intel 风格

---

##### ✅ 结论

> `crash` 工具内部使用 `gdb`（特别是 `gdb -x` 方式）来处理符号表和反汇编，而 `gdb` 默认输出汇编格式为 **Intel 语法**，除非特别指定（`set disassembly-flavor att` 才会切换为 AT\&T 风格）。

所以，**在 crash 中看到的汇编一般是 Intel 风格（无 `%`）**，符合你说的“后者”那种格式。

---

如你想验证当前 `gdb` 的汇编风格，可以在 `crash` 的交互中输入：

```bash
set disassembly-flavor
```

它会告诉你当前是 `intel` 还是 `att`。不过大多数 Linux 系统预设是 `intel`。

## 内联汇编

- [扩展内联汇编__asm__](https://blog.csdn.net/weixin_36209467/article/details/122113552)
- [<font color=Red>C语言内联汇编使用方法__asm__</font>](https://blog.csdn.net/qq_42931917/article/details/117779286)
- [Linux 0.11内核分析03：系统调用 中断描述符安装函数](https://blog.csdn.net/chenchengwudi/article/details/121089099#t5)
- [程序的机器级表示-汇编](https://pvcstillingradschool.github.io/miniWiki/programming/csapp/3_machine_level_programming.html)
- [main函数和启动例程 汇编程序的入口是_start，而C程序的入口是main函数。](https://www.cnblogs.com/xiaojianliu/articles/8733568.html)

## 栈帧回溯

- [栈帧详解ebp、esp](https://blog.csdn.net/qq_41658597/article/details/115603733)
- [EBP 和 ESP 详解](https://blog.csdn.net/qq_25814297/article/details/113475019)
- [关于汇编语言中PUSH和POP指令的一个小结](https://blog.csdn.net/qq_44288506/article/details/104767511)
- [2020-2021-1 20209320 《Linux内核原理与分析》第二周作业](https://www.cnblogs.com/dyyblog/p/13812922.html)
- [<font color=Red>初步分析汇编代码</font>](https://blog.csdn.net/assiduous_me/article/details/119488503)
- [一起学习64位ARM平台稳定性分析：函数调用标准](https://mp.weixin.qq.com/s/HRoddV3KuKlTl-3if5HN_w)
- [ARM栈回溯](https://mp.weixin.qq.com/s/I1I3Ee3b--hgnIoH_GCPjQ)
- [Stack Unwind 堆栈回溯](https://zhuanlan.zhihu.com/p/598210639)
- [Unwind 栈回溯详解：libunwind](https://blog.csdn.net/Rong_Toa/article/details/110846509)
- [Unwind 栈回溯详解](https://blog.csdn.net/pwl999/article/details/107569603)
- [当没有了framepointer该如何进行栈回溯？](https://zhuanlan.zhihu.com/p/665401236)
- [AARCH64平台的栈回溯](https://blog.csdn.net/lidan113lidan/article/details/121801335)
- [Arm64 栈回溯](https://mp.weixin.qq.com/s/lCu_-v0naIi6JpBqWzXcpA)

## x86

- [Intel® 64 位和 IA-32 架构开发手册说明三卷中文版](https://blog.csdn.net/weixin_43833642/article/details/108511881)

## 龙芯

- [龙芯架构参考手册](https://blog.csdn.net/weixin_43833642/article/details/108511881)

## 其它

- [编译器的技术文章](https://m.toutiao.com/article_series/7147341959684751880)
- [为什么 JVM 叫做基于栈的 RISC 虚拟机](https://mp.weixin.qq.com/s/E7XL4BVrJ99dG4RNo-olyQ)
