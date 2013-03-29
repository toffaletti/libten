cmake_minimum_required(VERSION 2.8)

include(ExternalProject)
ExternalProject_Add(googletest
    URL https://googletest.googlecode.com/files/gtest-1.6.0.zip
    URL_MD5 4577b49f2973c90bf9ba69aa8166b786
    # Disable install
    INSTALL_COMMAND ""
    )

ExternalProject_Get_Property(googletest source_dir binary_dir)
include_directories(${source_dir}/include)
link_directories(${binary_dir})

add_executable(test_descriptors test_descriptors.cc)
add_dependencies(test_descriptors googletest)
target_link_libraries(test_descriptors ten gtest gtest_main)

add_executable(test_task test_task.cc)
target_link_libraries(test_task ten gtest gtest_main)

add_executable(test_channel test_channel.cc)
target_link_libraries(test_channel ten gtest gtest_main)

add_executable(test_ioproc test_ioproc.cc)
target_link_libraries(test_ioproc ten gtest gtest_main)

add_executable(test_backoff test_backoff.cc)
target_link_libraries(test_backoff ten gtest gtest_main)

add_executable(test_zip test_zip.cc)
target_link_libraries(test_zip ten gtest gtest_main)

add_executable(test_json test_json.cc)
target_link_libraries(test_json ten jansson gtest gtest_main)

add_executable(test_buffer test_buffer.cc)
target_link_libraries(test_buffer ten gtest gtest_main)

add_executable(test_qutex test_qutex.cc)
target_link_libraries(test_qutex ten gtest gtest_main)

add_executable(test_net test_net.cc)
target_link_libraries(test_net ten gtest gtest_main)

add_executable(test_http test_http.cc)
target_link_libraries(test_http ten gtest gtest_main)

add_executable(test_uri test_uri.cc)
target_link_libraries(test_uri ten gtest gtest_main)

add_executable(test_hash_ring test_hash_ring.cc)
target_link_libraries(test_hash_ring ten gtest gtest_main)

add_executable(test_llqueue test_llqueue.cc)
target_link_libraries(test_llqueue ten gtest gtest_main)

add_executable(test_thread_local test_thread_local.cc)
target_link_libraries(test_thread_local ten gtest gtest_main)

add_executable(test_metrics test_metrics.cc)
target_link_libraries(test_metrics ten jansson gtest gtest_main)

set(test_options --gtest_shuffle)
add_test(test_descriptors test_descriptors ${test_options})
add_test(test_task test_task ${test_options})
add_test(test_channel test_channel ${test_options})
add_test(test_ioproc test_ioproc ${test_options})
add_test(test_backoff test_backoff ${test_options})
add_test(test_zip test_zip ${test_options})
add_test(test_json test_json ${test_options})
add_test(test_buffer test_buffer ${test_options})
add_test(test_qutex test_qutex ${test_options})
add_test(test_net test_net ${test_options})
add_test(test_http test_http ${test_options})
add_test(test_uri test_uri ${test_options})
add_test(test_hash_ring test_hash_ring ${test_options})
add_test(test_llqueue test_llqueue ${test_options})
add_test(test_thread_local test_thread_local ${test_options})
add_test(test_metrics test_metrics ${test_options})

