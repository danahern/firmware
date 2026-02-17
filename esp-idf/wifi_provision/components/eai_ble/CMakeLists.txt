# ESP-IDF component wrapper for eai_ble
# Compiles the NimBLE backend from the shared library

set(EAI_BLE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/eai_ble")

idf_component_register(
    SRCS "${EAI_BLE_ROOT}/src/freertos/ble.c"
    INCLUDE_DIRS "${EAI_BLE_ROOT}/include"
    REQUIRES bt eai_osal
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_EAI_BLE_BACKEND_FREERTOS=1)
