cmake_minimum_required(VERSION 2.8)
add_definitions(-ggdb)
add_executable(test test.cc)
target_link_libraries(test rt)