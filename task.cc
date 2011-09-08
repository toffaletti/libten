#include "logging.hh"
#include "task.hh"
#include "runner.hh"
#include "channel.hh"
#include <sstream>
#include <netdb.h>

namespace fw {

// static
atomic_count task::ntasks(0);

struct task::impl : boost::noncopyable, public boost::enable_shared_from_this<impl> {
    //! the runner this task is being executed in or migrated to
    runner in;
    //! function for this task to execute
    proc f;
    //! name and state are for future use, to make debugging easier
    std::string name;
    std::string state;
    //! timeout value for this task
    timespec ts;
    coroutine co;
    mutex mut;
    atomic_bits<uint32_t> flags;

    impl() : name("maintask"), flags(_TASK_RUNNING) {}
    impl(const proc &f_, size_t stack_size=16*1024)
        : f(f_), name("task"), co((coroutine::proc)task::start, this, stack_size),
        flags(_TASK_SLEEP)
    {
        ++ntasks;
    }

    ~impl() { if (!co.main()) { --ntasks; } }

    task to_task() { return shared_from_this(); }
};

static unsigned int timespec_to_milliseconds(const timespec &ts) {
    // convert timespec to milliseconds
    unsigned int ms = ts.tv_sec * 1000;
    // 1 millisecond is 1 million nanoseconds
    ms += ts.tv_nsec / 1000000;
    return ms;
}

static void milliseconds_to_timespec(unsigned int ms, timespec &ts) {
    // convert milliseconds to seconds and nanoseconds
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
}

task::task() { m.reset(new impl); }

task::task(const proc &f_, size_t stack_size) {
    m.reset(new impl(f_, stack_size));
}

std::ostream &operator << (std::ostream &o, const task::impl *t) {
    // TODO: replace the pointer with task "id" (atomicly incremented global counter)
    o << "[" << t->name << ":" << t->state << ":" << (void*)t << ":" << t->flags << ":" << t->ts << "]";
    return o;
}

std::ostream &operator << (std::ostream &o, const task &t) {
    // TODO: replace the pointer with task "id" (atomicly incremented global counter)
    o << "[" << t.m->name << ":" << t.m->state << ":" << (void*)t.m.get() << ":" << t.m->flags << "]";
    return o;
}

std::string task::str() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

task task::self() {
    return runner::self().get_task();
}

void task::cancel() {
    mutex::scoped_lock rl(m->mut);
    m->flags |= _TASK_INTERRUPT;
    // if this task isn't in a runner
    // it either will be soon (migrate)
    // or its exiting
    if (m->in) m->in.add_to_runqueue(*this);
    // order of operations:
    // check for this flag in task::yield
    // throw task::interrupt_unwind exception
    // catch exception to remove pollfds, etc. rethrow
    // remove from waiters inside runloop

    // reset the shared_ptr so the task can be freed
    m.reset();
}

bool task::done() const {
    return m->flags & _TASK_EXIT;
}

void task::swap(task &from, task &to) {
    assert(!(to.m->flags & _TASK_RUNNING));
    to.m->flags |= _TASK_RUNNING;

    assert(from.m->flags & _TASK_RUNNING);
    from.m->flags ^= _TASK_RUNNING;

    from.m->co.swap(&to.m->co);

    assert(to.m->flags & _TASK_RUNNING);
    to.m->flags ^= _TASK_RUNNING;

    assert(!(from.m->flags & _TASK_RUNNING));
    from.m->flags |= _TASK_RUNNING;
}

task task::spawn(const proc &f, runner *in, size_t stack_size) {
    task t(f, stack_size);
    if (in) {
        in->add_to_runqueue(t);
    } else {
        runner::self().add_to_runqueue(t);
    }
    return t;
}

void task::yield() {
    runner::swap_to_scheduler();

    if (task::self().m->flags & _TASK_INTERRUPT) {
        throw interrupt_unwind();
    }
}

void task::start(impl *i) {
    task t = i->to_task();
    try {
        t.m->f();
    } catch (interrupt_unwind &e) {
        // task was canceled
    } catch (backtrace_exception &e) {
        LOG(FATAL) << "uncaught exception in task(" << t << "): " << e.what() << "\n" << e.str();
    } catch(std::exception &e) {
        LOG(FATAL) << "uncaught exception in task(" << t << "): " << e.what();
    }
    // NOTE: the scheduler deletes tasks in exiting state
    // so this function won't ever return. don't expect
    // objects on this stack to have the destructor called
    t.m->flags |= _TASK_EXIT;
    t.m.reset();
    // this is a dangerous bit of code, the shared_ptr is reset
    // potentially freeing the impl *, however the scheduler
    // should always be holding a reference, so it is "safe"

    runner::swap_to_scheduler();
}

void task::migrate(runner *to) {
    task t = task::self();
    assert(t.m->co.main() == false);
    // if "to" is NULL, first available runner is used
    // or a new one is spawned
    // logic is in schedule()
    {
        mutex::scoped_lock l(t.m->mut);
        if (to)
            t.m->in = *to;
        else
            t.m->in.m.reset();
        t.m->flags |= _TASK_MIGRATE;
    }
    task::yield();
    // will resume in other runner
}

void task::sleep(unsigned int ms) {
    task t = task::self();
    assert(t.m->co.main() == false);
    milliseconds_to_timespec(ms, t.m->ts);
    runner::self().add_waiter(t);
    task::yield();
}

void task::suspend(mutex::scoped_lock &l) {
    assert(m->co.main() == false);
    // sleep flag must be set before calling suspend
    // see note in task::condition::wait about race condition
    assert(m->flags & _TASK_SLEEP);
    l.unlock();
    try {
        task::yield();
    } catch (interrupt_unwind &e) {
        l.lock();
        throw;
    } catch (std::exception &e) {
        l.lock();
        throw;
    }
    l.lock();
}

void task::resume() {
    mutex::scoped_lock rl(m->mut);
    task t = m->to_task();
    m->in.add_to_runqueue(t);
}

bool task::poll(int fd, short events, unsigned int ms) {
    pollfd fds = {fd, events, 0};
    return task::poll(&fds, 1, ms);
}

int task::poll(pollfd *fds, nfds_t nfds, int timeout) {
    task t = task::self();
    assert(t.m->co.main() == false);
    runner r = runner::self();
    // TODO: maybe make a state for waiting io?
    //t.m->flags |= _TASK_POLL;
    t.m->flags |= _TASK_SLEEP;
    if (timeout) {
        milliseconds_to_timespec(timeout, t.m->ts);
        r.add_waiter(t);
    }
    r.add_pollfds(t, fds, nfds);

    // will be woken back up by epoll loop in runner::schedule()
    try {
        task::yield();
    } catch (interrupt_unwind &e) {
        if (timeout) r.remove_waiter(t);
        r.remove_pollfds(fds, nfds);
        throw;
    }

    if (timeout) r.remove_waiter(t);
    return r.remove_pollfds(fds, nfds);
}

void task::set_state(const std::string &str) {
    m->state = str;
}

const std::string &task::get_state() const { return m->state; }

/* task::condition */

void task::condition::signal() {
    mutex::scoped_lock l(mm);
    if (!waiters.empty()) {
        task t = waiters.front();
        waiters.pop_front();
        t.resume();
    }
}

void task::condition::broadcast() {
    mutex::scoped_lock l(mm);
    for (task::deque::iterator i=waiters.begin();
        i!=waiters.end(); ++i)
    {
        i->resume();
    }
    waiters.clear();
}

void task::condition::wait(mutex::scoped_lock &l) {
    task t = task::self();
    // must set the flag before adding to waiters
    // because another thread could resume()
    // before this calls suspend()
    t.m->flags |= _TASK_SLEEP;
    {
        mutex::scoped_lock ll(mm);
        if (std::find(waiters.begin(), waiters.end(), t) == waiters.end()) {
            waiters.push_back(t);
        }
    }
    try {
        t.suspend(l);
    } catch (interrupt_unwind &e) {
        // remove task from waiters so it won't get signaled
        mutex::scoped_lock ll(mm);
        task::deque::iterator nend = std::remove(waiters.begin(), waiters.end(), t);
        waiters.erase(nend, waiters.end());
        throw;
    }
}

/* task::socket */

task::socket::socket(int fd) throw (errno_error) : s(fd) {}

task::socket::socket(int domain, int type, int protocol) throw (errno_error)
    : s(domain, type | SOCK_NONBLOCK, protocol)
{
}

int task::socket::dial(const char *addr, uint16_t port, unsigned int timeout_ms) {
    struct addrinfo *results = 0;
    struct addrinfo *result = 0;
    runner r = runner::self();
    task::migrate();
    int status = getaddrinfo(addr, NULL, NULL, &results);
    if (status == 0) {
        for (result = results; result != NULL; result = result->ai_next) {
            address addr(result->ai_addr, result->ai_addrlen);
            addr.port(port);
            status = connect(addr, timeout_ms);
            if (status == 0) break;
        }
    }
    freeaddrinfo(results);
    task::migrate(&r);
    return status;
}

int task::socket::connect(const address &addr, unsigned int timeout_ms) {
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

int task::socket::accept(address &addr, int flags, unsigned int timeout_ms) {
    int fd;
    while ((fd = s.accept(addr, flags | SOCK_NONBLOCK)) < 0) {
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

ssize_t task::socket::recv(void *buf, size_t len, int flags, unsigned int timeout_ms) {
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

ssize_t task::socket::send(const void *buf, size_t len, int flags, unsigned int timeout_ms) {
    size_t total_sent=0;
    while (total_sent < len) {
        ssize_t nw = s.send(&((const char *)buf)[total_sent], len-total_sent, flags);
        if (nw == -1) {
            if (errno == EINTR)
                continue;
            if (!IO_NOT_READY_ERROR)
                return -1;
            if (!task::poll(s.fd, EPOLLOUT, timeout_ms)) {
                errno = ETIMEDOUT;
                return total_sent;
            }
        } else {
            total_sent += nw;
        }
    }
    return total_sent;
}

class round_robin_scheduler : public scheduler {
private:
    struct task_poll_state {
        task::impl *t_in; // POLLIN task
        pollfd *p_in; // pointer to pollfd structure that is on the task's stack
        task::impl *t_out; // POLLOUT task
        pollfd *p_out; // pointer to pollfd structure that is on the task's stack
        uint32_t events; // events this fd is registered for
        task_poll_state() : t_in(0), p_in(0), t_out(0), p_out(0), events(0) {}
    };
    typedef std::vector<task_poll_state> poll_task_array;
    typedef std::vector<epoll_event> event_vector;

    struct task_impl_timeout_compare {
        bool operator ()(const task::impl *a, const task::impl *b) const {
            return a->ts < b->ts;
        }
    };

    typedef std::multiset<task::impl *, task_impl_timeout_compare> task_set;
    typedef std::deque<task::impl *> task_deque;

    task_set timeouts;
    task_deque runq;
    bool runq_dirty;
    //! current time cached in a few places through the event loop
    timespec now;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    poll_task_array pollfds;
    //! number of pollfds
    size_t npollfds;
    //! pipe used to wake up this runner
    pipe_fd pi;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! task used for scheduling (the "main" task, uses default stack)
    task me;
    //! currently executing task
    task current_task;
    //! all tasks belonging to this scheduler 
    task::set all;
    //! epoll events
    event_vector events;

public:
    round_robin_scheduler()
        : timeouts(task_impl_timeout_compare()), runq_dirty(false),
        npollfds(0), pi(O_NONBLOCK), current_task(me.m)
    {
        events.reserve(1000);
        // add the pipe used to wake up this runner from epoll_wait
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = pi.r.fd;
        if (pollfds.size() <= (size_t)pi.r.fd) {
            pollfds.resize(pi.r.fd+1);
        }
        efd.add(pi.r.fd, ev);
    }

    void add_pollfds(task &t, pollfd *fds, nfds_t nfds) {
        for (nfds_t i=0; i<nfds; ++i) {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            int fd = fds[i].fd;
            fds[i].revents = 0;
            // make room for the highest fd number
            if (pollfds.size() <= (size_t)fd) {
                pollfds.resize(fd+1);
            }
            ev.data.fd = fd;
            uint32_t events = pollfds[fd].events;

            if (fds[i].events & EPOLLIN) {
                assert(pollfds[fd].t_in == 0);
                pollfds[fd].t_in = t.m.get();
                pollfds[fd].p_in = &fds[i];
                pollfds[fd].events |= EPOLLIN;
            }

            if (fds[i].events & EPOLLOUT) {
                assert(pollfds[fd].t_out == 0);
                pollfds[fd].t_out = t.m.get();
                pollfds[fd].p_out = &fds[i];
                pollfds[fd].events |= EPOLLOUT;
            }

            ev.events = pollfds[fd].events;

            if (events == 0) {
                THROW_ON_ERROR(efd.add(fd, ev));
            } else if (events != pollfds[fd].events) {
                THROW_ON_ERROR(efd.modify(fd, ev));
            }
            ++npollfds;
        }
    }

    int remove_pollfds(pollfd *fds, nfds_t nfds) {
        int rvalue = 0;
        for (nfds_t i=0; i<nfds; ++i) {
            int fd = fds[i].fd;
            if (fds[i].revents) rvalue++;

            if (pollfds[fd].p_in == &fds[i]) {
                pollfds[fd].t_in = 0;
                pollfds[fd].p_in = 0;
                pollfds[fd].events ^= EPOLLIN;
            }

            if (pollfds[fd].p_out == &fds[i]) {
                pollfds[fd].t_out = 0;
                pollfds[fd].p_out = 0;
                pollfds[fd].events ^= EPOLLOUT;
            }

            if (pollfds[fd].events == 0) {
                THROW_ON_ERROR(efd.remove(fd));
            } else {
                epoll_event ev;
                memset(&ev, 0, sizeof(ev));
                ev.data.fd = fd;
                ev.events = pollfds[fd].events;
                THROW_ON_ERROR(efd.modify(fd, ev));
            }
            --npollfds;
        }
        return rvalue;
    }

    void add_to_runqueue(task &t) {
        if (t.m->flags & _TASK_EXIT) return;

        all.insert(t);
        // task can be in any state, for example a task might be running
        // but still get added to the runqueue from a resume in another
        // thread. this is perfectly valid, the important thing is that
        // this task will get run the next time through the loop
        t.m->flags ^= _TASK_SLEEP;

        // runq will be uniqued in schedule()
        runq_dirty = true;
        if (runq.empty()) {
            runq.push_back(t.m.get());
            wakeup();
        } else {
            runq.push_back(t.m.get());
        }
    }

    void add_waiter(task &t) {
        timespec to = t.m->ts;
        if (to.tv_sec != -1) {
            t.m->ts += (to + now);
        }
        t.m->flags |= _TASK_SLEEP;
        timeouts.insert(t.m.get());
    }

    void remove_waiter(task &t) {
        timeouts.erase(t.m.get());
    }

    void wakeup() {
        ssize_t nw = pi.write("\1", 1);
        (void)nw;
    }

    void erase_task(task &t) {
        task_deque::iterator i = std::remove(runq.begin(), runq.end(), t.m.get());
        runq.erase(i, runq.end());
        timeouts.erase(t.m.get());
        all.erase(t);
    }

    void schedule(runner &r, mutex::scoped_lock &l, int thread_timeout_ms) {
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        task_deque q;
        // TODO: maybe a better way to do this to avoid mallocs, make q class level?
        // would then need to clear() it before swap.
        q.swap(runq);
        // remove duplicates
        task_deque::iterator nend;
        if (runq_dirty) {
            runq_dirty = false;
            nend = std::unique(q.begin(), q.end());
        } else {
            nend = q.end();
        }

        for (task_deque::iterator i=q.begin(); i!=nend; ++i) {
            current_task = (*i)->to_task();
            l.unlock();
            {
                mutex::scoped_lock rl(current_task.m->mut);
                current_task.m->in = r;
            }
            task::swap(me, current_task);
            l.lock();
            if (current_task.m->flags & _TASK_INTERRUPT) {
                erase_task(current_task);
            } else if (current_task.m->flags & _TASK_MIGRATE) {
                current_task.m->flags ^= _TASK_MIGRATE;
                current_task.m->flags |= _TASK_SLEEP;
                l.unlock();
                mutex::scoped_lock rl(current_task.m->mut);
                if (current_task.m->in) {
                    current_task.m->in.add_to_runqueue(current_task);
                } else {
                    // TODO: might make more sense to
                    // do the spawn inside of ::migrate?
                    runner::spawn(current_task);
                }
                l.lock();
                erase_task(current_task);
            } else if (current_task.m->flags & _TASK_SLEEP) {
                // do nothing
            } else if (current_task.m->flags & _TASK_EXIT) {
                erase_task(current_task);
            } else {
                runq.push_back(*i);
            }
        }
        current_task = me;

        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        int timeout_ms = -1;

        if (!timeouts.empty()) {
            if ((*timeouts.begin())->ts <= now) {
                // epoll_wait must return asap
                timeout_ms = 0;
            } else {
                timeout_ms = timespec_to_milliseconds((*timeouts.begin())->ts - now);
                // avoid spinning on timeouts smaller than 1ms
                if (timeout_ms <= 0) timeout_ms = 1;
            }
        }

        if (!runq.empty()) {
            // don't block if there are tasks to run
            timeout_ms = 0;
        }

        if (!runq.empty() && timeouts.empty() && npollfds == 0) {
            // tasks to run, no need to epoll
            return;
        }

        if (timeout_ms == -1 && task::get_ntasks() > 0) {
            timeout_ms = thread_timeout_ms;
        }

        if (timeout_ms == -1 && timeouts.empty() && runq.empty() && npollfds == 0) {
            return;
        }

        if (timeout_ms != 0 || npollfds > 0) {
            // only process 1000 events each time through the event loop
            // to keep things moving along
            events.resize(1000);
#ifndef NDEBUG
            DVLOG(5) << (void *)this << " timeout_ms: " << timeout_ms << " waiters: " << timeouts.size() <<
                " runq: " << runq.size() << " tasks: " << (int)task::ntasks << " npollfds: " << npollfds;
            if (VLOG_IS_ON(5) && !timeouts.empty()) {
                DVLOG(5) << "now: " << now;
                std::copy(timeouts.begin(), timeouts.end(), std::ostream_iterator<task::impl *>(LOG(INFO), "\n"));
            }
#endif // DEBUG OFF

            // unlock around epoll
            l.unlock();
            efd.wait(events, timeout_ms);
            // lock to protect runq
            l.lock();

            if (events.empty() && timeout_ms > 0 && timeout_ms == thread_timeout_ms) {
                throw runner::timeout_exit();
            }

            // wake up io tasks
            for (event_vector::const_iterator i=events.begin();
                i!=events.end();++i)
            {
                // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
                // even if they arent asked for, so we must wake up the tasks on any event
                // to avoid just spinning in epoll.
                int fd = i->data.fd;
                if (pollfds[fd].t_in) {
                    pollfds[fd].p_in->revents = i->events;
                    task t = pollfds[fd].t_in->to_task();
                    add_to_runqueue(t);
                }

                // check to see if pollout is a different task than pollin
                if (pollfds[fd].t_out && pollfds[fd].t_out != pollfds[fd].t_in) {
                    pollfds[fd].p_out->revents = i->events;
                    task t = pollfds[fd].t_out->to_task();
                    add_to_runqueue(t);
                }

                if (i->data.fd == pi.r.fd) {
                    // our wake up pipe was written to
                    char buf[32];
                    // clear pipe
                    while (pi.read(buf, sizeof(buf)) > 0) {}
                } else if (pollfds[fd].t_in == 0 && pollfds[fd].t_out == 0) {
                    // TODO: otherwise we might want to remove fd from epoll
                    LOG(ERROR) << "event " << i->events << " for fd: " << i->data.fd << " but has no task";
                }
            }
        }

        // TODO: maybe use set_(symetric_?)difference instead of erase here
        //task_set to_copy = timeouts;
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        // wake up timeouts tasks
        task_set::iterator i=timeouts.begin();
        for (; i!=timeouts.end(); ++i) {
            if ((*i)->ts <= now) {
                task t = (*i)->to_task();
                add_to_runqueue(t);
            } else {
                break;
            }
        }
        timeouts.erase(timeouts.begin(), i);
    }

    task get_current_task() { return current_task; }

    void swap_to_scheduler() {
        current_task.m->co.swap(&me.m->co);
    }

    bool empty() { return all.empty(); }
};

scheduler *scheduler::create() {
    return new round_robin_scheduler();
}

} // end namespace fw
