#!/usr/bin/env bash
#
# build-tfa.sh — Build TF-A for Alif E8 AppKit from source
#
# E8-specific differences from E7:
#   ARM_TRUSTED_SRAM_BASE=0x08000000 — SRAM1 at 0x08000000 from A32 (same as E7)
#   FLASH_EN=0                       — E8 OSPI IP v2.01 flash init not yet supported
#   OSPI_NO_XIP_SER=1                — E8 OSPI v2.01 removed XIP_SER register
#   PRELOADED_BL33_BASE=0x80020000   — Kernel in MRAM (MRAM-only boot)
#   ARM_PRELOADED_DTB_BASE=0x80010000 — DTB in MRAM
#
# The OSPI clock gate on E8 is at 0x4902F03C (not enabled by default).
# This is handled in the device config JSON, not in TF-A.
#
# Usage:
#   ./build-tfa.sh              # clean build bl32-e8.bin (normal Linux boot)
#   ./build-tfa.sh --usb-init   # clean build bl32-usbinit-e8.bin (USB programming mode)
#   ./build-tfa.sh --both       # build both variants (clean between)
#   ./build-tfa.sh --no-clean   # incremental build (skip make realclean)
#   ./build-tfa.sh --dry-run    # show build command without executing

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
STAGING_DIR="$WORKSPACE_ROOT/tools/setools/build/images"
CONTAINER="tfa-build"
OUTPUT_BIN="/workspace/build/devkit_e7/debug/bl32.bin"

CLEAN=true
DRY_RUN=false
USB_INIT=false
BUILD_BOTH=false
for arg in "$@"; do
    case "$arg" in
        --no-clean) CLEAN=false ;;
        --dry-run)  DRY_RUN=true ;;
        --usb-init) USB_INIT=true ;;
        --both)     BUILD_BOTH=true ;;
        -h|--help)
            echo "Usage: $0 [--no-clean] [--dry-run] [--usb-init] [--both]"
            echo "  --no-clean  Skip make realclean (incremental build)"
            echo "  --dry-run   Show build command without executing"
            echo "  --usb-init  Build USB programming mode variant (bl32-usbinit-e8.bin)"
            echo "  --both      Build both bl32-e8.bin and bl32-usbinit-e8.bin"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

# --both recursively calls this script for each variant
if $BUILD_BOTH; then
    echo "=== Building both TF-A E8 variants ==="
    EXTRA_ARGS=""
    $DRY_RUN && EXTRA_ARGS="--dry-run"
    "$0" $EXTRA_ARGS
    "$0" --usb-init $EXTRA_ARGS
    echo ""
    echo "=== Both E8 variants built ==="
    exit 0
fi

BUILD_CMD='make -j$(nproc) PLAT=devkit_e7 ARCH=aarch32 AARCH32_SP=sp_min \
    CROSS_COMPILE=arm-linux-gnueabihf- ARM_LINUX_KERNEL_AS_BL33=1 \
    ARM_TRUSTED_SRAM_BASE=0x08000000 PRELOADED_BL33_BASE=0x80020000 \
    ARM_PRELOADED_DTB_BASE=0x80010000 RAM_PRELOADED_DTB_BASE=0x02390000 \
    ENABLE_PIE=1 ENABLE_STACK_PROTECTOR=strong \
    BL32_IN_XIP_MEM=1 BL32_XIP_BASE=0x80002000 \
    UART=2 HYPRAM_EN=1 FLASH_EN=0 AES_EN=0 MODEM_SRAM=0 \
    OSPI_NO_XIP_SER=1 DEBUG=1'

if $USB_INIT; then
    BUILD_CMD="$BUILD_CMD USB_INIT_HALT=1"
    OUTPUT_NAME="bl32-usbinit-e8.bin"
    VARIANT="USB programming mode"
else
    BUILD_CMD="$BUILD_CMD USB_INIT_HALT=0"
    OUTPUT_NAME="bl32-e8.bin"
    VARIANT="normal Linux boot"
fi
BUILD_CMD="$BUILD_CMD bl32"

if $DRY_RUN; then
    echo "Container: $CONTAINER"
    echo "Variant: $VARIANT"
    echo "Output: $OUTPUT_NAME"
    echo "Clean: $CLEAN"
    echo ""
    if $CLEAN; then
        echo "Step 1: make realclean"
    fi
    echo "Step 2: $BUILD_CMD"
    echo ""
    echo "Output: $OUTPUT_BIN -> $STAGING_DIR/$OUTPUT_NAME"
    exit 0
fi

# Verify container is running
if ! docker inspect "$CONTAINER" --format '{{.State.Status}}' 2>/dev/null | grep -q running; then
    echo "Error: container '$CONTAINER' is not running"
    echo "Start it with: docker start tfa-build"
    exit 1
fi

# Clean build
if $CLEAN; then
    echo "=== Cleaning previous build ==="
    docker exec "$CONTAINER" bash -c "cd /workspace && make realclean"
fi

# Build
echo "=== Building TF-A for E8 ($VARIANT) ==="
docker exec "$CONTAINER" bash -c "cd /workspace && $BUILD_CMD"

# Verify output exists
if ! docker exec "$CONTAINER" test -f "$OUTPUT_BIN"; then
    echo "Error: build output not found at $OUTPUT_BIN"
    exit 1
fi

# Get size and verify
SIZE=$(docker exec "$CONTAINER" stat -c %s "$OUTPUT_BIN")
echo ""
echo "=== Build output: $SIZE bytes ==="

# Copy to staging
mkdir -p "$STAGING_DIR"
docker cp "$CONTAINER:$OUTPUT_BIN" "$STAGING_DIR/$OUTPUT_NAME"
echo "Staged: $STAGING_DIR/$OUTPUT_NAME"

# Verify critical features
echo ""
echo "=== Verification ==="

if strings "$STAGING_DIR/$OUTPUT_NAME" | grep -q "USB clocks enabled"; then
    echo "OK: USB clock enabling present"
else
    echo "FAIL: 'USB clocks enabled' string NOT found"
    exit 1
fi

if strings "$STAGING_DIR/$OUTPUT_NAME" | grep -q "HyperRAM configured"; then
    echo "OK: HyperRAM initialization present"
else
    echo "FAIL: HyperRAM init not found"
    exit 1
fi

# FLASH_EN=0, so skip OSPI flash verification
echo "NOTE: OSPI flash init disabled (FLASH_EN=0)"

if $USB_INIT; then
    if strings "$STAGING_DIR/$OUTPUT_NAME" | grep -q "USB_INIT_HALT"; then
        echo "OK: USB_INIT_HALT mode present"
    else
        echo "FAIL: USB_INIT_HALT string not found"
        exit 1
    fi
fi

BUILD_DATE=$(strings "$STAGING_DIR/$OUTPUT_NAME" | grep "^Built :" || echo "(not found)")
echo "Build: $BUILD_DATE"

MD5=$(md5 -q "$STAGING_DIR/$OUTPUT_NAME" 2>/dev/null || md5sum "$STAGING_DIR/$OUTPUT_NAME" | cut -d' ' -f1)
echo "MD5: $MD5"
echo "Size: $(stat -f%z "$STAGING_DIR/$OUTPUT_NAME" 2>/dev/null || stat -c%s "$STAGING_DIR/$OUTPUT_NAME") bytes"

echo ""
echo "=== Ready to flash ==="
if $USB_INIT; then
    echo "Flash with: alif-flash.jlink_flash(config=\"linux-boot-e8-ospi-usbflash.json\")"
else
    echo "Flash with: alif-flash.jlink_flash(config=\"linux-boot-e8-mram.json\")"
fi
