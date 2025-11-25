#! /bin/bash

dd if=/dev/zero of=buster.img bs=1M seek=2047 count=1
sudo mkfs.ext4 -F buster.img
sudo mkdir -p /mnt/buster
sudo mount -o loop buster.img /mnt/buster
sudo cp -a linux-rootfs/. /mnt/buster/.
sudo umount /mnt/buster
sudo chmod 666 buster.img
