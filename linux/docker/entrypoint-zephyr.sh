#!/bin/bash
# Zephyr builder entrypoint
# Runs SDK setup on first start (registers cmake package in ~/.cmake)

set -e

SDK_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk}"
CMAKE_PKG="$HOME/.cmake/packages/Zephyr-sdk"

if [ ! -d "$CMAKE_PKG" ]; then
    echo "[entrypoint] Registering Zephyr SDK cmake package..."
    "$SDK_DIR/setup.sh" -t arm-zephyr-eabi -c
    echo "[entrypoint] SDK registered."
fi

# Ensure Zephyr toolchain capability cache dir is writable by this user
ZEPHYR_CACHE="${ZEPHYR_BASE:-/workspace/zephyr}/.cache/ToolchainCapabilityDatabase"
if [ ! -d "$ZEPHYR_CACHE" ]; then
    mkdir -p "$ZEPHYR_CACHE" 2>/dev/null || true
fi

exec "$@"
