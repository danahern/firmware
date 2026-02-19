# eai_sensor — Portable Sensor HAL

Android ISensors/ISensorModule concepts mapped to embedded platforms. Compile-time backend dispatch (Zephyr Sensor API, ESP-IDF I2C, POSIX stub). Session-based streaming with callback and polling modes.

## What It Does

- `eai_sensor_init/deinit()` — Module lifecycle (discovers sensors from backend)
- `eai_sensor_get_device/find_device()` — Enumerate sensor devices by index or type
- `eai_sensor_session_open/close()` — Session lifecycle on a specific device
- `eai_sensor_session_start/stop()` — Start/stop data delivery (callback or polling)
- `eai_sensor_session_read()` — Poll for sensor data with timeout
- `eai_sensor_session_flush()` — Deliver all buffered data immediately

## When to Use

Any app that reads sensors across Zephyr, ESP-IDF, or Linux. Use instead of calling Zephyr Sensor API, ESP-IDF I2C, or platform sensor APIs directly.

## How to Include

**Zephyr** — Add to `prj.conf`:
```
CONFIG_EAI_SENSOR=y
```

Then include:
```c
#include <eai_sensor/eai_sensor.h>
```

**Native/POSIX** — Compile `src/posix/sensor.c` with `CONFIG_EAI_SENSOR_BACKEND_POSIX` defined.

## API Reference

### Module Lifecycle

```c
eai_sensor_init();   /* discovers devices, sets up state */
eai_sensor_deinit(); /* closes sessions, releases resources */
```

### Device Enumeration

```c
int count = eai_sensor_get_device_count();

struct eai_sensor_device dev;
eai_sensor_get_device(0, &dev);          /* by index */
eai_sensor_find_device(EAI_SENSOR_TYPE_ACCEL, &dev); /* by type */
```

Default POSIX devices: accel (id=0), temp (id=1).

### Session-Based Streaming

```c
struct eai_sensor_session session;
struct eai_sensor_config config = {
    .rate_hz = 100,
    .max_latency_ms = 0,
};

eai_sensor_session_open(&session, dev.id, &config);

/* Callback mode */
eai_sensor_session_start(&session, on_data, user_ctx);
eai_sensor_session_flush(&session);  /* deliver buffered data now */

/* Polling mode */
eai_sensor_session_start(&session, NULL, NULL);
struct eai_sensor_data buf[4];
int count = eai_sensor_session_read(&session, buf, 4, timeout_ms);

eai_sensor_session_stop(&session);
eai_sensor_session_close(&session);
```

### Sensor Data

```c
struct eai_sensor_data {
    uint8_t device_id;
    enum eai_sensor_type type;
    uint64_t timestamp_ns;
    union {
        struct { int32_t x, y, z; } vec3;  /* mg, mdps, mgauss */
        int32_t scalar;                     /* mPa, m°C, m%RH, mlux, mm */
    };
};
```

Sensor types: ACCEL, GYRO, MAG, PRESSURE, TEMPERATURE, HUMIDITY, LIGHT, PROXIMITY.

All physical values in milliunits (no float): mg for acceleration, mdps for gyroscope, mgauss for magnetometer, mPa for pressure, m°C for temperature, m%RH for humidity, mlux for light, mm for proximity.

## Return Values

All functions return 0 on success, negative errno on error:
- `-EINVAL` — NULL pointer, invalid argument, not initialized
- `-ENODEV` — Device not found
- `-EBUSY` — Device already has an active session
- `-ENOMEM` — No session slots available

`session_read` returns reading count on success.

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_SENSOR` | n | Enable the sensor library |
| `CONFIG_EAI_SENSOR_BACKEND_ZEPHYR` | y | Zephyr Sensor API backend |
| `CONFIG_EAI_SENSOR_MAX_DEVICES` | 8 | Max sensor devices (1-32) |
| `CONFIG_EAI_SENSOR_MAX_SESSIONS` | 4 | Max concurrent sessions (1-16) |

## Testing

**Native (POSIX):** 25 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_sensor_tests
```

With sanitizers:
```bash
cmake -B build -DENABLE_SANITIZERS=ON && cmake --build build && ./build/eai_sensor_tests
```

## Files

```
lib/eai_sensor/
├── CLAUDE.md
├── CMakeLists.txt
├── Kconfig
├── zephyr/module.yml
├── include/eai_sensor/
│   ├── eai_sensor.h          # Umbrella include + init/deinit + test helpers
│   ├── types.h              # Enums, structs, backend dispatch
│   ├── device.h             # Device enumeration
│   └── session.h            # Session lifecycle + I/O
└── src/
    └── posix/
        ├── types.h           # POSIX backend types
        └── sensor.c          # Test stub backend
```
