# KVM 虚拟机端口转发配置

通过 iptables 将物理机端口转发到 KVM 虚拟机，实现公网访问虚拟机内部服务。

## 网络拓扑

```
公网 (116.6.197.242:16589)
    ↓ (路由器 NAT)
物理机 (192.168.0.205:16589) [网卡: ens2f1]
    ↓ (iptables DNAT + MASQUERADE)
虚拟机 (192.168.122.79:16589)
```

## 快速开始

```bash
# 使用 Makefile（推荐）
make setup          # 配置端口转发
make debug          # 诊断问题
make remove         # 删除规则

# 或直接运行脚本
sudo ./setup-port-forward.sh
sudo ./debug-port-forward.sh
sudo ./remove-port-forward.sh

# 测试
curl http://192.168.0.205:16589  # 物理机本地测试
curl http://116.6.197.242:16589  # 公网测试（需从外部机器）
```

## 脚本说明

| 文件 | 功能 | 说明 |
|------|------|------|
| **Makefile** | 快捷命令 | setup/remove/debug，支持自定义参数 |
| **setup-port-forward.sh** | 配置端口转发 | 配置 6 条规则：IP转发 + 5个iptables规则 |
| **remove-port-forward.sh** | 删除端口转发 | 循环删除所有匹配规则并验证 |
| **debug-port-forward.sh** | 诊断配置问题 | 检查 7 项（消除重复检查，性能优化） |

## iptables 规则详解

setup-port-forward.sh 配置了 6 条规则：

| 序号 | 规则 | 必须 | 作用 |
|------|------|------|------|
| 1 | IP 转发 | ✓ | 允许内核在网络接口之间转发数据包 |
| 2 | INPUT | ✓ | 允许外部流量进入物理机端口（公网访问必需） |
| 3 | PREROUTING (DNAT) | ✓ | 将外部流量目标地址改为虚拟机（核心规则） |
| 4 | OUTPUT (DNAT) | ✗ | 处理物理机访问自己 IP 的流量（本地测试用） |
| 5 | FORWARD | ✓ | 允许数据包在物理机和虚拟机之间双向转发 |
| 6 | POSTROUTING (MASQUERADE) | ✓ | 源地址改为物理机 IP（虚拟机内网 IP 无法被公网识别） |

### 流量路径详解

**外部访问流程（公网 → 虚拟机）：**
```
1. 外部客户端:12345 → 116.6.197.242:16589
   ↓ (路由器 NAT)
2. 外部客户端:12345 → 192.168.0.205:16589
   ↓ (INPUT 链允许)
   ↓ (PREROUTING DNAT: 目标改为虚拟机)
3. 外部客户端:12345 → 192.168.122.79:16589
   ↓ (FORWARD 链允许)
   ↓ (POSTROUTING MASQUERADE: 源改为物理机)
4. 192.168.122.1:54321 → 192.168.122.79:16589
   ↓ (虚拟机处理)
5. 192.168.122.79:16589 → 192.168.122.1:54321
   ↓ (NAT 连接跟踪表还原，原路返回)
6. 192.168.0.205:16589 → 外部客户端:12345

关键：虚拟机看到请求来自 192.168.122.1（物理机），回复也发给物理机
      如果没有 MASQUERADE，虚拟机会尝试回复公网 IP，但内网地址无法被公网识别
```

**物理机本地测试流程：**
```
1. 物理机:54321 → 192.168.0.205:16589
   ↓ (OUTPUT DNAT: 目标改为虚拟机)
2. 物理机:54321 → 192.168.122.79:16589
   ↓ (POSTROUTING MASQUERADE: 源改为物理机)
3. 192.168.122.1:54321 → 192.168.122.79:16589
```

## 手动配置

如果需要手动配置（按数据流入顺序）：

