#include "gtest/gtest.h"
#include "ten/mpsc_queue.hh"
#include <thread>
#include <vector>

using namespace ten;

TEST(MPSCQueue, Test) {
    mpsc_queue<int> q;
    size_t batch_size = 1000000;
    size_t num_producers = 8;

    std::vector<std::thread> threads;

    auto producer = [&] {
        for (size_t i=0; i<batch_size/num_producers; ++i) {
            q.push(i);
        }
    };

    auto consumer = [&] {
        size_t count = 0;
        for (;;) {
            auto r = q.pop();
            if (r.first) {
                ++count;
                if (count == batch_size) break;
            } else {
                std::this_thread::yield();
            }
        }
    };

    threads.emplace_back(consumer);
    for (size_t i=0; i<num_producers; ++i) {
        threads.emplace_back(producer);
    }

    for (auto &thread : threads) {
        thread.join();
    }
}
