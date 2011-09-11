cmake_minimum_required(VERSION 2.8)

add_executable(prime-sieve prime-sieve.cc)
target_link_libraries(prime-sieve fw)

add_executable(file-send file-send.cc)
target_link_libraries(file-send fw)

add_executable(old-echo-server old-echo-server.cc)
target_link_libraries(old-echo-server fw)

add_executable(echo-server echo-server.cc)
target_link_libraries(echo-server fw)

add_executable(chat-server chat-server.cc)
target_link_libraries(chat-server fw)

add_executable(stack-guard stack-guard.cc)
target_link_libraries(stack-guard fw)

add_executable(backtrace backtrace.cc)
target_link_libraries(backtrace fw)

add_executable(http-get http-get.cc)
target_link_libraries(http-get fw http uri)

add_executable(http-proxy http-proxy.cc)
target_link_libraries(http-proxy fw http uri)

add_executable(msg-pack msgpack.cc)
target_link_libraries(msg-pack fw msgpack)
