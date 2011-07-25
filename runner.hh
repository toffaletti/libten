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

    //! return the thread this runner is using
    thread get_thread() { return thread(tt); }

    //! spawn a new runner with a task that will execute
    static runner *spawn(const task::proc &f);

    //! return the runner for this thread
    static runner *self();

    //! schedule all tasks in this runner
    //! will block until all tasks exit
    void schedule();

    //! wake runner from sleep state
    //! runners go to sleep when they have no tasks
    //! to schedule.
    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }
public: /* task interface */

    void set_task(task &t) {
        t.set_runner(this);
        current_task = t;
    }

    task get_task() {
        return current_task;
    }

    //! add fds to this runners epoll fd
    //! param t task to wake up for fd events
    //! param fds pointer to array of pollfd structs
    //! param nfds number of pollfd structs in array
    void add_pollfds(task *t, pollfd *fds, nfds_t nfds) {
        for (nfds_t i=0; i<nfds; ++i) {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            int fd = fds[i].fd;
            // make room for the highest fd number
            if (pollfds.size() <= fd) {
                pollfds.resize(fd+1);
            }
            ev.events = fds[i].events;
            ev.data.fd = fd;
            assert(efd.add(fd, ev) == 0);
            pollfds[fd] = task_poll_state(t, &fds[i]);
        }
    }

    //! remove fds from epoll fd
    int remove_pollfds(pollfd *fds, nfds_t nfds) {
        int rvalue = 0;
        for (nfds_t i=0; i<nfds; ++i) {
            if (fds[i].revents) rvalue++;
            pollfds[fds[i].fd].t = 0;
            pollfds[fds[i].fd].pfd = 0;
            assert(efd.remove(fds[i].fd) == 0);
        }
        return rvalue;
    }

    //! add task to run queue.
    //! will wakeup runner if it was sleeping.
    bool add_to_runqueue(const task &t) {
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

    //! add task to wait list.
    //! used for io and timeouts
    void add_waiter(task &t);

    //! task used for scheduling
    task scheduler; // TODO: maybe this can just be a coroutine?

private:
    thread tt;
    mutex mut;
    condition cond;
    bool asleep;
    task current_task;
    task::deque runq;
    epoll_fd efd;
    typedef std::vector<task> task_heap;
    // tasks waiting with a timeout value set
    task_heap waiters;
    // key is the fd number
    struct task_poll_state {
        task *t;
        pollfd *pfd;
        task_poll_state() : t(0), pfd(0) {}
        task_poll_state(task *t_, pollfd *pfd_)
            : t(t_), pfd(pfd_) {}
    };
    typedef std::vector<task_poll_state> poll_task_array;
    poll_task_array pollfds;


    runner();
    runner(const task &t);
    runner(const task::proc &f);

    void sleep(mutex::scoped_lock &l);

    void run_queued_tasks();
    void check_io();

    bool add_to_runqueue_if_asleep(const task &t) {
        mutex::scoped_lock l(mut);
        if (asleep) {
            runq.push_back(t);
            wakeup_nolock();
            return true;
        }
        return false;
    }

    void delete_from_runqueue(const task &t) {
        mutex::scoped_lock l(mut);
        assert(t == runq.back());
        runq.pop_back();
    }

    // lock must already be held
    void wakeup_nolock() {
        if (asleep) {
            asleep = false;
            cond.signal();
        }
    }

    static void add_to_empty_runqueue(const task &);

    static void *start(void *arg);
};

#endif // RUNNER_HH
