# Zephyr Applications

## Development Guidelines

**IMPORTANT:** When working on code in this repository, follow the embedded development skill:

```
/embedded
```

This loads guidelines for memory management, error handling, code organization, Zephyr patterns, and MCP tool usage.

## Available Apps

| App | Board | Description |
|-----|-------|-------------|
| ble_data_transfer | nrf52840dk/nrf52840 | Simple BLE NUS echo server - receives data, echoes back with "Echo: " prefix |
| ble_wifi_bridge | esp32s3_eye/esp32s3/procpu | BLE-to-TCP bridge - forwards BLE data over WiFi to a TCP server |
| crash_debug | nrf54l15dk/nrf54l15/cpuapp | Crash debug demo - intentional HardFault with coredump analysis via MCP |
| wifi_provision | nrf7002dk/nrf5340/cpuapp | WiFi provisioning over BLE with TCP throughput server |

## Building Apps

ALWAYS use the zephyr-build MCP instead of running west directly:

```
zephyr-build.build(app="ble_data_transfer", board="nrf52840dk/nrf52840", pristine=true)
zephyr-build.build(app="ble_wifi_bridge", board="esp32s3_eye/esp32s3/procpu", pristine=true)
zephyr-build.build(app="crash_debug", board="nrf54l15dk/nrf54l15/cpuapp", pristine=true)
```

## Shared Component Patterns

### Reusable Modules (from ble_wifi_bridge)

These patterns should be extracted and reused when building new apps:

1. **ble_nus** - BLE Nordic UART Service abstraction
   - Callback-driven: `ble_nus_rx_cb_t` fires on data received
   - Clean init/send API: `ble_nus_init(callback)`, `ble_nus_send(data, len)`

2. **bridge** - Message queue data forwarding
   - Uses `K_MSGQ_DEFINE` for thread-safe queuing
   - Bidirectional forwarding: BLE-to-TCP and TCP-to-BLE queues
   - Dedicated processing thread with 10ms polling

3. **tcp_socket** - TCP client wrapper
   - Connect/send/receive with callbacks
   - `tcp_rx_cb_t` for received data notification

### Module Pattern

When creating new modules, follow this pattern:

```c
// module.h
typedef void (*module_event_cb_t)(const uint8_t *data, size_t len);
int module_init(module_event_cb_t callback);
int module_send(const uint8_t *data, size_t len);

// module.c
#include "module.h"
LOG_MODULE_REGISTER(module, LOG_LEVEL_INF);

static module_event_cb_t event_callback;
static K_MSGQ_DEFINE(module_queue, sizeof(struct msg), 10, 4);

int module_init(module_event_cb_t callback) {
    event_callback = callback;
    // ... setup
    return 0;
}
```

Key principles:
- Callback-driven events (type-safe function pointers)
- Static allocation only (`K_MSGQ_DEFINE`, `K_THREAD_STACK_DEFINE`)
- Each module manages its own state
- Follow naming: `module_action_thing()`
- Return negative errno values on error (-ENOTCONN, -EMSGSIZE, -ENOMEM)

### Creating New Apps

1. **Modular apps** - Copy structure from `ble_wifi_bridge` (CMakeLists.txt with multiple source files, Kconfig for parameters)
2. **Simple apps** - Copy from `ble_data_transfer` (single main.c)
3. Always include: `CMakeLists.txt`, `prj.conf`, `Kconfig` (if configurable)
4. Build output goes to `apps/<app>/build/` automatically (zephyr-build MCP passes `-d`)

## Environment

**Always activate the virtual environment before running commands:**

```bash
source .venv/bin/activate
source ../zephyr/zephyr-env.sh
```

## ESP32 Prerequisites

```bash
pip install "esptool>=5.0.2"
west blobs fetch hal_espressif
```

## Board Configurations

See workspace `CLAUDE.md` for full board/chip/target_chip mappings. App-specific boards are listed in the Available Apps table above.

## Shared Libraries

Each library has its own `CLAUDE.md` with full usage docs. Read it before using.

| Library | Purpose |
|---------|---------|
| `lib/crash_log/` | Boot-time coredump detection, shell commands (`crash check/info/dump/clear`), auto-report via RTT. Config overlays in `conf/`. |
| `lib/device_shell/` | Board management shell commands (`board info/uptime/reset`) |
| `lib/eai_osal/` | OS abstraction layer — portable mutex, semaphore, thread, queue, timer, event, critical section, time primitives. Zephyr backend. |
| `lib/wifi_prov/` | WiFi provisioning over BLE — custom GATT service, WiFi scan/connect, credential persistence, state machine. See `lib/wifi_prov/CLAUDE.md`. |

## Testing

Run all tests via twister (Docker — reproducible, matches CI):

```bash
make test
```

Or via local west (requires Zephyr SDK installed):

```bash
python3 ../zephyr/scripts/twister -T lib -p qemu_cortex_m3 -O ../.cache/twister -v
```

## Docker Builds

The `Makefile` wraps Docker-based builds using the same Zephyr CI container as GitHub Actions (`ghcr.io/zephyrproject-rtos/ci:v0.28.7`). Same SDK + toolchain = identical binaries regardless of host.

```bash
make build APP=crash_debug BOARD=nrf54l15dk/nrf54l15/cpuapp   # Build in container
make test                                                       # Twister on QEMU
make shell                                                      # Interactive container
make pull                                                       # Update container image
```

Artifacts land in `apps/<app>/build/<board>/zephyr/zephyr.{elf,hex}`. Flash with MCP tools natively.

| Test Suite | Platform | Tests |
|-----------|----------|-------|
| `libraries.crash_log` | qemu_cortex_m3 | 4 tests (clean boot API behavior) |
| `libraries.device_shell` | qemu_cortex_m3, native_sim | 3 tests (shell command output) |
| `libraries.eai_osal` | qemu_cortex_m3 | 44 tests (all OSAL primitives + work queues) |
| `libraries.wifi_prov` | qemu_cortex_m3 | 22 tests (credentials, message encode/decode, state machine) |

## Addons

Composable code generation modules in `addons/`. Specified via `create_app(libraries=["ble", "wifi"])` — each addon injects Kconfig, includes, globals, and init code into the generated app.

| Addon | Description |
|-------|-------------|
| `ble` | BLE peripheral with NUS — advertising, connection callbacks, NUS echo |
| `wifi` | WiFi station with DHCP — net_mgmt events, connect/disconnect handlers |
| `tcp` | TCP client socket — connect, send, receive with dedicated RX thread (depends: wifi) |

Addons vs libraries: **Libraries** (`lib/<name>/manifest.yml`) inject overlay config into CMakeLists.txt. **Addons** (`addons/<name>.yml`) generate code directly into main.c and prj.conf. Both are specified in the `libraries` parameter of `create_app`.

## Structure

- `apps/` - Application projects
- `addons/` - Composable code generation addons (YAML — BLE, WiFi, TCP)
- `lib/` - Shared libraries (each with its own CLAUDE.md)
- `lib/<name>/tests/` - Unit tests co-located with libraries (run via twister on QEMU)
- `boards/` - Custom board definitions (future)
- `drivers/` - Custom drivers (future)
