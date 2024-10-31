---
date: 2023/04/19 22:00:35
updated: 2024/01/05 10:36:55
---

# pbuilder教程

1. 安装环境

    ```bash
    sudo apt-get install pbuilder debootstrap devscripts dh-make
    ```

2. pbuilder配置
以下路径任选其一

    ```plain
    /etc/pbuilderrc
    /root/.pbuilderrc
    ```

    将以下内容写入上述配置文件，上述路径任选其一即可，以uos v20 专业版为例：

    ```plain
    MIRRORSITE=http://pools.uniontech.com/desktop-professional
    DEBOOTSTRAPOPTS=(
        '--variant=buildd'
    )
    ```

3. 创建pbuilder基础环境

    ```bash
    sudo pbuilder create --distribution eagle --debootstrapopts --no-check-gpg
    ```

    如果报错如下：
    E: No such script: /usr/share/debootstrap/scripts/eagle
    E: debootstrap failed
    E: debootstrap.log not present
    W: Aborting with an error

    ```bash
    cd /usr/share/debootstrap/scripts/
    cp sid eagle
    ```

    需要额外添加仓库源可以`login`后添加`apt`仓库源：

    ```bash
    sudo pbuilder login --basetgz /var/cache/pbuilder/base.tgz --save-after-login
    ```

4. 下载源码

    ```bash
    git clone "http://ut004487@gerrit.uniontech.com/a/linglong"
    cd linglong
    git checkout develop/snipe
    ```

    查看changelog版本号

    ```bash
    head debian/changelog
    ```

    版本号信息如下:

    ```plain
    linglong (1.3.1-1) unstable; urgency=medium

        * fixed some bug.

        -- liujianqiang <liujianqiang@uniontech.com>  Wed, 27 Apr 2022 15:12:16 +0800

        linglong (1.3.0-1) unstable; urgency=medium

        * 1. fix link library failed when using cmake.
        * 2. fix adjust dependency checkout directory.
    ```

    根据版本号创建符合dh_make规范的`<package>-<version>`格式

    ```bash
    cd ..
    cp -r linglong linglong-1.3.1
    ```

5. dh_make生成debian模板文件

    ```bash
    cd linglong-1.3.1
    dh_make --createorig -sy
    ```

6. dkpg-source生成构建源代码包

    ```bash
    dpkg-source -b .
    ```

7. 使用pbuilder构建deb包

    ```bash
    cd ..
    sudo pbuilder --build  --logfile log.txt --basetgz /var/cache/pbuilder/base.tgz --allow-untrusted --hookdir /var/cache/pbuilder/hooks --use-network yes --aptcache "" --buildresult . --debbuildopts -sa *.dsc
    ```

## More

[Debian 软件包制作流程](https://www.debian.org/doc/manuals/maint-guide/index.zh-cn.html)
[pbuilder编译构建工具分析](https://www.cnblogs.com/zszmhd/p/3628446.html)
[debian pbuilder使用](https://www.aftermath.cn//2022/03/06/debian-pbuilder/)
