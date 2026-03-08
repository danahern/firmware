#!/usr/bin/env bash
#
# make-ospi-image.sh — Create combined OSPI image: rootfs (padded to 8MB) + kernel
#
# The USB-to-OSPI flasher writes sequentially from address 0x00000000.
# OSPI layout matches DTB expectations:
#   0x00000000 (0xC0000000 XIP) — rootfs (cramfs-xip), padded to 8MB
#   0x00800000 (0xC0800000 XIP) — kernel (xipImage)
#
# Usage:
#   ./make-ospi-image.sh              # build combined image
#   ./make-ospi-image.sh --dry-run    # show sizes without writing

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGES_DIR="$SCRIPT_DIR/images"
ROOTFS="$IMAGES_DIR/rootfs-ospi.bin"
KERNEL="$IMAGES_DIR/xipImage-ospi"
OUTPUT="$IMAGES_DIR/ospi-combined.bin"
ROOTFS_PARTITION=8388608  # 8MB = 0x800000

DRY_RUN=false
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
fi

# Verify inputs exist
for f in "$ROOTFS" "$KERNEL"; do
    if [[ ! -f "$f" ]]; then
        echo "Error: $(basename "$f") not found at $f"
        echo "Run ./stage-ospi.sh first to copy Yocto artifacts"
        exit 1
    fi
done

ROOTFS_SIZE=$(stat -f%z "$ROOTFS")
KERNEL_SIZE=$(stat -f%z "$KERNEL")

if (( ROOTFS_SIZE > ROOTFS_PARTITION )); then
    echo "Error: rootfs ($ROOTFS_SIZE bytes) exceeds 8MB partition ($ROOTFS_PARTITION bytes)"
    exit 1
fi

PAD_SIZE=$((ROOTFS_PARTITION - ROOTFS_SIZE))
TOTAL_SIZE=$((ROOTFS_PARTITION + KERNEL_SIZE))

echo "rootfs:   $(basename "$ROOTFS")  $(numfmt --to=iec $ROOTFS_SIZE 2>/dev/null || echo "$ROOTFS_SIZE bytes")"
echo "kernel:   $(basename "$KERNEL")  $(numfmt --to=iec $KERNEL_SIZE 2>/dev/null || echo "$KERNEL_SIZE bytes")"
echo "padding:  $PAD_SIZE bytes (to fill 8MB rootfs partition)"
echo "total:    $(numfmt --to=iec $TOTAL_SIZE 2>/dev/null || echo "$TOTAL_SIZE bytes")"
echo "ETA:      ~$((TOTAL_SIZE / 61440)) sec at 60 KB/s"

if $DRY_RUN; then
    echo "[dry-run] Would write to: $OUTPUT"
    exit 0
fi

# Build combined image: rootfs + zero-pad + kernel
cp "$ROOTFS" "$OUTPUT"
dd if=/dev/zero bs=1 count=$PAD_SIZE >> "$OUTPUT" 2>/dev/null
cat "$KERNEL" >> "$OUTPUT"

ACTUAL_SIZE=$(stat -f%z "$OUTPUT")
echo ""
echo "Created: $OUTPUT ($ACTUAL_SIZE bytes)"
