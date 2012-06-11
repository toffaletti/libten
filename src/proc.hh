#ifndef TASK_PROC_HH
#define TASK_PROC_HH

#include <condition_variable>
#include "ten/descriptors.hh"
#include "ten/task.hh"
#include "ten/llqueue.hh"
#include "task_impl.hh"

namespace ten {

struct io_scheduler;

extern proc *this_proc();

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
    //! other threads use this to add tasks to runqueue
    llqueue<task *> dirtyq;
    //! true when asleep and runqueue is empty and no epoll
    bool asleep;
    //! true when asleep in epoll_wait
    std::atomic<bool> polling;
    //! true when canceled
    std::atomic<bool> canceled;
    //! cond used to wake up when runqueue is empty and no epoll
    std::condition_variable cond;
    //! used to wake up from epoll
    event_fd event;
    std::atomic<uint64_t> taskcount;
    //! current time cached in a few places through the event loop
    time_point<steady_clock> now;

    explicit proc(task *t = nullptr);

    proc(const proc &) = delete;
    proc &operator =(const proc &) = delete;

    ~proc();

    void schedule();

    io_scheduler &sched();

    bool is_ready() {
        return !runqueue.empty();
    }

    void cancel() {
        canceled = true;
        wakeup();
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

    static void startproc(proc *p_, task *t);

};

} // end namespace ten

#endif // TASK_PROC_HH
