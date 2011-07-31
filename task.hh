#ifndef TASK_HH
#define TASK_HH

#include "atomic.hh"
#include "thread.hh"
#include "coroutine.hh"
#include "timespec.hh"
#include "descriptors.hh"

#include <deque>
#include <utility>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "runner.hh"

// flags for task state
#define _TASK_SLEEP     (1<<0)
#define _TASK_RUNNING   (1<<1)
#define _TASK_EXIT      (1<<2)
#define _TASK_MIGRATE   (1<<3)

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

    //! spawn a task and add it to a runners run queue
    static task spawn(const proc &f, runner *in=NULL);

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

    friend std::ostream &operator << (std::ostream &o, const task &t) {
        o << "task:" << t.m.get();
        return o;
    }

private: /* implementation details */
    struct impl;
    typedef boost::shared_ptr<impl> shared_impl;

    //! number of tasks currently existing across all runners
    static atomic_count ntasks;

    shared_impl m;

    task(const shared_impl &m_) : m(m_) {}
    task(const proc &f_, size_t stack_size=16*1024);
    static void start(impl *);

private: /* runner interface */
    friend class runner;
    friend struct task_poll_state;
    friend struct task_timeout_heap_compare;

    void set_runner(runner &i);
    runner &get_runner() const;

    static task impl_to_task(task::impl *);

    coroutine *get_coroutine();
    void set_state(const std::string &str);
    void clear_flag(uint32_t f);
    void set_flag(uint32_t f);
    bool test_flag_set(uint32_t f) const;
    bool test_flag_not_set(uint32_t f) const;

    const std::string &get_state() const;
    const timespec &get_timeout() const;
    void set_abs_timeout(const timespec &abs);
    static int get_ntasks() { return ntasks; }

    task() {}
    explicit task(bool);

    static void swap(task &from, task &to);
private: /* condition interface */
    static task self();
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
        void signal() {
            mutex::scoped_lock l(mm);
            if (!waiters.empty()) {
                task t = waiters.front();
                waiters.pop_front();
                t.resume();
            }
        }

        //! resume all waiters
        void broadcast() {
            mutex::scoped_lock l(mm);
            for (task::deque::iterator i=waiters.begin();
                i!=waiters.end(); ++i)
            {
                i->resume();
            }
            waiters.clear();
        }

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
        //! mutex used to ensure thread safety
        mutex mm;
        //! tasks waiting on this condition
        task::deque waiters;
    };

public:
    class socket : boost::noncopyable {
    public:
        socket(int domain, int type, int protocol=0) throw (errno_error)
            : s(domain, type, protocol)
        {
            s.setnonblock();
        }

        void bind(address &addr) throw (errno_error) {
            s.bind(addr);
        }

        int connect(address &addr, unsigned int timeout_ms=0) __attribute__((warn_unused_result)) {
            while (s.connect(addr) < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EINPROGRESS || errno == EADDRINUSE) {
                    if (task::poll(s.fd, EPOLLOUT, timeout_ms)) {
                        return 0;
                    } else {
                        errno = ETIMEDOUT;
                    }
                }
                return -1;
            }
            return 0;
        }

        int accept(address &addr, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result)) {
            flags |= SOCK_NONBLOCK;
            int fd;
            while ((fd = s.accept(addr, flags)) < 0) {
                if (errno == EINTR)
                    continue;
                if (!IO_NOT_READY_ERROR)
                    return -1;
                if (!task::poll(s.fd, EPOLLIN, timeout_ms)) {
                    errno = ETIMEDOUT;
                    return -1;
                }
            }
            return fd;
        }

        ssize_t recv(void *buf, size_t len, int flags=0, unsigned int timeout_ms=0) __attribute__((warn_unused_result)) {
            ssize_t nr;
            while ((nr = s.recv(buf, len, flags)) < 0) {
                if (errno == EINTR)
                    continue;
                if (!IO_NOT_READY_ERROR)
                    break;
                if (!task::poll(s.fd, EPOLLIN, timeout_ms)) {
                    errno = ETIMEDOUT;
                    break;
                }
            }
            return nr;
        }

    private:
        socket_fd s;
    };
};

#endif // TASK_HH