```bash
# 公网 → 物理机 → 虚拟机
PUBLIC_IP="116.6.197.242"
HOST_IP="192.168.0.205"
HOST_PORT="16589"
HOST_IFACE="ens2f1"
VM_IP="192.168.122.79"
VM_PORT="16589"

# 1. 启用 IP 转发
sudo sysctl -w net.ipv4.ip_forward=1

# 2. 允许 INPUT 接收端口（公网访问必需）
sudo iptables -I INPUT -p tcp --dport ${HOST_PORT} -j ACCEPT

# 3. PREROUTING: 将到达物理机的流量转发到虚拟机
sudo iptables -t nat -A PREROUTING -p tcp --dport ${HOST_PORT} -j DNAT --to-destination ${VM_IP}:${VM_PORT}

# 4. OUTPUT: 处理本机发出的流量（用于本地测试）
sudo iptables -t nat -A OUTPUT -p tcp --dport ${HOST_PORT} -d ${HOST_IP} -j DNAT --to-destination ${VM_IP}:${VM_PORT}

# 5. FORWARD: 允许转发的流量通过
sudo iptables -I FORWARD -p tcp -d ${VM_IP} --dport ${VM_PORT} -j ACCEPT
sudo iptables -I FORWARD -p tcp -s ${VM_IP} --sport ${VM_PORT} -j ACCEPT

# 6. POSTROUTING: 对转发的流量做 SNAT，使虚拟机能正确回复
sudo iptables -t nat -A POSTROUTING -d ${VM_IP} -p tcp --dport ${VM_PORT} -j MASQUERADE
```

## 测试指南

### 本地测试

```bash
# 测试虚拟机直接访问
curl http://192.168.122.79:16589

# 测试物理机端口转发
curl http://192.168.0.205:16589
```

### 公网测试

**重要：** 从物理机访问公网 IP 会失败（hairpin NAT 问题），这是正常的。

正确的测试方法：

1. **从外部机器测试**（推荐）
   ```bash
   # 从另一台电脑、云服务器执行
   curl http://116.6.197.242:16589
   ```

2. **从手机测试**
   - 手机连接 4G/5G（不要连 WiFi）
   - 浏览器访问 `http://116.6.197.242:16589`

3. **使用在线工具**
   - https://www.yougetsignal.com/tools/open-ports/
   - https://ping.pe/116.6.197.242:16589

### 路由器配置

确保路由器已配置端口转发：
```
外部端口: 16589
内部 IP: 192.168.0.205
内部端口: 16589
协议: TCP
```

## 故障排查

### 运行诊断脚本

```bash
sudo ./debug-port-forward.sh
```

诊断脚本会检查（所有检查只执行一次，消除重复）：
1. 物理机端口监听状态
2. NAT PREROUTING（外部流量转发）
3. NAT OUTPUT（本机流量转发）
4. INPUT（公网访问必需）
5. FORWARD（流量转发）
6. NAT POSTROUTING（源地址转换）
7. IP 转发状态
8. 虚拟机连通性
9. 虚拟机端口状态
10. 路由器配置提示
11. 抓包监控命令

最后给出诊断总结，检查 7 项核心配置：
- NAT PREROUTING, NAT OUTPUT, INPUT, FORWARD, NAT POSTROUTING
- IP 转发, 虚拟机端口

### 常见问题

1. **虚拟机服务未监听**
   ```bash
   # 在虚拟机内检查
   ss -tlnp | grep 16589
   # 确保监听在 0.0.0.0:16589 而不是 127.0.0.1:16589
   ```

2. **物理机防火墙阻止**
   ```bash
   # 检查 INPUT 链
   sudo iptables -L INPUT -n -v
   # 添加允许规则
   sudo iptables -I INPUT -p tcp --dport 16589 -j ACCEPT
   ```

3. **路由器端口转发未生效**
   ```bash
   # 在物理机上抓包，从外部访问时观察
   sudo tcpdump -i ens2f1 port 16589 -n
   ```

4. **IP 转发未启用**
   ```bash
   # 检查
   cat /proc/sys/net/ipv4/ip_forward
   # 启用
   echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
   ```

