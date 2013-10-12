cmake_minimum_required(VERSION 2.8)

#include(ExternalProject)
include(${CMAKE_SOURCE_DIR}/cmake/ExternalProject.cmake)
ExternalProject_Add(googletest
    SVN_REPOSITORY http://googletest.googlecode.com/svn/trunk
    # release 1.7.0
    SVN_REVISION -r664
    # Disable install
    INSTALL_COMMAND ""
    # Disable update
    UPDATE_COMMAND ""
    )

ExternalProject_Get_Property(googletest source_dir binary_dir)
include_directories(${source_dir}/include)
link_directories(${binary_dir})

include(${CMAKE_SOURCE_DIR}/cmake/ParseArguments.cmake)

function(add_gtest gtest_name)
    PARSE_ARGUMENTS(GTEST "LIBS" "" ${ARGN})
    add_executable(${gtest_name} "${gtest_name}.cc")
    add_dependencies(${gtest_name} googletest)
    target_link_libraries(${gtest_name} gtest gtest_main ${GTEST_LIBS})
    add_test(${gtest_name} ${gtest_name} --gtest_shuffle)
endfunction(add_gtest)

add_gtest(test_descriptors LIBS ten)
add_gtest(test_task LIBS ten)
add_gtest(test_channel LIBS ten)
add_gtest(test_ioproc LIBS ten)
add_gtest(test_backoff LIBS ten)
add_gtest(test_zip LIBS ten)
add_gtest(test_json LIBS ten jansson)
add_gtest(test_buffer LIBS ten)
add_gtest(test_qutex LIBS ten)
add_gtest(test_net LIBS ten)
add_gtest(test_http LIBS ten)
add_gtest(test_uri LIBS ten)
add_gtest(test_hash_ring LIBS ten)
add_gtest(test_llqueue LIBS ten)
add_gtest(test_thread_local LIBS ten)
add_gtest(test_metrics LIBS ten jansson)
add_gtest(test_mpmc_queue LIBS ten)
add_gtest(test_mpsc_queue LIBS ten)
add_gtest(test_striped LIBS ten)

