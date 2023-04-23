---
date: 2023/04/19 21:58:01
updated: 2023/04/19 21:58:01
---

# rpm-ostree安装教程

Rpm-ostree通过autogen.sh自动生成makefile文件，点击下方链接了解autogen.sh。

Linux工具之autogen.sh

autotools自动编译系列之三---autogen.sh实例

## 1. 进入rpm-ostree代码目录下

```bash
cd rpm-ostree
```

## 2. 执行脚本autogen.sh

```bash
dnf install autoconf libtool automake
./autogen.sh
```

## 3、configure: error: Package requirements (gio-unix-2.0) were not met:

```text
Package 'gio-unix-2.0', required by 'virtual:world', not found
```

https://centos.pkgs.org/8/centos-baseos-x86_64/glib2-devel-2.56.4-10.el8_4.1.x86_64.rpm.html

```bash
dnf install glib2-devel
```

## 4. Package 'json-glib-1.0', required by 'virtual:world', not found

https://centos.pkgs.org/8/centos-appstream-x86_64/json-glib-devel-1.4.4-1.el8.x86_64.rpm.html

```bash
dnf install json-glib-devel
```

## 5. Package 'ostree-1', required by 'virtual:world', not found

https://centos.pkgs.org/8/raven-extras-x86_64/ostree-libs-2021.2-1.el8.x86_64.rpm.html

```bash
wget https://pkgs.dyn.su/el8/extras/x86_64/ostree-libs-2021.2-1.el8.x86_64.rpm
dnf install ostree-libs-2021.2-1.el8.x86_64.rpm
``` 

https://centos.pkgs.org/8/raven-extras-x86_64/ostree-devel-2021.2-1.el8.x86_64.rpm.html

```bash
wget https://pkgs.dyn.su/el8/extras/x86_64/ostree-devel-2021.2-1.el8.x86_64.rpm
dnf install ostree-devel-2021.2-1.el8.x86_64.rpm
```

## 6. Package 'rpm', required by 'virtual:world', not found

https://centos.pkgs.org/8/centos-baseos-x86_64/rpm-devel-4.14.3-14.el8_4.x86_64.rpm.html

```bash
dnf install rpm-devel
```

## 7. Package 'polkit-gobject-1', required by 'virtual:world', not found

https://centos.pkgs.org/8/centos-baseos-x86_64/polkit-devel-0.115-11.el8_4.1.x86_64.rpm.html

```bash
dnf install polkit-devel
```

## 8. Package 'libarchive', required by 'virtual:world', not found

https://centos.pkgs.org/8/centos-powertools-x86_64/libarchive-devel-3.3.3-1.el8.x86_64.rpm.html

```bash
dnf --enablerepo=powertools install libarchive-devel
```

## 9. configure: error: cargo is required for --enable-rust

```bash
dnf install rust cargo
```

## 10. config.status: error: in `/root/code/rpm-ostree':

config.status: error: Something went wrong bootstrapping makefile fragments
    for automatic dependency tracking.  Try re-running configure with the
    '--disable-dependency-tracking' option to at least be able to build
    the package (albeit without support for automatic dependency tracking).

```bash
./autogen.sh --disable-dependency-tracking
```
