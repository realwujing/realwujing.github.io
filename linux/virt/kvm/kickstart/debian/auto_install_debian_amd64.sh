#!/bin/bash

# 检查是否以root权限运行
if [ "$(id -u)" -ne 0 ]; then
  echo "错误：请以root权限运行此脚本（建议使用 sudo $0 ...）"
  exit 1
fi

# =============================================
# 全自动 Debian 12 安装脚本（系统推荐分区方案）
# =============================================

# 定义虚拟机参数
# 基础目录（支持环境变量或参数自定义）
ISO_DIR="${ISO_DIR:-/var/lib/libvirt/images}"
QEMU_DIR="${QEMU_DIR:-/var/lib/libvirt/images}"

# ISO配置
ISO_URL="https://get.debian.org/images/archive/12.12.0/amd64/iso-dvd/debian-12.12.0-amd64-DVD-1.iso"

# 虚拟机配置
# 用户名变量（支持环境变量、命令行参数 vm_user=xxx 或位置参数）
VM_USER=""
for arg in "$@"; do
  case "$arg" in
    vm_user=*)
      VM_USER="${arg#vm_user=}"
      ;;
  esac
done

# 兼容位置参数
if [ $# -ge 1 ] && [ -z "${1##vm_user=*}" ]; then
  : # 已处理
elif [ $# -ge 1 ]; then
  VM_USER="$1"
fi

# 帮助信息
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
  echo "用法: $0 vm_user=用户名 [vm_ram=内存MB] [vm_vcpus=CPU数] [vm_disk_size=磁盘GB] [iso_dir=ISO目录] [qemu_dir=磁盘目录]"
  echo "      或使用环境变量: VM_USER=用户名 VM_RAM=内存 $0"
  echo
  echo "参数说明："
  echo "  vm_user        虚拟机用户名（必填，无默认值）"
  echo "  vm_ram         分配内存（MB，默认: 16384）"
  echo "  vm_vcpus       分配CPU数（默认: 16）"
  echo "  vm_disk_size   磁盘大小（GB，默认: 256）"
  echo "  iso_dir        ISO存放目录（默认: /var/lib/libvirt/images）"
  echo "  qemu_dir       虚拟机磁盘目录（默认: /var/lib/libvirt/images）"
  exit 0
fi

# 检查是否输入了 vm_user 参数，否则退出
if [[ -z "$VM_USER" ]]; then
  echo "错误：必须指定虚拟机用户名 vm_user，例如："
  echo "  $0 vm_user=yourname"
  echo "  或  VM_USER=yourname $0"
  echo "  或  $0 yourname"
  exit 1
fi

# 资源参数默认值
VM_RAM="${VM_RAM:-16384}"         # 16GB内存
VM_VCPUS="${VM_VCPUS:-16}"        # 16个vCPU
VM_DISK_SIZE="${VM_DISK_SIZE:-256}" # 256GB磁盘

# 解析 key=value 形式参数
for arg in "$@"; do
  case "$arg" in
    vm_user=*)
      VM_USER="${arg#vm_user=}"
      ;;
    vm_ram=*)
      VM_RAM="${arg#vm_ram=}"
      ;;
    vm_vcpus=*)
      VM_VCPUS="${arg#vm_vcpus=}"
      ;;
    vm_disk_size=*)
      VM_DISK_SIZE="${arg#vm_disk_size=}"
      ;;
    iso_dir=*)
      ISO_DIR="${arg#iso_dir=}"
      ;;
    qemu_dir=*)
      QEMU_DIR="${arg#qemu_dir=}"
      ;;
  esac
done


VM_NAME="$VM_USER-debian12"

# 从 ISO_DIR 和 QEMU_DIR 生成实际路径
ISO_FILE="${ISO_DIR}/${ISO_URL##*/}"

# 从ISO文件名提取基础名（去掉.iso），并加上.qcow2
VM_DISK_BASENAME="${ISO_URL##*/}"      # 提取文件名：debian-12.10.0-amd64-netinst.iso
VM_DISK_BASENAME="${VM_DISK_BASENAME%.iso}"  # 去掉.iso：debian-12.10.0-amd64-netinst
VM_DISK_PATH="${QEMU_DIR}/$VM_USER-${VM_DISK_BASENAME}.qcow2"  # 最终路径

