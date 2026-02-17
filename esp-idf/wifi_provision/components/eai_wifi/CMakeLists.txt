# ESP-IDF component wrapper for eai_wifi
# Compiles the ESP-IDF backend from the shared library

set(EAI_WIFI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/eai_wifi")

idf_component_register(
    SRCS "${EAI_WIFI_ROOT}/src/freertos/wifi.c"
    INCLUDE_DIRS "${EAI_WIFI_ROOT}/include"
    REQUIRES esp_wifi esp_event esp_netif
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_EAI_WIFI_BACKEND_FREERTOS=1)
