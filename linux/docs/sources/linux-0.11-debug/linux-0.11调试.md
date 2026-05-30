# linux-0.11调试

本文档介绍如何在debian9上调试Linux 0.11内核。

## os 环境

```bash
wget https://get.debian.org/images/archive/9.13.0/amd64/iso-dvd/debian-9.13.0-amd64-DVD-1.iso
```

```bash
uname -a

Linux debian 4.9.0-13-amd64 #1 SMP Debian 4.9.228-1 (2020-07-05) x86_64 GNU/Linux
```

```bash
root@debian:/home/wujing/code# cat /etc/os-release 
PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
NAME="Debian GNU/Linux"
VERSION_ID="9"
VERSION="9 (stretch)"
VERSION_CODENAME=stretch
ID=debian
HOME_URL="https://www.debian.org/"
SUPPORT_URL="https://www.debian.org/support"
BUG_REPORT_URL="https://bugs.debian.org/"
```

### 内核环境

将内核版本替换成linux主线v4.4.161:

```bash
git clone https://mirrors.tuna.tsinghua.edu.cn/git/linux-stable.git
git checkout -b v4.4.161 v4.4.161
apt build-dep linux
cp ~/Downloads/deb/ubuntu/16.04/linux-image-4.4.160-0404160-generic/boot/config-4.4.160-0404160-generic .config
make menuconfig
# load .config
make bindeb-pkg -j10 2> make_error.log
sudo apt install ../*.deb
```

```bash
root@debian:~# uname -a
Linux debian 4.4.161 #1 SMP Sun Jun 8 12:17:27 UTC 2025 x86_64 GNU/Linux
```

直接用4.9内核也可以，这里是为方便调试qemu-2.8.1，将内核版本替换成4.4.161。

## linux-0.11源码下载

```bash
git clone https://github.com/yuan-xy/Linux-0.11.git
```

## 安装编译依赖

```bash
sudo apt install build-essential qemu-system-x86
```

## linux-0.11编译

```bash
cd Linux-0.11
make
```

## 开始调试

在debian9图形化界面中打开终端，进入Linux-0.11目录，执行以下命令启动qemu：

```bash
make debug
```

另起一个终端（这个终端可以是ssh到debian9上的），进入Linux-0.11目录，执行以下命令启动gdb：

```bash
gdb tools/system
```

### 16 位实模式

调试引导阶段（16位实模式）。

检查当前 GDB 架构设置​:

```gdb
(gdb) show architecture
The target architecture is set automatically (currently i386)
```

- i8086 → 调试器按 16 位实模式解析
- i386 → 调试器按 32 位保护模式解析

当前gdb中架构位32位模式，但是调试引导阶段需要是16位实模式。

在gdb中输入以下命令进入16位实模式：

```gdb
(gdb) set architecture i8086 # 设置为 8086 架构，才能正确反汇编 16 位实模式代码
```

- set architecture i8086 是 GDB（GNU 调试器）中的一个命令，用于 ​​设置当前调试目标的 CPU 架构为 Intel 8086​​。这个命令在调试 ​​实模式（Real Mode）​​ 程序（如 BIOS 引导扇区、MBR 或早期操作系统内核）时特别有用。

```gdb
(gdb) target remote localhost:1234 # 连接到qemu模拟器
(gdb) break *0x7C00
(gdb) continue
```

```gdb
(gdb) info registers
eax            0xaa55   43605
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0x6f04   0x6f04 <interruptible_sleep_on+82>
ebp            0x0      0x0 <startup_32>
esi            0x0      0
edi            0x0      0
eip            0x7c00   0x7c00 <do_segment_not_present+10>
eflags         0x202    [ IF ]
cs             0x0      0
ss             0x0      0
ds             0x0      0
es             0x0      0
fs             0x0      0
gs             0x0      0
```

#### 汇编风格

```gdb
(gdb) set disassembly-flavor att # 设置反汇编风格为 AT&T
(gdb) set disassembly-flavor intel # 设置反汇编风格为 Intel
```

