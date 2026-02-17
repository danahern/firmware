# eai_ipc — Portable Inter-Processor Communication

Endpoint-based messaging abstraction with backends for Zephyr IPC Service (RPMsg/ICMsg) and an in-process loopback for testing.

## What It Does

- `eai_ipc_init()` / `eai_ipc_deinit()` — Initialize/teardown IPC subsystem
- `eai_ipc_register_endpoint(ept, cfg)` — Register named endpoint; fires `bound()` when peer matches
- `eai_ipc_deregister_endpoint(ept)` — Unregister endpoint
- `eai_ipc_send(ept, data, len)` — Send data to peer (copy semantics)
- `eai_ipc_get_max_packet_size()` — Query transport limit (496 bytes)

## When to Use

Any app that needs inter-core messaging (e.g., A7 Linux + M4 Zephyr on STM32MP1). Use instead of calling `ipc_service_*` directly. Loopback backend enables testing IPC logic without real hardware.

## How to Include

1. **Zephyr (real hardware)** — Add to `prj.conf`:
   ```
   CONFIG_EAI_IPC=y
   CONFIG_EAI_IPC_BACKEND_ZEPHYR=y
   CONFIG_IPC_SERVICE=y
   ```
   Requires `ipc0` node in devicetree.

2. **Zephyr (testing)** — Add to `prj.conf`:
   ```
   CONFIG_EAI_IPC=y
   CONFIG_EAI_IPC_BACKEND_LOOPBACK=y
   ```

3. **Native (POSIX)** — Compile `src/loopback/ipc.c` with `EAI_IPC_BACKEND_LOOPBACK` defined.

4. **Source code**:
   ```c
   #include <eai_ipc/eai_ipc.h>

   void on_bound(void *ctx) { /* ready to send */ }
   void on_received(const void *data, size_t len, void *ctx) { /* handle data */ }

   struct eai_ipc_endpoint ept;
   struct eai_ipc_ept_cfg cfg = {
       .name = "my_channel",
       .cb = { .bound = on_bound, .received = on_received },
       .ctx = NULL,
   };

   eai_ipc_init();
   eai_ipc_register_endpoint(&ept, &cfg);
   eai_ipc_send(&ept, "hello", 5);
   ```

## Kconfig Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_EAI_IPC` | n | Enable library |
| `CONFIG_EAI_IPC_BACKEND_ZEPHYR` | n | Zephyr IPC Service backend |
| `CONFIG_EAI_IPC_BACKEND_LOOPBACK` | y | In-process loopback backend |
| `CONFIG_EAI_IPC_MAX_ENDPOINTS` | 8 | Max endpoints (loopback only) |
| `CONFIG_EAI_IPC_LOOPBACK_MAX_PACKET_SIZE` | 496 | Max payload size (loopback only) |

## Return Values

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-EINVAL` | NULL pointer, empty name, zero-length send |
| `-ENOMEM` | Endpoint table full (loopback) |
| `-ENOTCONN` | Endpoint not bound to peer |
| `-EMSGSIZE` | Payload exceeds max packet size |
| `-ENOENT` | Endpoint not registered |
| `-EIO` | IPC instance not ready (Zephyr) |

## Backends

**Zephyr IPC Service** — Wraps `ipc_service_open_instance()`, `ipc_service_register_endpoint()`, `ipc_service_send()`. Requires `ipc0` devicetree node. Backend data stores `struct ipc_ept` + callbacks.

**Loopback** — Fixed-size endpoint table with name-based pairing. When two endpoints register with the same name, both get `bound()` callbacks. `send()` delivers to peer's `received()` synchronously. Platform-agnostic (no OS dependencies).

## Key Design Decisions

- **Copy-only send.** RPMsg has 512-byte payload limit — messages are small, no zero-copy needed.
- **Callback-based receive.** Matches Zephyr IPC Service pattern. Callbacks must not block.
- **Name-based pairing.** Endpoint names must match on both cores. Loopback pairs in-process.
- **No `on_unbound`/`on_error`.** Minimal API — add if needed.
- **Negative errno returns.** Consistent with other eai_* libraries.

## Gotchas

- **RPMsg payload limit is 496 bytes** (512 - 16 byte header). Use `eai_ipc_get_max_packet_size()` to check.
- **Loopback delivery is synchronous.** `send()` calls peer's `received()` in the same call stack.
- **Zephyr backend needs IPC Service configured** in devicetree and Kconfig.
- **`EAI_IPC_EPT_BACKEND_SIZE` must fit backend data.** 128 bytes for Zephyr (contains `struct ipc_ept`), 8 bytes for loopback.

## Testing

**Native tests** (loopback backend): 14 tests
```bash
cd tests/native && cmake -B build && cmake --build build && ./build/eai_ipc_tests
```

**Zephyr tests** (loopback on QEMU): 14 tests
```
zephyr-build.build(app="firmware/lib/eai_ipc/tests", board="qemu_cortex_m3", pristine=true)
# Then run via QEMU or twister
```

## Files

```
lib/eai_ipc/
  include/eai_ipc/eai_ipc.h      # Public API (7 functions)
  src/zephyr/ipc.c                # Zephyr IPC Service backend
  src/loopback/ipc.c              # In-process loopback backend
  tests/src/main.c                # 14 ztest tests (Zephyr)
  tests/native/main.c             # 14 Unity tests (native)
  CMakeLists.txt
  Kconfig
```
