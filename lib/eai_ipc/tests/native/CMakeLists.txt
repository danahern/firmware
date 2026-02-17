cmake_minimum_required(VERSION 3.13)
project(eai_ipc_tests C)

set(IPC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_ipc_tests
    main.c
    ${IPC_DIR}/src/loopback/ipc.c
)
target_include_directories(eai_ipc_tests PRIVATE ${IPC_DIR}/include)
target_compile_definitions(eai_ipc_tests PRIVATE
    EAI_IPC_BACKEND_LOOPBACK
)
target_link_libraries(eai_ipc_tests unity)

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_ipc_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_ipc_tests PRIVATE -fsanitize=address,undefined)
endif()
