cmake_minimum_required(VERSION 2.8)
add_definitions(-ggdb -std=gnu++0x -DBOOST_TEST_DYN_LINK)

add_executable(test_thread test_thread.cc thread.cc)
#target_link_libraries(test_thread rt boost_unit_test_framework-mt)
target_link_libraries(test_thread rt boost_unit_test_framework)

add_executable(test test.cc thread.cc)
target_link_libraries(test rt)

add_executable(send send.cc)
target_link_libraries(send rt)