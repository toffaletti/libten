if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

if (NOT GCC_FLAGS)
    # general flags for any compilation with gcc/g++
    set(GCC_FLAGS "-pthread -march=core2")
    set(GCC_FLAGS "${GCC_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter")
    set(GCC_FLAGS "${GCC_FLAGS} -Wpointer-arith -Wcast-align -Wuninitialized -Wwrite-strings")
    set(GXX_FLAGS "-std=gnu++0x ${GCC_FLAGS}")
    set(GCC_FLAGS "${GCC_FLAGS} -std=gnu99 -Wstrict-prototypes -Wmissing-prototypes")

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
    set(RELEASE_FLAGS "-O3 -ggdb")

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
if (NOT HAVE_VALGRIND_H)
    add_definitions(-DNVALGRIND)
endif (NOT HAVE_VALGRIND_H)
