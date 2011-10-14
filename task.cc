#include <thread>
#include <condition_variable>
#include <cassert>
#include <algorithm>
#include <stdatomic.h>
#include "timespec.hh"
#include "coroutine.hh"
#include "logging.hh"
#include "task.hh"
#include "descriptors.hh"

#include <cxxabi.h>
#include <execinfo.h>
#include <iostream>
namespace hack {
// hack to avoid redefinition of ucontext
// by including it in a namespace
#include <asm-generic/ucontext.h>
}

namespace fw {

struct io_scheduler;

static std::atomic<uint64_t> taskidgen(0);
static __thread proc *_this_proc = 0;
static std::mutex procsmutex;
static proclist procs;
static std::once_flag init_flag;

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

struct task {
    char name[256];
    char state[256];
    std::function<void ()> fn;
    coroutine co;
    uint64_t id;
    proc *cproc;
    timespec *timeout;
    timespec *deadline;
    bool exiting;
    bool systask;
    bool canceled;
    
    task(const std::function<void ()> &f, size_t stacksize);
    ~task() {
        delete timeout;
        delete deadline;
    }

    void ready();
    void swap();

    void exit() {
        exiting = true;
        swap();
    }

    static void start(void *arg) {
        task *t = (task *)arg;
        try {
            t->fn();
        } catch (task_interrupted &e) {
            DVLOG(5) << "task " << t << " interrupted";
        }
        t->exit();
    }
};

struct proc {
    io_scheduler *_sched;
    std::thread *thread;
    std::mutex mutex;
    uint64_t nswitch;
    task *ctask;
    tasklist runqueue;
    tasklist alltasks;
    coroutine co;
    //! pipe used to wake up
    pipe_fd pi;
    std::atomic<uint64_t> taskcount;

    proc()
        : _sched(0), thread(0), nswitch(0), ctask(0), taskcount(0)
    {
        _this_proc = this;
        add(this);
    }

    proc(task *t)
        : _sched(0), nswitch(0), ctask(0), taskcount(0)
    {

        add(this);
        std::unique_lock<std::mutex> lk(mutex);
        thread = new std::thread(proc::startproc, this, t);
        thread->detach();
    }

    ~proc();

    void schedule();

    io_scheduler &sched();

    bool is_ready() {
        return !runqueue.empty();
    }

    void wakeupandunlock(std::unique_lock<std::mutex> &lk);

    void addtaskinproc(task *t) {
        ++taskcount;
        alltasks.push_back(t);
        t->cproc = this;
    }

    void deltaskinproc(task *t);

    static void add(proc *p) {
        std::unique_lock<std::mutex> lk(procsmutex);
        procs.push_back(p);
    }

    static void del(proc *p) {
        std::unique_lock<std::mutex> lk(procsmutex);
        auto i = std::find(procs.begin(), procs.end(), p);
        procs.erase(i);
    }

    static void startproc(proc *p_, task *t) {
        //std::unique_lock<std::mutex> lk(p_->mutex);
        _this_proc = p_;
        std::unique_ptr<proc> p(p_);
        p->addtaskinproc(t);
        t->ready();
        DVLOG(5) << "proc: " << p_ << " thread id: " << std::this_thread::get_id();
        p->schedule();
        DVLOG(5) << "proc done: " << std::this_thread::get_id();
        _this_proc = 0;
    }

};

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = new task(f, stacksize);
    new proc(t);
    return t->id;
}

uint64_t taskspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = new task(f, stacksize);
    _this_proc->addtaskinproc(t);
    t->ready();
    return t->id;
}

int64_t taskyield() {
    proc *p = _this_proc;
    uint64_t n = p->nswitch;
    task *t = p->ctask;
    t->ready();
    tasksetstate("yield");
    t->swap();
    return p->nswitch - n - 1;
}

void tasksystem() {
    proc *p = _this_proc;
    if (!p->ctask->systask) {
        p->ctask->systask = true;
        --p->taskcount;
    }
}

bool taskcancel(uint64_t id) {
    proc *p = _this_proc;
    task *t = 0;
    for (auto i = p->alltasks.cbegin(); i != p->alltasks.cend(); ++i) {
        if ((*i)->id == id) {
            t = *i;
            break;
        }
    }

    if (t) {
        // TODO: set state canceled
        t->canceled = true;
        t->ready();
    }
    return (bool)t;
}

void tasksetname(const char *fmt, ...)
{
	va_list arg;
	task *t = _this_proc->ctask;
	va_start(arg, fmt);
	vsnprintf(t->name, sizeof(t->name), fmt, arg);
	va_end(arg);
}

