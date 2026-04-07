DOCKER_IMAGE ?= wischner/sdcc-z80-idp:latest

WORKDIR      := $(CURDIR)
PROJECTS_DIR := $(abspath ..)
PROJECT_NAME := $(notdir $(WORKDIR))
UID          := $(shell id -u)
GID          := $(shell id -g)

DOCKER_RUN = docker run --rm \
	--user $(UID):$(GID) \
	-v "$(PROJECTS_DIR):/workspace" \
	-w "/workspace/$(PROJECT_NAME)" \
	$(DOCKER_IMAGE)

DOCKER_RUN_ROOT = docker run --rm \
	-v "$(PROJECTS_DIR):/workspace" \
	-w "/workspace/$(PROJECT_NAME)" \
	$(DOCKER_IMAGE)

all:
	@echo "[host] building libpartner dependency"
	@$(MAKE) -C ../libpartner build
	@echo "[host] building project inside docker"
	@$(DOCKER_RUN) make -C src all

docker-pull:
	@docker pull "$(DOCKER_IMAGE)"

fix-perms:
	@echo "[host] fixing ownership of build and bin outputs"
	@$(DOCKER_RUN_ROOT) sh -lc 'chown -R $(UID):$(GID) build bin 2>/dev/null || true'

clean:
	@echo "[host] removing build and bin outputs"
	@rm -rf build bin 2>/dev/null || true
	@if [ -e build ] || [ -e bin ]; then \
		echo "[host] retrying cleanup through docker as root"; \
		$(DOCKER_RUN_ROOT) sh -lc 'rm -rf build bin'; \
	fi

.PHONY: all clean docker-pull fix-perms
