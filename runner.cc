#include "runner.hh"
#include "task.hh"
#include <cxxabi.h>

#include <stdexcept>
#include <algorithm>
#include <set>

__thread runner::impl *runner::impl_ = NULL;

runner::list *runner::runners = new runner::list;
mutex *runner::tmutex = new mutex;

typedef std::vector<task> task_heap;
struct task_poll_state {
    task::impl *t;
    pollfd *pfd;
    task_poll_state() : t(0), pfd(0) {}
    task_poll_state(task::impl *t_, pollfd *pfd_)
        : t(t_), pfd(pfd_) {}
};
typedef std::vector<task_poll_state> poll_task_array;

//! internal data members
struct runner::impl : boost::noncopyable, boost::enable_shared_from_this<impl> {
    thread tt;
    mutex mut;
    task::deque runq;
    pipe_fd pi;
    epoll_fd efd;
    // tasks waiting with a timeout value set
    task_heap waiters;
    poll_task_array pollfds;
    //! time calculated once through event loop
    timespec now;
    //! task used for scheduling
    task scheduler; // TODO: maybe this can just be a coroutine?
    task current_task;

    impl();
    impl(task &t);
    ~impl() { }

    void delete_from_runqueue(task &t);
    void run_queued_tasks();
    void check_io();
    void add_pipe();
    void wakeup();
    //! lock must already be held before calling this
    void wakeup_nolock();
    bool add_to_runqueue(task &t);
    // tmutex must be held, shared_from_this is not thread safe
    runner to_runner(mutex::scoped_lock &) { return shared_from_this(); }
};



static unsigned int timespec_to_milliseconds(const timespec &ts) {
    // convert timespec to milliseconds
    unsigned int ms = ts.tv_sec * 1000;
    // 1 millisecond is 1 million nanoseconds
    ms += ts.tv_nsec / 1000000;
    return ms;
}

static std::ostream &operator << (std::ostream &o, task::deque &l) {
    o << "[";
    for (task::deque::iterator i=l.begin(); i!=l.end(); ++i) {
        o << *i << ",";
    }
    o << "]";
    return o;
}

void runner::append_to_list(runner &r, mutex::scoped_lock &l) {
    runners->push_back(r);
}

void runner::remove_from_list(runner &r) {
    mutex::scoped_lock l(*tmutex);
    runners->remove(r);
}

void runner::wakeup_all_runners() {
    runner current = runner::self();
    mutex::scoped_lock l(*tmutex);
    for (runner::list::iterator i=runners->begin(); i!=runners->end(); ++i) {
        if (current == *i) continue;
        i->m->wakeup();
    }
}

struct task_timeout_heap_compare {
    bool operator ()(const task &a, const task &b) const {
        // handle -1 case, which we want at the end
        if (a.get_timeout().tv_sec < 0) return true;
        return a.get_timeout() > b.get_timeout();
    }
};

template <typename SetT> struct in_set {
    SetT &set;

    in_set(SetT &s) : set(s) {}

    bool operator()(const typename SetT::key_type &k) {
        return set.count(k) > 0;
    }
};

runner runner::self() {
    mutex::scoped_lock l(*tmutex);
    if (impl_ == NULL) {
        runner::shared_impl i(new impl);
        runner r = i->to_runner(l);
        append_to_list(r, l);
        impl_ = i.get();
    }
    return impl_->to_runner(l);
}

runner runner::spawn(const task::proc &f) {
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    mutex::scoped_lock l(*tmutex);
    if (runners->size() >= ncpu) {
        runner r = runners->front();
        runners->pop_front();
        runners->push_back(r);
        task::spawn(f, &r);
        return r;
    }
    task t(f);
    runner r(t);
    append_to_list(r, l);
    return r;
}

bool runner::operator == (const runner &r) const {
    return m.get() == r.m.get();
}


void runner::impl::add_pipe() {
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = pi.r.fd;
    if (pollfds.size() <= pi.r.fd) {
        pollfds.resize(pi.r.fd+1);
    }
    efd.add(pi.r.fd, ev);
}

runner::impl::impl() : scheduler(true), current_task(scheduler.m), pi(O_NONBLOCK) {
    add_pipe();
    tt=thread::self();
}

runner::impl::impl(task &t) : scheduler(true), current_task(scheduler.m), pi(O_NONBLOCK) {
    add_pipe();
    add_to_runqueue(t);
    thread::create(tt, runner::start, this);
}

typedef std::vector<epoll_event> event_vector;

void runner::impl::run_queued_tasks() {
    mutex::scoped_lock l(mut);
    while (!runq.empty()) {
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        current_task = runq.front();
        // TODO: maybe current_task.set_runner() here
        runq.pop_front();
        l.unlock();
        task::swap(scheduler, current_task);
        if (current_task.test_flag_set(_TASK_MIGRATE)) {
            current_task.clear_flag(_TASK_MIGRATE);
            current_task.set_flag(_TASK_SLEEP);
            if (current_task.get_runner().m) {
                current_task.get_runner().add_to_runqueue(current_task);
            } else {
                mutex::scoped_lock ll(*tmutex);
                runner rr(current_task);
                append_to_list(rr, ll);
            }
            l.lock();
        } else if (current_task.test_flag_set(_TASK_SLEEP)) {
            l.lock();
        } else if (current_task.test_flag_set(_TASK_EXIT)) {
            l.lock();
        } else {
            runq.push_back(current_task);
            l.lock();
        }
    }
    current_task.m.reset();
}

