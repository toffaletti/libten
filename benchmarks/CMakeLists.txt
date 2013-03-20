cmake_minimum_required(VERSION 2.8)

add_executable(timer_event_loop EXCLUDE_FROM_ALL timer_event_loop.cc)
target_link_libraries(timer_event_loop ten)

add_executable(server_client EXCLUDE_FROM_ALL server_client.cc)
target_link_libraries(server_client ten)

add_executable(iconcur EXCLUDE_FROM_ALL iconcur.cc)
target_link_libraries(iconcur ten)

add_executable(ring EXCLUDE_FROM_ALL ring.cc)
target_link_libraries(ring ten)

add_executable(iopool EXCLUDE_FROM_ALL iopool.cc)
target_link_libraries(iopool ten boost_program_options)

add_executable(iowait EXCLUDE_FROM_ALL iowait.cc)
target_link_libraries(iowait ten)

add_custom_target(benchmarks DEPENDS
    timer_event_loop
    server_client
    iconcur
    ring
    iopool
    iowait
    )
