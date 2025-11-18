#!/bin/bash

set -x

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <虚机名> <ISO绝对路径位置>"
    exit 1
fi

vm_name="$1"
iso_path="$2"

#mkdir -p /var/"$vm_name"
#cd /var/"$vm_name"

#sshpass -p 'Ctyun@2301#' scp root@192.168.122.83:/result/$iso_path .
qemu-img create -f qcow2 "$vm_name.qcow2" 40G

hd_label=$(file "$iso_path" | awk -F"'" '{print $2}')

# 判断架构
if [ "$(arch)" == "x86_64" ]; then
    # x86_64 架构
    virt-install --virt-type kvm --name "$vm_name" --memory 16384 --vcpus=16 --location "$iso_path" \
    --disk path="$vm_name.qcow2",size=40,format=qcow2 --network network=default \
    --os-type=linux --os-variant=rhel7 --graphics none \
    --extra-args="BOOT_IMAGE=vmlinuz initrd=initrd.img inst.stage2=hd:LABEL=$hd_label console=ttyS0"
elif [ "$(arch)" == "aarch64" ]; then
    # aarch64 架构
    virt-install --virt-type kvm --name "$vm_name" --memory 16384 --vcpus=16 --location "$iso_path" \
    --disk path="$vm_name.qcow2",size=60,format=qcow2 --network network=default \
    --os-type=linux --os-variant=rhel7 --graphics none \
    --extra-args="inst.stage2=hd:LABEL=$hd_label console=ttyS0"
else
    echo "Unsupported architecture: $(arch)"
    exit 1
fi
