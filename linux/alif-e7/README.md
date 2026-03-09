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

### Fast OSPI Flashing via USB (~5 min)

Programs OSPI flash at ~47 KB/s via USB XMODEM using the `ospi_program_usb` MCP tool.
Compared to JLink OSPI flash loader (~3 KB/s, ~40 min) or SE-UART ISP (~45 min).

#### Hardware Requirements

- **J1 (PRG_USB)** — FTDI debug USB, always connected (SE-UART + UART2 console)
- **J2 (SoC USB, Micro-B)** — Alif DWC3 USB, connects CDC-ACM for XMODEM data transfer
- **JLink probe** — already connected via SWD on the AppKit board

Both J1 and J2 must be connected to your Mac.

#### Firmware Prerequisites (build once, rebuild when source changes)

Two firmware binaries are needed. Neither is included in the MCP — they're built from source.

**1. bl32-usbinit.bin** — TF-A variant that enables USB PHY then parks A32:

```bash
cd firmware/linux/alif-e7
./build-tfa.sh --usb-init     # → tools/setools/build/images/bl32-usbinit.bin (~30KB, ~5 sec)
```

Or build both variants at once:
```bash
./build-tfa.sh --both         # builds bl32-ospi.bin + bl32-usbinit.bin
```

Requires: `tfa-build` Docker container with `arm-linux-gnueabihf-gcc`.
The only difference from the normal TF-A is `USB_INIT_HALT=1`: after enabling USB clocks and calling SE AIPM to power the USB PHY, A32 parks in `wfe` instead of jumping to the kernel.

**2. flasher-hp.bin** — M55_HP XMODEM-to-OSPI receiver:

```bash
# Source: alifsemi/alif_usb-to-ospi-flasher (GitHub)
cd /path/to/alif_usb-to-ospi-flasher

# Install CMSIS Toolbox (one-time — bundled in repo)
export PATH="$(pwd)/cmsis-toolbox-darwin-arm64/bin:$PATH"

# Install ARM GNU Toolchain 14.2+ and set env
export GCC_TOOLCHAIN_14_2_1=/path/to/arm-gnu-toolchain-14.2/bin

# Build
cbuild alif.csolution.yml --context flasher.release+DevKit-E7-HP --toolchain GCC

# Stage to setools
cp out/flasher/DevKit-E7-HP/release/flasher.bin \
   /path/to/work/tools/setools/build/images/flasher-hp.bin
```

Requires CMSIS packs (auto-installed by cbuild):
- `AlifSemiconductor::Ensemble@2.1.0`
- `AlifSemiconductor::ThreadX@2.0.0`
- `ARM::CMSIS@6.1.0`

**3. ATOC config** — `build/config/linux-boot-e7-ospi-usbflash.json` (already in setools, no build needed).

#### End-to-End Workflow (MCP tools)

```
# Step 1: Generate programming mode ATOC (~1 sec)
alif-flash.gen_toc(config="build/config/linux-boot-e7-ospi-usbflash.json")

# Step 2: Flash programming mode to MRAM (~1 sec)
#   Writes: ATOC + bl32-usbinit.bin (A32) + flasher-hp.bin (M55_HP)
#   Triggers JLink NSRST reset → SE processes ATOC → both cores boot
#   A32 enables USB PHY, M55_HP starts XMODEM receiver
#   /dev/cu.usbmodem12001 (VID 0x0525) appears after ~3 sec
alif-flash.jlink_flash(config="build/config/linux-boot-e7-ospi-usbflash.json")

# Step 3: Transfer OSPI image via USB XMODEM (~4.3 min for 12MB)
#   Auto-detects Alif CDC-ACM device (VID 0x0525)
#   Returns structured result with bytes_sent, speed, flasher_message
alif-flash.ospi_program_usb(image="/path/to/ospi-combined.bin")

# Step 4: Restore normal boot ATOC (~2 sec)
alif-flash.gen_toc(config="build/config/linux-boot-e7-mram.json")
alif-flash.jlink_flash(config="build/config/linux-boot-e7-mram.json")

# Step 5: Power cycle → Linux boots from OSPI
```

**CRITICAL: Always `gen_toc` before `jlink_flash` when switching configs.**
AppTocPackage.bin is a shared file — without regenerating it, `jlink_flash` writes the wrong ATOC and the SE boots the wrong firmware.

#### Architecture: Why Two Cores?

The flasher uses a split architecture to work around an SE deadlock:

| Core | Firmware | Role |
|------|----------|------|
| A32 (Cortex-A32) | `bl32-usbinit.bin` | Calls SE AIPM to enable USB PHY power domain, then parks in WFE |
| M55_HP (Cortex-M55) | `flasher-hp.bin` | Direct register writes for USB clocks, DWC3 CDC-ACM init, XMODEM receiver, OSPI programmer |

**Why not single-core?** The SE AIPM service (`SERVICES_set_run_cfg(USB_PHY_MASK)`) works when called from A32's MHU channel but **deadlocks the SE when called from M55_HP's MHU channel**. There is no recovery from this deadlock except a hard maintenance erase + power cycle. The split architecture avoids this entirely.

#### Combined OSPI Image Layout

The `ospi-combined.bin` file concatenates rootfs (padded to 8MB) + kernel:

| Offset | Size | OSPI Address | XIP Address | Content |
|--------|------|-------------|-------------|---------|
| 0x000000 | 8MB | 0x00000000 | 0xC0000000 | rootfs (cramfs-xip, padded to 8MB) |
| 0x800000 | ~3.7MB | 0x00800000 | 0xC0800000 | kernel (xipImage) |

Build it with:
```bash
# Pad rootfs to exactly 8MB, append kernel
dd if=rootfs-ospi.bin of=ospi-combined.bin bs=1M count=8 conv=sync
cat xipImage-ospi.bin >> ospi-combined.bin
```

#### Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| No `/dev/cu.usbmodem*` (VID 0x0525) after flash | M55_HP flasher didn't boot, or J2 cable not connected | Check J2 cable. Check SE-UART for boot errors. Verify gen_toc was run for usbflash config. |
| `ospi_program_usb` picks wrong device | J-Link VCOM (VID 0x1366) detected instead of Alif | Specify `device="/dev/cu.usbmodem12001"` explicitly |
| "No response from receiver" timeout | Flasher not ready or wrong serial port | Wait 5 sec after jlink_flash for CDC-ACM to enumerate |
| Board won't boot after restore | Stale AppTocPackage.bin | Run `gen_toc` with normal boot config BEFORE `jlink_flash` |
| Firewall exceptions on SE-UART after restore | Residual hardware state from flasher | Full power cycle (unplug + replug), not just NSRST reset |
| SE deadlock (ISP unresponsive, JLink can't access cores) | M55_HP called SERVICES_set_run_cfg | Maintenance erase with native Alif tool + power cycle |

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
