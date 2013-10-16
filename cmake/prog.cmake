# For building programs inside libten;
#  these will only ever be tests, most likely

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

# general flags for any compilation with gcc/g++
set(GNU_FLAGS "-pthread")
set(GNU_FLAGS "${GNU_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter")
set(GNU_FLAGS "${GNU_FLAGS} -Wpointer-arith -Wcast-align -Wuninitialized -Wwrite-strings")

# profile guided optimization
if (WITH_PGO STREQUAL "generate")
    set(GNU_FLAGS "${GNU_FLAGS} -fprofile-generate")
endif (WITH_PGO STREQUAL "generate")

if (WITH_PGO STREQUAL "use")
    set(GNU_FLAGS "${GNU_FLAGS} -fprofile-use")
endif (WITH_PGO STREQUAL "use")

set(GXX_FLAGS "-std=c++11 ${GNU_FLAGS} -Wno-unused-local-typedefs")
set(GCC_FLAGS "-std=c11   ${GNU_FLAGS} -Wstrict-prototypes -Wmissing-prototypes")

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
