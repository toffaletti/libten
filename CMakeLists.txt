cmake_minimum_required(VERSION 2.8)
#add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK -DUSE_UCONTEXT)

add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK -DUSE_BOOST_FCONTEXT)
#include_directories(../boost.context-0.7.3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for boost.context
set(ASM asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(fw task.cc runner.cc ${ASM})
target_link_libraries(fw rt)

add_executable(test_descriptors test_descriptors.cc)
target_link_libraries(test_descriptors fw boost_unit_test_framework)

add_executable(test_task test_task.cc)
target_link_libraries(test_task fw boost_unit_test_framework)

add_executable(playground playground.cc)
target_link_libraries(playground fw)
