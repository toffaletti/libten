project(glog)
cmake_minimum_required(VERSION 2.8)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(glog STATIC
demangle.cc
logging.cc
raw_logging.cc
signalhandler.cc
symbolize.cc
utilities.cc
vlog_is_on.cc
)
