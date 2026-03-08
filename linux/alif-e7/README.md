# Linux for Alif Ensemble E7 AppKit

Yocto-based Linux image and cross-compiled userspace apps for the Alif E7 Cortex-A32 cores.

## Architecture

The E7 has a heterogeneous architecture:
- **2x Cortex-A32** — Linux (this directory)
- **2x Cortex-M55** — Zephyr RTOS
- **Ethos-U55** — NPU for ML inference

Boot chain: Secure Enclave → TF-A (sp_min) → xipImage (no U-Boot). Kernel runs XIP from OSPI.

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

| File | Description | Size |
|------|-------------|------|
| `bl32-ospi.bin` | TF-A BL32 (built from source, see below) | ~30KB |
| `devkit-e7-ospi.dtb` | Device tree blob (hand-patched, see below) | ~33KB |
| `xipImage-ospi` | XIP Linux kernel (from Yocto) | ~3.7MB |
| `rootfs-ospi.bin` | cramfs-xip root filesystem (from Yocto) | ~5MB |
| `m55_stub_hp.bin` | M55 stub (prebuilt, keeps M55_HP alive for JLink) | ~4KB |

## TF-A Build (from source)

TF-A is built from the `alif_arm-tf` repo using the `tfa-build` Docker container. The build takes ~5 seconds and should be done fresh every time rather than relying on prebuilt copies.

```bash
./build-tfa.sh          # clean build + stage + verify (~5 sec)
./build-tfa.sh --no-clean  # incremental build
```

**Critical build flags** (omitting any produces a broken/different binary):
- `ENABLE_PIE=1` — required for E7 memory layout
- `ENABLE_STACK_PROTECTOR=strong` — produces ~30KB binary (without: ~26KB, untested)
- `HYPRAM_EN=1` + `FLASH_EN=1` — initializes HyperRAM and OSPI hardware
- `PRELOADED_BL33_BASE=0xC0800000` — kernel XIP address in OSPI
- `ARM_PRELOADED_DTB_BASE=0x80200000` — DTB location in MRAM (SE REV_B4 minimum)
- `UART=2` — console output on UART2

**Verification**: The binary MUST contain the string "USB clocks enabled" (added in commit `56dfb6fd`). Without USB clock enabling, TF-A boots normally but the kernel silently fails to start. The `build-tfa.sh` script checks this automatically.

**Container setup** (one-time):
```bash
docker run -dit --name tfa-build \
  -v $(pwd)/alif_arm-tf:/workspace \
  alif-e7-sdk:latest tail -f /dev/null
```

## DTB

The DTB is maintained as a hand-patched binary (`devkit-e7-ospi.dtb`), not built from Yocto. This is because:
1. The upstream `devkit-e7-ospi.dts` needs UART2 fixes (remove pinctrl deps, add clock-frequency)
2. Yocto's DCT tool can silently disable HyperRAM depending on build variable state
3. The DTB patches are small and well-documented

**Current patches** (applied to base `devkit-e7-ospi.dtb` via `fdtput`):
1. `aliases/serial0` → UART2 (`/apb@49010000/serial@4901a000`)
2. `chosen/bootargs` → earlycon (no baud rate!), console, cramfs root from OSPI
3. UART2 node: removed `clocks`, `clock-names`, `pinctrl-0`, `pinctrl-names`; added `clock-frequency = <100000000>`

**Known-working DTB**: md5 `caea9c2cc0cda2ce3983647619972219` (33,549 bytes)

### Flashing

Two-step flash via JLink (MRAM direct write + OSPI flash loader):

```bash
# Stage all artifacts (builds TF-A, copies kernel/rootfs from Yocto container)
./stage-ospi.sh

# Flash MRAM (TF-A + DTB + M55 stub) — ~1 sec
alif-flash.jlink_flash(config="linux-boot-e7-mram.json", verify=true)

# Flash OSPI (kernel + rootfs) — ~10 min via JLink FLM
alif-flash.jlink_flash(config="linux-boot-e7-ospi-jlink.json", verify=true)
```

**Memory map (OSPI boot):**

| Image | Address | Memory | Config |
|-------|---------|--------|--------|
| TF-A (bl32-ospi.bin) | 0x80002000 | MRAM | linux-boot-e7-mram.json |
| DTB (devkit-e7-ospi.dtb) | 0x80200000 | MRAM | linux-boot-e7-mram.json |
| M55 stub | 0x50000000 | MRAM | linux-boot-e7-mram.json |
| RootFS (cramfs) | 0xC0000000 | OSPI | linux-boot-e7-ospi-jlink.json |
| Kernel (xipImage) | 0xC0800000 | OSPI | linux-boot-e7-ospi-jlink.json |

**Serial console:** UART2 via FTDI at 115200 baud:
```bash
screen /dev/cu.usbserial-BG03TY04 115200
```

### Fast OSPI Flashing via USB

Programs OSPI flash directly over USB at ~60 KB/s using an M55 flasher firmware and XMODEM protocol. ~7 min total vs ~40+ min for the 3-pass SE-UART approach.

**Prerequisites:**
- pyserial: `pip install pyserial`
- ARM GNU Toolchain (for building flasher firmware, one-time)

**Workflow:**
```bash
# 1. Stage Yocto artifacts
./stage-ospi.sh

# 2. Build combined OSPI image (rootfs padded to 8MB + kernel)
./make-ospi-image.sh

# 3. Flash programming mode ATOC (M55 flasher, no TFA)
alif-flash.gen_toc(config="linux-boot-e7-ospi-usbflash.json")
alif-flash.flash(wait_for_power_cycle=true)
# Power cycle (unplug/replug PRG_USB)

# 4. Send combined image via USB XMODEM (~3 min)
./flash-ospi-usb.sh

# 5. Restore normal boot ATOC (TFA + DTB, kernel/rootfs from OSPI)
alif-flash.gen_toc(config="linux-boot-e7-ospi.json")
alif-flash.flash(wait_for_power_cycle=true)
# Power cycle → Linux boots from new OSPI contents
```

**Building the flasher firmware** (one-time):
```bash
# Source: /path/to/alif_usb-to-ospi-flasher (alifsemi GitHub)
# Requires: ARM GNU Toolchain 14.2+, CMSIS Toolbox 2.12.0
# Requires packs: AlifSemiconductor::Ensemble@2.1.0, ThreadX@2.0.0, ARM::CMSIS@6.1.0
export GCC_TOOLCHAIN_14_2_1=/path/to/arm-gnu-toolchain/bin
cbuild alif.csolution.yml --context flasher.release+DevKit-E7-HP --toolchain GCC
cp out/flasher/DevKit-E7-HP/release/flasher.bin setools/images/flasher-hp.bin
```

**OSPI memory layout:**

| Region | OSPI Address | XIP Address | Content |
|--------|-------------|-------------|---------|
| 0x000000–0x7FFFFF | 0x00000000 | 0xC0000000 | rootfs (cramfs-xip, 8MB partition) |
| 0x800000+ | 0x00800000 | 0xC0800000 | kernel (xipImage) |

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
