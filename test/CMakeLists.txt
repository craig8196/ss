
cmake_minimum_required(VERSION 3.16)
enable_testing()

project(prove)

add_executable(prove prove.c)
target_include_directories(prove PRIVATE ../include)
target_link_libraries(prove PRIVATE ss)
target_code_coverage(prove)
add_test(NAME prove COMMAND prove)

