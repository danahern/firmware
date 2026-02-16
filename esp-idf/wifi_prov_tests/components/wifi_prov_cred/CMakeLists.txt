# ESP-IDF credential store implementation for testing
# Compiles wifi_prov_cred_esp.c from the wifi_provision project

set(CRED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../wifi_provision/components/wifi_prov_esp/wifi_prov_cred_esp.c")

idf_component_register(
    SRCS "${CRED_SRC}"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash wifi_prov_common
)
