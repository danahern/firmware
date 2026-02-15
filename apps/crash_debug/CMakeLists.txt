cmake_minimum_required(VERSION 3.20.0)

# Shared libraries (crash_log, device_shell) are auto-discovered
# via zephyr/module.yml — no ZEPHYR_EXTRA_MODULES needed.

# Flash-based coredump with shell + crash_log + device_shell
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/crash_log/conf/debug_base.conf")
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/crash_log/conf/debug_coredump_flash.conf")
list(APPEND OVERLAY_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../lib/device_shell/device_shell.conf")

# Board-specific DTS overlays for coredump flash partition
# are auto-discovered from boards/ directory

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crash_debug)

target_sources(app PRIVATE src/main.c)