void runner::impl::check_io() {
    event_vector events(efd.maxevents ? efd.maxevents : 1);
    bool done = false;
    while (!done) {
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        std::make_heap(waiters.begin(), waiters.end(), task_timeout_heap_compare());
        int timeout_ms = -1;
        if (!waiters.empty()) {
            if (waiters.front().get_timeout().tv_sec > 0) {
                if (waiters.front().get_timeout() <= now) {
                    // epoll_wait must return immediately
                    timeout_ms = 0;
                } else {
                    timeout_ms = timespec_to_milliseconds(waiters.front().get_timeout() - now);
                    // help avoid spinning on timeouts smaller than 1 ms
                    if (timeout_ms <= 0) timeout_ms = 1;
                }
            }
        }

        if (events.empty()) {
            events.resize(efd.maxevents ? efd.maxevents : 1);
        }

        if (task::get_ntasks() == 0) {
            wakeup_all_runners();
            return;
        }
        efd.wait(events, timeout_ms);

        std::set<task> wake_tasks;
        for (event_vector::const_iterator i=events.begin();
            i!=events.end();++i)
        {
            task::impl *ti = pollfds[i->data.fd].t;
            pollfd *pfd = pollfds[i->data.fd].pfd;
            if (ti && pfd) {
                pfd->revents = i->events;
                task t = ti->to_task();
                add_to_runqueue(t);
                wake_tasks.insert(t);
            } else if (i->data.fd == pi.r.fd) {
                // our wake up pipe was written to
                char buf[32];
                // clear pipe
                while (pi.read(buf, sizeof(buf)) > 0) {}
                done = true;
            } else {
                // TODO: otherwise we might want to remove fd from epoll
                fprintf(stderr, "event for fd: %i but has no task\n", i->data.fd);
            }
        }

        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        for (task_heap::iterator i=waiters.begin(); i!=waiters.end(); ++i) {
            if (i->get_timeout().tv_sec > 0 && i->get_timeout() <= now) {
                wake_tasks.insert(*i);
                add_to_runqueue(*i);
            }
        }

        task_heap::iterator nend =
            std::remove_if(waiters.begin(), waiters.end(), in_set<std::set<task> >(wake_tasks));
        waiters.erase(nend, waiters.end());
        // there are tasks to wake up!
        if (!wake_tasks.empty()) done = true;
    }
}

void runner::schedule() {
    while (task::get_ntasks() > 0) {
        m->run_queued_tasks();
        m->check_io();
    }
}

void runner::add_pollfds(task &t, pollfd *fds, nfds_t nfds) {
    for (nfds_t i=0; i<nfds; ++i) {
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        int fd = fds[i].fd;
        // make room for the highest fd number
        if (m->pollfds.size() <= fd) {
            m->pollfds.resize(fd+1);
        }
        ev.events = fds[i].events;
        ev.data.fd = fd;
        assert(m->efd.add(fd, ev) == 0);
        m->pollfds[fd] = task_poll_state(t.m.get(), &fds[i]);
    }
}

int runner::remove_pollfds(pollfd *fds, nfds_t nfds) {
    int rvalue = 0;
    for (nfds_t i=0; i<nfds; ++i) {
        if (fds[i].revents) rvalue++;
        m->pollfds[fds[i].fd].t = 0;
        m->pollfds[fds[i].fd].pfd = 0;
        assert(m->efd.remove(fds[i].fd) == 0);
    }
    return rvalue;
}

bool runner::add_to_runqueue(task &t) {
    return m->add_to_runqueue(t);
}

bool runner::impl::add_to_runqueue(task &t) {
    mutex::scoped_lock l(mut);
    assert(t.test_flag_not_set(_TASK_RUNNING));
    assert(t.test_flag_set(_TASK_SLEEP));
    t.clear_flag(_TASK_SLEEP);
    // TODO: might not need to check this anymore with flags
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

void *runner::start(void *arg) {
    mutex::scoped_lock l(*tmutex);
    impl_ = ((runner::impl *)arg);
    thread::self().detach();
    runner r;
    r = impl_->to_runner(l);
    l.unlock();
    try {
        r.schedule();
    } catch (abi::__forced_unwind&) {
        remove_from_list(r);
        throw;
    } catch (std::exception &e) {
        fprintf(stderr, "uncaught exception in runner: %s\n", e.what());
    }
    remove_from_list(r);
    l.lock();
    impl_ = NULL;
    return NULL;
}

void runner::add_waiter(task &t) {
    timespec to = t.get_timeout();
    if (to.tv_sec != -1) {
        t.set_abs_timeout(to + m->now);
    }
    t.set_flag(_TASK_SLEEP);
    m->waiters.push_back(t);
}

void runner::impl::delete_from_runqueue(task &t) {
    mutex::scoped_lock l(mut);
    assert(t == runq.back());
    runq.pop_back();
    t.set_flag(_TASK_SLEEP);
}

thread runner::get_thread() { return thread(m->tt); }

void runner::impl::wakeup() {
    mutex::scoped_lock l(mut);
    wakeup_nolock();
}

task runner::get_task() {
    return m->current_task;
}

runner::runner() {}
runner::runner(const shared_impl &m_) : m(m_) {}
runner::runner(task &t) { m.reset(new impl(t)); }

void runner::impl::wakeup_nolock() {
    ssize_t nw = pi.write("\1", 1);
    (void)nw;
}

void runner::swap_to_scheduler() {
    impl_->current_task.m->co.swap(&impl_->scheduler.m->co);
}

