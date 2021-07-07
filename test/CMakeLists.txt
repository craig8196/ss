
cmake_minimum_required(VERSION 3.16)
enable_testing()

project(prove)

add_executable(prove prove.c)
target_include_directories(prove PRIVATE ../include)
target_link_libraries(prove PRIVATE ss)
if (CODE_COVERAGE)
    target_code_coverage(prove)
endif()
add_test(NAME prove COMMAND prove)

