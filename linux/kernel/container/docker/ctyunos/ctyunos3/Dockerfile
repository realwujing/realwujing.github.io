# CTYunOS 3 Kernel Build Environment
FROM ctyunos:23.01

# Configure yum repositories
RUN cd /etc/yum.repos.d && mv ctyunos.repo ctyunos.repo.bak || true

COPY <<EOF /etc/yum.repos.d/ctyunos.repo
[everything]
name=ctyunos-everything
baseurl=https://repo.ctyun.cn/hostos/ctyunos-23.01/everything/\$basearch/
enabled=1
gpgcheck=0

[update]
name=ctyunos-update
baseurl=https://repo.ctyun.cn/hostos/ctyunos-23.01/update/\$basearch/
enabled=1
gpgcheck=0

[debuginfo]
name=ctyunos-debuginfo
baseurl=https://repo.ctyun.cn/hostos/ctyunos-23.01/debuginfo/\$basearch/
enabled=1
gpgcheck=0

[extras]
name=ctyunos-extras
baseurl=https://repo.ctyun.cn/hostos/ctyunos-23.01/extras/\$basearch/
gpgcheck=0
enabled=1

# access by aone
[aone-everything]
name=ctyunos-aone-everything
baseurl=http://121.237.176.8:50001/ctyun/ctyunos/ctyunos-23.01/everything/\$basearch/
enabled=1
gpgcheck=0

# access by aone
[aone-update]
name=ctyunos-aone-update
baseurl=http://121.237.176.8:50001/ctyun/ctyunos/ctyunos-23.01/update/\$basearch/
enabled=1
gpgcheck=0

# access by aone
[aone-debuginfo]
name=ctyunos-aone-debuginfo
baseurl=http://121.237.176.8:50001/ctyun/ctyunos/ctyunos-23.01/debuginfo/\$basearch/
enabled=1
gpgcheck=0

# access by aone
[aone-extras]
name=ctyunos-aone-extras
baseurl=http://121.237.176.8:50001/ctyun/ctyunos/ctyunos-23.01/extras/\$basearch/
enabled=1
gpgcheck=0
EOF

# Install basic tools
RUN yum makecache && \
    yum install -y git dnf-plugins-core bash-completion rpm-build rpmdevtools && \
    yum clean all

# Create directories
RUN mkdir -p /root/{code,rpmbuild,Downloads}

# Set working directory
WORKDIR /root/code

CMD ["/bin/bash"]
