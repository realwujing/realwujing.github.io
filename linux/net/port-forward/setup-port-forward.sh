#!/bin/bash
# Port forwarding: ${HOST_IP}:${HOST_PORT} -> ${VM_IP}:${VM_PORT}

# 加载配置（按数据流入顺序：公网 -> 物理机 -> 虚拟机）
PUBLIC_IP="${PUBLIC_IP:-116.6.197.242}"
HOST_IP="${HOST_IP:-192.168.0.205}"
HOST_PORT="${HOST_PORT:-16589}"
VM_IP="${VM_IP:-192.168.122.79}"
VM_PORT="${VM_PORT:-16589}"
HOST_IFACE="${HOST_IFACE:-ens2f1}"  # 物理机外网接口

echo "Setting up port forwarding: ${HOST_PORT} -> ${VM_IP}:${VM_PORT}"

# 1. 启用 IP 转发
#    作用: 允许 Linux 内核在不同网络接口之间转发数据包
#    必须: 是，否则数据包无法从物理机转发到虚拟机
echo 1 > /proc/sys/net/ipv4/ip_forward

# 2. INPUT 链: 允许物理机接收目标端口为 16589 的 TCP 数据包
#    作用: 允许外部流量进入物理机的 16589 端口
#    必须: 是，否则公网访问会被防火墙阻止
#    流量路径: 外部 -> 物理机:16589 -> INPUT 链检查
iptables -I INPUT -p tcp --dport ${HOST_PORT} -j ACCEPT

# 3. PREROUTING 链 (NAT 表): 目标地址转换 (DNAT)
#    作用: 将到达物理机 16589 端口的数据包目标地址改为虚拟机 IP:PORT
#    必须: 是，这是端口转发的核心规则
#    流量路径: 外部 -> 物理机:16589 -> PREROUTING (DNAT) -> 虚拟机:16589
#    说明: 只处理从外部进入的流量，不处理本机发出的流量
iptables -t nat -A PREROUTING -p tcp --dport ${HOST_PORT} -j DNAT --to-destination ${VM_IP}:${VM_PORT}

# 4. OUTPUT 链 (NAT 表): 处理本机发出的流量
#    作用: 将物理机访问自己 IP:16589 的流量转发到虚拟机
#    必须: 否，但有助于在物理机上测试 (curl http://${HOST_IP}:${HOST_PORT})
#    流量路径: 物理机 -> ${HOST_IP}:${HOST_PORT} -> OUTPUT (DNAT) -> 虚拟机:${VM_PORT}
#    注意: 不能用于访问公网 IP (116.6.197.242)，因为 hairpin NAT 问题
iptables -t nat -A OUTPUT -p tcp --dport ${HOST_PORT} -d ${HOST_IP} -j DNAT --to-destination ${VM_IP}:${VM_PORT}

# 5. FORWARD 链: 允许转发的数据包通过
#    作用: 允许数据包在物理机和虚拟机之间双向转发
#    必须: 是，否则数据包会被 FORWARD 链的默认策略阻止
#    规则 1: 允许发往虚拟机 16589 端口的数据包
#    规则 2: 允许从虚拟机 16589 端口返回的数据包
iptables -I FORWARD -p tcp -d ${VM_IP} --dport ${VM_PORT} -j ACCEPT
iptables -I FORWARD -p tcp -s ${VM_IP} --sport ${VM_PORT} -j ACCEPT

# 6. POSTROUTING 链 (NAT 表): 源地址转换 (SNAT/MASQUERADE)
#    作用: 将转发到虚拟机的数据包源地址改为物理机 IP
#    必须: 是，否则虚拟机无法正确回复（虚拟机 IP 是内网地址，公网客户端无法识别）
#    流量路径: 物理机 -> POSTROUTING (MASQUERADE) -> 虚拟机
#    说明: 虚拟机看到请求来自物理机，回复也发给物理机，再由物理机 NAT 转回公网
iptables -t nat -A POSTROUTING -d ${VM_IP} -p tcp --dport ${VM_PORT} -j MASQUERADE

echo "Port forwarding rules added successfully!"
echo ""
echo "Test from outside: curl http://${PUBLIC_IP}:${HOST_PORT}"
echo "Test from host: curl http://${HOST_IP}:${HOST_PORT}"
echo ""
echo "To remove rules, run: make remove"
