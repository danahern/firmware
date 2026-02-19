# eai_display — Portable Display HAL

Android HWComposer concepts simplified for embedded platforms. Compile-time backend dispatch (Zephyr Display API, ESP LCD, Linux DRM, POSIX stub). Layer-based compositing with brightness and vsync.

## What It Does

- `eai_display_init/deinit()` — Module lifecycle (discovers displays from backend)
- `eai_display_get_device/get_device_count()` — Enumerate display devices
- `eai_display_layer_open/close()` — Layer lifecycle (position, size, format)
- `eai_display_layer_write()` — Write pixel data to a layer
- `eai_display_commit()` — Present all layers to the display
- `eai_display_set/get_brightness()` — Backlight control (0-100%)
- `eai_display_set_vsync()` — Vsync callback on commit

## When to Use

Any app that needs display output across Zephyr, ESP-IDF, or Linux. Use instead of calling Zephyr Display API, ESP LCD, or DRM/KMS directly.

## How to Include

**Zephyr** — Add to `prj.conf`:
```
CONFIG_EAI_DISPLAY=y
```

Then include:
```c
#include <eai_display/eai_display.h>
```

**Native/POSIX** — Compile `src/posix/display.c` with `CONFIG_EAI_DISPLAY_BACKEND_POSIX` defined.

## API Reference

### Module Lifecycle

```c
eai_display_init();   /* discovers displays, sets up state */
eai_display_deinit(); /* closes layers, releases resources */
```

### Device Enumeration

```c
int count = eai_display_get_device_count();

struct eai_display_device dev;
eai_display_get_device(0, &dev);
/* dev.width, dev.height, dev.formats[], dev.max_fps, dev.max_layers */
```

Default POSIX display: 320x240 LCD, RGB565/RGB888, 60fps.

### Layer Write + Commit

```c
struct eai_display_layer layer;
struct eai_display_layer_config cfg = {
    .x = 0, .y = 0,
    .width = 320, .height = 240,
    .format = EAI_DISPLAY_FORMAT_RGB565,
};

eai_display_layer_open(&layer, display_id, &cfg);
eai_display_layer_write(&layer, pixel_data, pixel_data_size);
eai_display_commit(display_id);  /* present to screen */
eai_display_layer_close(&layer);
```

### Pixel Formats

| Format | Bits/Pixel | Use |
|--------|-----------|-----|
| `EAI_DISPLAY_FORMAT_MONO1` | 1 | E-ink, OLED mono |
| `EAI_DISPLAY_FORMAT_RGB565` | 16 | Most embedded LCDs |
| `EAI_DISPLAY_FORMAT_RGB888` | 24 | Higher color depth |
| `EAI_DISPLAY_FORMAT_ARGB8888` | 32 | Alpha compositing |

### Brightness + Vsync

```c
eai_display_set_brightness(display_id, 75);  /* 75% */

uint8_t pct;
eai_display_get_brightness(display_id, &pct);

eai_display_set_vsync(display_id, true, on_vsync, user_ctx);
```

## Return Values

All functions return 0 on success, negative errno on error:
- `-EINVAL` — NULL pointer, invalid argument, not initialized, out of bounds
- `-ENODEV` — Display not found
- `-ENOMEM` — No layer slots available
- `-ENOTSUP` — Feature not supported (e.g., no backlight)

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_DISPLAY` | n | Enable the display library |
| `CONFIG_EAI_DISPLAY_BACKEND_ZEPHYR` | y | Zephyr Display API backend |
| `CONFIG_EAI_DISPLAY_MAX_DEVICES` | 2 | Max display devices (1-4) |
| `CONFIG_EAI_DISPLAY_MAX_LAYERS` | 4 | Max layers per display (1-8) |

## Testing

**Native (POSIX):** 22 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_display_tests
```

With sanitizers:
```bash
cmake -B build -DENABLE_SANITIZERS=ON && cmake --build build && ./build/eai_display_tests
```

## Files

```
lib/eai_display/
├── CLAUDE.md
├── CMakeLists.txt
├── Kconfig
├── zephyr/module.yml
├── include/eai_display/
│   ├── eai_display.h         # Umbrella include + init/deinit + test helpers
│   ├── types.h              # Enums, structs, backend dispatch
│   ├── device.h             # Device enumeration
│   ├── layer.h              # Layer lifecycle + pixel I/O
│   └── property.h           # Brightness, commit, vsync
└── src/
    └── posix/
        ├── types.h           # POSIX backend types
        └── display.c         # Test stub backend
```
