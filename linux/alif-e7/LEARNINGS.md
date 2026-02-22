# Alif E7 AppKit Linux Learnings

Working notes from bring-up sessions. Verified items marked with checkmarks.

## CRITICAL: Always use J-Link for flashing images

**NEVER use `flash()` (SE-UART) for image updates. ALWAYS use `jlink_flash()` instead.**

- `jlink_flash`: ~44 KB/s, all 4 images in ~78 seconds
- `flash` (SE-UART): ~5 KB/s, all 4 images in ~19 minutes

The ONLY reason to use SE-UART `flash()` is for initial ATOC setup (first-ever flash) or if J-Link is physically unavailable. For all routine image flashing, use `jlink_flash`.

## Boot Chain Architecture

### Memory Map (MRAM)
| Component | Address | File | Status |
|-----------|---------|------|--------|
| TF-A (BL32/sp_min) | 0x80002000 | bl32.bin | Verified - boots, prints to VCOM |
| DTB | 0x80010000 | appkit-e7.dtb | Verified ✓ — 31853 bytes, compiled from source DTS |
| Kernel (XIP) | 0x80020000 | xipImage | Verified ✓ — boots, console output on VCOM |
| Rootfs (cramfs-xip) | 0x80300000 | alif-tiny-image-appkit-e7.cramfs-xip | Verified ✓ — "Linux Hello World" userspace running |

### TF-A → Kernel DTB Handoff
- TF-A is sp_min (BL32), built with `RESET_TO_SP_MIN=1`
- Build flag `ARM_LINUX_KERNEL_AS_BL33=1` enables DTB passing
- TF-A copies DTB: MRAM 0x80010000 → SRAM 0x02390000
- Passes to kernel via ARM boot protocol: r0=0, r1=~0 (DT boot), r2=0x02390000 (DTB pointer)
- Code: `alif_arm-tf/plat/arm/common/sp_min/arm_sp_min_setup.c` lines 102, 245-247

### DTB Console Configuration
- `appkit-e7-flatboard.dts` chosen node: `bootargs = "console=ttyS0,115200n8 ..."`
- ttyS0 = UART2 (only enabled UART in DTB — uart0 and uart1 are disabled)
- VCOM via J15 on UART2 position → console=ttyS0 goes to VCOM ✓

### Kernel Fallback Console
- `appkit_e7_defconfig` line 214: `CONFIG_CMDLINE="console=ttyS1"` (WiFi UART - WRONG for VCOM)
- `CONFIG_CMDLINE_FROM_BOOTLOADER=y` — uses DTB bootargs if provided, falls back to CONFIG_CMDLINE
- If TF-A fails to pass DTB → kernel uses ttyS1 → no output on VCOM

## Console Debug Progress

### DTB Handoff: VERIFIED ✓
- DTB magic (0xD00DFEED) confirmed at both 0x80010000 (MRAM) and 0x02390000 (SRAM copy)
- TF-A IS passing DTB to kernel via r2
- Bootargs in DTB: `console=ttyS0,115200n8 root=mtd:physmap-flash.0 rootfstype=cramfs ro loglevel=9`

### Key Finding: uart0 is DISABLED in DTB
- `uart0@49018000` has `status = "disabled"` in appkit-e7-flatboard.dts
- `uart1@49019000` has `status = "disabled"` (WiFi UART)
- `uart2@4901A000` has `status = "okay"` — the ONLY enabled UART
- No aliases node in DTB
- Kernel assigns ttyS numbers by probe order of **enabled** UARTs only
- Therefore: `ttyS0` = UART2 (0x4901A000), NOT UART0
- `console=ttyS0` in bootargs → kernel console goes to UART2

### Experiment: 115200 on VCOM (UART2 via J15) → 30 bytes garbled
- After JLink reset with J15 on UART2: 30 bytes garbled at 115200
- Zero bytes at 57600, 230400, 460800, 576000
- **Cause:** DTB `dwuartclk` had been corrupted to 20 MHz by dtc round-trip. Actual hardware clock IS 100 MHz. Kernel UART driver calculated wrong divisor (11 instead of 54) → ~568K actual baud instead of 115200.
- **Also:** `earlycon=uart8250,mmio32,0x49018000` points to UART0, not UART2. Earlycon output goes to wrong UART.

