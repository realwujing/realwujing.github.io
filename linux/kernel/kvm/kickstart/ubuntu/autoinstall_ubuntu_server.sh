#!/bin/bash

# 定义变量
VM_NAME="ubuntu24-server"
RAM_SIZE="4096"
VCPUS="8"
DISK_SIZE="256"
DISK_PATH="/var/lib/libvirt/images/${VM_NAME}.qcow2"
ISO_PATH="/var/lib/libvirt/images/ubuntu-24.04.2-live-server-amd64.iso"
SEED_ISO_PATH="/var/lib/libvirt/images/${VM_NAME}-seed.iso"

# 检查并下载ISO
if [ ! -f "$ISO_PATH" ]; then
    echo "ISO文件不存在，从清华源下载..."
    sudo wget -O "$ISO_PATH" https://mirrors.tuna.tsinghua.edu.cn/ubuntu-releases/24.04.2/ubuntu-24.04.3-live-server-amd64.iso
    echo "下载完成"
fi

# 创建临时目录用于存放自动安装配置
CONFIG_DIR=$(mktemp -d)
cd "$CONFIG_DIR" || exit

# 读取用户信息
echo "请输入用户名 (将同时用作登录名和显示名):"
read USER_NAME

# 生成加密密码（请先安装 whois 包：sudo apt install whois）
echo "请为用户 '$USER_NAME' 输入密码（输入时不会显示）:"
read -s USER_PASSWORD
PASSWORD_HASH=$(mkpasswd --method=sha-512 "$USER_PASSWORD")
echo  # 输出换行

# 创建 user-data 自动安装配置文件
cat > user-data << EOF
#cloud-config
autoinstall:
  version: 1
  apt:
    preserve_sources_list: false
    primary:
      - arches: [default]
        uri: https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
  updates: security
  locale: en_US.UTF-8
  timezone: Asia/Shanghai
  keyboard:
    layout: us
  storage:
    layout:
      name: direct
  identity:
    hostname: ubuntu
    realname: "$USER_NAME"
    username: "$USER_NAME"
    password: "$PASSWORD_HASH"
  ssh:
    install-server: true
    allow-pw: true
  late-commands:
    - curtin in-target -- systemctl enable ssh
EOF

# 创建空的 meta-data 文件
touch meta-data

# 生成种子ISO
cloud-localds seed.iso user-data meta-data

# 移动到 libvirt 目录
sudo mv seed.iso "$SEED_ISO_PATH"
sudo chmod 644 "$SEED_ISO_PATH"

# 启动安装
sudo virt-install \
  --name "$VM_NAME" \
  --memory "$RAM_SIZE" \
  --vcpus "$VCPUS" \
  --disk path="$DISK_PATH",size="$DISK_SIZE",bus=virtio \
  --disk path="$SEED_ISO_PATH",device=cdrom \
  --location "$ISO_PATH" \
  --extra-args="console=ttyS0,115200n8 serial" \
  --initrd-inject="$CONFIG_DIR/user-data" \
  --initrd-inject="$CONFIG_DIR/meta-data" \
  --network network=default \
  --graphics none \
  --os-variant ubuntu24.04 \
  --serial pty \
  --console pty,target_type=serial \
  --wait=-1

echo ""
echo "============================================"
echo "Ubuntu 24.04 Server 安装已完成！"
echo "虚拟机名称: $VM_NAME"
echo "连接虚拟机: virsh console $VM_NAME"
echo "管理命令: virsh start|shutdown|destroy $VM_NAME"
