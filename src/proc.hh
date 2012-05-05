#ifndef TASK_PROC_HH
#define TASK_PROC_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/task.hh"
#include "private.hh"

namespace ten {

struct io_scheduler;

proc *this_proc();

// TODO: api to register at-proc-exit cleanup functions
// this can be used to free io_scheduler, or other per-proc
// resources like dns resolving threads, etc.

struct proc {
    io_scheduler *_sched;
    std::thread *thread;
    std::mutex mutex;
    uint64_t nswitch;
    task *ctask;
    tasklist taskpool;
    tasklist runqueue;
    tasklist alltasks;
    coroutine co;
    //! true when asleep and runqueue is empty and no epoll
    bool asleep;
    //! true when asleep in epoll_wait
    bool polling;
    //! true when canceled
    bool canceled;
    //! cond used to wake up when runqueue is empty and no epoll
    std::condition_variable cond;
    //! used to wake up from epoll
    event_fd event;
    std::atomic<uint64_t> taskcount;
    //! current time cached in a few places through the event loop
    time_point<steady_clock> now;

    explicit proc(task *t = 0);

    proc(const proc &) = delete;
    proc &operator =(const proc &) = delete;

    ~proc();

    void schedule();

    io_scheduler &sched();

    bool is_ready() {
        return !runqueue.empty();
    }

    void cancel() {
        std::unique_lock<std::mutex> lk(mutex);
        canceled = true;
        wakeupandunlock(lk);
    }

    void wakeupandunlock(std::unique_lock<std::mutex> &lk);

    task *newtaskinproc(const std::function<void ()> &f, size_t stacksize) {
        auto i = std::find_if(taskpool.begin(), taskpool.end(), task_has_size(stacksize));
        task *t = 0;
        if (i != taskpool.end()) {
            t = *i;
            taskpool.erase(i);
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

    static void startproc(proc *p_, task *t);

};

} // end namespace ten

#endif // TASK_PROC_HH
