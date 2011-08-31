get_filename_component(CWD ${CMAKE_CURRENT_LIST_FILE} PATH)
include(${CWD}/flags.cmake)

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

include_directories(${CWD}/..) # for boost.context

file(RELATIVE_PATH REL ${CMAKE_CURRENT_SOURCE_DIR} ${CWD}/..)
get_filename_component(FW_INC ${CMAKE_CURRENT_BINARY_DIR}/${REL}/include ABSOLUTE)
get_filename_component(FW_LIB ${CMAKE_CURRENT_BINARY_DIR}/${REL}/lib ABSOLUTE)

include_directories(${FW_INC})
link_directories(${FW_LIB})
