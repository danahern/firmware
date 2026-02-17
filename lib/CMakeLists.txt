# Shared libraries — auto-discovered via zephyr/module.yml
# Apps just need CONFIG_CRASH_LOG=y or CONFIG_DEVICE_SHELL=y
add_subdirectory(crash_log)
add_subdirectory(device_shell)
add_subdirectory(eai_ble)
add_subdirectory(eai_log)
add_subdirectory(eai_osal)
add_subdirectory(eai_settings)
add_subdirectory(eai_ipc)
add_subdirectory(eai_wifi)
add_subdirectory(wifi_prov)
