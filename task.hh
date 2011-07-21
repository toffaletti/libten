#ifndef TASK_HH
#define TASK_HH

#include <poll.h>
#include <deque>
#include <list>
#include <boost/function.hpp>
#include "thread.hh"
#include "coroutine.hh"
#include "descriptors.hh"
#include "timespec.hh"

#if defined(__GLIBCXX__) // g++ 3.4+
using __gnu_cxx::__atomic_add;
using __gnu_cxx::__exchange_and_add;
#endif

class atomic_count
{
public:
    explicit atomic_count( int v ) : value_( v ) {}

    int operator++()
    {
        return __exchange_and_add( &value_, +1 ) + 1;
    }

    int operator--()
    {
        return __exchange_and_add( &value_, -1 ) - 1;
    }

    operator int() const
    {
        return __exchange_and_add( &value_, 0 );
    }

private:
    atomic_count(atomic_count const &);
    atomic_count & operator=(atomic_count const &);

    mutable _Atomic_word value_;
};


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
    };

    typedef boost::function<void ()> proc;

    static void spawn(const proc &f);

    static void yield();
    static void migrate(runner *to=NULL);
    static bool poll(int fd, short events, unsigned int ms=0);
    static int poll(pollfd *fds, nfds_t nfds, int timeout=0);
    static void sleep(unsigned int ms);

protected:
    friend class runner;
    friend struct task_timeout_heap_compare;
    task() : state(state_running), fds(0), nfds(0) {}
    ~task() { if (!co.main()) { --ntasks; } }

    static void swap(task *from, task *to);
    static task *self();

    void suspend(mutex::scoped_lock &l);
    void resume();

private:
    static atomic_count ntasks;

    coroutine co;
    proc f;
    volatile state_e state;
    timespec ts;
    runner *in;
    pollfd *fds;
    nfds_t nfds;

    task(const proc &f_, size_t stack_size=16*1024);

    static void start(task *);
public:

    class condition : boost::noncopyable {
    public:
        typedef std::deque<task *> task_deque;
        condition() {}

        void signal() {
            mutex::scoped_lock l(mm);
            if (!waiters.empty()) {
                task *t = waiters.front();
                waiters.pop_front();
                t->resume();
            }
        }

        void broadcast() {}

        void wait(mutex::scoped_lock &l) {
            task *t = task::self();
            {
                mutex::scoped_lock ll(mm);
                waiters.push_back(t);
            }
            t->suspend(l);
        }
    private:
        mutex mm;
        task_deque waiters;
    };
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

    void schedule();

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

    typedef std::vector<task *> task_heap;
    // tasks waiting with a timeout value set
    task_heap waiters;

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

    static void *start(void *arg);
};

#endif // TASK_HH

