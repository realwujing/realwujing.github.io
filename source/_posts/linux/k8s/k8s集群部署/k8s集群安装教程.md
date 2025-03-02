---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

# k8s集群安装教程

## master、node节点安装

1. 切换到root用户

    ```bash
    sudo su -
    ```

2. 配置host

    ```plain
    cat << EOF >> /etc/hosts
    192.169.1.210 k8s-master
    192.169.1.211 k8s-node1
    192.169.1.212 k8s-node2
    EOF
    ```

3. 修改主机名
每台主机都要修改

    ```bash
    hostnamectl set-hostname k8s-master
    hostnamectl set-hostname k8s-node1
    hostnamectl set-hostname k8s-node2
    ```

4. 关闭swap分区
每台主机都要关闭

    ```bash
    # 临时关闭
    swapoff -a
    # 永久关闭(老版本)
    sed -i '/swap/s/^\(.*\)$/#\1/g' /etc/fstab
    # 永久关闭(新版本)
    只需要编辑 /etc/fstab 文件，不要注释掉 swap 那一行
    而是在其后面默认的挂载选项 defaults 后面加上 noauto 变成 defaults,noauto。
    
    cat /etc/fstab
    # 查看是否注释
    ```

    * [https://www.freedesktop.org/software/systemd/man/systemd.swap.html](https://www.freedesktop.org/software/systemd/man/systemd.swap.html)
    * [openSUSE Tumbleweed 中禁用 SWAP](https://cnzhx.net/blog/disable-swap-in-opensuse-tumbleweed/)

5. 安装docker

    * [参考清华Docker Community Edition 镜像使用帮助安装docker](https://mirrors.tuna.tsinghua.edu.cn/help/docker-ce/)

6. 修改docker Cgroup驱动

    ```bash
    cat <<EOF> /etc/docker/daemon.json
    {
    "exec-opts":["native.cgroupdriver=systemd"]
    }
    EOF
    ```

7. 安装k8s

    * [添加阿里云 Kubernetes 镜像仓库](https://developer.aliyun.com/mirror/kubernetes?spm=a2c6h.13651102.0.0.73bf1b11EI1d2X)

    * [【Kubernetes系列】K8s由1.24.1降级为1.23.8
](https://blog.csdn.net/u012069313/article/details/125561711)

    * [Kubernetes 1.24 将结束对dockershim 的支持
](https://www.51cto.com/article/707507.html)

    * [Kubernetes 1.24 的删除和弃用](https://kubernetes.io/zh-cn/blog/2022/04/07/upcoming-changes-in-kubernetes-1-24/)

    ```bash
    apt-get install -y kubelet=1.23.8-00 kubeadm=1.23.8-00 kubectl=1.23.8-00
    ```

8. 设置kubectl命令补全

    ```bash
    apt-get install bash-completion
    ```

    ```bash
    echo 'source /usr/share/bash-completion/bash_completion' >>~/.bashrc
    ```

    ```bash
    echo 'source <(kubectl completion bash)' >>~/.bashrc
    ```

9. 初始化k8s-master节点

    ```text
    –apiserver-advertise-address 192.168.2.248 填写你自己k8s-master的IP地址
    ```

    ```bash
    kubeadm init \
    --apiserver-advertise-address 192.168.2.248 \
    --image-repository registry.aliyuncs.com/google_containers \
    --pod-network-cidr=10.244.0.0/16 \
    --service-cidr=10.1.0.0/16
    ```

10. 拷贝 kubectl 使用的连接 k8s 认证文件到默认路径

    ```bash
    exit
    ```

    ```bash
    mkdir -p $HOME/.kube
    ```

    ```bash
    sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
    ```

    ```bash
    sudo chown $(id -u):$(id -g) $HOME/.kube/config
    ```

    ``` bash
    echo 'export KUBECONFIG=$HOME/.kube/config' >>~/.bashrc
    ```

    * 初始化成功之后记录以下信息，添加node节点时使用。

    ```bash
    kubeadm join 192.169.1.210:6443 --token s2zrot.578bgfdqcr44aivw \
    --discovery-token-ca-cert-hash sha256:a4b764099ac50a152d8d9a7640c16380297bae8c7ffafd6e3ca76144bfde9f6c
    ```

11. 安装Pod网络插件 calico

    ```bash
    kubectl apply -f https://docs.projectcalico.org/v3.18/manifests/calico.yaml
    ```

    * [k8s master节点状态为 NotReady问题解决
](https://blog.csdn.net/w849593893/article/details/119883531)

12. 添加node节点
    * 以下操作在Node节点上执行

    ```bash
    sudo su -
    ```

    ```bash
    kubeadm join 192.169.1.210:6443 --token s2zrot.578bgfdqcr44aivw \
    --discovery-token-ca-cert-hash sha256:a4b764099ac50a152d8d9a7640c16380297bae8c7ffafd6e3ca76144bfde9f6c
    ```

    * [kubernetes忘记token或者token过期怎么加入k8s集群](https://www.cnblogs.com/linyouyi/p/10850904.html)

## k8s dashboard

* [部署和访问 Kubernetes 仪表板（Dashboard）](https://kubernetes.io/zh-cn/docs/tasks/access-application-cluster/web-ui-dashboard/)

1. 部署 Dashboard UI

    ```bash
    kubectl apply -f https://raw.githubusercontent.com/kubernetes/dashboard/v2.5.0/aio/deploy/recommended.yaml
    ```

    * [https://github.com/kubernetes/dashboard/blob/master/docs/user/accessing-dashboard/README.md](https://github.com/kubernetes/dashboard/blob/master/docs/user/accessing-dashboard/README.md)

    * [https://github.com/kubernetes/dashboard/blob/master/docs/user/access-control/creating-sample-user.md](https://github.com/kubernetes/dashboard/blob/master/docs/user/access-control/creating-sample-user.md)

2. 更改ClusterIP为NodePort

    ```bash
    kubectl -n kubernetes-dashboard edit service kubernetes-dashboard
    ```

3. 创建配置文件 dashboard-adminuser.yaml

    ```bash
    wget https://github.com/realwujing/k8s-learning/blob/dff12f0c9f2cf74ee13e0aa44babbd2786279bf2/k8s%E9%9B%86%E7%BE%A4%E9%83%A8%E7%BD%B2/dashboard-adminuser.yaml
    ```

    具体参考：

    * [k8s入门：kubernetes-dashboard 安装](https://blog.csdn.net/qq_41538097/article/details/125561769)

4. 执行

    ```bash
    kubectl apply -f dashboard-adminuser.yaml
    ```

5. k8s忘记dashboard密码

    ```bash
    kubectl -n kubernetes-dashboard get secret
    ```

    ```bash
    kubectl describe secret admin-user-token-76rq7 -n kubernetes-dashboard
    ```

    也可通过下方一行命令获取token：

    ```bash
    kubectl -n kubernetes-dashboard describe secret $(kubectl -n kubernetes-dashboard get secret | grep admin-user | awk '{print $1}')
    ```

6. 获取 kubernetes-dashboard 对外暴露端口

    ```bash
    kubectl get svc --all-namespaces
    ```

7. 访问k8s dashboard

    ```bash
    https://192.169.1.210:32188
    ```

8. [Chrome您的连接不是私密连接解决办法](https://www.jianshu.com/p/1719a27137e3)

    ```text
    thisisunsafe
    ```

## kubeadm reset

* [Reset Kubernetes Cluster using kubeadm](https://sandeepnkulkarni.wordpress.com/2020/07/11/reset-kubernetes-cluster-using-kubeadm/)

## master节点

* [k8s允许master节点参与调度的设置方法](https://www.cnblogs.com/panw/p/16643652.html)
* [k8s节点调度](https://blog.csdn.net/omaidb/article/details/121930341)
