project(ten)
cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ten.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_subdirectory(glog)
include_directories(glog)

include_directories(boost.context)
set(ASM boost.context/libs/context/src/asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(ten
    error.cc task.cc ioproc.cc net.cc ${ASM}
    boost.context/libs/context/src/fcontext.cpp)
add_dependencies(ten libglog)

target_link_libraries(ten rt glog)

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

add_subdirectory(uri)
add_subdirectory(http)

add_subdirectory(jansson)
add_subdirectory(stringencoders)
add_subdirectory(msgpack)