### DTB Clock Reference Chain
- UART nodes use `clocks = <0x12 0x03>` — phandle 0x12 = `dwuartclk`
- Two clock nodes: `uartclk` (20 MHz, phandle 0x0F) and `dwuartclk` (100 MHz, phandle 0x12)
- `clock-names = "baudclk", "apb_pclk"` — first clock (0x12) is baudclk, second (0x03) is apb_pclk
- Kernel `dw8250` driver uses baudclk for divisor calculation → must match actual hardware frequency

### Console Working: VERIFIED ✓
- **Root cause:** Previous DTB was decompiled/recompiled via `dtc`, corrupting `dwuartclk` to 20 MHz
- **Fix:** Compiled DTB from original source DTS (`appkit-e7-flatboard.dts`) using Zephyr SDK `arm-zephyr-eabi-cpp` + `dtc`
- Source DTS has correct `dwuartclk = 100 MHz` (0x5F5E100) — matches hardware and TF-A (`UART_CLOCK_FREQ = 100000000`)
- Added `earlycon=uart8250,mmio32,0x4901a000,115200n8` to bootargs for early kernel output
- DTB size changed (25160 → 31853 bytes) — required ATOC regeneration + SE-UART flash
- **Result:** "Linux Hello World" userspace output on VCOM at 115200 ✓

### DTB Compilation (from source — correct method)
```bash
CPP=~/zephyr-sdk-0.17.4/arm-zephyr-eabi/bin/arm-zephyr-eabi-cpp
DTS_DIR=alif_linux/arch/arm/boot/dts
INCLUDE_DIR=alif_linux/include
$CPP -nostdinc -I${INCLUDE_DIR} -I${DTS_DIR} -undef -D__DTS__ -x assembler-with-cpp \
  ${DTS_DIR}/appkit-e7-flatboard.dts -o /tmp/appkit-e7-pp.dts
dtc -I dts -O dtb -o /tmp/appkit-e7.dtb /tmp/appkit-e7-pp.dts
```
- **NEVER decompile/recompile a DTB via `dtc -I dtb -O dts | dtc -I dts -O dtb`** — round-trip corrupts clock values and other data
- Always compile from the original `.dts` source with proper preprocessing

## Build System

### Yocto Setup
- Orchestrator: `alifsemi/alif_linux-apss-build-setup`
- OE version: zeus (Yocto 3.0) — uses `_append`/`_remove` syntax, NOT `:append`/`:remove`
- Docker image: `apss/ubuntu-builder:v18.04`
- MACHINE: `appkit-e7` (on `devkit-ex-b0` branch ONLY)
- DISTRO: `apss-tiny` (minimal musl + busybox, ~1.3MB cramfs-xip)

### Machine Config
- `devkit-e7.conf.orig` in meta-alif-ensemble has TF-A build flags
- No `appkit-e7.conf` exists in meta-alif-ensemble on devkit-ex-b0 branch
- Must be created/provided separately

### TF-A Build Flags (from devkit-e7.conf.orig)
```
ARM_LINUX_KERNEL_AS_BL33=1
PRELOADED_BL33_BASE=0x80020000
ARM_PRELOADED_DTB_BASE=0x80010000
RAM_PRELOADED_DTB_BASE=0x02390000
RESET_TO_SP_MIN=1
ENABLE_PIE=1
TRUSTED_SRAM1=0x08000000
UART=2
HYPRAM_EN=1
```

### Kernel
- Source: `alif_linux/` branch `devkit-b0-5.4.y`, kernel 5.4.25
- Defconfig: `arch/arm/configs/appkit_e7_defconfig`
- DTS: `arch/arm/boot/dts/appkit-e7-flatboard.dts` (NOT appkit-e7.dts or devkit-e7.dts)
- XIP kernel at 0x80020000, SRAM at 0x02000000

### Flashing

Two proven methods, each with different tradeoffs:

