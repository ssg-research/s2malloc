cmake_minimum_required(VERSION 3.16)
project(unialloc)

set(CMAKE_C_STANDARD 11)
enable_testing()

include_directories(../../../include)

file(GLOB tests "*.c")
message(STATUS "files: ${tests}")

foreach(test ${tests})
    get_filename_component(test_case ${test} NAME_WLE)
    list(APPEND test_cases ${test_case})
endforeach()

foreach (test_case ${test_cases})
    add_executable(${test_case} ${test_case}.c)
    target_link_libraries(${test_case} flmalloc)
    add_test(${test_case} ${test_case})
    set_tests_properties(${test_case} PROPERTIES WILL_FAIL TRUE)
endforeach()
