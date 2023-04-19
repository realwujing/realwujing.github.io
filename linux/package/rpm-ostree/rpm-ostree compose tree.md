# rpm-ostree compose tree

基于Fedora-Workstation-Live-x86_64-28-1.1

rpm-ostree compose tree 用在server上。

https://pagure.io/fedora-atomic/tree/master

```bash
sudo /home/wujing/code/rpm-ostree/rpm-ostree compose tree --ex-unified-core --cachedir=cache --repo=/srv/centos-atomic/build-repo sig-atomic-buildscripts/centos-atomic-host.json
```

VS Code 下以 root 用户调试程序

CentOS下sudo免密配置

https://vault.centos.org/7.0.1406/os/x86_64/RPM-GPG-KEY-CentOS-7

```bash
cd /etc/pki/rpm-gpg
wget https://vault.centos.org/7.0.1406/os/x86_64/RPM-GPG-KEY-CentOS-7
```

"args": ["compose", "tree", "--ex-unified-core", "--cachedir=/srv/centos-atomic/cache", "--repo=/srv/centos-atomic/build-repo", "/srv/centos-atomic/sig-atomic-buildscripts/centos-atomic-host.json"],

http://yum.baseurl.org/api/yum/yum/depsolve.html

```bash
ostree_repo_prepare_transaction
```
