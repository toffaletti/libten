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
        t->fn();
        t->exit();
    }
};

#if 0
deadline::deadline(uint64_t milliseconds) {
    task t = task::self();
    if (t.m->flags & _TASK_HAS_DEADLINE) {
        throw errorx("trying to set deadline on task that already has a deadline");
    }
    std::unique_lock<std::mutex> lk(t.m->mut);
    milliseconds_to_timespec(milliseconds, t.m->dl);
    t.m->dl += runner::self().get_scheduler().cached_now();
    t.m->flags |= _TASK_HAS_DEADLINE;
}

deadline::~deadline() {
    task t = task::self();
    t.m->flags ^= _TASK_HAS_DEADLINE;
}
#endif

struct proc {
    io_scheduler *sched;
    std::thread *thread;
    std::mutex mutex;
    uint64_t nswitch;
    task *ctask;
    tasklist runqueue;
    tasklist alltasks;
    std::condition_variable runcv;
    coroutine co;
    std::atomic<uint64_t> taskcount;

    proc()
        : sched(0), thread(0), nswitch(0), ctask(0), taskcount(0)
    {
        _this_proc = this;
        add(this);
    }

    proc(struct task *t)
        : sched(0), nswitch(0), ctask(0), taskcount(0)
    {

        add(this);
        std::unique_lock<std::mutex> lk(mutex);
        thread = new std::thread(proc::startproc, this, t);
        thread->detach();
    }

    ~proc();

    void schedule();

    bool is_ready() {
        return !runqueue.empty();
    }

    void wakeupandunlock(std::unique_lock<std::mutex> &lk);

    void addtaskinproc(struct task *t) {
        ++taskcount;
        alltasks.push_back(t);
        t->cproc = this;
    }

    void deltaskinproc(struct task *t) {
        if (!t->systask) {
            --taskcount;
        }
        auto i = std::find(alltasks.begin(), alltasks.end(), t);
        alltasks.erase(i);
        t->cproc = 0;
        delete t;
    }

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
    exiting(false), systask(false)
{
    id = ++taskidgen;
}

void task::swap() {
    co.swap(&_this_proc->co);
}

void task::ready() {
    proc *p = cproc;
    std::unique_lock<std::mutex> lk(p->mutex);
    p->runqueue.push_back(this);
    if (p != _this_proc) {
        p->wakeupandunlock(lk);
    }
}

void qutex::lock() {
    task *t = _this_proc->ctask;
    {
        std::unique_lock<std::mutex> lk(m);
        if (owner == 0) {
            owner = t;
            return;
        }
        waiting.push_back(t);
    }
    t->swap();
    assert(owner == _this_proc->ctask);
}

void qutex::unlock() {
    task *t = 0;
    {
        std::unique_lock<std::mutex> lk(m);
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
        DVLOG(5) << "RENDEZ PUSH BACK: " << t;
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
        t->ready();
    }
}

void rendez::wakeupall() {
    while (!waiting.empty()) {
        task *t = waiting.front();
        waiting.pop_front();
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
        if (runqueue.empty()) {
            runcv.wait(lk, std::bind(&proc::is_ready, this));
        }
        struct task *t = runqueue.front();
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
            return *a->timeout < *b->timeout;
        }
    };
    typedef std::vector<epoll_event> event_vector;
    typedef std::multiset<task *, task_timeout_compare> timeoutset;
    //! tasks with timeouts set
    timeoutset timeouts;
    //! epoll events
    event_vector events;
    //! current time cached in a few places through the event loop
    timespec now;
    //! the epoll fd used for io in this runner
    epoll_fd efd;
    //! number of fds we've been asked to wait on
    size_t npollfds;
    //! pipe used to wake up from epoll_wait
    pipe_fd pi;

    io_scheduler() : npollfds(0), pi(O_NONBLOCK) {
        events.reserve(1000);
        THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
        // add the pipe used to wake up
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLET;
        efd.add(pi.r.fd, ev);
        taskspawn(std::bind(&io_scheduler::fdtask, this));
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

    void sleep(uint64_t ms) {
        task *t = _this_proc->ctask;
        add_timeout(t, ms);
        t->swap();
    }

    bool fdwait(int fd, int rw, uint64_t ms) {
        task *t = _this_proc->ctask;
        epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.data.ptr = t;
        const char *msg = "error";
        switch (rw) {
            case 'r':
                ev.events |= EPOLLIN;
                msg = "read";
                break;
            case 'w':
                ev.events |= EPOLLOUT;
                msg = "write";
                break;
        }
        efd.add(fd, ev);
        ++npollfds;
        if (ms) {
            add_timeout(t, ms);
        }
        tasksetstate("fdwait for %i %s", fd, msg);
        t->swap();

        if (ms) {
            timeouts.erase(t);
            int s = efd.remove(fd);
            // if remove succeeds, then an event did not occur on the fd
            // which means the timeout was reached
            if (s == 0) return false;
        }
        return true;
    }

private:
    void fdtask() {
        tasksetname("fdtask");
        tasksystem();
        for (;;) {
            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            // let everyone else run
            taskyield();

            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));

            int ms = -1;
            if (!timeouts.empty()) {
                task *front = *timeouts.begin();
                if (*front->timeout <= now) {
                    // epoll_wait must return asap
                    ms = 0;
                } else {
                    ms = timespec_to_milliseconds(*front->timeout - now);
                    // avoid spinning on timeouts smaller than 1ms
                    if (ms <= 0) ms = 1;
                }
            }

            if (ms != 0) {
                proc *p = _this_proc;
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
                for (auto i=events.cbegin(); i!=events.cend(); ++i) {
                    task *t = (task *)i->data.ptr;
                    if (t) {
                        --npollfds;
                        t->ready();
                    } else {
                        // our wakeup pipe was written to
                        char buf[32];
                        // clear pipe
                        while (pi.read(buf, sizeof(buf)) > 0) {}
                    }
                }
            }

            THROW_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &now));
            // wake up sleeping tasks
            auto i = timeouts.begin();
            for (; i != timeouts.end(); ++i) {
                task *t = *i;
                if (*t->timeout <= now) {
                    t->ready();
                } else {
                    break;
                }
            }
            timeouts.erase(timeouts.begin(), i);
        }
    }
};

void proc::wakeupandunlock(std::unique_lock<std::mutex> &lk) {
    if (sched) {
        ssize_t nw = sched->pi.write("\1", 1);
        (void)nw;
    } else {
        runcv.notify_one();
    }
    lk.unlock();
}

proc::~proc() {
    std::unique_lock<std::mutex> lk(mutex);
    delete sched;
    if (thread == 0) {
        for (;;) {
            {
                // TODO: count non-system procs
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
    lk.unlock();
    del(this);
    DVLOG(5) << "proc freed: " << this;
}


void tasksleep(uint64_t ms) {
    proc *p = _this_proc;
    if (p->sched == 0) {
        p->sched = new io_scheduler();
    }
    p->sched->sleep(ms);
}

bool fdwait(int fd, int rw, uint64_t ms) {
    proc *p = _this_proc;
    if (p->sched == 0) {
        p->sched = new io_scheduler();
    }
    return p->sched->fdwait(fd, rw, ms);
}

} // end namespace fw
