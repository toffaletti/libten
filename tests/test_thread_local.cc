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

void my_thread() {
    X *x1 = thread_local_ptr<X>();
    X *x2 = thread_local_ptr<X>();
    BOOST_CHECK_EQUAL(x1, x2);
    // unique tag, this will create a new X
    X *x3 = thread_local_ptr<X, 1234>();
    BOOST_CHECK(x1 != x3);
    // same tag, we'll get the x3 pointer again
    X *x4 = thread_local_ptr<X, 1234>();
    BOOST_CHECK_EQUAL(x3, x4);
    // test constructor arg passing
    int *plain = thread_local_ptr<int>(42);
    BOOST_CHECK_EQUAL(*plain, 42);
}

BOOST_AUTO_TEST_CASE(thread_local_test) {
    std::thread t(my_thread);
    t.join();

    BOOST_CHECK_EQUAL(X::new_count, 2);
    BOOST_CHECK_EQUAL(X::del_count, 2);
}

