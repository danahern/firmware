# WiFi Provision — ESP-IDF

ESP-IDF WiFi provisioning app sharing portable code from `lib/wifi_prov/`. Same BLE GATT protocol as the Zephyr version — macOS Python tools work unchanged against both targets.

## Architecture

```
main/main.c                         -- app_main(): init NVS, OSAL, wifi_prov, throughput server
components/
  eai_osal/                          -- OSAL FreeRTOS backend (compiles from lib/eai_osal/src/freertos/)
  wifi_prov_common/                  -- Portable sm.c + msg.c (compiles from lib/wifi_prov/src/)
    shim/zephyr/                     -- Zephyr → ESP-IDF compatibility headers (LOG_*, kernel types)
  wifi_prov_esp/                     -- Platform-specific implementations:
    wifi_prov_esp.c                  --   Orchestrator (OSAL work queues)
    wifi_prov_ble_esp.c              --   BLE GATT service (NimBLE)
    wifi_prov_wifi_esp.c             --   WiFi scan/connect (esp_wifi)
    wifi_prov_cred_esp.c             --   Credential persistence (NVS)
  throughput_server/                 -- TCP throughput server (POSIX sockets + OSAL thread)
```

## Component Dependency Graph

```
main
  ├── wifi_prov_esp
  │   ├── bt (NimBLE)
  │   ├── esp_wifi
  │   ├── esp_event
  │   ├── esp_netif
  │   ├── nvs_flash
  │   ├── wifi_prov_common
  │   │   └── log
  │   └── eai_osal
  │       └── freertos
  └── throughput_server
      ├── eai_osal
      └── lwip
```

## Code Sharing Strategy

Components reference `lib/` via relative paths — no code duplication:
- `eai_osal`: compiles `lib/eai_osal/src/freertos/*.c`
- `wifi_prov_common`: compiles `lib/wifi_prov/src/wifi_prov_sm.c` + `wifi_prov_msg.c`
- Zephyr shim headers in `wifi_prov_common/shim/` map `LOG_*` → `ESP_LOG*` and provide `errno.h`/`stdint.h`

## Build & Flash

```bash
# Set target (once)
idf.py set-target esp32

# Build
idf.py build

# Flash + monitor
idf.py -p /dev/cu.usbserial-110 flash monitor
```

## Key Configuration (sdkconfig.defaults)

- NimBLE enabled (not Bluedroid)
- WiFi STA mode
- NVS for credential persistence
- FreeRTOS timer task stack 4096 (needs headroom for OSAL timer callbacks)
- Main task stack 8192
- WiFi power save disabled (`esp_wifi_set_ps(WIFI_PS_NONE)`) for reliable TCP

## Platform-Specific Notes

- **WiFi power management**: Modem sleep blocks incoming TCP/ICMP. Disabled in `wifi_prov_wifi_esp.c`.
- **Scan results**: ESP-IDF delivers scan results in batch via `WIFI_EVENT_SCAN_DONE` + `esp_wifi_scan_get_ap_records()`, unlike Zephyr's per-result callbacks. `scan_done_callback` fires `WIFI_PROV_EVT_SCAN_DONE`.
- **WiFi disconnect semantics**: `WIFI_EVENT_STA_DISCONNECTED` fires for both auth failure and real disconnect. Orchestrator checks current state to differentiate.
- **BLE GATT**: NimBLE service uses same UUIDs and wire format as Zephyr `bt_gatt`. Factory reset requires writing `0xFF`.
- **Credentials**: NVS namespace `wifi_prov`, keys: `ssid`, `psk`, `sec`. Returns `-ENOENT` when no credentials stored.
- **OSAL work queue stack**: System work queue needs 4096 bytes (not 2048) for WiFi API calls on ESP32.

## Testing

22 unit tests in `firmware/esp-idf/wifi_prov_tests/`:
- 8 message encode/decode
- 9 state machine transitions
- 5 credential store (NVS)

```bash
cd firmware/esp-idf/wifi_prov_tests
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor
```

Integration tests via `hw-test-runner` MCP (same as Zephyr version):
- BLE discovery, WiFi scan, provisioning, TCP throughput, factory reset
