#!/bin/bash
# Remove port forwarding rules

# 加载配置（按数据流入顺序：公网 -> 物理机 -> 虚拟机）
PUBLIC_IP="${PUBLIC_IP:-116.6.197.242}"
HOST_IP="${HOST_IP:-192.168.0.205}"
HOST_PORT="${HOST_PORT:-16589}"
VM_IP="${VM_IP:-192.168.122.79}"
VM_PORT="${VM_PORT:-16589}"  # 物理机外网接口

echo "Removing port forwarding rules..."

# 删除所有匹配的规则（循环删除直到没有为止）
while iptables -D INPUT -p tcp --dport ${HOST_PORT} -j ACCEPT 2>/dev/null; do :; done
while iptables -t nat -D PREROUTING -p tcp --dport ${HOST_PORT} -j DNAT --to-destination ${VM_IP}:${VM_PORT} 2>/dev/null; do :; done
while iptables -t nat -D OUTPUT -p tcp --dport ${HOST_PORT} -d ${HOST_IP} -j DNAT --to-destination ${VM_IP}:${VM_PORT} 2>/dev/null; do :; done
while iptables -D FORWARD -p tcp -d ${VM_IP} --dport ${VM_PORT} -j ACCEPT 2>/dev/null; do :; done
while iptables -D FORWARD -p tcp -s ${VM_IP} --sport ${VM_PORT} -j ACCEPT 2>/dev/null; do :; done
while iptables -t nat -D POSTROUTING -d ${VM_IP} -p tcp --dport ${VM_PORT} -j MASQUERADE 2>/dev/null; do :; done

echo "Port forwarding rules removed!"
echo ""
echo "Verifying..."
iptables -t nat -L PREROUTING -n -v 2>/dev/null | grep -q 16589 && echo "  ⚠ NAT PREROUTING 仍然存在" || echo "  ✓ NAT PREROUTING 已清理"
iptables -t nat -L OUTPUT -n -v 2>/dev/null | grep -q 16589 && echo "  ⚠ NAT OUTPUT 仍然存在" || echo "  ✓ NAT OUTPUT 已清理"
iptables -L INPUT -n -v 2>/dev/null | grep -q "dpt:16589" && echo "  ⚠ INPUT 仍然存在" || echo "  ✓ INPUT 已清理"
iptables -L FORWARD -n -v 2>/dev/null | grep -q 16589 && echo "  ⚠ FORWARD 仍然存在" || echo "  ✓ FORWARD 已清理"
iptables -t nat -L POSTROUTING -n -v 2>/dev/null | grep -q 16589 && echo "  ⚠ NAT POSTROUTING 仍然存在" || echo "  ✓ NAT POSTROUTING 已清理"
