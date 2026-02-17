# WiFi Provision — ESP-IDF

ESP-IDF WiFi provisioning app using portable code from `lib/wifi_prov/`. Same BLE GATT protocol as the Zephyr version — all source files are shared, with platform differences handled by eai_* library backends.

## Architecture

```
main/main.c                         -- app_main(): wifi_prov_init(), wifi_prov_start(), throughput server
components/
  eai_osal/                          -- OSAL FreeRTOS backend (compiles from lib/eai_osal/src/freertos/)
  eai_ble/                           -- BLE NimBLE backend (compiles from lib/eai_ble/src/freertos/)
  eai_wifi/                          -- WiFi ESP-IDF backend (compiles from lib/eai_wifi/src/freertos/)
  eai_settings/                      -- Settings NVS backend (compiles from lib/eai_settings/src/freertos/)
  eai_log/                           -- Logging (header-only, ESP_LOG* backend)
  wifi_prov_common/                  -- All 6 portable wifi_prov source files from lib/wifi_prov/src/
  throughput_server/                 -- TCP throughput server (POSIX sockets + OSAL thread)
```

## Component Dependency Graph

```
main
  ├── wifi_prov_common
  │   ├── eai_log
  │   ├── eai_ble
  │   │   ├── bt (NimBLE)
  │   │   └── eai_osal
  │   ├── eai_wifi
  │   │   ├── esp_wifi
  │   │   ├── esp_event
  │   │   └── esp_netif
  │   ├── eai_settings
  │   │   └── nvs_flash
  │   └── eai_osal
  │       └── freertos
  └── throughput_server
      ├── eai_osal
      └── lwip
```

## Code Sharing Strategy

ALL wifi_prov source files are compiled from `lib/wifi_prov/src/` — no platform-specific code in this project. Platform differences are handled by the eai_* component wrappers, each compiling the FreeRTOS/ESP-IDF backend from the corresponding `lib/eai_*/src/freertos/` directory.

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

These are handled by the eai_* backends, not by wifi_prov code:

- **WiFi power management**: Modem sleep blocks incoming TCP/ICMP. Disabled in eai_wifi FreeRTOS backend.
- **Scan results**: ESP-IDF delivers scan results in batch via `WIFI_EVENT_SCAN_DONE`. eai_wifi iterates and delivers per-result callbacks.
- **BLE GATT**: NimBLE service uses same UUIDs and wire format as Zephyr `bt_gatt`. eai_ble handles the mapping.
- **Credentials**: eai_settings NVS backend splits "wifi_prov/ssid" into NVS namespace "wifi_prov" + key "ssid".
- **OSAL work queue stack**: System work queue needs 4096 bytes for WiFi API calls on ESP32.

## Testing

22 unit tests in `firmware/esp-idf/wifi_prov_tests/`:
- 8 message encode/decode
- 9 state machine transitions
- 5 credential store (via eai_settings NVS backend)

```bash
cd firmware/esp-idf/wifi_prov_tests
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor
```

Integration tests via `hw-test-runner` MCP (same as Zephyr version):
- BLE discovery, WiFi scan, provisioning, TCP throughput, factory reset
