#!/usr/bin/env bash
#
# flash-e7.sh — Copy Yocto artifacts from Docker + flash E7 MRAM
#
# Prerequisites:
#   1. SETOOLS installed at tools/setools/ (from workspace root)
#   2. Docker container "alif-apss-build" running with completed build
#   3. PRG_USB connected, board in maintenance mode
#
# Usage:
#   ./flash-e7.sh                  # copy + gen-toc + flash
#   ./flash-e7.sh --copy-only      # only copy artifacts from Docker
#   ./flash-e7.sh --flash-only     # only flash (artifacts already in place)
#   ./flash-e7.sh --maintenance    # enter maintenance mode + flash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
SETOOLS_DIR="$WORKSPACE_ROOT/tools/setools"
IMAGES_DIR="$SETOOLS_DIR/build/images"
CONFIG_DIR="$SETOOLS_DIR/build/config"
DOCKER_IMAGE_PATH="/home/apssbuilder/build-data/build-appkit-e7/tmp/deploy/images/appkit-e7"
CONTAINER_NAME="alif-apss-build"

ARTIFACTS=(
    "bl32.bin"
    "xipImage"
    "appkit-e7.dtb"
)
# cramfs-xip has a timestamped name — resolve the symlink target
CRAMFS_DOCKER="alif-tiny-image-appkit-e7.cramfs-xip"
CRAMFS_LOCAL="alif-tiny-image-appkit-e7.cramfs-xip"

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
        # Resolve symlinks by reading the link target
        REAL_NAME=$(docker exec "$CONTAINER_NAME" readlink -f "$DOCKER_IMAGE_PATH/$artifact" 2>/dev/null | xargs basename)
        docker cp "$CONTAINER_NAME:$DOCKER_IMAGE_PATH/$REAL_NAME" "$IMAGES_DIR/$artifact"
    done

    echo "  Copying cramfs-xip rootfs..."
    REAL_NAME=$(docker exec "$CONTAINER_NAME" readlink -f "$DOCKER_IMAGE_PATH/$CRAMFS_DOCKER" 2>/dev/null | xargs basename)
    docker cp "$CONTAINER_NAME:$DOCKER_IMAGE_PATH/$REAL_NAME" "$IMAGES_DIR/$CRAMFS_LOCAL"

    # Copy ATOC config from tracked location
    echo "  Copying ATOC config..."
    cp "$SCRIPT_DIR/linux-boot-e7.json" "$CONFIG_DIR/linux-boot-e7.json"

    echo "=== Artifacts ready ==="
    ls -lh "$IMAGES_DIR"/{bl32.bin,appkit-e7.dtb,xipImage,alif-tiny-image-appkit-e7.cramfs-xip}
}

flash_device() {
    local EXTRA_FLAGS=("$@")
    echo "=== Flashing E7 ==="
    python3 "$SCRIPT_DIR/alif-flash.py" flash --gen-toc "${EXTRA_FLAGS[@]}"
}

# Parse args
case "${1:-}" in
    --copy-only)
        copy_artifacts
        ;;
    --flash-only)
        flash_device
        ;;
    --maintenance)
        copy_artifacts
        flash_device --maintenance
        ;;
    *)
        copy_artifacts
        flash_device
        ;;
esac
