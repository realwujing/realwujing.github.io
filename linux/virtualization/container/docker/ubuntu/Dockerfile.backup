FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

COPY sources.list /etc/apt/

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
    wget && \
    useradd --create-home --no-log-init --shell /bin/bash wujing && \
    adduser wujing sudoRUN echo "wujing:2988" | chpasswd && \
    mkdir /home/wujing/.pip

USER wujing
WORKDIR /home/wujing

COPY .condarc ~/
COPY pip.conf ~/.pip

CMD ["/bin/bash"]