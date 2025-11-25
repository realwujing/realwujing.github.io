FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

COPY sources.list /etc/apt/

RUN apt update && apt install -y \
    sudo && \
    useradd --create-home --no-log-init --shell /bin/bash wujing && \
    echo 'wujing ALL=(ALL) ALL' >> /etc/sudoers && \
    echo 'wujing:2988' | chpasswd && \
    echo 'root:2988' | chpasswd && \
    mkdir /home/wujing/.pip

USER wujing
WORKDIR /home/wujing

COPY .condarc ~/
COPY pip.conf ~/.pip

CMD ["/bin/bash"]