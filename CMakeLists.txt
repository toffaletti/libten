cmake_minimum_required(VERSION 2.8)
add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK -DUSE_UCONTEXT)

add_library(fw task.cc coroutine.cc)
target_link_libraries(fw rt)

add_executable(test_task test_task.cc)
target_link_libraries(test_task fw boost_unit_test_framework)

add_executable(playground playground.cc)
target_link_libraries(playground fw)

add_executable(send send.cc)
target_link_libraries(send fw)