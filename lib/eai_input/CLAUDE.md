# eai_input — Portable Input HAL

Android EventHub/InputReader concepts simplified for embedded platforms. Compile-time backend dispatch (Zephyr Input, ESP-IDF GPIO/Touch, POSIX stub). Callback-driven with optional polling.

## What It Does

- `eai_input_init/deinit()` — Module lifecycle (discovers devices, registers callback)
- `eai_input_get_device/find_device()` — Enumerate input devices by index or type
- `eai_input_read()` — Poll for next event with timeout
- Callback mode delivers events immediately via `eai_input_event_cb_t`

## When to Use

Any app that needs input events across Zephyr, ESP-IDF, or Linux. Use instead of calling Zephyr Input, ESP-IDF GPIO, or evdev directly.

## How to Include

**Zephyr** — Add to `prj.conf`:
```
CONFIG_EAI_INPUT=y
```

Then include:
```c
#include <eai_input/eai_input.h>
```

**Native/POSIX** — Compile `src/posix/input.c` with `CONFIG_EAI_INPUT_BACKEND_POSIX` defined.

## API Reference

### Module Lifecycle

```c
/* Callback mode */
eai_input_init(on_event, user_ctx);

/* Polling mode */
eai_input_init(NULL, NULL);

eai_input_deinit();
```

### Device Enumeration

```c
int count = eai_input_get_device_count();

struct eai_input_device dev;
eai_input_get_device(0, &dev);
eai_input_find_device(EAI_INPUT_DEVICE_TOUCH, &dev);
```

Default POSIX devices: touch (id=0, 320x240), btn_a (id=1), btn_b (id=2).

### Event Reading (Polling)

```c
struct eai_input_event event;
int ret = eai_input_read(&event, timeout_ms);
if (ret == 0) {
    /* event.device_id, event.type, event.x, event.y, event.code */
}
```

### Input Events

```c
struct eai_input_event {
    uint8_t device_id;
    enum eai_input_event_type type;  /* PRESS, RELEASE, MOVE, LONG_PRESS */
    int16_t x, y;                    /* touch/gesture coordinates */
    uint16_t code;                   /* key/button code */
    uint32_t timestamp_ms;
};
```

Device types: TOUCH, BUTTON, ENCODER, KEYBOARD, GESTURE.

## Return Values

All functions return 0 on success, negative errno on error:
- `-EINVAL` — NULL pointer, invalid argument, not initialized
- `-ENODEV` — Device not found
- `-EAGAIN` — No event available (non-blocking read)

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_INPUT` | n | Enable the input library |
| `CONFIG_EAI_INPUT_BACKEND_ZEPHYR` | y | Zephyr Input subsystem backend |
| `CONFIG_EAI_INPUT_MAX_DEVICES` | 8 | Max input devices (1-16) |

## Testing

**Native (POSIX):** 17 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_input_tests
```

With sanitizers:
```bash
cmake -B build -DENABLE_SANITIZERS=ON && cmake --build build && ./build/eai_input_tests
```

## Files

```
lib/eai_input/
├── CLAUDE.md
├── CMakeLists.txt
├── Kconfig
├── zephyr/module.yml
├── include/eai_input/
│   ├── eai_input.h           # Umbrella include + init/deinit + test helpers
│   ├── types.h              # Enums, structs, backend dispatch
│   ├── device.h             # Device enumeration
│   └── event.h              # Event reading (polling)
└── src/
    └── posix/
        ├── types.h           # POSIX backend types (empty)
        └── input.c           # Test stub backend
```
