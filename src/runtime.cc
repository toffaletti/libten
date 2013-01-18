#include "ten/task/runtime.hh"
#include "ten/task/deadline.hh"
#include "ten/task/io.hh"
#include "thread_context.hh"


namespace ten {
extern void netinit();

namespace {
static std::mutex runtime_mutex;
static std::once_flag init_flag;
static std::vector<thread_context *> threads;
static std::atomic<uint64_t> thread_count{0};
static task_pimpl *waiting_task = nullptr;

struct runtime_tag {};
ten::thread_cached<runtime_tag, thread_context> this_ctx;

static void add_thread(thread_context *ctx) {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    threads.push_back(ctx);
    ++thread_count;
}

static void remove_thread(thread_context *ctx) {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    auto i = find(begin(threads), end(threads), ctx);
    if (i != end(threads)) {
        if (thread_count == 2 && waiting_task) {
            waiting_task->ready();
        }
        threads.erase(i);
        --thread_count;
    }
}

static void runtime_init() {
    CHECK(getpid() == syscall(SYS_gettid)) << "must call in main thread before anything else";
    //ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
    stack_t ss;
    ss.ss_sp = calloc(1, SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    THROW_ON_ERROR(sigaltstack(&ss, NULL));

    // allow log files and message queues to be created group writable
    umask(0);
    InitGoogleLogging(program_invocation_short_name);
    InstallFailureSignalHandler();
    FLAGS_logtostderr = true;

    struct sigaction act;

    // ignore SIGPIPE
    memset(&act, 0, sizeof(act));
    THROW_ON_ERROR(sigaction(SIGPIPE, NULL, &act));
    if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        THROW_ON_ERROR(sigaction(SIGPIPE, &act, NULL));
    }

    netinit();
}


} // anon

thread_context::thread_context() {
    std::call_once(init_flag, runtime_init);
    add_thread(this);
}

thread_context::~thread_context() {
    remove_thread(this);
}

///////////// scheduler /////////////

scheduler::scheduler() : _canceled{false} {
    // TODO: reenable this when our libstdc++ is fixed
    //static_assert(clock::is_steady, "clock not steady");
    update_cached_time();
    _task._scheduler = this;
    _current_task = &_task;
}

scheduler::~scheduler() {
    // TODO: this is a little ugly
    while (!_alltasks.empty()) {
        schedule();
    }
}

void scheduler::cancel() {
    _canceled = true;
    wakeup();
}

void scheduler::ready_for_io(task_pimpl *t) {
    if (t->_ready.exchange(true) == false) {
        _readyq.push_back(t);
    }
}

void scheduler::wakeup() {
    // TODO: speed this up?
    std::unique_lock<std::mutex> lock{_mutex};
    if (pending_io()) {
        get_io()->wakeup();
    } else {
        _cv.notify_one();
    }
}

void scheduler::ready(task_pimpl *t) {
    if (t->_ready.exchange(true) == false) {
        if (this != &this_ctx->scheduler) {
            _dirtyq.push(t);
            wakeup();
        } else {
            DVLOG(5) << "readyq scheduler::ready: " << t;
            _readyq.push_back(t);
        }
    }
}

void scheduler::remove_task(task_pimpl *t) {
    DVLOG(5) << "remove task " << t;
    using namespace std;
    DCHECK(t->_scheduler == this);
    // assert not in readyq
    DCHECK(
            find(begin(_readyq), end(_readyq), t) == end(_readyq)
          );

    // TODO: needed?
    //_alarms.remove(t);

    auto i = find_if(begin(_alltasks), end(_alltasks),
            [t](shared_task &other) {
            return other.get() == t;
            });
    DCHECK(i != end(_alltasks));
    _gctasks.push_back(*i);
    _alltasks.erase(i);
}

int scheduler::dump() {
    LOG(INFO) << &_task;
#ifdef TEN_TASK_TRACE
    LOG(INFO) << _task._trace.str();
#endif
    for (shared_task &t : _alltasks) {
        LOG(INFO) << t.get();
#ifdef TEN_TASK_TRACE
        LOG(INFO) << t->_trace.str();
#endif
    }
    return 0;
}

void scheduler::check_dirty_queue() {
    using namespace std;
    task_pimpl *t = nullptr;
    while (_dirtyq.pop(t)) {
        if (t == &_task ||
                find_if(begin(_alltasks), end(_alltasks),
                    [t](shared_task &other) {
                    return other.get() == t;
                    }) != end(_alltasks))
        {
            DVLOG(5) << "readyq adding " << t << " from dirtyq";
            _readyq.push_back(t);
        } else {
            DVLOG(5) << "found " << t << " in dirtyq but not in alltasks";
        }
    }
}

void scheduler::check_timeout_tasks() {
    _alarms.tick(_now, [this](task_pimpl *t, std::exception_ptr exception) {
        if (exception != nullptr && t->_exception == nullptr) {
            DVLOG(5) << "alarm with exception fired: " << t;
            t->_exception = exception;
        }
        ready(t);
    });
}

void scheduler::wait(std::unique_lock <std::mutex> &lock, optional<runtime::time_point> when) {
    if (pending_io()) {
        lock.unlock();
        get_io()->wait(when);
        lock.lock();
    } else {
        if (when) {
            _cv.wait_until(lock, *when);
        } else {
            _cv.wait(lock);
        }
    }
}

void scheduler::schedule() {
    // TODO: enable if we add main task to alltasks
    //DCHECK(!_alltasks.empty());
    task_pimpl *self = _current_task;

    do {
        if (_canceled && !_tasks_canceled) {
            // TODO: maybe if canceled is set we don't let any more tasks spawn?
            _tasks_canceled = true;
            for (auto t : _alltasks) {
                t->cancel();
            }
            _task.cancel();
        }

        check_dirty_queue();
        update_cached_time();
        check_timeout_tasks();

        if (_readyq.empty()) {
            if (_alltasks.empty()) {
                std::lock_guard<std::mutex> lock(runtime_mutex);
                if (waiting_task && this == waiting_task->_scheduler) {
                    waiting_task->ready();
                    continue;
                }
            }
            std::unique_lock<std::mutex> lock{_mutex};
            check_dirty_queue();
            if (!_readyq.empty()) {
                continue;
            }
            auto when = _alarms.when();
            wait(lock, when);
        }
    } while (_readyq.empty());

    using ::operator <<;
    DVLOG(5) << "readyq: " << _readyq;

    task_pimpl *t = _readyq.front();
    _readyq.pop_front();
    t->_ready.store(false);
    _current_task = t;
    DVLOG(5) << self << " swap to " << t;
#ifdef TEN_TASK_TRACE
    self->_trace.capture();
#endif
    if (t != self) {
        self->_ctx.swap(t->_ctx, reinterpret_cast<intptr_t>(t));
    }
    _current_task = self;
    _gctasks.clear();
}

void scheduler::attach(shared_task &t) {
    DCHECK(t && t->_scheduler == nullptr);
    t->_scheduler = this;
    _alltasks.push_back(t);
    DVLOG(5) << "attach readyq " << t.get();
    _readyq.push_back(t.get());
}

///////////// runtime ///////////////

task_pimpl *runtime::current_task() {
    return this_ctx->scheduler._current_task;
}

void runtime::sleep_until(const time_point &sleep_time) {
    thread_context *ctx = this_ctx.get();
    task_pimpl *t = ctx->scheduler._current_task;
    scheduler::alarm_clock::scoped_alarm sleep_alarm(ctx->scheduler._alarms, t, sleep_time);
    task_pimpl::cancellation_point cancelable;
    t->swap();
}

runtime::time_point runtime::now() {
    return this_ctx->scheduler._now;
}

shared_task runtime::task_with_id(uint64_t id) {
    return this_ctx->scheduler.task_with_id(id);
}

int runtime::poll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    return this_ctx->scheduler.get_io()->poll(fds, nfds, ms);
}

