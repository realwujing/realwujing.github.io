FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-c"]

COPY sources.list /etc/apt/
COPY .condarc ~/
COPY Anaconda3-2019.10-Linux-x86_64.sh ~/

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
    wget && \
    bash Anaconda3-2019.10-Linux-x86_64.sh -b && \
    rm Anaconda3-2019.10-Linux-x86_64.sh && \
    source /root/anaconda3/bin/activate && \
    conda init && \
    conda clean -i && \
    useradd --create-home --no-log-init --shell /bin/bash wujing && \
    adduser wujing sudoRUN echo 'wujing:2988' | chpasswd 

USER wujing
WORKDIR /home/wujing
