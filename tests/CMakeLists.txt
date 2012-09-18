cmake_minimum_required(VERSION 2.8)

add_executable(test_descriptors test_descriptors.cc)
target_link_libraries(test_descriptors ten boost_unit_test_framework-mt)

add_executable(test_task test_task.cc)
target_link_libraries(test_task ten boost_unit_test_framework-mt)

add_executable(test_channel test_channel.cc)
target_link_libraries(test_channel ten boost_unit_test_framework-mt)

add_executable(test_ioproc test_ioproc.cc)
target_link_libraries(test_ioproc ten boost_unit_test_framework-mt)

add_executable(test_backoff test_backoff.cc)
target_link_libraries(test_backoff boost_unit_test_framework-mt)

add_executable(test_zip test_zip.cc)
target_link_libraries(test_zip ten boost_unit_test_framework-mt)

add_executable(test_json test_json.cc)
target_link_libraries(test_json ten jansson boost_unit_test_framework-mt)

add_executable(test_buffer test_buffer.cc)
target_link_libraries(test_buffer ten boost_unit_test_framework-mt)

add_executable(test_qutex test_qutex.cc)
target_link_libraries(test_qutex ten boost_unit_test_framework-mt)

add_executable(test_net test_net.cc)
target_link_libraries(test_net ten boost_unit_test_framework-mt)

add_executable(test_http test_http.cc)
target_link_libraries(test_http ten boost_unit_test_framework-mt)

add_executable(test_uri test_uri.cc)
target_link_libraries(test_uri ten boost_unit_test_framework-mt)

add_executable(test_hash_ring test_hash_ring.cc)
target_link_libraries(test_hash_ring ten boost_unit_test_framework-mt)

add_executable(test_llqueue test_llqueue.cc)
target_link_libraries(test_llqueue ten boost_unit_test_framework-mt)

add_executable(test_thread_local test_thread_local.cc)
target_link_libraries(test_thread_local ten boost_unit_test_framework-mt)

add_executable(test_metrics test_metrics.cc)
target_link_libraries(test_metrics ten jansson boost_unit_test_framework-mt)


add_test(test_descriptors test_descriptors --random)
add_test(test_task test_task --random)
add_test(test_channel test_channel --random)
add_test(test_ioproc test_ioproc --random)
add_test(test_backoff test_backoff --random)
add_test(test_zip test_zip --random)
add_test(test_json test_json --random)
add_test(test_buffer test_buffer --random)
add_test(test_qutex test_qutex --random)
add_test(test_net test_net --random)
add_test(test_http test_http --random)
add_test(test_uri test_uri --random)
add_test(test_hash_ring test_hash_ring --random)
add_test(test_llqueue test_llqueue --random)
add_test(test_thread_local test_thread_local --random)
add_test(test_metrics test_metrics --random)

