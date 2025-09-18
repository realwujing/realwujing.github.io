# CTYunOS 3 Docker Environment Makefile
# Based on ctyunos3-docker-230117-x86_64.tar.xz

# Docker image configuration
IMAGE_NAME := ctyunos
IMAGE_TAG := 23.01
IMAGE_FULL := $(IMAGE_NAME):$(IMAGE_TAG)
CONTAINER_NAME := ctyunos-23.01

# Source files
DOCKER_IMAGE_SOURCE := ctyunos3-docker-230117-x86_64.tar.xz
DOCKER_IMAGE_URL := https://repo.ctyun.cn/hostos/ctyunos-23.01/docker_img/x86_64/$(DOCKER_IMAGE_SOURCE)
DOCKER_IMAGE_DIR := /home/wujing/Downloads/docker
DOCKER_IMAGE_PATH := $(DOCKER_IMAGE_DIR)/$(DOCKER_IMAGE_SOURCE)

# Cross-compile toolchain
CROSS_COMPILE_URL := https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz
CROSS_COMPILE_TAR := gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz

# Branch configuration (can be overridden via command line)
KERNEL_5_10_BRANCH ?= ctkernel-lts-5.10/develop
KERNEL_6_6_BRANCH ?= ctkernel-lts-6.6/develop

HOST_RPMBUILD_DIR := /home/wujing/code/rpmbuild
HOST_DOWNLOADS_DIR := /home/wujing/Downloads
HOST_CODE_DIR := /home/wujing/code

# Docker run options
DOCKER_RUN_OPTS := -it \
	--name $(CONTAINER_NAME) \
	-v $(HOST_CODE_DIR):/root/code \
	-v $(HOST_RPMBUILD_DIR):/root/rpmbuild \
	-v $(HOST_DOWNLOADS_DIR):/root/Downloads \
	-w /root/code

.PHONY: help download-image load-image build-image run-container stop-container clean rpmbuild-5.10-amd64 rpmbuild-6.6-amd64 download-cross-compile setup-cross-compile rpmbuild-5.10-arm64 rpmbuild-6.6-arm64 build-5.10-amd64 build-6.6-amd64 build-5.10-arm64 build-6.6-arm64 setup-container

help:	## Show this help message
	@echo "CTYunOS 3 Docker Environment Makefile"
	@echo ""
	@echo "Core commands:"
	@echo "  help                      Show this help message"
	@echo "  setup                     Setup development environment (download, build, run)"
	@echo "  shell                     Open shell in running container"
	@echo "  clean                     Clean up containers and images"
	@echo "  status                    Show container and image status"
	@echo ""
	@echo "Build commands:"
	@echo "  Default branches: 5.10=$(KERNEL_5_10_BRANCH), 6.6=$(KERNEL_6_6_BRANCH)"
	@echo ""
	@echo "  Use custom branch (replace 'my-branch' with your branch name):"
	@echo "    make build-5.10-amd64 KERNEL_5_10_BRANCH=my-branch"
	@echo "    make rpmbuild-6.6-arm64 KERNEL_6_6_BRANCH=my-branch"
	@echo ""
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		sed 's/:.*##/##/' | \
		awk -F'##' '{printf "  %-25s %s\n", $$1, $$2}' | \
		grep '^  build-[0-9]' | sort
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		sed 's/:.*##/##/' | \
		awk -F'##' '{printf "  %-25s %s\n", $$1, $$2}' | \
		grep '^  rpmbuild-' | sort
	@echo ""
	@echo "Advanced commands (for debugging):"
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		sed 's/:.*##/##/' | \
		awk -F'##' '{printf "  %-25s %s\n", $$1, $$2}' | \
		grep -v '^  build-[0-9]' | grep -v '^  rpmbuild-' | grep -v '^  help' | \
		grep -v '^  setup' | grep -v '^  shell' | grep -v '^  clean' | grep -v '^  status' | sort

download-image:	## Download CTYunOS 3 docker image
	@echo "Downloading CTYunOS 3 docker image to $(DOCKER_IMAGE_DIR)..."
	@mkdir -p $(DOCKER_IMAGE_DIR)
	@if [ ! -f $(DOCKER_IMAGE_PATH) ]; then \
		cd $(DOCKER_IMAGE_DIR) && wget $(DOCKER_IMAGE_URL); \
	else \
		echo "$(DOCKER_IMAGE_PATH) already exists"; \
	fi

