cmake_minimum_required(VERSION 3.16)
project(cbat_testing_recipe LANGUAGES C)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." cbat)
add_executable(writing_tests writing-tests.c)
target_link_libraries(writing_tests PRIVATE cbat::cbat)
set_target_properties(writing_tests PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(writing_tests PRIVATE /W4 /WX)
else()
    target_compile_options(writing_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()
add_test(NAME writing_tests COMMAND writing_tests)
add_test(NAME writing_tests_list COMMAND writing_tests --list)
set_tests_properties(writing_tests_list PROPERTIES
    PASS_REGULAR_EXPRESSION "test_allocation_failures")
add_test(NAME writing_tests_filter COMMAND writing_tests allocation)
set_tests_properties(writing_tests_filter PROPERTIES
    PASS_REGULAR_EXPRESSION "1 passed, 0 failed, 2 filtered out")