const char *taskgetname() {
    return _this_proc->ctask->name;
}

void tasksetstate(const char *fmt, ...)
{
	va_list arg;
	task *t = _this_proc->ctask;
	va_start(arg, fmt);
	vsnprintf(t->state, sizeof(t->state), fmt, arg);
	va_end(arg);
}

const char *taskgetstate() {
    return _this_proc->ctask->state;
}


task::task(const std::function<void ()> &f, size_t stacksize)
    : fn(f), co(task::start, this, stacksize), cproc(0), 
    timeout(0), deadline(0),
    exiting(false), systask(false), canceled(false)
{
    id = ++taskidgen;
}

void task::ready() {
    proc *p = cproc;
    assert(!exiting);
    std::unique_lock<std::mutex> lk(p->mutex);
    if (std::find(p->runqueue.cbegin(), p->runqueue.cend(), this) == p->runqueue.cend()) {
        DVLOG(5) << _this_proc->ctask << " adding task: " << this << " to runqueue for proc: " << p;
        p->runqueue.push_back(this);
        // XXX: does this need to be outside of the if(!found) ?
        if (p != _this_proc) {
            p->wakeupandunlock(lk);
        }
    } else {
        DVLOG(5) << "found task: " << this << " already in runqueue for proc: " << p;
    }
}

void qutex::lock() {
    task *t = _this_proc->ctask;
    {
        std::unique_lock<std::timed_mutex> lk(m);
        if (owner == 0 || owner == t) {
            owner = t;
            DVLOG(5) << "LOCK qutex: " << this << " owner: " << owner;
            return;
        }
        DVLOG(5) << "LOCK waiting: " << this << " add: " << t <<  " owner: " << owner;
        waiting.push_back(t);
    }
    t->swap();
    CHECK(owner == _this_proc->ctask) << "Qutex: " << this << " owner check failed: " <<
        owner << " != " << _this_proc->ctask << " t:" << t <<
        " owner->cproc: " << owner->cproc << " this_proc: " << _this_proc;
}

bool qutex::try_lock() {
    task *t = _this_proc->ctask;
    std::unique_lock<std::timed_mutex> lk(m, std::try_to_lock);
    if (lk.owns_lock()) {
        if (owner == 0) {
            owner = t;
            return true;
        }
    }
    return false;
}

template<typename Rep,typename Period>
bool qutex::try_lock_for(
        std::chrono::duration<Rep,Period> const&
        relative_time)
{
    //_this_proc->sched().add_timeout(ms);
    return false;
}

template<typename Clock,typename Duration>
bool qutex::try_lock_until(
        std::chrono::time_point<Clock,Duration> const&
        absolute_time)
{
    return false;
}


void qutex::unlock() {
    task *t = 0;
    {
        std::unique_lock<std::timed_mutex> lk(m);
        if (!waiting.empty()) {
            t = owner = waiting.front();
            waiting.pop_front();
        } else {
            owner = 0;
        }
        DVLOG(5) << "UNLOCK qutex: " << this << " owner: " << owner << " waiting: " << waiting.size();
    }
    if (t) t->ready();
}

void rendez::sleep(std::unique_lock<qutex> &lk) {
    task *t = _this_proc->ctask;
    if (std::find(waiting.begin(), waiting.end(), t) == waiting.end()) {
        DVLOG(5) << "RENDEZ " << this << " PUSH BACK: " << t;
        waiting.push_back(t);
    }
    lk.unlock();
    t->swap(); 
    lk.lock();
}

void rendez::wakeup() {
    if (!waiting.empty()) {
        task *t = waiting.front();
        waiting.pop_front();
        DVLOG(5) << "RENDEZ " << this << " wakeup: " << t;
        t->ready();
    }
}

void rendez::wakeupall() {
    while (!waiting.empty()) {
        task *t = waiting.front();
        waiting.pop_front();
        DVLOG(5) << "RENDEZ " << this << " wakeupall: " << t;
        t->ready();
    }
}