- set disassembly-flavor intel 和 set disassembly-flavor att 是 GDB 中的命令，用于设置反汇编输出的语法风格。Intel 和 AT&T 是两种不同的汇编语言语法风格，分别由 Intel 和 AT&T 公司定义。选择其中一种风格可以使反汇编代码更易于阅读和理解。

```gdb
(gdb) set disassembly-flavor att
(gdb) x/16i 0x7c00
=> 0x7c00 <do_segment_not_present+10>:  ljmp   $0x7c0,$0x5
   0x7c05 <do_segment_not_present+15>:  mov    $0x7c0,%ax
   0x7c08 <do_segment_not_present+18>:  mov    %ax,%ds
   0x7c0a <do_segment_not_present+20>:  mov    $0x9000,%ax
   0x7c0d <do_segment_not_present+23>:  mov    %ax,%es
   0x7c0f <do_segment_not_present+25>:  mov    $0x100,%cx
   0x7c12 <do_segment_not_present+28>:  sub    %si,%si
   0x7c14 <do_segment_not_present+30>:  sub    %di,%di
   0x7c16 <do_stack_segment>:   rep movsw %ds:(%si),%es:(%di)
   0x7c18 <do_stack_segment+2>: ljmp   $0x9000,$0x1d
   0x7c1d <do_stack_segment+7>: mov    %cs,%ax
   0x7c1f <do_stack_segment+9>: mov    %ax,%ds
   0x7c21 <do_stack_segment+11>:        mov    %ax,%es
   0x7c23 <do_stack_segment+13>:        mov    %ax,%ss
   0x7c25 <do_stack_segment+15>:        mov    $0xff00,%sp
   0x7c28 <do_stack_segment+18>:        mov    $0x0,%dx
(gdb) set disassembly-flavor intel
(gdb) x/16i 0x7c00
=> 0x7c00 <do_segment_not_present+10>:  jmp    0x7c0:0x5
   0x7c05 <do_segment_not_present+15>:  mov    ax,0x7c0
   0x7c08 <do_segment_not_present+18>:  mov    ds,ax
   0x7c0a <do_segment_not_present+20>:  mov    ax,0x9000
   0x7c0d <do_segment_not_present+23>:  mov    es,ax
   0x7c0f <do_segment_not_present+25>:  mov    cx,0x100
   0x7c12 <do_segment_not_present+28>:  sub    si,si
   0x7c14 <do_segment_not_present+30>:  sub    di,di
   0x7c16 <do_stack_segment>:   rep movs WORD PTR es:[di],WORD PTR ds:[si]
   0x7c18 <do_stack_segment+2>: jmp    0x9000:0x1d
   0x7c1d <do_stack_segment+7>: mov    ax,cs
   0x7c1f <do_stack_segment+9>: mov    ds,ax
   0x7c21 <do_stack_segment+11>:        mov    es,ax
   0x7c23 <do_stack_segment+13>:        mov    ss,ax
   0x7c25 <do_stack_segment+15>:        mov    sp,0xff00
   0x7c28 <do_stack_segment+18>:        mov    dx,0x0
```

以下是添加了详细中文注释的汇编代码，按两种语法格式（AT&T 和 Intel）分别标注关键操作的作用：

AT&T 语法（带 % 符号）：

