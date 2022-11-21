# bpftrace

## source install

1. git clone bpftrace

    ```bash
    https://github.com/iovisor/bpftrace.git
    git checkout -b v0.16.0 v0.16.0
    ```

2. apt install depends

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
    llvm-13-dev \
    llvm-13-runtime \
    libclang-13-dev \
    clang-13 \
    libpcap-dev \
    libgtest-dev \
    libgmock-dev \
    asciidoctor
    ```

3. cmake build and install

    ```bash
    cd bpftrace
    mkdir build
    cmake -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release .. # 此处关闭单元测试，是因为单元测试跑不过
    make -j8
    sudo make install
    ```

## ubuntu package install

- Should work on Ubuntu 19.04 and later.

```bash
sudo apt-get install -y bpftrace
```

## More

- [https://github.com/iovisor/bpftrace.git](https://github.com/iovisor/bpftrace.git)
- [bpftrace Install](https://github.com/iovisor/bpftrace/blob/master/INSTALL.md)
