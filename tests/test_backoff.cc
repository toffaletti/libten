#include "gtest/gtest.h"
#include "ten/backoff.hh"
#include "ten/logging.hh"
#include <chrono_io>

using namespace ten;
using namespace std::chrono;

TEST(BackoffTest, Test1) {
    auto b = make_backoff(seconds{1}, seconds{60});
    for (auto i=0; i<1000; ++i) {
        seconds delay = b.next_delay();
        EXPECT_TRUE(delay.count() >= 1);
        EXPECT_TRUE(delay.count() <= 60);
    }
}

TEST(BackoffTest, TestMin) {
    auto b = make_backoff(milliseconds{1500}, seconds{60}, 2);
    for (int x = 0; x < 10; ++x) {
        auto delay = b.next_delay();
        VLOG(1) << "delay = " << delay;
        EXPECT_TRUE(delay >= milliseconds{1500} && delay < seconds{60});
    }
}

