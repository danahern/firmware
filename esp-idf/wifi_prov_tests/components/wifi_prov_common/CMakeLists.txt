# Compiles portable wifi_prov modules needed for unit tests
# Tests only use msg, sm, and cred modules

set(WIFI_PROV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/wifi_prov")

idf_component_register(
    SRCS
        "${WIFI_PROV_ROOT}/src/wifi_prov_sm.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_msg.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_cred.c"
    INCLUDE_DIRS
        "${WIFI_PROV_ROOT}/include"
    REQUIRES
        eai_log
        eai_settings
)
