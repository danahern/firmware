#!/usr/bin/env bash
#
# stage-ospi.sh — Build TF-A + stage all artifacts for OSPI boot
#
# Complete staging pipeline:
#   1. Build TF-A from source (tfa-build container, ~5 sec)
#   2. Copy kernel + rootfs from yocto-build container
#   3. Verify all artifacts
#
# Output goes to tools/setools/build/images/ (where the alif-flash MCP reads from).
# DTB (devkit-e7-ospi.dtb) is NOT copied — it's maintained manually with fdtput
# patches and lives directly in images/.
#
# Usage:
#   ./stage-ospi.sh              # full build + copy + verify
#   ./stage-ospi.sh --no-tfa     # skip TF-A build (kernel + rootfs only)
#   ./stage-ospi.sh --dry-run    # show what would happen

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
STAGING_DIR="$WORKSPACE_ROOT/tools/setools/build/images"
YOCTO_CONTAINER="yocto-build"
DEPLOY_DIR="/home/builder/yocto/build-alif-e7/tmp/deploy/images/appkit-e7"

BUILD_TFA=true
DRY_RUN=false
for arg in "$@"; do
    case "$arg" in
        --no-tfa)   BUILD_TFA=false ;;
        --dry-run)  DRY_RUN=true ;;
        -h|--help)
            echo "Usage: $0 [--no-tfa] [--dry-run]"
            echo "  --no-tfa    Skip TF-A build (only stage kernel + rootfs)"
            echo "  --dry-run   Show what would happen without doing it"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

echo "=== Alif E7 OSPI Staging Pipeline ==="
echo "Staging directory: $STAGING_DIR"
echo ""

# --- Step 1: Build TF-A ---
if $BUILD_TFA; then
    echo "--- Step 1: Build TF-A ---"
    if $DRY_RUN; then
        echo "[dry-run] Would run: $SCRIPT_DIR/build-tfa.sh"
    else
        "$SCRIPT_DIR/build-tfa.sh"
    fi
    echo ""
fi

# --- Step 2: Stage kernel + rootfs from Yocto ---
echo "--- Step 2: Stage kernel + rootfs ---"

# Check if yocto-build container is running
if ! docker inspect "$YOCTO_CONTAINER" --format '{{.State.Status}}' 2>/dev/null | grep -q running; then
    echo "Warning: container '$YOCTO_CONTAINER' is not running — skipping kernel/rootfs"
    echo "Start it with: docker start yocto-build"
else
    mkdir -p "$STAGING_DIR"

    # Try appkit-e7 deploy dir first, fall back to devkit-e8
    if docker exec "$YOCTO_CONTAINER" test -d "$DEPLOY_DIR" 2>/dev/null; then
        ACTUAL_DEPLOY="$DEPLOY_DIR"
        ROOTFS_NAME="alif-tiny-image-appkit-e7.rootfs.cramfs-xip"
    else
        ACTUAL_DEPLOY="/home/builder/yocto/build-alif-e7/tmp/deploy/images/devkit-e8"
        ROOTFS_NAME="alif-tiny-image-devkit-e8.rootfs.cramfs-xip"
        echo "Note: Using devkit-e8 deploy dir (appkit-e7 not found)"
    fi

    SRCS=(
        "$ACTUAL_DEPLOY/xipImage"
        "$ACTUAL_DEPLOY/$ROOTFS_NAME"
    )
    DSTS=(
        "$STAGING_DIR/xipImage-ospi"
        "$STAGING_DIR/rootfs-ospi.bin"
    )

    for i in 0 1; do
        src="${SRCS[$i]}"
        dst="${DSTS[$i]}"
        name=$(basename "$dst")

        real=$(docker exec "$YOCTO_CONTAINER" readlink -f "$src" 2>/dev/null) || {
            echo "Warning: $src not found in container, skipping"
            continue
        }

        if $DRY_RUN; then
            size=$(docker exec "$YOCTO_CONTAINER" stat -c %s "$real" 2>/dev/null || echo "?")
            echo "[dry-run] $real -> $name ($size bytes)"
        else
            echo "Copying $name..."
            docker cp "$YOCTO_CONTAINER:$real" "$dst"
            ls -lh "$dst"
        fi
    done