```gdb
0x7c00:  ljmp   0x7c0,0x5       # 长跳转到 0x7C0:0x5（设置 CS:IP，进入新段）
0x7c05:  mov    $0x7c0,%ax       # AX = 0x7C0（引导扇区段地址）
0x7c08:  mov    %ax,%ds          # DS = AX（数据段指向 0x7C0 << 4 = 0x7C00）
0x7c0a:  mov    $0x9000,%ax      # AX = 0x9000（目标段地址）
0x7c0d:  mov    %ax,%es          # ES = AX（目标段指向 0x90000）
0x7c0f:  mov    $0x100,%cx       # CX = 256（复制 256 字 = 512 字节）
0x7c12:  sub    %si,%si          # SI = 0（源偏移地址）
0x7c14:  sub    %di,%di          # DI = 0（目标偏移地址）
0x7c16:  rep movsw %ds:(%si),%es:(%di) # 循环复制 DS:SI -> ES:DI（搬移到 0x90000）
0x7c18:  ljmp   0x9000,0x1d    # 跳转到 0x9000:0x1D（执行复制后的代码）
0x7c1d:  mov    %cs,%ax          # AX = CS（当前代码段 0x9000）
0x7c1f:  mov    %ax,%ds          # DS = AX（同步数据段）
0x7c21:  mov    %ax,%es          # ES = AX（同步附加段）
0x7c23:  mov    %ax,%ss          # SS = AX（栈段指向 0x90000）
0x7c25:  mov    $0xff00,%sp      # SP = 0xFF00（栈顶指针）
0x7c28:  mov    $0x0,%dx         # DX = 0（清空寄存器）
```

Intel 语法（无 % 符号）:

```gdb
0x7c00:  jmp    0x7c0:0x5         # 同上，跳转到 0x7C0:0x5
0x7c05:  mov    ax,0x7c0          # AX = 0x7C0
0x7c08:  mov    ds,ax             # DS = AX
0x7c0a:  mov    ax,0x9000         # AX = 0x9000
0x7c0d:  mov    es,ax             # ES = AX
0x7c0f:  mov    cx,0x100          # CX = 256
0x7c12:  sub    si,si             # SI = 0
0x7c14:  sub    di,di             # DI = 0
0x7c16:  rep movs WORD PTR es:[di],WORD PTR ds:[si] # 复制 DS:SI -> ES:DI
0x7c18:  jmp    0x9000:0x1d       # 跳转到 0x9000:0x1D
0x7c1d:  mov    ax,cs             # AX = CS
0x7c1f:  mov    ds,ax             # DS = AX
0x7c21:  mov    es,ax             # ES = AX
0x7c23:  mov    ss,ax             # SS = AX
0x7c25:  mov    sp,0xff00         # SP = 0xFF00
0x7c28:  mov    dx,0x0            # DX = 0
```

**关键操作说明**

1. ljmp / jmp：

   - 切换代码段（CS）和指令指针（IP），确保后续指令从新地址执行。

2. rep movsw：

   - 将 0x7C00 处的 512 字节（引导扇区）复制到 0x90000，避免后续加载 setup.s 时覆盖自身。

3. 段寄存器初始化：  

   - DS、ES、SS 均设置为 0x9000，使数据访问和栈操作指向新位置。

4. 栈指针设置：  

   - SP = 0xFF00 使得栈空间位于 0x90000 + 0xFF00 = 0x9FF00（向下增长）。

**为什么这是 bootsect.s？**

- 地址 0x7C00：BIOS 固定加载引导扇区的位置。

- 自复制行为：bootsect.s 的标准操作是复制自身到 0x90000 并跳转。

- 后续流程：跳转后通常会加载 setup.s（通过 int 0x13 磁盘中断）。

通过指令逻辑和内存地址可确认这是 bootsect.s 的代码。

```c
// vim boot/bootsect.s +50

47 #
 48         .equ ROOT_DEV, 0x301
 49         ljmp    $BOOTSEG, $_start
 50 _start:
 51         mov     $BOOTSEG, %ax   #将ds段寄存器设置为0x7C0
 52         mov     %ax, %ds
 53         mov     $INITSEG, %ax   #将es段寄存器设置为0x900
 54         mov     %ax, %es
 55         mov     $256, %cx               #设置移动计数值256字
 56         sub     %si, %si                #源地址 ds:si = 0x07C0:0x0000
 57         sub     %di, %di                #目标地址 es:si = 0x9000:0x0000
 58         rep                                     #重复执行并递减cx的值
 59         movsw                           #从内存[si]处移动cx个字到[di]处
 60         ljmp    $INITSEG, $go   #段间跳转，这里INITSEG指出跳转到的段地址，解释了cs的值为0x9000
 61 go:     mov     %cs, %ax                #将ds，es，ss都设置成移动后代码所在的段处(0x9000)
 62         mov     %ax, %ds
 63         mov     %ax, %es
 64 # put stack at 0x9ff00.
 65         mov     %ax, %ss
 66         mov     $0xFF00, %sp            # arbitrary value >>512
```

