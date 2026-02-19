cmake_minimum_required(VERSION 3.13)
project(eai_sensor_tests C)

set(SENSOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_sensor_tests
    main.c
    ${SENSOR_DIR}/src/posix/sensor.c
)
target_include_directories(eai_sensor_tests PRIVATE
    ${SENSOR_DIR}/include
)
target_compile_definitions(eai_sensor_tests PRIVATE
    CONFIG_EAI_SENSOR_BACKEND_POSIX
    EAI_SENSOR_TEST
    CONFIG_EAI_SENSOR_MAX_DEVICES=8
    CONFIG_EAI_SENSOR_MAX_SESSIONS=4
)
target_link_libraries(eai_sensor_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_sensor_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_sensor_tests PRIVATE -fsanitize=address,undefined)
endif()
