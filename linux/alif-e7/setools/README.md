# E7 SETOOLS Flashing

Flash Linux images to E7 MRAM via Alif's Secure Enclave UART.

## Prerequisites

1. **SETOOLS**: Download Alif Security Toolkit v1.107.00+ (macOS arm64) from [alifsemi.com/support/kits/ensemble-e7devkit/](https://alifsemi.com/support/kits/ensemble-e7devkit/). Extract to `tools/setools/` (workspace root). Registration required. Remove quarantine: `xattr -r -d com.apple.quarantine tools/setools/`

2. **Device config**: Run `tools/setools/tools-config` to select device **E7 (AE722F80F55D5LS)**.

3. **Python**: `pip3 install pyserial`

4. **Hardware**: Connect micro-USB to **PRG_USB** port. This provides:
   - SE-UART (ISP programming, 57600 baud)
   - UART2 (Linux console, 115200 baud) — requires J15 jumpers in UART2 position

5. **Docker**: APSS build container (`alif-apss-build`) with completed E7 build.

## Quick Start

```bash
# Full workflow: copy artifacts from Docker + generate ATOC + flash
./flash-e7.sh

# Enter maintenance mode first (needed for clean flash):
./flash-e7.sh --maintenance

# Or use alif-flash.py directly:
./alif-flash.py probe              # Check if SE is responsive
./alif-flash.py maintenance        # Enter maintenance mode
./alif-flash.py flash              # Flash images (SE must be in maintenance mode)
./alif-flash.py flash --maintenance --gen-toc  # Full flow
```

## alif-flash.py

Replaces the Alif `maintenance` binary (which hardcodes 55000 baud — wrong for E7 at 57600) and the 7 ad-hoc `isp_*.py` scripts with a single reliable tool.

### Commands

| Command | Description |
|---------|-------------|
| `probe` | Check if SE-UART is responsive, report maintenance mode status |
| `maintenance` | Enter maintenance mode: START_ISP → SET_MAINTENANCE → RESET → verify |
| `flash` | Write all images from ATOC JSON to MRAM |
| `flash --maintenance` | Enter maintenance mode first, then flash |
| `flash --gen-toc` | Run `app-gen-toc` before flashing |

### Options

| Option | Description |
|--------|-------------|
| `--port PORT` | Override serial port (auto-detected from `/dev/cu.usbmodem*`) |
| `--config PATH` | ATOC JSON config (default: `tools/setools/build/config/linux-boot-e7.json`) |
| `--setools-dir PATH` | SETOOLS directory (default: `tools/setools/`) |

## Memory Map (appkit-e7.conf, devkit-ex-b0 branch)

| Image | MRAM Address | Description |
|-------|-------------|-------------|
| bl32.bin | 0x80002000 | TF-A BL32 (Secure Payload) |
| appkit-e7.dtb | 0x80010000 | Device tree blob |
| xipImage | 0x80020000 | XIP Linux kernel (5.4.25) |
| alif-tiny-image-appkit-e7.cramfs-xip | 0x80300000 | Read-only root filesystem |

## ISP Protocol

The SE-UART ISP protocol at 57600 baud:

- **Packet format**: `[length, cmd, data..., checksum]` — all bytes sum to 0 mod 256
- **Key commands**: START_ISP (0x00), STOP_ISP (0x01), BURN_MRAM (0x08), DOWNLOAD_DATA (0x04), SET_MAINTENANCE (0x16), RESET_DEVICE (0x09)
- **Data transfer**: 240 bytes per chunk with 2-byte LE sequence number
- **Images must be 16-byte aligned** (tool pads automatically)

## Troubleshooting

- **SE not responding**: Unplug/replug PRG_USB, run `./alif-flash.py maintenance` within 2-3 seconds
- **`maintenance` binary uses wrong baud (55000)**: Use `alif-flash.py` instead — it uses 57600
- **`app-write-mram` silent exit**: Always use `-p` flag, or use `alif-flash.py` which handles padding
- **No serial ports**: Check PRG_USB cable connection
- **No console output after flash**: Power cycle board (unplug/replug), check J15 jumpers are in UART2 position, monitor at 115200 baud
- **Firewall flood on SE-UART**: Previous bad firmware causing FC5 exceptions. Enter maintenance mode and reflash.

## Build System

The correct build system for E7 AppKit:
- Repo: `alifsemi/alif_linux-apss-build-setup` (main)
- Layers: `devkit-ex-b0` branch (NOT scarthgap)
- Machine: `MACHINE="appkit-e7"` (NOT appkit-e8)
- Kernel: `alif_linux` branch `devkit-b0-5.4.y` (kernel 5.4.25)
- Build: `bitbake alif-tiny-image`

See `docs/alif-e7/AUGD00013-Getting-Started-with-Linux-Prebuilt-Images-v0.5.2_3.pdf` for reference.
