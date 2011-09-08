#ifndef TASK_HH
#define TASK_HH

#include "atomic.hh"
#include "thread.hh"
#include "coroutine.hh"
#include "timespec.hh"
#include "descriptors.hh"

#include <deque>
#include <set>
#include <utility>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "runner.hh"

namespace fw {

#define SEC2MS(s) (s*1000)

// flags for task state
#define _TASK_SLEEP     (1<<0)
#define _TASK_RUNNING   (1<<1)
#define _TASK_EXIT      (1<<2)
#define _TASK_MIGRATE   (1<<3)
#define _TASK_INTERRUPT (1<<4)

class runner;

//! coroutine with scheduling
//
//! tasks are scheduld by runners
//! runners can suspend tasks to wait
//! on io and timeouts while allowing
//! other tasks to run cooperatively.
class task {
public:
    typedef boost::function<void ()> proc;
    typedef std::deque<task> deque;
    typedef std::set<task> set;
    //! thrown when _TASK_INTERRUPT is set
    struct interrupt_unwind : std::exception {};

    //! spawn a task and add it to a runners run queue
    static task spawn(const proc &f,
        runner *in=NULL,
        size_t stack_size=DEFAULT_STACK_SIZE);

    //! yield the current task
    static void yield();
    //! migrate a task to a different runner
    //
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

    //! the current task
    static task self();

    task();
    void cancel();
    bool done() const;

    std::string str() const;
public: /* operators */

    bool operator == (const task &t) const {
        return m.get() == t.m.get();
    }

    bool operator != (const task &t) const {
        return m.get() != t.m.get();
    }

    bool operator < (const task &t) const {
        return m.get() < t.m.get();
    }

    friend std::ostream &operator << (std::ostream &o, const task &t);

private: /* implementation details */
    struct impl;
    friend std::ostream &operator << (std::ostream &o, const impl *t);
    typedef boost::shared_ptr<impl> shared_impl;

    //! number of tasks currently existing across all runners
    static atomic_count ntasks;

    shared_impl m;

    task(const shared_impl &m_) : m(m_) {}
    task(const proc &f_, size_t stack_size);
    static void start(impl *);

private: /* runner interface */
    friend class runner;
    friend struct task_poll_state;
    friend struct task_timeout_heap_compare;

    void set_state(const std::string &str);

    const std::string &get_state() const;
    static int get_ntasks() { return ntasks; }


    static void swap(task &from, task &to);
private: /* condition interface */
    //! suspend the task while the condition is false
    void suspend(mutex::scoped_lock &l);
    //! resume the task in the same runner where it was suspended
    void resume();

public:
    //! task aware condition.
    //
    //! must be used inside a spawned task.
    //! will only put the task to sleep
    //! allowing the runner to continue scheduling other tasks
    //! should be thread safe, to allow channel to be thread safe
    //! see channel for usage example
    class condition : boost::noncopyable {
    public:
        condition() {}

        //! signal one of the waiters
        void signal();

        //! resume all waiters
        void broadcast();

        //! wait for condition to be signaled
        void wait(mutex::scoped_lock &l);

        // TODO: timed wait, so channels can have timeouts
        bool timed_wait(mutex::scoped_lock &l, unsigned int ms);
    private:
        //! mutex used to ensure thread safety
        mutex mm;
        //! tasks waiting on this condition
        task::deque waiters;
    };

public:
    class socket : boost::noncopyable {
    public:
        socket(int fd) throw (errno_error);

        socket(int domain, int type, int protocol=0) throw (errno_error);

        void bind(address &addr) throw (errno_error) { s.bind(addr); }
        void listen(int backlog=128) throw (errno_error) { s.listen(backlog); }
        bool getpeername(address &addr) throw (errno_error) __attribute__((warn_unused_result)) {
            return s.getpeername(addr);
        }
        void getsockname(address &addr) throw (errno_error) {
            return s.getsockname(addr);
        }
        template <typename T> void getsockopt(int level, int optname,
            T &optval, socklen_t &optlen) throw (errno_error)
        {
            return s.getsockopt(level, optname, optval, optlen);
        }
        template <typename T> void setsockopt(int level, int optname,
            const T &optval, socklen_t optlen) throw (errno_error)
        {
            return s.setsockopt(level, optname, optval, optlen);
        }
        template <typename T> void setsockopt(int level, int optname,
            const T &optval) throw (errno_error)
        {
            return s.setsockopt(level, optname, optval);
        }

        //! dial requires a large 8MB stack size for getaddrinfo
        int dial(const char *addr, uint16_t port, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

        int connect(const address &addr, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

        int accept(address &addr, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

        ssize_t recv(void *buf, size_t len, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

        ssize_t send(const void *buf, size_t len, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result));

        void close() { s.close(); }

        socket_fd s;
    };

    friend class round_robin_scheduler;
};

//! task scheduler interface
class scheduler : public boost::noncopyable {
public:
    virtual void add_pollfds(task &t, pollfd *fds, nfds_t nfds) = 0;
    virtual int remove_pollfds(pollfd *fds, nfds_t nfds) = 0;
    virtual void add_to_runqueue(task &t) = 0;
    virtual void add_waiter(task &t) = 0;
    virtual bool remove_waiter(task &t) = 0;
    virtual void wakeup() = 0;
    virtual void schedule(runner &r, mutex::scoped_lock &l, int thread_timeout_ms) = 0;
    virtual task get_current_task() = 0;
    virtual void swap_to_scheduler() = 0;
    virtual bool empty() = 0;
    virtual ~scheduler() {}

    static scheduler *create();
};

} // end namespace fw

#endif // TASK_HH

