#ifndef TEN_SCHEDULER_HH
#define TEN_SCHEDULER_HH

#include "ten/optional.hh"
#include "t2/task.hh"
#include "t2/kernel.hh"
#include "t2/double_lock_queue.hh"
#include <condition_variable>
#include <atomic>
#include <thread>

namespace t2 {

struct thread_data {
    context ctx;
    std::shared_ptr<tasklet> self;

    thread_data(boost::context::fcontext_t &fctx)
        : ctx(fctx) {}
};

extern __thread thread_data *tld;
class scheduler;
extern scheduler *the_scheduler;

class scheduler {
private:
    double_lock_queue<std::shared_ptr<tasklet>> queue;
    std::mutex mutex;
    std::condition_variable not_empty;
    std::atomic<bool> done;
    std::vector<std::thread> threads;

    ten::optional<std::shared_ptr<tasklet>> next_task() {
        std::shared_ptr<tasklet> value;
        uint64_t spin_count = 0;
        while (!queue.pop(value)) {
            ++spin_count;
            if (done) return ten::nullopt;
            if (spin_count > 4000) {
                std::unique_lock<std::mutex> lock(mutex);
                not_empty.wait(lock);
            }
        }
        return value;
    }

    bool enqueue(std::shared_ptr<tasklet> &&t) {
        if (done) return false;
        queue.push(t);
        not_empty.notify_one();
        return true;
    }


    void loop() {
        boost::context::fcontext_t fctx;
        thread_data td(fctx);
        tld = &td;
        while (the_kernel->taskcount > 0 || !the_kernel->shutdown) {
            DVLOG(5) << "waiting for task to schedule";
            auto t = next_task();
            if (!t) break;
            DVLOG(5) << "got task to schedule: " << (*t).get();
            CHECK((*t)->_ready);
            if (the_kernel->shutdown) {
                DVLOG(5) << "canceling because done " << *t;
                tasklet::cancel(*t);
            }
            tld->self = std::move(*t);
            if (tld->self->swap()) {
                CHECK(tld->self);
                if (tld->self->_post) {
                    std::function<void ()> post = std::move(tld->self->_post);
                    post();
                }
                if (tld->self) {
                    DVLOG(5) << "scheduling " << tld->self;
                    if (tld->self->_ready.exchange(true) == false) {
                        enqueue(std::move(tld->self));
                    }
                }
            } else {
                tld->self.reset();
            }
        }
        DVLOG(5) << "exiting scheduler";
        done = true;
    }

public:
    scheduler() : done(false) {
        CHECK(the_scheduler == nullptr);
        the_scheduler = this;
        for (int i=0; i<4; ++i) {
            threads.emplace_back(std::bind(&scheduler::loop, this));
        }
    }

    ~scheduler() {
        done = true;
        LOG(INFO) << "joining";
        for (auto &thread : threads) {
            thread.join();
        }
    }

    void add(std::shared_ptr<tasklet> task) {
        CHECK(task);
        CHECK(task->_ready);
        DVLOG(5) << task << " added to scheduler";
        enqueue(std::move(task));
    }
};

} // t2

#endif
