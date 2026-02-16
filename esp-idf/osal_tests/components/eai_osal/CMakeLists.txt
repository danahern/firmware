# ESP-IDF component wrapper for eai_osal
# Compiles the FreeRTOS backend from the shared library

set(OSAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/eai_osal")

set(SRCS
    "${OSAL_ROOT}/src/freertos/mutex.c"
    "${OSAL_ROOT}/src/freertos/semaphore.c"
    "${OSAL_ROOT}/src/freertos/thread.c"
    "${OSAL_ROOT}/src/freertos/queue.c"
    "${OSAL_ROOT}/src/freertos/timer.c"
    "${OSAL_ROOT}/src/freertos/event.c"
    "${OSAL_ROOT}/src/freertos/critical.c"
    "${OSAL_ROOT}/src/freertos/time.c"
    "${OSAL_ROOT}/src/freertos/workqueue.c"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "${OSAL_ROOT}/include"
    PRIV_INCLUDE_DIRS "${OSAL_ROOT}/src/freertos"
    REQUIRES freertos
)

# Define the backend selection macro
target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_EAI_OSAL_BACKEND_FREERTOS=1)
