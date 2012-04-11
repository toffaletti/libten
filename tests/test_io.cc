#define BOOST_TEST_MODULE io test
#include <boost/test/unit_test.hpp>
#include "descriptors.hh"
#include "io.hh"

using namespace ten;

static void hello_world(io::writer &w) {
    w.write("hello ");
    w.write("world!\n");
}

BOOST_AUTO_TEST_CASE(basic_io) {
    io::fd_base f(1);
    hello_world(f);

    io::memstream m;
    hello_world(m);

    m.flush();
    f.write(m.ptr(), m.size());
}
