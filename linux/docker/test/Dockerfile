FROM ubuntu:20.04

COPY sources.list /etc/apt/

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR ~/

SHELL ["/bin/bash", "-lc"]

RUN  apt update && apt install -y \
    sudo \
    vim \
    git \
    g++ \
    gdb \
    cmake \
    wget