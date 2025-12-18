#!/bin/bash
# 诊断端口转发问题

# 加载配置（按数据流入顺序：公网 -> 物理机 -> 虚拟机）
PUBLIC_IP="${PUBLIC_IP:-116.6.197.242}"
HOST_IP="${HOST_IP:-192.168.0.205}"
HOST_PORT="${HOST_PORT:-16589}"
HOST_IFACE="${HOST_IFACE:-ens2f1}"
VM_IP="${VM_IP:-192.168.122.79}"
VM_PORT="${VM_PORT:-16589}"

echo "========== 端口转发诊断 =========="
echo ""

# 执行所有检查并保存结果
CHECK_NAT_PREROUTING=$(sudo iptables -t nat -L PREROUTING -n -v | grep ${HOST_PORT})
CHECK_NAT_OUTPUT=$(sudo iptables -t nat -L OUTPUT -n -v | grep ${HOST_PORT})
CHECK_NAT_POSTROUTING=$(sudo iptables -t nat -L POSTROUTING -n -v | grep ${HOST_PORT})
CHECK_INPUT=$(sudo iptables -L INPUT -n -v | grep "dpt:${HOST_PORT}")
CHECK_FORWARD=$(sudo iptables -L FORWARD -n -v | grep ${HOST_PORT})
CHECK_IP_FORWARD=$(cat /proc/sys/net/ipv4/ip_forward)
CHECK_VM_PING=$(ping -c 2 ${VM_IP} > /dev/null 2>&1 && echo "ok")
CHECK_VM_PORT=$(nc -zv -w 2 ${VM_IP} ${VM_PORT} > /dev/null 2>&1 && echo "ok")
CHECK_HOST_LISTEN=$(sudo ss -tlnp | grep ${HOST_PORT})

# 1. 物理机监听状态
echo "1. 物理机 ${HOST_PORT} 端口监听状态："
if [ -n "$CHECK_HOST_LISTEN" ]; then
    echo "   ⚠ 物理机正在监听 ${HOST_PORT}（异常，应该由虚拟机监听）"
    echo "$CHECK_HOST_LISTEN" | sed 's/^/     /'
else
    echo "   ✓ 物理机未监听 ${HOST_PORT}（正常）"
fi
echo ""

# 2. NAT PREROUTING
echo "2. NAT PREROUTING（外部流量转发）："
if [ -n "$CHECK_NAT_PREROUTING" ]; then
    echo "   ✓ 规则已配置"
    echo "$CHECK_NAT_PREROUTING" | sed 's/^/     /'
else
    echo "   ✗ 规则未配置"
fi
echo ""

# 3. NAT OUTPUT
echo "3. NAT OUTPUT（本机流量转发）："
if [ -n "$CHECK_NAT_OUTPUT" ]; then
    echo "   ✓ 规则已配置"
    echo "$CHECK_NAT_OUTPUT" | sed 's/^/     /'
else
    echo "   ✗ 规则未配置"
fi
echo ""

# 4. INPUT
echo "4. INPUT（公网访问必需）："
if [ -n "$CHECK_INPUT" ]; then
    echo "   ✓ 规则已配置"
    echo "$CHECK_INPUT" | sed 's/^/     /'
else
    echo "   ✗ 规则未配置"
fi
echo ""

# 5. FORWARD
echo "5. FORWARD（流量转发）："
if [ -n "$CHECK_FORWARD" ]; then
    echo "   ✓ 规则已配置"
    echo "$CHECK_FORWARD" | sed 's/^/     /'
else
    echo "   ✗ 规则未配置"
fi
echo ""

# 6. NAT POSTROUTING
echo "6. NAT POSTROUTING（源地址转换）："
if [ -n "$CHECK_NAT_POSTROUTING" ]; then
    echo "   ✓ 规则已配置"
    echo "$CHECK_NAT_POSTROUTING" | sed 's/^/     /'
else
    echo "   ✗ 规则未配置"
fi
echo ""

# 7. IP 转发
echo "7. IP 转发："
if [ "$CHECK_IP_FORWARD" = "1" ]; then
    echo "   ✓ 已启用"
else
    echo "   ✗ 未启用（需执行: echo 1 > /proc/sys/net/ipv4/ip_forward）"
fi
echo ""

# 8. 虚拟机连通性
echo "8. 虚拟机连通性："
if [ "$CHECK_VM_PING" = "ok" ]; then
    echo "   ✓ 虚拟机可达"
else
    echo "   ✗ 虚拟机不可达"
fi
echo ""

# 9. 虚拟机端口
echo "9. 虚拟机端口："
if [ "$CHECK_VM_PORT" = "ok" ]; then
    echo "   ✓ 虚拟机 ${VM_PORT} 端口开放"
else
    echo "   ✗ 虚拟机 ${VM_PORT} 端口无响应"
fi
echo ""

# 10. 路由器配置提示
echo "10. 路由器端口转发："
echo "    ⚠ 需手动确认路由器配置："
echo "      外部端口: ${HOST_PORT} → 内部 ${HOST_IP}:${HOST_PORT}"
echo "    ⚠ 外部测试: curl http://${PUBLIC_IP}:${HOST_PORT}"
echo ""

# 11. 抓包监控提示
echo "11. 实时监控："
echo "    sudo tcpdump -i ${HOST_IFACE} port ${HOST_PORT} -n"
echo ""

# 诊断总结
echo "=========================================="
echo "========== 诊断总结 =========="
echo "=========================================="
echo ""
COUNT=0

[ -n "$CHECK_NAT_PREROUTING" ] && echo "  ✓ NAT PREROUTING" && ((COUNT++)) || echo "  ✗ NAT PREROUTING"
[ -n "$CHECK_NAT_OUTPUT" ] && echo "  ✓ NAT OUTPUT" && ((COUNT++)) || echo "  ✗ NAT OUTPUT"
[ -n "$CHECK_INPUT" ] && echo "  ✓ INPUT" && ((COUNT++)) || echo "  ✗ INPUT"
[ -n "$CHECK_FORWARD" ] && echo "  ✓ FORWARD" && ((COUNT++)) || echo "  ✗ FORWARD"
[ -n "$CHECK_NAT_POSTROUTING" ] && echo "  ✓ NAT POSTROUTING" && ((COUNT++)) || echo "  ✗ NAT POSTROUTING"
[ "$CHECK_IP_FORWARD" = "1" ] && echo "  ✓ IP 转发" && ((COUNT++)) || echo "  ✗ IP 转发"
[ "$CHECK_VM_PORT" = "ok" ] && echo "  ✓ 虚拟机端口" && ((COUNT++)) || echo "  ✗ 虚拟机端口"

echo ""
if [ $COUNT -eq 7 ]; then
    echo "✓ 所有检查通过！端口转发配置正确。"
elif [ $COUNT -ge 6 ]; then
    echo "⚠ iptables 配置正确，但虚拟机端口未响应。请检查虚拟机内服务是否运行。"
else
    echo "✗ 发现 $((7-COUNT)) 个问题，请运行: make setup"
fi
echo ""


