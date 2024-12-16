---
date: 2023/04/19 16:32:36
updated: 2024/10/12 09:20:08
---

# systemd

- [<font color=Red>Systemd 入门教程：命令篇</font>](https://www.ruanyifeng.com/blog/2016/03/systemd-tutorial-commands.html)
- [<font color=Red>Systemd 入门教程：实战篇</font>](https://www.ruanyifeng.com/blog/2016/03/systemd-tutorial-part-two.html)

- [＜systemd问题定位手段＞](https://blog.csdn.net/wentian901218/article/details/117811137)

    systemd日志输出到串口控制台：

    ```bash
    systemd.log_level=debug systemd.log_target=console console=ttyS0,115200
    ```

## 配置文件编写

- [<font color=Red>systemd服务配置文件编写(1)</font>](https://www.junmajinlong.com/linux/systemd/service_1/)
- [<font color=Red>systemd service之：服务配置文件编写(2)-使用target组合多个服务</font>](https://www.junmajinlong.com/linux/systemd/service_2/)
- [Linux 使用 Systemd 管理进程服务，划重点~](https://mp.weixin.qq.com/s/1aYloTgM5c4riS0KVWKNdA)

- [systemctl管理的active(exited)状态说明](https://www.cnblogs.com/cxyc005/p/13323116.html)

- [<font color=Red>systemd 用户级别</font>](https://wiki.archlinux.org/title/Systemd_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)/User_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87))

## 日志

- [<font color=Red>表 3.5. 典型的 journalctl 命令片段列表</font>](https://www.debian.org/doc/manuals/debian-reference/ch03.zh-cn.html#_the_system_message)
- [<font color=Red>Linux系统查看日志命令journalctl的一些用法</font>](https://zhuanlan.zhihu.com/p/410995772)
- [如何使用 journalctl 查看和分析 systemd 日志（附实例）](https://www.toutiao.com/article/7200566974173151802)

- [rsyslog服务及Linux系统日志简介](https://www.jianshu.com/p/3b11a2b7c746)
- [Enable Rsyslog Logging on Debian 12](https://kifarunix.com/enable-rsyslog-logging-on-debian-12/)
- [Linux系统中的日志管理——journal、rsyslog、timedatectl、时间同步](https://blog.csdn.net/qq_45225437/article/details/104294044)

## swap

- [永久关闭linux swap](https://developer.aliyun.com/article/597885)
- [关闭SWAP分区](https://blog.51cto.com/6923450605400/735323)
- [systemd.swap — Swap unit configuration](https://www.freedesktop.org/software/systemd/man/systemd.swap.html)
- [openSUSE Tumbleweed 中禁用 SWAP](https://cnzhx.net/blog/disable-swap-in-opensuse-tumbleweed/)
- [openSUSE Tumbleweed 中禁用 SWAP](https://www.freedesktop.org/software/systemd/man/systemd.swap.html)

## reload

- [Linux Systemd 详细介绍: Unit、Unit File、Systemctl、Target](https://www.cnblogs.com/usmile/p/13065594.html)
- [linux systemctl 命令](https://www.cnblogs.com/sparkdev/p/8472711.html)

## 启动顺序

- [<font color=Red>systemd服务启动顺序分析工具</font>](https://blog.csdn.net/qq_31442743/article/details/118571723)
- [systemd启动流程分析](https://blog.csdn.net/rikeyone/article/details/108097837)

- [<font color=Red>输出单元间的依赖关系图</font>](https://www.jinbuguo.com/systemd/systemd-analyze.html)

    ```bash
    sudo apt install graphviz
    systemd-analyze dot "lightdm.service" | dot -Tsvg > systemd.svg
    ```

## 非图形界面

- [ubuntu不启动图形界面](https://zhuanlan.zhihu.com/p/344347732)

## 其他

- [XDG Autostart](https://wiki.archlinux.org/title/XDG_Autostart)

### chkconfig

- [ubuntu-18.04设置开机启动脚本](https://www.cnblogs.com/airdot/p/9688530.html)
- [Linux Ubuntu 20.04 —添加开机启动(服务/脚本)](https://www.cnblogs.com/Areas/p/13439000.html)
- [Ubuntu安装sysv-rc-conf配置开机启动服务](https://www.cnblogs.com/dongruiha/p/9941667.html)
- [centos开机启动项设置命令：chkconfig](https://www.cnblogs.com/zfying/archive/2013/03/12/2955710.html)

#### service

- [Linux中的systemctl和service](https://www.jianshu.com/p/ffe6990570d9)

- [Linux systemd资源控制初探](https://www.cnblogs.com/jimbo17/p/9107052.html)
- [在新 Linux 发行版上切换 cgroups 版本](https://www.vvave.net/archives/introduction-to-linux-kernel-control-groups-v2.html)