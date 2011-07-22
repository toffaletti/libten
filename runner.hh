#ifndef RUNNER_HH
#define RUNNER_HH

#include "descriptors.hh"
#include "task.hh"
#include <list>

class runner;

namespace detail {
extern __thread runner *runner_;
}

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

    static size_t count();

    void schedule();

private:
    friend class task;
    static mutex tmutex;
    static list runners;

    thread tt;

    mutex mut;
    condition cond;
    bool asleep;

    task scheduler;
    task *current_task;
    task::deque runq;
    epoll_fd efd;

    typedef std::vector<task *> task_heap;
    // tasks waiting with a timeout value set
    task_heap waiters;

    // key is the fd number
    typedef std::vector< std::pair<task *, pollfd *> > poll_task_array;
    poll_task_array pollfds;

    void add_waiter(task *t);

    runner() : asleep(false), current_task(&scheduler) {
        tt=thread::self();
    }

    runner(task *c) : asleep(false), current_task(&scheduler) {
        add_to_runqueue(c);
        thread::create(tt, start, this);
    }

    runner(const task::proc &f) : asleep(false), current_task(&scheduler) {
        task *c = new task(f);
        add_to_runqueue(c);
        thread::create(tt, start, this);
    }

    void sleep(mutex::scoped_lock &l) {
        // move to the head of the list
        // to simulate a FIFO
        // if there are too many threads
        // and not enough tasks, we can
        // use a cond.timedwait here and
        // exit a thread that times out

        // must set sleep here
        // another thread could add to runq
        // (a resume() for example)
        // during the unlock and if we're
        // not marked asleep, then we
        // end up sleeping while runq isn't empty
        asleep = true;
        l.unlock();
        {
            mutex::scoped_lock tl(tmutex);
            runners.remove(this);
            runners.push_front(this);
        }
        l.lock();
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

    void wakeup_all_runners() {
        mutex::scoped_lock l(tmutex);
        for (runner::list::iterator i=runners.begin(); i!=runners.end(); ++i) {
            if (this == *i) continue;
            (*i)->wakeup();
        }
    }

    void run_queued_tasks();
    void check_io();

    bool add_to_runqueue(task *t) {
        mutex::scoped_lock l(mut);
        task::deque::iterator i = std::find(runq.begin(), runq.end(), t);
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

    void set_task(task *t) {
        t->set_runner(this);
        current_task = t;
    }

    task *get_task() {
        return current_task;
    }

    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }

    static void add_to_empty_runqueue(task *);

    static void *start(void *arg);
};

#endif // RUNNER_HH
