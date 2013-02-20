#include "thread_context.hh"

namespace ten {

thread_cached<runtime_tag, thread_context> this_ctx;

namespace {
    static bool glog_inited;
    struct stoplog_t {
        ~stoplog_t() { if (glog_inited) ShutdownGoogleLogging(); }
    } stoplog;
}

static void info_handler(int sig_num, siginfo_t *info, void *ctxt) {
    taskdumpf();
}

static void runtime_init() {
    CHECK(getpid() == syscall(SYS_gettid)) << "must call in main thread before anything else";
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

thread_context::thread_context() {
    task::set_default_stacksize(default_stacksize);
    std::call_once(init_flag, runtime_init);
}

thread_context::~thread_context() {
}

} // ten
