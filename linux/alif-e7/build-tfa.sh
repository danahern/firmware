#!/usr/bin/env bash
#
# build-tfa.sh — Build TF-A for Alif E7 AppKit from source
#
# Builds bl32.bin in the tfa-build Docker container using the verified
# build flags and stages the output to tools/setools/build/images/.
#
# Source: alif_arm-tf repo (bind-mounted at /workspace in container)
# Output: bl32-ospi.bin → tools/setools/build/images/
#
# CRITICAL BUILD FLAGS (omitting any of these produces a broken binary):
#   ENABLE_PIE=1            — Position-independent code (required for E7)
#   ENABLE_STACK_PROTECTOR=strong — Stack canaries (~30KB binary; without = ~26KB)
#   HYPRAM_EN=1             — HyperRAM initialization
#   FLASH_EN=1              — OSPI flash initialization
#   PRELOADED_BL33_BASE=0xC0800000 — Kernel XIP address in OSPI
#   ARM_PRELOADED_DTB_BASE=0x80200000 — DTB in MRAM (SE REV_B4 minimum)
#   UART=2                  — Console on UART2
#
# The binary MUST contain "USB clocks enabled" string (added in commit 56dfb6fd).
# Without USB clock enabling, TF-A boots but kernel silently fails.
#
# Usage:
#   ./build-tfa.sh              # clean build + stage + verify
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
for arg in "$@"; do
    case "$arg" in
        --no-clean) CLEAN=false ;;
        --dry-run)  DRY_RUN=true ;;
        -h|--help)
            echo "Usage: $0 [--no-clean] [--dry-run]"
            echo "  --no-clean  Skip make realclean (incremental build)"
            echo "  --dry-run   Show build command without executing"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

BUILD_CMD='make -j$(nproc) PLAT=devkit_e7 ARCH=aarch32 AARCH32_SP=sp_min \
    CROSS_COMPILE=arm-linux-gnueabihf- ARM_LINUX_KERNEL_AS_BL33=1 \
    ARM_TRUSTED_SRAM_BASE=0x08000000 PRELOADED_BL33_BASE=0xC0800000 \
    ARM_PRELOADED_DTB_BASE=0x80200000 RAM_PRELOADED_DTB_BASE=0x02390000 \
    ENABLE_PIE=1 ENABLE_STACK_PROTECTOR=strong \
    BL32_IN_XIP_MEM=1 BL32_XIP_BASE=0x80002000 \
    UART=2 HYPRAM_EN=1 FLASH_EN=1 AES_EN=0 MODEM_SRAM=0 DEBUG=1 bl32'

if $DRY_RUN; then
    echo "Container: $CONTAINER"
    echo "Clean: $CLEAN"
    echo ""
    if $CLEAN; then
        echo "Step 1: make realclean"
    fi
    echo "Step 2: $BUILD_CMD"
    echo ""
    echo "Output: $OUTPUT_BIN → $STAGING_DIR/bl32-ospi.bin"
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
echo "=== Building TF-A ==="
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

# Size sanity check
if [ "$SIZE" -lt 28000 ]; then
    echo "WARNING: Binary is only $SIZE bytes (expected ~30KB)"
    echo "This likely means ENABLE_STACK_PROTECTOR=strong was not applied."
    echo "The binary may not boot correctly."
fi

# Copy to staging
mkdir -p "$STAGING_DIR"
docker cp "$CONTAINER:$OUTPUT_BIN" "$STAGING_DIR/bl32-ospi.bin"
echo "Staged: $STAGING_DIR/bl32-ospi.bin"

# Verify critical features
echo ""
echo "=== Verification ==="

if strings "$STAGING_DIR/bl32-ospi.bin" | grep -q "USB clocks enabled"; then
    echo "OK: USB clock enabling present"
else
    echo "FAIL: 'USB clocks enabled' string NOT found — this binary will NOT boot the kernel!"
    exit 1
fi

if strings "$STAGING_DIR/bl32-ospi.bin" | grep -q "HyperRAM configured"; then
    echo "OK: HyperRAM initialization present"
else
    echo "FAIL: HyperRAM init not found"
    exit 1
fi

if strings "$STAGING_DIR/bl32-ospi.bin" | grep -q "OSPI NOR Flash"; then
    echo "OK: OSPI flash initialization present"
else
    echo "FAIL: OSPI init not found"
    exit 1
fi

BUILD_DATE=$(strings "$STAGING_DIR/bl32-ospi.bin" | grep "^Built :")
echo "Build: $BUILD_DATE"

MD5=$(md5 -q "$STAGING_DIR/bl32-ospi.bin" 2>/dev/null || md5sum "$STAGING_DIR/bl32-ospi.bin" | cut -d' ' -f1)
echo "MD5: $MD5"
echo "Size: $(stat -f%z "$STAGING_DIR/bl32-ospi.bin" 2>/dev/null || stat -c%s "$STAGING_DIR/bl32-ospi.bin") bytes"

echo ""
echo "=== Ready to flash ==="
echo "Flash with: alif-flash.jlink_flash(config=\"linux-boot-e7-mram.json\", verify=true)"
