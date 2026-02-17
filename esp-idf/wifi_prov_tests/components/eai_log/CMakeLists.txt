# ESP-IDF component wrapper for eai_log (header-only)
# Just exposes include directory and sets the backend define

set(EAI_LOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/eai_log")

idf_component_register(
    INCLUDE_DIRS "${EAI_LOG_ROOT}/include"
    REQUIRES log
)

target_compile_definitions(${COMPONENT_LIB} INTERFACE CONFIG_EAI_LOG_BACKEND_FREERTOS=1)
