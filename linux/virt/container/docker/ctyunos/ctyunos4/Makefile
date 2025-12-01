# CTYunOS 4 Docker Environment Makefile
# Based on ctyunos:4-25.07-250905

# Get host user UID and GID first (needed for other paths)
HOST_UID := $(shell id -u)
HOST_GID := $(shell id -g)
HOST_USER := $(shell whoami)
HOST_HOME := $(shell echo $$HOME)

# Host directories
HOST_CODE_DIR := $(HOST_HOME)/code
HOST_RPMBUILD_DIR := $(HOST_CODE_DIR)/rpmbuild
HOST_DOWNLOADS_DIR := $(HOST_HOME)/Downloads

# Docker image configuration
IMAGE_NAME := ctyunos
BASE_TAG := 4-25.07-250905
BUILD_TAG := $(BASE_TAG)-build
DEV_TAG := $(BASE_TAG)-dev

BASE_IMAGE := $(IMAGE_NAME):$(BASE_TAG)
BUILD_IMAGE := $(IMAGE_NAME):$(BUILD_TAG)
DEV_IMAGE := $(IMAGE_NAME):$(DEV_TAG)
CONTAINER_NAME := $(IMAGE_NAME)-$(DEV_TAG)

# Source files
DOCKER_IMAGE_SOURCE := ctyunos4-docker-4-25.07-250905-x86_64.tar.xz
# DOCKER_IMAGE_URL := https://repo.ctyun.cn/hostos/ctyunos-4/docker_img/x86_64/$(DOCKER_IMAGE_SOURCE)
DOCKER_IMAGE_DIR := $(HOST_DOWNLOADS_DIR)/docker
DOCKER_IMAGE_PATH := $(DOCKER_IMAGE_DIR)/$(DOCKER_IMAGE_SOURCE)

# Cross-compile toolchain
CROSS_COMPILE_URL := https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz
CROSS_COMPILE_TAR := gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz
CROSS_COMPILE_DIR := gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
CROSS_COMPILE_PATH := /home/$(HOST_USER)/Downloads/cross_compile/$(CROSS_COMPILE_DIR)/bin/aarch64-none-linux-gnu-

# Branch configuration (can be overridden via command line)
KERNEL_5_10_BRANCH ?= ctkernel-lts-5.10/develop
KERNEL_6_6_BRANCH ?= ctkernel-lts-6.6/develop

# Docker run options with user mapping
DOCKER_RUN_OPTS := -it \
	--name $(CONTAINER_NAME) \
	--user $(HOST_UID):$(HOST_GID) \
	-v $(HOST_CODE_DIR):/home/$(HOST_USER)/code \
	-v $(HOST_RPMBUILD_DIR):/home/$(HOST_USER)/rpmbuild \
	-v $(HOST_DOWNLOADS_DIR):/home/$(HOST_USER)/Downloads \
	-w /home/$(HOST_USER)/code

# Docker build arguments
DOCKER_BUILD_ARGS := --build-arg USER_ID=$(HOST_UID) \
	--build-arg GROUP_ID=$(HOST_GID) \
	--build-arg USER_NAME=$(HOST_USER)

define SETUP_ARM64_ENV
export CROSS_COMPILE=$(CROSS_COMPILE_PATH) && \
export ARCH=arm64
endef

define PRINT_BUILD_TIME
END_TIME=$$(date +%s) && \
DURATION=$$((END_TIME - START_TIME)) && \
echo "======= Build Summary =======" && \
echo "Build completed at: $$(date '+%Y-%m-%d %H:%M:%S')" && \
echo "Total build time: $$((DURATION / 60)) minutes $$((DURATION % 60)) seconds"
endef

help:	## Show this help message
	@echo "CTYunOS 4 Docker Environment Makefile"
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
	@echo ""
	@echo "  Image architecture (build order):"
	@echo "    $(BASE_IMAGE)           Base OS image (from tar.xz)"
	@echo "    $(BUILD_IMAGE)     + Build tools (installed as root)"
	@echo "    $(DEV_IMAGE)       + User mapping (final dev image)"
	@echo ""
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		sed 's/:.*##/##/' | \
		awk -F'##' '{printf "  %-25s %s\n", $$1, $$2}' | \
		grep -v '^  build-[0-9]' | grep -v '^  rpmbuild-' | grep -v '^  help' | \
		grep -v '^  setup-container' | grep -v '^  shell' | \
		grep -v '^  restart-container' | grep -v '^  stop-container' | grep -v '^  clean' | grep -v '^  status'

