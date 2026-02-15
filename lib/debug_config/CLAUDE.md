# debug_config

Shared Kconfig overlay files for crash diagnostics. Not a Zephyr module — just `.conf` files that apps include.

## Available Overlays

### `debug_coredump.conf` — Real-time coredump via RTT

Coredump emitted at crash time through the logging backend. Output appears immediately in RTT. Simple, no flash partition needed, but the data is lost if no one is reading RTT when the crash happens.

```cmake
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/debug_config/debug_coredump.conf")
```

Enables: `DEBUG_COREDUMP` (logging backend), RTT logging, 4KB RTT buffer, debug optimizations.

**Use when:** you're actively monitoring the device and will see the crash in real time.

### `debug_coredump_flash.conf` — Persistent coredump in flash

Coredump stored to a flash partition at crash time. Survives reboot. The `crash_log` module automatically re-emits it as `#CD:` lines on the next boot.

```cmake
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/debug_config/debug_coredump_flash.conf")
```

Enables: `DEBUG_COREDUMP` (flash backend), flash drivers, `CRASH_LOG` + auto-report, shell over RTT, 4KB RTT buffer, debug optimizations, reboot support.

**Requires:** `crash_log` module in `ZEPHYR_EXTRA_MODULES` + a board DTS overlay with `coredump_partition`.

**Use when:** crashes may happen at any time (not just during monitored sessions).

## When to Use Which

| Scenario | Overlay |
|----------|---------|
| Debugging a crash you can reproduce | `debug_coredump.conf` |
| Field testing / intermittent crashes | `debug_coredump_flash.conf` |
| Want shell commands for crash management | `debug_coredump_flash.conf` |