```gdb
(gdb) add-symbol-file boot/bootsect.o 0x7c00
add symbol table from file "boot/bootsect.o" at
        .text_addr = 0x7c00
(y or n) y
Reading symbols from boot/bootsect.o...(no debugging symbols found)...done.
```

```gdb
(gdb) x/16i _start
   0x7c05 <do_segment_not_present+15>:  mov    $0x7c0,%ax
   0x7c08 <do_segment_not_present+18>:  mov    %ax,%ds
   0x7c0a <do_segment_not_present+20>:  mov    $0x9000,%ax
   0x7c0d <do_segment_not_present+23>:  mov    %ax,%es
   0x7c0f <do_segment_not_present+25>:  mov    $0x100,%cx
   0x7c12 <do_segment_not_present+28>:  sub    %si,%si
   0x7c14 <do_segment_not_present+30>:  sub    %di,%di
   0x7c16 <do_stack_segment>:   rep movsw %ds:(%si),%es:(%di)
   0x7c18 <do_stack_segment+2>: ljmp   $0x9000,$0x1d
   0x7c1d <do_stack_segment+7>: mov    %cs,%ax
   0x7c1f <do_stack_segment+9>: mov    %ax,%ds
   0x7c21 <do_stack_segment+11>:        mov    %ax,%es
   0x7c23 <do_stack_segment+13>:        mov    %ax,%ss
   0x7c25 <do_stack_segment+15>:        mov    $0xff00,%sp
   0x7c28 <do_stack_segment+18>:        mov    $0x0,%dx
   0x7c2b <do_stack_segment+21>:        mov    $0x2,%cx
```

根据反汇编确定当前代码位于bootsect.s汇编代码入口_start处。

`set architecture i8086`很重要，不然在gdb中`break *0x7C00`打断点后，`info registers`、`x/16i 0x7c00`压根对不上。

在 GDB 中，你看到的 do_segment_not_present 和 do_stack_segment 等符号名称是 GDB ​​错误地将当前代码关联到了内核的错误符号表​​上。这是因为：

- ​引导扇区代码（0x7C00）本身没有符号信息​​，但 GDB 加载了内核的符号表（包含这些错误匹配）。

- GDB 尝试将机器指令反汇编并匹配到已有符号​​，导致显示无关的函数名。

#### 临时解决方案(清除错误符号关联​)

```gdb
(gdb) symbol-file # 清除所有已加载的符号
Discard symbol table from `/home/wujing/code/linux/tools/system'? (y or n) y
No symbol file now.
(gdb) set max-completions 0 # 禁用符号匹配
(gdb) x/16i 0x7c00
=> 0x7c00:      ljmp   $0x7c0,$0x5
   0x7c05:      mov    $0x7c0,%ax
   0x7c08:      mov    %ax,%ds
   0x7c0a:      mov    $0x9000,%ax
   0x7c0d:      mov    %ax,%es
   0x7c0f:      mov    $0x100,%cx
   0x7c12:      sub    %si,%si
   0x7c14:      sub    %di,%di
   0x7c16:      rep movsw %ds:(%si),%es:(%di)
   0x7c18:      ljmp   $0x9000,$0x1d
   0x7c1d:      mov    %cs,%ax
   0x7c1f:      mov    %ax,%ds
   0x7c21:      mov    %ax,%es
   0x7c23:      mov    %ax,%ss
   0x7c25:      mov    $0xff00,%sp
   0x7c28:      mov    $0x0,%dx
(gdb) x/16i _start
No symbol table is loaded.  Use the "file" command.
(gdb) add-symbol-file boot/bootsect.o 0x7c00
add symbol table from file "boot/bootsect.o" at
        .text_addr = 0x7c00
