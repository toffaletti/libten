cmake_minimum_required(VERSION 2.8)

add_executable(prime-sieve prime-sieve.cc)
target_link_libraries(prime-sieve ten)

add_executable(file-send file-send.cc)
target_link_libraries(file-send ten)

add_executable(old-echo-server old-echo-server.cc)
target_link_libraries(old-echo-server ten)

add_executable(echo-server echo-server.cc)
target_link_libraries(echo-server ten)

add_executable(chat-server chat-server.cc)
target_link_libraries(chat-server ten)

add_executable(stack-guard stack-guard.cc)
target_link_libraries(stack-guard ten)

add_executable(backtrace backtrace.cc)
target_link_libraries(backtrace ten)

add_executable(http-get http-get.cc)
target_link_libraries(http-get ten http uri)

add_executable(http-proxy http-proxy.cc)
target_link_libraries(http-proxy ten http uri)

add_executable(http-server http-server.cc)
target_link_libraries(http-server ten http uri boost_program_options)

add_executable(msg-pack msgpack.cc)
target_link_libraries(msg-pack ten msgpack)

add_executable(rpc-server rpc-server.cc)
target_link_libraries(rpc-server ten msgpack)

# optional
add_executable(https-get EXCLUDE_FROM_ALL https-get.cc ../ssl.cc)
target_link_libraries(https-get ten http uri ssl)
