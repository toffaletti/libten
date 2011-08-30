cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${PROJECT_BINARY_DIR}/lib)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

# general flags for any compilation with gcc/g++
set(GCC_FLAGS "-pthread -march=core2")
set(GCC_FLAGS "${GCC_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter")
set(GCC_FLAGS "${GCC_FLAGS} -Wpointer-arith -Wcast-align -Wuninitialized -Wwrite-strings")
#set(GXX_FLAGS "-std=gnu++0x ${GCC_FLAGS}")
set(GXX_FLAGS "-std=gnu++98 ${GCC_FLAGS}")
set(GCC_FLAGS "${GCC_FLAGS} -Wstrict-prototypes -Wmissing-prototypes")

# profile guided optimization
if (WITH_PGO STREQUAL "generate")
    set(GCC_FLAGS "${GCC_FLAGS} -fprofile-generate")
endif (WITH_PGO STREQUAL "generate")

if (WITH_PGO STREQUAL "use")
    set(GCC_FLAGS"${GCC_FLAGS} -fprofile-use")
endif (WITH_PGO STREQUAL "use")

add_definitions(-D_REENTRANT -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_GNU_SOURCE)

# release- and debug-specific flags for gcc
set(DEBUG_FLAGS   "-O0 -ggdb -D_DEBUG -rdynamic")
set(RELEASE_FLAGS "-O3 -ggdb")

# now that we know what the UB flags are, paste them into the cmake macros

set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} ${GCC_FLAGS}")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${GXX_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")

add_definitions(-DBOOST_TEST_DYN_LINK)
#add_definitions(-DUSE_UCONTEXT)
add_definitions(-DUSE_BOOST_FCONTEXT)

#include_directories(../boost.context-0.7.3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for boost.context

configure_make_install(libglog
  glog-0.3.1
  ${CMAKE_CURRENT_BINARY_DIR}/include/glog/logging.h
  --prefix=${CMAKE_CURRENT_BINARY_DIR} --disable-shared --enable-static
)

set(ASM asm/fcontext_x86_64_sysv_elf_gas.S)
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

add_library(fw error.cc task.cc runner.cc ${ASM})
add_dependencies(fw libglog)
target_link_libraries(fw rt glog)

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

add_subdirectory(uri)
add_subdirectory(http)
