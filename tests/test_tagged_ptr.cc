#define BOOST_TEST_MODULE tagged_ptr test
#include <boost/test/unit_test.hpp>
#include <thread>
#include "ten/tagged_ptr.hh"

using namespace ten;

struct foo {
    int a;
    int b;
    int c;
};

BOOST_AUTO_TEST_CASE(tagged_test) {
    std::unique_ptr<foo> p(new foo());
    tagged_ptr<foo> tp(p.get());

    p->a = 1;
    p->b = 2;
    p->c = 3;

    BOOST_CHECK(tp.tag());
    BOOST_CHECK_EQUAL(p.get(), tp.ptr());

    BOOST_CHECK_EQUAL(1, tp->a);
    BOOST_CHECK_EQUAL(2, tp->b);
    BOOST_CHECK_EQUAL(3, tp->c);
}

BOOST_AUTO_TEST_CASE(thread_test) {
    uintptr_t f1 = 0;
    std::thread t1([&] {
            tagged_ptr<foo> p(new foo());
            f1 = p._data;
            });

    uintptr_t f2 = 0;
    std::thread t2([&] {
            tagged_ptr<foo> p(new foo());
            f2 = p._data;
            });

    t1.join();
    t2.join();

    BOOST_CHECK_NE(f1, f2);

    tagged_ptr<foo> tp1((foo*)f1);
    tagged_ptr<foo> tp2((foo*)f2);

    BOOST_CHECK_NE(tp1.tag(), tp2.tag());

    tp1.free();
    tp2.free();
}

