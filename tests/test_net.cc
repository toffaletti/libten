#define BOOST_TEST_MODULE net test
#include <boost/test/unit_test.hpp>
#include "net.hh"

using namespace ten;

const size_t default_stacksize=256*1024;

static void dial_google() {
    socket_fd s(AF_INET, SOCK_STREAM);
    int status = netdial(s.fd, "www.google.com", 80);
    BOOST_CHECK_EQUAL(status, 0);
}


BOOST_AUTO_TEST_CASE(task_socket_dial) {
    procmain p;
    taskspawn(dial_google);
    p.main();
}

