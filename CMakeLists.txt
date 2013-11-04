project(TEN)
cmake_minimum_required(VERSION 2.8)
enable_testing()

if (NOT TEN_SUBPROJECT)
  include(cmake/prog.cmake)
endif ()
include(cmake/ten.cmake)
include(cmake/ragel.cmake)
include(cmake/configure.cmake)

#set (CMAKE_EXE_LINKER_FLAGS -lc++abi)
#set (CMAKE_SHARED_LINKER_FLAGS -lc++abi)
#set (CMAKE_MODULE_LINKER_FLAGS -lc++abi)

add_subdirectory(double-conversion)
add_subdirectory(glog)

if (OPENSSL_FOUND)
    set(SSL_SRC src/ssl.cc)
else (OPENSSL_FOUND)
    message(FATAL_ERROR "openssl not found")
endif (OPENSSL_FOUND)

add_definitions(-DHTTP_PARSER_STRICT=0)
if (TASK_TRACE)
    message(STATUS "Task tracing: on")
    add_definitions(-DTEN_TASK_TRACE)
else (TASK_TRACE)
    message(STATUS "Task tracing: off")
endif (TASK_TRACE)

add_subdirectory(msgpack)
add_subdirectory(src)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/src/uri_parser.cc PROPERTIES GENERATED 1)

# for double-conversion.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/double-conversion/src/)

add_library(ten
    src/deadline.cc
    src/http_message.cc
    src/ioproc.cc
    src/json.cc
    src/jsonstream.cc
    src/metrics.cc
    src/ssl.cc
    src/term.cc
    ${CMAKE_CURRENT_BINARY_DIR}/src/uri_parser.cc
    src/zip.cc
    src/http_parser.c
    src/rendez.cc
    src/qutex.cc
    src/cares.cc
    src/net.cc
    src/compat.cc
    src/kernel.cc
    src/thread_context.cc
    src/stack_alloc.cc
    src/task.cc
    src/scheduler.cc
    src/io.cc
    src/error.cc
    src/context.cc
    src/app.cc
    src/chrono_io.cpp
    )
add_dependencies(ten libglog ragel_uri_parser double-conversion)

# TODO: compile miniz separately, so warnings remain high for ten-specific code in zip.cc
set_source_files_properties(src/zip.cc PROPERTIES
  COMPILE_FLAGS "-fno-strict-aliasing -Wno-strict-aliasing -Wno-attributes -Wno-extra")

target_link_libraries(ten glog double-conversion boost_context ${CARES_LIB} rt ${OPENSSL_LIBRARIES})

if (NOT TEN_SUBPROJECT)
    add_subdirectory(examples)
    add_subdirectory(tests)
    add_subdirectory(benchmarks)

    add_custom_target(world DEPENDS
        examples
        benchmarks
        )
endif ()