(y or n) y
Reading symbols from boot/bootsect.o...(no debugging symbols found)...done.
(gdb) x/16i _start
   0x7c05 <_start>:     mov    $0x7c0,%ax
   0x7c08 <_start+3>:   mov    %ax,%ds
   0x7c0a <_start+5>:   mov    $0x9000,%ax
   0x7c0d <_start+8>:   mov    %ax,%es
   0x7c0f <_start+10>:  mov    $0x100,%cx
   0x7c12 <_start+13>:  sub    %si,%si
   0x7c14 <_start+15>:  sub    %di,%di
   0x7c16 <_start+17>:  rep movsw %ds:(%si),%es:(%di)
   0x7c18 <_start+19>:  ljmp   $0x9000,$0x1d
   0x7c1d <go>: mov    %cs,%ax
   0x7c1f <go+2>:       mov    %ax,%ds
   0x7c21 <go+4>:       mov    %ax,%es
   0x7c23 <go+6>:       mov    %ax,%ss
   0x7c25 <go+8>:       mov    $0xff00,%sp
   0x7c28 <load_setup>: mov    $0x0,%dx
   0x7c2b <load_setup+3>:       mov    $0x2,%cx
(gdb)
```

现在很清晰的看到`*0x7c00`跟bootsect.s汇编入口_start对上了。

`(gdb) set max-completions 0 # 禁用符号匹配`后面调试32 位保护模式时可以用`(gdb) set max-completions unlimited  # 恢复默认的无限补全`恢复。

### 32 位保护模式

接下来介绍cpu切换到保护模式（32位）后的调试方法。

当CPU通过cr0寄存器切换到保护模式后：

```bash
(gdb) set architecture i386       # 切换到32位架构
(gdb) break startup_32           # 保护模式入口点
(gdb) continue
```

```gdb
(gdb) info files
Remote serial target in gdb-specific protocol:
Debugging a target over a serial line.
        While running this, GDB does not access memory from...
Local exec file:
        `/home/wujing/code/linux/tools/system', file type elf32-i386.
        Entry point: 0x0
        0x00000000 - 0x000183b4 is .text
        0x000183b4 - 0x000197c2 is .rodata
        0x000197c4 - 0x0001e5a8 is .eh_frame
        0x0001f000 - 0x00022ac1 is .data
        0x00022ae0 - 0x00026fb0 is .bss
        0x00007c00 - 0x00007e00 is .text in /home/wujing/code/linux/boot/bootsect.o
```

```gdb
(gdb) add-symbol-file tools/system 0x0  # 明确指定加载基址为 0x0

add symbol table from file "tools/system" at
        .text_addr = 0x0
(y or n) y
Reading symbols from tools/system...done.
```

```gdb
(gdb) set max-completions unlimited
```

```gdb
(gdb) b main
Breakpoint 2 at 0x6741: file init/main.c, line 107.
```

```gdb
(gdb) c
Continuing.

Breakpoint 2, main () at init/main.c:107
107     {                       /* The startup routine assumes (well, ...) this */
```

```gdb
(gdb) list
102     static long main_memory_start = 0;
103
104     struct drive_info { char dummy[32]; } drive_info;
105
106     void main(void)         /* This really IS void, no error here. */
107     {                       /* The startup routine assumes (well, ...) this */
108     /*
109      * Interrupts are still disabled. Do necessary setups, then
110      * enable them
111      */
```

```gdb
(gdb) x/16i $eip
=> 0x6741 <main>:       lea    0x24(%si),%cx
   0x6744 <main+3>:     add    $0x83,%al
   0x6746 <main+5>:     in     $0xf0,%al
   0x6748 <main+7>:     pushw  -0x4(%bx,%di)
   0x674b <main+10>:    push   %bp
   0x674c <main+11>:    mov    %sp,%bp
```

至此，linux-0.11编译调试入门基本完结。
