FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

set(UNIT_TESTS
    test_alarm
    test_crc8
    test_line_buffer
    test_moving_average
    test_node
    test_sht3x
)

foreach(name IN LISTS UNIT_TESTS)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE node_core unity)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
