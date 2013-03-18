#include "thread_context.hh"
#include <sys/syscall.h>

namespace ten {

bool glog_inited;

namespace {
    std::once_flag boot_flag;
}

static void kernel_boot() {
    CHECK(is_main_thread()) << "must call in main thread before anything else";

    InitGoogleLogging(program_invocation_short_name);
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

kernel::time_point kernel::now() {
    return this_ctx->scheduler._now;
}

bool kernel::is_main_thread() {
    // TODO: could cache this in thread_context
    return getpid() == syscall(SYS_gettid);
}

size_t kernel::cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

void kernel::shutdown() {
    this_ctx->cancel_all();
}

int32_t kernel::is_computer_on() { return 1; }

double kernel::is_computer_on_fire() {
    // TODO: implement
    return 7.0;
}

void kernel::boot() {
    std::call_once(boot_flag, kernel_boot);
}

void kernel::wait_for_tasks() {
    this_ctx->scheduler.wait_for_all(1);
}

kernel::kernel() {
    boot();
}

kernel::~kernel() {
    wait_for_tasks();
}

} // ten

