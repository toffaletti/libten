#define BOOST_TEST_MODULE ioproc test
#include <boost/test/unit_test.hpp>
#include "ioproc.hh"
#include "descriptors.hh"

using namespace fw;

size_t default_stacksize=4096;

void ioproc_sleeper() {
    auto io = ioproc();
    int ret = iocall<int>(io, std::bind(usleep, 100));
    BOOST_CHECK_EQUAL(ret, 0);
}

BOOST_AUTO_TEST_CASE(ioproc_sleep_test) {
    procmain p;
    taskspawn(ioproc_sleeper);
    p.main();
}

static void dial_google() {
    socket_fd s(AF_INET, SOCK_STREAM);
    // getaddrinfo requires a rather large stack
    auto io = ioproc(8*1024*1024);
    int status = iodial(io, s.fd, "www.google.com", 80);
    BOOST_CHECK_EQUAL(status, 0);
}

BOOST_AUTO_TEST_CASE(task_socket_dial) {
    procmain p;
    taskspawn(dial_google);
    p.main();
}

void ioproc_pool_sleeper(ioproc_pool &pool) {
    ioproc_pool::scoped_resource io(pool);
    intptr_t ret = iocall<int>(io, std::bind(usleep, 100));
    BOOST_CHECK_EQUAL(ret, 0);
}

BOOST_AUTO_TEST_CASE(ioproc_pool_test) {
    procmain p;
    ioproc_pool pool;
    for (int i=0; i<5; i++) {
        taskspawn(std::bind(ioproc_pool_sleeper, std::ref(pool)));
    }
    p.main();
    BOOST_CHECK_EQUAL(pool.size(), 5);
    pool.clear();
}

