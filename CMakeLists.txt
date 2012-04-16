project(ten)
cmake_minimum_required(VERSION 2.8)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ten.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ragel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure.cmake)

if (${CMAKE_EXE_LINKER_FLAGS} MATCHES "-static")
    add_definitions(-DBOOST_TEST_MAIN)
    # must force entire libpthread.a see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52590
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
else ()
    add_definitions(-DBOOST_TEST_DYN_LINK)
endif ()

if (BOOST_HOME AND IS_DIRECTORY ${BOOST_HOME}/boost)
    message(STATUS "Using boost ${BOOST_HOME}")
    include_directories(${BOOST_HOME})

    add_library(boost_unit_test_framework-mt
        ${BOOST_HOME}/libs/test/src/compiler_log_formatter.cpp
        ${BOOST_HOME}/libs/test/src/debug.cpp
        ${BOOST_HOME}/libs/test/src/exception_safety.cpp
        ${BOOST_HOME}/libs/test/src/execution_monitor.cpp
        ${BOOST_HOME}/libs/test/src/framework.cpp
        ${BOOST_HOME}/libs/test/src/interaction_based.cpp
        ${BOOST_HOME}/libs/test/src/logged_expectations.cpp
        ${BOOST_HOME}/libs/test/src/plain_report_formatter.cpp
        ${BOOST_HOME}/libs/test/src/progress_monitor.cpp
        ${BOOST_HOME}/libs/test/src/results_collector.cpp
        ${BOOST_HOME}/libs/test/src/results_reporter.cpp
        ${BOOST_HOME}/libs/test/src/test_tools.cpp
        ${BOOST_HOME}/libs/test/src/unit_test_log.cpp
        ${BOOST_HOME}/libs/test/src/unit_test_main.cpp
        ${BOOST_HOME}/libs/test/src/unit_test_monitor.cpp
        ${BOOST_HOME}/libs/test/src/unit_test_parameters.cpp
        ${BOOST_HOME}/libs/test/src/unit_test_suite.cpp
        ${BOOST_HOME}/libs/test/src/xml_log_formatter.cpp
        ${BOOST_HOME}/libs/test/src/xml_report_formatter.cpp
        )

    add_library(boost_program_options-mt
        ${BOOST_HOME}/libs/program_options/src/cmdline.cpp
        ${BOOST_HOME}/libs/program_options/src/config_file.cpp
        ${BOOST_HOME}/libs/program_options/src/options_description.cpp
        ${BOOST_HOME}/libs/program_options/src/parsers.cpp
        ${BOOST_HOME}/libs/program_options/src/variables_map.cpp
        ${BOOST_HOME}/libs/program_options/src/value_semantic.cpp
        ${BOOST_HOME}/libs/program_options/src/positional_options.cpp
        ${BOOST_HOME}/libs/program_options/src/utf8_codecvt_facet.cpp
        ${BOOST_HOME}/libs/program_options/src/convert.cpp
        ${BOOST_HOME}/libs/program_options/src/winmain.cpp
        ${BOOST_HOME}/libs/program_options/src/split.cpp
        )
else ()
    #message(FATAL_ERROR "Must set BOOST_HOME to a valid boost source dir")
endif()

add_subdirectory(glog)
include_directories(glog)

include_directories(boost.context)
if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(ASM boost.context/libs/context/src/asm/fcontext_x86_64_sysv_elf_gas.S)
else()
    set(ASM boost.context/libs/context/src/asm/fcontext_i386_sysv_elf_gas.S)
endif()
SET_SOURCE_FILES_PROPERTIES(${ASM} PROPERTIES LANGUAGE C)

if (CARES_LIB AND CARES_INCLUDE)
    set(RESOLVER_SRC src/cares.cc)
else(CARES_LIB AND CARES_INCLUDE)
    set(CARES_LIB "")
    set(RESOLVER_SRC src/resolve.cc)
endif (CARES_LIB AND CARES_INCLUDE)

if (OPENSSL_FOUND)
    set(SSL_SRC src/ssl.cc)
endif (OPENSSL_FOUND)

add_definitions(-DHTTP_PARSER_STRICT=0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(ten
    task/rendez.cc
    task/qutex.cc
    task/task.cc
    task/proc.cc
    src/error.cc src/ioproc.cc src/net.cc src/zip.cc src/json.cc ${ASM}
    ${RESOLVER_SRC}
    ${SSL_SRC}
    src/http_message.cc src/http_parser.c
    boost.context/libs/context/src/fcontext.cpp)
add_dependencies(ten libglog)

target_link_libraries(ten glog ${CARES_LIB} rt ${OPENSSL_LIBRARIES})

add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(benchmarks)

add_subdirectory(uri)

add_subdirectory(jansson)
add_subdirectory(stringencoders)
add_subdirectory(msgpack)

add_custom_target(world DEPENDS
    examples
    benchmarks
    )
