---
date: 2023/04/21 15:49:27
updated: 2023/04/21 15:49:27
---

# git-learning

## git全局设置用户名跟邮箱

git全局设置用户名跟邮箱，基本操作一次以后很少再变化。

设置全局用户名，xxx替换为用户名

```bash
git config --global user.name "xxx"
```

设置全局用户邮箱，xxx替换为用户邮箱

```bash
git config --global user.email "xxx"
```

- [git全局设置用户名跟邮箱相关命令](https://segmentfault.com/a/1190000038802019)

## 修改git 默认编辑器为vim

git默认编辑器为nano，不太常用。

```bash
git config --global core.editor vim
```

- [修改Git默认编辑器](https://www.jianshu.com/p/86a7640705cd)

## 彻底替换https为ssh

设置git url https请求替换为ssh方式

```bash
git config --global url."git@github.com:".insteadOf https://github.com/
```

- [github项目如何快速git clone https](https://www.jianshu.com/p/affe1af6781c)

## git设置ssh代理

类 UNIX 系统配置更改起来比较简单. 编辑 ~/.ssh/config 加入如下内容:

```bash
Host github.com *.github.com
  User git
  # SOCKS代理
  ProxyCommand nc -v -x 127.0.0.1:7890 %h %p
  # HTTPS代理
  ProxyCommand socat - PROXY:127.0.0.1:%h:%p,proxyport=7890
```

- [为 git 设置代理解决远程仓库无法连接问题](https://www.donnadie.top/set-git-proxy)

## 通过HTTPS 443端口建立SSH连接

```bash
ssh -T -p 443 git@ssh.github.com
```

正常输出如下：

```text
Hi realwujing! You've successfully authenticated, but GitHub does not provide shell access.
```

说明可以通过HTTPS 443端口建立SSH连接。

编辑 ~/.ssh/config 加入如下内容:

```bash
Host github.com *.github.com
  HostName ssh.github.com 
  User git
  Port 443
  # IdentityFile "~\.ssh\id_rsa"
  # SOCKS代理
  # ProxyCommand nc -v -x 127.0.0.1:7890 %h %p
  # HTTPS代理
  # ProxyCommand socat - PROXY:127.0.0.1:%h:%p,proxyport=7890
```

正常输出如下：

```bash
Hi realwujing! You've successfully authenticated, but GitHub does not provide shell access.
```

如果输出异常，建议将上述`ProxyCommand`开头的内容取消注释，即使用代理且采用443端口，`github pull push`功能肯定能用。
