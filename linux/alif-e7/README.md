# Linux for Alif Ensemble E7 AppKit

Yocto-based Linux image and cross-compiled userspace apps for the Alif E7 Cortex-A32 cores.

## Architecture

The E7 has a heterogeneous architecture:
- **2x Cortex-A32** — Linux (this directory)
- **2x Cortex-M55** — Zephyr RTOS
- **Ethos-U55** — NPU for ML inference

Boot chain: Secure Enclave → TF-A → xipImage (no U-Boot). Kernel runs XIP from MRAM.

## Yocto Image Build

### Build System

Uses the official **alifsemi/alif_linux-apss-build-setup** orchestrator inside Docker. This produces the correct build with:
- OE zeus (Yocto 3.0) — NOT scarthgap
- `MACHINE = "appkit-e7"` — from `meta-alif-ensemble` branch `devkit-ex-b0`
- `DISTRO = "apss-tiny"` — musl libc + poky-tiny + busybox init (~1.3MB cramfs-xip)
- Kernel 5.4.25 from `alif_linux` branch `devkit-b0-5.4.y`
- TF-A from `alif_arm-tf` branch `devkit-ex-b0`

### Prerequisites

1. Docker Desktop with sufficient resources (8GB+ RAM, 50GB+ disk)
2. Pull the official Alif builder image: `docker pull apss/ubuntu-builder:v18.04`

### First-Time Setup

```bash
# Create named volume for build data persistence
docker volume create alif-apss-data

# Start container
docker run -dit --name alif-apss-build \
  -v alif-apss-data:/home/apssbuilder/build-data \
  apss/ubuntu-builder:v18.04 \
  tail -f /dev/null

# Clone orchestrator and run setup (clones all layers + generates build config)
docker exec -u apssbuilder alif-apss-build bash -c "
  cd /home/apssbuilder &&
  git clone https://github.com/alifsemi/alif_linux-apss-build-setup.git apss-build-setup &&
  cd apss-build-setup && ./setup.sh
"
```

The orchestrator generates `auto.conf` with correct MACHINE, DISTRO, and BSP source URLs. Customizations go in `local.conf` — see `yocto-build/build-alif-e7/conf/local.conf`.

### Build

```bash
docker exec -u apssbuilder alif-apss-build bash -c "
  cd /home/apssbuilder/apss-build-setup &&
  source layers/openembedded-core/oe-init-build-env \
    /home/apssbuilder/build-data/build-appkit-e7 &&
  bitbake alif-tiny-image
"
```

First build: ~40-60 min (no sstate cache). Incremental: ~5 min.

### Customizing local.conf

Copy our tracked config into the container after the orchestrator generates the build dir:

```bash
docker cp yocto-build/build-alif-e7/conf/local.conf \
  alif-apss-build:/home/apssbuilder/build-data/build-appkit-e7/conf/local.conf
```

Key customizations (see `yocto-build/build-alif-e7/conf/local.conf`):
- `BB_NUMBER_THREADS = "4"` — prevents OOM in macOS Docker
- `PARALLEL_MAKE = "-j 4"` — same reason
- Zeus syntax: use `_append`/`_remove` (NOT `:append`/`:remove`)

### Output Artifacts

| File | Description |
|------|-------------|
| `xipImage` | XIP Linux kernel (~2.1MB, runs from MRAM) |
| `bl32.bin` | TF-A BL32 (~30KB, Secure Payload) |
| `appkit-e7.dtb` | Device tree blob (~25KB) |
| `alif-tiny-image-appkit-e7.cramfs-xip` | Read-only root filesystem (~1.3MB) |

### Flashing

Flashing requires Alif's SETOOLS to generate an ATOC (Application Table of Contents) and write it + binary images to MRAM via the Secure Enclave's UART.

**Setup:** Download SETOOLS from [alifsemi.com](https://alifsemi.com/support/kits/ensemble-e7devkit/) and extract to `tools/setools/` (gitignored). See `setools/README.md` for full instructions.

**Quick flash:**
```bash
# Connect PRG_USB, then:
cd firmware/linux/alif-e7/setools
./flash-e7.sh              # Copy artifacts from Docker + generate ATOC + flash
```

**MCP workflow:**
```bash
alif-flash.gen_toc(config="build/config/linux-boot-e7.json")
alif-flash.flash(config="build/config/linux-boot-e7.json", maintenance=true)
# Power cycle (unplug/replug PRG_USB)
alif-flash.monitor(port=VCOM, baud=115200, duration=30)
```

**ATOC config:** `setools/linux-boot-e7.json` defines the memory map (tracked in git).

| Image | MRAM Address |
|-------|-------------|
| bl32.bin (TF-A) | 0x80002000 |
| appkit-e7.dtb | 0x80010000 |
| xipImage | 0x80020000 |
| cramfs-xip rootfs | 0x80300000 |

**Serial console:** Monitor boot on UART2 (J15 jumpers in UART2 position):
```bash
screen /dev/cu.usbmodem<SECOND_PORT> 115200
```

## App Cross-Compilation

### Docker Image

A lightweight Docker image with the system ARM cross-compiler (no Buildroot needed):

```bash
docker build -t alif-e7-sdk -f firmware/linux/docker/Dockerfile.alif-e7 .
```

### MCP Workflow

```
linux-build.start_container(name="alif-e7-build", image="alif-e7-sdk", workspace_dir="/path/to/work")
linux-build.build(container="alif-e7-build", command="make -C /workspace/firmware/linux/apps BOARD=alif-e7 all install")
linux-build.collect_artifacts(container="alif-e7-build", host_path="/tmp/alif-e7-apps")
```

### Manual Build

```bash
BOARD=alif-e7 make -C firmware/linux/apps
```

## ADB over USB

ADB provides zero-config developer access over USB: `adb shell`, `adb push/pull`, `adb forward`.

The E7 uses a DWC3 USB controller (vs DWC2 on STM32MP1). The meta-eai layer carries:
- Kernel config fragment (`usb-gadget-adb.cfg`) enabling DWC3 gadget mode + FunctionFS
- Board-aware gadget script that auto-detects the UDC and sets E7 product strings
- Machine-conditional DWC3 kernel module dependencies

### Host Setup

```bash
brew install android-platform-tools
adb devices    # Shows: eai-alif-e7-001    device
adb shell      # Root shell on board
```

## Comparison with STM32MP1

| | STM32MP1 | Alif E7 |
|---|---|---|
| CPU | Cortex-A7 (1x) | Cortex-A32 (2x SMP) |
| Boot media | SD card (WIC) | MRAM/OSPI (xipImage) |
| Bootloader | TF-A + U-Boot | TF-A + Secure Enclave |
| Flash tool | dd to SD | SETOOLS/ATOC |
| Cross-compiler | Buildroot toolchain | System `arm-linux-gnueabihf-gcc` |
| CPU flags | `-mcpu=cortex-a7 -mfpu=neon-vfpv4` | `-mcpu=cortex-a32 -mfpu=neon` |
| USB controller | DWC2 | DWC3 |
| Yocto machine | `stm32mp1` | `appkit-e7` |
| Yocto release | scarthgap | zeus (via orchestrator) |
| Docker image | `yocto-builder` | `apss/ubuntu-builder:v18.04` |
