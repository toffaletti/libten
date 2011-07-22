#ifndef TASK_HH
#define TASK_HH

#include "atomic.hh"
#include "thread.hh"
#include "coroutine.hh"
#include "timespec.hh"
//#include "runner.hh"

#include <poll.h>
#include <deque>
#include <utility>
#include <boost/function.hpp>

class runner;

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
    task() : state(state_running) {}
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

#endif // TASK_HH

