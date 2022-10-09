# BCC

[BPF Compiler Collection (BCC)](https://github.com/iovisor/bcc.git)

## BCC安装教程

### ubuntu18.04、ubuntu20.04、Deepin 20.6

#### 依赖安装

```bash
sudo apt-get -y install bison build-essential cmake flex git libedit-dev \
libllvm6.0 llvm-6.0-dev libclang-6.0-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils
```

#### 代码下载

1. 代码下载方式一(推荐使用)

    ```bash
    wget https://github.com/iovisor/bcc/releases/download/v0.24.0/bcc-src-with-submodule.tar.gz
    tar -zxvf bcc-src-with-submodule.tar.gz
    ```

2. 代码下载方式二

    ```bash
    git clone https://github.com/iovisor/bcc.git
    git checkout -b v0.24.0
    ```

### Deepin 20.7

#### 依赖安装

```bash
sudo apt-get -y install bison build-essential cmake flex git libedit-dev \
llvm-13-dev libclang-13-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils
```

#### 代码下载

1. 代码下载方式一(推荐使用)

    ```bash
    wget https://github.com/iovisor/bcc/releases/download/v0.6.1/bcc-src-with-submodule.tar.gz
    tar -zxvf bcc-src-with-submodule.tar.gz
    ```

2. 代码下载方式二

    ```bash
    git clone https://github.com/iovisor/bcc.git
    git checkout -b v0.6.1
    ```

### 编译安装bcc

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

### Environment variable configuration

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

### Get started

```bash
sudo execsnoop
```

## More

- [https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---source](https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---source)
- [BCC在ubuntu18.04源码安装](https://blog.csdn.net/qq_33344148/article/details/123255679)
- [解决 sudo 执行命令时找不到命令问题](https://www.cnblogs.com/lfri/p/16277069.html)
- [linux系统下将python3设置为默认的python](https://blog.51cto.com/u_15351425/3727453)
