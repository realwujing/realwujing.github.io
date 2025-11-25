#!/bin/bash

# 定义变量
VM_NAME="ubuntu24-desktop-test"
RAM_SIZE="4096"
VCPUS="8"
DISK_SIZE="256"
DISK_PATH="/var/lib/libvirt/images/${VM_NAME}.qcow2"
ISO_PATH="/var/lib/libvirt/images/ubuntu-24.04.2-desktop-amd64.iso"  # 请务必修改为您的ISO实际路径
SEED_ISO_PATH="/var/lib/libvirt/images/${VM_NAME}-seed.iso"

# 创建临时目录用于存放自动安装配置
CONFIG_DIR=$(mktemp -d)
cd "$CONFIG_DIR"

# 读取用户信息
echo "请输入用户名 (将同时用作登录名和显示名):"
read USER_NAME

# 生成加密密码（请先安装 whois 包：sudo apt install whois）
# 下面这行会提示您输入密码,并生成加密后的哈希值
# 读入您想设置的密码,然后会生成一个加密哈希
echo "请为用户 '$USER_NAME' 输入密码（输入时不会显示）:"
read -s USER_PASSWORD
PASSWORD_HASH=$(mkpasswd --method=sha-512 "$USER_PASSWORD")
echo  # 输出换行

# 1. 创建针对桌面版的 user-data 自动安装配置文件
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
    - echo "deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble main restricted universe multiverse" > /target/etc/apt/sources.list
    - echo "deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-updates main restricted universe multiverse" >> /target/etc/apt/sources.list
    - echo "deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-backports main restricted universe multiverse" >> /target/etc/apt/sources.list
    - echo "deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-security main restricted universe multiverse" >> /target/etc/apt/sources.list
EOF

# 创建一个空的 meta-data 文件（cloud-init 要求）
touch meta-data

# 2. 生成种子ISO（seed.iso），其中包含自动安装配置
# 确保已安装 cloud-image-utils: sudo apt install cloud-image-utils
cloud-localds seed.iso user-data meta-data

# 将 seed.iso 移动到 libvirt 可访问的目录
sudo mv seed.iso "$SEED_ISO_PATH"
sudo chmod 644 "$SEED_ISO_PATH"

# 3. 使用 virt-install 启动安装
sudo virt-install \
  --name "$VM_NAME" \
  --memory "$RAM_SIZE" \
  --vcpus "$VCPUS" \
  --disk path="$DISK_PATH",size="$DISK_SIZE",bus=virtio \
  --disk path="$SEED_ISO_PATH",device=cdrom \
  --cdrom "$ISO_PATH" \
  --network network=default \
  --graphics spice,listen=0.0.0.0 \
  --video virtio \
  --os-variant ubuntu24.04 \
  --console pty,target_type=serial

echo ""
echo "============================================"
echo "Ubuntu 24.04 桌面版安装安装中！"
echo "虚拟机名称: $VM_NAME"
echo "可以使用以下命令连接查看:"
echo "图形界面: virt-viewer $VM_NAME"
echo "控制台: virsh console $VM_NAME"
echo "管理: virsh start|shutdown|destroy $VM_NAME"