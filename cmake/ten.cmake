get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/defaults.cmake)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

include_directories(${CWD}/..) # for boost.context
include_directories(${CWD}/../jansson)
include_directories(${CWD}/../msgpack)

file(RELATIVE_PATH REL ${CMAKE_CURRENT_SOURCE_DIR} ${CWD}/..)
get_filename_component(LIBTEN_INC ${CMAKE_CURRENT_BINARY_DIR}/${REL}/include ABSOLUTE)
get_filename_component(LIBTEN_LIB ${CMAKE_CURRENT_BINARY_DIR}/${REL}/lib ABSOLUTE)

include_directories(${LIBTEN_INC})
link_directories(${LIBTEN_LIB})
