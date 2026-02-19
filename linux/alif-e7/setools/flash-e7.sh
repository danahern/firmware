#!/usr/bin/env bash
#
# flash-e7.sh — Copy Yocto artifacts from Docker + run SETOOLS to flash E7
#
# Prerequisites:
#   1. SETOOLS installed at ../../tools/setools/ (from workspace root: tools/setools/)
#   2. Docker container "yocto-build" running (or volume "yocto-data" accessible)
#   3. PRG_USB connected, SE-UART port identified
#
# Usage:
#   ./flash-e7.sh [--copy-only | --flash-only]
#     (no args)    = copy artifacts + generate ATOC + flash
#     --copy-only  = only copy artifacts from Docker
#     --flash-only = only run app-gen-toc + app-write-mram (artifacts already in place)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
SETOOLS_DIR="$WORKSPACE_ROOT/tools/setools/app-release-exec"
IMAGES_DIR="$SETOOLS_DIR/build/images"
CONFIG_DIR="$SETOOLS_DIR/build/config"
DOCKER_IMAGE_PATH="/home/builder/yocto/build-alif-e7/tmp/deploy/images/devkit-e8"
CONTAINER_NAME="yocto-build"

ARTIFACTS=(
    "bl32.bin"
    "xipImage"
    "devkit-e8.dtb"
    "core-image-minimal-devkit-e8.cramfs-xip"
)

copy_artifacts() {
    echo "=== Copying Yocto artifacts from Docker ==="

    if ! docker inspect "$CONTAINER_NAME" &>/dev/null; then
        echo "Error: Docker container '$CONTAINER_NAME' not found."
        echo "Start it with: docker start $CONTAINER_NAME"
        exit 1
    fi

    mkdir -p "$IMAGES_DIR" "$CONFIG_DIR"

    for artifact in "${ARTIFACTS[@]}"; do
        echo "  Copying $artifact..."
        docker cp "$CONTAINER_NAME:$DOCKER_IMAGE_PATH/$artifact" "$IMAGES_DIR/$artifact"
    done

    # Copy ATOC config from tracked location
    echo "  Copying ATOC config..."
    cp "$SCRIPT_DIR/linux-boot-e7.json" "$CONFIG_DIR/linux-boot-e7.json"

    echo "=== Artifacts ready ==="
    ls -lh "$IMAGES_DIR"/
}

flash_device() {
    echo "=== Generating ATOC ==="

    if [ ! -x "$SETOOLS_DIR/app-gen-toc" ]; then
        echo "Error: app-gen-toc not found or not executable at $SETOOLS_DIR/app-gen-toc"
        echo "Download SETOOLS from https://alifsemi.com/support/kits/ensemble-e7devkit/"
        echo "Extract to: $WORKSPACE_ROOT/tools/setools/"
        exit 1
    fi

    cd "$SETOOLS_DIR"
    ./app-gen-toc -f build/config/linux-boot-e7.json

    echo "=== Writing to MRAM via SE-UART ==="
    echo "NOTE: Close any terminal sessions on the SE-UART port first!"
    ./app-write-mram -d
}

# Parse args
case "${1:-}" in
    --copy-only)
        copy_artifacts
        ;;
    --flash-only)
        flash_device
        ;;
    *)
        copy_artifacts
        flash_device
        ;;
esac
