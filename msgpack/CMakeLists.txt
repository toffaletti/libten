project(MSGPACK)
cmake_minimum_required(VERSION 2.8)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(msgpack STATIC
object.cpp
objectc.c
unpack.c
version.c
vrefbuffer.c
zone.c
)
