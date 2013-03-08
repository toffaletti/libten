project(glog)
cmake_minimum_required(VERSION 2.8)

add_definitions(-DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(glog STATIC
signalhandler.cc
vlog_is_on.cc
raw_logging.cc
logging.cc
utilities.cc
symbolize.cc
demangle.cc
)
