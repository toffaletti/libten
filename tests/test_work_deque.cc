#include "gtest/gtest.h"
#include "ten/work_deque.hh"
#include "ten/logging.hh"
#include <thread>
#include <vector>

using namespace ten;

TEST(WorkDeque, Test) {
    using namespace std::chrono;
    work_deque<int> q{100};
    const size_t work_units = 1000;
    const size_t stealers = 8;
    std::atomic<size_t> completed{0};

    std::vector<std::thread> threads;

    auto producer = [&] {
        for (size_t i=0; i<work_units; ++i) {
            q.push(i);
        }
        size_t count = 0;
        for (;;) {
            optional<int> work = q.take();
            if (work) {
                ++count;
                std::this_thread::sleep_for(milliseconds(1));
            } else {
                completed.fetch_add(count);
                VLOG(1) << "did " << count << " work";
                break;
            }
        }
    };

    auto stealer = [&] {
        size_t count = 0;
        for (;;) {
            auto r = q.steal();
            if (r.first) {
                if (r.second) {
                    ++count;
                    std::this_thread::sleep_for(milliseconds(1));
                } else {
                    completed.fetch_add(count);
                    VLOG(1) << "stole " << count << " work";
                    return;
                }
            } else {
                std::this_thread::yield();
            }
        }
    };

    threads.emplace_back(producer);
    for (size_t i=0; i<stealers; ++i) {
        threads.emplace_back(stealer);
    }

    for (auto &thread : threads) {
        thread.join();
    }

    ASSERT_EQ(work_units, completed.load());
}
