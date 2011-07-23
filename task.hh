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

//! coroutine with scheduling via a runner
//! can wait on io and timeouts
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

    //! spawn a task and add it to a runners run queue
    static task *spawn(const proc &f, runner *in=NULL);

    //! yield the current task
    static void yield();
    //! migrate a task to a different runner
    //! if no runner is specified an empty runner will be chosen
    //! if no empty runners exist, a new one will spawn
    //! this is useful for moving a task to a different thread
    //! when it is about to make a blocking call
    //! so it does not block any other tasks
    static void migrate(runner *to=NULL);
    //! wrapper around poll for single fd
    static bool poll(int fd, short events, unsigned int ms=0);
    //! task aware poll, for waiting on io. task will
    //! go to sleep until epoll events or timeout
    static int poll(pollfd *fds, nfds_t nfds, int timeout=0);
    //! put task to sleep
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
    coroutine co;
    volatile state_e state;

    task(const proc &f_, size_t stack_size=16*1024);
    static void start(task *);

private: /* condition interface */
    static task *self();
    void suspend(mutex::scoped_lock &l);
    void resume();

public:
    //! task aware condition.
    class condition : boost::noncopyable {
    public:
        condition() {}

        //! signal one of the waiters
        void signal() {
            mutex::scoped_lock l(mm);
            if (!waiters.empty()) {
                task *t = waiters.front();
                waiters.pop_front();
                t->resume();
            }
        }

        // TODO: resume all waiters
        void broadcast() {}

        //! wait for condition to be signaled
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

