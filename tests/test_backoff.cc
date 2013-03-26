#define BOOST_TEST_MODULE backoff test
#include <boost/test/unit_test.hpp>
#include "ten/backoff.hh"
#include <chrono_io>

using namespace ten;
using namespace std::chrono;

BOOST_AUTO_TEST_CASE(backoff_test) {
    auto b = make_backoff(seconds{1}, seconds{60});
    for (auto i=0; i<1000; ++i) {
        seconds delay = b.next_delay();
        BOOST_REQUIRE(delay.count() >= 1);
        BOOST_REQUIRE(delay.count() <= 60);
    }
}

BOOST_AUTO_TEST_CASE(backoff_test_min) {
    auto b = make_backoff(milliseconds{1500}, seconds{60}, 2);
    for (int x = 0; x < 10; ++x) {
        auto delay = b.next_delay();
        BOOST_MESSAGE("delay = " << delay);
        BOOST_REQUIRE(delay >= milliseconds{1500} && delay < seconds{60});
    }
}

