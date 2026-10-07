cmake_minimum_required(VERSION 3.20)

project(stm32_sensor_node
    VERSION 1.0.0
    DESCRIPTION "SHT3x sensor node on STM32F446 with FreeRTOS"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Portable application core: no RTOS, no hardware. Built for both targets.
# ---------------------------------------------------------------------------
add_library(node_core STATIC
    src/alarm.c
    src/crc8.c
    src/line_buffer.c
    src/moving_average.c
    src/node.c
    src/sht3x.c
)
target_include_directories(node_core PUBLIC src)
target_compile_options(node_core PRIVATE
    -Wall -Wextra -Werror -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes
)

if(CMAKE_CROSSCOMPILING)
    # -----------------------------------------------------------------------
    # Firmware for the NUCLEO-F446RE board
    # -----------------------------------------------------------------------
    add_subdirectory(firmware)
else()
    # -----------------------------------------------------------------------
    # Host build: unit tests
    # -----------------------------------------------------------------------
    option(ENABLE_SANITIZERS "Build host tests with ASan + UBSan" OFF)
    if(ENABLE_SANITIZERS)
        set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer
                            -fno-sanitize-recover=all)
        target_compile_options(node_core PUBLIC ${SANITIZER_FLAGS})
        target_link_options(node_core PUBLIC ${SANITIZER_FLAGS})
    endif()

    enable_testing()
    add_subdirectory(test)
endif()
