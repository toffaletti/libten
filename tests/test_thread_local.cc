#define BOOST_TEST_MODULE thread_local test
#include <boost/test/unit_test.hpp>
#include "ten/thread_local.hh"
#include <thread>
#include <atomic>

using namespace ten;

struct X {
    static std::atomic<int> new_count;
    static std::atomic<int> del_count;

    X() {
        ++new_count;
    }

    ~X() {
        ++del_count;
    }
};

std::atomic<int> X::new_count(0);
std::atomic<int> X::del_count(0);

struct tag1 {};
struct tag2 {};

thread_cached<tag1, X> x1;
thread_cached<tag1, X> x2;

void my_thread() {
    BOOST_CHECK_EQUAL(x1.get(), x2.get());
    // unique tag, this will create a new X
    thread_cached<tag2, X> x3;
    BOOST_CHECK(x1.get() != x3.get());
    // same tag, we'll get the x3 pointer again
    thread_cached<tag2, X> x4;
    BOOST_CHECK_EQUAL(x3.get(), x4.get());
}

BOOST_AUTO_TEST_CASE(thread_local_test) {
    std::thread t(my_thread);
    t.join();

    BOOST_CHECK_EQUAL(X::new_count, 2);
    BOOST_CHECK_EQUAL(X::del_count, 2);
}

