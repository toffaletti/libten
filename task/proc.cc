#include "task/proc.hh"
#include "task/io.hh"

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
        ssize_t nw = pi.write("\1", 1);
        (void)nw;
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
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what() << "\n" << e.str();
        std::exit(2);
    } catch (std::exception &e) {
        LOG(ERROR) << "unhandled error in proc::schedule: " << e.what();
        std::exit(2);
    }
}


proc::~proc() {
    std::unique_lock<std::mutex> lk(mutex);
    if (thread == 0) {
        {
            std::unique_lock<std::mutex> lk(procsmutex);
            for (auto i=procs.begin(); i!= procs.end(); ++i) {
                if (*i == this) continue;
                (*i)->cancel();
            }
        }
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
    // XXX: google glog handles this for us
    // in a more signal safe manner (no malloc)
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

    // install INFO handler
    THROW_ON_ERROR(sigaction(SIGUSR1, NULL, &act));
    if (act.sa_handler == SIG_DFL) {
        act.sa_sigaction = info_handler;
        act.sa_flags = SA_RESTART | SA_SIGINFO;
        THROW_ON_ERROR(sigaction(SIGUSR1, &act, NULL));
    }

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
