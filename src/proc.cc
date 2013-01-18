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
static proclist procs;
static std::once_flag init_flag;

static void set_this_proc(proc *p) {
    _this_proc = p;
}

proc *this_proc() {
    return _this_proc;
}

task *this_task() {
    return _this_proc->ctask;
}

void proc_waker::wait() {
    proc *p = this_proc();
    std::unique_lock<std::mutex> lk{mutex};
    while (!p->is_ready() && !p->is_canceled() && !p->is_dirty()) {
        asleep = true;
        cond.wait(lk);
    }
    asleep = false;
}

void proc::thread_entry(task *t) {
    procmain scope{t};
    t->ready();
    scope.main();
}

void proc::add(proc *p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    procs.push_back(p);
}

void proc::del(proc *p) {
    std::lock_guard<std::mutex> lk{procsmutex};
    auto i = std::find(procs.begin(), procs.end(), p);
    procs.erase(i);
}

void proc::deltaskinproc(task *t) {
    if (!t->systask) {
        --taskcount;
    }
    DCHECK(std::find(runqueue.begin(), runqueue.end(), t) == runqueue.end()) << "BUG: " << t
        << " found in runqueue while being deleted";
    auto i = std::find(alltasks.begin(), alltasks.end(), t);
    DCHECK(i != alltasks.end());
    alltasks.erase(i);
    DVLOG(5) << "POOLING task: " << t;
    t->clear();
    taskpool.push_back(t);
}

void proc::wakeup() {
    _waker->wake();
}

io_scheduler &proc::sched() {
    if (_sched == nullptr) {
        _sched = new io_scheduler();
    }
    return *_sched;
}

void proc::schedule() {
    try {
        DVLOG(5) << "p: " << this << " entering proc::schedule";
        for (;;) {
            if (taskcount == 0) break;
            // check dirty queue
            {
                task *t = nullptr;
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
            task *t = runqueue.front();
            runqueue.pop_front();
            ctask = t;
            if (!t->systask) {
                // dont increment for system tasks so
                // while(taskyield()) {} can be used to
                // wait for all other tasks to exit
                // really only useful for unit tests.
                ++nswitch;
            }
            DVLOG(5) << "p: " << this << " swapping to: " << t;
            t->_ready = false;
            co.swap(&t->co);
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
  : _sched(nullptr), nswitch(0), ctask(nullptr),
    canceled(false), taskcount(0), _main(main_)
{
    _waker = std::make_shared<proc_waker>();
    update_cached_time();
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
                    if (*i == this) continue;
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
            deltaskinproc(alltasks.front());
        }
        // free tasks in taskpool
        for (auto i=taskpool.begin(); i!=taskpool.end(); ++i) {
            delete (*i);
        }
        // must delete _sched *after* tasks because
        // they might try to remove themselves from timeouts set
        delete _sched;
    }
    // unlock before del()
    DVLOG(5) << "proc freed: " << this;
}

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = new task(f, stacksize);
    uint64_t tid = t->id;
    auto ctx = std::make_shared<proc_context>();
    ctx->t = t;
    ctx->thread = std::move(std::thread(proc::thread_entry, t));
    ctx->thread.detach();
    // XXX: task could be freed at this point
    return tid;
}

void procshutdown() {
    proc *p = this_proc();
    p->shutdown();
}

static void info_handler(int sig_num, siginfo_t *info, void *ctxt) {
    taskdumpf();
}

static void procmain_init() {
    CHECK(getpid() == syscall(SYS_gettid)) << "must call procmain in main thread before anything else";
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

    // install INFO handler
    THROW_ON_ERROR(sigaction(SIGUSR1, NULL, &act));
    if (act.sa_handler == SIG_DFL) {
        act.sa_sigaction = info_handler;
        act.sa_flags = SA_RESTART | SA_SIGINFO;
        THROW_ON_ERROR(sigaction(SIGUSR1, &act, NULL));
    }

    netinit();
}

procmain::procmain(task *t) {
    // only way i know of detecting the main thread
    bool is_main_thread = getpid() == syscall(SYS_gettid);
    std::call_once(init_flag, procmain_init);
    p = new proc(is_main_thread);
    set_this_proc(p);
    proc::add(p);
    if (t) {
        p->addtaskinproc(t);
    }
}

procmain::~procmain() {
    proc::del(p);
    set_this_proc(nullptr);
    delete p;
}

int procmain::main(int argc, char *argv[]) {
    DVLOG(5) << "proc: " << p << " thread id: " << std::this_thread::get_id();
    p->schedule();
    DVLOG(5) << "proc done: " << std::this_thread::get_id() << " " << p;
    return EXIT_SUCCESS;
}

const time_point<steady_clock> &procnow() {
    return this_proc()->cached_time();
}

} // end namespace ten