#### J-Link loadbin (Fast — images only)
- **Speed:** ~44 KB/s, all 4 images in ~78 seconds
- **Device:** `AE722F80F55D5_M55_HP` with custom `AlifE7.JLinkScript` (prevents reset)
- **Interface:** SWD at 4000 kHz
- **MCP tool:** `jlink_flash` (via alif-flash MCP) or raw JLinkExe
- **Setup:** `jlink_setup` installs device definition to `~/Library/Application Support/SEGGER/JLinkDevices/AlifSemi/`
- **Quirk:** Only `.bin` extension accepted — `.dtb` gets "unsupported format". Copies to temp `.bin` automatically.
- **Quirk:** "Failed to halt CPU" errors between downloads are harmless
- **Quirk:** Must use `-NoGui 1` flag or JLinkExe opens a GUI probe selector
- **Limitation:** Writes images only, NOT the ATOC. Use SE-UART for ATOC first.
- **Probe select:** `-USB <serial>` when multiple probes (e.g., `-USB 1223000022` for external J-Trace PRO)
- **Config:** `firmware/linux/alif-e7/setools/linux-boot-e7.json`

#### SE-UART ISP (Slow — needed for ATOC)
- **Speed:** ~5 KB/s at 57600 baud. Full flash (ATOC + 4 images) takes ~13.5 minutes.
- **Port:** FTDI adapter appears as `/dev/cu.usbserial-*` on macOS
- **MCP tool:** `flash` (via alif-flash MCP) or proprietary `app-write-mram`
- **Requires maintenance mode:** SET_MAINTENANCE + reset + reconnect before MRAM writes
- **USB stability:** Large writes can cause FTDI USB drops. MCP handles reconnect-on-drop.
- **Config:** `firmware/linux/alif-e7/setools/linux-boot-e7.json`

#### Flash Tool Image Path Resolution
- `flash()` MCP tool resolves image binaries from `<config_dir>/../images/`
- For config at `firmware/linux/alif-e7/setools/linux-boot-e7.json`, images come from `firmware/linux/alif-e7/images/`
- NOT from the setools directory itself — put images in the `images/` sibling directory

#### ATOC (Application Table of Contents)
- Generated by `app-gen-toc` from JSON config
- Contains image metadata: sizes, addresses, signatures
- **Must regenerate after any image size change** — SE validates at boot, size mismatch = rejection
- Flash ATOC via SE-UART ISP, then use JLink for fast image iteration

#### Post-Flash
- **Power cycle** (unplug/replug PRG_USB) required after SETOOLS flash for SE boot sequence
- **J-Link reset** of A32 core triggers TF-A boot only (skips SE) — useful for fast iteration

## Hardware Setup

### Probes
- **Onboard J-Link OB** (S/N: 1219307699): Provides VCOM serial + SWD to M55_HP. ISP flash and console.
- **External J-Trace PRO** (S/N: 1223000022): Connected to JTAG0 header. A32 debug, JLink flash, reset. Select with `-USB 1223000022`.

### Serial Ports (macOS) — Fixed Setup, No Jumper Switching
| Port pattern | Device | UART | Use | Baud |
|---|---|---|---|---|
| `/dev/cu.usbserial-*` | External FTDI adapter | SE-UART | ISP flash protocol | 57600 |
| `/dev/cu.usbmodem*` | Onboard JLink VCOM | UART2 | Linux console + TF-A | 115200 |

- J15 stays on UART2 position (pins 5-7/6-8) permanently
- FTDI handles SE-UART for ISP flashing
- VCOM handles console output — no jumper switching needed
- Auto-detect: prefer `usbserial` (FTDI) for ISP operations
- **JLink VCOM caveat:** Only produces output during active JLink session. Power cycle without JLink connected = no VCOM output.

### UART Mapping
- UART0 @ 0x49018000 — **disabled in DTB**, TF-A uses it directly via register writes
- UART1 @ 0x49019000 — **disabled in DTB**, WiFi UART (not accessible on headers)
- UART2 @ 0x4901A000 = ttyS0 — **only enabled UART**, Linux console, VCOM via J15
- TF-A prints on UART2 (visible on VCOM when J15 is UART2 position) ✓

## Failed Experiments

### DTB bootargs changes (no effect on kernel console)
- Changed `console=ttyS0` → `earlycon=uart8250,mmio32,0x4901a000 console=ttyS2` — no kernel output
- Changed to `earlycon=uart8250,mmio32,0x49018000 console=ttyS0` — no kernel output
- Changed `dwuartclk` from 20 MHz to 100 MHz — made it WORSE (garbled at 115200)
- **Why these failed:** Multiple compounding issues — earlycon pointed to wrong UART (UART0 instead of UART2), J15 was in wrong position, clock mismatch broke baud rate. Each change fixed one thing but not the others.
- **Lesson:** When debugging console output, verify ALL of: (1) which UART is enabled in DTB, (2) which UART J15 routes to VCOM, (3) earlycon address matches the VCOM UART, (4) clock frequency matches hardware.

