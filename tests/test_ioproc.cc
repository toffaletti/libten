#define BOOST_TEST_MODULE ioproc test
#include <boost/test/unit_test.hpp>
#include "ten/ioproc.hh"
#include "ten/descriptors.hh"

using namespace ten;

static void ioproc_sleeper() {
    ioproc io;
    int ret = iocall(io, std::bind(usleep, 100));
    BOOST_CHECK_EQUAL(ret, 0);
}

BOOST_AUTO_TEST_CASE(ioproc_sleep_test) {
    task::main([] {
        task::spawn(ioproc_sleeper);
    });
}

static void test_pool() {
    ioproc io{nostacksize, 4};
    iochannel reply_chan;

    for (int i=0; i<4; ++i) {
        iocallasync(io, std::bind(usleep, 100), reply_chan);
    }

    for (int i=0; i<4; ++i) {
        int r = iowait<int>(reply_chan);
        // usleep should return 0
        BOOST_CHECK_EQUAL(0, r);
    }
}

BOOST_AUTO_TEST_CASE(ioproc_thread_pool) {
    task::main([] {
        task::spawn(test_pool);
    });
}

static void fail() {
    throw std::runtime_error("fail has failed");
}

static void ioproc_failure() {
    ioproc io;
    std::string errmsg;
    try {
        iocall(io, fail);
        BOOST_CHECK(false);
    } catch (std::runtime_error &e) {
        errmsg = e.what();
    }
    BOOST_CHECK_EQUAL(errmsg, "fail has failed");
}

BOOST_AUTO_TEST_CASE(ioproc_error_test) {
    task::main([] {
        task::spawn(ioproc_failure);
    });
}
