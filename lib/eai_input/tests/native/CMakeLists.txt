cmake_minimum_required(VERSION 3.13)
project(eai_input_tests C)

set(INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_input_tests
    main.c
    ${INPUT_DIR}/src/posix/input.c
)
target_include_directories(eai_input_tests PRIVATE
    ${INPUT_DIR}/include
)
target_compile_definitions(eai_input_tests PRIVATE
    CONFIG_EAI_INPUT_BACKEND_POSIX
    EAI_INPUT_TEST
    CONFIG_EAI_INPUT_MAX_DEVICES=8
)
target_link_libraries(eai_input_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_input_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_input_tests PRIVATE -fsanitize=address,undefined)
endif()
