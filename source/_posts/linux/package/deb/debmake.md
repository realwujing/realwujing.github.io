---
date: 2023/04/19 22:00:35
updated: 2023/09/21 10:07:10
---

# 宿主机上打包deb教程

1. 安装环境

    ```bash
    sudo apt install dh-make
    ```

    根据debbian/control安装依赖：

    ```bash
    cd linglong-repo
    sudo apt build-dep .    # 使用.报错时，可以将.替换成debian/control中的Source字段
    cd ..
    ```

2. 根据版本号创建符合dh_make规范的<package>-<version>格式

    ```bash
    cp linglong-repo linglong-repo-0.0.1
    ```

3. dh_make生成debian模板文件

    ```bash
    cd linglong-repo-0.0.1
    dh_make --createorig -sy
    ```

4. dkpg-source生成构建源代码包

    ```bash
    dpkg-source -b .
    ```

5. crotrol文件

    ```bash
    Source: linglong-repo   # 源码名
    Section: unknown
    Priority: optional
    Maintainer: yuanqiliang <yuanqiliang@uniontech.com>    # 维护者
    Build-Depends: debhelper (>= 11), ostree, libostree-dev    # 编译依赖
    Standards-Version: 4.1.3
    Homepage: https://linglong.dev
    #Vcs-Browser: https://salsa.debian.org/debian/linglong-repo
    #Vcs-Git: https://salsa.debian.org/debian/linglong-repo.git

    Package: ll-repo-cli    # deb包名，多个的话拆分，见下方ll-repo-server
    Architecture: any
    Depends: ${shlibs:Depends}, ${misc:Depends}
    Description: ll-repo-cli    # 简介
     ll-repo-cli    # 稍微详细一点的简介

    Package: ll-repo-server
    Architecture: any
    Depends: ${shlibs:Depends}, ${misc:Depends}
    Description: ll-repo-server
     ll-repo-server
    ```

6. rules文件

    ```bash
    #!/usr/bin/make -f

    %:
        dh $@

    override_dh_shlibdeps:
        dh_shlibdeps --dpkg-shlibdeps-params=--ignore-missing-info

    override_dh_auto_test:
    ```

7. makefile

    ```bash
    SHELL=/bin/bash

    PREFIX = /usr   # 模拟安装此包到debian/tmp目录下的相对路径前缀

    all: build    # all要放前面

    install-swag:
        go install github.com/swaggo/swag/cmd/swag@v1.8.4

    swag-doc: install-swag
        swag init --parseDependency --parseInternal -d cmd/ll-repo-server

    build-vendor:
        go mod tidy
        go mod vendor

    build: build-vendor
        CGO_ENABLED=1 GOOS=linux GOARCH=amd64 go build -o ./bin/ll-repo-server -v ./cmd/ll-repo-server
        CGO_ENABLED=1 GOOS=linux GOARCH=amd64 go build -o ./bin/ll-repo-cli    -v ./cmd/ll-repo-cli

    run: build
        ./ll-repo-server

    cli-run: build
        ./ll-repo-cli

    docker:
        docker build \
            -t linglong-server:latest \
            -f Dockerfile .
            
    install:    # 安装命令
        install -Dm0755 bin/ll-repo-cli ${DESTDIR}/${PREFIX}/bin/ll-repo-cli
        install -Dm0755 bin/ll-repo-server ${DESTDIR}/${PREFIX}/bin/ll-repo-server

    clean:
        rm -rf bin vendor

    .PHONY: docker install help swag-doc
    ```

8. ll-repo-cli.install文件

    ```bash
    usr/bin/ll-repo-cli    # ll-repo-cli 安装到系统/usr/bin/ll-repo-cli
    ```

    拆分步骤7中的install安装命令，将ll-repo-cli安装需要的内容剥离。

9. ll-repo-server.install文件

    ```bash
    usr/bin/ll-repo-server    # ll-repo-server 安装到系统/usr/bin/ll-repo-server
    ```

    拆分步骤7中的install安装命令，将ll-repo-server安装需要的内容剥离。

    制作单个deb包无需步骤8、9。

10. changelog文件

    ```text
    linglong-repo (0.0.1-1) unstable; urgency=medium

    * Initial release (Closes: #nnnn)  <nnnn is the bug number of your ITP>

    -- unknown <wujing@wujing-PC>  Fri, 14 Oct 2022 17:36:33 +0800
    ```

11. 编译制作deb包

    ```bash
    #!/usr/bin/bash

    set -x

    dh_clean    # 调用makefile中的clean命令
    rm ../linglong-repo_0.0.1*.dsc ../linglong-repo_0.0.1*.xz -rf    # 删除 dpkg-source -b . dh_make --createorig -sy 命令生成的源码压缩包
    dh_make --createorig -sy    # 生成debian目录
    dpkg-source -b .    # 生成构建源代码包
    dpkg-buildpackage -uc -us    # 编译制作deb包
    ```

命令与上方有所重复，写成shell脚本，只是为了提高效率。

- [https://www.debian.org/doc/manuals/debmake-doc/ch04.zh-cn.html#packaging-tarball](https://www.debian.org/doc/manuals/debmake-doc/ch04.zh-cn.html#packaging-tarball)
