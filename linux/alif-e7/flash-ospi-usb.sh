#!/usr/bin/env bash
#
# flash-ospi-usb.sh — Flash OSPI via USB CDC-ACM XMODEM transfer
#
# Detects the Alif USB CDC-ACM device, sends the combined OSPI image
# via XMODEM-1K protocol, and reports progress.
#
# Prerequisites:
#   - Board flashed with programming mode config (linux-boot-e7-ospi-usbflash.json)
#   - Board power-cycled so M55 flasher firmware is running
#   - USB CDC-ACM device enumerated (PRG_USB cable connected)
#   - pyserial: pip install pyserial
#
# Usage:
#   ./flash-ospi-usb.sh                          # flash combined image
#   ./flash-ospi-usb.sh path/to/image.bin        # flash specific image
#   ./flash-ospi-usb.sh --device /dev/cu.usbmodemXXX  # specify device

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_IMAGE="$SCRIPT_DIR/images/ospi-combined.bin"

# Parse args
DEVICE=""
IMAGE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --device) DEVICE="$2"; shift 2 ;;
        *) IMAGE="$1"; shift ;;
    esac
done
IMAGE="${IMAGE:-$DEFAULT_IMAGE}"

if [[ ! -f "$IMAGE" ]]; then
    echo "Error: image not found: $IMAGE"
    echo "Run ./make-ospi-image.sh first"
    exit 1
fi

IMAGE_SIZE=$(stat -f%z "$IMAGE")
echo "Image: $(basename "$IMAGE") ($IMAGE_SIZE bytes)"
echo "ETA:   ~$((IMAGE_SIZE / 61440)) sec at 60 KB/s"

exec python3 "$SCRIPT_DIR/xmodem-send.py" ${DEVICE:+--device "$DEVICE"} "$IMAGE"
