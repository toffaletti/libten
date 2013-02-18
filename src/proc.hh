#ifndef LIBTEN_PROC_HH
#define LIBTEN_PROC_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/task.hh"
#include "ten/llqueue.hh"
#include "task_impl.hh"

namespace ten {

struct io_scheduler;

extern proc *this_proc();
extern task *this_task();

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

struct proc_context {
    std::thread thread;
    task *t;
};

struct proc {
protected:
    friend task *this_task();
    friend int64_t taskyield();

    // TODO: might be able to use std::atomic_ specializations for shared_ptr
    // to allow waker to change from normal scheduler to io scheduler waker
    // perhaps there is a better pattern...
    std::shared_ptr<proc_waker> _waker;
    io_scheduler *_sched;
    uint64_t nswitch;
    task *ctask;
    tasklist taskpool;
    tasklist runqueue;
    tasklist alltasks;
    context ctx;
    //! other threads use this to add tasks to runqueue
    llqueue<task *> dirtyq;
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
        for (auto i = alltasks.cbegin(); i != alltasks.cend(); ++i) {
            out << *i << "\n";
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
    void shutdown() {
        for (auto i = alltasks.cbegin(); i != alltasks.cend(); ++i) {
            task *t = *i;
            if (t == ctask) continue; // don't add ourself to the runqueue
            if (!t->systask) {
                t->cancel();
            }
        }
    }

    void ready(task *t, bool front=false) {
        DVLOG(5) << "readying: " << t;
        if (this != this_proc()) {
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

    bool cancel_task_by_id(uint64_t id) {
        task *t = nullptr;
        for (auto i = alltasks.cbegin(); i != alltasks.cend(); ++i) {
            if ((*i)->id == id) {
                t = *i;
                break;
            }
        }

        if (t) {
            t->cancel();
        }
        return (bool)t;
    }

    //! mark current task as system task
    void mark_system_task() {
        if (!ctask->systask) {
            ctask->systask = true;
            --taskcount;
        }
    }

    void wakeup();

    task *newtaskinproc(const std::function<void ()> &f, size_t stacksize) {
        auto i = std::find_if(taskpool.begin(), taskpool.end(), task_has_size(stacksize));
        task *t = nullptr;
        if (i != taskpool.end()) {
            t = *i;
            taskpool.erase(i);
            DVLOG(5) << "initing from pool: " << t;
            t->init(f);
        } else {
            t = new task(f, stacksize);
        }
        addtaskinproc(t);
        return t;
    }

    void addtaskinproc(task *t) {
        ++taskcount;
        alltasks.push_back(t);
        t->cproc = this;
    }

    void deltaskinproc(task *t);

    static void add(proc *p);
    static void del(proc *p);

    static void thread_entry(task *t);

};

} // end namespace ten

#endif // LIBTEN_PROC_HH
