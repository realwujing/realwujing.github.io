FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-c"]

WORKDIR /root

COPY sources.list /etc/apt/
COPY .condarc /root
COPY condainit /root
COPY mysql_secure.sh /root/
COPY mysql.sql /root/
COPY ssh_keygen.sh /root/
COPY start.sh /usr/local/

RUN apt update && apt install -y \
    sudo \
    apt-utils \
    vim \
    git \
    subversion \
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
    iptables \
    expect \
    wget && \
    wget https://mirrors.bfsu.edu.cn/anaconda/archive/Anaconda3-2020.02-Linux-x86_64.sh && \
    bash Anaconda3-2020.02-Linux-x86_64.sh -b -p /usr/local/anaconda3/ && \
    rm Anaconda3-2020.02-Linux-x86_64.sh && \
    source /usr/local/anaconda3/bin/activate && \
    conda init && \
    conda clean -i && \
    pip config set global.index-url https://mirrors.aliyun.com/pypi/simple/ && \
    source .bashrc && \
    conda create -y -n ai python=3.6 && \
    conda activate ai && \
    conda install -y tensorflow pytorch pandas matplotlib && \
    conda deactivate && \
    chmod 777 -R /usr/local/anaconda3/ && \
    usermod -d /var/lib/mysql/ mysql && \
    service mysql start && \
    chmod +x /root/mysql_secure.sh && \
    expect /root/mysql_secure.sh && \
    sed -i "s/bind-address.*= 127.0.0.1/bind-address            = 0.0.0.0/g" /etc/mysql/mysql.conf.d/mysqld.cnf && \
    mysql -P 3306 -uroot -p2988 < /root/mysql.sql && \
    service mysql restart && \
    service ssh start && \
    sed -i "s/#   PasswordAuthentication yes/    PasswordAuthentication yes/g" /etc/ssh/ssh_config && \
    chmod +x /root/ssh_keygen.sh && \
    expect /root/ssh_keygen.sh && \
    cd /root/.ssh && \
    cat id_rsa.pub >> authorized_keys && \
    chmod 600 authorized_keys && \
    chmod 700 /root/.ssh && \
    sed -i "s/#PubkeyAuthentication yes/PubkeyAuthentication yes/g" /etc/ssh/sshd_config && \
    service ssh restart && \
    chmod 777 /usr/local/start.sh && \
    touch /usr/local/start.log && \
    chmod 777 /usr/local/start.log && \
    useradd --create-home --no-log-init --shell /bin/bash wujing && \
    echo 'wujing ALL=(ALL) ALL' >> /etc/sudoers && \
    echo 'wujing:2988' | chpasswd && \
    echo 'root:2988' | chpasswd && \
    cd /root && \
    cat condainit >> /etc/profile && \
    echo "set completion-ignore-case on" >> ~/.inputrc && \
    echo -e >> /home/wujing/.bashrc && \
    echo 'source /etc/profile' >> /home/wujing/.bashrc && \
    echo "set completion-ignore-case on" >> /home/wujing/.inputrc && \
    source /etc/profile && \
    cp .condarc /home/wujing && \
    cp -r .config /home/wujing/ && \
    chmod 777 -R /home/wujing/.config/ && \
    cp /root/ssh_keygen.sh /home/wujing/ 

USER wujing
WORKDIR /home/wujing

RUN expect /home/wujing/ssh_keygen.sh && \
    cd /home/wujing/.ssh && \
    cat id_rsa.pub >> authorized_keys && \
    chmod 600 authorized_keys && \
    chmod 700 /home/wujing/.ssh && \
    cd /home/wujing/

ENTRYPOINT ["/bin/bash", "/usr/local/start.sh"]