download-image:	# Internal: Download CTYunOS 4 docker image (use 'load-image' instead)
	@echo "Downloading CTYunOS 4 docker image to $(DOCKER_IMAGE_DIR)..."
	@mkdir -p $(DOCKER_IMAGE_DIR)
	@if [ ! -f $(DOCKER_IMAGE_PATH) ]; then \
		echo "Please manually download $(DOCKER_IMAGE_SOURCE) to $(DOCKER_IMAGE_DIR)"; \
		# cd $(DOCKER_IMAGE_DIR) && wget $(DOCKER_IMAGE_URL); \
	else \
		echo "$(DOCKER_IMAGE_PATH) already exists"; \
	fi

load-image: download-image	## Load base OS image (→ ctyunos:4-25.07-250905)
	@echo "Extracting and loading CTYunOS 4 docker image..."
	@TAR_FILE=$(DOCKER_IMAGE_DIR)/$(DOCKER_IMAGE_SOURCE:.xz=); \
	if [ ! -f $$TAR_FILE ]; then \
		cd $(DOCKER_IMAGE_DIR) && xz -d -k $(DOCKER_IMAGE_SOURCE); \
	fi; \
	docker load < $$TAR_FILE
	@echo "Tagging image as $(DEV_IMAGE)..."
	@docker tag $$(docker images --format "{{.Repository}}:{{.Tag}}" | grep ctyunos | head -1) $(DEV_IMAGE)

build-base-image:	## Build image with build tools (→ ctyunos:4-25.07-250905-build)
	@echo "Building CTYunOS 4 base image with build tools..."
	@if ! docker images --format "table {{.Repository}}:{{.Tag}}" | grep -q "^$(BASE_IMAGE)$$"; then \
		echo "Base image $(BASE_IMAGE) not found, loading..."; \
		make load-image; \
	fi
	@echo "Installing build tools and configuring permissions..."
	@echo "FROM $(BASE_IMAGE)" > Dockerfile.build
	@echo "RUN yum install -y sudo shadow-utils git vim dnf-plugins-core && \\" >> Dockerfile.build
	@echo "    groupadd -g $(HOST_GID) $(HOST_USER) 2>/dev/null || true && \\" >> Dockerfile.build
	@echo "    useradd -m -u $(HOST_UID) -g $(HOST_GID) -s /bin/bash $(HOST_USER) 2>/dev/null || true && \\" >> Dockerfile.build
	@echo "    echo '$(HOST_USER) ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \\" >> Dockerfile.build
	@echo "    echo 'Defaults !requiretty' >> /etc/sudoers && \\" >> Dockerfile.build
	@echo "    mkdir -p /home/$(HOST_USER)/{code,rpmbuild,Downloads}" >> Dockerfile.build
	@docker build -t $(BUILD_IMAGE) -f Dockerfile.build .
	@rm -f Dockerfile.build
build-image:	## Build development image with user mapping (→ ctyunos:4-25.07-250905-dev)
	@echo "Preparing CTYunOS 4 development image..."
	@if ! docker images --format "table {{.Repository}}:{{.Tag}}" | grep -q "^$(BUILD_IMAGE)$$"; then \
		echo "Base build image $(BUILD_IMAGE) not found, building..."; \
		make build-base-image; \
	fi
	@echo "Tagging $(BUILD_IMAGE) as $(DEV_IMAGE)..."
	@docker tag $(BUILD_IMAGE) $(DEV_IMAGE)
	@echo "Development image ready. User mapping will be applied at runtime via --user flag."

setup-container:	## Ensure container is running
	@echo "Ensuring container is running..."
	@if ! docker images --format "{{.Repository}}:{{.Tag}}" | grep -q "^$(DEV_IMAGE)$$"; then \
		echo "Image $(DEV_IMAGE) not found, building..."; \
		$(MAKE) build-image; \
	fi
	@if [ ! $$(docker ps -q -f name=$(CONTAINER_NAME)) ]; then \
		if [ $$(docker ps -a -q -f name=$(CONTAINER_NAME)) ]; then \
			echo "Starting existing container..."; \
			docker start $(CONTAINER_NAME); \
		else \
			echo "Creating new container..."; \
			docker run -d $(DOCKER_RUN_OPTS) $(DEV_IMAGE) /bin/bash -c "while true; do sleep 30; done"; \
		fi \
	else \
		echo "Container $(CONTAINER_NAME) is already running"; \
	fi
	@echo "Setting up git configuration..."
	@docker exec $(CONTAINER_NAME) bash -c "git config --global --add safe.directory '*' 2>/dev/null || true"

restart-container:	## Restart existing container
	@echo "Restarting container $(CONTAINER_NAME)..."
	@docker restart $(CONTAINER_NAME) 2>/dev/null || make setup-container

stop-container:	## Stop running container
	@echo "Stopping container $(CONTAINER_NAME)..."
	@docker stop $(CONTAINER_NAME) 2>/dev/null

download-cross-compile:	# Internal: Download ARM64 cross-compile toolchain (use 'setup-cross-compile' instead)
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
	@echo "To use in container: export CROSS_COMPILE=$(CROSS_COMPILE_PATH)"
	@echo "                     export ARCH=arm64"

