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
std::mutex procsmutex;
std::deque<ptr<proc>> procs;
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
    t->ready(true);
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
