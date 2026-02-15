# Shared libraries — auto-discovered via zephyr/module.yml
# Apps just need CONFIG_CRASH_LOG=y or CONFIG_DEVICE_SHELL=y
add_subdirectory(crash_log)
add_subdirectory(device_shell)