static void backtrace_handler(int sig_num, siginfo_t *info, void *ctxt) {
    // http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
    // TODO: maybe use the google logging demangler that doesn't alloc
    hack::ucontext *uc = (hack::ucontext *)ctxt;

    // Get the address at the time the signal was raised from the instruction pointer
#if __i386__
    void *caller_address = (void *) uc->uc_mcontext.eip;
#elif __amd64__
    void *caller_address = (void *) uc->uc_mcontext.rip;
#else
    #error "arch not supported"
#endif

    fprintf(stderr, "signal %d (%s), address is %p from %p\n",
        sig_num, strsignal(sig_num), info->si_addr,
        (void *)caller_address);

    void *array[50];
    int size = backtrace(array, 50);

    // overwrite sigaction with caller's address
    array[1] = caller_address;

    char **messages = backtrace_symbols(array, size);

    // skip first stack frame (points here)
    for (int i = 1; i < size && messages != NULL; ++i) {
        char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

        // find parantheses and +address offset surrounding mangled name
        for (char *p = messages[i]; *p; ++p) {
            if (*p == '(') {
                mangled_name = p;
            } else if (*p == '+') {
                offset_begin = p;
            } else if (*p == ')') {
                offset_end = p;
                break;
            }
        }

        // if the line could be processed, attempt to demangle the symbol
        if (mangled_name && offset_begin && offset_end &&
            mangled_name < offset_begin)
        {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            char * real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

            if (status == 0) {
                // if demangling is successful, output the demangled function name
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
                    << real_name << "+" << offset_begin << offset_end
                    << std::endl;

            } else {
                // otherwise, output the mangled function name
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : "
                    << mangled_name << "+" << offset_begin << offset_end
                    << std::endl;
            }
            free(real_name);
        } else {
            // otherwise, print the whole line
            std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
        }
    }
    std::cerr << std::endl;

    free(messages);

    exit(EXIT_FAILURE);
}

static void procmain_init() {
    //ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    THROW_ON_ERROR(sigaltstack(&ss, NULL));

    // allow log files and message queues to be created group writable
    umask(0);
    google::InitGoogleLogging(program_invocation_short_name);
    google::InstallFailureSignalHandler();
    FLAGS_logtostderr = true;

    struct sigaction act;
#if 0
    // install SIGSEGV handler
    act.sa_sigaction = backtrace_handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    THROW_ON_ERROR(sigaction(SIGSEGV, &act, NULL));
    // install SIGABRT handler
    act.sa_sigaction = backtrace_handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    THROW_ON_ERROR(sigaction(SIGABRT, &act, NULL));
#endif
    // ignore SIGPIPE
    memset(&act, 0, sizeof(act));
    THROW_ON_ERROR(sigaction(SIGPIPE, NULL, &act));
    if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        THROW_ON_ERROR(sigaction(SIGPIPE, &act, NULL));
    }

    new proc();
}

procmain::procmain() {
    std::call_once(init_flag, procmain_init);
}

int procmain::main(int argc, char *argv[]) {
    _this_proc->schedule();
    return EXIT_SUCCESS;
}

void proc::schedule() {
    DVLOG(5) << "p: " << this << " entering proc::schedule";
    for (;;) {
        if (taskcount == 0) break;
        std::unique_lock<std::mutex> lk(mutex);
        while (runqueue.empty()) {
            char buf[1];
            lk.unlock();
            ssize_t nr = pi.read(buf, 1);
            (void)nr;
            lk.lock();
        }
        task *t = runqueue.front();
        runqueue.pop_front();
        ctask = t;
        ++nswitch;
        DVLOG(5) << "p: " << this << " swapping to: " << t;
        lk.unlock();
        co.swap(&t->co);
        lk.lock();
        ctask = 0;
        if (t->exiting) {
            deltaskinproc(t);
        }
    }
}

struct io_scheduler {
    struct task_timeout_compare {
        bool operator ()(const task *a, const task *b) const {
            return *a->timeout < *b->timeout || (*a->timeout == *b->timeout && a < b);
        }
    };
    struct task_poll_state {
        task *t_in; // POLLIN task
        pollfd *p_in; // pointer to pollfd structure that is on the task's stack
        task *t_out; // POLLOUT task
        pollfd *p_out; // pointer to pollfd structure that is on the task's stack
        uint32_t events; // events this fd is registered for
        task_poll_state() : t_in(0), p_in(0), t_out(0), p_out(0), events(0) {}
    };
    typedef std::vector<task_poll_state> poll_task_array;
    typedef std::vector<epoll_event> event_vector;
    typedef std::multiset<task *, task_timeout_compare> timeoutset;
    //! tasks with timeouts set
    timeoutset timeouts;
    //! array of tasks waiting on fds, indexed by the fd for speedy lookup
    poll_task_array pollfds;
    //! epoll events
    event_vector events;
    //! current time cached in a few places through the event loop
    timespec now;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! number of fds we've been asked to wait on
    size_t npollfds;

