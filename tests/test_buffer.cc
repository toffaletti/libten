#define BOOST_TEST_MODULE buffer test
#include <boost/test/unit_test.hpp>
#include "buffer.hh"

using namespace ten;

BOOST_AUTO_TEST_CASE(test1) {
    auto buf = buffer2::create(100);
}

