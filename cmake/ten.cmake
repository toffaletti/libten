get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/defaults.cmake)
include(FindOpenSSL)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

include_directories(${CWD}/..) # for boost.context
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
#get_filename_component(LIBTEN_INC ${CMAKE_CURRENT_BINARY_DIR}/${REL}/include ABSOLUTE)
#get_filename_component(LIBTEN_LIB ${CMAKE_CURRENT_BINARY_DIR}/${REL}/lib ABSOLUTE)
#include_directories(${LIBTEN_INC})
#link_directories(${LIBTEN_LIB})
