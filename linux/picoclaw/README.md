# PicoClaw Cross-Compilation for Embedded Targets

Cross-compile [PicoClaw](https://github.com/sipeed/picoclaw) (Go-based AI assistant gateway) for ARM Linux boards in the OpenClaw Dev Kit family.

## Quick Start

```bash
# Clone PicoClaw source (one-time)
git clone https://github.com/sipeed/picoclaw ../../picoclaw

# Build for STM32MP1 (Cortex-A7) or Alif E7 (Cortex-A32)
./build-picoclaw.sh armv7

# Build for RPi 4/5 or RK3576
./build-picoclaw.sh arm64

# Build both
./build-picoclaw.sh all
```

Output: `build/picoclaw-linux-{armv7,arm64}`

## Installing into Yocto Image

Copy the binary into the Yocto recipe's files directory:

```bash
cp build/picoclaw-linux-armv7 ../yocto/meta-eai/recipes-apps/picoclaw/files/picoclaw
```

Then rebuild the Yocto image — the `picoclaw-bin` recipe packages it.

## Docker Build (CI)

```bash
cd ../docker
docker build -t picoclaw-builder -f Dockerfile.picoclaw .
docker run --rm -v $(pwd)/../../picoclaw:/src -v $(pwd)/../picoclaw/build:/out picoclaw-builder armv7
```

## Binary Characteristics

| Arch | Size | Target Boards |
|------|------|---------------|
| armv7 (GOARCH=arm, GOARM=7) | ~15MB | STM32MP1-DK1, Alif E7 AppKit |
| arm64 (GOARCH=arm64) | ~16MB | RPi 4/5, RK3576 |

- Statically linked (CGO_ENABLED=0)
- No runtime dependencies — runs on musl or glibc
- Stripped (`-s -w` ldflags)

## Board-Specific Notes

### STM32MP1 (Tier 4 "Mini")
- 512MB DDR3L — plenty of room
- Binary goes in SD card rootfs via Yocto `picoclaw-bin` recipe
- Starts at boot via SysVinit init script

### Alif E7 (Tier 3 "Edge")
- **MRAM rootfs is ~5.7MB** — PicoClaw (15MB) does NOT fit
- Binary must be stored on external storage and loaded into 64MB OSPI PSRAM at runtime
- Options: ADB push to tmpfs, external SPI flash, or network download
- See constraints analysis in `plans/openclaw-dev-kit.md`

### Raspberry Pi (Tier 5 "Dev")
- Use arm64 build for 64-bit Raspberry Pi OS
- Or install full OpenClaw (Node.js) instead — RPi has plenty of resources

## Configuration

PicoClaw looks for config at `~/.picoclaw/config.json` (or path specified with `-c`).
The Yocto recipe installs a template to `/etc/picoclaw/config.json.default`.

Minimum config for first run:
1. Copy template: `cp /etc/picoclaw/config.json.default /etc/picoclaw/config.json`
2. Set API key for your LLM provider (Anthropic, OpenAI, etc.)
3. Enable at least one channel (Telegram, Discord, Slack)
4. Start: `/etc/init.d/picoclaw start`
