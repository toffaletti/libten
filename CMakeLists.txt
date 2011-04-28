cmake_minimum_required(VERSION 2.8)
add_definitions(-ggdb -std=gnu++0x)

add_executable(test test.cc)
target_link_libraries(test rt)

add_executable(send send.cc)
target_link_libraries(send rt)