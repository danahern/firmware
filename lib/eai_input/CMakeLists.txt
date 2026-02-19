# eai_input — portable input HAL
zephyr_library_named(eai_input)

zephyr_include_directories_ifdef(CONFIG_EAI_INPUT include)

zephyr_library_sources_ifdef(CONFIG_EAI_INPUT_BACKEND_ZEPHYR
    src/zephyr/input.c
)