load-image: download-image	## Load CTYunOS 3 docker image from tar.xz
	@echo "Extracting and loading CTYunOS 3 docker image..."
	@TAR_FILE=$(DOCKER_IMAGE_DIR)/ctyunos3-docker-230117-x86_64.tar; \
	if [ ! -f $$TAR_FILE ]; then \
		cd $(DOCKER_IMAGE_DIR) && xz -d -k $(DOCKER_IMAGE_SOURCE); \
	fi; \
	docker load < $$TAR_FILE
	@echo "Tagging image as $(IMAGE_FULL)..."
	@docker tag $$(docker images --format "{{.Repository}}:{{.Tag}}" | grep ctyunos | head -1) $(IMAGE_FULL)

build-image:	## Build custom CTYunOS 3 image from Dockerfile
	@echo "Building CTYunOS 3 image from Dockerfile..."
	@if ! docker images --format "table {{.Repository}}:{{.Tag}}" | grep -q "^ctyunos:23.01$$"; then \
		echo "Base image ctyunos:23.01 not found, loading..."; \
		make load-image; \
	fi
	@docker build -t $(IMAGE_FULL) -f Dockerfile .

run-container: build-image	## Run CTYunOS 3 container
	@echo "Starting CTYunOS 3 container..."
	@if [ $$(docker ps -a -q -f name=$(CONTAINER_NAME)) ]; then \
		echo "Container $(CONTAINER_NAME) already exists. Use 'make restart-container' to restart it."; \
	else \
		docker run -d $(DOCKER_RUN_OPTS) $(IMAGE_FULL) /bin/bash -c "while true; do sleep 30; done"; \
	fi

setup-container: build-image	## Ensure container is running
	@echo "Ensuring container is running..."
	@if [ ! $$(docker ps -q -f name=$(CONTAINER_NAME)) ]; then \
		if [ $$(docker ps -a -q -f name=$(CONTAINER_NAME)) ]; then \
			echo "Starting existing container..."; \
			docker start $(CONTAINER_NAME); \
		else \
			echo "Creating new container..."; \
			docker run -d $(DOCKER_RUN_OPTS) $(IMAGE_FULL) /bin/bash -c "while true; do sleep 30; done"; \
		fi \
	else \
		echo "Container $(CONTAINER_NAME) is already running"; \
	fi
	@echo "Setting up git configuration..."
	@docker exec $(CONTAINER_NAME) bash -c "git config --global --add safe.directory '*' 2>/dev/null || true"

restart-container:	## Restart existing container
	@echo "Restarting container $(CONTAINER_NAME)..."
	@docker start -ai $(CONTAINER_NAME) || make run-container

stop-container:	## Stop running container
	@echo "Stopping container $(CONTAINER_NAME)..."
	@docker stop $(CONTAINER_NAME) 2>/dev/null || true

remove-container:	## Remove container
	@echo "Removing container $(CONTAINER_NAME)..."
	@docker rm $(CONTAINER_NAME) 2>/dev/null || true

download-cross-compile:	## Download ARM64 cross-compile toolchain
	@echo "Downloading ARM64 cross-compile toolchain..."
	@mkdir -p $(HOST_DOWNLOADS_DIR)/cross_compile
	@if [ ! -f $(HOST_DOWNLOADS_DIR)/cross_compile/$(CROSS_COMPILE_TAR) ]; then \
		cd $(HOST_DOWNLOADS_DIR)/cross_compile && wget $(CROSS_COMPILE_URL); \
	else \
		echo "$(CROSS_COMPILE_TAR) already exists"; \
	fi

setup-cross-compile: download-cross-compile	## Extract cross-compile toolchain
	@echo "Setting up cross-compile toolchain..."
	@cd $(HOST_DOWNLOADS_DIR)/cross_compile && tar -xJf $(CROSS_COMPILE_TAR)
	@echo "Cross-compile toolchain extracted to $(HOST_DOWNLOADS_DIR)/cross_compile/"
	@echo "To use in container: export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-"
	@echo "                     export ARCH=arm64"

rpmbuild-5.10-amd64: setup-container	## Build kernel RPM package (ctkernel-lts-5.10, AMD64)
	@echo "======= Building RPM Package (5.10/AMD64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_5_10_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_5_10_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_5_10_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		yum-builddep build/kernel.spec -y && \
		for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do \
			rm -rf /root/rpmbuild/\$$dir/*; \
		done && \
		mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} && \
		rm -rf /root/rpmbuild/SOURCES/kernel.tar.gz && \
		tar --xform='s/^./kernel/' --exclude='.git' -chzf /root/rpmbuild/SOURCES/kernel.tar.gz . && \
		cp build/* /root/rpmbuild/SOURCES && \
		echo '======= Starting RPM Build Process =======' && \
		time rpmbuild -ba build/kernel.spec && \
		echo '======= RPM Build Completed =======' && \
		echo 'RPM packages location: /root/rpmbuild/RPMS/'" && \
	echo "RPM build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

