get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/defaults.cmake)
include(FindOpenSSL)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

include_directories(${CWD}/..) # for stlencoders and stringencoders
include_directories(${CWD}/../include)
include_directories(${CWD}/../jansson)
include_directories(${CWD}/../msgpack)
include_directories(${CWD}/../glog)

find_library(CARES_LIB cares PATHS 
    /usr/lib/x86_64-linux-gnu/
    )
find_file(CARES_INCLUDE ares.h)
if (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "Using c-ares: ${CARES_LIB} ${CARES_INCLUDE}")
    add_definitions(-DHAVE_CARES)
else (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "c-ares not found")
endif (CARES_LIB AND CARES_INCLUDE)

file(RELATIVE_PATH REL ${CMAKE_CURRENT_SOURCE_DIR} ${CWD}/..)
