cmake_minimum_required(VERSION 3.20.0)

# Register library modules before find_package(Zephyr)
list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/crash_log
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/device_shell
)

# Flash-based coredump with shell + crash_log + device_shell
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/debug_config/debug_coredump_flash.conf")
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_shell/device_shell.conf")

# Board-specific DTS overlays for coredump flash partition
# are auto-discovered from boards/ directory

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crash_debug)

target_sources(app PRIVATE src/main.c)