rpmbuild-6.6-amd64: setup-container	## Build kernel RPM package (ctkernel-lts-6.6, AMD64)
	@echo "======= Building RPM Package (6.6/AMD64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_6_6_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_6_6_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_6_6_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		yum-builddep build/spec/kernel.spec -y && \
		for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do \
			rm -rf /root/rpmbuild/\$$dir/*; \
		done && \
		mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} && \
		rm -rf /root/rpmbuild/SOURCES/kernel.tar.gz && \
		tar --xform='s/^./kernel/' --exclude='.git' -chzf /root/rpmbuild/SOURCES/kernel.tar.gz . && \
		cp build/* /root/rpmbuild/SOURCES && \
		echo '======= Starting RPM Build Process =======' && \
		time rpmbuild -ba build/spec/kernel.spec && \
		echo '======= RPM Build Completed =======' && \
		echo 'RPM packages location: /root/rpmbuild/RPMS/'" && \
	echo "RPM build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

rpmbuild-5.10-arm64: setup-container setup-cross-compile	## Build kernel RPM package (ctkernel-lts-5.10, ARM64)
	@echo "======= Building RPM Package (5.10/ARM64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_5_10_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_5_10_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_5_10_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- && \
		export ARCH=arm64 && \
		make openeuler_defconfig && \
		echo '======= Starting RPM Build Process (ARM64) =======' && \
		make binrpm-pkg -j\$$(nproc) && \
		echo '======= RPM Build Completed =======' && \
		echo 'RPM packages location: ~/rpmbuild/RPMS/'" && \
	echo "RPM build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

rpmbuild-6.6-arm64: setup-container setup-cross-compile	## Build kernel RPM package (ctkernel-lts-6.6, ARM64)
	@echo "======= Building RPM Package (6.6/ARM64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_6_6_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_6_6_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_6_6_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- && \
		export ARCH=arm64 && \
		make openeuler_defconfig && \
		echo '======= Starting RPM Build Process (ARM64) =======' && \
		make binrpm-pkg -j\$$(nproc) && \
		echo '======= RPM Build Completed =======' && \
		echo 'RPM packages location: ~/rpmbuild/RPMS/'" && \
	echo "RPM build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

# Compile targets (without RPM packaging)
build-5.10-amd64: setup-container	## Compile kernel only (ctkernel-lts-5.10, AMD64)
	@echo "======= Starting Kernel Build (5.10/AMD64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_5_10_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_5_10_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_5_10_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		echo '======= Installing build dependencies via yum-builddep =======' && \
		yum-builddep build/kernel.spec -y && \
		echo '======= Configuring kernel =======' && \
		time make openeuler_defconfig && \
		echo '======= Starting compilation =======' && \
		time make -j\$$(nproc)" && \
	END_TIME=$$(date +%s) && \
	DURATION=$$((END_TIME - START_TIME)) && \
	echo "======= Build Summary =======" && \
	echo "Build completed at: $$(date '+%Y-%m-%d %H:%M:%S')" && \
	echo "Total build time: $$((DURATION / 60)) minutes $$((DURATION % 60)) seconds" && \
	echo "Build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

build-6.6-amd64: setup-container	## Compile kernel only (ctkernel-lts-6.6, AMD64)
	@echo "======= Starting Kernel Build (6.6/AMD64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_6_6_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_6_6_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR (mapped from \$$HOST_WORK_DIR)\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_6_6_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		echo \"Working in directory: \$$PWD\" && \
		echo '======= Installing build dependencies via yum-builddep =======' && \
		yum-builddep build/spec/kernel.spec -y && \
		echo '======= Configuring kernel =======' && \
		time make openeuler_defconfig && \
		echo '======= Starting compilation =======' && \
		time make -j\$$(nproc)" && \
	END_TIME=$$(date +%s) && \
	DURATION=$$((END_TIME - START_TIME)) && \
	echo "======= Build Summary =======" && \
	echo "Build completed at: $$(date '+%Y-%m-%d %H:%M:%S')" && \
	echo "Total build time: $$((DURATION / 60)) minutes $$((DURATION % 60)) seconds" && \
	echo "Build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

