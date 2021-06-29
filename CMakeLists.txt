
cmake_minimum_required(VERSION 3.16)
enable_testing()
project(ss VERSION 0.0.1 DESCRIPTION "Simple string library.")

include(GNUInstallDirs)

if(CODE_COVERAGE)
    set(CMAKE_BUILD_TYPE DEBUG)
    include(cmake-scripts/code-coverage.cmake)
endif()

add_subdirectory(test)

add_library(ss SHARED src/ss.c)
set_target_properties(ss PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(ss PROPERTIES SOVERSION 0)
set_target_properties(ss PROPERTIES PUBLIC_HEADER include/ss.h)
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_C_FLAGS_DEBUG "-O0")

configure_file(ss.pc.in ss.pc @ONLY)

target_include_directories(ss PRIVATE include)
target_include_directories(ss PRIVATE src)

if(CODE_COVERAGE)
    target_code_coverage(ss)
endif()

if(BUILD_DOCUMENTATION)
    set(MYPROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set(MYPROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(PROJECT_README_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    configure_file(${doxyfile_in} ${doxyfile} @ONLY)
    include(cmake-scripts/doxygen.cmake)
    build_docs(ADD_TO_DOC ss)
endif()

install(TARGETS ss
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/ss.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


