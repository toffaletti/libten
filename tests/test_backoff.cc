#define BOOST_TEST_MODULE backoff test
#include <boost/test/unit_test.hpp>
#include "ten/backoff.hh"

using namespace ten;
using namespace std::chrono;

BOOST_AUTO_TEST_CASE(backoff_test) {
    backoff<seconds> b(seconds(1), seconds(60));
    for (auto i=0; i<1000; ++i) {
        seconds delay = b.next_delay();
        BOOST_REQUIRE(delay.count() >= 1);
        BOOST_REQUIRE(delay.count() <= 60);
    }
}

BOOST_AUTO_TEST_CASE(backoff_test_min) {
    backoff<seconds> b(seconds(1), seconds(60));
    seconds delay = b.next_delay();
    BOOST_MESSAGE("delay=" << delay.count());
    BOOST_REQUIRE(delay.count() >= 1 && delay.count() < 60);
}

