# eai_sensor — portable sensor HAL
zephyr_library_named(eai_sensor)

zephyr_include_directories_ifdef(CONFIG_EAI_SENSOR include)

zephyr_library_sources_ifdef(CONFIG_EAI_SENSOR_BACKEND_ZEPHYR
    src/zephyr/sensor.c
)
