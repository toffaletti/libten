project(ten)
cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ten.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)

if (${CMAKE_EXE_LINKER_FLAGS} MATCHES "-static")
    add_definitions(-DBOOST_TEST_MAIN)
    # must force entire libpthread.a see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52590
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
else ()
    add_definitions(-DBOOST_TEST_DYN_LINK)
endif ()

set(Boost_FIND_REQUIRED ON)
set(Boost_USE_MULTITHREADED OFF) # Linux doesn't need this
if (BOOST_ROOT)
  # Prevent falling back to system paths when using a custom Boost prefix.
  set(Boost_NO_SYSTEM_PATHS ON)
endif ()

find_package(Boost 1.51.0 COMPONENTS
    context unit_test_framework program_options)
if (NOT Boost_FOUND)
    message(FATAL_ERROR, "Boost >= 1.51 not found")
else()
    message(STATUS "boost includes: ${Boost_INCLUDE_DIRS}")
    message(STATUS "boost libs: ${Boost_LIBRARY_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

#set (CMAKE_EXE_LINKER_FLAGS -lc++abi)
#set (CMAKE_SHARED_LINKER_FLAGS -lc++abi)
#set (CMAKE_MODULE_LINKER_FLAGS -lc++abi)

add_subdirectory(glog)

if (CARES_LIB AND CARES_INCLUDE)
    set(RESOLVER_SRC src/cares.cc)
else()
    set(CARES_LIB "")
    set(RESOLVER_SRC src/resolve.cc)
endif ()

if (OPENSSL_FOUND)
    set(SSL_SRC src/ssl.cc)
endif (OPENSSL_FOUND)

add_definitions(-DHTTP_PARSER_STRICT=0)

add_subdirectory(msgpack)
add_subdirectory(src)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/src/uri_parser.cc PROPERTIES GENERATED 1)

add_library(ten
    src/rendez.cc
    src/qutex.cc
    src/task.cc
    src/proc.cc
    src/error.cc
    src/ioproc.cc
    src/net.cc
    src/zip.cc
    src/json.cc
    src/metrics.cc
    src/term.cc
    ${ASM}
    ${RESOLVER_SRC}
    ${SSL_SRC}
    src/http_message.cc src/http_parser.c
    ${CMAKE_CURRENT_BINARY_DIR}/src/uri_parser.cc
    )
add_dependencies(ten libglog ragel_uri_parser boost_context)

# TODO: compile miniz separately, so warnings remain high for ten-specific code in zip.cc
set_source_files_properties(src/zip.cc PROPERTIES
  COMPILE_FLAGS "-fno-strict-aliasing -Wno-strict-aliasing -Wno-attributes -Wno-extra")

target_link_libraries(ten glog boost_context ${CARES_LIB} rt ${OPENSSL_LIBRARIES})

if (CMAKE_PROJECT_NAME STREQUAL "ten")
    add_subdirectory(examples)
    add_subdirectory(tests)
    add_subdirectory(benchmarks)

    add_custom_target(world DEPENDS
        examples
        benchmarks
        )
endif ()