fi

echo ""

# --- Step 3: Verify all artifacts ---
echo "--- Step 3: Verify staged artifacts ---"

ERRORS=0

# TF-A
if [ -f "$STAGING_DIR/bl32-ospi.bin" ]; then
    SIZE=$(stat -f%z "$STAGING_DIR/bl32-ospi.bin" 2>/dev/null || stat -c%s "$STAGING_DIR/bl32-ospi.bin")
    if strings "$STAGING_DIR/bl32-ospi.bin" | grep -q "USB clocks enabled"; then
        echo "OK  TF-A: bl32-ospi.bin ($SIZE bytes, USB clocks present)"
    else
        echo "FAIL TF-A: bl32-ospi.bin missing USB clock enabling!"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "MISS TF-A: bl32-ospi.bin not found"
    ERRORS=$((ERRORS + 1))
fi

# DTB
if [ -f "$STAGING_DIR/devkit-e7-ospi.dtb" ]; then
    SIZE=$(stat -f%z "$STAGING_DIR/devkit-e7-ospi.dtb" 2>/dev/null || stat -c%s "$STAGING_DIR/devkit-e7-ospi.dtb")
    MD5=$(md5 -q "$STAGING_DIR/devkit-e7-ospi.dtb" 2>/dev/null || md5sum "$STAGING_DIR/devkit-e7-ospi.dtb" | cut -d' ' -f1)
    echo "OK  DTB:  devkit-e7-ospi.dtb ($SIZE bytes, md5 $MD5)"
    # Also ensure .dtb.bin copy exists for JLink
    if [ ! -f "$STAGING_DIR/devkit-e7-ospi.dtb.bin" ] || \
       [ "$STAGING_DIR/devkit-e7-ospi.dtb" -nt "$STAGING_DIR/devkit-e7-ospi.dtb.bin" ]; then
        cp "$STAGING_DIR/devkit-e7-ospi.dtb" "$STAGING_DIR/devkit-e7-ospi.dtb.bin"
        echo "     Copied .dtb → .dtb.bin (JLink loadbin requires .bin extension)"
    fi
else
    echo "MISS DTB:  devkit-e7-ospi.dtb not found (maintained manually)"
    ERRORS=$((ERRORS + 1))
fi

# Kernel
if [ -f "$STAGING_DIR/xipImage-ospi" ]; then
    SIZE=$(stat -f%z "$STAGING_DIR/xipImage-ospi" 2>/dev/null || stat -c%s "$STAGING_DIR/xipImage-ospi")
    echo "OK  Kernel: xipImage-ospi ($SIZE bytes)"
else
    echo "MISS Kernel: xipImage-ospi not found"
    ERRORS=$((ERRORS + 1))
fi

# RootFS
if [ -f "$STAGING_DIR/rootfs-ospi.bin" ]; then
    SIZE=$(stat -f%z "$STAGING_DIR/rootfs-ospi.bin" 2>/dev/null || stat -c%s "$STAGING_DIR/rootfs-ospi.bin")
    echo "OK  RootFS: rootfs-ospi.bin ($SIZE bytes)"
else
    echo "MISS RootFS: rootfs-ospi.bin not found"
    ERRORS=$((ERRORS + 1))
fi

# M55 stub
if [ -f "$STAGING_DIR/m55_stub_hp.bin" ]; then
    echo "OK  M55:  m55_stub_hp.bin (prebuilt)"
else
    echo "MISS M55:  m55_stub_hp.bin not found"
    ERRORS=$((ERRORS + 1))
fi

echo ""
if [ $ERRORS -gt 0 ]; then
    echo "WARNING: $ERRORS artifact(s) missing or invalid!"
    exit 1
else
    echo "All artifacts verified."
    echo ""
    echo "Flash MRAM (TFA + DTB):  alif-flash.jlink_flash(config=\"linux-boot-e7-mram.json\", verify=true)"
    echo "Flash OSPI (kernel + rootfs): alif-flash.jlink_flash(config=\"linux-boot-e7-ospi-jlink.json\", verify=true)"
fi
