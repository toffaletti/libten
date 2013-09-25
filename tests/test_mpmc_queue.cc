#include "gtest/gtest.h"
#include "ten/mpmc_bounded_queue.hh"
#include <array>
#include <thread>

using namespace ten;
using std::size_t;

typedef mpmc_bounded_queue<int, 1024> queue_t;

static size_t const thread_count = 4;
static size_t const batch_size = 1;
static size_t const iter_count = 1000000;

static void thread_func(queue_t &queue) {
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
}

TEST(MPMCQueue, Test) {
    queue_t queue;

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
