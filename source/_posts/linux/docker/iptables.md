---
date: 2024/07/23 11:12:26
updated: 2024/07/23 11:12:26
---

# iptables

- [docker容器启动后添加端口映射_realwujing的博客-CSDN博客](https://blog.csdn.net/qq_30013585/article/details/116191215)

```bash
sudo iptables -t nat -A  DOCKER -p tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080

sudo iptables -t nat -A  DOCKER -p tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080
```

## 1、PREROUTING链

### 1.1 查看NAT表中的PREROUTING链

```bash
sudo iptables -t nat --list-rules PREROUTING
```

### 结果

```bash
-P PREROUTING ACCEPT
-A PREROUTING -p tcp -m addrtype --dst-type LOCAL -j DOCKER

sudo iptables -t nat -A PREROUTING -p tcp -m addrtype --dst-type LOCAL -j DOCKER
sudo iptables -t nat -A PREROUTING -p tcp -m tcp --dport 8081 -j DNAT --to-destination 192.168.9.151:8080
sudo iptables -t nat --list-rules DOCKER
sudo iptables -t nat -D DOCKER ! -i docker0 -p tcp -m tcp --dport 8081 -j DNAT --to-destination 172.17.0.2:8080

sudo iptables -t nat --list-rules POSTROUTING
sudo iptables -t nat -D POSTROUTING -s 172.17.0.2/32 -d 172.17.0.2/32 -p tcp -m tcp --dport 8080 -j MASQUERADE

sudo iptables --list-rules DOCKER
sudo iptables -t filter -D DOCKER -d 172.17.0.2/32 ! -i docker0 -o docker0 -p tcp -m tcp --dport 8080 -j ACCEPT

curl 172.17.0.2:8080/test/hello

sudo iptables -t nat -vnL DOCKER --line-number

sudo iptables -t nat -D DOCKER 4

sudo iptables -t nat --list-rules PREROUTING
sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 8081 -j DNAT --to-destination 192.168.9.151:8080
```
