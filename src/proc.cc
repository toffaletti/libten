#include "proc.hh"
#include "io.hh"

#include <cxxabi.h>
#include <execinfo.h>
#include <iostream>
#include <condition_variable>
#include <sys/syscall.h>

namespace ten {

namespace hack {
// hack to avoid redefinition of ucontext
// by including it in a namespace
#include <asm-generic/ucontext.h>
}

__thread proc *_this_proc = nullptr;
static std::mutex procsmutex;
static std::deque<ptr<proc>> procs;
static std::once_flag init_flag;

static void set_this_proc(ptr<proc> p) {
    _this_proc = p.get();
}

ptr<proc> this_proc() {
    return ptr<proc>{_this_proc};
}

void proc_waker::wait() {
    ptr<proc> p = this_proc();
    std::unique_lock<std::mutex> lk{mutex};
    while (!p->is_ready() && !p->is_canceled() && !p->is_dirty()) {
        asleep = true;
        cond.wait(lk);
    }
    asleep = false;
}

void proc::thread_entry(std::shared_ptr<task::pimpl> t) {
    procmain scope{t};
    t->ready();
    scope.main();
}

void proc::add(ptr<proc> p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    procs.push_back(p);
}

void proc::del(ptr<proc> p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    auto i = std::find(procs.begin(), procs.end(), p);
    procs.erase(i);
}

io_scheduler &proc::sched() {
    if (_sched == nullptr) {
        _sched.reset(new io_scheduler());
    }
    return *_sched;
}

void proc::schedule() {
    try {
        DVLOG(5) << "p: " << this << " entering proc::schedule";
        while (taskcount > 0) {
            // check dirty queue
            {
                ptr<task::pimpl> t = nullptr;
                while (dirtyq.pop(t)) {
                    DVLOG(5) << "dirty readying " << t;
                    runqueue.push_front(t);
                }
            }
            if (runqueue.empty()) {
                _waker->wait();
                // need to go through dirty loop again because
                // runqueue could still be empty
                if (!dirtyq.empty()) continue;
            }
            if (canceled) {
                // set canceled to false so we don't
                // execute this every time through the loop
                // while the tasks are cleaning up
                canceled = false;
                procshutdown();
                if (runqueue.empty()) continue;
            }
            ptr<task::pimpl> t{runqueue.front()};
            runqueue.pop_front();
            ctask = t;
            DVLOG(5) << "p: " << this << " swapping to: " << t;
            t->is_ready = false;
            ctx.swap(t->ctx, reinterpret_cast<intptr_t>(t.get()));
            ctask = nullptr;
            
            if (!t->fn) {
                deltaskinproc(t);
            }
        }
    } catch (backtrace_exception &e) {
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what() << "\n" << e.backtrace_str();
        std::exit(2);
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what();
        std::exit(2);
    }
}

proc::proc(bool main_)
  : ctask(nullptr),
    _sched(nullptr), 
    canceled(false),
    taskcount(0),
    _main(main_)
{
    _waker = std::make_shared<proc_waker>();
    update_cached_time();
}

void proc::shutdown() {
    for (auto &t : alltasks) {
        if (t.get() == ctask.get()) continue; // don't add ourself to the runqueue
        if (!t->systask) {
            t->cancel();
        }
    }
}

void proc::ready(ptr<task::pimpl> t, bool front) {
    DVLOG(5) << "readying: " << t;
    if (this != this_proc().get()) {
        dirtyq.push(t);
        wakeup();
    } else {
        if (front) {
            runqueue.push_front(t);
        } else {
            runqueue.push_back(t);
        }
    }
}

bool proc::cancel_task_by_id(uint64_t id) {
    bool found = false;
    for (auto &t : alltasks) {
        if (t->id == id) {
            found = true;
            t->cancel();
            break;
        }
    }
    return found;
}

void proc::mark_system_task() {
    if (!ctask->systask) {
        ctask->systask = true;
        --taskcount;
    }
}

void proc::wakeup() {
    _waker->wake();
}

void proc::addtaskinproc(std::shared_ptr<task::pimpl> t) {
    ++taskcount;
    t->cproc.reset(this);
    alltasks.push_back(std::move(t));
}

void proc::deltaskinproc(ptr<task::pimpl> t) {
    if (!t->systask) {
        --taskcount;
    }
    DCHECK(std::find(runqueue.begin(), runqueue.end(), t) == runqueue.end()) << "BUG: " << t
        << " found in runqueue while being deleted";
    auto i = std::find_if(alltasks.begin(), alltasks.end(), [=](std::shared_ptr<task::pimpl> &tt) -> bool {
        return tt.get() == t.get();
    });
    DCHECK(i != alltasks.end());
    alltasks.erase(i);
}

proc::~proc() {
    {
        if (_main) {
            // if the main proc is exiting we need to cancel
            // all other procs (threads) and wait for them
            size_t nprocs = procs.size();
            {
                std::lock_guard<std::mutex> plk{procsmutex};
                for (auto i=procs.begin(); i!= procs.end(); ++i) {
                    if (i->get() == this) continue;
                    (*i)->cancel();
                }
            }
            for (;;) {
                // TODO: remove this busy loop in favor of sleeping the proc
                {
                    std::lock_guard<std::mutex> plk{procsmutex};
                    size_t np = procs.size();
                    if (np == 0)
                        break;
                }
                std::this_thread::yield();
            }

            DVLOG(5) << "sleeping last proc to allow " << nprocs << " threads to exit and cleanup";
            for (size_t i=0; i<nprocs*2; ++i) {
                // nasty hack for mysql thread cleanup
                // because it happens *after* all of my code, i have no way of waiting
                // for it to finish with an event (unless i joined all threads)
                usleep(100);
                std::this_thread::yield();
            }
        }
        // TODO: now that threads are remaining joinable
        // maybe shutdown can be cleaner...look into this
        // example: maybe if canceled we don't detatch
        // and we can join in the above loop instead of sleep loop
        //if (thread.joinable()) {
        //    thread.detach();
        //}
        // XXX: there is also a thing that will trigger a cond var
        // after thread has fully exited. that is another possiblity
        runqueue.clear();
        // clean up system tasks
        while (!alltasks.empty()) {
            deltaskinproc(ptr<task::pimpl>{alltasks.front().get()});
        }
        // must delete _sched *after* tasks because
        // they might try to remove themselves from timeouts set
        delete _sched.get();
    }
    // unlock before del()
    DVLOG(5) << "proc freed: " << this;
}

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    std::shared_ptr<task::pimpl> t = std::make_shared<task::pimpl>(f, stacksize);
    uint64_t tid = t->id;
    std::thread procthread{proc::thread_entry, std::move(t)};
    procthread.detach();
    // XXX: task could be freed at this point
    return tid;
}

