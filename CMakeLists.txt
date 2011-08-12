cmake_minimum_required(VERSION 2.8)
enable_testing()

#add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK -DUSE_UCONTEXT)
#add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK -DUSE_BOOST_FCONTEXT)
add_definitions(-rdynamic -pthread -ggdb -std=gnu++98 -DBOOST_TEST_DYN_LINK -DUSE_BOOST_FCONTEXT)

#include_directories(../boost.context-0.7.3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for boost.context
set(ASM asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(fw task.cc runner.cc ${ASM})
target_link_libraries(fw rt)

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)
