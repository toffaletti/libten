#define BOOST_TEST_MODULE thread test
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

#include "thread.hh"
#include "semaphore.hh"

BOOST_AUTO_TEST_CASE(mutex_test) {
    mutex m;
    mutex::scoped_lock l(m);
    BOOST_CHECK(l.trylock() == false);
}


