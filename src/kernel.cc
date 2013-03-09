#include "kernel_private.hh"
#include "thread_context.hh"
#include <sys/syscall.h>

namespace ten {

namespace {
    static bool glog_inited;
    struct stoplog_t {
        ~stoplog_t() { if (glog_inited) ShutdownGoogleLogging(); }
    } stoplog;

    const char *glog_name = nullptr;

    std::once_flag boot_flag;
}

namespace kernel {

static void kernel_boot() {
    CHECK(is_main_thread()) << "must call in main thread before anything else";
    task::set_default_stacksize(default_stacksize);

    static char *perm_glog_name = glog_name ? strdup(glog_name) : program_invocation_short_name;
    InitGoogleLogging(perm_glog_name);
    glog_inited = true;

    // default to logging everything to stderr only.
    // ten::application turns this off again in favor of finer control.
    FLAGS_logtostderr = true;

    stack_t ss;
    ss.ss_sp = calloc(1, SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    throw_if(sigaltstack(&ss, NULL) == -1);

    InstallFailureSignalHandler();

    struct sigaction act;

    // ignore SIGPIPE
    memset(&act, 0, sizeof(act));
    throw_if(sigaction(SIGPIPE, NULL, &act) == -1);
    if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        throw_if(sigaction(SIGPIPE, &act, NULL) == -1);
    }

    netinit();
}

void set_logname(const char *name) {
    glog_name = name;
}

time_point now() {
    return this_ctx->scheduler._now;
}

ptr<task::pimpl> current_task() {
    return this_ctx->scheduler._current_task;
}

bool is_main_thread() {
    // TODO: could cache this in thread_context
    return getpid() == syscall(SYS_gettid);
}

size_t cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

void boot() {
    std::call_once(boot_flag, kernel_boot);
}

void shutdown() {
    this_ctx->scheduler.shutdown();
}

bool is_computer_on() { return true; }

bool is_computer_on_fire() {
    // TODO: implement
    return false;
}

} // kernel
} // ten

