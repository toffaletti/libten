project(jansson)
cmake_minimum_required(VERSION 2.8)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DJSON_HAVE_LOCALECONV)

add_library(jansson STATIC
dump.c
error.c
hashtable.c
load.c
memory.c
pack_unpack.c
strbuffer.c
utf.c
value.c
strconv.c
)