bool runtime::fdwait(int fd, int rw, optional_timeout ms) {
    return this_ctx->scheduler.get_io()->fdwait(fd, rw, ms);
}

void runtime::shutdown() {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    for (thread_context *r : threads) {
        r->scheduler.cancel();
    }
}

void runtime::wait_for_all() {
    using namespace std;
    CHECK(is_main_thread());
    thread_context *ctx = this_ctx.get();
    {
        lock_guard<mutex> lock(runtime_mutex);
        waiting_task = ctx->scheduler._current_task;
        DVLOG(5) << "waiting for all " << waiting_task;
    }

    while (!ctx->scheduler._alltasks.empty() || thread_count > 1) {
        ctx->scheduler.schedule();
    }

    {
        lock_guard<mutex> lock(runtime_mutex);
        DVLOG(5) << "done waiting for all";
        waiting_task = nullptr;
    }
}


// XXX: only safe to call from debugger
int runtime::dump_all() {
    using namespace std;
    lock_guard<mutex> lock(runtime_mutex);
    for (thread_context *r : threads) {
        LOG(INFO) << "runtime: " << r;
        r->scheduler.dump();
    }
    return 0;
}

void runtime::attach(shared_task &t) {
    this_ctx->scheduler.attach(t);
}


///////////// deadline //////////////
// deadline is here because of this_ctx

