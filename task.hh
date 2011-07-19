#ifndef TASK_HH
#define TASK_HH

#include <deque>
#include <list>
#include <boost/function.hpp>
#include "thread.hh"
#include "coroutine.hh"
#include "descriptors.hh"
#include "timespec.hh"

class runner;

namespace detail {
extern __thread runner *runner_;
}

class task : boost::noncopyable {
public:
    enum state_e {
        state_idle,
        state_running,
        state_exiting,
        state_migrating,
        state_timeout,
    };

    typedef boost::function<void ()> proc;

    static void spawn(const proc &f);

    static void yield();
    static void migrate(runner *to=NULL);
    static bool poll(int fd, int events, unsigned int ms=0);
    static void sleep(unsigned int ms);

protected:
    friend class runner;
    task() : state(state_running) {}

    static void swap(task *from, task *to);
    static task *self();

private:
    coroutine co;
    proc f;
    state_e state;
    timespec ts;
    runner *in;

    task(const proc &f_, size_t stack_size=16*1024);

    static void start(task *);
};

typedef std::deque<task*> task_deque;

class runner : boost::noncopyable {
public:
    typedef std::list<runner *> list;

    thread get_thread() { return thread(tt); }

    static runner *spawn(const task::proc &f) {
        return new runner(f);
    }

    static runner *self() {
        if (detail::runner_ == NULL) {
            detail::runner_ = new runner;
        }
        return detail::runner_;
    }

    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }

    void schedule(bool loop=true);

    static size_t count();

protected:
    friend class task;

    bool add_to_runqueue(task *t) {
        mutex::scoped_lock l(mut);
        task_deque::iterator i = std::find(runq.begin(), runq.end(), t);
        // don't add multiple times
        // this is possible with io loop and timeout loop
        if (i == runq.end()) {
            runq.push_back(t);
            wakeup_nolock();
            return true;
        }
        return false;
    }

    bool add_to_runqueue_if_asleep(task *c) {
        mutex::scoped_lock l(mut);
        if (asleep) {
            runq.push_back(c);
            wakeup_nolock();
            return true;
        }
        return false;
    }

    void delete_from_runqueue(task *c) {
        mutex::scoped_lock l(mut);
        assert(c == runq.back());
        runq.pop_back();
    }

    // lock must already be held
    void wakeup_nolock() {
        if (asleep) {
            asleep = false;
            cond.signal();
        }
    }

    static void add_to_empty_runqueue(task *);

    void set_task(task *c) {
        c->in = this;
        current_task = c;
    }

    task *get_task() {
        return current_task;
    }

private:
    static mutex tmutex;
    static list runners;

    thread tt;

    mutex mut;
    condition cond;
    bool asleep;

    task scheduler;
    task *current_task;
    task_deque runq;
    epoll_fd efd;

    struct task_timeout_heap_compare {
        bool operator ()(const task *a, const task *b) const {
            return a->ts > b->ts;
        }
    };
    typedef std::vector<task *> task_heap;
    // tasks waiting with a timeout value set
    task_heap waiters;

    void add_waiter(task *t);

    runner() : asleep(false), current_task(0) {
        tt=thread::self();
    }

    runner(task *c) : asleep(false), current_task(0) {
        add_to_runqueue(c);
        thread::create(tt, start, this);
    }

    runner(const task::proc &f) : asleep(false), current_task(0) {
        task *c = new task(f);
        add_to_runqueue(c);
        thread::create(tt, start, this);
    }

    void sleep() {
        // move to the head of the list
        // to simulate a FIFO
        // if there are too many threads
        // and not enough tasks, we can
        // use a cond.timedwait here and
        // exit a thread that times out
        {
            mutex::scoped_lock l(tmutex);
            runners.remove(this);
            runners.push_front(this);
        }
        mutex::scoped_lock l(mut);
        asleep = true;
        while (asleep) {
            cond.wait(l);
        }
    }

    void append_to_list() {
        mutex::scoped_lock l(tmutex);
        runners.push_back(this);
    }

    void remove_from_list() {
        mutex::scoped_lock l(tmutex);
        runners.remove(this);
    }

    void run_queued_tasks();
    void check_io();

    static void *start(void *arg);
};

#endif // TASK_HH