# Kickstart/Preseed配置
KS_CONFIG="${QEMU_DIR}/debian-preseed.cfg"

# 示例：打印关键路径
echo "虚拟机磁盘路径: ${VM_DISK_PATH}"
echo "ISO文件路径: ${ISO_FILE}"
echo "Preseed配置路径: ${KS_CONFIG}"

# 生成随机8位密码，包含数字、大小写字母
VM_PASSWD=$(tr -dc 'A-Za-z0-9' </dev/urandom | head -c 8)
ENCRYPTED_PWD=$(echo "$VM_PASSWD" | openssl passwd -6 -salt xyz123 -stdin)

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

# ========== 镜像源设置 ==========
# 启用网络镜像（作为DVD的补充或Fallback，以及安装后的默认源）
d-i apt-setup/use_mirror boolean true
d-i mirror/country string manual
d-i mirror/http/hostname string mirrors.tuna.tsinghua.edu.cn
d-i mirror/http/directory string /debian
d-i mirror/http/proxy string

# ========== 账户设置 ==========
d-i passwd/root-login boolean true
d-i passwd/root-password-crypted password $ENCRYPTED_PWD
d-i passwd/user-fullname string $VM_USER
d-i passwd/username string $VM_USER
d-i passwd/user-password-crypted password $ENCRYPTED_PWD

# ========== 磁盘分区（LVM - All in one） ==========
d-i partman-auto/method string lvm
d-i partman-auto-lvm/guided_size string max
d-i partman-auto-lvm/new_vg_name string debian-vg

d-i partman-auto/disk string /dev/[sv]da
d-i partman-auto/choose_recipe select atomic

# LVM 自动确认
d-i partman-lvm/device_remove_lvm boolean true
d-i partman-lvm/confirm boolean true
d-i partman-lvm/confirm_nooverwrite boolean true

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

# 禁用光盘扫描弹窗
d-i apt-setup/cdrom/set-first boolean false
d-i apt-setup/cdrom/set-next boolean false
d-i apt-setup/cdrom/set-failed boolean false
d-i apt-setup/disable-cdrom-entries boolean true
d-i apt-setup/cdrom/another boolean false

# ========== 软件包选择 ==========
# 安装标准系统工具、SSH服务以及 GNOME 桌面环境
tasksel tasksel/first multiselect standard, ssh-server, desktop, gnome-desktop
d-i pkgsel/include string openssh-server qemu-guest-agent spice-vdagent sudo
d-i pkgsel/upgrade select none
d-i pkgsel/language-packs multiselect en, zh
d-i pkgsel/update-policy select none

# ========== 禁止在安装的时候弹出popularity ==========
popularity-contest popularity-contest/participate boolean false
 
# ========== Boot loader installation ==========
# UEFI settings
d-i grub-efi/install_devices multiselect auto
d-i grub-installer/only_debian boolean true
# Legacy bootdev (disabled for UEFI compatibility)
# d-i grub-installer/bootdev string /dev/[sv]da
 
# 安装完成后弹出CDROM并重启
d-i cdrom-detect/eject boolean true
d-i finish-install/reboot_in_progress note

# ========== 安装后脚本 ==========
d-i preseed/late_command string \
    in-target gpasswd -a $VM_USER sudo; \
    in-target mkdir -p /home/$VM_USER/Downloads; \
    in-target mkdir -p /home/$VM_USER/Documents; \
    in-target mkdir -p /home/$VM_USER/Music; \
    in-target mkdir -p /home/$VM_USER/Pictures; \
    in-target mkdir -p /home/$VM_USER/Videos; \
    in-target chown -R $VM_USER:$VM_USER /home/$VM_USER; \
    in-target chmod -R 755 /home/$VM_USER; \
    in-target systemctl enable qemu-guest-agent; \
    in-target systemctl enable spice-vdagent; \
    in-target sed -i '/^deb cdrom:/s/^/#/' /etc/apt/sources.list;
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
if virsh list --all | grep -qw "$VM_NAME"; then
  echo "检测到虚拟机 $VM_NAME 已存在。"
  for i in 1 2 3; do
    read -p "确认要删除虚拟机 $VM_NAME 吗？(第 $i 次/共3次，输入 yes 确认): " confirm
    if [ "$confirm" != "yes" ]; then
      echo "未确认，跳过本次删除。"
      exit 1
    fi
  done
  echo "正在清理旧虚拟机..."
  virsh destroy "$VM_NAME"
  virsh undefine "$VM_NAME" --nvram

  rm -f "$VM_DISK_PATH" 2>/dev/null
