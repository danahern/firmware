cmake_minimum_required(VERSION 3.13)
project(eai_audio_tests C)

set(AUDIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(OSAL_DIR ${AUDIO_DIR}/../eai_osal)

# Unity
add_library(unity unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable
add_executable(eai_audio_tests
    main.c
    ${AUDIO_DIR}/src/posix/audio.c
)
target_include_directories(eai_audio_tests PRIVATE
    ${AUDIO_DIR}/include
)
target_compile_definitions(eai_audio_tests PRIVATE
    CONFIG_EAI_AUDIO_BACKEND_POSIX
    EAI_AUDIO_TEST
    CONFIG_EAI_AUDIO_MAX_PORTS=4
    CONFIG_EAI_AUDIO_MAX_ROUTES=4
)
target_link_libraries(eai_audio_tests unity)

# Optional mixer tests (requires eai_osal POSIX)
option(ENABLE_MIXER "Enable mixer tests (requires eai_osal)" ON)
if(ENABLE_MIXER)
    target_sources(eai_audio_tests PRIVATE
        mixer_tests.c
        ${AUDIO_DIR}/src/mixer.c
        ${OSAL_DIR}/src/posix/mutex.c
        ${OSAL_DIR}/src/posix/semaphore.c
        ${OSAL_DIR}/src/posix/thread.c
        ${OSAL_DIR}/src/posix/queue.c
        ${OSAL_DIR}/src/posix/timer.c
        ${OSAL_DIR}/src/posix/event.c
        ${OSAL_DIR}/src/posix/critical.c
        ${OSAL_DIR}/src/posix/time.c
        ${OSAL_DIR}/src/posix/workqueue.c
    )
    target_include_directories(eai_audio_tests PRIVATE
        ${OSAL_DIR}/include
        ${AUDIO_DIR}/src  # for mixer.h
    )
    target_compile_definitions(eai_audio_tests PRIVATE
        CONFIG_EAI_OSAL_BACKEND_POSIX
        CONFIG_EAI_AUDIO_MIXER
        EAI_AUDIO_MIXER_TESTS
    )
endif()

# Optional sanitizers
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(eai_audio_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eai_audio_tests PRIVATE -fsanitize=address,undefined)
endif()
