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
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#define _TASK_SLEEP     (1<<0)
#define _TASK_RUNNING   (1<<1)
#define _TASK_EXIT      (1<<2)
#define _TASK_MIGRATE   (1<<3)

class runner;

//! coroutine with scheduling via a runner
//! can wait on io and timeouts
class task {
public:
    typedef boost::function<void ()> proc;
    typedef std::deque<task> deque;

    struct impl : boost::noncopyable, public boost::enable_shared_from_this<impl> {
        runner *in;
        proc f;
        // TODO: allow user to set state and name
        std::string name;
        std::string state;
        timespec ts;
        coroutine co;
        uint32_t flags;

        impl() : flags(_TASK_RUNNING) {}
        impl(const proc &f_, size_t stack_size=16*1024);
        ~impl() { if (!co.main()) { --ntasks; } }

        task to_task() { return shared_from_this(); }
    };
    typedef boost::shared_ptr<impl> shared_impl;

    bool operator == (const task &t) const {
        return m.get() == t.m.get();
    }

    bool operator != (const task &t) const {
        return m.get() != t.m.get();
    }

    bool operator < (const task &t) const {
        return m.get() < t.m.get();
    }

    friend std::ostream &operator << (std::ostream &o, const task &t) {
        o << t.m.get();
        return o;
    }

public:
    //! spawn a task and add it to a runners run queue
    static task spawn(const proc &f, runner *in=NULL);

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
    void set_runner(runner *i) { m->in = i; }
    runner *get_runner() const { return m->in; }
    void set_state(const std::string &str) {
        m->state = str;
    }

    inline void clear_flag(uint32_t f) { m->flags ^= f; }
    inline void set_flag(uint32_t f) { m->flags |= f; }
    inline bool test_flag_set(uint32_t f) const { return m->flags & f; }
    inline bool test_flag_not_set(uint32_t f) const { return m->flags ^ f; }

    const std::string &get_state() const { return m->state; }
    const timespec &get_timeout() const { return m->ts; }
    void set_abs_timeout(const timespec &abs) { m->ts = abs; }
    static int get_ntasks() { return ntasks; }

    task() { m.reset(new impl); }

    static void swap(task &from, task &to);
private:
    static atomic_count ntasks;

    shared_impl m;

    task(const shared_impl &m_) : m(m_) {}
    task(const proc &f_, size_t stack_size=16*1024) {
        m.reset(new impl(f_, stack_size));
    }
    static void start(impl *);

private: /* condition interface */
    static task self();
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
                task t = waiters.front();
                waiters.pop_front();
                t.resume();
            }
        }

        // TODO: resume all waiters
        void broadcast() {}

        //! wait for condition to be signaled
        void wait(mutex::scoped_lock &l) {
            task t = task::self();
            {
                mutex::scoped_lock ll(mm);
                waiters.push_back(t);
            }
            t.suspend(l);
        }
    private:
        mutex mm;
        task::deque waiters;
    };
};

#endif // TASK_HH

