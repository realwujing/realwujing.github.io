#!/bin/bash

cd

# /etc/fstab
sudo cat << EOF >> /etc/fstab
# /dev/sda1
UUID=cc5da720-dbfb-4bc2-8f35-f566d1603508       /media/wujing/data      ext4            rw,relatime     0 2
/media/wujing/data/Downloads /home/wujing/Downloads none defaults,bind 0 0
/media/wujing/data/Documents /home/wujing/Documents none defaults,bind 0 0
/media/wujing/data/code /home/wujing/code none defaults,bind 0 0
EOF
```

sudo systemctl daemon-reload
sudo mount -a

# node nvm

# proxy
export https_proxy=http://127.0.0.1:7890;export http_proxy=http://127.0.0.1:7890;export all_proxy=socks5://127.0.0.1:7890

curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.1/install.sh | bash

source ~/.bashrc

nvm install v18.18.2

# install debs
sudo apt install git subversion openjdk-11-jdk virt-manager vim vim-gtk ctags cscope tmux -y

# virt-manager net
sudo virsh net-list --all
sudo virsh net-start default
sudo virsh net-autostart default

if [ ! -d code ]; then
    mkdir -p code
    cd code
fi

if [ ! -d code/realwujing.github.io ]; then
    git clone https://github.com/realwujing/realwujing.github.io.git
fi

# gdb
if [ ! -d code/gef ]; then
    git clone https://github.com/hugsy/gef
fi

cd

if [ ! -d .gdb ]; then
    mkdir -p .gdb
fi

cd .gdb

if [ ! -d .gdb/qt5printers ]; then
    git clone https://github.com/Lekensteyn/qt5printers.git
fi

ln -s /home/wujing/code/gef/gef.py .gdbinit-gef.py
ln -s /home/wujing/code/realwujing.github.io/debug/gdb/.gdbinit .gdbinit

# ssh
ssh-keygen
ln -s /home/wujing/code/realwujing.github.io/linux/ssh/config .ssh/config

# git
ln -s /home/wujing/code/realwujing.github.io/git/.gitcommit_template .gitcommit_template
ln -s /home/wujing/code/realwujing.github.io/git/.gitconfig .gitconfig
ln -s /home/wujing/code/realwujing.github.io/git/.gitignore_global .gitignore_global

# vim
git clone https://github.com/VundleVim/Vundle.vim.git ~/.vim/bundle/Vundle.vim
ln -s /home/wujing/code/realwujing.github.io/linux/vim/.vimrc .vimrc
vim +PluginInstall +qall

tar -zcvf ~/vim-git.tar.gz ~/code/realwujing.github.io/git/.gitcommit_template ~/code/realwujing.github.io/git/.gitconfig ~/code/realwujing.github.io/git/.gitignore_global ~/.vim ~/.vimrc