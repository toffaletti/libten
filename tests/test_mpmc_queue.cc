#include "gtest/gtest.h"
#include "ten/mpmc_bounded_queue.hh"
#include <array>
#include <thread>

using namespace ten;
using std::size_t;

TEST(MPMCQueue, Test) {
    typedef mpmc_bounded_queue<int, 128, uint8_t> queue_t;
    static size_t const thread_count = 4;
    static size_t const batch_size = 1;
    static size_t const iter_count = 1000000;

    queue_t queue;

    auto thread_func = [](queue_t &queue) {
        int data;

        for (size_t iter = 0; iter != iter_count; ++iter) {
            for (size_t i = 0; i != batch_size; i += 1) {
                while (!queue.enqueue(i)) {
                    std::this_thread::yield();
                }
            }
            for (size_t i = 0; i != batch_size; i += 1) {
                while (!queue.dequeue(data)) {
                    std::this_thread::yield();
                }
            }
        }
    };

    std::array<std::thread, thread_count> threads;
    for (size_t i = 0; i != thread_count; ++i) {
        threads[i] = std::move(std::thread(
            std::bind(thread_func, std::ref(queue))
            ));
    }

    for (size_t i = 0; i != thread_count; ++i) {
        threads[i].join();
    }
}

TEST(MPMCQueue, QueueOverflow8) {
    typedef mpmc_bounded_queue<int, 8, uint8_t> queue_t;
    int i = 0;

    queue_t queue;

    for (size_t x=1; x<10; ++x) {
        for (; i < 10000; ++i) {
            if (!queue.enqueue(i)) break;
        }

        while (queue.dequeue(i)) {
        }

        ASSERT_EQ((x*queue_t::buffer_size)-x, i);
    }
}

TEST(MPMCQueue, QueueOverflow64) {
    typedef mpmc_bounded_queue<int, 64, uint8_t> queue_t;
    int i = 0;

    queue_t queue;

    for (size_t x=1; x<10; ++x) {
        for (; i < 10000; ++i) {
            if (!queue.enqueue(i)) break;
        }

        while (queue.dequeue(i)) {
        }

        ASSERT_EQ((x*queue_t::buffer_size)-x, i);
    }
}

TEST(MPMCQueue, QueueOverflow128) {
    typedef mpmc_bounded_queue<int, 128, uint8_t> queue_t;
    int i = 0;

    queue_t queue;

    for (size_t x=1; x<10; ++x) {
        for (; i < 10000; ++i) {
            if (!queue.enqueue(i)) break;
        }

        while (queue.dequeue(i)) {
        }

        ASSERT_EQ((x*queue_t::buffer_size)-x, i);
    }
}

TEST(MPMCQueue, QueueOverflow256) {
    typedef mpmc_bounded_queue<int, 256, uint8_t> queue_t;
    int i = 0;

    queue_t queue;

    for (size_t x=1; x<10; ++x) {
        for (; i < 10000; ++i) {
            if (!queue.enqueue(i)) break;
        }

        while (queue.dequeue(i)) {
        }

        ASSERT_EQ((x*queue_t::buffer_size)-x, i);
    }
}
