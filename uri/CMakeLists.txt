cmake_minimum_required(VERSION 2.8)
project(uri)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    ragel_gen(uri_parser -G2)
else (CMAKE_BUILD_TYPE STREQUAL "Release")
    ragel_gen(uri_parser)
endif (CMAKE_BUILD_TYPE STREQUAL "Release")

add_library(uri STATIC ${CMAKE_CURRENT_BINARY_DIR}/uri_parser.cc)

#add_definitions(-DBOOST_TEST_DYN_LINK)
add_executable(test_uri test_uri.cc)
target_link_libraries(test_uri uri boost_unit_test_framework-mt)

add_test(test_uri test_uri --random)
