# crash_log

Zephyr module for boot-time coredump detection and shell-based crash management. Works with Zephyr's flash-backed coredump subsystem to detect crashes that happened in previous runs.

## What It Does

1. **Boot-time check** (`SYS_INIT`, priority 99): Checks flash for a stored coredump. If found, automatically emits it as `#CD:` hex lines via `LOG_ERR` so the `analyze_coredump` MCP tool can parse it from RTT output.
2. **Shell commands**: `crash check/info/dump/clear` for interactive crash management over RTT.
3. **C API**: `crash_log_has_coredump()`, `crash_log_emit()`, `crash_log_clear()` for programmatic access.

## When to Use

Use this when an app might crash **after boot** (not just during startup). Without crash_log, a coredump emitted via the logging backend at crash time may be lost if no one is watching RTT. With crash_log + the flash backend, the coredump is stored to flash and automatically re-emitted on the next boot.

## How to Include

### 1. Register as a Zephyr module in your app's CMakeLists.txt

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/crash_log
)
```

### 2. Include the flash coredump overlay

```cmake
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/debug_config/debug_coredump_flash.conf")
```

This overlay enables: coredump flash backend, crash_log module, RTT logging, shell, and debug build settings.

### 3. Add a DTS overlay for your board

A `coredump_partition` must exist in the device tree. Board overlays are provided in `boards/`:

```cmake
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_LIST_DIR}/../../lib/crash_log/boards/${BOARD}.overlay")
```

Supported boards:
- `nrf52840dk_nrf52840` - 12KB partition at 0xFD000 (shrinks storage from 32KB to 20KB)
- `nrf5340dk_nrf5340_cpuapp` - 12KB partition at 0xFD000 (shrinks storage from 32KB to 20KB)
- `nrf54l15dk_nrf54l15_cpuapp` - 12KB partition at 0x17A000 (shrinks storage from 36KB to 24KB)

For new boards: create `boards/<board>.overlay` defining a `coredump_partition` with at least 12KB.

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_CRASH_LOG` | - | Enable crash_log module (requires `DEBUG_COREDUMP` + `FLASH_PARTITION` backend) |
| `CONFIG_CRASH_LOG_AUTO_REPORT` | y | Emit stored coredump as `#CD:` lines automatically on boot |
| `CONFIG_CRASH_LOG_SHELL` | y (if SHELL) | Add `crash` shell commands |

## Shell Commands

| Command | Description |
|---------|-------------|
| `crash check` | Check if a stored coredump exists (shows size) |
| `crash info` | Parse coredump header and show PC, LR, SP, fault reason |
| `crash dump` | Re-emit stored coredump as `#CD:` lines via logging |
| `crash clear` | Erase stored coredump from flash |

## MCP Workflow

```
1. App crashes → Zephyr stores coredump to flash
2. Device reboots → crash_log SYS_INIT emits #CD: lines via RTT
3. Capture RTT:     embedded-probe.rtt_read(session_id, timeout_ms=5000)
4. Analyze:         embedded-probe.analyze_coredump(log_text=<rtt_output>, elf_path="...zephyr.elf")
```

Or interactively via shell:
```
crash check    → "CRASH STORED (156 bytes)"
crash info     → PC: 0x00000770, LR: 0x00000751, Reason: CPU exception
crash dump     → emits #CD: lines for MCP capture
crash clear    → erases after analysis
```

## Dependencies

- `CONFIG_DEBUG_COREDUMP=y`
- `CONFIG_DEBUG_COREDUMP_BACKEND_FLASH_PARTITION=y`
- `CONFIG_FLASH=y`, `CONFIG_FLASH_MAP=y`, `CONFIG_STREAM_FLASH=y`
- A `coredump_partition` in the device tree
