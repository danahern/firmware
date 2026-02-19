cmake_minimum_required(VERSION 3.13)
project(eai_display_tests C)

set(DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_display_tests
    main.c
    ${DISPLAY_DIR}/src/posix/display.c
)
target_include_directories(eai_display_tests PRIVATE
    ${DISPLAY_DIR}/include
)
target_compile_definitions(eai_display_tests PRIVATE
    CONFIG_EAI_DISPLAY_BACKEND_POSIX
    EAI_DISPLAY_TEST
    CONFIG_EAI_DISPLAY_MAX_DEVICES=2
    CONFIG_EAI_DISPLAY_MAX_LAYERS=4
)
target_link_libraries(eai_display_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_display_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_display_tests PRIVATE -fsanitize=address,undefined)
endif()
