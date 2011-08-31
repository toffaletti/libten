cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fw.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)

add_definitions(-DBOOST_TEST_DYN_LINK)

configure_make_install(libglog
  glog-0.3.1
  ${CMAKE_CURRENT_BINARY_DIR}/include/glog/logging.h
  --prefix=${CMAKE_CURRENT_BINARY_DIR} --disable-shared --enable-static
)

set(ASM asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(fw error.cc task.cc runner.cc ${ASM})
add_dependencies(fw libglog)
target_link_libraries(fw rt glog)

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

add_subdirectory(uri)
add_subdirectory(http)
