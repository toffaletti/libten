#include "task.hh"
#include "runner.hh"
#include "channel.hh"
#include <sstream>
#include <netdb.h>

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
    atomic_bits<uint32_t> flags;

    impl() : name("maintask"), flags(_TASK_RUNNING) {}
    impl(const proc &f_, size_t stack_size=16*1024)
        : name("task"), co((coroutine::proc)task::start, this, stack_size),
        f(f_), flags(_TASK_SLEEP)
    {
        ++ntasks;
    }

    ~impl() { if (!co.main()) { --ntasks; } }

    task to_task() { return shared_from_this(); }
};


static void milliseconds_to_timespec(unsigned int ms, timespec &ts) {
    // convert milliseconds to seconds and nanoseconds
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
}

task::task() { m.reset(new impl); }

task::task(const proc &f_, size_t stack_size) {
    m.reset(new impl(f_, stack_size));
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
    m->flags |= _TASK_INTERRUPT;
    m->in.add_to_runqueue(*this);
    // order of operations:
    // check for this flag in task::yield
    // throw task::interrupt_unwind exception
    // catch exception to remove pollfds, etc. rethrow
    // remove from waiters inside runloop
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
    } catch (channel_closed_error &e) {
        fprintf(stderr, "caught channel close error in task(%p)\n", t.m.get());
    } catch(std::exception &e) {
        fprintf(stderr, "exception in task(%p): %s\n", t.m.get(), e.what());
        abort();
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
    if (to)
        t.set_runner(*to);
    else
        t.m->in.m.reset();
    t.m->flags |= _TASK_MIGRATE;
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
    }
    l.lock();
}

void task::resume() {
    task t = m->to_task();
    assert(m->in.add_to_runqueue(t) == true);
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
    } else {
        t.m->ts.tv_sec = -1;
        t.m->ts.tv_nsec = -1;
    }
    r.add_waiter(t);
    r.add_pollfds(t, fds, nfds);

    // will be woken back up by epoll loop in runner::schedule()
    try {
        task::yield();
    } catch (interrupt_unwind &e) {
        r.remove_pollfds(fds, nfds);
        throw;
    }

    return r.remove_pollfds(fds, nfds);
}

void task::set_runner(runner &i) {
    m->in = i;
}

runner &task::get_runner() const {
    return m->in;
}

void task::set_state(const std::string &str) {
    m->state = str;
}

void task::clear_flag(uint32_t f) { m->flags ^= f; }
void task::set_flag(uint32_t f) { m->flags |= f; }
bool task::test_flag_set(uint32_t f) const { return m->flags & f; }
bool task::test_flag_not_set(uint32_t f) const { return m->flags ^ f; }

const std::string &task::get_state() const { return m->state; }
const timespec &task::get_timeout() const { return m->ts; }
void task::set_abs_timeout(const timespec &abs) { m->ts = abs; }

task task::impl_to_task(task::impl *i) {
    return i->to_task();
}

coroutine *task::get_coroutine() {
    return &m->co;
}

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
    t.set_flag(_TASK_SLEEP);
    {
        mutex::scoped_lock ll(mm);
        if (std::find(waiters.begin(), waiters.end(), t) == waiters.end()) {
            waiters.push_back(t);
        }
    }
    t.suspend(l);
}

/* task::socket */

task::socket::socket(int fd) throw (errno_error) : s(fd) {}

task::socket::socket(int domain, int type, int protocol) throw (errno_error)
    : s(domain, type, protocol)
{
    s.setnonblock();
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
    ssize_t total_sent=0;
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
        }
        total_sent += nw;
    }
    return total_sent;
}

