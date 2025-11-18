# CTYunOS 3 Kernel Build Environment
# This Dockerfile creates a development image with user configuration
FROM ctyunos:23.01-build

# Add argument for user/group IDs (will be passed from Makefile)
ARG USER_ID=1000
ARG GROUP_ID=1000
ARG USER_NAME=builder

# Create user with same UID/GID as host user
RUN groupadd -g ${GROUP_ID} ${USER_NAME} && \
    useradd -m -u ${USER_ID} -g ${GROUP_ID} -s /bin/bash ${USER_NAME} && \
    echo "${USER_NAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Create working directories
RUN mkdir -p /home/${USER_NAME}/{code,rpmbuild,Downloads}

# Switch to non-root user
USER ${USER_NAME}

# Set working directory
WORKDIR /home/${USER_NAME}/code

CMD ["/bin/bash"]
