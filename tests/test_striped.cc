#include "gtest/gtest.h"
#include "ten/striped.hh"
#include <mutex>

using namespace ten;

TEST(Striped, Test) {
    striped<std::mutex> striped_lock{10};

    for (int i=0; i<100; ++i) {
        std::unique_lock<std::mutex> lock{striped_lock.get(i)};
    }
}
