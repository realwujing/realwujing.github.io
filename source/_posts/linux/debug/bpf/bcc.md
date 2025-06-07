---
date: 2024/07/23 10:29:55
updated: 2024/07/23 10:29:55
---

# bcc

[BPF Compiler Collection (BCC)](https://github.com/iovisor/bcc.git)

本教程适用于Deepin 20.6、Deepin 20.7。

## 安装依赖

查看宿主机已安装llvm版本：

```bash
dpkg -l | grep libllvm
```

结果输出如下：

```text
ii  libllvm11:amd64                               1:11.0.1-2                                 amd64        Modular compiler and toolchain technologies, runtime library
```

下方安装llvm、clang时版本需要选定为11。

```bash
sudo apt -y install bison \
build-essential \
cmake \
flex \
git \
libedit-dev \
llvm-11-dev \
libclang-11-dev \
python \
zlib1g-dev \
libelf-dev \
libfl-dev \
python3-distutils
```

## 源码下载

1. 代码下载方式一(推荐使用)

    ```bash
    wget https://github.com/iovisor/bcc/releases/download/v0.25.0/bcc-src-with-submodule.tar.gz
    tar -zxvf bcc-src-with-submodule.tar.gz
    ```

2. 代码下载方式二

    ```bash
    git clone https://github.com/iovisor/bcc.git
    git checkout -b v0.25.0
    git submodule update --init --recursive
    ```

## 编译安装bcc

```bash
set -ex
mkdir bcc/build
cd bcc/build
cmake ..
make
sudo make install
cmake -DPYTHON_CMD=python3 .. # build python3 binding
pushd src/python/
make
sudo make install
popd
```

## Environment variable configuration

```bash
echo "export PATH=$PATH:/usr/share/bcc/tools" >> /etc/profile
```

```text
sudo visudo后追加/usr/share/bcc/tools到secure_path后面
Defaults        secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/usr/share/bcc/tools"
```

```bash
source /etc/profile
```

## 将python3设置为默认的python

直接执行这两个命令即可：

```bash
sudo update-alternatives --install /usr/bin/python python /usr/bin/python2 100
sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 150
```

上述命令执行后，python3已被设置为默认的python。

切换到Python2，执行：

```bash
sudo update-alternatives --config python
```

## Get started

```bash
sudo execsnoop
```

```bash
trace-bpfcc -tKU -I 'sound/pci/hda/hda_codec.h' 'r::snd_hda_codec_configure(struct hda_codec *codec) "ret:%d", retval'
```

```bash
trace-bpfcc 'r::parse_user_pin_configs "ret:%d", retval'
```

## More

- [https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---source](https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---source)
- [BCC在ubuntu18.04源码安装](https://blog.csdn.net/qq_33344148/article/details/123255679)
- [解决 sudo 执行命令时找不到命令问题](https://www.cnblogs.com/lfri/p/16277069.html)
- [linux系统下将python3设置为默认的python](https://blog.51cto.com/u_15351425/3727453)
