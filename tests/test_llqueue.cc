#include "gtest/gtest.h"
#include "ten/llqueue.hh"

using namespace ten;

TEST(LowLockQueue, Test) {
    llqueue<intptr_t> q;
    q.push(20);
    q.push(21);
    q.push(22);
    q.push(23);
    intptr_t v = 0;
    ASSERT_TRUE(q.pop(v));
    EXPECT_EQ(20, v);

    ASSERT_TRUE(q.pop(v));
    EXPECT_EQ(21, v);

    ASSERT_TRUE(q.pop(v));
    EXPECT_EQ(22, v);

    ASSERT_TRUE(q.pop(v));
    EXPECT_EQ(23, v);

    ASSERT_TRUE(!q.pop(v));
}
