# E7 SETOOLS Flashing

Flash Linux images to E7 MRAM via Alif's Secure Enclave UART.

## Prerequisites

1. **SETOOLS**: Download Alif Security Toolkit v1.107.00+ (macOS arm64) from [alifsemi.com/support/kits/ensemble-e7devkit/](https://alifsemi.com/support/kits/ensemble-e7devkit/). Extract to `tools/setools/` (workspace root). Registration required. Remove quarantine: `xattr -r -d com.apple.quarantine tools/setools/`

2. **Device config**: Run `tools/setools/tools-config` to select your device part and revision.

3. **Hardware**: Connect micro-USB to **PRG_USB** port (closest to board corner). This provides power + two serial ports:
   - First port = SE-UART (SETOOLS programming)
   - Second port = UART2 (Linux console, 115200 baud)
   - **Note**: The J-Link USB port provides a CDC serial port but it is NOT the SE-UART. You need PRG_USB specifically.

4. **Docker**: Yocto build container (`yocto-build`) with completed E7 build (DISTRO=apss-tiny).

## Quick Start

```bash
# Full workflow: copy artifacts from Docker + generate ATOC + flash
./flash-e7.sh

# Or step by step:
./flash-e7.sh --copy-only    # Just copy artifacts
./flash-e7.sh --flash-only   # Just run SETOOLS (artifacts already in place)
```

## Files

| File | Purpose |
|------|---------|
| `linux-boot-e7.json` | ATOC config — memory addresses for each image |
| `flash-e7.sh` | Helper script — copies artifacts + runs SETOOLS |
| `README.md` | This file |

## Memory Map

| Image | MRAM Address | Description |
|-------|-------------|-------------|
| bl32.bin | 0x80002000 | TF-A BL32 (Secure Payload) |
| devkit-e8.dtb | 0x80010000 | Device tree blob |
| xipImage | 0x80020000 | XIP Linux kernel |
| cramfs-xip | 0x80380000 | Read-only root filesystem |

Addresses from `devkit-e8.conf` (SMP=1, BASE_IMAGE=1).

## Serial Port Setup

Identify ports after connecting PRG_USB:
```bash
ls /dev/cu.usbmodem*    # or ls /dev/cu.usbserial*
```

The tool auto-discovers serial ports. If multiple ports exist, it will prompt for selection.

Monitor Linux console on the **second** port:
```bash
screen /dev/cu.usbmodem<SECOND_PORT> 115200
```

## Troubleshooting

- **`app-write-mram` silent exit (no output)**: Always use `-p` flag. Without it, the tool calls `os._exit(1)` silently when images aren't 16-byte aligned.
- **`-i` FileNotFoundError**: Paths must start with `../` (tool strips first 3 chars for legacy `bin/` directory compat). Example: `./app-write-mram -v -p -i "../build/images/bl32.bin 0x80002000"`
- **"Target did not respond"**: Wrong serial port. Verify PRG_USB is connected (not just J-Link USB). The J-Link CDC port (`/dev/cu.usbmodem*` with SEGGER serial number) cannot communicate with the Secure Enclave.
- **macOS quarantine**: If tools fail to run, remove quarantine: `xattr -r -d com.apple.quarantine tools/setools/`
- **`app-write-mram` hangs on port**: Close any terminal sessions on the SE-UART port first (only one process can use it).
- **No serial ports**: Check PRG_USB cable. DevKit jumper J26: pins 1-3 and 2-4 connected.
- **Boot hangs**: Monitor console UART for TF-A output. If no output at all, SETOOLS may not have written correctly — try `app-write-mram` again.
