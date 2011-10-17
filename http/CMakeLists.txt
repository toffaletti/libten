cmake_minimum_required(VERSION 2.8)
project(http)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DHTTP_PARSER_STRICT=0)
add_library(http http_message.cc http_parser.c)
add_executable(test_http test_http.cc)
target_link_libraries(test_http http fw boost_unit_test_framework-mt)

add_test(test_http test_http --random)
