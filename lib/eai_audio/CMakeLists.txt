# eai_audio — portable audio HAL
zephyr_library_named(eai_audio)

zephyr_include_directories_ifdef(CONFIG_EAI_AUDIO include)

zephyr_library_sources_ifdef(CONFIG_EAI_AUDIO_MIXER
    src/mixer.c
)

zephyr_library_sources_ifdef(CONFIG_EAI_AUDIO_BACKEND_ZEPHYR
    src/zephyr/audio.c
)