## 常见错误和解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 公网访问失败 | INPUT 规则未配置 | `sudo iptables -I INPUT -p tcp --dport 16589 -j ACCEPT` |
| 物理机本地测试失败 | OUTPUT 规则未配置 | 运行 `sudo ./setup-port-forward.sh` |
| 虚拟机端口无响应 | 服务未运行或监听 127.0.0.1 | 检查服务，确保监听 `0.0.0.0:16589` |
| 从物理机访问公网 IP 失败 | hairpin NAT 问题 | 正常现象，从外部机器测试 |
| IP 转发未启用 | 系统配置 | `echo 1 \| sudo tee /proc/sys/net/ipv4/ip_forward` |
| 规则删不干净 | 重复添加 | 使用 `sudo ./remove-port-forward.sh`（循环删除） |

## 持久化配置

使用 iptables-persistent (Debian/Ubuntu):

```bash
sudo apt install iptables-persistent
sudo netfilter-persistent save
```

## 自定义配置

使用 Makefile 自定义参数：

```bash
# 修改虚拟机 IP 和端口
make setup VM_IP=192.168.122.100 VM_PORT=8080

# 修改物理机端口
make setup HOST_PORT=8080

# 完整自定义
make setup PUBLIC_IP=1.2.3.4 HOST_IP=192.168.1.100 HOST_PORT=8080 VM_IP=192.168.122.100 VM_PORT=9000
```

或直接设置环境变量：

```bash
export VM_IP=192.168.122.100
export VM_PORT=8080
sudo -E ./setup-port-forward.sh
```

## 注意事项

1. **MASQUERADE 的必要性**
   - 虚拟机 IP (192.168.122.79) 是内网地址，公网无法识别
   - 没有 MASQUERADE，虚拟机无法正确回复公网客户端
   - MASQUERADE 让虚拟机以为请求来自物理机，回复也发给物理机

2. **虚拟机服务配置**
   ```bash
   # 在虚拟机内启动测试服务
   python3 -m http.server 16589
   ```
   - 必须监听 `0.0.0.0:16589`，不能只监听 `127.0.0.1`
   - 检查虚拟机内防火墙是否允许端口

3. **路由器配置**
   - 必须配置端口转发：`公网端口 → 物理机IP:端口`
   - 使用 `tcpdump` 验证路由器是否转发流量

4. **端口选择**
   - 避免运营商封禁的端口（80, 443, 8080 等）
   - 建议使用高端口 (10000-65535)

5. **持久化**
   - iptables 规则重启后会丢失
   - 使用 `iptables-persistent` 持久化

## 快速参考

### 常用命令

```bash
# 使用 Makefile
make help           # 查看帮助和当前配置
make setup          # 配置端口转发
make debug          # 诊断问题
make remove         # 删除规则

# 查看 iptables 规则
sudo iptables -t nat -L -n -v    # NAT 表
sudo iptables -L -n -v           # filter 表

# 查看 IP 转发状态
cat /proc/sys/net/ipv4/ip_forward

# 查看虚拟机 IP
virsh domifaddr <vm-name>

# 测试虚拟机端口
nc -zv 192.168.122.79 16589

# 实时监控流量
sudo tcpdump -i ens2f1 port 16589 -n

# 查看连接跟踪（验证 NAT 是否工作）
sudo conntrack -L | grep 16589
```

### 验证配置

```bash
# 查看 NAT 规则
sudo iptables -t nat -L -n -v | grep 16589

# 查看 FORWARD 规则
sudo iptables -L FORWARD -n -v | grep 16589

# 查看 INPUT 规则
sudo iptables -L INPUT -n -v | grep 16589
```

## 相关链接

- [iptables 官方文档](https://netfilter.org/documentation/)
- [iptables 教程](https://www.frozentux.net/iptables-tutorial/iptables-tutorial.html)
- [KVM 网络配置](https://wiki.libvirt.org/page/Networking)
