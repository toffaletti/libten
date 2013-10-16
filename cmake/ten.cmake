set(TEN_CMAKE_INCLUDED 1)

include(FindOpenSSL)
include(CheckSymbolExists)
include(CheckIncludeFiles)

## boost

set(Boost_FIND_REQUIRED ON)
set(Boost_USE_MULTITHREADED OFF) # Linux doesn't need this
if (BOOST_ROOT)
  # Prevent falling back to system paths when using a custom Boost prefix.
  set(Boost_NO_SYSTEM_PATHS ON)
endif ()
find_package(Boost 1.51.0 COMPONENTS
    context program_options)
if (NOT Boost_FOUND)
    message(FATAL_ERROR, "Boost >= 1.51 not found")
else()
    message(STATUS "boost includes: ${Boost_INCLUDE_DIRS}")
    message(STATUS "boost libs: ${Boost_LIBRARY_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

## jansson

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

## c-ares

find_library(CARES_LIB cares)
find_file(CARES_INCLUDE ares.h)
if (CARES_LIB AND CARES_INCLUDE)
    message(STATUS "Using c-ares: ${CARES_LIB} ${CARES_INCLUDE}")
    add_definitions(-DHAVE_CARES)
else (CARES_LIB AND CARES_INCLUDE)
    message(FATAL_ERROR "c-ares not found")
endif (CARES_LIB AND CARES_INCLUDE)

## valgrind

check_include_files("valgrind/valgrind.h" HAVE_VALGRIND_H)
if (HAVE_VALGRIND_H)
    message(STATUS "Valgrind found")
else()
    message(STATUS "Valgrind not found")
    add_definitions(-DNVALGRIND)
endif ()

## finally, libten

add_definitions(-D_REENTRANT -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_GNU_SOURCE)
# needed for std::thread features
add_definitions(-D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD)

if (NOT TEN_SOURCE_DIR)
    get_filename_component(TEN_SOURCE_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
    get_filename_component(TEN_SOURCE_DIR ${TEN_SOURCE_DIR}/.. ABSOLUTE)
endif ()
include_directories(${TEN_SOURCE_DIR}) # for stlencoders and stringencoders
include_directories(${TEN_SOURCE_DIR}/include)
include_directories(${TEN_SOURCE_DIR}/msgpack)
include_directories(${TEN_SOURCE_DIR}/glog)
