---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

# kubernetes

- [<font color=Red>学习 Kubernetes 基础知识</font>](https://kubernetes.io/zh-cn/docs/tutorials/kubernetes-basics/)
- [Kubernetes中文社区|中文文档](http://docs.kubernetes.org.cn/)
- <https://jimmysong.io/kubernetes-handbook/>

- [名词解释 Pods 在Kubernetes中，最小的管理元素不是一个个独立的容器，而是Pod](https://www.kubernetes.org.cn/kubernetes-pod)
- [k8s集群核心概念pod](https://blog.csdn.net/weixin_43296313/article/details/121334485)

## 教程

- [<font color=Red>k8s系列教程</font>](https://www.cnblogs.com/caodan01/category/2009111.html)
- [1kubernetes简介及架构图](https://www.cnblogs.com/caodan01/p/15102328.html)
- [2kubeadm安装k8s](https://www.cnblogs.com/caodan01/p/15102386.html)
- [kubernetes之安装集群图形化界面Dadhboard](https://www.cnblogs.com/caodan01/p/15102902.html)
- [3二进制安装k8s](https://www.cnblogs.com/caodan01/p/15104491.html)
- [4k8s之资源管理 ； yaml语言](https://www.cnblogs.com/caodan01/p/15107971.html)
- [5kubernetes实战初入门](https://www.cnblogs.com/caodan01/p/15120056.html)
- [6k8s之Pod结构、配置、生命周期、调度](https://www.cnblogs.com/caodan01/p/15123078.html)
- [7k8s之Pod控制器详解](https://www.cnblogs.com/caodan01/p/15133014.html)
- [8k8s之service和ingress详解](https://www.cnblogs.com/caodan01/p/15133112.html)
- [9k8s之Endpoints、健康服务检查、高可用、搭建wordpress](https://www.cnblogs.com/caodan01/p/15136177.html)
- [10k8s之数据持久化](https://www.cnblogs.com/caodan01/p/15136217.html)
- [13基本存储、高级存储、存储配置](https://www.cnblogs.com/caodan01/p/15137507.html)
- [14k8s之StorageClass，ConfigMap，Secret](https://www.cnblogs.com/caodan01/p/15137960.html)

## K8s集群安装教程

- [<font color=Red>k8s集群安装教程</font>](https://github.com/realwujing/linux-learning/blob/master/k8s/k8s%E9%9B%86%E7%BE%A4%E5%AE%89%E8%A3%85%E6%95%99%E7%A8%8B.md)
- [使用kubeadm初始化k8s集群的一些问题解决办法](https://blog.csdn.net/sinat_32582203/article/details/119795858)
- [【问题解决】[kubelet-check] The HTTP call equal to ‘curl -sSL http://localhost:10248/healthz‘ failed wite](https://blog.csdn.net/qq_43762191/article/details/125567365)
- [kubeadm初始化异常](https://blog.51cto.com/u_15502785/5167925)
- [Kubernetes(二)——安装部署集群](https://blog.csdn.net/weixin_46415378/article/details/124435362?spm=1001.2014.3001.5502)
- [2.7 安装网络插件 calico](https://huangzhongde.cn/istio/Chapter2/Chapter2-7.html)
- [16 张图硬核讲解 Kubernetes 网络](https://mp.weixin.qq.com/s/lywYEHS_4Egv-Yp7NjBUlA)
- [搭建k8s集群并安装kubeSphere](https://blog.csdn.net/qq_44306975/article/details/121603411)

- [kubernetes忘记token或者token过期怎么加入k8s集群](https://www.cnblogs.com/linyouyi/p/10850904.html)

## dashboard

- [K8S 安装 Dashboard](https://blog.csdn.net/mshxuyi/article/details/108425487)
- [部署和访问 Kubernetes 仪表板（Dashboard）](https://kubernetes.io/zh-cn/docs/tasks/access-application-cluster/web-ui-dashboard/)
- [<font color=Red>Accessing Dashboard</font>](https://github.com/kubernetes/dashboard/blob/master/docs/user/accessing-dashboard/README.md)
- <https://github.com/kubernetes/dashboard/blob/master/docs/user/access-control/creating-sample-user.md>

- [<font color=Red>k8s在Ubuntu上安装dashboard</font>](https://blog.csdn.net/wangkaizheng123/article/details/107492833)
- [<font color=Red>k8s忘记dashboard密码</font>](https://blog.csdn.net/qq_34386723/article/details/106290681)
- [<font color=Red>Kubernetes Dashboard 生成token</font>](https://blog.csdn.net/zhangkaiadl/article/details/122125364)

    ```bash
    - kubectl-nkubernetes-dashboard get secret $(kubectl-nkubernetes-dashboard getsa/admin-user -ojsonpath="{.secrets[0].name}") -o go-template="{{.data.token| base64decode}}"
    ```

## NotReady

- [k8s master节点状态为 NotReady问题解决](https://blog.csdn.net/w849593893/article/details/119883531)

    ```bash
    kubectlget secret -nkube-system |grepadmin|awk'{print $1}'

    kubectldescribe secret dashboard-admin-token-mqspz-nkube-system|grep'^token'|awk'{print $2}'

    kubeadmjoin 10.20.52.86:6443 token pxdv83.8358nle1q7qwhhvp
    ```


- [K8s 选 cgroupfs 还是 systemd？](https://www.toutiao.com/article/7120818388343079428)
- [Kubernetes开启Swap支持](https://huangzhongde.cn/post/Kubernetes/Kubernetes_enable_Swap_support/)

## Debian 10

- [Debian10下的k8s快速部署（基于kubeadm）](https://copyfuture.com/blogs-details/20210325174246270m)

## kubectl

- [在 macOS 系统上安装和设置 kubectl](https://kubernetes.io/zh-cn/docs/tasks/tools/install-kubectl-macos/)
- [<font color=Red>本页列举了常用的 “kubectl” 命令和标志 </font>](https://kubernetes.io/zh-cn/docs/reference/kubectl/cheatsheet/)
- [<font color=Red>Kubectl 常用命令分类记录</font>](http://events.jianshu.io/p/e5bf55d2a695)
- [kubectl describe命令详解](https://www.cnblogs.com/wengzhijie/p/11412523.html)
- [<font color=Red>K8S中 yaml 文件详解（pod、deployment、service）</font>](https://blog.csdn.net/duanbaoke/article/details/119813814)
- [为Docker Desktop for Mac/Windows开启Kubernetes和Istio](https://github.com/AliyunContainerService/k8s-for-docker-desktop)

## master节点

- [k8s允许master节点参与调度的设置方法](https://www.cnblogs.com/panw/p/16643652.html)
- [k8s节点调度](https://blog.csdn.net/omaidb/article/details/121930341)

## 更新镜像

- [kubectl 更新容器镜像](https://www.likecs.com/show-305889366.html)
- [Kubernetes之kubectl常用命令使用指南:1:创建和删除](https://blog.csdn.net/liumiaocn/article/details/73913597)
- [K8s系列之：kubectl子命令详解set image](https://blog.csdn.net/zhengzaifeidelushang/article/details/122545990)

- [kubernetes使用用户名和密码拉取docker镜像](https://blog.csdn.net/fuck487/article/details/102721725)
- [如何在 K8S 集群范围使用 imagePullSecret？](https://www.modb.pro/db/383273)

## minikube

- [macOS安装minikube](https://www.cnblogs.com/qa-freeroad/p/14182522.html)
- [minikube 单机多节点](https://blog.csdn.net/qq_34146694/article/details/110955691)
- [解决 Mac 安装最新版 minikube 出现的问题](https://xkcoding.com/2019/01/14/solve-mac-install-minikube-problem.html)
- <https://minikube.sigs.k8s.io/docs/start/>

- [Ubuntu 18.04/20.04 部署minikube](https://blog.csdn.net/u010953609/article/details/121489147)
- [Mac上k8s安装之minikube 安装与使用](https://blog.csdn.net/fish_study_csdn/article/details/120707928)
- [Minikube体验](https://www.cnblogs.com/cocowool/p/minikube_setup_and_first_sample.html)
- [Minikube 入门介绍](https://blog.csdn.net/cheng_fu/article/details/109507796)
- <https://www.linuxtechi.com/install-minikube-on-debian-10/>

- [How to Run Locally Built Docker Images in Kubernetes](https://medium.com/swlh/how-to-run-locally-built-docker-images-in-kubernetes-b28fbc32cc1d)
- [【最全国内安装教程】通过minikube运行单节点Kubernetes集群](https://blog.51cto.com/u_14449327/4974873)
- <https://github.com/AliyunContainerService/minikube/wiki>

## configmap

- [k8s中configmap挂载配置nginx.conf](https://blog.csdn.net/weixin_47415962/article/details/116003059)
- [实战：ConfigMap(可变应用配置管理)-2021.11.25](https://mdnice.com/writing/9758102aed3a4acf8807bf2d34c2ee34)

## subPath

- [【原创】Kubernetes-subPath的使用](https://www.cnblogs.com/gdut1425/p/13112176.html)

## secret

- [kubernetes secret和serviceaccount删除](https://www.cnblogs.com/williamzheng/p/11464883.html)

## 存储

- [Kubernetes K8S之存储PV-PVC详解](https://www.cnblogs.com/zhanglianghhh/p/13861817.html)
- [k8s之PV、PVC、StorageClass详解](https://www.toutiao.com/article/6922438959575040523/)
- [kubernetes（k8s）PVC的使用](https://www.jianshu.com/p/7de8d639aa09)
- [K8s中pv和pvc的使用](https://blog.csdn.net/xiaoguangtouqiang/article/details/104053077)
- [k8s之PV、PVC、StorageClass详解](https://www.toutiao.com/article/6922438959575040523)
- [<font color=Red>k8s持久化存储PV、PVC、StorageClass</font>](https://www.cnblogs.com/qlqwjy/p/15817294.html)

## yaml

- [golang的yaml解析（使用“-”分隔的多文档解析）](https://blog.csdn.net/u013798334/article/details/115160683)

## Calico

- [k8s pod访问不通外网问题排查](https://www.cnblogs.com/abcdef/p/11651974.html)
- [https://blog.51cto.com/u_11101184/3134907](http://%E3%80%8CKubernetes%E3%80%8D-%20%E5%B8%B8%E8%A7%81%20Calico%20%E9%97%AE%E9%A2%98%20@1.52.98.246/)
- [[Solved] calico/node is not ready: BIRD is not ready: BGP not established (Calico 3.6 / k8s 1.14.1)](https://www.unixcloudfusion.in/2022/02/solved-caliconode-is-not-ready-bird-is.html)
- [图解 Kubernetes 容器网络发展](https://mp.weixin.qq.com/s/M0BkXBMkNBECnmC6UBRDIg)

## namespace

- [今天讲讲k8s中的namespace](https://www.toutiao.com/article/7106748671789826592)

## path

- [kubectl patch](https://www.cnblogs.com/lizhaoxian/p/11544394.html)

## 服务实践

- [k8s集群上部署mysql服务实践](https://www.dandelioncloud.cn/article/details/1507208260900966401)
- [k8s部署mysql](https://blog.csdn.net/wuchenlhy/article/details/124353338)
- [k8s集群安装部署单机MySQL（使用StorageClass作为后端存储）](https://blog.csdn.net/alwaysbefine/article/details/125633538)
- [k8s部署mysql数据持久化](http://t.zoukankan.com/pluto-charon-p-14411780.html)

- [解决k8s集群在节点运行kubectl出现的错误：The connection to the server localhost:8080 was refused - did you specify t](https://blog.csdn.net/koukouwuwu/article/details/118152375)
- [kubectl命令出现错误“The connection to the server localhost:8080 was refused”](https://blog.csdn.net/lisongyue123/article/details/109643218)

- [Kubernetes新版本不支持Docker，是真的吗？](https://www.toutiao.com/article/7204823884712985144)
