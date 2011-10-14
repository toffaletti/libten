cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fw.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)

add_definitions(-DBOOST_TEST_DYN_LINK)

configure_make_install(libglog
  glog-0.3.1
  ${CMAKE_CURRENT_BINARY_DIR}/include/glog/logging.h
  --prefix=${CMAKE_CURRENT_BINARY_DIR} --disable-shared --enable-static
)

set(ASM asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(fw error.cc task.cc ioproc.cc net.cc ${ASM})
add_dependencies(fw libglog)

set(GLOG_LIBS glog)
# if libunwind is installed glog configure picks it up
# and you must link with it
find_library(LIBUNWIND unwind)
if (LIBUNWIND)
    set(GLOG_LIBS ${GLOG_LIBS} unwind)
endif (LIBUNWIND)
target_link_libraries(fw rt ${GLOG_LIBS})

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

add_subdirectory(uri)
add_subdirectory(http)

add_subdirectory(jansson)
add_subdirectory(stringencoders)
add_subdirectory(msgpack)

#add_library(fw2 error.cc proc.cc ${ASM})
#add_executable(play error.cc proc.cc ${ASM})
#add_dependencies(play libglog)
#target_link_libraries(play glog)
