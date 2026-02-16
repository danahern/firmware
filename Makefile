# Zephyr Apps — Docker Build Makefile
#
# Reproducible builds using the same Zephyr CI container as GitHub Actions.
# Hardware operations (flash, BLE test, etc.) use native MCP tools.
#
# Usage:
#   make build APP=crash_debug BOARD=nrf54l15dk/nrf54l15/cpuapp
#   make test
#   make clean APP=crash_debug BOARD=nrf54l15dk/nrf54l15/cpuapp
#   make shell

DOCKER_IMAGE := ghcr.io/zephyrproject-rtos/ci:v0.28.7
WORKSPACE    := $(abspath ..)
BOARD_DIR     = $(subst /,_,$(BOARD))
SDK_SETUP     = /opt/toolchains/zephyr-sdk-*/setup.sh -c 2>/dev/null

DOCKER_BASE = docker run --rm \
	-v $(WORKSPACE):/workspace \
	-w /workspace/zephyr-apps \
	-e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	$(DOCKER_IMAGE)

.PHONY: build test clean shell pull

## Build an app for a board
## Artifacts: apps/<APP>/build/<BOARD>/zephyr/zephyr.{elf,hex}
build:
ifndef APP
	$(error APP is required. Usage: make build APP=<app> BOARD=<board>)
endif
ifndef BOARD
	$(error BOARD is required. Usage: make build APP=<app> BOARD=<board>)
endif
	$(DOCKER_BASE) bash -c '$(SDK_SETUP) && west build -b $(BOARD) apps/$(APP) -d apps/$(APP)/build/$(BOARD_DIR) --pristine=auto'

## Run all library unit tests on QEMU
test:
	$(DOCKER_BASE) bash -c '$(SDK_SETUP) && west twister -T lib -p qemu_cortex_m3 -v --inline-logs -O /workspace/.cache/twister'

## Clean build artifacts for an app+board
clean:
ifndef APP
	$(error APP is required. Usage: make clean APP=<app> BOARD=<board>)
endif
ifndef BOARD
	$(error BOARD is required. Usage: make clean APP=<app> BOARD=<board>)
endif
	rm -rf apps/$(APP)/build/$(BOARD_DIR)

## Interactive shell in the build container
shell:
	docker run --rm -it \
		-v $(WORKSPACE):/workspace \
		-w /workspace/zephyr-apps \
		-e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
		$(DOCKER_IMAGE) bash

## Pull the CI container image
pull:
	docker pull $(DOCKER_IMAGE)