    io_scheduler() : npollfds(0) {
        events.reserve(1000);
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        // add the pipe used to wake up
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLET;
        int pi_fd = _this_proc->pi.r.fd;
        ev.data.fd = pi_fd;
        if (pollfds.size() <= (size_t)pi_fd) {
            pollfds.resize(pi_fd+1);
        }
        efd.add(pi_fd, ev);
        taskspawn(std::bind(&io_scheduler::fdtask, this));
    }

    void add_pollfds(task *t, pollfd *fds, nfds_t nfds) {
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
                CHECK(pollfds[fd].t_in == 0);
                pollfds[fd].t_in = t;
                pollfds[fd].p_in = &fds[i];
                pollfds[fd].events |= EPOLLIN;
            }

            if (fds[i].events & EPOLLOUT) {
                CHECK(pollfds[fd].t_out == 0);
                pollfds[fd].t_out = t;
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
                efd.remove(fd);
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

    void add_timeout(task *t, uint64_t ms) {
        if (t->timeout == 0) {
            t->timeout = new timespec();
        }
        milliseconds_to_timespec(ms, *t->timeout);
        (*t->timeout) += now;
        if (t->deadline) {
            if (*t->timeout > *t->deadline) {
                // don't sleep past the deadline
                *t->timeout = *t->deadline;
            }
        }
        timeouts.insert(t);
    }

    bool del_timeout(task *t) {
        if (t->timeout == 0) return false;
        return timeouts.erase(t);
    }

    void sleep(uint64_t ms) {
        task *t = _this_proc->ctask;
        add_timeout(t, ms);
        t->swap();
    }

    bool fdwait(int fd, int rw, uint64_t ms) {
        short events = 0;
        switch (rw) {
            case 'r':
                events |= EPOLLIN;
                break;
            case 'w':
                events |= EPOLLOUT;
                break;
        }
        pollfd fds = {fd, events, 0};
        return poll(&fds, 1, ms) > 0;
    }

    int poll(pollfd *fds, nfds_t nfds, uint64_t ms) {
        task *t = _this_proc->ctask;
        if (nfds == 1) {
            tasksetstate("poll fd %i r: %i w: %i %ul ms",
                    fds->fd, fds->events & EPOLLIN, fds->events & EPOLLOUT, ms);
        } else {
            tasksetstate("poll %u fds for %ul ms", nfds, ms);
        }
        if (ms) {
            add_timeout(t, ms);
        }
        add_pollfds(t, fds, nfds);

        DVLOG(5) << "task: " << t << " poll for " << nfds << " fds";
        try {
            t->swap();
        } catch (task_interrupted &e) {
            if (ms) timeouts.erase(t);
            remove_pollfds(fds, nfds);
            throw;
        }

        if (ms) timeouts.erase(t);
        return remove_pollfds(fds, nfds);
    }

    void fdtask() {
        tasksetname("fdtask");
        tasksystem();
        for (;;) {
            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            // let everyone else run
            taskyield();
            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            proc *p = _this_proc;
            task *t = 0;

            int ms = -1;
            if (!timeouts.empty()) {
                t = *timeouts.begin();
                if (*t->timeout <= now) {
                    // epoll_wait must return asap
                    ms = 0;
                } else {
                    ms = timespec_to_milliseconds(*t->timeout - now);
                    // avoid spinning on timeouts smaller than 1ms
                    if (ms <= 0) ms = 1;
                }
            }

            if (ms != 0) {
                std::unique_lock<std::mutex> lk(p->mutex);
                if (!p->runqueue.empty()) {
                    // don't block on epoll if tasks are ready to run
                    ms = 0;
                }
            }

            if (ms != 0 || npollfds > 0) {
                tasksetstate("epoll");
                // only process 1000 events each iteration to keep it fair
                events.resize(1000);
                efd.wait(events, ms);

                int wakeup_pipe_fd = p->pi.r.fd;
                // wake up io tasks
                for (auto i=events.cbegin(); i!=events.cend(); ++i) {
                    // NOTE: epoll will also return EPOLLERR and EPOLLHUP for every fd
                    // even if they arent asked for, so we must wake up the tasks on any event
                    // to avoid just spinning in epoll.
                    int fd = i->data.fd;
                    if (pollfds[fd].t_in) {
                        pollfds[fd].p_in->revents = i->events;
                        t = pollfds[fd].t_in;
                        DVLOG(5) << "IN EVENT on task: " << t << " state: " << t->state;
                        t->ready();
                    }

                    // check to see if pollout is a different task than pollin
                    if (pollfds[fd].t_out && pollfds[fd].t_out != pollfds[fd].t_in) {
                        pollfds[fd].p_out->revents = i->events;
                        t = pollfds[fd].t_out;
                        DVLOG(5) << "OUT EVENT on task: " << t << " state: " << t->state;
                        t->ready();
                    }

                    if (i->data.fd == wakeup_pipe_fd) {
                        // our wake up pipe was written to
                        char buf[32];
                        // clear pipe
                        while (p->pi.read(buf, sizeof(buf)) > 0) {}
                    } else if (pollfds[fd].t_in == 0 && pollfds[fd].t_out == 0) {
                        // TODO: otherwise we might want to remove fd from epoll
                        LOG(ERROR) << "event " << i->events << " for fd: " << i->data.fd << " but has no task";
                    }
                }
            }

            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            // wake up sleeping tasks
            auto i = timeouts.begin();
            for (; i != timeouts.end(); ++i) {
                t = *i;
                if (*t->timeout <= now) {
                    DVLOG(5) << "TIMEOUT on task: " << t;
                    t->ready();
                } else {
                    break;
                }
            }
            timeouts.erase(timeouts.begin(), i);
        }
    }
};

void task::swap() {
    // swap to scheduler coroutine
    co.swap(&_this_proc->co);

    if (canceled) {
        throw task_interrupted();
    }

    if (deadline && _this_proc->sched().now >= *deadline) {
        throw deadline_reached();
    }
}

void proc::wakeupandunlock(std::unique_lock<std::mutex> &lk) {
    // TODO: maybe an if (asleep)
    ssize_t nw = pi.write("\1", 1);
    (void)nw;
    lk.unlock();
}

proc::~proc() {
    std::unique_lock<std::mutex> lk(mutex);
    if (thread == 0) {
        for (;;) {
            // TODO: remove this busy loop in favor of sleeping the proc
            {
                std::unique_lock<std::mutex> lk(procsmutex);
                size_t np = procs.size();
                if (np == 1)
                    break;
            }
            std::this_thread::yield();
        }
        DVLOG(5) << "procs empty";
    } else {
        delete thread;
    }
    // clean up system tasks
    while (!alltasks.empty()) {
        deltaskinproc(alltasks.front());
    }
    // must delete _sched *after* tasks because
    // they might try to remove themselves from timeouts set
    delete _sched;
    lk.unlock();
    del(this);
    DVLOG(5) << "proc freed: " << this;
}

io_scheduler &proc::sched() {
    if (_sched == 0) {
        pi.setnonblock();
        _sched = new io_scheduler();
    }
    return *_sched;
}

void tasksleep(uint64_t ms) {
    _this_proc->sched().sleep(ms);
}

bool fdwait(int fd, int rw, uint64_t ms) {
    return _this_proc->sched().fdwait(fd, rw, ms);
}

int taskpoll(pollfd *fds, nfds_t nfds, uint64_t ms) {
    return _this_proc->sched().poll(fds, nfds, ms);
}

bool rendez::sleep_for(std::unique_lock<qutex> &lk, unsigned int ms) {
    task *t = _this_proc->ctask;
    if (std::find(waiting.begin(), waiting.end(), t) == waiting.end()) {
        DVLOG(5) << "RENDEZ PUSH BACK: " << t;
        waiting.push_back(t);
    }
    lk.unlock();
    _this_proc->sched().add_timeout(t, ms);
    t->swap();
    lk.lock();
    _this_proc->sched().del_timeout(t);
    // if we're not in the waiting list then we were signaled to wakeup
    return std::find(waiting.begin(), waiting.end(), t) == waiting.end();
}

void proc::deltaskinproc(task *t) {
    if (!t->systask) {
        --taskcount;
    }
    auto i = std::find(alltasks.begin(), alltasks.end(), t);
    alltasks.erase(i);
    t->cproc = 0;
    if (_sched) {
        _sched->del_timeout(t);
    }
    DVLOG(5) << "FREEING task: " << t;
    delete t;
}

deadline::deadline(uint64_t milliseconds) {
    task *t = _this_proc->ctask;
    if (t->deadline) {
        throw errorx("task %p already has a deadline", t);
    }
    t->deadline = new timespec();
    milliseconds_to_timespec(milliseconds, *t->deadline);
    *t->deadline += _this_proc->sched().now;
}

deadline::~deadline() {
    task *t = _this_proc->ctask;
    CHECK(t->deadline);
    delete t->deadline;
    t->deadline = 0;
}

} // end namespace fw
