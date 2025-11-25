---
title: '基于debian9编译调试qemu'
date: '2025/11/25 17:15:02'
updated: '2025/11/25 17:15:02'
---

# 基于debian9编译调试qemu

## os环境

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
for r in $(git remote); do
    matches=$(git ls-remote --tags "$r" 2>/dev/null | grep v4.4.161)
    if [ -n "$matches" ]; then
        echo "Remote: $r"
        echo "$matches"
        echo
    fi
done
```

## qemu源码下载

```bash
apt source qemu
```

可以看到当前目录多了三个文件：
```bash
qemu_2.8+dfsg-6+deb9u17.debian.tar.xz
qemu_2.8+dfsg-6+deb9u17.dsc
qemu_2.8+dfsg.orig.tar.xz
```

进入qemu-2.8+dfsg目录：
```bash
cd qemu-2.8+dfsg
```

### 编译调试

安装编译依赖：
```bash
apt build-dep qemu
```

开启32线程并行编译，保留调试符号，避免剥离:
```bash
time DEB_BUILD_OPTIONS="parallel=32 nostrip" debian/rules binary-arch 2> make_error.log
```

#### bugfix

```bash
root@ffef21905078:~/code/qemu-2.8+dfsg# git log --oneline
e709e55 include/qemu/bitops.h: fix the preprocessor not recognizing the `sizeof` operator
46d8e51 first commit
```

```bash
git show e709e55

commit e709e5589dcef5a731abcb232b7f27388bf9ded2
Author: wujing <realwujing@qq.com>
Date:   Sun Jun 8 19:43:06 2025 +0800

    include/qemu/bitops.h: fix the preprocessor not recognizing the `sizeof` operator
    
    cat make_error.log
    In file included from /home/wujing/code/qemu-2.8+dfsg/include/qemu/hbitmap.h:15:0,
                     from /home/wujing/code/qemu-2.8+dfsg/include/block/dirty-bitmap.h:5,
                     from /home/wujing/code/qemu-2.8+dfsg/include/block/block.h:9,
                     from /home/wujing/code/qemu-2.8+dfsg/include/block/block_int.h:28,
                     from /home/wujing/code/qemu-2.8+dfsg/block/raw-posix.c:30:
    /usr/include/linux/swab.h: In function ‘__swab’:
    /home/wujing/code/qemu-2.8+dfsg/include/qemu/bitops.h:20:34: warning: "sizeof" is not defined [-Wundef]
     #define BITS_PER_LONG           (sizeof (unsigned long) * BITS_PER_BYTE)
                                      ^
    /home/wujing/code/qemu-2.8+dfsg/include/qemu/bitops.h:20:41: error: missing binary operator before token "("
     #define BITS_PER_LONG           (sizeof (unsigned long) * BITS_PER_BYTE)
                                             ^
    make: *** [block/raw-posix.o] Error 1
    make: *** 正在等待未完成的任务....
    
    apt policy gcc
    gcc:
      已安装：4:6.3.0-4
      候选： 4:6.3.0-4
      版本列表：
     *** 4:6.3.0-4 500
            500 http://archive.debian.org/debian stretch/main amd64 Packages
            100 /var/lib/dpkg/status
    
    - [qemu 安装 recipe for target 'block/file-posix.o' failed error:
    missing binary operator before token
    "(](https://blog.csdn.net/FJDJFKDJFKDJFKD/article/details/105982709)
    
    Signed-off-by: wujing <realwujing@qq.com>
    Signed-off-by: QiLiang Yuan <yuanql9@chinatelecom.cn>

diff --git a/include/qemu/bitops.h b/include/qemu/bitops.h
index 1881284..cb49cbf 100644
--- a/include/qemu/bitops.h
+++ b/include/qemu/bitops.h
@@ -17,7 +17,7 @@
 #include "atomic.h"
 
 #define BITS_PER_BYTE           CHAR_BIT
-#define BITS_PER_LONG           (sizeof (unsigned long) * BITS_PER_BYTE)
+#define BITS_PER_LONG           (__SIZEOF_LONG__ * BITS_PER_BYTE)
 
 #define BIT(nr)                 (1UL << (nr))
 #define BIT_MASK(nr)            (1UL << ((nr) % BITS_PER_LONG))
```

合入此补丁后，重新编译qemu即可编译通过。

