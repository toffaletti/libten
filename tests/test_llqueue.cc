#define BOOST_TEST_MODULE llqueue test
#include <boost/test/unit_test.hpp>
#include "ten/llqueue.hh"

using namespace ten;

BOOST_AUTO_TEST_CASE(llqueue_test) {
    llqueue<intptr_t> q;
    q.push(20);
    q.push(21);
    q.push(22);
    q.push(23);
    intptr_t v = 0;
    BOOST_CHECK(q.pop(v));
    BOOST_CHECK_EQUAL(20, v);

    BOOST_CHECK(q.pop(v));
    BOOST_CHECK_EQUAL(21, v);

    BOOST_CHECK(q.pop(v));
    BOOST_CHECK_EQUAL(22, v);

    BOOST_CHECK(q.pop(v));
    BOOST_CHECK_EQUAL(23, v);

    BOOST_CHECK(!q.pop(v));
}
