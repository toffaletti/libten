#ifndef TASK_HH
#define TASK_HH

#include "atomic.hh"
#include "thread.hh"
#include "coroutine.hh"
#include "timespec.hh"

#include <poll.h>
#include <deque>
#include <utility>
#include <boost/function.hpp>

class runner;

class task : boost::noncopyable {
public:
    typedef std::deque<task*> deque;
    enum state_e {
        state_idle,
        state_running,
        state_exiting,
        state_migrating,
    };

    typedef boost::function<void ()> proc;

    static task *spawn(const proc &f, runner *in=NULL);

    static void yield();
    static void migrate(runner *to=NULL);
    static bool poll(int fd, short events, unsigned int ms=0);
    static int poll(pollfd *fds, nfds_t nfds, int timeout=0);
    static void sleep(unsigned int ms);

public: /* runner interface */
    void set_runner(runner *i) { in = i; }
    runner *get_runner() const { return in; }
    void set_state(state_e st, const std::string &str) {
        state = st;
        state_msg = str;
    }
    state_e get_state() const { return state; }
    const timespec &get_timeout() const { return ts; }
    void set_abs_timeout(const timespec &abs) { ts = abs; }
    static int get_ntasks() { return ntasks; }

    task() : state(state_running) {}
    ~task() { if (!co.main()) { --ntasks; } }

    static void swap(task *from, task *to);
private:
    static atomic_count ntasks;

    runner *in;
    proc f;
    std::string state_msg;
    timespec ts;
    volatile state_e state;
    coroutine co;

    task(const proc &f_, size_t stack_size=16*1024);
    static void start(task *);

private: /* condition interface */
    static task *self();
    void suspend(mutex::scoped_lock &l);
    void resume();


public:

    class condition : boost::noncopyable {
    public:
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
        task::deque waiters;
    };
};

#endif // TASK_HH

