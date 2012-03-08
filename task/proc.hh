#ifndef TASK_PROC_HH
#define TASK_PROC_HH

#include "descriptors.hh"
#include "task/private.hh"
#include "task.hh"

namespace ten {

struct io_scheduler;

extern __thread proc *_this_proc;

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
    //! pipe used to wake up from epoll
    pipe_fd pi;
    std::atomic<uint64_t> taskcount;
    //! current time cached in a few places through the event loop
    time_point<monotonic_clock> now;

    proc(task *t = 0)
        : _sched(0), nswitch(0), ctask(0),
        asleep(false), polling(false), canceled(false), pi(O_NONBLOCK), taskcount(0)
    {
        now = monotonic_clock::now();
        add(this);
        if (t) {
            std::unique_lock<std::mutex> lk(mutex);
            thread = new std::thread(proc::startproc, this, t);
            thread->detach();
        } else {
            // main thread proc
            _this_proc = this;
            thread = 0;
        }
    }

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

    static void startproc(proc *p_, task *t) {
        _this_proc = p_;
        std::unique_ptr<proc> p(p_);
        p->addtaskinproc(t);
        t->ready();
        DVLOG(5) << "proc: " << p_ << " thread id: " << std::this_thread::get_id();
        p->schedule();
        DVLOG(5) << "proc done: " << std::this_thread::get_id() << " " << p_;
    }

};

} // end namespace ten

#endif // TASK_PROC_HH
