# ESP-IDF component wrapper for eai_settings
# Compiles the NVS backend from the shared library

set(EAI_SETTINGS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/eai_settings")

idf_component_register(
    SRCS "${EAI_SETTINGS_ROOT}/src/freertos/settings.c"
    INCLUDE_DIRS "${EAI_SETTINGS_ROOT}/include"
    REQUIRES nvs_flash
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_EAI_SETTINGS_BACKEND_FREERTOS=1)
