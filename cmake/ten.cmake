get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/defaults.cmake)
include(FindOpenSSL)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

add_definitions(-D_GLIBCXX_USE_CLOCK_MONOTONIC=1 -D_GLIBCXX_USE_CLOCK_REALTIME=1)

include_directories(${CWD}/..) # for stlencoders and stringencoders
include_directories(${CWD}/../include)
include_directories(${CWD}/../msgpack)
include_directories(${CWD}/../glog)

find_library(JANSSON_LIB jansson)
find_file(JANSSON_INCLUDE jansson.h)
if (JANSSON_LIB AND JANSSON_INCLUDE)
    message(STATUS "Using jansson: ${JANSSON_LIB} ${JANSSON_INCLUDE}")
    add_definitions(-DHAVE_JANSSON)
else (JANSSON_LIB AND JANSSON_INCLUDE)
    message(FATAL_ERROR "jansson not found")
endif (JANSSON_LIB AND JANSSON_INCLUDE)



find_library(CARES_LIB cares)
find_file(CARES_INCLUDE ares.h)
if (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "Using c-ares: ${CARES_LIB} ${CARES_INCLUDE}")
    add_definitions(-DHAVE_CARES)
else (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "c-ares not found")
endif (CARES_LIB AND CARES_INCLUDE)

file(RELATIVE_PATH REL ${CMAKE_CURRENT_SOURCE_DIR} ${CWD}/..)