build-5.10-arm64: setup-container setup-cross-compile	## Compile kernel only (ctkernel-lts-5.10, ARM64)
	@echo "======= Starting Kernel Build (5.10/ARM64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_5_10_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_5_10_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_5_10_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		echo '======= Installing build dependencies via yum-builddep =======' && \
		yum-builddep build/kernel.spec -y && \
		export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- && \
		export ARCH=arm64 && \
		echo '======= Configuring kernel (ARM64) =======' && \
		time make openeuler_defconfig && \
		echo '======= Starting compilation (ARM64) =======' && \
		time make -j\$$(nproc)" && \
	END_TIME=$$(date +%s) && \
	DURATION=$$((END_TIME - START_TIME)) && \
	echo "======= Build Summary =======" && \
	echo "Build completed at: $$(date '+%Y-%m-%d %H:%M:%S')" && \
	echo "Total build time: $$((DURATION / 60)) minutes $$((DURATION % 60)) seconds" && \
	echo "Build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

build-6.6-arm64: setup-container setup-cross-compile	## Compile kernel only (ctkernel-lts-6.6, ARM64)
	@echo "======= Starting Kernel Build (6.6/ARM64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c "\
		echo '======= Finding appropriate source directory =======' && \
		WORK_DIR='' && \
		if git -C /root/code/linux worktree list | grep -q '$(KERNEL_6_6_BRANCH)'; then \
			HOST_WORK_DIR=\$$(git -C /root/code/linux worktree list | grep '$(KERNEL_6_6_BRANCH)' | awk '{print \$$1}' | head -1); \
			WORK_DIR=\$$(echo \$$HOST_WORK_DIR | sed 's|/home/wujing/code|/root/code|'); \
			echo \"Using existing worktree: \$$WORK_DIR\"; \
		else \
			WORK_DIR='/root/code/linux'; \
			echo \"Using main repository and checking out branch\"; \
			cd \$$WORK_DIR && git checkout $(KERNEL_6_6_BRANCH); \
		fi && \
		cd \$$WORK_DIR && \
		echo '======= Installing build dependencies via yum-builddep =======' && \
		yum-builddep build/spec/kernel.spec -y && \
		export CROSS_COMPILE=/root/Downloads/cross_compile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- && \
		export ARCH=arm64 && \
		echo '======= Configuring kernel (ARM64) =======' && \
		time make openeuler_defconfig && \
		echo '======= Starting compilation (ARM64) =======' && \
		time make -j\$$(nproc)" && \
	END_TIME=$$(date +%s) && \
	DURATION=$$((END_TIME - START_TIME)) && \
	echo "======= Build Summary =======" && \
	echo "Build completed at: $$(date '+%Y-%m-%d %H:%M:%S')" && \
	echo "Total build time: $$((DURATION / 60)) minutes $$((DURATION % 60)) seconds" && \
	echo "Build completed successfully, stopping container..." && \
	docker stop $(CONTAINER_NAME) || true

shell:	## Open shell in running container
	@echo "Opening shell in container $(CONTAINER_NAME)..."
	@if [ ! $$(docker ps -q -f name=$(CONTAINER_NAME)) ]; then \
		echo "Container not running, setting up..."; \
		make setup-container; \
	fi
	@docker exec -it $(CONTAINER_NAME) /bin/bash

logs:	## Show container logs
	@if [ $$(docker ps -a -q -f name=$(CONTAINER_NAME)) ]; then \
		docker logs $(CONTAINER_NAME); \
	else \
		echo "Container $(CONTAINER_NAME) does not exist. Run 'make setup-container' first."; \
	fi

clean-images:	## Remove CTYunOS docker images
	@echo "Removing CTYunOS docker images..."
	@docker rmi $(IMAGE_FULL) 2>/dev/null || true

clean-dangling:	## Remove dangling (none) images
	@echo "Removing dangling images..."
	@docker image prune -f

clean: stop-container remove-container	## Clean up containers and images
	@echo "Cleaning up..."
	@make clean-images
	@make clean-dangling

status:	## Show container and image status
	@echo "=== Docker Images ==="
	@docker images | grep -E "(ctyunos|REPOSITORY)" || echo "No CTYunOS images found"
	@echo ""
	@echo "=== Container Status ==="
	@docker ps -a | grep -E "($(CONTAINER_NAME)|CONTAINER)" || echo "No containers found"

# Development shortcuts
setup: build-image run-container	## Setup development environment (download, build, run)
	@echo "Development environment ready! Use 'make shell' to enter container."

dev-setup: build-image run-container	## Quick setup for development
	@echo "Development environment ready!"

dev-clean: clean	## Clean development environment
	@echo "Development environment cleaned!"
