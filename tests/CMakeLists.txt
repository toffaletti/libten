cmake_minimum_required(VERSION 2.8)

add_executable(test_descriptors test_descriptors.cc)
target_link_libraries(test_descriptors fw boost_unit_test_framework)

add_executable(test_task test_task.cc)
target_link_libraries(test_task fw boost_unit_test_framework)

add_executable(test_channel test_channel.cc)
target_link_libraries(test_channel fw boost_unit_test_framework)

add_executable(test_ioproc test_ioproc.cc)
target_link_libraries(test_ioproc fw boost_unit_test_framework)

add_test(test_descriptors test_descriptors --random)
add_test(test_task test_task --random)
add_test(test_channel test_channel --random)
add_test(test_ioproc test_ioproc --random)
