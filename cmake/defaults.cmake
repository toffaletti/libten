if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

option(USE_GPROF
    "Profile the project using gprof"
    OFF)
if(USE_GPROF)
    message("Adding profiling info for gprof...")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pg")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif(USE_GPROF)

option(USE_PPROF
    "Profile the project using gprof"
    OFF)
if(USE_PPROF)
    message("Linking with -lprofiler")
    set(EXTRA_LIBS profiler)
endif(USE_PPROF)

if (NOT GCC_FLAGS)
    # general flags for any compilation with gcc/g++
    # someday use "-march=core2 -msse4.1" after retiring old Athlons still in use
    set(GCC_FLAGS "-pthread -mtune=core2")
    set(GCC_FLAGS "${GCC_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter")
    set(GCC_FLAGS "${GCC_FLAGS} -Wpointer-arith -Wcast-align -Wuninitialized -Wwrite-strings")
    #set(GCC_FLAGS "${GCC_FLAGS} -Wshadow")
    set(GXX_FLAGS "-std=c++11 -stdlib=libc++ ${GCC_FLAGS}")
    set(GCC_FLAGS "${GCC_FLAGS} -std=c11 -Wstrict-prototypes -Wmissing-prototypes")

    # profile guided optimization
    if (WITH_PGO STREQUAL "generate")
        set(GCC_FLAGS "${GCC_FLAGS} -fprofile-generate")
    endif (WITH_PGO STREQUAL "generate")

    if (WITH_PGO STREQUAL "use")
        set(GCC_FLAGS"${GCC_FLAGS} -fprofile-use")
    endif (WITH_PGO STREQUAL "use")

    add_definitions(-D_REENTRANT -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_GNU_SOURCE)
    # needed for std::thread features
    add_definitions(-D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD)

    # release- and debug-specific flags for gcc
    set(DEBUG_FLAGS   "-O0 -ggdb -D_DEBUG -rdynamic")
    set(RELEASE_FLAGS "-O2")

    # now that we know what the UB flags are, paste them into the cmake macros

    set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} ${GCC_FLAGS}")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${GXX_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
endif (NOT GCC_FLAGS)

include(CheckIncludeFiles)
check_include_files("valgrind/valgrind.h" HAVE_VALGRIND_H)
if (HAVE_VALGRIND_H)
    message(STATUS "Valgrind found")
else()
    message(STATUS "Valgrind not found")
    add_definitions(-DNVALGRIND)
endif ()
