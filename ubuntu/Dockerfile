FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-c"]

COPY sources.list /etc/apt/
COPY .condarc ~/
COPY condainit ~/

WORKDIR ~/

RUN apt update && apt install -y \
    sudo \
    vim \
    git \
    g++ \
    gdb \
    cmake \
    openjdk-11-jdk \
    mysql-server \
    redis \
    nginx \
    openssh-server \
    net-tools \
    iputils-ping \
    telnet \
    lsb-core \
    wget && \
    wget https://mirrors.bfsu.edu.cn/anaconda/archive/Anaconda3-2020.02-Linux-x86_64.sh && \
    bash Anaconda3-2020.02-Linux-x86_64.sh -b -p /usr/local/anaconda3/ && \
    rm Anaconda3-2020.02-Linux-x86_64.sh && \
    source /usr/local/anaconda3/bin/activate && \
    conda init && \
    conda clean -i && \
    pip config set global.index-url https://mirrors.bfsu.edu.cn/pypi/web/simple && \
    usermod -d /var/lib/mysql/ mysql && \
    useradd --create-home --no-log-init --shell /bin/bash wujing && \
    echo 'wujing ALL=(ALL) ALL' >> /etc/sudoers && \
    echo 'wujing:2988' | chpasswd && \
    echo 'root:2988' | chpasswd && \
    cat condainit >> /etc/profile && \
    echo "set completion-ignore-case on" >> ~/.inputrc && \
    echo -e >> /home/wujing/.bashrc && \
    echo 'source /etc/profile' >> /home/wujing/.bashrc && \
    echo "set completion-ignore-case on" >> /home/wujing/.inputrc && \
    source /etc/profile && \
    cp .condarc /home/wujing && \
    cp -r ~/.config /home/wujing/

USER wujing
WORKDIR /home/wujing