void procshutdown() {
    ptr<proc> p = this_proc();
    p->shutdown();
}

static void info_handler(int sig_num, siginfo_t *info, void *ctxt) {
    taskdumpf();
}

namespace {
    static bool glog_inited;
    struct stoplog_t {
        ~stoplog_t() { if (glog_inited) ShutdownGoogleLogging(); }
    } stoplog;
}

static void procmain_init() {
    CHECK(getpid() == syscall(SYS_gettid)) << "must call procmain in main thread before anything else";
    //ncpu_ = sysconf(_SC_NPROCESSORS_ONLN);
    stack_t ss;
    ss.ss_sp = calloc(1, SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    throw_if(sigaltstack(&ss, NULL) == -1);


    umask(02); // allow group-readable logs
    InitGoogleLogging(program_invocation_short_name);
    glog_inited = true;
    InstallFailureSignalHandler();

    struct sigaction act;

    // ignore SIGPIPE
    memset(&act, 0, sizeof(act));
    throw_if(sigaction(SIGPIPE, NULL, &act) == -1);
    if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        throw_if(sigaction(SIGPIPE, &act, NULL) == -1);
    }

    // install INFO handler
    throw_if(sigaction(SIGUSR1, NULL, &act) == -1);
    if (act.sa_handler == SIG_DFL) {
        act.sa_sigaction = info_handler;
        act.sa_flags = SA_RESTART | SA_SIGINFO;
        throw_if(sigaction(SIGUSR1, &act, NULL) == -1);
    }
    netinit();
}

procmain::procmain(std::shared_ptr<task::pimpl> t) {
    // only way i know of detecting the main thread
    task::set_default_stacksize(default_stacksize);
    bool is_main_thread = getpid() == syscall(SYS_gettid);
    std::call_once(init_flag, procmain_init);
    p.reset(new proc(is_main_thread));
    set_this_proc(p);
    proc::add(p);
    if (t) {
        p->addtaskinproc(t);
    }
}

procmain::~procmain() {
    proc::del(p);
    set_this_proc(nullptr);
    delete p.get();
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "proc: " << p.get() << " thread id: " << std::this_thread::get_id();
    p->schedule();
    DVLOG(5) << "proc done: " << std::this_thread::get_id() << " " << p.get();
    return EXIT_SUCCESS;
}

const time_point<steady_clock> &procnow() {
    return this_proc()->cached_time();
}

} // end namespace ten
