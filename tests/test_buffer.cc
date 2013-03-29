#include "gtest/gtest.h"
#include "ten/buffer.hh"

using namespace ten;

TEST(Buffer, Test1) {
    static std::vector<char> aa(500, 0x41);
    static std::vector<char> bb(1000, 0x42);
    EXPECT_TRUE(std::is_pod<buffer::head>::value);

    buffer b{100};
    b.reserve(1000); // force a realloc
    EXPECT_TRUE(b.end() - b.back() >= 1000);
    std::fill(b.back(), b.end(1000), 0x41);
    b.commit(1000);
    EXPECT_EQ(1000, b.size());
    b.remove(500);
    b.reserve(1000);
    EXPECT_TRUE(b.end() - b.back() >= 1000);
    EXPECT_TRUE(std::equal(b.front(), b.back(), aa.begin()));
    std::fill(b.front(), b.end(500), 0x42);
    b.commit(500);
    EXPECT_EQ(1000, b.size());
    EXPECT_TRUE(std::equal(b.front(), b.back(), bb.begin()));
    b.remove(500);
    b.reserve(1000);
    EXPECT_TRUE(b.end() - b.back() >= 1000);
    EXPECT_THROW(b.remove(20000), std::runtime_error);
    EXPECT_THROW(b.commit(20000), std::runtime_error);
}

TEST(Buffer, Test2) {
    // this is just to see if realloc is returning new pointers
    {
        buffer b{100000};
        b.reserve(100001);
    }

    {
        buffer b{100000};
        b.reserve(100001);
    }
}

