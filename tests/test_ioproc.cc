#include "gtest/gtest.h"
#include "ten/ioproc.hh"
#include "ten/descriptors.hh"

using namespace ten;

static void ioproc_sleeper() {
    ioproc io;
    int ret = iocall(io, std::bind(usleep, 100));
    EXPECT_EQ(ret, 0);
}

TEST(IoProc, Sleep) {
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
        EXPECT_EQ(0, r);
    }
}

TEST(IoProc, ThreadPool) {
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
    EXPECT_THROW(iocall(io, fail), std::runtime_error);
}

TEST(IoProc, Failure) {
    task::main([] {
        task::spawn(ioproc_failure);
    });
}