struct deadline_pimpl {
    scheduler::alarm_clock::scoped_alarm alarm;
};

deadline::deadline(std::chrono::milliseconds ms)
    : _pimpl(std::make_shared<deadline_pimpl>())
{
    if (ms.count() < 0)
        throw errorx("negative deadline: %jdms", intmax_t(ms.count()));
    if (ms.count() > 0) {
        thread_context *r = this_ctx.get();
        task_pimpl *t = r->scheduler._current_task;
        _pimpl->alarm = std::move(scheduler::alarm_clock::scoped_alarm(
                    r->scheduler._alarms, t, ms+runtime::now(), deadline_reached()
                    )
                );
        DVLOG(1) << "deadline alarm armed: " << _pimpl->alarm._armed << " in " << ms.count() << "ms";
    }
}

void deadline::cancel() {
    _pimpl->alarm.cancel();
}

deadline::~deadline() {
    cancel();
}

std::chrono::milliseconds deadline::remaining() const {
    return _pimpl->alarm.remaining();
}

//////////////////// io ///////////////////

io::io() {
    _events.reserve(1000);
    // TODO: event_fd needs to be non blocking with edge-trigger?
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = _evfd.fd;
    _efd.add(_evfd.fd, ev);
}

io::~io() {
}

void io::wait(optional<runtime::time_point> when) {
    _events.resize(1000);

    int ms = -1;
    auto now = runtime::clock::now();
    if (when && *when > now) {
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(*when - now).count();
        struct itimerspec tspec{};
        struct itimerspec oldspec{};
        tspec.it_value.tv_sec = ms / 1000;
        tspec.it_value.tv_nsec = (ms % 1000) * 1000000;
        _tfd.settime(0, tspec, oldspec);
    } else {
        ms = 0;
    }

    _efd.wait(_events, ms);
    task_pimpl *t = nullptr;
    for (epoll_event &ev : _events) {
        DVLOG(5) << "epoll events " << ev.events << " on " << ev.data.fd;

        // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
        // even if they arent asked for, so we must wake up the tasks on any event
        // to avoid just spinning in epoll.
        int fd = ev.data.fd;
        if (!pollfds[fd].in.empty()) {
            pollfds[fd].in.front().pfd->revents = ev.events;
            // TODO: maybe pop_front here?
            t = pollfds[fd].in.front().task;
            DVLOG(5) << "IN EVENT on task: " << t;
            t->ready_for_io();
        }

        if (!pollfds[fd].out.empty()) {
            pollfds[fd].out.front().pfd->revents = ev.events;
            // TODO: maybe pop_front here?
            t = pollfds[fd].out.front().task;
            DVLOG(5) << "OUT EVENT on task: " << t;
            t->ready_for_io();
        }

        if (ev.data.fd == _evfd.fd) {
            // our wake up eventfd was written to
            // clear events by reading value
            _evfd.read();
        } else if (ev.data.fd == _tfd.fd) {
        } else if (pollfds[fd].in.empty() && pollfds[fd].out.empty()) {
            // TODO: otherwise we might want to remove fd from epoll
            LOG(ERROR) << "event " << ev.events << " for fd: "
                << ev.data.fd << " but has no task";
        }
    }
}