rpmbuild-5.10-amd64: setup-container	## Build kernel RPM package (ctkernel-lts-5.10, AMD64)
	@echo "======= Building RPM Package (5.10/AMD64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_5_10_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Cleaning rpmbuild directories =======" && \
		for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do \
			rm -rf /home/$(HOST_USER)/rpmbuild/$$dir/*; \
		done && \
		echo "======= Creating rpmbuild directories =======" && \
		mkdir -p /home/$(HOST_USER)/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} && \
		echo "======= Compressing source code =======" && \
		rm -rf /home/$(HOST_USER)/rpmbuild/SOURCES/kernel.tar.gz && \
		tar --xform="s/^./kernel/" --exclude=".git" -chzf /home/$(HOST_USER)/rpmbuild/SOURCES/kernel.tar.gz . && \
		echo "======= Copying build files =======" && \
		cp build/* /home/$(HOST_USER)/rpmbuild/SOURCES && \
		echo "======= Starting RPM Build Process =======" && \
		time rpmbuild -ba "$$SPEC" && \
		echo "======= RPM Build Completed =======" && \
		echo "RPM packages location: /home/$(HOST_USER)/rpmbuild/RPMS/"' && \
	docker stop $(CONTAINER_NAME)

rpmbuild-6.6-amd64: setup-container	## Build kernel RPM package (ctkernel-lts-6.6, AMD64)
	@echo "======= Building RPM Package (6.6/AMD64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_6_6_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Cleaning rpmbuild directories =======" && \
		for dir in BUILD BUILDROOT RPMS SOURCES SPECS SRPMS; do \
			rm -rf /home/$(HOST_USER)/rpmbuild/$$dir/*; \
		done && \
		echo "======= Creating rpmbuild directories =======" && \
		mkdir -p /home/$(HOST_USER)/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} && \
		echo "======= Compressing source code =======" && \
		rm -rf /home/$(HOST_USER)/rpmbuild/SOURCES/kernel.tar.gz && \
		tar --xform="s/^./kernel/" --exclude=".git" -chzf /home/$(HOST_USER)/rpmbuild/SOURCES/kernel.tar.gz . && \
		echo "======= Copying build files =======" && \
		cp build/* /home/$(HOST_USER)/rpmbuild/SOURCES && \
		echo "======= Starting RPM Build Process =======" && \
		time rpmbuild -ba "$$SPEC" && \
		echo "======= RPM Build Completed =======" && \
		echo "RPM packages location: /home/$(HOST_USER)/rpmbuild/RPMS/"' && \
	docker stop $(CONTAINER_NAME)

rpmbuild-5.10-arm64: setup-container setup-cross-compile	## Build kernel RPM package (ctkernel-lts-5.10, ARM64)
	@echo "======= Building RPM Package (5.10/ARM64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_5_10_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "======= Setting up ARM64 environment =======" && \
		$(SETUP_ARM64_ENV) && \
		echo "======= Configuring kernel (ARM64) =======" && \
		make openeuler_defconfig && \
		echo "======= Starting RPM Build Process (ARM64) =======" && \
		make binrpm-pkg -j$$(nproc) && \
		echo "======= RPM Build Completed =======" && \
		echo "RPM packages location: ~/rpmbuild/RPMS/"' && \
	docker stop $(CONTAINER_NAME)

rpmbuild-6.6-arm64: setup-container setup-cross-compile	## Build kernel RPM package (ctkernel-lts-6.6, ARM64)
	@echo "======= Building RPM Package (6.6/ARM64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_6_6_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "======= Setting up ARM64 environment =======" && \
		$(SETUP_ARM64_ENV) && \
		echo "======= Configuring kernel (ARM64) =======" && \
		make ctyunos_defconfig && \
		echo "======= Starting RPM Build Process (ARM64) =======" && \
		make binrpm-pkg -j$$(nproc) && \
		echo "======= RPM Build Completed =======" && \
		echo "RPM packages location: ~/rpmbuild/RPMS/"' && \
	docker stop $(CONTAINER_NAME)

build-5.10-amd64: setup-container	## Compile kernel only (ctkernel-lts-5.10, AMD64)
	@echo "======= Starting Kernel Build (5.10/AMD64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_5_10_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "Working in: $$(pwd)" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Configuring kernel =======" && \
		time make openeuler_defconfig && \
		echo "======= Starting compilation =======" && \
		time make -j$$(nproc)' && \
	$(PRINT_BUILD_TIME) && \
	docker stop $(CONTAINER_NAME)

build-6.6-amd64: setup-container	## Compile kernel only (ctkernel-lts-6.6, AMD64)
	@echo "======= Starting Kernel Build (6.6/AMD64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_6_6_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "Working in: $$(pwd)" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Configuring kernel =======" && \
		time make ctyunos_defconfig && \
		echo "======= Starting compilation =======" && \
		time make -j$$(nproc)' && \
	$(PRINT_BUILD_TIME) && \
	docker stop $(CONTAINER_NAME)

build-5.10-arm64: setup-container setup-cross-compile	## Compile kernel only (ctkernel-lts-5.10, ARM64)
	@echo "======= Starting Kernel Build (5.10/ARM64) ======="
	@echo "Using branch: $(KERNEL_5_10_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_5_10_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "Working in: $$(pwd)" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Setting up ARM64 environment =======" && \
		$(SETUP_ARM64_ENV) && \
		echo "======= Configuring kernel (ARM64) =======" && \
		time make openeuler_defconfig && \
		echo "======= Starting compilation (ARM64) =======" && \
		time make -j$$(nproc)' && \
	$(PRINT_BUILD_TIME) && \
	docker stop $(CONTAINER_NAME)

build-6.6-arm64: setup-container setup-cross-compile	## Compile kernel only (ctkernel-lts-6.6, ARM64)
	@echo "======= Starting Kernel Build (6.6/ARM64) ======="
	@echo "Using branch: $(KERNEL_6_6_BRANCH)"
	@echo "Build started at: $$(date '+%Y-%m-%d %H:%M:%S')"
	@START_TIME=$$(date +%s) && \
	docker exec -i $(CONTAINER_NAME) bash -c ' \
		set -e && \
		BRANCH="$(KERNEL_6_6_BRANCH)" && \
		echo "======= Finding source directory =======" && \
		WORK_DIR=$$(git -C /home/$(HOST_USER)/code/linux worktree list 2>/dev/null | grep "$$BRANCH" | awk "{print \$$1}" | head -1) && \
		if [ -z "$$WORK_DIR" ]; then \
			BRANCH_DIR=$$(printf "%s" "$$BRANCH" | sed "s|/|-|g") && \
			if [ -d /home/$(HOST_USER)/code/$$BRANCH_DIR ]; then \
				WORK_DIR=/home/$(HOST_USER)/code/$$BRANCH_DIR; \
			else \
				WORK_DIR=/home/$(HOST_USER)/code/linux && \
				cd $$WORK_DIR && git checkout $$BRANCH; \
			fi; \
		fi && \
		echo "Using worktree: $$WORK_DIR" && \
		cd "$$WORK_DIR" && \
		echo "Working in: $$(pwd)" && \
		echo "======= Finding spec file =======" && \
		SPEC=$$(find build -type f -name "*.spec" -print -quit 2>/dev/null) && \
		if [ -z "$$SPEC" ]; then \
			echo "No .spec found in build/" && exit 1; \
		fi && \
		echo "Using spec: $$SPEC" && \
		echo "======= Installing dependencies =======" && \
		sudo yum-builddep "$$SPEC" -y && \
		echo "======= Setting up ARM64 environment =======" && \
		$(SETUP_ARM64_ENV) && \
		echo "======= Configuring kernel (ARM64) =======" && \
		time make ctyunos_defconfig && \
		echo "======= Starting compilation (ARM64) =======" && \
		time make -j$$(nproc)' && \
	$(PRINT_BUILD_TIME) && \
	docker stop $(CONTAINER_NAME)

shell:	## Open shell in running container
	@echo "Opening shell in container $(CONTAINER_NAME)..."
	@if [ ! $$(docker ps -q -f name=$(CONTAINER_NAME)) ]; then \
		echo "Container not running, setting up..."; \
		make setup-container; \
	fi
	@docker exec -it $(CONTAINER_NAME) /bin/bash

clean-images:	## Remove CTYunOS docker images (including base images)
	@echo "WARNING: This will remove all CTYunOS images!"
	@echo "Removing CTYunOS docker images..."
	@docker rmi $(DEV_IMAGE)
	@docker rmi $(BUILD_IMAGE)
	@docker rmi $(BASE_IMAGE)

clean-container:	## Stop and remove container only (keep all images)
	@echo "Stopping and removing container $(CONTAINER_NAME)..."
	@docker stop $(CONTAINER_NAME)
	@docker rm $(CONTAINER_NAME)
	@echo "Container removed. All images ($(BASE_IMAGE), $(BUILD_IMAGE), $(DEV_IMAGE)) preserved."
	@echo "Use 'make setup-container' to quickly recreate container from existing images."

clean: clean-container clean-images	## Clean up everything (containers + all images)

status:	## Show container and image status
	@echo "=== Docker Images ==="
	@docker images | grep -E "(ctyunos|REPOSITORY)" || echo "No CTYunOS images found"
	@echo ""
	@echo "=== Container Status ==="
	@docker ps -a | grep -E "($(CONTAINER_NAME)|CONTAINER)" || echo "No containers found"
