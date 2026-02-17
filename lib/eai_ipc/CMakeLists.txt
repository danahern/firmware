# eai_ipc — portable inter-processor communication
zephyr_library_named(eai_ipc)
zephyr_include_directories_ifdef(CONFIG_EAI_IPC include)

zephyr_library_sources_ifdef(CONFIG_EAI_IPC_BACKEND_ZEPHYR
    src/zephyr/ipc.c
)

zephyr_library_sources_ifdef(CONFIG_EAI_IPC_BACKEND_LOOPBACK
    src/loopback/ipc.c
)
