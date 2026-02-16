idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES unity wifi_prov_common wifi_prov_cred nvs_flash
)
