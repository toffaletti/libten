#ifndef RUNNER_HH
#define RUNNER_HH

#include "descriptors.hh"
#include "task.hh"
#include <list>

//! scheduler for tasks

//! runs tasks in an OS-thread
//! uses epoll for io events and timeouts
class runner : boost::noncopyable {
public:
    typedef std::list<runner *> list;

    //! \return the thread this runner is using
    thread get_thread() { return thread(tt); }

    //! spawn a new runner with a task that will execute
    static runner *spawn(const task::proc &f);

    //! \return the runner for this thread
    static runner *self();

    //! schedule all tasks in this runner
    //! will block until all tasks exit
    void schedule();

    //! wake runner from sleep state
    //
    //! runners go to sleep when they have no tasks
    //! to schedule.
    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }
private: /* task interface */
    friend class task;

    void set_task(task &t) {
        t.set_runner(this);
        current_task = t;
    }

    task get_task() {
        return current_task;
    }

    //! add fds to this runners epoll fd
    //! \param t task to wake up for fd events
    //! \param fds pointer to array of pollfd structs
    //! \param nfds number of pollfd structs in array
    void add_pollfds(task::impl *t, pollfd *fds, nfds_t nfds);

    //! remove fds from epoll fd
    int remove_pollfds(pollfd *fds, nfds_t nfds);

    //! add task to run queue.
    //! will wakeup runner if it was sleeping.
    bool add_to_runqueue(task &t);

    //! add task to wait list.
    //! used for io and timeouts
    void add_waiter(task &t);

    //! task used for scheduling
    task scheduler; // TODO: maybe this can just be a coroutine?

private: /* internal */
    thread tt;
    mutex mut;
    //condition cond;
    task current_task;
    task::deque runq;
    pipe_fd pi;
    epoll_fd efd;
    typedef std::vector<task> task_heap;
    // tasks waiting with a timeout value set
    task_heap waiters;
    // key is the fd number
    struct task_poll_state {
        task::impl *t;
        pollfd *pfd;
        task_poll_state() : t(0), pfd(0) {}
        task_poll_state(task::impl *t_, pollfd *pfd_)
            : t(t_), pfd(pfd_) {}
    };
    typedef std::vector<task_poll_state> poll_task_array;
    poll_task_array pollfds;
    //! time calculated once through event loop
    timespec now;

    runner();
    runner(task &t);
    ~runner();

    void add_pipe();

    void run_queued_tasks();
    void check_io();

    void delete_from_runqueue(task &t) {
        mutex::scoped_lock l(mut);
        assert(t == runq.back());
        runq.pop_back();
        t.set_flag(_TASK_SLEEP);
    }

    //! lock must already be held
    void wakeup_nolock() {
        ssize_t nw = pi.write("\1", 1);
        (void)nw;
    }

    static void *start(void *arg);
};

#endif // RUNNER_HH
