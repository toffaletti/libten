#include "scheduler.hh"
#include "io.hh"
#include <sys/syscall.h>

namespace ten {

extern std::mutex procsmutex;
extern std::deque<ptr<proc>> procs;

scheduler::scheduler()
  : ctask(nullptr),
    _sched(nullptr), 
    canceled(false),
    taskcount(0)
{
    _waker = std::make_shared<proc_waker>();
    update_cached_time();
}

scheduler::~scheduler() {
    if (getpid() == syscall(SYS_gettid)) {
        // if the main proc is exiting we need to cancel
        // all other procs (threads) and wait for them
        size_t nprocs = procs.size();
        {
            std::lock_guard<std::mutex> plk{procsmutex};
            for (auto i=procs.begin(); i!= procs.end(); ++i) {
                if ((*i)->_scheduler.get() == this) continue;
                (*i)->cancel();
            }
        }
        for (;;) {
            // TODO: remove this busy loop in favor of sleeping the proc
            {
                std::lock_guard<std::mutex> plk{procsmutex};
                size_t np = procs.size();
                if (np == 0)
                    break;
            }
            std::this_thread::yield();
        }

        DVLOG(5) << "sleeping last proc to allow " << nprocs << " threads to exit and cleanup";
        for (size_t i=0; i<nprocs*2; ++i) {
            // nasty hack for mysql thread cleanup
            // because it happens *after* all of my code, i have no way of waiting
            // for it to finish with an event (unless i joined all threads)
            usleep(100);
            std::this_thread::yield();
        }
    }
    // TODO: now that threads are remaining joinable
    // maybe shutdown can be cleaner...look into this
    // example: maybe if canceled we don't detatch
    // and we can join in the above loop instead of sleep loop
    //if (thread.joinable()) {
    //    thread.detach();
    //}
    // XXX: there is also a thing that will trigger a cond var
    // after thread has fully exited. that is another possiblity
    runqueue.clear();
    // clean up system tasks
    while (!alltasks.empty()) {
        remove_task(ptr<task::pimpl>{alltasks.front().get()});
    }
    // unlock before del()
    DVLOG(5) << "proc freed: " << this;
}

void scheduler::shutdown() {
    for (auto &t : alltasks) {
        if (t.get() == ctask.get()) continue; // don't add ourself to the runqueue
        if (!t->systask) {
            t->cancel();
        }
    }
}

io_scheduler &scheduler::sched() {
    if (_sched == nullptr) {
        _sched.reset(new io_scheduler());
    }
    return *_sched;
}

void scheduler::schedule() {
    try {
        DVLOG(5) << "p: " << this << " entering proc::schedule";
        while (taskcount > 0) {
            // check dirty queue
            {
                ptr<task::pimpl> t = nullptr;
                while (dirtyq.pop(t)) {
                    DVLOG(5) << "dirty readying " << t;
                    runqueue.push_front(t);
                }
            }
            if (runqueue.empty()) {
                _waker->wait();
                // need to go through dirty loop again because
                // runqueue could still be empty
                if (!dirtyq.empty()) continue;
            }
            if (canceled) {
                // set canceled to false so we don't
                // execute this every time through the loop
                // while the tasks are cleaning up
                canceled = false;
                procshutdown();
                if (runqueue.empty()) continue;
            }
            ptr<task::pimpl> t{runqueue.front()};
            runqueue.pop_front();
            ctask = t;
            DVLOG(5) << "p: " << this << " swapping to: " << t;
            t->is_ready = false;
            ctx.swap(t->ctx, reinterpret_cast<intptr_t>(t.get()));
            ctask = nullptr;
            
            if (!t->fn) {
                remove_task(t);
            }
        }
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what() << "\n" << e.backtrace_str();
        std::exit(2);
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what();
        std::exit(2);
    }
}

void scheduler::attach_task(std::shared_ptr<task::pimpl> t) {
    ++taskcount;
    t->_scheduler.reset(this);
    alltasks.push_back(std::move(t));
}

void scheduler::remove_task(ptr<task::pimpl> t) {
    if (!t->systask) {
        --taskcount;
    }
    DCHECK(std::find(runqueue.begin(), runqueue.end(), t) == runqueue.end()) << "BUG: " << t
        << " found in runqueue while being deleted";
    auto i = std::find_if(alltasks.begin(), alltasks.end(), [=](std::shared_ptr<task::pimpl> &tt) -> bool {
        return tt.get() == t.get();
    });
    DCHECK(i != alltasks.end());
    alltasks.erase(i);
}



void scheduler::ready(ptr<task::pimpl> t, bool front) {
    DVLOG(5) << "readying: " << t;
    if (t->is_ready.exchange(true) == false) {
        if (this != &this_ctx->scheduler) {
            dirtyq.push(t);
            wakeup();
        } else {
            if (front) {
                runqueue.push_front(t);
            } else {
                runqueue.push_back(t);
            }
        }
    }
}

void scheduler::ready_for_io(ptr<task::pimpl> t) {
    DVLOG(5) << "readying for io: " << t;
    if (t->is_ready.exchange(true) == false) {
        runqueue.push_back(t);
    }
}

void scheduler::unsafe_ready(ptr<task::pimpl> t) {
    DVLOG(5) << "readying: " << t;
    runqueue.push_back(t);
}

bool scheduler::cancel_task_by_id(uint64_t id) {
    bool found = false;
    for (auto &t : alltasks) {
        if (t->id == id) {
            found = true;
            t->cancel();
            break;
        }
    }
    return found;
}

void scheduler::mark_system_task() {
    if (!ctask->systask) {
        ctask->systask = true;
        --taskcount;
    }
}

void scheduler::wakeup() {
    _waker->wake();
}

} // ten