else
  echo "虚拟机 $VM_NAME 不存在，无需清理。"
fi

# 打印虚拟机信息方法
print_vm_info() {
  echo "正在获取虚拟机IP地址..."
  VM_IP=""
  for i in {1..30}; do
    VM_IP=$(virsh domifaddr "$VM_NAME" --source agent 2>/dev/null | awk '$1 != "lo" && /ipv4/ {print $4}' | cut -d'/' -f1 | head -n1)
    if [ -n "$VM_IP" ]; then
      break
    fi
    sleep 1
  done

  echo "=============================================="
  echo "虚拟机创建完成！"
  echo "用户名: $VM_USER"
  echo "密码:   $VM_PASSWD"
  if [ -n "$VM_IP" ]; then
    echo "SSH登录方式:"
    echo "  ssh $VM_USER@$VM_IP"
  else
    echo "未能自动获取虚拟机IP，请稍后手动查询。"
    echo "常用命令: virsh domifaddr $VM_NAME --source agent"
  fi
  echo "=============================================="

  # 删除 Kickstart 配置文件
  rm -rf $KS_CONFIG
}

# 执行全自动安装
# 策略：直接在 virt-install 中配置图形设备，但使用 --noautoconsole 不打开图形窗口
# 并通过 --wait 0 让命令立即返回，随即连接串口控制台查看文本安装进度。
echo "正在启动安装..."
virt-install \
 --name "$VM_NAME" \
 --memory "$VM_RAM" \
 --vcpus "$VM_VCPUS" \
 --boot uefi \
 --disk path="$VM_DISK_PATH",size=$VM_DISK_SIZE,format=qcow2,bus=virtio \
 --disk path="$ISO_FILE",device=cdrom \
 --location "$ISO_FILE" \
 --os-variant debian11 \
 --network bridge=virbr0 \
 --graphics spice \
 --video virtio \
 --channel unix,target_type=virtio,name=org.qemu.guest_agent.0 \
 --channel spicevmc,target_type=virtio,name=com.redhat.spice.0 \
 --console pty,target_type=serial \
 --initrd-inject "$KS_CONFIG" \
 --noautoconsole \
 --wait 0 \
 --extra-args "\
 console=ttyS0,115200n8 \
 serial \
 auto=true \
 file=/$(basename "$KS_CONFIG") \
 -- quiet"

if [ $? -eq 0 ]; then
  echo "虚拟机已启动，正在连接串口控制台..."
  echo "-------------------------------------------------------"
  echo "提示：您将看到文本安装界面。安装完成后虚拟机将自动重启，"
  echo "      控制台连接会自动断开（这是正常现象）。"
  echo "-------------------------------------------------------"
  sleep 2
  
  # 连接控制台（这将阻塞直到虚拟机重启）
  virsh console "$VM_NAME"
  
  # 安装结束，检查虚拟机状态
  echo ""
  echo "安装阶段完成，检查虚拟机状态..."
  
  sleep 3
  VM_STATE=$(virsh domstate "$VM_NAME" 2>/dev/null)
  
  if [ "$VM_STATE" = "shut off" ]; then
    echo "虚拟机已关机，正在启动..."
    virsh start "$VM_NAME"
    sleep 2
  elif [ "$VM_STATE" = "running" ]; then
    echo "虚拟机已正常重启进入系统。"
  fi
  
  print_vm_info
else
  echo "启动安装失败。"
  exit 1
fi
