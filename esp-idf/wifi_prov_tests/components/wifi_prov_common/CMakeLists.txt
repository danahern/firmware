# Compiles portable wifi_prov modules from the shared library
# Uses Zephyr shim headers so sm.c and msg.c compile unchanged

set(WIFI_PROV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/wifi_prov")

idf_component_register(
    SRCS
        "${WIFI_PROV_ROOT}/src/wifi_prov_sm.c"
        "${WIFI_PROV_ROOT}/src/wifi_prov_msg.c"
    INCLUDE_DIRS
        "${WIFI_PROV_ROOT}/include"
    PRIV_INCLUDE_DIRS
        "shim"
    REQUIRES log
)
