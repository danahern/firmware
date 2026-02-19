# eai_audio — Portable Audio HAL

Android Audio HAL concepts mapped to embedded platforms. Compile-time backend dispatch (Zephyr I2S, ALSA, ESP-IDF, POSIX stub). Optional mini-flinger software mixer for multiple output streams.

## What It Does

- `eai_audio_init/deinit()` — Module lifecycle (discovers ports from backend)
- `eai_audio_get_port/find_port()` — Enumerate physical audio endpoints
- `eai_audio_stream_open/close/start/pause()` — Stream lifecycle
- `eai_audio_stream_write/read()` — Blocking audio I/O with timeout
- `eai_audio_set/get_gain()` — Per-port volume in centibels
- `eai_audio_set_route/get_route()` — Source-to-sink audio routing
- Mini-flinger mixer — Lightweight thread that mixes 2-4 output streams

## When to Use

Any app that needs audio I/O across Zephyr, ESP-IDF, or Linux. Use instead of calling I2S, ALSA, or platform audio APIs directly.

## How to Include

**Zephyr** — Add to `prj.conf`:
```
CONFIG_EAI_AUDIO=y
CONFIG_EAI_AUDIO_MIXER=y  # optional: software mixer
CONFIG_EAI_OSAL=y          # required if mixer enabled
```

Then include:
```c
#include <eai_audio/eai_audio.h>
```

**Native/POSIX** — Compile `src/posix/audio.c` with `CONFIG_EAI_AUDIO_BACKEND_POSIX` defined.

## API Reference

### Module Lifecycle

```c
eai_audio_init();   /* discovers ports, sets up state */
eai_audio_deinit(); /* closes streams, releases resources */
```

### Port Enumeration

```c
int count = eai_audio_get_port_count();

struct eai_audio_port port;
eai_audio_get_port(0, &port);          /* by index */
eai_audio_find_port(EAI_AUDIO_PORT_SPEAKER, EAI_AUDIO_OUTPUT, &port); /* by type+dir */
```

Default POSIX ports: speaker (output, id=0), mic (input, id=1).

### Stream I/O

```c
struct eai_audio_stream stream;
struct eai_audio_config config = {
    .sample_rate = 16000,
    .format = EAI_AUDIO_FORMAT_PCM_S16_LE,
    .channels = EAI_AUDIO_CHANNEL_MONO,
    .frame_count = 256,
};

eai_audio_stream_open(&stream, port_id, &config);
eai_audio_stream_start(&stream);
eai_audio_stream_write(&stream, pcm_data, frame_count, timeout_ms);
eai_audio_stream_pause(&stream);
eai_audio_stream_close(&stream);

uint64_t pos;
eai_audio_stream_get_position(&stream, &pos);
```

### Gain Control

```c
eai_audio_set_gain(port_id, -2000);  /* -20 dB in centibels */
int32_t gain_cb;
eai_audio_get_gain(port_id, &gain_cb);
```

Gain is clamped to port's min/max range. Returns `-ENOTSUP` for ports without gain.

### Routing

```c
eai_audio_set_route(source_port_id, sink_port_id);  /* input -> output */
int count = eai_audio_get_route_count();
struct eai_audio_route route;
eai_audio_get_route(0, &route);
```

## Mini-Flinger (Software Mixer)

Optional software mixer that combines multiple output streams. Enabled via `CONFIG_EAI_AUDIO_MIXER=y`.

Architecture:
```
stream_write() → Slot 0: ring_buf ──┐
stream_write() → Slot 1: ring_buf ──┼── mix (sum + clip) → backend_hw_write()
stream_write() → Slot 2: ring_buf ──┤
stream_write() → Slot 3: ring_buf ──┘
```

- Single high-priority eai_osal thread
- 2-4 pre-allocated slots (Kconfig: `CONFIG_EAI_AUDIO_MIXER_SLOTS`)
- Per-slot ring buffer, int16 mixing via int32 accumulator
- Per-stream Q16 fixed-point volume (0x10000 = unity, 0 = mute)
- Hard clip to int16 range, silence on underrun

## Return Values

All functions return 0 on success, negative errno on error:
- `-EINVAL` — NULL pointer or invalid argument
- `-ENODEV` — Port not found
- `-ENOTSUP` — Operation not supported (e.g., write to input stream)
- `-EBUSY` — Port already has an active stream
- `-ENOMEM` — No available slots/routes

`stream_write/read` return frame count on success.

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_AUDIO` | n | Enable the audio library |
| `CONFIG_EAI_AUDIO_BACKEND_ZEPHYR` | y | Zephyr I2S backend |
| `CONFIG_EAI_AUDIO_MIXER` | y | Software mixer (requires EAI_OSAL) |
| `CONFIG_EAI_AUDIO_MIXER_SLOTS` | 4 | Max mixer stream slots (2-8) |
| `CONFIG_EAI_AUDIO_MAX_PORTS` | 4 | Max audio ports (2-16) |
| `CONFIG_EAI_AUDIO_MAX_ROUTES` | 4 | Max simultaneous routes (1-16) |

## Backends

| Backend | Config | Audio I/O | Port Discovery |
|---------|--------|-----------|----------------|
| **Zephyr** | `CONFIG_EAI_AUDIO_BACKEND_ZEPHYR` | I2S API | Devicetree |
| **ALSA** | `CONFIG_EAI_AUDIO_BACKEND_ALSA` | tinyalsa | `/proc/asound/` |
| **FreeRTOS** | `CONFIG_EAI_AUDIO_BACKEND_FREERTOS` | ESP-IDF I2S | Static table |
| **POSIX** | `CONFIG_EAI_AUDIO_BACKEND_POSIX` | Buffer stub | Fake ports |

## Testing

**Native (POSIX):** 43 tests (31 core API + 12 mixer)
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_audio_tests
```

With sanitizers:
```bash
cmake -B build-san -DENABLE_SANITIZERS=ON && cmake --build build-san && ./build-san/eai_audio_tests
```

Without mixer (core API only):
```bash
cmake -B build -DENABLE_MIXER=OFF && cmake --build build && ./build/eai_audio_tests
```

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Mixer format | S16_LE only (v1) | Common on embedded. S24/S32 deferred. |
| Port table | Static arrays | No malloc. Audio hardware doesn't hot-plug on RTOS. |
| Stream I/O | Blocking with timeout | Matches eai_osal pattern. |
| Gain units | Centibels (int32) | No float needed. 1/100 dB precision. |
| Mixer | Separate `mixer.c` | Platform-independent. Backend only provides `hw_write()`. |
| Sample rate conversion | None (v1) | Streams must match hardware rate. |

## Files

```
lib/eai_audio/
├── CLAUDE.md
├── CMakeLists.txt
├── Kconfig
├── zephyr/module.yml
├── include/eai_audio/
│   ├── eai_audio.h          # Umbrella include + init/deinit + test helpers
│   ├── types.h              # Enums, structs, backend dispatch
│   ├── stream.h             # Stream lifecycle + I/O
│   ├── port.h               # Port enumeration
│   ├── gain.h               # Gain control
│   └── route.h              # Routing
├── src/
│   ├── mixer.h              # Internal mixer API
│   ├── mixer.c              # Mini-flinger (uses eai_osal)
│   └── posix/
│       ├── types.h           # POSIX backend types
│       └── audio.c           # Test stub backend
└── tests/
    └── native/
        ├── CMakeLists.txt    # Build with optional mixer
        ├── main.c            # 31 core API tests
        ├── mixer_tests.c     # 12 mixer tests
        └── unity/            # Vendored Unity v2.6.0
```
