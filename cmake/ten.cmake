get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/defaults.cmake)
include(FindOpenSSL)
include(CheckSymbolExists)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

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

set(CMAKE_REQUIRED_LIBRARIES "jansson")
CHECK_SYMBOL_EXISTS(json_string_length "jansson.h" HAVE_JANSSON_STRLEN)
if (HAVE_JANSSON_STRLEN)
    add_definitions(-DHAVE_JANSSON_STRLEN)
endif (HAVE_JANSSON_STRLEN)

find_library(CARES_LIB cares)
find_file(CARES_INCLUDE ares.h)
if (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "Using c-ares: ${CARES_LIB} ${CARES_INCLUDE}")
    add_definitions(-DHAVE_CARES)
else (CARES_LIB AND CARES_INCLUDE)
    message(FATAL_ERROR "c-ares not found")
endif (CARES_LIB AND CARES_INCLUDE)

file(RELATIVE_PATH REL ${CMAKE_CURRENT_SOURCE_DIR} ${CWD}/..)
