cmake_minimum_required(VERSION 3.16)
project(unialloc)

enable_testing()
add_subdirectory(src)
add_subdirectory(test/ctest/expect_fail)
add_subdirectory(test/ctest/expect_pass)
