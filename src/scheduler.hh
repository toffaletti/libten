#ifndef LIBTEN_SCHEDULER_HH
#define LIBTEN_SCHEDULER_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/llqueue.hh"
#include "task_impl.hh"

namespace ten {

struct io_scheduler;

// TODO: api to register at-proc-exit cleanup functions
// this can be used to free io_scheduler, or other per-proc
// resources like dns resolving threads, etc.

//! proc waker is the api for waking a proc from sleep
// io_scheduler uses this
class proc_waker {
public:
    //! used to wake up from epoll
    event_fd event;
    //! true when asleep in epoll_wait
    std::atomic<bool> polling;

public: // non-epoll data
    std::mutex mutex;
    //! cond used to wake up when runqueue is empty and no epoll
    std::condition_variable cond;
    //! true when asleep and runqueue is empty and no epoll
    bool asleep;
public:
    proc_waker() : event(0, EFD_NONBLOCK), polling(false), asleep(false) {}

    void wake() {
        if (polling.exchange(false)) {
            DVLOG(5) << "eventing " << this;
            event.write(1);
        } else {
            std::lock_guard<std::mutex> lk{mutex};
            if (asleep) {
                DVLOG(5) << "notifying " << this;
                cond.notify_one();
            }
        }
    }

    void wait();
};

class scheduler {
    friend class proc;
private:
    ptr<task::pimpl> ctask;
    context ctx;
    std::deque<ptr<task::pimpl>> runqueue;
    std::unique_ptr<io_scheduler> _sched;
    std::deque<std::shared_ptr<task::pimpl>> alltasks;
    std::shared_ptr<proc_waker> _waker;
    //! other threads use this to add tasks to runqueue
    llqueue<ptr<task::pimpl>> dirtyq;
    //! true when canceled
    std::atomic<bool> canceled;
    //! tasks in this proc
    std::atomic<uint64_t> taskcount;
    //! current time cached in a few places through the event loop
    proc_time_t now;
public:
    scheduler();
    ~scheduler();

    void shutdown();
    io_scheduler &sched();
    void schedule();

    const proc_time_t & update_cached_time() {
        now = proc_clock_t::now();
        return now;
    }

    const proc_time_t &cached_time() const {
        return now;
    }

    void attach_task(std::shared_ptr<task::pimpl> t);
    void remove_task(ptr<task::pimpl> t);

    void dump_tasks(std::ostream &out) const {
        for (auto &t : alltasks) {
            out << t << "\n";
        }
    }

    void wakeup();

    bool cancel_task_by_id(uint64_t id);
    //! mark current task as system task
    void mark_system_task();

    std::shared_ptr<proc_waker> get_waker() {
        return _waker;
    }

    ptr<task::pimpl> current_task() const {
        return ctask;
    }

    bool is_canceled() const {
        return canceled;
    }

    void cancel() {
        canceled = true;
        wakeup();
    }

    bool is_dirty() {
        return !dirtyq.empty();
    }

    bool is_ready() const {
        return !runqueue.empty();
    }

    context &sched_context() { return ctx; }

private:
    friend class task;
    friend void this_task::yield();
    void ready(ptr<task::pimpl> t, bool front=false);
    void ready_for_io(ptr<task::pimpl> t);
    void unsafe_ready(ptr<task::pimpl> t);
};

} // ten

#endif

