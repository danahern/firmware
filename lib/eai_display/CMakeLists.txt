# eai_display — portable display HAL
zephyr_library_named(eai_display)

zephyr_include_directories_ifdef(CONFIG_EAI_DISPLAY include)

zephyr_library_sources_ifdef(CONFIG_EAI_DISPLAY_BACKEND_ZEPHYR
    src/zephyr/display.c
)
