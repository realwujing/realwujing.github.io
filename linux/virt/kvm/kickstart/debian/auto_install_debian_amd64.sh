#!/bin/bash

# =============================================
# 全自动 Debian 12 安装脚本（系统推荐分区方案）
# =============================================

# 定义虚拟机参数
# 基础目录（复用路径）
ISO_DIR="/data/yql"

# ISO配置
ISO_URL="https://get.debian.org/images/archive/12.10.0/amd64/iso-cd/debian-12.10.0-amd64-netinst.iso"
ISO_FILE="${ISO_DIR}/${ISO_URL##*/}"

# 虚拟机配置
VM_NAME="yql-debian12"
VM_RAM="16384"       # 16GB内存
VM_VCPUS="16"        # 16个vCPU
VM_DISK_SIZE="256"    # 256GB磁盘

# 从ISO文件名提取基础名（去掉.iso），并加上.qcow2
VM_DISK_BASENAME="${ISO_URL##*/}"      # 提取文件名：debian-12.10.0-amd64-netinst.iso
VM_DISK_BASENAME="${VM_DISK_BASENAME%.iso}"  # 去掉.iso：debian-12.10.0-amd64-netinst
VM_DISK_PATH="${ISO_DIR}/$VM_NAME-${VM_DISK_BASENAME}.qcow2"  # 最终路径


# Kickstart/Preseed配置
KS_CONFIG="${ISO_DIR}/debian-preseed.cfg"

# 示例：打印关键路径
echo "虚拟机磁盘路径: ${VM_DISK_PATH}"
echo "ISO文件路径: ${ISO_FILE}"
echo "Preseed配置路径: ${KS_CONFIG}"

# 生成加密密码（明文为 autoinstall）
ENCRYPTED_PWD=$(echo 'autoinstall' | openssl passwd -6 -salt xyz123 -stdin)

# 创建 Kickstart 配置文件
cat > "$KS_CONFIG" << EOF
========== 基本系统设置 ==========
d-i debian-installer/locale string en_US
d-i keyboard-configuration/xkb-keymap select us
d-i console-setup/ask_detect boolean false

# ========== 语言 & 区域设置 ==========
# 1. 系统语言设置为英文（界面显示英文）
d-i debian-installer/language string en
d-i debian-installer/country string US
d-i debian-installer/locale string en_US.UTF-8

# 2. 额外支持中文UTF-8（确保能显示和处理中文）
d-i localechooser/supported-locales multiselect en_US.UTF-8, zh_CN.UTF-8

# ========== 键盘布局 ==========
d-i keyboard-configuration/xkb-keymap select us  # 英文键盘

# ========== 时区 ==========
d-i clock-setup/utc boolean true
d-i time/zone string Asia/Shanghai
d-i clock-setup/ntp boolean true

# ========== 网络设置 ==========
d-i netcfg/choose_interface select auto
d-i netcfg/get_hostname string debian 
d-i netcfg/get_domain string debian 

# ========== 镜像源设置（清华源） ==========
d-i mirror/country string manual
d-i mirror/http/hostname string mirrors.tuna.tsinghua.edu.cn
d-i mirror/http/directory string /debian
d-i mirror/http/proxy string

# ========== 账户设置 ==========
d-i passwd/root-login boolean true
d-i passwd/root-password-crypted password $ENCRYPTED_PWD
d-i passwd/user-fullname string wujing
d-i passwd/username string wujing
d-i passwd/user-password-crypted password $ENCRYPTED_PWD

# ========== 磁盘分区（系统自动推荐方案） ==========
d-i partman-auto/method string regular
d-i partman-auto/disk string /dev/[sv]da
d-i partman-auto/choose_recipe select atomic
d-i partman-partitioning/confirm_write_new_label boolean true
d-i partman/choose_partition select finish
d-i partman/confirm boolean true
d-i partman/confirm_nooverwrite boolean true

# ========== Apt 软件源设置 ==========
# 启用 non-free 软件包源
d-i apt-setup/non-free boolean true

# 启用 contrib 软件包源
d-i apt-setup/contrib boolean true

# 选择所有主要软件包源（main + contrib + non-free）
d-i apt-setup/services-select multiselect main, contrib, non-free

# 允许使用未经身份验证的软件包（仅限安装阶段）
d-i debian-installer/allow_unauthenticated boolean true

# 禁用光盘源（强制使用网络镜像）
d-i apt-setup/cdrom/set-first boolean false
d-i apt-setup/cdrom/set-next boolean false  
d-i apt-setup/cdrom/set-failed boolean false

# ========== 软件包选择 ==========
tasksel tasksel/first multiselect standard, ssh-server
d-i pkgsel/include string openssh-server qemu-guest-agent sudo
d-i pkgsel/upgrade select none
d-i pkgsel/language-packs multiselect en, zh
d-i pkgsel/update-policy select none

# ========== 禁止在安装的时候弹出popularity ==========
popularity-contest popularity-contest/participate boolean false
 
# ========== Boot loader installation ==========
d-i grub-installer/only_debian boolean true
d-i grub-installer/bootdev string /dev/[sv]da
 
# 安装完成之后不要弹出安装完成的界面，直接重启
d-i finish-install/reboot_in_progress note

# ========== 安装后脚本 ==========
d-i preseed/late_command string \
    in-target gpasswd -a wujing sudo; \
    in-target mkdir -p /home/wujing/Downloads; \
    in-target mkdir -p /home/wujing/Documents; \
    in-target mkdir -p /home/wujing/Music; \
    in-target mkdir -p /home/wujing/Pictures; \
    in-target mkdir -p /home/wujing/Videos; \
    in-target chown -R wujing:wujing /home/wujing; \
    in-target chmod -R 755 /home/wujing; \
    in-target systemctl enable qemu-guest-agent;
EOF

# 创建目录（确保路径存在）
mkdir -p "$ISO_DIR"

# 下载ISO（如果不存在）
if [ ! -f "$ISO_FILE" ]; then
 echo "正在下载 Debian ISO 到 ${ISO_DIR}..."
 wget -O "$ISO_FILE" "$ISO_URL" || { echo "下载失败，请检查网络或URL"; exit 1; }
fi

# 清理旧虚拟机（关键修改：放在virt-install之前）
echo "正在清理旧虚拟机..."
virsh destroy "$VM_NAME" 2>/dev/null
virsh undefine "$VM_NAME" 2>/dev/null
rm -f "$VM_DISK_PATH" 2>/dev/null

# 执行全自动安装
virt-install \
 --name "$VM_NAME" \
 --memory "$VM_RAM" \
 --vcpus "$VM_VCPUS" \
 --disk path="$VM_DISK_PATH",size=$VM_DISK_SIZE,format=qcow2,bus=virtio \
 --location "$ISO_FILE" \
 --os-type linux \
 --os-variant debian11 \
 --network bridge=virbr0 \
 --graphics none \
 --console pty,target_type=serial \
 --initrd-inject "$KS_CONFIG" \
 --extra-args "\
 console=ttyS0,115200n8 \
 auto=true \
 file=/$(basename "$KS_CONFIG") \
 DEBCONF_DEBUG=5 \
 -- quiet"