# device_shell

Zephyr module that adds basic device management shell commands over RTT.

## What It Does

Registers a `board` command group in the Zephyr shell (named `board` to avoid conflict with Zephyr's built-in `device` command):

| Command | Description |
|---------|-------------|
| `board info` | Board name, SOC, Zephyr version, build timestamp |
| `board uptime` | Time since boot (hours, minutes, seconds) |
| `board reset` | Cold reboot via `sys_reboot()` |

## When to Use

Include in any app where you want interactive device info over RTT. Useful for verifying which firmware is running, checking uptime, or triggering a reboot without power cycling.

## How to Include

### 1. Register as a Zephyr module

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/device_shell
)
```

### 2. Include the shell overlay

```cmake
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_shell/device_shell.conf")
```

This enables `CONFIG_SHELL=y`, `CONFIG_SHELL_BACKEND_RTT=y`, and `CONFIG_DEVICE_SHELL=y`.

## Kconfig

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_DEVICE_SHELL` | - | Enable device shell commands (requires `CONFIG_SHELL`) |

## Dependencies

- `CONFIG_SHELL=y`
- `CONFIG_REBOOT=y` (for `board reset` command)
