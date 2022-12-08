# bpftrace

## source install

git clone bpftrace:

```bash
https://github.com/iovisor/bpftrace.git
git checkout -b v0.16.0
git submodule update --init --recursive
```

## depends

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
sudo apt-get install -y \
bison \
cmake \
flex \
g++ \
git \
libelf-dev \
zlib1g-dev \
libfl-dev \
systemtap-sdt-dev \
binutils-dev \
libcereal-dev \
llvm-11-dev \
llvm-11-runtime \
libclang-11-dev \
clang-11 \
libpcap-dev \
libgtest-dev \
libgmock-dev \
asciidoctor
```

## cmake build and install

```bash
cd bpftrace
mkdir build
cd build
../build-libs.sh
cmake -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release .. # 此处关闭单元测试，是因为单元测试跑不过
make -j8
sudo make install
```

## Environment variable configuration

```bash
echo "export PATH=$PATH:/usr/local/share/bpftrace/tools" >> /etc/profile
```

add /usr/local/share/bpftrace/tools to /etc/sudoers:

```bash
sudo visudo
```

```text
Defaults        secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/share/bpftrace/tools"
```

## Get started

```bash
sudo bpftrace -l
sudo execsnoop.bt
```

## More

- [https://github.com/iovisor/bpftrace.git](https://github.com/iovisor/bpftrace.git)
- [bpftrace Install](https://github.com/iovisor/bpftrace/blob/master/INSTALL.md)
