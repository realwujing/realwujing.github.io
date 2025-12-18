# KVM 虚拟机端口转发 Makefile
# 使用方法:
#   make setup          - 配置端口转发 (setup-port-forward.sh)
#   make remove         - 删除端口转发 (remove-port-forward.sh)
#   make debug          - 诊断配置问题 (debug-port-forward.sh)
#
# 自定义配置:
#   make setup VM_IP=192.168.122.100 HOST_PORT=8080

# 默认配置（按数据流入顺序：公网 -> 物理机 -> 虚拟机）
PUBLIC_IP ?= 116.6.197.242
HOST_IP ?= 192.168.0.205
HOST_PORT ?= 16589
HOST_IFACE ?= ens2f1
VM_IP ?= 192.168.122.79
VM_PORT ?= 16589

# 导出变量供脚本使用
export PUBLIC_IP HOST_IP HOST_PORT HOST_IFACE VM_IP VM_PORT

.PHONY: help setup remove debug

help:	## 显示帮助信息
	@echo "KVM 虚拟机端口转发管理"
	@echo ""
	@echo "当前配置 (数据流入顺序):"
	@echo "  公网: $(PUBLIC_IP):$(HOST_PORT)"
	@echo "      ↓ (路由器 NAT)"
	@echo "  物理机: $(HOST_IP):$(HOST_PORT) [网卡: $(HOST_IFACE)]"
	@echo "      ↓ (iptables DNAT)"
	@echo "  虚拟机: $(VM_IP):$(VM_PORT)"
	@echo ""
	@echo "命令:"
	@echo "  make setup          配置端口转发 (setup-port-forward.sh)"
	@echo "  make remove         删除端口转发 (remove-port-forward.sh)"
	@echo "  make debug          诊断配置问题 (debug-port-forward.sh)"
	@echo ""
	@echo "可自定义参数:"
	@echo "  PUBLIC_IP           公网 IP (默认: 116.6.197.242)"
	@echo "  HOST_IP             物理机 IP (默认: 192.168.0.205)"
	@echo "  HOST_PORT           物理机端口 (默认: 16589)"
	@echo "  HOST_IFACE          物理机网卡 (默认: ens2f1)"
	@echo "  VM_IP               虚拟机 IP (默认: 192.168.122.79)"
	@echo "  VM_PORT             虚拟机端口 (默认: 16589)"
	@echo ""
	@echo "示例:"
	@echo "  make setup VM_IP=192.168.122.100 HOST_PORT=8080"
	@echo "  make debug VM_IP=192.168.122.100 VM_PORT=8080"

setup:	## 配置端口转发
	@sudo -E bash setup-port-forward.sh

remove:	## 删除端口转发
	@sudo -E bash remove-port-forward.sh

debug:	## 诊断配置问题
	@sudo -E bash debug-port-forward.sh
