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
4. Put build output in `apps/<app>/build/` (use `-d` flag)

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

| Board | Chip | target_chip | Use Case |
|-------|------|-------------|----------|
| nrf52840dk/nrf52840 | nRF52840 | nRF52840_xxAA | BLE development |
| esp32s3_eye/esp32s3/procpu | ESP32-S3 | ESP32-S3 | WiFi + BLE |
| esp32_devkitc/esp32/procpu | ESP32 | ESP32 | WiFi + BLE |
| nrf5340dk/nrf5340/cpuapp | nRF5340 | nRF5340_xxAA | BLE + net core |
| nrf54l15dk/nrf54l15/cpuapp | nRF54L15 | nrf54l15 | Crash debug, low-power BLE |
| native_sim | - | - | Unit testing |

## Shared Libraries

Each library has its own `CLAUDE.md` with full usage docs. Read it before using.

| Library | Purpose |
|---------|---------|
| `lib/crash_log/` | Boot-time coredump detection, shell commands (`crash check/info/dump/clear`), auto-report via RTT |
| `lib/device_shell/` | Board management shell commands (`board info/uptime/reset`) |
| `lib/debug_config/` | Shared Kconfig overlays for crash diagnostics (RTT-only and flash-backed variants) |

## Testing

Run all tests via twister:

```bash
python3 ../zephyr/scripts/twister -T tests -p qemu_cortex_m3 -v
```

| Test Suite | Platform | Tests |
|-----------|----------|-------|
| `libraries.crash_log` | qemu_cortex_m3 | 4 tests (clean boot API behavior) |
| `libraries.device_shell` | qemu_cortex_m3, native_sim | 3 tests (shell command output) |

## Structure

- `apps/` - Application projects
- `lib/` - Shared libraries (each with its own CLAUDE.md)
- `tests/` - Unit tests (run via twister on QEMU)
- `boards/` - Custom board definitions (future)
- `drivers/` - Custom drivers (future)
