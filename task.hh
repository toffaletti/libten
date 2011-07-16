#ifndef TASK_HH
#define TASK_HH

#include <deque>
#include <list>
#include <boost/function.hpp>
#include "thread.hh"
#include "coroutine.hh"
#include "descriptors.hh"

class runner;

namespace detail {
extern __thread runner *runner_;
}

class task : boost::noncopyable {
public:
    typedef boost::function<void ()> proc;

    static void spawn(const proc &f);

    static void yield();
    static void migrate();
    static void migrate_to(runner *to);
    static void poll(int fd, int events);

protected:
    friend class runner;
    task() : exiting(false) {}

    static void swap(task *from, task *to);
    static task *self();

private:
    coroutine co;
    proc f;
    bool exiting;

    task(const proc &f_, size_t stack_size=4096*1);

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

    void add_to_runqueue(task *c) {
        mutex::scoped_lock l(mut);
        runq.push_back(c);
        wakeup_nolock();
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

    void set_task(task *c) { current_task = c; }
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
        runners.push_back(this);
    }

    static void *start(void *arg);
};

#endif // TASK_HH