void io::add_pollfds(task_pimpl *t, pollfd *fds, nfds_t nfds) {
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
        uint32_t saved_events = pollfds[fd].events;

        if (fds[i].events & EPOLLIN) {
            pollfds[fd].in.emplace_back(t, &fds[i]);
            pollfds[fd].events |= EPOLLIN;
        }

        if (fds[i].events & EPOLLOUT) {
            pollfds[fd].out.emplace_back(t, &fds[i]);
            pollfds[fd].events |= EPOLLOUT;
        }

        ev.events = pollfds[fd].events | EPOLLONESHOT;

        DVLOG(5) << "fd: " << fd << " events: " << pollfds[fd].events;

        if (saved_events == 0) {
            THROW_ON_ERROR(_efd.add(fd, ev));
        } else if (saved_events != pollfds[fd].events) {
            THROW_ON_ERROR(_efd.modify(fd, ev));
        }
        ++npollfds;
    }
}

int io::remove_pollfds(pollfd *fds, nfds_t nfds) {
    using namespace std;
    int rvalue = 0;
    for (nfds_t i=0; i<nfds; ++i) {
        int fd = fds[i].fd;
        if (fds[i].revents) rvalue++;

        // IN
        auto it = find_if(begin(pollfds[fd].in), end(pollfds[fd].in),
                [&](task_poll_state &st) {
                    return st.pfd == &fds[i];
                    });
        if (it != end(pollfds[fd].in)) {
            pollfds[fd].in.erase(it);
            if (pollfds[fd].in.empty()) {
                pollfds[fd].events ^= EPOLLIN;
            }
        }
        

        // OUT
        it = find_if(begin(pollfds[fd].out), end(pollfds[fd].out),
                [&](task_poll_state &st) {
                    return st.pfd == &fds[i];
                    });
        if (it != end(pollfds[fd].out)) {
            pollfds[fd].out.erase(it);
            if (pollfds[fd].out.empty()) {
                pollfds[fd].events ^= EPOLLOUT;
            }
        }
        
        if (pollfds[fd].events == 0) {
            _efd.remove(fd);
        } else {
            epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.data.fd = fd;
            ev.events = pollfds[fd].events;
            THROW_ON_ERROR(_efd.modify(fd, ev));
        }
        --npollfds;
    }
    return rvalue;
}

bool io::fdwait(int fd, int rw, optional_timeout ms) {
    short events_ = 0;
    switch (rw) {
        case 'r':
            events_ |= EPOLLIN;
            break;
        case 'w':
            events_ |= EPOLLOUT;
            break;
    }
    pollfd fds = {fd, events_, 0};
    if (poll(&fds, 1, ms) > 0) {
        if ((fds.revents & EPOLLERR) || (fds.revents & EPOLLHUP)) {
            return false;
        }
        return true;
    }
    return false;
}

int io::poll(pollfd *fds, nfds_t nfds, optional_timeout ms) {
    thread_context *ctx = this_ctx.get();
    task_pimpl *t = ctx->scheduler._current_task;
    if (nfds == 1) {
        t->setstate("poll fd %i r: %i w: %i %ul ms",
                fds->fd, fds->events & EPOLLIN, fds->events & EPOLLOUT, (ms ? ms->count() : 0));
    } else {
        t->setstate("poll %u fds for %ul ms", nfds, (ms ? ms->count() : 0));
    }
    // TODO: support timeouts
    add_pollfds(t, fds, nfds);

    DVLOG(5) << "task: " << t << " poll for " << nfds << " fds";
    try {
        optional<scheduler::alarm_clock::scoped_alarm> timeout_alarm;
        if (ms) {
            runtime::time_point timeout_time{ctx->scheduler._now
                + *ms};
            timeout_alarm.emplace(ctx->scheduler._alarms, t, timeout_time);
        }
        task_pimpl::cancellation_point cancelable;
        t->swap();
    } catch (...) {
        remove_pollfds(fds, nfds);
        throw;
    }

    return remove_pollfds(fds, nfds);
}

} // ten

