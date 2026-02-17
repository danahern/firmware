# Compiles all portable wifi_prov modules from the shared library
# All modules now use eai_* abstractions — no platform-specific code needed

set(WIFI_PROV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/wifi_prov")

idf_component_register(
    SRCS
        "${WIFI_PROV_ROOT}/src/wifi_prov_sm.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_msg.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_cred.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_ble.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_wifi.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov.c"
    INCLUDE_DIRS
        "${WIFI_PROV_ROOT}/include"
    REQUIRES
        eai_log
        eai_ble
        eai_wifi
        eai_settings
        eai_osal
)
