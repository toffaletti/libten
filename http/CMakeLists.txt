cmake_minimum_required(VERSION 2.8)
project(http)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
ragel_gen(http_request_parser)
ragel_gen(http_response_parser)

add_library(http http_message.cc http_request_parser.cc http_response_parser.cc)
add_executable(test_http test_http.cc)
target_link_libraries(test_http http boost_unit_test_framework-mt)

add_test(test_http test_http --random)
