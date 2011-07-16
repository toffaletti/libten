cmake_minimum_required(VERSION 2.8)
add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK)

add_library(fw task.cc coroutine.cc)
target_link_libraries(fw rt)

add_executable(test_thread test_thread.cc)
target_link_libraries(test_thread fw boost_unit_test_framework)

add_executable(test test.cc)
target_link_libraries(test fw)

add_executable(send send.cc)
target_link_libraries(send fw)