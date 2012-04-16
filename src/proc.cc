#include "proc.hh"
#include "io.hh"

#include <cxxabi.h>
#include <execinfo.h>
#include <iostream>

namespace ten {

namespace hack {
// hack to avoid redefinition of ucontext
// by including it in a namespace
#include <asm-generic/ucontext.h>
}

__thread proc *_this_proc = 0;
static std::mutex procsmutex;
static proclist procs;
static std::once_flag init_flag;

void proc::add(proc *p) {
    std::unique_lock<std::mutex> lk(procsmutex);
    procs.push_back(p);
}

void proc::del(proc *p) {
    std::unique_lock<std::mutex> lk(procsmutex);
    auto i = std::find(procs.begin(), procs.end(), p);
    procs.erase(i);
}

void proc::deltaskinproc(task *t) {
    if (!t->systask) {
        --taskcount;
    }
    auto i = std::find(alltasks.begin(), alltasks.end(), t);
    CHECK(i != alltasks.end());
    alltasks.erase(i);
    DVLOG(5) << "POOLING task: " << t;
    taskpool.push_back(t);
    t->clear();
}

void proc::wakeupandunlock(std::unique_lock<std::mutex> &lk) {
    CHECK(lk.mutex() == &mutex);
    if (asleep) {
        cond.notify_one();
    } else if (polling) {
        polling = false;
        event.write(1);
    }
    lk.unlock();
}

io_scheduler &proc::sched() {
    if (_sched == 0) {
        _sched = new io_scheduler();
    }
    return *_sched;
}

void proc::schedule() {
    try {
        DVLOG(5) << "p: " << this << " entering proc::schedule";
        for (;;) {
            if (taskcount == 0) break;
            std::unique_lock<std::mutex> lk(mutex);
            while (runqueue.empty() && !canceled) {
                asleep = true;
                cond.wait(lk);
            }
            asleep = false;
            if (canceled) {
                // set canceled to false so we don't
                // execute this every time through the loop
                // while the tasks are cleaning up
                canceled = false;
                lk.unlock();
                procshutdown();
                lk.lock();
                CHECK(!runqueue.empty()) << "BUG: runqueue empty?";
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
            lk.unlock();
            co.swap(&t->co);
            lk.lock();
            ctask = 0;
            
            if (t->exiting) {
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

proc::proc(task *t)
  : _sched(0), nswitch(0), ctask(0),
    asleep(false), polling(false), canceled(false), taskcount(0)
{
    now = monotonic_clock::now();
    add(this);
    std::unique_lock<std::mutex> lk(mutex);
    if (t) {
        thread = new std::thread(proc::startproc, this, t);
        thread->detach();
    } else {
        // main thread proc
        _this_proc = this;
        thread = 0;
    }
}

proc::~proc() {
    std::unique_lock<std::mutex> lk(mutex);
    if (thread == 0) {
        {
            std::unique_lock<std::mutex> plk(procsmutex);
            for (auto i=procs.begin(); i!= procs.end(); ++i) {
                if (*i == this) continue;
                (*i)->cancel();
            }
        }
        for (;;) {
            // TODO: remove this busy loop in favor of sleeping the proc
            {
                std::unique_lock<std::mutex> plk(procsmutex);
                size_t np = procs.size();
                if (np == 1)
                    break;
            }
            std::this_thread::yield();
        }
        std::this_thread::yield();
        // nasty hack for mysql thread cleanup
        // because it happens *after* all of my code, i have no way of waiting
        // for it to finish with an event (unless i joined all threads)
        DVLOG(5) << "sleeping last proc for 1ms to allow other threads to really exit";
        usleep(1000);
    }
    delete thread;
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
    lk.unlock();
    del(this);
    DVLOG(5) << "proc freed: " << this;
    _this_proc = 0;
}

uint64_t procspawn(const std::function<void ()> &f, size_t stacksize) {
    task *t = new task(f, stacksize);
    uint64_t tid = t->id;
    new proc(t);
    // task could be freed at this point
    return tid;
}

void procshutdown() {
    proc *p = _this_proc;
    for (auto i = p->alltasks.cbegin(); i != p->alltasks.cend(); ++i) {
        task *t = *i;
        if (t == p->ctask) continue; // don't add ourself to the runqueue
        if (!t->systask) {
            t->cancel();
        }
    }
}

static void info_handler(int sig_num, siginfo_t *info, void *ctxt) {
    taskdumpf();
}

static void procmain_init() {
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

    new proc();
}

procmain::procmain() {
    std::call_once(init_flag, procmain_init);
    if (_this_proc == 0) {
        // needed for tests which call procmain a lot
        new proc();
    }
}

int procmain::main(int argc, char *argv[]) {
    std::unique_ptr<proc> p(_this_proc);
    p->schedule();
    return EXIT_SUCCESS;
}

const time_point<monotonic_clock> &procnow() {
    return _this_proc->now;
}

} // end namespace ten
