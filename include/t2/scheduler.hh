#ifndef TEN_SCHEDULER_HH
#define TEN_SCHEDULER_HH

#include "t2/channel.hh"
#include "t2/task.hh"
#include "t2/kernel.hh"

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
    channel<std::shared_ptr<tasklet>> chan;
    std::vector<std::thread> threads;

    void loop() {
        boost::context::fcontext_t fctx;
        thread_data td(fctx);
        tld = &td;
        while (the_kernel->taskcount > 0 || !the_kernel->shutdown) {
            DVLOG(5) << "waiting for task to schedule";
            auto t = chan.recv();
            DVLOG(5) << "got task to schedule: " << (*t).get();
            if (!t) break;
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
                        chan.send(std::move(tld->self));
                    }
                }
            } else {
                tld->self.reset();
            }
        }
        DVLOG(5) << "exiting scheduler";
        chan.close();
    }

public:
    scheduler() {
        for (int i=0; i<4; ++i) {
            threads.emplace_back(std::bind(&scheduler::loop, this));
        }
    }

    ~scheduler() {
        LOG(INFO) << "joining";
        for (auto &thread : threads) {
            thread.join();
        }
    }

    void add(std::shared_ptr<tasklet> task) {
        chan.send(std::move(task));
    }
};

} // t2

#endif
