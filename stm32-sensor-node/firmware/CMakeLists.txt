# ---------------------------------------------------------------------------
# FreeRTOS kernel (fetched at configure time)
# ---------------------------------------------------------------------------
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "" FORCE)
set(FREERTOS_HEAP 4            CACHE STRING "" FORCE)

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(freertos_kernel)

# ---------------------------------------------------------------------------
# Firmware image
# ---------------------------------------------------------------------------
add_executable(sensor_node
    main.c
    bsp.c
    startup_stm32f446.c
)
set_target_properties(sensor_node PROPERTIES SUFFIX ".elf")

target_link_libraries(sensor_node PRIVATE node_core freertos_kernel)
target_compile_options(sensor_node PRIVATE
    -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/stm32f446re.ld)
target_link_options(sensor_node PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=$<TARGET_FILE_DIR:sensor_node>/sensor_node.map
    -Wl,--print-memory-usage
)
set_target_properties(sensor_node PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

add_custom_command(TARGET sensor_node POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:sensor_node>
            $<TARGET_FILE_DIR:sensor_node>/sensor_node.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:sensor_node>
    COMMENT "Creating sensor_node.bin"
)
