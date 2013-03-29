#include "gtest/gtest.h"
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
    EXPECT_EQ(x1.get(), x2.get());
    // unique tag, this will create a new X
    thread_cached<tag2, X> x3;
    EXPECT_NE(x1.get(), x3.get());
    // same tag, we'll get the x3 pointer again
    thread_cached<tag2, X> x4;
    EXPECT_EQ(x3.get(), x4.get());
}

TEST(ThreadLocal, Test1) {
    std::thread t(my_thread);
    t.join();

    EXPECT_EQ(X::new_count, 2);
    EXPECT_EQ(X::del_count, 2);
}

