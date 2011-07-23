#ifndef RUNNER_HH
#define RUNNER_HH

#include "descriptors.hh"
#include "task.hh"
#include <list>

class runner : boost::noncopyable {
public:
    typedef std::list<runner *> list;

    thread get_thread() { return thread(tt); }

    static runner *spawn(const task::proc &f) {
        return new runner(f);
    }

    static runner *self();

    void schedule();

    void wakeup() {
        mutex::scoped_lock l(mut);
        wakeup_nolock();
    }
public: /* task interface */

    void set_task(task *t) {
        t->set_runner(this);
        current_task = t;
    }

    task *get_task() {
        return current_task;
    }

    void add_pollfds(task *t, pollfd *fds, nfds_t nfds) {
        for (nfds_t i=0; i<nfds; ++i) {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            // make room for the highest fd number
            int fd = fds[i].fd;
            if (pollfds.size() <= fd) {
                pollfds.resize(fd+1);
            }
            ev.events = fds[i].events;
            ev.data.fd = fd;
            assert(efd.add(fd, ev) == 0);
            pollfds[fd] = task_poll_state(t, &fds[i]);
        }
    }

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

    void add_waiter(task *t);

    task scheduler;

private:
    thread tt;
    mutex mut;
    condition cond;
    bool asleep;
    task *current_task;
    task::deque runq;
    epoll_fd efd;
    typedef std::vector<task *> task_heap;
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


    runner() : asleep(false), current_task(&scheduler) {
        tt=thread::self();
    }

    runner(task *t) : asleep(false), current_task(&scheduler) {
        add_to_runqueue(t);
        thread::create(tt, start, this);
    }

    runner(const task::proc &f) : asleep(false), current_task(&scheduler) {
        task::spawn(f, this);
        thread::create(tt, start, this);
    }

    void sleep(mutex::scoped_lock &l);

    void run_queued_tasks();
    void check_io();

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

    static void *start(void *arg);
};

#endif // RUNNER_HH
