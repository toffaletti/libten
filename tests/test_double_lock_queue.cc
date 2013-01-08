#define BOOST_TEST_MODULE llqueue test
#include <boost/test/unit_test.hpp>
#include "t2/double_lock_queue.hh"

BOOST_AUTO_TEST_CASE(dlqueue_test) {
    double_lock_queue<intptr_t> q;
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

    double_lock_queue<int> qq;
    qq.push(1);
    qq.push(2); // should be valgrind clean
}
