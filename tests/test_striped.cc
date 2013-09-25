#include "gtest/gtest.h"
#include "ten/striped.hh"
#include <mutex>

using namespace ten;

TEST(StripedLock, Test) {
    striped<std::mutex> striped_lock{10};

    for (int i=0; i<100; ++i) {
        std::unique_lock<std::mutex> lock{striped_lock.get(i)};
    }
}

TEST(StripedMultiGet, Test) {
    striped<int> striped_int{10};

    {
        std::vector<size_t> r1 = striped_int.multi_get(1, 2, 3, 4);
        std::vector<size_t> r2 = striped_int.multi_get(4, 3, 1, 2);
        EXPECT_EQ(r1, r2);
    }

    {
        std::vector<size_t> r1 = striped_int.multi_get(1, 21, 305, 49);
        std::vector<size_t> r2 = striped_int.multi_get(49, 305, 1, 21);
        EXPECT_EQ(r1, r2);
    }

}
