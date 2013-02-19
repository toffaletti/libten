#ifndef LIBTEN_PROC_HH
#define LIBTEN_PROC_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/task.hh"
#include "ten/llqueue.hh"
#include "task_impl.hh"

namespace ten {

struct io_scheduler;

extern ptr<proc> this_proc();
extern ptr<task::pimpl> this_task();

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

class proc {
private:
    friend ptr<task::pimpl> this_task();

    // TODO: might be able to use std::atomic_ specializations for shared_ptr
    // to allow waker to change from normal scheduler to io scheduler waker
    // perhaps there is a better pattern...
    std::shared_ptr<proc_waker> _waker;
    ptr<io_scheduler> _sched;
    ptr<task::pimpl> ctask;
    std::deque<ptr<task::pimpl>> taskpool;
    std::deque<ptr<task::pimpl>> runqueue;
    std::deque<ptr<task::pimpl>> alltasks;
    context ctx;
    //! other threads use this to add tasks to runqueue
    llqueue<ptr<task::pimpl>> dirtyq;
    //! true when canceled
    std::atomic<bool> canceled;
    //! tasks in this proc
    std::atomic<uint64_t> taskcount;
    //! current time cached in a few places through the event loop
    proc_time_t now;
    bool _main;

public:
    explicit proc(bool main_);

    proc(const proc &) = delete;
    proc &operator =(const proc &) = delete;

    ~proc();

    void schedule();

    context &sched_context() { return ctx; }

    io_scheduler &sched();

    bool is_canceled() const {
        return canceled;
    }

    bool is_dirty() {
        return !dirtyq.empty();
    }

    bool is_ready() const {
        return !runqueue.empty();
    }

    void dump_tasks(std::ostream &out) const {
        for (auto &t : alltasks) {
            out << t << "\n";
        }
    }

    //void set_waker(const std::shared_ptr<proc_waker> &new_waker) {
    //    _waker = new_waker;
    //}

    std::shared_ptr<proc_waker> get_waker() {
        return _waker;
    }

    void cancel() {
        canceled = true;
        wakeup();
    }

    const proc_time_t & update_cached_time() {
        now = proc_clock_t::now();
        return now;
    }

    const proc_time_t &cached_time() {
        return now;
    }

    //! cancel all tasks in this proc
    void shutdown();
    void ready(ptr<task::pimpl> t, bool front=false);
    bool cancel_task_by_id(uint64_t id);
    //! mark current task as system task
    void mark_system_task();
    void wakeup();

    ptr<task::pimpl> newtaskinproc(const std::function<void ()> &f, size_t stacksize);
    void addtaskinproc(ptr<task::pimpl> t);
    void deltaskinproc(ptr<task::pimpl> t);

    static void add(ptr<proc> p);
    static void del(ptr<proc> p);

    static void thread_entry(ptr<task::pimpl> t);

};

} // end namespace ten

#endif // LIBTEN_PROC_HH