### ATOC size mismatch after DTB edit
- Modified DTB (25120 → 25160 bytes) but didn't regenerate ATOC
- SE boot log showed DTB size mismatch — SE validates image sizes against ATOC metadata
- Fixed by running `gen_toc` to regenerate ATOC, then reflashing both
- **Lesson:** Any image size change requires ATOC regeneration.

### Monitoring VCOM after power cycle (no output)
- After power cycle (unplug/replug PRG_USB), VCOM shows nothing
- JLink VCOM requires an active JLink debug session to relay UART data
- Only works: connect JLink → reset A32 → read VCOM while JLink is connected
- **Lesson:** JLink VCOM is not a passive serial port. No JLink session = no output.

### Alternate baud rates on VCOM
- Tried 460800 on VCOM — got ~81% readable characters (garbled)
- **Lesson (corrected):** UART peripheral clock IS 100 MHz (confirmed by TF-A source, hardware divisor register, and working console). The 20 MHz value was a dtc round-trip corruption artifact. Always compile DTB from source.

### SE boot log findings
- All images loaded: TFA(30136), DTB(31853), KERNEL(2161568), ROOTFS(1363968)
- `[ERROR] Pin mux failed for [port:15, pin:0..7]` — 8 pin mux errors during SE device config (GPIOV/joystick, NOT UART2)
- SE frequency confirmed 100.03 MHz
- UART2 hardware path verified: wrote to UART2 THR via JLink → appeared on VCOM at 115200 ✓
- UART2 divisor = 54 → 100MHz / (16*54) = 115741 ≈ 115200 (correct) ✓
- Pin mux errors are unrelated to UART2 — port 15 = GPIOV (joystick/LCD), UART2 uses P1_0/P1_1 (port 1)
- **Console works** after compiling DTB from source with correct 100 MHz dwuartclk ✓

### Serial FD leak from background cat commands
- Backgrounding `cat /dev/cu.usbmodem*` in bash leaks FDs to the parent process
- Parent (claude) inherits read FDs that consume all UART data silently
- Subsequent reads get 0 bytes because leaked FDs drain the buffer first
- **Cannot close from child shell** — FDs belong to parent process
- **Fix:** Replug USB to reset port, or restart Claude process
- **Prevention:** Never use `cat` on serial ports in background. Use the MCP `monitor` tool instead.

### JLink MRAM writes fail after SE boot
- `loadbin` via JLinkExe → "Writing target memory failed" + "CPU could not be halted"
- Happens on BOTH probes (onboard JLink OB and external J-Trace PRO)
- Reading MRAM via JLink works fine (`savebin`, `mem32`)
- **Cause:** SE configures MRAM firewall after boot, blocking writes through M55 debug port
- Previously worked because board was in different state (pre-SE-boot or maintenance mode)
- **Workaround:** Use ISP protocol (maintenance mode) for MRAM writes, or flash before SE completes boot
- **TODO:** Investigate if JLink writes work immediately after power cycle (race condition with SE boot)

## Key Files
| File | Purpose |
|------|---------|
| `alif_arm-tf/plat/arm/board/devkit_e7/include/platform_def.h` | TF-A platform constants |
| `alif_arm-tf/plat/arm/board/devkit_e7/platform.mk` | TF-A build config |
| `alif_arm-tf/plat/arm/common/sp_min/arm_sp_min_setup.c` | DTB copy + kernel handoff |
| `alif_linux/arch/arm/configs/appkit_e7_defconfig` | Kernel config |
| `alif_linux/arch/arm/boot/dts/appkit-e7-flatboard.dts` | Device tree source |
| `firmware/linux/alif-e7/setools/linux-boot-e7.json` | Flash layout config |
| `yocto-build/meta-alif-ensemble/conf/machine/devkit-e7.conf.orig` | Machine config template |
| `yocto-build/meta-alif-ensemble/recipes-bsp/trusted-firmware-a/trusted-firmware-a.bb` | TF-A recipe |
